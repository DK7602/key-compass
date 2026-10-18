// Song Mapper - recorded instrument samples (see SampleLibrary.h)
// The recordings are from tonejs-instruments by Nicholaus P. Brosowsky (CC BY 3.0): acoustic guitar from the
// University of Iowa Electronic Music Studios, electric bass from Karoryfer Samples. Trimmed and re-encoded.
#include "SampleLibrary.h"
#include "BinaryData.h"
#include <chrono>

namespace sm
{
    SampleLibrary::SampleLibrary()
    {
        for (auto& map : nearest) map.fill (-1);
        loader = std::thread ([this] { load(); });
    }

    SampleLibrary::~SampleLibrary()
    {
        cancel.store (true);
        if (loader.joinable()) loader.join();
    }

    bool SampleLibrary::waitUntilReady (int milliseconds) const
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds (milliseconds);
        while (! isReady() && std::chrono::steady_clock::now() < end)
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        return isReady();
    }

    const SampleZone* SampleLibrary::zoneFor (Set set, int note) const noexcept
    {
        if (! isReady() || set < 0 || set >= NumSets) return nullptr;
        const int i = nearest[(size_t) set][(size_t) juce::jlimit (0, 127, note)];
        return i >= 0 ? &zones[(size_t) set][(size_t) i] : nullptr;
    }

    void SampleLibrary::load()
    {
        // samples.txt: "<set> <midi note> <tuning error in cents> <seconds>" per recording
        int listSize = 0;
        const char* list = BinaryData::getNamedResource ("samples_txt", listSize);
        if (list == nullptr) return;

        juce::OggVorbisAudioFormat ogg;
        juce::StringArray lines;
        lines.addLines (juce::String::fromUTF8 (list, listSize));
        for (const auto& line : lines)
        {
            if (cancel.load()) return;
            auto tokens = juce::StringArray::fromTokens (line, " ", "");
            if (tokens.size() < 3) continue;
            const Set set = tokens[0] == "bass" ? Bass : (tokens[0] == "gtr" ? Guitar : NumSets);
            if (set == NumSets) continue;
            const int note = tokens[1].getIntValue();
            const double cents = tokens[2].getDoubleValue();

            int size = 0;
            const auto resource = tokens[0] + "_" + juce::String (note) + "_ogg";
            const char* bytes = BinaryData::getNamedResource (resource.toRawUTF8(), size);
            if (bytes == nullptr || size <= 0) continue;

            std::unique_ptr<juce::AudioFormatReader> reader (
                ogg.createReaderFor (new juce::MemoryInputStream (bytes, (size_t) size, false), true));
            if (reader == nullptr || reader->lengthInSamples <= 0) continue;

            const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 8.0));
            juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
            reader->read (&buffer, 0, length, 0, true, true);

            SampleZone zone;
            zone.rootNote = note;
            zone.sampleRate = reader->sampleRate;
            zone.rootRatio = std::pow (2.0, -cents / 1200.0);       // a sharp recording plays a touch slower
            zone.data.resize ((size_t) length);
            const float* src = buffer.getReadPointer (0);
            for (int i = 0; i < length; ++i)
                zone.data[(size_t) i] = (juce::int16) juce::jlimit (-32767, 32767, (int) std::lround (src[i] * 32767.0f));
            zones[(size_t) set].push_back (std::move (zone));
        }

        for (int s = 0; s < NumSets; ++s)
        {
            auto& z = zones[(size_t) s];
            for (int n = 0; n < 128; ++n)
            {
                int best = -1, bestDist = 1000;
                for (int i = 0; i < (int) z.size(); ++i)
                {
                    // prefer shifting a recording down rather than up when two are equally close (keeps the attack natural)
                    const int d = std::abs (z[(size_t) i].rootNote - n) * 2 + (z[(size_t) i].rootNote < n ? 1 : 0);
                    if (d < bestDist) { bestDist = d; best = i; }
                }
                nearest[(size_t) s][(size_t) n] = best;
            }
        }
        ready.store (true, std::memory_order_release);
    }
}
