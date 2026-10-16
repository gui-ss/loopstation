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
        // Constant-Q bands collect more broadband energy as frequency rises.
        b.inverseBandwidth = 1.f / (80.f * std::pow(2.f, float(i) / bandScale));
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
    // Remove rectifier ripple from the MODULATOR, not bass from the audio.
    // Two poles suppress F0-rate envelope modulation, especially on low voices.
    envelopeSmoothing = 1.f - std::exp(-1.f / (.004f * sampleRate));
    unvoicedSmoothing = 1.f - std::exp(-1.f / (.008f * sampleRate));
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
    for (auto& band : analysis)
    {
        band.z1.fill(0); band.z2.fill(0); band.envelope.fill(0);
        band.smooth1.fill(0); band.smooth2.fill(0);
    }
    unvoiced.fill(0); unvoicedTarget.fill(0); analysisCountdown = 0;
    partials.fill({}); partialCountdown = 0;
    phase = 0;
}

void RobotEffect::beginBlock(int enabledOverride) noexcept
{
    auto p = parameters.load(std::memory_order_relaxed);
    if(enabledOverride>=0) p=(p & ~uint32_t(1)) | uint32_t(enabledOverride!=0);
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

void RobotEffect::updatePartials(float hz, float source, float blend) noexcept
{
    // Harmonic density grows as 1/NOTE frequency; compensate energy, not bass.
    const float densityGain = .65f * std::sqrt(hz / 261.625565f);
    auto* partial = partials.data();
    const auto* positions = harmonicPosition.data();
    const auto* weights = pulseWeight.data();
    const auto* bank = analysis.data();
    for (int h = 0; h < harmonics; ++h)
    {
        float target[2] {};
        const float position = source + positions[h];
        if (position >= 0 && position < bands - 1 && float(h + 1) * hz < maxFrequency)
        {
            const int index = int(position);
            const float fraction = position - float(index);
            const int coarse = index & ~1, nextCoarse = (index + 1) & ~1;
            float edge = position < 1.f ? position : 1.f;
            const float top = float(bands - 1) - position;
            const float nyquist = (maxFrequency - float(h + 1) * hz) / (maxFrequency * .15f);
            if (top < edge) edge = top;
            if (nyquist < edge) edge = nyquist;
            for (size_t ch = 0; ch < 2; ++ch)
            {
                // MODE 1: coarse 12-band envelope / impulse-like carrier.
                // MODE 2: fine 24-band envelope / softened pulse carrier.
                const float fine = bank[index].smooth2[ch] * (1 - fraction) + bank[index + 1].smooth2[ch] * fraction;
                const float broad = .5f * ((bank[coarse].smooth2[ch] + bank[coarse + 1].smooth2[ch]) * (1 - fraction)
                    + (bank[nextCoarse].smooth2[ch] + bank[nextCoarse + 1].smooth2[ch]) * fraction);
                target[ch] = edge * densityGain * (broad * (1 - blend) + fine * weights[h] * blend);
            }
        }
        for (size_t ch = 0; ch < 2; ++ch)
            partial[h].step[ch] = (target[ch] - partial[h].value[ch]) / 16.f;
    }
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
            b.smooth1[ch] += envelopeSmoothing * (b.envelope[ch] - b.smooth1[ch]);
            b.smooth2[ch] += envelopeSmoothing * (b.smooth1[ch] - b.smooth2[ch]);
        }
    // Spectral flatness separates broadband consonants/breath from pitched
    // material. It is NOT an F0 tracker. Preserve noise-like input instead of
    // forcing every transient onto a buzzy periodic carrier. No parallel dry bus.
    if (--analysisCountdown <= 0)
    {
        analysisCountdown = 32;
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float sum = 0, logs = 0;
            for (const auto& b : analysis)
            {
                const float energy = b.smooth2[ch] * b.smooth2[ch] * b.inverseBandwidth + 1.e-15f;
                sum += energy; logs += std::log(energy);
            }
            const float flatness = std::exp(logs / bands) / (sum / bands);
            unvoicedTarget[ch] = sum > 1.e-10f ? std::clamp((flatness - .45f) / .3f, 0.f, 1.f) : 0.f;
        }
    }
    const float hz = frequency.getNextValue();
    const float source = spectrumPosition.getNextValue() - shift.getNextValue();
    const float blend = mode.getNextValue();
    // Spectral mapping varies slowly; interpolate amplitudes at audio rate.
    // Bounded 16-frame work spacing, independent of the driver's block size.
    if (--partialCountdown <= 0)
    {
        updatePartials(hz, source, blend);
        partialCountdown = 16;
    }
    float result[2] {};
    auto* partial = partials.data();
    const auto* table = sine.data();
    for (int h = 0; h < harmonics; ++h)
    {
        const double cycle = phase * (h + 1); // non-negative and < 64
        const float tablePosition = float(cycle - int(cycle)) * float(tableSize);
        const int cell = int(tablePosition) < tableSize ? int(tablePosition) : tableSize - 1;
        const float oscillator = table[cell] + (tablePosition - float(cell)) * (table[cell + 1] - table[cell]);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            partial[h].value[ch] += partial[h].step[ch];
            result[ch] += oscillator * partial[h].value[ch];
        }
    }
    phase += double(hz) / sampleRate;
    phase -= std::floor(phase);
    const float amount = wet.getNextValue();
    for (size_t ch = 0; ch < 2; ++ch)
    {
        unvoiced[ch] += unvoicedSmoothing * (unvoicedTarget[ch] - unvoiced[ch]);
        result[ch] = result[ch] * (1 - unvoiced[ch]) + input[ch] * unvoiced[ch];
    }
    // Stereo-linked safety knee, linear at ordinary levels. Unlike an always-on
    // tanh this does not add distortion/aliasing to every synthesized vowel.
    const float peak = std::max(std::abs(result[0]), std::abs(result[1]));
    if (peak > .55f)
    {
        const float excess = peak - .55f;
        const float gain = (.55f + .25f * excess / (.25f + excess)) / peak;
        result[0] *= gain; result[1] *= gain;
    }
    left += amount * (result[0] - left); right += amount * (result[1] - right);
    if (amount == 0) reset();
}
