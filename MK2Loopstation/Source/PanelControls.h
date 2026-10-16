#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Decorative housing reused by the four display parameter controls.
class PanelKnob final : public juce::Component
{
public:
    explicit PanelKnob(bool showPositionMarker = false);
    void paint(juce::Graphics&) override;
private:
    const bool marker;
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

class PanelButton final : public juce::Button
{
public:
    enum class Style { ledCircle, ledPill, utility, plainCircle, write };
    PanelButton(const juce::String& caption, Style, juce::Colour colour = juce::Colour(0xffbfcedc));
    void paintButton(juce::Graphics&, bool, bool) override;
    bool hitTest(int x, int y) override;
private:
    const Style style;
    const juce::Colour led;
};

