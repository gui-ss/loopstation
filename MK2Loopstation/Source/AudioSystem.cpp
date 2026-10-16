#include "AudioSystem.h"
#include <algorithm>
#include <cmath>
#include <chrono>

static_assert(std::atomic<float>::is_always_lock_free && std::atomic<bool>::is_always_lock_free,
              "Monitor controls must be lock-free on the audio thread");

namespace
{
// Shared WASAPI exposes independent Windows input/output endpoints, without
// taking exclusive ownership or adding driver-mode controls to this stage.
class WindowsAudioManager final : public juce::AudioDeviceManager
{
    void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types) override
    {
        if (auto* type = juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::shared))
            types.add(type);
    }
};
}

AudioSystem::AudioSystem() : AudioSystem(std::make_unique<WindowsAudioManager>()) {}
AudioSystem::AudioSystem(std::unique_ptr<juce::AudioDeviceManager> deviceManager, int capacityFrames)
    : manager(std::move(deviceManager))
{
    static_assert(maximumAudioBytes < size_t(900) * 1024 * 1024, "Loop RAM budget exceeded");
    try
    {
        for (auto& track : tracks) track = std::make_unique<LoopTrack>(capacityFrames);
    }
    catch (const std::bad_alloc&)
    {
        // All-or-nothing: never show five usable tracks with missing storage.
        for (auto& track : tracks) track.reset();
        status = L"Memória insuficiente para as cinco pistas e Undo (até 881 MiB). Libere memória e reinicie o aplicativo.";
    }
}

AudioSystem::~AudioSystem()
{
    stopTimer();
    manager->removeAudioCallback(this);
    manager->closeAudioDevice();
}

void AudioSystem::initialise()
{
    if (initialised || ! storageReady()) return;
    initialised = true;
    manager->addAudioCallback(this);
    // Keep the requested names even if opening the hardware fails.
    if (auto* type = manager->getAvailableDeviceTypes().getFirst())
    {
        type->scanForDevices();
        inputName = type->getDeviceNames(true)[type->getDefaultDeviceIndex(true)];
        outputName = type->getDeviceNames(false)[type->getDefaultDeviceIndex(false)];
    }
    error = manager->initialise(2, 2, nullptr, false);
    if (error.isEmpty()) optimiseBufferSize();
    updateStatus();
    startTimer(250);
}

juce::StringArray AudioSystem::deviceNames(bool input) const
{
    if (auto* type = manager->getCurrentDeviceTypeObject()) return type->getDeviceNames(input);
    return {};
}

void AudioSystem::selectDevices(const juce::String& input, const juce::String& output)
{
    inputName = input;
    outputName = output;
    applySelection();
}

void AudioSystem::reconnect()
{
    if (auto* type = manager->getCurrentDeviceTypeObject()) type->scanForDevices();
    applySelection();
}

LoopTrack::Snapshot AudioSystem::trackSnapshot(int index) const
{
    if (! juce::isPositiveAndBelow(index, trackCount) || ! storageReady()) return {};
    return tracks[static_cast<size_t>(index)]->snapshot();
}

void AudioSystem::setTrackVolume(int index, float gain)
{
    if (juce::isPositiveAndBelow(index, trackCount) && storageReady())
        tracks[static_cast<size_t>(index)]->setVolume(gain);
}

void AudioSystem::selectTrack(int index)
{
    if (juce::isPositiveAndBelow(index, trackCount)) selected = index;
}

void AudioSystem::setLoopSync(int index, bool enabled)
{
    if (! juce::isPositiveAndBelow(index, trackCount)) return;
    const auto bit = uint32_t(1) << index;
    if (enabled) syncMask.fetch_or(bit, std::memory_order_relaxed);
    else syncMask.fetch_and(~bit, std::memory_order_relaxed);
}

void AudioSystem::setAllLoopSync(bool enabled)
{ syncMask.store(enabled ? 31u : 0u, std::memory_order_relaxed); }

bool AudioSystem::loopSyncEnabled(int index) const
{
    return juce::isPositiveAndBelow(index, trackCount)
        && (syncMask.load(std::memory_order_relaxed) & (uint32_t(1) << index)) != 0;
}

