#pragma once
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

// PG p.40. RATE has its own domain; it is NOT the OSC BOT FX SEQUENCE enum.
struct StepSlicerParameters
{
    bool enabled = false;
    int rate = 107, maximum = 16, depth = 100, threshold = -30, gain = 6;
    std::array<int,16> length, level;
    StepSlicerParameters() { length.fill(50); level.fill(100); }
    inline static constexpr std::array<const char*,14> rateNames {
        "4MEAS","2MEAS","1MEAS","1/2","1/4.","1/2T","1/4",
        "1/8.","1/4T","1/8","1/16.","1/8T","1/16","1/32" };
    inline static constexpr std::array<double,14> beats {16,8,4,2,1.5,4./3,1,.75,2./3,.5,.375,1./3,.25,.125};
    static constexpr int lastRate = 114, eighthTriplet = 112;
    static double stepBeats(int r) noexcept { return beats[size_t(std::clamp(r-101,0,13))]; }
    // The guide does not publish the numeric curve: explicit local 0.1..20 Hz approximation.
    static double numericHz(int r) noexcept { return .1*std::pow(200.,std::clamp(r,0,100)/100.); }
};

// One GUI writer; one bounded, coherent read per callback. No retries or locks.
class StepSlicerMailbox
{
public:
    StepSlicerMailbox() noexcept { set({}); }
    void set(const StepSlicerParameters& p) noexcept
    {
        serial.fetch_add(1);
        controls.store(uint64_t(p.enabled) | (uint64_t(std::clamp(p.rate,0,114))<<1)
            | (uint64_t(std::clamp(p.maximum,1,16))<<8) | (uint64_t(std::clamp(p.depth,0,100))<<13)
            | (uint64_t(std::clamp(p.threshold,-30,0)+30)<<20) | (uint64_t(std::clamp(p.gain,0,20))<<25));
        for (int g=0;g<4;++g)
        {
            uint64_t word=0;
            for (int j=0;j<4;++j)
                word |= (uint64_t(std::clamp(p.length[size_t(g*4+j)],1,100))
                    | (uint64_t(std::clamp(p.level[size_t(g*4+j)],0,100))<<7)) << (j*14);
            steps[size_t(g)].store(word);
        }
        serial.fetch_add(1);
    }
    bool read(StepSlicerParameters& p) const noexcept
    {
        const auto before=serial.load(); if (before&1) return false;
        const auto c=controls.load();
        std::array<uint64_t,4> v; for(size_t i=0;i<4;++i) v[i]=steps[i].load();
        if(serial.load()!=before) return false;
        p.enabled=(c&1)!=0; p.rate=int((c>>1)&127); p.maximum=int((c>>8)&31);
        p.depth=int((c>>13)&127); p.threshold=int((c>>20)&31)-30; p.gain=int((c>>25)&31);
        for(int i=0;i<16;++i)
        { const auto w=v[size_t(i/4)]>>(14*(i%4)); p.length[size_t(i)]=int(w&127); p.level[size_t(i)]=int((w>>7)&127); }
        return true;
    }
private:
    std::atomic<uint64_t> serial{0},controls{0};
    std::array<std::atomic<uint64_t>,4> steps{};
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
};
