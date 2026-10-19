#include "TrackControls.h"
#include "VisualStyle.h"

namespace
{
void makeVisualOnly(juce::Button& button)
{
    button.setWantsKeyboardFocus(false);
    button.setMouseClickGrabsKeyboardFocus(false);
}

bool circularHitTest(const juce::Component& component, int x, int y)
{
    return component.getLocalBounds().toFloat().getCentre().getDistanceFrom(
               { static_cast<float>(x), static_cast<float>(y) })
           <= juce::jmin(component.getWidth(), component.getHeight()) * 0.5f;
}

void useCircularCanvas(juce::Graphics& g, const juce::Component& component)
{
    const auto side = static_cast<float>(juce::jmin(component.getWidth(), component.getHeight()));
    g.addTransform(juce::AffineTransform::scale(side / 174.0f).translated(
        (component.getWidth() - side) * 0.5f, (component.getHeight() - side) * 0.5f));
}

juce::Path arc(float radius, float start, float end)
{
    juce::Path path;
    path.addCentredArc(87.0f, 87.0f, radius, radius, 0.0f,
                      juce::degreesToRadians(start), juce::degreesToRadians(end), true);
    return path;
}
}

LedPillButton::LedPillButton(const juce::String& label, juce::Colour ledColour)
    : Button(label), led(ledColour)
{
    setButtonText(label);
    VisualStyle::prepareToggle(*this);
}

void LedPillButton::paintButton(juce::Graphics& g, bool, bool down)
{
    g.addTransform(juce::AffineTransform::scale(getWidth() / 72.0f, getHeight() / 39.0f));
    if (down) g.addTransform(juce::AffineTransform::translation(0, 1.0f));
    const auto active = getToggleState();
    const juce::Rectangle<float> rim(6.0f, 6.0f, 60.0f, 27.0f);
    g.setColour(juce::Colour(0xff090b0e));
    g.fillRoundedRectangle(rim.expanded(4.0f), 18.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff34373d), 0.0f, 5.0f,
                                         juce::Colour(0xff111216), 0.0f, 32.0f, false));
    g.fillRoundedRectangle(rim, 13.5f);
    if (active)
    {
        g.setColour(led.withAlpha(0.22f));
        g.fillRoundedRectangle(rim.reduced(3.0f), 10.5f);
    }
    juce::Path diffuser;
    diffuser.addRoundedRectangle(rim, 13.5f);
    VisualStyle::ledStroke(g, diffuser, active ? 4.6f : 3.5f, active ? led : led.darker(0.5f), true);
    g.setColour(juce::Colour(0xff06080a));
    g.drawRoundedRectangle(rim.reduced(3.0f), 10.5f, 1.0f);
    g.setColour(led.interpolatedWith(juce::Colours::white, 0.78f));
    g.setFont(juce::FontOptions("Arial", 11.5f, juce::Font::bold));
    g.drawText(getButtonText(), 7, 7, 58, 25, juce::Justification::centred);
}

StopButton::StopButton() : Button("STOP") { VisualStyle::prepareToggle(*this); }
bool StopButton::hitTest(int x, int y) { return circularHitTest(*this, x, y); }

void StopButton::paintButton(juce::Graphics& g, bool, bool down)
{
    useCircularCanvas(g, *this);
    if (down) g.addTransform(juce::AffineTransform::translation(0, 2.0f));
    g.setColour(juce::Colour(0xff090a0d));
    g.fillEllipse(0.0f, 0.0f, 174.0f, 174.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe0edf6), 0.0f, 8.0f,
                                         juce::Colour(0xff90a6b9), 0.0f, 169.0f, false));
    g.fillEllipse(7.0f, 7.0f, 160.0f, 160.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26282d), 0.0f, 20.0f,
                                         juce::Colour(0xff15171b), 0.0f, 156.0f, false));
    g.fillEllipse(20.0f, 20.0f, 134.0f, 134.0f);
    if (getToggleState())
    {
        juce::Path ring;
        ring.addEllipse(13.0f, 13.0f, 148.0f, 148.0f);
        VisualStyle::ledStroke(g, ring, 10.0f, juce::Colour(0xffb9e6ff), true);
        g.setColour(juce::Colour(0xff344b5a));
        g.fillEllipse(24.0f, 24.0f, 126.0f, 126.0f);
    }
    g.setColour(juce::Colour(0xff07090b));
    g.drawEllipse(22.0f, 22.0f, 130.0f, 130.0f, 2.0f);
    g.setColour(juce::Colour(0xffa7bacb));
    g.fillRect(69.0f, 69.0f, 36.0f, 36.0f);
    g.setColour(juce::Colour(0xffc0d0df));
    g.drawRect(69.0f, 69.0f, 36.0f, 36.0f, 2.0f);
}

