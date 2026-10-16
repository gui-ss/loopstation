#pragma once
#include "TrackFxParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// Shared stereo slot processors. Control thread writes targets; audio thread owns DSP.
// TRACK FX A / LPF followed by TRACK FX C / DISTORTION.
class TrackEffects final
{
public:
    TrackEffects();
    void setParameters(TrackFxParameters) noexcept;
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void beginBlock() noexcept;
    void processFrame(float& left, float& right) noexcept;
    void processLpf(float& left, float& right) noexcept;
    void processDistortion(float& left, float& right) noexcept;
private:
    juce::dsp::StateVariableTPTFilter<float> lpf;
    juce::dsp::WaveShaper<float> distortion { [](float x) { return std::tanh(x); } };
    juce::dsp::FirstOrderTPTFilter<float> toneFilter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff { 1000.0f };
    juce::SmoothedValue<float> resonance { 0.5f }, wet { 0.0f };
    juce::SmoothedValue<float> distortionWet { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> drive { 16.0f }, tone { 4000.0f };
    juce::SmoothedValue<float> distortionLevel { 0.25f }, distortionBlend { 1.0f };
    // Both effects and all parameters travel together; no torn control snapshots.
    std::atomic<uint64_t> parameters { TrackFxParameters{}.packed() };
    float maxCutoff = 21600.0f, appliedCutoff = -1, appliedQ = -1;
    int coefficientCountdown = 0, toneCountdown = 0;
    float appliedTone = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackEffects)
};
