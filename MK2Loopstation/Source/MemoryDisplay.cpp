#include "MemoryDisplay.h"
#include "VisualStyle.h"

namespace
{
const juce::Colour lcdInk(0xfff4f5e9);
const juce::Colour lcdBackground(0xff272568);

const char* loopStateName(TrackVisualState state)
{
    switch (state)
    {
        case TrackVisualState::rec: return "REC";
        case TrackVisualState::play: return "PLAY";
        case TrackVisualState::overdub: return "OVERDUB";
        case TrackVisualState::blank: return "STOP";
    }
    return "STOP";
}

juce::Path clippedCorners(juce::Rectangle<float> rect, float corner)
{
    const auto x = rect.getX(), y = rect.getY(), r = rect.getRight(), b = rect.getBottom();
    juce::Path path;
    path.startNewSubPath(x + corner, y);
    path.lineTo(r - corner, y); path.lineTo(r, y + corner);
    path.lineTo(r, b - corner); path.lineTo(r - corner, b);
    path.lineTo(x + corner, b); path.lineTo(x, b - corner);
    path.lineTo(x, y + corner); path.closeSubPath();
    return path;
}
}

MemoryDisplay::MemoryDisplay()
{
    setName("Memory display");
    setInterceptsMouseClicks(false, false);
    updateAccessibleDescription();
}

MemoryDisplay::~MemoryDisplay()
{
    stopTimer();
}

void MemoryDisplay::setTrackSettings(bool open)
{
    if (open) { tempoEditing = false; model.inputEditor.open = false; model.trackEditorOpen = false; routingEditing = false; }
    trackSettings = open;
    if (open) quantizeNoticeUntil = 0;
    updateAccessibleDescription(); repaint();
}

void MemoryDisplay::toggleRoutingEditing()
{
    if (! isTrackEditing()) return;
    routingEditing = ! routingEditing;
    quantizeNoticeUntil = 0;
    updateAccessibleDescription(); repaint();
}

void MemoryDisplay::updateRouting(const std::array<bool, 5>& enabled, int a, int c)
{
    if (trackFxEnabled == enabled && inserts[0] == a && inserts[1] == c) return;
    trackFxEnabled = enabled; inserts = {{ a, c }};
    updateAccessibleDescription(); repaint();
}

