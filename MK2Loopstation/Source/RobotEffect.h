#pragma once
#include "RobotParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// Low-latency channel vocoder, NOT a reconstruction of proprietary BOSS DSP.
// Fixed analysis bands -> spectral-envelope shift -> fixed-note harmonic synthesis.
class RobotEffect final
{
public:
    RobotEffect();
    void setParameters(RobotParameters p) noexcept { parameters.store(p.packed(), std::memory_order_relaxed); }
    void prepare(double rate) noexcept;
    void reset() noexcept;
    void beginBlock(int enabledOverride = -1) noexcept;
    bool active() const noexcept { return wet.getTargetValue()>0 || wet.getCurrentValue()>0; }
    void processFrame(float& left, float& right) noexcept;
private:
    static constexpr int bands = 24, harmonics = 64, tableSize = 4096;
    struct Band
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float inverseBandwidth = 1;
        std::array<float, 2> z1 {}, z2 {}, envelope {}, smooth1 {}, smooth2 {};
        float analyse(float x, size_t channel) noexcept;
    };
    std::array<Band, bands> analysis;
    std::array<float, tableSize + 1> sine {};
    std::array<float, harmonics> harmonicPosition {}, pulseWeight {};
    struct Partial { float value[2] {}, step[2] {}; };
    std::array<Partial, harmonics> partials {};
    int partialCountdown = 0;
    void updatePartials(float hz, float source, float blend) noexcept;
    float sampleRate = 48000, maxFrequency = 8000, bandScale = 1, attack = 0, release = 0;
    float envelopeSmoothing = 0, unvoicedSmoothing = 0;
    std::array<float, 2> unvoiced {}, unvoicedTarget {};
    int analysisCountdown = 0;
    double phase = 0;
    juce::SmoothedValue<float> frequency { 130.81278f }, spectrumPosition { 0.f };
    juce::SmoothedValue<float> shift { 0.f }, mode { 1.f }, wet { 0.f };
    std::atomic<uint32_t> parameters { RobotParameters{}.packed() };
    uint32_t applied = ~uint32_t(0);
};
