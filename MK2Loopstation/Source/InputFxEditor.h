#pragma once
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include "RobotParameters.h"
#include "OscBotParameters.h"
#include "ReverbParameters.h"
#include "StepSlicerParameters.h"
#include "G2BParameters.h"
#include "FxSequence.h"

// INPUT FX A-D banks. Each slot retains its selected type and per-type values in RAM.
struct InputFxEditor
{
    inline static constexpr std::array<const char*, 9> types { "LPF", "BPF", "HPF", "PHASER", "ROBOT", "OSC BOT", "REVERB", "STEP SLICER", "G2B" };
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
        OscBotParameters oscBot;
        FxSequenceParameters sequence;
        ReverbParameters reverb;
        StepSlicerParameters slicer;
        G2BParameters g2b;
        // RATE, DEPTH, RESONANCE, CUTOFF/MANUAL, STEP RATE, D.LEVEL, E.LEVEL, STAGE.
        // STEP RATE index 0 is OFF; 1..101 means 0..100. RATE remains numeric.
        std::array<std::array<int, 8>, 4> values {{
            {{50, 50, 50, 50, 0, 100, 50, 1}}, {{50, 50, 50, 50, 0, 100, 50, 1}},
            {{50, 50, 50, 50, 0, 100, 50, 1}}, {{50, 50, 50, 50, 0, 100, 50, 1}}
        }};
    };
    bool open = false;
    bool multi = true; // Preserve the existing simultaneous-slot behaviour.
    int selectedSlot = 0, page = 0, bank = 0;
    std::array<Slot, 4> slots;
    std::array<std::array<Slot,4>,4> savedBanks;
    std::array<int,4> bankTargets {}; // FX TARGET is independent of the edited slot.
    std::array<bool,4> bankMulti {{true,true,true,true}};

    InputFxEditor()
    {
        for (size_t i = 0; i < slots.size(); ++i) slots[i].type = static_cast<int>(i);
        for(auto& b:savedBanks) b=slots;
    }
    juce::String bankLetter() const { return juce::String::charToString(juce::juce_wchar('A'+bank)); }
    void selectBank(int index)
    {
        if(!juce::isPositiveAndBelow(index,4)||index==bank) return;
        savedBanks[size_t(bank)]=slots; bankMulti[size_t(bank)]=multi;
        bank=index; slots=savedBanks[size_t(bank)]; multi=bankMulti[size_t(bank)];
        page=pageCount()-1;
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
    int pageCount() const { return 1+(selectedType()==8?1:selectedType()==7?10:selectedType() == 5 ? 8 : selectedType() == 6 ? 2 : selectedType() == 4 ? 1 : selectedType() == 3 ? 3 : 2); }
    int parameterIndex(int knob) const { return page * 4 + knob - 1; }
    Field field(int knob) const
    {
        if (! juce::isPositiveAndBelow(knob, 4)) return {};
        if(page==pageCount()-1)
        {
            if(knob==0) return {"BANK","BANK","",bankLetter(),bank,3,true};
            if(knob==1) return {"INPUT MODE","INPUT","MODE",multi?"MULTI":"SINGLE",int(multi),1,true};
            if(knob==2) return {"FX TARGET","FX","TARGET",juce::String::charToString(juce::juce_wchar('A'+fxTarget())),fxTarget(),3,true};
            return {};
        }
        const auto& slot = slots[static_cast<size_t>(selectedSlot)];
        const int type = slot.type;
        if (page == 0 && knob == 0)
            return { "FX TYPE", "FX", "TYPE", types[static_cast<size_t>(type)], type, 8, true };
        if(type==8)
        {
            if(knob==1) return {"BALANCE","BAL-","ANCE",juce::String(slot.g2b.balance),slot.g2b.balance,100,true};
            if(knob==2) return {"MODE","MODE","",juce::String(slot.g2b.mode),slot.g2b.mode,2,true,1};
            return {};
        }
        if(type==7)
        {
            const auto& p=slot.slicer;
            if(page==0)
            {
                if(knob==1) return {"RATE","RATE","",p.rate<=100?juce::String(p.rate):juce::String(StepSlicerParameters::rateNames[size_t(p.rate-101)]),p.rate,StepSlicerParameters::lastRate,true};
                if(knob==2) return {"STEP MAX","STEP","MAX",juce::String(p.maximum),p.maximum,16,true,1};
                if(knob==3) return {"DEPTH","DEPTH","",juce::String(p.depth),p.depth,100,true};
            }
            if(page==1)
            {
                if(knob==0) return {"COMP THRESHOLD","COMP","THRESH",juce::String(p.threshold)+"dB",p.threshold,0,true,-30};
                if(knob==1) return {"COMP GAIN","COMP","GAIN","+"+juce::String(p.gain)+"dB",p.gain,20,true};
                return {};
            }
            const int step=(page-2)*2+knob/2;
            if(!juce::isPositiveAndBelow(step,16)) return {};
            const bool length=(knob%2)==0; const int v=length?p.length[size_t(step)]:p.level[size_t(step)];
            return {(length?"LENGTH":"LEVEL")+juce::String(step+1),"STEP"+juce::String(step+1),length?"LENGTH":"LEVEL",juce::String(v),v,100,true,length?1:0};
        }
        if (type == 6)
        {
            const auto& p = slot.reverb;
            const auto cut = [](float hz)
            { return hz < 1000.f ? juce::String(hz, hz == std::floor(hz) ? 0 : 1) + "Hz" : juce::String(hz / 1000.f, 2) + "k"; };
            switch (parameterIndex(knob))
            {
                case 0: return { "TIME", "TIME", "", juce::String(p.time / 10.0, 1) + "s", p.time, 100, true, 1 };
                case 1: return { "PRE DELAY", "PRE", "DELAY", juce::String(p.preDelay) + "ms", p.preDelay, 500, true };
                case 2: return { "DENSITY", "DENSITY", "", juce::String(p.density), p.density, 10, true, 1 };
                case 3: return { "D.LEVEL", "D.LEVEL", "", juce::String(p.dLevel), p.dLevel, 100, true };
                case 4: return { "LOW CUT", "LOW", "CUT", p.lowCutFlat() ? juce::String("FLAT") : cut(p.lowCutHz()), p.lowCut, ReverbParameters::cutSteps, true };
                case 5: return { "HIGH CUT", "HIGH", "CUT", p.highCutFlat() ? juce::String("FLAT") : cut(p.highCutHz()), p.highCut, ReverbParameters::cutSteps, true };
                case 6: return { "E.LEVEL", "E.LEVEL", "", juce::String(p.eLevel), p.eLevel, 100, true };
                default: return {};
            }
        }
        if (type == 5)
        {
            if (page >= 2)
            {
                const auto& q=slot.sequence;
                if (page==2)
                {
                    if (knob==3) return {"TARGET","TAR-","GET","NOTE",0,0,true};
                    const bool v=knob==0?q.enabled:knob==1?q.sync:q.retrig;
                    const char* names[]={"SW","SYNC","RETRIG"};
                    return {names[knob],names[knob],"",v?"ON":"OFF",int(v),1,true};
                }
                if (page==3)
                {
                    if (knob==0)
                    {
                        const char* rates[]={"4MEAS","2MEAS","1MEAS","1/2","1/4","1/8","1/16","1/32"};
                        return {"RATE","RATE","",q.rate<=100?juce::String(q.rate):juce::String(rates[q.rate-101]),q.rate,FxSequenceParameters::lastRate,true};
                    }
                    if (knob==1) return {"MAX","MAX","",juce::String(q.maximum),q.maximum,16,true,1};
                    if (knob==2) return {"INPUT MODE","INPUT","MODE",multi?"MULTI":"SINGLE",int(multi),1,true};
                    return {};
                }
                const int index=(page-4)*4+knob, note=q.notes[size_t(index)];
                const auto name="VAL"+juce::String(index+1);
                return {name,name,"",juce::String(RobotParameters::notes[size_t(note%12)])+juce::String(note/12-1),note,127,true,24};
            }
            const auto& p = slot.oscBot;
            switch (parameterIndex(knob))
            {
                case 0: return { "OSC", "OSC", "", OscBotParameters::waves[size_t(p.osc)], p.osc, 4, true };
                case 1: return { "TONE", "TONE", "", juce::String(p.tone), p.tone, 50, true, -50 };
                case 2: return { "ATTACK", "ATTACK", "", juce::String(p.attack), p.attack, 100, true };
                case 3: return { "NOTE", "NOTE", "", juce::String(RobotParameters::notes[size_t(p.note % 12)]) + juce::String(p.note / 12 - 1), p.note, 127, true, 24 };
                case 4: return { "MOD SENS", "MOD", "SENS", juce::String(p.modSens), p.modSens, 50, true, -50 };
                case 5: return { "BALANCE", "BAL-", "ANCE", juce::String(p.balance), p.balance, 100, true };
                default: return {};
            }
        }
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
        if(page==pageCount()-1)
        { if(knob==0) selectBank(value); if(knob==1) multi=value!=0; if(knob==2) bankTargets[size_t(bank)]=value; return; }
        auto& slot = slots[static_cast<size_t>(selectedSlot)];
        if (page == 0 && knob == 0) slot.type = value;
        else if(slot.type==8)
        { if(knob==1) slot.g2b.balance=value; if(knob==2) slot.g2b.mode=value; }
        else if(slot.type==7)
        {
            auto& p=slot.slicer;
            if(page==0) { if(knob==1) p.rate=value; if(knob==2) p.maximum=value; if(knob==3) p.depth=value; }
            else if(page==1) { if(knob==0) p.threshold=value; if(knob==1) p.gain=value; }
            else { const auto index=size_t((page-2)*2+knob/2); if(knob%2==0) p.length[index]=value; else p.level[index]=value; }
        }
        else if (slot.type == 6)
        {
            auto& p = slot.reverb;
            switch (parameterIndex(knob))
            {
                case 0: p.time = value; break;
                case 1: p.preDelay = value; break;
                case 2: p.density = value; break;
                case 3: p.dLevel = value; break;
                case 4: p.lowCut = value; break;
                case 5: p.highCut = value; break;
                case 6: p.eLevel = value; break;
                default: break;
            }
        }
        else if (slot.type == 4)
        {
            if (knob == 1) slot.robot.note = value;
            if (knob == 2) slot.robot.formant = value;
            if (knob == 3) slot.robot.mode = value;
        }
        else if (slot.type == 5)
        {
            if (page>=2)
            {
                auto& q=slot.sequence;
                if (page==2)
                { if(knob==0) q.enabled=value!=0; if(knob==1) q.sync=value!=0; if(knob==2) q.retrig=value!=0; }
                else if (page==3)
                { if(knob==0) q.rate=value; if(knob==1) q.maximum=value; if(knob==2) multi=value!=0; }
                else q.notes[size_t((page-4)*4+knob)]=value;
                return;
            }
            auto& p = slot.oscBot;
            switch (parameterIndex(knob))
            {
                case 0: p.osc = value; break;
                case 1: p.tone = value; break;
                case 2: p.attack = value; break;
                case 3: p.note = value; break;
                case 4: p.modSens = value; break;
                case 5: p.balance = value; break;
                default: break;
            }
        }
        else slot.values[static_cast<size_t>(slot.type)][static_cast<size_t>(parameterIndex(knob))] = value;
    }
    int fxTarget() const { return bankTargets[size_t(bank)]; }
    Field performanceField() const
    {
        const auto& s=slots[size_t(fxTarget())];
        // Parameter Guide knob pictogram, NOT the FX SEQUENCE star (pp.34-42).
        switch(s.type)
        {
            case 4: return {"NOTE","NOTE","",RobotParameters::notes[size_t(s.robot.note)],s.robot.note,11,true};
            case 5: return {"BALANCE","BAL-","ANCE",juce::String(s.oscBot.balance),s.oscBot.balance,100,true};
            case 6: return {"E.LEVEL","E.LEVEL","",juce::String(s.reverb.eLevel),s.reverb.eLevel,100,true};
            case 7: return {"DEPTH","DEPTH","",juce::String(s.slicer.depth),s.slicer.depth,100,true};
            case 8: return {"BALANCE","BAL-","ANCE",juce::String(s.g2b.balance),s.g2b.balance,100,true};
            default: return {}; // Legacy INPUT filters/PHASER have no DSP; do not pretend otherwise.
        }
    }
    void setPerformanceValue(int value)
    {
        const auto field=performanceField();
        if(!field.available) return;
        value=juce::jlimit(field.minimum,field.maximum,value);
        auto& s=slots[size_t(fxTarget())];
        switch(s.type)
        {
            case 4: s.robot.note=value; break;
            case 5: s.oscBot.balance=value; break;
            case 6: s.reverb.eLevel=value; break;
            case 7: s.slicer.depth=value; break;
            case 8: s.g2b.balance=value; break;
            default: break;
        }
    }
    void navigate(int direction) { page = juce::jlimit(0, pageCount() - 1, page + (direction > 0 ? 1 : -1)); }
};