LoopTrack::SyncStatus AudioSystem::trackSyncStatus(int index) const
{
    if (! juce::isPositiveAndBelow(index, trackCount) || ! storageReady()) return {};
    return tracks[static_cast<size_t>(index)]->syncStatus();
}

bool AudioSystem::setBpm(double value)
{
    // Offline OFF must also release the tempo guard, after joining the driver.
    if (syncMask.load(std::memory_order_relaxed) == 0 && ! running.load())
    { manager->closeAudioDevice(); clock.endSync(); }
    const bool accepted = clock.setBpm(value);
    rejectedBpm = ! accepted && clock.tempoLocked();
    if (accepted) tap.reset();
    return accepted;
}

bool AudioSystem::commandTrack(int index, LoopTrack::Command command)
{
    if (! juce::isPositiveAndBelow(index, trackCount) || ! storageReady()) return false;
    auto& track = *tracks[static_cast<size_t>(index)];
    // With no stream, close/join the device before servicing STOP/CLEAR on the
    // message thread. Never leave a stale REC request to run after reconnection.
    auto* device = manager->getCurrentAudioDevice();
    if (device == nullptr || ! device->isPlaying() || deviceFailed.load())
    {
        manager->closeAudioDevice();
        track.suspend();
        const bool accepted = track.request(command);
        auto sync = clock.syncBlock();
        sync.configured = loopSyncEnabled(index);
        sync.enabled &= sync.configured;
        track.process(nullptr, 0, nullptr, 0, 0, clock.process(0), sync);
        releaseUnusedSync();
        return accepted;
    }
    return track.request(command);
}

void AudioSystem::applySelection()
{
    if (! storageReady()) return;
    monitoring.store(false, std::memory_order_relaxed);
    stabilityFallback = false;
    // Close first: even an unchanged setup must be retried after a driver failure.
    manager->closeAudioDevice();
    deviceFailed.store(false);
    error.clear();
    const bool missingInput = inputName.isNotEmpty() && ! deviceNames(true).contains(inputName);
    const bool missingOutput = outputName.isNotEmpty() && ! deviceNames(false).contains(outputName);
    if (! missingInput && ! missingOutput)
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = inputName;
        setup.outputDeviceName = outputName;
        setup.useDefaultInputChannels = setup.useDefaultOutputChannels = false;
        if (inputName.isNotEmpty()) setup.inputChannels.setRange(0, 2, true);
        if (outputName.isNotEmpty()) setup.outputChannels.setRange(0, 2, true);
        error = manager->setAudioDeviceSetup(setup, true);
        if (error.isEmpty()) optimiseBufferSize();
    }
    updateStatus();
}

bool AudioSystem::canMonitor() const
{
    auto* device = manager->getCurrentAudioDevice();
    const auto actual = manager->getAudioDeviceSetup();
    return device != nullptr && running.load() && ! deviceFailed.load() && device->isPlaying()
        && actual.inputDeviceName == inputName && actual.outputDeviceName == outputName
        && deviceNames(true).contains(inputName) && deviceNames(false).contains(outputName)
        && ! device->getActiveInputChannels().isZero() && ! device->getActiveOutputChannels().isZero();
}

void AudioSystem::setMonitoringEnabled(bool enabled)
{
    monitoring.store(enabled && canMonitor(), std::memory_order_relaxed);
    sendChangeMessage();
}

void AudioSystem::setMonitorVolume(float gain)
{
    volume.store(std::isfinite(gain) ? juce::jlimit(0.0f, 1.0f, gain) : 0.0f, std::memory_order_relaxed);
    sendChangeMessage();
}

void AudioSystem::optimiseBufferSize()
{
    auto* device = manager->getCurrentAudioDevice();
    if (device == nullptr || ! device->isOpen()) return;
    const auto original = manager->getAudioDeviceSetup();
    stableBufferSize = original.bufferSize;
    auto candidate = original;
    const int target = static_cast<int>(std::ceil(original.sampleRate * 0.005));
    for (int size : device->getAvailableBufferSizes())
        if (size >= target && size < candidate.bufferSize) candidate.bufferSize = size;
    if (candidate.bufferSize != original.bufferSize)
    {
        if (manager->setAudioDeviceSetup(candidate, true).isNotEmpty())
        {
            error = manager->setAudioDeviceSetup(original, true);
            stabilityFallback = true;
        }
    }
    xrunsAtWindowStart = manager->getXRunCount();
    xrunWindowStart = juce::Time::getMillisecondCounterHiRes();
}

