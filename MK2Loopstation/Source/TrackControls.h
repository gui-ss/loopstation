#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DisplayModel.h"

// Drawing is shared by engine-driven and demonstration tracks.
class LedPillButton final : public juce::Button
{
public:
    LedPillButton(const juce::String& label, juce::Colour ledColour);
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    const juce::Colour led;
};

class StopButton final : public juce::Button
{
public:
    StopButton();
    void paintButton(juce::Graphics&, bool, bool) override;
    bool hitTest(int x, int y) override;
    bool wasShiftClick() const { return shiftClick; }
private:
    void clicked(const juce::ModifierKeys& mods) override { shiftClick = mods.isShiftDown(); }
    bool shiftClick = false;
};

class LedRingButton final : public juce::Button
{
public:
    using VisualState = TrackVisualState;
    explicit LedRingButton(int trackNumber);
    void setVisualState(VisualState);
    void setIndicatorFill(float fill) { if (indicator != fill) { indicator = fill; repaint(); } }
    float getIndicatorFill() const noexcept { return indicator; }
    VisualState getVisualState() const noexcept { return state; }
    void paintButton(juce::Graphics&, bool, bool) override;
    bool hitTest(int x, int y) override;
    void setAutomaticCycleEnabled(bool enabled) { automaticCycle = enabled; }
private:
    void clicked() override;
    const int track;
    VisualState state = VisualState::blank;
    bool automaticCycle = true;
    float indicator = 0;
};

class VolumeFader final : public juce::Slider
{
public:
    VolumeFader();
    ~VolumeFader() override;
private:
    class FaderLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        int getSliderThumbRadius(juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&, int, int, int, int,
                              float, float, float, juce::Slider::SliderStyle,
                              juce::Slider&) override;
    };
    FaderLookAndFeel appearance;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VolumeFader)
};
