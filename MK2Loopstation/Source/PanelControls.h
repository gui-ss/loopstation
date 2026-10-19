#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FxKnobPickup.h"

// Decorative housing reused by the four display parameter controls.
class PanelKnob final : public juce::Component
{
public:
    explicit PanelKnob(bool showPositionMarker = false);
    void paint(juce::Graphics&) override;
private:
    const bool marker;
};

// Same housing as the original large controls; only the marker becomes movable.
class PerformanceFxKnob final : public juce::Slider
{
public:
    PerformanceFxKnob();
    void paint(juce::Graphics&) override {}
    void resized() override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override { return false; }
    void bind(int identity, int effective, int maximum, FxKnobMode mode, bool available);
    bool takeMovement(int& value) { return pickup.move(getValue(), value); }
    bool waiting() const { return pickup.waiting(); }
    double effectivePosition() const { return pickup.effectivePosition(); }
private:
    PanelKnob face;
    FxKnobPickup pickup;
    int steps = 100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceFxKnob)
};

class ParameterKnob final : public juce::Slider
{
public:
    ParameterKnob();
    void paint(juce::Graphics&) override {}
    void resized() override;
    void paintOverChildren(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override { return false; }
    void setTempoMode(bool enabled) { tempoMode = enabled; setMouseDragSensitivity(enabled ? 2600 : 200); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    bool tempoMode = false;
    PanelKnob face;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterKnob)
};

class PanelButton final : public juce::Button, private juce::Timer
{
public:
    enum class Style { ledCircle, ledPill, utility, plainCircle, write };
    PanelButton(const juce::String& caption, Style, juce::Colour colour = juce::Colour(0xffbfcedc));
    void paintButton(juce::Graphics&, bool, bool) override;
    bool hitTest(int x, int y) override;
    enum class FxLight { off, red, pink, blue };
    void setFxLight(FxLight state);
    FxLight getFxLight() const { return light; }
    // Optional EDIT gesture. The expanded hit area also covers the printed EDIT label.
    void enableEditGesture() { expandedEdit = true; }
    std::function<void()> onLongPress;
    std::function<void(juce::Point<int>)> onEditDragRelease;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    bool explicitLight = false, expandedEdit = false;
    bool held = false, longPress = false, dragged = false;
    FxLight light = FxLight::off;
    const Style style;
    const juce::Colour led;
};

