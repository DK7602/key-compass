// Key Compass - the Android app's screen
#include "MobileEditor.h"

MobileEditor::MobileEditor (KeyCompassProcessor& p)
    : processor (p), lookAndFeel (*assets), canvas (p, *assets, uiState)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (canvas);
    canvas.onPageChosen = [this] (int page)
    {
        if (page == 0) processor.getAudition().stop();
        canvas.showPage (page);
        processor.setEditorPage (page);
    };
    canvas.showPage (p.getEditorPage());
    tick (0.0f);
    startTimerHz (30);
}

MobileEditor::~MobileEditor()
{
    stopTimer();
    processor.getAudition().stop();
    setLookAndFeel (nullptr);
}

void MobileEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void MobileEditor::resized()
{
    auto area = getLocalBounds();
    if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        area = d->safeAreaInsets.subtractedFrom (area);
    const float s = juce::jmin ((float) area.getWidth() / (float) kcui::Layout::designWidth,
                                (float) area.getHeight() / (float) kcui::Layout::designHeight);
    const float x = (float) area.getX() + ((float) area.getWidth() - (float) kcui::Layout::designWidth * s) * 0.5f;
    canvas.setTransform (juce::AffineTransform::scale (s).translated (x, (float) area.getY()));
}

bool MobileEditor::handleBackButton()
{
    if (canvas.mapper.isPracticeOpen()) { canvas.mapper.closePractice(); return true; }
    if (canvas.getPage() == 1) { canvas.onPageChosen (0); return true; }
    return false;
}

void MobileEditor::setMicrophoneAllowed (bool allowed)
{
    micAllowed = allowed;
    repaint();
}

void MobileEditor::timerCallback()
{
    tick (1.0f / 30.0f);
}

void MobileEditor::tick (float seconds)
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
