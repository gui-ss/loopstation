#pragma once

#include "DisplayModel.h"
#include <cmath>

// Historical class name retained: progress AND levels now come only from audio snapshots.
class DisplaySimulation
{
public:
    struct Motion
    {
        double seconds = 0.0;
        float progress = 0.0f;
        std::array<float, 2> level {}, peak {};
    };

    const Motion& get(size_t index) const { return motion[index]; }
    void reset(size_t index) { motion[index] = {}; }
    void updateProgress(const std::array<float, 5>& loopProgress, const DisplayModel& model)
    {
        for (size_t i = 0; i < motion.size(); ++i)
            motion[i].progress = model.tracks[i].state == TrackVisualState::blank ? 0.0f
                : loopProgress[i];
    }

    void updateLevels(const std::array<std::array<float,2>,5>& levels)
    {
        for(size_t i=0;i<5;++i) motion[i].level=motion[i].peak=levels[i];
    }
    // Legacy caller compatibility: a visual timer never advances audio data.
    void advance(double, const DisplayModel&) noexcept {}

private:
    std::array<Motion, 5> motion;
};
