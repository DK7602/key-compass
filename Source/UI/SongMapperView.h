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

    //==============================================================================
    class HeaderView;
    class ChordPanel;
    class TimelinePanel;
    class BrowserPanel;
    class DragMedallion;
    class MapperBackdrop;
    class PracticeView;

    class SongMapperPage : public juce::Component, private juce::ChangeListener, private juce::Timer
    {
    public:
        SongMapperPage (KeyCompassProcessor&, Assets&);
        ~SongMapperPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override;

        std::function<void()> onSwitchToCompass;

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
        void showSongMenu();

        std::unique_ptr<MapperBackdrop> backdrop;
        std::unique_ptr<HeaderView> header;
        std::unique_ptr<ChordPanel> chordPanel;
        std::unique_ptr<TimelinePanel> timeline;
        std::unique_ptr<BrowserPanel> browser;
        std::unique_ptr<DragMedallion> medallion;
        std::unique_ptr<PracticeView> practice;
        KButton switchButton, tapButton, captureButton, songButton, helpButton;
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
