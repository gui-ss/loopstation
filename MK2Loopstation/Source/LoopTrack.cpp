#include "LoopTrack.h"
#include <cmath>

static_assert(std::atomic<uint64_t>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free
              && std::atomic<float>::is_always_lock_free && std::atomic<bool>::is_always_lock_free,
              "Loop controls must be lock-free");
static_assert(LoopTrack::maximumFrames < (1 << 24), "Snapshot reserves 24 bits for length");

LoopTrack::LoopTrack(int capacityFrames) : buffer(juce::jlimit(1, maximumFrames, capacityFrames))
{
    prepare(48000);
}

LoopTrack::Snapshot LoopTrack::snapshot() const noexcept
{
    const auto value = published.load(std::memory_order_acquire);
    return { static_cast<State>(value & 7u), static_cast<int>((value >> 4u) & 0x00ffffffu),
             (value & 8u) != 0, static_cast<float>((value >> 32u) / 4294967296.0),
             (value & (1u << 28u)) != 0, (value & (1u << 29u)) != 0 };
}

LoopTrack::SyncStatus LoopTrack::syncStatus() const noexcept
{
    const auto value = publishedSync.load(std::memory_order_acquire);
    return { static_cast<SyncWait>(value & 3u), static_cast<SyncIssue>(value >> 2u) };
}

bool LoopTrack::needsSyncReference() const noexcept
{
    return (length > 0 && ! syncSetting) || syncBound || syncCapture || syncWait != SyncWait::none
        || pending.load(std::memory_order_relaxed) == static_cast<uint32_t>(Command::primary);
}

bool LoopTrack::retainsSyncReference() const noexcept
{ return syncBound || syncCapture || syncWait != SyncWait::none; }

bool LoopTrack::compatible(const MusicalClock::SyncBlock& sync) const noexcept
{
    if (length == 0 || sync.framesPerBar <= 0) return false;
    const double frames = sync.framesPerBar * recordedRate / deviceRate;
    const double bars = std::round(length / frames);
    return bars >= 1 && std::abs(length - bars * frames) < 1.e-5;
}

double LoopTrack::syncPosition(double bar) const noexcept
{
    double result = std::fmod((bar - playbackStartBar) * syncedBarFrames, static_cast<double>(length));
    if (result < 0) result += length;
    // Remove floating roundoff at integer sample boundaries, not real fractions.
    if (std::abs(result - std::round(result)) < 1.e-5) result = std::round(result);
    return result >= length ? 0 : result;
}

bool LoopTrack::request(Command command) noexcept
{
    if (command == Command::clear && snapshot().state != State::stopped) return false;
    if (command == Command::undo && ! snapshot().canUndo) return false;
    uint32_t expected = 0;
    return pending.compare_exchange_strong(expected, static_cast<uint32_t>(command), std::memory_order_release);
}

void LoopTrack::setVolume(float gain) noexcept
{
    volume.store(std::isfinite(gain) ? juce::jlimit(0.0f, 1.0f, gain) : 0.0f, std::memory_order_relaxed);
}

void LoopTrack::publish() noexcept
{
    const bool canUndo = undoAvailable && (state == State::playing || state == State::stopped);
    const auto phase = length > 0 && (state == State::playing || state == State::overdubbing)
        ? static_cast<uint64_t>(position / length * 4294967296.0) : 0;
    publishedSync.store(static_cast<uint32_t>(syncWait) | (static_cast<uint32_t>(syncIssue) << 2u), std::memory_order_release);
    published.store((static_cast<uint64_t>(length) << 4u) | (canUndo ? 8u : 0u)
                    | (awaitingBoundary ? (1u << 28u) : 0u)
                    | (recordingQuantized && state == State::recording ? (1u << 29u) : 0u)
                    | static_cast<uint32_t>(state) | (phase << 32u), std::memory_order_release);
}

void LoopTrack::prepare(double rate) noexcept
{
    suspend();
    deviceRate = std::isfinite(rate) && rate > 0 ? rate : 48000.0;
    recordLimit = static_cast<int>(juce::jmin(static_cast<double>(buffer.capacity()), deviceRate * maximumSeconds));
    playbackStep = recordedRate / deviceRate;
    tailLength = juce::jmax(1, juce::roundToInt(deviceRate * 0.005));
    fader.reset(deviceRate, 0.02);
    fader.setCurrentAndTargetValue(volume.load(std::memory_order_relaxed));
    transport.reset(deviceRate, 0.005);
    transport.setCurrentAndTargetValue(0);
    dubGain.reset(deviceRate, 0.005);
}

void LoopTrack::suspend() noexcept
{
    syncWait = SyncWait::none;
    if (state == State::overdubbing) finishOverdub();
    if (state == State::recording) finishRecording(false);
    if (state == State::playing) state = State::stopped;
    position = 0;
    syncPlaying = false;
    tailRemaining = 0;
    undoRemaining = 0; undoRampStart = false;
    lastLeft = lastRight = 0;
    pending.exchange(0, std::memory_order_acquire);
    publish();
}

