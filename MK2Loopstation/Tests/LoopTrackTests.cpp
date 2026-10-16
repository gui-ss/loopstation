#include "TrackEffects.h"
#include "LoopTrack.h"
#include "MusicalClock.h"
#include <array>
#include <cmath>
#include <stdexcept>
#if defined(_MSC_VER) && defined(_DEBUG)
 #include <crtdbg.h>
#endif

namespace
{
void check(bool ok, const char* message) { if (! ok) throw std::runtime_error(message); }
void near(float actual, float expected, const char* message)
{ check(std::abs(actual - expected) < 1.0e-5f, message); }
thread_local bool insideCallback = false;
thread_local int allocations = 0;
#if defined(_MSC_VER) && defined(_DEBUG)
int allocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{
    if (insideCallback && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations;
    return 1;
}
#endif

struct Block
{
    std::array<float, 4096> inputL {}, inputR {}, outputL {}, outputR {};
    const float* inputs[2] { inputL.data(), inputR.data() };
    float* outputs[2] { outputL.data(), outputR.data() };
    void process(LoopTrack& track, int frames, int ins = 2, int outs = 2, double barFrames = 0)
    {
        outputL.fill(0); outputR.fill(0);
        insideCallback = true;
        track.process(inputs, ins, outputs, outs, frames, barFrames);
        insideCallback = false;
    }
};
using State = LoopTrack::State;
using Command = LoopTrack::Command;

void distortionParametersAndSafety()
{
    const auto render = [](TrackFxParameters parameters)
    {
        TrackEffects fx; fx.setParameters(parameters); fx.prepare(48000);
        std::array<float, 4096> result {};
        for (int block = 0; block < 3; ++block)
        {
            insideCallback = true; fx.beginBlock(); insideCallback = false;
            for (int i = 0; i < 4096; ++i)
            {
                float left = static_cast<float>(0.1 * std::sin(juce::MathConstants<double>::twoPi * i / 64)), right = 0;
                insideCallback = true; fx.processFrame(left, right); insideCallback = false;
                near(right, 0, "DISTORTION stereo channels leaked");
                result[static_cast<size_t>(i)] = left;
            }
        }
        return result;
    };
    const auto amplitude = [](const std::array<float, 4096>& signal, int harmonic)
    {
        double real = 0, imaginary = 0;
        for (int i = 0; i < 4096; ++i)
        {
            const auto angle = juce::MathConstants<double>::twoPi * harmonic * i / 64;
            real += signal[static_cast<size_t>(i)] * std::cos(angle);
            imaginary += signal[static_cast<size_t>(i)] * std::sin(angle);
        }
        return std::sqrt(real * real + imaginary * imaginary) / 2048;
    };
    TrackFxParameters p; p.distortionEnabled = true;
    const auto wet = render(p);
    p.distortionEnabled = false; const auto dry = render(p); p.distortionEnabled = true;
    p.drive = 0; const auto mild = render(p); p.drive = 60;
    check(amplitude(wet, 3) / amplitude(wet, 1) > 4 * amplitude(mild, 3) / amplitude(mild, 1), "DRIVE did not change harmonic content");
    p.tone = 0; const auto dark = render(p); p.tone = 100; const auto bright = render(p); p.tone = 65;
    check(amplitude(bright, 3) > amplitude(dark, 3) * 3, "TONE did not shape distorted harmonics");
    p.level = 100; const auto loud = render(p); p.level = 0; const auto silent = render(p); p.level = 50;
    p.mix = 50; const auto half = render(p); p.mix = 0; const auto dryMix = render(p); p.mix = 100;
    for (size_t i = 0; i < wet.size(); ++i)
    {
        near(loud[i], wet[i] * 2, "LEVEL scaling incorrect"); near(silent[i], 0, "LEVEL=0 did not mute wet path");
        near(half[i], 0.5f * (dry[i] + wet[i]), "MIX balance incorrect");
        check(dryMix[i] == dry[i], "MIX=0 was not exact dry");
    }
    p.enabled = true;
    const auto both = render(p); p.distortionEnabled = false; const auto lpfOnly = render(p);
    check(amplitude(both, 3) > amplitude(lpfOnly, 3) + 0.001, "LPF suppressed/bypassed DISTORTION routing");
    p.mix = 0; p.distortionEnabled = true; const auto lpfDryMix = render(p);
    for (size_t i = 0; i < both.size(); ++i) check(lpfDryMix[i] == lpfOnly[i], "MIX=0 bypassed the LPF too");

    TrackEffects fx;
    for (double rate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        p = {}; p.distortionEnabled = true; fx.setParameters(p); fx.prepare(rate);
        for (int i = 0; i < 48000; ++i)
        {
            if (i % 128 == 0)
            {
                p.drive = p.tone = p.level = p.mix = (i / 128) % 2 ? 10000 : -10000;
                p.enabled = (i / 256) % 2 != 0; p.distortionEnabled = (i / 512) % 2 != 0;
                fx.setParameters(p);
                insideCallback = true; fx.beginBlock(); insideCallback = false;
            }
            float l = 0.9f * std::sin(i * 0.19f), r = -l;
            insideCallback = true; fx.processFrame(l, r); insideCallback = false;
            check(std::isfinite(l) && std::isfinite(r) && std::abs(l) <= 1.001f && std::abs(r) <= 1.001f,
                  "DISTORTION unstable/excessive peak at extreme parameters/sample rate");
            near(l, -r, "DISTORTION stereo symmetry lost");
        }
    }
    p = {}; p.distortionEnabled = true; fx.setParameters(p); fx.prepare(48000);
    float previous = 0;
    for (int block = 0; block < 12; ++block)
    {
        p.drive = p.tone = p.level = p.mix = block % 2 ? 100 : 0;
        p.distortionEnabled = block % 3 != 0; fx.setParameters(p);
        insideCallback = true; fx.beginBlock(); insideCallback = false;
        for (int i = 0; i < 4800; ++i)
        {
            float l = 0.1f, r = l;
            insideCallback = true; fx.processFrame(l, r); insideCallback = false;
            if (block > 0) check(std::abs(l - previous) < 0.01f, "DISTORTION parameter/bypass change caused a discontinuity");
            previous = l;
        }
    }
}

void lpfResponseAndSafety()
{
    TrackEffects fx;
    const auto frame = [&](float& l, float& r)
    {
        insideCallback = true;
        fx.processFrame(l, r);
        insideCallback = false;
    };
    const auto begin = [&]
    {
        insideCallback = true; fx.beginBlock(); insideCallback = false;
    };
    const auto response = [&](float frequency)
    {
        fx.setParameters({ true, 41, 0 }); fx.prepare(48000);
        double energy = 0;
        for (int i = 0; i < 48000; ++i)
        {
            if (i % 64 == 0) begin();
            float l = 0.4f * std::sin(juce::MathConstants<float>::twoPi * frequency * i / 48000.0f), r = 0;
            frame(l, r);
            near(r, 0, "LPF leaked between stereo channels");
            if (i >= 24000) energy += l * l;
        }
        return std::sqrt(energy / 24000);
    };
    const auto low = response(100), high = response(8000);
    check(low > 0.25 && low < 0.29, "LPF removed the low-frequency passband");
    check(high < low * 0.01, "LPF did not attenuate high frequencies");

    fx.setParameters({ true, 100, 100 }); fx.prepare(48000);
    for (int i = 0; i < 4000; ++i) { float l = 0.25f, r = l; frame(l, r); }
    float previous = 0.0625f;
    // Sweep both extremes and toggle while processing DC: smooth response, no steps.
    for (int section = 0; section < 8; ++section)
    {
        fx.setParameters({ section % 2 == 0, section % 2 ? 0 : 100, section % 2 ? 0 : 100 }); begin();
        for (int i = 0; i < 4800; ++i)
        {
            float l = 0.25f, r = l; frame(l, r);
            check(std::abs(l - previous) < 0.01f, "LPF toggle/parameter change caused an abrupt step");
            near(l, r, "LPF mono duplication diverged"); previous = l;
        }
    }
    fx.setParameters({ false, 0, 100 }); begin();
    for (int i = 0; i < 4800; ++i)
    {
        float l = 0.37f, r = -0.19f; frame(l, r);
        if (i > 1000) check(l == 0.37f && r == -0.19f, "Bypass is not bit-exact");
    }
    for (double rate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        fx.setParameters({ true, 100, 100 }); fx.prepare(rate);
        for (int i = 0; i < 48000; ++i)
        {
            if (i % 256 == 0)
            {
                fx.setParameters({ true, (i / 256) % 2 ? -10000 : 10000, (i / 512) % 2 ? 10000 : -10000 });
                begin();
            }
            float l = 0.4f * std::sin(static_cast<float>(i) * 0.19f), r = -l;
            frame(l, r);
            check(std::isfinite(l) && std::abs(l) < 0.9f && std::isfinite(r) && std::abs(r) < 0.9f,
                  "LPF unstable or excessive gain at extreme parameters/rates");
        }
    }
}

void transportAndPeriod()
{
    LoopTrack track(1024); track.setVolume(1); track.prepare(1000);
    Block b;
    for (int i = 0; i < 257; ++i) { b.inputL[i] = 0.2f + i * 0.001f; b.inputR[i] = -b.inputL[i]; }
    check(track.request(Command::primary), "REC request rejected");
    check(! track.request(Command::stop), "Mailbox overwrote unconsumed REC");
    b.process(track, 257);
    check(track.snapshot().state == State::recording && track.snapshot().frames == 257, "Recorded duration mismatch");
    for (float sample : b.outputL) near(sample, 0, "Recording leaked into output");
    check(! track.request(Command::clear), "Clear accepted while recording");
    check(track.request(Command::primary), "PLAY request rejected"); b.process(track, 0);
    check(track.snapshot().state == State::playing, "REC did not become PLAY");
    b.process(track, 1028, 0);
    for (int i = 257; i < 771; ++i)
    {
        near(b.outputL[i], b.outputL[i + 257], "Loop period changed or paused");
        near(b.outputR[i], -b.outputL[i], "Stereo channels not preserved");
    }
    near(b.outputL[257 + 20], b.inputL[20], "Stored audio was not reproduced");
    near(b.outputL[256], 0, "End boundary taper missing"); near(b.outputL[257], 0, "Start boundary taper missing");
    check(! track.request(Command::clear), "Clear accepted while playing");
    b.inputL.fill(1); b.inputR.fill(1);
    b.process(track, 257);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 257, "PLAY replaced loop");
    near(b.outputL[20], 0.22f, "PLAY modified stored audio");
    track.setVolume(0); b.process(track, 257);
    near(b.outputL[200], 0, "Fader did not silence playback");
    track.setVolume(0.5f); b.process(track, 257);
    near(b.outputL[100], 0.15f, "Fader gain not applied");
    track.request(Command::stop); b.process(track, 10);
    check(track.snapshot().state == State::stopped && track.snapshot().frames == 257, "STOP erased the phrase");
    for (int i = 5; i < 10; ++i) near(b.outputL[i], 0, "STOP tail did not end");
    track.request(Command::primary);
    check(! track.request(Command::clear), "Clear replaced pending resume");
    b.process(track, 257);
    check(track.snapshot().state == State::playing, "STOP did not resume existing loop");
    near(b.outputL[20], 0.11f, "Resume did not restart existing phrase");
    track.suspend(); track.prepare(2000);
    check(track.snapshot().state == State::stopped && track.snapshot().frames == 257, "Device restart lost the loop");
    track.request(Command::primary); b.process(track, 1028);
    for (int i = 20; i < 514; ++i) near(b.outputL[i], b.outputL[i + 514], "Sample-rate change altered phrase duration");
    track.request(Command::stop); b.process(track, 20);
    check(track.request(Command::clear), "Stopped clear rejected"); b.process(track, 0);
    check(track.snapshot().state == State::empty && track.snapshot().frames == 0, "Clear did not reset phrase");
    track.request(Command::primary); b.process(track, 31);
    track.request(Command::stop); b.process(track, 0);
    check(track.snapshot().state == State::stopped && track.snapshot().frames == 31, "STOP during REC discarded capture");
}

void boundariesAndMissingDevices()
{
    Block b; b.inputL.fill(0.8f); b.inputR.fill(0.2f);
    LoopTrack track(64); track.setVolume(1); track.prepare(1000);
    track.request(Command::primary); b.process(track, 0, 0);
    check(track.snapshot().state == State::empty, "Recorded without input");
    track.request(Command::primary); b.process(track, 0);
    track.request(Command::primary); b.process(track, 0);
    check(track.snapshot().state == State::empty, "Zero-frame capture became a loop");
    track.request(Command::primary); b.process(track, 80, 1);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 64, "Buffer limit not handled exactly");
    near(b.outputL[79], 0.8f, "Limit introduced a silent block before playback");
    near(b.outputR[79], 0.8f, "Mono capture was not duplicated");
    track.suspend(); track.prepare(1000); track.request(Command::primary); b.process(track, 64, 0, 1);
    near(b.outputL[20], 0.8f, "Playback without microphone failed");
    b.process(track, 5, 0, 0);
    check(track.snapshot().state == State::stopped, "Missing output did not stop playback");
    track.request(Command::clear); b.process(track, 0, 0, 0);
    check(track.snapshot().state == State::empty, "Offline clear failed");
    track.request(Command::primary); b.process(track, 17);
    b.process(track, 15, 0);
    check(track.snapshot().state == State::stopped && track.snapshot().frames == 17, "Missing mic lost existing recording or appended silence");
    track.request(Command::clear); b.process(track, 0);
    track.request(Command::primary); b.process(track, 12);
    track.suspend();
    check(track.snapshot().state == State::stopped && track.snapshot().frames == 12, "Device stop during REC lost captured frames");
    LoopTrack tiny(1); tiny.setVolume(1); tiny.prepare(1000);
    tiny.request(Command::primary); b.process(tiny, 32);
    check(tiny.snapshot().frames == 1 && tiny.snapshot().state == State::playing, "One-frame loop invalid");
    near(b.outputL[31], 0.8f, "One-frame loop failed to repeat");
}

