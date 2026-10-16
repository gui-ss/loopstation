#include "AudioSystem.h"
#include "MainComponent.h"
#if defined(_MSC_VER) && defined(_DEBUG)
 #include <crtdbg.h>
#endif
#include <iostream>
#include <stdexcept>
#include <thread>
#include <array>
#include <cmath>
#include <limits>

void runLoopTrackTests();
void runRobotEffectTests();
void runMusicalClockTests();
void runTempoPanelTests();

struct AudioSystemTestAccess
{
    static MusicalClock::SyncBlock syncBlock(const AudioSystem& audio) { return audio.clock.syncBlock(); }
    static void serviceCommands(AudioSystem& audio, const float* const* inputs, int numInputs,
                                float* const* outputs, int numOutputs)
    { audio.audioDeviceIOCallbackWithContext(inputs, numInputs, outputs, numOutputs, 0, {}); }
};

namespace
{
void require(bool value, const char* message) { if (! value) throw std::runtime_error(message); }

struct Hardware
{
    juce::StringArray inputs { "Mic 1", "Mic 2" }, outputs { "Output 1", "Output 2" };
    bool failOpen = false;
    double sampleRate = 48000;
    int openDevices = 0;
    int inputChannels = 1, outputChannels = 2, xruns = 0;
    bool rejectSmallBuffer = false;
};

class Device final : public juce::AudioIODevice
{
public:
    Device(Hardware& h, juce::String in, juce::String out)
        : AudioIODevice(out.isNotEmpty() ? out : in, "Test Audio"), hardware(h), input(in), output(out) {}
    ~Device() override { close(); }
    juce::StringArray getOutputChannelNames() override { return names(output, hardware.outputChannels); }
    juce::StringArray getInputChannelNames() override { return names(input, hardware.inputChannels); }
    juce::Array<double> getAvailableSampleRates() override { return { hardware.sampleRate }; }
    juce::Array<int> getAvailableBufferSizes() override { return { 128, 256, 512 }; }
    int getDefaultBufferSize() override { return 512; }
    juce::String open(const juce::BigInteger& in, const juce::BigInteger& out, double, int size) override
    {
        close();
        if (hardware.failOpen) return "Device unavailable (test)";
        if (hardware.rejectSmallBuffer && size < 512) return "Unsupported buffer (test)";
        inputs = in; outputs = out;
        inputs.setRange(hardware.inputChannels, 32, false);
        outputs.setRange(hardware.outputChannels, 32, false);
        bufferSize = size;
        opened = true; ++hardware.openDevices;
        return {};
    }
    void close() override { stop(); if (opened) --hardware.openDevices; opened = false; }
    bool isOpen() override { return opened; }
    void start(juce::AudioIODeviceCallback* c) override { callback = c; callback->audioDeviceAboutToStart(this); }
    void stop() override { if (callback != nullptr) callback->audioDeviceStopped(); callback = nullptr; }
    bool isPlaying() override { return callback != nullptr; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return bufferSize; }
    double getCurrentSampleRate() override { return hardware.sampleRate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return outputs; }
    juce::BigInteger getActiveInputChannels() const override { return inputs; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }
    int getXRunCount() const noexcept override { return hardware.xruns; }
    Hardware& hardware;
    juce::String input, output;
    juce::AudioIODeviceCallback* callback = nullptr;
private:
    static juce::StringArray names(const juce::String& name, int count)
    {
        juce::StringArray result;
        if (name.isNotEmpty()) for (int i = 0; i < count; ++i) result.add(juce::String(i + 1));
        return result;
    }
    int bufferSize = 512;
    juce::BigInteger inputs, outputs;
    bool opened = false;
};

class DeviceType final : public juce::AudioIODeviceType
{
public:
    explicit DeviceType(Hardware& h) : AudioIODeviceType("Test Audio"), hardware(h) {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool input) const override { return input ? hardware.inputs : hardware.outputs; }
    int getDefaultDeviceIndex(bool input) const override { return getDeviceNames(input).isEmpty() ? -1 : 0; }
    int getIndexOfDevice(juce::AudioIODevice* d, bool input) const override
    {
        auto* device = static_cast<Device*>(d);
        return getDeviceNames(input).indexOf(input ? device->input : device->output);
    }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice(const juce::String& output, const juce::String& input) override
    { return new Device(hardware, input, output); }
    void changed() { callDeviceChangeListeners(); }
private:
    Hardware& hardware;
};

class Manager final : public juce::AudioDeviceManager
{
public:
    explicit Manager(Hardware& h) : hardware(h) {}
    DeviceType* type = nullptr;
private:
    void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types) override
    { type = new DeviceType(hardware); types.add(type); }
    Hardware& hardware;
};

void tick() { juce::Thread::sleep(300); juce::Timer::callPendingTimersSynchronously(); }
void contains(const AudioSystem& audio, const juce::String& text)
{ require(audio.statusText().contains(text), audio.statusText().toRawUTF8()); }

void testDevices()
{
    Hardware hardware;
    {
        auto manager = std::make_unique<Manager>(hardware);
        auto* backend = manager.get();
        AudioSystem audio(std::move(manager), 240000);
        audio.initialise();
        contains(audio, "Microfone conectado"); contains(audio, L"Saída conectada");
        require(audio.deviceNames(true).size() == 2 && audio.deviceNames(false).size() == 2, "Enumeration failed");
        audio.selectDevices("Mic 2", "Output 2");
        auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
        require(device->input == "Mic 2" && device->output == "Output 2", "Independent selection failed");

        // Exercise the real manager -> production callback with nonzero mic data.
        float input[128], left[128], right[128];
        std::fill_n(input, 128, 0.75f); std::fill_n(left, 128, 1.0f); std::fill_n(right, 128, -1.0f);
        const float* inputs[] { input }; float* outputs[] { left, right };
        device->callback->audioDeviceIOCallbackWithContext(inputs, 1, outputs, 2, 128, {});
        for (int i = 0; i < 128; ++i) require(left[i] == 0 && right[i] == 0 && input[i] == 0.75f, "Microphone monitoring or non-silent output");

        audio.setMonitoringEnabled(true);
        require(audio.isMonitoringEnabled(), "Monitor could not be enabled with connected devices");

        hardware.inputs.removeString("Mic 2"); backend->type->changed(); tick();
        contains(audio, "Microfone desconectado");
        require(! audio.isMonitoringEnabled(), "Unplug left monitoring enabled");
        require(audio.selectedInput() == "Mic 2" && backend->getCurrentAudioDevice() == nullptr, "Missing mic silently replaced");
        hardware.inputs.add("Mic 2"); backend->type->changed(); tick();
        audio.reconnect(); contains(audio, "Microfone conectado");
        require(! audio.isMonitoringEnabled(), "Reconnect unexpectedly enabled monitoring");

        hardware.outputs.removeString("Output 2"); backend->type->changed(); tick();
        contains(audio, L"Saída desconectada");
        require(audio.selectedOutput() == "Output 2" && backend->getCurrentAudioDevice() == nullptr, "Missing output silently replaced");
        hardware.outputs.add("Output 2"); audio.reconnect();
        contains(audio, L"Saída conectada");

        hardware.failOpen = true; audio.reconnect();
        contains(audio, "Device unavailable (test)");
        require(audio.selectedInput() == "Mic 2", "Open failure lost selection");
        hardware.failOpen = false; audio.reconnect(); contains(audio, "Microfone conectado");
        device = static_cast<Device*>(backend->getCurrentAudioDevice());
        audio.setMonitoringEnabled(true);
        std::thread failure([device] { device->callback->audioDeviceError("runtime failure"); });
        failure.join(); tick(); contains(audio, "interrompeu");
        require(! audio.isMonitoringEnabled(), "Driver error left monitoring enabled");
        require(backend->getCurrentAudioDevice() == nullptr, "Runtime failure did not close device");
        audio.reconnect(); contains(audio, "Microfone conectado");

        audio.selectDevices({}, "Output 1"); contains(audio, L"Microfone não conectado"); contains(audio, L"Saída conectada");
        audio.setMonitoringEnabled(true); require(! audio.isMonitoringEnabled(), "Monitoring enabled without input");
        audio.selectDevices("Mic 1", {}); contains(audio, "Microfone conectado"); contains(audio, L"Saída não conectada");
        audio.selectDevices({}, {}); contains(audio, "nenhum dispositivo selecionado");
        require(backend->getCurrentAudioDevice() == nullptr, "None failed to close device");
        audio.selectDevices("Missing", "Output 1"); contains(audio, L"indisponível");
        audio.selectDevices("Mic 1", "Output 1"); contains(audio, "Microfone conectado");
    }
    require(hardware.openDevices == 0, "Shutdown leaked device");

    for (int scenario = 0; scenario < 3; ++scenario)
    {
        Hardware empty;
        empty.inputs.clear();
        if (scenario == 1) empty.outputs.clear();
        if (scenario == 2) { empty.inputs.add("Denied mic"); empty.failOpen = true; }
        AudioSystem audio(std::make_unique<Manager>(empty), 240000);
        audio.initialise();
        contains(audio, scenario == 0 ? L"Microfone não conectado" : scenario == 1 ? L"Nenhum dispositivo" : L"Device unavailable");
    }
}

