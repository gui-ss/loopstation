#include "PanelControls.h"
#include "VisualStyle.h"

PanelKnob::PanelKnob(bool showPositionMarker) : marker(showPositionMarker)
{
    setInterceptsMouseClicks(false, false);
}

PerformanceFxKnob::PerformanceFxKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0, 1); // Physical position must survive a change of target/range.
    setValue(0.5, juce::dontSendNotification);
    setMouseDragSensitivity(400);
    setScrollWheelEnabled(true);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(face);
}

void PerformanceFxKnob::bind(int identity, int effective, int maximum, FxKnobMode mode, bool available)
{
    steps = maximum;
    pickup.bind(identity, effective, available ? maximum : 0, mode);
    setEnabled(available);
    repaint();
}

void PerformanceFxKnob::resized()
{
    juce::Slider::resized();
    face.setBounds(getLocalBounds());
}

void PerformanceFxKnob::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (!isEnabled() || wheel.deltaY == 0 || steps <= 0) return;
    const double direction = (wheel.deltaY > 0 ? 1.0 : -1.0) * (wheel.isReversed ? -1.0 : 1.0);
    setValue(getValue() + direction / steps, juce::sendNotificationSync);
}

void PerformanceFxKnob::paintOverChildren(juce::Graphics& g)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float radius = juce::jmin(getWidth(), getHeight()) * 0.30f;
    const auto mark = [&](double position, float inner, float outer, juce::Colour colour, float width)
    {
        const float angle = juce::degreesToRadians(float(-135.0 + position * 270.0));
        g.setColour(colour);
        g.drawLine({centre.getPointOnCircumference(radius * inner, angle),
                    centre.getPointOnCircumference(radius * outer, angle)}, width);
    };
    mark(getValue(), .68f, .98f, juce::Colour(0xff080a0d), 4.0f);
    mark(getValue(), .72f, .94f, juce::Colour(0xffe6eaf0), 2.0f);
    // Hollow outer tick is the effective value awaiting pickup, not an LED.
    if (isEnabled() && waiting())
        mark(effectivePosition(), 1.23f, 1.37f, juce::Colour(0xffb6c8d9), 1.2f);
}

ParameterKnob::ParameterKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0, 100, 1);
    setValue(50, juce::dontSendNotification);
    setMouseDragSensitivity(200);
    setScrollWheelEnabled(true);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(face);
    setEnabled(false);
}

void ParameterKnob::resized()
{
    juce::Slider::resized();
    face.setBounds(getLocalBounds());
}

void ParameterKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! tempoMode) { juce::Slider::mouseWheelMove(event, wheel); return; }
    if (! isEnabled() || wheel.deltaY == 0) return;
    const double direction = (wheel.deltaY > 0 ? 1.0 : -1.0) * (wheel.isReversed ? -1.0 : 1.0);
    setValue(getValue() + direction * (event.mods.isShiftDown() ? 1 : 10), juce::sendNotificationSync);
}

void ParameterKnob::paintOverChildren(juce::Graphics& g)
{
    if (! isEnabled()) return;
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float radius = juce::jmin(getWidth(), getHeight()) * 0.25f;
    const float angle = juce::degreesToRadians(-135.0f + static_cast<float>(valueToProportionOfLength(getValue())) * 270.0f);
    g.setColour(juce::Colour(0xffe6eaf0));
    g.drawLine({ centre.getPointOnCircumference(radius * 0.65f, angle),
                 centre.getPointOnCircumference(radius, angle) }, 1.4f);
}

