// Key Compass - editor
#include "PluginEditor.h"
#include "UI/Manual.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
    constexpr float minScale = 0.36f, maxScale = 1.2f;
}

//==============================================================================
KeyCompassCanvas::KeyCompassCanvas (KeyCompassProcessor& p, kcui::Assets& a, const kcui::UiState& s)
    : staticLayer (a),
      dialLayer (a, s),
      lidLayer (a, s),
      mapper (p, a),
      processor (p),
      modeBox (a, *p.getState().getParameter (KeyCompassProcessor::modeId),
               { "Track", "Mono", "Song" },
               { "one instrument", "voice, bass or lead", "whole mix / master" }),
      responseBox (a, *p.getState().getParameter (KeyCompassProcessor::responseId),
                   { "Fast", "Balanced", "Steady" },
                   { "reacts quickly", "recommended", "waits for more music" }),
      tuningBox (a, *p.getState().getParameter (KeyCompassProcessor::tuningId)),
      lockButton (a, kcui::RoundToggle::Icon::Lock),
      flatsButton (a, kcui::RoundToggle::Icon::Flat),
      mapperButton (a, "SONG MAPPER", kcui::KButton::Look::Plaque),
      manualButton (a, "MANUAL", kcui::KButton::Look::Plaque),
      lockAttachment (p.getState(), KeyCompassProcessor::lockId, lockButton),
      flatsAttachment (p.getState(), KeyCompassProcessor::flatsId, flatsButton)
{
    setSize (kcui::Layout::designWidth, kcui::Layout::designHeight);

    staticLayer.setBounds (getLocalBounds());
    staticLayer.setBufferedToImage (true);
    addAndMakeVisible (staticLayer);
    addAndMakeVisible (dialLayer);
    addAndMakeVisible (lidLayer);

    // Song Mapper page sits above the compass but below the shared bottom controls
    mapper.setBounds (getLocalBounds());
    addChildComponent (mapper);
    mapper.onSwitchToCompass = [this] { if (onPageChosen) onPageChosen (0); };

    mapperButton.setBounds (20, 30, 250, 53);
    mapperButton.fontSize = 23.0f;
    mapperButton.setTooltip ("Open the Song Mapper: chord diagrams, progressions by genre and a song timeline you can drag into your DAW");
    mapperButton.onClick = [this] { if (onPageChosen) onPageChosen (1); };
    addAndMakeVisible (mapperButton);

    manualButton.setBounds (kcui::Layout::designWidth - 20 - 250, 30, 250, 53);
    manualButton.fontSize = 23.0f;
    manualButton.setTooltip ("Open the Key Compass manual (a PDF that opens in your PDF viewer or web browser)");
    manualButton.onClick = [] { kcui::openManual(); };
    addAndMakeVisible (manualButton);

    modeBox.setBounds (kcui::Layout::modeBox);
    responseBox.setBounds (kcui::Layout::responseBox);
    tuningBox.setBounds (kcui::Layout::tuningBox);
    lockButton.setBounds (kcui::Layout::lockButton);
    flatsButton.setBounds (kcui::Layout::flatsButton);
    clearButton.setBounds (kcui::Layout::clearButton);

    modeBox.setTooltip ("What are you listening to?\n"
                        "TRACK: one instrument - chords and key.\n"
                        "MONO: a single voice, bass or lead - note and tuning.\n"
                        "SONG: the whole mix (put it on the Stereo Out).");
    responseBox.setTooltip ("How quickly the key reacts. FAST follows changes, STEADY waits for more music before deciding.");
    tuningBox.setTooltip ("Reference pitch (A4). Click the arrows (Shift = 0.1 Hz), drag up/down or scroll. Double-click for 440 Hz.");
    lockButton.setTooltip ("Lock Key: keep the current key. Chords and notes keep updating.");
    flatsButton.setTooltip (juce::String (juce::CharPointer_UTF8 ("Show flats (B\xe2\x99\xad) instead of sharps (A\xe2\x99\xaf).")));
    clearButton.setTooltip ("Clear the chord history and start listening for the key again.");
    clearButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    clearButton.onClick = [this] { processor.getEngine().requestReset(); };

    for (juce::Component* c : { (juce::Component*) &modeBox, (juce::Component*) &responseBox, (juce::Component*) &tuningBox,
                                (juce::Component*) &lockButton, (juce::Component*) &flatsButton, (juce::Component*) &clearButton })
        addAndMakeVisible (c);
}

void KeyCompassCanvas::showPage (int newPage)
{
    page = juce::jlimit (0, 1, newPage);
    const bool compass = page == 0;
    dialLayer.setVisible (compass);
    lidLayer.setVisible (compass);
    mapperButton.setVisible (compass);
    manualButton.setVisible (compass);
    mapper.setVisible (! compass);
    // detector settings belong to the compass; the Song Mapper puts its own controls in these plates
    for (juce::Component* c : { (juce::Component*) &modeBox, (juce::Component*) &responseBox, (juce::Component*) &tuningBox,
                                (juce::Component*) &lockButton, (juce::Component*) &clearButton })
        c->setVisible (compass);
}