LedRingButton::LedRingButton(int trackNumber)
    : Button("Loop " + juce::String(trackNumber)), track(trackNumber)
{
    makeVisualOnly(*this);
    setVisualState(VisualState::blank);
}

void LedRingButton::setVisualState(VisualState newState)
{
    state = newState;
    const char* labels[] = { "STOP/BLANK", "REC", "PLAY", "OVERDUB" };
    setTitle("Loop " + juce::String(track) + " - " + labels[static_cast<int>(state)]);
    repaint();
}

void LedRingButton::clicked()
{
    if (automaticCycle) setVisualState(static_cast<VisualState>((static_cast<int>(state) + 1) % 4));
}
bool LedRingButton::hitTest(int x, int y) { return circularHitTest(*this, x, y); }

void LedRingButton::paintButton(juce::Graphics& g, bool, bool down)
{
    useCircularCanvas(g, *this);
    if (down) g.addTransform(juce::AffineTransform::scale(0.985f, 0.985f, 87.0f, 87.0f));
    const bool lit = state != VisualState::blank;
    const juce::Colour colours[] = { juce::Colour(0xff4a515c), juce::Colour(0xffff2549),
                                     juce::Colour(0xff27f6a0), juce::Colour(0xffffb52e) };
    const auto colour = colours[static_cast<int>(state)];
    const juce::Rectangle<float> bounds(0.0f, 0.0f, 174.0f, 174.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4b4d54), 0.0f, 0.0f,
                                         juce::Colour(0xff08090b), 0.0f, 174.0f, false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xff060709));
    g.fillEllipse(bounds.reduced(3.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff27292f), 0.0f, 8.0f,
                                         juce::Colour(0xff101115), 0.0f, 166.0f, false));
    g.fillEllipse(bounds.reduced(8.0f));
    g.setColour(juce::Colour(0xff929aa5));
    g.strokePath(arc(85.0f, 266.0f, 307.0f), juce::PathStrokeType(1.5f));
    g.strokePath(arc(85.0f, 83.0f, 100.0f), juce::PathStrokeType(1.5f));
    g.setColour(juce::Colour(0xff3b3d43));
    g.strokePath(arc(83.0f, 144.0f, 225.0f), juce::PathStrokeType(1.0f));

    // Outer red indicator follows the audio snapshot. The inner button retains
    // the independent REC/PLAY/DUB colours already used by the panel.
    constexpr int segments=32;
    const int illuminated=indicator<=0 ? 0 : std::min(segments,int(std::ceil(indicator*segments)));
    for(int i=0;i<segments;++i)
    {
        const float start=220.f+i*(280.f/segments);
        VisualStyle::ledStroke(g,arc(69.5f,start+.45f,start+280.f/segments-.45f),10.f,
                              juce::Colour(0xffff354f),i<illuminated);
    }
    g.setColour(juce::Colour(0xff878a95).withAlpha(.25f));
    g.strokePath(arc(73.5f,228.f,491.f),juce::PathStrokeType(.8f));
    g.setColour(juce::Colour(0xff080a0c));
    g.fillEllipse(bounds.reduced(29.0f));
    g.setColour(juce::Colour(0xff252a2e));
    g.drawEllipse(bounds.reduced(31.0f), 3.0f);
    for (int angle = 0; angle < 360; angle += 45)
    {
        const auto radians = juce::degreesToRadians(static_cast<float>(angle));
        const juce::Point<float> centre(87.0f, 87.0f);
        const auto inner = centre.getPointOnCircumference(51.0f, radians);
        const auto outer = centre.getPointOnCircumference(57.0f, radians);
        g.setColour(juce::Colour(0xff0b0d10));
        g.drawLine({ inner, outer }, 3.0f);
    }
    g.setColour(juce::Colour(0xffa3afbb));
    g.strokePath(arc(55.0f, 87.0f, 99.0f), juce::PathStrokeType(3.0f));
    g.strokePath(arc(55.0f, 261.0f, 274.0f), juce::PathStrokeType(3.0f));
    juce::Path innerRing;
    innerRing.addEllipse(bounds.reduced(42.0f));
    VisualStyle::ledStroke(g, innerRing, 4.5f, colour, lit);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff252a2d), 0.0f, 44.0f,
                                         juce::Colour(0xff12191b), 0.0f, 130.0f, false));
    g.fillEllipse(bounds.reduced(45.5f));
    if (lit)
    {
        g.setColour(colour.withAlpha(0.085f));
        g.fillEllipse(bounds.reduced(45.5f));
    }
    g.setColour(lit ? colour.interpolatedWith(juce::Colours::white, 0.65f) : juce::Colour(0xff858e9b));
    juce::Path play;
    play.addTriangle(58.0f, 77.0f, 58.0f, 95.0f, 78.0f, 86.0f);
    g.fillPath(play);
    g.drawLine(83.0f, 97.0f, 94.0f, 76.0f, 3.0f);
    g.fillEllipse(99.0f, 78.0f, 17.0f, 17.0f);
    if (lit)
    {
        const char* labels[] = { "", "REC", "PLAY", "OVERDUB" };
        g.setFont(juce::FontOptions("Arial", 8.5f, juce::Font::bold));
        g.drawText(labels[static_cast<int>(state)], 48, 105, 78, 12, juce::Justification::centred);
    }
}

