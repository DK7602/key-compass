// Key Compass - the Android app's screen: the compass and the Song Mapper laid out for phones and tablets
#pragma once

#include "../PluginEditor.h"

class MobileEditor : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit MobileEditor (KeyCompassProcessor&);
    ~MobileEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool handleBackButton();                 // true = handled (something was closed)
    void setMicrophoneAllowed (bool allowed);
    void tick (float seconds);               // for tests: advance the animation without waiting

private:
    void timerCallback() override;

    KeyCompassProcessor& processor;
    juce::SharedResourcePointer<kcui::Assets> assets;
    kcui::CompassLookAndFeel lookAndFeel;
    kcui::UiState uiState;
    KeyCompassCanvas canvas;
    bool micAllowed = true;
    bool lastFlats = false, lastLock = false;
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MobileEditor)
};
