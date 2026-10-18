// Song Mapper - audition sound and sequencer
#include "Audition.h"

namespace sm
{
    AuditionEngine::AuditionEngine()
    {
        prepare (44100.0);
    }

    void AuditionEngine::prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        const int capacity = (int) (sampleRate / 30.0) + 8;
        for (auto& v : voices)
        {
            v.active = false;
            v.line.assign ((size_t) capacity, 0.0f);
        }
    }

    //==============================================================================
    // message thread
    void AuditionEngine::pushCommand (Command c)
    {
        c.serial = nextSerial++;
        commands.push (c);
    }

    void AuditionEngine::freeFinishedSequences()
    {
        const auto acked = ackSerial.load();
        const auto playing = playingSerial.load();
        const auto* shown = activeSeq.load();
        for (auto it = ownedSequences.begin(); it != ownedSequences.end();)
        {
            if (it->serial <= acked && it->serial != playing && it->seq.get() != shown)
                it = ownedSequences.erase (it);
            else
                ++it;
        }
    }

    void AuditionEngine::playChord (const std::vector<int>& notes, Instrument instrument, double seconds)
    {
        Command c;
        c.type = Command::Chord;
        c.numNotes = (int) juce::jmin<size_t> (notes.size(), c.notes.size());
        for (int i = 0; i < c.numNotes; ++i) c.notes[(size_t) i] = notes[(size_t) i];
        c.instrument = instrument;
        c.seconds = seconds;
        pushCommand (c);
    }

    void AuditionEngine::playSequence (Sequence s, double bpm, bool loop, bool guitarSound)
    {
        Command c;
        c.type = Command::Sequence;
        c.bpm = juce::jlimit (20.0, 400.0, bpm);
        c.loop = loop;
        c.guitar = guitarSound;
        c.serial = nextSerial++;

        OwnedSequence owned;
        owned.serial = c.serial;
        owned.seq = std::make_unique<Sequence> (std::move (s));
        c.seq = owned.seq.get();
        const auto* previous = activeSeq.load();
        const bool wasPlaying = seqPlaying.load();
        activeSeq.store (c.seq);
        seqPlaying.store (true);
        currentEventIndex.store (-1);
        if (! commands.push (c))
        {
            // the audio side is not running (or far behind): don't pretend to play
            const Sequence* expected = c.seq;
            activeSeq.compare_exchange_strong (expected, previous);
            seqPlaying.store (wasPlaying && previous != nullptr);
            return;
        }
        ownedSequences.push_back (std::move (owned));
        freeFinishedSequences();
    }

    void AuditionEngine::updateSequence (Sequence s, double bpm, bool loop, bool guitarSound)
    {
        if (! seqPlaying.load())
            return;
        Command c;
        c.type = Command::Swap;
        c.bpm = juce::jlimit (20.0, 400.0, bpm);
        c.loop = loop;
        c.guitar = guitarSound;
        c.serial = nextSerial++;

        OwnedSequence owned;
        owned.serial = c.serial;
        owned.seq = std::make_unique<Sequence> (std::move (s));
        c.seq = owned.seq.get();
        const auto* previous = activeSeq.load();
        activeSeq.store (c.seq);              // before the push: the audio thread compares against it when a sequence ends
        if (! commands.push (c))
        {
            // the audio thread is not running (or is far behind): forget this version
            const Sequence* expected = c.seq;
            activeSeq.compare_exchange_strong (expected, previous);
            return;
        }
        ownedSequences.push_back (std::move (owned));
        freeFinishedSequences();
    }

    void AuditionEngine::stop()
    {
        Command c;
        c.type = Command::Stop;
        activeSeq.store (nullptr);
        seqPlaying.store (false);
        currentEventIndex.store (-1);
        pushCommand (c);
        freeFinishedSequences();
    }

    //==============================================================================
    // audio thread
    void AuditionEngine::finishSequence()
    {
        // finished - unless the UI has already asked for something newer
        if (activeSeq.load() == seq)
        {
            seqPlaying.store (false);
            currentEventIndex.store (-1);
        }
        seq = nullptr;
        playingSerial.store (0);
    }

    void AuditionEngine::releaseAll (bool fast)
    {
        for (auto& v : voices)
            if (v.active)
            {
                v.releasing = true;
                v.releaseMul = (float) std::exp (-1.0 / ((fast ? 0.04 : 0.12) * sampleRate));
                v.feedback = juce::jmin (v.feedback, 0.90f);
            }
    }

    void AuditionEngine::startNotes (const int* notes, int count, Instrument instrument, juce::int64 lengthSamples, int offset,
                                     float velocity, double strumSamples)
    {
        std::array<int, 8> sorted {};
        count = juce::jlimit (0, 8, count);
        for (int i = 0; i < count; ++i) sorted[(size_t) i] = notes[i];
        for (int i = 1; i < count; ++i)          // insertion sort (count <= 8)
            for (int j = i; j > 0 && sorted[(size_t) j] < sorted[(size_t) j - 1]; --j)
                std::swap (sorted[(size_t) j], sorted[(size_t) j - 1]);
        // strumSamples: > 0 low-to-high, < 0 high-to-low, 0 = the default (a quick guitar strum, piano together)
        const bool guitar = instrument == Instrument::Guitar;
        const double strum = std::abs (strumSamples) > 1.0e-9 ? strumSamples : (guitar && count > 1 ? 0.013 * sampleRate : 0.0);
        for (int i = 0; i < count; ++i)
        {
            const double order = strum >= 0.0 ? (double) i : (double) (count - 1 - i);
            const int delay = (int) (std::abs (strum) * order);
            startVoice (sorted[(size_t) i], instrument, juce::jmax<juce::int64> (1, lengthSamples - delay), offset + delay,
                        velocity * (i == 0 || count == 1 ? 1.06f : 0.88f));
        }
    }

    void AuditionEngine::startVoice (int note, Instrument instrument, juce::int64 lengthSamples, int delay, float velocity)
    {
        const bool guitar = instrument != Instrument::Piano;    // guitar and bass fall back to the plucked-string model
        Voice* v = nullptr;
        for (auto& cand : voices) if (! cand.active) { v = &cand; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& cand : voices) if (cand.level < v->level) v = &cand;   // steal the quietest
        }

        v->active = true;
        v->guitar = guitar;
        v->instrument = instrument;
        v->zone = nullptr;
        v->releasing = false;
        v->note = note;
        v->delay = juce::jmax (0, delay);
        v->samplesUntilRelease = juce::jmax<juce::int64> (1, lengthSamples);
        v->freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        v->env = 0.0f;
        v->level = 1.0f;
        v->releaseMul = (float) std::exp (-1.0 / (0.12 * sampleRate));

        // recorded guitar / bass when the samples are loaded
        if (guitar)
            v->zone = samples->zoneFor (instrument == Instrument::Bass ? SampleLibrary::Bass : SampleLibrary::Guitar, note);
        if (v->zone != nullptr)
        {
            const auto& z = *v->zone;
            v->pos = 0.0;
            v->step = z.rootRatio * std::pow (2.0, (note - z.rootNote) / 12.0) * z.sampleRate / sampleRate;
            v->gain = juce::jlimit (0.05f, 1.2f, velocity) * (instrument == Instrument::Bass ? 1.15f : 0.95f);
            v->env = 1.0f;
            v->releaseMul = (float) std::exp (-1.0 / ((instrument == Instrument::Bass ? 0.07 : 0.16) * sampleRate));
            return;
        }

        if (! guitar)
        {
            const double tau0 = 1.9 * std::pow (261.6 / v->freq, 0.35);
            for (size_t h = 0; h < 6; ++h)
            {
                const double hf = v->freq * (double) (h + 1);
                v->phase[h] = 0.0;
                v->amp[h] = hf < sampleRate * 0.45 ? velocity / std::pow ((double) (h + 1), 1.5) : 0.0;
                const double tau = tau0 / std::pow ((double) (h + 1), 0.7);
                v->decay[h] = std::exp (-1.0 / (tau * sampleRate));
            }
        }
        else
        {
            const double delayLen = juce::jlimit (2.0, (double) v->line.size() - 4.0, sampleRate / v->freq - 0.5);
            v->lineLen = (int) delayLen;
            v->fractional = (float) (delayLen - v->lineLen);
            const double t60 = 3.2 * std::pow (110.0 / v->freq, 0.3);
            v->feedback = (float) std::exp (std::log (0.001) / (v->freq * t60));
            v->brightness = 0.5f;

            // excitation: a short, softened noise burst
            float lp = 0.0f;
            const int fill = v->lineLen + 2;
            for (int i = 0; i < fill; ++i)
            {
                const float n = random.nextFloat() * 2.0f - 1.0f;
                lp += 0.55f * (n - lp);
                v->line[(size_t) i] = lp * velocity;
            }
            v->writePos = fill % (int) v->line.size();
            v->lastOut = 0.0f;
            v->env = 1.0f;
        }
    }

    float AuditionEngine::renderVoice (Voice& v)
    {
        if (v.delay > 0) { --v.delay; return 0.0f; }

        if (! v.releasing && --v.samplesUntilRelease <= 0)
            v.releasing = true;

        float out = 0.0f;
        if (v.zone != nullptr)
        {
            // recorded sample, 4-point Hermite interpolation
            const auto& data = v.zone->data;
            const int n = (int) data.size();
            const int i = (int) v.pos;
            if (i + 2 >= n)
            {
                v.active = false;
                return 0.0f;
            }
            const float f = (float) (v.pos - (double) i);
            const float xm1 = data[(size_t) (i > 0 ? i - 1 : 0)], x0 = data[(size_t) i], x1 = data[(size_t) i + 1], x2 = data[(size_t) i + 2];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            const float sample = (((c3 * f + c2) * f + c1) * f + x0) * (1.0f / 32768.0f);
            v.pos += v.step;
            if (v.releasing) v.env *= v.releaseMul;
            out = sample * v.gain * v.env;
            v.level = v.env * v.gain * (1.0f - (float) i / (float) n);
            if (v.releasing && v.env < 1.0e-4f) v.active = false;
            return out;
        }
        if (! v.guitar)
        {
            if (v.releasing)        v.env *= v.releaseMul;
            else if (v.env < 1.0f)  v.env = juce::jmin (1.0f, v.env + (float) (1.0 / (0.004 * sampleRate)));
            double s = 0.0;
            const double w0 = juce::MathConstants<double>::twoPi * v.freq / sampleRate;
            for (size_t h = 0; h < 6; ++h)
            {
                if (v.amp[h] <= 1.0e-6) continue;
                s += v.amp[h] * std::sin (v.phase[h]);
                v.phase[h] += w0 * (double) (h + 1);
                if (v.phase[h] > juce::MathConstants<double>::twoPi) v.phase[h] -= juce::MathConstants<double>::twoPi;
                v.amp[h] *= v.decay[h];
            }
            out = (float) s * v.env;
            v.level = v.env * (float) v.amp[0];
        }
        else
        {
            const int size = (int) v.line.size();
            auto read = [&] (double delay)
            {
                double p = (double) v.writePos - delay;
                while (p < 0) p += size;
                const int i0 = (int) p;
                const float frac = (float) (p - i0);
                const float a = v.line[(size_t) (i0 % size)], b = v.line[(size_t) ((i0 + 1) % size)];
                return a + (b - a) * frac;
            };
            const double d = (double) v.lineLen + v.fractional;
            const float y1 = read (d), y2 = read (d + 1.0);
            float fb = v.feedback;
            if (v.releasing) fb *= 0.995f;                 // damp the string...
            const float y = fb * (0.5f * y1 + 0.5f * y2);
            v.line[(size_t) v.writePos] = y;
            v.writePos = (v.writePos + 1) % size;
            if (v.releasing) v.env *= v.releaseMul;       // ...and fade it out
            out = y * v.env;
            v.level = v.level * 0.999f + 0.001f * std::abs (y) * 4.0f;
        }

        if (v.releasing && (v.guitar ? v.env < 1.0e-4f : v.level < 1.0e-4f))
            v.active = false;
        return out;
    }

    void AuditionEngine::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        Command c;
        while (commands.pop (&c, 1) == 1)
        {
            switch (c.type)
            {
                case Command::Chord:
                    startNotes (c.notes.data(), c.numNotes, c.instrument, (juce::int64) (c.seconds * sampleRate), 0, 0.8f, 0.0);
                    break;
                case Command::Sequence:
                    releaseAll (true);
                    seq = c.seq;
                    playingSerial.store (c.serial);
                    seqBpm = c.bpm;
                    seqLoop = c.loop;
                    seqGuitar = c.guitar;
                    seqPos = 0.0;
                    nextEvent = 0;
                    if (seq != nullptr && seq->totalBeats <= 0.0)
                        finishSequence();      // nothing to play
                    break;
                case Command::Swap:
                    if (seq == nullptr)
                    {
                        // the old version finished just before this arrived
                        if (activeSeq.load() == c.seq)
                        {
                            seqPlaying.store (false);
                            currentEventIndex.store (-1);
                        }
                        playingSerial.store (0);
                        break;
                    }
                    seq = c.seq;
                    playingSerial.store (c.serial);
                    seqBpm = c.bpm;
                    seqLoop = c.loop;
                    seqGuitar = c.guitar;
                    if (seq->totalBeats <= 0.0)
                    {
                        finishSequence();
                        break;
                    }
                    if (seqPos >= seq->totalBeats)
                        seqPos = seqLoop ? std::fmod (seqPos, seq->totalBeats) : seq->totalBeats;
                    {
                        // carry on from the same place: the next event is the first one not yet reached
                        size_t lo = 0, hi = seq->events.size();
                        while (lo < hi)
                        {
                            const size_t mid = (lo + hi) / 2;
                            if (seq->events[mid].startBeat < seqPos) lo = mid + 1; else hi = mid;
                        }
                        nextEvent = lo;
                        currentEventIndex.store (lo > 0 ? (int) lo - 1 : -1);
                    }
                    break;
                case Command::Stop:
                    seq = nullptr;
                    playingSerial.store (0);
                    releaseAll (true);
                    break;
            }
            ackSerial.store (c.serial);
        }

        if (seq != nullptr && seq->totalBeats > 0.0)
        {
            const double beatsPerSample = seqBpm / 60.0 / sampleRate;
            const double blockEnd = seqPos + numSamples * beatsPerSample;
            while (nextEvent < seq->events.size() && seq->events[nextEvent].startBeat < blockEnd)
            {
                const auto& e = seq->events[nextEvent];
                const int offset = juce::jlimit (0, numSamples - 1, (int) ((e.startBeat - seqPos) / beatsPerSample));
                const auto len = (juce::int64) (e.lengthBeats / beatsPerSample - 0.03 * sampleRate);
                startNotes (e.notes.data(), (int) juce::jmin<size_t> (e.notes.size(), 8), instrumentFor (e),
                            juce::jmax<juce::int64> (len, 1), offset, e.velocity, (double) e.strum / beatsPerSample);
                currentEventIndex.store ((int) nextEvent);
                ++nextEvent;
            }
            seqPos = blockEnd;
            if (seqPos >= seq->totalBeats)
            {
                if (seqLoop)
                {
                    seqPos -= seq->totalBeats;
                    nextEvent = 0;
                }
                else
                {
                    finishSequence();
                }
            }
            if (seq != nullptr) seqPosition.store (seqPos);
        }

        bool any = false;
        for (auto& v : voices) if (v.active) { any = true; break; }
        if (! any) return;

        const float gain = volume.load() * 0.32f;
        const int channels = buffer.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (auto& v : voices)
                if (v.active) sum += renderVoice (v);
            const float out = std::tanh (sum * gain * 1.4f) * 0.8f;
            for (int ch = 0; ch < channels; ++ch)
                buffer.addSample (ch, startSample + i, out);
        }
    }
}