void overdubAndUndo()
{
    LoopTrack track(4096); track.setVolume(1); track.prepare(1000);
    Block b; b.inputL.fill(0.1f); b.inputR.fill(0.2f);
    track.request(Command::primary); b.process(track, 1024);
    check(! track.request(Command::undo), "Undo accepted in REC");
    track.request(Command::primary); b.process(track, 1024);
    check(! track.request(Command::undo), "Undo could delete initial recording");
    b.inputL.fill(0.05f); b.inputR.fill(-0.02f);
    track.request(Command::primary); b.process(track, 1024);
    check(track.snapshot().state == State::overdubbing, "PLAY did not enter overdub");
    near(b.outputL[100], 0.1f, "Prior loop stopped playing during overdub");
    check(! track.request(Command::undo), "Undo accepted during overdub");
    b.process(track, 1024);
    near(b.outputL[100], 0.15f, "Second revolution lost first layer");
    track.request(Command::primary); b.process(track, 1024);
    check(track.snapshot().state == State::playing && track.snapshot().canUndo, "Overdub did not finish with undo");
    check(track.snapshot().frames == 1024, "Overdub changed loop duration");
    near(b.outputL[100], 0.2f, "Multi-revolution overdub not accumulated");
    near(b.outputR[100], 0.16f, "Stereo overdub mixed channels");
    const auto firstL = b.outputL, firstR = b.outputR;
    track.request(Command::primary); b.process(track, 0);
    check(! track.request(Command::undo), "Undo accepted in zero-frame active overdub");
    track.request(Command::primary); b.process(track, 0);
    check(track.snapshot().canUndo, "Zero-frame take consumed previous undo");

    b.inputL.fill(0.03f); b.inputR.fill(0.01f);
    track.request(Command::primary);
    for (int i = 0; i < 3; ++i) b.process(track, 1024);
    track.request(Command::primary); b.process(track, 1024);
    near(b.outputL[100], 0.29f, "Second take missing layers");
    check(track.request(Command::undo), "Completed overdub undo rejected"); b.process(track, 1024);
    for (int i = 10; i < 1024; ++i)
    {
        near(b.outputL[i], firstL[i], "Undo did not restore complete previous take");
        near(b.outputR[i], firstR[i], "Undo did not restore stereo take");
    }
    check(! track.request(Command::undo), "Second undo removed an older overdub");

    // Partial take crossing the wrap; STOP commits it, resume keeps it, undo
    // restores the entire older phrase, including never-touched chunks.
    b.process(track, 900); track.request(Command::primary); b.process(track, 300);
    track.request(Command::stop); b.process(track, 20);
    check(track.snapshot().state == State::stopped && track.snapshot().canUndo, "STOP lost overdub/undo");
    track.request(Command::primary); b.process(track, 1024);
    near(b.outputL[50], firstL[50] + 0.03f, "Resume lost partial overdub at wrap");
    track.request(Command::stop); b.process(track, 20);
    track.request(Command::undo); b.process(track, 0);
    track.request(Command::primary); b.process(track, 1024);
    for (int i = 10; i < 1024; ++i) near(b.outputL[i], firstL[i], "Stopped undo lost old audio");

    // No input cannot enter dub; input loss completes a take without silence
    // replacing the phrase, while a device stop finalizes and preserves it.
    track.request(Command::primary); b.process(track, 0, 0);
    check(track.snapshot().state == State::playing, "Entered overdub without mic");
    track.request(Command::primary); b.process(track, 100);
    b.process(track, 0, 0);
    check(track.snapshot().state == State::playing && track.snapshot().canUndo, "Input loss discarded take");
    track.request(Command::primary); b.process(track, 100); track.suspend();
    check(track.snapshot().state == State::stopped && track.snapshot().canUndo, "Device stop discarded take");
    track.request(Command::clear); b.process(track, 0);
    check(! track.snapshot().canUndo && ! track.request(Command::undo), "Clear retained undo history");
}

