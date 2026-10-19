#include "AudioSystem.h"
#include "MainComponent.h"
#include "InputFxShortcuts.h"
#if defined(_MSC_VER) && defined(_DEBUG)
 #include <crtdbg.h>
#endif
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <array>
#include <cmath>
#include <limits>

void runLoopTrackTests();
void runRobotEffectTests();
void runOscBotEffectTests();
void runReverbEffectTests();
void runG2BEffectTests();
void runStepSlicerEffectTests();
void runMusicalClockTests();
void runTempoPanelTests();

struct AudioSystemTestAccess
{
    static int currentBank(const AudioSystem& a) { return a.inputBank; }
    static double slicerPosition(const AudioSystem& a,int bank,int slot) { return a.inputBanks[size_t(bank)]->slicers[size_t(slot)].position(); }
    static uint64_t consumedInput(const AudioSystem& audio) { return audio.consumedInputActivation; }
    static bool oscEnabled(const AudioSystem& audio,int slot) { return (audio.inputBanks[0]->oscs[size_t(slot)].applied&1)!=0; }
    static int sequenceNote(const AudioSystem& audio,int slot) { return audio.inputBanks[0]->oscs[size_t(slot)].soundingNote; }
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


void testFxBanks6I1()
{
    using Group=EffectGroup; using Light=PanelButton::FxLight; using C=LoopTrack::Command;
    FiveTracks f; MainComponent panel(f.audio); TopPanelComponent* top=nullptr;
    MemoryDisplay* lcd=nullptr;
    std::array<PanelButton*,2> edits{};
    std::array<std::array<PanelButton*,4>,2> slots{};
    std::array<ParameterKnob*,4> knobs{};
    PanelButton *exit=nullptr,*enter=nullptr,*next=nullptr,*previous=nullptr;
    for(auto* child:panel.getChildren()) if(auto* t=dynamic_cast<TopPanelComponent*>(child)) top=t;
    require(top,"FX panel absent");
    for(auto* child:top->getChildren())
    {
        if(auto* d=dynamic_cast<MemoryDisplay*>(child)) lcd=d;
        if(auto* k=dynamic_cast<ParameterKnob*>(child))
            for(int i=0;i<4;++i) if(k->getName()=="Parameter knob "+juce::String(i+1)) knobs[size_t(i)]=k;
        if(auto* b=dynamic_cast<PanelButton*>(child))
        {
            for(int g=0;g<2;++g)
            {
                const juce::String prefix=g==0?"INPUT FX ":"TRACK FX ";
                if(b->getTitle()==prefix+"EDIT") edits[size_t(g)]=b;
                for(int i=0;i<4;++i) if(b->getTitle()==prefix+juce::String::charToString(char('A'+i))) slots[size_t(g)][size_t(i)]=b;
            }
            if(b->getButtonText()=="EXIT") exit=b;
            if(b->getButtonText()=="ENTER") enter=b;
            if(b->getButtonText()=="Next") next=b;
            if(b->getButtonText()=="Previous") previous=b;
        }
    }
    require(lcd && edits[1] && slots[1][3] && knobs[3] && exit && enter && next && previous,"FX bank controls absent");
    const auto event=[](juce::Component* c,juce::Point<float> p,juce::Point<float> down,bool pressed)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
            juce::ModifierKeys(pressed?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,c,c,
            juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,false);
    };
    const auto shortEdit=[&](int g)
    {
        auto* b=edits[size_t(g)]; const juce::Point<float> p(10,5); // printed EDIT label, not only ring
        b->mouseDown(event(b,p,p,true)); b->mouseUp(event(b,p,p,false));
    };
    const auto holdEdit=[&](int g)
    {
        auto* b=edits[size_t(g)]; const juce::Point<float> p(10,5);
        b->mouseDown(event(b,p,p,true));
        // Service the message-thread timers throughout the hold, as the real event loop does.
        for(int n=0;n<80;++n) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
        require(lcd->bankSelecting(Group(g)),"EDIT long press did not enter BANK SELECT while held");
        b->mouseUp(event(b,p,p,false));
        require(lcd->bankSelecting(Group(g)),"EDIT release incorrectly confirmed bank selection");
        require(lcd->getTitle().contains("BANK SELECT"),"LCD missing bank mode");
    };
    const auto chooseBank=[&](int g,int bank)
    {
        holdEdit(g); slots[size_t(g)][size_t(bank)]->onClick();
        require(lcd->fxBank(Group(g))==bank,"physical A-D did not change real bank");
        for(int i=0;i<4;++i) require(slots[size_t(g)][size_t(i)]->getFxLight()==(i==bank?Light::blue:Light::off),"bank LED not BLUE/OFF");
        shortEdit(g); require(!lcd->bankSelecting(Group(g)),"EDIT did not confirm");
    };
    const auto set=[&](int i,int value){knobs[size_t(i)]->setValue(value,juce::sendNotificationSync);};
    const auto preview=[&](const char* name)
    {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("tmp"); folder.createDirectory();
        juce::FileOutputStream out(folder.getChildFile(name)); out.setPosition(0); out.truncate(); juce::PNGImageFormat png;
        require(png.writeImageToStream(top->createComponentSnapshot(top->getLocalBounds(),true,1.5f),out),"bank preview failed");
    };
    // Existing loop continues through navigation and all bank/slot edits.
    f.mic(.1f); f.command(0,C::primary); f.render(4096); f.command(0,C::primary);
    for(int bank=0;bank<4;++bank)
    {
        chooseBank(0,bank); f.render(256);
        require(AudioSystemTestAccess::currentBank(f.audio)==bank,"INPUT bank never reached callback");
        require(lcd->trackBank()==0,"INPUT changed TRACK bank");
        for(int slot=0;slot<4;++slot)
        {
            slots[0][size_t(slot)]->onClick(); set(0,5); // OSC BOT; no DSP changes
            next->onClick(); set(0,48+bank*4+slot); // NOTE, page 2 knob 1
            require(lcd->inputOscBot(slot).note==48+bank*4+slot,"INPUT bank-slot NOTE not editable");
            const auto id=juce::String::charToString(char('A'+bank))+"-"+juce::String::charToString(char('A'+slot));
            require(lcd->getTitle().contains("FX "+id),"INPUT LCD bank-slot identifier wrong");
            require(!lcd->inputEnabled(slot),"slot selection enabled effect");
            // Distinct enabled pattern in each bank, toggled only outside editor.
            exit->onClick(); if(slot<=bank) slots[0][size_t(slot)]->onClick(); shortEdit(0);
            slots[0][size_t(slot)]->onClick();
        }
        f.render(256); require(f.audio.trackSnapshot(0).frames==4096 && f.audio.trackSnapshot(0).state==LoopTrack::State::playing,"bank navigation disturbed loop");
    }
    for(int bank=0;bank<4;++bank)
    {
        chooseBank(0,bank);
        for(int i=0;i<4;++i)
        {
            slots[0][size_t(i)]->onClick();
            require(lcd->inputOscBot(i).note==48+bank*4+i,"INPUT 16-slot values not independent");
            require(lcd->inputEnabled(i)==(i<=bank),"INPUT bank ON/OFF lost");
            require(slots[0][size_t(i)]->getFxLight()==(i<=bank?Light::pink:Light::off),"selected slot light not PINK/OFF");
        }
    }
    preview("fx-bank-input-dd-6i1.png");
    const int inputBank=lcd->inputBank();
    // TRACK uses the existing A/LPF and C/DIST processors with separate saved bank values.
    for(int bank=0;bank<4;++bank)
    {
        chooseBank(1,bank); require(lcd->inputBank()==inputBank,"TRACK changed INPUT bank");
        for(int slot=0;slot<4;++slot)
        {
            slots[1][size_t(slot)]->onClick();
            const auto id=juce::String::charToString(char('A'+bank))+"-"+juce::String::charToString(char('A'+slot));
            require(lcd->getTitle().contains("FX "+id),"TRACK LCD hides B/D or wrong bank");
            if(slot==0 || slot==2)
            {
                set(0,20+bank*13+slot); enter->onClick(); set(0,(bank+slot)%6); set(1,bank); exit->onClick();
            }
            else require(!knobs[0]->isEnabled(),"visual TRACK placeholder edits unrelated LPF");
            exit->onClick(); if(slot<=bank) slots[1][size_t(slot)]->onClick(); shortEdit(1);
        }
        f.render(8192);
        require(f.audio.trackFxInsert(0)==bank && f.audio.trackFxInsert(2)==(bank+2)%6,"TRACK bank INSERT publication lost");
    }
    for(int bank=0;bank<4;++bank)
    {
        chooseBank(1,bank); f.render(8192);
        require(lcd->getTrackFx().cutoff==20+bank*13 && lcd->getTrackFx().drive==22+bank*13,"TRACK bank parameters lost");
        require(lcd->fxTarget(Group::track)==bank,"TRACK bank FX TARGET lost");
        require(f.audio.trackFxInsert(0)==bank && f.audio.trackFxInsert(2)==(bank+2)%6,"restored bank not routed");
        for(int i=0;i<4;++i)
        {
            slots[1][size_t(i)]->onClick();
            require(lcd->trackEnabled(i)==(i<=bank),"TRACK bank-slot ON/OFF lost");
            require(slots[1][size_t(i)]->getFxLight()==(i<=bank?Light::pink:Light::off),"TRACK selected state not PINK/OFF");
        }
    }
    preview("fx-bank-track-dd-6i1.png");
    holdEdit(1); preview("fx-bank-blue-6i1.png"); shortEdit(1);
    slots[1][0]->onClick(); require(slots[1][0]->getFxLight()==Light::pink && slots[1][2]->getFxLight()==Light::red,"RED/PINK selection confused");
    preview("fx-bank-red-pink-6i1.png");
    // Single mouse EDIT-drag-release selects a slot before the long-press threshold.
    exit->onClick(); auto* edit=edits[1]; const juce::Point<float> down(10,5);
    auto destination=edit->getLocalPoint(slots[1][2],slots[1][2]->getLocalBounds().getCentre()).toFloat();
    const bool enabled=lcd->trackEnabled(2);
    edit->mouseDown(event(edit,down,down,true)); edit->mouseDrag(event(edit,destination,down,true)); edit->mouseUp(event(edit,destination,down,false));
    require(lcd->isTrackEditing() && lcd->editedSlot()==2 && lcd->trackEnabled(2)==enabled,"captured EDIT drag did not select without toggling");
    // A cancelled press neither opens editor nor leaves a timer armed.
    exit->onClick(); edit->mouseDown(event(edit,down,down,true)); edit->mouseDrag(event(edit,{-30,-30},down,true)); edit->mouseUp(event(edit,{-30,-30},down,false));
    require(!lcd->isTrackEditing() && !lcd->bankSelecting(Group::track),"cancelled EDIT opened modal");
    // Real bank change changes audio through the existing TRACK DSP, not just LEDs.
    chooseBank(0,0); exit->onClick();
    for(int i=0;i<4;++i) if(lcd->inputEnabled(i)) slots[0][size_t(i)]->onClick();
    f.stopAll(); f.command(0,C::clear);
    for(size_t n=0;n<4096;++n) f.inputL[n]=f.inputR[n]=.15f*std::sin(float(n)*juce::MathConstants<float>::twoPi/8);
    f.command(0,C::primary); f.render(4096); f.command(0,C::primary); f.mic(0);
    const auto configure=[&](int bank,int cutoff,bool distort)
    {
        chooseBank(1,bank); slots[1][0]->onClick(); set(0,cutoff); enter->onClick(); set(0,1); exit->onClick();
        slots[1][2]->onClick(); set(0,80); enter->onClick(); set(0,1); exit->onClick(); exit->onClick();
        if(!lcd->trackEnabled(0)) slots[1][0]->onClick();
        if(lcd->trackEnabled(2)!=distort) slots[1][2]->onClick();
    };
    configure(0,0,false); configure(1,100,false); configure(2,100,true);
    std::array<std::array<float,4096>,3> sound{};
    for(int bank=0;bank<3;++bank) {chooseBank(1,bank);f.render(24000);sound[size_t(bank)]=f.left;}
    double low=0,high=0,delta=0;
    for(size_t n=0;n<4096;++n) {low+=sound[0][n]*sound[0][n];high+=sound[1][n]*sound[1][n];delta+=std::abs(sound[2][n]-sound[1][n]);}
    require(high>.1 && low<high*.01 && delta>1,"TRACK bank switch did not restore real LPF/DIST processing");
    require(f.audio.trackSnapshot(0).frames==4096 && f.audio.trackSnapshot(0).state==LoopTrack::State::playing,"bank switching changed recorded phrase");
    std::cout<<"PASS 6I.1: mouse EDIT short/hold/drag/cancel, 16 slots per section, independent banks/values/ON-OFF/INSERT/FX TARGET, OFF/RED/PINK/BLUE, LCD IDs, loop continuity and real LPF/DIST bank audio\n";
}