void testMonitoring()
{
    for (int inputCount : { 1, 2 }) for (int outputCount : { 1, 2 })
    {
        Hardware hardware;
        hardware.inputChannels = inputCount; hardware.outputChannels = outputCount;
        auto manager = std::make_unique<Manager>(hardware); auto* backend = manager.get();
        AudioSystem audio(std::move(manager), 240000); audio.initialise();
        require(! audio.isMonitoringEnabled() && std::abs(audio.monitorVolume() - 0.1f) < 1.0e-6f, "Unsafe monitor defaults");
        auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
        require(device->getActiveInputChannels().countNumberOfSetBits() == inputCount, "Input channel negotiation");
        require(device->getActiveOutputChannels().countNumberOfSetBits() == outputCount, "Output channel negotiation");
        require(device->getCurrentBufferSizeSamples() == 256, "Conservative low-latency buffer not chosen");
        std::array<float, 2048> l, r, a, b;
        l.fill(0.8f); r.fill(0.2f); a.fill(99.0f); b.fill(99.0f);
        const float* inputs[] { l.data(), r.data() }; float* outputs[] { a.data(), b.data() };
        const auto render = [&] { device->callback->audioDeviceIOCallbackWithContext(inputs, inputCount, outputs, outputCount, 2048, {}); };
        render(); for (auto sample : a) require(sample == 0, "Disabled monitor leaked signal");
        audio.setMonitoringEnabled(true); render();
        const float expectedLeft = inputCount == 2 && outputCount == 1 ? 0.05f : 0.08f;
        const float expectedRight = inputCount == 2 ? 0.02f : 0.08f;
        require(a[0] > 0 && a[0] < expectedLeft && std::abs(a.back() - expectedLeft) < 1.0e-6f, "Enable ramp or channel routing failed");
        if (outputCount == 2) require(std::abs(b.back() - expectedRight) < 1.0e-6f, "Right channel routing failed");
        audio.setMonitorVolume(0.5f); render();
        require(a[0] > expectedLeft && a[0] < expectedLeft * 5 && std::abs(a.back() - expectedLeft * 5) < 1.0e-6f, "Volume ramp failed");
        audio.setMonitorVolume(0); render(); require(a.back() == 0, "Zero volume failed");
        audio.setMonitorVolume(1); render();
        audio.setMonitoringEnabled(false); render();
        for (auto sample : a) require(sample == 0, "Disable did not silence immediately");
        audio.setMonitoringEnabled(true);
        device->callback->audioDeviceIOCallbackWithContext(nullptr, 0, outputs, outputCount, 2048, {});
        for (auto sample : a) require(sample == 0, "Missing input leaked stale output");
        inputs[0] = nullptr; inputs[1] = nullptr; render();
        for (auto sample : a) require(sample == 0, "Null input leaked stale output");
        // Null/extra output channels are tolerated and unused channels are cleared.
        float* extraOutputs[] { nullptr, b.data(), a.data() };
        device->callback->audioDeviceIOCallbackWithContext(inputs, inputCount, extraOutputs, 3, 2048, {});
        for (auto sample : a) require(sample == 0, "Unused channel was not silenced");
        device->callback->audioDeviceIOCallbackWithContext(inputs, inputCount, nullptr, 0, 2048, {});
        audio.setMonitorVolume(2); require(audio.monitorVolume() == 1, "Gain exceeded unity");
        audio.setMonitorVolume(-1); require(audio.monitorVolume() == 0, "Negative gain accepted");
        audio.setMonitorVolume(std::numeric_limits<float>::quiet_NaN()); require(audio.monitorVolume() == 0, "Invalid gain accepted");
        audio.selectDevices("Mic 2", "Output 2"); require(! audio.isMonitoringEnabled(), "Device selection retained live monitoring");
        audio.setMonitoringEnabled(true);
        hardware.xruns += 3; tick(); tick();
        require(backend->getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 512, "Xrun fallback failed");
        require(! audio.isMonitoringEnabled(), "Stability fallback re-enabled monitor");
        contains(audio, "estabilidade");
    }
    Hardware hardware; hardware.rejectSmallBuffer = true;
    auto manager = std::make_unique<Manager>(hardware); auto* backend = manager.get();
    AudioSystem audio(std::move(manager), 240000); audio.initialise();
    require(backend->getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 512, "Rejected buffer did not fall back");
    contains(audio, "Microfone conectado");
}

void testDistortionSignalPath()
{
    Hardware hardware;
    auto manager = std::make_unique<Manager>(hardware); auto* backend = manager.get();
    AudioSystem audio(std::move(manager), 240000); audio.initialise(); audio.setTrack1Volume(1);
    std::array<float, 4096> input {}, left {}, right {}, dry {};
    for (int i = 0; i < 4096; ++i) input[i] = static_cast<float>(0.1 * std::sin(juce::MathConstants<double>::twoPi * i / 64.0));
    const float* inputs[] { input.data() }; float* outputs[] { left.data(), right.data() };
    const auto render = [&]
    {
        auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
        device->callback->audioDeviceIOCallbackWithContext(inputs, 1, outputs, 2, 4096, {});
    };
    const auto harmonic = [&](int number)
    {
        double real = 0, imaginary = 0;
        for (int i = 256; i < 3840; ++i)
        {
            const double angle = juce::MathConstants<double>::twoPi * number * i / 64.0;
            real += left[i] * std::cos(angle); imaginary += left[i] * std::sin(angle);
        }
        return std::sqrt(real * real + imaginary * imaginary);
    };
    audio.commandTrack1(LoopTrack::Command::primary); render();
    audio.commandTrack1(LoopTrack::Command::primary); render(); render(); dry = left;
    const double dryThird = harmonic(3) / harmonic(1);
    TrackFxParameters fx; fx.distortionEnabled = true;
    audio.setTrack1Effects(fx); render(); render();
    const double wetThird = harmonic(3) / harmonic(1);
    require(dryThird < 0.001 && wetThird > 0.03, "DISTORTION did not add harmonics through the actual AudioSystem callback");
    for (int i = 256; i < 3840; ++i)
        require(std::abs(left[i] - right[i]) < 1.0e-6f, "DISTORTION mono/stereo output mismatch");
    fx.distortionEnabled = false; audio.setTrack1Effects(fx); render(); render();
    for (int i = 256; i < 3840; ++i) require(left[i] == dry[i], "DISTORTION bypass changed original RAM/playback");
    std::cout << "DISTORTION callback sine 750 Hz, peak 0.1: H3/H1 dry=" << dryThird
              << ", wet=" << wetThird << "; exact bypass restored\n";
}

void testTempoKeepsFreeLoops()
{
    Hardware hardware;
    auto manager = std::make_unique<Manager>(hardware); auto* backend = manager.get();
    AudioSystem audio(std::move(manager), 240000); audio.initialise(); audio.setTrack1Volume(1);
    std::array<float, 4093> input {}, left {}, right {}, baseline {};
    for (int i = 0; i < 4093; ++i) input[i] = 0.1f * std::sin(i * 0.07f);
    const float* ins[] { input.data() }; float* outs[] { left.data(), right.data() };
    const auto render = [&](int frames)
    {
        auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
        device->callback->audioDeviceIOCallbackWithContext(ins, 1, outs, 2, frames, {});
    };
    audio.setBpm(90); render(480);
    require(std::abs(audio.musicalPosition().beats - .015) < 1e-12, "Clock did not run without tracks");
    audio.commandTrack1(LoopTrack::Command::primary); render(1234);
    audio.setBpm(140); render(2859);
    audio.commandTrack1(LoopTrack::Command::primary); render(4093); render(4093); baseline = left;
    require(audio.track1Snapshot().frames == 4093, "Tempo quantized free recording");
    for (double tempo : { 40.0, 90.0, 120.0, 140.0, 300.0 })
    {
        const double before = audio.musicalPosition().beats;
        audio.setBpm(tempo); require(audio.bpm() == tempo, "Manual BPM not immediately available");
        render(4093);
        require(std::abs(audio.musicalPosition().beats - before - 4093 * tempo / (60 * 48000)) < 1e-10,
                "Callback clock used wrong BPM/sample count");
        for (int i = 0; i < 4093; ++i) require(left[i] == baseline[i], "BPM altered loop speed/pitch/content");
    }
    render(1000);
    require(std::abs(audio.track1Snapshot().progress - 1000.0 / 4093) < 1e-7, "Audio position snapshot incorrect");
    audio.setBpm(90.1);
    require(std::abs(audio.track1Snapshot().progress - 1000.0 / 4093) < 1e-7, "BPM moved loop cursor");
    audio.commandTrack1(LoopTrack::Command::stop); render(256);
    const double stoppedBeat = audio.musicalPosition().beats; render(480);
    require(audio.musicalPosition().beats > stoppedBeat, "Track STOP stopped clock");
    audio.selectDevices({}, {});
    const double offlineBeat = audio.musicalPosition().beats;
    audio.setBpm(120); require(! audio.clockRunning() && audio.musicalPosition().beats == offlineBeat, "Offline tempo advanced clock");
    audio.selectDevices("Mic 1", "Output 1"); render(480);
    require(std::abs(audio.musicalPosition().beats - offlineBeat - .02) < 1e-10, "Reconnect reset phase");
    std::cout << "PASS: unquantized 4093-frame loop, bit-exact playback at 40/90/120/140/300 BPM\n";
}