void overdubContinuityAndLimits()
{
    LoopTrack track(2048), reference(2048);
    Block b, dry;
    for (int i = 0; i < 1021; ++i)
    {
        b.inputL[i] = dry.inputL[i] = 0.2f * std::sin(i * 0.08f);
        b.inputR[i] = dry.inputR[i] = -b.inputL[i];
    }
    for (auto* t : { &track, &reference }) { t->setVolume(1); t->prepare(48000); t->request(Command::primary); }
    b.process(track, 1021); dry.process(reference, 1021);
    track.request(Command::primary); reference.request(Command::primary);
    b.process(track, 2042); dry.process(reference, 2042);
    b.inputL.fill(0); b.inputR.fill(0); dry.inputL.fill(0); dry.inputR.fill(0);
    // Irregular block lengths and non-boundary transitions must keep phase.
    for (int i = 0; i < 20; ++i)
    {
        track.request(Command::primary);
        b.process(track, 137); dry.process(reference, 137);
        for (int j = 0; j < 137; ++j) near(b.outputL[j], dry.outputL[j], "PLAY/DUB reset phase or introduced gap");
        check(track.snapshot().frames == 1021, "Loop period changed");
    }
    // Full-scale input over many revolutions remains bounded.
    b.inputL.fill(1); b.inputR.fill(-1); track.request(Command::primary);
    for (int i = 0; i < 30; ++i)
    {
        b.process(track, 4096);
        for (int j = 0; j < 4096; ++j)
            check(std::isfinite(b.outputL[j]) && std::abs(b.outputL[j]) <= 1.00001f
                  && std::abs(b.outputR[j]) <= 1.00001f, "Overdub accumulated excessive level");
    }
    track.request(Command::stop); b.process(track, 0);
    track.request(Command::undo); b.process(track, 0);
    // Changed sample rate keeps time and adds one layer per original frame.
    LoopTrack rateTrack(2048); rateTrack.setVolume(1); rateTrack.prepare(1000);
    b.inputL.fill(0.1f); b.inputR.fill(0.1f);
    rateTrack.request(Command::primary); b.process(rateTrack, 1024);
    rateTrack.request(Command::primary); b.process(rateTrack, 0);
    rateTrack.prepare(2000); rateTrack.request(Command::primary); b.process(rateTrack, 2048);
    rateTrack.request(Command::primary); b.process(rateTrack, 2048);
    rateTrack.request(Command::primary); b.process(rateTrack, 2048);
    near(b.outputL[400], 0.2f, "Rate change doubled overdub gain");
    check(rateTrack.snapshot().frames == 1024, "Rate change changed loop length");
    for (int i = 0; i < 2048; ++i) b.inputL[i] = b.outputL[i];
    b.process(rateTrack, 2048, 0);
    for (int i = 0; i < 2048; ++i) near(b.outputL[i], b.inputL[i], "Rate-changed loop lost period");
    // Downsampling the device also writes every crossed original frame.
    rateTrack.prepare(500); rateTrack.request(Command::primary); b.process(rateTrack, 512);
    b.inputL.fill(0.1f); b.inputR.fill(0.1f);
    rateTrack.request(Command::primary); b.process(rateTrack, 512);
    rateTrack.request(Command::primary); b.process(rateTrack, 512);
    near(b.outputL[200], 0.3f, "Lower device rate left holes in overdub");

    // Undo on a rapidly changing waveform cannot overshoot the two signals.
    track.request(Command::primary); b.process(track, 4096);
    b.inputL.fill(0.8f); b.inputR.fill(-0.8f);
    track.request(Command::primary); b.process(track, 4096);
    track.request(Command::primary); b.process(track, 4096);
    check(track.request(Command::undo), "Undo smoothing setup failed"); b.process(track, 4096);
    for (int i = 0; i < 4096; ++i)
        check(std::abs(b.outputL[i]) <= 1.00001f && std::abs(b.outputR[i]) <= 1.00001f,
              "Undo ramp exceeded signal peaks");
}