void MemoryDisplay::navigate(int direction)
{
    if (tempoEditing) return;
    if (direction == 0 || trackSettings) return;
    if (model.trackEditorOpen)
    {
        model.trackEditorSlot = direction > 0 ? 2 : 0;
        updateAccessibleDescription();
        repaint();
        return;
    }
    if (model.inputEditor.open)
    {
        model.inputEditor.navigate(direction);
        updateAccessibleDescription();
        repaint();
        return;
    }
    const auto& order = DisplayModel::screenOrder;
    for (size_t i = 0; i < order.size(); ++i)
        if (order[i] == model.screen)
        {
            model.screen = order[(i + (direction > 0 ? 1 : order.size() - 1)) % order.size()];
            break;
        }
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::setInputEditing(bool editing)
{
    if (editing) { tempoEditing = false; trackSettings = false; }
    if (editing) model.trackEditorOpen = false;
    if (editing && ! model.inputEditor.open) model.inputEditor.page = 0;
    model.inputEditor.open = editing;
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::setTrackEditing(bool editing)
{
    if (editing) { tempoEditing = false; trackSettings = false; }
    if (editing) model.inputEditor.open = false;
    model.trackEditorOpen = editing;
    routingEditing = false;
    updateAccessibleDescription();
    repaint();
}

InputFxEditor::Field MemoryDisplay::getEditorField(int knob) const
{
    if (tempoEditing)
        return knob == 3 ? InputFxEditor::Field { "BPM", "TEMPO", "", juce::String(model.bpm, 1),
                                                juce::roundToInt(model.bpm * 10), 3000, true }
                         : InputFxEditor::Field {};
    if (trackSettings)
        return knob == 0 ? InputFxEditor::Field { "FX", "FX", "", selectedFxEnabled() ? "ON" : "OFF",
                                                 selectedFxEnabled() ? 1 : 0, 1, true } : InputFxEditor::Field {};
    if (isRoutingEditing())
        return knob == 0 ? InputFxEditor::Field { "INSERT", "INSERT", "", insertName(), insertDestination(), 5, true }
                         : InputFxEditor::Field {};
    if (! model.trackEditorOpen) return model.inputEditor.field(knob);
    if (model.trackEditorSlot == 2)
    {
        if (! juce::isPositiveAndBelow(knob, 4)) return {};
        const char* names[] { "DRIVE", "TONE", "LEVEL", "MIX" };
        const int values[] { model.trackFx.drive, model.trackFx.tone, model.trackFx.level, model.trackFx.mix };
        return { names[knob], names[knob], "", juce::String(values[knob]), values[knob], 100, true };
    }
    if (knob == 0) return { "CUTOFF", "CUT", "OFF", juce::String(model.trackFx.cutoff), model.trackFx.cutoff, 100, true };
    if (knob == 1) return { "RESONANCE", "RESO-", "NANCE", juce::String(model.trackFx.resonance), model.trackFx.resonance, 100, true };
    return {};
}

void MemoryDisplay::selectInputSlot(int index)
{
    model.inputEditor.selectSlot(index);
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::setEditorField(int knob, int value)
{
    if (tempoEditing) return; // Only AudioSystem may change tempo.
    if (trackSettings || isRoutingEditing())
    {
        if (knob == 0)
        {
            if (trackSettings) trackFxEnabled[size_t(model.currentTrack)] = value != 0;
            else inserts[model.trackEditorSlot == 2 ? 1 : 0] = juce::jlimit(0, 5, value);
        }
        updateAccessibleDescription(); repaint(); return;
    }
    if (model.trackEditorOpen)
    {
        if (model.trackEditorSlot == 2)
        {
            int* fields[] { &model.trackFx.drive, &model.trackFx.tone, &model.trackFx.level, &model.trackFx.mix };
            if (juce::isPositiveAndBelow(knob, 4)) *fields[knob] = TrackFxParameters::bounded(value);
        }
        else if (knob == 0) model.trackFx.cutoff = TrackFxParameters::bounded(value);
        else if (knob == 1) model.trackFx.resonance = TrackFxParameters::bounded(value);
        updateAccessibleDescription();
        repaint();
        return;
    }
    if (! model.inputEditor.open) return;
    model.inputEditor.setField(knob, value);
    if (!model.inputEditor.multi)
    {
        int keep=model.inputEditor.selectedSlot;
        if (!model.inputEffects[size_t(keep)].enabled)
            for(int i=0;i<4;++i) if(model.inputEffects[size_t(i)].enabled) {keep=i; break;}
        for(int i=0;i<4;++i) if(i!=keep) model.inputEffects[size_t(i)].enabled=false;
    }
    auto& effect = model.inputEffects[static_cast<size_t>(model.inputEditor.selectedSlot)];
    effect.name = InputFxEditor::types[static_cast<size_t>(model.inputEditor.selectedType())];
    effect.lines = model.inputEditor.selectedType() == 3 ? std::array<juce::String, 2>{ "PHA", "SER" }
                                             : model.inputEditor.selectedType() == 4 ? std::array<juce::String, 2>{ "RO", "BOT" }
                                             : model.inputEditor.selectedType() == 5 ? std::array<juce::String, 2>{ "OSC", "BOT" }
                                             : std::array<juce::String, 2>{ effect.name, "" };
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::updateTrack(int index, TrackVisualState state, bool hasPhrase)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(model.tracks.size()))) return;
    if (isTimerRunning()) timerCallback();
    auto& track = model.tracks[static_cast<size_t>(index)];
    track.state = state;
    track.hasPhrase = hasPhrase;
    if (state == TrackVisualState::blank) simulation.reset(static_cast<size_t>(index));
    bool active = false;
    for (const auto& item : model.tracks) active = active || item.state != TrackVisualState::blank;
    if (active && ! isTimerRunning())
    {
        lastSimulationTime = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(30);
    }
    else if (! active) stopTimer();
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::selectTrack(int index)
{
    if (! juce::isPositiveAndBelow(index, 5) || model.currentTrack == index) return;
    model.currentTrack = index;
    // Reuse the LCD status strip, including a temporary notice over FX headings.
    quantizeNoticeUntil = trackSettings ? 0 : juce::Time::getMillisecondCounterHiRes() + 1500;
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::updateVolume(int index, float volume)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(model.tracks.size()))) return;
    model.tracks[static_cast<size_t>(index)].volume = juce::jlimit(0.0f, 1.0f, volume);
    simulation.advance(0.0, model);
    if (model.screen == DisplayModel::Screen::loopLevel) repaint();
}

void MemoryDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    simulation.advance((now - lastSimulationTime) / 1000.0, model);
    lastSimulationTime = now;
    if (! model.inputEditor.open && (model.screen == DisplayModel::Screen::loopTracks || model.screen == DisplayModel::Screen::loopLevel))
        repaint();
}

void MemoryDisplay::setTempoEditing(bool editing)
{
    tempoEditing = editing;
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::updateClock(double bpm, const std::array<float, 5>& loopProgress)
{
    const bool changed = model.bpm != bpm;
    model.bpm = bpm;
    simulation.updateProgress(loopProgress, model);
    if (changed) updateAccessibleDescription();
    if (changed || model.screen == DisplayModel::Screen::loopTracks) repaint();
}

void MemoryDisplay::updateQuantize(bool enabled, bool recording, bool effective, bool waiting)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool expired = quantizeNoticeUntil != 0 && now >= quantizeNoticeUntil;
    if (expired) quantizeNoticeUntil = 0;
    if (quantizeEnabled == enabled && quantizeRecording == recording
        && quantizeEffective == effective && quantizeWaiting == waiting)
    { if (expired) repaint(); return; }
    if (quantizeEnabled != enabled) quantizeNoticeUntil = now + 1500;
    quantizeEnabled = enabled; quantizeRecording = recording;
    quantizeEffective = effective; quantizeWaiting = waiting;
    updateAccessibleDescription();
    repaint();
}

juce::String MemoryDisplay::quantizeText() const
{
    juce::String text = "T" + juce::String(model.currentTrack + 1) + (quantizeEnabled ? " Q ON" : " Q OFF");
    if (quantizeWaiting) text += " WAIT";
    else if (quantizeRecording && quantizeEffective != quantizeEnabled)
        text += quantizeEffective ? " REC ON" : " REC OFF";
    return text;
}

void MemoryDisplay::paintQuantize(juce::Graphics& g)
{
    const bool fxEditor = ! tempoEditing && (model.inputEditor.open || model.trackEditorOpen || trackSettings);
    // FX headings reappear after a short Q notice; WAIT stays until the engine
    // completes/cancels capture. Never overwrite parameters or knob legends.
    if (fxEditor && ! quantizeWaiting && quantizeNoticeUntil == 0) return;
    const float x = tempoEditing ? 50.0f : 0.0f;
    const float width = tempoEditing ? 78.0f : fxEditor ? 128.0f : 85.0f;
    g.setColour(lcdBackground); g.fillRect(x, 0.0f, width, 9.0f);
    g.setColour(lcdInk);
    VisualStyle::lcdText(g, quantizeText(), { x, 0, width, 8 }, 0.75f);
}

void MemoryDisplay::updateLoopSync(bool enabled, const juce::String& notice, bool bpmBlocked)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (syncNoticeUntil != 0 && now >= syncNoticeUntil) { syncNoticeUntil = 0; repaint(); }
    if (loopSync == enabled && syncNotice == notice && tempoBlocked == bpmBlocked) return;
    if (loopSync != enabled) syncNoticeUntil = now + 1500;
    loopSync = enabled; syncNotice = notice; tempoBlocked = bpmBlocked;
    updateAccessibleDescription(); repaint();
}

void MemoryDisplay::paintLoopSync(juce::Graphics& g)
{
    const bool editor = ! tempoEditing && (model.inputEditor.open || model.trackEditorOpen || trackSettings);
    const auto message = tempoBlocked ? juce::String("BPM LOCK: L OFF") : syncNotice;
    const juce::String mode = loopSync ? "LS ON MEASURE" : "LS OFF MEASURE";
    if (editor)
    {
        if (message.isEmpty() && syncNoticeUntil == 0) return;
        g.setColour(lcdBackground); g.fillRect(0, 0, 128, 9);
        g.setColour(lcdInk);
        VisualStyle::lcdText(g, "T" + juce::String(model.currentTrack + 1) + " "
                                + (message.isEmpty() ? mode : message), { 0, 0, 128, 8 }, .75f);
        return;
    }
    // Reuse the existing footer; no panel controls or editor parameter cells move.
    g.setColour(lcdBackground); g.fillRect(0, 52, 94, 12);
    g.setColour(lcdInk);
    VisualStyle::lcdText(g, mode, { 1, 52, 92, 6 }, .65f);
    VisualStyle::lcdText(g, message, { 1, 58, 92, 6 }, .65f);
}

