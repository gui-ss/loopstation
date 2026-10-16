#include "MainComponent.h"
#include "AudioSettingsWindow.h"

class MK2Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MK2 Loopstation"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise(const juce::String&) override
    {
        audio.initialise();
        window = std::make_unique<MainWindow>(getApplicationName(), audio);
        if (! audio.storageReady())
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Loopstation - RAM", audio.statusText());
    }

    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, AudioSystem& system)
            : DocumentWindow(name, juce::Colour(0xff17181b), allButtons), audio(system)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(audio), true);
            // Listen only to the bare panel, not to its buttons, knobs or faders.
            getContentComponent()->addMouseListener(this, false);
            setResizable(true, false);
            setResizeLimits(800, 480, 3840, 2160);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        ~MainWindow() override { getContentComponent()->removeMouseListener(this); }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (! event.mods.isPopupMenu()) return;
            showAudioSettings();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key.getKeyCode() == 'L' && key.getModifiers().getRawFlags() == 0)
            {
                if (! syncKeyHeld) audio.setAllLoopSync(! audio.allLoopSyncEnabled());
                syncKeyHeld = true;
                return true;
            }
            if (key.getKeyCode() == 'Q' && key.getModifiers().getRawFlags() == 0)
            {
                if (! quantizeKeyHeld) audio.setQuantizeEnabled(! audio.quantizeEnabled());
                quantizeKeyHeld = true;
                return true;
            }
            if (key.getKeyCode() == 'Z' && key.getModifiers().isCtrlDown()
                && ! key.getModifiers().isShiftDown() && ! key.getModifiers().isAltDown())
            {
                audio.undoSelectedTrack();
                return true;
            }
            if (key != juce::KeyPress(juce::KeyPress::F5Key)) return false;
            showAudioSettings();
            return true;
        }

        void showAudioSettings()
        {
            if (settings == nullptr) settings = std::make_unique<AudioSettingsWindow>(audio);
            settings->setVisible(true);
            settings->toFront(true);
        }

        bool keyStateChanged(bool) override
        {
            if (! juce::KeyPress::isKeyCurrentlyDown('L')) syncKeyHeld = false;
            if (! juce::KeyPress::isKeyCurrentlyDown('Q')) quantizeKeyHeld = false;
            return false;
        }

        void activeWindowStatusChanged() override
        {
            juce::DocumentWindow::activeWindowStatusChanged();
            if (! isActiveWindow()) { quantizeKeyHeld = false; syncKeyHeld = false; }
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    private:
        AudioSystem& audio;
        bool quantizeKeyHeld = false, syncKeyHeld = false;
        std::unique_ptr<AudioSettingsWindow> settings;
    };

    AudioSystem audio;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(MK2Application)
