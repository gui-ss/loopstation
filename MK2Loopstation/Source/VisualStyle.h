#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace VisualStyle
{
void prepareToggle(juce::Button&);
void ledStroke(juce::Graphics&, const juce::Path&, float width, juce::Colour, bool lit);
void grain(juce::Graphics&, juce::Rectangle<float>, float opacity);
void trackNumber(juce::Graphics&, int number, juce::Rectangle<float>);
void lcdText(juce::Graphics&, const juce::String&, juce::Rectangle<float>, float pixelSize);
}
