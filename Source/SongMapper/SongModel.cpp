// Song Mapper - song model, favourites, MIDI export
#include "SongModel.h"

namespace sm
{
    SongMap::SongMap()
    {
        // a small starting template
        addSection ("Intro", 4);
        addSection ("Verse 1", 8);
        addSection ("Chorus 1", 8);
        selectedSection = 1;
        undoStack.clear();
    }

    //==============================================================================
    int SongMap::addSection (const juce::String& name, int bars)
    {
        checkpoint();
        Section s;
        s.name = name;
        s.bars = juce::jlimit (1, 256, bars);
        s.uid = nextUid++;
        sections.push_back (s);
        selectedSection = (int) sections.size() - 1;
        changed();
        return selectedSection;
    }

    void SongMap::removeSection (int index)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) sections.size())) return;
        sections.erase (sections.begin() + index);
        selectedSection = juce::jmin (selectedSection, (int) sections.size() - 1);
        changed();
    }

    void SongMap::moveSection (int from, int to)
    {
        checkpoint();
        const int n = (int) sections.size();
        if (! juce::isPositiveAndBelow (from, n)) return;
        to = juce::jlimit (0, n - 1, to);
        if (from == to) return;
        auto s = sections[(size_t) from];
        sections.erase (sections.begin() + from);
        sections.insert (sections.begin() + to, s);
        selectedSection = to;
        changed();
    }

    void SongMap::duplicateSection (int index)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) sections.size())) return;
        auto s = sections[(size_t) index];
        s.uid = nextUid++;
        // "Chorus 1" -> "Chorus 2"
        auto base = s.name.trimCharactersAtEnd ("0123456789 ");
        s.name = nextSectionName (base.isEmpty() ? s.name : base);
        sections.insert (sections.begin() + index + 1, s);
        selectedSection = index + 1;
        changed();
    }

    void SongMap::renameSection (int index, const juce::String& name)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) sections.size()) || name.trim().isEmpty()) return;
        sections[(size_t) index].name = name.trim();
        changed();
    }

    void SongMap::setSectionBars (int index, int bars)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) sections.size())) return;
        sections[(size_t) index].bars = juce::jlimit (1, 256, bars);
        changed();
    }

    void SongMap::setSectionBarsPerChord (int index, float bpc)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) sections.size())) return;
        sections[(size_t) index].barsPerChord = bpc;
        changed();
    }

    void SongMap::insertChords (int section, const std::vector<Chord>& chords, int at)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (section, (int) sections.size()) || chords.empty()) return;
        auto& v = sections[(size_t) section].chords;
        if (at < 0 || at > (int) v.size()) at = (int) v.size();
        v.insert (v.begin() + at, chords.begin(), chords.end());
        selectedSection = section;
        changed();
    }

    void SongMap::removeChord (int section, int index)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (section, (int) sections.size())) return;
        auto& v = sections[(size_t) section].chords;
        if (! juce::isPositiveAndBelow (index, (int) v.size())) return;
        v.erase (v.begin() + index);
        changed();
    }

    void SongMap::moveChord (int fs, int fi, int ts, int ti)
    {
        checkpoint();
        const int n = (int) sections.size();
        if (! juce::isPositiveAndBelow (fs, n) || ! juce::isPositiveAndBelow (ts, n)) return;
        auto& from = sections[(size_t) fs].chords;
        if (! juce::isPositiveAndBelow (fi, (int) from.size())) return;
        const Chord c = from[(size_t) fi];
        from.erase (from.begin() + fi);
        auto& to = sections[(size_t) ts].chords;
        if (fs == ts && ti > fi) --ti;
        ti = juce::jlimit (0, (int) to.size(), ti);
        to.insert (to.begin() + ti, c);
        selectedSection = ts;
        changed();
    }

    void SongMap::replaceChord (int section, int index, const Chord& c)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (section, (int) sections.size()) || ! c.isValid()) return;
        auto& v = sections[(size_t) section].chords;
        if (! juce::isPositiveAndBelow (index, (int) v.size())) return;
        v[(size_t) index] = c;
        changed();
    }

    void SongMap::setTray (const std::vector<Chord>& chords)
    {
        checkpoint();
        tray = chords;
        if (tray.size() > 16) tray.resize (16);
        selectedTray = tray.empty() ? -1 : 0;
        changed();
    }

    void SongMap::insertTray (const Chord& c, int at)
    {
        checkpoint();
        if (! c.isValid() || tray.size() >= 16) return;
        if (at < 0 || at > (int) tray.size()) at = (int) tray.size();
        tray.insert (tray.begin() + at, c);
        selectedTray = at;
        changed();
    }

    void SongMap::removeTray (int index)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) tray.size())) return;
        tray.erase (tray.begin() + index);
        selectedTray = juce::jmin (selectedTray, (int) tray.size() - 1);
        changed();
    }

    void SongMap::moveTray (int from, int to)
    {
        checkpoint();
        const int n = (int) tray.size();
        if (! juce::isPositiveAndBelow (from, n)) return;
        to = juce::jlimit (0, n - 1, to);
        if (from == to) return;
        const Chord c = tray[(size_t) from];
        tray.erase (tray.begin() + from);
        tray.insert (tray.begin() + to, c);
        selectedTray = to;
        changed();
    }

    void SongMap::replaceTray (int index, const Chord& c)
    {
        checkpoint();
        if (! juce::isPositiveAndBelow (index, (int) tray.size()) || ! c.isValid()) return;
        tray[(size_t) index] = c;
        selectedTray = index;
        changed();
    }

    void SongMap::clearSection (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) sections.size()) || sections[(size_t) index].chords.empty()) return;
        checkpoint();
        sections[(size_t) index].chords.clear();
        changed();
    }

    void SongMap::clearAllChords()
    {
        bool any = false;
        for (const auto& sec : sections) any |= ! sec.chords.empty() || ! sec.bass.notes.empty() || ! sec.lead.notes.empty();
        if (! any) return;
        checkpoint();
        for (auto& sec : sections)
        {
            sec.chords.clear();
            sec.bass.notes.clear();
            sec.lead.notes.clear();
        }
        changed();
    }

    //==============================================================================
    const char* bassFillName (BassFill f)
    {
        switch (f)
        {
            case BassFill::RootPulse: return "Root pulse (8ths)";
            case BassFill::RootFifth: return "Root and fifth";
            case BassFill::Octaves:   return "Octaves";
            case BassFill::Walking:   return "Walking";
            case BassFill::Roots:
            case BassFill::NumFills:
            default:                  return "Roots (held)";
        }
    }

    Line& SongMap::line (int section, Lane lane)
    {
        auto& sec = sections[(size_t) juce::jlimit (0, (int) sections.size() - 1, section)];
        return lane == Lane::Lead ? sec.lead : sec.bass;
    }

    const Line& SongMap::line (int section, Lane lane) const
    {
        const auto& sec = sections[(size_t) juce::jlimit (0, (int) sections.size() - 1, section)];
        return lane == Lane::Lead ? sec.lead : sec.bass;
    }

    double SongMap::sectionBeats (int section) const
    {
        if (! juce::isPositiveAndBelow (section, (int) sections.size())) return 0.0;
        return sections[(size_t) section].bars * beatsPerBar();
    }

    double SongMap::lineLoopBeats (int section, Lane lane) const
    {
        const double total = sectionBeats (section);
        const int loopBars = line (section, lane).loopBars;
        return loopBars > 0 ? juce::jmin (total, loopBars * beatsPerBar()) : total;
    }

    void SongMap::setLineEnabled (int section, Lane lane, bool on)
    {
        if (! juce::isPositiveAndBelow (section, (int) sections.size()) || lane == Lane::Chords) return;
        if (line (section, lane).enabled == on) return;
        checkpoint();
        auto& ln = line (section, lane);
        ln.enabled = on;
        if (on && ln.notes.empty())
        {
            ln.loopBars = juce::jmin (4, sections[(size_t) section].bars);
            // a new bass row starts from the chords' roots, so there is something to hear straight away
            if (lane == Lane::Bass && ! sections[(size_t) section].chords.empty())
                fillBassInternal (section, BassFill::Roots);
        }
        changed();
    }

    void SongMap::setLineLoop (int section, Lane lane, int loopBars)
    {
        if (! juce::isPositiveAndBelow (section, (int) sections.size()) || lane == Lane::Chords) return;
        if (line (section, lane).loopBars == loopBars) return;
        checkpoint();
        line (section, lane).loopBars = juce::jmax (0, loopBars);
        changed();
    }

    void SongMap::setLineNotes (int section, Lane lane, std::vector<Note> notes, bool undoable)
    {
        if (! juce::isPositiveAndBelow (section, (int) sections.size()) || lane == Lane::Chords) return;
        if (undoable) checkpoint();
        std::sort (notes.begin(), notes.end(), [] (const Note& x, const Note& y)
                   { return x.start < y.start || (std::abs (x.start - y.start) < 1.0e-9 && x.pitch < y.pitch); });
        line (section, lane).notes = std::move (notes);
        changed();
    }

    void SongMap::setKeyOverride (int key)
    {
        key = juce::jlimit (-1, 23, key);
        if (key >= 0) lastSetKey = key;
        else if (keyOverride >= 0) lastSetKey = keyOverride;     // unlocking: remember the key it was locked to
        if (keyOverride == key) return;
        keyOverride = key;
        changed();
    }

    bool SongMap::keyFromChords (Key& out) const
    {
        std::vector<TimedChord> run;
        const double bpb = beatsPerBar();
        for (const auto& sec : sections)
        {
            const int n = (int) sec.chords.size();
            if (n == 0 || sec.bars <= 0) continue;
            // the chords repeat through the section, as they play
            const double total = sec.bars * bpb;
            const double len = sec.barsPerChord <= 0.0f ? total / n : sec.barsPerChord * bpb;
            if (len <= 0.0) continue;
            for (int i = 0; (double) i * len < total - 1.0e-6 && i < 4096; ++i)
                run.push_back ({ sec.chords[(size_t) (i % n)], juce::jmin (len, total - i * len) });
        }
        if (run.empty())
            for (const auto& c : tray)
                run.push_back ({ c, bpb });
        return sm::keyFromChords (run, out);
    }

    void SongMap::fillBass (int section, BassFill pattern)
    {
        if (! juce::isPositiveAndBelow (section, (int) sections.size()) || sections[(size_t) section].chords.empty()) return;
        checkpoint();
        fillBassInternal (section, pattern);
        changed();
    }

    void SongMap::fillBassInternal (int section, BassFill pattern)
    {
        auto& sec = sections[(size_t) section];
        const int n = (int) sec.chords.size();
        if (n == 0) return;
        const double bpb = beatsPerBar(), total = sec.bars * bpb;
        const double len = sec.barsPerChord <= 0.0f ? total / n : sec.barsPerChord * bpb;
        double cycle = len * n;
        const double cycleBars = cycle / bpb;
        int loopBars = 0;
        if (sec.barsPerChord > 0.0f && std::abs (cycleBars - std::round (cycleBars)) < 1.0e-6 && cycleBars <= 8.0 + 1.0e-6
             && cycleBars < sec.bars + 1.0e-6)
            loopBars = (int) std::lround (cycleBars);
        else
            cycle = total;

        // bass register: E1 .. D#2 for the roots
        auto rootPitch = [] (const Chord& c) { const int pc = c.bass >= 0 ? c.bass : c.root; return 28 + ((pc - 28) % 12 + 12) % 12; };
        auto interval = [] (const Chord& c, std::initializer_list<int> wanted, int fallback)
        {
            const auto& iv = qualities()[(size_t) c.quality].intervals;
            for (int w : wanted)
                for (int i : iv)
                    if (i % 12 == w) return w;
            return fallback;
        };

        struct Span { double t0, t1; Chord c; };
        std::vector<Span> spans;
        for (int i = 0; (double) i * len < cycle - 1.0e-6; ++i)
            spans.push_back ({ i * len, juce::jmin ((i + 1) * len, cycle), sec.chords[(size_t) (i % n)] });

        std::vector<Note> out;
        auto add = [&out] (double t, double l, int p, float v) { out.push_back ({ t, juce::jmax (0.1, l), juce::jlimit (0, 127, p), v }); };
        for (size_t j = 0; j < spans.size(); ++j)
        {
            const auto& sp = spans[j];
            const int r = rootPitch (sp.c);
            const int fifth = interval (sp.c, { 7, 6, 8 }, 7);
            const int third = interval (sp.c, { 4, 3, 2, 5 }, 4);
            const int nextRoot = rootPitch (spans[(j + 1) % spans.size()].c);
            const double spanLen = sp.t1 - sp.t0;
            auto onBar = [bpb] (double t) { return std::abs (std::fmod (t, bpb)) < 1.0e-6; };
            switch (pattern)
            {
                case BassFill::RootPulse:
                    for (double t = sp.t0; t < sp.t1 - 1.0e-6; t += 0.5)
                        add (t, juce::jmin (0.5, sp.t1 - t) - 0.06, r, onBar (t) ? 0.86f : 0.70f);
                    break;
                case BassFill::RootFifth:
                {
                    int k = 0;
                    for (double t = sp.t0; t < sp.t1 - 1.0e-6; t += 1.0, ++k)
                        add (t, juce::jmin (1.0, sp.t1 - t) - 0.08, k % 2 == 0 ? r : r + fifth, onBar (t) ? 0.86f : 0.74f);
                    break;
                }
                case BassFill::Octaves:
                {
                    int k = 0;
                    for (double t = sp.t0; t < sp.t1 - 1.0e-6; t += 0.5, ++k)
                        add (t, juce::jmin (0.5, sp.t1 - t) - 0.06, k % 2 == 0 ? r : r + 12, onBar (t) ? 0.86f : 0.72f);
                    break;
                }
                case BassFill::Walking:
                {
                    // quarter notes through the chord, stepping into the next root on the last beat
                    const int steps[] = { 0, third, fifth, 12, fifth, third };
                    int k = 0;
                    for (double t = sp.t0; t < sp.t1 - 1.0e-6; t += 1.0, ++k)
                    {
                        const bool last = t + 1.0 >= sp.t1 - 1.0e-6;
                        int p = r + steps[k % 6];
                        if (last && spanLen >= 2.0 - 1.0e-6)
                            p = (nextRoot - 1 >= 26 && nextRoot != r) ? nextRoot - 1 : nextRoot + 1;   // chromatic approach
                        add (t, juce::jmin (1.0, sp.t1 - t) - 0.06, p, onBar (t) ? 0.86f : 0.76f);
                    }
                    break;
                }
                case BassFill::Roots:
                case BassFill::NumFills:
                default:
                    add (sp.t0, spanLen - 0.06, r, 0.84f);
                    break;
            }
        }
        sec.bass.enabled = true;
        sec.bass.loopBars = loopBars;
        sec.bass.notes = std::move (out);
    }

    void SongMap::resetSections (const std::vector<std::pair<juce::String, int>>& layout)
    {
        checkpoint();
        sections.clear();
        for (const auto& item : layout)
        {
            Section s;
            s.name = nextSectionName (item.first);
            s.bars = juce::jlimit (1, 256, item.second);
            s.uid = nextUid++;
            sections.push_back (s);
        }
        selectedSection = sections.empty() ? -1 : 0;
        changed();
    }

    void SongMap::clearTray()
    {
        checkpoint();
        tray.clear();
        selectedTray = -1;
        changed();
    }

    juce::String SongMap::nextSectionName (const juce::String& base) const
    {
        int highest = 0;
        bool plainExists = false;
        for (const auto& s : sections)
        {
            if (s.name == base) plainExists = true;
            if (s.name.startsWith (base + " "))
                highest = juce::jmax (highest, s.name.fromLastOccurrenceOf (" ", false, false).getIntValue());
        }
        const bool numbered = base == "Verse" || base == "Chorus" || base == "Pre-Chorus" || base == "Bridge"
                              || base == "Solo" || base == "Drop" || base == "Breakdown" || base == "Hook" || base == "Post-Chorus";
        if (! numbered && ! plainExists && highest == 0) return base;
        return base + " " + juce::String (juce::jmax (highest, plainExists ? 1 : 0) + 1);
    }

    int SongMap::totalBars() const
    {
        int t = 0;
        for (const auto& s : sections) t += s.bars;
        return t;
    }

    //==============================================================================
    std::vector<int> SongMap::notesFor (const Chord& c) const
    {
        if (cacheGuitar != guitar || cacheStyle != style)
        {
            noteCache.clear();
            cacheGuitar = guitar;
            cacheStyle = style;
        }
        const auto key = c.id();
        auto it = noteCache.find (key);
        if (it != noteCache.end()) return it->second;

        std::vector<int> notes = guitar ? guitarVoicing (c, style).midiNotes() : pianoVoicing (c, style);
        if (notes.empty()) notes = pianoVoicing (c, GuitarStyle::Standard);
        noteCache[key] = notes;
        return notes;
    }

    const char* playStyleName (PlayStyle s)
    {
        switch (s)
        {
            case PlayStyle::Pulse:    return "Pulse";
            case PlayStyle::Strum:    return "Strum";
            case PlayStyle::Arpeggio: return "Arpeggio";
            case PlayStyle::Block:
            case PlayStyle::NumStyles:
            default:                  return "Block";
        }
    }

    const char* playStyleDescription (PlayStyle s)
    {
        switch (s)
        {
            case PlayStyle::Pulse:    return "straight 8ths";
            case PlayStyle::Strum:    return "down-up guitar strum";
            case PlayStyle::Arpeggio: return "one note at a time";
            case PlayStyle::Block:
            case PlayStyle::NumStyles:
            default:                  return "held chords";
        }
    }

    std::vector<int> SongMap::playedNotes (const Chord& c) const
    {
        auto notes = notesFor (c);
        for (auto& n : notes) n = juce::jlimit (0, 127, n + 12 * juce::jlimit (-2, 2, octave));
        std::sort (notes.begin(), notes.end());
        notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
        return notes;
    }

    void SongMap::addChordHits (Sequence& seq, const std::vector<int>& notes, double start, double length, double gridStart,
                                int section, int chordIndex, int trayIndex) const
    {
        if (notes.empty() || length <= 1.0e-6) return;
        const double end = start + length;
        auto hit = [&] (double pos, double len, std::vector<int> ns, float vel, float strum)
        {
            SequenceEvent e;
            e.startBeat = pos;
            e.lengthBeats = juce::jmax (0.05, len);
            e.notes = std::move (ns);
            e.velocity = vel;
            e.strum = strum;
            e.section = section;
            e.chordIndex = chordIndex;
            e.trayIndex = trayIndex;
            seq.events.push_back (std::move (e));
        };

        if (playStyle == PlayStyle::Block)
        {
            hit (start, length, notes, 0.80f, 0.0f);
            return;
        }

        // the rhythms run on an 8th-note grid that stays locked to the bar lines
        const int perBar = juce::jmax (2, (int) std::lround (beatsPerBar() * 2.0));
        const double eighth = 0.5, eps = 1.0e-6;
        std::vector<double> slots;
        for (int k = (int) std::ceil ((start - gridStart) / eighth - eps); gridStart + k * eighth < end - eps; ++k)
            slots.push_back (gridStart + k * eighth);
        if (slots.empty() || slots.front() > start + eps) slots.insert (slots.begin(), start);
        auto slotInBar = [&] (double pos) { return ((int) std::lround ((pos - gridStart) / eighth) % perBar + perBar) % perBar; };

        if (playStyle == PlayStyle::Pulse)
        {
            for (size_t k = 0; k < slots.size(); ++k)
            {
                const double pos = slots[k];
                const double next = k + 1 < slots.size() ? slots[k + 1] : end;
                const int sb = slotInBar (pos);
                hit (pos, juce::jmin (0.40, next - pos - 0.03), notes, sb == 0 ? 0.88f : (sb % 2 == 0 ? 0.74f : 0.60f), 0.0f);
            }
            return;
        }

        if (playStyle == PlayStyle::Strum)
        {
            // D . D U . U D U  (4/4)   D . D U D U  (3/4)   D . U D . U  (6/8)
            std::vector<int> down, up;
            if (perBar == 8)                { down = { 0, 2, 6 }; up = { 3, 5, 7 }; }
            else if (timeSignature == 2)    { down = { 0, 3 };    up = { 2, 5 }; }
            else                            { down = { 0, 2, 4 }; up = { 3, 5 }; }
            auto contains = [] (const std::vector<int>& v, int x) { return std::find (v.begin(), v.end(), x) != v.end(); };
            std::vector<std::pair<double, bool>> hits;     // position, isDown
            for (double pos : slots)
            {
                const int sb = slotInBar (pos);
                const bool first = pos <= start + eps;
                if (first || contains (down, sb)) hits.push_back ({ pos, true });
                else if (contains (up, sb))       hits.push_back ({ pos, false });
            }
            const float spread = 0.022f;                    // ~11 ms between strings at 120 bpm
            for (size_t h = 0; h < hits.size(); ++h)
            {
                const double pos = hits[h].first;
                const double next = h + 1 < hits.size() ? hits[h + 1].first : end;
                const bool isDown = hits[h].second;
                const bool onBeat = slotInBar (pos) % 2 == 0;
                std::vector<int> ns = notes;
                if (! isDown && ns.size() > 3) ns.erase (ns.begin(), ns.begin() + (long) (ns.size() - 3));   // upstrokes catch the top strings
                hit (pos, next - pos - 0.04, ns, isDown ? (onBeat && slotInBar (pos) == 0 ? 0.90f : 0.80f) : 0.58f, isDown ? spread : -spread);
            }
            return;
        }

        // Arpeggio: up through the chord on 8ths; each note rings until just before it comes round again
        const int n = (int) notes.size();
        for (size_t k = 0; k < slots.size(); ++k)
        {
            const double pos = slots[k];
            const int note = notes[k % (size_t) n];
            const double again = k + (size_t) n < slots.size() ? slots[k + (size_t) n] : end;   // when this note comes round again
            const double ring = again - pos - 0.03;
            hit (pos, ring, { note }, slotInBar (pos) == 0 ? 0.86f : 0.70f, 0.0f);
        }
    }

    bool SongMap::sectionHasContent (int section) const
    {
        if (! juce::isPositiveAndBelow (section, (int) sections.size())) return false;
        const auto& s = sections[(size_t) section];
        return ! s.chords.empty() || (s.bass.enabled && ! s.bass.notes.empty()) || (s.lead.enabled && ! s.lead.notes.empty());
    }

    Sequence SongMap::buildSequence (int onlySection, int laneMask, bool skipEmpty) const
    {
        Sequence seq;
        const double bpb = beatsPerBar();
        double t = 0.0;
        for (int si = 0; si < (int) sections.size(); ++si)
        {
            if (onlySection >= 0 && si != onlySection) continue;
            if (skipEmpty && onlySection < 0 && ! sectionHasContent (si)) continue;
            const auto& s = sections[(size_t) si];
            const double total = s.bars * bpb;
            seq.spans.push_back ({ si, t, total });
            const int n = (int) s.chords.size();
            if (n > 0 && (laneMask & laneBit (Lane::Chords)) != 0)
            {
                const double len = s.barsPerChord <= 0.0f ? total / n : s.barsPerChord * bpb;
                double pos = 0.0;
                int i = 0;
                while (pos < total - 1.0e-6)
                {
                    addChordHits (seq, playedNotes (s.chords[(size_t) (i % n)]), t + pos, juce::jmin (len, total - pos), t,
                                  si, i % n, -1);
                    pos += len;
                    ++i;
                }
            }
            if ((laneMask & laneBit (Lane::Bass)) != 0) addLineNotes (seq, si, Lane::Bass, t);
            if ((laneMask & laneBit (Lane::Lead)) != 0) addLineNotes (seq, si, Lane::Lead, t);
            t += total;
        }
        // the sequencer plays events in time order
        std::stable_sort (seq.events.begin(), seq.events.end(),
                          [] (const SequenceEvent& x, const SequenceEvent& y) { return x.startBeat < y.startBeat; });
        seq.totalBeats = t;
        return seq;
    }

    void SongMap::addLineNotes (Sequence& seq, int si, Lane lane, double sectionStart) const
    {
        const auto& ln = line (si, lane);
        if (! ln.enabled || ln.notes.empty()) return;
        const double total = sectionBeats (si);
        const double loop = lineLoopBeats (si, lane);
        const int count = (int) ln.notes.size();
        // each note ends before the next note of the same pitch starts, and inside its loop
        std::vector<double> ends ((size_t) count);
        for (int k = 0; k < count; ++k)
        {
            const auto& nk = ln.notes[(size_t) k];
            double e = juce::jmin (nk.start + nk.length, loop);
            for (int j = 0; j < count; ++j)
            {
                const auto& nj = ln.notes[(size_t) j];
                if (j != k && nj.pitch == nk.pitch && nj.start > nk.start + 1.0e-6)
                    e = juce::jmin (e, nj.start);
            }
            ends[(size_t) k] = e;
        }
        for (double rep = 0.0; rep < total - 1.0e-6; rep += loop)
        {
            for (int k = 0; k < count; ++k)
            {
                const auto& n = ln.notes[(size_t) k];
                if (n.start >= loop - 1.0e-6) continue;
                const double at = rep + n.start;
                if (at >= total - 1.0e-6) continue;
                const double len = juce::jmin (ends[(size_t) k] - n.start, total - at) - 0.02;
                SequenceEvent e;
                e.startBeat = sectionStart + at;
                e.lengthBeats = juce::jmax (0.05, len);
                e.notes = { juce::jlimit (0, 127, n.pitch) };
                e.velocity = n.velocity;
                e.section = si;
                e.lane = (int) lane;
                e.noteIndex = k;
                e.sound = lane == Lane::Bass ? 1 : -1;
                seq.events.push_back (std::move (e));
            }
        }
    }

    Sequence SongMap::buildTraySequence() const
    {
        Sequence seq;
        const double bpb = beatsPerBar();
        for (int i = 0; i < (int) tray.size(); ++i)
            addChordHits (seq, playedNotes (tray[(size_t) i]), i * bpb, bpb, 0.0, -1, -1, i);
        seq.totalBeats = (double) tray.size() * bpb;
        return seq;
    }

    Sequence SongMap::sequenceFor (const std::vector<Chord>& chords) const
    {
        Sequence seq;
        const double bpb = beatsPerBar();
        for (int i = 0; i < (int) chords.size(); ++i)
            addChordHits (seq, playedNotes (chords[(size_t) i]), i * bpb, bpb, 0.0, -1, -1, -1);
        seq.totalBeats = (double) chords.size() * bpb;
        return seq;
    }

    //==============================================================================
    bool SongMap::laneHasNotes (int onlySection, Lane lane) const
    {
        for (int si = 0; si < (int) sections.size(); ++si)
        {
            if (onlySection >= 0 && si != onlySection) continue;
            const auto& sec = sections[(size_t) si];
            if (lane == Lane::Chords ? ! sec.chords.empty() : (line (si, lane).enabled && ! line (si, lane).notes.empty()))
                return true;
        }
        return false;
    }

    juce::MidiFile SongMap::createMidi (int onlySection, double bpm, int laneMask) const
    {
        constexpr int ppq = 960;
        const bool oneSection = onlySection >= 0 && juce::isPositiveAndBelow (onlySection, (int) sections.size());
        static const char* const laneNames[3] = { "Chords", "Bass", "Lead" };

        // which rows go in: every requested row that has notes (chords always, if requested, so a file is never empty)
        std::vector<Lane> lanes;
        for (int l = 0; l < 3; ++l)
            if ((laneMask & (1 << l)) != 0 && (l == 0 || laneHasNotes (onlySection, (Lane) l)))
                lanes.push_back ((Lane) l);
        if (lanes.empty())
            for (int l = 0; l < 3; ++l)
                if ((laneMask & (1 << l)) != 0) { lanes.push_back ((Lane) l); break; }
        const bool single = lanes.size() == 1;

        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        double totalBeats = 0.0;
        for (int si = 0; si < (int) sections.size(); ++si)
            if (! oneSection || si == onlySection) totalBeats += sectionBeats (si);

        for (size_t li = 0; li < lanes.size(); ++li)
        {
            const Lane lane = lanes[li];
            juce::MidiMessageSequence track;
            juce::String title = oneSection ? sections[(size_t) onlySection].name : songName;
            if (! single || lane != Lane::Chords) title += juce::String (" - ") + laneNames[(int) lane];
            auto name = juce::MidiMessage::textMetaEvent (3, title);
            name.setTimeStamp (0);
            track.addEvent (name);

            if (li == 0)    // tempo, metre and section markers live in the first track
            {
                auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / juce::jlimit (20.0, 400.0, bpm)));
                tempo.setTimeStamp (0);
                track.addEvent (tempo);
                auto ts = juce::MidiMessage::timeSignatureMetaEvent (timeSigNumerator(), timeSigDenominator());
                ts.setTimeStamp (0);
                track.addEvent (ts);
                double t = 0.0;
                for (int si = 0; si < (int) sections.size(); ++si)
                {
                    if (oneSection && si != onlySection) continue;
                    auto marker = juce::MidiMessage::textMetaEvent (6, sections[(size_t) si].name);
                    marker.setTimeStamp (t * ppq);
                    track.addEvent (marker);
                    t += sectionBeats (si);
                }
            }

            const auto seq = buildSequence (onlySection, laneBit (lane));
            for (const auto& e : seq.events)
            {
                const double off = (e.startBeat + e.lengthBeats) * ppq - 1.0;
                const size_t count = e.notes.size();
                for (size_t k = 0; k < count; ++k)
                {
                    const int note = juce::jlimit (0, 127, e.notes[k]);
                    // strums: each string a little later, low-to-high on downstrokes, high-to-low on upstrokes
                    const double order = e.strum > 0.0f ? (double) k : (double) (count - 1 - k);
                    const double on = (e.startBeat + std::abs (e.strum) * order) * ppq;
                    const float v = e.velocity * (k == 0 ? 1.0f : 0.92f);
                    const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) std::lround (v * 118.0f));
                    track.addEvent (juce::MidiMessage::noteOn (1, note, vel), on);
                    track.addEvent (juce::MidiMessage::noteOff (1, note), juce::jmax (on + 1.0, off));
                }
            }
            auto end = juce::MidiMessage::endOfTrack();
            end.setTimeStamp (juce::jmax (totalBeats * ppq, 1.0));
            track.addEvent (end);
            track.updateMatchedPairs();
            track.sort();
            file.addTrack (track);
        }
        return file;
    }

    juce::File SongMap::writeMidiFile (int onlySection, double bpm, const juce::File& folder, int laneMask) const
    {
        folder.createDirectory();
        juce::String base = songName;
        if (juce::isPositiveAndBelow (onlySection, (int) sections.size()))
            base += " - " + sections[(size_t) onlySection].name;
        if (laneMask == laneBit (Lane::Bass)) base += " - Bass";
        if (laneMask == laneBit (Lane::Lead)) base += " - Lead";
        if (laneMask == laneBit (Lane::Chords) && (laneHasNotes (onlySection, Lane::Bass) || laneHasNotes (onlySection, Lane::Lead)))
            base += " - Chords";
        auto f = folder.getChildFile (juce::File::createLegalFileName (base) + ".mid");
        f.deleteFile();
        juce::FileOutputStream out (f);
        if (! out.openedOk()) return {};
        const auto midi = createMidi (onlySection, bpm, laneMask);
        midi.writeTo (out, midi.getNumTracks() > 1 ? 1 : 0);
        out.flush();
        return f;
    }

    //==============================================================================
    static juce::String chordsToString (const std::vector<Chord>& v)
    {
        juce::StringArray a;
        for (const auto& c : v) a.add (c.id());
        return a.joinIntoString (" ");
    }

    static juce::String notesToString (const std::vector<Note>& v)
    {
        juce::StringArray a;
        for (const auto& n : v)
            a.add (juce::String (n.start, 4) + "," + juce::String (n.length, 4) + "," + juce::String (n.pitch) + "," + juce::String (n.velocity, 3));
        return a.joinIntoString (";");
    }

    static std::vector<Note> notesFromString (const juce::String& s)
    {
        std::vector<Note> v;
        for (const auto& item : juce::StringArray::fromTokens (s, ";", ""))
        {
            const auto f = juce::StringArray::fromTokens (item, ",", "");
            if (f.size() < 3) continue;
            Note n;
            n.start = juce::jmax (0.0, f[0].getDoubleValue());
            n.length = juce::jlimit (0.05, 1024.0, f[1].getDoubleValue());
            n.pitch = juce::jlimit (0, 127, f[2].getIntValue());
            n.velocity = f.size() > 3 ? juce::jlimit (0.05f, 1.0f, (float) f[3].getDoubleValue()) : 0.8f;
            v.push_back (n);
        }
        return v;
    }

    static std::vector<Chord> chordsFromString (const juce::String& s)
    {
        std::vector<Chord> v;
        for (const auto& t : juce::StringArray::fromTokens (s, " ", ""))
        {
            const auto c = parseChord (t.toStdString());
            if (c.isValid()) v.push_back (c);
        }
        return v;
    }

    juce::ValueTree SongMap::toValueTree() const
    {
        juce::ValueTree t ("SongMap");
        t.setProperty ("name", songName, nullptr);
        if (songId.isNotEmpty()) t.setProperty ("songId", songId, nullptr);
        if (libraryStamp != 0)   t.setProperty ("libStamp", juce::String (libraryStamp), nullptr);
        t.setProperty ("guitar", guitar, nullptr);
        t.setProperty ("style", (int) style, nullptr);
        t.setProperty ("keyOverride", keyOverride, nullptr);
        t.setProperty ("lastSetKey", lastSetKey, nullptr);
        t.setProperty ("followHostTempo", followHostTempo, nullptr);
        t.setProperty ("bpm", manualBpm, nullptr);
        t.setProperty ("timeSig", timeSignature, nullptr);
        t.setProperty ("genre", genre, nullptr);
        t.setProperty ("genreStyle", genreStyle, nullptr);
        t.setProperty ("favs", showFavourites, nullptr);
        t.setProperty ("volume", auditionVolume, nullptr);
        t.setProperty ("loop", loopPlayback, nullptr);
        t.setProperty ("playStyle", (int) playStyle, nullptr);
        t.setProperty ("octave", octave, nullptr);
        t.setProperty ("showLines", showLines, nullptr);
        t.setProperty ("mutes", (muteChords ? 1 : 0) | (muteBass ? 2 : 0) | (muteLead ? 4 : 0), nullptr);
        t.setProperty ("tray", chordsToString (tray), nullptr);
        t.setProperty ("selectedSection", selectedSection, nullptr);
        for (const auto& s : sections)
        {
            juce::ValueTree st ("Section");
            st.setProperty ("name", s.name, nullptr);
            st.setProperty ("bars", s.bars, nullptr);
            st.setProperty ("barsPerChord", s.barsPerChord, nullptr);
            st.setProperty ("chords", chordsToString (s.chords), nullptr);
            for (int l = 1; l <= 2; ++l)
            {
                const auto& ln = l == 1 ? s.bass : s.lead;
                const juce::String key = l == 1 ? "bass" : "lead";
                st.setProperty (key + "On", ln.enabled, nullptr);
                st.setProperty (key + "Loop", ln.loopBars, nullptr);
                st.setProperty (key + "Notes", notesToString (ln.notes), nullptr);
            }
            t.appendChild (st, nullptr);
        }
        return t;
    }

    void SongMap::fromValueTree (const juce::ValueTree& t, bool keepUndo)
    {
        if (! t.hasType ("SongMap")) return;
        if (keepUndo)
        {
            undoStack.push_back (capture());
            redoStack.clear();
        }
        else
        {
            undoStack.clear();
            redoStack.clear();
        }
        songName = t.getProperty ("name", "Song Map").toString();
        songId = t.getProperty ("songId", "").toString();
        libraryStamp = t.getProperty ("libStamp", "0").toString().getLargeIntValue();
        guitar = (bool) t.getProperty ("guitar", true);
        style = (GuitarStyle) juce::jlimit (0, (int) GuitarStyle::NumStyles - 1, (int) t.getProperty ("style", 0));
        keyOverride = juce::jlimit (-1, 23, (int) t.getProperty ("keyOverride", -1));
        lastSetKey = juce::jlimit (-1, 23, (int) t.getProperty ("lastSetKey", keyOverride));
        followHostTempo = (bool) t.getProperty ("followHostTempo", true);
        manualBpm = juce::jlimit (30.0, 300.0, (double) t.getProperty ("bpm", 120.0));
        timeSignature = juce::jlimit (0, 2, (int) t.getProperty ("timeSig", 0));
        genre = juce::jmax (0, (int) t.getProperty ("genre", 0));
        genreStyle = juce::jmax (0, (int) t.getProperty ("genreStyle", 0));
        showFavourites = (bool) t.getProperty ("favs", false);
        auditionVolume = juce::jlimit (0.0f, 1.0f, (float) (double) t.getProperty ("volume", 0.7));
        loopPlayback = (bool) t.getProperty ("loop", false);
        playStyle = (PlayStyle) juce::jlimit (0, (int) PlayStyle::NumStyles - 1, (int) t.getProperty ("playStyle", 0));
        octave = juce::jlimit (-2, 2, (int) t.getProperty ("octave", 0));
        showLines = (bool) t.getProperty ("showLines", true);
        {
            const int m = (int) t.getProperty ("mutes", 0);
            muteChords = (m & 1) != 0; muteBass = (m & 2) != 0; muteLead = (m & 4) != 0;
        }
        tray = chordsFromString (t.getProperty ("tray").toString());
        selectedTray = tray.empty() ? -1 : 0;

        sections.clear();
        for (const auto& st : t)
        {
            if (! st.hasType ("Section")) continue;
            Section s;
            s.name = st.getProperty ("name", "Section").toString();
            s.bars = juce::jlimit (1, 256, (int) st.getProperty ("bars", 8));
            s.barsPerChord = (float) (double) st.getProperty ("barsPerChord", 1.0);
            s.chords = chordsFromString (st.getProperty ("chords").toString());
            for (int l = 1; l <= 2; ++l)
            {
                auto& ln = l == 1 ? s.bass : s.lead;
                const juce::String key = l == 1 ? "bass" : "lead";
                ln.enabled = (bool) st.getProperty (key + "On", false);
                ln.loopBars = juce::jlimit (0, 64, (int) st.getProperty (key + "Loop", 4));
                ln.notes = notesFromString (st.getProperty (key + "Notes").toString());
            }
            s.uid = nextUid++;
            sections.push_back (s);
        }
        selectedSection = juce::jlimit (-1, (int) sections.size() - 1, (int) t.getProperty ("selectedSection", 0));
        noteCache.clear();
        changed();
    }

    //==============================================================================
    bool SongMap::Snapshot::sameAs (const SongMap& m) const
    {
        if (tray != m.tray || sections.size() != m.sections.size()
             || selectedSection != m.selectedSection || selectedTray != m.selectedTray)
            return false;
        for (size_t i = 0; i < sections.size(); ++i)
        {
            const auto& a = sections[i];
            const auto& b = m.sections[i];
            if (a.name != b.name || a.bars != b.bars || std::abs (a.barsPerChord - b.barsPerChord) > 1.0e-4f
                 || a.chords != b.chords || a.uid != b.uid || a.bass != b.bass || a.lead != b.lead)
                return false;
        }
        return true;
    }

    SongMap::Snapshot SongMap::capture() const
    {
        return { sections, tray, selectedSection, selectedTray };
    }

    void SongMap::restore (const Snapshot& snap)
    {
        sections = snap.sections;
        tray = snap.tray;
        selectedSection = snap.selectedSection;
        selectedTray = snap.selectedTray;
        changed();
    }

    void SongMap::checkpoint()
    {
        if (! undoStack.empty() && undoStack.back().sameAs (*this)) return;
        undoStack.push_back (capture());
        if (undoStack.size() > 60) undoStack.erase (undoStack.begin());
        redoStack.clear();
    }

    void SongMap::undo()
    {
        while (! undoStack.empty() && undoStack.back().sameAs (*this))   // skip no-op checkpoints
            undoStack.pop_back();
        if (undoStack.empty()) return;
        redoStack.push_back (capture());
        auto snap = undoStack.back();
        undoStack.pop_back();
        restore (snap);
    }

    void SongMap::redo()
    {
        if (redoStack.empty()) return;
        undoStack.push_back (capture());
        auto snap = redoStack.back();
        redoStack.pop_back();
        restore (snap);
    }

    //==============================================================================
    FavouritesStore::FavouritesStore (const juce::File& fileToUse)
    {
        file = fileToUse != juce::File() ? fileToUse
                                         : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                               .getChildFile ("KeyCompass").getChildFile ("favourites.json");
        load();
    }

    std::vector<Favourite> FavouritesStore::parse (const juce::String& text)
    {
        std::vector<Favourite> out;
        const auto json = juce::JSON::parse (text);
        if (auto* arr = json.getArray())
            for (const auto& v : *arr)
                if (v["roman"].toString().isNotEmpty())
                    out.push_back ({ v["name"].toString(), v["roman"].toString(), v["genre"].toString() });
        return out;
    }

    std::vector<Favourite> FavouritesStore::merge (const std::vector<Favourite>& a, const std::vector<Favourite>& b)
    {
        auto out = a;
        for (const auto& f : b)
            if (std::none_of (out.begin(), out.end(), [&] (const Favourite& x) { return x.roman == f.roman; }))
                out.push_back (f);
        return out;
    }

    void FavouritesStore::load()
    {
        const juce::ScopedLock sl (lock);
        items = parse (file.loadFileAsString());
    }

    juce::String FavouritesStore::toJson() const
    {
        const juce::ScopedLock sl (lock);
        juce::Array<juce::var> arr;
        for (const auto& f : items)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", f.name);
            o->setProperty ("roman", f.roman);
            o->setProperty ("genre", f.genre);
            arr.add (juce::var (o));
        }
        return juce::JSON::toString (juce::var (arr));
    }

    void FavouritesStore::save() const
    {
        const auto json = toJson();
        file.getParentDirectory().createDirectory();
        file.replaceWithText (json);
    }

    std::vector<Favourite> FavouritesStore::all() const
    {
        const juce::ScopedLock sl (lock);
        return items;
    }

    bool FavouritesStore::contains (const juce::String& roman) const
    {
        const juce::ScopedLock sl (lock);
        for (const auto& f : items) if (f.roman == roman) return true;
        return false;
    }

    void FavouritesStore::add (const Favourite& f)
    {
        {
            const juce::ScopedLock sl (lock);
            if (std::any_of (items.begin(), items.end(), [&] (const Favourite& x) { return x.roman == f.roman; })) return;
            items.push_back (f);
        }
        userChanged();
    }

    void FavouritesStore::remove (const juce::String& roman)
    {
        {
            const juce::ScopedLock sl (lock);
            items.erase (std::remove_if (items.begin(), items.end(), [&] (const Favourite& f) { return f.roman == roman; }), items.end());
        }
        userChanged();
    }

    void FavouritesStore::toggle (const Favourite& f)
    {
        if (contains (f.roman)) remove (f.roman);
        else                    add (f);
    }

    void FavouritesStore::userChanged()
    {
        save();
        sendChangeMessage();
        if (onUserChange) onUserChange();
    }

    void FavouritesStore::replaceAll (const std::vector<Favourite>& newItems)
    {
        {
            const juce::ScopedLock sl (lock);
            items = newItems;
        }
        save();
        sendChangeMessage();
    }
}
