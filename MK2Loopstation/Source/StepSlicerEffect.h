#pragma once
#include "StepSlicerParameters.h"
#include <juce_dsp/juce_dsp.h>

class StepSlicerEffect
{
public:
    void setParameters(const StepSlicerParameters& value) noexcept { mailbox.set(value); }
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void beginBlock(double beatsPerSample, double secondsPerSample, int enabledOverride=-1) noexcept;
    void processFrame(float& left,float& right) noexcept;
    bool active() const noexcept { return enable.getCurrentValue()>0 || enable.getTargetValue()>0; }
    int step() const noexcept { return int(std::floor(phase+1.e-9)) % p.maximum; }
    double position() const noexcept { return phase; } // Audio-thread diagnostics only.
private:
    StepSlicerMailbox mailbox;
    StepSlicerParameters p;
    juce::SmoothedValue<float> enable,depth,threshold,makeup;
    double rate=48000,phase=0,increment=0,attack=0,release=0;
    float envelope=0,gate=0,gateDelta=1.f/48;
    bool wasOn=false;
};
