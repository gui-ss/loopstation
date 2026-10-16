#pragma once

#include "AudioSystem.h"
#include <juce_gui_basics/juce_gui_basics.h>

class AudioSettingsWindow final : public juce::DocumentWindow
{
public:
    explicit AudioSettingsWindow(AudioSystem&);
    void closeButtonPressed() override { setVisible(false); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress(juce::KeyPress::F5Key)) return false;
        toFront(true);
        return true;
    }
};
