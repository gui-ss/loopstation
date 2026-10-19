#pragma once
#include "G2BParameters.h"
#include <atomic>
#include <vector>
#include <juce_dsp/juce_dsp.h>

// GUITAR TO BASS approximation (the BOSS algorithms are not public). Input is summed to mono,
// shifted one octave down and mixed with the direct stereo signal by BALANCE.
//  MODE 1: pitch tracker + flip-flop octave divider (square sub-octave, envelope-following, low-passed).
//  MODE 2: dual-read-head delay-line pitch shifter (ratio 0.5), head spacing locked to the measured
//          period so both heads add in phase; wide-band input keeps vocal articulation.
class G2BEffect
{
public:
    G2BEffect();
    void setParameters(const G2BParameters& value) noexcept { packed.store(value.packed(), std::memory_order_relaxed); }
    void prepare(double sampleRate);
    void reset() noexcept;
    void beginBlock(int enabledOverride = -1) noexcept;
    void processFrame(float& left, float& right) noexcept;
    bool active() const noexcept { return enable.getCurrentValue() > 0 || enable.getTargetValue() > 0; }

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float run(float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void clear() noexcept { z1 = z2 = 0; }
    };
    struct DcBlock
    {
        float r = .995f, x1 = 0, y1 = 0;
        float run(float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
        void clear() noexcept { x1 = y1 = 0; }
    };
    static Biquad lowpass(double hz, double sampleRate) noexcept;
    void clearState() noexcept;
    float readLine(double delay) const noexcept;

    std::atomic<uint32_t> packed { G2BParameters{}.packed() };
    G2BParameters p;
    juce::SmoothedValue<float> enable, balance, modeMix; // modeMix: 0 = MODE 1, 1 = MODE 2
    double rate = 48000;
    bool dirty = false;

    Biquad trackLp, m1Lp, m2Lp;
    DcBlock m1Dc, m2Dc;
    float envelope = 0, envAttack = .1f, envRelease = .001f, previous = 0;
    bool high = false, haveCross = false, flip = false;
    double count = 0, lastCross = 0, period = 0;

    std::vector<float> line;
    int writePos = 0, validSamples = 0;
    double phase = 0, grainSamples = 1, maxDelay = 1, baseDelay = 1;
};
