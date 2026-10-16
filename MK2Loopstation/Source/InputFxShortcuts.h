#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

// Local to the main DocumentWindow, never registered with the OS/global desktop.
class InputFxShortcuts
{
public:
    inline static const std::array<int,5> codes {juce::KeyPress::F6Key,juce::KeyPress::F7Key,
        juce::KeyPress::F8Key,juce::KeyPress::F9Key,juce::KeyPress::F10Key};
    std::function<void(int)> onPress;
    bool keyPressed(const juce::KeyPress& key, juce::Component* focused, bool active)
    {
        const auto found=std::find(codes.begin(),codes.end(),key.getKeyCode());
        if(found==codes.end() || key.getModifiers().getRawFlags()!=0 || !active) return false;
        const auto index=int(found-codes.begin());
        const auto bit=1u<<index;
        const bool repeated=(held&bit)!=0;
        held |= bit;
        for(auto* c=focused;c!=nullptr;c=c->getParentComponent())
            if(auto* editor=dynamic_cast<juce::TextEditor*>(c);editor!=nullptr && !editor->isReadOnly()) return false;
        if(!repeated && onPress) onPress(index);
        return true;
    }
    // Explicit mask makes press/release/focus behaviour deterministic in tests.
    void updateDownKeys(unsigned down) noexcept { held &= down; }
    void focusChanged(unsigned down) noexcept { held=down; }
    static unsigned downKeys()
    {
        unsigned mask=0;
        for(size_t i=0;i<codes.size();++i) if(juce::KeyPress::isKeyCurrentlyDown(codes[i])) mask |= 1u<<i;
        return mask;
    }
private:
    unsigned held=0;
};
