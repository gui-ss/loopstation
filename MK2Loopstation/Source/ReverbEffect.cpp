#include "ReverbEffect.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float lineMs[8] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 79.3f };
constexpr float diffuserMs[2][4] { { 4.7f, 7.9f, 11.3f, 14.9f }, { 5.3f, 8.6f, 12.1f, 16.1f } };
constexpr float twoPi = 6.28318530718f;
}

ReverbEffect::ReverbEffect() { prepare(48000); }

void ReverbEffect::prepare(double sampleRate)
{
    rate = float(std::isfinite(sampleRate) && sampleRate >= 8000 ? sampleRate : 48000);
    preSize = int(.5f * rate) + 4;
    for (auto& b : preBuffer) b.assign(size_t(preSize), 0.f);
    for (int i = 0; i < lines; ++i)
        delay[size_t(i)].assign(size_t(std::max(8, int(lineMs[i] * .001f * rate))), 0.f);
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < stages; ++s)
            diffuser[size_t(c)][size_t(s)].assign(size_t(std::max(4, int(diffuserMs[c][s] * .001f * rate))), 0.f);
    dampCoefficient = 1.f - std::exp(-twoPi * 6000.f / rate);

    for (auto* v : { &timeSeconds, &lowHz, &highHz }) { v->reset(rate, .05); v->setCurrentAndTargetValue(1000.f); }
    timeSeconds.reset(rate, .15);
    for (auto* v : { &preDelay, &density, &dryGain, &wetGain, &enable, &lowMix, &highMix }) v->reset(rate, .03);
    preDelay.reset(rate, .1);
    density.reset(rate, .1);
    enable.reset(rate, .02);
    applied = ~uint64_t(0);
    beginBlock();
    for (auto* v : { &timeSeconds, &lowHz, &highHz })
        v->setCurrentAndTargetValue(v->getTargetValue());
    for (auto* v : { &preDelay, &density, &dryGain, &wetGain, &enable, &lowMix, &highMix })
        v->setCurrentAndTargetValue(v->getTargetValue());
    reset();
}

void ReverbEffect::clear() noexcept
{
    for (auto& b : preBuffer) std::fill(b.begin(), b.end(), 0.f);
    for (auto& d : delay) std::fill(d.begin(), d.end(), 0.f);
    for (auto& c : diffuser) for (auto& d : c) std::fill(d.begin(), d.end(), 0.f);
    delayPos.fill(0); damped.fill(0.f); preWrite = 0;
    for (auto& c : diffuserPos) c.fill(0);
    for (auto* f : { &lowCutFilter, &highCutFilter }) for (auto& z : f->z) z[0] = z[1] = 0.f;
    dirty = false;
}

void ReverbEffect::reset() noexcept { clear(); control = 0; }

void ReverbEffect::beginBlock() noexcept
{
    const auto packed = parameters.load(std::memory_order_relaxed);
    const auto p = ReverbParameters::unpack(packed);
    enable.setTargetValue(p.enabled ? 1.f : 0.f);
    if (packed == applied) return;
    applied = packed;
    timeSeconds.setTargetValue(float(p.time) / 10.f);
    preDelay.setTargetValue(float(p.preDelay) * .001f * rate);
    density.setTargetValue(float(p.density));
    dryGain.setTargetValue(float(p.dLevel) / 100.f);
    wetGain.setTargetValue(float(p.eLevel) / 100.f);
    // FLAT crossfades the filter out and keeps its last frequency (no retuning clicks).
    lowMix.setTargetValue(p.lowCutFlat() ? 0.f : 1.f);
    if (! p.lowCutFlat()) lowHz.setTargetValue(p.lowCutHz());
    highMix.setTargetValue(p.highCutFlat() ? 0.f : 1.f);
    if (! p.highCutFlat()) highHz.setTargetValue(p.highCutHz());
}

void ReverbEffect::design(Biquad& b, float hz, bool highpass, float sampleRate) noexcept
{
    const float w = twoPi * std::clamp(hz, 10.f, .45f * sampleRate) / sampleRate;
    const float cs = std::cos(w), alpha = std::sin(w) / (2.f * .70710678f), a0 = 1.f + alpha;
    const float k = highpass ? (1.f + cs) * .5f : (1.f - cs) * .5f;
    b.b0 = k / a0; b.b1 = (highpass ? -2.f * k : 2.f * k) / a0; b.b2 = k / a0;
    b.a1 = -2.f * cs / a0; b.a2 = (1.f - alpha) / a0;
}

float ReverbEffect::run(Biquad& b, int c, float x) noexcept
{
    const float y = b.b0 * x + b.z[c][0];
    b.z[c][0] = b.b1 * x - b.a1 * y + b.z[c][1];
    b.z[c][1] = b.b2 * x - b.a2 * y;
    return y;
}

