// Key Compass - music theory helpers
#include "MusicTheory.h"

namespace kc
{
    static const char* const sharpNames[12] = { "C", "C\xE2\x99\xAF", "D", "D\xE2\x99\xAF", "E", "F",
                                                "F\xE2\x99\xAF", "G", "G\xE2\x99\xAF", "A", "A\xE2\x99\xAF", "B" };
    static const char* const flatNames[12]  = { "C", "D\xE2\x99\xAD", "D", "E\xE2\x99\xAD", "E", "F",
                                                "G\xE2\x99\xAD", "G", "A\xE2\x99\xAD", "A", "B\xE2\x99\xAD", "B" };

    std::string pitchClassName (int pc, bool useFlats)
    {
        if (pc < 0) return "-";
        pc %= 12;
        return useFlats ? flatNames[pc] : sharpNames[pc];
    }

    std::string noteNameWithOctave (int midiNote, bool useFlats)
    {
        if (midiNote < 0) return "-";
        return pitchClassName (midiNote % 12, useFlats) + std::to_string (midiNote / 12 - 1);
    }

    std::string chordSuffix (ChordType t)
    {
        switch (t)
        {
            case ChordType::Major:      return "";
            case ChordType::Minor:      return "m";
            case ChordType::Diminished: return "dim";
            case ChordType::Augmented:  return "aug";
            case ChordType::Sus2:       return "sus2";
            case ChordType::Sus4:       return "sus4";
            case ChordType::Dominant7:  return "7";
            case ChordType::Major7:     return "maj7";
            case ChordType::Minor7:     return "m7";
            case ChordType::Power:      return "5";
            case ChordType::NumTypes:   break;
        }
        return "";
    }

    std::string chordName (const ChordId& c, bool useFlats)
    {
        if (! c.isValid()) return "-";
        if (c.isSingleNote) return pitchClassName (c.root, useFlats);
        auto s = pitchClassName (c.root, useFlats) + chordSuffix (c.type);
        if (c.bass >= 0 && c.bass != c.root)
            s += "/" + pitchClassName (c.bass, useFlats);
        return s;
    }

    std::string keyName (const KeyId& k, bool useFlats, bool upperCase)
    {
        if (! k.isValid()) return "-";
        return pitchClassName (k.tonic, useFlats) + (upperCase ? (k.minor ? " MINOR" : " MAJOR")
                                                               : (k.minor ? " minor" : " major"));
    }

    std::string keyShortName (const KeyId& k, bool useFlats)
    {
        if (! k.isValid()) return "-";
        return pitchClassName (k.tonic, useFlats) + (k.minor ? "m" : "");
    }

    bool isInKey (const KeyId& key, int pc) noexcept
    {
        if (! key.isValid()) return false;
        static const bool majorScale[12] = { 1,0,1,0,1,1,0,1,0,1,0,1 };
        static const bool minorScale[12] = { 1,0,1,1,0,1,0,1,1,0,1,1 }; // natural minor + raised 7th
        const int iv = ((pc - key.tonic) % 12 + 12) % 12;
        return key.minor ? minorScale[iv] : majorScale[iv];
    }

    std::array<ChordId, 6> keyFamily (const KeyId& key) noexcept
    {
        std::array<ChordId, 6> f {};
        if (! key.isValid()) return f;
        const int t = key.tonic;
        auto mk = [] (int root, ChordType type) { ChordId c; c.root = ((root % 12) + 12) % 12; c.type = type; return c; };

        if (! key.minor)
            f = { mk (t, ChordType::Major), mk (t + 2, ChordType::Minor), mk (t + 4, ChordType::Minor),
                  mk (t + 5, ChordType::Major), mk (t + 7, ChordType::Major), mk (t + 9, ChordType::Minor) };
        else
            f = { mk (t, ChordType::Minor), mk (t + 3, ChordType::Major), mk (t + 5, ChordType::Minor),
                  mk (t + 7, ChordType::Minor), mk (t + 8, ChordType::Major), mk (t + 10, ChordType::Major) };
        return f;
    }