void KeyCompassCanvas::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("Window size");
    // the size the window really has (a page that has never been resized has no saved size yet)
    const float current = getTransform().mat00;
    const float sizes[] = { 0.45f, 0.56f, 0.70f, 0.85f, 1.0f };
    const char* names[] = { "Small", "Medium", "Large", "Extra large", "Full (100%)" };
    for (int i = 0; i < 5; ++i)
        menu.addItem (i + 1, names[i], true, std::abs (current - sizes[i]) < 0.01f);
    menu.addSeparator();
    menu.addItem (101, page == 0 ? "Open Song Mapper" : "Back to Key Compass");
    menu.addItem (100, "Clear history and key");
    menu.addItem (102, "Open the manual");
    menu.addSectionHeader (kcui::versionText());

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safe = juce::Component::SafePointer<KeyCompassCanvas> (this), sizes] (int result)
                        {
                            if (safe == nullptr || result == 0) return;
                            if (result == 100)            safe->processor.getEngine().requestReset();
                            else if (result == 102)       kcui::openManual();
                            else if (result == 101)       { if (safe->onPageChosen) safe->onPageChosen (1 - safe->page); }
                            else if (safe->onScaleChosen) safe->onScaleChosen (sizes[result - 1]);
                        });
}

//==============================================================================
KeyCompassEditor::KeyCompassEditor (KeyCompassProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      lookAndFeel (*assets),
      canvas (p, *assets, uiState)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (canvas);
    canvas.onScaleChosen = [this] (float s) { applyScale (s); };
    canvas.onPageChosen = [this] (int page) { switchPage (page); };

    tooltipWindow = std::make_unique<kcui::TipWindow> (this, lookAndFeel, 750);

    // Pick the page before touching the size: setResizeLimits() resizes the (still empty) window to its
    // smallest size, and that must not be mistaken for a size the user chose.
    canvas.showPage (p.getEditorPage());

    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) kcui::Layout::designWidth * minScale), juce::roundToInt ((float) kcui::Layout::designHeight * minScale),
                     juce::roundToInt ((float) kcui::Layout::designWidth * maxScale), juce::roundToInt ((float) kcui::Layout::designHeight * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kcui::Layout::designWidth / (double) kcui::Layout::designHeight);

    applyScale (scaleForPage (canvas.getPage()));
    constructed = true;     // from here on, resizing the window is the user's choice and is remembered

    tick (0.0f);
    startTimerHz (30);

   #if JucePlugin_Build_Standalone
    // As a standalone app, listen to the microphone straight away (the processor never plays it back).
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->getMuteInputValue().setValue (false);
   #endif
}

KeyCompassEditor::~KeyCompassEditor()
{
    stopTimer();
    processor.getAudition().stop();
    tooltipWindow.reset();
    setLookAndFeel (nullptr);
}

float KeyCompassEditor::defaultScale() const
{
    // Both pages open as large as comfortably fits the screen, until the user picks a size of their own.
    float s = 0.72f;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        s = juce::jlimit (0.56f, 0.86f, (float) (d->userBounds.getHeight() - 110) / (float) kcui::Layout::designHeight);
    return s;
}

float KeyCompassEditor::scaleForPage (int page) const
{
    const float saved = page == 0 ? processor.getEditorScale() : processor.getMapperScale();
    return saved > 0.0f ? saved : defaultScale();
}

void KeyCompassEditor::switchPage (int page)
{
    page = juce::jlimit (0, 1, page);
    if (page == canvas.getPage()) return;
    if (page == 0) processor.getAudition().stop();
    canvas.showPage (page);
    processor.setEditorPage (page);
    applyScale (scaleForPage (page));
    resized();
}

void KeyCompassEditor::applyScale (float s)
{
    s = juce::jlimit (minScale, maxScale, s);
    setSize (juce::roundToInt ((float) kcui::Layout::designWidth * s), juce::roundToInt ((float) kcui::Layout::designHeight * s));
}

void KeyCompassEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void KeyCompassEditor::resized()
{
    const float s = (float) getWidth() / (float) kcui::Layout::designWidth;
    canvas.setTransform (juce::AffineTransform::scale (s));
    if (! constructed) return;
    if (canvas.getPage() == 0) processor.setEditorScale (s);
    else                       processor.setMapperScale (s);
}

void KeyCompassEditor::timerCallback()
{
    tick (1.0f / 30.0f);
}

void KeyCompassEditor::tick (float seconds)
{
    auto& params = processor.getState();
    const bool flats = params.getRawParameterValue (KeyCompassProcessor::flatsId)->load() > 0.5f;
    const int mode = (int) std::lround (params.getRawParameterValue (KeyCompassProcessor::modeId)->load());
    const bool lock = params.getRawParameterValue (KeyCompassProcessor::lockId)->load() > 0.5f;

    uiState.flats = flats;
    uiState.mode = (kc::AnalysisMode) juce::jlimit (0, 2, mode);
    uiState.locked = lock;
    canvas.mapper.setFlats (flats);

    bool changed = uiState.advance (processor.getEngine().getSnapshot(), seconds);
    if (flats != lastFlats || mode != lastMode || lock != lastLock)
    {
        changed = true;
        lastFlats = flats;
        lastMode = mode;
        lastLock = lock;
    }

    if (changed)
    {
        canvas.dialLayer.repaint();
        canvas.lidLayer.repaint();
    }
}
