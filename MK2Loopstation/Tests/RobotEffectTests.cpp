#include "RobotEffect.h"
#include "InputFxEditor.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
namespace
{
bool processingRobot = false;
int robotAllocations = 0;
#if defined(_MSC_VER) && defined(_DEBUG)
int robotAllocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{ if (processingRobot && type == _HOOK_ALLOC) ++robotAllocations; return true; }
#endif
void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
void frame(RobotEffect& fx, float& l, float& r)
{
    const juce::ScopedNoDenormals noDenormals;
    processingRobot = true; fx.processFrame(l, r); processingRobot = false;
}
std::vector<float> render(RobotParameters p, bool vowel = false, float inputHz = 900)
{
    RobotEffect fx; fx.setParameters(p); fx.prepare(48000); fx.beginBlock();
    std::vector<float> result(16384);
    for (int n = 0; n < 32768; ++n)
    {
        float x = .2f * std::sin(juce::MathConstants<float>::twoPi * inputHz * n / 48000.f);
        if (vowel)
        {
            x = 0;
            for (int h = 1; h <= 24; ++h)
            {
                const float frequency = h * 187.f;
                const float a = std::exp(-std::pow((frequency - 600.f) / 160.f, 2.f))
                    + .6f * std::exp(-std::pow((frequency - 1550.f) / 220.f, 2.f))
                    + .25f * std::exp(-std::pow((frequency - 2600.f) / 300.f, 2.f));
                x += .085f * a * std::sin(juce::MathConstants<float>::twoPi * frequency * n / 48000.f);
            }
        }
        float l = x, r = x; frame(fx, l, r);
        check(l == r && std::isfinite(l) && std::abs(l) <= .8f, "ROBOT mono/stability/output bound failed");
        if (n >= 16384) result[size_t(n - 16384)] = l;
    }
    return result;
}
double power(const std::vector<float>& signal, double hz)
{
    const double angle = juce::MathConstants<double>::twoPi * hz / 48000.;
    const double c = std::cos(angle), s = std::sin(angle);
    double x = 1, y = 0, real = 0, imag = 0;
    for (float sample : signal)
    {
        real += sample * x; imag += sample * y;
        const double next = x * c - y * s; y = y * c + x * s; x = next;
    }
    return (real * real + imag * imag) / double(signal.size() * signal.size());
}
std::array<double, 48> spectrum(const std::vector<float>& signal, int note)
{
    std::array<double, 48> p {};
    for (size_t i = 0; i < p.size(); ++i) p[i] = power(signal, RobotParameters::frequency(note) * (i + 1));
    return p;
}
double centroid(const std::array<double, 48>& p)
{
    double energy = 0, weighted = 0;
    for (size_t i = 0; i < p.size(); ++i) { energy += p[i]; weighted += (i + 1) * p[i]; }
    check(energy > 1.e-6, "ROBOT wet signal too quiet");
    return weighted / energy;
}
void testRobotSignal()
{
    RobotParameters p; p.enabled = true;
    for (int note : { 0, 4, 7, 11 })
    {
        p.note = note; const auto signal = render(p);
        double correct = 0; for (double x : spectrum(signal, note)) correct += x;
        double wrong = 0; for (double x : spectrum(signal, (note + 3) % 12)) wrong += x;
        check(correct > 30 * wrong && correct > 20 * power(signal, 900), "NOTE did not replace input pitch with fixed harmonic grid");
    }
    p.note = 0;
    std::array<double, 3> formants {};
    for (int i = 0; i < 3; ++i)
    {
        p.formant = (i - 1) * 50;
        formants[size_t(i)] = centroid(spectrum(render(p, true), 0));
    }
    check(formants[0] < formants[1] * .85 && formants[2] > formants[1] * 1.2,
          "FORMANT must move spectral shape, not merely volume/pitch");
    p.formant = 0; p.mode = 1; auto a = spectrum(render(p, true), 0);
    p.mode = 2; auto b = spectrum(render(p, true), 0);
    double sumA = 0, sumB = 0, difference = 0;
    for (size_t i = 0; i < a.size(); ++i) { sumA += a[i]; sumB += b[i]; }
    for (size_t i = 0; i < a.size(); ++i) difference += std::abs(a[i] / sumA - b[i] / sumB);
    check(difference > .25, "MODE only changed gain or did not change harmonic structure");
    std::cout << "ROBOT formant spectral centroids (-50/0/+50), in harmonics: " << formants[0] << '/' << formants[1]
              << '/' << formants[2] << "; normalized MODE spectral difference=" << difference << '\n';
    RobotEffect fx;
    for (double rate : { 8000., 44100., 48000., 96000., 192000. })
    {
        p.enabled = true; fx.setParameters(p); fx.prepare(rate); fx.beginBlock();
        for (int n = 0; n < 24000; ++n)
        {
            if (n % 256 == 0)
            {
                p.note = (n / 256) % 12; p.formant = n % 512 ? -50 : 50; p.mode = n % 512 ? 1 : 2;
                processingRobot = true; fx.setParameters(p); fx.beginBlock(); processingRobot = false;
            }
            float l = .3f * std::sin(n * .07f), r = 0; frame(fx, l, r);
            check(std::isfinite(l) && std::abs(l) <= .8f && r == 0, "ROBOT unsafe extremes or stereo crosstalk");
        }
        p.enabled = false; fx.setParameters(p); fx.beginBlock();
        for (int n = 0; n < int(rate * .03); ++n) { float l = .1f, r = -.2f; frame(fx, l, r); }
        float l = .1f, r = -.2f; frame(fx, l, r);
        check(l == .1f && r == -.2f, "ROBOT OFF not exact dry after crossfade");
        p.enabled = true; fx.setParameters(p); fx.reset(); fx.beginBlock();
        for (int n = 0; n < 1024; ++n)
        { l = r = 0; frame(fx, l, r); check(l == 0 && r == 0, "ROBOT carrier leaked on silence"); }
    }
}
void testRobotEditor()
{
    InputFxEditor e; e.open = true; e.selectSlot(1);
    const auto bpf = e.slots[1].values;
    e.setField(0, 4);
    check(e.pageCount() == 2 && e.field(1).name == "NOTE" && e.field(2).minimum == -50 && e.field(3).minimum == 1,
          "ROBOT official parameters/ranges missing");
    e.setField(1, 11); e.setField(2, -50); e.setField(3, 1);
    e.selectSlot(2); e.setField(0, 4); e.setField(2, 50); e.selectSlot(1);
    check(e.field(1).text == "B" && e.field(2).value == -50 && e.field(3).value == 1,
          "ROBOT values not independent per slot");
    e.setField(0, 1); check(e.slots[1].values == bpf, "ROBOT replaced BPF configuration");
    e.setField(0, 4); check(e.field(2).value == -50, "Switching type erased ROBOT parameters");
}
}
void runRobotEffectTests()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    const auto previous = _CrtSetAllocHook(robotAllocationHook);
#endif
    try { testRobotSignal(); testRobotEditor(); }
    catch (...)
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetAllocHook(previous);
#endif
        throw;
    }
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(previous);
#endif
    check(robotAllocations == 0, "Allocation in ROBOT processing/control callback");
    std::cout << "PASS: ROBOT NOTE, FORMANT, MODE, official editor ranges/slot preservation, bypass, silence, stereo, sample rates and no allocations\n";
}
