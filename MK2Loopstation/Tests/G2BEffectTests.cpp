#include "G2BEffect.h"
#include "InputFxEditor.h"
#include <algorithm>
#include <chrono>
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
constexpr double pi2 = 6.283185307179586;
void check(bool ok, const char* m) { if (! ok) throw std::runtime_error(std::string("G2B: ") + m); }

struct Out { Buffer l, r; };

// Signal with a voiced-like spectrum: fundamental plus 2nd/3rd harmonics.
float voice(double f, double n, double sr)
{ const double w = pi2 * f * n / sr; return float(.3 * std::sin(w) + .1 * std::sin(2 * w) + .05 * std::sin(3 * w)); }

template <class Gen>
Out run(G2BParameters p, double sr, int block, int frames, Gen gen)
{
    G2BEffect fx; fx.setParameters(p); fx.prepare(sr);
    Out o { Buffer(size_t(frames)), Buffer(size_t(frames)) };
    for (int i = 0; i < frames;)
    {
        const int n = std::min(block, frames - i);
        fx.beginBlock();
        for (int j = 0; j < n; ++j, ++i)
        {
            float l = gen(i), r = gen(i) * .5f; fx.processFrame(l, r);
            check(std::isfinite(l) && std::isfinite(r), "NaN/Inf");
            o.l[size_t(i)] = l; o.r[size_t(i)] = r;
        }
    }
    return o;
}

double tone(const Buffer& b, double hz, double sr, int from, int to)
{
    double re = 0, im = 0;
    for (int i = from; i < to; ++i)
    {
        const double w = pi2 * hz * i / sr, x = b[size_t(i)] * (.5 - .5 * std::cos(pi2 * (i - from) / (to - from)));
        re += x * std::cos(w); im += x * std::sin(w);
    }
    return std::sqrt(re * re + im * im) / (to - from);
}
double rms(const Buffer& b, int from, int to)
{ double s = 0; for (int i = from; i < to; ++i) s += double(b[size_t(i)]) * b[size_t(i)]; return std::sqrt(s / (to - from)); }

G2BParameters on(int balance, int mode) { G2BParameters p; p.enabled = true; p.balance = balance; p.mode = mode; return p; }

// Strongest candidate among a fine grid; verifies the shifted fundamental by measurement.
double dominant(const Buffer& b, double sr, double lo, double hi, int from, int to)
{
    double best = 0, bestHz = 0;
    for (double f = lo; f <= hi; f += .5) { const double a = tone(b, f, sr, from, to); if (a > best) { best = a; bestHz = f; } }
    return bestHz;
}

void testParameters()
{
    G2BParameters d; check(d.balance == 50 && d.mode == 2 && ! d.enabled, "official defaults BALANCE 50 / MODE 2");
    for (int b : { 0, 1, 50, 99, 100 }) for (int m : { 1, 2 })
    { auto p = G2BParameters::unpack(on(b, m).packed()); check(p.enabled && p.balance == b && p.mode == m, "pack/unpack"); }
    G2BParameters bad; bad.balance = 500; bad.mode = 9;
    auto u = G2BParameters::unpack(bad.packed()); check(u.balance == 100 && u.mode == 2, "clamping");
}

void testOctaveDown()
{
    for (double sr : { 44100., 48000. }) for (int mode : { 1, 2 })
    {
        const double inputs[] { 55, 82.41, 110, 146.83, 220, 261.63, 329.63, 440 };
        for (double f : inputs)
        {
            const int frames = int(sr * 1.5), a = int(sr * .6), b = int(sr * 1.4);
            auto o = run(on(100, mode), sr, 256, frames, [&](int n) { return voice(f, n, sr); });
            const double found = dominant(o.l, sr, f * .25, f * 1.4, a, b);
            const double target = f / 2; std::cout << "G2B probe sr=" << sr << " mode=" << mode << " in=" << f << " out=" << found << " amp@found=" << tone(o.l, found, sr, a, b) << " amp@target=" << tone(o.l, target, sr, a, b) << " amp@2target=" << tone(o.l, 2*target, sr, a, b) << "\n";
            const double cents = std::abs(1200 * std::log2(found / target));
            if (mode == 2) check(cents < 60, "MODE 2 fundamental is not one octave below");
            else check(found > 0 && std::abs(found - target) < target * .12, "MODE 1 fundamental is not one octave below");
            const double sub = tone(o.l, target, sr, a, b), orig = tone(o.l, f, sr, a, b);
            check(sub > 2 * orig, "shifted fundamental must dominate the shifted second harmonic");
        }
    }
    // The headline requirement: 220 Hz in, ~110 Hz out.
    const double sr = 48000;
    for (int mode : { 1, 2 })
    {
        auto o = run(on(100, mode), sr, 128, 96000, [&](int n) { return voice(220, n, sr); });
        const double f0 = dominant(o.l, sr, 60, 300, 30000, 90000);
        check(std::abs(f0 - 110) < 6, "220 Hz sine did not produce ~110 Hz");
        std::cout << "G2B MODE " << mode << ": 220 Hz in -> " << f0 << " Hz out\n";
    }
}

