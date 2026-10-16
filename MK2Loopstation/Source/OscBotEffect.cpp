#include "OscBotEffect.h"

static_assert(std::atomic<uint64_t>::is_always_lock_free, "OSC BOT requires lock-free x64 controls");

struct OscBotEffect::Tables
{
    static constexpr int size = 4096, levels = 10;
    // SAW, softened vintage saw, SQUARE, 25% RECT. DETUNE uses three SAWs.
    float data[4][levels][size + 1] {};
    Tables()
    {
        constexpr double pi = juce::MathConstants<double>::pi;
        // Incrementally accumulate harmonics; construction never runs in process().
        for (int n = 0; n < size; ++n)
        {
            double values[4] {};
            int level = 0;
            for (int h = 1; h <= 512; ++h)
            {
                const double angle = 2 * pi * h * n / size;
                const double saw = -2 * std::sin(angle) / (pi * h);
                values[0] += saw;
                values[1] += saw / std::pow(double(h), .25);
                if (h & 1) values[2] += 2 * saw;
                values[3] += 4 * std::sin(pi * h * .25) * std::cos(angle - pi * h * .25) / (pi * h);
                if (h == (1 << level))
                {
                    for (int w = 0; w < 4; ++w) data[w][level][n] = float(values[w]);
                    ++level;
                }
            }
        }
        for (auto& wave : data)
        {
            float peak = 0;
            for (const auto& level : wave)
                for (int n = 0; n < size; ++n) peak = std::max(peak, std::abs(level[n]));
            for (auto& level : wave)
            {
                for (int n = 0; n < size; ++n) level[n] /= peak;
                level[size] = level[0];
            }
        }
    }
};

