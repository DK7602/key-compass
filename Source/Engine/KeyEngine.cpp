// Key Compass - analysis engine
#include "KeyEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace kc
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double silenceGateDb = -60.0;
       #ifdef KC_TUNING
        double softmaxTemperature = 0.05, fitWeight = 1.2, bassWeight = 0.12, chordWeight = 0.15;
       #else
        constexpr double softmaxTemperature = 0.05, fitWeight = 1.2, bassWeight = 0.12, chordWeight = 0.15;
       #endif

        template <typename T> T clamp01 (T v) noexcept { return v < T (0) ? T (0) : (v > T (1) ? T (1) : v); }

        inline int wrap12 (int v) noexcept { return ((v % 12) + 12) % 12; }
    }

    //==============================================================================
    void KeyEngine::Biquad::setLowpass (double fc, double fs, double q) noexcept
    {
        const double w0 = 2.0 * pi * fc / fs;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cw) * 0.5 / a0;
        b1 = (1.0 - cw) / a0;
        b2 = b0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
        z1 = z2 = 0.0;
    }

    //==============================================================================
    KeyEngine::KeyEngine()
    {
       #ifdef KC_TUNING
        if (auto* v = std::getenv ("KC_T"))  softmaxTemperature = std::atof (v);
        if (auto* v = std::getenv ("KC_FW")) fitWeight = std::atof (v);
        if (auto* v = std::getenv ("KC_BW")) bassWeight = std::atof (v);
        if (auto* v = std::getenv ("KC_CW")) chordWeight = std::atof (v);
       #endif
        reinitialise (44100.0);
        resetAnalysisState (false);
        publishSnapshot();
    }

    void KeyEngine::prepare (double sampleRate) noexcept
    {
        if (sampleRate > 4000.0)
            requestedSampleRate.store (sampleRate);
    }

    void KeyEngine::restoreKey (KeyId key, float conf) noexcept
    {
        restoreKeyConfidence.store (conf);
        restoreKeyIndex.store (key.isValid() ? key.index() : -1);
    }

    //==============================================================================
    // Audio thread
    void KeyEngine::pushMidi (int noteNumber, float velocity, int sampleOffsetInBlock) noexcept
    {
        if (noteNumber < 0 || noteNumber > 127) return;
        MidiEvent e;
        e.time = audioSamplesPushed + std::max (0, sampleOffsetInBlock);
        e.note = (int16_t) noteNumber;
        e.velocity = velocity;
        midiFifo.push (e);
    }

    void KeyEngine::pushAllNotesOff (int sampleOffsetInBlock) noexcept
    {
        MidiEvent e;
        e.time = audioSamplesPushed + std::max (0, sampleOffsetInBlock);
        e.note = allNotesOffCode;
        midiFifo.push (e);
    }

    void KeyEngine::pushSustainPedal (bool down, int sampleOffsetInBlock) noexcept
    {
        MidiEvent e;
        e.time = audioSamplesPushed + std::max (0, sampleOffsetInBlock);
        e.note = down ? pedalDownCode : pedalUpCode;
        midiFifo.push (e);
    }

    void KeyEngine::pushAudio (const float* mono, int numSamples) noexcept
    {
        if (numSamples <= 0) return;
        const size_t written = audioFifo.push (mono, (size_t) numSamples);
        if (written < (size_t) numSamples)
            droppedSamples.fetch_add ((int64_t) ((size_t) numSamples - written), std::memory_order_relaxed);
        audioSamplesPushed += numSamples;
    }

    //==============================================================================
    void KeyEngine::reinitialise (double sampleRate)
    {
        inputRate = sampleRate;
        decimation = std::max (1, (int) std::floor (sampleRate / 11025.0 + 1.0e-6));
        analysisRate = sampleRate / decimation;

        static const double qs[4] = { 0.50980, 0.60134, 0.89998, 2.56292 };   // 8th-order Butterworth
        for (size_t i = 0; i < antiAlias.size(); ++i)
            antiAlias[i].setLowpass (0.38 * analysisRate, sampleRate, qs[i]);

        fftSize = analysisRate <= 12500.0 ? 4096 : 8192;
        hopSize = fftSize / 16;

        size_t ringSize = 1;
        while (ringSize < (size_t) fftSize * 2) ringSize <<= 1;
        ring.assign (ringSize, 0.0f);
        ringMask = ringSize - 1;

        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / fftSize));

        fftData.assign ((size_t) fftSize, {});
        twiddles.resize ((size_t) fftSize / 2);
        for (int k = 0; k < fftSize / 2; ++k)
            twiddles[(size_t) k] = std::polar (1.0f, (float) (-2.0 * pi * k / fftSize));

        bitReverse.resize ((size_t) fftSize);
        int bits = 0;
        while ((1 << bits) < fftSize) ++bits;
        for (int i = 0; i < fftSize; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            bitReverse[(size_t) i] = r;
        }

        fftMag.assign ((size_t) fftSize / 2 + 1, 0.0f);
        envPrefix.assign ((size_t) fftSize / 2 + 2, 0.0f);
        peaks.reserve (512);

        yinWindow = (int) std::lround (0.05 * analysisRate);
        yinTauMax = (int) std::lround (analysisRate / 40.0);
        yinTauMin = std::max (2, (int) std::lround (analysisRate / 2000.0));
        yinBuffer.assign ((size_t) (yinWindow + yinTauMax + 2), 0.0f);
        yinDiff.assign ((size_t) yinTauMax + 2, 0.0f);
        yinRaw.assign ((size_t) yinTauMax + 2, 0.0f);

        decimatedCount = 0;
        samplesSinceHop = 0;
        decimationPhase = 0;
        shortChroma.fill (0.0f);
        displayChroma.fill (0.0f);
    }

    void KeyEngine::resetAnalysisState (bool keepKey)
    {
        keyChroma.fill (0.0);
        keyBass.fill (0.0);
        chordHistogram.fill (0.0);
        keyScore.fill (0.0);
        keyProb.fill (0.0);
        smoothedScore.fill (0.0);
        smoothedValid = false;
        evidence = 0.0;
        if (! keepKey)
        {
            currentKey = {};
            confidence = 0.0f;
            keyRestored = false;
        }
        challengerKey = {};
        runnerUpKey = {};
        challengerTime = 0.0;
        showRunnerUp = false;

        shortChroma.fill (0.0f);
        displayChroma.fill (0.0f);
        noteLevels.fill (0.0f);
        currentChord = pendingChord = lastCommittedChord = {};
        pendingTime = silenceTime = 0.0;
        lastRealChordTime = -100.0;

        monoCandidate = monoNote = -1;
        monoCandidateTime = monoSilence = 0.0;
        monoVoiced = false;
        recentPitchCount = 0;
        lastHistoryNote = -1;

        historySize = 0;
    }

    KeyEngine::Timing KeyEngine::currentTiming() const noexcept
    {
        Timing t {};
        switch ((ResponseSpeed) responseParam.load())
        {
            case ResponseSpeed::Fast:     t = { 10.0, 1.2, 4.0,  1.0, 0.10, 0.12, 0.05 }; break;
            case ResponseSpeed::Steady:   t = { 60.0, 5.0, 14.0, 3.5, 0.28, 0.35, 0.12 }; break;
            case ResponseSpeed::Balanced:
            default:                      t = { 25.0, 2.5, 8.0,  2.0, 0.16, 0.20, 0.08 }; break;
        }

        if ((AnalysisMode) modeParam.load() == AnalysisMode::Song)
        {
            t.keyTau *= 2.0;  t.keyHold *= 1.6;  t.evidenceFull *= 1.5;  t.evidenceMin *= 1.5;
            t.chordTau *= 1.5; t.chordCommit *= 1.5;
        }
        return t;
    }

    //==============================================================================
    // Analysis thread
    void KeyEngine::process()
    {
        const double sr = requestedSampleRate.load();
        if (std::abs (sr - inputRate) > 0.5)
            reinitialise (sr);

        bool stateChanged = false;
        if (resetRequested.exchange (false))
        {
            resetAnalysisState (lockParam.load() && currentKey.isValid());
            stateChanged = true;
        }

        const int rk = restoreKeyIndex.exchange (-1);
        if (rk >= 0)
        {
            stateChanged = true;
            currentKey = KeyId::fromIndex (rk);
            restoredConfidence = restoreKeyConfidence.load();
            confidence = restoredConfidence;
            keyRestored = true;
        }

        const int64_t dropped = droppedSamples.load (std::memory_order_relaxed);
        if (dropped != droppedSeen)
        {
            inputSamplesConsumed += dropped - droppedSeen;   // keep MIDI timestamps aligned
            droppedSeen = dropped;
        }

        float chunk[512];
        bool anyHop = false;

        for (;;)
        {
            const size_t n = audioFifo.pop (chunk, 512);
            if (n == 0) break;

            for (size_t i = 0; i < n; ++i)
            {
                double x = (double) chunk[i] + 1.0e-18;   // tiny DC stops denormals in the filters
                if (decimation > 1)
                    for (auto& bq : antiAlias)
                        x = bq.process (x);

                ++inputSamplesConsumed;

                if (++decimationPhase >= decimation)
                {
                    decimationPhase = 0;
                    ring[(size_t) (decimatedCount & ringMask)] = (float) x;
                    ++decimatedCount;

                    if (++samplesSinceHop >= hopSize)
                    {
                        samplesSinceHop = 0;
                        processHop();
                        anyHop = true;
                    }
                }
            }
        }

        if (! anyHop && stateChanged)
            publishSnapshot();   // e.g. "clear history" pressed while the transport is stopped
    }

    //==============================================================================
    void KeyEngine::consumeMidiUntil (int64_t endTime, int64_t startTime)
    {
        midiHeldTime.fill (0.0);
        midiOnsets.fill (0);
        if (midiCursor < startTime) midiCursor = startTime;

        auto accumulateTo = [this] (int64_t upTo)
        {
            if (upTo <= midiCursor) return;
            const double dt = (double) (upTo - midiCursor) / inputRate;
            for (int n = 0; n < 128; ++n)
                if (midiHeld[(size_t) n] > 0.0f)
                    midiHeldTime[(size_t) n] += dt;
            midiCursor = upTo;
        };

        MidiEvent e;
        while (midiFifo.peek (e) && e.time <= endTime)
        {
            midiFifo.discardOne();
            accumulateTo (e.time);
            midiEverSeen = true;
            lastMidiActivity = std::max (lastMidiActivity, e.time);

            if (e.note == allNotesOffCode)
            {
                midiHeld.fill (0.0f);
                midiKeyDown.fill (false);
                sustainPedal = false;
            }
            else if (e.note == pedalDownCode)
            {
                sustainPedal = true;
            }
            else if (e.note == pedalUpCode)
            {
                sustainPedal = false;
                for (size_t n = 0; n < 128; ++n)
                    if (! midiKeyDown[n]) midiHeld[n] = 0.0f;       // release notes only the pedal was holding
            }
            else if (e.velocity > 0.0f)
            {
                midiHeld[(size_t) e.note] = e.velocity;
                midiKeyDown[(size_t) e.note] = true;
                ++midiOnsets[(size_t) (e.note % 12)];
                lastMidiNoteOn = e.note;
            }
            else
            {
                midiKeyDown[(size_t) e.note] = false;
                if (! sustainPedal)
                    midiHeld[(size_t) e.note] = 0.0f;
            }
        }
        accumulateTo (endTime);

        for (auto v : midiHeld)
            if (v > 0.0f) { lastMidiActivity = endTime; break; }
    }

    //==============================================================================
    void KeyEngine::processHop()
    {
        const Timing tm = currentTiming();
        const double hopSec = hopSize / analysisRate;
        const auto mode = (AnalysisMode) modeParam.load();

        const int64_t hopEnd = inputSamplesConsumed;
        const int64_t hopStart = hopEnd - (int64_t) hopSize * decimation;
        consumeMidiUntil (hopEnd, hopStart);

        // Input level over the last two hops
        {
            const int len = std::min<int> (hopSize * 2, (int) std::min<uint64_t> (decimatedCount, (uint64_t) ring.size()));
            double acc = 0.0;
            for (int i = 1; i <= len; ++i)
            {
                const double v = ring[(size_t) ((decimatedCount - (uint64_t) i) & ringMask)];
                acc += v * v;
            }
            inputLevelDb = len > 0 ? (float) (10.0 * std::log10 (acc / len + 1.0e-20)) : -100.0f;
        }
        const bool audioActive = inputLevelDb > silenceGateDb;

        bool anyHeld = false;
        for (auto v : midiHeld) if (v > 0.0f) { anyHeld = true; break; }
        int onsets = 0;
        for (auto o : midiOnsets) onsets += o;

        const bool midiRecent = lastMidiActivity >= 0 && (double) (hopEnd - lastMidiActivity) < 2.0 * inputRate;
        sourceIsMidi = midiEverSeen && (anyHeld || onsets > 0 || midiRecent);

        float chroma[12] = {}, bass[12] = {};
        double weight = 0.0;
        bool haveChroma = false;
        int bassPc = -1;

        if (sourceIsMidi)
        {
            int lowest = 128;
            for (int n = 0; n < 128; ++n)
            {
                const double f = midiHeldTime[(size_t) n] / hopSec;
                if (f > 0.0) chroma[n % 12] += (float) std::min (1.0, f);
                if (midiHeld[(size_t) n] > 0.0f && n < lowest) lowest = n;
            }
            for (int pc = 0; pc < 12; ++pc)
                chroma[pc] += 0.5f * (float) midiOnsets[(size_t) pc];

            float s = 0; for (auto c : chroma) s += c;
            haveChroma = s > 0.0f;
            if (lowest < 128) { bass[lowest % 12] = 1.0f; bassPc = lowest % 12; }
            weight = haveChroma ? 1.0 : 0.0;
            signalPresent = anyHeld || onsets > 0;

            if (mode == AnalysisMode::Mono)
            {
                int note = -1;
                if (lastMidiNoteOn >= 0 && midiHeld[(size_t) lastMidiNoteOn] > 0.0f) note = lastMidiNoteOn;
                else for (int n = 127; n >= 0; --n) if (midiHeld[(size_t) n] > 0.0f) { note = n; break; }

                if (note >= 0)
                {
                    const float a4 = a4Param.load();
                    monoFreq = (float) (a4 * std::pow (2.0, (note - 69) / 12.0));
                    if (note != monoNote)
                    {
                        monoNote = note;
                        if (note != lastHistoryNote)
                        {
                            HistoryEntry h; h.kind = HistoryEntry::Kind::Note; h.midiNote = note;
                            pushHistory (h);
                            lastHistoryNote = note;
                        }
                    }
                    monoCents = 0.0f;
                    monoVoiced = true;
                }
                else
                {
                    monoVoiced = false;
                }
            }
        }
        else if (mode == AnalysisMode::Mono)
        {
            float f = 0.0f, clarity = 0.0f;
            const bool voiced = audioActive && runYin (f, clarity);
            updateMonoNote (voiced, f, clarity, hopSec, tm);

            if (voiced && monoNote >= 0)
            {
                chroma[monoNote % 12] = 1.0f;
                haveChroma = true;
                weight = clarity;
            }
            signalPresent = monoVoiced;
        }
        else
        {
            float tonal = 0.0f;
            if (audioActive)
            {
                computeSpectrum();
                computeAudioChroma (chroma, bass, tonal);
            }

            float s = 0; for (auto c : chroma) s += c;
            haveChroma = s > 0.0f;

            weight = mode == AnalysisMode::Song ? clamp01 ((tonal - 0.25) / 0.40)
                                                : clamp01 ((tonal - 0.12) / 0.38);
            signalPresent = audioActive && haveChroma && tonal > 0.08f;

            float bs = 0, bmax = 0; int bmaxPc = -1;
            for (int pc = 0; pc < 12; ++pc) { bs += bass[pc]; if (bass[pc] > bmax) { bmax = bass[pc]; bmaxPc = pc; } }
            if (bmaxPc >= 0 && bmax > 0.55f * bs && bs > 0.12f * s)
                bassPc = bmaxPc;
        }

        // ---- key evidence ----
        if (haveChroma && weight > 0.0)
            accumulateKeyEvidence (chroma, bass, weight, hopSec, tm);

        // ---- chords & live notes ----
        if (mode != AnalysisMode::Mono)
        {
            if (haveChroma && signalPresent)
            {
                float mx = 0; for (auto c : chroma) mx = std::max (mx, c);
                const float aChord = sourceIsMidi ? 0.0f : (float) std::exp (-hopSec / tm.chordTau);
                const float aDisp  = sourceIsMidi ? 0.0f : (float) std::exp (-hopSec / 0.07);
                for (int pc = 0; pc < 12; ++pc)
                {
                    const float v = mx > 0 ? chroma[pc] / mx : 0.0f;
                    shortChroma[(size_t) pc]   = aChord * shortChroma[(size_t) pc]   + (1.0f - aChord) * v;
                    displayChroma[(size_t) pc] = aDisp  * displayChroma[(size_t) pc] + (1.0f - aDisp)  * v;
                }

                float score = 0.0f;
                const ChordId cand = matchChord (shortChroma.data(), bassPc, currentChord, score);
                const bool ok = cand.isValid() && score >= (sourceIsMidi ? 0.5f : 0.62f);
                updateChord (cand, ok, hopSec, tm);

                float dmx = 0; for (auto c : displayChroma) dmx = std::max (dmx, c);
                for (int pc = 0; pc < 12; ++pc)
                {
                    const float v = dmx > 0 ? displayChroma[(size_t) pc] / dmx : 0.0f;
                    noteLevels[(size_t) pc] = sourceIsMidi ? (chroma[pc] > 0.0f ? 1.0f : 0.0f)
                                                           : clamp01 ((v - 0.30f) / 0.60f);
                }
            }
            else
            {
                updateChord ({}, false, hopSec, tm);
                noteLevels.fill (0.0f);
                for (auto& c : displayChroma) c *= 0.8f;
            }

            if (currentChord.isValid() && signalPresent && ! currentChord.isSingleNote)
            {
                const int tc = triadClassIndex (currentChord);
                if (tc >= 0) chordHistogram[(size_t) tc] += hopSec * std::max (0.3, weight);
            }
        }
        else
        {
            noteLevels.fill (0.0f);
            if (monoVoiced && monoNote >= 0)
                noteLevels[(size_t) (monoNote % 12)] = 1.0f;
            currentChord = {};
        }

        updateKeyDecision (hopSec, tm);
        publishSnapshot();
    }

    //==============================================================================
    void KeyEngine::fft (std::vector<std::complex<float>>& a) const
    {
        const int n = fftSize;
        for (int i = 0; i < n; ++i)
        {
            const int j = bitReverse[(size_t) i];
            if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const auto w = twiddles[(size_t) (j * step)];
                    auto& u = a[(size_t) (i + j)];
                    auto& v = a[(size_t) (i + j + half)];
                    const float vr = v.real() * w.real() - v.imag() * w.imag();
                    const float vi = v.real() * w.imag() + v.imag() * w.real();
                    v = { u.real() - vr, u.imag() - vi };
                    u = { u.real() + vr, u.imag() + vi };
                }
            }
        }
    }

    void KeyEngine::computeSpectrum()
    {
        const uint64_t n = (uint64_t) fftSize;
        for (uint64_t i = 0; i < n; ++i)
        {
            float x = 0.0f;
            if (decimatedCount >= n - i)
                x = ring[(size_t) ((decimatedCount - n + i) & ringMask)];
            fftData[(size_t) i] = { x * window[(size_t) i], 0.0f };
        }

        fft (fftData);

        for (int k = 0; k <= fftSize / 2; ++k)
            fftMag[(size_t) k] = std::abs (fftData[(size_t) k]);
    }

    void KeyEngine::computeAudioChroma (float* chroma, float* bassChroma, float& tonalness)
    {
        const double a4 = a4Param.load();
        const double binHz = analysisRate / fftSize;
        const int half = fftSize / 2;
        const int kLo = std::max (3, (int) std::ceil (38.0 / binHz));
        const int kHi = std::min (half - 3, (int) std::floor (std::min (3800.0, 0.36 * analysisRate) / binHz));

        envPrefix[0] = 0.0f;
        for (int k = 0; k <= half; ++k)
            envPrefix[(size_t) k + 1] = envPrefix[(size_t) k] + fftMag[(size_t) k];

        float maxMag = 0.0f;
        for (int k = kLo; k <= kHi; ++k) maxMag = std::max (maxMag, fftMag[(size_t) k]);

        tonalness = 0.0f;
        if (maxMag <= 1.0e-9f) return;

        const float absFloor = 0.12f * (float) fftSize / 4096.0f;   // ~ -75 dBFS sine
        double peakEnergy = 0.0, totalEnergy = 0.0;
        peaks.clear();

        for (int k = kLo; k <= kHi; ++k)
        {
            const float m = fftMag[(size_t) k];
            totalEnergy += (double) m * m;

            if (m <= fftMag[(size_t) k - 1] || m < fftMag[(size_t) k + 1]
                || m < fftMag[(size_t) k - 2] || m < fftMag[(size_t) k + 2])
                continue;
            if (m < maxMag * 0.0045f || m < absFloor)
                continue;

            const int w = std::max (3, (int) std::lround (k * 0.06));
            const int a = std::max (0, k - w), b = std::min (half, k + w);
            const float env = (envPrefix[(size_t) b + 1] - envPrefix[(size_t) a]) / (float) (b - a + 1);
            if (m < 1.7f * env)
                continue;

            const double l = std::log (fftMag[(size_t) k - 1] + 1.0e-12);
            const double c = std::log (m + 1.0e-12);
            const double r = std::log (fftMag[(size_t) k + 1] + 1.0e-12);
            const double den = l - 2.0 * c + r;
            double d = den < 0.0 ? 0.5 * (l - r) / den : 0.0;
            d = std::max (-0.5, std::min (0.5, d));

            peaks.push_back ({ (k + d) * binHz, std::exp (c - 0.25 * (l - r) * d), k });
            peakEnergy += (double) fftMag[(size_t) k - 1] * fftMag[(size_t) k - 1] + (double) m * m
                        + (double) fftMag[(size_t) k + 1] * fftMag[(size_t) k + 1];
        }

        tonalness = totalEnergy > 0.0 ? (float) (peakEnergy / totalEnergy) : 0.0f;
        if (peaks.empty()) return;

        // Harmonic suppression: partials 3, 5, 6, 7 of a lower peak would otherwise
        // add a false fifth / third / seventh to the chroma.
        static const int harm[4]     = { 3, 5, 6, 7 };
        static const double gain[4]  = { 0.50, 0.35, 0.30, 0.25 };
        const size_t np = peaks.size();
        for (size_t i = 0; i < np; ++i)
        {
            if (peaks[i].freq > 1400.0) break;
            for (int h = 0; h < 4; ++h)
            {
                const double target = peaks[i].freq * harm[h];
                size_t bestJ = np; double bestC = 40.0;
                for (size_t j = i + 1; j < np; ++j)
                {
                    const double cents = 1200.0 * std::log2 (peaks[j].freq / target);
                    if (cents > 40.0) break;
                    if (std::abs (cents) < bestC) { bestC = std::abs (cents); bestJ = j; }
                }
                if (bestJ < np)
                    peaks[bestJ].amp = std::max (peaks[bestJ].amp - gain[h] * peaks[i].amp, 0.15 * peaks[bestJ].amp);
            }
        }

        double maxAmp = 0.0;
        for (const auto& p : peaks) maxAmp = std::max (maxAmp, p.amp);

        for (const auto& p : peaks)
        {
            const double midi = 69.0 + 12.0 * std::log2 (p.freq / a4);
            const long n = std::lround (midi);
            const double dev = midi - (double) n;
            const double wd = std::cos (pi * dev) * std::cos (pi * dev);
            const double a = std::sqrt (p.amp / maxAmp);
            const double z = (midi - 60.0) / 22.0;
            const double wf = std::exp (-0.5 * z * z);
            const int pc = wrap12 ((int) n);
            chroma[pc] += (float) (a * wd * wf);
            if (p.freq < 190.0)
                bassChroma[pc] += (float) (a * wd);
        }
    }

    //==============================================================================
    bool KeyEngine::runYin (float& frequency, float& clarity)
    {
        const int W = yinWindow, tMax = yinTauMax, tMin = yinTauMin;
        const int len = W + tMax;
        if (decimatedCount < (uint64_t) len) return false;

        float* x = yinBuffer.data();
        double energy = 0.0;
        for (int i = 0; i < len; ++i)
        {
            x[i] = ring[(size_t) ((decimatedCount - (uint64_t) len + (uint64_t) i) & ringMask)];
            if (i >= tMax) energy += (double) x[i] * x[i];
        }
        if (10.0 * std::log10 (energy / W + 1.0e-20) < -55.0) return false;

        float* d = yinDiff.data();
        float* raw = yinRaw.data();
        d[0] = 1.0f;
        raw[0] = 0.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tMax; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const float diff = x[j] - x[j + tau];
                s += (double) diff * diff;
            }
            running += s;
            raw[tau] = (float) s;
            d[tau] = running > 0.0 ? (float) (s * tau / running) : 1.0f;
        }

        int tauEst = -1;
        for (int tau = tMin; tau < tMax; ++tau)
        {
            if (d[tau] < 0.15f)
            {
                while (tau + 1 < tMax && d[tau + 1] < d[tau]) ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 0)
        {
            float best = 1.0f;
            for (int tau = tMin; tau < tMax; ++tau)
                if (d[tau] < best) { best = d[tau]; tauEst = tau; }
            if (best > 0.30f) return false;
        }

        // Interpolate on the raw difference function (less biased than on the normalised one)
        double betterTau = tauEst;
        if (tauEst > 1 && tauEst < tMax)
        {
            const double s0 = raw[tauEst - 1], s1 = raw[tauEst], s2 = raw[tauEst + 1];
            const double den = s0 - 2.0 * s1 + s2;
            if (den > 0.0) betterTau = tauEst + std::max (-1.0, std::min (1.0, 0.5 * (s0 - s2) / den));
        }

        frequency = (float) (analysisRate / betterTau);
        clarity = 1.0f - d[tauEst];
        return clarity > 0.72f && frequency > 30.0f && frequency < 2100.0f;
    }

    void KeyEngine::updateMonoNote (bool voiced, float frequency, float clarity, double hopSec, const Timing& tm)
    {
        (void) clarity;
        if (! voiced)
        {
            monoSilence += hopSec;
            if (monoSilence > 0.12) { monoVoiced = false; recentPitchCount = 0; monoCandidate = -1; }
            return;
        }

        monoSilence = 0.0;
        const double a4 = a4Param.load();
        const float midi = (float) (69.0 + 12.0 * std::log2 (frequency / a4));

        // median of the last few estimates removes single-frame octave slips
        for (size_t i = recentPitches.size() - 1; i > 0; --i) recentPitches[i] = recentPitches[i - 1];
        recentPitches[0] = midi;
        recentPitchCount = std::min (recentPitchCount + 1, (int) recentPitches.size());
        std::array<float, 5> tmp {};
        std::copy (recentPitches.begin(), recentPitches.begin() + recentPitchCount, tmp.begin());
        for (int i = 1; i < recentPitchCount; ++i)          // tiny insertion sort
            for (int j = i; j > 0 && tmp[(size_t) j - 1] > tmp[(size_t) j]; --j)
                std::swap (tmp[(size_t) j - 1], tmp[(size_t) j]);
        const float med = tmp[(size_t) recentPitchCount / 2];

        const int nearest = (int) std::lround (med);
        const int target = (monoNote >= 0 && std::abs (med - (float) monoNote) < 0.62f) ? monoNote : nearest;

        if (target != monoNote)
        {
            if (target == monoCandidate) monoCandidateTime += hopSec;
            else { monoCandidate = target; monoCandidateTime = hopSec; }

            if (monoCandidateTime >= tm.noteCommit || monoNote < 0)
            {
                monoNote = target;
                monoCandidate = -1;
                monoCents = (med - (float) monoNote) * 100.0f;
                if (monoNote != lastHistoryNote)
                {
                    HistoryEntry h; h.kind = HistoryEntry::Kind::Note; h.midiNote = monoNote;
                    pushHistory (h);
                    lastHistoryNote = monoNote;
                }
            }
        }
        else
        {
            monoCandidate = -1;
        }

        if (monoNote >= 0)
        {
            const float cents = (midi - (float) monoNote) * 100.0f;
            const float alpha = (float) (1.0 - std::exp (-hopSec / 0.06));
            if (std::abs (cents) < 70.0f)
                monoCents += alpha * (cents - monoCents);
        }
        monoFreq = frequency;
        monoVoiced = monoNote >= 0;
    }

    //==============================================================================
    void KeyEngine::accumulateKeyEvidence (const float* chroma, const float* bassChroma, double weight,
                                           double hopSec, const Timing& tm)
    {
        double s = 0.0, sb = 0.0;
        for (int i = 0; i < 12; ++i) { s += chroma[i]; sb += bassChroma[i]; }
        if (s <= 0.0 || weight <= 0.0) return;

        const double dt = weight * hopSec;
        const double lambda = std::exp (-dt / tm.keyTau);

        for (int pc = 0; pc < 12; ++pc)
        {
            keyChroma[(size_t) pc] = keyChroma[(size_t) pc] * lambda + dt * chroma[pc] / s;
            keyBass[(size_t) pc]   = keyBass[(size_t) pc] * lambda + (sb > 0.0 ? dt * bassChroma[pc] / sb : 0.0);
        }
        for (auto& c : chordHistogram) c *= lambda;
        evidence = evidence * lambda + dt;
    }

    void KeyEngine::updateKeyDecision (double hopSec, const Timing& tm)
    {
        const bool locked = lockParam.load();

        double s = 0.0;
        for (auto v : keyChroma) s += v;
        if (s <= 1.0e-9)
        {
            confidence = keyRestored ? restoredConfidence : 0.0f;
            showRunnerUp = false;
            return;
        }

        // Pearson correlation of the pitch-class histogram with every rotated key profile
        const double mean = s / 12.0;
        double var = 0.0;
        for (auto v : keyChroma) var += (v - mean) * (v - mean);
        const double sdK = std::sqrt (var) + 1.0e-12;

        const auto& maj = majorProfile();
        const auto& mnr = minorProfile();
        auto profileStats = [] (const std::array<float, 12>& p, double& m, double& sd)
        {
            m = 0; for (auto v : p) m += v; m /= 12.0;
            sd = 0; for (auto v : p) sd += (v - m) * (v - m); sd = std::sqrt (sd);
        };
        double mMaj, sdMaj, mMin, sdMin;
        profileStats (maj, mMaj, sdMaj);
        profileStats (mnr, mMin, sdMin);

        double bs = 0.0, cs = 0.0;
        for (auto v : keyBass) bs += v;
        for (auto v : chordHistogram) cs += v;

        std::array<double, 24> corr {};
        for (int k = 0; k < 24; ++k)
        {
            const int tonic = k % 12;
            const bool minor = k >= 12;
            const auto& p = minor ? mnr : maj;
            const double pm = minor ? mMin : mMaj, psd = minor ? sdMin : sdMaj;
            double acc = 0.0;
            for (int i = 0; i < 12; ++i)
                acc += (keyChroma[(size_t) ((tonic + i) % 12)] - mean) * (p[(size_t) i] - pm);
            corr[(size_t) k] = acc / (sdK * psd);

            // Diatonic fit: share of the pitch-class energy that belongs to the key's scale.
            double fit = 0.0;
            for (int i = 0; i < 12; ++i)
                if (isInKey (KeyId { tonic, minor }, (tonic + i) % 12))
                    fit += keyChroma[(size_t) ((tonic + i) % 12)];
            fit /= s;

            // Tonic emphasis: how often the tonic is in the bass and the tonic chord is playing.
            double bonus = 0.0;
            if (bs > 0.0) bonus += bassWeight * keyBass[(size_t) tonic] / bs;
            if (cs > 0.0) bonus += chordWeight * chordHistogram[(size_t) k] / cs;
            keyScore[(size_t) k] = corr[(size_t) k] + fitWeight * fit + bonus;
        }

        // Smooth the scores over a few seconds so a looping progression doesn't make the
        // result swing with whichever chord happens to be playing right now.
        {
            const double a = 1.0 - std::exp (-hopSec / (2.0 * tm.keyHold));
            if (! smoothedValid) { smoothedScore = keyScore; smoothedValid = true; }
            else for (size_t k = 0; k < 24; ++k) smoothedScore[k] += a * (keyScore[k] - smoothedScore[k]);
            keyScore = smoothedScore;
        }

        double mx = -1.0e9;
        for (auto v : keyScore) mx = std::max (mx, v);
        double z = 0.0;
        for (int k = 0; k < 24; ++k) { keyProb[(size_t) k] = std::exp ((keyScore[(size_t) k] - mx) / softmaxTemperature); z += keyProb[(size_t) k]; }
        for (auto& p : keyProb) p /= z;

        int best = 0, second = -1;
        for (int k = 1; k < 24; ++k) if (keyScore[(size_t) k] > keyScore[(size_t) best]) best = k;
        for (int k = 0; k < 24; ++k)
            if (k != best && (second < 0 || keyScore[(size_t) k] > keyScore[(size_t) second])) second = k;

        // ---- decide the displayed key (with hysteresis) ----
        if (! currentKey.isValid())
        {
            if (evidence >= tm.evidenceMin)
                currentKey = KeyId::fromIndex (best);
        }
        else if (! locked)
        {
            const int cur = currentKey.index();
            if (best != cur && keyScore[(size_t) best] - keyScore[(size_t) cur] > 0.015)
            {
                if (challengerKey.isValid() && challengerKey.index() == best) challengerTime += hopSec;
                else { challengerKey = KeyId::fromIndex (best); challengerTime = hopSec; }

                if (challengerTime >= tm.keyHold)
                {
                    currentKey = challengerKey;
                    challengerKey = {};
                    challengerTime = 0.0;
                    keyRestored = false;
                    HistoryEntry h; h.kind = HistoryEntry::Kind::KeyChange; h.key = currentKey;
                    pushHistory (h);
                }
            }
            else
            {
                challengerTime = std::max (0.0, challengerTime - hopSec);
            }
        }

        if (! currentKey.isValid())
        {
            confidence = 0.0f;
            showRunnerUp = false;
            return;
        }

        const int cur = currentKey.index();
        const double p = keyProb[(size_t) cur];
        const double evidenceFactor = 1.0 - std::exp (-2.0 * evidence / tm.evidenceFull);
        const double quality = std::max (0.25, std::min (1.0, (corr[(size_t) cur] - 0.20) / 0.45));
        confidence = (float) clamp01 (p * evidenceFactor * quality);

        if (keyRestored && evidence < tm.evidenceMin)
            confidence = std::max (confidence, restoredConfidence);

#ifdef KC_DEBUG
        {
            static int dbg = 0;
            if (++dbg % 86 == 0)
            {
                std::printf ("   chroma:");
                for (int i = 0; i < 12; ++i) std::printf (" %s=%.2f", pitchClassName (i, false).c_str(), keyChroma[(size_t) i] / s);
                std::printf ("\n   bass:");
                for (int i = 0; i < 12; ++i) if (bs > 0 && keyBass[(size_t) i] / bs > 0.05) std::printf (" %s=%.2f", pitchClassName (i, false).c_str(), keyBass[(size_t) i] / bs);
                std::printf ("\n   top:");
                std::array<int, 24> idx {}; for (int i = 0; i < 24; ++i) idx[(size_t) i] = i;
                std::sort (idx.begin(), idx.end(), [this] (int a, int b) { return keyScore[(size_t) a] > keyScore[(size_t) b]; });
                for (int i = 0; i < 4; ++i) std::printf (" %s r=%.3f S=%.3f p=%.2f |", keyShortName (KeyId::fromIndex (idx[(size_t) i]), false).c_str(), corr[(size_t) idx[(size_t) i]], keyScore[(size_t) idx[(size_t) i]], keyProb[(size_t) idx[(size_t) i]]);
                std::printf ("\n");
            }
        }
#endif
        const int runner = best != cur ? best : second;
        runnerUpKey = KeyId::fromIndex (runner);
        const bool isRelative = runner >= 0 && ((currentKey.minor && runner == (currentKey.tonic + 3) % 12)
                                                || (! currentKey.minor && runner == 12 + (currentKey.tonic + 9) % 12));
        showRunnerUp = runner >= 0 && keyProb[(size_t) runner] >= (isRelative ? 0.10 : 0.25) * p;
    }

    //==============================================================================
    ChordId KeyEngine::matchChord (const float* x, int bassPc, const ChordId& current, float& bestScore) const
    {
        bestScore = 0.0f;
        double norm = 0.0;
        for (int i = 0; i < 12; ++i) norm += (double) x[i] * x[i];
        if (norm <= 1.0e-12) return {};
        norm = std::sqrt (norm);

        double e[12];
        int maxPc = 0;
        for (int i = 0; i < 12; ++i) { e[i] = x[i] / norm; if (e[i] > e[maxPc]) maxPc = i; }

        const bool currentIsNote = current.isValid() && current.isSingleNote;
        if (e[maxPc] * e[maxPc] > (currentIsNote && current.root == maxPc ? 0.62 : 0.70))
        {
            ChordId c; c.root = maxPc; c.isSingleNote = true;
            bestScore = (float) (e[maxPc] * e[maxPc]);
            return c;
        }

        ChordId best;
        double bestS = -1.0e9;
        for (const auto& t : chordTemplates())
        {
            const double invSqrt = 1.0 / std::sqrt ((double) t.numNotes);
            for (int r = 0; r < 12; ++r)
            {
                double s = 0.0;
                for (int i = 0; i < t.numNotes; ++i) s += e[(r + t.intervals[i]) % 12];
                s = s * invSqrt - t.penalty;
                if (bassPc == r) s += 0.03;
                if (current.isValid() && ! current.isSingleNote && current.root == r && current.type == t.type)
                    s += 0.04;   // stickiness: avoids flip-flopping between two similar chords
                if (s > bestS) { bestS = s; best.root = r; best.type = t.type; }
            }
        }

        best.bass = -1;
        if (bassPc >= 0 && bassPc != best.root && chordContains (best, bassPc))
            best.bass = bassPc;

        bestScore = (float) bestS;
        return best;
    }

    void KeyEngine::updateChord (const ChordId& cand, bool have, double hopSec, const Timing& tm)
    {
        if (! have)
        {
            // Keep showing the last chord when the music stops (the UI dims it),
            // so you can hit stop and still read it.
            silenceTime += hopSec;
            if (silenceTime > 0.6)
            {
                pendingChord = {};
                pendingTime = 0.0;
            }
            return;
        }

        silenceTime = 0.0;
        if (cand.sameHarmony (pendingChord)) pendingTime += hopSec;
        else { pendingChord = cand; pendingTime = hopSec; }
        pendingChord.bass = cand.bass;

        double need = sourceIsMidi ? std::min (tm.chordCommit, 0.06) : tm.chordCommit;
        if (pendingChord.isSingleNote) need *= 1.5;

        if (pendingTime >= need)
        {
            if (! pendingChord.sameHarmony (currentChord))
            {
                currentChord = pendingChord;
                const double now = (double) decimatedCount / analysisRate;
                if (! currentChord.isSingleNote)
                    lastRealChordTime = now;

                // Single notes only go into the chord history when the part is monophonic
                // (e.g. a bass line); otherwise they are just chord tails and clutter it.
                const bool logIt = ! currentChord.isSingleNote || now - lastRealChordTime > 4.0;

                if (logIt && ! currentChord.sameHarmony (lastCommittedChord))
                {
                    HistoryEntry h; h.kind = HistoryEntry::Kind::Chord; h.chord = currentChord;
                    pushHistory (h);
                    lastCommittedChord = currentChord;
                }
            }
            else
            {
                currentChord.bass = pendingChord.bass;
            }
        }
    }

    void KeyEngine::pushHistory (const HistoryEntry& e)
    {
        if (historySize == (int) history.size())
        {
            std::move (history.begin() + 1, history.end(), history.begin());
            --historySize;
        }
        history[(size_t) historySize++] = e;
    }

    //==============================================================================
    void KeyEngine::publishSnapshot()
    {
        EngineSnapshot s;
        s.serial = ++serial;
        s.mode = (AnalysisMode) modeParam.load();
        s.signalPresent = signalPresent;
        s.sourceIsMidi = sourceIsMidi;
        s.inputLevelDb = inputLevelDb;

        s.hasKey = currentKey.isValid();
        s.key = currentKey;
        s.confidence = confidence;
        s.locked = lockParam.load();
        s.runnerUp = runnerUpKey;
        s.showRunnerUp = showRunnerUp && currentKey.isValid() && ! s.locked;
        s.evidenceSeconds = (float) evidence;

        s.noteLevels = noteLevels;
        s.chord = currentChord;

        s.monoVoiced = monoVoiced;
        s.monoNote = monoNote;
        s.monoCents = monoCents;
        s.monoFrequency = monoFreq;

        s.history = history;
        s.historySize = historySize;

        std::lock_guard<std::mutex> lock (snapshotLock);
        snapshot = s;
    }

    EngineSnapshot KeyEngine::getSnapshot() const
    {
        std::lock_guard<std::mutex> lock (snapshotLock);
        return snapshot;
    }
}
