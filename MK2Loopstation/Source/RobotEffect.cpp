#include "RobotEffect.h"

static_assert(std::atomic<uint32_t>::is_always_lock_free, "Robot controls must be lock-free");

RobotEffect::RobotEffect()
{
    for (int i = 0; i <= tableSize; ++i)
        sine[size_t(i)] = std::sin(juce::MathConstants<float>::twoPi * float(i) / float(tableSize));
    prepare(48000);
}

float RobotEffect::Band::analyse(float x, size_t ch) noexcept
{
    const float y = b0 * x + z1[ch];
    z1[ch] = b1 * x - a1 * y + z2[ch];
    z2[ch] = b2 * x - a2 * y;
    return y;
}

void RobotEffect::prepare(double rate) noexcept
{
    sampleRate = float(std::isfinite(rate) && rate >= 8000 ? rate : 48000);
    maxFrequency = std::min(8000.f, .42f * sampleRate);
    bandScale = float(bands - 1) / std::log2(maxFrequency / 80.f);
    for (int i = 0; i < bands; ++i)
    {
        const auto c = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(
            sampleRate, 80.f * std::pow(2.f, float(i) / bandScale), 4.f);
        auto& b = analysis[size_t(i)];
        b.b0 = c[0] / c[3]; b.b1 = c[1] / c[3]; b.b2 = c[2] / c[3];
        b.a1 = c[4] / c[3]; b.a2 = c[5] / c[3];
    }
    for (int h = 1; h <= harmonics; ++h)
    {
        harmonicPosition[size_t(h - 1)] = std::log2(float(h)) * bandScale;
        // 25%-duty pulse harmonic pattern, pre-emphasised for vocal articulation.
        pulseWeight[size_t(h - 1)] = 1.41421356f * std::sin(juce::MathConstants<float>::pi * float(h) * .25f)
                                   / std::pow(float(h), .15f);
    }
    attack = 1.f - std::exp(-1.f / (.003f * sampleRate));
    release = 1.f - std::exp(-1.f / (.018f * sampleRate));
    frequency.reset(sampleRate, .02); spectrumPosition.reset(sampleRate, .02);
    shift.reset(sampleRate, .03); mode.reset(sampleRate, .02); wet.reset(sampleRate, .02);
    applied = ~uint32_t(0); beginBlock();
    frequency.setCurrentAndTargetValue(frequency.getTargetValue());
    spectrumPosition.setCurrentAndTargetValue(spectrumPosition.getTargetValue());
    shift.setCurrentAndTargetValue(shift.getTargetValue());
    mode.setCurrentAndTargetValue(mode.getTargetValue());
    wet.setCurrentAndTargetValue(0); // Fade in after startup/reconnection.
    reset();
}

void RobotEffect::reset() noexcept
{
    for (auto& band : analysis) { band.z1.fill(0); band.z2.fill(0); band.envelope.fill(0); }
    phase = 0;
}

void RobotEffect::beginBlock() noexcept
{
    const auto p = parameters.load(std::memory_order_relaxed);
    // Also restore the target after prepare() starts the bypass envelope at zero.
    wet.setTargetValue((p & 1u) ? 1.f : 0.f);
    if (p == applied) return;
    applied = p;
    const float hz = RobotParameters::frequency(int((p >> 1u) & 15u));
    frequency.setTargetValue(hz);
    spectrumPosition.setTargetValue(std::log2(hz / 80.f) * bandScale);
    shift.setTargetValue(float(int((p >> 5u) & 127u) - 50) * (.75f / 50.f) * bandScale);
    mode.setTargetValue(float((p >> 12u) & 1u));
}

void RobotEffect::processFrame(float& left, float& right) noexcept
{
    if (! std::isfinite(left)) left = 0;
    if (! std::isfinite(right)) right = 0;
    if (! wet.isSmoothing() && wet.getCurrentValue() == 0) return; // Exact dry, no idle analysis/synthesis.
    const float input[2] { std::clamp(left, -1.f, 1.f), std::clamp(right, -1.f, 1.f) };
    for (auto& b : analysis)
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float magnitude = std::abs(b.analyse(input[ch], ch));
            b.envelope[ch] += (magnitude > b.envelope[ch] ? attack : release) * (magnitude - b.envelope[ch]);
        }
    const float hz = frequency.getNextValue();
    const float source = spectrumPosition.getNextValue() - shift.getNextValue();
    const float blend = mode.getNextValue();
    float result[2] {};
    for (int h = 1; h <= harmonics && float(h) * hz < maxFrequency; ++h)
    {
        const float position = source + harmonicPosition[size_t(h - 1)];
        if (position < 0 || position >= bands - 1) continue;
        const int index = int(position);
        const float fraction = position - float(index);
        const double cycle = phase * h;
        const float tablePosition = float(cycle - std::floor(cycle)) * float(tableSize);
        const auto cell = std::min(size_t(tablePosition), size_t(tableSize - 1));
        const float oscillator = sine[cell] + (tablePosition - float(cell)) * (sine[cell + 1] - sine[cell]);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float fine = analysis[size_t(index)].envelope[ch] * (1 - fraction)
                             + analysis[size_t(index + 1)].envelope[ch] * fraction;
            // MODE 1: coarse 12-channel envelope / impulse-like harmonic carrier.
            // MODE 2: 24-channel envelope / softened 25%-duty pulse carrier.
            const auto coarse = size_t(index & ~1), nextCoarse = size_t((index + 1) & ~1);
            const float broad = .5f * ((analysis[coarse].envelope[ch] + analysis[coarse + 1].envelope[ch]) * (1 - fraction)
                + (analysis[nextCoarse].envelope[ch] + analysis[nextCoarse + 1].envelope[ch]) * fraction);
            const float edge = std::min({ 1.f, position, float(bands - 1) - position,
                                          (maxFrequency - float(h) * hz) / (maxFrequency * .15f) });
            result[ch] += edge * oscillator * (broad * (1 - blend) + fine * pulseWeight[size_t(h - 1)] * blend);
        }
    }
    phase += double(hz) / sampleRate;
    phase -= std::floor(phase);
    const float amount = wet.getNextValue();
    // Bounded, symmetric wet output; no dry mix except the short bypass crossfade.
    const float l = .8f * std::tanh(result[0] * .65f / .8f);
    const float r = .8f * std::tanh(result[1] * .65f / .8f);
    left += amount * (l - left); right += amount * (r - right);
    if (amount == 0) reset();
}