// Stage 6I: operate the actual panel callbacks and the existing production audio path.
void testFxKnobs6I()
{
    using Group=EffectGroup;
    using Mode=FxKnobMode;
    using C=LoopTrack::Command;
    int value=0;
    FxKnobPickup pickup;
    pickup.bind(0,80,100,Mode::hook);
    require(!pickup.move(.2,value) && !pickup.move(.79,value),"HOOK wrote before pickup");
    require(pickup.move(.81,value) && value==81,"HOOK failed upward crossing");
    pickup.bind(0,81,100,Mode::hook);
    require(pickup.move(.7,value) && value==70,"HOOK lost its own capture");
    pickup.bind(0,20,100,Mode::hook);
    require(!pickup.move(.3,value) && pickup.move(.19,value),"external edit/downward pickup");
    pickup.bind(100,80,100,Mode::hook);
    require(!pickup.move(.2,value),"new bank retained capture");
    pickup.bind(101,80,100,Mode::immediate);
    require(pickup.move(.21,value) && value==21,"IMMEDIATE failed to apply physical position");
    pickup.bind(102,11,11,Mode::hook);
    require(!pickup.move(.5,value) && pickup.move(1,value) && value==11,"discrete pickup/range");
    require(!pickup.move(1,value),"stationary knob wrote a parameter");

    FiveTracks f; MainComponent panel(f.audio);
    TopPanelComponent* top=nullptr; MemoryDisplay* display=nullptr;
    std::array<ParameterKnob*,4> small{};
    PerformanceFxKnob *input=nullptr,*track=nullptr;
    juce::Button *edit=nullptr,*trackEdit=nullptr,*next=nullptr,*back=nullptr,*exit=nullptr,*menu=nullptr,*enter=nullptr;
    std::array<juce::Button*,4> trackButtons{};
    for(auto* c:panel.getChildren()) if(auto* t=dynamic_cast<TopPanelComponent*>(c)) top=t;
    require(top!=nullptr,"6I top panel missing");
    for(auto* c:top->getChildren())
    {
        if(auto* d=dynamic_cast<MemoryDisplay*>(c)) display=d;
        if(auto* k=dynamic_cast<ParameterKnob*>(c))
            for(int i=0;i<4;++i) if(k->getName()=="Parameter knob "+juce::String(i+1)) small[size_t(i)]=k;
        if(auto* k=dynamic_cast<PerformanceFxKnob*>(c))
        {if(k->getName()=="INPUT FX KNOB") input=k; else if(k->getName()=="TRACK FX KNOB") track=k;}
        if(auto* b=dynamic_cast<juce::Button*>(c))
        {
            if(b->getTitle()=="INPUT FX EDIT") edit=b;
            if(b->getTitle()=="TRACK FX EDIT") trackEdit=b;
            if(b->getButtonText()=="Next") next=b;
            if(b->getButtonText()=="Previous") back=b;
            if(b->getButtonText()=="EXIT") exit=b;
            if(b->getButtonText()=="MENU") menu=b;
            if(b->getButtonText()=="ENTER") enter=b;
            for(int i=0;i<4;++i) if(b->getTitle()=="TRACK FX "+juce::String::charToString(char('A'+i))) trackButtons[size_t(i)]=b;
        }
    }
    require(display&&small[3]&&input&&track&&edit&&trackEdit&&next&&back&&exit&&menu&&enter&&trackButtons[3],"6I controls missing");
    const auto set=[&](int knob,int v){small[size_t(knob)]->setValue(v,juce::sendNotificationSync);};
    const auto first=[&]{for(int i=0;i<12;++i)back->onClick();};
    const auto last=[&]{for(int i=0;i<12;++i)next->onClick();};
    const auto move=[](PerformanceFxKnob* k,double v){k->setValue(v,juce::sendNotificationSync);};
    const auto screenshot=[&](const char* name,juce::Component& c,float scale)
    {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("tmp");folder.createDirectory();
        juce::FileOutputStream out(folder.getChildFile(name));out.setPosition(0);out.truncate();juce::PNGImageFormat png;
        require(png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds(),true,scale),out),"6I preview");
    };
    require(display->fxKnobMode()==Mode::immediate && !input->isEnabled() && track->isEnabled(),"defaults or fake INPUT DSP");
    f.mic(.1f);f.command(0,C::primary);f.render(4096);f.command(0,C::primary);
    edit->onClick();
    for(int bank=0;bank<4;++bank)
    {
        last();set(0,bank);
        for(int slot=0;slot<4;++slot)
        {
            top->activateInputSlot(slot);first();set(0,5);last();set(2,slot);
            const int balance=15+bank*20+slot*3;
            move(input,balance*.01);
            require(display->inputOscBot(slot).balance==balance && display->fxTarget(Group::input)==slot,"bank/slot target did not write its own OSC");
            f.render(256);
            require(f.audio.trackSnapshot(0).frames==4096 && f.audio.trackSnapshot(0).state==LoopTrack::State::playing,"bank/target changed loop");
        }
    }
    for(int bank=0;bank<4;++bank)
    {
        last();set(0,bank);
        require(display->fxTarget(Group::input)==3,"FX TARGET not stored by bank");
        for(int slot=0;slot<4;++slot) require(display->inputOscBot(slot).balance==15+bank*20+slot*3,"bank/slot values overwritten");
    }
    last();set(0,0);set(2,0);
    // LCD slot selection must not become FX TARGET, or change SINGLE/MULTI/gates.
    exit->onClick();top->activateInputSlot(0);top->activateInputSlot(1);
    require(display->inputEnabled(0)&&display->inputEnabled(1),"MULTI setup");
    edit->onClick();top->activateInputSlot(1);last();set(2,3);
    move(input,.61);
    require(display->inputOscBot(3).balance==61 && display->inputOscBot(1).balance==18 && display->inputEnabled(0)&&display->inputEnabled(1),"target changed editing slot, gates or other values");
    set(1,0);require(!display->inputMulti() && display->inputEnabled(1)&&!display->inputEnabled(0),"SINGLE lost semantics");
    move(input,.62);require(!display->inputEnabled(3),"performance knob enabled an effect");
    set(1,1);set(2,0);
    for(int slot=0;slot<4;++slot) display->updateEffect(Group::input,slot,false);
    top->publishInputFx();top->activateInputSlot(0);first();
    // Exact official mappings share LCD storage, including all discrete ROBOT notes.
    const int types[]={4,5,6,7,8};
    const int page[]={0,1,1,0,0}, encoder[]={1,2,3,3,1};
    const char* names[]={"NOTE","BALANCE","E.LEVEL","DEPTH","BALANCE"};
    for(int i=0;i<5;++i)
    {
        first();set(0,types[i]);
        move(input,.0);move(input,1.0);
        const auto mapped=display->performanceField(Group::input);
        require(mapped.name==names[i] && mapped.value==mapped.maximum,"official input mapping wrong");
        for(int n=0;n<page[i];++n)next->onClick();
        require(display->getEditorField(encoder[i]).value==mapped.maximum,"large/LCD storage diverged");
        set(encoder[i],i==0?3:80);
        require(std::abs(input->effectivePosition()-(i==0?3./11:.8))<1.e-9 && input->getValue()==1,"LCD edit moved physical knob");
    }
    // HOOK is shared SETUP state; LCD edits re-arm pickup without moving the physical control.
    first();set(0,5);next->onClick();move(input,.2);set(2,80);
    menu->onClick();next->onClick();require(display->getEditorField(0).name=="FX KNOB MODE","SETUP mode missing");set(0,1);
    screenshot("fx-knob-mode-6i.png",*display,4);
    require(display->fxKnobMode()==Mode::hook && input->waiting()&&track->waiting(),"shared HOOK mode");
    int publications=0;
    const auto publishOsc=top->onInputOscBotChanged;
    top->onInputOscBotChanged=[&](int slot,OscBotParameters parameters){++publications;publishOsc(slot,parameters);};
    exit->onClick();move(input,.3);
    require(publications==0,"unlatched HOOK published DSP parameters");
    require(display->inputOscBot(0).balance==80 && display->getTitle().contains("HOOK WAIT"),"HOOK premature write/readout");
    screenshot("fx-knob-hook-6i.png",*display,4);
    screenshot("fx-knobs-panel-6i.png",panel,1.5f);
    move(input,.79);require(display->inputOscBot(0).balance==80,"HOOK below crossing");
    move(input,.81);require(display->inputOscBot(0).balance==81&&!input->waiting(),"HOOK upward crossing failed");
    move(input,.7);require(display->inputOscBot(0).balance==70,"captured HOOK did not follow");
    set(2,20);move(input,.3);require(display->inputOscBot(0).balance==20&&input->waiting(),"LCD edit did not re-arm");
    move(input,.19);require(display->inputOscBot(0).balance==19,"downward capture failed");
    last();set(2,1);move(input,.25);require(display->inputOscBot(1).balance==18&&input->waiting(),"target change retained pickup");
    set(0,1);move(input,.3);require(display->inputOscBot(3).balance==44&&input->waiting(),"bank change retained pickup");
    set(0,0);set(2,0);first();set(0,4);move(input,.4);
    require(display->inputRobot(0).note==3 && input->waiting(),"type change retained pickup");
    menu->onClick();next->onClick();set(0,0);exit->onClick();
    // Mouse wheel is one legal value (one semitone), independent of wheel event amplitude.
    move(input,0);
    auto event=[&](juce::Component* c,juce::Point<float> at,juce::Point<float> down,juce::ModifierKeys mods)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,mods,1,0,0,0,0,c,c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,false);};
    juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;
    for(int note=1;note<=11;++note)
    {input->mouseWheelMove(event(input,{30,30},{30,30},{}),wheel);require(display->inputRobot(0).note==note,"discrete wheel skipped note");}
    input->mouseWheelMove(event(input,{30,30},{30,30},{}),wheel);require(display->inputRobot(0).note==11,"wheel exceeded B");
    first();set(0,5);move(input,.5);
    const auto down=event(input,{66,66},{66,66},juce::ModifierKeys::leftButtonModifier);
    input->mouseDown(down);require(display->inputOscBot(0).balance==50,"mouse down jumped");
    input->mouseDrag(event(input,{66,62},{66,66},juce::ModifierKeys::leftButtonModifier));
    input->mouseUp(event(input,{66,62},{66,66},{}));
    require(display->inputOscBot(0).balance>50 && display->inputOscBot(0).balance<=52,"drag not gradual");
    // Synthetic live input: BALANCE 0 is dry; 100 changes shape, not merely LCD/level.
    f.stopAll();f.audio.setMonitoringEnabled(true);f.audio.setMonitorVolume(.25f);
    display->updateEffect(Group::input,0,true);top->publishInputFx();f.mic(.15f);
    move(input,0);f.render(24000);std::array<float,4096> dry=f.left;
    move(input,1);f.render(24000);double difference=0,variation=0;
    for(size_t n=1;n<f.left.size();++n) {difference+=std::abs(f.left[n]-dry[n]);variation+=std::abs(f.left[n]-f.left[n-1]);}
    require(difference>5 && variation>.1,"INPUT knob did not change waveform in production DSP");
    // FX SEQUENCE remains running when BALANCE is moved; never writes NOTE or resets phase.
    first();next->onClick();next->onClick();set(0,1);next->onClick();set(0,102);set(1,2);next->onClick();set(0,60);set(1,67);
    f.render(200000);const auto q=display->inputSequence(0);const auto manualNote=display->inputOscBot(0).note;
    require(AudioSystemTestAccess::sequenceNote(f.audio,0)==67,"sequence did not reach second step");
    move(input,.8);f.render(256);
    require(AudioSystemTestAccess::sequenceNote(f.audio,0)==67 && display->inputSequence(0).notes==q.notes
            && display->inputOscBot(0).note==manualNote,"performance knob reset sequence or modified NOTE");
    // Capture with knob-controlled INPUT processing, replay with INPUT OFF.
    f.command(2,C::primary);f.render(4096);f.command(2,C::primary);
    display->updateEffect(Group::input,0,false);top->publishInputFx();f.audio.setMonitoringEnabled(false);f.mic(0);f.render(24000);
    double replay=0;for(float x:f.left)replay+=std::abs(x);
    require(replay>.1 && f.audio.trackSnapshot(2).frames==4096,"processed input not recorded");
    f.command(2,C::primary);f.render(4096);f.command(2,C::primary);f.audio.selectTrack(2);require(f.audio.undoSelectedTrack(),"Undo rejected after performance capture");f.render(256);f.stopAll();

    // Re-record a periodic 6 kHz tone; LPF response must be measurable through actual loop routing.
    f.command(0,C::clear);for(size_t n=0;n<4096;++n) f.inputL[n]=f.inputR[n]=.15f*std::sin(float(n)*juce::MathConstants<float>::twoPi/8);
    f.command(0,C::primary);f.render(4096);f.command(0,C::primary);f.mic(0);
    exit->onClick();trackButtons[0]->onClick();trackEdit->onClick();
    enter->onClick();set(0,0);set(1,0);screenshot("fx-knob-target-6i.png",*display,4);
    const auto energy=[&]{double sum=0;for(float x:f.left)sum+=x*x;return sum/f.left.size();};
    move(track,1);f.render(24000);const auto high=energy();
    move(track,0);f.render(24000);const auto low=energy();
    require(high>1.e-4 && low<high*.01,"TRACK knob not connected to LPF audio");
    display->selectTrack(4);move(track,.3);require(display->getTrackFx().cutoff==30 && display->fxTarget(Group::track)==0 && f.audio.trackFxInsert(0)==0,"track selection rerouted FX target");
    const auto cutoff=display->getTrackFx().cutoff;
    set(1,1);require(!track->isEnabled(),"TRACK B silently redirected");set(1,3);require(!track->isEnabled(),"TRACK D silently redirected");
    set(1,2);move(track,.25);require(display->getTrackFx().drive==25&&display->getTrackFx().cutoff==cutoff,"C knob changed LPF");
    exit->onClick();exit->onClick(); // Slots select without toggling while editing (6I.1).
    trackButtons[0]->onClick();trackButtons[2]->onClick();
    move(track,0);f.render(24000);const auto soft=f.left;
    move(track,1);f.render(24000);double distDelta=0;for(size_t n=0;n<4096;++n)distDelta+=std::abs(f.left[n]-soft[n]);
    require(distDelta>1,"TRACK C DRIVE did not change production audio");
    require(f.audio.trackSnapshot(0).frames==4096,"performance changed loop length");
    // Concurrent GUI parameter/target/bank publications versus sole audio-thread DSP state.
    // The worker never touches UI; allocations measured only in the callback.
    edit->onClick();first();set(0,5);last();
    std::atomic<bool> finite{true},ready{false};
    std::thread audioThread([&]
    {
        ready=true;
        for(int block=0;block<1500;++block)
        {f.render(256);for(int n=0;n<256;++n)if(!std::isfinite(f.left[size_t(n)]))finite=false;}
    });
    while(!ready.load()) std::this_thread::yield();
    for(int n=0;n<300;++n)
    {set(0,n%4);set(2,(n/4)%4);move(input,(n%101)*.01);move(track,((n+30)%101)*.01);}
    audioThread.join();require(finite,"concurrent knob updates produced nonfinite audio");
    std::cout<<"PASS: 6I official mappings, 16 independent INPUT targets, shared IMMEDIATE/HOOK, external-edit pickup, mouse/wheel, LCD/DSP, live OSC waveform, LPF attenuation="<<low/high<<", DIST delta="<<distDelta<<", capture/Undo and concurrent publication\n";
}