void PanelKnob::paint(juce::Graphics& g)
{
    const auto side = static_cast<float>(juce::jmin(getWidth(), getHeight()));
    g.addTransform(juce::AffineTransform::scale(side / 100.0f).translated(
        (getWidth() - side) * 0.5f, (getHeight() - side) * 0.5f));
    const juce::Rectangle<float> bounds(1.0f, 1.0f, 98.0f, 98.0f);
    g.setColour(juce::Colour(0xff07080a));
    g.fillEllipse(bounds);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff494b51), 0.0f, 0.0f,
                                         juce::Colour(0xff090a0c), 0.0f, 100.0f, false));
    g.drawEllipse(bounds.reduced(2.0f), 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff292b31), 0.0f, 10.0f,
                                         juce::Colour(0xff0c0d10), 0.0f, 92.0f, false));
    g.fillEllipse(bounds.reduced(6.0f));
    g.setColour(juce::Colour(0xff08090c));
    g.fillEllipse(bounds.reduced(14.0f));
    // Shallow serrations and concentric rims suggest the moulded hardware grip.
    const juce::Point<float> centre(50.0f, 50.0f);
    for (int i = 0; i < 72; ++i)
    {
        const auto angle = juce::degreesToRadians(i * 5.0f);
        const auto inner = centre.getPointOnCircumference(33.7f, angle);
        const auto outer = centre.getPointOnCircumference(35.5f, angle);
        g.setColour(i < 36 ? juce::Colour(0xff62646b) : juce::Colour(0xff383a40));
        g.drawLine({ inner, outer }, 0.65f);
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303139), 24.0f, 17.0f,
                                         juce::Colour(0xff17181d), 77.0f, 86.0f, false));
    g.fillEllipse(17.0f, 17.0f, 66.0f, 66.0f);
    {
        juce::Graphics::ScopedSaveState save(g);
        juce::Path face;
        face.addEllipse(17.5f, 17.5f, 65.0f, 65.0f);
        g.reduceClipRegion(face);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x154f5763), 34.0f, 25.0f,
                                             juce::Colours::transparentBlack, 75.0f, 79.0f, true));
        g.fillAll();
        VisualStyle::grain(g, { 18, 18, 64, 64 }, 0.045f);
    }
    g.setColour(juce::Colour(0xff45464d));
    g.drawEllipse(17.3f, 17.3f, 65.4f, 65.4f, 0.55f);
    if (marker)
    {
        g.setColour(juce::Colour(0xff07090c));
        g.fillRoundedRectangle(47.8f, 19.0f, 4.4f, 11.5f, 2.0f);
        g.setColour(juce::Colour(0xffe6eaf0));
        g.fillRoundedRectangle(49.0f, 20.0f, 2.0f, 9.0f, 1.0f);
    }
}

PanelButton::PanelButton(const juce::String& caption, Style buttonStyle, juce::Colour colour)
    : Button(caption.replaceCharacter('\n', ' ')), style(buttonStyle), led(colour)
{
    setButtonText(caption);
    VisualStyle::prepareToggle(*this);
}

bool PanelButton::hitTest(int x, int y)
{
    if (expandedEdit) return getLocalBounds().contains(x,y);
    if (style != Style::ledCircle && style != Style::plainCircle)
        return Button::hitTest(x, y);
    return getLocalBounds().toFloat().getCentre().getDistanceFrom(
               { static_cast<float>(x), static_cast<float>(y) })
           <= juce::jmin(getWidth(), getHeight()) * 0.5f;
}