void LoopTrack::startPlayback() noexcept
{
    ++playbackEpoch;
    state = State::playing;
    position = 0;
    playbackStep = recordedRate / deviceRate;
    tailRemaining = 0;
    transport.setCurrentAndTargetValue(0);
    transport.setTargetValue(1);
}

void LoopTrack::finishRecording(bool play) noexcept
{
    awaitingBoundary = false; playAtFrame = 0;
    if (length == 0) { state = State::empty; syncCapture = syncBound = syncPlaying = false; return; }
    // Short, non-destructive boundary tapers: keep every recorded frame and the
    // exact loop period, without inserting silence or shortening with overlap.
    edgeFrames = juce::jmin(juce::roundToInt(recordedRate * 0.003), (length - 1) / 2);
    if (syncCapture)
    {
        const double bars = std::round(length / syncedBarFrames);
        syncBound = bars >= 1 && std::abs(length - bars * syncedBarFrames) < 1.e-5;
        if (! syncBound) syncIssue = SyncIssue::incompatible;
        playbackStartBar = recordingStartBar + bars;
    }
    if (play) { startPlayback(); syncPlaying = syncCapture && syncBound; }
    else { state = State::stopped; syncPlaying = false; }
    syncCapture = false;
}

void LoopTrack::beginRecording(double barFrames, bool synced) noexcept
{
    state = State::recording; length = 0; recordedRate = deviceRate;
    awaitingBoundary = false; playAtFrame = 0; captureLimit = recordLimit;
    // Freeze the grid and option for this take. Subsequent BPM/Q edits affect
    // the next initial recording, never resize this phrase or an overdub.
    syncCapture = synced;
    recordingQuantized = (synced || quantizeEnabled()) && std::isfinite(barFrames) && barFrames >= 1;
    recordingBarFrames = recordingQuantized ? barFrames : 0;
    if (recordingQuantized)
    {
        const double bars = std::floor(recordLimit / recordingBarFrames);
        if (bars >= 1)
            captureLimit = juce::jmin(recordLimit, static_cast<int>(std::ceil(bars * recordingBarFrames - 1.0e-7)));
        // If even one bar exceeds capacity, memory safety takes precedence:
        // finish at the buffer limit. Normal 60 s / 40..300 BPM always fits.
    }
}

void LoopTrack::requestPlayback(bool hasOutput) noexcept
{
    if (! recordingQuantized) { finishRecording(hasOutput); return; }
    if (awaitingBoundary) return; // Repeated clicks cannot postpone the target.
    double bars = juce::jmax(1.0, std::floor(length / recordingBarFrames));
    double boundary = std::ceil(bars * recordingBarFrames - 1.0e-7);
    if (boundary < length) boundary = std::ceil(++bars * recordingBarFrames - 1.0e-7);
    playAtFrame = static_cast<int>(juce::jmin(static_cast<double>(captureLimit), boundary));
    if (length >= playAtFrame) finishRecording(hasOutput);
    else awaitingBoundary = true;
}

void LoopTrack::stop() noexcept
{
    syncWait = SyncWait::none;
    syncIssue = SyncIssue::none;
    syncPlaying = false;
    if (state == State::overdubbing) finishOverdub();
    if (state == State::recording) finishRecording(false);
    else if (state == State::playing)
    {
        state = State::stopped;
        tailLeft = lastLeft; tailRight = lastRight;
        tailRemaining = tailLength;
    }
    position = 0;
    undoRemaining = 0; undoRampStart = false;
}

void LoopTrack::finishOverdub() noexcept
{
    // Remove a short, tapered portion of the actual additions, never of the
    // previous phrase. Fixed 64-entry bound, even after many loop revolutions.
    for (size_t i = 0; i < additionCount; ++i)
    {
        const auto& a = additions[(additionHead + additions.size() - 1 - i) % additions.size()];
        const float amount = 1.0f - static_cast<float>(i) / static_cast<float>(additionCount);
        buffer.set(a.frame, buffer.get(0, a.frame) - amount * a.left,
                            buffer.get(1, a.frame) - amount * a.right);
    }
    additionCount = 0;
    if (dubWritten) undoAvailable = true;
    state = State::playing;
}

