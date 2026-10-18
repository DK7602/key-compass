// Key Compass - the Android app's screen: the compass and the Song Mapper laid out for phones and tablets.
//
//  Classic : a tablet held upright - the desktop pages, scaled (the Song Mapper's three panels side by side)
//  Phone   : a phone held upright - one thing at a time, chosen with the tabs along the bottom
//  Wide    : a phone or tablet on its side - tabs down the left; as many panels side by side as fit
#pragma once

#include "../PluginEditor.h"
#include "ManualViewer.h"

class MobileEditor : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit MobileEditor (KeyCompassProcessor&);
    ~MobileEditor() override;

    enum class Layout { Classic, Phone, Wide };
    enum Tab { compassTab = 0, chordsTab, songTab, libraryTab, moreTab, numTabs };

    void paint (juce::Graphics&) override;
    void resized() override;

    void showTab (int tab);
    int currentTab() const noexcept               { return tab; }
    Layout currentLayout() const noexcept         { return layout; }
    int panelsSideBySide() const noexcept         { return panes; }

    bool handleBackButton();                      // true = handled (something was closed)
    void setMicrophoneAllowed (bool allowed);
    std::function<void()> onRequestMicrophone;    // the app asks Android for the microphone again
    void tick (float seconds);                    // for tests: advance the animation without waiting

    // the manual, drawn page by page over everything (Android's PDF renderer); false = it can't be shown here
    bool showManual (const juce::File& pdf);
    void showManualWith (std::unique_ptr<ManualViewer::Renderer>);
    bool isManualOpen() const noexcept            { return manual != nullptr; }

    // tests: pretend the status / navigation bars and the on-screen keyboard take this much room
    void setSimulatedInsets (juce::BorderSize<int> safeArea, int keyboard = 0);

    KeyCompassCanvas& getCanvas() noexcept        { return canvas; }
    kcui::SongMapperPage& getMapper() noexcept    { return canvas.mapper; }
    juce::Component* findTabButton (int tab);
    juce::Component& getMoreSheet();

    class TabBar;
    class CompassView;
    class CompassStrip;
    class MoreSheet;

private:
    void timerCallback() override;
    juce::Rectangle<int> safeArea (int* keyboardInset = nullptr) const;
    void layoutClassic (juce::Rectangle<int> area);
    void layoutTouch (juce::Rectangle<int> area, bool sideways);
    void putMapperInCanvas (bool inCanvas);
    int effectiveTab() const;                     // the tab as the current layout shows it

    KeyCompassProcessor& processor;
    juce::SharedResourcePointer<kcui::Assets> assets;
    kcui::CompassLookAndFeel lookAndFeel;
    kcui::UiState uiState;
    KeyCompassCanvas canvas;

    std::unique_ptr<TabBar> tabs;
    std::unique_ptr<CompassView> compass;
    std::unique_ptr<CompassStrip> strip;
    std::unique_ptr<MoreSheet> more;
    juce::Component practiceLayer;                // the practice view and the manual cover the screen (inside the bars)
    std::unique_ptr<ManualViewer> manual;
    juce::ScopedMessageBox shareBox;

    Layout layout = Layout::Phone;
    int tab = compassTab, panes = 1;
    bool mapperInCanvas = true, micAllowed = true;
    bool lastFlats = false, lastLock = false;
    int lastMode = -1, lastKeyboard = -1;
    juce::Rectangle<int> lastSafe;
    juce::BorderSize<int> simulatedSafe;
    int simulatedKeyboard = 0;
    bool simulating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MobileEditor)
};
