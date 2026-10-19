#pragma once
#include <algorithm>
#include <cmath>

// Message-thread pickup state only. The effect editor remains the parameter owner.
enum class FxKnobMode { immediate, hook };
class FxKnobPickup
{
public:
    void bind(int identity, int effectiveValue, int maximum, FxKnobMode newMode)
    {
        if (identity != target || effectiveValue != expected || maximum != steps || newMode != mode)
            captured = false;
        target = identity; expected = effectiveValue; steps = maximum; mode = newMode;
    }
    bool move(double next, int& value)
    {
        next = std::clamp(next, 0.0, 1.0);
        const double previous = physical;
        physical = next;
        value = int(std::lround(next * steps));
        if (steps <= 0 || next == previous) return false;
        const double effective = double(expected) / steps;
        if (mode == FxKnobMode::immediate || value == expected
            || (previous <= effective && next >= effective)
            || (previous >= effective && next <= effective)) captured = true;
        if (!captured) return false;
        expected = value; // Acknowledge this write; subsequent external edits re-arm pickup.
        return true;
    }
    bool waiting() const { return mode == FxKnobMode::hook && !captured && steps > 0; }
    double position() const { return physical; }
    double effectivePosition() const { return steps > 0 ? double(expected) / steps : physical; }
private:
    int target = -1, expected = 0, steps = 0;
    double physical = 0.5;
    FxKnobMode mode = FxKnobMode::immediate;
    bool captured = false;
};
