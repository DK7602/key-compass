// Song Mapper - chords, roman numerals and voicings (plain C++, no JUCE)
#pragma once

#include <array>
#include <string>
#include <vector>

namespace sm
{
    //==============================================================================
    struct Quality
    {
        const char* id;          // stable id used when saving
        const char* suffix;      // shown after the root, e.g. "m7"
        const char* longName;    // e.g. "minor 7"
        std::vector<int> intervals;
        bool minorish;           // lower-case roman numeral
    };

    const std::vector<Quality>& qualities();
    int qualityIndex (const std::string& id);   // -1 if unknown

    struct Chord
    {
        int root = -1;      // pitch class 0..11
        int quality = 0;    // index into qualities()
        int bass = -1;      // slash bass pitch class, -1 = none

        bool isValid() const noexcept { return root >= 0; }
        bool operator== (const Chord& o) const noexcept { return root == o.root && quality == o.quality && bass == o.bass; }
        bool operator!= (const Chord& o) const noexcept { return ! operator== (o); }

        std::vector<int> pitchClasses() const;          // root first
        std::string name (bool flats) const;            // "F#m7/C#" with real accidentals (UTF-8)
        std::string longName (bool flats) const;        // "F# minor 7 over C#"
        std::string id() const;                         // ASCII, parseable: "F#m7/C#"
    };

    std::string pcName (int pc, bool flats);            // UTF-8 with sharp/flat glyphs
    std::string pcNameAscii (int pc, bool flats);       // "F#", "Bb"

    // Parses "F#m7", "Bbmaj9", "C/E", "Gsus", "A-7", "Dadd9" ... returns invalid Chord on failure
    Chord parseChord (const std::string& text);

    //==============================================================================
    // Keys: tonic pitch class + minor flag
    struct Key { int tonic = 9; bool minor = true; };

    // Roman numerals relative to a key, e.g. "bVII7", "ii7", "V/vi" not supported.
    // A progression written for a minor key (tonic numeral lower-case) played in a major key
    // is moved to the relative minor (and vice-versa) so it keeps the key's notes.
    Chord romanToChord (const std::string& numeral, int tonicPc);
    std::string chordToRoman (const Chord& c, int tonicPc);
    bool progressionIsMinor (const std::vector<std::string>& numerals);
    std::vector<Chord> resolveProgression (const std::vector<std::string>& numerals, const Key& key);
    std::vector<std::string> splitNumerals (const std::string& text);

    // Diatonic triads/sevenths of a key (7 chords, in scale order)
    std::vector<Chord> diatonicChords (const Key& key, bool sevenths);

    // A chord and how long it sounds (any unit, e.g. beats), in playing order
    struct TimedChord { Chord chord; double weight = 1.0; };

    // The key that best fits a run of chords: how many of their notes are in the key's scale,
    // how long the key's own chord sounds, and whether the song starts / ends on it.
    // Returns false when there are no chords. relativeMargin (optional): how far ahead of the
    // best other key it is (0 = a tie, 1 = nothing else fits at all).
    bool keyFromChords (const std::vector<TimedChord>& chords, Key& out, float* relativeMargin = nullptr);

    //==============================================================================
    enum class GuitarStyle : int { Standard = 0, OpenEasy, Triad, Barre, Power, Shell, Inversion, Color, NumStyles };
    const char* guitarStyleName (GuitarStyle s);

    struct GuitarVoicing
    {
        std::array<int, 6> frets { { -1, -1, -1, -1, -1, -1 } };   // low E .. high E, -1 = muted
        int barreFret = 0;          // 0 = no barre
        int barreFrom = 0, barreTo = 0;   // string indices covered by the barre
        bool valid = false;
        Chord shown;                // the chord actually voiced (styles can simplify or extend it)

        int lowestFret() const;     // lowest fretted (non-open) fret, 0 if none
        int highestFret() const;
        std::vector<int> midiNotes() const;
    };

    constexpr std::array<int, 6> standardTuning { { 40, 45, 50, 55, 59, 64 } };

    // The chord a style actually plays (e.g. Power -> root+5, Color -> adds a 9th)
    Chord chordForStyle (const Chord& c, GuitarStyle style);
    GuitarVoicing guitarVoicing (const Chord& c, GuitarStyle style);

    // Piano: left-hand bass + right-hand chord, sorted MIDI notes
    std::vector<int> pianoVoicing (const Chord& c, GuitarStyle style);
}