void testQuantizeIntegration()
{
    Hardware hardware;
    auto manager = std::make_unique<Manager>(hardware); auto* backend = manager.get();
    AudioSystem audio(std::move(manager), 240000); audio.initialise(); audio.setTrack1Volume(1);
    std::array<float, 1024> input {}, left {}, right {}; input.fill(.2f);
    const float* ins[] { input.data() }; float* outs[] { left.data(), right.data() };
    const auto render = [&](int frames)
    {
        do
        {
            const int n = std::min(1024, frames);
            auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
            device->callback->audioDeviceIOCallbackWithContext(ins, 1, outs, 2, n, {});
            frames -= n;
        } while (frames > 0);
    };
    require(! audio.quantizeEnabled(), "AudioSystem quantize default is not OFF");
    audio.setBpm(120); audio.setQuantizeEnabled(true);
    render(137); // Start deliberately outside the global bar grid.
    audio.commandTrack1(LoopTrack::Command::primary); render(45000);
    audio.commandTrack1(LoopTrack::Command::primary); render(0);
    require(audio.track1Snapshot().awaitingBoundary, "AudioSystem did not route quantize/grid to track");
    render(51000);
    require(audio.track1Snapshot().state == LoopTrack::State::playing && audio.track1Snapshot().frames == 96000,
            "AudioSystem quantized against global phase instead of recording start");
    require(std::abs(audio.musicalPosition().beats - (96137.0 / 24000)) < 1e-10, "Quantize reset global clock");
    render(1024); require(std::abs(left[1000] - .2f) < 1e-6f, "Quantized loop not audible");
    audio.commandTrack1(LoopTrack::Command::stop); render(0);
    audio.commandTrack1(LoopTrack::Command::clear); render(0);
    audio.commandTrack1(LoopTrack::Command::primary); render(987);
    audio.commandTrack1(LoopTrack::Command::primary); render(0);
    audio.selectDevices("Mic 2", "Output 2");
    require(audio.track1Snapshot().state == LoopTrack::State::stopped && audio.track1Snapshot().frames == 987
            && ! audio.track1Snapshot().awaitingBoundary, "Device switch retained pending wait or erased audio");
    audio.commandTrack1(LoopTrack::Command::primary); render(1024);
    require(audio.track1Snapshot().state == LoopTrack::State::playing && audio.track1Snapshot().frames == 987,
            "Partial phrase after device change was requantized");
    std::cout << "PASS: QUANTIZE 1/2 bars at 120 BPM (96000/192000 frames), free OFF, mid-block PLAY, STOP/device cancellation and loop/overdub/Undo preservation\n";
}

void testLoopIntegration()
{
    Hardware hardware;
    auto manager = std::make_unique<Manager>(hardware); auto* backend = manager.get();
    AudioSystem audio(std::move(manager), 240000); audio.initialise(); audio.setTrack1Volume(0.5f);
    std::array<float, 4096> input, left, right;
    input.fill(0.4f);
    const float* inputs[] { input.data() }; float* outputs[] { left.data(), right.data() };
    const auto render = [&](int channels = 1)
    {
        auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
        device->callback->audioDeviceIOCallbackWithContext(inputs, channels, outputs, 2, 4096, {});
    };
    audio.commandTrack1(LoopTrack::Command::primary); render();
    require(audio.track1Snapshot().state == LoopTrack::State::recording, "AudioSystem did not record");
    for (float sample : left) require(sample == 0, "Recording enabled monitoring implicitly");
    input.fill(0.1f);
    audio.commandTrack1(LoopTrack::Command::primary); render();
    require(std::abs(left[1000] - 0.2f) < 1.0e-5f, "Loop not audible with monitoring off");
    audio.setMonitoringEnabled(true); render();
    require(std::abs(left[1000] - 0.21f) < 1.0e-5f, "Monitor and loop not mixed independently");
    audio.setTrack1Effects({ true, 10, 100 }); render(); render();
    require(left[1000] > 0.015f && left[1000] < 0.18f, "TRACK FX A not inserted in loop path");
    TrackFxParameters both; both.enabled = true; both.cutoff = 10; both.resonance = 100; both.distortionEnabled = true;
    audio.setTrack1Effects(both); render(); render();
    require(std::isfinite(left[1000]) && left[1000] > 0.01f && left[1000] < 0.3f, "Combined effects invalid output");
    audio.setTrack1Volume(0); render();
    require(std::abs(left[1000] - 0.01f) < 1.0e-5f, "Track fader changed direct monitor");
    audio.commandTrack1(LoopTrack::Command::primary); render(); render();
    require(audio.track1Snapshot().state == LoopTrack::State::overdubbing, "Callback did not enter overdub");
    require(! audio.commandTrack1(LoopTrack::Command::undo), "Callback accepted undo during take");
    audio.commandTrack1(LoopTrack::Command::primary); render();
    audio.setTrack1Effects({ false, 10, 100 }); render();
    audio.setMonitoringEnabled(false); audio.setTrack1Volume(0.5f); render();
    require(std::abs(left[1000] - 0.3f) < 1.0e-5f,
            "Overdub recorded monitor/FX/output, or fader affected capture instead of just playback");
    require(audio.commandTrack1(LoopTrack::Command::undo), "Callback rejected completed take undo"); render();
    require(std::abs(left[1000] - 0.2f) < 1.0e-5f, "Callback undo failed to restore initial recording");
    require(! audio.commandTrack1(LoopTrack::Command::undo), "Callback undo deleted initial recording");
    audio.commandTrack1(LoopTrack::Command::stop); render();
    require(left[1000] == 0 && audio.track1Snapshot().frames == 4096, "STOP failed to preserve recorded loop");
    audio.selectDevices({}, "Output 1");
    require(audio.track1Snapshot().frames == 4096 && audio.track1Snapshot().state == LoopTrack::State::stopped,
            "Device reconfiguration lost or restarted loop");
    audio.commandTrack1(LoopTrack::Command::primary); render(0);
    require(std::abs(left[1000] - 0.2f) < 1.0e-5f, "Stored loop needs an input to play");
    auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
    device->callback->audioDeviceError("test error"); render(0);
    require(left[1000] == 0 && audio.track1Snapshot().state == LoopTrack::State::stopped, "Driver error left loop playing");
    tick();
    audio.commandTrack1(LoopTrack::Command::clear);
    require(audio.track1Snapshot().state == LoopTrack::State::empty, "Offline guarded clear failed");
}

