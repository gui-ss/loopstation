#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// Tempo is a single atomic control, in tenths of BPM. Phase belongs exclusively
// to the audio callback; UI reads the last published phase, never advances it.
class MusicalClock final
{
public:
    struct Position
    {
        double beats;
        uint64_t beat; // Zero-based absolute beat.
        uint64_t bar;  // Zero-based 4/4 bar.
        int beatInBar; // 0..3
        double fraction;
    };
    struct SyncBlock
    {
        bool enabled = false;
        double bar = 0, framesPerBar = 0; // Position at block START, from this same clock.
        bool configured = false; // SW can remain ON when an operation is refused.
    };
    struct Timing { double beatsPerSample = 1./24000., secondsPerSample = 1./48000.; };
    Timing blockTiming() const noexcept { return timing; } // Audio thread only.
    static constexpr double minimumBpm = 40.0, maximumBpm = 300.0;
    bool setBpm(double bpm) noexcept
    {
        if (! std::isfinite(bpm)) return false;
        const int requested = static_cast<int>(std::lround(std::clamp(bpm, minimumBpm, maximumBpm) * 10));
        auto previous = tempo.load(std::memory_order_relaxed);
        do
        {
            if ((previous & tempoLock) != 0) return (previous & 0xffff) == requested;
        } while (! tempo.compare_exchange_weak(previous, requested, std::memory_order_relaxed));
        return true;
    }
    double bpm() const noexcept { return (tempo.load(std::memory_order_relaxed) & 0xffff) * 0.1; }
    bool tempoLocked() const noexcept { return (tempo.load(std::memory_order_relaxed) & tempoLock) != 0; }
    // Audio thread only. Lock and read tempo atomically, so a concurrent UI edit
    // cannot slip between checking material and establishing the shared grid.
    void beginSync() noexcept
    {
        if (syncPeriod > 0) return;
        const double lockedBpm = (tempo.fetch_or(tempoLock, std::memory_order_relaxed) & 0xffff) * .1;
        const double frames = std::max(1.0, std::round(sampleRate * 240.0 / lockedBpm));
        syncPeriod = frames / sampleRate;
        const double nextMeasure = std::ceil(beats / 4.0 - 1.e-10) * 4.0;
        const double waitFrames = std::max(0.0, std::ceil((nextMeasure - beats) * frames / 4.0 - 1.e-7));
        syncOrigin = beats + waitFrames * 4.0 / frames;
    }
    void endSync() noexcept
    {
        syncPeriod = 0;
        tempo.fetch_and(~tempoLock, std::memory_order_relaxed);
    }
    SyncBlock syncBlock() const noexcept
    { return { syncPeriod > 0, (beats - syncOrigin) / 4.0, syncPeriod * sampleRate }; }
    // Device lifecycle, with callback stopped. Preserve musical phase on restart.
    void prepare(double rate) noexcept
    { sampleRate = std::isfinite(rate) && rate > 0 ? rate : 48000.0; }
    // Returns the 4/4 bar length from the very same tempo read used for this
    // block. Tracks may latch it for a recording without resetting this clock.
    double process(int samples) noexcept
    {
        const double blockBpm = bpm();
        const double nominalBarFrames = 240.0 * sampleRate / blockBpm;
        const double barFrames = syncPeriod > 0 ? syncPeriod * sampleRate : nominalBarFrames;
        timing = {4.0 / barFrames, 1.0 / sampleRate};
        if (samples <= 0) return nominalBarFrames;
        // Compensated accumulation limits rounding drift across small blocks.
        const double increment = samples * 4.0 / barFrames - rounding;
        const double next = beats + increment;
        rounding = (next - beats) - increment;
        beats = next;
        published.store(beats, std::memory_order_relaxed);
        return nominalBarFrames;
    }
    Position position() const noexcept
    {
        const double value = published.load(std::memory_order_relaxed);
        const auto beat = static_cast<uint64_t>(value);
        return { value, beat, beat / 4, static_cast<int>(beat % 4), value - static_cast<double>(beat) };
    }
private:
    Timing timing;
    static constexpr int tempoLock = 1 << 16;
    double syncPeriod = 0, syncOrigin = 0; // Seconds/bar and beat origin, not another running clock.
    std::atomic<int> tempo { 1200 };
    std::atomic<double> published { 0 };
    double sampleRate = 48000, beats = 0, rounding = 0;
    static_assert(std::atomic<int>::is_always_lock_free && std::atomic<double>::is_always_lock_free);
};

// Message-thread only. Times are monotonic seconds; explicit timestamps allow
// deterministic tests without waiting or depending on a GUI timer.
class TapTempo final
{
public:
    void reset() noexcept { started = false; count = head = 0; }
    double tap(double now) noexcept // 0 means no valid estimate yet.
    {
        if (! std::isfinite(now) || now < 0) return 0;
        if (! started) { last = now; started = true; return 0; }
        const double interval = now - last;
        if (interval <= 0 || interval > 60.0 / MusicalClock::minimumBpm + 1.0e-9)
        { reset(); last = now; started = true; return 0; }
        // Ignore bounce, but do not alias sustained out-of-range tapping.
        if (interval < 0.08) return 0;
        if (interval < 60.0 / MusicalClock::maximumBpm - 1.0e-9)
        { reset(); last = now; started = true; return 0; }
        last = now;
        intervals[head] = interval;
        head = (head + 1) % intervals.size();
        count = std::min(count + 1, intervals.size());
        auto sorted = intervals;
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(count));
        const size_t trim = count >= 3 ? 1 : 0;
        double sum = 0;
        for (size_t i = trim; i < count - trim; ++i) sum += sorted[i];
        return std::clamp(60.0 * static_cast<double>(count - 2 * trim) / sum,
                          MusicalClock::minimumBpm, MusicalClock::maximumBpm);
    }
private:
    std::array<double, 5> intervals {};
    size_t count = 0, head = 0;
    double last = 0;
    bool started = false;
};