void testBalance()
{
    const double sr = 48000; const int N = 48000;
    // Pure sine: a voiced second harmonic legitimately shifts onto the original f0.
    auto gen = [&](int n) { return float(.3 * std::sin(pi2 * 220 * n / sr)); };
    auto wet = run(on(100, 2), sr, 256, N, gen);
    auto dry = run(on(0, 2), sr, 256, N, gen);
    auto mid = run(on(50, 2), sr, 256, N, gen);
    for (int i = 20000; i < N; ++i)
    {
        check(std::abs(dry.l[size_t(i)] - gen(i)) < 1.e-5f && std::abs(dry.r[size_t(i)] - gen(i) * .5f) < 1.e-5f, "BALANCE 0 is not the direct signal");
        check(std::abs(mid.l[size_t(i)] - .5f * (gen(i) + wet.l[size_t(i)])) < 2.e-3f, "BALANCE 50 is not the mix");
    }
    check(tone(wet.l, 220, sr, 24000, 48000) < .15 * tone(wet.l, 110, sr, 24000, 48000), "BALANCE 100 contains duplicated dry audio");
    check(tone(mid.l, 110, sr, 24000, 48000) > .3 * tone(wet.l, 110, sr, 24000, 48000), "BALANCE 50 lacks processed component");
    check(tone(mid.l, 220, sr, 24000, 48000) > .3 * tone(dry.l, 220, sr, 24000, 48000), "BALANCE 50 lacks direct component");
    // Disabled: exact bypass.
    G2BParameters off = on(100, 2); off.enabled = false;
    auto b = run(off, sr, 256, 4800, gen);
    for (int i = 0; i < 4800; ++i) check(b.l[size_t(i)] == gen(i), "disabled G2B is not bit-exact bypass");
}

void testModesDiffer()
{
    const double sr = 48000;
    auto gen = [&](int n) { return voice(196, n, sr); };
    auto a = run(on(100, 1), sr, 256, 72000, gen), b = run(on(100, 2), sr, 256, 72000, gen);
    double diff = 0, ref = 0;
    for (int i = 30000; i < 72000; ++i) { diff += std::pow(a.l[size_t(i)] - b.l[size_t(i)], 2); ref += std::pow(b.l[size_t(i)], 2); }
    check(diff > .25 * ref, "MODE 1 and MODE 2 are not distinct");
    // Different harmonic content: MODE 1 is a divided square (odd harmonics of 98 Hz), MODE 2 keeps the voice spectrum.
    const double h3a = tone(a.l, 294, sr, 30000, 72000) / tone(a.l, 98, sr, 30000, 72000);
    const double h2b = tone(b.l, 196, sr, 30000, 72000) / tone(b.l, 98, sr, 30000, 72000);
    const double h2a = tone(a.l, 196, sr, 30000, 72000) / tone(a.l, 98, sr, 30000, 72000);
    check(h3a > .05 && h2b > h2a, "MODE spectra not characteristically different");
    std::cout << "G2B modes: relative difference=" << diff / ref << " MODE1 h2/h1=" << h2a << " MODE2 h2/h1=" << h2b << "\n";
}

void testSilenceAndSafety()
{
    const double sr = 48000;
    for (int mode : { 1, 2 })
    {
        auto z = run(on(100, mode), sr, 256, 48000, [](int) { return 0.f; });
        for (int i = 0; i < 48000; ++i) check(z.l[size_t(i)] == 0.f && z.r[size_t(i)] == 0.f, "silence produced output");
        // Note changes, hard onsets, noise burst (beatbox-like), loud input.
        auto o = run(on(100, mode), sr, 64, 192000, [&](int n)
        {
            const int seg = n / 24000; const double f[] { 98, 130.8, 82.4, 196, 110, 220, 87.3, 164.8 };
            const double env = (n % 24000) < 400 ? (n % 24000) / 400. : 1.;
            if (seg == 5) return float(((n * 1103515245u + 12345u) >> 16 & 255) / 128. - 1) * .5f * float((n % 24000) < 2000);
            return float(1.8 * env) * voice(f[seg % 8], n, sr);
        });
        double mean = 0, peak = 0; for (float v : o.l) { mean += v; peak = std::max(peak, double(std::abs(v))); }
        mean /= double(o.l.size());
        check(peak <= .99, "processed output clips");
        check(std::abs(mean) < .01, "DC offset");
        double maxStep = 0; for (size_t i = 1; i < o.l.size(); ++i) maxStep = std::max(maxStep, double(std::abs(o.l[i] - o.l[i - 1])));
        check(maxStep < .7, "click/discontinuity");
        // Sub-bass limit: nothing meaningful below 30 Hz.
        check(tone(o.l, 15, sr, 48000, 96000) < .02, "excess subgrave");
        // Invalid samples must not poison state.
        G2BEffect fx; fx.setParameters(on(100, mode)); fx.prepare(sr); fx.beginBlock();
        float l = std::nanf(""), r = INFINITY; fx.processFrame(l, r);
        for (int i = 0; i < 1000; ++i) { l = .2f; r = .2f; fx.processFrame(l, r); check(std::isfinite(l) && std::isfinite(r), "NaN poisoned state"); }
    }
}

