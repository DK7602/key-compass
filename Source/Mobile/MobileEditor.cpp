// Key Compass - the Android app's screen (see MobileEditor.h for the three layouts)
#include "MobileEditor.h"
#include "../UI/Manual.h"
#include "../Version.h"

using namespace juce;
using namespace kcui;

namespace
{
    // the parts of the compass page shown on a phone (design pixels of the 1090 x 1390 page)
    const Rectangle<int> wholeCompass { 205, 30, 680, 1190 };      // lid and dial, upright
    const Rectangle<int> lidPart      { 290, 40, 510, 560 };       // side by side on a phone on its side
    const Rectangle<int> dialPart     { 250, 625, 590, 590 };

    constexpr int chordW = 308, timelineW = 442, browserW = 308, panelGap = 8;

    void paintEnamelPlate (Graphics& g, Rectangle<float> r, float corner)
    {
        ColourGradient body (Colour (0xff2e2621), r.getX(), r.getY(), Colour (0xff0b0806), r.getX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, corner);
        ColourGradient border (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (r.reduced (0.8f), corner, 1.8f);
    }
}

//==============================================================================
// The tabs: along the bottom (phone upright) or down the left (on its side)
//==============================================================================
class MobileEditor::TabBar : public Component
{
public:
    struct Item { int tab; String label; };

    explicit TabBar (Assets& a) : assets (a) {}

    std::function<void (int)> onTab;

    void setItems (std::vector<Item> newItems, int currentTab, bool isVertical)
    {
        items = std::move (newItems);
        current = currentTab;
        vertical = isVertical;
        repaint();
    }

    Rectangle<int> itemBounds (int i) const
    {
        const int n = jmax (1, (int) items.size());
        auto r = getLocalBounds();
        if (vertical)
        {
            const int h = jmin (96, r.getHeight() / n);
            return { 0, (r.getHeight() - h * n) / 2 + i * h, r.getWidth(), h };
        }
        const int w = r.getWidth() / n;
        return { i * w, 0, i == n - 1 ? r.getWidth() - i * w : w, r.getHeight() };
    }

