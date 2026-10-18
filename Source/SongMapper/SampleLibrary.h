// Song Mapper - recorded instrument samples for the audition sound (acoustic guitar, electric bass).
// One shared library per process: decoded from the plug-in's binary data on a background thread.
// The audio thread only reads it after isReady() is true; nothing changes after that.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <thread>
#include <vector>

namespace sm
{
    struct SampleZone
    {
        int rootNote = 60;
        double rootRatio = 1.0;              // playback-speed factor that corrects the recording's tuning
        double sampleRate = 44100.0;
        std::vector<juce::int16> data;       // mono
    };

    class SampleLibrary
    {
    public:
        enum Set : int { Guitar = 0, Bass = 1, NumSets };

        SampleLibrary();
        ~SampleLibrary();

        bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
        bool waitUntilReady (int milliseconds) const;     // for tests

        // the recording closest to this note (nullptr when not loaded)
        const SampleZone* zoneFor (Set set, int note) const noexcept;

    private:
        void load();

        std::array<std::vector<SampleZone>, NumSets> zones;
        std::array<std::array<int, 128>, NumSets> nearest {};
        std::atomic<bool> ready { false }, cancel { false };
        std::thread loader;

        JUCE_DECLARE_NON_COPYABLE (SampleLibrary)
    };
}
