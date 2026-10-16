#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Parameter Guide p.38. Integer note is MIDI/scientific pitch C1=24..G9=127.
// A value object independent of the editor: a future sequencer can supply NOTE
// through the same parameter snapshot, without owning oscillator state.
struct OscBotParameters
{
    bool enabled = false;
    int osc = 0, tone = 0, attack = 50, note = 36, modSens = 0, balance = 50;
    inline static constexpr std::array<const char*, 5> waves
        { "SAW", "VINTAGE SAW", "DETUNE SAW", "SQUARE", "RECT" };
    static float frequency(int midiNote) noexcept
    { return 440.f * std::exp2(float(std::clamp(midiNote, 24, 127) - 69) / 12.f); }
    uint64_t packed() const noexcept
    {
        return uint64_t(enabled) | (uint64_t(std::clamp(osc, 0, 4)) << 1)
            | (uint64_t(std::clamp(tone, -50, 50) + 50) << 4)
            | (uint64_t(std::clamp(attack, 0, 100)) << 11)
            | (uint64_t(std::clamp(note, 24, 127)) << 18)
            | (uint64_t(std::clamp(modSens, -50, 50) + 50) << 25)
            | (uint64_t(std::clamp(balance, 0, 100)) << 32);
    }
    static OscBotParameters unpack(uint64_t p) noexcept
    {
        return { (p & 1) != 0, int((p >> 1) & 7), int((p >> 4) & 127) - 50,
                 int((p >> 11) & 127), int((p >> 18) & 127),
                 int((p >> 25) & 127) - 50, int((p >> 32) & 127) };
    }
};