    int itemIndexFor (int t) const
    {
        for (int i = 0; i < (int) items.size(); ++i) if (items[(size_t) i].tab == t) return i;
        return -1;
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        ColourGradient leather (Colour (0xff221a13), r.getX(), r.getY(), Colour (0xff0a0705), vertical ? r.getRight() : r.getX(),
                                vertical ? r.getY() : r.getBottom(), false);
        g.setGradientFill (leather);
        g.fillRect (r);
        // a brass rule along the edge that meets the screen
        ColourGradient brass (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, vertical ? r.getRight() : r.getX(),
                              vertical ? r.getY() : r.getBottom(), false);
        g.setGradientFill (brass);
        if (vertical) g.fillRect (r.getRight() - 2.5f, r.getY(), 2.5f, r.getHeight());
        else          g.fillRect (r.getX(), r.getY(), r.getWidth(), 2.5f);

        for (int i = 0; i < (int) items.size(); ++i)
        {
            const auto& it = items[(size_t) i];
            const bool on = it.tab == current;
            auto b = itemBounds (i).toFloat().reduced (vertical ? 6.0f : 3.0f, vertical ? 4.0f : 6.0f);
            if (vertical) b.removeFromRight (2.5f); else b.removeFromTop (2.5f);
            if (on)
            {
                ColourGradient lit (Colour (0x70f9dc8f), b.getCentreX(), b.getCentreY(), Colour (0x00f9dc8f), b.getCentreX(), b.getBottom(), true);
                g.setGradientFill (lit);
                g.fillRoundedRectangle (b, 10.0f);
                g.setColour (Palette::gold.withAlpha (0.85f));
                g.drawRoundedRectangle (b.reduced (1.0f), 10.0f, 1.4f);
            }
            auto iconArea = b.withTrimmedBottom (b.getHeight() * 0.38f).withTrimmedTop (b.getHeight() * 0.06f);
            const float sz = jmin (iconArea.getWidth(), iconArea.getHeight()) * 0.74f;
            drawIcon (g, it.tab, it.label, iconArea.withSizeKeepingCentre (sz, sz), on ? Palette::goldLight : Palette::goldDim);
            g.setColour (on ? Palette::goldLight : Palette::cream.withAlpha (0.72f));
            Font f = assets.cinzel (jmin (15.0f, b.getHeight() * 0.27f));
            while (textWidth (f, it.label) > b.getWidth() - 4.0f && f.getHeight() > 8.0f) f = f.withHeight (f.getHeight() - 0.5f);
            g.setFont (f);
            g.drawText (it.label, b.withTop (b.getBottom() - b.getHeight() * 0.38f).translated (0.0f, -1.0f), Justification::centred, false);
        }
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 20) return;
        for (int i = 0; i < (int) items.size(); ++i)
            if (itemBounds (i).contains (e.getPosition()))
            {
                if (onTab) onTab (items[(size_t) i].tab);
                return;
            }
    }

private:
    static void drawIcon (Graphics& g, int tab, const String& label, Rectangle<float> r, Colour c)
    {
        g.setColour (c);
        const float lw = jmax (1.4f, r.getWidth() * 0.075f);
        const auto ctr = r.getCentre();
        const float R = r.getWidth() * 0.5f;
        Path p;
        if (tab == MobileEditor::compassTab)
        {
            g.drawEllipse (r.reduced (lw * 0.5f), lw);
            p.addQuadrilateral (ctr.x, ctr.y - R * 0.78f, ctr.x + R * 0.22f, ctr.y, ctr.x, ctr.y + R * 0.78f, ctr.x - R * 0.22f, ctr.y);
            g.fillPath (p);
            g.setColour (Colour (0xffd23a24));
            Path north;
            north.addTriangle (ctr.x, ctr.y - R * 0.78f, ctr.x + R * 0.22f, ctr.y, ctr.x - R * 0.22f, ctr.y);
            g.fillPath (north);
        }
        else if (tab == MobileEditor::chordsTab)
        {
            // a guitar chord box
            auto b = r.reduced (R * 0.18f, R * 0.08f);
            for (int s = 0; s < 4; ++s)
            {
                const float x = b.getX() + b.getWidth() * (float) s / 3.0f;
                g.drawLine (x, b.getY(), x, b.getBottom(), lw * 0.8f);
            }
            for (int f = 0; f < 4; ++f)
            {
                const float y = b.getY() + b.getHeight() * (float) f / 3.0f;
                g.drawLine (b.getX(), y, b.getRight(), y, f == 0 ? lw * 1.6f : lw * 0.8f);
            }
            const float d = b.getWidth() * 0.26f;
            g.fillEllipse (b.getX() + b.getWidth() / 3.0f - d * 0.5f, b.getY() + b.getHeight() * 0.5f - d * 0.5f, d, d);
            g.fillEllipse (b.getRight() - d * 0.5f, b.getY() + b.getHeight() * 0.83f - d * 0.5f, d, d);
        }
        else if (tab == MobileEditor::songTab && label.contains ("MAPPER"))
        {
            // song mapper: the three panels
            const float w = r.getWidth() * 0.28f;
            for (int i = 0; i < 3; ++i)
                g.drawRoundedRectangle (r.getX() + (float) i * (w + r.getWidth() * 0.08f), r.getY() + (i == 1 ? 0.0f : R * 0.25f),
                                        w, r.getHeight() - (i == 1 ? 0.0f : R * 0.25f), 2.0f, lw);
        }
        else if (tab == MobileEditor::songTab)
        {
            // a timeline: sections of different lengths
            const float h = r.getHeight() * 0.2f;
            const float lens[3][2] = { { 0.0f, 0.45f }, { 0.25f, 0.85f }, { 0.1f, 1.0f } };
            for (int i = 0; i < 3; ++i)
                g.fillRoundedRectangle (r.getX() + r.getWidth() * lens[i][0], r.getY() + r.getHeight() * (0.1f + 0.32f * (float) i),
                                        r.getWidth() * (lens[i][1] - lens[i][0]), h, h * 0.4f);
        }
        else if (tab == MobileEditor::libraryTab)
        {
            // a stack of cards
            for (int i = 0; i < 3; ++i)
            {
                auto b = Rectangle<float> (r.getX() + R * 0.1f, r.getY() + (float) i * R * 0.62f, r.getWidth() - R * 0.2f, R * 0.5f);
                g.drawRoundedRectangle (b, 2.0f, lw * 0.9f);
                g.fillRect (b.getX() + R * 0.2f, b.getCentreY() - lw * 0.4f, b.getWidth() * 0.5f, lw * 0.8f);
            }
        }
        else
        {
            // settings: three sliders
            for (int i = 0; i < 3; ++i)
            {
                const float y = r.getY() + r.getHeight() * (0.18f + 0.32f * (float) i);
                g.drawLine (r.getX(), y, r.getRight(), y, lw);
                const float kx = r.getX() + r.getWidth() * (i == 1 ? 0.72f : (i == 0 ? 0.3f : 0.5f));
                const float kr = R * 0.2f;
                g.fillEllipse (kx - kr, y - kr, kr * 2.0f, kr * 2.0f);
            }
        }
    }

    Assets& assets;
    std::vector<Item> items;
    int current = 0;
    bool vertical = false;
};

//==============================================================================
// The compass on a phone: the lid and dial (one above the other, or side by side), cut from the page
//==============================================================================
class MobileEditor::CompassView : public Component
{
public:
    CompassView (Assets& a, const UiState& s) : bgA (a), bgB (a), dial (a, s), lid (a, s)
    {
        setInterceptsMouseClicks (false, true);
        for (auto* w : { &winA, &winB })
        {
            addAndMakeVisible (*w);
            w->setInterceptsMouseClicks (false, false);
            w->setOpaque (true);         // the page art fills it: nothing behind needs drawing when the needle moves
        }
        winA.addAndMakeVisible (stageA);
        winB.addAndMakeVisible (stageB);
        for (auto* st : { &stageA, &stageB })
            st->setBounds (0, 0, kcui::Layout::designWidth, kcui::Layout::designHeight);
        bgA.setBounds (stageA.getLocalBounds());
        bgB.setBounds (stageB.getLocalBounds());
        bgA.setBufferedToImage (true);
        bgB.setBufferedToImage (true);
        stageA.addAndMakeVisible (bgA);
        stageB.addAndMakeVisible (bgB);
        stageA.addAndMakeVisible (lid);
        stageA.addAndMakeVisible (dial);

        micNote.setButtonText ("The microphone is off - tap here to allow it");
        micNote.onClick = [this] { if (onMicTapped) onMicTapped(); };
        addChildComponent (micNote);
    }

    std::function<void()> onMicTapped;