thread_local bool fiveTrackCallback = false;
thread_local int fiveTrackAllocations = 0;
#if defined(_MSC_VER) && defined(_DEBUG)
int fiveTrackAllocationHook(int type, void*, size_t, int, long, const unsigned char*, int)
{
    if (fiveTrackCallback && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++fiveTrackAllocations;
    return 1;
}
#endif

struct FiveTracks
{
    Hardware hardware;
    Manager* backend = nullptr;
    AudioSystem audio;
    std::array<float, 4096> inputL {}, inputR {}, left {}, right {};
    std::unique_ptr<juce::AudioDeviceManager> makeManager(double rate)
    {
        hardware.inputChannels = 2;
        hardware.sampleRate = rate;
        auto manager = std::make_unique<Manager>(hardware);
        backend = manager.get();
        return manager;
    }
    explicit FiveTracks(double rate = 48000, int capacity = 240000) : audio(makeManager(rate), capacity)
    {
        audio.initialise();
        // AudioDeviceManager itself lazily sizes its mixing scratch buffer on
        // first delivery. Warm that framework buffer at the largest test block
        // before measuring allocations across the five-track engine/transitions.
        const float* inputs[] { inputL.data(), inputR.data() };
        float* outputs[] { left.data(), right.data() };
        auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
        device->callback->audioDeviceIOCallbackWithContext(inputs, 2, outputs, 2, 4096, {});
        for (int i = 0; i < 5; ++i) audio.setTrackVolume(i, 1);
    }
    void mic(float value) { inputL.fill(value); inputR.fill(-.5f * value); }
    void render(int frames, int inputChannels = 2, int outputChannels = 2)
    {
        const float* inputs[] { inputL.data(), inputR.data() };
        float* outputs[] { left.data(), right.data() };
        do
        {
            const int n = std::min(4096, frames);
            auto* device = static_cast<Device*>(backend->getCurrentAudioDevice());
            fiveTrackCallback = true;
            // Zero frames service a boundary command without advancing the test signal.
            // JUCE's device wrapper asserts on zero blocks (and allocates its debug log).
            if (n == 0) AudioSystemTestAccess::serviceCommands(audio, inputs, inputChannels, outputs, outputChannels);
            else device->callback->audioDeviceIOCallbackWithContext(inputs, inputChannels, outputs, outputChannels, n, {});
            fiveTrackCallback = false;
            frames -= n;
        } while (frames > 0);
    }
    void command(int index, LoopTrack::Command value)
    {
        require(audio.commandTrack(index, value), "Per-track command rejected");
        render(0);
    }
    void stopAll()
    {
        for (int i = 0; i < 5; ++i) command(i, LoopTrack::Command::stop);
        render(512);
    }
    void playAll()
    { for (int i = 0; i < 5; ++i) command(i, LoopTrack::Command::primary); }
    void near(float expected, const char* message)
    {
        require(std::abs(left[2000] - expected) < 1.e-5f, message);
        require(std::abs(right[2000] + .5f * expected) < 1.e-5f, "Stereo routing/independence failed");
    }
};

void testFiveTracks()
{
    using Command = LoopTrack::Command;
    using State = LoopTrack::State;
    FiveTracks f;
    require(! f.audio.quantizeEnabled() && f.audio.storageReady(), "Unsafe five-track defaults");
    require(! f.audio.commandTrack(-1, Command::primary) && ! f.audio.commandTrack(5, Command::clear),
            "Invalid index could target another track");
    for (int i = 0; i < 5; ++i)
    {
        f.mic(.01f * (i + 1));
        f.command(i, Command::primary); f.render(4096 + i * 257);
        require(f.audio.trackSnapshot(i).state == State::recording, "Track failed to record independently");
        for (int j = 0; j < i; ++j)
            require(f.audio.trackSnapshot(j).state == State::playing, "Recording stopped a different track");
        f.command(i, Command::primary);
        require(f.audio.trackSnapshot(i).frames == 4096 + i * 257, "Q OFF changed free duration");
    }
    // Isolate and resume every track. Other playback must never enter its RAM.
    f.stopAll(); f.mic(0);
    for (int i = 0; i < 5; ++i)
    {
        f.command(i, Command::primary); f.render(4096);
        f.near(.01f * (i + 1), "Initial capture contained another track/output");
        f.command(i, Command::stop); f.render(512);
    }
    f.playAll(); f.render(4096); f.near(.15f, "Five playback signals not summed");
    for (int i = 0; i < 5; ++i)
    {
        f.stopAll(); f.audio.setTrackVolume(i, .25f); f.playAll(); f.render(4096);
        f.near(.15f - .0075f * (i + 1), "Fader affected wrong track or mix");
        f.audio.setTrackVolume(i, 1);
    }
    // One multi-revolution take on EACH track while the others play, FX+monitor on.
    TrackFxParameters fx; fx.enabled = true; fx.distortionEnabled = true;
    f.audio.setTrack1Effects(fx); f.audio.setMonitoringEnabled(true);
    for (int i = 0; i < 5; ++i)
    {
        f.command(i, Command::stop); f.render(512);
        f.command(i, Command::primary); f.command(i, Command::primary);
        f.mic(.02f); f.render(3 * (4096 + i * 257));
        require(! f.audio.commandTrack(i, Command::undo), "Undo accepted during active overdub");
        f.command(i, Command::primary);
        require(f.audio.trackSnapshot(i).canUndo && f.audio.trackSnapshot(i).frames == 4096 + i * 257,
                "Overdub changed duration or failed to preserve Undo");
    }
    f.audio.setTrack1Effects({}); f.audio.setMonitoringEnabled(false); f.mic(0); f.stopAll();
    for (int i = 0; i < 5; ++i)
    {
        f.command(i, Command::primary); f.render(4096);
        f.near(.01f * (i + 1) + .06f, "Overdub captured monitor/FX/other tracks, or lost multiple laps");
        f.audio.selectTrack(i);
        require(f.audio.undoSelectedTrack(), "Selected-track Undo rejected");
        // Selection changes after enqueue must NOT redirect that pending command.
        f.audio.selectTrack((i + 1) % 5); f.render(0);
        f.command(i, Command::stop); f.render(512);
        f.command(i, Command::primary); f.render(4096);
        f.near(.01f * (i + 1), "Undo did not restore only selected track");
        for (int j = i + 1; j < 5; ++j)
            require(f.audio.trackSnapshot(j).canUndo, "Undo consumed a different track's history");
        require(! f.audio.commandTrack(i, Command::undo), "Repeated Undo erased original recording");
        require(! f.audio.commandTrack(i, Command::clear), "Playing track allowed destructive clear");
        f.command(i, Command::stop); f.render(512);
    }
    // Effects stay on track 1; enabling both cannot alter 2-5 or their stored audio.
    f.audio.setTrack1Effects(fx);
    for (int i = 1; i < 5; ++i)
    {
        f.command(i, Command::primary); f.render(4096);
        f.near(.01f * (i + 1), "Track-1 effects leaked to another track");
        f.command(i, Command::stop); f.render(512);
    }
    f.audio.setTrack1Effects({});
    f.playAll(); f.render(4096);
    const double before = f.audio.musicalPosition().beats;
    f.render(480);
    require(std::abs(f.audio.musicalPosition().beats - before - .02) < 1e-10,
            "Five tracks advanced the shared clock more than once");
    f.audio.selectDevices("Mic 2", "Output 2");
    for (int i = 0; i < 5; ++i)
    {
        require(f.audio.trackSnapshot(i).state == State::stopped
                && f.audio.trackSnapshot(i).frames == 4096 + i * 257, "Device change lost a track");
        f.command(i, Command::clear);
        require(f.audio.trackSnapshot(i).state == State::empty, "Per-track clear failed");
        for (int j = i + 1; j < 5; ++j)
            require(f.audio.trackSnapshot(j).hasPhrase(), "Clear erased a different track");
    }
    std::cout << "PASS: five independent captures, concurrent playback, faders, multi-lap overdubs, targeted Undo, STOP/resume/clear, FX isolation, device preservation\n";
}

void testFiveQuantizedTracksAndMix()
{
    using Command = LoopTrack::Command;
    using State = LoopTrack::State;
    FiveTracks f; f.audio.setQuantizeEnabled(true); f.mic(.3f);
    // Staggered starts prove boundaries are relative to EACH capture, not a global reset.
    for (int i = 0; i < 5; ++i) { f.command(i, Command::primary); f.render(777); }
    std::array<int, 5> requestedAt {};
    for (int i = 0; i < 5; ++i)
    {
        requestedAt[static_cast<size_t>(i)] = f.audio.trackSnapshot(i).frames;
        f.command(i, Command::primary);
        require(f.audio.trackSnapshot(i).awaitingBoundary, "Per-track WAIT absent");
    }
    int advanced = 0;
    for (int i = 0; i < 5; ++i)
    {
        const int boundary = 96000 - requestedAt[static_cast<size_t>(i)];
        f.render(boundary - advanced - 1);
        require(f.audio.trackSnapshot(i).state == State::recording, "Track ended before its boundary");
        f.render(1); advanced = boundary;
        require(f.audio.trackSnapshot(i).state == State::playing && f.audio.trackSnapshot(i).frames == 96000,
                "Independent one-bar capture not sample-exact");
    }
    f.render(4096);
    for (size_t i = 0; i < f.left.size(); ++i)
        require(std::isfinite(f.left[i]) && f.left[i] < .98f && f.left[i] > .8f
                && std::abs(f.right[i] + .5f * f.left[i]) < 1e-6f,
                "Loud five-track mix clipped or changed stereo image");
    // Record two bars on every track; another simultaneous capture can be stopped safely.
    f.stopAll();
    for (int i = 0; i < 5; ++i) { f.command(i, Command::clear); f.command(i, Command::primary); }
    f.render(96001);
    for (int i = 0; i < 5; ++i) f.command(i, Command::primary);
    f.command(2, Command::stop);
    f.render(95999);
    for (int i = 0; i < 5; ++i)
        require(f.audio.trackSnapshot(i).frames == (i == 2 ? 96001 : 192000)
                && f.audio.trackSnapshot(i).state == (i == 2 ? State::stopped : State::playing),
                "Two-bar quantize or per-track STOP cancellation failed");
    f.command(2, Command::primary); f.render(4096);
    require(f.audio.trackSnapshot(2).frames == 96001, "Resume requantized partial capture");
    // Mono output and direct monitoring share the protected bus; no monitor enters RAM.
    f.audio.setMonitorVolume(1); f.audio.setMonitoringEnabled(true); f.mic(.8f); f.render(4096, 1, 1);
    for (float sample : f.left) require(std::isfinite(sample) && std::abs(sample) < .98f, "Mono mix clipped");
    std::cout << "PASS: staggered quantize on all five tracks, exact 96000/192000 frames, STOP cancellation, stereo-linked mix headroom\n";
}

int untilSyncBar(const FiveTracks& f)
{
    const auto block = AudioSystemTestAccess::syncBlock(f.audio);
    require(block.enabled, "Shared sync grid not established");
    return std::max(0, static_cast<int>(std::ceil((std::ceil(block.bar - 1.e-10) - block.bar)
                                               * block.framesPerBar - 1.e-6)));
}

void recordSyncedMeasures(FiveTracks& f, int bar)
{
    using C = LoopTrack::Command;
    f.audio.setAllLoopSync(true);
    for (size_t i = 0; i < f.inputL.size(); ++i)
    { f.inputL[i] = .02f * std::sin(static_cast<float>(i) * .07f); f.inputR[i] = -.5f * f.inputL[i]; }
    f.command(0, C::primary);
    require(f.audio.trackSyncStatus(0).waiting == LoopTrack::SyncWait::record, "REC did not wait for measure");
    const int wait = untilSyncBar(f);
    f.render(wait - 1);
    require(f.audio.trackSnapshot(0).state == LoopTrack::State::empty, "REC started before measure");
    f.render(1);
    require(f.audio.trackSnapshot(0).state == LoopTrack::State::recording && f.audio.trackSnapshot(0).frames == 0,
            "REC did not start exactly on measure boundary");
    for (int i = 1; i < 5; ++i) f.command(i, C::primary);
    f.render(bar / 4);
    f.command(0, C::primary); f.command(3, C::primary);
    f.render(bar - bar / 4);
    require(f.audio.trackSnapshot(0).state == LoopTrack::State::playing, "1-bar capture did not finish");
    f.render(1); f.command(1, C::primary); f.command(4, C::primary); f.render(bar - 1);
    f.render(bar + 1); f.command(2, C::primary); f.render(bar - 1);
    const int measures[] { 1, 2, 4, 1, 2 };
    for (int i = 0; i < 5; ++i)
    {
        const auto state = f.audio.trackSnapshot(i);
        require(state.state == LoopTrack::State::playing && state.frames == measures[i] * bar
                && state.progress < 1.e-6f, "1/2/4-bar capture length or shared phase wrong");
    }
}

void testMeasureSync()
{
    using C = LoopTrack::Command; using S = LoopTrack::State;
    FiveTracks f(48000, 480000);
    recordSyncedMeasures(f, 96000);
    f.render(4096); const auto baseline = f.left;
    f.render(4 * 96000 - 4096); f.render(4096);
    for (int i = 256; i < 4096; ++i) // Exclude the existing 5 ms initial transport fade.
        require(std::abs(f.left[static_cast<size_t>(i)] - baseline[static_cast<size_t>(i)]) < 1.e-6f,
                "Synced common cycle inserted silence or changed playback samples");
    f.render(4 * 96000 - 4096);
    require(! f.audio.quantizeEnabled(), "LOOP SYNC overwrote independent QUANTIZE preference");
    require(! f.audio.setBpm(140) && f.audio.bpm() == 120 && f.audio.bpmChangeBlocked(),
            "BPM edit silently desynchronized recorded phrases");
    f.render(1234); const auto phase1 = f.audio.trackSnapshot(0).progress;
    f.command(2, C::stop); f.command(2, C::primary);
    require(f.audio.trackSyncStatus(2).waiting == LoopTrack::SyncWait::play
            && f.audio.trackSnapshot(0).progress == phase1, "Resume did not wait or reset another track");
    f.render(untilSyncBar(f));
    require(f.audio.trackSnapshot(2).state == S::playing && f.audio.trackSnapshot(2).progress < 1.e-6f,
            "MEASURE resume did not start at beginning of phrase on next bar");
    const auto phase2 = f.audio.trackSnapshot(2).progress;
    f.command(0, C::primary); f.render(96000 * 4); f.command(0, C::primary);
    require(f.audio.trackSnapshot(0).canUndo && f.audio.trackSnapshot(0).frames == 96000
            && f.audio.trackSnapshot(2).progress == phase2, "Synced overdub altered period/phase");
    f.audio.selectTrack(0); require(f.audio.undoSelectedTrack(), "Synced Undo rejected"); f.render(0);
    require(f.audio.trackSnapshot(0).progress < 1.e-6f && f.audio.trackSnapshot(0).frames == 96000,
            "Synced Undo reset phase/changed duration");
    // Cancel a pending PLAY, then a pending REC; no other phrase is disturbed.
    f.render(137); f.command(4, C::stop); f.command(4, C::primary); f.command(4, C::stop);
    f.render(untilSyncBar(f)); require(f.audio.trackSnapshot(4).state == S::stopped, "STOP failed to cancel PLAY wait");
    f.command(4, C::clear); f.render(137); f.command(4, C::primary); f.command(4, C::stop);
    f.render(untilSyncBar(f)); require(f.audio.trackSnapshot(4).state == S::empty, "Cancelled pending REC captured audio");
    // Partial capture must survive STOP and be refused, never rounded/cropped silently.
    f.command(4, C::primary); f.render(1234); f.command(4, C::stop); f.command(4, C::primary);
    require(f.audio.trackSnapshot(4).state == S::stopped && f.audio.trackSnapshot(4).frames == 1234
            && f.audio.trackSyncStatus(4).issue == LoopTrack::SyncIssue::incompatible,
            "Partial/incompatible phrase was changed or silently synchronized");
    f.audio.setLoopSync(4, false); f.render(0); f.command(4, C::primary);
    require(f.audio.trackSnapshot(4).state == S::playing && f.audio.trackSnapshot(4).frames == 1234,
            "Individual LOOP SYNC OFF did not restore free playback");
    f.audio.setLoopSync(4, true); f.render(0);
    require(f.audio.trackSyncStatus(4).issue == LoopTrack::SyncIssue::stopFirst,
            "Enabling sync rephased a running free loop");
    f.stopAll(); require(! f.audio.setBpm(90), "STOP unlocked BPM despite synchronized phrases");
    f.audio.setAllLoopSync(false); f.render(0);
    require(f.audio.setBpm(140), "SYNC OFF failed to release tempo lock");
    for (int i = 0; i < 5; ++i) f.command(i, C::primary);
    require(f.audio.trackSnapshot(2).state == S::playing, "SYNC OFF resume retained scheduling");
    f.stopAll(); f.audio.setAllLoopSync(true); f.render(0); f.command(0, C::primary);
    require(f.audio.trackSnapshot(0).state == S::stopped
            && f.audio.trackSyncStatus(0).issue == LoopTrack::SyncIssue::incompatible,
            "Old 120 BPM phrase accepted at incompatible 140 BPM");
    f.audio.setAllLoopSync(false); f.render(0); f.audio.setBpm(120);
    f.audio.setAllLoopSync(true); f.command(0, C::primary); f.render(untilSyncBar(f));
    require(f.audio.trackSnapshot(0).state == S::playing, "Compatible existing phrase could not rejoin");
    std::cout << "PASS: LOOP SYNC MEASURE 5 tracks; 1/2/4 measures (96000/192000/384000 frames), waits/cancellation, resume, overdub/Undo, individual OFF, incompatible phrase protection and BPM guard\n";
}

void testSyncDriftAndRates()
{
    using C = LoopTrack::Command;
    for (const double rate : { 1000.0, 44100.0, 96000.0 })
    {
        FiveTracks f(rate, static_cast<int>(rate * 12));
        f.audio.setBpm(123.4);
        const int bar = static_cast<int>(std::round(rate * 240 / 123.4));
        recordSyncedMeasures(f, bar);
        const int cycles = rate == 1000 ? 301 : 3;
        f.render(bar * 4 * cycles + 123);
        const int bars[] { 1, 2, 4, 1, 2 };
        for (int i = 0; i < 5; ++i)
            require(std::abs(f.audio.trackSnapshot(i).progress - 123.0 / (bar * bars[i])) < 2.e-7,
                    "Fractional BPM loops drifted from shared sample grid");
        // Device switch stops, preserves origin/phrases and adapts clock to new rate.
        const auto before = AudioSystemTestAccess::syncBlock(f.audio);
        f.hardware.sampleRate = rate * 2; f.audio.selectDevices("Mic 2", "Output 2");
        const auto after = AudioSystemTestAccess::syncBlock(f.audio);
        require(std::abs(before.bar - after.bar) < 1.e-10
                && std::abs(after.framesPerBar - 2 * before.framesPerBar) < 1.e-7, "Device change lost shared origin");
        f.command(0, C::primary); f.render(untilSyncBar(f)); f.render(246);
        require(std::abs(f.audio.trackSnapshot(0).progress - 123.0 / bar) < 2.e-7,
                "Rate change changed phrase duration or resume phase");
        // Clearing one track does not remove the grid; clearing all releases it.
        f.stopAll(); f.command(0, C::clear);
        require(f.audio.bpmLocked(), "Clearing one track erased reference for other tracks");
        for (int i = 1; i < 5; ++i) f.command(i, C::clear);
        require(f.audio.setBpm(90), "Clearing final synchronized phrase left BPM locked");
    }
    {
        FiveTracks mixed(1000, 60000); mixed.audio.setBpm(123.4);
        recordSyncedMeasures(mixed, 1945);
        mixed.command(4, C::stop); mixed.command(4, C::clear);
        mixed.audio.setLoopSync(4, false); mixed.audio.setQuantizeEnabled(true);
        mixed.command(4, C::primary); mixed.render(17600); mixed.command(4, C::primary);
        mixed.render(1849);
        require(mixed.audio.trackSnapshot(4).frames == 19449
                && mixed.audio.trackSnapshot(4).state == LoopTrack::State::playing,
                "SYNC OFF + Q ON changed existing fractional-BPM quantize rounding");
    }
    // If a rate change makes new capture bars non-integral, don't resample/crop RAM.
    FiveTracks f(48000, 480000); f.audio.setBpm(123.4);
    const int bar = static_cast<int>(std::round(48000.0 * 240 / 123.4));
    recordSyncedMeasures(f, bar); f.stopAll(); f.command(4, C::clear);
    f.hardware.sampleRate = 44100; f.audio.selectDevices("Mic 2", "Output 2");
    f.command(4, C::primary); f.render(32);
    require(f.audio.trackSyncStatus(4).issue == LoopTrack::SyncIssue::sampleGrid
            && ! f.audio.trackSnapshot(4).hasPhrase(), "Incompatible sample grid silently modified new take");
    f.audio.setLoopSync(4, false); f.render(0); f.command(4, C::primary); f.render(1234); f.command(4, C::primary);
    require(f.audio.trackSnapshot(4).frames == 1234, "Rate-grid refusal broke free recording");
    std::cout << "PASS: 301 four-bar repetitions without drift, fractional BPM, 44.1/96 kHz, preserved origin on rate switch, clear/unlock and safe incompatible-rate refusal\n";
}

void testSyncLifecycleLimits()
{
    using C = LoopTrack::Command; using S = LoopTrack::State;
    {
        FiveTracks f(48000, 1000); f.audio.setAllLoopSync(true); f.command(0, C::primary); f.render(512);
        require(f.audio.trackSyncStatus(0).issue == LoopTrack::SyncIssue::capacity
                && f.audio.trackSnapshot(0).state == S::empty && ! f.audio.bpmLocked(),
                "Too-small buffer did not refuse REC visibly and release reference");
    }
    {
        FiveTracks f; f.audio.setAllLoopSync(true); f.command(0, C::primary);
        f.audio.selectDevices({}, {});
        require(f.audio.trackSyncStatus(0).waiting == LoopTrack::SyncWait::none && f.audio.setBpm(90),
                "Disconnect retained an empty pending start/tempo lock");
    }
    {
        FiveTracks f; f.audio.setAllLoopSync(true); f.command(0, C::primary); f.render(untilSyncBar(f));
        f.command(0, C::stop);
        require(f.audio.trackSnapshot(0).state == S::empty && f.audio.setBpm(90),
                "Zero-frame recording kept sync reference forever");
    }
    {
        FiveTracks f(48000, 480000); recordSyncedMeasures(f, 96000);
        const double originPosition = AudioSystemTestAccess::syncBlock(f.audio).bar;
        f.audio.selectDevices({}, {}); f.audio.commandTrack(0, C::stop);
        require(f.audio.bpmLocked() && ! f.audio.setBpm(90)
                && AudioSystemTestAccess::syncBlock(f.audio).bar == originPosition,
                "Offline STOP discarded shared reference for stored phrases");
        for (int i = 0; i < 5; ++i) f.audio.commandTrack(i, C::clear);
        require(f.audio.setBpm(90), "Offline final clear did not release tempo");
    }
    std::cout << "PASS: sync buffer limit, disconnect cancellation, zero-frame REC and offline phrase/origin preservation\n";
}




void testInputRobotPanel()
{
    FiveTracks f; MainComponent panel(f.audio);
    TopPanelComponent* top = nullptr;
    for (auto* child : panel.getChildren()) if (auto* t = dynamic_cast<TopPanelComponent*>(child)) top = t;
    juce::Button *edit = nullptr, *slotB = nullptr, *exit = nullptr;
    MemoryDisplay* display = nullptr;
    std::array<ParameterKnob*, 4> knobs {};
    for (auto* child : top->getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*>(child))
        {
            if (b->getTitle() == "INPUT FX EDIT") edit = b;
            if (b->getTitle() == "INPUT FX B") slotB = b;
            if (b->getButtonText() == "EXIT") exit = b;
        }
        if (auto* d = dynamic_cast<MemoryDisplay*>(child)) display = d;
        if (auto* k = dynamic_cast<ParameterKnob*>(child))
            for (int i = 0; i < 4; ++i) if (k->getName() == "Parameter knob " + juce::String(i + 1)) knobs[size_t(i)] = k;
    }
    require(edit && slotB && exit && display && knobs[3], "INPUT FX controls missing");
    edit->onClick(); slotB->onClick();
    require(display->getEditorField(0).text == "BPF", "Existing B slot overwritten");
    knobs[0]->setValue(4, juce::sendNotificationSync);
    require(display->getEditorField(1).name == "NOTE" && knobs[2]->getMinimum() == -50 && knobs[3]->getMinimum() == 1,
            "ROBOT four encoders do not expose official ranges");
    knobs[1]->setValue(1, juce::sendNotificationSync);
    knobs[2]->setValue(-50, juce::sendNotificationSync);
    knobs[3]->setValue(1, juce::sendNotificationSync);
    require(display->getTitle().contains("NOTE:C#") && display->getTitle().contains("FORMANT:-50")
            && display->getTitle().contains("MODE:1"), "ROBOT display values do not match encoders");
    const auto event = juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {20,20},
        juce::ModifierKeys(), 1, 0, 0, 0, 0, knobs[2], knobs[2], juce::Time::getCurrentTime(),
        {20,20}, juce::Time::getCurrentTime(), 1, false);
    juce::MouseWheelDetails wheel {}; wheel.deltaY = .1f;
    knobs[2]->mouseWheelMove(event, wheel);
    require(display->getEditorField(2).value > -50, "FORMANT wheel did not update parameter");
    knobs[2]->setValue(50, juce::sendNotificationSync);
    {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mk2-6h1-robot-display.png");
        juce::FileOutputStream out(file); out.setPosition(0); out.truncate(); juce::PNGImageFormat png;
        require(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(), true, 4.f), out), "ROBOT preview failed");
    }
    exit->onClick(); slotB->setToggleState(true, juce::dontSendNotification); slotB->onClick();
    f.audio.setMonitoringEnabled(true); f.audio.setMonitorVolume(.25f);
    for (int n = 0; n < 4096; ++n) f.inputL[size_t(n)] = .2f * std::sin(n * .15f);
    f.render(8192);
    float delta = 0; for (int n = 512; n < 4000; ++n) delta += std::abs(f.left[size_t(n)] - .25f * f.inputL[size_t(n)]);
    require(delta > 1, "Physical INPUT FX B/encoders not connected to real audio");
    edit->onClick(); slotB->onClick();
    require(slotB->getToggleState() && display->getEditorField(2).value == 50, "Opening ROBOT edit toggled/lost its state");
    knobs[0]->setValue(1, juce::sendNotificationSync); f.render(4096);
    for (int n = 1500; n < 4000; ++n)
        require(f.left[size_t(n)] == .25f * f.inputL[size_t(n)], "Changing to visual BPF left ROBOT DSP on");
    knobs[0]->setValue(4, juce::sendNotificationSync);
    require(display->getEditorField(1).text == "C#" && display->getEditorField(2).value == 50 && display->getEditorField(3).value == 1,
            "Changing type lost ROBOT values");
    exit->onClick(); slotB->setToggleState(false, juce::dontSendNotification); slotB->onClick(); f.render(4096);
    for (int n = 1500; n < 4000; ++n)
        require(f.left[size_t(n)] == .25f * f.inputL[size_t(n)], "Physical INPUT FX B OFF not dry");
    std::cout << "PASS: INPUT ROBOT real panel -> engine, NOTE/FORMANT/MODE, wheel, LCD, slot selection, preserved BPF and EXIT\n";
}

