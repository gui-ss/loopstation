#include "TrackComponent.h"
#include "VisualStyle.h"

TrackComponent::TrackComponent(int trackNumber)
    : number(trackNumber), loopButton(trackNumber)
{
    loopButton.setAutomaticCycleEnabled(false);
    stopButton.setClickingTogglesState(false);
    trackButton.setClickingTogglesState(false);
    trackButton.setToggleState(false, juce::dontSendNotification);
    fxButton.setToggleState(true, juce::dontSendNotification);
    fxButton.onClick = [this] { if (onFxChanged) onFxChanged(fxButton.getToggleState()); };
    trackButton.onClick = [this] { if (onSelected) onSelected(); };
    fxButton.setName("FX " + juce::String(number));
    trackButton.setName("TRACK " + juce::String(number));
    stopButton.setName("STOP " + juce::String(number));
    volume.setName("Volume " + juce::String(number));
    fxButton.setTitle(fxButton.getName());
    trackButton.setTitle(trackButton.getName());
    stopButton.setTitle(stopButton.getName());
    volume.setTitle(volume.getName());
    volume.onValueChange = [this]
    {
        if (onVolumeChanged) onVolumeChanged(static_cast<float>(volume.getValue()));
    };
    stopButton.onClick = [this]
    { if (onStopRequested) onStopRequested(stopButton.wasShiftClick()); };
    loopButton.onClick = [this]
    { if (onPrimaryRequested) onPrimaryRequested(); };
    for (auto* control : std::initializer_list<juce::Component*>
         { &fxButton, &trackButton, &stopButton, &volume, &loopButton })
        addAndMakeVisible(control);
}

void TrackComponent::applyEngineState(TrackVisualState state, bool hasPhrase)
{
    if (loopButton.getVisualState() == state && hasVisualPhrase == hasPhrase) return;
    loopButton.setVisualState(state);
    hasVisualPhrase = hasPhrase;
    trackButton.setToggleState(hasPhrase, juce::dontSendNotification);
    stopButton.setToggleState(state == TrackVisualState::blank && hasPhrase, juce::dontSendNotification);
    if (onVisualStateChanged) onVisualStateChanged(state, hasPhrase);
}

void TrackComponent::paint(juce::Graphics& g)
{
    // Shared reference grid for all five tracks.
    g.addTransform(juce::AffineTransform::scale(getWidth() / 220.0f, getHeight() / 390.0f));
    const juce::Rectangle<float> bounds(1.0f, 1.0f, 218.0f, 388.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff242529), 0.0f, 0.0f,
                                         juce::Colour(0xff101113), 220.0f, 0.0f, false));
    g.fillRoundedRectangle(bounds, 4.0f);
    VisualStyle::grain(g, bounds.reduced(4.0f), 0.024f);
    g.setColour(juce::Colour(0xff08090a));
    g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    juce::Path lip;
    lip.startNewSubPath(2.0f, 1.0f);
    lip.lineTo(218.0f, 1.0f);
    lip.lineTo(210.0f, 9.0f);
    lip.lineTo(8.0f, 9.0f);
    lip.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff54565d), 0.0f, 1.0f,
                                         juce::Colour(0xff292b30), 0.0f, 9.0f, false));
    g.fillPath(lip);
    g.setColour(juce::Colour(0xff53565b));
    g.drawLine(6.0f, 10.0f, 6.0f, 318.0f, 0.8f);
    g.drawLine(213.0f, 10.0f, 213.0f, 318.0f, 0.8f);
    g.setColour(juce::Colour(0xff292b2f));
    g.drawLine(8.0f, 387.0f, 211.0f, 387.0f, 1.0f);

    // Preserve the printed hardware label; all tracks use guarded Shift-click to clear.
    g.setColour(juce::Colour(0xffe2e4e6));
    g.setFont(juce::FontOptions("Arial", 6.5f, juce::Font::bold));
    g.drawText("CLEAR: HOLD", 32, 133, 66, 12, juce::Justification::centred);
    VisualStyle::trackNumber(g, number, { 28.0f, 219.0f, 23.0f, 16.0f });
}

void TrackComponent::resized()
{
    const auto place = [this](juce::Component& control, juce::Rectangle<float> bounds)
    {
        control.setBounds(bounds.transformedBy(juce::AffineTransform::scale(
            getWidth() / 220.0f, getHeight() / 390.0f)).toNearestInt());
    };
    place(fxButton,    { 29.0f, 25.0f, 74.0f, 39.0f });
    place(trackButton, { 29.0f, 80.0f, 74.0f, 39.0f });
    place(stopButton,  { 30.0f, 149.0f, 70.0f, 66.0f });
    place(volume,     { 132.0f, 17.0f, 70.0f, 193.0f });
    place(loopButton, { 27.0f, 214.0f, 174.0f, 174.0f });
}