    void layoutIn (Rectangle<int> area, bool sideBySide)
    {
        auto fit = [] (Component& win, Component& stage, Rectangle<int> part, Rectangle<int> box)
        {
            const float s = jmin ((float) box.getWidth() / (float) part.getWidth(), (float) box.getHeight() / (float) part.getHeight());
            const int w = roundToInt ((float) part.getWidth() * s), h = roundToInt ((float) part.getHeight() * s);
            win.setBounds (box.withSizeKeepingCentre (w, h));
            stage.setTransform (AffineTransform::translation ((float) -part.getX(), (float) -part.getY()).scaled (s));
        };

        if (! sideBySide)
        {
            winB.setVisible (false);
            if (dial.getParentComponent() != &stageA) stageA.addAndMakeVisible (dial);
            fit (winA, stageA, wholeCompass, area);
        }
        else
        {
            winB.setVisible (true);
            if (dial.getParentComponent() != &stageB) stageB.addAndMakeVisible (dial);
            const int gap = 10;
            const float s = jmin ((float) (area.getWidth() - gap) / (float) (lidPart.getWidth() + dialPart.getWidth()),
                                  (float) area.getHeight() / (float) jmax (lidPart.getHeight(), dialPart.getHeight()));
            const int wl = roundToInt ((float) lidPart.getWidth() * s), wd = roundToInt ((float) dialPart.getWidth() * s);
            auto row = area.withSizeKeepingCentre (wl + gap + wd, area.getHeight());
            fit (winA, stageA, lidPart, row.removeFromLeft (wl));
            row.removeFromLeft (gap);
            fit (winB, stageB, dialPart, row);
        }
        micNote.setBounds (area.removeFromBottom (44).reduced (10, 2));
    }

    void setMicNoteVisible (bool show) { micNote.setVisible (show); }
    void repaintLive()                 { dial.repaint(); lid.repaint(); }

    void paintOverChildren (Graphics& g) override
    {
        // a thin brass rim around each window onto the compass
        for (auto* w : { &winA, &winB })
        {
            if (! w->isVisible()) continue;
            const auto r = w->getBounds().toFloat();
            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawRect (r.expanded (2.0f), 2.0f);
            ColourGradient rim (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, r.getX(), r.getBottom(), false);
            g.setGradientFill (rim);
            g.drawRect (r, 1.6f);
        }
    }

private:
    class MicButton : public Button
    {
    public:
        MicButton() : Button ("mic") {}
        void paintButton (Graphics& g, bool highlighted, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (Colour (0xf0241008).withMultipliedBrightness (down ? 0.8f : (highlighted ? 1.15f : 1.0f)));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (Colour (0xffd0583c));
            g.drawRoundedRectangle (r, 9.0f, 1.6f);
            g.setColour (Palette::cream);
            g.setFont (FontOptions (16.0f, Font::bold));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (10, 2), Justification::centred, 2, 0.8f);
        }
    };

    Component winA, winB, stageA, stageB;
    StaticLayer bgA, bgB;
    DialLayer dial;
    LidLayer lid;
    MicButton micNote;
};

//==============================================================================
// Under the compass: what you change while listening (the rest is on the settings tab)
//==============================================================================
class MobileEditor::CompassStrip : public Component
{
public:
    CompassStrip (KeyCompassProcessor& p, Assets& a)
        : assets (a), processor (p),
          modeBox (a, *p.getState().getParameter (KeyCompassProcessor::modeId), { "Track", "Mono", "Song" },
                   { "one instrument", "voice, bass or lead", "whole mix" }),
          lockButton (a, RoundToggle::Icon::Lock), flatsButton (a, RoundToggle::Icon::Flat),
          clearButton (a, "CLEAR"),
          lockAttachment (p.getState(), KeyCompassProcessor::lockId, lockButton),
          flatsAttachment (p.getState(), KeyCompassProcessor::flatsId, flatsButton)
    {
        for (Component* c : { (Component*) &modeBox, (Component*) &lockButton, (Component*) &flatsButton, (Component*) &clearButton })
            addAndMakeVisible (c);
        clearButton.fontSize = 17.0f;
        clearButton.onClick = [this] { processor.getEngine().requestReset(); };
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 5);
        const int round = jmin (r.getHeight(), 46);
        clearButton.setBounds (r.removeFromRight (jmin (104, r.getWidth() / 4)).reduced (0, 2));
        r.removeFromRight (6);
        modeBox.setBounds (r.removeFromLeft (jmin (140, r.getWidth() * 3 / 8)).reduced (0, 1));
        r.removeFromLeft (6);
        // lock and flats, each with its name beside it
        const int each = r.getWidth() / 2;
        lockArea = r.removeFromLeft (each);
        flatsArea = r;
        lockButton.setBounds (lockArea.removeFromLeft (round).withSizeKeepingCentre (round, round));
        flatsButton.setBounds (flatsArea.removeFromLeft (round).withSizeKeepingCentre (round, round));
    }

    void paint (Graphics& g) override
    {
        paintEnamelPlate (g, getLocalBounds().toFloat().reduced (2.0f), 12.0f);
        g.setColour (Palette::goldLight.withAlpha (0.9f));
        g.setFont (assets.cinzel (13.0f));
        g.drawFittedText ("LOCK", lockArea.withTrimmedLeft (4), Justification::centredLeft, 1, 0.6f);
        g.drawFittedText ("FLATS", flatsArea.withTrimmedLeft (4), Justification::centredLeft, 1, 0.6f);
    }

