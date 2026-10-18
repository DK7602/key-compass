// Song Mapper - the song being mapped (sections of chords), its settings, favourites and MIDI export.
// Lives in the processor so it is saved with the DAW project; edited on the message thread only.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include "ChordTheory.h"
#include <map>

namespace sm
{
    // A single note of a bass or lead line. Times are in quarter-note beats from the start of the line's loop.
    struct Note
    {
        double start = 0.0, length = 1.0;
        int pitch = 60;
        float velocity = 0.8f;
        bool operator== (const Note& o) const noexcept
        {
            return std::abs (start - o.start) < 1.0e-6 && std::abs (length - o.length) < 1.0e-6 && pitch == o.pitch
                   && std::abs (velocity - o.velocity) < 1.0e-4f;
        }
        bool operator!= (const Note& o) const noexcept { return ! operator== (o); }
    };

    // A bass or lead line in a section: a pattern of notes that repeats every loopBars (0 = the whole section)
    struct Line
    {
        bool enabled = false;
        int loopBars = 2;
        std::vector<Note> notes;
        bool operator== (const Line& o) const noexcept { return enabled == o.enabled && loopBars == o.loopBars && notes == o.notes; }
        bool operator!= (const Line& o) const noexcept { return ! operator== (o); }
    };

    enum class Lane : int { Chords = 0, Bass = 1, Lead = 2 };
    constexpr int laneBit (Lane l) noexcept { return 1 << (int) l; }
    constexpr int allLanes = 7;

    // How a bass line can be filled in from the section's chords
    enum class BassFill : int { Roots = 0, RootPulse, RootFifth, Octaves, Walking, NumFills };
    const char* bassFillName (BassFill f);

    struct Section
    {
        juce::String name;
        int bars = 8;
        float barsPerChord = 1.0f;          // 0 = spread the chords evenly over the section
        std::vector<Chord> chords;
        Line bass, lead;
        juce::uint32 uid = 0;
    };

    struct SequenceEvent
    {
        double startBeat = 0.0, lengthBeats = 0.0;   // quarter-note beats
        std::vector<int> notes;                      // sorted low to high
        float velocity = 0.8f;                       // 0..1
        float strum = 0.0f;                          // beats between strings: > 0 low-to-high (down), < 0 high-to-low (up), 0 = together
        int section = -1, chordIndex = -1;           // what to light up while it plays (-1/-1 = tray item)
        int trayIndex = -1;
        int lane = 0;                                // Lane: 0 chords, 1 bass, 2 lead
        int noteIndex = -1;                          // for bass / lead notes: index in the section's line
        int sound = -1;                              // -1 = the sequence's instrument (guitar or piano), 0 = piano, 1 = bass, 2 = guitar
    };

    struct Sequence
    {
        struct Span { int section = -1; double start = 0.0, length = 0.0; };
        std::vector<SequenceEvent> events;
        std::vector<Span> spans;                     // where each section sits in this sequence
        double totalBeats = 0.0;
        int sectionAt (double beat, double* offsetInSection = nullptr) const noexcept
        {
            for (const auto& s : spans)
                if (beat >= s.start && beat < s.start + s.length)
                {
                    if (offsetInSection != nullptr) *offsetInSection = beat - s.start;
                    return s.section;
                }
            return -1;
        }
    };

    struct Favourite { juce::String name, roman, genre; };

    // How chords are played back and written to MIDI
    enum class PlayStyle : int { Block = 0, Pulse, Strum, Arpeggio, NumStyles };
    const char* playStyleName (PlayStyle s);
    const char* playStyleDescription (PlayStyle s);

    //==============================================================================
    class SongMap : public juce::ChangeBroadcaster
    {
    public:
        SongMap();

        // ---- the song ----
        std::vector<Section> sections;
        std::vector<Chord> tray;                 // working progression shown with diagrams
        int selectedSection = -1;
        int selectedTray = -1;
        juce::String songName { "Song Map" };
        juce::String songId;                     // which song in My Songs this is (see Sync/SongLibrary.h); "" = not saved there yet
        juce::int64 libraryStamp = 0;            // the My Songs version this copy matches (to notice a newer one from another device)

        // ---- settings ----
        bool guitar = true;
        GuitarStyle style = GuitarStyle::Standard;
        int keyOverride = -1;                    // 0..23 (tonic + 12*minor), -1 = follow the key (heard, or from the chords)
        int lastSetKey = -1;                     // the last key set by hand: kept when Lock Key is turned off and nothing else gives a key
        bool followHostTempo = true;
        double manualBpm = 120.0;
        int timeSignature = 0;                   // 0 = 4/4, 1 = 3/4, 2 = 6/8
        int genre = 0, genreStyle = 0;
        bool showFavourites = false;
        float auditionVolume = 0.7f;
        bool loopPlayback = false;
        PlayStyle playStyle = PlayStyle::Block;
        int octave = 0;                          // -2..+2, shifts the chords' preview sound and MIDI
        bool showLines = true;                   // show the Bass / Lead rows in the timeline
        bool muteChords = false, muteBass = false, muteLead = false;   // preview only

        // ---- edits (all send a change message) ----
        int addSection (const juce::String& name, int bars = 8);
        void removeSection (int index);
        void moveSection (int from, int to);
        void duplicateSection (int index);
        void renameSection (int index, const juce::String& name);
        void setSectionBars (int index, int bars);
        void setSectionBarsPerChord (int index, float barsPerChord);
        void insertChords (int section, const std::vector<Chord>& chords, int at = -1);
        void removeChord (int section, int index);
        void moveChord (int fromSection, int fromIndex, int toSection, int toIndex);
        void replaceChord (int section, int index, const Chord& c);