void testInputRobotIntegration()
{
    using Command = LoopTrack::Command;
    FiveTracks f;
    RobotParameters robot; robot.enabled = true; robot.note = 4;
    f.audio.setInputRobot(1, robot);
    for (int n = 0; n < 4096; ++n)
    { f.inputL[size_t(n)] = .2f * std::sin(juce::MathConstants<float>::twoPi * 75.f * n / 4096.f); f.inputR[size_t(n)] = 0; }
    f.render(4096);
    for (float x : f.left) require(x == 0, "ROBOT enabled monitoring automatically");
    f.audio.setMonitorVolume(.25f); f.audio.setMonitoringEnabled(true); f.render(4096);
    float difference = 0, energy = 0;
    for (int n = 512; n < 4000; ++n)
    {
        difference += std::abs(f.left[size_t(n)] - .25f * f.inputL[size_t(n)]);
        energy += std::abs(f.left[size_t(n)]);
        require(f.right[size_t(n)] == 0, "Input ROBOT leaked across stereo channels");
    }
    require(difference > 2 && energy > 1, "ROBOT is not audible without a loop / dry input path still used");
    for (int i = 0; i < 5; ++i) require(!f.audio.trackSnapshot(i).hasPhrase(), "Live ROBOT required/created a loop");
    // Record wet, with FX OFF on track 2 recording the SAME microphone dry.
    f.audio.setTrackFxEnabled(1, false); f.render(4096);
    f.command(0, Command::primary); f.command(1, Command::primary);
    f.render(4096);
    auto wetCapture = f.left; // Exactly one monitored wet path, gain .25.
    f.command(0, Command::primary); f.command(1, Command::primary);
    f.stopAll(); f.audio.setMonitoringEnabled(false); f.mic(0);
    f.command(0, Command::primary); f.render(4096);
    for (int n = 512; n < 3800; ++n)
        require(std::abs(f.left[size_t(n)] - 4 * wetCapture[size_t(n)]) < 1.e-5f,
                "Processed input not recorded once, or ROBOT reapplied to loop playback");
    f.command(0, Command::stop); f.render(4096);
    f.command(1, Command::primary); f.render(4096);
    for (int n = 512; n < 3800; ++n)
    {
        const float raw = .2f * std::sin(juce::MathConstants<float>::twoPi * 75.f * n / 4096.f);
        require(std::abs(f.left[size_t(n)] - raw) < 1.e-5f, "Per-track FX OFF did not preserve raw microphone capture");
    }
    f.stopAll(); robot.enabled = false; f.audio.setInputRobot(1, robot);
    for (int n = 0; n < 4096; ++n)
        f.inputL[size_t(n)] = .2f * std::sin(juce::MathConstants<float>::twoPi * 75.f * n / 4096.f);
    f.audio.setMonitoringEnabled(true); f.render(4096);
    for (int n = 1500; n < 4000; ++n)
        require(f.left[size_t(n)] == .25f * f.inputL[size_t(n)], "ROBOT OFF doubled/lost live dry monitor");
    f.command(2, Command::primary); f.render(4096); f.command(2, Command::primary); f.stopAll();
    f.audio.setMonitoringEnabled(false); f.mic(0); robot.enabled = true; f.audio.setInputRobot(1, robot);
    f.command(2, Command::primary); f.render(4096);
    for (int n = 512; n < 3800; ++n)
    {
        const float raw = .2f * std::sin(juce::MathConstants<float>::twoPi * 75.f * n / 4096.f);
        require(std::abs(f.left[size_t(n)] - raw) < 1.e-5f, "ROBOT ON changed a previously dry recording");
    }
    // Wet overdub is captured once. Undo restores the dry original, with TRACK FX on.
    TrackFxParameters trackFx; trackFx.enabled = true; trackFx.distortionEnabled = true;
    f.audio.setTrackFxInsert(0, 0); f.audio.setTrackFxInsert(2, 0); f.audio.setTrackEffects(trackFx);
    f.command(2, Command::primary);
    for (int n = 0; n < 4096; ++n) f.inputL[size_t(n)] = .1f * std::sin(n * .1f);
    f.render(8192); f.command(2, Command::primary);
    f.audio.selectTrack(2); require(f.audio.undoSelectedTrack(), "ROBOT overdub blocked Undo");
    f.render(0); f.stopAll(); f.audio.setTrackEffects({}); f.mic(0);
    f.command(2, Command::primary); f.render(8192); // Allow TRACK FX bypass ramps to settle.
    for (int n = 512; n < 3800; ++n)
        require(std::abs(f.left[size_t(n)] - .2f * std::sin(juce::MathConstants<float>::twoPi * 75.f * n / 4096.f)) < 1.e-5f,
                "Wet overdub/Undo corrupted initial audio or captured output");
    f.stopAll(); f.audio.setMonitoringEnabled(true); f.render(4096, 0);
    for (float x : f.left) require(std::abs(x) < 1.e-7f, "ROBOT produced stale voice without input");
    f.audio.selectDevices("Mic 2", "Output 2");
    require(!f.audio.isMonitoringEnabled() && f.audio.trackSnapshot(0).hasPhrase(), "ROBOT broke safe reconnect/phrase preservation");
    std::cout << "PASS: INPUT ROBOT live-only monitoring, one wet path, pre-record FX/FX OFF, stored wet/dry, no playback reprocessing, overdub/Undo/TRACK FX, missing input/reconnect\n";
}