void quantizedRecording()
{
    for (int bars : { 1, 2 })
    {
        const int target = bars * 96000;
        LoopTrack track(200000); track.setVolume(1); track.prepare(48000); track.setQuantizeEnabled(true);
        MusicalClock clock; clock.prepare(48000); clock.setBpm(120); clock.process(17003);
        Block b; b.inputL.fill(.2f); b.inputR.fill(-.1f);
        const auto process = [&](int count)
        {
            do { const int n = juce::jmin(1024, count); b.process(track, n, 2, 2, clock.process(n)); count -= n; }
            while (count > 0);
        };
        track.request(Command::primary); process(12345 + (bars - 1) * 96000);
        check(track.snapshot().state == State::recording && track.snapshot().recordingQuantized,
              "Quantized REC did not start immediately");
        track.request(Command::primary); process(0);
        check(track.snapshot().awaitingBoundary && track.snapshot().state == State::recording,
              "Pending PLAY changed REC/LED too early");
        track.request(Command::primary); process(0); // Does not move the boundary.
        process(target - track.snapshot().frames - 1);
        check(track.snapshot().frames == target - 1 && track.snapshot().awaitingBoundary, "REC ended before bar boundary");
        // Finish in the middle of a block: one REC sample, then 512 PLAY samples.
        b.process(track, 513, 2, 2, clock.process(513));
        const auto state = track.snapshot();
        check(state.frames == target && state.state == State::playing && ! state.awaitingBoundary,
              "Quantized length/state not sample-exact");
        near(state.progress, 512.0f / target, "PLAY waited for another block");
        near(b.outputL[400], .2f, "Quantized transition inserted silent block");
        check(std::abs(clock.position().beats - (17003.0 + target + 512) * 120 / (60 * 48000)) < 1e-10,
              "Recording reset musical clock");
        std::vector<float> cycle(static_cast<size_t>(target)); // Outside callback.
        for (int pass = 0; pass < 2; ++pass)
            for (int offset = 0; offset < target;)
            {
                const int n = juce::jmin(1024, target - offset);
                b.process(track, n, 2, 2, clock.process(n));
                for (int i = 0; i < n; ++i)
                {
                    if (pass == 0) cycle[static_cast<size_t>(offset + i)] = b.outputL[i];
                    else near(b.outputL[i], cycle[static_cast<size_t>(offset + i)], "Quantized loop period changed/paused");
                    near(b.outputR[i], -.5f * b.outputL[i], "Quantized stereo capture corrupted");
                }
                offset += n;
            }
        b.inputL.fill(.02f); b.inputR.fill(.01f);
        track.request(Command::primary); process(0);
        check(track.snapshot().state == State::overdubbing && ! track.snapshot().awaitingBoundary, "Quantize delayed overdub");
        process(target); track.request(Command::primary); process(0);
        check(track.snapshot().frames == target && track.snapshot().canUndo, "Overdub resized quantized loop");
        track.request(Command::stop); process(0); track.request(Command::primary); process(1024);
        near(b.outputL[1000], .22f, "STOP/resume lost quantized overdub");
        track.request(Command::stop); process(0); track.request(Command::undo); process(0);
        track.request(Command::primary); process(1024);
        near(b.outputL[1000], .2f, "Undo changed initial quantized recording");
    }
}

