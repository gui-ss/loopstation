#pragma once
#include "ReverbParameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

static_assert(std::atomic<uint64_t>::is_always_lock_free, "REVERB requires lock-free controls");

// Stereo feedback-delay-network reverb (8 lines, Householder feedback matrix) with
// pre-delay and Schroeder all-pass input diffusion. An approximation, not BOSS DSP.
// prepare() allocates; processFrame()/beginBlock() never allocate or lock.
class ReverbEffect final
{
public:
    ReverbEffect();
    void setParameters(ReverbParameters p) noexcept { parameters.store(p.packed(), std::memory_order_relaxed); }
    void prepare(double sampleRate);
    void reset() noexcept;
    void beginBlock() noexcept;
    bool active() const noexcept { return enable.getTargetValue() > 0 || enable.getCurrentValue() > 0; }
    void processFrame(float& left, float& right) noexcept;

private:
    static constexpr int lines = 8, stages = 4, controlInterval = 16;
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z[2][2] {};
    };
    using Linear = juce::SmoothedValue<float>;
    using Log = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;
    void clear() noexcept;
    void updateControl() noexcept;
    static void design(Biquad&, float hz, bool highpass, float rate) noexcept;
    static float run(Biquad&, int channel, float x) noexcept;

    std::atomic<uint64_t> parameters { ReverbParameters{}.packed() };
    uint64_t applied = ~uint64_t(0);
    float rate = 48000;
    std::array<std::vector<float>, 2> preBuffer;
    int preSize = 0, preWrite = 0;
    std::array<std::vector<float>, lines> delay;
    std::array<int, lines> delayPos {};
    std::array<float, lines> damped {}, gain {}, inject {}, tap {};
    std::array<std::array<std::vector<float>, stages>, 2> diffuser;
    std::array<std::array<int, stages>, 2> diffuserPos {};
    float diffuserGain = .45f, dampCoefficient = .5f, wetScale = 1.f;
    Biquad lowCutFilter, highCutFilter;
    int control = 0;
    bool dirty = false;
    Log timeSeconds, lowHz, highHz;
    Linear preDelay, density, dryGain, wetGain, enable, lowMix, highMix;
};