void testSharedTrackFxRouting()
{
    using Command = LoopTrack::Command;
    FiveTracks f;
    TrackFxParameters fx; fx.enabled = true; fx.cutoff = 20; fx.resonance = 100;
    fx.distortionEnabled = true;
    f.audio.setTrackEffects(fx);
    f.audio.setTrackFxInsert(0, 0); f.audio.setTrackFxInsert(2, 0);
    f.audio.setMonitoringEnabled(true);
    // Record each original while earlier loops, ALL effects and monitor are audible.
    for (int i = 0; i < 5; ++i)
    {
        f.mic(.02f * (i + 1)); f.command(i, Command::primary); f.render(4096);
        f.command(i, Command::primary);
    }
    f.audio.setMonitoringEnabled(false); f.mic(0); f.stopAll();
    const auto nearStereo = [&](float l, float r, const char* error)
    { require(std::abs(f.left[2000] - l) < .0002f && std::abs(f.right[2000] - r) < .0002f, error); };
    const auto distorted = [&](float x) { return .25f * std::tanh(TrackFxParameters::driveGain(fx.drive) * x); };
    // Each existing slot can address each real track, without copying parameters/DSP.
    for (int slot : { 0, 2 })
    {
        fx.enabled = slot == 0; fx.distortionEnabled = slot == 2;
        f.audio.setTrackEffects(fx);
        for (int target = 1; target <= 5; ++target)
        {
            f.audio.setTrackFxInsert(slot, target);
            f.audio.selectTrack(target % 5); // Selection must not redirect INSERT.
            f.command(target - 1, Command::primary); f.render(8192);
            const float x = .02f * target;
            nearStereo(slot == 0 ? .25f * x : distorted(x),
                       slot == 0 ? -.125f * x : distorted(-.5f * x), "Shared slot did not reach selected INSERT track");
            f.audio.setTrackFxEnabled(target - 1, false); f.render(8192);
            nearStereo(x, -.5f * x, "Per-track FX OFF failed to bypass or original capture contains FX");
            f.audio.setTrackFxEnabled(target - 1, true);
            f.audio.setTrackVolume(target - 1, .5f); f.render(8192);
            nearStereo(.5f * (slot == 0 ? .25f * x : distorted(x)),
                       .5f * (slot == 0 ? -.125f * x : distorted(-.5f * x)), "Individual insert no longer precedes fader");
            f.audio.setTrackVolume(target - 1, 1);
            f.command(target - 1, Command::stop); f.render(4096);
            const int other = target % 5;
            f.command(other, Command::primary); f.render(4096);
            nearStereo(.02f * (other + 1), -.01f * (other + 1), "Individual slot leaked into another track");
            f.command(other, Command::stop); f.render(4096);
        }
    }
    f.playAll(); f.audio.setTrackFxInsert(2, 0); f.render(8192);
    nearStereo(distorted(.3f), distorted(-.15f), "ALL must process one summed signal, not five distortion instances");
    f.audio.setTrackFxEnabled(1, false); f.render(8192);
    nearStereo(.04f + distorted(.26f), -.02f + distorted(-.13f), "FX OFF removed/changed the wrong ALL-bus contribution");
    f.audio.setTrackFxEnabled(1, true);
    fx.enabled = true; fx.distortionEnabled = false;
    f.audio.setTrackEffects(fx); f.audio.setTrackFxInsert(0, 0); f.render(8192);
    nearStereo(.075f, -.0375f, "LPF INSERT ALL did not process all enabled tracks");
    fx.distortionEnabled = true; f.audio.setTrackEffects(fx); f.render(8192);
    nearStereo(distorted(.075f), distorted(-.0375f), "Shared ALL LPF -> DISTORTION chain incorrect");
    f.audio.setTrackFxInsert(0, 2); f.audio.setTrackFxInsert(2, 4); f.render(8192);
    nearStereo(.3f - .04f - .08f + .01f + distorted(.08f),
               -.15f + .02f + .04f - .005f + distorted(-.04f), "Independent A/C destinations interfered");
    f.audio.setTrackFxInsert(0, 0); f.audio.setTrackFxInsert(2, 4); f.render(8192);
    nearStereo(.25f * (.3f - .08f + distorted(.08f)),
               .25f * (-.15f + .04f + distorted(-.04f)), "Individual insert -> ALL bus ordering incorrect");
    // Recording/overdub only receive the mic, even with shared processing active.
    f.command(3, Command::primary); f.mic(.01f); f.render(8192); f.command(3, Command::primary);
    f.audio.setTrackEffects({}); f.mic(0); f.stopAll();
    for (int i = 0; i < 5; ++i)
    {
        f.command(i, Command::primary); f.render(4096);
        const float x = .02f * (i + 1) + (i == 3 ? .02f : 0);
        nearStereo(x, -.5f * x, "FX/other tracks/monitor leaked into original or overdub RAM");
        if (i == 3)
        {
            f.audio.selectTrack(i); require(f.audio.undoSelectedTrack(), "Routed overdub Undo rejected");
            f.render(4096); nearStereo(.08f, -.04f, "Routed overdub Undo did not restore original");
        }
        f.command(i, Command::stop); f.render(4096);
    }
    f.command(0, Command::primary); f.render(8192, 0, 1);
    require(std::abs(f.left[2000] - .005f) < .0002f, "Routing broke mono output/no-input playback");
    std::cout << "PASS: shared LPF/DISTORTION ALL and TRACK1-5, per-track FX, independent INSERT, faders, capture/monitor isolation, overdub/Undo and mono\n";
}

