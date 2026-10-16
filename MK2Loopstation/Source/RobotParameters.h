#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Parameter Guide p.36. Octave is not exposed by the hardware parameter table.
struct RobotParameters
{
    bool enabled = false;
    int note = 0, formant = 0, mode = 2;
    inline static constexpr std::array<const char*, 12> notes
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static float frequency(int note) noexcept
    { return 130.81278265f * std::pow(2.f, float(std::clamp(note, 0, 11)) / 12.f); }
    uint32_t packed() const noexcept
    {
        return (enabled ? 1u : 0u) | (uint32_t(std::clamp(note, 0, 11)) << 1u)
            | (uint32_t(std::clamp(formant, -50, 50) + 50) << 5u)
            | (uint32_t(std::clamp(mode, 1, 2) - 1) << 12u);
    }
};