void PanelButton::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto active = explicitLight ? light != FxLight::off : getToggleState();
    const auto colour = !explicitLight ? led : light==FxLight::blue ? juce::Colour(0xff438fff)
        : light==FxLight::pink ? juce::Colour(0xffff98d7) : juce::Colour(0xffff4057);
    const bool circle = style == Style::ledCircle || style == Style::plainCircle;
    const float width = circle ? 40.0f : (style == Style::write ? 38.0f : 76.0f);
    const float height = style == Style::write ? 14.0f : 40.0f;
    if (expandedEdit)
        g.addTransform(juce::AffineTransform::scale(getWidth() * .9f / width, getHeight() * (36.f/65.f) / height)
                           .translated(getWidth() * .05f, getHeight() * (29.f/65.f)));
    else g.addTransform(juce::AffineTransform::scale(getWidth() / width, getHeight() / height));
    if (down) g.addTransform(juce::AffineTransform::translation(0, 0.7f));
    const juce::Rectangle<float> bounds(0.0f, 0.0f, width, height);

    if (style == Style::write)
    {
        if (active)
        {
            g.setColour(juce::Colour(0xffa4c7d7));
            g.fillRoundedRectangle(bounds.reduced(0.7f), 2.0f);
        }
        g.setColour(active ? juce::Colour(0xff101820) : juce::Colour(0xffdadde4));
        g.drawRoundedRectangle(bounds.reduced(0.7f), 2.0f, 0.8f);
        g.setFont(juce::FontOptions("Arial", 10.0f, juce::Font::bold));
        g.drawText(getButtonText(), bounds, juce::Justification::centred);
        return;
    }

    if (style == Style::utility)
    {
        g.setColour(juce::Colour(0xff101216));
        g.fillRoundedRectangle(bounds.reduced(1.0f, 5.0f), 15.0f);
        g.setColour(active ? juce::Colour(0xffb8f0f7) : juce::Colour(0xffd5d7dd));
        g.drawRoundedRectangle(bounds.reduced(1.0f, 5.0f), 15.0f, 0.8f);
        g.setColour(juce::Colour(0xff050608));
        g.fillEllipse(width - 29.0f, 8.0f, 25.0f, 25.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff393c44), 0.0f, 9.0f,
                                             juce::Colour(0xff14161b), 0.0f, 30.0f, false));
        g.fillEllipse(width - 27.0f, 10.0f, 21.0f, 21.0f);
        if (active)
        {
            g.setColour(juce::Colour(0xff668c99));
            g.fillEllipse(width - 26.0f, 11.0f, 19.0f, 19.0f);
            juce::Path indicator;
            indicator.addEllipse(width - 27.0f, 10.0f, 21.0f, 21.0f);
            VisualStyle::ledStroke(g, indicator, 1.3f, juce::Colour(0xffbdeefa), true);
        }
        g.setColour(juce::Colour(0xffe7e8ec));
        g.setFont(juce::FontOptions("Arial", 10.0f, juce::Font::bold));
        g.drawText(getButtonText(), 5, 5, 44, 30, juce::Justification::centred);
        return;
    }

    const auto rim = bounds.reduced(circle ? 4.5f : 7.0f, circle ? 4.5f : 7.5f);
    const auto radius = rim.getHeight() * 0.5f;
    g.setColour(juce::Colour(0xff08090b));
    g.fillRoundedRectangle(rim.expanded(3.5f), radius + 3.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff363940), 0.0f, 5.0f,
                                         juce::Colour(0xff15171b), 0.0f, 35.0f, false));
    g.fillRoundedRectangle(rim, radius);
    if (active)
    {
        g.setColour(colour.withAlpha(style == Style::plainCircle ? 0.35f : 0.18f));
        g.fillRoundedRectangle(rim.reduced(2.0f), radius - 2.0f);
    }
    if (style == Style::plainCircle)
    {
        g.setColour(juce::Colour(0xff55585f));
        g.drawRoundedRectangle(rim, radius, 1.0f);
        if (active)
        {
            juce::Path indicator;
            indicator.addRoundedRectangle(rim, radius);
            VisualStyle::ledStroke(g, indicator, 2.0f, colour, true);
        }
        return;
    }
    juce::Path diffuser;
    diffuser.addRoundedRectangle(rim, radius);
    VisualStyle::ledStroke(g, diffuser, active ? 4.0f : 3.3f, active ? colour : (explicitLight ? juce::Colour(0xff3a3c42) : colour.darker(0.45f)), !explicitLight || active);
    g.setColour(colour.interpolatedWith(juce::Colours::white, 0.8f));
    auto lines = juce::StringArray::fromLines(getButtonText());
    const auto fontSize = circle ? 24.0f : (lines.size() > 1 ? 11.5f : 12.0f);
    g.setFont(juce::FontOptions("Arial", fontSize, juce::Font::bold));
    const float lineHeight = circle ? 24.0f : 10.0f;
    float y = (height - lineHeight * lines.size()) * 0.5f;
    for (const auto& line : lines)
    {
        g.setFont(juce::FontOptions("Arial", line.length() > 8 ? 9.0f : fontSize, juce::Font::bold));
        g.drawText(line, juce::Rectangle<float>(4.0f, y, width - 8.0f, lineHeight),
                   juce::Justification::centred);
        y += lineHeight;
    }
}


void PanelButton::setFxLight(FxLight state)
{
    if (explicitLight && light==state) return;
    explicitLight=true; light=state; repaint();
}

void PanelButton::mouseDown(const juce::MouseEvent& e)
{
    if (!onLongPress) { juce::Button::mouseDown(e); return; }
    if (!e.mods.isLeftButtonDown()) return;
    held=true; longPress=false; dragged=false; setState(buttonDown); startTimer(650);
}

void PanelButton::timerCallback()
{
    stopTimer();
    if (held && !dragged && onLongPress) { longPress=true; onLongPress(); }
}

void PanelButton::mouseDrag(const juce::MouseEvent& e)
{
    if (!onLongPress) { juce::Button::mouseDrag(e); return; }
    if (held && e.getDistanceFromDragStart()>8) { dragged=true; stopTimer(); }
}

void PanelButton::mouseUp(const juce::MouseEvent& e)
{
    if (!onLongPress) { juce::Button::mouseUp(e); return; }
    stopTimer(); setState(buttonNormal);
    if (!held) return;
    held=false;
    if (dragged && onEditDragRelease) onEditDragRelease(e.getScreenPosition());
    else if (!longPress && hitTest(e.x,e.y) && onClick) onClick();
}
