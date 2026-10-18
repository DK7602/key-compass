// Key Compass - the Android app: one window, the microphone (and any MIDI keyboard) as input,
// the Song Mapper's sound out of the speaker. Saves itself whenever it goes to the background.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../PluginProcessor.h"
#include "../Version.h"
#include "../UI/Manual.h"
#include "MobileEditor.h"

namespace
{
    juce::File appDataFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("KeyCompass");
        dir.createDirectory();
        return dir;
    }
}

//==============================================================================
class KeyCompassMobileApp : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override       { return "Key Compass"; }
    const juce::String getApplicationVersion() override    { return KC_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String& commandLine) override
    {
        // behave as the desktop standalone app does: listen to the input, never play the microphone back
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        processor = std::make_unique<KeyCompassProcessor>();
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        loadState();

        window = std::make_unique<MainWindow> (*processor, commandLine);
        if (auto* ed = editor())
            ed->onRequestMicrophone = [this] { askForMicrophoneAgain(); };
        startAudio();
        startTimer (20000);      // autosave: Android can close a background app without warning
    }

    void shutdown() override
    {
        stopTimer();
        saveState();
        stopAudio();
        window.reset();
        processor.reset();
    }

    void suspended() override
    {
        saveState();
        stopAudio();
    }

    void resumed() override
    {
        startAudio();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    bool backButtonPressed() override
    {
        // close whatever is open on top (the practice view, a menu, another tab) before leaving the app
        if (auto* ed = editor())
            return ed->handleBackButton();
        return false;
    }

private:
    MobileEditor* editor() const
    {
        return window != nullptr ? dynamic_cast<MobileEditor*> (window->getContentComponent()) : nullptr;
    }

    void askForMicrophoneAgain()
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio, [this] (bool granted)
        {
            if (granted)
            {
                micAllowed = true;
                auto setup = deviceManager.getAudioDeviceSetup();
                setup.useDefaultInputChannels = true;
                deviceManager.setAudioDeviceSetup (setup, true);
                if (auto* ed = editor()) ed->setMicrophoneAllowed (true);
                return;
            }
            // Android stops asking after the second "Don't allow"; then only Settings can turn it on
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Microphone",
                "Key Compass listens through the microphone to find the key and chords.\n\n"
                "To allow it: open your phone's Settings > Apps > Key Compass > Permissions > Microphone > Allow.\n\n"
                "A MIDI keyboard plugged into the phone works without it.");
        });
    }

    //==============================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (KeyCompassProcessor& p, const juce::String& commandLine)
            : DocumentWindow ("Key Compass", juce::Colours::black, 0)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MobileEditor (p), false);
           #if JUCE_ANDROID || JUCE_IOS
            juce::ignoreUnused (commandLine);
            setFullScreen (true);
           #else
            // desktop test runs: --size=WxH pretends to be that phone or tablet
            int w = 412, h = 880;
            const auto size = commandLine.fromFirstOccurrenceOf ("--size=", false, false).upToFirstOccurrenceOf (" ", false, false);
            if (size.containsChar ('x'))
            {
                w = juce::jmax (200, size.upToFirstOccurrenceOf ("x", false, false).getIntValue());
                h = juce::jmax (200, size.fromFirstOccurrenceOf ("x", false, false).getIntValue());
            }
            setResizable (true, false);
            centreWithSize (w, h);
           #endif
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    //==============================================================================
    void startAudio()
    {
        if (audioRunning || processor == nullptr) return;
        audioRunning = true;

        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio, [this] (bool granted)
        {
            if (processor == nullptr || ! audioRunning) return;
            micAllowed = granted;
            if (! deviceManagerReady)
            {
                deviceManager.initialise (granted ? 2 : 0, 2, nullptr, true);
                deviceManagerReady = true;
            }
            else
            {
                deviceManager.restartLastAudioDevice();
            }
            if (granted && deviceManager.getCurrentAudioDevice() != nullptr
                && deviceManager.getCurrentAudioDevice()->getActiveInputChannels().isZero())
            {
                // permission arrived after the device was opened without inputs: reopen it with the microphone
                auto setup = deviceManager.getAudioDeviceSetup();
                setup.useDefaultInputChannels = true;
                deviceManager.setAudioDeviceSetup (setup, true);
            }
            player.setProcessor (processor.get());
            deviceManager.addAudioCallback (&player);
            enableMidiInputs();
            if (auto* ed = editor())
                ed->setMicrophoneAllowed (granted);
        });
    }

    void stopAudio()
    {
        if (! audioRunning) return;
        audioRunning = false;
        if (processor != nullptr) processor->getAudition().stop();
        deviceManager.removeAudioCallback (&player);
        deviceManager.removeMidiInputDeviceCallback ({}, &player);
        player.setProcessor (nullptr);
        deviceManager.closeAudioDevice();
    }

    // any MIDI keyboard that is plugged in (USB or Bluetooth) plays into the compass
    void enableMidiInputs()
    {
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (! deviceManager.isMidiInputDeviceEnabled (d.identifier))
                deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        deviceManager.removeMidiInputDeviceCallback ({}, &player);
        deviceManager.addMidiInputDeviceCallback ({}, &player);
    }

    //==============================================================================
    juce::File stateFile() const { return appDataFolder().getChildFile ("state.bin"); }

    void loadState()
    {
        juce::MemoryBlock data;
        if (stateFile().loadFileAsData (data) && data.getSize() > 0)
            processor->setStateInformation (data.getData(), (int) data.getSize());
    }

    void saveState()
    {
        if (processor == nullptr) return;
        juce::MemoryBlock data;
        processor->getStateInformation (data);
        if (data.getSize() == 0 || data == lastSaved) return;
        auto tmp = stateFile().getSiblingFile ("state.tmp");
        if (tmp.replaceWithData (data.getData(), data.getSize()) && tmp.moveFileTo (stateFile()))
            lastSaved = data;
    }

    void timerCallback() override
    {
        saveState();
        if (audioRunning && deviceManagerReady && midiDevicesSeen != juce::MidiInput::getAvailableDevices().size())
        {
            midiDevicesSeen = juce::MidiInput::getAvailableDevices().size();
            enableMidiInputs();
        }
    }

    std::unique_ptr<KeyCompassProcessor> processor;
    std::unique_ptr<MainWindow> window;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer player;
    juce::MemoryBlock lastSaved;
    bool audioRunning = false, deviceManagerReady = false, micAllowed = false;
    int midiDevicesSeen = -1;
};

START_JUCE_APPLICATION (KeyCompassMobileApp)