void testStepSlicerIntegration()
{
    using C=LoopTrack::Command;
    for(double rate:{44100.,48000.})
    {
        FiveTracks f(rate);f.audio.setBpm(165);f.mic(.1f);
        f.audio.setMonitoringEnabled(true);f.audio.setMonitorVolume(.25f);
        OscBotParameters osc;osc.enabled=true;osc.balance=100;osc.note=60;
        f.audio.setInputOscBot(0,osc);osc.note=72;f.audio.setInputOscBot(1,osc);
        FxSequenceParameters seq;seq.enabled=true;seq.rate=102;seq.maximum=9;
        seq.notes={60,58,61,65,63,65,68,67,70,36,36,36,36,36,36,36};f.audio.setInputSequence(0,seq);
        seq.maximum=6;seq.notes={80,79,82,80,82,85,36,36,36,36,36,36,36,36,36,36};f.audio.setInputSequence(1,seq);
        StepSlicerParameters p;p.enabled=true;p.rate=112;p.depth=50;p.threshold=-15;
        f.audio.setInputStepSlicer(2,p);
        ReverbParameters reverb;reverb.enabled=true;reverb.preDelay=0;reverb.eLevel=64;reverb.density=4;
        f.audio.setInputReverb(3,reverb);f.audio.setInputActivation(15,0,3,true,0,4);
        f.render(48000);
        for(int i=0;i<5;++i) f.command(i,C::primary);
        f.render(4096);const auto capturedL=f.left,capturedR=f.right;
        double energy=0;for(float v:capturedL)energy+=v*v;require(energy>1.e-5,"four-FX chain silent");
        for(int i=0;i<5;++i) f.command(i,C::primary);
        f.stopAll(); f.audio.setMonitoringEnabled(false);
        reverb.enabled=false;f.audio.setInputReverb(3,reverb);f.audio.setInputActivation(0,0,3,false,0,4);f.mic(0);f.render(4096);
        for(int track=0;track<5;++track)
        {
            f.command(track,C::primary);f.render(8192);
            for(int n=512;n<3800;++n)
                require(std::abs(f.left[size_t(n)]-4*capturedL[size_t(n)])<2.e-5f &&
                        std::abs(f.right[size_t(n)]-4*capturedR[size_t(n)])<2.e-5f,"SLICER/REVERB not captured exactly or re-applied during playback");
            f.command(track,C::primary);f.mic(.1f);f.audio.setInputActivation(7,0,3,false,0,4);f.render(8192);
            f.command(track,C::primary);f.command(track,C::undo);
            f.command(track,C::stop);f.mic(0);f.audio.setInputActivation(0,0,3,false,0,4);
            f.command(track,C::primary);f.render(8192);
            for(int n=512;n<3800;++n) require(std::abs(f.left[size_t(n)]-4*capturedL[size_t(n)])<2.e-5f,"SLICER overdub Undo failed");
            f.command(track,C::stop);
        }
        f.playAll(); f.audio.setMonitoringEnabled(true); f.mic(.1f);
        f.audio.setInputActivation(15,0,3,true,0,4); reverb.enabled=true;f.audio.setInputReverb(3,reverb);
        TrackFxParameters trackFx;trackFx.enabled=true;trackFx.distortionEnabled=true;
        f.audio.setTrackEffects(trackFx);f.render(8192);
        std::array<double,200> timings{};double total=0;
        for(auto& t:timings)
        {
            const auto start=std::chrono::steady_clock::now();f.render(256);
            t=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();total+=t;
            for(int n=0;n<256;++n) require(std::isfinite(f.left[size_t(n)])&&std::abs(f.left[size_t(n)])<.98f,"unsafe loaded mix");
        }
        std::sort(timings.begin(),timings.end());
        std::cout<<"STEP SLICER + 2 OSC/SEQ + REVERB + 5 loops + TRACK FX rate="<<rate<<" meanMs="<<total/200<<" p99Ms="<<timings[197]<<" maxMs="<<timings.back()<<" deadlineMs="<<256000/rate<<'\n';
        f.stopAll();f.audio.setInputActivation(8,0,3,false,0,4);f.mic(0);f.render(2048);
        double tail=0;for(int n=1000;n<2048;++n)tail+=double(f.left[size_t(n)])*f.left[size_t(n)];
        require(tail>1.e-10,"D reverb tail cut when sources/slicer disabled");
    }
    // Distinguish real rhythmic changes from LCD-only state; each bank/slot is independent.
    FiveTracks f;f.mic(.2f);f.audio.setMonitoringEnabled(true);f.audio.setMonitorVolume(.25f);
    StepSlicerParameters p;p.enabled=true;p.depth=100;p.threshold=0;p.gain=0;p.rate=100;p.maximum=2;p.level[1]=0;
    for(int bank=0;bank<4;++bank) for(int slot=0;slot<4;++slot)
    {
        f.audio.setInputStepSlicer(slot,p,bank);f.audio.setInputActivation(1u<<slot,0,0,false,bank,1u<<slot);f.render(8192);
        double low=1,high=0;for(float v:f.left){low=std::min(low,double(v));high=std::max(high,double(v));}
        require(low==0 && high>.049,"bank/slot slicer not routed");
    }
    f.audio.setInputActivation(8,0,0,false,0,8);f.render(2048);
    const double old=AudioSystemTestAccess::slicerPosition(f.audio,3,3);f.render(1024);
    require(AudioSystemTestAccess::slicerPosition(f.audio,3,3)==old,"inactive bank advanced");
    f.audio.setInputActivation(0,0,0,false,0,8);f.render(4096);f.near(.05f,"SLICER bypass not original microphone");
    std::cout<<"PASS: STEP SLICER four-FX chain, capture/playback/overdub/Undo five tracks, tails, all banks/slots and callback routing\n";
}

