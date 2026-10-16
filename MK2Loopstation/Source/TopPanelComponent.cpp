#include "TopPanelComponent.h"

TopPanelComponent::TopPanelComponent()
{
    using Style = PanelButton::Style;
    const juce::Colour red(0xffff4057), green(0xff35efa0), purple(0xffd888ed), silver(0xffb6c8d9);
    const auto button = [this](const juce::String& text, Style style, juce::Colour colour,
                               juce::Rectangle<float> bounds)
    {
        auto control = std::make_unique<PanelButton>(text, style, colour);
        if (text.length() == 1)
            control->setTitle((bounds.getX() < 600.0f ? "INPUT FX " : "TRACK FX ") + text);
        else if (text.isEmpty())
            control->setTitle(bounds.getX() < 600.0f ? "INPUT FX EDIT" : "TRACK FX EDIT");
        auto* result = control.get();
        addControl(std::move(control), bounds);
        return result;
    };
    const auto knob = [this](juce::Rectangle<float> bounds, bool marker)
    {
        addControl(std::make_unique<PanelKnob>(marker), bounds);
    };

    knob({ 43.0f, 44.0f, 132.0f, 132.0f }, true);
    knob({ 1025.0f, 44.0f, 132.0f, 132.0f }, true);
    knob({ 868.0f, 43.0f, 64.0f, 64.0f }, true);
    addAndMakeVisible(display);
    for (int i = 0; i < 4; ++i)
    {
        const auto caption = juce::String::charToString(static_cast<juce::juce_wchar>('A' + i));
        const auto colour = i == 3 ? purple : red;
        auto* inputFx = button(caption, Style::ledCircle, colour, { 36.0f + i * 55.0f, 189.0f, 42.0f, 42.0f });
        auto* trackFx = button(caption, Style::ledCircle, colour, { 959.0f + i * 55.0f, 189.0f, 42.0f, 42.0f });
        inputFxButtons[static_cast<size_t>(i)] = inputFx;
        inputFx->onClick = [this, inputFx, i]
        {
            if (display.isInputEditing())
            {
                display.selectInputSlot(i);
                refreshParameterKnobs();
            }
            else
            {
                display.updateEffect(EffectGroup::input, i, inputFx->getToggleState());
                if (onInputRobotChanged) onInputRobotChanged(i, display.inputRobot(i));
            }
        };
        trackFx->onClick = [this, trackFx, i]
        {
            display.updateEffect(EffectGroup::track, i, trackFx->getToggleState());
            if ((i == 0 || i == 2) && onTrackFxChanged) onTrackFxChanged(display.getTrackFx());
            if (display.isTrackEditing()) refreshParameterKnobs();
        };
        auto parameter = std::make_unique<ParameterKnob>();
        auto* control = parameter.get();
        parameterKnobs[static_cast<size_t>(i)] = control;
        control->setName("Parameter knob " + juce::String(i + 1));
        control->setTitle(control->getName());
        control->onValueChange = [this, control, i]
        {
            if (display.isTempoEditing())
            {
                if (i == 3 && onBpmChanged) onBpmChanged(control->getValue() * 0.1);
                return;
            }
            const bool changesType = display.getEditorField(i).name == "FX TYPE";
            display.setEditorField(i, juce::roundToInt(control->getValue()));
            if (display.isTrackSettings() && onTrackFxEnabled)
                onTrackFxEnabled(display.selectedTrack(), display.selectedFxEnabled());
            else if (display.isRoutingEditing() && onTrackInsertChanged)
                onTrackInsertChanged(display.editedSlot(), display.insertDestination());
            else if (display.isTrackEditing() && onTrackFxChanged) onTrackFxChanged(display.getTrackFx());
            if (display.isInputEditing() && onInputRobotChanged)
                onInputRobotChanged(display.selectedInputSlot(), display.inputRobot(display.selectedInputSlot()));
            if (changesType) refreshParameterKnobs();
        };
        addControl(std::move(parameter), { 456.0f + i * 76.0f, 181.0f, 60.0f, 60.0f });
    }
    inputEditButton = button("", Style::ledCircle, silver, { 203.0f, 120.0f, 36.0f, 36.0f });
    inputEditButton->setClickingTogglesState(false);
    inputEditButton->onClick = [this] { setInputEditing(true); };
    trackEditButton = button("", Style::ledCircle, silver, { 961.0f, 120.0f, 36.0f, 36.0f });
    trackEditButton->setClickingTogglesState(false);
    trackEditButton->onClick = [this] { setTrackEditing(true); };
    button("ALL\nSTART/STOP", Style::ledPill, green, { 260.0f, 188.0f, 84.0f, 43.0f });
    button("UNDO\nREDO", Style::ledPill, silver, { 352.0f, 188.0f, 84.0f, 43.0f });
    tapButton = button("TAP\nTEMPO", Style::ledPill, green, { 767.0f, 188.0f, 84.0f, 43.0f });
    tapButton->setClickingTogglesState(false);
    tapButton->setTriggeredOnMouseDown(true);
    tapButton->onClick = [this]
    {
        if (onTapTempo) onTapTempo();
        setTempoEditing(true);
    };
    button("START/\nSTOP", Style::ledPill, silver, { 857.0f, 188.0f, 84.0f, 43.0f });
    button("MENU", Style::utility, silver, { 394.0f, 65.0f, 76.0f, 40.0f });
    button("LOOP", Style::utility, silver, { 394.0f, 117.0f, 76.0f, 40.0f });
    auto* exitButton = button("EXIT", Style::plainCircle, silver, { 734.0f, 72.0f, 30.0f, 30.0f });
    exitButton->onClick = [this]
    {
        if (display.isTempoEditing()) { setTempoEditing(false); return; }
        if (display.isTrackSettings()) { display.setTrackSettings(false); refreshParameterKnobs(); return; }
        if (display.isRoutingEditing()) { display.toggleRoutingEditing(); refreshParameterKnobs(); return; }
        if (display.isInputEditing()) setInputEditing(false);
        if (display.isTrackEditing()) setTrackEditing(false);
    };
    auto* enter = button("ENTER", Style::plainCircle, silver, { 795.0f, 72.0f, 30.0f, 30.0f });
    enter->setClickingTogglesState(false);
    enter->onClick = [this] { display.toggleRoutingEditing(); refreshParameterKnobs(); };
    button("WRITE", Style::write, silver, { 763.0f, 90.0f, 34.0f, 12.0f });
    auto* previous = button("Previous", Style::plainCircle, silver, { 734.0f, 124.0f, 30.0f, 30.0f });
    auto* next = button("Next", Style::plainCircle, silver, { 795.0f, 124.0f, 30.0f, 30.0f });
    for (auto* arrow : { previous, next })
    {
        arrow->setClickingTogglesState(false);
    }
    previous->onClick = [this] { display.navigate(-1); refreshParameterKnobs(); };
    next->onClick = [this] { display.navigate(1); refreshParameterKnobs(); };
    button("Rhythm edit", Style::plainCircle, silver, { 861.0f, 125.0f, 28.0f, 28.0f });
}