void quantizeBoundariesAndCancellation()
{
    Block b; b.inputL.fill(.2f); b.inputR.fill(.2f);
    MusicalClock clock; clock.prepare(1000);
    LoopTrack track(60000); track.prepare(1000); track.setVolume(1);
    const auto process = [&](int count, int inputs = 2)
    {
        do { const int n = juce::jmin(1024, count); b.process(track, n, inputs, 2, clock.process(n)); count -= n; }
        while (count > 0);
    };
    const auto clear = [&] { track.request(Command::stop); process(0); track.request(Command::clear); process(0); };
    check(! track.quantizeEnabled(), "Quantize default is not OFF");
    track.request(Command::primary); process(317); track.setQuantizeEnabled(true);
    track.request(Command::primary); process(0);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 317, "Q edit changed already-started free take");
    clear(); track.request(Command::primary); process(500);
    clock.setBpm(60); track.request(Command::primary); process(0); track.setQuantizeEnabled(false);
    check(track.snapshot().recordingQuantized && track.snapshot().awaitingBoundary, "Q toggle cancelled latched grid");
    process(1500);
    check(track.snapshot().frames == 2000 && track.snapshot().state == State::playing, "BPM/Q change moved pending boundary");
    for (int requested : { 1999, 2000, 2001 })
    {
        clear(); clock.setBpm(120); track.setQuantizeEnabled(true);
        track.request(Command::primary); process(requested);
        track.request(Command::primary); process(0);
        const int expected = requested <= 2000 ? 2000 : 4000;
        process(expected - requested);
        check(track.snapshot().frames == expected && track.snapshot().state == State::playing, "Boundary rounding off by one bar");
    }
    clear(); track.request(Command::primary); process(321); track.request(Command::primary); process(100);
    track.request(Command::stop); process(0);
    check(track.snapshot().state == State::stopped && track.snapshot().frames == 421 && ! track.snapshot().awaitingBoundary,
          "STOP did not cancel pending recording safely");
    track.request(Command::primary); process(100);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 421, "Resume requantized partial take");
    clear(); track.request(Command::primary); process(301); track.request(Command::primary); process(0);
    process(0, 0);
    check(track.snapshot().state == State::stopped && ! track.snapshot().awaitingBoundary && track.snapshot().frames == 301,
          "Input loss left recording pending");
    clear(); track.request(Command::primary); process(303); track.request(Command::primary); process(0);
    track.prepare(2000);
    check(track.snapshot().state == State::stopped && ! track.snapshot().awaitingBoundary && track.snapshot().frames == 303,
          "Device change lost take or retained stale pending boundary");
    track.prepare(1000); clear(); clock.setBpm(123.5);
    track.request(Command::primary); process(2001); track.request(Command::primary); process(3887 - 2001);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 3887, "Fractional bar rounding accumulated drift");
    clear(); clock.setBpm(120); track.request(Command::primary); process(60000);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 60000, "60-second bar limit failed");
    clear(); clock.setBpm(90); track.request(Command::primary); process(58667);
    check(track.snapshot().state == State::playing && track.snapshot().frames == 58667, "Limit not rounded to last complete bar");
    LoopTrack tiny(123); tiny.prepare(1000); tiny.setQuantizeEnabled(true); tiny.request(Command::primary);
    b.process(tiny, 256, 2, 2, 2000);
    check(tiny.snapshot().state == State::playing && tiny.snapshot().frames == 123, "Short buffer exceeded capacity");
}