void LoopTrack::addOverdub(int frame, float left, float right) noexcept
{
    // A zero-frame take must not discard the preceding undo operation.
    if (! dubWritten) buffer.snapshot();
    const auto safeSum = [](float original, float added)
    {
        if (! std::isfinite(added) || added == 0) return original;
        const float sum = original + juce::jlimit(-1.0f, 1.0f, added);
        const float magnitude = std::abs(sum);
        // Linear at ordinary levels, soft knee above 0.9, peak below unity.
        if (magnitude <= 0.9f) return sum;
        const float excess = magnitude - 0.9f;
        return std::copysign(0.9f + 0.1f * excess / (0.1f + excess), sum);
    };
    const float oldL = buffer.get(0, frame), oldR = buffer.get(1, frame);
    const float newL = safeSum(oldL, left), newR = safeSum(oldR, right);
    buffer.makeWritable(frame);
    buffer.set(frame, newL, newR);
    additions[additionHead] = { frame, newL - oldL, newR - oldR };
    additionHead = (additionHead + 1) % additions.size();
    additionCount = juce::jmin(additionCount + 1, additions.size());
    dubWritten = true;
}

float LoopTrack::sampleAt(int channel, int frame) const noexcept
{
    float envelope = 1;
    if (edgeFrames > 0)
        envelope = juce::jmin(1.0f, static_cast<float>(juce::jmin(frame, length - 1 - frame)) / edgeFrames);
    return buffer.get(channel, frame) * envelope;
}

