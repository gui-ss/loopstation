#include "MusicalClock.h"
#include <stdexcept>
#include <limits>
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
 #include <crtdbg.h>
#endif
namespace
{
void check(bool ok, const char* why) { if (! ok) throw std::runtime_error(why); }
thread_local bool processing = false;
thread_local int allocations = 0;
#if defined(_MSC_VER) && defined(_DEBUG)
int allocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{
    if (processing && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations;
    return 1;
}
#endif
void clockAndTap()
{
    MusicalClock clock;
    const auto advance = [&](int n) { processing = true; clock.process(n); processing = false; };
    clock.prepare(48000); advance(12000);
    check(std::abs(clock.position().beats - 0.5) < 1e-12, "Clock not sample based");
    clock.setBpm(90); advance(48000);
    check(std::abs(clock.position().beats - 2) < 1e-12, "BPM change reset phase");
    clock.prepare(96000); advance(96000);
    check(std::abs(clock.position().beats - 3.5) < 1e-12, "Rate change lost phase");
    clock.setBpm(120); advance(24000);
    check(clock.position().bar == 1 && clock.position().beatInBar == 0
          && clock.position().fraction == 0, "4/4 rollover incorrect");
    const double before = clock.position().beats; advance(0); advance(-10);
    check(clock.position().beats == before, "Invalid block advanced clock");
    clock.setBpm(140.04); check(std::abs(clock.bpm() - 140) < 1e-12, "Manual rounding incorrect");
    clock.setBpm(90.1); check(std::abs(clock.bpm() - 90.1) < 1e-12, "Decimal BPM lost");
    check(! clock.setBpm(std::numeric_limits<double>::quiet_NaN()), "NaN BPM accepted");
    check(! clock.setBpm(std::numeric_limits<double>::infinity()), "Infinite BPM accepted");
    clock.setBpm(0); check(clock.bpm() == 40, "Minimum BPM invalid");
    clock.setBpm(400); check(clock.bpm() == 300, "Maximum BPM invalid");
    MusicalClock small, large; small.prepare(44100); large.prepare(44100);
    small.setBpm(123.4); large.setBpm(123.4);
    processing = true;
    for (int i = 0; i < 100000; ++i) small.process(127);
    large.process(12700000);
    processing = false;
    check(std::abs(small.position().beats - large.position().beats) < 1e-9, "Clock depends on block partition");
    TapTempo tap;
    for (double bpm : { 40.0, 90.0, 120.0, 140.0, 300.0 })
    {
        tap.reset(); double estimate = 0;
        check(tap.tap(100) == 0, "First tap changed BPM");
        for (int i = 1; i < 8; ++i) estimate = tap.tap(100 + i * 60.0 / bpm);
        check(std::abs(estimate - bpm) < 1e-7, "Tap BPM calculation incorrect");
    }
    tap.reset(); double time = 0; tap.tap(time); double estimate = 0;
    for (double interval : { .50, .49, .51, .50, .90 }) { time += interval; estimate = tap.tap(time); }
    check(std::abs(estimate - 120) < 2, "Tap outlier destabilized tempo");
    check(tap.tap(time + 4) == 0, "Long pause included in tap estimate");
    check(std::abs(tap.tap(time + 4.5) - 120) < 1e-9, "Tap did not restart after pause");
    tap.reset(); tap.tap(0); check(tap.tap(.03) == 0, "Bounce accepted");
    check(tap.tap(.5) == 120, "Bounce shifted tap origin");
    tap.reset(); tap.tap(0);
    for (int i = 1; i < 20; ++i) check(tap.tap(i * .15) == 0, "Too-fast tapping aliased to legal BPM");
    check(tap.tap(-1) == 0 && tap.tap(std::numeric_limits<double>::infinity()) == 0, "Invalid timestamp accepted");
    check(tap.tap(0) == 0, "Backwards timestamp accepted");
}
}
void runMusicalClockTests()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    const auto old = _CrtSetAllocHook(allocationHook);
#endif
    try { clockAndTap(); }
    catch (...)
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetAllocHook(old);
#endif
        throw;
    }
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(old);
#endif
    check(allocations == 0, "Clock allocated in callback");
    std::cout << "PASS: TAP, manual BPM, 4/4 clock, phase/rate changes, drift and no clock allocations\n";
}