private:
    Assets& assets;
    KeyCompassProcessor& processor;
    ChoiceBox modeBox;
    RoundToggle lockButton, flatsButton;
    KButton clearButton;
    AudioProcessorValueTreeState::ButtonAttachment lockAttachment, flatsAttachment;
    Rectangle<int> lockArea, flatsArea;
};

//==============================================================================
// The settings tab: the Song Mapper's plates and plaques, the compass's listening settings, sharing, the manual
//==============================================================================
class MobileEditor::MoreSheet : public Component, private ChangeListener
{
public:
    MoreSheet (KeyCompassProcessor& p, SongMapperPage& pg, Assets& a)
        : processor (p), page (pg), song (p.getSongMap()), assets (a),
          tapButton (a, "TAP TEMPO", KButton::Look::Plaque), captureButton (a, "CAPTURE CHORDS", KButton::Look::Plaque),
          songButton (a, String (CharPointer_UTF8 ("SONG \xe2\x96\xbe")), KButton::Look::Plaque),
          playStyleBox (a, { "Block", "Pulse", "Strum", "Arpeggio" },
                        { sm::playStyleDescription (sm::PlayStyle::Block), sm::playStyleDescription (sm::PlayStyle::Pulse),
                          sm::playStyleDescription (sm::PlayStyle::Strum), sm::playStyleDescription (sm::PlayStyle::Arpeggio) }),
          meterBox (a, { "4/4", "3/4", "6/8" }, { "four beats to a bar", "waltz time", "two swaying beats" }),
          octaveBox (a, { "-2 OCT", "-1 OCT", "NORMAL", "+1 OCT", "+2 OCT" }, 2),
          songLock (a, RoundToggle::Icon::Lock), flats (a, RoundToggle::Icon::Flat),
          shareMidi (a, "SHARE MIDI"), shareSong (a, "SHARE SONG FILE"), openSong (a, "OPEN SONG FILE"),
          clearSong (a, "CLEAR SONG"),
          modeBox (a, *p.getState().getParameter (KeyCompassProcessor::modeId), { "Track", "Mono", "Song" },
                   { "one instrument", "voice, bass or lead", "whole mix" }),
          responseBox (a, *p.getState().getParameter (KeyCompassProcessor::responseId), { "Fast", "Balanced", "Steady" },
                       { "reacts quickly", "recommended", "waits for more music" }),
          tuningBox (a, *p.getState().getParameter (KeyCompassProcessor::tuningId)),
          clearHistory (a, "CLEAR HISTORY"), manual (a, "OPEN THE MANUAL"),
          flatsAttachment (p.getState(), KeyCompassProcessor::flatsId, flats)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::all);

        for (Component* c : { (Component*) &tapButton, (Component*) &captureButton, (Component*) &songButton, (Component*) &playStyleBox,
                              (Component*) &meterBox, (Component*) &octaveBox, (Component*) &songLock, (Component*) &flats, (Component*) &volume,
                              (Component*) &shareMidi, (Component*) &shareSong, (Component*) &openSong, (Component*) &clearSong,
                              (Component*) &modeBox, (Component*) &responseBox, (Component*) &tuningBox, (Component*) &clearHistory,
                              (Component*) &manual })
            content.addAndMakeVisible (c);

        tapButton.onClick = [this] { page.tapTempo(); };
        captureButton.onClick = [this] { page.captureDetectedChords(); };
        songButton.onClick = [this] { page.showSongMenu(); };
        playStyleBox.onChange = [this] (int i) { song.playStyle = (sm::PlayStyle) i; song.changed(); };
        meterBox.onChange = [this] (int i) { song.timeSignature = i; song.changed(); };
        octaveBox.onChange = [this] (int i) { song.octave = i - 2; song.changed(); };
        songLock.onClick = [this] { page.setLockKey (songLock.getToggleState()); };
        songLock.setClickingTogglesState (true);
        volume.setSliderStyle (Slider::LinearHorizontal);
        volume.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.0, 0.01);
        volume.setColour (Slider::trackColourId, Palette::gold);
        volume.setColour (Slider::backgroundColourId, Colour (0xff17110c));
        volume.setColour (Slider::thumbColourId, Palette::goldLight);
        volume.onValueChange = [this]
        {
            song.auditionVolume = (float) volume.getValue();
            processor.getAudition().setVolume (song.auditionVolume);
        };
        shareMidi.onClick = [this] { page.shareMidi (-1); };
        shareSong.onClick = [this] { page.shareSongFile(); };
        openSong.onClick = [this] { page.openSongFile(); };
        clearSong.onClick = [this]
        {
            page.stopPlayback();
            song.clearAllChords();
        };
        clearHistory.onClick = [this] { processor.getEngine().requestReset(); };
        manual.onClick = [] { openManual(); };
        for (auto* b : { &shareMidi, &shareSong, &openSong, &clearSong, &clearHistory, &manual })
            b->fontSize = 16.5f;

        content.paintLabels = [this] (Graphics& g) { paintLabels (g); };
        song.addChangeListener (this);
        sync();
    }

    ~MoreSheet() override { song.removeChangeListener (this); }

    void paint (Graphics& g) override
    {
        paintFramedPanel (g, assets, getLocalBounds().toFloat(), "SETTINGS");
    }

    void resized() override
    {
        // the framed panel's parchment (the same insets as the Song Mapper's panels)
        auto r = getLocalBounds().withTrimmedLeft (27).withTrimmedRight (22).withTrimmedTop (44).withTrimmedBottom (27);
        viewport.setBounds (r);
        layoutContent (r.getWidth() - viewport.getScrollBarThickness() - 2);
    }

