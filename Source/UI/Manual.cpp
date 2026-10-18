// Key Compass - the built-in manual (see Manual.h)
#include "Manual.h"
#include "BinaryData.h"
#include "../Version.h"

namespace kcui
{
    juce::File writeManualFile()
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource ("KeyCompassManual_pdf", size);
        if (data == nullptr || size <= 0) return {};

        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("KeyCompass");
        dir.createDirectory();
        auto file = dir.getChildFile ("Key Compass Manual.pdf");

        // rewrite only when the bundled manual differs (a new version of the plug-in)
        juce::MemoryBlock current;
        if (! file.existsAsFile() || file.getSize() != (juce::int64) size || ! file.loadFileAsData (current)
            || current.getSize() != (size_t) size || std::memcmp (current.getData(), data, (size_t) size) != 0)
        {
            if (! file.replaceWithData (data, (size_t) size))
            {
                // fall back to the temp folder if app data is not writable
                file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Key Compass Manual.pdf");
                if (! file.replaceWithData (data, (size_t) size)) return {};
            }
        }
        return file;
    }

    std::function<bool (const juce::File&)>& manualOpener()
    {
        static std::function<bool (const juce::File&)> opener;
        return opener;
    }

    bool openManual()
    {
        const auto file = writeManualFile();
        if (file.existsAsFile() && manualOpener())
            return manualOpener() (file);
        if (! file.existsAsFile()) return false;
        if (file.startAsProcess()) return true;
        return juce::URL (file).launchInDefaultBrowser();
    }

    juce::String versionText()
    {
       #if defined (JucePlugin_VersionString)
        const juce::String version (JucePlugin_VersionString);
       #else
        const juce::String version (KC_VERSION_STRING);
       #endif
        return "Key Compass " + version + "  -  built " + juce::String (__DATE__).replace ("  ", " ");
    }
}