void testG2BIntegration()
{
    using C=LoopTrack::Command;
    for(double rate:{44100.,48000.})
    {
        FiveTracks f(rate);f.audio.setBpm(165);
        f.audio.setMonitoringEnabled(true);f.audio.setMonitorVolume(1.f);
        for(int n=0;n<4096;++n){const float v=float(.3*std::sin(6.283185307179586*220.*n/rate));f.inputL[size_t(n)]=v;f.inputR[size_t(n)]=v;}
        // Bank B slot A is the We Drive Alone layer: G2B, MODE 2, BALANCE 100. Bank A keeps OSC BOT.
        G2BParameters g;g.enabled=true;g.balance=100;g.mode=2;
        OscBotParameters osc;osc.enabled=true;osc.balance=100;osc.note=60;f.audio.setInputOscBot(0,osc,0);
        f.audio.setInputG2B(0,g,1);
        auto energy=[&](int from,int to){double e=0;for(int n=from;n<to;++n)e+=double(f.left[size_t(n)])*f.left[size_t(n)];return e;};
        auto lowRatio=[&](){const double w=6.283185307179586;double a=0,b=0,c=0,d=0;for(int n=1000;n<4096;++n){const double x=f.left[size_t(n)];a+=x*std::cos(w*110*n/rate);b+=x*std::sin(w*110*n/rate);c+=x*std::cos(w*220*n/rate);d+=x*std::sin(w*220*n/rate);}return std::sqrt(a*a+b*b)/std::max(1.e-9,std::sqrt(c*c+d*d));};
        f.audio.setInputActivation(0,0,0,false,1,0,0);f.render(48000);f.render(4096);
        const double offRatio=lowRatio();require(offRatio<.1,"G2B off still has 110 Hz");
        f.audio.setInputActivation(1,0,0,false,1,0,1);f.render(48000);f.render(4096);
        const double onRatio=lowRatio();require(onRatio>5,"G2B on did not move 220 Hz down");
        require(energy(1000,4096)>1.e-3,"G2B silent");
        // Record the G2B layer on all five tracks, then play back with G2B off: it is baked in, not retroactive.
        for(int track=0;track<5;++track)
        {
            f.audio.setInputActivation(1,0,0,false,1,0,1);f.render(4096);
            f.command(track,C::primary);f.render(24000);f.command(track,C::primary);
            f.audio.setMonitoringEnabled(false);f.audio.setInputActivation(0,0,0,false,1,0,0);f.render(4096);
            f.render(8192);require(energy(1000,4096)>1.e-4,"G2B layer not recorded on track");
            f.command(track,C::stop);f.audio.setMonitoringEnabled(true);
        }
        // Existing banks are untouched: bank A still produces the OSC BOT type, bank B has no OSC BOT.
        f.audio.setInputActivation(1,0,1,false,0,0,0);f.render(8192);require(energy(1000,4096)>1.e-4,"bank A OSC BOT lost");
        std::cout<<"G2B integration rate="<<rate<<" 110/220 off="<<offRatio<<" on="<<onRatio<<'\n';
    }
    std::cout<<"PASS: G2B INPUT FX recorded on five tracks, banks preserved, not applied retroactively\n";
}


// Deterministic transport measurement: unique stereo samples cross arbitrary
// callback and 256-frame scratch boundaries. Compare every playback sample,
// including the documented non-destructive edge/transport ramps.
void testCaptureAlignment6H4()
{
    using C=LoopTrack::Command;
    for(double rate:{44100.,48000.}) for(int block:{17,257,1024}) for(bool wet:{false,true})
    {
        FiveTracks f(rate); f.audio.setMonitoringEnabled(true); f.audio.setMonitorVolume(.25f);
        G2BParameters g;g.enabled=true;g.balance=100;g.mode=2;
        f.audio.setInputG2B(2,g,2);f.audio.setInputActivation(wet?4u:0u,0,0,false,2,0,4);
        f.render(4096);
        constexpr int length=10007;
        std::array<std::vector<float>,2> recorded {std::vector<float>(length),std::vector<float>(length)};
        for(int t=0;t<5;++t) f.command(t,C::primary);
        for(int offset=0;offset<length;)
        {
            const int n=std::min(block,length-offset);
            for(int j=0;j<n;++j)
            {
                const double time=(offset+j)/rate;
                f.inputL[size_t(j)]=float(.12*std::sin(6.283185307179586*(147*time+30*time*time)));
                f.inputR[size_t(j)]=float(.07*std::cos(6.283185307179586*(191*time+13*time*time)));
            }
            f.render(n);
            for(int j=0;j<n;++j)
            {
                recorded[0][size_t(offset+j)]=4*f.left[size_t(j)];recorded[1][size_t(offset+j)]=4*f.right[size_t(j)];
                if(!wet) require(std::abs(4*f.left[size_t(j)]-f.inputL[size_t(j)])<1.e-7f
                                 &&std::abs(4*f.right[size_t(j)]-f.inputR[size_t(j)])<1.e-7f,"dry monitor duplicates/delays input");
            }
            offset+=n;
        }
        for(int t=0;t<5;++t) {f.command(t,C::primary);require(f.audio.trackSnapshot(t).frames==length,"capture duration changed");}
        f.stopAll();f.audio.setMonitoringEnabled(false);f.audio.setInputActivation(0,0,0,false,2,0,4);f.mic(0);f.render(4096);
        for(int t=0;t<5;++t)
        {
            f.command(t,C::primary);
            for(int offset=0;offset<2*length;)
            {
                const int n=std::min(block,2*length-offset);f.render(n);
                for(int j=0;j<n;++j)
                {
                    const int total=offset+j,index=total%length;
                    const float edge=std::min(1.f,float(std::min(index,length-1-index))/float(juce::roundToInt(.003*rate)));
                    // JUCE SmoothedValue uses floor for ramp sample counts.
                    const float fade=std::min(1.f,float(total+1)/float(int(.005*rate)));
                    require(std::abs(f.left[size_t(j)]-recorded[0][size_t(index)]*edge*fade)<2.e-5f
                            &&std::abs(f.right[size_t(j)]-recorded[1][size_t(index)]*edge*fade)<2.e-5f,
                            "capture/playback sample mismatch, extra delay, missing/duplicated samples or dry/wet duplication");
                }
                offset+=n;
            }
            require(f.audio.trackSnapshot(t).progress<1.e-6f,"loop drift after two repetitions");
            f.command(t,C::stop);f.render(512);
        }
    }
    FiveTracks f;f.audio.setMonitoringEnabled(true);f.audio.setMonitorVolume(.25f);
    for(int n=0;n<4096;++n) f.inputL[size_t(n)]=f.inputR[size_t(n)]=float(.2*std::sin(6.283185307179586*220*n/48000));
    for(int bank=0;bank<4;++bank) for(int slot=0;slot<4;++slot)
    {
        G2BParameters g;g.enabled=true;g.balance=100;g.mode=2;f.audio.setInputG2B(slot,g,bank);
        f.audio.setInputActivation(1u<<slot,0,0,false,bank,0,1u<<slot);f.render(24000);f.render(4096);
        double difference=0,energy=0;
        for(int n=1000;n<4000;++n){const double y=4*f.left[size_t(n)];difference+=std::pow(y-f.inputL[size_t(n)],2);energy+=y*y;}
        require(energy>.1&&difference>energy*.5,"G2B bank/slot not processing");
        f.audio.setInputActivation(0,0,0,false,bank,0,1u<<slot);f.render(4096);
        for(int n=1000;n<4000;++n) require(std::abs(4*f.left[size_t(n)]-f.inputL[size_t(n)])<1.e-6f,"G2B OFF not dry in bank/slot");
    }
    std::cout<<"PASS 6H4: exact stereo capture/playback 10007 frames, 2 wraps, 5 tracks, dry/G2B, 44.1/48k, blocks 17/257/1024; G2B all 16 bank/slot routes\n";
}