void MemoryDisplay::updateEffect(EffectGroup group, int index, bool enabled)
{
    auto& effects = group == EffectGroup::input ? model.inputEffects : model.trackEffects;
    if (! juce::isPositiveAndBelow(index, static_cast<int>(effects.size()))) return;
    if (group==EffectGroup::input && enabled && !model.inputEditor.multi)
        for (auto& e:model.inputEffects) e.enabled=false;
    effects[static_cast<size_t>(index)].enabled = enabled;
    if (group == EffectGroup::track && index == 0) model.trackFx.enabled = enabled;
    if (group == EffectGroup::track && index == 2) model.trackFx.distortionEnabled = enabled;
    if (group == EffectGroup::track && (index == 0 || index == 2)) model.trackEditorSlot = index;
    updateAccessibleDescription();
    repaint();
}

void MemoryDisplay::updateAccessibleDescription()
{
    juce::String description = "Display - ";
    switch (model.screen)
    {
        case DisplayModel::Screen::memoryNumber: description += "MEMORY NUMBER"; break;
        case DisplayModel::Screen::trackStatus: description += "TRACK STATUS"; break;
        case DisplayModel::Screen::loopTracks: description += "LOOP TRACKS"; break;
        case DisplayModel::Screen::loopStatus: description += "LOOP STATUS"; break;
        case DisplayModel::Screen::loopLevel: description += "LOOP LEVEL"; break;
        case DisplayModel::Screen::inputFx: description += "INPUT FX"; break;
        case DisplayModel::Screen::trackFx: description += "TRACK FX"; break;
    }
    description += " | " + juce::String(model.memoryNumber).paddedLeft('0', 2)
                   + " " + model.memoryName + " | BPM " + juce::String(model.bpm, 1);
    for (size_t i = 0; i < model.tracks.size(); ++i)
    {
        const auto& track = model.tracks[i];
        const char* state = "EMPTY";
        switch (track.state)
        {
            case TrackVisualState::blank: state = track.hasPhrase ? "STOPPED" : "EMPTY"; break;
            case TrackVisualState::rec: state = "REC"; break;
            case TrackVisualState::play: state = "PLAY"; break;
            case TrackVisualState::overdub: state = "OVERDUB"; break;
        }
        description += " | " + juce::String(static_cast<int>(i) + 1) + ":" + state;
    }
    description += " | CURRENT:" + juce::String(model.currentTrack + 1)
                 + " | UNDO TARGET:TRK" + juce::String(model.currentTrack + 1);
    if (model.screen == DisplayModel::Screen::loopStatus)
        description += " | TRK" + juce::String(model.currentTrack + 1) + ": "
                       + loopStateName(model.tracks[static_cast<size_t>(model.currentTrack)].state);
    if (model.screen == DisplayModel::Screen::loopTracks || model.screen == DisplayModel::Screen::loopLevel)
        description += model.screen == DisplayModel::Screen::loopTracks
            ? " | PROGRESS: TRACKS 1-5 AUDIO"
            : " | SIMULATED LEVELS";
    for (const auto group : { EffectGroup::input, EffectGroup::track })
    {
        const auto& effects = group == EffectGroup::input ? model.inputEffects : model.trackEffects;
        for (size_t i = 0; i < effects.size(); ++i)
            description += juce::String(group == EffectGroup::input ? " | INPUT FX " : " | TRACK FX ")
                           + juce::String::charToString(static_cast<juce::juce_wchar>('A' + i))
                           + ":" + (effects[i].enabled ? "ON" : "OFF") + " (" + effects[i].name + ")";
    }
    if (model.inputEditor.open)
    {
        description = "Display - INPUT FX EDIT | FX A-" + model.inputEditor.slotLetter() + " | "
                      + juce::String(InputFxEditor::types[static_cast<size_t>(model.inputEditor.selectedType())])
                      + " | PAGE:" + juce::String(model.inputEditor.page + 1)
                      + description.fromFirstOccurrenceOf(" | ", true, false);
        for (int i = 0; i < 4; ++i)
        {
            const auto field = model.inputEditor.field(i);
            if (field.available) description += " | " + field.name + ":" + field.text;
        }
    }
    if (model.trackEditorOpen)
    {
        juce::String heading = "Display - TRACK FX EDIT | FX A-" + getTrackSlotLetter()
            + (model.trackEditorSlot == 2 ? " | DISTORTION" : " | LPF") + " | INSERT:" + insertName();
        for (int i = 0; i < 4; ++i)
        {
            const auto field = getEditorField(i);
            if (field.available) heading += " | " + field.name + ":" + field.text;
        }
        description = heading + description.fromFirstOccurrenceOf(" | ", true, false);
    }
    if (trackSettings) description = "Display - TRACK " + juce::String(model.currentTrack + 1)
        + " SETTINGS | FX:" + (selectedFxEnabled() ? "ON" : "OFF") + description.fromFirstOccurrenceOf(" | ", true, false);
    description += " | INSERT A:" + (inserts[0] == 0 ? juce::String("ALL") : "TRACK" + juce::String(inserts[0]))
                 + " | INSERT C:" + (inserts[1] == 0 ? juce::String("ALL") : "TRACK" + juce::String(inserts[1]));
    if (isRoutingEditing()) description = "Display - TRACK FX INSERT | SLOT:" + getTrackSlotLetter() + description.fromFirstOccurrenceOf(" | ", true, false);
    if (tempoEditing) description = "Display - TEMPO | BPM " + juce::String(model.bpm, 1) + " | KNOB 4";
    description += " | QUANTIZE:" + juce::String(quantizeEnabled ? "ON" : "OFF");
    if (quantizeRecording) description += quantizeEffective ? " | REC QUANTIZE:ON" : " | REC QUANTIZE:OFF";
    if (quantizeWaiting) description += " | REC WAIT BAR";
    description += " | LOOP SYNC:" + juce::String(loopSync ? "ON" : "OFF") + " | MODE:MEASURE";
    if (syncNotice.isNotEmpty()) description += " | " + syncNotice;
    if (tempoBlocked) description += " | BPM BLOCKED: synchronized phrases require original tempo; disable LOOP SYNC to edit";
    setTitle(description);
}

