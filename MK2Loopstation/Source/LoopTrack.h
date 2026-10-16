#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cstdint>
#include "LoopStorage.h"
#include "MusicalClock.h"

// One audio-thread-owned phrase. RAM is allocated/touched by the constructor,
// never by process(), prepare(), suspend(), or a transport transition.
class LoopTrack final
{
public:
    enum class State : uint32_t { empty, recording, playing, stopped, overdubbing };
    enum class Command : uint32_t { none, primary, stop, clear, undo };
    enum class SyncWait : uint32_t { none, record, play };
    enum class SyncIssue : uint32_t { none, incompatible, stopFirst, sampleGrid, capacity };
    struct SyncStatus { SyncWait waiting; SyncIssue issue; };
    SyncStatus syncStatus() const noexcept;
    // Audio-thread queries used by the shared transport; never inspect RAM from UI.
    bool needsSyncReference() const noexcept;
    bool retainsSyncReference() const noexcept;
    struct Snapshot
    {
        State state;
        int frames;
        bool canUndo;
        float progress;
        bool awaitingBoundary;
        bool recordingQuantized;
        bool hasPhrase() const { return frames > 0; }
    };
    static constexpr int maximumSeconds = 60;
    static constexpr int maximumFrames = maximumSeconds * 192000;
    explicit LoopTrack(int capacityFrames = maximumFrames);

    // UI producer: bounded mailbox, no waiting or command reordering.
    bool request(Command) noexcept;
    Snapshot snapshot() const noexcept;
    void setVolume(float) noexcept;
    void setQuantizeEnabled(bool enabled) noexcept { quantize.store(enabled, std::memory_order_relaxed); }
    bool quantizeEnabled() const noexcept { return quantize.load(std::memory_order_relaxed); }

    // Audio thread (or device lifecycle after callbacks have stopped).
    void prepare(double sampleRate) noexcept;
    void suspend() noexcept;
    // Optional playbackGains exports pre-fader audio plus its envelope for shared
    // routing. AudioSystem consistently uses this mode; standalone callers keep
    // the original additive, post-fader output contract.
    void process(const float* const* inputs, int numInputs, float* const* outputs,
                 int numOutputs, int numSamples, double barFrames = 0,
                 MusicalClock::SyncBlock sync = {}, float* playbackGains = nullptr) noexcept; // Adds to existing output.

private:
    void finishRecording(bool play) noexcept;
    void startPlayback() noexcept;
    void stop() noexcept;
    void finishOverdub() noexcept;
    void addOverdub(int frame, float left, float right) noexcept;
    void publish() noexcept;
    float sampleAt(int channel, int frame) const noexcept;
    void beginRecording(double barFrames, bool synced = false) noexcept;
    bool compatible(const MusicalClock::SyncBlock&) const noexcept;
    double syncPosition(double bar) const noexcept;
    void requestPlayback(bool hasOutput) noexcept;

    SyncWait syncWait = SyncWait::none;
    SyncIssue syncIssue = SyncIssue::none;
    bool syncBound = false, syncCapture = false, syncPlaying = false, syncSetting = false;
    double pendingBar = 0, recordingStartBar = 0, playbackStartBar = 0, syncedBarFrames = 0;
    std::atomic<uint32_t> publishedSync { 0 };
    LoopStorage buffer;
    State state = State::empty;
    int length = 0, recordLimit = 0, edgeFrames = 0, tailRemaining = 0, tailLength = 1;
    int captureLimit = 0, playAtFrame = 0;
    double recordingBarFrames = 0;
    bool awaitingBoundary = false, recordingQuantized = false;
    double deviceRate = 48000, recordedRate = 48000, position = 0, playbackStep = 1;
    float lastLeft = 0, lastRight = 0, tailLeft = 0, tailRight = 0;
    float lastPlaybackGain = 0; // Exported with pre-fader playback for shared insert routing.
    bool undoAvailable = false, dubWritten = false, undoRampStart = false;
    int undoRemaining = 0;
    float undoLeft = 0, undoRight = 0;
    // Short retrospective fade of the last additions when closing a take.
    struct Addition { int frame = 0; float left = 0, right = 0; };
    std::array<Addition, 64> additions {};
    size_t additionHead = 0, additionCount = 0;
    juce::SmoothedValue<float> dubGain { 0.0f };
    juce::SmoothedValue<float> fader { 0.88f }, transport { 0.0f };
    std::atomic<uint32_t> pending { 0 };
    std::atomic<uint64_t> published { 0 };
    std::atomic<float> volume { 0.88f };
    std::atomic<bool> quantize { false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopTrack)
};
