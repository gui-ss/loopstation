#include "MainComponent.h"
#include "VisualStyle.h"

MainComponent::MainComponent(AudioSystem& system) : audio(system)
{
    setOpaque(true);
    addAndMakeVisible(topPanel);
    topPanel.onInputRobotChanged = [this](int slot, RobotParameters p) { audio.setInputRobot(slot, p); };
    topPanel.onInputSequenceChanged = [this](int slot, FxSequenceParameters p) { audio.setInputSequence(slot,p); };
    topPanel.onInputOscBotChanged = [this](int slot, OscBotParameters p) { audio.setInputOscBot(slot, p); };
    topPanel.onInputActivationChanged = [this](uint32_t mask,uint32_t robots,uint32_t oscs,bool restart)
    { audio.setInputActivation(mask,robots,oscs,restart); };
    topPanel.publishInputFx();
    topPanel.onTrackFxChanged = [this](TrackFxParameters value) { audio.setTrackEffects(value); };
    topPanel.onTrackInsertChanged = [this](int slot, int target) { audio.setTrackFxInsert(slot, target); timerCallback(); };
    topPanel.onTrackFxEnabled = [this](int index, bool enabled) { audio.setTrackFxEnabled(index, enabled); timerCallback(); };
    topPanel.onTapTempo = [this] { audio.tapTempo(); timerCallback(); };
    topPanel.onBpmChanged = [this](double value) { audio.setBpm(value); timerCallback(); };
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        tracks[i] = std::make_unique<TrackComponent>(static_cast<int>(i) + 1);
        tracks[i]->onVisualStateChanged = [this, i](TrackVisualState state, bool hasPhrase)
        {
            topPanel.updateTrack(static_cast<int>(i), state, hasPhrase);
        };
        tracks[i]->onVolumeChanged = [this, i](float volume)
        {
            topPanel.updateVolume(static_cast<int>(i), volume);
            audio.setTrackVolume(static_cast<int>(i), volume);
        };
        tracks[i]->onPrimaryRequested = [this, i] { requestTrack(static_cast<int>(i), LoopTrack::Command::primary); };
        tracks[i]->onStopRequested = [this, i](bool clear)
        { requestTrack(static_cast<int>(i), clear ? LoopTrack::Command::clear : LoopTrack::Command::stop); };
        tracks[i]->onSelected = [this, i] { selectTrack(static_cast<int>(i)); topPanel.openTrackSettings(); };
        tracks[i]->onFxChanged = [this, i](bool enabled) { audio.setTrackFxEnabled(static_cast<int>(i), enabled); timerCallback(); };
        addAndMakeVisible(*tracks[i]);
    }
    startTimerHz(30);
    setSize(1200, 680);
}

namespace
{
TrackVisualState visualState(LoopTrack::State state)
{
    if (state == LoopTrack::State::recording) return TrackVisualState::rec;
    if (state == LoopTrack::State::playing) return TrackVisualState::play;
    if (state == LoopTrack::State::overdubbing) return TrackVisualState::overdub;
    return TrackVisualState::blank;
}
}