void testResponsiveness()
{
    // Beatbox-style articulation: the sub-octave onset must follow the voice within ~60 ms.
    const double sr = 48000;
    for (int mode : { 1, 2 })
    {
        auto o = run(on(100, mode), sr, 128, 48000, [&](int n) { return n < 24000 ? 0.f : voice(147, n, sr); });
        int first = -1; for (int i = 24000; i < 48000; ++i) if (std::abs(o.l[size_t(i)]) > .02f) { first = i - 24000; break; }
        check(first >= 0 && first < int(.060 * sr), "onset too slow");
        check(rms(o.l, 40000, 48000) > .02, "sustained output too weak");
        std::cout << "G2B MODE " << mode << ": onset after " << 1000. * first / sr << " ms, sustained rms " << rms(o.l, 40000, 48000) << "\n";
    }
    // Level tracks dynamics.
    auto soft = run(on(100, 2), sr, 256, 48000, [&](int n) { return .3f * voice(130, n, sr); });
    auto loud = run(on(100, 2), sr, 256, 48000, [&](int n) { return voice(130, n, sr); });
    check(rms(loud.l, 30000, 48000) > 1.8 * rms(soft.l, 30000, 48000), "output ignores voice dynamics");
}

void testBlocksStereoAndStates()
{
    for (double sr : { 44100., 48000., 96000. })
    {
        auto gen = [&](int n) { return voice(180, n, sr); };
        auto a = run(on(100, 2), sr, 1, 20000, gen), b = run(on(100, 2), sr, 480, 20000, gen);
        for (int i = 0; i < 20000; ++i) check(std::abs(a.l[size_t(i)] - b.l[size_t(i)]) < 1.e-5f, "output depends on block size");
        // Right channel carried the same (mono-summed) wet signal: balance 100 yields identical channels.
        for (int i = int(.021 * sr); i < 20000; ++i) check(std::abs(a.l[size_t(i)] - a.r[size_t(i)]) < 1.e-4f, "stereo channels differ at BALANCE 100");
    }
    // Mono-equivalent input (L == R) behaves like stereo with the same sum.
    G2BEffect m, s; m.setParameters(on(100, 2)); s.setParameters(on(100, 2)); m.prepare(48000); s.prepare(48000);
    for (int i = 0; i < 24000; ++i)
    {
        if (i % 128 == 0) { m.beginBlock(); s.beginBlock(); }
        const float x = voice(150, i, 48000);
        float ml = x, mr = x, sl = x * 1.5f, sr2 = x * .5f; m.processFrame(ml, mr); s.processFrame(sl, sr2);
        if (i > 960) check(std::abs(ml - sl) < 1.e-5f, "mono and stereo-sum inputs differ");
    }
    // Parameter changes while running are smooth (no click).
    G2BEffect fx; fx.setParameters(on(0, 1)); fx.prepare(48000);
    float prev = 0, maxStep = 0;
    for (int i = 0; i < 96000; ++i)
    {
        if (i % 64 == 0) { auto p = on((i / 4000) % 2 ? 100 : 0, 1 + (i / 7000) % 2); fx.setParameters(p); fx.beginBlock(); }
        float l = voice(140, i, 48000), r = l; fx.processFrame(l, r);
        if (i > 1) maxStep = std::max(maxStep, std::abs(l - prev)); prev = l;
    }
    check(maxStep < .8f, "click when BALANCE/MODE change");
    // On/off transitions are ramped; reset clears state.
    G2BEffect e; e.setParameters(on(100, 2)); e.prepare(48000);
    prev = 0; maxStep = 0;
    for (int i = 0; i < 48000; ++i)
    {
        if (i % 64 == 0) { auto p = on(100, 2); p.enabled = (i / 8000) % 2 == 0; e.setParameters(p); e.beginBlock(); }
        float l = voice(140, i, 48000), r = l; e.processFrame(l, r);
        if (i > 1) maxStep = std::max(maxStep, std::abs(l - prev)); prev = l;
    }
    check(maxStep < .8f, "click when enabling/disabling");
    e.reset(); e.setParameters(on(100, 2)); e.beginBlock(); float z = 0, zr = 0; e.processFrame(z, zr); check(z == 0, "reset leaves state");
}

