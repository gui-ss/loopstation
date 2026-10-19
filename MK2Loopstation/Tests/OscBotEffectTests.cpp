#include "OscBotEffect.h"
#include "InputFxEditor.h"
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
namespace
{
bool inOsc = false;
int allocations = 0;
#if defined(_MSC_VER) && defined(_DEBUG)
int allocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{ if (inOsc && type == _HOOK_ALLOC) ++allocations; return true; }
#endif
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void frame(OscBotEffect& fx, float& l, float& r)
{
    inOsc = true; fx.processFrame(l, r); inOsc = false;
    check(std::isfinite(l) && std::isfinite(r), "OSC BOT non-finite output");
}
std::vector<float> render(OscBotParameters p, double rate = 48000)
{
    OscBotEffect fx; fx.setParameters(p); fx.prepare(rate); fx.beginBlock();
    std::vector<float> out(32768);
    const int warmup = int(rate * 1.5); // Includes ATTACK=100 settlement.
    for (int n = 0; n < warmup + int(out.size()); ++n)
    {
        float l = .2f, r = 0; frame(fx, l, r);
        check(r == 0 && std::abs(l) < .61f, "OSC BOT channel leakage or unsafe peak");
        if (n >= warmup) out[size_t(n - warmup)] = l;
    }
    return out;
}
struct Spectrum
{
    std::vector<double> power;
    double energy = 0, centroid = 0;
};
Spectrum spectrum(const std::vector<float>& audio)
{
    juce::dsp::FFT fft(15);
    std::vector<float> buffer(65536, 0);
    for (size_t n = 0; n < audio.size(); ++n)
        buffer[n] = audio[n] * float(.5 - .5 * std::cos(juce::MathConstants<double>::twoPi * n / audio.size()));
    fft.performFrequencyOnlyForwardTransform(buffer.data());
    Spectrum result; result.power.resize(16385);
    for (size_t i = 1; i < result.power.size(); ++i)
    {
        result.power[i] = double(buffer[i]) * buffer[i];
        result.energy += result.power[i]; result.centroid += i * result.power[i];
    }
    result.centroid /= result.energy;
    return result;
}
void testSignal()
{
    check(std::abs(OscBotParameters::frequency(24) - 32.7032f) < .001f
        && OscBotParameters::frequency(69) == 440
        && std::abs(OscBotParameters::frequency(127) - 12543.854f) < .02f, "OSC BOT NOTE octave conversion");
    OscBotParameters p; p.enabled = true; p.balance = 100; p.attack = 0; p.tone = 50;
    double worstAlias = 0;
    std::array<std::vector<float>, 5> waveforms;
    for (double rate : { 44100., 48000. })
        for (int note : { 24, 60, 96, 127 })
            for (int wave = 0; wave < 5; ++wave)
            {
                p.note = note; p.osc = wave;
                auto audio = render(p, rate); auto s = spectrum(audio);
                check(s.energy > 1, "OSC BOT silent at supported NOTE");
                const double hz = OscBotParameters::frequency(note);
                const auto peak = std::max_element(s.power.begin(), s.power.end()) - s.power.begin();
                check(std::abs(double(peak) * rate / audio.size() - hz) < hz * .006 + 3, "OSC BOT wrong fundamental");
                if (note >= 96)
                {
                    double residue = 0;
                    for (size_t bin = 1; bin < s.power.size(); ++bin)
                    {
                        bool harmonic = false;
                        for (double detune : { 1., .995965, 1.0040516 })
                        {
                            if (wave != 2 && detune != 1.) continue;
                            const double spacing = hz * detune * audio.size() / rate;
                            const int h = int(std::round(bin / spacing));
                            if (h >= 1 && std::abs(double(bin) - h * spacing) < 5) harmonic = true;
                        }
                        if (!harmonic) residue += s.power[bin];
                    }
                    worstAlias = std::max(worstAlias, residue / s.energy);
                    check(residue / s.energy < .0001, "OSC BOT high-note alias/nonharmonic energy excessive");
                }
                if (note == 60 && rate == 48000) waveforms[size_t(wave)] = std::move(audio);
            }
    for (int a = 0; a < 5; ++a)
        for (int b = a + 1; b < 5; ++b)
        {
            double delta = 0, energy = 0;
            for (size_t i = 0; i < waveforms[size_t(a)].size(); ++i)
            { delta += std::pow(waveforms[size_t(a)][i] - waveforms[size_t(b)][i], 2); energy += std::pow(waveforms[size_t(a)][i], 2); }
            check(delta / energy > .001, "OSC BOT oscillator choices identical");
        }
    p.note = 48; p.osc = 0;
    double colour[3], mod[3];
    for (int i = 0; i < 3; ++i)
    {
        p.tone = (i - 1) * 50; p.modSens = 0; colour[i] = spectrum(render(p)).centroid;
        p.tone = 0; p.modSens = (i - 1) * 50; mod[i] = spectrum(render(p)).centroid;
    }
    check(colour[0] < colour[1] * .9 && colour[2] > colour[1] * 1.1, "OSC BOT TONE not changing spectral shape");
    check(mod[0] < mod[1] * .9 && mod[2] > mod[1] * 1.1, "OSC BOT signed MOD SENS not controlling timbre");
    std::cout << "OSC BOT worst high-note nonharmonic energy=" << worstAlias
        << "; TONE centroids=" << colour[0] << '/' << colour[1] << '/' << colour[2]
        << "; MOD SENS centroids=" << mod[0] << '/' << mod[1] << '/' << mod[2] << '\n';
}
void testEnvelopeAndControls()
{
    for (double rate : { 44100., 48000. })
    {
        OscBotParameters p; p.enabled = true; p.balance = 100;
        double onset[3] {};
        for (int i = 0; i < 3; ++i)
        {
            OscBotEffect fx; p.attack = i * 50; fx.setParameters(p); fx.prepare(rate); fx.beginBlock();
            for (int n = 0; n < int(rate * .1); ++n)
            { float l = 0, r = 0; frame(fx, l, r); check(l == 0 && r == 0, "OSC BOT free-running in silence"); }
            for (int n = 0; n < int(rate * .08); ++n)
            { float l = .2f, r = 0; frame(fx, l, r); onset[i] += l * l; }
            float tail = 0;
            for (int n = 0; n < int(rate); ++n)
            { float l = 0, r = 0; frame(fx, l, r); if (n > int(rate * .95)) tail = std::max(tail, std::abs(l)); }
            check(tail < 1.e-6f, "OSC BOT carrier remained after silence");
        }
        check(onset[0] > onset[1] * 3 && onset[1] > onset[2] * 3, "OSC BOT ATTACK curve ineffective");
        OscBotEffect dry, half, wet, reference;
        p.attack = 0; p.balance = 0; dry.setParameters(p); dry.prepare(rate); dry.beginBlock();
        p.balance = 50; half.setParameters(p); half.prepare(rate); half.beginBlock();
        p.balance = 100; wet.setParameters(p); wet.prepare(rate); wet.beginBlock();
        reference.setParameters(p); reference.prepare(rate); reference.beginBlock();
        for (int n = 0; n < int(rate * .2); ++n)
        {
            const float input = .2f * std::sin(n * .06f);
            float a = input, b = input, c = input, d = input, r = 0;
            frame(dry, a, r); frame(half, b, r); frame(wet, c, r);
            if (n % 31 == 0) reference.beginBlock();
            frame(reference, d, r);
            check(a == input && std::abs(b - (a + c) * .5f) < 1.e-6f && c == d, "OSC BOT BALANCE/block continuity wrong");
        }
        p.enabled = false; wet.setParameters(p); wet.beginBlock();
        for (int n = 0; n < int(rate * .03); ++n) { float l = .1f, r = -.2f; frame(wet, l, r); }
        float l = .1f, r = -.2f; frame(wet, l, r); check(l == .1f && r == -.2f, "OSC BOT bypass not exact");
        p.enabled = true; wet.setParameters(p); wet.beginBlock();
        float maxJump = 0, previous = 0;
        for (int n = 0; n < int(rate * .5); ++n)
        {
            if (n % 256 == 0)
            {
                p.note = (n / 256) % 2 ? 24 : 127; p.osc = (n / 256) % 5;
                p.tone = n % 512 ? -50 : 50; p.modSens = -p.tone;
                inOsc = true; wet.setParameters(p); wet.beginBlock(); inOsc = false;
            }
            l = .1f; r = 0; frame(wet, l, r);
            check(std::abs(l) <= .61f && r == 0, "OSC BOT transitions unsafe");
            maxJump = std::max(maxJump, std::abs(l - previous)); previous = l;
        }
        check(maxJump < .4f, "OSC BOT parameter transition discontinuity excessive");
        l = std::numeric_limits<float>::infinity(); r = std::numeric_limits<float>::quiet_NaN(); frame(wet, l, r);
        // Changing device rate uses existing storage and clears channel history.
        wet.prepare(8000); wet.beginBlock();
        p.note = 127; wet.setParameters(p); wet.prepare(8000); wet.beginBlock();
        for (int n = 0; n < 8000; ++n) { l = .1f; r = 0; frame(wet, l, r); }
        check(std::abs(l) < 1.e-8f, "OSC BOT out-of-band note aliased at low sample rate");
        std::cout << "OSC BOT rate=" << rate << " onsetEnergy(0/50/100)=" << onset[0] << '/' << onset[1] << '/' << onset[2]
            << " parameterMaxStep=" << maxJump << '\n';
    }
}
void testEditor()
{
    InputFxEditor editor; editor.open = true; editor.selectSlot(1); editor.setField(0, 4);
    editor.setField(1, 7); editor.setField(2, -23);
    editor.selectSlot(2); editor.setField(0, 5);
    check(editor.pageCount() == 9 && editor.field(1).text == "SAW" && editor.field(2).value == 0 && editor.field(3).value == 50,
        "OSC BOT official page-one defaults");
    editor.setField(1, 4); editor.setField(2, 50); editor.setField(3, 0);
    editor.navigate(1);
    check(editor.field(0).text == "C2" && editor.field(1).value == 0 && editor.field(2).value == 50 && !editor.field(3).available,
        "OSC BOT official page-two defaults");
    editor.setField(0, 24); check(editor.field(0).text == "C1", "OSC BOT minimum note label");
    editor.setField(0, 999); check(editor.field(0).text == "G9", "OSC BOT maximum note label");
    editor.setField(0, 37); check(editor.field(0).text == "C#2", "OSC BOT semitone label");
    editor.setField(1, -50); editor.setField(2, 100);
    editor.selectSlot(1); check(editor.field(1).value == 7 && editor.field(2).value == -23, "OSC BOT overwrote ROBOT slot");
    editor.selectSlot(2); check(editor.field(1).text == "RECT", "OSC BOT lost slot settings");
    editor.setField(0, 4); editor.setField(0, 5); editor.navigate(1);
    check(editor.field(0).value == 37 && editor.field(1).value == -50 && editor.field(2).value == 100, "OSC BOT lost values across type switch");
    const auto p = OscBotParameters::unpack(OscBotParameters{}.packed());
    check(p.osc == 0 && p.tone == 0 && p.attack == 50 && p.note == 36 && p.modSens == 0 && p.balance == 50, "OSC BOT packed defaults");
}
}
void runOscBotEffectTests()
{
    const juce::ScopedNoDenormals noDenormals;
#if defined(_MSC_VER) && defined(_DEBUG)
    const auto previous = _CrtSetAllocHook(allocationHook);
#endif
    try { testSignal(); testEnvelopeAndControls(); testEditor(); }
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
    check(allocations == 0, "Allocation in OSC BOT processing");
    std::cout << "PASS: OSC BOT five waves/anti-alias, notes, TONE, ATTACK, MOD SENS, BALANCE, silence/bypass, rates/stereo, pages/defaults and no allocations\n";
}