void testIndicators6H4()
{
    using S=LoopTrack::State;using C=LoopTrack::Command;using M=IndicatorMode;
    TrackIndicatorReading reading;
    for(auto mode:{M::status,M::loopPosition,M::level}) require(indicatorFill(mode,reading,1)==0,"empty indicator");
    reading.transport={S::recording,123,false,0,false,false};
    require(indicatorFill(M::status,reading,4.1)==1 && indicatorFill(M::status,reading,4.6)==0,"REC status tempo pulse");
    require(indicatorFill(M::loopPosition,reading,4.1)==0 && indicatorFill(M::level,reading,4.1)==0,"initial REC invented progress/level");
    reading.transport={S::stopped,123,false,0,false,false};
    require(indicatorFill(M::status,reading,4.1)==1 && indicatorFill(M::loopPosition,reading,4.1)==0,"STOP indicator modes");
    reading.transport.state=S::overdubbing;reading.transport.progress=.75f;reading.level={.1f,.05f};
    require(indicatorFill(M::loopPosition,reading,4.1)==.75f && indicatorFill(M::status,reading,4.6)==0,"DUB actual progress/tempo pulse");
    reading.transport.state=S::playing;
    require(indicatorFill(M::status,reading,5)==.25f,"STATUS is one measure, not loop fraction");
    require(indicatorFill(M::level,reading,5)>.6f,"LEVEL amplitude mapping");

    FiveTracks f(48000,600000);
    const std::array<int,5> lengths{144000,288000,576000,192000,96000}; // 3,6,12,4,2 seconds.
    const std::array<float,5> amplitudes{.30f,.03f,0.f,.10f,.05f};
    TrackIndicatorFrame frame;
    const auto snapshot=[&]() { require(f.audio.readIndicators(frame),"uncontended snapshot unavailable"); };
    for(int t=0;t<5;++t)
    {
        f.mic(amplitudes[size_t(t)]);f.command(t,C::primary);f.render(lengths[size_t(t)]);
        snapshot();require(frame.tracks[size_t(t)].transport.state==S::recording,"REC publication");
        f.command(t,C::primary);f.command(t,C::stop);f.render(512);
        snapshot();require(frame.tracks[size_t(t)].transport.frames==lengths[size_t(t)]&&frame.tracks[size_t(t)].transport.state==S::stopped,"stopped phrase publication");
    }
    f.mic(0);f.playAll();f.render(72000);snapshot();
    for(size_t t=0;t<5;++t)
    {
        require(std::abs(frame.tracks[t].transport.progress-72000.f/lengths[t])<2.e-6f,"3/6/12 second independent progress");
        require(std::abs(frame.tracks[t].level[0]-amplitudes[t])<1.e-5f,"track meter cross-talk or gain mismatch");
        require(frame.tracks[t].transport.state==S::playing,"simultaneous PLAY state");
    }
    require(indicatorFill(M::level,frame.tracks[0],frame.beats)>indicatorFill(M::level,frame.tracks[1],frame.beats)
            &&indicatorFill(M::level,frame.tracks[2],frame.beats)==0,"strong/weak/silent LEVEL");
    require(std::abs(frame.beats-f.audio.musicalPosition().beats)<1.e-10,"snapshot clock not callback clock");
    f.audio.setBpm(90);f.render(12345);snapshot();
    for(size_t t=0;t<5;++t) require(std::abs(frame.tracks[t].transport.progress-84345.f/lengths[t])<2.e-6f,"BPM changed loop indicator speed");
    f.audio.setTrackVolume(0,0);f.render(48000);snapshot();
    require(frame.tracks[0].level[0]<.001f &&frame.tracks[1].level[0]>.029f,"fader level release or independent meters");
    f.command(1,C::primary);snapshot();require(frame.tracks[1].transport.state==S::overdubbing,"DUB snapshot");
    const auto position=frame.tracks[1].transport.progress;
    f.render(1024);f.command(1,C::primary);f.command(1,C::undo);snapshot();
    require(frame.tracks[1].transport.state==S::playing && frame.tracks[1].transport.progress>position,"DUB/Undo cursor discontinuity");
    f.command(1,C::stop);snapshot();
    require(frame.tracks[1].transport.progress==0&&frame.tracks[1].level[0]==0,"STOP retains fictitious movement/level");
    f.command(1,C::clear);snapshot();require(frame.tracks[1].transport.state==S::empty&&frame.tracks[1].transport.frames==0,"clear snapshot");
    f.stopAll();f.command(0,C::primary);f.render(lengths[0]);snapshot();require(frame.tracks[0].transport.progress<1.e-6f,"LED wrap not actual loop wrap");

    // UI route and return page: knob must not modify hidden FX or tempo.
    MainComponent panel(f.audio);TopPanelComponent* top=nullptr;MemoryDisplay* display=nullptr;
    juce::Button *menu=nullptr,*exit=nullptr,*inputEdit=nullptr;ParameterKnob* first=nullptr;
    for(auto* child:panel.getChildren()) if(auto* candidate=dynamic_cast<TopPanelComponent*>(child)) top=candidate;
    for(auto* child:top->getChildren())
    {
        if(auto* d=dynamic_cast<MemoryDisplay*>(child)) display=d;
        if(auto* k=dynamic_cast<ParameterKnob*>(child)) if(k->getName()=="Parameter knob 1") first=k;
        if(auto* b=dynamic_cast<juce::Button*>(child))
        {if(b->getButtonText()=="MENU") menu=b;if(b->getButtonText()=="EXIT") exit=b;if(b->getTitle()=="INPUT FX EDIT") inputEdit=b;}
    }
    require(menu&&exit&&first&&display&&inputEdit,"indicator UI controls missing");
    inputEdit->onClick();const auto previous=display->getEditorField(0).value;
    menu->onClick();require(display->isIndicatorEditing()&&first->isEnabled(),"MENU did not open INDICATOR");
    for(int mode=0;mode<3;++mode)
    {first->setValue(mode,juce::sendNotificationSync);require(int(top->indicatorMode())==mode,"indicator encoder");}
    first->setValue(1,juce::sendNotificationSync);
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("tmp");folder.createDirectory();
    {juce::FileOutputStream out(folder.getChildFile("indicator-lcd-6h4.png"));out.setPosition(0);out.truncate();juce::PNGImageFormat png;
     require(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(),true,4),out),"indicator LCD preview");}
    exit->onClick();require(!display->isIndicatorEditing()&&display->isInputEditing()&&display->getEditorField(0).value==previous,"indicator EXIT lost editor or changed type");
    exit->onClick();f.audio.setTrackVolume(0,1);f.playAll();f.render(30000);
    // Timer copies the last audio publication; it cannot advance it without audio.
    juce::Thread::sleep(40);juce::Timer::callPendingTimersSynchronously();snapshot();
    std::array<float,5> fills{};size_t index=0;
    for(auto* child:panel.getChildren()) if(auto* track=dynamic_cast<TrackComponent*>(child))
        for(auto* button:track->getChildren()) if(auto* ring=dynamic_cast<LedRingButton*>(button))
        {fills[index]=ring->getIndicatorFill();require(std::abs(fills[index]-indicatorFill(M::loopPosition,frame.tracks[index],frame.beats))<1.e-6f,"ring did not use audio snapshot");++index;}
    require(index==5,"five rings missing");
    {juce::FileOutputStream out(folder.getChildFile("indicators-panel-6h4.png"));out.setPosition(0);out.truncate();juce::PNGImageFormat png;
     require(png.writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds(),true,1.5f),out),"indicator panel preview");}


    // Quantized end and synchronized start remain audio boundaries, including a
    // boundary reached in the middle of a driver callback.
    {
        FiveTracks q; q.mic(.1f);q.audio.setBpm(120);q.audio.setQuantizeEnabled(true);
        q.command(0,C::primary);q.render(12000);q.command(0,C::primary);
        TrackIndicatorFrame value;require(q.audio.readIndicators(value),"Q snapshot");
        require(value.tracks[0].transport.state==S::recording && value.tracks[0].transport.awaitingBoundary
                &&indicatorFill(M::loopPosition,value.tracks[0],value.beats)==0,"pending Q indicator prematurely PLAY");
        q.render(84000+513);q.audio.readIndicators(value);
        require(value.tracks[0].transport.frames==96000&&std::abs(value.tracks[0].transport.progress-513.f/96000)<1.e-6f,"quantized mid-block LED phase");
        q.command(0,C::stop);q.command(0,C::clear);q.audio.setAllLoopSync(true);
        q.command(0,C::primary);q.render(240000);q.command(0,C::primary);q.render(192000);q.audio.readIndicators(value);
        const auto live=q.audio.trackSnapshot(0);
        require(value.tracks[0].transport.state==S::playing&&value.tracks[0].transport.frames==live.frames
                &&value.tracks[0].transport.progress==live.progress,"LOOP SYNC indicator disagrees with actual cursor");
    }
    // Meter includes individual insert but not another track's signal or the ALL sum.
    {
        TrackFxRouting routing;TrackFxParameters p;p.enabled=true;p.cutoff=0;p.resonance=0;
        routing.setParameters(p);routing.prepare(48000);routing.beginBlock();
        std::array<float,5> gains{1,1,1,1,1};std::array<std::array<float,2>,5> measured{};
        double filtered=0,clean=0;
        for(int n=0;n<4800;++n)
        {
            const float x=float(.1*std::sin(6.283185307179586*5000*n/48000));
            std::array<float,5> input{x,x,0,0,0};float l=0,r=0;
            routing.processFrame(input,input,l,r,gains,&measured);
            if(n>2400){filtered+=measured[0][0];clean+=measured[1][0];}
            require(measured[2][0]==0,"meter copied common bus into silent track");
        }
        require(filtered<clean*.2,"meter ignores individual LPF insert");
    }
    // Independent reader/writer stress: every field encodes the same generation.
    TrackIndicatorMailbox mailbox;std::atomic<bool> done{false};std::atomic<int> accepted{0};std::atomic<bool> coherent{true};
    std::thread reader([&]
    {
        TrackIndicatorFrame value;
        while(!done.load()) if(mailbox.read(value))
        {
            const int n=int(value.beats);const float level=float(n%100)/100.f;
            for(const auto& track:value.tracks)
                if(track.transport.frames!=n||track.level[0]!=level||track.level[1]!=level||track.transport.progress!=level) coherent=false;
            ++accepted;
        }
    });
    for(int n=1;n<=50000;++n)
    {
        TrackIndicatorFrame value;value.beats=n;
        for(auto& track:value.tracks){track.transport.frames=n;track.transport.progress=float(n%100)/100.f;track.level.fill(track.transport.progress);}
        mailbox.publish(value);
    }
    done=true;reader.join();require(coherent&&accepted>0,"torn concurrent indicator publication");
    std::cout<<"PASS 6H4: STATUS/LOOP POSITION/LEVEL, 3/6/12 s and five actual loops, independent levels, silence/fader, STOP/clear/DUB/Undo/wrap/BPM, MENU/EXIT/rings and coherent atomic snapshots ("<<accepted<<" reads)\n";
}

void testStepSlicerPanel()
{
    FiveTracks f;MainComponent panel(f.audio);TopPanelComponent* top=nullptr;MemoryDisplay* display=nullptr;
    for(auto* c:panel.getChildren()) if(auto* t=dynamic_cast<TopPanelComponent*>(c))top=t;
    std::array<ParameterKnob*,4> knobs{};juce::Button *edit=nullptr,*next=nullptr,*exit=nullptr;
    for(auto* c:top->getChildren())
    {
        if(auto* d=dynamic_cast<MemoryDisplay*>(c))display=d;
        if(auto* k=dynamic_cast<ParameterKnob*>(c)) for(int i=0;i<4;++i)if(k->getName()=="Parameter knob "+juce::String(i+1))knobs[size_t(i)]=k;
        if(auto* b=dynamic_cast<juce::Button*>(c))
        {if(b->getTitle()=="INPUT FX EDIT")edit=b;if(b->getButtonText()=="Next")next=b;if(b->getButtonText()=="EXIT")exit=b;}
    }
    require(top&&display&&edit&&next&&exit&&knobs[3],"SLICER panel controls missing");
    panel.setSize(1200,660);
    const auto preview=[&](const char* name)
    {
        auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
        juce::FileOutputStream stream(file);stream.setPosition(0);stream.truncate();juce::PNGImageFormat png;
        require(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(),true,4),stream),"SLICER LCD render");
    };
    edit->onClick();top->activateInputSlot(2);knobs[0]->setValue(7,juce::sendNotificationSync); // Real knob callbacks -> AudioSystem.
    require(display->getEditorField(1).name=="RATE","SLICER controls not refreshed");
    knobs[1]->setValue(112,juce::sendNotificationSync);knobs[2]->setValue(9,juce::sendNotificationSync);knobs[3]->setValue(50,juce::sendNotificationSync);
    preview("mk2-step-slicer-general.png");
    next->onClick();knobs[0]->setValue(-15,juce::sendNotificationSync);knobs[1]->setValue(6,juce::sendNotificationSync);
    next->onClick();knobs[0]->setValue(25,juce::sendNotificationSync);knobs[1]->setValue(0,juce::sendNotificationSync);
    preview("mk2-step-slicer-steps.png");
    const auto saved=display->inputStepSlicer(2);require(saved.rate==112&&saved.maximum==9&&saved.threshold==-15&&saved.length[0]==25&&saved.level[0]==0,"encoder editing failed");
    for(int i=0;i<8;++i)next->onClick();require(display->getEditorField(0).name=="BANK","BANK page inaccessible");
    preview("mk2-step-slicer-bank.png");
    knobs[0]->setValue(1,juce::sendNotificationSync);require(display->inputBank()==1,"BANK encoder failed");f.render(2048);
    require(AudioSystemTestAccess::currentBank(f.audio)==1,"LCD BANK not connected to audio");
    knobs[0]->setValue(0,juce::sendNotificationSync);f.render(2048);require(display->inputStepSlicer(2).length[0]==25,"bank A settings lost");
    exit->onClick();top->activateInputSlot(2);f.render(256);
    require(display->inputEnabled(2) && ((AudioSystemTestAccess::consumedInput(f.audio)>>16)&4),"C gate not published");
    edit->onClick();top->activateInputSlot(3);require(display->inputEnabled(2),"edit selection toggled effect");exit->onClick();
    std::cout<<"PASS: STEP SLICER LCD encoders, BANK selector, EXIT and slot gates\n";
}

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

