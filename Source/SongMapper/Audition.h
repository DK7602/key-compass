// Song Mapper - built-in audition sound (piano or plucked guitar) and a tiny sequencer.
// The UI (message thread) sends requests; render() runs on the audio thread and adds the sound to the output.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Engine/LockFree.h"
#include "SongModel.h"
#include "SampleLibrary.h"
#include <array>
#include <atomic>
#include <deque>
#include <memory>

namespace sm
{
    // the sounds: piano (synthesised), acoustic guitar and electric bass (recorded samples)
    enum class Instrument : int { Piano = 0, Guitar = 1, Bass = 2 };

    class AuditionEngine
    {
    public:
        AuditionEngine();

        // ---- message thread ----
        void playChord (const std::vector<int>& notes, Instrument instrument, double seconds = 1.6);
        void playChord (const std::vector<int>& notes, bool guitarSound, double seconds = 1.6)
        {
            playChord (notes, guitarSound ? Instrument::Guitar : Instrument::Piano, seconds);
        }
        bool samplesReady() const noexcept { return samples->isReady(); }
        bool waitForSamples (int milliseconds) const { return samples->waitUntilReady (milliseconds); }
        void playSequence (Sequence seq, double bpm, bool loop, bool guitarSound);
        // swap in an edited version of the playing sequence without restarting it (notes already sounding carry on)
        void updateSequence (Sequence seq, double bpm, bool loop, bool guitarSound);
        void stop();
        void setVolume (float v) noexcept { volume.store (juce::jlimit (0.0f, 1.0f, v)); }

        // what is sounding now (for highlighting)
        bool isSequencePlaying() const noexcept    { return seqPlaying.load(); }
        int currentEvent() const noexcept          { return currentEventIndex.load(); }
        double positionBeats() const noexcept      { return seqPosition.load(); }
        const Sequence* currentSequence() const noexcept { return activeSeq.load(); }

        // ---- audio thread ----
        void prepare (double sampleRate);
        void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    private:
        struct Command
        {
            enum Type : int { Chord, Sequence, Swap, Stop } type = Chord;
            std::array<int, 8> notes {};
            int numNotes = 0;
            bool guitar = true;
            Instrument instrument = Instrument::Guitar;
            double seconds = 1.0, bpm = 120.0;
            bool loop = false;
            const sm::Sequence* seq = nullptr;
            juce::uint32 serial = 0;
        };

        struct Voice
        {
            bool active = false, guitar = true, releasing = false;
            Instrument instrument = Instrument::Guitar;
            // recorded sample playback
            const SampleZone* zone = nullptr;
            double pos = 0.0, step = 1.0;
            float gain = 1.0f;
            int note = 0, delay = 0;
            juce::int64 samplesUntilRelease = 0;
            double freq = 0.0;
            // piano
            std::array<double, 6> phase {}, amp {}, decay {};
            // guitar (Karplus-Strong)
            std::vector<float> line;
            int lineLen = 0, writePos = 0;
            float fractional = 0.0f, lastOut = 0.0f, feedback = 0.996f, brightness = 0.5f;
            float env = 0.0f, releaseMul = 1.0f, level = 0.0f;
        };

        void startNotes (const int* notes, int count, Instrument instrument, juce::int64 lengthSamples, int offset,
                         float velocity, double strumSamples);
        void startVoice (int note, Instrument instrument, juce::int64 lengthSamples, int delay, float velocity);
        Instrument instrumentFor (const SequenceEvent& e) const noexcept
        {
            if (e.sound == 1) return Instrument::Bass;
            if (e.sound == 0) return Instrument::Piano;
            if (e.sound == 2) return Instrument::Guitar;
            return seqGuitar ? Instrument::Guitar : Instrument::Piano;
        }
        float renderVoice (Voice& v);
        void releaseAll (bool fast);
        void finishSequence();
        void pushCommand (Command c);
        void freeFinishedSequences();

        double sampleRate = 44100.0;
        juce::SharedResourcePointer<SampleLibrary> samples;
        std::array<Voice, 48> voices;
        kc::SpscRing<Command> commands { 256 };
        juce::Random random;

        // Sequences are owned by the message thread; the audio thread only reads them.
        // One is freed only once the audio thread has moved past it (ackSerial / playingSerial).
        struct OwnedSequence { juce::uint32 serial = 0; std::unique_ptr<Sequence> seq; };
        std::deque<OwnedSequence> ownedSequences;
        std::atomic<const Sequence*> activeSeq { nullptr };
        juce::uint32 nextSerial = 1;                       // message thread
        std::atomic<juce::uint32> ackSerial { 0 }, playingSerial { 0 };

        // sequencer state (audio thread)
        const Sequence* seq = nullptr;
        double seqBpm = 120.0, seqPos = 0.0;
        bool seqLoop = false, seqGuitar = true;
        size_t nextEvent = 0;

        std::atomic<bool> seqPlaying { false };
        std::atomic<int> currentEventIndex { -1 };
        std::atomic<double> seqPosition { 0.0 };
        std::atomic<float> volume { 0.7f };
    };
}