        void clearSection (int index);
        void clearAllChords();                   // CLEAR SONG: empties every section (chords and notes), keeps the structure

        // ---- bass / lead lines ----
        Line& line (int section, Lane lane);
        const Line& line (int section, Lane lane) const;
        void setLineEnabled (int section, Lane lane, bool on);
        void setLineLoop (int section, Lane lane, int loopBars);
        void setLineNotes (int section, Lane lane, std::vector<Note> notes, bool undoable = true);
        void fillBass (int section, BassFill pattern);
        double sectionBeats (int section) const;
        double lineLoopBeats (int section, Lane lane) const;    // length of one repeat of the line
        int audibleLanes() const noexcept        { return (muteChords ? 0 : 1) | (muteBass ? 0 : 2) | (muteLead ? 0 : 4); }
        void resetSections (const std::vector<std::pair<juce::String, int>>& layout);   // a new song from a template

        void setTray (const std::vector<Chord>& chords);
        void insertTray (const Chord& c, int at = -1);
        void removeTray (int index);
        void moveTray (int from, int to);
        void replaceTray (int index, const Chord& c);
        void clearTray();

        juce::String nextSectionName (const juce::String& base) const;

        // ---- key ----
        void setKeyOverride (int key);           // 0..23 locks that key (and remembers it in lastSetKey), -1 = follow
        // The key the song's own chords point to: every section, each chord weighted by how long it plays
        // (the progression tray if no section has chords yet). false = there are no chords anywhere.
        bool keyFromChords (Key& out) const;
        void changed() { sendChangeMessage(); }

        // ---- undo (sections + progression only; settings are not undone) ----
        void checkpoint();                       // call before an edit (the edit methods above do it themselves)
        bool canUndo() const noexcept            { return ! undoStack.empty(); }
        bool canRedo() const noexcept            { return ! redoStack.empty(); }
        void undo();
        void redo();

        // ---- timing ----
        double beatsPerBar() const noexcept      { return timeSignature == 0 ? 4.0 : 3.0; }
        int timeSigNumerator() const noexcept    { return timeSignature == 0 ? 4 : (timeSignature == 1 ? 3 : 6); }
        int timeSigDenominator() const noexcept  { return timeSignature == 2 ? 8 : 4; }
        juce::String timeSigName() const         { return timeSignature == 0 ? "4/4" : (timeSignature == 1 ? "3/4" : "6/8"); }
        int totalBars() const;

        // ---- sound / notes ----
        std::vector<int> notesFor (const Chord& c) const;      // using the current instrument and voicing style
        // -1 = whole song. skipEmpty: leave out sections with nothing in them (the preview), so a loop never plays bars of silence
        Sequence buildSequence (int onlySection = -1, int laneMask = allLanes, bool skipEmpty = false) const;
        bool sectionHasContent (int section) const;
        Sequence buildTraySequence() const;
        Sequence sequenceFor (const std::vector<Chord>& chords) const;   // one bar per chord (browser previews)
        std::vector<int> playedNotes (const Chord& c) const;            // notesFor + octave shift

        // ---- MIDI ----
        // laneMask: which rows to include. More than one row with notes = one MIDI track per row.
        juce::MidiFile createMidi (int onlySection, double bpm, int laneMask = laneBit (Lane::Chords)) const;
        juce::File writeMidiFile (int onlySection, double bpm, const juce::File& folder, int laneMask = allLanes) const;
        bool laneHasNotes (int onlySection, Lane lane) const;

        // ---- state ----
        juce::ValueTree toValueTree() const;
        void fromValueTree (const juce::ValueTree&, bool keepUndo = false);   // keepUndo: Undo brings back what was there before

    private:
        void addLineNotes (Sequence& seq, int section, Lane lane, double sectionStart) const;
        void fillBassInternal (int section, BassFill pattern);
        void addChordHits (Sequence& seq, const std::vector<int>& notes, double start, double length, double gridStart,
                           int section, int chordIndex, int trayIndex) const;

        struct Snapshot
        {
            std::vector<Section> sections;
            std::vector<Chord> tray;
            int selectedSection = -1, selectedTray = -1;
            bool sameAs (const SongMap& m) const;
        };
        Snapshot capture() const;
        void restore (const Snapshot&);
        std::vector<Snapshot> undoStack, redoStack;

        juce::uint32 nextUid = 1;
        mutable std::map<std::string, std::vector<int>> noteCache;
        mutable bool cacheGuitar = true;
        mutable GuitarStyle cacheStyle = GuitarStyle::Standard;
    };

    //==============================================================================
    // Favourite progressions, shared by every instance and project (a small JSON file in the user's app data).
    // Cloud sync reads and replaces it from its own thread, so everything here is locked.
    class FavouritesStore : public juce::ChangeBroadcaster
    {
    public:
        explicit FavouritesStore (const juce::File& fileToUse = {});     // default: favourites.json in the app's data
        std::vector<Favourite> all() const;
        bool contains (const juce::String& roman) const;
        void add (const Favourite& f);
        void remove (const juce::String& roman);
        void toggle (const Favourite& f);

        // ---- cloud sync ----
        std::function<void()> onUserChange;                  // starred or unstarred here (not by sync)
        juce::String toJson() const;
        static std::vector<Favourite> parse (const juce::String& json);
        static std::vector<Favourite> merge (const std::vector<Favourite>& a, const std::vector<Favourite>& b);
        void replaceAll (const std::vector<Favourite>& newItems);    // from the cloud: saved, no onUserChange

    private:
        void load();
        void save() const;
        void userChanged();
        juce::File file;
        std::vector<Favourite> items;
        mutable juce::CriticalSection lock;
    };
}
