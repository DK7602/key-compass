// Song Mapper - chords, roman numerals and voicings
#include "ChordTheory.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace sm
{
    namespace
    {
        const char* const sharpUtf8[12] = { "C", "C\xE2\x99\xAF", "D", "D\xE2\x99\xAF", "E", "F", "F\xE2\x99\xAF", "G", "G\xE2\x99\xAF", "A", "A\xE2\x99\xAF", "B" };
        const char* const flatUtf8[12]  = { "C", "D\xE2\x99\xAD", "D", "E\xE2\x99\xAD", "E", "F", "G\xE2\x99\xAD", "G", "A\xE2\x99\xAD", "A", "B\xE2\x99\xAD", "B" };
        const char* const sharpAscii[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const char* const flatAscii[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

        inline int wrap12 (int v) noexcept { return ((v % 12) + 12) % 12; }

        // replace UTF-8 accidentals / symbols with ASCII equivalents used by the parsers
        std::string normalise (std::string s)
        {
            const std::pair<const char*, const char*> map[] = {
                { "\xE2\x99\xAF", "#" }, { "\xE2\x99\xAD", "b" }, { "\xC2\xB0", "dim" }, { "\xC3\xB8", "m7b5" },
                { "\xCE\x94", "maj" }, { "\xE2\x88\x86", "maj" }, { "\xE2\x80\x93", "-" }, { "\xE2\x88\x92", "-" }
            };
            for (const auto& [from, to] : map)
            {
                size_t pos;
                while ((pos = s.find (from)) != std::string::npos)
                    s.replace (pos, std::strlen (from), to);
            }
            s.erase (std::remove_if (s.begin(), s.end(), [] (unsigned char ch) { return std::isspace (ch); }), s.end());
            return s;
        }
    }

    //==============================================================================
    const std::vector<Quality>& qualities()
    {
        static const std::vector<Quality> q = {
            { "maj",   "",      "major",            { 0, 4, 7 },             false },
            { "m",     "m",     "minor",            { 0, 3, 7 },             true  },
            { "5",     "5",     "power chord",      { 0, 7 },                false },
            { "dim",   "dim",   "diminished",       { 0, 3, 6 },             true  },
            { "aug",   "aug",   "augmented",        { 0, 4, 8 },             false },
            { "sus2",  "sus2",  "suspended 2",      { 0, 2, 7 },             false },
            { "sus4",  "sus4",  "suspended 4",      { 0, 5, 7 },             false },
            { "6",     "6",     "6",                { 0, 4, 7, 9 },          false },
            { "m6",    "m6",    "minor 6",          { 0, 3, 7, 9 },          true  },
            { "7",     "7",     "dominant 7",       { 0, 4, 7, 10 },         false },
            { "maj7",  "maj7",  "major 7",          { 0, 4, 7, 11 },         false },
            { "m7",    "m7",    "minor 7",          { 0, 3, 7, 10 },         true  },
            { "mMaj7", "mMaj7", "minor-major 7",    { 0, 3, 7, 11 },         true  },
            { "m7b5",  "m7\xE2\x99\xAD" "5", "half-diminished", { 0, 3, 6, 10 }, true },
            { "dim7",  "dim7",  "diminished 7",     { 0, 3, 6, 9 },          true  },
            { "7sus4", "7sus4", "7 suspended 4",    { 0, 5, 7, 10 },         false },
            { "add9",  "add9",  "add 9",            { 0, 4, 7, 14 },         false },
            { "madd9", "madd9", "minor add 9",      { 0, 3, 7, 14 },         true  },
            { "69",    "6/9",   "6/9",              { 0, 4, 7, 9, 14 },      false },
            { "9",     "9",     "dominant 9",       { 0, 4, 7, 10, 14 },     false },
            { "maj9",  "maj9",  "major 9",          { 0, 4, 7, 11, 14 },     false },
            { "m9",    "m9",    "minor 9",          { 0, 3, 7, 10, 14 },     true  },
            { "11",    "11",    "dominant 11",      { 0, 4, 7, 10, 14, 17 }, false },
            { "m11",   "m11",   "minor 11",         { 0, 3, 7, 10, 14, 17 }, true  },
            { "13",    "13",    "dominant 13",      { 0, 4, 7, 10, 14, 21 }, false },
            { "7b9",   "7\xE2\x99\xAD" "9", "7 flat 9", { 0, 4, 7, 10, 13 },  false },
            { "7#9",   "7\xE2\x99\xAF" "9", "7 sharp 9", { 0, 4, 7, 10, 15 }, false },
        };
        return q;
    }

    int qualityIndex (const std::string& id)
    {
        const auto& q = qualities();
        for (size_t i = 0; i < q.size(); ++i)
            if (id == q[i].id) return (int) i;
        return -1;
    }

    std::string pcName (int pc, bool flats)       { return pc < 0 ? "?" : (flats ? flatUtf8 : sharpUtf8)[wrap12 (pc)]; }
    std::string pcNameAscii (int pc, bool flats)  { return pc < 0 ? "?" : (flats ? flatAscii : sharpAscii)[wrap12 (pc)]; }

    std::vector<int> Chord::pitchClasses() const
    {
        std::vector<int> pcs;
        if (! isValid()) return pcs;
        for (int iv : qualities()[(size_t) quality].intervals)
        {
            const int pc = wrap12 (root + iv);
            if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end()) pcs.push_back (pc);
        }
        return pcs;
    }

    std::string Chord::name (bool flats) const
    {
        if (! isValid()) return "-";
        std::string s = pcName (root, flats) + qualities()[(size_t) quality].suffix;
        if (bass >= 0 && bass != root) s += "/" + pcName (bass, flats);
        return s;
    }

    std::string Chord::longName (bool flats) const
    {
        if (! isValid()) return "";
        std::string s = pcName (root, flats) + " " + qualities()[(size_t) quality].longName;
        if (bass >= 0 && bass != root) s += " over " + pcName (bass, flats);
        return s;
    }

    std::string Chord::id() const
    {
        if (! isValid()) return "";
        const auto& q = qualities()[(size_t) quality];
        std::string s = pcNameAscii (root, false) + (quality == 0 ? "" : q.id);
        if (bass >= 0 && bass != root) s += "/" + pcNameAscii (bass, false);
        return s;
    }

    //==============================================================================
    static bool parseNote (const std::string& s, size_t& pos, int& pc)
    {
        if (pos >= s.size()) return false;
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A..G
        const char ch = (char) std::toupper ((unsigned char) s[pos]);
        if (ch < 'A' || ch > 'G') return false;
        pc = base[ch - 'A'];
        ++pos;
        while (pos < s.size() && (s[pos] == '#' || s[pos] == 'b'))
        {
            // a 'b' followed by nothing quality-like is a flat; "Bb" etc.
            pc += s[pos] == '#' ? 1 : -1;
            ++pos;
        }
        pc = wrap12 (pc);
        return true;
    }

    static int qualityFromText (const std::string& raw)
    {
        static const std::map<std::string, std::string> exact = {
            { "", "maj" }, { "M", "maj" }, { "maj", "maj" }, { "major", "maj" },
            { "m", "m" }, { "min", "m" }, { "-", "m" }, { "minor", "m" }, { "mi", "m" },
            { "5", "5" }, { "dim", "dim" }, { "o", "dim" }, { "aug", "aug" }, { "+", "aug" }, { "#5", "aug" },
            { "sus2", "sus2" }, { "2", "sus2" }, { "sus4", "sus4" }, { "sus", "sus4" }, { "4", "sus4" },
            { "6", "6" }, { "maj6", "6" }, { "m6", "m6" }, { "min6", "m6" }, { "-6", "m6" },
            { "7", "7" }, { "dom7", "7" },
            { "maj7", "maj7" }, { "M7", "maj7" }, { "Maj7", "maj7" }, { "ma7", "maj7" }, { "majmaj7", "maj7" },
            { "m7", "m7" }, { "min7", "m7" }, { "-7", "m7" }, { "mi7", "m7" },
            { "mMaj7", "mMaj7" }, { "mmaj7", "mMaj7" }, { "m(maj7)", "mMaj7" }, { "mM7", "mMaj7" }, { "-maj7", "mMaj7" }, { "minmaj7", "mMaj7" },
            { "m7b5", "m7b5" }, { "m7(b5)", "m7b5" }, { "-7b5", "m7b5" }, { "min7b5", "m7b5" }, { "m7b57", "m7b5" },
            { "dim7", "dim7" }, { "o7", "dim7" },
            { "7sus4", "7sus4" }, { "7sus", "7sus4" },
            { "add9", "add9" }, { "add2", "add9" }, { "(add9)", "add9" },
            { "madd9", "madd9" }, { "m(add9)", "madd9" }, { "-add9", "madd9" }, { "minadd9", "madd9" }, { "madd2", "madd9" },
            { "69", "69" }, { "6/9", "69" }, { "6add9", "69" },
            { "9", "9" }, { "maj9", "maj9" }, { "M9", "maj9" }, { "m9", "m9" }, { "min9", "m9" }, { "-9", "m9" },
            { "11", "11" }, { "m11", "m11" }, { "min11", "m11" }, { "-11", "m11" },
            { "13", "13" }, { "7b9", "7b9" }, { "7(b9)", "7b9" }, { "7#9", "7#9" }, { "7(#9)", "7#9" },
        };
        auto it = exact.find (raw);
        if (it == exact.end())
        {
            std::string lower = raw;
            std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
            it = exact.find (lower);
        }
        return it == exact.end() ? -1 : qualityIndex (it->second);
    }

    Chord parseChord (const std::string& text)
    {
        const std::string s = normalise (text);
        size_t pos = 0;
        Chord c;
        int root = 0;
        if (! parseNote (s, pos, root)) return {};

        std::string rest = s.substr (pos), bassText;
        // slash bass: last '/' followed by a note letter (but not "6/9")
        const size_t slash = rest.rfind ('/');
        if (slash != std::string::npos && slash + 1 < rest.size() && std::isalpha ((unsigned char) rest[slash + 1]))
        {
            bassText = rest.substr (slash + 1);
            rest = rest.substr (0, slash);
        }

        const int q = qualityFromText (rest);
        if (q < 0) return {};
        c.root = root;
        c.quality = q;

        if (! bassText.empty())
        {
            size_t bp = 0;
            int bass = 0;
            if (! parseNote (bassText, bp, bass) || bp != bassText.size()) return {};
            if (bass != root) c.bass = bass;
        }
        return c;
    }

    //==============================================================================
    std::vector<std::string> splitNumerals (const std::string& text)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char ch : text)
        {
            if (ch == ' ' || ch == ',' || ch == '\t')
            {
                if (! cur.empty()) out.push_back (cur);
                cur.clear();
            }
            else cur += ch;
        }
        if (! cur.empty()) out.push_back (cur);
        // allow "I-V-vi-IV" style with dashes between numerals (but keep "-7" suffixes impossible here)
        std::vector<std::string> expanded;
        for (auto& t : out)
        {
            size_t start = 0;
            for (size_t i = 0; i <= t.size(); ++i)
                if (i == t.size() || (t[i] == '-' && i > 0))
                {
                    if (i > start) expanded.push_back (t.substr (start, i - start));
                    start = i + 1;
                }
        }
        return expanded;
    }

    static bool parseNumeralCore (const std::string& s, size_t& pos, int& degree, bool& upper)
    {
        int acc = 0;
        while (pos < s.size() && (s[pos] == 'b' || s[pos] == '#'))
        {
            acc += s[pos] == '#' ? 1 : -1;
            ++pos;
        }
        static const std::pair<const char*, int> numerals[] = { { "VII", 11 }, { "VI", 9 }, { "IV", 5 }, { "V", 7 }, { "III", 4 }, { "II", 2 }, { "I", 0 } };
        for (const auto& [txt, deg] : numerals)
        {
            const size_t len = std::strlen (txt);
            if (pos + len > s.size()) continue;
            bool matchUpper = true, matchLower = true;
            for (size_t i = 0; i < len; ++i)
            {
                matchUpper &= s[pos + i] == txt[i];
                matchLower &= s[pos + i] == (char) std::tolower (txt[i]);
            }
            if (matchUpper || matchLower)
            {
                degree = wrap12 (deg + acc);
                upper = matchUpper;
                pos += len;
                return true;
            }
        }
        return false;
    }

    Chord romanToChord (const std::string& numeral, int tonicPc)
    {
        const std::string s = normalise (numeral);
        size_t pos = 0;
        int degree = 0;
        bool upper = true;
        if (! parseNumeralCore (s, pos, degree, upper)) return {};

        std::string suffix = s.substr (pos), bassPart;
        const size_t slash = suffix.find ('/');
        if (slash != std::string::npos && slash + 1 < suffix.size() && suffix[slash + 1] != '9')
        {
            bassPart = suffix.substr (slash + 1);
            suffix = suffix.substr (0, slash);
        }

        std::string q;
        if (suffix.empty())                                          q = upper ? "maj" : "m";
        else if (suffix == "dim" || suffix == "o")                   q = "dim";
        else if (suffix == "dim7" || suffix == "o7")                 q = "dim7";
        else if (suffix == "m7b5" || suffix == "m7b57")              q = "m7b5";
        else if (suffix == "+" || suffix == "aug")                   q = "aug";
        else if (suffix == "7")                                      q = upper ? "7" : "m7";
        else if (suffix == "maj7" || suffix == "M7" || suffix == "maj") q = upper ? "maj7" : "mMaj7";
        else if (suffix == "9")                                      q = upper ? "9" : "m9";
        else if (suffix == "maj9")                                   q = "maj9";
        else if (suffix == "11")                                     q = upper ? "11" : "m11";
        else if (suffix == "6")                                      q = upper ? "6" : "m6";
        else if (suffix == "add9" || suffix == "add2")               q = upper ? "add9" : "madd9";
        else if (suffix == "m")                                      q = "m";
        else if (suffix == "m7")                                     q = "m7";
        else if (suffix == "m9")                                     q = "m9";
        else
        {
            const int qi = qualityFromText (suffix);
            if (qi < 0) return {};
            q = qualities()[(size_t) qi].id;
        }

        Chord c;
        c.root = wrap12 (tonicPc + degree);
        c.quality = qualityIndex (q);
        if (! bassPart.empty())
        {
            size_t bp = 0;
            int bdeg = 0;
            bool bu = true;
            if (parseNumeralCore (bassPart, bp, bdeg, bu))
                c.bass = wrap12 (tonicPc + bdeg);
            if (c.bass == c.root) c.bass = -1;
        }
        return c;
    }

    std::string chordToRoman (const Chord& c, int tonicPc)
    {
        if (! c.isValid()) return "?";
        static const char* const deg[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
        const auto& q = qualities()[(size_t) c.quality];
        std::string n = deg[wrap12 (c.root - tonicPc)];
        if (q.minorish)
            for (auto& ch : n) if (ch == 'I' || ch == 'V') ch = (char) std::tolower (ch);

        const std::string id = q.id;
        std::string suffix;
        if (id == "maj" || id == "m")             suffix = "";
        else if (id == "dim")                     suffix = "dim";
        else if (id == "dim7")                    suffix = "dim7";
        else if (id == "m7b5")                    suffix = "m7b5";
        else if (id == "aug")                     suffix = "+";
        else if (id == "7" || id == "m7")         suffix = "7";
        else if (id == "maj7" || id == "mMaj7")   suffix = "maj7";
        else if (id == "9" || id == "m9")         suffix = "9";
        else if (id == "11" || id == "m11")       suffix = "11";
        else if (id == "6" || id == "m6")         suffix = "6";
        else if (id == "add9" || id == "madd9")   suffix = "add9";
        else                                      suffix = id;
        n += suffix;

        if (c.bass >= 0 && c.bass != c.root)
            n += std::string ("/") + deg[wrap12 (c.bass - tonicPc)];
        return n;
    }

    bool progressionIsMinor (const std::vector<std::string>& numerals)
    {
        // 1) a tonic chord (I or i) decides it
        for (const auto& t : numerals)
        {
            const std::string s = normalise (t);
            size_t pos = 0;
            int degree = 0;
            bool upper = true;
            if (parseNumeralCore (s, pos, degree, upper) && degree == 0 && s[0] != 'b' && s[0] != '#')
                return ! upper;
        }

        // 2) otherwise: which mode explains more of the chords? (e.g. "iv bVI bVII" is minor, "ii V" is major)
        struct RootColour { int root; bool minorish; };
        static const RootColour majorSet[] = { { 0, false }, { 2, true }, { 4, true }, { 5, false }, { 7, false }, { 9, true }, { 11, true } };
        static const RootColour minorSet[] = { { 0, true }, { 2, true }, { 3, false }, { 5, true }, { 7, true }, { 7, false }, { 8, false }, { 10, false } };
        int majorFit = 0, minorFit = 0;
        for (const auto& t : numerals)
        {
            const Chord c = romanToChord (t, 0);
            if (! c.isValid()) continue;
            const bool mi = qualities()[(size_t) c.quality].minorish;
            for (const auto& d : majorSet) if (d.root == c.root && d.minorish == mi) { ++majorFit; break; }
            for (const auto& d : minorSet) if (d.root == c.root && d.minorish == mi) { ++minorFit; break; }
        }
        return minorFit > majorFit;
    }

    std::vector<Chord> resolveProgression (const std::vector<std::string>& numerals, const Key& key)
    {
        const bool minorProg = progressionIsMinor (numerals);
        int tonic = key.tonic;
        if (minorProg && ! key.minor) tonic = wrap12 (key.tonic + 9);      // relative minor
        if (! minorProg && key.minor) tonic = wrap12 (key.tonic + 3);      // relative major

        std::vector<Chord> out;
        for (const auto& n : numerals)
        {
            const Chord c = romanToChord (n, tonic);
            if (c.isValid()) out.push_back (c);
        }
        return out;
    }

    std::vector<Chord> diatonicChords (const Key& key, bool sevenths)
    {
        const char* const majTriads[7] = { "I", "ii", "iii", "IV", "V", "vi", "viidim" };
        const char* const maj7[7]      = { "Imaj7", "ii7", "iii7", "IVmaj7", "V7", "vi7", "viim7b5" };
        const char* const minTriads[7] = { "i", "iidim", "bIII", "iv", "v", "bVI", "bVII" };
        const char* const min7[7]      = { "i7", "iim7b5", "bIIImaj7", "iv7", "v7", "bVImaj7", "bVII7" };
        const char* const* set = key.minor ? (sevenths ? min7 : minTriads) : (sevenths ? maj7 : majTriads);
        std::vector<Chord> out;
        for (int i = 0; i < 7; ++i) out.push_back (romanToChord (set[i], key.tonic));
        return out;
    }

    //==============================================================================
    bool keyFromChords (const std::vector<TimedChord>& chords, Key& out, float* relativeMargin)
    {
        static const int majorScale[7] = { 0, 2, 4, 5, 7, 9, 11 }, minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };

        std::vector<TimedChord> cs;
        for (const auto& tc : chords)
            if (tc.chord.isValid() && tc.weight > 0.0) cs.push_back (tc);
        if (cs.empty()) return false;

        double total = 0.0;
        for (const auto& tc : cs) total += tc.weight;
        const double unit = total / (double) cs.size();     // the length of an average chord

        double best = -1.0e30, second = -1.0e30;
        Key bestKey { 0, false };
        for (int minor = 0; minor < 2; ++minor)
            for (int tonic = 0; tonic < 12; ++tonic)
            {
                const int* sc = minor ? minorScale : majorScale;
                auto degree = [&] (int pc) { return wrap12 (pc - tonic); };
                auto inScale = [&] (int pc) { const int d = degree (pc); for (int i = 0; i < 7; ++i) if (sc[i] == d) return true; return false; };

                // does this chord work as the key's home chord? (major 3rd for a major key, minor 3rd for a minor key)
                auto homeChord = [&] (const Chord& c)
                {
                    if (c.root != tonic) return 0.0;
                    const auto& iv = qualities()[(size_t) c.quality].intervals;
                    const bool m3 = std::find (iv.begin(), iv.end(), 3) != iv.end();
                    const bool M3 = std::find (iv.begin(), iv.end(), 4) != iv.end();
                    const bool fifth = std::find (iv.begin(), iv.end(), 7) != iv.end();
                    if (! fifth) return 0.0;                              // dim / aug on the key note: not a home chord
                    if (minor ? (m3 && ! M3) : (M3 && ! m3)) return 1.0;
                    if (! m3 && ! M3) return 0.5;                         // power chord or sus: could be either
                    return 0.0;
                };

                double score = 0.0;
                for (const auto& tc : cs)
                {
                    auto pcs = tc.chord.pitchClasses();
                    if (tc.chord.bass >= 0 && std::find (pcs.begin(), pcs.end(), tc.chord.bass) == pcs.end()) pcs.push_back (tc.chord.bass);
                    const double home = homeChord (tc.chord);
                    double fit = 0.0;
                    for (int pc : pcs)
                    {
                        if (inScale (pc))                                  fit += 1.0;
                        else if (minor && degree (pc) == 11)               fit += 0.0;   // the raised 7th of a minor key (the major V chord)
                        else if (! minor && home > 0.0 && degree (pc) == 10) fit += 0.0; // a 7 on the home chord (blues / rock I7)
                        else                                               fit -= 2.0;
                    }
                    score += tc.weight * fit / (double) pcs.size();
                    score += tc.weight * 0.8 * home;
                }
                score += unit * 1.0 * homeChord (cs.front().chord);   // songs usually start at home ...
                score += unit * 0.5 * homeChord (cs.back().chord);    // ... and often end there

                if (score > best)        { second = best; best = score; bestKey = { tonic, minor != 0 }; }
                else if (score > second) { second = score; }
            }

        out = bestKey;
        if (relativeMargin != nullptr)
            *relativeMargin = best > 0.0 ? (float) std::clamp ((best - second) / best, 0.0, 1.0) : 0.0f;
        return true;
    }

    //==============================================================================
    const char* guitarStyleName (GuitarStyle s)
    {
        switch (s)
        {
            case GuitarStyle::Standard:  return "Standard";
            case GuitarStyle::OpenEasy:  return "Open / Easy";
            case GuitarStyle::Triad:     return "Triad";
            case GuitarStyle::Barre:     return "Barre";
            case GuitarStyle::Power:     return "Power Chord";
            case GuitarStyle::Shell:     return "Shell";
            case GuitarStyle::Inversion: return "Inversion";
            case GuitarStyle::Color:     return "Color / Extended";
            case GuitarStyle::NumStyles: break;
        }
        return "";
    }

    static Chord withQuality (Chord c, const char* id)
    {
        c.quality = qualityIndex (id);
        return c;
    }

    Chord chordForStyle (const Chord& c, GuitarStyle style)
    {
        if (! c.isValid()) return c;
        const std::string id = qualities()[(size_t) c.quality].id;

        switch (style)
        {
            case GuitarStyle::OpenEasy:
            {
                static const std::map<std::string, const char*> m = { { "9", "7" }, { "maj9", "maj7" }, { "m9", "m7" }, { "11", "7" },
                                                                      { "m11", "m7" }, { "13", "7" }, { "69", "6" }, { "7b9", "7" }, { "7#9", "7" } };
                auto it = m.find (id);
                return it != m.end() ? withQuality (c, it->second) : c;
            }
            case GuitarStyle::Triad:
            {
                static const std::map<std::string, const char*> m = {
                    { "maj", "maj" }, { "5", "maj" }, { "6", "maj" }, { "7", "maj" }, { "maj7", "maj" }, { "add9", "maj" }, { "69", "maj" },
                    { "9", "maj" }, { "maj9", "maj" }, { "11", "maj" }, { "13", "maj" }, { "7b9", "maj" }, { "7#9", "maj" },
                    { "m", "m" }, { "m6", "m" }, { "m7", "m" }, { "mMaj7", "m" }, { "madd9", "m" }, { "m9", "m" }, { "m11", "m" },
                    { "dim", "dim" }, { "m7b5", "dim" }, { "dim7", "dim" }, { "aug", "aug" }, { "sus2", "sus2" }, { "sus4", "sus4" }, { "7sus4", "sus4" } };
                auto it = m.find (id);
                Chord r = withQuality (c, it != m.end() ? it->second : "maj");
                return r;
            }
            case GuitarStyle::Power:
            {
                Chord r = withQuality (c, "5");
                r.bass = -1;
                return r;
            }
            case GuitarStyle::Shell:
            {
                static const std::map<std::string, const char*> m = { { "9", "7" }, { "maj9", "maj7" }, { "m9", "m7" }, { "11", "7" },
                                                                      { "m11", "m7" }, { "13", "7" }, { "7b9", "7" }, { "7#9", "7" },
                                                                      { "69", "6" }, { "add9", "maj" }, { "madd9", "m" } };
                auto it = m.find (id);
                return it != m.end() ? withQuality (c, it->second) : c;
            }
            case GuitarStyle::Color:
            {
                static const std::map<std::string, const char*> m = { { "maj", "add9" }, { "m", "madd9" }, { "7", "9" }, { "maj7", "maj9" },
                                                                      { "m7", "m9" }, { "6", "69" }, { "sus4", "7sus4" }, { "5", "sus2" },
                                                                      { "dim", "dim7" } };
                auto it = m.find (id);
                return it != m.end() ? withQuality (c, it->second) : c;
            }
            case GuitarStyle::Standard:
            case GuitarStyle::Barre:
            case GuitarStyle::Inversion:
            case GuitarStyle::NumStyles:
                break;
        }
        return c;
    }

    //==============================================================================
    int GuitarVoicing::lowestFret() const
    {
        int lo = 0;
        for (int f : frets) if (f > 0 && (lo == 0 || f < lo)) lo = f;
        return lo;
    }

    int GuitarVoicing::highestFret() const
    {
        int hi = 0;
        for (int f : frets) hi = std::max (hi, f);
        return hi;
    }

    std::vector<int> GuitarVoicing::midiNotes() const
    {
        std::vector<int> n;
        for (int s = 0; s < 6; ++s)
            if (frets[(size_t) s] >= 0) n.push_back (standardTuning[(size_t) s] + frets[(size_t) s]);
        return n;
    }

    namespace
    {
        struct ToneRules
        {
            std::set<int> allowed, required, optional;
            int bassPc = -1;            // lowest note must be this pc
            bool bassNotRoot = false;   // inversion
            int root = 0, third = -1;
        };

        ToneRules makeRules (const Chord& shown, const Chord& original, GuitarStyle style)
        {
            ToneRules r;
            r.root = shown.root;
            const auto& iv = qualities()[(size_t) shown.quality].intervals;
            for (int i : iv)
            {
                const int pc = wrap12 (shown.root + i);
                r.allowed.insert (pc);
                const int ii = i % 12;
                if (ii == 3 || ii == 4) r.third = pc;
                const bool fifth = (ii == 7);
                const bool ext13 = iv.size() >= 6 && (i == 14);
                if ((fifth && iv.size() >= 4) || ext13) r.optional.insert (pc);
                else                                     r.required.insert (pc);
            }

            if (style == GuitarStyle::Shell)
            {
                // root, 3rd (or sus tone) and 7th/6th only
                r.allowed.clear(); r.required.clear(); r.optional.clear();
                for (int i : iv)
                {
                    const int ii = i % 12;
                    if (ii == 7 && iv.size() >= 4) continue;     // drop the 5th when a 7th/6th exists
                    if (i > 12) continue;
                    const int pc = wrap12 (shown.root + i);
                    r.allowed.insert (pc);
                    r.required.insert (pc);
                }
            }

            if (original.bass >= 0 && style != GuitarStyle::Power)
            {
                r.bassPc = original.bass;
                r.allowed.insert (original.bass);
            }
            else if (style == GuitarStyle::Inversion)
            {
                r.bassNotRoot = true;
            }
            else
            {
                r.bassPc = shown.root;
            }
            return r;
        }

        struct Candidate { GuitarVoicing v; double score = 1.0e9; };

        double evaluate (const std::array<int, 6>& f, const ToneRules& rules, GuitarStyle style, GuitarVoicing& out)
        {
            int first = -1, last = -1, n = 0, opens = 0, internalMutes = 0, highMutes = 0;
            for (int s = 0; s < 6; ++s)
                if (f[(size_t) s] >= 0) { if (first < 0) first = s; last = s; ++n; if (f[(size_t) s] == 0) ++opens; }
            if (n < 2) return 1.0e9;
            for (int s = first; s <= last; ++s) if (f[(size_t) s] < 0) ++internalMutes;
            highMutes = 5 - last;

            // tones present
            std::set<int> present;
            for (int s = 0; s < 6; ++s)
                if (f[(size_t) s] >= 0) present.insert (wrap12 (standardTuning[(size_t) s] + f[(size_t) s]));
            for (int pc : rules.required) if (! present.count (pc)) return 1.0e9;
            int missingOptional = 0;
            for (int pc : rules.optional) if (! present.count (pc)) ++missingOptional;

            // bass
            const int bassPc = wrap12 (standardTuning[(size_t) first] + f[(size_t) first]);
            if (rules.bassPc >= 0 && bassPc != rules.bassPc) return 1.0e9;
            if (rules.bassNotRoot && bassPc == rules.root) return 1.0e9;

            // span and fingers
            int minF = 99, maxF = 0, fretted = 0;
            for (int s = 0; s < 6; ++s)
                if (f[(size_t) s] > 0) { minF = std::min (minF, f[(size_t) s]); maxF = std::max (maxF, f[(size_t) s]); ++fretted; }
            const int span = fretted > 0 ? maxF - minF : 0;
            if (span > 3) return 1.0e9;

            bool barre = false;
            int barreFrom = 0, barreTo = 0;
            if (fretted > 4 || (style == GuitarStyle::Barre && fretted > 0))
            {
                int lo = -1, hi = -1;
                for (int s = 0; s < 6; ++s) if (f[(size_t) s] == minF) { if (lo < 0) lo = s; hi = s; }
                // a full barre lies across every string from the lowest barred string upwards
                if (style == GuitarStyle::Barre) hi = last;
                if (lo < 0 || hi - lo < 1) return 1.0e9;
                for (int s = lo; s <= hi; ++s) if (f[(size_t) s] < minF) return 1.0e9;   // open/muted under a barre
                int fingers = 1;
                for (int s = 0; s < 6; ++s) if (f[(size_t) s] > minF) ++fingers;
                if (fingers > 4) return 1.0e9;
                barre = true; barreFrom = lo; barreTo = hi;
            }

            // two strings sounding the very same pitch waste a string and sound thin
            int unisons = 0;
            for (int a = 0; a < 6; ++a)
                for (int b = a + 1; b < 6; ++b)
                    if (f[(size_t) a] >= 0 && f[(size_t) b] >= 0
                        && standardTuning[(size_t) a] + f[(size_t) a] == standardTuning[(size_t) b] + f[(size_t) b])
                        ++unisons;

            double score = 1.5 * unisons;
            if (opens > 0 && maxF > 3 && style != GuitarStyle::Barre) score += 1.5 * (maxF - 3);
            double fretSum = 0;
            for (int s = 0; s < 6; ++s) if (f[(size_t) s] > 0) fretSum += f[(size_t) s];
            const double avgFret = fretted > 0 ? fretSum / fretted : 0.0;

            score += 0.35 * avgFret + 1.2 * span + 0.8 * missingOptional;
            const bool thirdInBass = rules.third >= 0 && bassPc == rules.third;

            switch (style)
            {
                case GuitarStyle::Standard:
                case GuitarStyle::Color:
                case GuitarStyle::Inversion:
                    if (internalMutes > (style == GuitarStyle::Color ? 1 : 0)) return 1.0e9;
                    if (n < 4) score += 3.0;
                    score += -0.65 * n - 0.4 * opens + 0.6 * minF * (fretted ? 1 : 0) + 1.5 * highMutes + 2.4 * internalMutes;
                    if (barre) score += 1.0;
                    if (style == GuitarStyle::Inversion && thirdInBass) score -= 1.0;
                    break;

                case GuitarStyle::OpenEasy:
                    if (internalMutes > 0 || n < 3) return 1.0e9;
                    score += -0.45 * n - 0.6 * opens + 2.0 * std::max (0, maxF - 3) + 1.5 * highMutes;
                    if (barre) score += 6.0;
                    if (fretted > 3) score += 1.0;
                    break;

                case GuitarStyle::Triad:
                    if (n != 3 || internalMutes > 0) return 1.0e9;
                    score += 0.25 * std::abs (first - 2) + (bassPc == rules.root ? -0.5 : 0.0) + 0.15 * opens;
                    break;

                case GuitarStyle::Power:
                    if (n < 2 || n > 3 || internalMutes > 0 || first > 1) return 1.0e9;
                    for (int s2 = first + 1; s2 < 6; ++s2) if (f[(size_t) s2] == 0) return 1.0e9;   // only the bass may ring open
                    score += 0.6 * first - 0.2 * n;
                    break;

                case GuitarStyle::Shell:
                    if (n < 3 || n > 4 || internalMutes > 1 || first > 1) return 1.0e9;
                    score += 0.5 * internalMutes + 0.4 * (n - 3) - 0.2 * first + 0.6 * opens;
                    break;

                case GuitarStyle::Barre:
                    if (! barre || n < 5 || internalMutes > 0 || opens > 0 || minF < 1 || first > 1) return 1.0e9;
                    score += -0.3 * n;
                    break;

                case GuitarStyle::NumStyles: break;
            }

            out.frets = f;
            out.barreFret = barre ? minF : 0;
            out.barreFrom = barreFrom;
            out.barreTo = barreTo;
            out.valid = true;
            return score;
        }
    }

    // Classic open-position shapes players expect for Standard / Open-Easy (low E .. high E, -1 = muted)
    static bool canonicalShape (const Chord& c, std::array<int, 6>& out, bool useBarres)
    {
        if (c.bass >= 0) return false;
        static const std::map<std::string, std::array<int, 6>> shapes = {
            { "C",      { { -1, 3, 2, 0, 1, 0 } } },  { "D",     { { -1, -1, 0, 2, 3, 2 } } }, { "E",     { { 0, 2, 2, 1, 0, 0 } } },
            { "G",      { { 3, 2, 0, 0, 0, 3 } } },   { "A",     { { -1, 0, 2, 2, 2, 0 } } },  { "Am",    { { -1, 0, 2, 2, 1, 0 } } },
            { "Em",     { { 0, 2, 2, 0, 0, 0 } } },   { "Dm",    { { -1, -1, 0, 2, 3, 1 } } }, { "E7",    { { 0, 2, 0, 1, 0, 0 } } },
            { "A7",     { { -1, 0, 2, 0, 2, 0 } } },  { "D7",    { { -1, -1, 0, 2, 1, 2 } } }, { "G7",    { { 3, 2, 0, 0, 0, 1 } } },
            { "C7",     { { -1, 3, 2, 3, 1, 0 } } },  { "B7",    { { -1, 2, 1, 2, 0, 2 } } },  { "Am7",   { { -1, 0, 2, 0, 1, 0 } } },
            { "Em7",    { { 0, 2, 2, 0, 3, 0 } } },   { "Dm7",   { { -1, -1, 0, 2, 1, 1 } } }, { "Cmaj7", { { -1, 3, 2, 0, 0, 0 } } },
            { "Fmaj7",  { { -1, -1, 3, 2, 1, 0 } } }, { "Amaj7", { { -1, 0, 2, 1, 2, 0 } } },  { "Dmaj7", { { -1, -1, 0, 2, 2, 2 } } },
            { "Gmaj7",  { { 3, 2, 0, 0, 0, 2 } } },   { "Asus2", { { -1, 0, 2, 2, 0, 0 } } },  { "Asus4", { { -1, 0, 2, 2, 3, 0 } } },
            { "Dsus2",  { { -1, -1, 0, 2, 3, 0 } } }, { "Dsus4", { { -1, -1, 0, 2, 3, 3 } } }, { "Esus4", { { 0, 2, 2, 2, 0, 0 } } },
            { "Cadd9",  { { -1, 3, 2, 0, 3, 0 } } },  { "Gadd9", { { 3, 0, 0, 2, 0, 3 } } },   { "Em9",   { { 0, 2, 0, 0, 0, 2 } } },
            { "A7sus4", { { -1, 0, 2, 0, 3, 0 } } },  { "E5",    { { 0, 2, 2, -1, -1, -1 } } }, { "A5",   { { -1, 0, 2, 2, -1, -1 } } },
            { "D5",     { { -1, -1, 0, 2, 3, -1 } } }, { "Am9",  { { -1, 0, 5, 5, 0, 0 } } },  { "C6",    { { -1, 3, 2, 2, 1, 0 } } },
            { "Bm7",    { { -1, 2, 0, 2, 0, 2 } } },  { "F#m7",  { { 2, -1, 2, 2, 2, -1 } } }, { "E9",   { { 0, 2, 0, 1, 0, 2 } } },
        };
        // full barre shapes used by Standard only
        static const std::map<std::string, std::array<int, 6>> barres = {
            { "A#",  { { -1, 1, 3, 3, 3, 1 } } }, { "A#7", { { -1, 1, 3, 1, 3, 1 } } }, { "A#m", { { -1, 1, 3, 3, 2, 1 } } },
            { "F#",  { { 2, 4, 4, 3, 2, 2 } } },  { "C#",  { { -1, 4, 6, 6, 6, 4 } } }, { "G#",  { { 4, 6, 6, 5, 4, 4 } } },
            { "D#",  { { -1, 6, 8, 8, 8, 6 } } }, { "Fm",  { { 1, 3, 3, 1, 1, 1 } } },  { "Gm",  { { 3, 5, 5, 3, 3, 3 } } },
            { "Cm",  { { -1, 3, 5, 5, 4, 3 } } }, { "C#m", { { -1, 4, 6, 6, 5, 4 } } }, { "G#m", { { 4, 6, 6, 4, 4, 4 } } },
            { "F#7", { { 2, 4, 2, 3, 2, 2 } } },  { "F7",  { { 1, 3, 1, 2, 1, 1 } } },  { "Fm7", { { 1, 3, 1, 1, 1, 1 } } },
            { "Gm7", { { 3, 5, 3, 3, 3, 3 } } },  { "Cm7", { { -1, 3, 5, 3, 4, 3 } } }, { "A#maj7", { { -1, 1, 3, 2, 3, 1 } } },
            { "D#m", { { -1, 6, 8, 8, 7, 6 } } }, { "Bm",  { { -1, 2, 4, 4, 3, 2 } } }, { "F#m", { { 2, 4, 4, 2, 2, 2 } } },
        };
        if (useBarres)
        {
            auto bt = barres.find (c.id());
            if (bt != barres.end()) { out = bt->second; return true; }
        }
        auto it = shapes.find (c.id());
        if (it == shapes.end()) return false;
        out = it->second;
        return true;
    }

    GuitarVoicing guitarVoicing (const Chord& c, GuitarStyle style)
    {
        GuitarVoicing best;
        if (! c.isValid()) return best;

        if (style == GuitarStyle::Standard || style == GuitarStyle::OpenEasy)
        {
            const Chord shownC = chordForStyle (c, style);
            std::array<int, 6> shape {};
            if (canonicalShape (shownC, shape, style == GuitarStyle::Standard))
            {
                best.frets = shape;
                best.valid = true;
                best.shown = shownC;
                int fretted = 0, minF = 99, lo = -1, hi = -1, last = -1;
                for (int i = 0; i < 6; ++i) if (shape[(size_t) i] > 0) { ++fretted; minF = std::min (minF, shape[(size_t) i]); }
                for (int i = 0; i < 6; ++i) if (shape[(size_t) i] >= 0) last = i;
                for (int i = 0; i < 6; ++i) if (shape[(size_t) i] == minF) { if (lo < 0) lo = i; hi = i; }
                if (fretted > 4 && lo >= 0) { best.barreFret = minF; best.barreFrom = lo; best.barreTo = std::max (hi, last); }
                return best;
            }
        }

        const Chord shown = chordForStyle (c, style);
        const ToneRules rules = makeRules (shown, c, style);
        const bool allowOpen = style != GuitarStyle::Barre;
        double bestScore = 1.0e9;

        std::array<std::vector<int>, 6> cand;
        std::array<int, 6> f {};

        for (int w = 0; w <= 12; ++w)
        {
            for (int s = 0; s < 6; ++s)
            {
                auto& v = cand[(size_t) s];
                v.clear();
                v.push_back (-1);
                const int open = standardTuning[(size_t) s];
                if (allowOpen && rules.allowed.count (wrap12 (open))) v.push_back (0);
                for (int fr = std::max (1, w); fr <= w + 3; ++fr)
                    if (rules.allowed.count (wrap12 (open + fr))) v.push_back (fr);
            }

            std::function<void (int)> rec = [&] (int s)
            {
                if (s == 6)
                {
                    GuitarVoicing v;
                    const double sc = evaluate (f, rules, style, v);
                    if (sc < bestScore) { bestScore = sc; best = v; }
                    return;
                }
                for (int fr : cand[(size_t) s]) { f[(size_t) s] = fr; rec (s + 1); }
            };
            rec (0);
        }

        best.shown = shown;
        if (style != GuitarStyle::Power) best.shown.bass = c.bass;

        // some chords have no shape in a style (e.g. a power chord as a full barre): fall back sensibly
        if (! best.valid && style != GuitarStyle::Standard)
            return guitarVoicing (c, style == GuitarStyle::Triad || style == GuitarStyle::Shell ? GuitarStyle::OpenEasy : GuitarStyle::Standard);
        if (! best.valid && c.bass >= 0)
        {
            Chord noSlash = c;
            noSlash.bass = -1;
            return guitarVoicing (noSlash, GuitarStyle::Standard);
        }
        return best;
    }

    std::vector<int> pianoVoicing (const Chord& c, GuitarStyle style)
    {
        std::vector<int> notes;
        if (! c.isValid()) return notes;
        const Chord shown = chordForStyle (c, style == GuitarStyle::Barre ? GuitarStyle::Standard : style);
        auto iv = qualities()[(size_t) shown.quality].intervals;

        // left hand: bass note between G2 (43) and F#3 (54)
        const int bassPc = c.bass >= 0 && style != GuitarStyle::Power ? c.bass : shown.root;
        int bass = 43 + wrap12 (bassPc - 43);
        notes.push_back (bass);

        if (style == GuitarStyle::Power)
        {
            notes.push_back (bass + 7);
            notes.push_back (bass + 12);
            return notes;
        }

        if (style == GuitarStyle::Shell)
        {
            std::vector<int> keep;
            for (int i : iv) if (! ((i % 12) == 7 && iv.size() >= 4) && i <= 12) keep.push_back (i);
            iv = keep;
        }

        // right hand: chord tones in close position from about A3 upwards
        std::vector<int> rh;
        for (int i : iv) rh.push_back (i);
        if (iv.size() >= 4 && style != GuitarStyle::Shell)
            rh.erase (std::remove (rh.begin(), rh.end(), 0), rh.end());          // root already in the bass
        if (style == GuitarStyle::Inversion && rh.size() > 1)
            std::rotate (rh.begin(), rh.begin() + 1, rh.end());

        int prev = 56;
        for (int i : rh)
        {
            const int pc = wrap12 (shown.root + i);
            int nnote = prev + 1 + wrap12 (pc - (prev + 1));
            notes.push_back (nnote);
            prev = nnote;
        }
        std::sort (notes.begin(), notes.end());
        notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
        return notes;
    }
}