    int keyWedgeStart (const KeyId& key) noexcept
    {
        if (! key.isValid()) return 0;
        // Major key: scale notes occupy circle positions (tonic-1 .. tonic+5).
        // Minor key: same notes as its relative major (tonic + 3 semitones).
        const int majorTonic = key.minor ? (key.tonic + 3) % 12 : key.tonic;
        return (circlePosition (majorTonic) + 11) % 12;
    }

    const std::array<ChordTemplate, numChordTypes>& chordTemplates() noexcept
    {
        static const std::array<ChordTemplate, numChordTypes> t {{
            { ChordType::Major,      3, { 0, 4, 7, 0 },  0.000f },
            { ChordType::Minor,      3, { 0, 3, 7, 0 },  0.000f },
            { ChordType::Diminished, 3, { 0, 3, 6, 0 },  0.060f },
            { ChordType::Augmented,  3, { 0, 4, 8, 0 },  0.080f },
            { ChordType::Sus2,       3, { 0, 2, 7, 0 },  0.060f },
            { ChordType::Sus4,       3, { 0, 5, 7, 0 },  0.050f },
            { ChordType::Dominant7,  4, { 0, 4, 7, 10 }, 0.035f },
            { ChordType::Major7,     4, { 0, 4, 7, 11 }, 0.045f },
            { ChordType::Minor7,     4, { 0, 3, 7, 10 }, 0.035f },
            { ChordType::Power,      2, { 0, 7, 0, 0 },  0.070f },
        }};
        return t;
    }

    bool chordContains (const ChordId& c, int pc) noexcept
    {
        if (! c.isValid()) return false;
        if (c.isSingleNote) return c.root == pc;
        const auto& t = chordTemplates()[(size_t) c.type];
        for (int i = 0; i < t.numNotes; ++i)
            if ((c.root + t.intervals[i]) % 12 == pc)
                return true;
        return false;
    }

    int triadClassIndex (const ChordId& c) noexcept
    {
        if (! c.isValid() || c.isSingleNote) return -1;
        switch (c.type)
        {
            case ChordType::Major: case ChordType::Dominant7: case ChordType::Major7: return c.root;
            case ChordType::Minor: case ChordType::Minor7:                             return c.root + 12;
            case ChordType::Diminished: case ChordType::Augmented: case ChordType::Sus2:
            case ChordType::Sus4: case ChordType::Power: case ChordType::NumTypes:     break;
        }
        return -1;
    }

    //==============================================================================
    // Blend of the Temperley (Kostka-Payne) and Krumhansl-Kessler profiles,
    // each normalised to unit mean before averaging.
    static std::array<float, 12> blend (const float* a, const float* b)
    {
        float sa = 0, sb = 0;
        for (int i = 0; i < 12; ++i) { sa += a[i]; sb += b[i]; }
        std::array<float, 12> r {};
        for (int i = 0; i < 12; ++i)
            r[(size_t) i] = 0.5f * (a[i] * 12.0f / sa) + 0.5f * (b[i] * 12.0f / sb);
        return r;
    }

    static const float temperleyMajor[12] = { 0.748f, 0.060f, 0.488f, 0.082f, 0.670f, 0.460f, 0.096f, 0.715f, 0.104f, 0.366f, 0.057f, 0.400f };
    static const float temperleyMinor[12] = { 0.712f, 0.084f, 0.474f, 0.618f, 0.049f, 0.460f, 0.105f, 0.747f, 0.404f, 0.067f, 0.133f, 0.330f };
    static const float kkMajor[12]        = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float kkMinor[12]        = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    const std::array<float, 12>& majorProfile() noexcept
    {
        static const auto p = blend (temperleyMajor, kkMajor);
        return p;
    }

    const std::array<float, 12>& minorProfile() noexcept
    {
        static const auto p = blend (temperleyMinor, kkMinor);
        return p;
    }
}
