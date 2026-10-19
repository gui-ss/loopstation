#pragma once
#include "DisplayModel.h"
#include "DisplaySimulation.h"
#include "TrackIndicator.h"
#include "FxKnobPickup.h"
#include <juce_gui_basics/juce_gui_basics.h>

class MemoryDisplay final : public juce::Component
{
public:
    MemoryDisplay();
    ~MemoryDisplay() override = default;
    void navigate(int direction);
    FxKnobMode fxKnobMode() const { return knobMode; }
    int fxTarget(EffectGroup group) const { return group==EffectGroup::input ? model.inputEditor.fxTarget() : trackFxTarget; }
    int performanceIdentity(EffectGroup group) const;
    juce::String performanceName(EffectGroup group) const;
    InputFxEditor::Field performanceField(EffectGroup group) const;
    void setPerformanceValue(EffectGroup group, int value);
    void showPerformanceValue(EffectGroup group, bool waiting, double physical);
    void clearPerformanceNotice() { performanceUntil=0; updateAccessibleDescription(); repaint(); }

    void setIndicatorEditing(bool open);
    bool isIndicatorEditing() const { return indicatorEditing; }
    IndicatorMode indicatorMode() const { return indicator; }
    void updateLevels(const std::array<std::array<float,2>,5>& levels) { simulation.updateLevels(levels); if(model.screen==DisplayModel::Screen::loopLevel) repaint(); }
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
    StepSlicerParameters inputStepSlicer(int slot) const
    {
        if(!juce::isPositiveAndBelow(slot,4)) return {};
        auto p=model.inputEditor.slots[size_t(slot)].slicer;
        p.enabled=model.inputEditor.slots[size_t(slot)].type==7 && model.inputEffects[size_t(slot)].enabled;
        return p;
    }
    G2BParameters inputG2B(int slot) const
    {
        if(!juce::isPositiveAndBelow(slot,4)) return {};
        auto p=model.inputEditor.slots[size_t(slot)].g2b;
        p.enabled=model.inputEditor.slots[size_t(slot)].type==8 && model.inputEffects[size_t(slot)].enabled;
        return p;
    }
    int trackBank() const { return activeTrackBank; }
    int fxBank(EffectGroup group) const { return group==EffectGroup::input ? inputBank() : trackBank(); }
    juce::String trackBankLetter() const { return juce::String::charToString(juce::juce_wchar('A'+activeTrackBank)); }
    void selectFxBank(EffectGroup, int);
    void selectTrackSlot(int);
    bool trackEnabled(int slot) const { return model.trackEffects[size_t(slot)].enabled; }
    void setBankSelecting(EffectGroup group, bool enabled);
    bool bankSelecting(EffectGroup group) const { return bankSelect[size_t(group)]; }
    int trackInsert(int slot) const { return inserts[slot==2?1:0]; }
    int inputBank() const { return model.inputEditor.bank; }
    bool inputMulti() const { return model.inputEditor.multi; }
    int inputType(int slot) const { return model.inputEditor.slots[size_t(slot)].type; }
    bool inputEnabled(int slot) const { return model.inputEffects[size_t(slot)].enabled; }
    int selectedInputSlot() const { return model.inputEditor.selectedSlot; }
    TrackFxParameters getTrackFx() const { return model.trackFx; }
    juce::String getTrackSlotLetter() const { return juce::String::charToString(juce::juce_wchar('A'+model.trackEditorSlot)); }
    InputFxEditor::Field getEditorField(int knob) const;
    void setEditorField(int knob, int value);
    void selectInputSlot(int index);
    juce::String getInputSlotLetter() const { return model.inputEditor.slotLetter(); }
    void paint(juce::Graphics&) override;

private:
    void updateAccessibleDescription();
    void paintLcd(juce::Graphics&);
    void paintPerformanceValue(juce::Graphics&);
    void paintQuantize(juce::Graphics&);
    void paintLoopSync(juce::Graphics&);
    juce::String quantizeText() const;
    void paintEffects(juce::Graphics&, EffectGroup);
    void paintTrackIcons(juce::Graphics&, bool showProgress);
    void paintLoopStatus(juce::Graphics&);
    void paintLoopLevel(juce::Graphics&);
    void paintInputEditor(juce::Graphics&);
    DisplayModel model;
    std::array<std::array<bool,4>,4> inputBankEnabled{};
    struct TrackBank
    {
        TrackFxParameters parameters;
        std::array<bool,4> enabled {};
        std::array<int,2> destinations {{1,1}};
        int target=0, slot=0;
    };
    std::array<TrackBank,4> trackBanks;
    int activeTrackBank=0;
    std::array<bool,2> bankSelect {};
    DisplaySimulation simulation;
    FxKnobMode knobMode=FxKnobMode::immediate;
    int setupPage=0, trackFxTarget=0;
    double performanceUntil=0;
    EffectGroup performanceGroup=EffectGroup::input;
    bool performanceWaiting=false;
    int performancePhysical=0;
    bool indicatorEditing=false;
    IndicatorMode indicator=IndicatorMode::status;
    bool tempoEditing = false, trackSettings = false, routingEditing = false;
    std::array<bool, 5> trackFxEnabled {{ true, true, true, true, true }};
    std::array<int, 2> inserts {{ 1, 1 }};
    bool quantizeEnabled = false, quantizeRecording = false, quantizeEffective = false, quantizeWaiting = false;
    double quantizeNoticeUntil = 0, syncNoticeUntil = 0;
    bool loopSync = false, tempoBlocked = false;
    juce::String syncNotice;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MemoryDisplay)
};