void MainComponent::timerCallback()
{
    std::array<float, AudioSystem::trackCount> positions {};
    std::array<bool, AudioSystem::trackCount> fxEnabled {};
    for (int i = 0; i < AudioSystem::trackCount; ++i)
    {
        const auto state = audio.trackSnapshot(i);
        tracks[static_cast<size_t>(i)]->applyEngineState(visualState(state.state), state.hasPhrase());
        positions[static_cast<size_t>(i)] = state.progress;
        fxEnabled[size_t(i)] = audio.trackFxEnabled(i);
        tracks[size_t(i)]->applyFxEnabled(fxEnabled[size_t(i)]);
    }
    const auto selected = audio.trackSnapshot(audio.selectedTrack());
    topPanel.selectTrack(audio.selectedTrack());
    topPanel.updateRouting(fxEnabled, audio.trackFxInsert(0), audio.trackFxInsert(2));
    topPanel.updateClock(audio.bpm(), audio.musicalPosition().beats, positions, audio.clockRunning());
    const bool selectedSync = audio.loopSyncEnabled(audio.selectedTrack());
    topPanel.updateQuantize(audio.quantizeEnabled(), selected.state == LoopTrack::State::recording && ! selectedSync,
                            selected.recordingQuantized, selected.awaitingBoundary && ! selectedSync);
    const auto sync = audio.trackSyncStatus(audio.selectedTrack());
    const char* notice = "";
    if (sync.waiting == LoopTrack::SyncWait::record) notice = "WAIT REC";
    else if (sync.waiting == LoopTrack::SyncWait::play) notice = "WAIT PLAY";
    else if (selected.awaitingBoundary && audio.loopSyncEnabled(audio.selectedTrack())) notice = "REC WAIT END";
    else switch (sync.issue)
    {
        case LoopTrack::SyncIssue::incompatible: notice = "INCOMPAT: L OFF"; break;
        case LoopTrack::SyncIssue::stopFirst: notice = "STOP FIRST"; break;
        case LoopTrack::SyncIssue::sampleGrid: notice = "RATE GRID: L OFF"; break;
        case LoopTrack::SyncIssue::capacity: notice = "BAR > CAPACITY"; break;
        case LoopTrack::SyncIssue::none: break;
    }
    topPanel.updateLoopSync(audio.loopSyncEnabled(audio.selectedTrack()), notice, audio.bpmChangeBlocked());
}

void MainComponent::selectTrack(int index)
{
    audio.selectTrack(index);
    timerCallback();
}

void MainComponent::requestTrack(int index, LoopTrack::Command command)
{
    audio.selectTrack(index);
    audio.commandTrack(index, command);
    timerCallback();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101114));
    juce::Graphics::ScopedSaveState savedState(g);
    g.addTransform(juce::AffineTransform::scale(panelBounds.getWidth() / panelWidth)
                       .translated(panelBounds.getX(), panelBounds.getY()));

    const juce::Rectangle<float> chassis(2.0f, 2.0f, 1196.0f, 650.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff45474e), 0.0f, 0.0f,
                                         juce::Colour(0xff131417), 0.0f, 660.0f, false));
    g.fillRoundedRectangle(chassis, 20.0f);
    g.setColour(juce::Colour(0xff0a0b0d));
    g.fillRoundedRectangle(chassis.reduced(11.0f), 13.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26272c), 0.0f, 15.0f,
                                         juce::Colour(0xff17191d), 0.0f, 646.0f, false));
    g.fillRoundedRectangle(chassis.reduced(14.0f), 11.0f);
    VisualStyle::grain(g, chassis.reduced(20.0f), 0.023f);
    g.setColour(juce::Colour(0xff60636a));
    g.drawRoundedRectangle(chassis.reduced(1.0f), 19.0f, 0.6f);

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd5d8e1), 0.0f, 248.0f,
                                         juce::Colour(0xff7e8390), 0.0f, 255.0f, false));
    g.fillRect(16.0f, 248.0f, 1168.0f, 7.0f);
    g.setColour(juce::Colour(0xffe0e1e5));
    g.fillRect(16.0f, 248.0f, 1168.0f, 1.0f);
}

void MainComponent::resized()
{
    const auto available = getLocalBounds().toFloat().reduced(8.0f);
    const auto scale = juce::jmin(available.getWidth() / panelWidth,
                                 available.getHeight() / panelHeight);
    panelBounds = available.withSizeKeepingCentre(panelWidth * scale, panelHeight * scale);
    topPanel.setBounds(juce::Rectangle<float>(0.0f, 0.0f, panelWidth, 248.0f)
                           .transformedBy(juce::AffineTransform::scale(scale)
                               .translated(panelBounds.getX(), panelBounds.getY())).toNearestInt());

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const juce::Rectangle<float> trackBounds(48.0f + static_cast<float>(i) * 221.0f,
                                                 258.0f, 220.0f, 390.0f);
        tracks[i]->setBounds(trackBounds.transformedBy(juce::AffineTransform::scale(scale)
                                  .translated(panelBounds.getX(), panelBounds.getY())).toNearestInt());
    }
}