private:
    struct Content : public Component
    {
        std::function<void (Graphics&)> paintLabels;
        void paint (Graphics& g) override { if (paintLabels) paintLabels (g); }
    };

    struct Label { Rectangle<int> area; String text; bool heading; };

    void layoutContent (int w)
    {
        labels.clear();
        int y = 6;
        const int rowH = 50, labelW = jmin (150, w * 2 / 5), gap = 8;
        auto heading = [&] (const String& t)
        {
            y += 8;
            labels.push_back ({ { 0, y, w, 30 }, t, true });
            y += 36;
        };
        auto row = [&] (const String& label, Component& c, int cw = -1)
        {
            labels.push_back ({ { 0, y, labelW, rowH }, label, false });
            const int x = labelW + gap;
            const int width = cw > 0 ? jmin (cw, w - x) : w - x;
            c.setBounds (x, y + 3, width, rowH - 6);
            y += rowH + 4;
        };
        auto pair = [&] (Component& a, Component& b)
        {
            const int half = (w - gap) / 2;
            a.setBounds (0, y, half, rowH - 4);
            b.setBounds (half + gap, y, w - half - gap, rowH - 4);
            y += rowH + 2;
        };
        auto full = [&] (Component& c)
        {
            c.setBounds (0, y, w, rowH - 4);
            y += rowH + 2;
        };

        heading ("SONG MAPPER");
        pair (tapButton, captureButton);
        songButton.setBounds ((w - jmin (w, 240)) / 2, y, jmin (w, 240), rowH - 4);
        y += rowH + 2;
        row ("PLAY STYLE", playStyleBox);
        row ("TIME SIG.", meterBox);
        row ("OCTAVE", octaveBox);
        {
            // LOCK KEY and FLATS share a row: round glass buttons with their names beside them
            const int round = 52, half = w / 2;
            songLock.setBounds (0, y, round, round);
            labels.push_back ({ { round + 6, y, half - round - 6, round }, "LOCK KEY", false });
            flats.setBounds (half, y, round, round);
            labels.push_back ({ { half + round + 6, y, w - half - round - 6, round }, "FLATS", false });
            y += round + 8;
        }
        row ("VOLUME", volume);
        full (shareMidi);
        pair (shareSong, openSong);
        full (clearSong);

        heading ("LISTENING");
        row ("MODE", modeBox);
        row ("RESPONSE", responseBox);
        row ("TUNING A4", tuningBox);
        full (clearHistory);

        heading ("HELP");
        full (manual);
        labels.push_back ({ { 0, y + 4, w, 26 }, versionText(), false });
        y += 40;
        content.setSize (w, y);
    }

    void paintLabels (Graphics& g)
    {
        for (const auto& l : labels)
        {
            if (l.heading)
            {
                g.setColour (Palette::ink);
                g.setFont (assets.cinzel (20.0f));
                g.drawText (l.text, l.area, Justification::centredLeft, false);
                g.setColour (Colour (0xff6b4520).withAlpha (0.45f));
                g.fillRect ((float) l.area.getX(), (float) l.area.getBottom() - 2.0f, (float) l.area.getWidth(), 1.0f);
                g.setColour (Colours::white.withAlpha (0.5f));
                g.fillRect ((float) l.area.getX(), (float) l.area.getBottom() - 1.0f, (float) l.area.getWidth(), 1.0f);
            }
            else if (l.text.startsWith ("Key Compass "))
            {
                g.setColour (Palette::inkSoft);
                g.setFont (assets.baskItalic (14.0f));
                g.drawFittedText (l.text, l.area, Justification::centred, 1, 0.7f);
            }
            else
            {
                g.setColour (Palette::ink);
                g.setFont (assets.cinzel (16.0f));
                g.drawFittedText (l.text, l.area, Justification::centredLeft, 1, 0.7f);
            }
        }
    }

    void changeListenerCallback (ChangeBroadcaster*) override { sync(); }

    void sync()
    {
        playStyleBox.setIndex ((int) song.playStyle);
        meterBox.setIndex (song.timeSignature);
        octaveBox.setIndex (song.octave + 2);
        songLock.setToggleState (song.keyOverride >= 0, dontSendNotification);
        volume.setValue (song.auditionVolume, dontSendNotification);
    }

    KeyCompassProcessor& processor;
    SongMapperPage& page;
    sm::SongMap& song;
    Assets& assets;
    Viewport viewport;
    Content content;
    std::vector<Label> labels;
    KButton tapButton, captureButton, songButton;
    PlainChoiceBox playStyleBox, meterBox;
    PlainStepper octaveBox;
    RoundToggle songLock, flats;
    Slider volume;
    KButton shareMidi, shareSong, openSong, clearSong;
    ChoiceBox modeBox, responseBox;
    TuningBox tuningBox;
    KButton clearHistory, manual;
    AudioProcessorValueTreeState::ButtonAttachment flatsAttachment;
};

