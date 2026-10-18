// Key Compass - editor
#pragma once

#include "PluginProcessor.h"
#include "UI/CompassViews.h"
#include "UI/Controls.h"
#include "UI/SongMapperView.h"

//==============================================================================
// Everything is laid out in design pixels (the painted background's size) on this
// canvas; the editor scales the canvas to whatever size the window is.
class KeyCompassCanvas : public juce::Component
{
public:
    KeyCompassCanvas (KeyCompassProcessor&, kcui::Assets&, const kcui::UiState&);

    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (float)> onScaleChosen;
    std::function<void (int)> onPageChosen;

    void showPage (int page);       // 0 = Key Compass, 1 = Song Mapper
    int getPage() const noexcept    { return page; }

    kcui::StaticLayer staticLayer;
    kcui::DialLayer dialLayer;
    kcui::LidLayer lidLayer;
    kcui::SongMapperPage mapper;

private:
    KeyCompassProcessor& processor;
    kcui::ChoiceBox modeBox, responseBox;
    kcui::TuningBox tuningBox;
    kcui::RoundToggle lockButton, flatsButton;
    kcui::PlateButton clearButton;
    kcui::KButton mapperButton, manualButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment lockAttachment, flatsAttachment;
    int page = 0;
};

//==============================================================================
class KeyCompassEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit KeyCompassEditor (KeyCompassProcessor&);
    ~KeyCompassEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For tests / screenshots: advance the animation without waiting for the timer
    void tick (float seconds);
    KeyCompassCanvas& getCanvas() noexcept { return canvas; }

private:
    void timerCallback() override;
    void applyScale (float scale);
    void switchPage (int page);
    float defaultScale() const;
    float scaleForPage (int page) const;
    bool constructed = false;

    KeyCompassProcessor& processor;
    juce::SharedResourcePointer<kcui::Assets> assets;
    kcui::CompassLookAndFeel lookAndFeel;
    kcui::UiState uiState;
    KeyCompassCanvas canvas;
    std::unique_ptr<kcui::TipWindow> tooltipWindow;
    bool lastFlats = false;
    int lastMode = -1;
    bool lastLock = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyCompassEditor)
};