void AudioSystem::checkBufferStability()
{
    auto* device = manager->getCurrentAudioDevice();
    if (device == nullptr || ! device->isPlaying() || stabilityFallback) return;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const int xruns = manager->getXRunCount();
    auto setup = manager->getAudioDeviceSetup();
    if (xruns - xrunsAtWindowStart >= 3 && setup.bufferSize < stableBufferSize)
    {
        monitoring.store(false, std::memory_order_relaxed);
        setup.bufferSize = stableBufferSize;
        error = manager->setAudioDeviceSetup(setup, true);
        stabilityFallback = true;
    }
    if (now - xrunWindowStart >= 2000.0 || xruns < xrunsAtWindowStart)
    {
        xrunsAtWindowStart = xruns;
        xrunWindowStart = now;
    }
}

void AudioSystem::timerCallback() { updateStatus(); checkBufferStability(); }

void AudioSystem::updateStatus()
{
    const auto inputs = deviceNames(true), outputs = deviceNames(false);
    const bool missingInput = inputName.isNotEmpty() && ! inputs.contains(inputName);
    const bool missingOutput = outputName.isNotEmpty() && ! outputs.contains(outputName);
    const auto actual = manager->getAudioDeviceSetup();
    const bool changedDevice = actual.inputDeviceName != inputName || actual.outputDeviceName != outputName;
    if (missingInput || missingOutput || changedDevice || deviceFailed.load())
        monitoring.store(false, std::memory_order_relaxed);
    // JUCE can fall back to a default endpoint after hot-unplug. Do not silently
    // adopt a different microphone: keep the choice visible and require retry.
    if (manager->getCurrentAudioDevice() != nullptr
        && (missingInput || missingOutput || changedDevice || deviceFailed.load()))
        manager->closeAudioDevice();

    juce::String next;
    if (missingInput || missingOutput)
        next = (missingInput ? juce::String(L"Microfone desconectado ou indisponível. ") : juce::String())
             + (missingOutput ? juce::String(L"Saída desconectada ou indisponível. ") : juce::String())
             + "Conecte o dispositivo e clique em Reconectar, ou selecione outro.";
    else if (error.isNotEmpty() || deviceFailed.load())
        next = L"Não foi possível conectar o áudio. Verifique o dispositivo e a permissão de microfone do Windows.\n"
             + (error.isNotEmpty() ? error : juce::String(L"O dispositivo interrompeu a conexão. Clique em Reconectar."));
    else if (inputName.isEmpty() && outputName.isEmpty())
        next = inputs.isEmpty() && outputs.isEmpty() ? L"Nenhum dispositivo de áudio disponível."
                                                    : L"Áudio desconectado: nenhum dispositivo selecionado.";
    else if (auto* device = manager->getCurrentAudioDevice(); device != nullptr && device->isOpen()
             && device->isPlaying() && running.load())
    {
        const bool inputOpen = inputName.isNotEmpty() && ! device->getActiveInputChannels().isZero();
        const bool outputOpen = outputName.isNotEmpty() && ! device->getActiveOutputChannels().isZero();
        next = (inputOpen ? juce::String("Microfone conectado. ") : juce::String(L"Microfone não conectado. "))
             + (outputOpen ? juce::String(L"Saída conectada.") : juce::String(L"Saída não conectada."));
    }
    else
        next = L"Áudio desconectado. Clique em Reconectar para tentar novamente.";

    if (stabilityFallback && error.isEmpty()) next += L"\nBuffer padrão mantido para estabilidade.";
    if (status != next || knownInputs != inputs || knownOutputs != outputs
        || publishedMonitoring != isMonitoringEnabled())
    {
        status = next;
        knownInputs = inputs;
        knownOutputs = outputs;
        publishedMonitoring = isMonitoringEnabled();
        sendChangeMessage();
    }
}