//==============================================================================
MobileEditor::MobileEditor (KeyCompassProcessor& p)
    : processor (p), lookAndFeel (*assets), canvas (p, *assets, uiState)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    addChildComponent (canvas);
    canvas.onPageChosen = [this] (int page)
    {
        if (page == 0) processor.getAudition().stop();
        canvas.showPage (page);
        processor.setEditorPage (page);
        tab = page == 0 ? (int) compassTab : (int) songTab;
        processor.setMobileTab (tab);
    };

    tabs = std::make_unique<TabBar> (*assets);
    tabs->onTab = [this] (int t) { showTab (t); };
    addChildComponent (*tabs);

    compass = std::make_unique<CompassView> (*assets, uiState);
    compass->onMicTapped = [this] { if (onRequestMicrophone) onRequestMicrophone(); };
    addChildComponent (*compass);
    strip = std::make_unique<CompassStrip> (p, *assets);
    addChildComponent (*strip);
    more = std::make_unique<MoreSheet> (p, canvas.mapper, *assets);
    addChildComponent (*more);

    practiceLayer.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (practiceLayer);

    tab = jlimit (0, (int) numTabs - 1, p.getMobileTab());
    // MANUAL buttons, F1 and the menus all end up here
    manualOpener() = [safe = SafePointer<MobileEditor> (this)] (const File& f)
    {
        return safe != nullptr && safe->showManual (f);
    };
    resized();
    tick (0.0f);
    startTimerHz (30);
}

MobileEditor::~MobileEditor()
{
    stopTimer();
    manualOpener() = nullptr;
    manual.reset();
    processor.getAudition().stop();
    canvas.mapper.closePractice();
    canvas.mapper.practiceHost = nullptr;
    putMapperInCanvas (true);
    setLookAndFeel (nullptr);
}

juce::Component* MobileEditor::findTabButton (int t)
{
    return tabs->isVisible() && tabs->itemIndexFor (t) >= 0 ? tabs.get() : nullptr;
}

juce::Component& MobileEditor::getMoreSheet() { return *more; }

bool MobileEditor::showManual (const File& pdf)
{
    if (auto r = ManualViewer::openPdf (pdf))
    {
        showManualWith (std::move (r));
        return true;
    }
    // can't draw it here: hand the PDF to another app (a PDF reader, Drive...)
    shareBox = ContentSharer::shareFilesScoped ({ URL (pdf) }, [] (bool, const String&) {}, this);
    return true;
}

void MobileEditor::showManualWith (std::unique_ptr<ManualViewer::Renderer> r)
{
    manual = std::make_unique<ManualViewer> (*assets, std::move (r));
    manual->onClose = [safe = SafePointer<MobileEditor> (this)]
    {
        // it closes from inside its own button: delete it afterwards
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->manual.reset(); });
    };
    practiceLayer.addAndMakeVisible (*manual);
    manual->setBounds (practiceLayer.getLocalBounds());
    manual->toFront (true);
}

void MobileEditor::setSimulatedInsets (BorderSize<int> safe, int keyboard)
{
    simulating = true;
    simulatedSafe = safe;
    simulatedKeyboard = keyboard;
    resized();
}

Rectangle<int> MobileEditor::safeArea (int* keyboardInset) const
{
    auto area = getLocalBounds();
    int kb = 0;
    if (simulating)
    {
        area = simulatedSafe.subtractedFrom (area);
        kb = simulatedKeyboard;
    }
    else if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
    {
        area = d->safeAreaInsets.subtractedFrom (area);
        kb = d->keyboardInsets.getBottom();
    }
    if (keyboardInset != nullptr) *keyboardInset = kb;
    return area;
}

void MobileEditor::putMapperInCanvas (bool inCanvas)
{
    auto& page = canvas.mapper;
    if (inCanvas == mapperInCanvas && page.getParentComponent() != nullptr) return;
    mapperInCanvas = inCanvas;
    page.closePractice();
    if (inCanvas)
    {
        page.setMobileLayout ({});
        page.setTransform ({});
        canvas.addChildComponent (page, 3);           // above the compass layers, below the shared plates
        page.setBounds (0, 0, kcui::Layout::designWidth, kcui::Layout::designHeight);
        page.practiceHost = nullptr;
    }
    else
    {
        addChildComponent (page);
        practiceLayer.toFront (false);
        page.practiceHost = [this] { return &practiceLayer; };
    }
}

int MobileEditor::effectiveTab() const
{
    if (layout == Layout::Classic) return canvas.getPage() == 0 ? (int) compassTab : (int) songTab;
    if (tab == moreTab || tab == compassTab) return tab;
    if (panes >= 3) return songTab;
    if (panes == 2) return tab == libraryTab ? (int) libraryTab : (int) chordsTab;
    return tab;
}

void MobileEditor::showTab (int t)
{
    t = jlimit (0, (int) numTabs - 1, t);
    if (t == compassTab) processor.getAudition().stop();
    tab = t;
    processor.setMobileTab (tab);
    if (layout == Layout::Classic)
    {
        const int page = (t == compassTab || t == moreTab) ? 0 : 1;
        if (page != canvas.getPage()) canvas.onPageChosen (page);
        return;
    }
    resized();
}

void MobileEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    // the old map behind everything, darkened so the panels stand out
    const auto& map = assets->mobileMap;
    if (map.isValid())
    {
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        g.drawImage (map, getLocalBounds().toFloat(), RectanglePlacement::fillDestination);
        g.fillAll (Colours::black.withAlpha (effectiveTab() == compassTab ? 0.35f : 0.5f));
    }
}

