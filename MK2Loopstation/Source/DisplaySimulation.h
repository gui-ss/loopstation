#pragma once

#include "DisplayModel.h"
#include <cmath>

// Only level/peak animation remains simulated. Progress comes from each audio track.
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

    void advance(double elapsedSeconds, const DisplayModel& model)
    {
        const auto dt = juce::jmax(0.0, elapsedSeconds);
        for (size_t i = 0; i < motion.size(); ++i)
        {
            auto& value = motion[i];
            const auto& track = model.tracks[i];
            if (track.state == TrackVisualState::blank)
            {
                value = {};
                continue;
            }
            value.seconds += dt;
            for (size_t channel = 0; channel < value.level.size(); ++channel)
            {
                const double phase = value.seconds * (2.3 + i * 0.4) + i * 1.7 + channel * 0.8;
                value.level[channel] = track.volume * static_cast<float>(0.55 + 0.4 * std::sin(phase));
                value.peak[channel] = track.volume == 0.0f ? 0.0f
                    : juce::jmax(value.level[channel], value.peak[channel] - static_cast<float>(dt * 0.35));
            }
        }
    }

private:
    std::array<Motion, 5> motion;
};