void AudioSystem::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    clock.prepare(device->getCurrentSampleRate());
    trackFx.prepare(device->getCurrentSampleRate());
    for (auto& robot : inputRobots) robot.prepare(device->getCurrentSampleRate());
    for (int i = 0; i < trackCount; ++i)
    {
        inputFxGate[size_t(i)].reset(device->getCurrentSampleRate(), .02);
        inputFxGate[size_t(i)].setCurrentAndTargetValue(trackFxEnabled(i) ? 1.f : 0.f);
    }
    for (auto& track : tracks) track->prepare(device->getCurrentSampleRate());
    monitoring.store(false, std::memory_order_relaxed);
    monitorGain.reset(device->getCurrentSampleRate(), 0.02);
    monitorGain.setCurrentAndTargetValue(0.0f);
    running.store(true);
}
void AudioSystem::releaseUnusedSync() noexcept
{
    const auto mask = syncMask.load(std::memory_order_relaxed);
    for (int i = 0; i < trackCount; ++i)
        if ((mask & (uint32_t(1) << i)) != 0 && tracks[static_cast<size_t>(i)]
            && tracks[static_cast<size_t>(i)]->retainsSyncReference()) return;
    clock.endSync();
}

void AudioSystem::audioDeviceStopped()
{
    for (auto& robot : inputRobots) robot.reset();
    for (auto& track : tracks) if (track) track->suspend();
    releaseUnusedSync();
    monitoring.store(false, std::memory_order_relaxed);
    running.store(false);
}
void AudioSystem::audioDeviceError(const juce::String&)
{
    monitoring.store(false, std::memory_order_relaxed);
    deviceFailed.store(true);
}

void AudioSystem::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs,
    float* const* outputs, int numOutputs, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    // Real-time boundary: only lock-free control reads, smoothing and sample arithmetic.
    blockSyncMask = syncMask.load(std::memory_order_relaxed);
    if (! deviceFailed.load(std::memory_order_relaxed))
        for (int i = 0; i < trackCount; ++i)
            if ((blockSyncMask & (uint32_t(1) << i)) != 0 && tracks[static_cast<size_t>(i)]->needsSyncReference())
            { clock.beginSync(); break; }
    blockSync = clock.syncBlock(); // Shared start position BEFORE advancing this block.
    const double barFrames = ! deviceFailed.load(std::memory_order_relaxed) ? clock.process(numSamples) : 0;
    if (deviceFailed.load(std::memory_order_relaxed))
    {
        for (auto& track : tracks) track->suspend();
        releaseUnusedSync();
    }
    for (int channel = 0; channel < numOutputs; ++channel)
        if (outputs[channel] != nullptr) std::fill_n(outputs[channel], numSamples, 0.f);
    if (! monitoring.load(std::memory_order_relaxed) || deviceFailed.load(std::memory_order_relaxed)
        || numInputs <= 0 || inputs == nullptr)
        monitorGain.setCurrentAndTargetValue(0.f);
    else monitorGain.setTargetValue(volume.load(std::memory_order_relaxed));
    if (! deviceFailed.load(std::memory_order_relaxed))
        processTracks(inputs, numInputs, outputs, numOutputs, numSamples, barFrames);
    else for (auto& robot : inputRobots) robot.reset();
}

void AudioSystem::tapTempo()
{
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const double estimate = tap.tap(now);
    if (estimate > 0)
    {
        if (syncMask.load(std::memory_order_relaxed) == 0 && ! running.load())
        { manager->closeAudioDevice(); clock.endSync(); }
        rejectedBpm = ! clock.setBpm(estimate) && clock.tempoLocked();
    }
}