void testTrackPanelRouting()
{
    FiveTracks f; MainComponent panel(f.audio);
    std::array<TrackComponent*, 5> tracks {}; int count = 0;
    MemoryDisplay* display = nullptr;
    for (auto* child : panel.getChildren())
    {
        if (auto* t = dynamic_cast<TrackComponent*>(child)) tracks[static_cast<size_t>(count++)] = t;
        if (auto* top = dynamic_cast<TopPanelComponent*>(child))
            for (auto* c : top->getChildren()) if (auto* d = dynamic_cast<MemoryDisplay*>(c)) display = d;
    }
    require(count == 5 && display, "Five engine-driven UI tracks missing");
    f.mic(.1f);
    for (int i = 0; i < 5; ++i)
    {
        juce::Button* select = nullptr; LedRingButton* ring = nullptr; VolumeFader* fader = nullptr;
        for (auto* child : tracks[static_cast<size_t>(i)]->getChildren())
        {
            if (auto* b = dynamic_cast<juce::Button*>(child)) if (b->getName() == "TRACK " + juce::String(i + 1)) select = b;
            if (auto* r = dynamic_cast<LedRingButton*>(child)) ring = r;
            if (auto* v = dynamic_cast<VolumeFader*>(child)) fader = v;
        }
        require(select && ring && fader, "Per-track UI controls missing");
        ring->onClick(); f.render(4096); select->onClick(); // refresh actual snapshot
        require(ring->getVisualState() == TrackVisualState::rec && f.audio.selectedTrack() == i,
                "Track ring remained simulated or selected wrong target");
        ring->onClick(); f.render(0); select->onClick();
        require(ring->getVisualState() == TrackVisualState::play
                && display->getTitle().contains("CURRENT:" + juce::String(i + 1)), "LED/LCD not driven by engine");
        fader->setValue(.5, juce::sendNotificationSync);
    }
    tracks[1]->onSelected();
    f.audio.commandTrack(4, LoopTrack::Command::stop); f.render(256);
    // A plain TRACK selection never changes transport and engine updates never retarget Undo.
    require(f.audio.selectedTrack() == 1 && f.audio.trackSnapshot(1).state == LoopTrack::State::playing,
            "TRACK selection changed transport/Undo target");
    f.render(4096); f.near(.2f, "Panel faders did not control their own engine tracks");

    TopPanelComponent* top = nullptr;
    for (auto* child : panel.getChildren()) if (auto* t = dynamic_cast<TopPanelComponent*>(child)) top = t;
    juce::Button *edit = nullptr, *enter = nullptr, *exit = nullptr, *next = nullptr;
    ParameterKnob* first = nullptr;
    for (auto* child : top->getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*>(child))
        {
            if (b->getTitle() == "TRACK FX EDIT") edit = b;
            if (b->getButtonText() == "ENTER") enter = b;
            if (b->getButtonText() == "EXIT") exit = b;
            if (b->getButtonText() == "Next") next = b;
        }
        if (auto* k = dynamic_cast<ParameterKnob*>(child)) if (k->getName() == "Parameter knob 1") first = k;
    }
    require(edit && enter && exit && next && first, "Routing editor controls missing");
    const auto preview = [&](const char* name)
    {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
        juce::FileOutputStream out(file); out.setPosition(0); out.truncate();
        juce::PNGImageFormat png;
        require(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(), true, 4.f), out), "Could not render routing LCD");
    };
    preview("mk2-6h-track.png");
    require(display->isTrackSettings() && display->getTitle().contains("TRACK 2 SETTINGS"), "TRACK did not open selected settings");
    first->setValue(0, juce::sendNotificationSync);
    require(! f.audio.trackFxEnabled(1) && f.audio.trackFxEnabled(0), "TRACK setting changed another FX gate");
    juce::Button* fx2 = nullptr;
    for (auto* child : tracks[1]->getChildren())
        if (auto* b = dynamic_cast<juce::Button*>(child)) if (b->getName() == "FX 2") fx2 = b;
    require(fx2 && ! fx2->getToggleState(), "Track FX LED disagrees with settings");
    fx2->setToggleState(true, juce::dontSendNotification); fx2->onClick();
    require(f.audio.trackFxEnabled(1) && first->getValue() == 1, "Physical FX button did not update engine/LCD");
    exit->onClick(); require(! display->isTrackSettings(), "EXIT did not leave TRACK settings");
    edit->onClick(); enter->onClick();
    require(display->isRoutingEditing() && first->getTitle().contains("INSERT"), "ENTER did not expose INSERT");
    first->setValue(3, juce::sendNotificationSync);
    require(f.audio.trackFxInsert(0) == 3 && f.audio.trackFxInsert(2) == 1, "LPF INSERT changed C or ignored knob");
    next->onClick(); first->setValue(0, juce::sendNotificationSync);
    require(f.audio.trackFxInsert(2) == 0 && f.audio.trackFxInsert(0) == 3, "C INSERT ALL changed A");
    preview("mk2-6h-insert.png");
    exit->onClick(); require(display->isTrackEditing() && ! display->isRoutingEditing(), "EXIT did not return to parameters");
    require(display->getEditorField(0).name == "DRIVE" && display->getEditorField(0).value == 60,
            "INSERT editing modified distortion parameters");
    exit->onClick(); tracks[4]->onSelected();
    require(f.audio.selectedTrack() == 4 && f.audio.trackFxInsert(0) == 3 && f.audio.trackFxInsert(2) == 0,
            "TRACK selection redirected shared effects");
    std::cout << "PASS: panel FX/track settings/INSERT knobs/EXIT and independent selection match engine\n";

}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    try { runRobotEffectTests(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    try
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        const auto previous = _CrtSetAllocHook(fiveTrackAllocationHook);
#endif
        testFiveTracks(); testFiveQuantizedTracksAndMix(); testTrackPanelRouting(); testSharedTrackFxRouting(); testInputRobotIntegration(); testInputRobotPanel();
        testMeasureSync(); testSyncDriftAndRates(); testSyncLifecycleLimits();
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetAllocHook(previous);
#endif
        require(fiveTrackAllocations == 0, "Allocation inside five-track production callback");
        std::cout << "PASS: no allocations in five-track callback after JUCE manager scratch warmup\n";
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    try { runMusicalClockTests(); testTempoKeepsFreeLoops(); testQuantizeIntegration(); runTempoPanelTests(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    try { testDevices(); testMonitoring(); runLoopTrackTests(); testLoopIntegration(); testDistortionSignalPath(); std::cout << "PASS: devices, monitoring, LoopTrack REC/PLAY/OVERDUB/STOP/UNDO, multi-pass takes, fixed period/phase, COW snapshots, LPF/DISTORTION response/bypass/limits/stereo/RAM preservation, no callback allocations, independent mix, reconnect and shutdown\n"; }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    return 0;
}
