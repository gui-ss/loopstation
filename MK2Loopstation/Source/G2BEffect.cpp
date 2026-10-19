#include "G2BEffect.h"
#include <cmath>

namespace
{
constexpr double twoPi = 6.283185307179586;
// Smooth ceiling: identity below 0.8, bounded by 0.98 above. Dry audio is never touched by it.
inline float protect(float x) noexcept
{
    const float a = std::abs(x);
    if (a <= .8f) return x;
    const float e = a - .8f;
    return std::copysign(.8f + .18f * e / (.18f + e), x);
}
}

G2BEffect::G2BEffect() { prepare(48000); }

G2BEffect::Biquad G2BEffect::lowpass(double hz, double sampleRate) noexcept
{
    const double w = twoPi * std::min(hz, .45 * sampleRate) / sampleRate, c = std::cos(w);
    const double alpha = std::sin(w) / (2 * .70710678), a0 = 1 + alpha;
    Biquad b;
    b.b0 = float((1 - c) / 2 / a0); b.b1 = float((1 - c) / a0); b.b2 = b.b0;
    b.a1 = float(-2 * c / a0); b.a2 = float((1 - alpha) / a0);
    return b;
}

void G2BEffect::prepare(double sampleRate)
{
    rate = std::isfinite(sampleRate) && sampleRate > 8000 ? sampleRate : 48000;
    enable.reset(rate, .020); balance.reset(rate, .010); modeMix.reset(rate, .015);
    trackLp = lowpass(350, rate); m1Lp = lowpass(260, rate); m2Lp = lowpass(900, rate);
    m1Dc.r = m2Dc.r = float(1 - twoPi * 30 / rate);
    envAttack = float(1 - std::exp(-1 / (.001 * rate))); envRelease = float(1 - std::exp(-1 / (.030 * rate)));
    grainSamples = .036 * rate; maxDelay = .5 * grainSamples; baseDelay = .006 * rate;
    line.assign(size_t(.060 * rate + 16), 0.f); // Covers two periods at 45 Hz + 6 ms base delay.
    p = G2BParameters::unpack(packed.load(std::memory_order_relaxed));
    enable.setCurrentAndTargetValue(0);
    balance.setCurrentAndTargetValue(p.balance * .01f);
    modeMix.setCurrentAndTargetValue(p.mode == 2 ? 1.f : 0.f);
    clearState(); dirty = false;
}

void G2BEffect::clearState() noexcept
{
    trackLp.clear(); m1Lp.clear(); m2Lp.clear(); m1Dc.clear(); m2Dc.clear();
    envelope = previous = 0; high = haveCross = flip = false; count = lastCross = period = 0;
    validSamples = 0; writePos = 0; phase = 0; maxDelay = .018 * rate;
}

void G2BEffect::reset() noexcept { clearState(); dirty = false; enable.setCurrentAndTargetValue(0); }

void G2BEffect::beginBlock(int enabledOverride) noexcept
{
    p = G2BParameters::unpack(packed.load(std::memory_order_relaxed));
    const bool on = enabledOverride < 0 ? p.enabled : enabledOverride != 0;
    enable.setTargetValue(on ? 1.f : 0.f);
    balance.setTargetValue(p.balance * .01f);
    modeMix.setTargetValue(p.mode == 2 ? 1.f : 0.f);
}

float G2BEffect::readLine(double delay) const noexcept
{
    const double size = double(line.size());
    delay = std::clamp(delay, 1.0, size - 3);
    if (delay > double(validSamples - 1)) return 0.f; // Unwritten history after O(1) reset.
    double pos = double(writePos) - delay;
    if (pos < 0) pos += size;
    const int i0 = int(pos); const float f = float(pos - i0);
    const int i1 = i0 + 1 < int(line.size()) ? i0 + 1 : 0;
    return line[size_t(i0)] + f * (line[size_t(i1)] - line[size_t(i0)]);
}

void G2BEffect::processFrame(float& left, float& right) noexcept
{
    if (! std::isfinite(left)) left = 0;
    if (! std::isfinite(right)) right = 0;
    if (! active()) { if (dirty) { clearState(); dirty = false; } return; }
    dirty = true;
    const float x = .5f * (left + right);

    // Pitch tracker on a 350 Hz low-passed copy: Schmitt trigger, rising-edge intervals.
    const float y = trackLp.run(x), a = std::abs(y);
    envelope += (a > envelope ? envAttack : envRelease) * (a - envelope);
    const float h = std::max(.1f * envelope, 2.e-4f);
    count += 1;
    if (! high && y > h)
    {
        high = true;
        const double t = count - double(std::clamp((y - h) / std::max(y - previous, 1.e-9f), 0.f, 1.f));
        if (haveCross)
        {
            const double iv = t - lastCross;
            if (iv > rate / 500 && iv < rate / 45)
                period = period > 0 && std::abs(iv - period) < .25 * period ? period + .3 * (iv - period) : iv;
        }
        haveCross = true; lastCross = t; flip = ! flip;
    }
    else if (high && y < -h) high = false;
    previous = y;

    // MODE 1: square sub-octave following the voice level.
    const float m1 = m1Dc.run(m1Lp.run((flip ? 1.f : -1.f) * envelope * .9f));

    // MODE 2: both wrap distance AND head separation must be whole input periods.
    // Previously only head 2 was aligned while head 1 jumped an unrelated 18 ms;
    // the window modulation then dominated the intended f/2 (146.83 -> 63.7 Hz).
    // For a stable period P, span=2P, separation=P and each delay grows by .5
    // sample/sample. Thus both wraps are phase coherent at the desired octave.
    line[size_t(writePos)] = x;
    validSamples=std::min(validSamples+1,int(line.size()));
    const double targetSpan=period>0 ? 2*period : .018*rate;
    // Bound read-head acceleration on pitch changes; no abrupt delay jumps.
    maxDelay+=std::clamp(targetSpan-maxDelay,-.05,.05);
    phase += .5/maxDelay; if(phase>=1) phase-=1;
    const double d1=baseDelay+phase*maxDelay;
    double ph2=phase+.5; if(ph2>=1) ph2-=1;
    const double d2=baseDelay+ph2*maxDelay;
    const float s1 = float(std::sin(3.141592653589793 * phase)), w1 = s1 * s1, w2 = 1 - w1;
    const float m2 = m2Dc.run(m2Lp.run(w1 * readLine(d1) + w2 * readLine(d2)));
    if (++writePos >= int(line.size())) writePos = 0;

    const float mm = modeMix.getNextValue();
    const float wet = protect(m1 + mm * (m2 - m1));
    const float b = balance.getNextValue(), e = enable.getNextValue();
    left += e * (left + b * (wet - left) - left);
    right += e * (right + b * (wet - right) - right);
}
