#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// RC-505 MK2 manual p.42: ranges and defaults are official. The intermediate steps
// (TIME 0.1 s, PRE DELAY 1 ms, 1/3-octave cut table) are local choices, NOT official.
struct ReverbParameters
{
    static constexpr int cutSteps = 29;
    inline static constexpr std::array<float, cutSteps> cutFrequencies {
        20.f, 25.f, 31.5f, 40.f, 50.f, 63.f, 80.f, 100.f, 125.f, 160.f, 200.f, 250.f, 315.f, 400.f, 500.f,
        630.f, 800.f, 1000.f, 1250.f, 1600.f, 2000.f, 2500.f, 3150.f, 4000.f, 5000.f, 6300.f, 8000.f,
        10000.f, 12500.f };

    bool enabled = false;
    int time = 30;          // 1..100 => 0.1..10.0 s
    int preDelay = 500;     // 0..500 ms
    int density = 5;        // 1..10
    int dLevel = 100;       // 0..100
    int lowCut = 0;         // 0 = FLAT, n = cutFrequencies[n-1]
    int highCut = cutSteps; // cutSteps = FLAT, n < cutSteps = cutFrequencies[n]
    int eLevel = 50;        // 0..100

    bool lowCutFlat() const noexcept { return lowCut <= 0; }
    float lowCutHz() const noexcept { return cutFrequencies[size_t(std::clamp(lowCut, 1, cutSteps) - 1)]; }
    bool highCutFlat() const noexcept { return highCut >= cutSteps; }
    float highCutHz() const noexcept { return cutFrequencies[size_t(std::clamp(highCut, 0, cutSteps - 1))]; }

    uint64_t packed() const noexcept
    {
        return uint64_t(enabled)
            | (uint64_t(std::clamp(time, 1, 100)) << 1)
            | (uint64_t(std::clamp(preDelay, 0, 500)) << 8)
            | (uint64_t(std::clamp(density, 1, 10)) << 17)
            | (uint64_t(std::clamp(dLevel, 0, 100)) << 21)
            | (uint64_t(std::clamp(lowCut, 0, cutSteps)) << 28)
            | (uint64_t(std::clamp(highCut, 0, cutSteps)) << 33)
            | (uint64_t(std::clamp(eLevel, 0, 100)) << 38);
    }
    static ReverbParameters unpack(uint64_t p) noexcept
    {
        return { (p & 1) != 0, int((p >> 1) & 127), int((p >> 8) & 511), int((p >> 17) & 15),
                 int((p >> 21) & 127), int((p >> 28) & 31), int((p >> 33) & 31), int((p >> 38) & 127) };
    }
};