void MobileEditor::resized()
{
    int keyboard = 0;
    const auto area = safeArea (&keyboard);
    lastSafe = area;
    lastKeyboard = keyboard;
    if (area.isEmpty()) return;

    const float classicScale = jmin ((float) area.getWidth() / (float) kcui::Layout::designWidth,
                                     (float) area.getHeight() / (float) kcui::Layout::designHeight);
    const bool upright = area.getHeight() >= area.getWidth();
    if (upright && area.getWidth() >= 700 && classicScale >= 0.64f)
        layoutClassic (area);
    else
        layoutTouch (area, ! upright);

    practiceLayer.setBounds (area);
    for (auto* c : practiceLayer.getChildren())
        c->setBounds (practiceLayer.getLocalBounds());
    practiceLayer.toFront (false);
    keepTypingVisible (keyboard);
    repaint();
}

void MobileEditor::keepTypingVisible (int keyboard)
{
    // The on-screen keyboard covers the bottom of the screen. If you're typing into something it would hide
    // (a chord name, a section name), slide the Song Mapper up so the text stays in view.
    Component* moved = layout == Layout::Classic ? (Component*) &canvas : (Component*) &canvas.mapper;
    const auto base = layout == Layout::Classic ? canvasTransform : AffineTransform();
    int shift = 0;
    if (keyboard > 0)
        if (auto* focused = Component::getCurrentlyFocusedComponent())
            if (dynamic_cast<TextEditor*> (focused) != nullptr && moved->isParentOf (focused))
            {
                moved->setTransform (base);
                const auto r = getLocalArea (focused, focused->getLocalBounds());
                const int visibleBottom = getHeight() - keyboard - 10;
                if (r.getBottom() > visibleBottom)
                    shift = jmin (r.getBottom() - visibleBottom, jmax (0, r.getY() - 40));
            }
    moved->setTransform (base.translated (0.0f, (float) -shift));
}

void MobileEditor::layoutClassic (Rectangle<int> area)
{
    layout = Layout::Classic;
    panes = 3;
    tabs->setVisible (false);
    compass->setVisible (false);
    strip->setVisible (false);
    more->setVisible (false);
    putMapperInCanvas (true);
    canvas.setVisible (true);

    const float s = jmin ((float) area.getWidth() / (float) kcui::Layout::designWidth,
                          (float) area.getHeight() / (float) kcui::Layout::designHeight);
    const float x = (float) area.getX() + ((float) area.getWidth() - (float) kcui::Layout::designWidth * s) * 0.5f;
    const float y = (float) area.getY() + ((float) area.getHeight() - (float) kcui::Layout::designHeight * s) * 0.5f;
    canvasTransform = AffineTransform::scale (s).translated (x, y);
    canvas.setTransform (canvasTransform);
    const int page = (tab == compassTab || tab == moreTab) ? 0 : 1;
    canvas.showPage (page);
    processor.setEditorPage (page);
}

