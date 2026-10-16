#include "FxSequence.h"

void FxSequence::beginBlock(double beatsPerSample, double secondsPerSample, bool effectOn) noexcept
{
    mailbox.read(p);
    increment = p.rate <= 100 ? FxSequenceParameters::numericHz(p.rate)*secondsPerSample
                             : beatsPerSample/FxSequenceParameters::stepBeats(p.rate);
    stepsPerSecond = secondsPerSample>0 ? increment/secondsPerSample : 0;
    if (p.enabled && !wasEnabled) phase=0;
    phase=std::fmod(phase,double(p.maximum));
    activation=effectOn && !wasEffectOn && p.enabled && p.retrig;
    wasEffectOn=effectOn; wasEnabled=p.enabled;
}
void FxSequence::alignActivation(double phraseSeconds) noexcept
{
    if (activation) phase=std::fmod(std::max(0.,phraseSeconds)*stepsPerSecond,double(p.maximum));
    activation=false;
}
int FxSequence::nextNote(int manualNote) noexcept
{
    if (!p.enabled) return manualNote;
    // Small epsilon puts exact sample-grid boundaries on that sample, not one later.
    const int index=int(std::floor(phase+1.e-9)) % p.maximum;
    const int note=p.notes[size_t(index)];
    phase+=increment;
    if (phase>=p.maximum) phase-=std::floor(phase/p.maximum)*p.maximum;
    return note;
}
