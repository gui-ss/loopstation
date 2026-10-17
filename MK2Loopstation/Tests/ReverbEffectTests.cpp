#include "ReverbEffect.h"
#include "OscBotEffect.h"
#include "InputFxEditor.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace
{
using Buffer = std::vector<float>;
struct Pair { Buffer l, r; };
constexpr double pi2 = 6.283185307179586;

void check(bool ok, const char* m) { if (! ok) throw std::runtime_error(std::string("REVERB: ") + m); }

// Test-only "We Drive Alone" scenario (INPUT FX A-D). Never applied to user slots.
ReverbParameters preset()
{
    ReverbParameters p; p.enabled = true; p.time = 30; p.preDelay = 0; p.density = 4;
    p.dLevel = 100; p.lowCut = 0; p.highCut = ReverbParameters::cutSteps; p.eLevel = 64;
    return p;
}
Pair impulse(size_t n, bool left = true)
{ Pair p { Buffer(n, 0.f), Buffer(n, 0.f) }; (left ? p.l : p.r)[0] = 1.f; return p; }
Pair noise(size_t n, int seed, float gain = .25f)
{
    juce::Random random(seed); Pair p { Buffer(n), Buffer(n) };
    for (size_t i = 0; i < n; ++i) { p.l[i] = gain * (random.nextFloat() * 2 - 1); p.r[i] = gain * (random.nextFloat() * 2 - 1); }
    return p;
}
Pair process(ReverbParameters p, const Pair& in, double rate = 48000, int block = 64)
{
    ReverbEffect fx; fx.setParameters(p); fx.prepare(rate);
    Pair out { Buffer(in.l.size()), Buffer(in.l.size()) };
    for (size_t n = 0; n < in.l.size(); ++n)
    {
        if (n % size_t(block) == 0) fx.beginBlock();
        float l = in.l[n], r = in.r[n]; fx.processFrame(l, r);
        check(std::isfinite(l) && std::isfinite(r), "non-finite output");
        out.l[n] = l; out.r[n] = r;
    }
    return out;
}
double energy(const Buffer& b, size_t from, size_t to)
{ double e = 0; for (size_t i = from; i < to && i < b.size(); ++i) e += double(b[i]) * b[i]; return e; }
double rms(const Buffer& b, size_t from) { return std::sqrt(energy(b, from, b.size()) / double(b.size() - from)); }
double maxDiff(const Buffer& a, const Buffer& b)
{ double m = 0; for (size_t i = 0; i < a.size(); ++i) m = std::max(m, double(std::abs(a[i] - b[i]))); return m; }
double tone(const Buffer& b, size_t from, size_t n, double hz, double rate)
{
    const double c = 2 * std::cos(pi2 * hz / rate); double s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i) { const double s0 = b[from + i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
int cutIndex(float hz)
{
    for (size_t i = 0; i < ReverbParameters::cutFrequencies.size(); ++i)
        if (ReverbParameters::cutFrequencies[i] == hz) return int(i);
    return -1;
}

void testParametersAndPacking()
{
    ReverbParameters d;
    check(d.time == 30 && d.preDelay == 500 && d.density == 5 && d.dLevel == 100 && d.lowCut == 0
          && d.highCut == ReverbParameters::cutSteps && d.eLevel == 50 && ! d.enabled, "manual defaults");
    check(ReverbParameters::cutFrequencies.front() == 20.f && ReverbParameters::cutFrequencies.back() == 12500.f, "cut range");
    ReverbParameters p; p.enabled = true; p.time = 100; p.preDelay = 500; p.density = 10; p.dLevel = 0;
    p.lowCut = 29; p.highCut = 0; p.eLevel = 100;
    const auto u = ReverbParameters::unpack(p.packed());
    check(u.enabled && u.time == 100 && u.preDelay == 500 && u.density == 10 && u.dLevel == 0
          && u.lowCut == 29 && u.highCut == 0 && u.eLevel == 100, "pack round trip");
    ReverbParameters bad; bad.time = -5; bad.preDelay = 9999; bad.density = 99; bad.eLevel = 500;
    const auto c = ReverbParameters::unpack(bad.packed());
    check(c.time == 1 && c.preDelay == 500 && c.density == 10 && c.eLevel == 100, "clamping");
}

void testTimeAndTail()
{
    auto run = [](int t) { auto p = preset(); p.time = t; p.dLevel = 0; p.eLevel = 100; return process(p, impulse(48000 * 4)); };
    const auto shortT = run(3), longT = run(60);
    check(energy(longT.l, 48000, 60000) > 100 * energy(shortT.l, 48000, 60000), "TIME changes tail length");
    const auto t2 = run(20);
    const double early = energy(t2.l, 14400, 26400), late = energy(t2.l, 144000, 156000);
    check(early > 1e-8, "tail exists");
    check(late < early * 1e-3, "tail keeps decaying in silence");
}

void testPreDelay()
{
    auto p = preset(); p.preDelay = 100; p.dLevel = 0; p.eLevel = 100;
    const auto delayed = process(p, impulse(48000));
    check(energy(delayed.l, 0, 4700) < 1e-12 && energy(delayed.r, 0, 4700) < 1e-12, "PRE DELAY silences the start");
    check(energy(delayed.l, 4800, 24000) > 1e-8, "reverb starts after PRE DELAY");
    p.preDelay = 0;
    check(energy(process(p, impulse(48000)).l, 1500, 4700) > 1e-8, "PRE DELAY=0 reverberates early");
    p.preDelay = 500; p.dLevel = 100; p.eLevel = 0;
    check(process(p, impulse(48000)).l[0] == 1.f, "PRE DELAY never delays the dry signal");
}

void testDensity()
{
    auto count = [](int d)
    {
        auto p = preset(); p.density = d; p.dLevel = 0; p.eLevel = 100;
        const auto o = process(p, impulse(24000)); int n = 0;
        for (size_t i = 2400; i < 12000; ++i) n += std::abs(o.l[i]) > 1e-4f;
        return n;
    };
    check(count(10) > count(1) * 13 / 10, "DENSITY changes echo density");
}

void testFiltersAndLevels()
{
    const auto in = noise(96000, 1);
    auto base = preset(); base.time = 10; base.dLevel = 0; base.eLevel = 100;
    auto lowBand = [](const Buffer& b)
    {
        Buffer o; for (size_t i = 24000; i + 480 < b.size(); i += 480)
        { double s = 0; for (size_t k = 0; k < 480; ++k) s += b[i + k]; o.push_back(float(s / 480)); }
        return rms(o, 0);
    };
    auto highBand = [](const Buffer& b)
    { Buffer d; for (size_t i = 24001; i < b.size(); ++i) d.push_back(b[i] - b[i - 1]); return rms(d, 0); };
    const auto flat = process(base, in);
    auto lc = base; lc.lowCut = cutIndex(1000.f) + 1;
    check(lowBand(process(lc, in).l) < lowBand(flat.l) * .5, "LOW CUT attenuates lows");
    auto hc = base; hc.highCut = cutIndex(1000.f);
    check(highBand(process(hc, in).l) < highBand(flat.l) * .5, "HIGH CUT attenuates highs");

    auto cut = preset(); cut.dLevel = 100; cut.eLevel = 0; cut.lowCut = 29; cut.highCut = 0;
    check(maxDiff(process(cut, in).l, in.l) < 1e-6, "filters never touch the dry signal");

    auto dryOnly = preset(); dryOnly.dLevel = 100; dryOnly.eLevel = 0;
    check(maxDiff(process(dryOnly, in).l, in.l) < 1e-6, "D=100/E=0 is dry only");
    auto half = dryOnly; half.dLevel = 50;
    Buffer expected = in.l; for (auto& v : expected) v *= .5f;
    check(maxDiff(process(half, in).l, expected) < 1e-6, "D.LEVEL scales only dry");
    auto wetOnly = preset(); wetOnly.dLevel = 0; wetOnly.eLevel = 100;
    const auto w = process(wetOnly, in);
    check(energy(w.l, 0, w.l.size()) > 1e-3, "D=0/E=100 outputs reverb");
    auto both = preset(); both.dLevel = 100; both.eLevel = 100;
    const auto sum = process(both, in);
    Buffer residual(in.l.size()); for (size_t i = 0; i < residual.size(); ++i) residual[i] = sum.l[i] - in.l[i];
    check(maxDiff(residual, w.l) < 1e-4, "dry is not duplicated; E.LEVEL scales only wet");
    auto e50 = preset(); e50.dLevel = 100; e50.eLevel = 50;
    const auto h = process(e50, in);
    for (size_t i = 0; i < residual.size(); ++i) residual[i] = (h.l[i] - in.l[i]) - .5f * w.l[i];
    double worst = 0; for (float v : residual) worst = std::max(worst, double(std::abs(v)));
    check(worst < 1e-4, "E.LEVEL linearity");
}

void testStereo()
{
    auto p = preset(); p.dLevel = 0; p.eLevel = 100;
    const auto left = process(p, impulse(48000, true));
    check(energy(left.l, 0, 48000) > 1e-8 && energy(left.r, 0, 48000) > 1e-8, "left input reaches both channels");
    check(maxDiff(left.l, left.r) > 1e-6, "channels are decorrelated");
    const auto right = process(p, impulse(48000, false));
    check(energy(right.l, 0, 48000) > 1e-8 && energy(right.r, 0, 48000) > 1e-8, "right input reaches both channels");
}

void testParameterChangesAreSmooth()
{
    ReverbEffect fx; auto p = preset(); fx.setParameters(p); fx.prepare(48000);
    Buffer out(48000 * 3);
    for (size_t n = 0; n < out.size(); ++n)
    {
        if (n == 72000)
        { p.time = 90; p.density = 9; p.preDelay = 300; p.lowCut = 12; p.highCut = 20; p.dLevel = 40; p.eLevel = 90; fx.setParameters(p); }
        if (n % 64 == 0) fx.beginBlock();
        float l = .3f * float(std::sin(pi2 * 220 * double(n) / 48000)), r = l;
        fx.processFrame(l, r); out[n] = l;
    }
    double before = 0, after = 0;
    for (size_t n = 48001; n < 72000; ++n) before = std::max(before, double(std::abs(out[n] - out[n - 1])));
    for (size_t n = 72001; n < 120000; ++n) after = std::max(after, double(std::abs(out[n] - out[n - 1])));
    check(after < before * 3 + .05, "parameter changes are click-free");
}

void testRatesAndBlocks()
{
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int block : { 1, 64, 512 })
        {
            const auto o = process(preset(), impulse(size_t(rate * 1.5)), rate, block);
            double peak = 0; for (float v : o.l) peak = std::max(peak, double(std::abs(v)));
            check(peak < 4, "bounded output");
            check(energy(o.l, size_t(rate * .2), size_t(rate * .5)) > 1e-8, "tail at every sample rate");
        }
}

void testIndependentInstances()
{
    auto a = preset(), b = preset(); b.time = 80; b.density = 9; b.highCut = 10;
    const auto in = noise(24000, 7);
    const auto aloneA = process(a, in);
    ReverbEffect fa, fb; fa.setParameters(a); fb.setParameters(b); fa.prepare(48000); fb.prepare(48000);
    Buffer together(in.l.size());
    for (size_t n = 0; n < in.l.size(); ++n)
    {
        if (n % 64 == 0) { fa.beginBlock(); fb.beginBlock(); }
        float l = in.l[n], r = in.r[n], l2 = l, r2 = r; fa.processFrame(l, r); fb.processFrame(l2, r2); together[n] = l;
    }
    check(maxDiff(together, aloneA.l) == 0, "instances keep independent state");
}

#if defined(_MSC_VER) && defined(_DEBUG)
bool inReverb = false; int allocations = 0;
int allocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{ if (inReverb && type == _HOOK_ALLOC) ++allocations; return true; }
#endif
void testNoAllocationInProcess()
{
    ReverbEffect fx; fx.setParameters(preset()); fx.prepare(48000);
#if defined(_MSC_VER) && defined(_DEBUG)
    const auto previous = _CrtSetAllocHook(allocationHook);
    inReverb = true;
#endif
    auto p = preset();
    for (int n = 0; n < 48000; ++n)
    {
        if (n % 64 == 0) { p.time = 10 + n % 90; p.density = 1 + n % 10; p.preDelay = n % 500; fx.setParameters(p); fx.beginBlock(); }
        float l = .1f, r = .1f; fx.processFrame(l, r);
    }
#if defined(_MSC_VER) && defined(_DEBUG)
    inReverb = false; _CrtSetAllocHook(previous);
    check(allocations == 0, "no allocation while processing");
#endif
}

void testEditor()
{
    InputFxEditor e; e.open = true; e.selectSlot(3);
    e.slots[3].oscBot.note = 55;
    check(e.field(0).maximum == 6, "FX TYPE includes REVERB");
    e.setField(0, 6);
    check(e.selectedType() == 6 && e.field(0).text == "REVERB", "select REVERB");
    check(e.field(1).name == "TIME" && e.field(1).text == "3.0s", "TIME default");
    check(e.field(2).name == "PRE DELAY" && e.field(2).text == "500ms", "PRE DELAY default");
    check(e.field(3).name == "DENSITY" && e.field(3).text == "5", "DENSITY default");
    e.navigate(1);
    check(e.field(0).name == "D.LEVEL" && e.field(0).text == "100", "D.LEVEL");
    check(e.field(1).name == "LOW CUT" && e.field(1).text == "FLAT", "LOW CUT flat");
    check(e.field(2).name == "HIGH CUT" && e.field(2).text == "FLAT", "HIGH CUT flat");
    check(e.field(3).name == "E.LEVEL" && e.field(3).text == "50", "E.LEVEL");
    e.setField(3, 64); e.setField(2, 18);
    check(e.slots[3].reverb.eLevel == 64 && e.slots[3].reverb.highCut == 18, "page 2 edits");
    e.navigate(-1); e.setField(1, 30); e.setField(2, 0); e.setField(3, 4);
    const auto& r = e.slots[3].reverb;
    check(r.time == 30 && r.preDelay == 0 && r.density == 4, "page 1 edits");
    check(e.slots[0].reverb.eLevel == 50 && e.slots[0].reverb.density == 5, "slots are independent");
    check(e.slots[3].oscBot.note == 55, "other effect values preserved");
    check(e.pageCount() == 2, "two pages");
}

// A+B OSC BOT mixed as AudioSystem does, then D=REVERB (We Drive Alone scenario).
Buffer oscChain(bool both, size_t n)
{
    OscBotParameters a; a.enabled = true; a.note = 60; a.balance = 100;
    OscBotParameters b = a; b.note = 67; b.enabled = both;
    OscBotEffect fa, fb; fa.setParameters(a); fb.setParameters(b); fa.prepare(48000); fb.prepare(48000);
    fa.beginBlock(); fb.beginBlock();
    OscBotEffect* voices[2] { &fa, &fb }; Buffer out(n);
    for (size_t i = 0; i < n; ++i)
    {
        float l = .3f * float(std::sin(pi2 * 220 * double(i) / 48000)), r = l, dl = 0, dr = 0, w = 0;
        for (auto* v : voices) { float vl = l, vr = r; v->processFrame(vl, vr); dl += vl - l; dr += vr - r; w += v->wetWeight(); }
        out[i] = l + dl / std::max(1.f, w);
    }
    return out;
}
void testTwoOscBotsWithReverb()
{
    const size_t n = 48000 * 4;
    const auto both = oscChain(true, n), single = oscChain(false, n);
    auto wetOnly = preset(); wetOnly.dLevel = 0;
    const auto w = process(wetOnly, { both, both });
    const auto mix = process(preset(), { both, both });
    Buffer residual(n); for (size_t i = 0; i < n; ++i) residual[i] = mix.l[i] - both[i];
    check(maxDiff(residual, w.l) < 1e-4, "dry OSC BOT mix is not duplicated");
    const auto wa = process(wetOnly, { single, single });
    const size_t from = 48000 * 2, len = 48000;
    const double ref = tone(w.l, from, len, 300, 48000);
    check(tone(w.l, from, len, 261.63, 48000) > 3 * ref, "reverb contains OSC BOT A");
    check(tone(w.l, from, len, 392.0, 48000) > 3 * ref, "reverb contains OSC BOT B");
    check(tone(w.l, from, len, 392.0, 48000) > 3 * tone(wa.l, from, len, 392.0, 48000), "B adds harmony beyond A alone");
    double peak = 0; for (float v : mix.l) peak = std::max(peak, double(std::abs(v)));
    check(peak < 4, "bounded combined output");
}
}

void runReverbEffectTests()
{
    testParametersAndPacking(); testTimeAndTail(); testPreDelay(); testDensity(); testFiltersAndLevels();
    testStereo(); testParameterChangesAreSmooth(); testRatesAndBlocks(); testIndependentInstances();
    testNoAllocationInProcess(); testEditor(); testTwoOscBotsWithReverb();
    std::cout << "PASS: REVERB parameters, TIME/tail, PRE DELAY, DENSITY, cuts, D/E level, stereo, smoothing, rates, editor, 2 OSC BOT\n";
}
