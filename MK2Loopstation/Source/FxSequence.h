#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

// PG p.34: RATE is the duration of ONE step, not the complete pattern.
struct FxSequenceParameters
{
    bool enabled = false, sync = false, retrig = false;
    int rate = 105, maximum = 16; // quarter note; TARGET is NOTE in this stage.
    std::array<int, 16> notes {{36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36}};
    static constexpr int lastRate = 108;
    static double stepBeats(int value) noexcept
    {
        constexpr double beats[] {16,8,4,2,1,.5,.25,.125};
        return beats[std::clamp(value - 101, 0, 7)];
    }
    // The guide supplies no Hz curve. Explicit local approximation, NOT a BOSS calibration.
    static double numericHz(int value) noexcept { return .1 * std::pow(200., std::clamp(value,0,100) / 100.); }
};

// Single GUI producer; atomic words prevent data races even if a writer overlaps
// a read. One bounded attempt per block, never spin/wait on the audio thread.
class FxSequenceMailbox
{
public:
    FxSequenceMailbox() noexcept { set({}); }
    void set(const FxSequenceParameters& p) noexcept
    {
        serial.fetch_add(1);
        controls.store(uint64_t(p.enabled) | (uint64_t(p.sync)<<1) | (uint64_t(p.retrig)<<2)
            | (uint64_t(std::clamp(p.rate,0,FxSequenceParameters::lastRate))<<3)
            | (uint64_t(std::clamp(p.maximum,1,16))<<10));
        for (int group=0; group<2; ++group)
        {
            uint64_t word=0;
            for (int i=0;i<8;++i) word |= uint64_t(std::clamp(p.notes[size_t(group*8+i)],24,127)) << (7*i);
            values[group].store(word);
        }
        serial.fetch_add(1);
    }
    bool read(FxSequenceParameters& p) const noexcept
    {
        const auto before=serial.load();
        if (before & 1) return false;
        const auto c=controls.load(), a=values[0].load(), b=values[1].load();
        if (serial.load()!=before) return false;
        p.enabled=(c&1)!=0; p.sync=(c&2)!=0; p.retrig=(c&4)!=0;
        p.rate=int((c>>3)&127); p.maximum=int((c>>10)&31);
        for(int i=0;i<16;++i) p.notes[size_t(i)]=int(((i<8?a:b)>>(7*(i%8)))&127);
        return true;
    }
private:
    std::atomic<uint64_t> serial{0}, controls{0}, values[2]{};
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
};

class FxSequence
{
public:
    void setParameters(const FxSequenceParameters& value) noexcept { mailbox.set(value); }
    // Both increments come from MusicalClock's one tempo/sample-rate snapshot.
    void beginBlock(double beatsPerSample, double secondsPerSample, bool effectOn) noexcept;
    int nextNote(int manualNote) noexcept;
    void phraseStart() noexcept { if (p.enabled && p.sync) phase=0; }
    void alignActivation(double phraseSeconds) noexcept;
    // Explicit UI group start, not a persistent RETRIG edit. SYNC follows the
    // same reference phrase; without it (or without playback) start at VAL1.
    void startGroup(double phraseSeconds) noexcept
    {
        if(p.enabled) phase=p.sync && phraseSeconds>=0
            ? std::fmod(phraseSeconds*stepsPerSecond,double(p.maximum)) : 0;
        activation=false;
    }
    void reset() noexcept { phase=0; wasEnabled=wasEffectOn=false; }
    bool synced() const noexcept { return p.enabled && p.sync; }
    int step() const noexcept { return int(phase) % p.maximum; }
private:
    FxSequenceMailbox mailbox;
    FxSequenceParameters p;
    double phase=0, increment=0, stepsPerSecond=0;
    bool wasEnabled=false, wasEffectOn=false, activation=false;
};
