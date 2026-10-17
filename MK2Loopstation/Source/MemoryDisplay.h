#pragma once
#pragma once
#include "DisplayModel.h"
#include "DisplaySimulation.h"
#include <juce_gui_basics/juce_gui_basics.h>

class MemoryDisplay final : public juce::Component, private juce::Timer
{
public:
    MemoryDisplay();
    ~MemoryDisplay() override;
    void navigate(int direction);
    void updateTrack(int index, TrackVisualState state, bool hasPhrase);
    void updateEffect(EffectGroup group, int index, bool enabled);
    void updateVolume(int index, float volume);
    void updateClock(double bpm, const std::array<float, 5>& loopProgress);
    void selectTrack(int index);
    void setTrackSettings(bool);
    bool isTrackSettings() const { return ! tempoEditing && trackSettings; }
    bool isRoutingEditing() const { return isTrackEditing() && routingEditing; }
    void toggleRoutingEditing();
    void updateRouting(const std::array<bool, 5>& enabled, int a, int c);
    int selectedTrack() const { return model.currentTrack; }
    int editedSlot() const { return model.trackEditorSlot; }
    int insertDestination() const { return inserts[model.trackEditorSlot == 2 ? 1 : 0]; }
    bool selectedFxEnabled() const { return trackFxEnabled[size_t(model.currentTrack)]; }
    juce::String insertName() const { return insertDestination() == 0 ? "ALL" : "TRACK" + juce::String(insertDestination()); }
    void updateQuantize(bool enabled, bool recording, bool effective, bool waiting);
    void updateLoopSync(bool enabled, const juce::String& notice, bool bpmBlocked);
    double bpm() const { return model.bpm; } // Read-only UI mirror of AudioSystem.
    void setTempoEditing(bool);
    bool isTempoEditing() const { return tempoEditing; }
    void setInputEditing(bool editing);
    bool isInputEditing() const { return ! tempoEditing && model.inputEditor.open; }
    bool isTrackEditing() const { return ! tempoEditing && model.trackEditorOpen; }
    void setTrackEditing(bool);
    RobotParameters inputRobot(int slot) const
    {
        if (! juce::isPositiveAndBelow(slot, 4)) return {};
        auto value = model.inputEditor.slots[size_t(slot)].robot;
        value.enabled = model.inputEditor.slots[size_t(slot)].type == 4 && model.inputEffects[size_t(slot)].enabled;
        return value;
    }
    OscBotParameters inputOscBot(int slot) const
    {
        if (!juce::isPositiveAndBelow(slot, 4)) return {};
        auto value = model.inputEditor.slots[size_t(slot)].oscBot;
        value.enabled = model.inputEditor.slots[size_t(slot)].type == 5 && model.inputEffects[size_t(slot)].enabled;
        return value;
    }
    ReverbParameters inputReverb(int slot) const
    {
        if (! juce::isPositiveAndBelow(slot, 4)) return {};
        auto value = model.inputEditor.slots[size_t(slot)].reverb;
        value.enabled = model.inputEditor.slots[size_t(slot)].type == 6 && model.inputEffects[size_t(slot)].enabled;
        return value;
    }
    FxSequenceParameters inputSequence(int slot) const
    {
        if (! juce::isPositiveAndBelow(slot, 4)) return {};
        auto value = model.inputEditor.slots[size_t(slot)].sequence;
        return value;
    }
    bool inputMulti() const { return model.inputEditor.multi; }
    int inputType(int slot) const { return model.inputEditor.slots[size_t(slot)].type; }
    bool inputEnabled(int slot) const { return model.inputEffects[size_t(slot)].enabled; }
    int selectedInputSlot() const { return model.inputEditor.selectedSlot; }
    TrackFxParameters getTrackFx() const { return model.trackFx; }
    juce::String getTrackSlotLetter() const { return model.trackEditorSlot == 2 ? "C" : "A"; }
    InputFxEditor::Field getEditorField(int knob) const;
    void setEditorField(int knob, int value);
    void selectInputSlot(int index);
    juce::String getInputSlotLetter() const { return model.inputEditor.slotLetter(); }
    void paint(juce::Graphics&) override;

private:
    void updateAccessibleDescription();
    void paintLcd(juce::Graphics&);
    void paintQuantize(juce::Graphics&);
    void paintLoopSync(juce::Graphics&);
    juce::String quantizeText() const;
    void paintEffects(juce::Graphics&, EffectGroup);
    void paintTrackIcons(juce::Graphics&, bool showProgress);
    void paintLoopStatus(juce::Graphics&);
    void paintLoopLevel(juce::Graphics&);
    void paintInputEditor(juce::Graphics&);
    void timerCallback() override;
    DisplayModel model;
    DisplaySimulation simulation;
    double lastSimulationTime = 0.0;
    bool tempoEditing = false, trackSettings = false, routingEditing = false;
    std::array<bool, 5> trackFxEnabled {{ true, true, true, true, true }};
    std::array<int, 2> inserts {{ 1, 1 }};
    bool quantizeEnabled = false, quantizeRecording = false, quantizeEffective = false, quantizeWaiting = false;
    double quantizeNoticeUntil = 0, syncNoticeUntil = 0;
    bool loopSync = false, tempoBlocked = false;
    juce::String syncNotice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MemoryDisplay)
};