VolumeFader::VolumeFader()
{
    setLookAndFeel(&appearance);
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 1.0);
    setValue(0.88, juce::dontSendNotification);
    setSliderSnapsToMousePosition(true);
    setScrollWheelEnabled(false);
    setPopupDisplayEnabled(false, false, nullptr);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

VolumeFader::~VolumeFader() { setLookAndFeel(nullptr); }

int VolumeFader::FaderLookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
    return juce::jmax(3, juce::roundToInt(slider.getWidth() * 0.18f));
}

void VolumeFader::FaderLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int, int, int, int, float sliderPos, float, float,
    juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto bounds = slider.getLocalBounds().toFloat();
    const auto scale = bounds.getWidth() / 70.0f;
    const auto well = bounds.reduced(2.0f * scale, 0.0f);
    g.setColour(juce::Colour(0xff08090b));
    g.fillRect(well);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202227), well.getX(), 0.0f,
                                         juce::Colour(0xff101114), well.getRight(), 0.0f, false));
    g.fillRect(well.reduced(3.0f * scale, 0.0f));
    g.setColour(juce::Colour(0xff45474d));
    g.drawVerticalLine(juce::roundToInt(well.getX() + scale), 3.0f * scale, bounds.getBottom() - scale);
    g.setColour(juce::Colour(0xff08090b));
    g.fillRect(bounds.getCentreX() - 5.0f * scale, 8.0f * scale,
               10.0f * scale, bounds.getHeight() - 18.0f * scale);
    g.setColour(juce::Colour(0xff33363b));
    g.drawVerticalLine(juce::roundToInt(bounds.getCentreX() + 5.0f * scale),
                       8.0f * scale, bounds.getHeight() - 10.0f * scale);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4d5058), well.getX(), 0.0f,
                                         juce::Colour(0xff1b1d22), well.getRight(), 0.0f, false));
    g.fillRect(well.withTop(well.getBottom() - 7.0f * scale));
    g.setColour(juce::Colour(0xff050608));
    g.fillRect(well.reduced(8.0f * scale, 0.0f).withHeight(7.0f * scale));
    const juce::Rectangle<float> cap(5.0f * scale, sliderPos - 10.0f * scale,
                                     bounds.getWidth() - 10.0f * scale, 21.0f * scale);
    g.setColour(juce::Colour(0x99000000));
    g.fillRoundedRectangle(cap.translated(0.0f, 4.0f * scale).expanded(2.0f * scale), 2.0f * scale);
    juce::ColourGradient plastic(juce::Colour(0xff41454c), 0.0f, cap.getY(),
                                juce::Colour(0xff090a0c), 0.0f, cap.getBottom(), false);
    plastic.addColour(0.35, juce::Colour(0xff171a1f));
    g.setGradientFill(plastic);
    g.fillRoundedRectangle(cap, 2.0f * scale);
    g.setColour(juce::Colour(0xff07080a));
    for (int row : { 4, 7, 16, 19 })
        g.drawHorizontalLine(juce::roundToInt(cap.getY() + row * scale), cap.getX() + 2 * scale, cap.getRight() - 2 * scale);
    g.setColour(juce::Colour(0xff626772));
    g.drawHorizontalLine(juce::roundToInt(cap.getY() + scale), cap.getX() + scale, cap.getRight() - scale);
    g.setColour(juce::Colour(0xff8d949e));
    g.fillRect(cap.getX() + scale, sliderPos - 2.0f * scale, cap.getWidth() - 2.0f * scale, 5.0f * scale);
    g.setColour(juce::Colour(0xfff0f3f6));
    g.fillRect(cap.getX() + scale, sliderPos - scale, cap.getWidth() - 2.0f * scale, 2.0f * scale);
}