OscBotEffect::OscBotEffect()
{
    static const Tables bank; // Static guard is reached only by construction.
    tables = &bank;
    prepare(48000);
}
void OscBotEffect::prepare(double sampleRate) noexcept
{
    rate = float(std::isfinite(sampleRate) && sampleRate >= 8000 ? sampleRate : 48000);
    detectorAttack = 1 - std::exp(-1.f / (.003f * rate));
    detectorRelease = 1 - std::exp(-1.f / (.03f * rate));
    release = 1 - std::exp(-1.f / (.05f * rate));
    for (auto* value : { &frequency, &toneRatio, &attackCoefficient, &modulation, &balance, &wet })
        value->reset(rate, .02);
    for (auto& value : waveMix) value.reset(rate, .02);
    applied = ~uint64_t(0); beginBlock();
    for (auto* value : { &frequency, &toneRatio, &attackCoefficient, &modulation, &balance })
        value->setCurrentAndTargetValue(value->getTargetValue());
    for (auto& value : waveMix) value.setCurrentAndTargetValue(value.getTargetValue());
    wet.setCurrentAndTargetValue(0);
    reset(); sequence.reset(); soundingNote=-1;
}
void OscBotEffect::reset() noexcept
{
    for (auto& channel : channels) channel = {};
    controlCountdown = 0;
}
void OscBotEffect::beginBlock(int enabledOverride) noexcept
{
    auto packed = parameters.load(std::memory_order_relaxed);
    if(enabledOverride>=0) packed=(packed & ~uint64_t(1)) | uint64_t(enabledOverride!=0);
    const auto p = OscBotParameters::unpack(packed);
    wet.setTargetValue(p.enabled ? 1.f : 0.f);
    if (packed == applied) return;
    applied = packed;
    manualNote=p.note;
    if (!sequence.synced()) frequency.setTargetValue(OscBotParameters::frequency(p.note));
    soundingNote=-1;
    const float colour = float(p.tone + 50) / 100;
    toneRatio.setTargetValue(1.1f + 12 * colour * colour);
    const float a = float(p.attack) / 100;
    attackCoefficient.setTargetValue(1 - std::exp(-1.f / ((.002f + .498f * a * a) * rate)));
    modulation.setTargetValue(float(p.modSens) / 50);
    balance.setTargetValue(float(p.balance) / 100);
    for (int w = 0; w < 5; ++w) waveMix[size_t(w)].setTargetValue(p.osc == w ? 1.f : 0.f);
}
float OscBotEffect::readWave(int wave, double phase, float hz) const noexcept
{
    // Both interpolated tables are strictly band-limited below 0.45*sampleRate.
    // Fade-in the denser table only once all its harmonics fit; continuous LOD.
    const float allowed = .45f * rate / hz;
    int level = 0, harmonics = 1;
    while (level < Tables::levels - 1 && float(harmonics * 2) <= allowed) { ++level; harmonics *= 2; }
    const float blend = level == 0 ? 1.f : std::clamp(allowed / harmonics - 1, 0.f, 1.f);
    const float position = float(phase * Tables::size);
    const int cell = std::min(int(position), Tables::size - 1);
    const float fraction = position - cell;
    const auto* high = tables->data[wave][level];
    const auto* low = tables->data[wave][level > 0 ? level - 1 : 0];
    const float a = low[cell] + fraction * (low[cell + 1] - low[cell]);
    const float b = high[cell] + fraction * (high[cell + 1] - high[cell]);
    // Notes above the supported band fade out rather than fold to a false pitch.
    const float audible = std::clamp((.49f * rate - hz) / (.04f * rate), 0.f, 1.f);
    return (a + blend * (b - a)) * audible;
}
void OscBotEffect::processFrame(float& left, float& right) noexcept
{
    const int note=sequence.nextNote(manualNote);
    if (note!=soundingNote)
    { frequency.setTargetValue(OscBotParameters::frequency(note)); soundingNote=note; }
    if (!std::isfinite(left)) left = 0;
    if (!std::isfinite(right)) right = 0;
    if (!wet.isSmoothing() && wet.getCurrentValue() == 0) return;
    const float hz = frequency.getNextValue(), colour = toneRatio.getNextValue();
    const float attack = attackCoefficient.getNextValue(), mod = modulation.getNextValue();
    const float mix = balance.getNextValue(), enabled = wet.getNextValue();
    float weights[5];
    for (int w = 0; w < 5; ++w) weights[w] = waveMix[size_t(w)].getNextValue();
    const bool update = --controlCountdown <= 0;
    if (update) controlCountdown = 16;
    float* output[] { &left, &right };
    constexpr float detune[] { 1.f, .995965f, 1.0040516f }; // +/-7 cents, fixed local approximation.
    for (int c = 0; c < 2; ++c)
    {
        auto& s = channels[c];
        const float dry = *output[c];
        const float magnitude = std::min(1.f, std::abs(dry));
        s.detector += (magnitude > s.detector ? detectorAttack : detectorRelease) * (magnitude - s.detector);
        const float target = std::clamp(2 * (s.detector - .0002f), 0.f, .6f);
        s.amplitude += (target > s.amplitude ? attack : release) * (target - s.amplitude);
        if (update)
        {
            // Manual specifies input-controlled modulation, not its destination
            // or curve. Local approximation: signed envelope -> tone cutoff.
            const float cutoff = std::clamp(hz * colour * std::exp2(2 * mod * std::min(1.f, s.detector * 4)), 20.f, .45f * rate);
            const float coefficient = 1 - std::exp(-juce::MathConstants<float>::twoPi * cutoff / rate);
            s.coefficientStep = (coefficient - s.coefficient) / 16;
        }
        s.coefficient += s.coefficientStep;
        float value = weights[0] * readWave(0, s.phase[0], hz)
            + weights[1] * readWave(1, s.phase[0], hz)
            + weights[3] * readWave(2, s.phase[0], hz)
            + weights[4] * readWave(3, s.phase[0], hz);
        if (weights[2] > 0)
            for (int i = 0; i < 3; ++i) value += weights[2] * readWave(0, s.phase[i], hz * detune[i]) / 3;
        for (int i = 0; i < 3; ++i)
        {
            s.phase[i] += double(std::min(hz * detune[i], .49f * rate)) / rate;
            s.phase[i] -= int(s.phase[i]);
        }
        // Filter the oscillator before the envelope: no residual tone in silence.
        s.low1 += s.coefficient * (value - s.low1);
        s.low2 += s.coefficient * (s.low1 - s.low2);
        const float synth = s.low2 * s.amplitude;
        // Convex blend: no extra dry path or gain boost. BALANCE=0 is exact dry.
        *output[c] = dry + enabled * mix * (synth - dry);
    }
    if (enabled == 0) reset();
}
