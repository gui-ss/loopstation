#pragma once
#include "LoopTrack.h"
#include <array>
#include <atomic>
#include <cmath>

// PG p.30: the outer indicator is independent from REC/PLAY/DUB button colour.
enum class IndicatorMode { status, loopPosition, level };
inline const char* indicatorModeName(IndicatorMode mode) noexcept
{
    constexpr const char* names[]{"STATUS","LOOP POSITION","LEVEL"};
    return names[std::clamp(int(mode),0,2)];
}
struct TrackIndicatorReading
{
    LoopTrack::Snapshot transport{};
    std::array<float,2> level{}; // Post-fader, post-individual INSERT; before common ALL bus.
};
struct TrackIndicatorFrame
{
    std::array<TrackIndicatorReading,5> tracks{};
    double beats=0;
    bool running=false;
};

// Single audio/lifecycle writer. All payload fields are atomic, so even a
// rejected read is legal C++. Sequential ordering makes the version check
// cover every payload access. Neither side waits; UI keeps its last good frame.
class TrackIndicatorMailbox final
{
public:
    void publish(const TrackIndicatorFrame& frame) noexcept
    {
        version.fetch_add(1);
        for(size_t i=0;i<5;++i)
        {
            const auto& t=frame.tracks[i];const auto& s=t.transport;
            const auto flags=uint64_t(s.state)|(s.canUndo?8u:0u)|(s.awaitingBoundary?16u:0u)|(s.recordingQuantized?32u:0u);
            tracks[i].stateLength.store((uint64_t(s.frames)<<32)|flags);
            tracks[i].progress.store(s.progress);
            for(size_t ch=0;ch<2;++ch) tracks[i].level[ch].store(t.level[ch]);
        }
        beats.store(frame.beats);running.store(frame.running);
        version.fetch_add(1);
    }
    bool read(TrackIndicatorFrame& destination) const noexcept
    {
        for(int attempt=0;attempt<2;++attempt)
        {
            const auto before=version.load();if(before&1u) continue;
            TrackIndicatorFrame frame;
            for(size_t i=0;i<5;++i)
            {
                const auto word=tracks[i].stateLength.load();
                frame.tracks[i].transport={LoopTrack::State(word&7u),int(word>>32),(word&8u)!=0,
                    tracks[i].progress.load(),(word&16u)!=0,(word&32u)!=0};
                for(size_t ch=0;ch<2;++ch) frame.tracks[i].level[ch]=tracks[i].level[ch].load();
            }
            frame.beats=beats.load();frame.running=running.load();
            if(before==version.load()){destination=frame;return true;}
        }
        return false;
    }
private:
    struct PublishedTrack
    {
        std::atomic<uint64_t> stateLength{0};
        std::atomic<float> progress{0};
        std::array<std::atomic<float>,2> level{};
    };
    std::array<PublishedTrack,5> tracks{};
    std::atomic<uint64_t> version{0};
    std::atomic<double> beats{0};
    std::atomic<bool> running{false};
    static_assert(std::atomic<uint64_t>::is_always_lock_free&&std::atomic<float>::is_always_lock_free
                  &&std::atomic<double>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free);
};

// Drawing fraction only. No clock or accumulated animation state in the UI.
inline float indicatorLevel(float amplitude) noexcept
{
    if(!std::isfinite(amplitude)||amplitude<=.001f) return 0;
    return std::clamp((20.f*std::log10(amplitude)+60.f)/60.f,0.f,1.f);
}
inline float indicatorFill(IndicatorMode mode,const TrackIndicatorReading& track,double beats) noexcept
{
    using State=LoopTrack::State;
    const auto& s=track.transport;
    if(s.state==State::empty) return 0;
    const bool playing=s.state==State::playing||s.state==State::overdubbing;
    if(mode==IndicatorMode::loopPosition) return playing&&s.frames>0 ? std::clamp(s.progress,0.f,1.f) : 0;
    if(mode==IndicatorMode::level) return playing ? indicatorLevel(std::max(track.level[0],track.level[1])) : 0;
    if(s.state==State::stopped) return s.frames>0?1.f:0.f;
    const double phase=std::isfinite(beats)&&beats>=0?beats:0;
    if(s.state==State::recording||s.state==State::overdubbing) return phase-std::floor(phase)<.5 ? 1.f:0.f;
    return float(std::fmod(phase,4.)/4.); // STATUS PLAY shows one measure, not whole phrase.
}
