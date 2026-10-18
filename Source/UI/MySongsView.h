// Key Compass - My Songs (every song you've worked on) and Cloud Sync (the same songs on every device).
// Opens over the Song Mapper page on the PC, and over the whole screen in the phone / tablet app.
#pragma once

#include "SongMapperView.h"
#include "../Sync/SongLink.h"

namespace kcui
{
    class MySongsView : public juce::Component, private kcsync::SongLibrary::Listener, private juce::ChangeListener,
                        private juce::Timer
    {
    public:
        explicit MySongsView (SongMapperPage&);
        ~MySongsView() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDown (const juce::MouseEvent&) override;

        void showCloud (bool cloud);
        bool isShowingCloud() const noexcept     { return cloudPage; }
        bool handleBack();                       // cloud page -> songs -> closed (true = it did something)
        std::function<void()> onClose;

        juce::Component* findRow (const juce::String& songName);     // tests
        juce::Component& getTurnOnButton() noexcept { return turnOnButton; }

    private:
        class SongRow;
        struct ListContent : public juce::Component {};

        void libraryChanged() override           { refresh(); }
        void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
        void timerCallback() override            { repaint(); }    // "2 min ago" moves on
        void refresh();
        void layoutList();
        void layoutCloud();
        juce::Rectangle<float> panelArea() const;
        bool roomy() const noexcept              { return getWidth() > 560 && getHeight() > 700; }
        juce::Rectangle<int> innerArea() const;
        void songMenu (const kcsync::Entry&, juce::Component& target);
        void renameSong (const kcsync::Entry&);
        void deleteSong (const kcsync::Entry&);
        void chooseNewSong();
        void setBusy (bool b, const juce::String& message = {});
        void paintCloud (juce::Graphics&);
        juce::String describe (const kcsync::Entry&) const;

        SongMapperPage& page;
        kcsync::SongLink& link;
        kcsync::SyncHub& hub;
        Assets& assets;

        KButton newButton, cloudButton, closeButton, backButton;
        juce::Viewport listViewport;
        ListContent listContent;
        juce::OwnedArray<SongRow> rows;

        KButton turnOnButton, joinButton, copyButton, syncNowButton, turnOffButton;
        juce::TextEditor codeEditor;
        bool cloudPage = false, busy = false;
        juce::String cloudMessage;
        struct Words { juce::Rectangle<int> area; juce::String text; int style; };   // 0 heading, 1 text, 2 status, 3 message
        std::vector<Words> words;                // the cloud page's writing
        juce::Rectangle<int> codeBox, statusArea;
        std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    };

    // A short message across the top for a few seconds ("Updated from your phone"); tap to dismiss
    class NoticeBanner : public juce::Component, private juce::Timer
    {
    public:
        explicit NoticeBanner (Assets& a) : assets (a) { setInterceptsMouseClicks (true, false); }
        void show (const juce::String& text, juce::Component& host, juce::Rectangle<int> area, float scale);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override  { setVisible (false); }
        const juce::String& getText() const noexcept     { return message; }
    private:
        void timerCallback() override                    { stopTimer(); setVisible (false); }
        Assets& assets;
        juce::String message;
    };
}