void testOscBotInputIntegration()
{
    using Command = LoopTrack::Command;
    for (double rate : { 44100., 48000. })
    {
        FiveTracks f(rate);
        OscBotParameters osc; osc.enabled = true; osc.attack = 0; osc.balance = 100; osc.note = 48;
        f.audio.setInputOscBot(2, osc);
        for (int n = 0; n < 4096; ++n)
        {
            f.inputL[size_t(n)] = .2f * std::sin(juce::MathConstants<float>::twoPi * 75.f * n / 4096.f);
            f.inputR[size_t(n)] = 0;
        }
        const auto dryCapture = f.inputL;
        f.render(8192);
        for (float x : f.left) require(x == 0, "OSC BOT enabled monitoring itself");
        f.audio.setMonitoringEnabled(true); f.audio.setMonitorVolume(.25f); f.render(8192);
        double delta = 0, energy = 0;
        for (int n = 512; n < 3800; ++n)
        { delta += std::abs(f.left[size_t(n)] - .25f * dryCapture[size_t(n)]); energy += std::abs(f.left[size_t(n)]); }
        require(delta > 5 && energy > 1, "OSC BOT not live without a loop");
        for (int i = 0; i < 5; ++i) require(!f.audio.trackSnapshot(i).hasPhrase(), "OSC BOT created a loop");
        f.audio.setTrackFxEnabled(1, false); f.render(4096);
        f.command(0, Command::primary); f.command(1, Command::primary); f.render(4096);
        const auto wetCapture = f.left;
        f.command(0, Command::primary); f.command(1, Command::primary); f.stopAll();
        f.audio.setMonitoringEnabled(false); osc.enabled = false; f.audio.setInputOscBot(2, osc); f.mic(0);
        f.command(0, Command::primary); f.render(8192);
        for (int n = 512; n < 3800; ++n)
            require(std::abs(f.left[size_t(n)] - 4 * wetCapture[size_t(n)]) < 1.e-5f && f.right[size_t(n)] == 0,
                "OSC BOT wet recording not retained with effect OFF / duplicate monitor path");
        // Add processed input, complete the overdub, then restore the recorded synth.
        osc.enabled = true; osc.note = 60; f.audio.setInputOscBot(2, osc);
        f.mic(.1f); f.command(0, Command::primary); f.render(8192); f.command(0, Command::primary);
        f.audio.selectTrack(0); require(f.audio.undoSelectedTrack(), "OSC BOT overdub Undo rejected");
        f.render(0); f.stopAll(); f.mic(0);
        osc.enabled = false; f.audio.setInputOscBot(2, osc);
        f.command(0, Command::primary); f.render(8192);
        for (int n = 512; n < 3800; ++n)
            require(std::abs(f.left[size_t(n)] - 4 * wetCapture[size_t(n)]) < 1.e-5f, "OSC BOT overdub/Undo changed original");
        f.stopAll(); f.command(1, Command::primary); f.render(8192);
        for (int n = 512; n < 3800; ++n)
            require(std::abs(f.left[size_t(n)] - dryCapture[size_t(n)]) < 1.e-5f, "OSC BOT ignored per-track FX OFF");
        f.stopAll(); f.inputL = dryCapture; f.inputR.fill(0);
        f.audio.setMonitoringEnabled(true); f.render(4096);
        for (int n = 1500; n < 3800; ++n)
            require(f.left[size_t(n)] == .25f * dryCapture[size_t(n)], "OSC BOT bypass changed dry monitor");
        osc.enabled = true; f.audio.setInputOscBot(2, osc); f.render(8192);
        f.render(4096, 0);
        for (float x : f.left) require(x == 0, "OSC BOT retained oscillator without input device");
        f.audio.selectDevices("Mic 2", "Output 2");
        require(!f.audio.isMonitoringEnabled() && f.audio.trackSnapshot(0).hasPhrase(), "OSC BOT reconnect lost settings/loop");
    }
    std::cout << "PASS: OSC BOT live monitoring, wet/dry capture, playback OFF, no double processing, overdub/Undo, stereo, missing input and reconnect at 44.1/48 kHz\n";
}



void testInputFxShortcuts()
{
    FiveTracks f;MainComponent panel(f.audio);TopPanelComponent* top=nullptr;
    for(auto* c:panel.getChildren()) if(auto* t=dynamic_cast<TopPanelComponent*>(c)) top=t;
    MemoryDisplay* display=nullptr;juce::Button *edit=nullptr,*exit=nullptr,*next=nullptr;
    std::array<juce::Button*,4> slots{};std::array<ParameterKnob*,4> knobs{};
    for(auto* c:top->getChildren())
    {
        if(auto* d=dynamic_cast<MemoryDisplay*>(c)) display=d;
        if(auto* b=dynamic_cast<juce::Button*>(c))
        {
            if(b->getTitle()=="INPUT FX EDIT") edit=b;
            if(b->getButtonText()=="EXIT") exit=b;
            if(b->getButtonText()=="Next") next=b;
            for(int i=0;i<4;++i) if(b->getTitle()=="INPUT FX "+juce::String::charToString(char('A'+i))) slots[size_t(i)]=b;
        }
        if(auto* k=dynamic_cast<ParameterKnob*>(c))
            for(int i=0;i<4;++i) if(k->getName()=="Parameter knob "+juce::String(i+1)) knobs[size_t(i)]=k;
    }
    require(display&&slots[3]&&knobs[3]&&edit&&next&&exit,"shortcut UI controls");
    edit->onClick();
    for(int i=0;i<4;++i) {slots[size_t(i)]->onClick();knobs[0]->setValue(5,juce::sendNotificationSync);}
    exit->onClick();
    InputFxShortcuts keys; keys.onPress=[&](int index){panel.inputFxShortcut(index);};
    const auto press=[&](int index){require(keys.keyPressed(juce::KeyPress(InputFxShortcuts::codes[size_t(index)]),knobs[0],true),"shortcut not handled");};
    const auto mask=[&]{unsigned m=0;for(int i=0;i<4;++i)if(display->inputEnabled(i))m|=1u<<i;return m;};
    for(int i=0;i<4;++i)
    {
        keys.updateDownKeys(0);press(i);f.render(1);
        require(mask()==(1u<<i)&&slots[size_t(i)]->getToggleState()&&AudioSystemTestAccess::oscEnabled(f.audio,i),"F6-F9 wrong slot/gate/LED");
        for(int repeat=0;repeat<5;++repeat) press(i);
        require(mask()==(1u<<i),"auto-repeat toggled effect");
        keys.updateDownKeys(0);press(i);f.render(1024);
        require(mask()==0&&!AudioSystemTestAccess::oscEnabled(f.audio,i),"individual OFF failed");
    }
    keys.updateDownKeys(0);press(4);f.render(1);
    require(mask()==3&&(AudioSystemTestAccess::consumedInput(f.audio)&15)==3,"F10 OFF/OFF did not commit BOTH");
    keys.updateDownKeys(0);press(4);f.render(1);require(mask()==0,"F10 ON/ON did not disable BOTH");
    keys.updateDownKeys(0);press(0);keys.updateDownKeys(0);press(4);f.render(1);require(mask()==3,"F10 partial activation failed");
    // The physical mouse path then keyboard operate on the same display model.
    slots[0]->setToggleState(false,juce::dontSendNotification);slots[0]->onClick();require(mask()==2,"mouse and keys diverged");
    keys.updateDownKeys(0);press(0);require(mask()==3,"keyboard after mouse diverged");
    juce::TextEditor text; keys.updateDownKeys(0);
    require(!keys.keyPressed(juce::KeyPress(juce::KeyPress::F10Key),&text,true)&&mask()==3,"typing intercepted");
    keys.updateDownKeys(0);require(!keys.keyPressed(juce::KeyPress(juce::KeyPress::F10Key),nullptr,false)&&mask()==3,"inactive shortcut");
    require(!keys.keyPressed(juce::KeyPress(juce::KeyPress::F5Key),nullptr,true),"F5 conflict");
    for(int code:{'L','Q','Z'}) require(!keys.keyPressed(juce::KeyPress(code),nullptr,true),"existing keyboard conflict");
    require(!keys.keyPressed(juce::KeyPress(juce::KeyPress::F6Key,juce::ModifierKeys::ctrlModifier,0),nullptr,true),"modified F6 consumed");
    keys.focusChanged(16);press(4);require(mask()==3,"held key on focus return retriggered");
    keys.updateDownKeys(0);press(4);require(mask()==0,"focus-release recovery failed");
    keys.updateDownKeys(0);press(2);keys.updateDownKeys(0);press(3);
    keys.updateDownKeys(0);press(4);require(mask()==15,"F10 changed C/D while enabling A/B");
    keys.updateDownKeys(0);press(4);require(mask()==12,"F10 changed C/D while disabling A/B");
    keys.updateDownKeys(0);press(2);keys.updateDownKeys(0);press(3);
    // Editing retains physical A-D semantics: select a slot without toggling it.
    edit->onClick();keys.updateDownKeys(0);press(1);
    require(display->selectedInputSlot()==1&&mask()==0,"editor shortcut not same as physical button");
    next->onClick();next->onClick();next->onClick();knobs[2]->setValue(0,juce::sendNotificationSync);exit->onClick();
    keys.updateDownKeys(0);press(0);keys.updateDownKeys(0);press(1);require(mask()==2,"SINGLE restriction lost");
    f.render(1);const auto before=AudioSystemTestAccess::consumedInput(f.audio);keys.updateDownKeys(0);press(4);f.render(1);
    require(mask()==2&&!display->inputMulti(),"F10 silently changed SINGLE");
    require((AudioSystemTestAccess::consumedInput(f.audio)>>16)==(before>>16),"SINGLE submitted group restart");
    std::cout<<"PASS: F6-F9/F10 local dispatch, auto-repeat, editable text/focus/modifiers, mouse parity, SINGLE/MULTI, editor selection; F5/L/Q/Undo keys unclaimed\n";
}

void testInputFxGroupAudio()
{
    const int a[]={60,58,61,65,63,65,68,67,70},b[]={80,79,82,80,82,85};
    for(bool sync:{false,true}) for(bool retrig:{false,true}) for(bool playback:{false,true})
    {
        FiveTracks f;f.audio.setBpm(165);f.mic(.15f);
        if(playback)
        { f.command(0,LoopTrack::Command::primary);f.render(220000);f.command(0,LoopTrack::Command::primary);f.render(150000); }
        OscBotParameters osc;osc.enabled=true;osc.balance=100;
        f.audio.setInputActivation(0,0,3,false);f.render(1);
        // Simulate preemption between the two legacy parameter setters: gates
        // MUST remain both off until the single group publication is consumed.
        f.audio.setInputOscBot(0,osc);f.render(1);
        require(!AudioSystemTestAccess::oscEnabled(f.audio,0)&&!AudioSystemTestAccess::oscEnabled(f.audio,1),"torn gate before group commit A");
        f.audio.setInputOscBot(1,osc);f.render(1);
        require(!AudioSystemTestAccess::oscEnabled(f.audio,0)&&!AudioSystemTestAccess::oscEnabled(f.audio,1),"torn gate before group commit B");
        for(int slot=0;slot<2;++slot)
        {
            FxSequenceParameters seq;seq.enabled=true;seq.sync=sync;seq.retrig=retrig;seq.rate=102;seq.maximum=slot==0?9:6;
            for(int n=0;n<seq.maximum;++n) seq.notes[size_t(n)]=slot==0?a[n]:b[n];
            f.audio.setInputSequence(slot,seq);
        }
        f.audio.setInputActivation(1,0,3,false);f.render(7500); // A already running.
        const double clockBefore=f.audio.musicalPosition().beats;
        f.audio.setInputActivation(3,0,3,true);f.render(1);
        require(AudioSystemTestAccess::oscEnabled(f.audio,0)&&AudioSystemTestAccess::oscEnabled(f.audio,1),"group did not enable on same boundary");
        const int alignedStep=sync && playback ? 1 : 0;
        require(AudioSystemTestAccess::sequenceNote(f.audio,0)==a[alignedStep]&&AudioSystemTestAccess::sequenceNote(f.audio,1)==b[alignedStep],"group missed VAL1/phrase-aligned step");
        require(std::abs(f.audio.musicalPosition().beats-clockBefore-165./(60*48000))<1.e-9,"group reset MusicalClock");
        f.audio.setInputActivation(0,0,3,false);f.render(1024);
        require(!AudioSystemTestAccess::oscEnabled(f.audio,0)&&!AudioSystemTestAccess::oscEnabled(f.audio,1),"group OFF not coherent");
        for(unsigned initial:{2u,0u})
        {
            f.audio.setInputActivation(initial,0,3,false);f.render(1);
            f.audio.setInputActivation(3,0,3,true);f.render(1);
            require(AudioSystemTestAccess::sequenceNote(f.audio,0)==a[alignedStep]&&AudioSystemTestAccess::sequenceNote(f.audio,1)==b[alignedStep],"B-only/OFF-OFF start not aligned");
        }
    }
    // Full independent 9/6-step patterns after an explicit group start, no loop.
    for(double rate:{44100.,48000.})
    {
        FiveTracks f(rate);f.audio.setBpm(165);f.mic(.1f);
        OscBotParameters osc;osc.enabled=true;osc.balance=100;
        for(int slot=0;slot<2;++slot)
        {
            f.audio.setInputOscBot(slot,osc);
            FxSequenceParameters seq;seq.enabled=true;seq.retrig=true;seq.rate=102;seq.maximum=slot==0?9:6;
            for(int n=0;n<seq.maximum;++n)seq.notes[size_t(n)]=slot==0?a[n]:b[n];
            f.audio.setInputSequence(slot,seq);
        }
        f.audio.setInputActivation(1,0,3,false);f.render(11001);
        f.audio.setInputActivation(3,0,3,true);
        const int total=int(std::ceil(rate*60/165*8*10));
        for(int position=0;position<total;)
        {
            const int frames=std::min(4096,total-position);f.render(frames);position+=frames;
            const int step=int(std::floor((position-1)*165./(rate*60*8)+1.e-8));
            require(AudioSystemTestAccess::sequenceNote(f.audio,0)==a[step%9]&&AudioSystemTestAccess::sequenceNote(f.audio,1)==b[step%6],"group patterns drifted/lost independent MAX");
        }
        f.audio.setBpm(140);f.render(1000);
        const int afterBpm=int(std::floor((total*165./(rate*60)+999*140./(rate*60))/8+1.e-8));
        require(AudioSystemTestAccess::sequenceNote(f.audio,0)==a[afterBpm%9]&&AudioSystemTestAccess::sequenceNote(f.audio,1)==b[afterBpm%6],"group BPM edit broke common step grid");
        // Latest requested state wins when several UI operations precede a block.
        f.audio.setInputActivation(0,0,3,false);f.audio.setInputActivation(3,0,3,true);f.audio.setInputActivation(0,0,3,false);f.render(1);
        require(!AudioSystemTestAccess::oscEnabled(f.audio,0)&&!AudioSystemTestAccess::oscEnabled(f.audio,1),"stale group command resurrected effects");
    }
    std::cout<<"PASS: one atomic group gate/restart, partial-active alignment, SYNC/RETRIG combinations with/without loops, 165 BPM MAX9/6 at 44.1/48 kHz and unchanged clock\n";
}