void LoopTrack::process(const float* const* inputs, int numInputs, float* const* outputs,
                        int numOutputs, int numSamples, double barFrames, MusicalClock::SyncBlock sync, float* playbackGains) noexcept
{
    const juce::ScopedNoDenormals noDenormals;
    const float* inputLeft = inputs != nullptr && numInputs > 0 ? inputs[0] : nullptr;
    const float* inputRight = inputs != nullptr && numInputs > 1 ? inputs[1] : inputLeft;
    const bool hasInput = inputLeft != nullptr || inputRight != nullptr;
    bool hasOutput = false;
    for (int ch = 0; outputs != nullptr && ch < numOutputs; ++ch) hasOutput |= outputs[ch] != nullptr;
    syncSetting = sync.configured;
    if (! sync.enabled)
    {
        syncWait = SyncWait::none;
        syncBound = false;
        syncPlaying = false; // Continue from the current audio cursor, without seeking.
        if (! sync.configured) syncIssue = SyncIssue::none;
    }
    else if (! syncCapture)
    {
        syncBound = compatible(sync);
        if (length > 0) syncIssue = ! syncBound ? SyncIssue::incompatible : SyncIssue::none;
        if ((state == State::playing || state == State::overdubbing || state == State::recording) && ! syncPlaying)
            syncIssue = SyncIssue::stopFirst; // Never rephase a running free phrase silently.
    }
    const auto command = static_cast<Command>(pending.exchange(0, std::memory_order_acquire));
    if (command == Command::clear && state == State::stopped)
    {
        length = 0; state = State::empty; tailRemaining = 0;
        syncWait = SyncWait::none; syncBound = syncCapture = syncPlaying = false;
        syncIssue = SyncIssue::none;
        undoAvailable = false; buffer.discard();
        lastLeft = lastRight = 0; // No bulk clearing on the audio thread.
    }
    else if (command == Command::stop) stop();
    else if (command == Command::undo && undoAvailable
             && (state == State::playing || state == State::stopped))
    {
        buffer.restore(); undoAvailable = false;
        undoRampStart = state == State::playing;
        undoRemaining = undoRampStart ? tailLength : 0;
    }
    else if (command == Command::primary)
    {
        if (syncWait != SyncWait::none) {} // Repeated clicks do not move the pending boundary.
        else if (sync.enabled && ((state == State::empty && hasInput)
                                  || (state == State::stopped && length > 0 && hasOutput)))
        {
            if (state == State::stopped && ! compatible(sync)) syncIssue = SyncIssue::incompatible;
            else if (state == State::empty && std::abs(sync.framesPerBar - std::round(sync.framesPerBar)) > 1.e-5)
                syncIssue = SyncIssue::sampleGrid; // Preserve old phrases after an incompatible rate switch.
            else if (state == State::empty && sync.framesPerBar > recordLimit)
                syncIssue = SyncIssue::capacity;
            else
            {
                syncWait = state == State::empty ? SyncWait::record : SyncWait::play;
                pendingBar = std::max(0.0, std::ceil(sync.bar - 1.e-10));
                syncIssue = SyncIssue::none;
            }
        }
        else if (state == State::empty && hasInput) beginRecording(barFrames);
        else if (state == State::recording) requestPlayback(hasOutput);
        else if (state == State::stopped && length > 0 && hasOutput) { startPlayback(); syncPlaying = false; }
        else if (state == State::playing && hasInput && hasOutput)
        {
            dubWritten = false;
            additionHead = additionCount = 0;
            dubGain.setCurrentAndTargetValue(0); dubGain.setTargetValue(1);
            state = State::overdubbing;
        }
        else if (state == State::overdubbing) finishOverdub();
    }
    if ((state == State::recording && ! hasInput)
        || ((state == State::playing || state == State::overdubbing) && ! hasOutput)) stop();
    if (state == State::overdubbing && ! hasInput) finishOverdub();
    if ((syncWait == SyncWait::record && ! hasInput) || (syncWait == SyncWait::play && ! hasOutput))
        syncWait = SyncWait::none;
    const auto startPending = [&](double bar)
    {
        if (syncWait == SyncWait::none || bar + 1.e-10 < pendingBar) return;
        if (syncWait == SyncWait::record)
        {
            recordingStartBar = pendingBar;
            syncedBarFrames = std::round(sync.framesPerBar);
            beginRecording(syncedBarFrames, true);
        }
        else
        {
            playbackStartBar = pendingBar;
            syncedBarFrames = sync.framesPerBar * recordedRate / deviceRate;
            startPlayback(); syncPlaying = true;
            position = syncPosition(bar);
        }
        syncWait = SyncWait::none;
    };
    startPending(sync.bar);
    if (syncPlaying && sync.enabled) position = syncPosition(sync.bar);
    fader.setTargetValue(volume.load(std::memory_order_relaxed));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const double sampleBar = sync.enabled ? sync.bar + sample / sync.framesPerBar : 0;
        startPending(sampleBar);
        float left = 0, right = 0;
        if (playbackGains != nullptr) playbackGains[sample] = 0;
        const float gain = fader.getNextValue();
        if (state == State::recording)
        {
            buffer.set(length, inputLeft != nullptr ? inputLeft[sample] : 0,
                               inputRight != nullptr ? inputRight[sample] : 0);
            ++length;
            if (length >= captureLimit || (awaitingBoundary && length >= playAtFrame))
            {
                finishRecording(hasOutput);
                if (syncPlaying && sync.enabled) position = syncPosition(sampleBar + 1.0 / sync.framesPerBar);
            } // Next sample, even in this block, is PLAY.
        }
        else if (state == State::playing || state == State::overdubbing)
        {
            const int frame = static_cast<int>(position);
            const int next = frame + 1 == length ? 0 : frame + 1;
            const float fraction = static_cast<float>(position - frame);
            const float level = gain * transport.getNextValue();
            const float l = sampleAt(0, frame), r = sampleAt(1, frame);
            left = l + fraction * (sampleAt(0, next) - l);
            right = r + fraction * (sampleAt(1, next) - r);
            lastPlaybackGain = level;
            if (playbackGains != nullptr) playbackGains[sample] = level;
            else { left *= level; right *= level; }
            if (undoRemaining > 0)
            {
                if (undoRampStart)
                {
                    undoLeft = lastLeft; undoRight = lastRight;
                    undoRampStart = false;
                }
                const float fade = static_cast<float>(undoRemaining--) / tailLength;
                left = left * (1.0f - fade) + undoLeft * fade;
                right = right * (1.0f - fade) + undoRight * fade;
            }
            if (state == State::overdubbing)
            {
                const float captureGain = dubGain.getNextValue();
                const float micL = (inputLeft != nullptr ? inputLeft[sample] : 0) * captureGain;
                const float micR = (inputRight != nullptr ? inputRight[sample] : 0) * captureGain;
                // Integrate the input over the original recording's frame grid.
                // Also preserves time/gain if the device sample rate changed.
                const double end = position + playbackStep;
                for (double cursor = position; cursor < end;)
                {
                    const int index = static_cast<int>(cursor);
                    const double boundary = juce::jmin(end, std::floor(cursor) + 1.0);
                    const float weight = static_cast<float>(boundary - cursor);
                    addOverdub(index % length, micL * weight, micR * weight);
                    cursor = boundary;
                }
            }
            position += playbackStep;
            if (position >= length) { position -= std::floor(position / length) * length; ++playbackEpoch; }
        }
        else if (tailRemaining > 0)
        {
            const float fade = static_cast<float>(tailRemaining--) / tailLength;
            left = tailLeft * fade; right = tailRight * fade;
            if (playbackGains != nullptr) playbackGains[sample] = lastPlaybackGain;
        }
        lastLeft = left; lastRight = right;
        if (outputs != nullptr && numOutputs == 1 && outputs[0] != nullptr)
            outputs[0][sample] += 0.5f * left + 0.5f * right;
        else if (outputs != nullptr)
        {
            if (numOutputs > 0 && outputs[0] != nullptr) outputs[0][sample] += left;
            if (numOutputs > 1 && outputs[1] != nullptr) outputs[1][sample] += right;
        }
    }
    if (sync.enabled)
    {
        // Service a start exactly at block end without adding a silent block.
        startPending(sync.bar + numSamples / sync.framesPerBar);
        if (syncPlaying) position = syncPosition(sync.bar + numSamples / sync.framesPerBar);
    }
    publish();
}