void MobileEditor::layoutTouch (Rectangle<int> area, bool sideways)
{
    layout = sideways ? Layout::Wide : Layout::Phone;
    canvas.setVisible (false);
    putMapperInCanvas (false);
    auto& page = canvas.mapper;

    // tabs: down the left on a screen on its side, along the bottom when upright
    auto content = area;
    const auto tabArea = sideways ? content.removeFromLeft (jmin (96, area.getWidth() / 8 + 10))
                                  : content.removeFromBottom (jmin (70, area.getHeight() / 11 + 6));

    // how many of the Song Mapper's panels fit side by side at a readable size
    const int headerH = sideways ? jlimit (52, 72, content.getHeight() / 6) : jlimit (60, 86, content.getHeight() / 10);
    auto mapperArea = content.withTrimmedTop (headerH);
    const float W = (float) mapperArea.getWidth() - 8.0f, H = (float) mapperArea.getHeight() - 6.0f;
    const float s3 = jmin ((W - 2.0f * panelGap) / (float) (chordW + timelineW + browserW), H / 610.0f);
    const float s2 = jmin ((W - (float) panelGap) / (float) (chordW + timelineW), H / 610.0f);
    panes = s3 >= 0.80f ? 3 : (s2 >= 0.80f ? 2 : 1);

    std::vector<TabBar::Item> items { { compassTab, "COMPASS" } };
    if (panes == 3)      items.push_back ({ songTab, "SONG MAPPER" });
    else if (panes == 2) { items.push_back ({ chordsTab, "CHORDS" }); items.push_back ({ libraryTab, "LIBRARY" }); }
    else                 { items.push_back ({ chordsTab, "CHORDS" }); items.push_back ({ songTab, "SONG" }); items.push_back ({ libraryTab, "LIBRARY" }); }
    items.push_back ({ moreTab, "SETTINGS" });
    const int shown = effectiveTab();
    tabs->setItems (items, shown, sideways);
    tabs->setBounds (tabArea);
    tabs->setVisible (true);

    const bool onCompass = shown == compassTab, onMore = shown == moreTab, onMapper = ! onCompass && ! onMore;

    // ---- the compass ----
    compass->setVisible (onCompass);
    strip->setVisible (onCompass);
    if (onCompass)
    {
        auto c = content.reduced (6, 4);
        const int stripH = jlimit (50, 62, c.getHeight() / 12);
        strip->setBounds (c.removeFromBottom (stripH).withSizeKeepingCentre (jmin (c.getWidth(), 560), stripH));
        c.removeFromBottom (6);
        compass->setBounds (c);
        compass->layoutIn (c.withZeroOrigin(), sideways);
        compass->setMicNoteVisible (! micAllowed);
    }

    // ---- settings ----
    more->setVisible (onMore);
    if (onMore)
    {
        auto c = content.reduced (4, 4);
        const float s = jlimit (0.8f, 1.3f, (float) c.getWidth() / 360.0f);
        const int w = jmin (c.getWidth(), roundToInt (520.0f * s));
        c = c.withSizeKeepingCentre (w, c.getHeight());
        more->setTransform ({});
        more->setBounds (0, 0, roundToInt ((float) c.getWidth() / s), roundToInt ((float) c.getHeight() / s));
        more->setTransform (AffineTransform::scale (s).translated ((float) c.getX(), (float) c.getY()));
    }

    // ---- the Song Mapper ----
    page.setVisible (onMapper);
    page.setBounds (content);
    if (onMapper)
    {
        SongMapperPage::MobileLayout ml;
        ml.active = true;
        const auto local = content.withZeroOrigin();
        ml.header = local.withHeight (headerH).withSizeKeepingCentre (jmin (local.getWidth(), 560), headerH).reduced (4, 3);
        // on its side every bit of height counts: the KEY / TEMPO plaque sits over the panels' top edge,
        // where their titles are, as it does over the timeline on the desktop page
        const int overlap = sideways ? jmin (headerH / 2, 30) : 0;
        auto r = local.withTrimmedTop (headerH - overlap).reduced (4, 3);
        ml.hideTitles = overlap > 0;
        const float rw = (float) r.getWidth(), rh = (float) r.getHeight();
        if (panes == 3)
        {
            const float s = jmin (1.3f, s3);
            ml.scale = s;
            const int cw = roundToInt ((float) chordW * s), bw = roundToInt ((float) browserW * s);
            ml.chords = r.removeFromLeft (cw);
            ml.browser = r.removeFromRight (bw);
            ml.timeline = r.reduced (panelGap, 0);
        }
        else if (panes == 2)
        {
            const float s = jmin (1.3f, s2);
            ml.scale = s;
            const int lw = roundToInt ((float) chordW * s);
            const auto left = r.removeFromLeft (lw);
            if (shown == libraryTab) ml.browser = left; else ml.chords = left;
            ml.timeline = r.withTrimmedLeft (panelGap);
        }
        else
        {
            // one panel: as large as reads comfortably; a short, wide screen uses the panels' wide layouts
            float s = jmin (1.3f, rw / 330.0f, rh / 590.0f);
            if (s < 0.9f && rw > rh)
                s = jmin (1.25f, rw / 620.0f, rh / 395.0f);
            ml.scale = jmax (0.6f, s);
            if (shown == chordsTab)       ml.chords = r;
            else if (shown == songTab)    { ml.timeline = r; ml.scale = jmax (0.6f, jmin (ml.scale, rw / 395.0f)); }   // a little wider for the rolls
            else                          ml.browser = r;
        }
        page.setMobileLayout (ml);
        practiceLayer.toFront (false);
    }
}

bool MobileEditor::handleBackButton()
{
    auto& page = canvas.mapper;
    if (PopupMenu::dismissAllActiveMenus()) return true;
    if (manual != nullptr) { manual.reset(); return true; }
    if (page.isPracticeOpen()) { page.closePractice(); return true; }
    if (layout == Layout::Classic)
    {
        if (canvas.getPage() == 1) { canvas.onPageChosen (0); return true; }
        return false;
    }
    if (tab != compassTab) { showTab (compassTab); return true; }
    return false;
}

void MobileEditor::setMicrophoneAllowed (bool allowed)
{
    micAllowed = allowed;
    if (compass != nullptr) compass->setMicNoteVisible (! allowed && compass->isVisible());
}

void MobileEditor::timerCallback()
{
    tick (1.0f / 30.0f);
    // the bars and the on-screen keyboard can come and go (rotation, typing a chord name)
    int kb = 0;
    const auto area = safeArea (&kb);
    if (area != lastSafe || kb != lastKeyboard)
        resized();
    // keep the screen on while you play along (Android lets it sleep otherwise)
    // keep the screen on while you're playing along: music heard in the last two minutes, the song playing,
    // or the practice view open (Android would otherwise dim it mid-song)
    const double now = Time::getMillisecondCounterHiRes();
    if (processor.getEngine().getSnapshot().signalPresent || processor.getAudition().isSequencePlaying() || canvas.mapper.isPracticeOpen())
        lastActivityMs = now;
    const bool awake = isShowing() && now - lastActivityMs < 120000.0;
    if (awake != keepingAwake)
    {
        keepingAwake = awake;
        Desktop::getInstance().setScreenSaverEnabled (! keepingAwake);
    }
}

void MobileEditor::tick (float seconds)
{
    auto& params = processor.getState();
    const bool flats = params.getRawParameterValue (KeyCompassProcessor::flatsId)->load() > 0.5f;
    const int mode = (int) std::lround (params.getRawParameterValue (KeyCompassProcessor::modeId)->load());
    const bool lock = params.getRawParameterValue (KeyCompassProcessor::lockId)->load() > 0.5f;
    uiState.flats = flats;
    uiState.mode = (kc::AnalysisMode) jlimit (0, 2, mode);
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
        if (canvas.isVisible())
        {
            canvas.dialLayer.repaint();
            canvas.lidLayer.repaint();
        }
        if (compass->isVisible()) compass->repaintLive();
    }
}
