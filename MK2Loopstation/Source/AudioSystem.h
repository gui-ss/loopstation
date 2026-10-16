#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include "LoopTrack.h"
#include "MusicalClock.h"
#include "TrackFxRouting.h"
#include "RobotEffect.h"
#include "OscBotEffect.h"

// Owns device lifetime independently of the panel and the settings window.
// Device operations and status snapshots belong to the message thread.
class AudioSystem final : public juce::ChangeBroadcaster, private juce::AudioIODeviceCallback,
                          private juce::Timer
{
public:
    AudioSystem();
    explicit AudioSystem(std::unique_ptr<juce::AudioDeviceManager>, int capacityFrames = LoopTrack::maximumFrames);
    ~AudioSystem() override;
    void initialise();
    void selectDevices(const juce::String& input, const juce::String& output);
    void reconnect();
    void setMonitoringEnabled(bool);
    bool isMonitoringEnabled() const { return monitoring.load(std::memory_order_relaxed); }
    bool canMonitor() const;
    void setMonitorVolume(float);
    float monitorVolume() const { return volume.load(std::memory_order_relaxed); }
    static constexpr int trackCount = 5;
    // Two stereo float versions per track: ~879 MiB plus ~2 MiB of COW indices
    // at 60 s / 192 kHz. Fixed upper bound, allocated/touched before audio starts.
    static constexpr size_t maximumAudioBytes = size_t(trackCount) * LoopTrack::maximumFrames * 4 * sizeof(float);
    bool storageReady() const { return tracks[0] != nullptr; }
    bool commandTrack(int index, LoopTrack::Command);
    LoopTrack::Snapshot trackSnapshot(int index) const;
    void setTrackVolume(int index, float gain);
    void selectTrack(int index); // Message thread only; never changed by engine updates.
    int selectedTrack() const { return selected; }
    bool undoSelectedTrack() { return commandTrack(selected, LoopTrack::Command::undo); }
    bool commandTrack1(LoopTrack::Command command) { return commandTrack(0, command); }
    LoopTrack::Snapshot track1Snapshot() const { return trackSnapshot(0); }
    void setTrack1Volume(float gain) { setTrackVolume(0, gain); }
    // GUI producer: gates, type masks and group-start serial form ONE release
    // publication. Later UI requests may coalesce before the next callback.
    void setInputActivation(uint32_t mask, uint32_t robotMask, uint32_t oscMask, bool restartPair) noexcept
    {
        const auto old=inputActivation.load(std::memory_order_relaxed);
        const auto serial=(old & ~uint64_t(0xffff)) + (restartPair ? uint64_t(1)<<16 : 0);
        inputActivation.store(serial | 0x1000u | (mask&15u) | ((robotMask&15u)<<4) | ((oscMask&15u)<<8),std::memory_order_release);
    }
    void setInputRobot(int slot, RobotParameters parameters)
    { if (juce::isPositiveAndBelow(slot, 4)) inputRobots[size_t(slot)].setParameters(parameters); }
    void setInputOscBot(int slot, OscBotParameters parameters)
    { if (juce::isPositiveAndBelow(slot, 4)) inputOscBots[size_t(slot)].setParameters(parameters); }
    void setInputSequence(int slot, const FxSequenceParameters& p)
    { if (juce::isPositiveAndBelow(slot,4)) inputOscBots[size_t(slot)].setSequence(p); }
    void setTrackEffects(TrackFxParameters value) { trackFx.setParameters(value); }
    void setTrack1Effects(TrackFxParameters value) { setTrackEffects(value); } // Legacy caller; destinations are independent.
    void setTrackFxInsert(int slot, int target) { trackFx.setInsert(slot, target); }
    int trackFxInsert(int slot) const { return trackFx.insert(slot); }
    void setTrackFxEnabled(int index, bool enabled) { trackFx.setTrackEnabled(index, enabled); }
    bool trackFxEnabled(int index) const { return trackFx.trackEnabled(index); }
    void setQuantizeEnabled(bool enabled) { quantize.store(enabled, std::memory_order_relaxed); }
    bool quantizeEnabled() const { return quantize.load(std::memory_order_relaxed); }
    void setLoopSync(int index, bool enabled);
    void setAllLoopSync(bool enabled);
    bool loopSyncEnabled(int index) const;
    bool allLoopSyncEnabled() const { return syncMask.load(std::memory_order_relaxed) == 31u; }
    LoopTrack::SyncStatus trackSyncStatus(int index) const;
    bool setBpm(double value);
    bool bpmChangeBlocked() const { return rejectedBpm && clock.tempoLocked(); }
    bool bpmLocked() const { return clock.tempoLocked(); }
    double bpm() const { return clock.bpm(); }
    void tapTempo();
    MusicalClock::Position musicalPosition() const { return clock.position(); }
    bool clockRunning() const { return running.load() && ! deviceFailed.load(); }
    juce::StringArray deviceNames(bool input) const;
    const juce::String& selectedInput() const { return inputName; }
    const juce::String& selectedOutput() const { return outputName; }
    const juce::String& statusText() const { return status; }

private:
    friend struct AudioSystemTestAccess; // Tests can service zero-frame commands without the device wrapper.
    void applySelection();
    void releaseUnusedSync() noexcept;
    void updateStatus();
    void optimiseBufferSize();
    void checkBufferStability();
    void processTracks(const float* const*, int, float* const*, int, int, double) noexcept;
    void timerCallback() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                         const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String&) override;

    std::unique_ptr<juce::AudioDeviceManager> manager;
    std::array<std::unique_ptr<LoopTrack>, trackCount> tracks;
    std::atomic<uint64_t> inputActivation{0};
    uint64_t consumedInputActivation=0; // Audio thread only.
    std::array<RobotEffect, 4> inputRobots;
    std::array<OscBotEffect, 4> inputOscBots;
    int sequenceReference=-1;
    uint64_t previousPhraseEpoch=0;
    std::array<std::array<float, 256>, 2> processedInput {}, captureInput {};
    std::array<juce::SmoothedValue<float>, 5> inputFxGate;
    TrackFxRouting trackFx; // One LPF + one DISTORTION shared by all five tracks.
    static constexpr int routingBlockSize = 256;
    std::array<std::array<std::array<float, routingBlockSize>, 3>, trackCount> playbackScratch {};
    int selected = 0;
    std::atomic<bool> quantize { false };
    std::atomic<uint32_t> syncMask { 0 };
    uint32_t blockSyncMask = 0; // Audio-thread block snapshot of individual SW values.
    MusicalClock::SyncBlock blockSync;
    bool rejectedBpm = false; // Message thread only.
    MusicalClock clock;
    TapTempo tap;
    juce::String inputName, outputName, error, status;
    juce::StringArray knownInputs, knownOutputs;
    std::atomic<bool> running { false }, deviceFailed { false };
    std::atomic<bool> monitoring { false };
    std::atomic<float> volume { 0.1f };
    juce::SmoothedValue<float> monitorGain { 0.0f }; // Only touched by audio lifecycle/callback.
    int stableBufferSize = 0, xrunsAtWindowStart = 0;
    double xrunWindowStart = 0;
    bool stabilityFallback = false, publishedMonitoring = false;
    bool initialised = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSystem)
};
