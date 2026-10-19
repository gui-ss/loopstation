#pragma once

#include "PanelControls.h"
#include "MemoryDisplay.h"
#include <memory>
#include <vector>

class TopPanelComponent final : public juce::Component
{
public:
    TopPanelComponent();
    void paint(juce::Graphics&) override;
    void resized() override;
    void updateTrack(int index, TrackVisualState state, bool hasPhrase);
    void updateVolume(int index, float volume);
    void selectTrack(int index) { display.selectTrack(index); if (display.isTrackSettings()) refreshParameterKnobs(); }
    void openTrackSettings();
    IndicatorMode indicatorMode() const { return display.indicatorMode(); }
    void updateLevels(const std::array<std::array<float,2>,5>& levels) { display.updateLevels(levels); }
    void updateRouting(const std::array<bool, 5>& enabled, int a, int c)
    { display.updateRouting(enabled, a, c); if (display.isTrackSettings() || display.isRoutingEditing()) refreshParameterKnobs(); }
    std::function<void(int, bool)> onTrackFxEnabled;
    std::function<void(int, int)> onTrackInsertChanged;
    std::function<void(TrackFxParameters)> onTrackFxChanged;
    std::function<void(TrackFxParameters,int,int)> onTrackBankChanged;
    std::function<void(int, RobotParameters)> onInputRobotChanged;
    std::function<void(int, OscBotParameters)> onInputOscBotChanged;
    std::function<void(int, FxSequenceParameters)> onInputSequenceChanged;
    std::function<void(int, ReverbParameters)> onInputReverbChanged;
    std::function<void(uint32_t, uint32_t, uint32_t, bool)> onInputActivationChanged;
    std::function<void(int,StepSlicerParameters)> onInputStepSlicerChanged;
    std::function<void(int,G2BParameters)> onInputG2BChanged;
    std::function<void(uint32_t,uint32_t,uint32_t,bool,int,uint32_t,uint32_t)> onInputBankActivationG2BChanged;
    std::function<void(uint32_t,uint32_t,uint32_t,bool,int,uint32_t)> onInputBankActivationChanged;
    int inputBank() const { return display.inputBank(); }
    void activateInputSlot(int slot);
    bool toggleInputPair(); // No-op in SINGLE; one publication in MULTI.
    void publishInputFx(bool restartPair = false);
    std::function<void()> onTapTempo;
    std::function<void(double)> onBpmChanged;
    void updateClock(double bpm, double beats, const std::array<float, 5>& loopProgress, bool running);
    void updateLoopSync(bool enabled, const juce::String& notice, bool blocked)
    { display.updateLoopSync(enabled, notice, blocked); }
    void updateQuantize(bool enabled, bool recording, bool effective, bool waiting)
    { display.updateQuantize(enabled, recording, effective, waiting); }
private:
    struct PlacedControl
    {
        std::unique_ptr<juce::Component> component;
        juce::Rectangle<float> bounds;
    };
    void addControl(std::unique_ptr<juce::Component>, juce::Rectangle<float>);
    void setInputEditing(bool);
    void editClick(EffectGroup);
    void beginBankSelect(EffectGroup);
    void activateFxSlot(EffectGroup, int, bool editOnly=false);
    void refreshFxButtons();
    void cancelBankSelect();
    void setTrackEditing(bool);
    void refreshParameterKnobs();
    void refreshPerformanceKnobs();
    std::array<PerformanceFxKnob*,2> performanceKnobs {};
    void setTempoEditing(bool);
    std::vector<PlacedControl> controls;
    std::array<ParameterKnob*, 4> parameterKnobs {};
    PanelButton* inputEditButton = nullptr;
    PanelButton* trackEditButton = nullptr;
    PanelButton* tapButton = nullptr;
    std::array<PanelButton*, 4> inputFxButtons {}, trackFxButtons {};
    MemoryDisplay display;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopPanelComponent)
};
