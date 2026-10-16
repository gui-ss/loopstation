#pragma once
#include "OscBotParameters.h"
#include "FxSequence.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// Input-driven band-limited subtractive synthesizer; not proprietary BOSS DSP.
class OscBotEffect final
{
public:
    OscBotEffect();
    void setParameters(OscBotParameters p) noexcept { parameters.store(p.packed(), std::memory_order_relaxed); }
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void beginBlock(int enabledOverride = -1) noexcept;
    void setSequence(const FxSequenceParameters& p) noexcept { sequence.setParameters(p); }
    void beginSequenceBlock(double beatsPerSample, double secondsPerSample) noexcept
    { sequence.beginBlock(beatsPerSample, secondsPerSample, OscBotParameters::unpack(applied).enabled); }
    void phraseStart() noexcept { sequence.phraseStart(); }
    void startGroup(double seconds) noexcept { sequence.startGroup(seconds); }
    void alignActivation(double seconds) noexcept { sequence.alignActivation(seconds); }
    bool sequenceSynced() const noexcept { return sequence.synced(); }
    float wetWeight() const noexcept { return wet.getCurrentValue(); }
    bool active() const noexcept { return wet.getTargetValue()>0 || wet.getCurrentValue()>0; }
    void processFrame(float& left, float& right) noexcept;
private:
    friend struct AudioSystemTestAccess;
    FxSequence sequence;
    int manualNote=36, soundingNote=-1;
    struct Tables;
    const Tables* tables; // Shared immutable bank, constructed before audio starts.
    struct Channel
    {
        double phase[3] {};
        float detector = 0, amplitude = 0, low1 = 0, low2 = 0;
        float coefficient = 0, coefficientStep = 0;
    };
    Channel channels[2];
    float rate = 48000, detectorAttack = 0, detectorRelease = 0, release = 0;
    int controlCountdown = 0;
    juce::SmoothedValue<float> frequency, toneRatio, attackCoefficient, modulation, balance, wet;
    std::array<juce::SmoothedValue<float>, 5> waveMix;
    std::atomic<uint64_t> parameters { OscBotParameters{}.packed() };
    uint64_t applied = ~uint64_t(0);
    float readWave(int wave, double phase, float hz) const noexcept;
};
