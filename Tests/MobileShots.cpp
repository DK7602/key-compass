// Key Compass - draws the Android app's screens at phone and tablet sizes into PNG files (developer test, Linux).
//   KeyCompassMobileShots <output folder> [filter]
// Each shot sets up the song, picks a tab, sometimes taps something, then saves what the screen shows.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Mobile/MobileEditor.h"

using namespace juce;

namespace
{
    // a few seconds of F# minor (F#m - D - A - E) into the detector, so the compass has something to show
    void feedTestAudio (KeyCompassProcessor& proc)
    {
        const double sr = 48000.0;
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        const int chords[4][3] = { { 54, 57, 61 }, { 50, 54, 57 }, { 57, 61, 64 }, { 52, 56, 59 } };
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        double phase[3] = { 0, 0, 0 };
        const int blocksPerChord = (int) (sr * 0.9 / 512);
        for (int round = 0; round < 2; ++round)
            for (int c = 0; c < 4; ++c)
                for (int b = 0; b < blocksPerChord; ++b)
                {
                    buf.clear();
                    for (int k = 0; k < 3; ++k)
                    {
                        const double f = 440.0 * std::pow (2.0, (chords[c][k] - 69) / 12.0);
                        for (int i = 0; i < 512; ++i)
                        {
                            const float v = 0.12f * (float) (std::sin (phase[k]) + 0.4 * std::sin (2.0 * phase[k]) + 0.2 * std::sin (3.0 * phase[k]));
                            buf.addSample (0, i, v);
                            buf.addSample (1, i, v);
                            phase[k] += MathConstants<double>::twoPi * f / sr;
                        }
                    }
                    proc.processBlock (buf, midi);
                    if (b % 8 == 0) Thread::sleep (2);
                }
        Thread::sleep (300);
    }

    void pump (int ms)
    {
        MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    bool save (Component& c, const File& file)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        file.deleteFile();
        FileOutputStream out (file);
        return out.openedOk() && PNGImageFormat().writeImageToStream (img, out);
    }
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    const String filter = argc > 2 ? String (argv[2]) : String();
    outDir.createDirectory();
    kcui::touchUIFlag() = true;

    AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Standalone);
    KeyCompassProcessor proc;
    AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Undefined);
    feedTestAudio (proc);

    struct Shot { String name; int w, h; };
    const Shot shots[] = { { "phone_portrait", 412, 880 }, { "tablet_portrait", 800, 1280 } };

    int failures = 0;
    for (const auto& s : shots)
    {
        if (filter.isNotEmpty() && ! s.name.contains (filter)) continue;
        auto ed = std::make_unique<MobileEditor> (proc);
        ed->setSize (s.w, s.h);
        ed->tick (0.5f);
        pump (200);
        const auto f = outDir.getChildFile (s.name + ".png");
        if (! save (*ed, f)) { ++failures; std::cout << "FAILED " << f.getFullPathName() << std::endl; }
        else std::cout << "wrote " << f.getFullPathName() << std::endl;
    }
    return failures == 0 ? 0 : 1;
}
