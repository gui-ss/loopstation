#include "StepSlicerEffect.h"

void StepSlicerEffect::prepare(double sampleRate) noexcept
{
    rate=std::isfinite(sampleRate)&&sampleRate>0 ? sampleRate : 48000;
    enable.reset(rate,.020); depth.reset(rate,.010); threshold.reset(rate,.020); makeup.reset(rate,.020);
    attack=std::exp(-1./(.002*rate)); release=std::exp(-1./(.080*rate));
    gateDelta=float(1./std::max(1.,.001*rate));
    mailbox.read(p);
    depth.setCurrentAndTargetValue(p.depth*.01f);
    threshold.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(float(p.threshold)));
    makeup.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(float(p.gain)));
    reset();
}
void StepSlicerEffect::reset() noexcept
{
    phase=0; envelope=gate=0; wasOn=false; enable.setCurrentAndTargetValue(0);
}
void StepSlicerEffect::beginBlock(double beatsPerSample,double secondsPerSample,int enabledOverride) noexcept
{
    mailbox.read(p);
    const bool on=enabledOverride<0?p.enabled:enabledOverride!=0;
    if(on&&!wasOn) phase=0; // Local retrigger only. Never resets MusicalClock or loops.
    wasOn=on;
    enable.setTargetValue(on?1.f:0.f);
    depth.setTargetValue(p.depth*.01f);
    threshold.setTargetValue(juce::Decibels::decibelsToGain(float(p.threshold)));
    makeup.setTargetValue(juce::Decibels::decibelsToGain(float(p.gain)));
    increment=p.rate<=100 ? StepSlicerParameters::numericHz(p.rate)*secondsPerSample
                         : beatsPerSample/StepSlicerParameters::stepBeats(p.rate);
    if(!std::isfinite(increment)||increment<0) increment=0;
    phase=std::fmod(phase,double(p.maximum));
}
void StepSlicerEffect::processFrame(float& left,float& right) noexcept
{
    // Bypass is exact for finite input after the ramp. Invalid upstream data cannot poison state.
    if(!std::isfinite(left)) left=0; if(!std::isfinite(right)) right=0;
    if(!active()) { envelope=gate=0; return; }
    const float peak=std::min(16.f,std::max(std::abs(left),std::abs(right)));
    const double coeff=peak>envelope?attack:release;
    envelope=float(peak+(envelope-peak)*coeff);
    const float th=threshold.getNextValue();
    // Linked peak compressor, fixed 4:1 ratio. Algorithm/times are local approximations.
    // pow(x,.75) = sqrt(x)*sqrt(sqrt(x)): bounded, no log/pow in the per-sample path.
    const float ratio=std::min(1.f,th/std::max(envelope,1.e-9f));
    const float root=std::sqrt(ratio);
    const float compressedGain=root*std::sqrt(root)*makeup.getNextValue();
    const auto index=size_t(step());
    const double fraction=phase-std::floor(phase+1.e-9);
    const float target=fraction<p.length[index]*.01 ? p.level[index]*.01f : 0.f;
    gate+=std::clamp(target-gate,-gateDelta,gateDelta);
    const float mod=1.f-depth.getNextValue()*(1.f-gate), wet=enable.getNextValue();
    float wetL=std::clamp(left,-16.f,16.f)*compressedGain;
    float wetR=std::clamp(right,-16.f,16.f)*compressedGain;
    // Stereo-linked smooth ceiling before gating: preserves image and silent steps.
    const float wetPeak=std::max(std::abs(wetL),std::abs(wetR));
    if(wetPeak>.8f)
    {
        const float excess=wetPeak-.8f;
        const float protection=(.8f+.18f*excess/(.18f+excess))/wetPeak;
        wetL*=protection; wetR*=protection;
    }
    left+=wet*(wetL*mod-left); right+=wet*(wetR*mod-right);
    phase+=increment;
    if(phase>=p.maximum) phase-=std::floor(phase/p.maximum)*p.maximum;
}
