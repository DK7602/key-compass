// Key Compass - analysis engine
//
// Threading model
//  - Audio thread:     pushMidi() / pushAudio()   (lock-free, never blocks)
//  - Analysis thread:  process()                  (does all the maths)
//  - UI thread:        getSnapshot(), setters     (short mutex only around the snapshot copy)
//
// Sources
//  - Audio: chroma (pitch-class energy) from an FFT of a decimated signal, or YIN pitch in Mono mode
//  - MIDI:  when MIDI notes arrive they are used instead of audio (exact notes)
#pragma once

#include "MusicTheory.h"
#include "LockFree.h"

#include <array>
#include <atomic>
#include <complex>
#include <cstdint>
#include <mutex>
#include <vector>

namespace kc
{
    enum class AnalysisMode : int  { Track = 0, Mono = 1, Song = 2 };
    enum class ResponseSpeed : int { Fast = 0, Balanced = 1, Steady = 2 };

    struct HistoryEntry
    {
        enum class Kind : uint8_t { Chord, Note, KeyChange };
        Kind kind = Kind::Chord;
        ChordId chord;          // Kind::Chord
        int midiNote = -1;      // Kind::Note
        KeyId key;              // Kind::KeyChange
    };

    struct EngineSnapshot
    {
        uint32_t serial = 0;
        AnalysisMode mode = AnalysisMode::Track;
        bool signalPresent = false;      // something audible / held right now
        bool sourceIsMidi = false;
        float inputLevelDb = -100.0f;

        // Key
        bool hasKey = false;
        KeyId key;
        float confidence = 0.0f;         // 0..1
        bool locked = false;
        KeyId runnerUp;
        bool showRunnerUp = false;
        float evidenceSeconds = 0.0f;

        // Live
        std::array<float, 12> noteLevels {};   // 0..1 per pitch class
        ChordId chord;                          // current chord / single note (invalid = none)

        // Mono pitch
        bool monoVoiced = false;
        int monoNote = -1;
        float monoCents = 0.0f;
        float monoFrequency = 0.0f;

        static constexpr int maxHistory = 16;
        std::array<HistoryEntry, maxHistory> history {};   // oldest first
        int historySize = 0;
    };

    //==============================================================================
    class KeyEngine
    {
    public:
        KeyEngine();

        // ---- audio thread (real-time safe) ----
        // Call pushMidi() for a block's events first (offset = sample position inside the block),
        // then pushAudio() with the block's samples.
        void pushMidi (int noteNumber, float velocity, int sampleOffsetInBlock) noexcept;  // velocity <= 0 means note-off
        void pushAllNotesOff (int sampleOffsetInBlock) noexcept;
        void pushSustainPedal (bool down, int sampleOffsetInBlock) noexcept;
        void pushAudio (const float* mono, int numSamples) noexcept;

        // ---- any thread ----
        void prepare (double sampleRate) noexcept;          // applied lazily by the analysis thread
        void setMode (AnalysisMode m) noexcept              { modeParam.store ((int) m); }
        void setResponse (ResponseSpeed r) noexcept         { responseParam.store ((int) r); }
        void setReferencePitch (float a4Hz) noexcept        { a4Param.store (a4Hz); }
        void setLocked (bool shouldLock) noexcept           { lockParam.store (shouldLock); }
        void requestReset() noexcept                        { resetRequested.store (true); }
        void restoreKey (KeyId key, float confidence) noexcept;   // e.g. a locked key saved with the project

        // ---- analysis thread ----
        void process();

        // ---- UI thread ----
        EngineSnapshot getSnapshot() const;

    private:
        //==============================================================================
        struct MidiEvent { int64_t time = 0; int16_t note = 0; float velocity = 0; };
        static constexpr int16_t allNotesOffCode = -1, pedalDownCode = -2, pedalUpCode = -3;

        struct Biquad
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            void setLowpass (double fc, double fs, double q) noexcept;
            inline double process (double x) noexcept
            {
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
        };

        struct Timing
        {
            double keyTau, keyHold, evidenceFull, evidenceMin, chordTau, chordCommit, noteCommit;
        };

        struct Peak { double freq; double amp; int bin; };

        // setup
        void reinitialise (double sampleRate);
        void resetAnalysisState (bool keepKey);
        Timing currentTiming() const noexcept;

