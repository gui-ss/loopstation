#include "AudioSettingsWindow.h"

namespace
{
class AudioSettingsContent final : public juce::Component, private juce::ChangeListener
{
public:
    explicit AudioSettingsContent(AudioSystem& system) : audio(system)
    {
        inputLabel.setText("Microfone / entrada", juce::dontSendNotification);
        outputLabel.setText(L"Saída de áudio", juce::dontSendNotification);
        input.setTitle("Microfone / entrada");
        output.setTitle(L"Saída de áudio");
        reconnect.setButtonText("Reconectar / atualizar dispositivos");
        monitoring.setButtonText("Monitorar microfone");
        volumeLabel.setText("Volume do retorno", juce::dontSendNotification);
        volume.setTitle("Volume do retorno");
        volume.setSliderStyle(juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 28);
        volume.setRange(0, 100, 1);
        volume.setTextValueSuffix(" %");
        note.setText(L"Use fones de ouvido para evitar microfonia.\nFechar esta janela mantém o estado do monitoramento.",
                     juce::dontSendNotification);
        status.setJustificationType(juce::Justification::topLeft);
        note.setJustificationType(juce::Justification::topLeft);
        for (auto* child : std::initializer_list<juce::Component*>{ &inputLabel, &outputLabel, &input, &output,
                                                                   &status, &reconnect, &note, &monitoring, &volumeLabel, &volume })
            addAndMakeVisible(child);
        input.onChange = [this] { audio.selectDevices(selectedName(input, true), audio.selectedOutput()); refresh(); };
        output.onChange = [this] { audio.selectDevices(audio.selectedInput(), selectedName(output, false)); refresh(); };
        reconnect.onClick = [this] { audio.reconnect(); refresh(); };
        monitoring.onClick = [this] { audio.setMonitoringEnabled(monitoring.getToggleState()); refresh(); };
        volume.onValueChange = [this] { audio.setMonitorVolume(static_cast<float>(volume.getValue()) * 0.01f); };
        audio.addChangeListener(this);
        refresh();
        setSize(600, 480);
    }
    ~AudioSettingsContent() override { audio.removeChangeListener(this); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff25272c)); }
    void resized() override
    {
        inputLabel.setBounds(20, 16, 560, 24); input.setBounds(20, 44, 560, 30);
        outputLabel.setBounds(20, 86, 560, 24); output.setBounds(20, 114, 560, 30);
        status.setBounds(20, 162, 560, 100);
        reconnect.setBounds(20, 270, 300, 32);
        monitoring.setBounds(20, 312, 560, 28);
        volumeLabel.setBounds(20, 348, 560, 24);
        volume.setBounds(20, 375, 560, 30);
        note.setBounds(20, 424, 560, 48);
    }
private:
    juce::String selectedName(const juce::ComboBox& box, bool isInput) const
    {
        if (box.getSelectedId() == 1) return {};
        const auto& names = isInput ? inputNames : outputNames;
        const int index = box.getSelectedId() - 2;
        return juce::isPositiveAndBelow(index, names.size()) ? names[index]
                          : (isInput ? audio.selectedInput() : audio.selectedOutput());
    }
    void populate(juce::ComboBox& box, juce::StringArray& cached, bool isInput)
    {
        cached = audio.deviceNames(isInput);
        const auto& chosen = isInput ? audio.selectedInput() : audio.selectedOutput();
        box.clear(juce::dontSendNotification);
        box.addItem("Nenhum", 1);
        box.addItemList(cached, 2);
        int id = chosen.isEmpty() ? 1 : cached.indexOf(chosen) + 2;
        if (chosen.isNotEmpty() && ! cached.contains(chosen))
        {
            id = cached.size() + 2;
            box.addItem(chosen + L" (indisponível)", id);
        }
        box.setSelectedId(id, juce::dontSendNotification);
    }
    void refresh()
    {
        populate(input, inputNames, true);
        populate(output, outputNames, false);
        status.setText(L"Estado da conexão:\n" + audio.statusText(), juce::dontSendNotification);
        monitoring.setToggleState(audio.isMonitoringEnabled(), juce::dontSendNotification);
        monitoring.setEnabled(audio.canMonitor());
        volume.setValue(juce::roundToInt(audio.monitorVolume() * 100.0f), juce::dontSendNotification);
    }
    void changeListenerCallback(juce::ChangeBroadcaster*) override { refresh(); }
    AudioSystem& audio;
    juce::StringArray inputNames, outputNames;
    juce::ComboBox input, output;
    juce::Label inputLabel, outputLabel, status, note;
    juce::TextButton reconnect;
    juce::ToggleButton monitoring;
    juce::Label volumeLabel;
    juce::Slider volume;
};
}

AudioSettingsWindow::AudioSettingsWindow(AudioSystem& audio)
    : DocumentWindow(L"Configurações de áudio", juce::Colour(0xff25272c), closeButton)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new AudioSettingsContent(audio), true);
    setResizable(false, false);
    centreWithSize(getWidth(), getHeight());
}
