#include "StepSlicerEffect.h"
#include "MusicalClock.h"
#include "InputFxEditor.h"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>

namespace
{
void check(bool ok,const char* msg) { if(!ok) throw std::runtime_error(std::string("STEP SLICER: ")+msg); }
using Signal=std::vector<float>;
Signal render(StepSlicerParameters p,double sr=48000,int block=256,double bpm=120,double seconds=2)
{
    StepSlicerEffect fx; fx.setParameters(p); fx.prepare(sr);
    MusicalClock clock; clock.prepare(sr); clock.setBpm(bpm);
    Signal out(size_t(sr*seconds));
    for(size_t i=0;i<out.size();)
    {
        const int n=std::min(block,int(out.size()-i)); clock.process(n); const auto t=clock.blockTiming();
        fx.beginBlock(t.beatsPerSample,t.secondsPerSample);
        for(int j=0;j<n;++j,++i)
        {
            float l=.2f,r=-.1f; fx.processFrame(l,r);
            check(std::isfinite(l)&&std::abs(l)<.98f,"unsafe output");
            check(std::abs(r+.5f*l)<1.e-6f,"stereo gain mismatch"); out[i]=l;
        }
    }
    return out;
}
double mean(const Signal& s,int a,int b) { double v=0; for(int i=a;i<b;++i) v+=s[size_t(i)]; return v/(b-a); }
StepSlicerParameters clean()
{ StepSlicerParameters p; p.enabled=true; p.threshold=0;p.gain=0; return p; }
void timingAndSteps()
{
    for(const double sr:{44100.,48000.}) for(const double bpm:{90.,120.,165.})
    {
        auto p=clean();p.rate=StepSlicerParameters::eighthTriplet;
        for(int i=0;i<16;++i) {p.length[size_t(i)]=20+i*5;p.level[size_t(i)]=i*6;}
        auto a=render(p,sr,1,bpm,3),b=render(p,sr,257,bpm,3),c=render(p,sr,1024,bpm,3);
        check(a==b&&b==c,"block size changes sample timing");
        for(int maximum=1;maximum<=16;++maximum)
        {
            p.maximum=maximum;
            StepSlicerEffect fx;fx.setParameters(p);fx.prepare(sr);fx.beginBlock(bpm/(60*sr),1/sr);
            const double frames=sr*60/bpm/3;
            for(int n=0;n<int(frames*(maximum+2));++n)
            {
                check(fx.step()==int(std::floor(n/frames+1.e-8))%maximum,"step order/wrap drift");
                float l=.1f,r=.1f;fx.processFrame(l,r);
            }
        }
    }
    // Every musical RATE and both numeric extremes produce their documented/local period.
    for(int rate: {0,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114})
    {
        auto p=clean();p.rate=rate;StepSlicerEffect fx;fx.setParameters(p);fx.prepare(48000);
        fx.beginBlock(165./(60*48000),1./48000);
        for(int i=0;i<48000;++i) {float l=0,r=0;fx.processFrame(l,r);}
        const double expected=rate<=100?StepSlicerParameters::numericHz(rate):2.75/StepSlicerParameters::stepBeats(rate);
        check(std::abs(fx.position()-std::fmod(expected,16.))<1.e-8,"RATE time mapping");
    }
    auto p=clean();StepSlicerEffect fx;fx.setParameters(p);fx.prepare(48000);
    fx.beginBlock(120./(60*48000),1./48000);
    for(int i=0;i<12000;++i){float l=.1f,r=.1f;fx.processFrame(l,r);}
    check(std::abs(fx.position()-.5)<1.e-9,"initial tempo");
    fx.beginBlock(165./(60*48000),1./48000);
    for(int i=0;i<12000;++i){float l=.1f,r=.1f;fx.processFrame(l,r);}
    check(std::abs(fx.position()-1.1875)<1.e-8,"BPM edit resets phase");
}
void completePatternAndLiveChanges()
{
    auto p=clean();p.rate=100;
    for(int i=0;i<16;++i){p.level[size_t(i)]=i*6;p.length[size_t(i)]=10+5*i;}
    const auto signal=render(p,48000,257,165,2.5);
    // Examine all steps in the second/third repetition, clear of the 1 ms edges.
    for(int cycle=1;cycle<3;++cycle)for(int step=0;step<16;++step)
    {
        const int start=(cycle*16+step)*2400;
        check(std::abs(signal[size_t(start+100)]-.2f*p.level[size_t(step)]*.01f)<1.e-6f,"per-step audio value or pattern wrap");
        const int off=start+int(2400*p.length[size_t(step)]*.01)+100;
        check(signal[size_t(off)]==0,"per-step audio window");
    }
    StepSlicerEffect fx;fx.setParameters(p);fx.prepare(44100);
    for(int n=0;n<44100;++n)
    {
        if(n%137==0)
        {
            p.rate=(n/137)%115;p.maximum=1+(n/137)%16;p.gain=(n/137)%21;p.threshold=-((n/137)%31);
            p.depth=(n/137)%101;p.level[size_t((n/137)%16)]=(n/137)%101;
            fx.setParameters(p);fx.beginBlock(165./(60*44100),1./44100);
        }
        float l=.25f*std::sin(float(n)*.07f),r=l;fx.processFrame(l,r);
        check(std::isfinite(l)&&std::abs(l)<.98f&&l==r,"live edits or mono coherence");
    }
    fx.prepare(48000);fx.beginBlock(120./(60*48000),1./48000);
    for(int i=0;i<1000;++i){float l=0,r=0;fx.processFrame(l,r);check(l==0&&r==0,"sample-rate reset/silence");}
}
void modulationAndCompressor()
{
    auto p=clean();auto a=render(p);
    check(std::abs(mean(a,3000,10000)-.2)<1.e-6 && mean(a,15000,23000)==0,"real slice/window");
    p.length[0]=20;auto shortGate=render(p);check(mean(shortGate,6000,10000)==0,"STEP LENGTH ignored");
    p.length[0]=100; p.level[1]=0;p.level[2]=25;auto levels=render(p);
    check(mean(levels,26000,35000)==0,"STEP LEVEL zero not silent");
    check(std::abs(mean(levels,51000,58000)-.05)<1.e-6,"STEP LEVEL does not scale");
    p.depth=0;auto dry=render(p);check(std::abs(mean(dry,26000,35000)-.2)<1.e-6,"DEPTH zero still gates");
    p.depth=50;auto half=render(p);check(std::abs(mean(half,26000,35000)-.1)<1.e-6,"DEPTH midpoint");
    p.depth=0;p.threshold=-30;auto compressed=render(p);
    check(mean(compressed,30000,40000)<.06,"threshold has no compression");
    p.gain=6;auto louder=render(p);check(std::abs(mean(louder,30000,40000)/mean(compressed,30000,40000)-std::pow(10.,.3))<1.e-5,"makeup gain");
    p.gain=20;auto limited=render(p);for(auto v:limited) check(std::abs(v)<.98f,"gain safety");
    StepSlicerEffect fx;fx.setParameters(clean());fx.prepare(48000);fx.beginBlock(1./24000,1./48000);
    float prev=.2f,maxJump=0;
    for(int n=0;n<60000;++n)
    {
        if(n==17000){auto off=clean();off.enabled=false;fx.setParameters(off);fx.beginBlock(1./24000,1./48000);}
        if(n==30000){fx.setParameters(clean());fx.beginBlock(1./24000,1./48000);}
        float l=.2f,r=.1f;fx.processFrame(l,r);maxJump=std::max(maxJump,std::abs(l-prev));prev=l;
        if(n>19000&&n<30000) check(l==.2f&&r==.1f,"bypass not exact");
    }
    check(maxJump<.005f,"switch/gate discontinuity");
    float l=std::numeric_limits<float>::quiet_NaN(),r=std::numeric_limits<float>::infinity();fx.processFrame(l,r);
    check(std::isfinite(l)&&std::isfinite(r),"nonfinite poisoning");
    std::cout<<"STEP SLICER gate/switch max delta (DC .2)="<<maxJump<<"\n";
}
void mailboxAndEditor()
{
    StepSlicerMailbox box;std::atomic<bool> done{false};
    std::thread writer([&]{ for(int n=0;n<10000;++n){StepSlicerParameters p;p.depth=n%101;p.level.fill(p.depth);box.set(p);}done=true; });
    StepSlicerParameters p;
    while(!done.load()) if(box.read(p)) for(int v:p.level) check(v==p.depth,"torn parameters");
    writer.join();
    InputFxEditor e;e.open=true;e.selectedSlot=2;e.setField(0,7);
    check(e.pageCount()==11 && e.field(1).value==107,"catalog/default RATE");
    for(int i=0;i<16;++i)
    {e.page=2+i/2;e.setField((i%2)*2,i+1);e.setField((i%2)*2+1,100-i);}
    for(int b=1;b<4;++b)
    {e.page=e.pageCount()-1;e.setField(0,b);e.page=0;e.setField(0,7);e.page=2;e.setField(0,20+b);}
    e.page=e.pageCount()-1;e.setField(0,0);
    for(int i=0;i<16;++i)
    {e.page=2+i/2;check(e.field(i%2*2).value==i+1&&e.field(i%2*2+1).value==100-i,"bank/step storage overwritten");}
    for(int b=1;b<4;++b)
    {e.selectBank(b);e.page=2;check(e.field(0).value==20+b,"banks share values");}
    e.selectBank(1);e.page=e.pageCount()-1;e.setField(1,0);
    e.selectBank(0);check(e.multi,"bank B SINGLE changed bank A MULTI");
    e.selectBank(1);check(!e.multi,"bank B SINGLE not retained");
}
}
void runStepSlicerEffectTests()
{
    timingAndSteps();completePatternAndLiveChanges();modulationAndCompressor();mailboxAndEditor();
    std::cout<<"PASS: STEP SLICER rates/steps1-16/length/level/depth/compressor/bypass/stereo/blocks/44.1-48k/BPM/mailbox/banks\n";
}