        // per-hop work
        void processHop();
        void consumeMidiUntil (int64_t endTime, int64_t startTime);
        void computeSpectrum();
        void computeAudioChroma (float* chroma, float* bassChroma, float& tonalness);
        bool runYin (float& frequency, float& clarity);
        void accumulateKeyEvidence (const float* chroma, const float* bassChroma, double weight, double hopSeconds, const Timing&);
        void updateKeyDecision (double hopSeconds, const Timing&);
        ChordId matchChord (const float* chroma, int bassPc, const ChordId& current, float& score) const;
        void updateChord (const ChordId& candidate, bool haveCandidate, double hopSeconds, const Timing&);
        void updateMonoNote (bool voiced, float frequency, float clarity, double hopSeconds, const Timing&);
        void pushHistory (const HistoryEntry& e);
        void publishSnapshot();
        void fft (std::vector<std::complex<float>>& data) const;

        //==============================================================================
        // Parameters (written from any thread)
        std::atomic<int> modeParam { (int) AnalysisMode::Track };
        std::atomic<int> responseParam { (int) ResponseSpeed::Balanced };
        std::atomic<float> a4Param { 440.0f };
        std::atomic<bool> lockParam { false };
        std::atomic<bool> resetRequested { false };
        std::atomic<double> requestedSampleRate { 44100.0 };
        std::atomic<int> restoreKeyIndex { -1 };
        std::atomic<float> restoreKeyConfidence { 0.0f };

        // Audio-thread -> analysis-thread queues
        SpscRing<float> audioFifo { 1 << 18 };
        SpscRing<MidiEvent> midiFifo { 1 << 12 };
        int64_t audioSamplesPushed = 0;           // audio thread only
        std::atomic<int64_t> droppedSamples { 0 }; // samples lost because the FIFO was full
        int64_t droppedSeen = 0;

        // Analysis-thread state -------------------------------------------------------
        double inputRate = 0.0, analysisRate = 11025.0;
        int decimation = 4, decimationPhase = 0;
        std::array<Biquad, 4> antiAlias {};
        std::vector<float> ring;                  // decimated signal history
        size_t ringMask = 0;
        uint64_t decimatedCount = 0;
        int64_t inputSamplesConsumed = 0;
        int hopSize = 256, fftSize = 4096, samplesSinceHop = 0;
        std::vector<float> window, fftMag, envPrefix, yinBuffer, yinDiff, yinRaw;
        int yinWindow = 512, yinTauMax = 256, yinTauMin = 4;
        std::vector<std::complex<float>> fftData, twiddles;
        std::vector<int> bitReverse;
        std::vector<Peak> peaks;

        // MIDI state
        std::array<float, 128> midiHeld {};         // sounding (held by key or pedal)
        std::array<bool, 128> midiKeyDown {};       // physically held
        bool sustainPedal = false;
        std::array<double, 128> midiHeldTime {};
        std::array<int, 12> midiOnsets {};
        int64_t midiCursor = 0;
        int64_t lastMidiActivity = -1;
        int lastMidiNoteOn = -1;
        bool midiEverSeen = false;

        // Key evidence (decayed histograms)
        std::array<double, 12> keyChroma {}, keyBass {};
        std::array<double, 24> chordHistogram {};
        double evidence = 0.0;
        std::array<double, 24> keyScore {}, keyProb {}, smoothedScore {};
        bool smoothedValid = false;
        KeyId currentKey, challengerKey, runnerUpKey;
        double challengerTime = 0.0;
        float confidence = 0.0f, restoredConfidence = 0.0f;
        bool showRunnerUp = false;
        bool keyRestored = false;

        // Live chord / notes
        std::array<float, 12> shortChroma {}, displayChroma {}, noteLevels {};
        ChordId currentChord, pendingChord, lastCommittedChord;
        double pendingTime = 0.0, silenceTime = 0.0, lastRealChordTime = -100.0;
        bool signalPresent = false, sourceIsMidi = false;
        float inputLevelDb = -100.0f;

        // Mono
        int monoCandidate = -1, monoNote = -1;
        double monoCandidateTime = 0.0, monoSilence = 0.0;
        float monoCents = 0.0f, monoFreq = 0.0f;
        bool monoVoiced = false;
        std::array<float, 5> recentPitches {};
        int recentPitchCount = 0;
        int lastHistoryNote = -1;

        // History
        std::array<HistoryEntry, EngineSnapshot::maxHistory> history {};
        int historySize = 0;

        // Snapshot shared with the UI
        mutable std::mutex snapshotLock;
        EngineSnapshot snapshot;
        uint32_t serial = 0;
    };
}