void MemoryDisplay::paint(juce::Graphics& g)
{
    // Keep the existing bezel and physical screen bounds unchanged.
    g.addTransform(juce::AffineTransform::scale(getWidth() / 214.0f, getHeight() / 100.0f));
    const juce::Rectangle<float> bezel(0.5f, 0.5f, 213.0f, 99.0f);
    juce::ColourGradient metal(juce::Colour(0xffc7cad5), 0, 0, juce::Colour(0xff9197a7), 0, 100, false);
    metal.addColour(0.22, juce::Colour(0xffb7bdcb));
    metal.addColour(0.75, juce::Colour(0xffc1c5d0));
    g.setGradientFill(metal);
    g.fillRoundedRectangle(bezel, 3.0f);
    VisualStyle::grain(g, bezel.reduced(2.0f), 0.045f);
    g.setColour(juce::Colour(0xffe1e3e9));
    g.drawRoundedRectangle(bezel, 3.0f, 0.7f);
    g.setColour(juce::Colour(0xff0a0b12));
    g.fillRoundedRectangle(bezel.reduced(24.0f, 6.0f), 3.0f);
    const auto screen = bezel.reduced(29.0f, 12.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff39348e), screen.getTopLeft(),
                                         juce::Colour(0xff232363), screen.getBottomRight(), false));
    g.fillRect(screen);
    g.setColour(juce::Colour(0xff696e92));
    g.drawRect(screen.expanded(1.5f), 1.0f);
    g.reduceClipRegion(screen.toNearestInt());
    g.addTransform(juce::AffineTransform::scale(screen.getWidth() / 128.0f, screen.getHeight() / 64.0f)
                       .translated(screen.getX(), screen.getY()));
    paintLcd(g);
    paintQuantize(g);
    paintLoopSync(g);
}

