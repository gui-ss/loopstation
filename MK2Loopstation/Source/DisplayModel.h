#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include "InputFxEditor.h"
#include "TrackFxParameters.h"

enum class TrackVisualState { blank, rec, play, overdub };
enum class EffectGroup { input, track };

struct EffectDisplayStatus
{
    juce::String name;
    std::array<juce::String, 2> lines;
    bool enabled = false; // TRACK FX A/C control real effects; other slots remain visual.
};

struct TrackDisplayStatus
{
    TrackVisualState state = TrackVisualState::blank;
    // Phrase presence from the engine for all five tracks.
    bool hasPhrase = false;
    float volume = 0.88f;
};

struct DisplayModel
{
    enum class Screen { memoryNumber, trackStatus, loopTracks, loopStatus, loopLevel, inputFx, trackFx };
    inline static constexpr std::array<Screen, 7> screenOrder {
        Screen::memoryNumber, Screen::trackStatus, Screen::loopTracks,
        Screen::loopStatus, Screen::loopLevel, Screen::inputFx, Screen::trackFx
    };
    Screen screen = Screen::memoryNumber;
    InputFxEditor inputEditor;
    bool trackEditorOpen = false;
    int trackEditorSlot = 0; // A/C have DSP; B/D remain accessible visual placeholders.
    TrackFxParameters trackFx;
    int memoryNumber = 1;
    juce::String memoryName = "Memory01";
    double bpm = 120.0;
    std::array<TrackDisplayStatus, 5> tracks;
    int currentTrack = 0;
    // TRACK FX A/C have audio processors; other slots remain visual.
    std::array<EffectDisplayStatus, 4> inputEffects {{
        { "LPF", { "LPF", "" } },
        { "BPF", { "BPF", "" } },
        { "HPF", { "HPF", "" } },
        { "PHASER", { "PHA", "SER" } }
    }};
    std::array<EffectDisplayStatus, 4> trackEffects {{
        { "LPF", { "LPF", "" } },
        { "FLANGER", { "FLN", "GER" } },
        { "DISTORTION", { "DIST", "ORT" } },
        { "GRANULAR DELAY", { "GRN", "DLY" } }
    }};
};
