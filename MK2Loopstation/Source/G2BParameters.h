#pragma once
#include <algorithm>
#include <cstdint>

// RC-505 MK2 Parameter Guide p.35: BALANCE 0..100 (default 50), MODE 1..2 (default 2).
// These are the only user-visible G2B parameters. Internal DSP constants are local choices.
struct G2BParameters
{
    bool enabled = false;
    int balance = 50; // 0 = direct only, 100 = processed only
    int mode = 2;     // 1 = RC-series-inspired divider, 2 = MK2-inspired dual-head shifter

    uint32_t packed() const noexcept
    {
        return uint32_t(enabled) | (uint32_t(std::clamp(balance, 0, 100)) << 1)
             | (uint32_t(std::clamp(mode, 1, 2)) << 8);
    }
    static G2BParameters unpack(uint32_t p) noexcept
    {
        return { (p & 1u) != 0, std::clamp(int((p >> 1) & 127u), 0, 100), std::clamp(int((p >> 8) & 3u), 1, 2) };
    }
};
