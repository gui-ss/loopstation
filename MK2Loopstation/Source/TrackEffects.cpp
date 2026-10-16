#include "TrackEffects.h"

static_assert(std::atomic<uint64_t>::is_always_lock_free, "FX controls must be lock-free");

TrackEffects::TrackEffects()
{
    lpf.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    prepare(48000); // Allocate the two filter channel states before any callback.
}

void TrackEffects::setParameters(TrackFxParameters value) noexcept
{
    parameters.store(value.packed(), std::memory_order_relaxed);
}

void TrackEffects::prepare(double rate) noexcept
{
    rate = std::isfinite(rate) && rate > 0 ? rate : 48000.0;
    maxCutoff = static_cast<float>(rate * 0.45);
    lpf.prepare({ rate, 1, 2 }); // Same two channels: no resizing during playback.
    toneFilter.prepare({ rate, 1, 2 });
    cutoff.reset(rate, 0.04);
    resonance.reset(rate, 0.04);
    wet.reset(rate, 0.02);
    distortionWet.reset(rate, 0.02);
    drive.reset(rate, 0.04);
    tone.reset(rate, 0.04);
    distortionLevel.reset(rate, 0.04);
    distortionBlend.reset(rate, 0.04);
    beginBlock();
    cutoff.setCurrentAndTargetValue(cutoff.getTargetValue());
    resonance.setCurrentAndTargetValue(resonance.getTargetValue());
    wet.setCurrentAndTargetValue(wet.getTargetValue());
    distortionWet.setCurrentAndTargetValue(distortionWet.getTargetValue());
    drive.setCurrentAndTargetValue(drive.getTargetValue());
    tone.setCurrentAndTargetValue(tone.getTargetValue());
    distortionLevel.setCurrentAndTargetValue(distortionLevel.getTargetValue());
    distortionBlend.setCurrentAndTargetValue(distortionBlend.getTargetValue());
    reset();
}

void TrackEffects::reset() noexcept
{
    lpf.reset();
    toneFilter.reset();
    coefficientCountdown = toneCountdown = 0;
    appliedCutoff = appliedQ = -1;
    appliedTone = -1;
}

void TrackEffects::beginBlock() noexcept
{
    const auto value = parameters.load(std::memory_order_relaxed);
    cutoff.setTargetValue(juce::jmin(maxCutoff, TrackFxParameters::cutoffHz(static_cast<int>(value & 255u))));
    resonance.setTargetValue(TrackFxParameters::resonanceQ(static_cast<int>((value >> 8u) & 255u)));
    wet.setTargetValue((value & (1u << 16u)) != 0 ? 1.0f : 0.0f);
    distortionWet.setTargetValue((value & (1u << 17u)) != 0 ? 1.0f : 0.0f);
    drive.setTargetValue(TrackFxParameters::driveGain(static_cast<int>((value >> 18u) & 127u)));
    tone.setTargetValue(juce::jmin(maxCutoff, TrackFxParameters::toneHz(static_cast<int>((value >> 25u) & 127u))));
    distortionLevel.setTargetValue(static_cast<float>((value >> 32u) & 127u) * 0.005f);
    distortionBlend.setTargetValue(static_cast<float>((value >> 39u) & 127u) * 0.01f);
    lpf.snapToZero();
    toneFilter.snapToZero();
}

void TrackEffects::processFrame(float& left, float& right) noexcept
{
    processLpf(left, right);
    processDistortion(left, right);
}

void TrackEffects::processLpf(float& left, float& right) noexcept
{
    const float hz = cutoff.getNextValue(), q = resonance.getNextValue();
    // Bounded control-rate coefficient updates; never build/allocate IIR coefficients.
    if (--coefficientCountdown <= 0)
    {
        if (hz != appliedCutoff) { lpf.setCutoffFrequency(hz); appliedCutoff = hz; }
        if (q != appliedQ) { lpf.setResonance(q); appliedQ = q; }
        coefficientCountdown = 16;
    }
    // Keep the filter warm in bypass. Compensation prevents resonance gain boost.
    const float compensation = 0.5f / q;
    float l = lpf.processSample(0, std::isfinite(left) ? left : 0) * compensation;
    float r = lpf.processSample(1, std::isfinite(right) ? right : 0) * compensation;
    if (! std::isfinite(l) || ! std::isfinite(r)) { reset(); l = r = 0; }
    l = juce::jlimit(-1.0f, 1.0f, l);
    r = juce::jlimit(-1.0f, 1.0f, r);
    const float mix = wet.getNextValue();
    if (mix != 0)
    {
        left += mix * (l - left);
        right += mix * (r - right);
    }
}

void TrackEffects::processDistortion(float& left, float& right) noexcept
{
    const float toneFrequency = tone.getNextValue();
    if (--toneCountdown <= 0)
    {
        if (toneFrequency != appliedTone) { toneFilter.setCutoffFrequency(toneFrequency); appliedTone = toneFrequency; }
        toneCountdown = 16;
    }
    const float distortionMix = distortionWet.getNextValue() * distortionBlend.getNextValue();
    const float preGain = drive.getNextValue(), postGain = distortionLevel.getNextValue();
    const float dryLeft = std::isfinite(left) ? left : 0.0f, dryRight = std::isfinite(right) ? right : 0.0f;
    // Warm TONE even in bypass. Saturation precedes a non-resonant tone filter;
    // LEVEL bounds the wet peak to 0.5 maximum (0.25 at the default setting).
    float distortedLeft = toneFilter.processSample(0, distortion.processSample(preGain * dryLeft));
    float distortedRight = toneFilter.processSample(1, distortion.processSample(preGain * dryRight));
    if (! std::isfinite(distortedLeft) || ! std::isfinite(distortedRight))
    { toneFilter.reset(); distortedLeft = distortedRight = 0; }
    distortedLeft = juce::jlimit(-1.0f, 1.0f, distortedLeft) * postGain;
    distortedRight = juce::jlimit(-1.0f, 1.0f, distortedRight) * postGain;
    if (distortionMix == 0) return; // Exact bypass/MIX=0; LPF remains independent.
    left = dryLeft + distortionMix * (distortedLeft - dryLeft);
    right = dryRight + distortionMix * (distortedRight - dryRight);
}
