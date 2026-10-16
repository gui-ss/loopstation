#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include "RobotParameters.h"

// INPUT FX bank A: ROBOT has DSP; earlier types remain visual. Each slot retains its selected type and per-type values.
struct InputFxEditor
{
    inline static constexpr std::array<const char*, 5> types { "LPF", "BPF", "HPF", "PHASER", "ROBOT" };
    struct Field
    {
        juce::String name, topLabel, bottomLabel, text;
        int value = 0, maximum = 100;
        bool available = false;
        int minimum = 0;
    };
    struct Slot
    {
        int type = 0;
        RobotParameters robot; // Separate storage never overwrites the previous effect values.
        // RATE, DEPTH, RESONANCE, CUTOFF/MANUAL, STEP RATE, D.LEVEL, E.LEVEL, STAGE.
        // STEP RATE index 0 is OFF; 1..101 means 0..100. RATE remains numeric.
        std::array<std::array<int, 8>, 4> values {{
            {{50, 50, 50, 50, 0, 100, 50, 1}}, {{50, 50, 50, 50, 0, 100, 50, 1}},
            {{50, 50, 50, 50, 0, 100, 50, 1}}, {{50, 50, 50, 50, 0, 100, 50, 1}}
        }};
    };
    bool open = false;
    int selectedSlot = 0, page = 0;
    std::array<Slot, 4> slots;

    InputFxEditor()
    {
        for (size_t i = 0; i < slots.size(); ++i) slots[i].type = static_cast<int>(i);
    }
    int selectedType() const { return slots[static_cast<size_t>(selectedSlot)].type; }
    juce::String slotLetter() const
    {
        return juce::String::charToString(static_cast<juce::juce_wchar>('A' + selectedSlot));
    }
    void selectSlot(int index)
    {
        if (! open || ! juce::isPositiveAndBelow(index, 4) || index == selectedSlot) return;
        selectedSlot = index;
        page = 0; // A different type may have fewer pages; always reveal its FX TYPE.
    }
    int pageCount() const { return selectedType() == 4 ? 1 : selectedType() == 3 ? 3 : 2; }
    int parameterIndex(int knob) const { return page * 4 + knob - 1; }
    Field field(int knob) const
    {
        if (! juce::isPositiveAndBelow(knob, 4)) return {};
        const auto& slot = slots[static_cast<size_t>(selectedSlot)];
        const int type = slot.type;
        if (page == 0 && knob == 0)
            return { "FX TYPE", "FX", "TYPE", types[static_cast<size_t>(type)], type, 4, true };
        if (type == 4)
        {
            if (knob == 1) return { "NOTE", "NOTE", "", RobotParameters::notes[size_t(slot.robot.note)], slot.robot.note, 11, true };
            if (knob == 2) return { "FORMANT", "FOR-", "MANT", (slot.robot.formant > 0 ? "+" : "") + juce::String(slot.robot.formant), slot.robot.formant, 50, true, -50 };
            if (knob == 3) return { "MODE", "MODE", "", juce::String(slot.robot.mode), slot.robot.mode, 2, true, 1 };
        }
        const int index = parameterIndex(knob);
        if (! juce::isPositiveAndBelow(index, type == 3 ? 8 : 5)) return {};
        const int value = slot.values[static_cast<size_t>(type)][static_cast<size_t>(index)];
        const char* name[] = { "RATE", "DEPTH", "RESONANCE", type == 3 ? "MANUAL" : "CUTOFF",
                               "STEP RATE", "D.LEVEL", "E.LEVEL", "STAGE" };
        const char* top[] = { "RATE", "DEPTH", "RESO-", type == 3 ? "MANUAL" : "CUT",
                              "STEP", "D.LEVEL", "E.LEVEL", "STAGE" };
        const char* bottom[] = { "", "", "NANCE", type == 3 ? "" : "OFF", "RATE", "", "", "" };
        juce::String text = juce::String(value);
        int maximum = 100;
        if (index == 4) { maximum = 101; text = value == 0 ? "OFF" : juce::String(value - 1); }
        if (index == 7)
        {
            maximum = 3;
            const char* stages[] = { "4", "8", "12", "BI-PHASE" };
            text = stages[value];
        }
        return { name[index], top[index], bottom[index], text, value, maximum, true };
    }
    void setField(int knob, int value)
    {
        if (! open) return;
        const auto item = field(knob);
        if (! item.available) return;
        value = juce::jlimit(item.minimum, item.maximum, value);
        auto& slot = slots[static_cast<size_t>(selectedSlot)];
        if (page == 0 && knob == 0) slot.type = value;
        else if (slot.type == 4)
        {
            if (knob == 1) slot.robot.note = value;
            if (knob == 2) slot.robot.formant = value;
            if (knob == 3) slot.robot.mode = value;
        }
        else slot.values[static_cast<size_t>(slot.type)][static_cast<size_t>(parameterIndex(knob))] = value;
    }
    void navigate(int direction) { page = juce::jlimit(0, pageCount() - 1, page + (direction > 0 ? 1 : -1)); }
};