void MemoryDisplay::paintLcd(juce::Graphics& g)
{
    if (tempoEditing)
    {
        g.setColour(lcdInk);
        VisualStyle::lcdText(g, "TEMPO", { 2, 1, 46, 9 }, 1.0f);
        g.fillRect(1, 11, 126, 1);
        VisualStyle::lcdText(g, juce::String(model.bpm, 1), { 2, 20, 124, 24 }, 2.5f);
        VisualStyle::lcdText(g, "TEMPO", { 96, 54, 30, 8 }, 0.8f);
        return;
    }
    if (model.inputEditor.open || model.trackEditorOpen || trackSettings)
    {
        paintInputEditor(g);
        return;
    }
    // Reference: Portuguese Owner's Manual p. 8, all seven playback screens.
    // Lock and phantom-power icons are intentionally absent: those features are off.
    g.setColour(lcdInk);
    g.fillEllipse(89, 5, 3, 2);
    g.fillRect(91, 1, 1, 5);
    g.fillRect(92, 1, 2, 1);
    g.fillRect(95, 3, 3, 1);
    g.fillRect(95, 5, 3, 1);
    VisualStyle::lcdText(g, juce::String(model.bpm, 1), { 99, 0, 28, 8 }, 0.85f);

    const auto number = juce::String(model.memoryNumber).paddedLeft('0', 2);
    if (model.screen == DisplayModel::Screen::memoryNumber)
    {
        VisualStyle::lcdText(g, number, { 0, 11, 128, 25 }, 3.1f);
        VisualStyle::lcdText(g, model.memoryName, { 0, 35, 128, 15 }, 1.65f);
    }
    else
    {
        g.fillRect(2, 11, 16, 12);
        g.setColour(lcdBackground);
        VisualStyle::lcdText(g, number, { 2, 11, 16, 12 }, 1.25f);
        g.setColour(lcdInk);
        VisualStyle::lcdText(g, model.memoryName, { 20, 11, 66, 12 }, 1.4f);
        switch (model.screen)
        {
            case DisplayModel::Screen::trackStatus: paintTrackIcons(g, false); break;
            case DisplayModel::Screen::loopTracks: paintTrackIcons(g, true); break;
            case DisplayModel::Screen::loopStatus: paintLoopStatus(g); break;
            case DisplayModel::Screen::loopLevel: paintLoopLevel(g); break;
            case DisplayModel::Screen::inputFx: paintEffects(g, EffectGroup::input); break;
            case DisplayModel::Screen::trackFx: paintEffects(g, EffectGroup::track); break;
            case DisplayModel::Screen::memoryNumber: break;
        }
    }

    // Knob-assignment legends from the reference, not functional effects or menus.
    g.setColour(lcdInk);
    VisualStyle::lcdText(g, "MEMORY", { 2, 56, 32, 7 }, 0.8f);
    VisualStyle::lcdText(g, "MFX", { 110, 53, 15, 5 }, 0.65f);
    VisualStyle::lcdText(g, "REVERB", { 96, 59, 30, 5 }, 0.7f);
}

