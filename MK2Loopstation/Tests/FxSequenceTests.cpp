#include "FxSequence.h"
#include "MusicalClock.h"
#include "OscBotEffect.h"
#include "InputFxEditor.h"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
}
void runFxSequenceTests()
{
    for (double rate:{44100.,48000.})
    {
        MusicalClock clock; clock.prepare(rate); clock.setBpm(165);
        FxSequence a,b; FxSequenceParameters pa,pb;
        pa.enabled=pb.enabled=true; pa.rate=pb.rate=102; pa.maximum=9; pb.maximum=6;
        const int na[]={60,58,61,65,63,65,68,67,70}, nb[]={80,79,82,80,82,85};
        for(int i=0;i<9;++i) pa.notes[size_t(i)]=na[i];
        for(int i=0;i<6;++i) pb.notes[size_t(i)]=nb[i];
        a.setParameters(pa); b.setParameters(pb);
        // User presets, a full A cycle plus one step, irregular driver blocks.
        const int total=int(std::ceil(rate*60/165*8*10));
        for(int base=0;base<total;)
        {
            const int frames=std::min(257,total-base); clock.process(frames);
            const auto timing=clock.blockTiming();
            a.beginBlock(timing.beatsPerSample,timing.secondsPerSample,true);
            b.beginBlock(timing.beatsPerSample,timing.secondsPerSample,true);
            a.alignActivation(-1); b.alignActivation(-1);
            for(int n=0;n<frames;++n)
            {
                const auto step=int(std::floor((base+n)*165./(rate*60*8)+1.e-8));
                check(a.nextNote(36)==na[step%9],"165 BPM A: step/order/wrap/sample boundary");
                check(b.nextNote(72)==nb[step%6],"165 BPM B: independent step/order/wrap");
            }
            base+=frames;
        }
    }
    FxSequence seq; FxSequenceParameters p; p.enabled=true; p.maximum=3;
    p.notes[0]=48; p.notes[1]=60; p.notes[2]=72; seq.setParameters(p);
    MusicalClock clock; clock.prepare(48000); clock.setBpm(120); clock.process(12000);
    auto timing=clock.blockTiming(); seq.beginBlock(timing.beatsPerSample,timing.secondsPerSample,true);
    for(int i=0;i<12000;++i) check(seq.nextNote(36)==48,"quarter note half step");
    clock.setBpm(240); clock.process(6000); timing=clock.blockTiming();
    seq.beginBlock(timing.beatsPerSample,timing.secondsPerSample,true);
    for(int i=0;i<6000;++i) check(seq.nextNote(36)==48,"BPM change lost phase");
    check(seq.nextNote(36)==60,"BPM change did not adjust step duration");
    // SYNC OFF ignores loop events; ON cues at an audio-thread phrase event.
    seq.phraseStart(); check(seq.nextNote(36)==60,"SYNC OFF was reset by phrase");
    p.sync=true; seq.setParameters(p); seq.beginBlock(1./12000,1./48000,true);
    seq.phraseStart(); check(seq.nextNote(36)==48,"SYNC ON did not cue step 1");
    p.sync=false; p.retrig=true; seq.setParameters(p);
    seq.beginBlock(1./12000,1./48000,false); seq.alignActivation(-1);
    for(int i=0;i<15000;++i) seq.nextNote(36);
    seq.beginBlock(1./12000,1./48000,true); seq.alignActivation(-1);
    check(seq.nextNote(36)==48,"RETRIG without loop did not restart");
    seq.beginBlock(1./12000,1./48000,false);
    seq.beginBlock(1./12000,1./48000,true); seq.alignActivation(.3);
    check(seq.nextNote(36)==60,"RETRIG did not align to current phrase position");
    p.enabled=false; seq.setParameters(p); seq.beginBlock(1./12000,1./48000,true);
    check(seq.nextNote(55)==55,"SW OFF did not restore manual NOTE");
    // Every documented straight note and numeric endpoint; no block-size dependency.
    for(int r:{0,50,100,101,102,103,104,105,106,107,108})
    {
        FxSequence s; p.enabled=true;p.rate=r;p.maximum=2; s.setParameters(p);
        s.beginBlock(1./24000,1./48000,true);
        const double period=r<=100?48000/FxSequenceParameters::numericHz(r):24000*FxSequenceParameters::stepBeats(r);
        for(int n=0;n<int(std::ceil(period*2))+1;++n)
            check(s.nextNote(36)==(int(std::floor(n/period+1.e-8))%2==0?48:60),"RATE step duration or wrap");
        p.maximum=1;s.setParameters(p);s.beginBlock(1./24000,1./48000,true);
        for(int n=0;n<1000;++n) check(s.nextNote(36)==48,"MAX=1 escaped its note");
    }
    // DSP waveform must track sequence NOTE, rather than just publishing values.
    OscBotEffect effect; OscBotParameters osc; osc.enabled=true;osc.balance=100;osc.attack=0;
    effect.setParameters(osc); effect.prepare(48000);
    p={};p.enabled=true;p.maximum=2;p.notes[0]=60;p.notes[1]=72;p.rate=105;
    effect.setSequence(p);effect.beginBlock();effect.beginSequenceBlock(1./24000,1./48000);effect.alignActivation(-1);
    int crossings[2]{};float previous=0,peak=0;
    for(int n=0;n<48000;++n)
    {
        float l=.2f,r=.1f;effect.processFrame(l,r);
        check(std::isfinite(l)&&std::abs(l)<.7f,"sequenced OSC peak/nonfinite");
        if(n%24000>=4000 && previous<=0 && l>0) ++crossings[n/24000];
        previous=l;peak=std::max(peak,std::abs(l));
    }
    check(peak>.01f && crossings[1]>crossings[0]*1.9 && crossings[1]<crossings[0]*2.1,"sequence NOTE not changing oscillator pitch");
    InputFxEditor editor;editor.open=true;editor.setField(0,5);
    check(editor.pageCount()==9,"sequence pages missing");
    editor.page=2;editor.setField(0,1);editor.setField(1,1);editor.setField(2,1);
    check(editor.field(3).text=="NOTE","sequence TARGET");
    editor.page=3;editor.setField(0,102);editor.setField(1,9);editor.setField(2,1);
    check(editor.field(0).text=="2MEAS" && editor.multi,"RATE/MULTI editor");
    for(int page=4;page<8;++page) {editor.page=page;for(int k=0;k<4;++k) editor.setField(k,48+(page-4)*4+k);}
    editor.selectSlot(1);editor.setField(0,5);editor.page=4;editor.setField(0,80);
    editor.selectSlot(0);editor.page=7;check(editor.field(3).value==63,"slot A step 16 lost");
    check(editor.slots[0].sequence.maximum==9 && editor.slots[1].sequence.notes[0]==80,"slot sequence isolation");
    std::cout<<"PASS: FX SEQUENCE user presets 165 BPM/2MEAS exact sample order/wrap at 44.1/48 kHz; clock/BPM, SW/SYNC/RETRIG, rates/MAX, real octave change, 16 LCD values\n";
}