void TopPanelComponent::openTrackSettings()
{
    display.setTrackSettings(true);
    inputEditButton->setToggleState(false, juce::dontSendNotification);
    trackEditButton->setToggleState(false, juce::dontSendNotification);
    for (auto* button : inputFxButtons) button->setClickingTogglesState(true);
    refreshParameterKnobs();
}

void TopPanelComponent::setInputEditing(bool editing)
{
    display.setInputEditing(editing);
    trackEditButton->setToggleState(display.isTrackEditing(), juce::dontSendNotification);
    inputEditButton->setToggleState(editing, juce::dontSendNotification);
    // Suppress JUCE's automatic toggle before the click, preserving the real LED state.
    for (auto* button : inputFxButtons) button->setClickingTogglesState(! editing);
    refreshParameterKnobs();
}

void TopPanelComponent::setTrackEditing(bool editing)
{
    display.setTrackEditing(editing);
    trackEditButton->setToggleState(editing, juce::dontSendNotification);
    inputEditButton->setToggleState(display.isInputEditing(), juce::dontSendNotification);
    for (auto* button : inputFxButtons) button->setClickingTogglesState(! display.isInputEditing());
    refreshParameterKnobs();
}

void TopPanelComponent::refreshParameterKnobs()
{
    for (size_t i = 0; i < parameterKnobs.size(); ++i)
    {
        auto& control = *parameterKnobs[i];
        const auto field = display.getEditorField(static_cast<int>(i));
        const bool tempo = display.isTempoEditing();
        control.setTempoMode(tempo && i == 3);
        control.setRange(tempo && i == 3 ? 400 : field.minimum, field.maximum, 1);
        control.setValue(field.value, juce::dontSendNotification);
        const bool editing = tempo || display.isInputEditing() || display.isTrackEditing() || display.isTrackSettings();
        control.setEnabled(editing && field.available);
        control.setTitle(tempo && i == 3 ? "TEMPO BPM - wheel: 1 BPM; Shift+wheel: 0.1 BPM" : editing && field.available ? (display.isTrackSettings() ? "TRACK SETTINGS " : display.isTrackEditing() ? "TRACK FX " + display.getTrackSlotLetter() + " " : "INPUT FX " + display.getInputSlotLetter() + " ") + field.name
                         : "Parameter knob " + juce::String(static_cast<int>(i) + 1));
        control.repaint();
    }
}