void MemoryDisplay::paintInputEditor(juce::Graphics& g)
{
    // Owner's Manual p. 16: FX TYPE first, arrows page through four fields at a time.
    const auto& editor = model.inputEditor;
    g.setColour(lcdInk);
    if (trackSettings || isRoutingEditing())
    {
        const auto field = getEditorField(0);
        VisualStyle::lcdText(g, trackSettings ? "TRACK " + juce::String(model.currentTrack + 1) + " SETTINGS"
                                           : "TRACK FX A-" + getTrackSlotLetter(), { 1, 0, 126, 8 }, .9f);
        g.fillRect(1, 9, 126, 1);
        VisualStyle::lcdText(g, field.name, { 1, 14, 126, 9 }, 1.f);
        VisualStyle::lcdText(g, field.text, { 1, 28, 126, 13 }, 1.5f);
        VisualStyle::lcdText(g, trackSettings ? "FX" : "INSERT", { 1, 53, 31, 8 }, .85f);
        VisualStyle::lcdText(g, "EXIT:BACK", { 60, 53, 66, 8 }, .8f);
        return;
    }
    const bool distortionPage = model.trackEditorOpen && model.trackEditorSlot == 2;
    if (distortionPage)
    {
        VisualStyle::lcdText(g, "FX A-C DISTORTION", { 1, 0, 94, 8 }, 0.8f);
        VisualStyle::lcdText(g, model.trackFx.distortionEnabled ? "ON" : "OFF", { 96, 0, 16, 8 }, 0.7f);
    }
    else VisualStyle::lcdText(g, model.trackEditorOpen ? "TRACK FX:FX A-A" : "INPUT FX:FX A-" + editor.slotLetter(), { 1, 0, 77, 8 }, 0.8f);
    if (!model.trackEditorOpen && editor.selectedType()==5 && editor.page>=2)
    {
        g.setColour(lcdBackground); g.fillRect(0,0,112,8); g.setColour(lcdInk);
        VisualStyle::lcdText(g,"FX SEQ:A-"+editor.slotLetter()+" "+juce::String(editor.page-1)+"/6",{1,0,111,8},.8f);
    }
    g.fillRect(1, 9, 126, 1);
    juce::Path arrows;
    if (! model.trackEditorOpen && editor.page > 0) arrows.addTriangle(114, 4, 118, 1, 118, 7);
    if (! model.trackEditorOpen && editor.page + 1 < editor.pageCount()) arrows.addTriangle(127, 4, 123, 1, 123, 7);
    if (model.trackEditorOpen && model.trackEditorSlot == 2) arrows.addTriangle(114, 4, 118, 1, 118, 7);
    if (model.trackEditorOpen && model.trackEditorSlot == 0) arrows.addTriangle(127, 4, 123, 1, 123, 7);
    g.fillPath(arrows);
    if (model.trackEditorOpen && ! distortionPage)
    {
        VisualStyle::lcdText(g, "LPF", { 69, 15, 57, 10 }, 1.3f);
        VisualStyle::lcdText(g, model.trackFx.enabled ? "ON" : "OFF", { 69, 32, 57, 8 }, 1.0f);
        VisualStyle::lcdText(g, insertName(), { 69, 53, 57, 8 }, 1.0f);
    }
    for (int i = 0; i < 4; ++i)
    {
        const auto field = getEditorField(i);
        if (! field.available) continue;
        const float x = static_cast<float>(i) * 32.0f;
        g.setColour(lcdInk);
        const float textPixel = juce::jmin(1.0f, 30.0f / (field.text.length() * 6.0f - 1.0f));
        VisualStyle::lcdText(g, field.text, { x, 12, 32, 10 }, textPixel);
        // A stepped, pixel-grid disc and pointer, matching the monochrome knob icons.
        for (int row = -7; row <= 7; ++row)
            for (int col = -7; col <= 7; ++col)
                if (row * row + col * col <= 49)
                    g.fillRect(x + 16 + col, 34.0f + row, 1.0f, 1.0f);
        g.fillRect(x + 8, 41.0f, 2.0f, 2.0f);
        g.fillRect(x + 23, 41.0f, 2.0f, 2.0f);
        const float angle = juce::degreesToRadians(-135.0f + 270.0f * (field.value - field.minimum) / juce::jmax(1,field.maximum - field.minimum));
        const juce::Point<float> centre(x + 16, 34);
        const auto tip = centre.getPointOnCircumference(5.0f, angle);
        g.setColour(lcdBackground);
        g.drawLine({ centre, tip }, 2.0f);
        g.setColour(lcdInk);
        if (field.bottomLabel.isEmpty())
            VisualStyle::lcdText(g, field.topLabel, { x, 53, 32, 7 }, juce::jmin(0.9f, 30.0f / (field.topLabel.length() * 6.0f - 1.0f)));
        else
        {
            VisualStyle::lcdText(g, field.topLabel, { x, 49, 32, 7 }, 0.9f);
            VisualStyle::lcdText(g, field.bottomLabel, { x, 57, 32, 7 }, 0.9f);
        }
    }
}

