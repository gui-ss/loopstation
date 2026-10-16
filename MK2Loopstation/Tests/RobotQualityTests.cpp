#include "RobotEffect.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void requireQuality(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
// Deterministic source-filter vowel. Input F0 is deliberately below the target
// on low-voice cases; normalise the fixture, never the rendered output.
std::vector<float> voice(double rate, double fundamental, int count)
{
    std::vector<float> input(size_t(count), 0.f);
    double peak = 0;
    for (int i = 0; i < count; ++i)
    {
        double x = 0;
        for (int h = 1; h * fundamental < 7000; ++h)
        {
            const double hz = h * fundamental;
            const double amplitude = .12 / h + std::exp(-std::pow((hz - 600) / 180, 2))
                + .6 * std::exp(-std::pow((hz - 1550) / 260, 2))
                + .25 * std::exp(-std::pow((hz - 2600) / 350, 2));
            x += amplitude * std::sin(juce::MathConstants<double>::twoPi * hz * i / rate);
        }
        input[size_t(i)] = float(x); peak = std::max(peak, std::abs(x));
    }
    for (auto& x : input) x *= float(.35 / peak);
    return input;
}
struct Metrics
{
    double peak = 0, rms = 0, dc = 0, step = 0, residual = 0;
};
Metrics metrics(const std::vector<float>& data, int start, double period)
{
    Metrics m;
    double error = 0, energy = 0;
    for (int i = start; i < int(data.size()); ++i)
    {
        const double x = data[size_t(i)];
        requireQuality(std::isfinite(x), "ROBOT produced non-finite audio");
        m.peak = std::max(m.peak, std::abs(x)); m.dc += x; energy += x * x;
        m.step = std::max(m.step, std::abs(x - data[size_t(i - 1)]));
        const double previous = i - period;
        const auto index = size_t(previous);
        const double y = data[index] + (previous - double(index)) * (data[index + 1] - data[index]);
        error += (x - y) * (x - y);
    }
    m.rms = std::sqrt(energy / (data.size() - start));
    m.dc /= double(data.size() - start);
    m.residual = error / (energy + 1.e-20);
    return m;
}
void testUnvoicedAndContinuity()
{
    for (double rate : { 44100., 48000. })
        for (int mode : { 1, 2 })
        {
            RobotEffect a, b;
            RobotParameters p { true, 0, 0, mode };
            a.setParameters(p); b.setParameters(p); a.prepare(rate); b.prepare(rate);
            uint32_t random = 12345;
            double dryPower = 0, wetPower = 0, cross = 0;
            double maxStep = 0, tail = 0;
            float previous = 0;
            const int count = int(rate);
            for (int n = 0; n < count; ++n)
            {
                // Continuous pitched vowel-like source -> breath/consonant ->
                // isolated percussive attack -> sustained silence.
                random = random * 1664525u + 1013904223u;
                float x = n < int(.25 * rate) ? .15f * std::sin(float(n * 2 * juce::MathConstants<double>::pi * 80 / rate))
                    : n < int(.55 * rate) ? .18f * (float(random >> 8) / 8388608.f - 1.f)
                    : n == int(.6 * rate) ? .7f : 0.f;
                // Different host block boundaries must not reset/rephase the DSP.
                if (n % 31 == 0) a.beginBlock();
                if (n % 257 == 0) b.beginBlock();
                if (n == int(.1 * rate)) { p.note = 11; a.setParameters(p); b.setParameters(p); a.beginBlock(); b.beginBlock(); }
                float l = x, r = 0, second = x, silent = 0;
                a.processFrame(l, r); b.processFrame(second, silent);
                requireQuality(l == second && r == 0 && silent == 0, "ROBOT depends on block boundaries or shared channel/instance state");
                requireQuality(std::isfinite(l) && std::abs(l) <= .8f, "ROBOT transient unsafe");
                if (n < int(.25 * rate)) maxStep = std::max(maxStep, double(std::abs(l - previous)));
                if (n > int(.4 * rate) && n < int(.55 * rate))
                { dryPower += x*x; wetPower += l*l; cross += x*l; }
                if (n > int(.95 * rate)) tail = std::max(tail, double(std::abs(l)));
                previous = l;
            }
            const double correlation = cross / std::sqrt(dryPower * wetPower + 1.e-30);
            const double gain = std::sqrt(wetPower / dryPower);
            std::cout << "ROBOT unvoiced rate=" << rate << " mode=" << mode << " correlation=" << correlation
                      << " rmsGain=" << gain << " noteTransitionMaxStep=" << maxStep << " tailPeak=" << tail << '\n';
            requireQuality(correlation > .8 && gain > .65 && gain < 1.1, "ROBOT lost unvoiced articulation or amplified noise");
            requireQuality(maxStep < .1 && tail < .0001, "ROBOT transition/tail discontinuity");
            // A disconnected/invalid input must not poison subsequent state.
            float l = std::numeric_limits<float>::quiet_NaN(), r = std::numeric_limits<float>::infinity();
            a.processFrame(l, r);
            requireQuality(std::isfinite(l) && std::isfinite(r), "ROBOT invalid input poisoned state");
            for (int i = 0; i < count; ++i) { l = .1f; r = 0; a.processFrame(l, r); }
            requireQuality(std::abs(l) < .00001f, "ROBOT synthesized steady DC");
        }
}
}