void TopPanelComponent::setTempoEditing(bool editing)
{
    display.setTempoEditing(editing);
    for (auto* b : inputFxButtons) b->setClickingTogglesState(! display.isInputEditing());
    inputEditButton->setToggleState(display.isInputEditing(), juce::dontSendNotification);
    trackEditButton->setToggleState(display.isTrackEditing(), juce::dontSendNotification);
    refreshParameterKnobs();
}

void TopPanelComponent::updateClock(double bpm, double beats, const std::array<float, 5>& loopProgress, bool running)
{
    display.updateClock(bpm, loopProgress);
    tapButton->setToggleState(running && beats - std::floor(beats) < 0.16, juce::dontSendNotification);
    if (display.isTempoEditing()) refreshParameterKnobs();
}

void TopPanelComponent::addControl(std::unique_ptr<juce::Component> component,
                                   juce::Rectangle<float> bounds)
{
    addAndMakeVisible(*component);
    controls.push_back({ std::move(component), bounds });
}

void TopPanelComponent::resized()
{
    const auto transform = juce::AffineTransform::scale(getWidth() / 1200.0f, getHeight() / 248.0f);
    for (auto& control : controls)
        control.component->setBounds(control.bounds.transformedBy(transform).toNearestInt());
    display.setBounds(juce::Rectangle<float>(493, 60, 214, 100).transformedBy(transform).toNearestInt());
}

void TopPanelComponent::updateTrack(int index, TrackVisualState state, bool hasPhrase)
{
    display.updateTrack(index, state, hasPhrase);
}

void TopPanelComponent::updateVolume(int index, float volume)
{
    display.updateVolume(index, volume);
}