void testFxSequenceIntegration()
{
    using C=LoopTrack::Command;
    const auto setup=[](FiveTracks& f,int mask)
    {
        f.audio.setBpm(165); f.mic(.15f);
        f.audio.setMonitoringEnabled(true); f.audio.setMonitorVolume(.25f);
        for(int slot=0;slot<2;++slot)
        {
            OscBotParameters osc; osc.enabled=(mask&(1<<slot))!=0; osc.balance=100;osc.note=slot==0?36:72;
            FxSequenceParameters seq; seq.enabled=true;seq.rate=102;seq.maximum=slot==0?9:6;
            const int a[]={60,58,61,65,63,65,68,67,70}, b[]={80,79,82,80,82,85};
            for(int i=0;i<seq.maximum;++i) seq.notes[size_t(i)]=slot==0?a[i]:b[i];
            f.audio.setInputSequence(slot,seq); f.audio.setInputOscBot(slot,osc);
        }
    };
    std::array<std::array<float,4096>,3> signals;
    for(int mask=1;mask<=3;++mask)
    { FiveTracks f; setup(f,mask); f.render(32768); signals[size_t(mask-1)]=f.left; }
    double energyA=0,energyB=0,difference=0;
    for(size_t i=0;i<4096;++i)
    {
        energyA+=signals[0][i]*signals[0][i]; energyB+=signals[1][i]*signals[1][i];
        difference+=std::abs(signals[0][i]-signals[1][i]);
        require(std::abs(signals[2][i]-.5f*(signals[0][i]+signals[1][i]))<1.e-6f,
                "MULTI erased a voice or duplicated dry monitor");
    }
    require(energyA>.01 && energyB>.01 && difference>1,"A/B isolated sequences not audible/distinct");
    FiveTracks f; setup(f,3); f.render(8192);
    // Faster steps exercise actual note changes within the recorded block.
    for(int slot=0;slot<2;++slot)
    { FxSequenceParameters seq;seq.enabled=true;seq.rate=108;seq.maximum=2;seq.notes[0]=60+12*slot;seq.notes[1]=65+12*slot;f.audio.setInputSequence(slot,seq); }
    for(int i=0;i<5;++i) f.command(i,C::primary);
    f.render(4096);const auto captured=f.left;
    for(int i=0;i<5;++i) f.command(i,C::primary);
    f.stopAll();f.audio.setMonitoringEnabled(false); f.mic(0);
    for(int slot=0;slot<2;++slot) f.audio.setInputOscBot(slot,{});
    for(int track=0;track<5;++track)
    {
        f.command(track,C::primary);f.render(8192);
        for(size_t i=512;i<3800;++i)
            require(std::abs(f.left[i]-4*captured[i])<1.e-5f,"five-track sequence capture/playback OFF mismatch");
        f.command(track,C::stop);f.render(512);
    }
    std::cout<<"PASS: sequence A/B isolated and convex MULTI audio; all five tracks capture generated notes and replay with INPUT FX OFF\n";
}


void testSequenceSyncTransport()
{
    using C=LoopTrack::Command;
    for(bool quantized:{false,true})
    {
        FiveTracks f;f.mic(.15f);f.audio.setQuantizeEnabled(quantized);
        OscBotParameters osc;osc.enabled=true;osc.balance=100;f.audio.setInputOscBot(0,osc);
        FxSequenceParameters seq;seq.enabled=true;seq.sync=true;seq.rate=108;seq.maximum=3;
        seq.notes[0]=60;seq.notes[1]=64;seq.notes[2]=67;f.audio.setInputSequence(0,seq);
        f.command(0,C::primary);f.render(4096);f.command(0,C::primary);
        const int length=quantized?96000:4096;
        if(quantized) f.render(length-4096);
        require(f.audio.trackSnapshot(0).state==LoopTrack::State::playing,"sequence changed REC completion");
        f.render(1);require(AudioSystemTestAccess::sequenceNote(f.audio,0)==60,"SYNC missed first playback sample");
        f.render(3000);require(AudioSystemTestAccess::sequenceNote(f.audio,0)==64,"SYNC did not advance");
        f.render(length-3001);f.render(1);
        require(AudioSystemTestAccess::sequenceNote(f.audio,0)==60,"SYNC missed sample-exact phrase wrap");
        f.command(0,C::stop);f.command(0,C::primary);f.render(1);
        require(AudioSystemTestAccess::sequenceNote(f.audio,0)==60,"SYNC missed STOP/resume");
        seq.sync=false;seq.retrig=true;f.audio.setInputSequence(0,seq);f.render(3100);
        osc.enabled=false;f.audio.setInputOscBot(0,osc);f.render(100);
        osc.enabled=true;f.audio.setInputOscBot(0,osc);f.render(1);
        require(AudioSystemTestAccess::sequenceNote(f.audio,0)==64,"RETRIG failed phrase-position alignment");
    }
    // LOOP SYNC delayed playback also cues on its scheduled sample.
    FiveTracks f(48000,240000);f.mic(.1f);f.audio.setAllLoopSync(true);
    OscBotParameters osc;osc.enabled=true;f.audio.setInputOscBot(0,osc);
    FxSequenceParameters seq;seq.enabled=true;seq.sync=true;seq.rate=108;seq.maximum=2;seq.notes[0]=60;seq.notes[1]=72;
    f.audio.setInputSequence(0,seq);f.command(0,C::primary);f.render(untilSyncBar(f));
    f.render(4096);f.command(0,C::primary);f.render(96000-4096);f.render(1);
    require(f.audio.trackSnapshot(0).state==LoopTrack::State::playing && AudioSystemTestAccess::sequenceNote(f.audio,0)==60,
            "sequence LOOP SYNC REC->PLAY alignment");
    std::cout<<"PASS: sequence SYNC sample-exact free/quantized/LOOP SYNC playback/wrap/resume and RETRIG alignment\n";
}

void testFxSequencePanel()
{
    FiveTracks f;MainComponent panel(f.audio);TopPanelComponent* top=nullptr;
    for(auto* c:panel.getChildren()) if(auto* t=dynamic_cast<TopPanelComponent*>(c)) top=t;
    juce::Button *edit=nullptr,*exit=nullptr,*next=nullptr;
    std::array<juce::Button*,4> slots{};std::array<ParameterKnob*,4> knobs{};MemoryDisplay* display=nullptr;
    for(auto* c:top->getChildren())
    {
        if(auto* b=dynamic_cast<juce::Button*>(c))
        {
            if(b->getTitle()=="INPUT FX EDIT") edit=b;
            if(b->getButtonText()=="EXIT") exit=b;
            if(b->getButtonText()=="Next") next=b;
            for(int i=0;i<4;++i) if(b->getTitle()=="INPUT FX "+juce::String::charToString(char('A'+i))) slots[size_t(i)]=b;
        }
        if(auto* k=dynamic_cast<ParameterKnob*>(c))
            for(int i=0;i<4;++i) if(k->getName()=="Parameter knob "+juce::String(i+1)) knobs[size_t(i)]=k;
        if(auto* d=dynamic_cast<MemoryDisplay*>(c)) display=d;
    }
    require(edit&&exit&&next&&display&&slots[3]&&knobs[3],"sequence panel controls");
    edit->onClick();
    for(int slot=0;slot<2;++slot)
    {
        slots[size_t(slot)]->onClick();knobs[0]->setValue(5,juce::sendNotificationSync);
        next->onClick();knobs[2]->setValue(100,juce::sendNotificationSync);next->onClick();
        knobs[0]->setValue(1,juce::sendNotificationSync);
        require(!knobs[3]->isEnabled() && display->getEditorField(3).text=="NOTE","TARGET should be fixed NOTE");
        next->onClick();knobs[0]->setValue(102,juce::sendNotificationSync);knobs[1]->setValue(slot==0?9:6,juce::sendNotificationSync);
        require(display->getEditorField(2).text=="MULTI","MULTI not exposed");
        const int a[]={60,58,61,65,63,65,68,67,70},b[]={80,79,82,80,82,85};
        const int count=slot==0?9:6;
        for(int i=0;i<count;++i)
        { if(i%4==0) next->onClick();knobs[size_t(i%4)]->setValue(slot==0?a[i]:b[i],juce::sendNotificationSync); }
        const auto q=display->inputSequence(slot);
        require(q.enabled&&q.rate==102&&q.maximum==count&&q.notes[size_t(count-1)]==(slot==0?70:85),"LCD sequence edits lost");
    }
    exit->onClick();
    for(int slot=0;slot<2;++slot) {slots[size_t(slot)]->setToggleState(true,juce::dontSendNotification);slots[size_t(slot)]->onClick();}
    require(slots[0]->getToggleState()&&slots[1]->getToggleState(),"MULTI buttons mutually exclusive");
    f.mic(.15f);f.audio.setMonitoringEnabled(true);f.audio.setMonitorVolume(.25f);f.render(8192);
    double difference=0;for(float x:f.left) difference+=std::abs(x-.0375f);
    require(difference>5,"LCD sequence not wired to live engine");
    edit->onClick();slots[0]->onClick();next->onClick();next->onClick();next->onClick();
    knobs[2]->setValue(0,juce::sendNotificationSync);
    require(slots[0]->getToggleState()&&!slots[1]->getToggleState(),"SINGLE did not enforce one active effect");
    knobs[2]->setValue(1,juce::sendNotificationSync);
    next->onClick();require(display->getEditorField(0).value==60&&display->inputSequence(1).notes[0]==80,"SINGLE/MULTI lost sequence");
    auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mk2-fx-sequence.png");
    juce::FileOutputStream out(file);out.setPosition(0);out.truncate();juce::PNGImageFormat png;
    require(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(),true,4),out),"sequence LCD render");
    exit->onClick();require(!display->isInputEditing(),"sequence EXIT");
    std::cout<<"PASS: LCD sequence pages/encoders/slots/SINGLE-MULTI/EXIT reach production DSP\n";
}