void snapshotChunkBoundaries()
{
    // Multiple index pages, untouched regions, repeated undo/new take, and a
    // final partial chunk. No bulk snapshot copying, allocations or stale maps.
    constexpr int size = 40003;
    LoopStorage storage(size);
    for (int i = 0; i < size; ++i) storage.set(i, i * 0.00001f, -i * 0.00001f);
    for (int take = 0; take < 20; ++take)
    {
        insideCallback = true; storage.snapshot();
        for (int i : { 0, 63, 64, 16383, 16384, 32767, 32768, size - 1 })
        { storage.makeWritable(i); storage.set(i, 0.8f, -0.7f); }
        storage.restore(); insideCallback = false;
        for (int i = 0; i < size; ++i)
        {
            near(storage.get(0, i), i * 0.00001f, "COW undo corrupted untouched page/chunk");
            near(storage.get(1, i), -i * 0.00001f, "COW undo corrupted stereo");
        }
    }
}
}

void runLoopTrackTests()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    const auto previousHook = _CrtSetAllocHook(allocationHook);
#endif
    try { transportAndPeriod(); boundariesAndMissingDevices(); overdubAndUndo();
          overdubContinuityAndLimits(); snapshotChunkBoundaries(); quantizedRecording(); quantizeBoundariesAndCancellation();
          lpfResponseAndSafety();  distortionParametersAndSafety(); }
    catch (...)
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetAllocHook(previousHook);
#endif
        throw;
    }
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(previousHook);
#endif
    check(allocations == 0, "Heap allocation detected inside LoopTrack::process");
}
