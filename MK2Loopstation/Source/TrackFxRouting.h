#pragma once
#include "TrackEffects.h"
#include <array>

// Parameter Guide pp. 2, 6: FX is per track, INSERT is per shared slot.
// 0 = ALL; 1..5 = TRACK1..5. Defaults retain the earlier TRACK1 insert.
static_assert(std::atomic<unsigned>::is_always_lock_free, "Routing controls must be lock-free");

class TrackFxRouting final
{
public:
    void setParameters(TrackFxParameters value) noexcept { effects.setParameters(value); }
    void setInsert(int slot, int destination) noexcept
    {
        if ((slot != 0 && slot != 2) || destination < 0 || destination > 5) return;
        const unsigned shift = slot == 0 ? 0u : 3u;
        auto old = targets.load(std::memory_order_relaxed);
        while (! targets.compare_exchange_weak(old, (old & ~(7u << shift)) | (unsigned(destination) << shift),
                                               std::memory_order_relaxed)) {}
    }
    int insert(int slot) const noexcept
    { return int((targets.load(std::memory_order_relaxed) >> (slot == 0 ? 0u : 3u)) & 7u); }
    void setTrackEnabled(int track, bool enabled) noexcept
    {
        if (track < 0 || track >= 5) return;
        const auto bit = 1u << unsigned(track);
        if (enabled) mask.fetch_or(bit, std::memory_order_relaxed);
        else mask.fetch_and(~bit, std::memory_order_relaxed);
    }
    bool trackEnabled(int track) const noexcept
    { return track >= 0 && track < 5 && (mask.load(std::memory_order_relaxed) & (1u << unsigned(track))) != 0; }
    void prepare(double rate) noexcept
    {
        effects.prepare(rate);
        current = requested = targets.load(std::memory_order_relaxed);
        routeGain.reset(rate, .02); routeGain.setCurrentAndTargetValue(1);
        for (int i = 0; i < 5; ++i)
        { gates[size_t(i)].reset(rate, .02); gates[size_t(i)].setCurrentAndTargetValue(trackEnabled(i) ? 1.f : 0.f); }
    }
    void beginBlock() noexcept
    {
        effects.beginBlock();
        requested = targets.load(std::memory_order_relaxed);
        routeGain.setTargetValue(requested == current ? 1.f : 0.f);
        const auto enabled = mask.load(std::memory_order_relaxed);
        for (int i = 0; i < 5; ++i) gates[size_t(i)].setTargetValue((enabled & (1u << unsigned(i))) ? 1.f : 0.f);
    }
    void processFrame(const std::array<float, 5>& inputL, const std::array<float, 5>& inputR,
                      float& left, float& right, const std::array<float, 5>& gains,
                      std::array<std::array<float,2>,5>* meters=nullptr) noexcept
    {
        if (! routeGain.isSmoothing() && routeGain.getCurrentValue() == 0 && current != requested)
        { current = requested; effects.reset(); routeGain.setTargetValue(1); }
        const float route = routeGain.getNextValue();
        std::array<float, 5> l {}, r {}, gateValues {};
        float dryL = 0, dryR = 0, bypassL = 0, bypassR = 0;
        for (size_t i = 0; i < l.size(); ++i)
        {
            const float gate = gates[i].getNextValue();
            gateValues[i]=gate;
            dryL += inputL[i] * gains[i]; dryR += inputR[i] * gains[i];
            bypassL += inputL[i] * (1 - gate) * gains[i]; bypassR += inputR[i] * (1 - gate) * gains[i];
            l[i] = inputL[i] * gate; r[i] = inputR[i] * gate;
        }
        const int a = int(current & 7u), c = int((current >> 3u) & 7u);
        // Individual inserts precede the common ALL bus. Same-scope slots are A -> C.
        // One processor per slot, even with ALL: nonlinear processing acts on the sum.
        if (a != 0) effects.processLpf(l[size_t(a - 1)], r[size_t(a - 1)]);
        if (c != 0) effects.processDistortion(l[size_t(c - 1)], r[size_t(c - 1)]);
        float busL = 0, busR = 0;
        for (size_t i = 0; i < l.size(); ++i) { busL += l[i] * gains[i]; busR += r[i] * gains[i]; }
        // Observe contributions without changing the mix or duplicating shared FX.
        // ALL is nonlinear processing of a sum, so it cannot be attributed per track.
        if(meters) for(size_t i=0;i<5;++i)
        {
            (*meters)[i][0]=std::abs(gains[i]*(inputL[i]+route*(l[i]-inputL[i]*gateValues[i])));
            (*meters)[i][1]=std::abs(gains[i]*(inputR[i]+route*(r[i]-inputR[i]*gateValues[i])));
        }
        if (a == 0) effects.processLpf(busL, busR);
        if (c == 0) effects.processDistortion(busL, busR);
        left = dryL + route * (bypassL + busL - dryL);
        right = dryR + route * (bypassR + busR - dryR);
    }
private:
    TrackEffects effects;
    std::atomic<unsigned> targets { 1u | (1u << 3u) }, mask { 31u };
    unsigned current = 9u, requested = 9u;
    juce::SmoothedValue<float> routeGain { 1.f };
    std::array<juce::SmoothedValue<float>, 5> gates;
};