void runRobotQualityTests(bool enforce)
{
    const juce::ScopedNoDenormals noDenormals;
    std::cout << "ROBOT quality: rate,F0,note,formant,mode,peak,rms,dc,maxStep,periodResidual,RTfraction,p99ms,maxMs\n";
    for (double rate : { 44100., 48000. })
        for (double f0 : { 80., 187. })
        {
            auto input = voice(rate, f0, int(rate * .6));
            for (int note : { 0, 11 })
                for (int formant : { -50, 0, 50 })
                    for (int mode : { 1, 2 })
                    {
                        RobotEffect robot;
                        robot.setParameters({ true, note, formant, mode }); robot.prepare(rate);
                        std::vector<float> output(input.size());
                        std::vector<double> timings;
                        double total = 0;
                        for (int offset = 0; offset < int(input.size()); offset += 256)
                        {
                            const int end = std::min(offset + 256, int(input.size()));
                            const auto begin = std::chrono::steady_clock::now();
                            robot.beginBlock();
                            for (int i = offset; i < end; ++i)
                            {
                                float l = input[size_t(i)], r = 0;
                                robot.processFrame(l, r);
                                requireQuality(r == 0, "ROBOT channel state leaked");
                                output[size_t(i)] = l;
                            }
                            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
                            total += elapsed; timings.push_back(elapsed * 1000);
                        }
                        const auto m = metrics(output, int(rate * .3), rate / RobotParameters::frequency(note));
                        std::sort(timings.begin(), timings.end());
                        const double fraction = total * rate / double(input.size());
                        std::cout << rate << ',' << f0 << ',' << note << ',' << formant << ',' << mode << ','
                                  << m.peak << ',' << m.rms << ',' << m.dc << ',' << m.step << ',' << m.residual << ','
                                  << fraction << ',' << timings[size_t(.99 * (timings.size() - 1))] << ',' << timings.back() << '\n';
                        if (enforce)
                        {
                            requireQuality(m.peak <= .81 && m.rms > .002 && m.rms < .25, "ROBOT vowel output level unsafe or inaudible");
                            requireQuality(std::abs(m.dc) < .005, "ROBOT vowel DC excessive");
                            requireQuality(m.residual < .015, "ROBOT excessive non-periodic modulation on sustained vowel");
                        }
                        // Tail must decay without resetting the instance or muting externally.
                        float tail = 0;
                        for (int i = 0; i < int(rate * .4); ++i)
                        {
                            float l = 0, r = 0; robot.processFrame(l, r);
                            if (i > int(rate * .35)) tail = std::max(tail, std::abs(l));
                        }
                        if (enforce) requireQuality(tail < .0001f, "ROBOT leaked into sustained silence");
                    }
        }
    if (enforce) testUnvoicedAndContinuity();
    std::cout << "PASS: ROBOT deterministic low/high voice, NOTE extremes, FORMANT/MODE matrix, stereo isolation and silence\n";
}
