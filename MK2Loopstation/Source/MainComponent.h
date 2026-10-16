#pragma once

#include "TrackComponent.h"
#include "TopPanelComponent.h"
#include "AudioSystem.h"
#include <array>
#include <memory>

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent(AudioSystem&);
    ~MainComponent() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void requestTrack(int index, LoopTrack::Command);
    void selectTrack(int index);
    AudioSystem& audio;
    // Coordinates describe the chassis, excluding the white photograph margins.
    static constexpr float panelWidth = 1200.0f;
    static constexpr float panelHeight = 660.0f;
    juce::Rectangle<float> panelBounds;
    TopPanelComponent topPanel;
    std::array<std::unique_ptr<TrackComponent>, 5> tracks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