void ReverbEffect::updateControl() noexcept
{
    const float t = std::max(.05f, timeSeconds.skip(controlInterval));
    const float dens = density.skip(controlInterval);
    diffuserGain = .15f + .06f * dens;                    // DENSITY 1..10 => 0.21..0.75
    const float active = 2.f + (dens - 1.f) * 6.f / 9.f;  // 2..8 injected/tapped lines
    float energy[2] {};
    for (int i = 0; i < lines; ++i)
    {
        inject[size_t(i)] = std::clamp(active - float(i), 0.f, 1.f);
        energy[i & 1] += inject[size_t(i)] * inject[size_t(i)];
    }
    float meanSquare = 0;
    for (int i = 0; i < lines; ++i)
    {
        // RT60: -60 dB after t seconds, per line length.
        gain[size_t(i)] = std::exp(-6.9077553f * float(delay[size_t(i)].size()) / (rate * t));
        meanSquare += gain[size_t(i)] * gain[size_t(i)] / float(lines);
        const float n = 1.f / std::sqrt(std::max(energy[i & 1], 1e-6f));
        inject[size_t(i)] *= n;
        tap[size_t(i)] = inject[size_t(i)] * (((i >> 1) & 1) != 0 ? -1.f : 1.f);
    }
    wetScale = .8f * std::sqrt(std::max(.01f, 1.f - meanSquare));
    design(lowCutFilter, lowHz.skip(controlInterval), true, rate);
    design(highCutFilter, highHz.skip(controlInterval), false, rate);
}

void ReverbEffect::processFrame(float& left, float& right) noexcept
{
    if (! std::isfinite(left)) left = 0;
    if (! std::isfinite(right)) right = 0;
    if (! enable.isSmoothing() && enable.getCurrentValue() == 0)
    { if (dirty) clear(); return; }
    dirty = true;
    if (control <= 0) { updateControl(); control = controlInterval; }
    --control;

    const float on = enable.getNextValue(), dry = dryGain.getNextValue(), eff = wetGain.getNextValue();
    const float pd = preDelay.getNextValue(), lm = lowMix.getNextValue(), hm = highMix.getNextValue();
    const float in[2] { left, right };
    float x[2];
    for (int c = 0; c < 2; ++c)
    {
        auto& buffer = preBuffer[size_t(c)];
        buffer[size_t(preWrite)] = in[c];
        float read = float(preWrite) - pd;           // Only the reverb input is delayed.
        if (read < 0) read += float(preSize);
        const int i0 = std::min(int(read), preSize - 1);
        const int i1 = i0 + 1 >= preSize ? 0 : i0 + 1;
        const float frac = read - float(i0);
        float v = buffer[size_t(i0)] + frac * (buffer[size_t(i1)] - buffer[size_t(i0)]);
        for (int s = 0; s < stages; ++s)
        {
            auto& d = diffuser[size_t(c)][size_t(s)];
            int& pos = diffuserPos[size_t(c)][size_t(s)];
            const float delayed = d[size_t(pos)], w = v - diffuserGain * delayed;
            d[size_t(pos)] = w;
            v = delayed + diffuserGain * w;
            if (++pos >= int(d.size())) pos = 0;
        }
        x[c] = v;
    }
    if (++preWrite >= preSize) preWrite = 0;

    float y[lines], sum = 0;
    for (int i = 0; i < lines; ++i)
    {
        const float o = delay[size_t(i)][size_t(delayPos[size_t(i)])];
        damped[size_t(i)] += dampCoefficient * (o - damped[size_t(i)]);
        y[i] = damped[size_t(i)] * gain[size_t(i)];
        sum += y[i];
    }
    const float reflect = sum * (2.f / float(lines)); // Householder: orthogonal, energy-preserving
    float wet[2] {};
    for (int i = 0; i < lines; ++i)
    {
        auto& d = delay[size_t(i)];
        int& pos = delayPos[size_t(i)];
        d[size_t(pos)] = inject[size_t(i)] * x[i & 1] + y[i] - reflect;
        if (++pos >= int(d.size())) pos = 0;
        wet[i & 1] += tap[size_t(i)] * y[i];
    }
    float out[2];
    for (int c = 0; c < 2; ++c)
    {
        float t = wet[c] * wetScale;
        t += lm * (run(lowCutFilter, c, t) - t);   // filters touch only the reverberated signal
        t += hm * (run(highCutFilter, c, t) - t);
        out[c] = t;
    }
    if (! std::isfinite(out[0]) || ! std::isfinite(out[1]))
    { clear(); left = in[0]; right = in[1]; return; }
    // D.LEVEL scales only the incoming signal; E.LEVEL only the reverberated one.
    left = in[0] + on * (dry * in[0] + eff * out[0] - in[0]);
    right = in[1] + on * (dry * in[1] + eff * out[1] - in[1]);
}
