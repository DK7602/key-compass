// Key Compass - the built-in manual: a PDF inside the plug-in, opened in the default PDF viewer / browser
#pragma once

#include <juce_core/juce_core.h>

namespace kcui
{
    juce::File writeManualFile();   // writes (or refreshes) the manual next to the favourites; returns the file
    bool openManual();              // writes it and opens it (default PDF viewer, usually the web browser)
    juce::String versionText();     // "Key Compass 1.3.0 - built Oct 10 2026", so you can see which build is loaded

    // The Android app shows the manual itself (a PDF can't be handed to another app as a plain file there)
    std::function<bool (const juce::File&)>& manualOpener();
}
