// Key Compass - music theory helpers (names, scales, chords, key profiles)
#pragma once

#include <array>
#include <string>

namespace kc
{
    //==============================================================================
    enum class ChordType : int
    {
        Major = 0, Minor, Diminished, Augmented, Sus2, Sus4,
        Dominant7, Major7, Minor7, Power,
        NumTypes
    };

    constexpr int numChordTypes = (int) ChordType::NumTypes;

    struct ChordId
    {
        int root = -1;                 // pitch class 0..11, -1 = none
        ChordType type = ChordType::Major;
        int bass = -1;                 // pitch class of bass note if different from root (slash chord), else -1
        bool isSingleNote = false;     // a single pitch class, not a chord

        bool isValid() const noexcept  { return root >= 0; }
        bool sameHarmony (const ChordId& o) const noexcept
        {
            return root == o.root && isSingleNote == o.isSingleNote
                    && (isSingleNote || type == o.type);
        }
    };

    struct KeyId
    {
        int tonic = -1;                // pitch class 0..11, -1 = none
        bool minor = false;

        bool isValid() const noexcept          { return tonic >= 0; }
        int index() const noexcept             { return tonic + (minor ? 12 : 0); }
        static KeyId fromIndex (int i) noexcept { return i < 0 ? KeyId {} : KeyId { i % 12, i >= 12 }; }
        bool operator== (const KeyId& o) const noexcept { return tonic == o.tonic && minor == o.minor; }
        bool operator!= (const KeyId& o) const noexcept { return ! operator== (o); }
    };

    //==============================================================================
    // Names use real accidentals (U+266F sharp / U+266D flat), UTF-8 encoded.
    std::string pitchClassName (int pc, bool useFlats);
    std::string noteNameWithOctave (int midiNote, bool useFlats);
    std::string chordSuffix (ChordType t);
    std::string chordName (const ChordId& c, bool useFlats);
    std::string keyName (const KeyId& k, bool useFlats, bool upperCase);   // e.g. "F# MINOR" / "F# minor"
    std::string keyShortName (const KeyId& k, bool useFlats);             // e.g. "F#m" / "A"

    // Position of a pitch class on the circle of fifths (C=0, G=1, D=2 ... F=11)
    constexpr int circlePosition (int pc) noexcept     { return (pc * 7) % 12; }
    constexpr int pitchClassAtCircle (int pos) noexcept { return (pos * 7) % 12; } // 7*7=49=1 mod 12, self-inverse

    // True if pc belongs to the key (natural scale; for minor keys the raised
    // leading tone of harmonic minor is also accepted).
    bool isInKey (const KeyId& key, int pc) noexcept;

    // Triad chords that belong to the key, in the order shown by the UI:
    // major: I ii iii IV V vi   minor: i III iv v VI VII
    std::array<ChordId, 6> keyFamily (const KeyId& key) noexcept;

    // Circle-of-fifths positions (0..11) covered by the key's 7 scale notes: first position and count (always 7).
    int keyWedgeStart (const KeyId& key) noexcept;

    // Chord template intervals
    struct ChordTemplate { ChordType type; int numNotes; int intervals[4]; float penalty; };
    const std::array<ChordTemplate, numChordTypes>& chordTemplates() noexcept;
    bool chordContains (const ChordId& c, int pc) noexcept;

    // Maps a chord to a key-triad index for the "tonic chord" statistic: 0..11 major, 12..23 minor, -1 other.
    int triadClassIndex (const ChordId& c) noexcept;

    //==============================================================================
    // Key profiles (index 0 = tonic)
    const std::array<float, 12>& majorProfile() noexcept;
    const std::array<float, 12>& minorProfile() noexcept;
}