void MemoryDisplay::paintTrackIcons(juce::Graphics& g, bool showProgress)
{
    for (size_t i = 0; i < model.tracks.size(); ++i)
    {
        const auto& track = model.tracks[i];
        const float x = 3.0f + static_cast<float>(i) * 25.0f;
        const auto shape = clippedCorners({ x, 27, 21, 21 }, 3.0f);
        const bool running = track.state != TrackVisualState::blank;
        g.setColour(lcdInk);
        if (running && ! showProgress)
            g.fillPath(shape); // REC, PLAY and OVERDUB share the status icon.
        else if (running || track.hasPhrase)
        {
            if (running)
            {
                juce::Graphics::ScopedSaveState save(g);
                g.reduceClipRegion(shape);
                juce::Path sector;
                // Fill clockwise from six o'clock, clipped to the octagonal icon.
                sector.addPieSegment(x - 5, 22, 31, 31, juce::MathConstants<float>::pi,
                    juce::MathConstants<float>::pi + juce::MathConstants<float>::twoPi * simulation.get(i).progress, 0);
                g.fillPath(sector);
            }
            g.strokePath(shape, juce::PathStrokeType(0.8f));
        }
        else
            g.strokePath(clippedCorners({ x + 1, 44, 19, 3 }, 1.5f), juce::PathStrokeType(0.8f));

        if (static_cast<int>(i) == model.currentTrack && (running || track.hasPhrase))
        {
            const auto marker = clippedCorners({ x + 8, 35, 5, 5 }, 1.0f);
            g.setColour(showProgress || running ? lcdBackground : lcdInk);
            g.fillPath(marker);
            if (showProgress)
            {
                g.setColour(lcdInk);
                g.strokePath(marker, juce::PathStrokeType(0.8f));
            }
        }
    }
}

void MemoryDisplay::paintLoopStatus(juce::Graphics& g)
{
    g.setColour(lcdInk);
    VisualStyle::lcdText(g, "TRK", { 2, 35, 22, 14 }, 1.25f);
    VisualStyle::lcdText(g, juce::String(model.currentTrack + 1) + ":", { 25, 27, 22, 23 }, 2.0f);
    const juce::String state = loopStateName(model.tracks[static_cast<size_t>(model.currentTrack)].state);
    // OVERDUB needs narrower lettering to fit the original status field.
    const float pixel = juce::jmin(2.8f, 76.0f / (state.length() * 6.0f - 1.0f));
    VisualStyle::lcdText(g, state, { 50, 27, 76, 23 }, pixel);
}

void MemoryDisplay::paintLoopLevel(juce::Graphics& g)
{
    g.setColour(lcdInk);
    for (size_t i = 0; i < model.tracks.size(); ++i)
        for (size_t channel = 0; channel < 2; ++channel)
        {
            const auto& motion = simulation.get(i);
            const float x = 3.0f + static_cast<float>(i) * 25.0f + static_cast<float>(channel) * 10.0f;
            const int height = juce::roundToInt(motion.level[channel] * 22.0f);
            if (height > 0) g.fillRect(x, 48.0f - height, 9.0f, static_cast<float>(height));
            if (motion.peak[channel] > 0.0f)
                g.fillRect(x, 46.0f - juce::roundToInt(motion.peak[channel] * 22.0f), 9.0f, 1.0f);
            else
                g.fillRect(x, 48.0f, 9.0f, 1.0f);
        }
}

void MemoryDisplay::paintEffects(juce::Graphics& g, EffectGroup group)
{
    const bool input = group == EffectGroup::input;
    const auto& effects = input ? model.inputEffects : model.trackEffects;
    // The suffix A is the reference's fixed bank label, not a selected effect slot.
    g.setColour(lcdInk);
    VisualStyle::lcdText(g, input ? "INPUT FX-A" : "TRACK FX-A", { 2, 24, 41.3f, 6 }, 0.7f);
    for (size_t i = 0; i < effects.size(); ++i)
    {
        const auto& effect = effects[i];
        const juce::Rectangle<float> cell(1.0f + static_cast<float>(i) * 32.0f, 31, 29, 17);
        g.setColour(lcdInk);
        if (effect.enabled) g.fillRect(cell);
        g.setColour(effect.enabled ? lcdBackground : lcdInk);
        if (effect.lines[1].isEmpty())
            VisualStyle::lcdText(g, effect.lines[0], cell, 1.2f);
        else
        {
            VisualStyle::lcdText(g, effect.lines[0], cell.withHeight(8), 1.1f);
            VisualStyle::lcdText(g, effect.lines[1], cell.withY(40).withHeight(8), 1.1f);
        }
    }
}