#if defined(_MSC_VER) && defined(_DEBUG)
bool inG2B = false; int allocations = 0;
int allocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{ if (inG2B && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations; return true; }
#endif

void testNoAllocationAndCpu()
{
    for (double sr : { 44100., 48000. })
    {
        G2BEffect fx; fx.setParameters(on(100, 2)); fx.prepare(sr);
#if defined(_MSC_VER) && defined(_DEBUG)
        const auto previous = _CrtSetAllocHook(allocationHook); inG2B = true; allocations = 0;
#endif
        const int blocks = 2000, size = 256;
        std::array<double,2000> times{};
        for (int b = 0; b < blocks; ++b)
        {
            auto p = on((b * 7) % 101, 1 + b % 2); fx.setParameters(p);
            const auto t0 = std::chrono::steady_clock::now();
            fx.beginBlock();
            for (int j = 0; j < size; ++j) { float l = voice(150 + (b % 50), b * size + j, sr), r = l; fx.processFrame(l, r); }
            times[size_t(b)]=std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
#if defined(_MSC_VER) && defined(_DEBUG)
        inG2B = false; _CrtSetAllocHook(previous); check(allocations == 0, "allocation inside G2B processing");
#endif
        double total = 0; for (double t : times) total += t;
        std::sort(times.begin(), times.end());
        const double deadline = 1000. * size / sr;
        std::cout << "G2B rate=" << sr << " block=256 meanMs=" << total / blocks << " p99Ms=" << times[size_t(blocks * .99)]
                  << " maxMs=" << times.back() << " deadlineMs=" << deadline << " cpu%=" << 100 * total / blocks / deadline << "\n";
        check(total / blocks < .25 * deadline, "G2B uses too much CPU");
    }
}

void testEditor()
{
    InputFxEditor e; e.open = true;
    check(e.field(0).maximum == 8 && std::string(InputFxEditor::types[8]) == "G2B", "FX TYPE includes G2B");
    check(std::string(InputFxEditor::types[5]) == "OSC BOT" && std::string(InputFxEditor::types[6]) == "REVERB"
          && std::string(InputFxEditor::types[7]) == "STEP SLICER" && std::string(InputFxEditor::types[4]) == "ROBOT", "existing type indices changed");
    e.selectSlot(0); e.setField(0, 5); e.setField(0, 8);
    check(e.selectedType() == 8 && e.field(0).text == "G2B", "FX TYPE G2B");
    check(e.field(1).name == "BALANCE" && e.field(1).value == 50 && e.field(1).maximum == 100, "BALANCE default/range");
    check(e.field(2).name == "MODE" && e.field(2).value == 2 && e.field(2).minimum == 1 && e.field(2).maximum == 2, "MODE default/range");
    check(! e.field(3).available, "G2B exposes unofficial controls");
    e.page = 1; check(e.field(0).name == "BANK", "G2B has exactly one parameter page before BANK/INPUT MODE");
    e.page = 0; e.setField(1, 100); e.setField(2, 1); e.setField(2, 9); e.setField(1, 500);
    check(e.slots[0].g2b.balance == 100 && e.slots[0].g2b.mode == 2, "editing/clamping");
    e.setField(2, 1);
    e.selectSlot(1); e.setField(0, 5); e.setField(0, 6); e.setField(0, 8);
    check(e.slots[1].g2b.balance == 50 && e.slots[1].g2b.mode == 2, "slots are independent");
    e.selectBank(1); e.page=0; e.setField(0, 8); check(e.slots[1].g2b.balance == 50, "banks independent");
    e.selectBank(0); check(e.slots[0].g2b.mode == 1 && e.slots[0].g2b.balance == 100, "bank A preserved");
    e.selectSlot(0); e.setField(0, 5);
    check(e.slots[0].oscBot.osc >= 0 && e.selectedType() == 5, "switching type preserves other settings");
}
}

void runG2BEffectTests()
{
    testParameters(); testOctaveDown(); testBalance(); testModesDiffer(); testSilenceAndSafety();
    testResponsiveness(); testBlocksStereoAndStates(); testNoAllocationAndCpu(); testEditor();
    std::cout << "PASS: G2B octave-down (220->110 Hz, several notes/rates), BALANCE 0/50/100, MODE 1 vs 2, silence, transients, DC/clipping/NaN, blocks, no allocation, CPU, editor\n";
}