void TopPanelComponent::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(getWidth() / 1200.0f, getHeight() / 248.0f));
    const juce::Colour ink(0xffe3e5ea), red(0xffd73b47);
    const auto text = [&g](const juce::String& caption, juce::Rectangle<float> bounds,
                           float size, juce::Colour colour)
    {
        g.setColour(colour);
        g.setFont(juce::FontOptions("Arial", size, juce::Font::bold));
        g.drawText(caption, bounds, juce::Justification::centred);
    };
    const auto line = [&g](float x1, float y1, float x2, float y2, juce::Colour colour, float width)
    {
        g.setColour(colour);
        g.drawLine(x1, y1, x2, y2, width);
    };

    // Rear-connector legends are printed decoration, not connection status.
    struct Legend { const char* label; float x; float width; };
    const Legend legends[] = {
        { "MIC 1", 112, 44 }, { "MIC 2", 199, 44 }, { "L -- INST 1 -- R", 289, 78 },
        { "L -- INST 2 -- R", 386, 78 }, { "MAIN", 492, 42 }, { "SUB 1", 542, 42 },
        { "SUB 2", 591, 42 }, { "PHONES", 642, 45 }, { "CTL/EXP", 701, 48 },
        { "IN", 807, 28 }, { "OUT", 894, 30 }, { "USB", 975, 34 },
        { "POWER", 1039, 42 }, { "DC IN", 1100, 42 }
    };
    for (const auto& legend : legends)
        text(legend.label, { legend.x, 14.0f, legend.width, 10.0f }, 7.2f, ink);
    line(114, 25, 461, 25, ink, 0.65f);
    line(495, 25, 686, 25, ink, 0.65f);
    text("INPUT", { 266, 22, 36, 10 }, 7.5f, ink);
    text("OUTPUT", { 563, 22, 44, 10 }, 7.5f, ink);
    text("MIDI", { 849, 22, 35, 10 }, 7.5f, ink);
    line(112, 33, 1088, 33, red, 1.7f);
    juce::Path header;
    header.startNewSubPath(373, 33);
    header.lineTo(827, 33);
    header.lineTo(782, 48);
    header.lineTo(418, 48);
    header.closeSubPath();
    g.setColour(juce::Colour(0xffd02937));
    g.fillPath(header);
    g.setColour(juce::Colour(0xff17191d));
    g.setFont(juce::FontOptions("Arial Black", 19.0f, juce::Font::bold));
    g.drawText("RC-505mkII LOOP STATION", 407, 32, 386, 17, juce::Justification::centred);

    // Printed brand mark and the white arcs bordering the two large encoders.
    g.setColour(ink);
    g.drawRect(179.0f, 46.0f, 17.0f, 17.0f, 1.5f);
    g.drawRoundedRectangle({ 182.0f, 48.5f, 11.0f, 11.5f }, 3.0f, 1.5f);
    g.setFont(juce::FontOptions("Arial Black", 31.0f, juce::Font::bold));
    g.drawText("BOSS", 199, 41, 108, 28, juce::Justification::centredLeft);
    for (int side = 0; side < 2; ++side)
    {
        const float centreX = side == 0 ? 109.0f : 1091.0f;
        for (int part = 0; part < 2; ++part)
        {
            const float start = (side == 0 ? 0.0f : 180.0f) + (part == 0 ? 0.0f : 95.0f);
            const float end = (side == 0 ? 0.0f : 180.0f) + (part == 0 ? 85.0f : 180.0f);
            juce::Path arc;
            arc.addCentredArc(centreX, 110, 72, 72, 0,
                              juce::degreesToRadians(start), juce::degreesToRadians(end), true);
            g.setColour(ink);
            g.strokePath(arc, juce::PathStrokeType(1.6f));
        }
        const float editX = side == 0 ? 203.0f : 961.0f;
        g.setColour(ink);
        g.drawRoundedRectangle({ editX - 2, 91, 40, 14 }, 7, 0.8f);
        text("EDIT", { editX, 91, 36, 14 }, 10, ink);
        text("(FX BANK: HOLD)", { editX - 23, 106, 82, 10 }, 6.2f, ink);
    }
    text("INPUT FX", { 167, 164, 80, 18 }, 16.0f, red);
    text("TRACK FX", { 950, 164, 85, 18 }, 16.0f, red);
    line(112, 182, 246, 182, red, 1.3f);
    line(952, 182, 1088, 182, red, 1.3f);
    text("EXIT", { 731, 57, 35, 14 }, 9.5f, ink);
    text("ENTER", { 790, 57, 40, 14 }, 9.5f, ink);
    text("OUTPUT", { 834, 40, 49, 13 }, 10, ink);
    text("LEVEL", { 834, 52, 49, 13 }, 10, ink);
    text("RHYTHM", { 859, 161, 77, 15 }, 12, ink);
    g.setColour(ink);
    g.drawRoundedRectangle({ 885, 132, 42, 13 }, 6.5f, 0.8f);
    text("EDIT", { 887, 131, 36, 14 }, 10, ink);
    for (float x : { 881.0f, 920.0f })
    {
        g.setColour(juce::Colour(0xff0c0d10));
        g.fillEllipse(x - 1, 108, 5, 5);
        g.setColour(ink);
        g.fillEllipse(x, 109, 3, 3);
    }
    g.setColour(ink);
    juce::Path arrows;
    arrows.addTriangle(742, 116, 754, 112, 754, 120);
    arrows.addTriangle(805, 112, 817, 116, 805, 120);
    arrows.addTriangle(895, 157, 899, 151, 903, 157);
    arrows.addTriangle(895, 178, 903, 178, 899, 184);
    g.fillPath(arrows);
}