void AudioSystem::processTracks(const float* const* inputs, int numInputs,
    float* const* outputs, int numOutputs, int numSamples, double barFrames) noexcept
{
    const juce::ScopedNoDenormals noDenormals;
    const bool enabled = quantize.load(std::memory_order_relaxed);
    // Fixed chunks bound scratch RAM irrespective of driver callback size.
    // Pre-fader playback and smoothed gains stay separate from microphone/monitor.
    trackFx.beginBlock();
    for (auto& robot : inputRobots) robot.beginBlock();
    for (int i = 0; i < trackCount; ++i) inputFxGate[size_t(i)].setTargetValue(trackFxEnabled(i) ? 1.f : 0.f);
    bool hasOutput = false;
    for (int ch = 0; outputs != nullptr && ch < numOutputs; ++ch) hasOutput |= outputs[ch] != nullptr;
    bool keepReference = false;
    for (int offset = 0; offset < numSamples || (offset == 0 && numSamples == 0);)
    {
        const int frames = std::min(routingBlockSize, numSamples - offset);
        const float* mic[2] { inputs != nullptr && numInputs > 0 && inputs[0] ? inputs[0] + offset : nullptr,
                             inputs != nullptr && numInputs > 1 && inputs[1] ? inputs[1] + offset : nullptr };
        const bool haveInput = mic[0] != nullptr || mic[1] != nullptr;
        if (! haveInput) for (auto& robot : inputRobots) robot.reset();
        for (int frame = 0; frame < frames; ++frame)
        {
            float l = mic[0] ? mic[0][frame] : 0.f;
            float r = numInputs == 1 ? l : mic[1] ? mic[1][frame] : 0.f;
            if (haveInput) for (auto& robot : inputRobots) robot.processFrame(l, r);
            processedInput[0][size_t(frame)] = l; processedInput[1][size_t(frame)] = r;
        }
        for (int i = 0; i < trackCount; ++i)
        {
            // PG p.2: per-track FX also gates INPUT FX for recording/overdub.
            // Live monitoring stays independent from these recording destinations.
            for (int frame = 0; frame < frames; ++frame)
            {
                const float gain = inputFxGate[size_t(i)].getNextValue();
                const float l = mic[0] ? mic[0][frame] : 0.f;
                const float r = numInputs == 1 ? l : mic[1] ? mic[1][frame] : 0.f;
                captureInput[0][size_t(frame)] = l + gain * (processedInput[0][size_t(frame)] - l);
                captureInput[1][size_t(frame)] = r + gain * (processedInput[1][size_t(frame)] - r);
            }
            const float* capture[2] { mic[0] ? captureInput[0].data() : nullptr,
                                      (mic[1] || (numInputs == 1 && mic[0])) ? captureInput[1].data() : nullptr };
            auto& scratch = playbackScratch[size_t(i)];
            for (auto& channel : scratch) std::fill_n(channel.data(), frames, 0.f);
            float* output[2] { scratch[0].data(), scratch[1].data() };
            auto sync = blockSync;
            sync.configured = (blockSyncMask & (uint32_t(1) << i)) != 0;
            sync.enabled = sync.enabled && sync.configured;
            if (sync.enabled) sync.bar += offset / sync.framesPerBar;
            auto& track = tracks[size_t(i)];
            track->setQuantizeEnabled(enabled);
            track->process(capture, std::min(numInputs, 2), output, hasOutput ? 2 : 0,
                           frames, barFrames, sync, scratch[2].data());
        }
        for (int frame = 0; frame < frames; ++frame)
        {
            std::array<float, trackCount> l {}, r {}, gains {};
            for (size_t i = 0; i < tracks.size(); ++i)
            { l[i] = playbackScratch[i][0][frame]; r[i] = playbackScratch[i][1][frame]; gains[i] = playbackScratch[i][2][frame]; }
            float left = 0, right = 0;
            trackFx.processFrame(l, r, left, right, gains);
            const float monitor = monitorGain.getNextValue();
            left += processedInput[0][size_t(frame)] * monitor;
            right += processedInput[1][size_t(frame)] * monitor;
            if (numOutputs == 1 && outputs[0]) outputs[0][offset + frame] += .5f * (left + right);
            else if (numOutputs > 1)
            {
                if (outputs[0]) outputs[0][offset + frame] += left;
                if (outputs[1]) outputs[1][offset + frame] += right;
            }
        }
        if (frames == 0) break;
        offset += frames;
    }
    for (const auto& track : tracks) keepReference |= track->retainsSyncReference();
    if (! keepReference) clock.endSync();
    // Unity below -1.94 dBFS; a smooth knee reserves output headroom on loud
    // sums. Stereo-linked gain preserves the image, with peak strictly < .98.
    // No normalization based on track count or lookahead.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float peak = 0;
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
            {
                auto& value = outputs[ch][sample];
                if (! std::isfinite(value)) value = 0;
                peak = std::max(peak, std::abs(value));
            }
        if (peak > .8f)
        {
            const float excess = peak - .8f;
            const float gain = (.8f + .18f * (excess / (.18f + excess))) / peak;
            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr) outputs[ch][sample] *= gain;
        }
    }
}
