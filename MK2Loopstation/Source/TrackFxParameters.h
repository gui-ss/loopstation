#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Small UI value model; normalized values follow the reference's 0..100 scale.
// Hz/Q mapping is local to this prototype, not an emulation of BOSS DSP.
struct TrackFxParameters
{
    bool enabled = false;
    int cutoff = 70, resonance = 0;
    bool distortionEnabled = false; // TRACK FX C, independent of LPF in A.
    int drive = 60, tone = 65, level = 50, mix = 100;
    static int bounded(int value) noexcept { return std::clamp(value, 0, 100); }
    static float cutoffHz(int value) noexcept { return 40.0f * std::pow(450.0f, bounded(value) / 100.0f); }
    static float resonanceQ(int value) noexcept { return 0.5f + 1.5f * bounded(value) / 100.0f; }
    static float driveGain(int value) noexcept { return 2.0f * std::pow(32.0f, bounded(value) / 100.0f); }
    static float toneHz(int value) noexcept { return 500.0f * std::pow(24.0f, bounded(value) / 100.0f); }
    uint64_t packed() const noexcept
    {
        return static_cast<uint64_t>(bounded(cutoff)) | (static_cast<uint64_t>(bounded(resonance)) << 8u)
            | (enabled ? uint64_t{1} << 16u : 0u) | (distortionEnabled ? uint64_t{1} << 17u : 0u)
            | (static_cast<uint64_t>(bounded(drive)) << 18u) | (static_cast<uint64_t>(bounded(tone)) << 25u)
            | (static_cast<uint64_t>(bounded(level)) << 32u) | (static_cast<uint64_t>(bounded(mix)) << 39u);
    }
};