void testOscBotPanel()
{
    FiveTracks f; MainComponent panel(f.audio);
    TopPanelComponent* top = nullptr;
    for (auto* child : panel.getChildren()) if (auto* t = dynamic_cast<TopPanelComponent*>(child)) top = t;
    juce::Button *edit = nullptr, *slotB = nullptr, *slotC = nullptr, *exit = nullptr, *next = nullptr, *back = nullptr;
    MemoryDisplay* display = nullptr;
    std::array<ParameterKnob*, 4> knobs {};
    for (auto* child : top->getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*>(child))
        {
            if (b->getTitle() == "INPUT FX EDIT") edit = b;
            if (b->getTitle() == "INPUT FX B") slotB = b;
            if (b->getTitle() == "INPUT FX C") slotC = b;
            if (b->getButtonText() == "EXIT") exit = b;
            if (b->getButtonText() == "Next") next = b;
            if (b->getButtonText() == "Previous") back = b;
        }
        if (auto* d = dynamic_cast<MemoryDisplay*>(child)) display = d;
        if (auto* k = dynamic_cast<ParameterKnob*>(child))
            for (int i = 0; i < 4; ++i) if (k->getName() == "Parameter knob " + juce::String(i + 1)) knobs[size_t(i)] = k;
    }
    require(edit && slotB && slotC && exit && next && back && display && knobs[3], "OSC BOT panel controls missing");
    edit->onClick(); slotB->onClick(); knobs[0]->setValue(4, juce::sendNotificationSync);
    knobs[1]->setValue(7, juce::sendNotificationSync); knobs[2]->setValue(-23, juce::sendNotificationSync);
    slotC->onClick(); knobs[0]->setValue(5, juce::sendNotificationSync);
    require(display->getEditorField(1).text == "SAW" && knobs[2]->getValue() == 0 && knobs[3]->getValue() == 50,
            "OSC BOT panel defaults incorrect");
    const auto screenshot = [&](const char* name)
    {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
        juce::FileOutputStream stream(file); stream.setPosition(0); stream.truncate();
        juce::PNGImageFormat png;
        require(png.writeImageToStream(display->createComponentSnapshot(display->getLocalBounds(), true, 4), stream), "OSC BOT LCD render failed");
    };
    knobs[1]->setValue(1, juce::sendNotificationSync); screenshot("mk2-osc-bot-page1.png");
    knobs[3]->setValue(0, juce::sendNotificationSync); next->onClick();
    require(display->getEditorField(0).text == "C2" && knobs[0]->getMinimum() == 24 && knobs[0]->getMaximum() == 127
            && knobs[1]->getMinimum() == -50 && knobs[2]->getValue() == 50, "OSC BOT page two ranges");
    knobs[0]->setValue(48, juce::sendNotificationSync);
    knobs[1]->setValue(-50, juce::sendNotificationSync);
    knobs[2]->setValue(100, juce::sendNotificationSync);
    require(display->getEditorField(2).value == 100 && display->getEditorField(2).text == "100",
            "OSC BOT BALANCE encoder did not reach 100");
    screenshot("mk2-osc-bot-page2.png");
    exit->onClick(); slotC->setToggleState(true, juce::dontSendNotification); slotC->onClick();
    f.audio.setMonitoringEnabled(true); f.audio.setMonitorVolume(.25f); f.mic(.1f); f.render(8192);
    float delta = 0;
    for (int n = 1000; n < 3800; ++n) delta += std::abs(f.left[size_t(n)] - .025f);
    require(delta > 5, "OSC BOT panel not connected to DSP");
    edit->onClick(); slotC->onClick(); next->onClick();
    require(slotC->getToggleState() && display->getEditorField(0).value == 48 && display->getEditorField(1).value == -50,
            "OSC BOT editor toggled slot/lost values");
    // The generic mouse-wheel encoder also adjusts the full semitone range.
    const auto event = juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {20,20},
        juce::ModifierKeys(), 1, 0, 0, 0, 0, knobs[0], knobs[0], juce::Time::getCurrentTime(),
        {20,20}, juce::Time::getCurrentTime(), 1, false);
    juce::MouseWheelDetails wheel {}; wheel.deltaY = .1f; knobs[0]->mouseWheelMove(event, wheel);
    require(display->getEditorField(0).value > 48, "OSC BOT NOTE wheel ignored");
    back->onClick(); knobs[0]->setValue(2, juce::sendNotificationSync); f.render(4096);
    for (int n = 1500; n < 3800; ++n) require(f.left[size_t(n)] == .025f, "OSC BOT remained active after selecting visual HPF");
    slotB->onClick();
    require(display->getEditorField(0).text == "ROBOT" && display->getEditorField(1).value == 7
            && display->getEditorField(2).value == -23, "OSC BOT destroyed ROBOT in B");
    slotC->onClick(); knobs[0]->setValue(5, juce::sendNotificationSync);
    exit->onClick(); slotC->setToggleState(false, juce::dontSendNotification); slotC->onClick(); f.render(4096);
    for (int n = 1500; n < 3800; ++n) require(f.left[size_t(n)] == .025f, "OSC BOT physical OFF not dry");
    std::cout << "PASS: OSC BOT catalog/two LCD pages/encoders/wheel/ON-OFF/EXIT to DSP; ROBOT B preserved\n";
}

void testOscBotCallbackBudget()
{
    for (double rate : { 44100., 48000. })
    {
        FiveTracks f(rate);
        for (int n = 0; n < 4096; ++n)
        { f.inputL[size_t(n)] = .08f * std::sin(n * .1f); f.inputR[size_t(n)] = .06f * std::sin(n * .13f); }
        f.playAll(); f.render(4096); f.playAll();
        TrackFxParameters effects; effects.enabled = true; effects.distortionEnabled = true;
        f.audio.setTrackFxInsert(0, 0); f.audio.setTrackFxInsert(2, 0); f.audio.setTrackEffects(effects);
        f.audio.setInputRobot(1, { true, 0, 0, 2 });
        f.audio.setMonitoringEnabled(true);
        OscBotParameters p; p.enabled = true; p.osc = 2; p.note = 24; p.balance = 100;
        for (int slot : { 0, 2, 3 })
        {
            f.audio.setInputOscBot(slot,p);
            FxSequenceParameters seq;seq.enabled=true;seq.rate=100;
            for(int i=0;i<16;++i) seq.notes[size_t(i)]=24+(i+slot)%12;
            f.audio.setInputSequence(slot,seq);
        }
        f.render(8192);
        std::array<double, 200> times {};
        double total = 0;
        for (auto& ms : times)
        {
            const auto begin = std::chrono::steady_clock::now(); f.render(256);
            ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            total += ms;
            for (int n = 0; n < 256; ++n)
                require(std::isfinite(f.left[size_t(n)]) && std::abs(f.left[size_t(n)]) < .98f, "OSC BOT loaded callback unsafe");
        }
        std::sort(times.begin(), times.end());
        std::cout << "OSC BOT full callback (3 sequences + ROBOT + 5 loops + TRACK FX) rate=" << rate
            << " meanMs=" << total / times.size() << " p99Ms=" << times[197]
            << " maxMs=" << times.back() << " deadlineMs=" << 256000 / rate << '\n';
    }
}

void testRobotCallbackBudget()
{
    // Report, rather than assert wall time: OS scheduling is not deterministic.
    // These timings include all five playing tracks, both TRACK FX and monitor.
    for (double rate : { 44100., 48000. })
    {
        FiveTracks f(rate);
        for (int n = 0; n < 4096; ++n)
        { f.inputL[size_t(n)] = .04f * std::sin(n * .1f); f.inputR[size_t(n)] = .03f * std::sin(n * .13f); }
        f.playAll(); f.render(4096); f.playAll();
        TrackFxParameters effects; effects.enabled = true; effects.distortionEnabled = true;
        f.audio.setTrackFxInsert(0, 0); f.audio.setTrackFxInsert(2, 0); f.audio.setTrackEffects(effects);
        f.audio.setMonitoringEnabled(true);
        for (int active : { 0, 1, 4 })
        {
            for (int slot = 0; slot < 4; ++slot) f.audio.setInputRobot(slot, { slot < active, 0, 0, 2 });
            f.render(4096);
            std::array<double, 200> times {};
            double total = 0;
            for (auto& ms : times)
            {
                const auto begin = std::chrono::steady_clock::now();
                f.render(256);
                ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
                total += ms;
                for (int n = 0; n < 256; ++n)
                    require(std::isfinite(f.left[size_t(n)]) && std::abs(f.left[size_t(n)]) < .98f, "ROBOT loaded callback unsafe");
            }
            std::sort(times.begin(), times.end());
            std::cout << "ROBOT full callback rate=" << rate << " activeSlots=" << active << " meanMs=" << total / times.size()
                      << " p99Ms=" << times[197] << " maxMs=" << times.back() << " deadlineMs=" << 256000 / rate << '\n';
        }
    }
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

void runFxSequenceTests();
void runRobotQualityTests(bool enforce);
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    try
    {
        if(argc>1 && std::string(argv[1])=="--fx-banks")
        {testFxBanks6I1();return 0;}
        if(argc>1 && std::string(argv[1])=="--fx-knobs")
        {
#if defined(_MSC_VER) && defined(_DEBUG)
            const auto old=_CrtSetAllocHook(fiveTrackAllocationHook);
#endif
            testFxKnobs6I();
#if defined(_MSC_VER) && defined(_DEBUG)
            _CrtSetAllocHook(old);
#endif
            require(fiveTrackAllocations==0,"6I callback allocation");return 0;
        }
        if(argc>1 && std::string(argv[1])=="--indicators-6h4")
        {testIndicators6H4();return 0;}
        if(argc>1 && std::string(argv[1])=="--recording-6h4")
        {testCaptureAlignment6H4();testMeasureSync();testSyncDriftAndRates();testQuantizeIntegration();runLoopTrackTests();return 0;}
        if(argc>1 && std::string(argv[1])=="--g2b")
        {runG2BEffectTests();testG2BIntegration();return 0;}
        if(argc>1 && std::string(argv[1])=="--step-slicer")
        {runStepSlicerEffectTests();testStepSlicerIntegration();testStepSlicerPanel();return 0;}
        if (argc > 1 && std::string(argv[1]) == "--input-fx-shortcuts")
        { testInputFxShortcuts(); testInputFxGroupAudio(); return 0; }
        if (argc > 1 && std::string(argv[1]) == "--fx-sequence")
        { runFxSequenceTests(); return 0; }
        runFxSequenceTests();
        if (argc > 1 && std::string(argv[1]) == "--robot-quality")
        { runRobotQualityTests(argc < 3); return 0; }
        if (argc > 1 && std::string(argv[1]) == "--osc-bot")
        { runOscBotEffectTests(); return 0; }
        if (argc > 1 && std::string(argv[1]) == "--reverb")
        { runReverbEffectTests(); return 0; }
        if (argc > 1 && std::string(argv[1]) == "--osc-panel")
        { testOscBotPanel(); return 0; }
        runOscBotEffectTests();
        runReverbEffectTests();
        runStepSlicerEffectTests();
        runG2BEffectTests();
        runRobotQualityTests(true);
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    try { runRobotEffectTests(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    try
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        const auto previous = _CrtSetAllocHook(fiveTrackAllocationHook);
#endif
        testFxBanks6I1();testFxKnobs6I();testIndicators6H4();testCaptureAlignment6H4();
        testStepSlicerIntegration();testStepSlicerPanel();testInputFxShortcuts();testInputFxGroupAudio();
        testFxSequenceIntegration();testSequenceSyncTransport();testFxSequencePanel();
        testG2BIntegration();
        testFiveTracks(); testFiveQuantizedTracksAndMix(); testTrackPanelRouting(); testSharedTrackFxRouting(); testInputRobotIntegration(); testInputRobotPanel(); testRobotCallbackBudget();
        testOscBotInputIntegration();
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
