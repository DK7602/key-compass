// Key Compass - Song Mapper page
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Controls.h"
#include "../SongMapper/SongModel.h"
#include "../SongMapper/ProgressionLibrary.h"

class KeyCompassProcessor;

namespace kcui
{
    class SongMapperPage;

    //==============================================================================
    // Small building blocks
    class KButton : public juce::Button
    {
    public:
        enum class Look { Plaque, Enamel, Round };
        KButton (Assets& a, const juce::String& text, Look look = Look::Enamel);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        float fontSize = 0.0f;      // 0 = automatic
        bool lit = false;           // shown "on" (gold glow)
    private:
        void paintFace (juce::Graphics&, bool highlighted, bool down);
        Assets& assets;
        Look look;
    };

    class MenuBox : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit MenuBox (Assets& a);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }

        juce::String text, caption;
        float fontSize = 0.0f;
        std::function<void (juce::PopupMenu&)> buildMenu;
        std::function<void (int)> onResult;
    private:
        Assets& assets;
    };

    void paintBrassPanel (juce::Graphics&, Assets&, juce::Rectangle<float> r, const juce::String& title);
    void paintParchment (juce::Graphics&, Assets&, juce::Rectangle<float> r, float corner);
    // where a guitar chord diagram's strings and frets sit inside 'area' (shared by drawing and clicking)
    struct GuitarGeometry
    {
        juce::Rectangle<float> area, grid;
        float sw = 0, fh = 0, topMarks = 0;      // string spacing, fret height, room above the nut
        int base = 1;                            // fret shown at the top
    };
    GuitarGeometry guitarGeometry (const sm::GuitarVoicing&, juce::Rectangle<float> area, bool compact, float scale = 1.0f);
    std::array<int, 6> fingeringFor (const sm::GuitarVoicing&);      // 1-4 per string, 0 = not fretted
    void drawGuitarDiagram (juce::Graphics&, Assets&, const sm::GuitarVoicing&, juce::Rectangle<float> area, bool compact,
                            float scale = 1.0f, const int* fingers = nullptr);
    void drawKeyboard (juce::Graphics&, juce::Rectangle<float> area, const std::vector<int>& notes, int lowNote, int numWhite,
                       float scale = 1.0f);

    // Phone / tablet layouts: the brass-framed parchment panel drawn at any size, with a title on its scroll
    // (on the desktop the panels are part of the painted page)
    void paintFramedPanel (juce::Graphics&, Assets&, juce::Rectangle<float> r, const juce::String& title);

    // Press and hold on a touch screen stands in for the right mouse button (the menus)
    class LongPress : private juce::Timer
    {
    public:
        std::function<void()> onLongPress;
        void mouseDown (const juce::MouseEvent&);      // starts the clock (touch screens only)
        void mouseDrag (const juce::MouseEvent&);      // moving the finger cancels it
        void mouseUp()                 { stopTimer(); }
        void cancel()                  { stopTimer(); }
        bool fired() const noexcept    { return didFire; }   // since the last press
        static constexpr int holdMs = 430;
    private:
        void timerCallback() override;
        bool didFire = false;
    };

    //==============================================================================
    class HeaderView;
    class ChordPanel;
    class TimelinePanel;
    class BrowserPanel;
    class DragMedallion;
    class MapperBackdrop;
    class PracticeView;
    class MySongsView;
    class NoticeBanner;

    class SongMapperPage : public juce::Component, private juce::ChangeListener, private juce::Timer
    {
    public:
        SongMapperPage (KeyCompassProcessor&, Assets&);
        ~SongMapperPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override;

        std::function<void()> onSwitchToCompass;

        // ---- phone / tablet layouts (the Android app) ----
        // Each panel is shown on its own at 'scale' (dp per design pixel) in the given area of this page;
        // the page's painted backdrop, plaques and plates are hidden (the app has its own tabs and settings).
        struct MobileLayout
        {
            bool active = false;
            float scale = 1.0f;
            juce::Rectangle<int> header, chords, timeline, browser;   // empty = not shown
            bool hideTitles = false;                                   // the header sits over the panels' title scrolls
        };
        void setMobileLayout (const MobileLayout&);
        bool isMobileLayout() const noexcept                { return mobile.active; }
        const MobileLayout& getMobileLayout() const noexcept { return mobile; }
        std::function<juce::Component*()> practiceHost;      // the component the practice view covers (the app's screen)
        std::function<void()> onBeforeMenu;                    // e.g. hide the on-screen keyboard

        // ---- sharing (touch screens) and song files (everywhere) ----
        void shareMidi (int onlySection, int laneMask = -1);  // the share sheet (Drive, Gmail, Files...)
        void shareSongFile();
        void saveSongFile();                                   // desktop: a save dialog
        void openSongFile();
        bool loadSongXml (const juce::String& xmlText);        // false if it isn't a Key Compass song
        juce::String songFileXml() const;
        void setLockKey (bool lock);
        void showSongMenu();

        // ---- My Songs and cloud sync (see MySongsView.h, Sync/SongLink.h) ----
        void showMySongs (bool cloudPage = false);
        void closeMySongs();
        bool isMySongsOpen() const noexcept;
        MySongsView* mySongsView() const noexcept     { return mySongs.get(); }
        void showNotice (const juce::String& text, int milliseconds = 0);   // a short message across the top (0: as long as it takes to read)
        NoticeBanner* noticeBanner() const noexcept   { return notice.get(); }
        // Where a change shows up, for those messages: the phone app says which tab ("They're on the CHORDS tab.")
        // when that panel isn't on the screen. Empty when it is (and always on the PC).
        enum class Spot { progression = 0, song = 1, library = 2 };
        std::function<juce::String (int spot)> tabShowing;          // set by the phone app: the tab name, or "" if on screen
        // before + "CHORDS" + after when the panel is on another tab, otherwise 'otherwise'
        juce::String seeItOn (Spot, const juce::String& before, const juce::String& after, const juce::String& otherwise = {}) const;
        juce::String chordList (const std::vector<sm::Chord>&) const;   // "C · G · Am · F"
        void layoutOverlays();                        // the app's screen changed size
        void addTemplateItems (juce::PopupMenu&);     // "New song" choices (ids 11-19)
        bool startNewSong (int itemId);               // false if it isn't one of them
        void askAboutNewerVersion();

        // ---- shared state & helpers for the panels ----
        KeyCompassProcessor& proc;
        Assets& assets;
        sm::SongMap& song;
        juce::SharedResourcePointer<sm::FavouritesStore> favourites;
        bool flats = false;
        sm::Chord focused;
        int playingSection = -1, playingChord = -1, playingTray = -1;
        int playScope = -2;                            // what is playing: -1 the song, >= 0 one section, -2 the progression / a preview

        // Where the Song Mapper's key comes from. With Lock Key off it follows, in this order: what Key Compass
        // is hearing right now, the song's own chords, the last key heard, the last key you set.
        enum class KeySource { None, Set, Heard, Chords, Kept };
        KeySource keySource (sm::Key* key = nullptr, bool ignoreLock = false) const;
        sm::Key currentKey() const;
        bool keyIsDetected() const;
        bool keyKnown() const;
        double effectiveBpm() const;
        bool tempoFromHost() const;
        juce::String keyText() const;
        bool useFlats() const;                         // the FLATS switch, or the key is a "flat" key (F, Bb, Dm...)
        static bool keyUsesFlats (int tonic, bool minor) noexcept;
        juce::String chordText (const sm::Chord& c) const { return juce::String (juce::CharPointer_UTF8 (c.name (useFlats()).c_str())); }

        void focusChord (const sm::Chord& c, bool audition = true);
        void auditionChord (const sm::Chord& c);
        void auditionNote (int midiNote, int sound = -1);   // sound: -1 the chord instrument, 0 piano, 1 bass, 2 guitar
        void playSong (int onlySection);
        void playTray();
        void playProgression (const std::vector<sm::Chord>& chords);
        void stopPlayback();
        bool isPlaying() const;
        void tapTempo();
        void captureDetectedChords();
        void addToSelectedSection (const std::vector<sm::Chord>& chords);
        void useProgression (const std::vector<sm::Chord>& chords);       // the library's USE: it becomes the progression
        void addAsNewSection (const std::vector<sm::Chord>& chords);
        void clearProgression();                                           // with a message (Undo brings it back)
        void clearSongChords();
        juce::File midiFile (int onlySection, int laneMask = -1) const;   // -1 = every row that is showing
        int visibleLanes() const noexcept { return song.showLines ? sm::allLanes : sm::laneBit (sm::Lane::Chords); }
        int playLanes() const noexcept    { return visibleLanes() & song.audibleLanes(); }
        bool isNotePlaying (int section, sm::Lane lane, int noteIndex) const;
        double sectionPlayhead (int section) const;    // beats into this section while it plays, else -1
        void refreshPlayback();                        // after an edit: the playing song carries on with the change
        void updatePlaybackDisplay();                  // what is sounding now -> highlights (30 times a second)
        void showSection (int index);                  // scroll the song list so this section is at the top
        int lastAuditionedNote = -1;                   // the last single note played (diagram / roll clicks)

        // practice view: the chords big, to play along by hand. source: -3 automatic, -2 the progression,
        // -1 the chord picked, >= 0 a song section
        void showPractice (int source = -3);
        void closePractice();
        bool isPracticeOpen() const noexcept { return practice != nullptr && practiceSource() > -100; }
        int practiceSource() const;
        juce::Component* practiceView() const noexcept;   // for tests
        int practiceSize[2] = { 3, 5 };                   // practice card size step for piano / guitar (4 and 8 to a page)
        void exportMidi (int onlySection);
        void saveTrayAsFavourite();
        // menu item ids are idBase + the chord's index in outChords
        void fillChordMenu (juce::PopupMenu& m, int idBase, std::vector<sm::Chord>& outChords) const;
        static int chordMenuIndex (int result, int idBase, const std::vector<sm::Chord>& chords) noexcept
        {
            const int i = result - idBase;
            return result >= idBase && i < (int) chords.size() ? i : -1;
        }
        void setFlats (bool f);
        void refreshAll();
        void renameSong();
        void syncControls();
        void takeFocus();                 // so Ctrl+Z / space work after clicking in the page
        bool keyPressed (const juce::KeyPress&) override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;
        void layoutMobile();
        juce::File songFileFor (const juce::String& name) const;

        std::unique_ptr<MapperBackdrop> backdrop;
        std::unique_ptr<HeaderView> header;
        std::unique_ptr<ChordPanel> chordPanel;
        std::unique_ptr<TimelinePanel> timeline;
        std::unique_ptr<BrowserPanel> browser;
        std::unique_ptr<DragMedallion> medallion;
        std::unique_ptr<PracticeView> practice;
        std::unique_ptr<MySongsView> mySongs;
        std::unique_ptr<NoticeBanner> notice;
        bool askingNewer = false;
        juce::Component* overlayHost (float& scale, juce::Rectangle<int>& area);
        KButton switchButton, tapButton, captureButton, songButton, helpButton;
        KButton undoButton, redoButton;                // phone / tablet layouts: either side of the KEY / TEMPO plaque
        MobileLayout mobile;
        juce::ScopedMessageBox shareBox;
        // the shared control plates at the bottom, with Song Mapper settings in them
        PlainChoiceBox playStyleBox, meterBox;
        PlainStepper octaveBox;
        RoundToggle lockKey;
        PlateButton clearSong;
        juce::Slider volume;
        std::vector<double> taps;
        std::vector<int> playingNotes;                 // (section << 16) | (lane << 12) | note index
        std::unique_ptr<juce::FileChooser> chooser;
        int lastKeyIndex = -100;
        mutable double lastHeardMs = -1.0e12;          // when Key Compass last heard something with a key in it
        double lastBpm = -1.0;
    };
}
