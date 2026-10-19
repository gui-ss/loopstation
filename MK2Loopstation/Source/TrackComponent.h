#pragma once

#include "TrackControls.h"

class TrackComponent final : public juce::Component
{
public:
    explicit TrackComponent(int trackNumber);
    void applyFxEnabled(bool enabled) { fxButton.setToggleState(enabled, juce::dontSendNotification); }
    std::function<void(bool)> onFxChanged;
    void applyIndicator(float fill) { loopButton.setIndicatorFill(fill); }
    void applyEngineState(TrackVisualState, bool hasPhrase);
    void paint(juce::Graphics&) override;
    void resized() override;
    std::function<void(TrackVisualState, bool)> onVisualStateChanged;
    std::function<void(float)> onVolumeChanged;
    std::function<void()> onPrimaryRequested;
    std::function<void()> onSelected;
    std::function<void(bool clear)> onStopRequested;

private:
    const int number;
    LedPillButton fxButton { "FX", juce::Colour(0xffff4057) };
    LedPillButton trackButton { "TRACK", juce::Colour(0xff35efa0) };
    StopButton stopButton;
    VolumeFader volume;
    LedRingButton loopButton;
    bool hasVisualPhrase = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
