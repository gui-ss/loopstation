#include "TopPanelComponent.h"
#include "MusicalClock.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool ok, const char* message) { if (! ok) throw std::runtime_error(message); }
}
void runTempoPanelTests()
{
    TopPanelComponent panel; panel.setSize(1200, 248);
    MusicalClock clock;
    const auto refresh = [&] { panel.updateClock(clock.bpm(), clock.position().beats, { .25f, .5f, .75f, .125f, .875f }, true); };
    int taps = 0;
    panel.onTapTempo = [&] { ++taps; refresh(); };
    panel.onBpmChanged = [&](double bpm) { clock.setBpm(bpm); refresh(); };
    juce::Button *tap = nullptr, *exit = nullptr, *inputEdit = nullptr;
    ParameterKnob* fourth = nullptr;
    MemoryDisplay* display = nullptr;
    for (auto* child : panel.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*>(child))
        {
            if (b->getButtonText() == "TAP\nTEMPO") tap = b;
            if (b->getButtonText() == "EXIT") exit = b;
            if (b->getTitle() == "INPUT FX EDIT") inputEdit = b;
        }
        if (auto* k = dynamic_cast<ParameterKnob*>(child))
            if (k->getName() == "Parameter knob 4") fourth = k;
        if (auto* d = dynamic_cast<MemoryDisplay*>(child)) display = d;
    }
    check(tap && exit && fourth && display && inputEdit, "Tempo controls missing");
    tap->onClick();
    check(taps == 1 && display->isTempoEditing() && fourth->isEnabled(), "TAP did not open knob-4 tempo editor");
    fourth->setValue(900, juce::sendNotificationSync);
    check(clock.bpm() == 90 && display->getTitle().contains("90.0"), "Manual BPM not shared immediately");
    const auto event = [&](int mods)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), { 20, 20 },
            juce::ModifierKeys(mods), 1, 0, 0, 0, 0, fourth, fourth, juce::Time::getCurrentTime(),
            { 20, 20 }, juce::Time::getCurrentTime(), 1, false);
    };
    juce::MouseWheelDetails wheel {}; wheel.deltaY = 0.1f;
    fourth->mouseWheelMove(event(0), wheel);
    check(clock.bpm() == 91, "Tempo wheel did not step by 1 BPM");
    fourth->mouseWheelMove(event(juce::ModifierKeys::shiftModifier), wheel);
    check(std::abs(clock.bpm() - 91.1) < 1e-12, "Fine tempo wheel lost decimal");
    fourth->setValue(400, juce::sendNotificationSync); wheel.deltaY = -0.1f;
    fourth->mouseWheelMove(event(0), wheel); check(clock.bpm() == 40, "Tempo knob exceeded lower limit");
    fourth->setValue(3000, juce::sendNotificationSync); wheel.deltaY = 0.1f;
    fourth->mouseWheelMove(event(0), wheel); check(clock.bpm() == 300, "Tempo knob exceeded upper limit");
    fourth->setValue(1400, juce::sendNotificationSync); exit->onClick();
    check(! display->isTempoEditing() && ! fourth->isEnabled(), "EXIT left tempo editing active");
    for (int i = 0; i < 7; ++i)
    { display->navigate(1); check(display->getTitle().contains("BPM 140.0"), "Playback page retained stale BPM"); }
    inputEdit->onClick(); check(display->isInputEditing(), "Input editor unavailable");
    const auto previousField = display->getEditorField(3).value;
    tap->onClick(); fourth->setValue(1200, juce::sendNotificationSync); exit->onClick();
    check(display->isInputEditing() && display->getEditorField(3).value == previousField,
          "Tempo editor changed FX parameter or lost return page");
    panel.updateClock(120, 8.01, { .25f, .5f, .75f, .125f, .875f }, true); check(tap->getToggleState(), "Beat pulse absent");
    panel.updateClock(120, 8.5, { .25f, .5f, .75f, .125f, .875f }, true); check(! tap->getToggleState(), "Beat pulse not clock driven");
    panel.updateClock(120, 8.01, { .25f, .5f, .75f, .125f, .875f }, false); check(! tap->getToggleState(), "Offline clock kept blinking");
    DisplaySimulation motion; DisplayModel model;
    for (auto& track : model.tracks) track.state = TrackVisualState::play;
    motion.updateProgress({ .25f, .5f, .75f, .125f, .875f }, model); motion.advance(100, model);
    check(motion.get(0).progress == .25f, "Visual timer advanced real loop progress");
    check(motion.get(1).progress == .5f && motion.get(4).progress == .875f,
          "Tracks 2-5 did not use independent real positions");
    display->selectTrack(2);
    display->updateTrack(4, TrackVisualState::play, true);
    display->updateTrack(0, TrackVisualState::rec, true);
    check(display->getTitle().contains("CURRENT:3") && display->getTitle().contains("UNDO TARGET:TRK3"),
          "Asynchronous transport update stole selected Undo target");
    display->updateQuantize(true, true, true, true);
    check(display->getTitle().contains("QUANTIZE:ON") && display->getTitle().contains("REC WAIT BAR")
          && display->getTitle().contains("1:REC"), "Pending quantize not shown as REC/WAIT");
    display->updateQuantize(false, true, true, true);
    check(display->getTitle().contains("QUANTIZE:OFF") && display->getTitle().contains("REC QUANTIZE:ON"),
          "Next-take Q option confused with active recording grid");
    display->updateQuantize(false, false, false, false);
    check(! display->getTitle().contains("REC WAIT BAR"), "Completed/cancelled recording retained WAIT");
    display->updateLoopSync(true, "WAIT REC", false);
    display->setInputEditing(false); display->setTrackEditing(false); display->setTempoEditing(false);
    {
        const auto preview = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mk2-6gb-display.png");
        juce::FileOutputStream out(preview); out.setPosition(0); out.truncate();
        juce::PNGImageFormat png;
        check(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(), true, 4.0f), out),
              "Could not render LOOP SYNC display preview");
    }
    check(display->getTitle().contains("LOOP SYNC:ON") && display->getTitle().contains("MODE:MEASURE")
          && display->getTitle().contains("WAIT REC"), "LCD did not expose pending MEASURE recording");
    display->updateLoopSync(true, "INCOMPAT: L OFF", true);
    check(display->getTitle().contains("INCOMPAT") && display->getTitle().contains("BPM BLOCKED"),
          "LCD hid incompatible phrase/tempo refusal");
    display->updateLoopSync(false, "", false);
    check(display->getTitle().contains("LOOP SYNC:OFF") && ! display->getTitle().contains("WAIT REC")
          && ! display->getTitle().contains("BPM BLOCKED"), "LCD retained cancelled sync warning");
    std::cout << "PASS: TAP routing, knob 4, decimal wheel, EXIT/FX preservation, seven BPM screens and sample-clock pulse\n";
}
