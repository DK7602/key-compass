// Key Compass - Song Mapper page
#include "SongMapperView.h"
#include "../PluginProcessor.h"
#include "Manual.h"
#include "../Version.h"
#include <optional>
#include <numeric>

namespace kcui
{
    using namespace juce;

    namespace
    {
        String utf8 (const std::string& s) { return String (CharPointer_UTF8 (s.c_str())); }

        // Page layout (design pixels)
        const Rectangle<float> lidEllipse   { 546.0f - 242.0f, 318.0f - 284.0f, 484.0f, 568.0f };
        const Rectangle<int>   headerArea   { 352, 226, 388, 94 };
        const Rectangle<int>   chordArea    { 10, 250, 308, 610 };
        const Rectangle<int>   timelineArea { 324, 332, 442, 914 };
        const Rectangle<int>   browserArea  { 772, 250, 308, 610 };
        const Rectangle<int>   medallionArea { 872, 14, 198, 198 };

        const Colour enamelText = Palette::cream;
        const Colour panelInk   = Palette::ink;
        const Colour chipBlue   { 0xff0a3480 };

        var makeDescription (const String& type)
        {
            auto* o = new DynamicObject();
            o->setProperty ("type", type);
            return var (o);
        }

        Image chipImage (Assets& a, const String& text, float scale)
        {
            const float w = jmax (64.0f, textWidth (a.bask (20.0f), text) + 22.0f), h = 36.0f;
            Image img (Image::ARGB, roundToInt (w * scale), roundToInt (h * scale), true);
            Graphics g (img);
            g.addTransform (AffineTransform::scale (scale));
            g.setColour (Colour (0xfff7ebcf));
            g.fillRoundedRectangle (1.0f, 1.0f, w - 2.0f, h - 2.0f, 7.0f);
            g.setColour (Palette::goldDark);
            g.drawRoundedRectangle (1.0f, 1.0f, w - 2.0f, h - 2.0f, 7.0f, 2.0f);
            g.setColour (Palette::ink);
            g.setFont (a.bask (20.0f));
            g.drawText (text, Rectangle<float> (0, 0, w, h), Justification::centred, false);
            return img;
        }

        float editorScaleOf (Component* c)
        {
            return Component::getApproximateScaleFactorForComponent (c);
        }

        void startChipDrag (Component* source, Assets& a, const var& desc, const String& label)
        {
            if (auto* container = DragAndDropContainer::findParentDragContainerFor (source))
            {
                const float s = editorScaleOf (source);
                container->startDragging (desc, source, ScaledImage (chipImage (a, label, s * 2.0f), 2.0), false);
            }
        }

        // a raised paper card: soft shadow, warm gradient with paper grain, light upper lip, darker lower lip
        void paintCard (Graphics& g, Assets& a, Rectangle<float> r, float radius, Colour top, Colour bottom,
                        Colour edge, float edgeW, float shadow = 1.0f, float grain = 0.22f)
        {
            if (shadow > 0.0f)
            {
                Path sp;
                sp.addRoundedRectangle (r.translated (1.2f, 2.4f), radius);
                DropShadow (Colours::black.withAlpha (0.38f * shadow), 5, {}).drawForPath (g, sp);
            }
            Path p;
            p.addRoundedRectangle (r, radius);
            ColourGradient bg (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
            g.setGradientFill (bg);
            g.fillPath (p);
            if (grain > 0.0f && a.face.isValid())
            {
                Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (p);
                g.setOpacity (grain);
                g.drawImage (a.face, r.withSizeKeepingCentre (jmax (r.getWidth(), r.getHeight()), jmax (r.getWidth(), r.getHeight())),
                             RectanglePlacement::fillDestination);
            }
            ColourGradient lip (Colours::white.withAlpha (0.65f), r.getX(), r.getY(), Colours::black.withAlpha (0.22f), r.getX(), r.getBottom(), false);
            lip.addColour (0.45, Colours::transparentWhite);
            g.setGradientFill (lip);
            g.drawRoundedRectangle (r.reduced (1.3f), jmax (1.0f, radius - 1.3f), 1.5f);
            g.setColour (edge);
            g.drawRoundedRectangle (r.reduced (0.4f), radius, edgeW);
        }

        // the usable parchment inside a baked brass panel (22 px frame, title scroll on top)
        Rectangle<int> panelContent (Rectangle<int> b)
        {
            return b.withTrimmedLeft (27).withTrimmedRight (27).withTrimmedTop (40).withTrimmedBottom (27);
        }

        // Cinzel's "1" looks like an "I", so anything with digits is set in Baskerville
        Font valueFont (Assets& a, const String& text, float h)
        {
            return text.containsAnyOf ("0123456789") ? a.bask (h * 1.02f) : a.cinzel (h);
        }

        String styleName (sm::GuitarStyle st, bool guitar)
        {
            if (guitar) return sm::guitarStyleName (st);
            switch (st)
            {
                case sm::GuitarStyle::OpenEasy:  return "Easy (simple triad)";
                case sm::GuitarStyle::Power:     return "Power (root + 5th)";
                case sm::GuitarStyle::Shell:     return "Shell (jazz)";
                case sm::GuitarStyle::Inversion: return "Inversion";
                case sm::GuitarStyle::Color:     return "Color / Extended";
                case sm::GuitarStyle::Triad:     return "Triad";
                case sm::GuitarStyle::Standard:
                case sm::GuitarStyle::Barre:
                case sm::GuitarStyle::NumStyles:
                default:                         return "Standard";
            }
        }

        // the name shown in the (narrow) voicing box
        String shortStyleName (sm::GuitarStyle st, bool guitar)
        {
            switch (st)
            {
                case sm::GuitarStyle::OpenEasy:  return guitar ? "OPEN" : "EASY";
                case sm::GuitarStyle::Triad:     return "TRIAD";
                case sm::GuitarStyle::Barre:     return guitar ? "BARRE" : "STANDARD";
                case sm::GuitarStyle::Power:     return "POWER";
                case sm::GuitarStyle::Shell:     return "SHELL";
                case sm::GuitarStyle::Inversion: return "INVERSION";
                case sm::GuitarStyle::Color:     return "COLOR";
                case sm::GuitarStyle::Standard:
                case sm::GuitarStyle::NumStyles:
                default:                         return "STANDARD";
            }
        }

        // Which octaves of keyboard to draw so every note is visible
        void keyboardRange (const std::vector<int>& notes, int minOctaves, int& lowNote, int& numWhite)
        {
            if (notes.empty()) { lowNote = 48; numWhite = 7 * minOctaves; return; }
            const int lo = *std::min_element (notes.begin(), notes.end()), hi = *std::max_element (notes.begin(), notes.end());
            lowNote = (lo / 12) * 12;
            const int octaves = juce::jmax (minOctaves, (hi - lowNote) / 12 + 1);
            numWhite = 7 * octaves;
        }

        std::vector<sm::Chord> chordsFromVar (const var& v)
        {
            std::vector<sm::Chord> out;
            for (auto& t : StringArray::fromTokens (v["chords"].toString(), " ", ""))
            {
                auto c = sm::parseChord (t.toStdString());
                if (c.isValid()) out.push_back (c);
            }
            return out;
        }

        String chordsToVarString (const std::vector<sm::Chord>& v)
        {
            StringArray a;
            for (auto& c : v) a.add (c.id());
            return a.joinIntoString (" ");
        }
    }

    //==============================================================================
    // Building blocks
    //==============================================================================
    KButton::KButton (Assets& a, const String& label, Look l) : Button (label), assets (a), look (l)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    void KButton::paintButton (Graphics& g, bool highlighted, bool down)
    {
        if (! isEnabled())
        {
            g.beginTransparencyLayer (0.45f);
            paintFace (g, false, false);
            g.endTransparencyLayer();
            return;
        }
        paintFace (g, highlighted, down);
    }

    void KButton::paintFace (Graphics& g, bool highlighted, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const bool on = lit || getToggleState();

        if (look == Look::Plaque)
        {
            // brass-framed parchment plate (rendered art; buttons keep roughly its 226 x 48 shape)
            r = getLocalBounds().toFloat();
            const auto& img = assets.plaque;
            if (img.isValid())
            {
                Graphics::ScopedSaveState save (g);
                g.setImageResamplingQuality (Graphics::highResamplingQuality);
                g.drawImage (img, r);
            }
            auto inner = r.reduced (7.5f * r.getHeight() / 48.0f);
            if (on)
            {
                g.setColour (Palette::blue.withAlpha (0.16f));
                g.fillRoundedRectangle (inner, 4.0f);
            }
            if (highlighted || down)
            {
                g.setColour (down ? Colours::black.withAlpha (0.14f) : Colours::white.withAlpha (0.20f));
                g.fillRoundedRectangle (inner, 4.0f);
            }
            inner = inner.reduced (inner.getHeight() * 0.32f, 0.0f);
            if (down) inner.translate (0.0f, 1.0f);
            // letterpress: light lip under dark ink
            const Font pf = valueFont (assets, getButtonText(), fontSize > 0 ? fontSize : jmin (24.0f, r.getHeight() * 0.44f));
            g.setFont (pf);
            g.setColour (Colours::white.withAlpha (0.45f));
            g.drawFittedText (getButtonText(), inner.translated (0.0f, 1.2f).toNearestInt(), Justification::centred, 1, 0.7f);
            g.setColour (on ? Colour (0xff0a3480) : Colour (0xff3e200a));
            g.drawFittedText (getButtonText(), inner.toNearestInt(), Justification::centred, 1, 0.7f);
            return;
        }

        if (look == Look::Round)
        {
            const auto c = r.getCentre();
            const float R = jmin (r.getWidth(), r.getHeight()) * 0.5f;
            ColourGradient ring (Palette::goldLight, c.x, c.y - R, Palette::goldDark, c.x, c.y + R, false);
            g.setGradientFill (ring);
            g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
            const float r2 = R - 3.0f;
            ColourGradient lens = on ? ColourGradient (Colour (0xffd8f3ff), c.x - r2 * 0.3f, c.y - r2 * 0.4f, Colour (0xff0a2a5e), c.x + r2, c.y + r2, true)
                                     : ColourGradient (Colour (0xff4b3e32), c.x - r2 * 0.3f, c.y - r2 * 0.4f, Colour (0xff070504), c.x + r2, c.y + r2, true);
            g.setGradientFill (lens);
            g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
            if (highlighted || down)
            {
                g.setColour (down ? Colours::black.withAlpha (0.2f) : Colours::white.withAlpha (0.12f));
                g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
            }
            g.setColour (on ? Colours::white : Palette::gold);
            g.setFont (assets.bask (fontSize > 0 ? fontSize : R * 1.05f));
            g.drawText (getButtonText(), r, Justification::centred, false);
            return;
        }

        // Enamel
        ColourGradient body (on ? Colour (0xff6e4c1a) : Colour (0xff2e2621), r.getX(), r.getY(),
                             on ? Colour (0xff2c1c08) : Colour (0xff0b0806), r.getX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, 7.0f);
        if (highlighted || down)
        {
            g.setColour (down ? Colours::black.withAlpha (0.25f) : Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        ColourGradient border (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (r, 7.0f, on ? 2.6f : 1.8f);
        if (on)
        {
            g.setColour (Palette::gold.withAlpha (0.25f));
            g.drawRoundedRectangle (r.reduced (3.0f), 5.0f, 2.0f);
        }
        g.setColour (on ? Colour (0xfffff0c0) : enamelText);
        g.setFont (valueFont (assets, getButtonText(), fontSize > 0 ? fontSize : jmin (19.0f, r.getHeight() * 0.5f)));
        g.drawFittedText (getButtonText(), r.reduced (6.0f, 1.0f).toNearestInt(), Justification::centred, 1, 0.7f);
    }

    MenuBox::MenuBox (Assets& a) : assets (a)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    void MenuBox::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        ColourGradient body (Colour (0xff2e2621), r.getX(), r.getY(), Colour (0xff0b0806), r.getX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, 7.0f);
        if (isMouseOver (true))
        {
            g.setColour (Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        ColourGradient border (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (r, 7.0f, 1.8f);

        auto content = r.reduced (9.0f, 2.0f);
        Rectangle<float> captionArea;
        if (caption.isNotEmpty())
            captionArea = content.withHeight (content.getHeight() * 0.40f);
        auto chev = content.removeFromRight (16.0f);
        if (caption.isNotEmpty()) chev = chev.withTrimmedTop (content.getHeight() * 0.3f);
        Path p;
        const float cx = chev.getCentreX(), cy = chev.getCentreY();
        p.startNewSubPath (cx - 5.5f, cy - 2.5f);
        p.lineTo (cx, cy + 3.5f);
        p.lineTo (cx + 5.5f, cy - 2.5f);
        g.setColour (Palette::gold);
        g.strokePath (p, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));

        if (caption.isNotEmpty())
        {
            g.setColour (Palette::goldDim);
            Font cf = assets.cinzel (jmin (13.0f, content.getHeight() * 0.36f));
            while (textWidth (cf, caption) > captionArea.getWidth() && cf.getHeight() > 7.0f) cf = cf.withHeight (cf.getHeight() - 0.5f);
            g.setFont (cf);
            g.drawText (caption, captionArea, Justification::bottomLeft, false);
            content.removeFromTop (content.getHeight() * 0.40f);
        }
        g.setColour (enamelText);
        g.setFont (valueFont (assets, text, fontSize > 0 ? fontSize : jmin (19.0f, content.getHeight() * 0.62f)));
        g.drawFittedText (text, content.toNearestInt(), caption.isNotEmpty() ? Justification::centredLeft : Justification::centred, 1, 0.6f);
    }

    void MenuBox::mouseDown (const MouseEvent&)
    {
        if (! buildMenu) return;
        PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        buildMenu (m);
        const auto screen = getScreenBounds();
        showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (screen).withMinimumWidth (jmax (screen.getWidth(), 220)).withStandardItemHeight (menuItemHeight()),
                         [safe = Component::SafePointer<MenuBox> (this)] (int result)
                         {
                             if (safe != nullptr && result != 0 && safe->onResult) safe->onResult (result);
                         });
    }

    void paintParchment (Graphics& g, Assets& a, Rectangle<float> r, float corner)
    {
        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.drawImage (a.face, r, RectanglePlacement::fillDestination);
        ColourGradient vignette (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                 Colours::black.withAlpha (0.22f), r.getX(), r.getY(), true);
        g.setGradientFill (vignette);
        g.fillRect (r);
    }

    void paintBrassPanel (Graphics& g, Assets& a, Rectangle<float> r, const String& title)
    {
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (4.0f, 7.0f), 16.0f);

        ColourGradient frame (Palette::goldLight, r.getX(), r.getY(), Colour (0xff6e4410), r.getRight(), r.getBottom(), false);
        frame.addColour (0.35, Colour (0xffd6a54a));
        frame.addColour (0.7, Colour (0xff9a6a22));
        g.setGradientFill (frame);
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (Colour (0xff3a2208));
        g.drawRoundedRectangle (r.reduced (0.5f), 16.0f, 1.6f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (3.0f), 13.0f, 1.0f);

        auto inner = r.reduced (9.0f);
        paintParchment (g, a, inner, 9.0f);
        g.setColour (Colour (0xff4a2c0c));
        g.drawRoundedRectangle (inner, 9.0f, 2.0f);

        // corner rivets
        for (auto p : { inner.getTopLeft(), inner.getTopRight(), inner.getBottomLeft(), inner.getBottomRight() })
        {
            const float dx = p.x < inner.getCentreX() ? 9.0f : -9.0f, dy = p.y < inner.getCentreY() ? 9.0f : -9.0f;
            ColourGradient rv (Palette::goldLight, p.x + dx - 2.0f, p.y + dy - 2.0f, Palette::goldDark, p.x + dx + 4.0f, p.y + dy + 4.0f, true);
            g.setGradientFill (rv);
            g.fillEllipse (p.x + dx - 4.5f, p.y + dy - 4.5f, 9.0f, 9.0f);
        }

        if (title.isNotEmpty())
        {
            const Font f = a.cinzel (19.0f);
            const float w = jmin (inner.getWidth() - 16.0f, textWidth (f, title) + 46.0f);
            Rectangle<float> plate (inner.getCentreX() - w * 0.5f, inner.getY() + 5.0f, w, 34.0f);
            ColourGradient pg (Colour (0xff3a2a1a), plate.getX(), plate.getY(), Colour (0xff120c07), plate.getX(), plate.getBottom(), false);
            g.setGradientFill (pg);
            g.fillRoundedRectangle (plate, 6.0f);
            ColourGradient pb (Palette::goldLight, plate.getX(), plate.getY(), Palette::goldDark, plate.getX(), plate.getBottom(), false);
            g.setGradientFill (pb);
            g.drawRoundedRectangle (plate, 6.0f, 2.0f);
            g.setColour (Palette::goldLight);
            g.setFont (f);
            g.drawFittedText (title, plate.reduced (10.0f, 2.0f).toNearestInt(), Justification::centred, 1, 0.7f);
        }
    }

    void paintFramedPanel (Graphics& g, Assets& a, Rectangle<float> r, const String& title)
    {
        const auto& img = a.mobilePanel;
        if (! img.isValid())
        {
            paintBrassPanel (g, a, r, title);
            return;
        }
        // The art is the CHORD DIAGRAMS panel at 1.5x with a blank scroll. It is drawn in nine slices so the
        // frame keeps its thickness at any size: corners as they are, edges and parchment stretched.
        // Each row has its own column cuts (the scroll rollers at the top, the ornaments at the bottom).
        Graphics::ScopedSaveState save (g);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        constexpr float k = 1.0f / 1.5f;                                  // art pixels -> design pixels
        const float iw = (float) img.getWidth(), ih = (float) img.getHeight();
        const float rows[4] = { 0.0f, 62.0f, ih - 120.0f, ih };
        const float cuts[3][2] = { { 100.0f, iw - 100.0f }, { 45.0f, iw - 45.0f }, { 125.0f, iw - 125.0f } };
        const float topH = rows[1] * k, botH = (rows[3] - rows[2]) * k;
        const float dy[4] = { r.getY(), r.getY() + topH, r.getBottom() - botH, r.getBottom() };
        for (int row = 0; row < 3; ++row)
        {
            const float sx[4] = { 0.0f, cuts[row][0], cuts[row][1], iw };
            const float leftW = cuts[row][0] * k, rightW = (iw - cuts[row][1]) * k;
            const float dx[4] = { r.getX(), r.getX() + leftW, r.getRight() - rightW, r.getRight() };
            for (int col = 0; col < 3; ++col)
            {
                const Rectangle<float> dst (dx[col], dy[row], dx[col + 1] - dx[col], dy[row + 1] - dy[row]);
                if (dst.getWidth() < 0.5f || dst.getHeight() < 0.5f) continue;
                const auto src = Rectangle<float> (sx[col], rows[row], sx[col + 1] - sx[col], rows[row + 1] - rows[row]).toNearestInt();
                // overlap by a hair so no seams show between the slices
                g.drawImage (img.getClippedImage (src), dst.expanded (0.35f), RectanglePlacement::stretchToFit, false);
            }
        }
        if (title.isNotEmpty())
        {
            // engraved on the scroll: dark ink with a light lip under it
            const float scrollL = r.getX() + 96.0f * k, scrollR = r.getRight() - 96.0f * k;
            const Rectangle<float> t (scrollL, r.getY() + 5.0f * k, scrollR - scrollL, 30.0f * k);
            Font f = a.cinzel (17.0f);
            while (textWidth (f, title) > t.getWidth() - 8.0f && f.getHeight() > 9.0f) f = f.withHeight (f.getHeight() - 0.5f);
            g.setFont (f);
            g.setColour (Colours::white.withAlpha (0.45f));
            g.drawText (title, t.translated (0.0f, 1.0f), Justification::centred, false);
            g.setColour (Colour (0xff3e2a14));
            g.drawText (title, t, Justification::centred, false);
        }
    }

    void LongPress::mouseDown (const MouseEvent& e)
    {
        didFire = false;
        stopTimer();
        if (isTouchUI() && ! e.mods.isPopupMenu())
            startTimer (holdMs);
    }

    void LongPress::mouseDrag (const MouseEvent& e)
    {
        if (e.getDistanceFromDragStart() > 10)
            stopTimer();
    }

    void LongPress::timerCallback()
    {
        stopTimer();
        didFire = true;
        if (onLongPress) onLongPress();
    }

    // the i-th white key of a keyboard that starts at the white key lowNote
    static int whiteKeyNote (int lowNote, int i) noexcept
    {
        static const int whiteSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
        int first = 0;                                   // lowNote's place among the white keys of its octave
        while (first < 6 && whiteSteps[first] < lowNote % 12) ++first;
        const int w = first + i;
        return (lowNote / 12) * 12 + 12 * (w / 7) + whiteSteps[w % 7];
    }

    void drawKeyboard (Graphics& g, Rectangle<float> area, const std::vector<int>& notes, int lowNote, int numWhite, float scale)
    {
        const float ww = area.getWidth() / (float) numWhite;
        // lowNote must be a white key
        auto isLit = [&] (int n) { return std::find (notes.begin(), notes.end(), n) != notes.end(); };

        for (int i = 0; i < numWhite; ++i)
        {
            const int note = whiteKeyNote (lowNote, i);
            Rectangle<float> k (area.getX() + i * ww, area.getY(), ww, area.getHeight());
            ColourGradient kg (Colour (0xfffffdf6), k.getX(), k.getY(), Colour (0xffe8dcc4), k.getX(), k.getBottom(), false);
            g.setGradientFill (kg);
            g.fillRect (k.reduced (0.6f, 0.0f));
            if (isLit (note))
            {
                ColourGradient lit (Colour (0xffbfe6ff), k.getX(), k.getBottom(), Colour (0xff3fa9ff), k.getX(), k.getY(), false);
                g.setGradientFill (lit);
                g.fillRect (k.reduced (1.4f, 0.0f).withTrimmedTop (area.getHeight() * 0.55f));
                g.setColour (Colour (0xff1c5fb5));
                const float d = 8.0f * scale;
                g.fillEllipse (k.getCentreX() - d * 0.5f, k.getBottom() - 14.0f * scale, d, d);
            }
            g.setColour (Colour (0xff3b2710).withAlpha (0.6f));
            g.drawRect (k, 0.8f);
            if (note % 12 == 0)
            {
                g.setColour (Colour (0xff6b4d2c));
                g.setFont (FontOptions (9.0f * scale));
                g.drawText ("C" + String (note / 12 - 1), k.withTrimmedTop (k.getHeight() - 13.0f * scale), Justification::centred, false);
            }
        }
        for (int i = 0; i < numWhite - 1; ++i)
        {
            const int white = whiteKeyNote (lowNote, i);
            if (white % 12 == 4 || white % 12 == 11) continue;      // no black key after E or B
            const int note = white + 1;
            Rectangle<float> k (area.getX() + (i + 1) * ww - ww * 0.32f, area.getY(), ww * 0.64f, area.getHeight() * 0.6f);
            ColourGradient kg (Colour (0xff3a3632), k.getX(), k.getY(), Colour (0xff050505), k.getX(), k.getBottom(), false);
            g.setGradientFill (kg);
            g.fillRoundedRectangle (k, 1.5f);
            if (isLit (note))
            {
                g.setColour (Colour (0xff3fa9ff));
                g.fillRoundedRectangle (k.reduced (1.5f).withTrimmedTop (k.getHeight() * 0.35f), 1.5f);
            }
        }
        g.setColour (Colour (0xff4a2c0c));
        g.drawRect (area, 1.2f);
    }

    GuitarGeometry guitarGeometry (const sm::GuitarVoicing& v, Rectangle<float> area, bool compact, float scale)
    {
        GuitarGeometry geo;
        geo.area = area;
        geo.base = (v.highestFret() <= 4) ? 1 : v.lowestFret();
        geo.topMarks = (compact ? 9.0f : 18.0f) * scale;
        const float labelW = (compact ? 0.0f : 20.0f) * scale;
        geo.grid = area.withTrimmedTop (geo.topMarks).withTrimmedLeft (labelW).withTrimmedRight (labelW * 0.3f);
        geo.sw = geo.grid.getWidth() / 5.0f;
        geo.fh = geo.grid.getHeight() / 5.0f;
        return geo;
    }

    std::array<int, 6> fingeringFor (const sm::GuitarVoicing& v)
    {
        // a simple, common fingering: the barre is the index finger; the other fretted notes take the
        // next fingers, lowest fret first (and the lower string first on the same fret)
        std::array<int, 6> fingers { { 0, 0, 0, 0, 0, 0 } };
        int next = 1;
        if (v.barreFret > 0)
        {
            for (int s = v.barreFrom; s <= v.barreTo; ++s)
                if (v.frets[(size_t) s] == v.barreFret) fingers[(size_t) s] = 1;
            next = 2;
        }
        std::vector<std::pair<int, int>> rest;     // fret, string
        for (int s = 0; s < 6; ++s)
            if (v.frets[(size_t) s] > 0 && fingers[(size_t) s] == 0) rest.push_back ({ v.frets[(size_t) s], s });
        std::sort (rest.begin(), rest.end());
        for (auto& [f, s] : rest)
        {
            juce::ignoreUnused (f);
            fingers[(size_t) s] = juce::jmin (4, next++);
        }
        return fingers;
    }

    void drawGuitarDiagram (Graphics& g, Assets& a, const sm::GuitarVoicing& v, Rectangle<float> area, bool compact, float scale, const int* fingers)
    {
        if (! v.valid) return;
        const auto geo = guitarGeometry (v, area, compact, scale);
        const auto& grid = geo.grid;
        const int base = geo.base;
        const float sw = geo.sw, fh = geo.fh, topMarks = geo.topMarks;
        const float labelW = (compact ? 0.0f : 20.0f) * scale;
        const float lw = jmax (0.8f, (compact ? 0.8f : 1.2f) * scale);

        g.setColour (Colour (0xff3b2710));
        for (int s = 0; s < 6; ++s)
            g.drawLine (grid.getX() + s * sw, grid.getY(), grid.getX() + s * sw, grid.getBottom(), lw * (1.0f + (5 - s) * 0.06f));
        for (int f = 0; f <= 5; ++f)
            g.drawLine (grid.getX(), grid.getY() + f * fh, grid.getRight(), grid.getY() + f * fh,
                        f == 0 && base == 1 ? (compact ? 2.4f : 4.0f) * scale : jmax (0.8f, (compact ? 0.8f : 1.1f) * scale));

        if (base > 1 && ! compact)
        {
            g.setFont (a.bask (14.0f * scale));
            g.drawText (String (base) + "fr", Rectangle<float> (area.getX() - 4.0f * scale, grid.getY(), labelW + 2.0f * scale, fh), Justification::centredRight, false);
        }
        else if (base > 1)
        {
            g.setFont (FontOptions (8.0f * scale));
            g.drawText (String (base), Rectangle<float> (area.getX() - 12.0f * scale, grid.getY(), 10.0f * scale, fh), Justification::centredRight, false);
        }

        const float dot = jmin (sw, fh) * (compact ? 0.62f : 0.66f);
        auto fingerText = [&] (int finger, Point<float> c)
        {
            if (fingers == nullptr || finger <= 0) return;
            g.setColour (Palette::cream);
            g.setFont (a.bask (dot * 0.62f));
            g.drawText (String (finger), Rectangle<float> (c.x - dot * 0.5f, c.y - dot * 0.5f, dot, dot), Justification::centred, false);
        };
        if (v.barreFret > 0)
        {
            const float y = grid.getY() + (v.barreFret - base + 0.5f) * fh;
            g.setColour (Colour (0xff2a1a0c));
            g.fillRoundedRectangle (grid.getX() + v.barreFrom * sw - dot * 0.5f, y - dot * 0.42f,
                                    (v.barreTo - v.barreFrom) * sw + dot, dot * 0.84f, dot * 0.42f);
            fingerText (1, { grid.getX() + v.barreFrom * sw, y });
        }

        for (int s = 0; s < 6; ++s)
        {
            const int f = v.frets[(size_t) s];
            const float x = grid.getX() + s * sw;
            if (f < 0)
            {
                const float m = (compact ? 2.5f : 4.5f) * scale, y = area.getY() + topMarks * 0.45f;
                g.setColour (Colour (0xff6b4d2c));
                g.drawLine (x - m, y - m, x + m, y + m, (compact ? 1.0f : 1.6f) * scale);
                g.drawLine (x - m, y + m, x + m, y - m, (compact ? 1.0f : 1.6f) * scale);
            }
            else if (f == 0)
            {
                const float m = (compact ? 2.6f : 4.6f) * scale, y = area.getY() + topMarks * 0.45f;
                g.setColour (Colour (0xff3b2710));
                g.drawEllipse (x - m, y - m, 2 * m, 2 * m, (compact ? 1.0f : 1.5f) * scale);
            }
            else if (! (v.barreFret > 0 && f == v.barreFret && s >= v.barreFrom && s <= v.barreTo))
            {
                const float y = grid.getY() + (f - base + 0.5f) * fh;
                g.setColour (Colour (0xff2a1a0c));
                g.fillEllipse (x - dot * 0.5f, y - dot * 0.5f, dot, dot);
                fingerText (fingers != nullptr ? fingers[s] : 0, { x, y });
            }
        }
    }

    //==============================================================================
    // Header: key and tempo plaque inside the lid
    //==============================================================================
    class HeaderView : public Component, public SettableTooltipClient
    {
    public:
        explicit HeaderView (SongMapperPage& p) : page (p) { setMouseCursor (MouseCursor::PointingHandCursor); }

        void paint (Graphics& g) override
        {
            // the brass-framed parchment plate and its KEY / TEMPO labels are part of the page art;
            // on a phone there is no painted page, so the plate is cut out of it
            if (page.isMobileLayout() && page.assets.mapperBackground.isValid())
            {
                const auto src = headerArea.toFloat() * 1.5f;
                g.setImageResamplingQuality (Graphics::highResamplingQuality);
                g.drawImage (page.assets.mapperBackground.getClippedImage (src.toNearestInt()), getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
            }
            auto left = keyZone().toFloat(), right = tempoZone().toFloat();

            auto drawCol = [&] (Rectangle<float> z, const String& value, const String& badge, bool glow)
            {
                z.removeFromTop (22.0f);                                     // engraved label
                auto bad = z.removeFromBottom (21.0f).withTrimmedBottom (2.0f);
                Font vf = page.assets.baskItalic (29.0f);
                while (textWidth (vf, value) > z.getWidth() - 8.0f && vf.getHeight() > 16.0f) vf = vf.withHeight (vf.getHeight() - 1.0f);
                if (glow)
                    drawGlowingText (g, value, vf, z, Justification::centred, Colour (0xff0b1a3a), Palette::blue, 9, 0.75f);
                else
                    drawGlowingText (g, value, vf, z, Justification::centred, Colour (0xff2a1608), {}, 0, 0.0f);
                g.setColour (Palette::inkSoft);
                g.setFont (page.assets.baskItalic (16.0f));
                g.drawFittedText (badge, bad.toNearestInt(), Justification::centred, 1, 0.8f);
            };

            const auto src = page.keySource();
            const char* badge = "add chords, or click to set";
            switch (src)
            {
                case SongMapperPage::KeySource::Set:    badge = "set by you - click to change"; break;
                case SongMapperPage::KeySource::Heard:  badge = "heard - click to change"; break;
                case SongMapperPage::KeySource::Chords: badge = "from chords - click to change"; break;
                case SongMapperPage::KeySource::Kept:   badge = "last set - click to change"; break;
                case SongMapperPage::KeySource::None:   break;
            }
            drawCol (left, page.keyText(), badge, src != SongMapperPage::KeySource::None);
            const double bpmValue = page.effectiveBpm();
            const String bpm = String (bpmValue, std::abs (bpmValue - std::round (bpmValue)) < 0.05 ? 0 : 1) + " BPM";
            drawCol (right, bpm, String (CharPointer_UTF8 (page.tempoFromHost() ? "from DAW  \xc2\xb7  " : "drag or tap  \xc2\xb7  ")) + page.song.timeSigName(), false);
        }

        Rectangle<int> keyZone() const   { return getLocalBounds().reduced (11).removeFromLeft (getWidth() / 2 - 14); }
        Rectangle<int> tempoZone() const { return getLocalBounds().reduced (11).removeFromRight (getWidth() / 2 - 14); }

        void mouseDown (const MouseEvent& e) override
        {
            page.takeFocus();
            dragging = false;
            if (keyZone().contains (e.getPosition()))
            {
                showKeyMenu();
                return;
            }
            if (e.mods.isPopupMenu() || e.y > getHeight() - 26)
            {
                showTempoMenu();
                return;
            }
            dragStart = page.effectiveBpm();
            dragging = true;
        }

        void mouseUp (const MouseEvent& e) override
        {
            // a tap (no drag) on the tempo opens its options on a touch screen
            if (dragging && isTouchUI() && e.getDistanceFromDragStart() < 6 && tempoZone().contains (e.getPosition()))
                showTempoMenu();
            dragging = false;
        }

        void mouseDrag (const MouseEvent& e) override
        {
            if (! dragging) return;
            if (isTouchUI() && e.getDistanceFromDragStart() < 6) return;
            const double v = jlimit (30.0, 300.0, std::round (dragStart - e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.1 : 0.5)));
            page.song.manualBpm = v;
            page.song.followHostTempo = false;
            page.song.changed();
        }

        void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
        {
            if (! tempoZone().contains (e.getPosition())) return;
            const double dir = (w.deltaY > 0 ? 1.0 : -1.0);
            page.song.manualBpm = jlimit (30.0, 300.0, std::round (page.effectiveBpm() + dir));
            page.song.followHostTempo = false;
            page.song.changed();
        }

        void showKeyMenu()
        {
            PopupMenu m, major, minor;
            m.setLookAndFeel (&getLookAndFeel());
            m.addItem (1, "Follow the detected key (heard, or from the chords)", true, page.song.keyOverride < 0);
            {
                sm::Key fk;
                const auto fs = page.keySource (&fk, true);
                const String name = utf8 (sm::pcName (fk.tonic, page.flats || SongMapperPage::keyUsesFlats (fk.tonic, fk.minor))) + (fk.minor ? " minor" : " major");
                String why = "nothing heard and no chords yet";
                if (fs == SongMapperPage::KeySource::Heard)       why = "now: " + name + ", heard";
                else if (fs == SongMapperPage::KeySource::Chords) why = "now: " + name + ", from your chords";
                else if (fs == SongMapperPage::KeySource::Kept)   why = "now: " + name + ", the last key you set";
                m.addItem (2, "      " + why, false, false);
            }
            m.addSeparator();
            for (int pc = 0; pc < 12; ++pc)
            {
                major.addItem (100 + pc, utf8 (sm::pcName (pc, page.flats || SongMapperPage::keyUsesFlats (pc, false))) + " major", true, page.song.keyOverride == pc);
                minor.addItem (200 + pc, utf8 (sm::pcName (pc, page.flats || SongMapperPage::keyUsesFlats (pc, true))) + " minor", true, page.song.keyOverride == pc + 12);
            }
            m.addSubMenu ("Major keys", major);
            m.addSubMenu ("Minor keys", minor);
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (getScreenBounds()).withStandardItemHeight (menuItemHeight()),
                             [this, safe = SafePointer<HeaderView> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 if (r == 1) page.song.setKeyOverride (-1);
                                 else if (r >= 200) page.song.setKeyOverride ((r - 200) + 12);
                                 else if (r >= 100) page.song.setKeyOverride (r - 100);
                                 page.song.changed();
                             });
        }

        void showTempoMenu()
        {
            PopupMenu m;
            m.setLookAndFeel (&getLookAndFeel());
            m.addItem (1, "Follow the DAW tempo", true, page.song.followHostTempo);
            m.addSeparator();
            m.addItem (10, "4/4", true, page.song.timeSignature == 0);
            m.addItem (11, "3/4", true, page.song.timeSignature == 1);
            m.addItem (12, "6/8", true, page.song.timeSignature == 2);
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (getScreenBounds()).withStandardItemHeight (menuItemHeight()),
                             [this, safe = SafePointer<HeaderView> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 if (r == 1) page.song.followHostTempo = ! page.song.followHostTempo;
                                 else page.song.timeSignature = r - 10;
                                 page.song.changed();
                             });
        }

    private:
        SongMapperPage& page;
        double dragStart = 120.0;
        bool dragging = false;
    };

    //==============================================================================
    // A brass button with an "expand" icon (four corner arrows)
    //==============================================================================
    class ExpandButton : public Button
    {
    public:
        ExpandButton() : Button ("expand") { setMouseCursor (MouseCursor::PointingHandCursor); }
        void paintButton (Graphics& g, bool highlighted, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.5f);
            const auto c = r.getCentre();
            const float R = jmin (r.getWidth(), r.getHeight()) * 0.5f;
            ColourGradient ring (Palette::goldLight, c.x, c.y - R, Palette::goldDark, c.x, c.y + R, false);
            g.setGradientFill (ring);
            g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
            const float r2 = R - 3.0f;
            ColourGradient lens (Colour (0xff4b3e32), c.x - r2 * 0.3f, c.y - r2 * 0.4f, Colour (0xff070504), c.x + r2, c.y + r2, true);
            g.setGradientFill (lens);
            g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
            if (highlighted || down)
            {
                g.setColour (down ? Colours::black.withAlpha (0.2f) : Colours::white.withAlpha (0.12f));
                g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
            }
            // four arrows pointing out to the corners
            const float a = r2 * 0.62f, h = r2 * 0.30f;
            Path p;
            for (int i = 0; i < 4; ++i)
            {
                const float sx = (i == 0 || i == 3) ? -1.0f : 1.0f, sy = (i < 2) ? -1.0f : 1.0f;
                const Point<float> tip (c.x + sx * a, c.y + sy * a);
                p.startNewSubPath (tip.x - sx * h, tip.y);
                p.lineTo (tip);
                p.lineTo (tip.x, tip.y - sy * h);
                p.startNewSubPath (tip);
                p.lineTo (c.x + sx * a * 0.25f, c.y + sy * a * 0.25f);
            }
            g.setColour (highlighted ? Palette::goldLight : Palette::gold);
            g.strokePath (p, PathStrokeType (jmax (1.5f, r2 * 0.14f), PathStrokeType::curved, PathStrokeType::rounded));
        }
    };

    //==============================================================================
    // Left panel: chord diagram + working progression ("tray")
    //==============================================================================
    class TrayGrid : public Component, public DragAndDropTarget
    {
    public:
        explicit TrayGrid (SongMapperPage& p) : page (p) { setComponentID ("trayGrid"); }

        static constexpr int cols = 4, rows = 2, slots = cols * rows;

        Rectangle<float> slotRect (int i) const
        {
            const float gap = 6.0f;
            const float w = ((float) getWidth() - gap * (cols - 1)) / cols, h = ((float) getHeight() - gap * (rows - 1)) / rows;
            return { (i % cols) * (w + gap), (i / cols) * (h + gap), w, h };
        }

        int slotAt (Point<float> p) const
        {
            for (int i = 0; i < slots; ++i) if (slotRect (i).expanded (3.0f).contains (p)) return i;
            return -1;
        }

        void paint (Graphics& g) override
        {
            auto& tray = page.song.tray;
            for (int i = 0; i < slots; ++i)
            {
                auto r = slotRect (i);
                if (i < (int) tray.size())
                {
                    const bool sel = i == page.song.selectedTray, playing = i == page.playingTray;
                    r = r.withTrimmedBottom (2.0f).withTrimmedRight (1.0f);
                    if (playing)
                    {
                        Path p;
                        p.addRoundedRectangle (r, 7.0f);
                        DropShadow (Palette::blue, 9, {}).drawForPath (g, p);
                    }
                    else if (sel)
                    {
                        Path p;
                        p.addRoundedRectangle (r, 7.0f);
                        DropShadow (Palette::gold.withAlpha (0.8f), 6, {}).drawForPath (g, p);
                    }
                    paintCard (g, page.assets, r, 7.0f, playing ? Colour (0xffeef8ff) : Colour (0xfffff8e6),
                               playing ? Colour (0xffcfe8ff) : Colour (0xffecd9b2),
                               sel ? Palette::goldDark : Colour (0xff7a5a2c), sel ? 2.2f : 1.0f, playing || sel ? 0.0f : 0.9f, 0.18f);
                    if (insertIndex == i)
                    {
                        g.setColour (Palette::blue);
                        g.fillRect (r.getX() - 4.0f, r.getY() + 4.0f, 3.0f, r.getHeight() - 8.0f);
                    }

                    const auto& c = tray[(size_t) i];
                    auto top = r.withHeight (28.0f);
                    Font f = page.assets.bask (21.0f);
                    const String name = page.chordText (c);
                    while (textWidth (f, name) > r.getWidth() - 6.0f && f.getHeight() > 11.0f) f = f.withHeight (f.getHeight() - 1.0f);
                    g.setColour (playing ? chipBlue : panelInk);
                    g.setFont (f);
                    g.drawText (name, top, Justification::centred, false);

                    auto d = r.withTrimmedTop (29.0f).reduced (12.0f, 5.0f);
                    if (page.song.guitar)
                        drawGuitarDiagram (g, page.assets, sm::guitarVoicing (c, page.song.style), d.withSizeKeepingCentre (jmin (d.getWidth(), 44.0f), d.getHeight()), true);
                    else
                    {
                        const auto notes = page.song.notesFor (c);
                        int low = 48, whites = 14;
                        keyboardRange (notes, 2, low, whites);
                        drawKeyboard (g, d.withSizeKeepingCentre (d.getWidth() + 10.0f, jmin (d.getHeight(), 32.0f)), notes, low, whites);
                    }
                }
                else
                {
                    const float dashes[] = { 5.0f, 4.0f };
                    Path p, dp;
                    p.addRoundedRectangle (r.reduced (1.0f), 7.0f);
                    PathStrokeType (1.6f).createDashedStroke (dp, p, dashes, 2);
                    g.setColour (Colour (0xff7a5a2c).withAlpha (i == (int) tray.size() ? 0.95f : 0.45f));
                    g.fillPath (dp);
                    if (i == (int) tray.size())
                    {
                        g.setColour (Palette::inkSoft);
                        g.setFont (page.assets.bask (34.0f));
                        g.drawText ("+", r.withTrimmedBottom (14.0f), Justification::centred, false);
                        g.setFont (page.assets.baskItalic (15.0f));
                        g.drawFittedText ("add", r.withTrimmedTop (r.getHeight() - 26.0f).toNearestInt(), Justification::centred, 1, 0.8f);
                    }
                    if (insertIndex == i)
                    {
                        g.setColour (Palette::blue);
                        g.fillRect (r.getX() - 4.0f, r.getY() + 4.0f, 3.0f, r.getHeight() - 8.0f);
                    }
                }
            }
        }

        void mouseDown (const MouseEvent& e) override
        {
            page.takeFocus();
            pressed = slotAt (e.position);
            dragStarted = false;
            auto& tray = page.song.tray;
            if (pressed < 0) return;

            if (pressed >= (int) tray.size())
            {
                showAddMenu();
                return;
            }
            if (e.mods.isPopupMenu())
            {
                showCardMenu (pressed);
                return;
            }
            page.song.selectedTray = pressed;
            page.focusChord (tray[(size_t) pressed]);
            // touch screens: press and hold for the card's menu
            hold.onLongPress = [this, idx = pressed] { if (! dragStarted) showCardMenu (idx); };
            hold.mouseDown (e);
        }

        void mouseUp (const MouseEvent&) override { hold.mouseUp(); }

        void mouseDrag (const MouseEvent& e) override
        {
            hold.mouseDrag (e);
            auto& tray = page.song.tray;
            if (hold.fired() || dragStarted || pressed < 0 || pressed >= (int) tray.size() || e.getDistanceFromDragStart() < 6) return;
            hold.cancel();
            dragStarted = true;
            auto d = makeDescription ("chord");
            d.getDynamicObject()->setProperty ("chords", String (tray[(size_t) pressed].id()));
            d.getDynamicObject()->setProperty ("from", "tray");
            d.getDynamicObject()->setProperty ("index", pressed);
            startChipDrag (this, page.assets, d, page.chordText (tray[(size_t) pressed]));
        }

        void showAddMenu()
        {
            PopupMenu m;
            m.setLookAndFeel (&getLookAndFeel());
            auto chords = std::make_shared<std::vector<sm::Chord>>();
            page.fillChordMenu (m, 1000, *chords);
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (getScreenBounds()).withStandardItemHeight (menuItemHeight()),
                             [this, chords, safe = SafePointer<TrayGrid> (this)] (int r)
                             {
                                 const int i = SongMapperPage::chordMenuIndex (r, 1000, *chords);
                                 if (safe == nullptr || i < 0) return;
                                 const auto c = (*chords)[(size_t) i];
                                 page.song.insertTray (c);
                                 page.focusChord (c);
                             });
        }

        void showCardMenu (int index)
        {
            PopupMenu m, change;
            m.setLookAndFeel (&getLookAndFeel());
            auto chords = std::make_shared<std::vector<sm::Chord>>();
            page.fillChordMenu (change, 1000, *chords);
            m.addItem (1, "Play");
            m.addItem (2, "Add to selected section");
            m.addSubMenu ("Change to", change);
            m.addItem (3, "Duplicate");
            m.addItem (5, "Practice view (big diagrams)");
            m.addSeparator();
            m.addItem (4, "Remove");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, index, chords, safe = SafePointer<TrayGrid> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0 || index >= (int) page.song.tray.size()) return;
                                 const auto c = page.song.tray[(size_t) index];
                                 if (r == 5) { page.showPractice (-2); return; }
                                 if (r == 1) page.auditionChord (c);
                                 else if (r == 2) page.addToSelectedSection ({ c });
                                 else if (r == 3) page.song.insertTray (c, index + 1);
                                 else if (r == 4) page.song.removeTray (index);
                                 else if (const int i = SongMapperPage::chordMenuIndex (r, 1000, *chords); i >= 0)
                                 {
                                     page.song.replaceTray (index, (*chords)[(size_t) i]);
                                     page.focusChord ((*chords)[(size_t) i]);
                                 }
                             });
        }

        // drag & drop
        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            const auto t = d.description["type"].toString();
            return t == "chord" || t == "progression";
        }
        void itemDragMove (const SourceDetails& d) override
        {
            int s = slotAt (d.localPosition.toFloat());
            if (s < 0 || s > (int) page.song.tray.size()) s = (int) page.song.tray.size();
            if (s != insertIndex) { insertIndex = s; repaint(); }
        }
        void itemDragExit (const SourceDetails&) override { insertIndex = -1; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            const int at = insertIndex < 0 ? (int) page.song.tray.size() : insertIndex;
            insertIndex = -1;
            const auto chords = chordsFromVar (d.description);
            if (d.description["from"].toString() == "tray")
            {
                const int from = (int) d.description["index"];
                page.song.moveTray (from, at > from ? at - 1 : at);     // "at" is an insertion point
            }
            else if (d.description["type"].toString() == "progression")
            {
                page.song.setTray (chords);
            }
            else
            {
                int pos = at;
                for (auto& c : chords) page.song.insertTray (c, pos++);
            }
            repaint();
        }

    private:
        SongMapperPage& page;
        int pressed = -1, insertIndex = -1;
        bool dragStarted = false;
        LongPress hold;
    };

    class ChordPanel : public Component, private Timer
    {
    public:
        explicit ChordPanel (SongMapperPage& p)
            : page (p), grid (p),
              guitarBtn (p.assets, "GUITAR"), pianoBtn (p.assets, "PIANO"), voicing (p.assets),
              keyChords (p.assets, "KEY CHORDS"),
              playBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"))), addBtn (p.assets, "ADD TO SECTION"),
              saveBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x98\x85 SAVE"))), clearBtn (p.assets, "CLEAR")
        {
            for (Component* c : { (Component*) &grid, (Component*) &guitarBtn, (Component*) &pianoBtn, (Component*) &voicing, (Component*) &entry,
                                  (Component*) &keyChords, (Component*) &playBtn, (Component*) &addBtn, (Component*) &saveBtn, (Component*) &clearBtn,
                                  (Component*) &expandBtn })
                addAndMakeVisible (c);
            expandBtn.setTooltip ("Practice view: show the chords big, to play along by hand (Esc closes it)");
            expandBtn.onClick = [this] { page.showPractice(); };

            guitarBtn.onClick = [this] { page.song.guitar = true; page.song.changed(); };
            pianoBtn.onClick  = [this] { page.song.guitar = false; page.song.changed(); };
            guitarBtn.setTooltip ("Show guitar chord shapes (and audition with a guitar sound)");
            pianoBtn.setTooltip ("Show piano voicings (and audition with a piano sound)");

            voicing.fontSize = 15.5f;
            voicing.setTooltip ("Voicing: how the chord is played on the instrument (Standard, Open, Triad, Barre, Power, Shell, Inversion, Color)");
            voicing.buildMenu = [this] (PopupMenu& m)
            {
                for (int i = 0; i < (int) sm::GuitarStyle::NumStyles; ++i)
                {
                    const auto st = (sm::GuitarStyle) i;
                    if (! page.song.guitar && st == sm::GuitarStyle::Barre) continue;     // no barre chords on a piano
                    m.addItem (i + 1, styleName (st, page.song.guitar), true, page.song.style == st
                                                                              || (! page.song.guitar && st == sm::GuitarStyle::Standard && page.song.style == sm::GuitarStyle::Barre));
                }
            };
            voicing.onResult = [this] (int r) { page.song.style = (sm::GuitarStyle) (r - 1); page.song.changed(); };

            entry.setTextToShowWhenEmpty (String (CharPointer_UTF8 ("Type a chord\xe2\x80\xa6")), Palette::goldDim);
            entry.setColour (TextEditor::backgroundColourId, Colour (0xff17110c));
            entry.setColour (TextEditor::textColourId, Palette::cream);
            entry.setColour (TextEditor::outlineColourId, Palette::goldDark);
            entry.setColour (TextEditor::focusedOutlineColourId, Palette::gold);
            entry.setColour (CaretComponent::caretColourId, Palette::goldLight);
            entry.setFont (page.assets.bask (21.0f));
            entry.setIndents (8, 6);
            entry.setTooltip ("Type any chord name, e.g. C, Am7, F#m7b5, Bbmaj9, D/F#, Gsus4, E5, and press Enter");
            entry.onReturnKey = [this]
            {
                const auto c = sm::parseChord (entry.getText().toStdString());
                if (c.isValid())
                {
                    page.song.insertTray (c);
                    page.focusChord (c);
                    entry.clear();
                    entryError = false;
                }
                else entryError = true;
                repaint();
            };
            entry.onTextChange = [this] { if (entryError) { entryError = false; repaint(); } };

            keyChords.setTooltip ("Pick from the chords that belong to the key");
            keyChords.onClick = [this] { grid.showAddMenu(); };
            playBtn.setTooltip ("Play the progression in order (click again to stop)");
            playBtn.onClick = [this] { if (page.playingTray >= 0) page.stopPlayback(); else page.playTray(); };
            addBtn.setTooltip ("Add these chords to the selected song section (click a section on the timeline to select it)");
            addBtn.onClick = [this] { page.addToSelectedSection (page.song.tray); };
            saveBtn.setTooltip ("Save this progression to your favourites");
            saveBtn.onClick = [this] { page.saveTrayAsFavourite(); };
            clearBtn.setTooltip ("Remove all chords from the progression");
            clearBtn.onClick = [this] { page.song.clearTray(); };
        }

        void resized() override
        {
            auto r = panelContent (getLocalBounds());
            // A short, wide panel (a phone on its side) puts the diagram on the left and the progression on the right
            const bool wide = r.getWidth() > 500 && r.getHeight() < 470;
            auto left = wide ? r.removeFromLeft ((r.getWidth() - 16) / 2) : r;
            if (wide) r.removeFromLeft (16);
            auto& top = wide ? left : r;

            auto row = top.removeFromTop (38);
            guitarBtn.setBounds (row.removeFromLeft (72).reduced (0, 3));
            row.removeFromLeft (3);
            pianoBtn.setBounds (row.removeFromLeft (62).reduced (0, 3));
            row.removeFromLeft (4);
            voicing.setBounds (row.reduced (0, 1));
            nameArea = top.removeFromTop (40);

            // the diagram and the progression cards share whatever height the panel has beyond the desktop's
            int diagramH = 148, gridH = 168;
            if (wide)
            {
                diagramH = jmax (110, top.getHeight());
                gridH = jlimit (120, 260, r.getHeight() - 26 - 6 - 34 - 5 - 34 - 5 - 36);
            }
            else
            {
                const int extra = r.getHeight() - (148 + 26 + 168 + 6 + 34 + 5 + 34 + 5 + 36);
                if (extra >= 0)
                {
                    diagramH += jmin (120, (extra * 55) / 100);
                    gridH += jmin (150, extra - (diagramH - 148));
                }
                else
                {
                    const int fromDiagram = jmin (-extra, 38);
                    diagramH -= fromDiagram;
                    gridH = jmax (120, gridH - (-extra - fromDiagram));
                }
            }
            diagramArea = top.removeFromTop (diagramH);
            expandBtn.setBounds (diagramArea.getRight() - 40, diagramArea.getY() + 2, 40, 40);
            trayLabel = r.removeFromTop (26);
            grid.setBounds (r.removeFromTop (gridH));
            r.removeFromTop (6);
            auto er = r.removeFromTop (34);
            keyChords.setBounds (er.removeFromRight (104));
            er.removeFromRight (5);
            entry.setBounds (er);
            r.removeFromTop (5);
            auto br = r.removeFromTop (34);
            const int w = (br.getWidth() - 10) / 3;
            playBtn.setBounds (br.removeFromLeft (w));
            br.removeFromLeft (5);
            saveBtn.setBounds (br.removeFromLeft (w));
            br.removeFromLeft (5);
            clearBtn.setBounds (br);
            r.removeFromTop (5);
            addBtn.setBounds (r.removeFromTop (jmin (36, r.getHeight())));
            addBtn.fontSize = 16.5f;
            saveBtn.fontSize = playBtn.fontSize = clearBtn.fontSize = 17.0f;
            keyChords.fontSize = 16.0f;
            guitarBtn.fontSize = pianoBtn.fontSize = 17.0f;
        }

        void paint (Graphics& g) override
        {
            if (page.isMobileLayout()) paintFramedPanel (g, page.assets, getLocalBounds().toFloat(), page.getMobileLayout().hideTitles ? String() : String ("CHORD DIAGRAMS"));
            guitarBtn.lit = page.song.guitar;
            pianoBtn.lit = ! page.song.guitar;
            voicing.text = shortStyleName (page.song.guitar || page.song.style != sm::GuitarStyle::Barre ? page.song.style : sm::GuitarStyle::Standard, page.song.guitar);
            playBtn.lit = page.playingTray >= 0;
            {
                const auto& secs = page.song.sections;
                const int sel = page.song.selectedSection;
                const String target = isPositiveAndBelow (sel, (int) secs.size()) ? secs[(size_t) sel].name.toUpperCase()
                                                                                   : (secs.empty() ? String ("NEW SECTION") : secs.front().name.toUpperCase());
                addBtn.setButtonText (String (CharPointer_UTF8 ("ADD ALL TO \xe2\x80\x9c")) + target + String (CharPointer_UTF8 ("\xe2\x80\x9d")));
                addBtn.setEnabled (! page.song.tray.empty());
            }

            const auto& c = page.focused;
            if (c.isValid())
            {
                auto n = nameArea.toFloat();
                const String name = page.chordText (c);
                const String longName = "(" + utf8 (c.longName (page.useFlats())) + ")";
                Font nf = page.assets.bask (34.0f), lf = page.assets.baskItalic (18.0f);
                const float nw = textWidth (nf, name), lw = textWidth (lf, longName);
                float x = n.getCentreX() - (nw + 8.0f + lw) * 0.5f;
                if (nw + 8.0f + lw > n.getWidth()) x = n.getX();
                g.setColour (panelInk);
                g.setFont (nf);
                g.drawText (name, Rectangle<float> (x, n.getY(), nw + 2.0f, n.getHeight()), Justification::centredLeft, false);
                g.setColour (Palette::inkSoft);
                g.setFont (lf);
                g.drawFittedText (longName, Rectangle<float> (x + nw + 8.0f, n.getY() + 4.0f, n.getRight() - x - nw - 8.0f, n.getHeight()).toNearestInt(),
                                  Justification::centredLeft, 1, 0.7f);

                auto d = diagramArea.toFloat();
                if (page.song.guitar)
                {
                    const auto v = sm::guitarVoicing (c, page.song.style);
                    drawGuitarDiagram (g, page.assets, v, guitarRect(), false);
                    if (v.shown.isValid() && v.shown.quality != c.quality)
                    {
                        g.setColour (Palette::inkSoft);
                        g.setFont (page.assets.baskItalic (16.0f));
                        g.drawText ("plays " + page.chordText (v.shown), d.removeFromRight (70.0f).withTrimmedTop (d.getHeight() - 22.0f), Justification::centredRight, false);
                    }
                }
                else
                {
                    const auto notes = page.song.notesFor (c);
                    int low = 36, whites = 21;
                    keyboardRange (notes, 3, low, whites);
                    drawKeyboard (g, keyboardRect(), notes, low, whites);
                    StringArray names;
                    for (int nn : notes) names.add (utf8 (sm::pcName (nn % 12, page.useFlats())) + String (nn / 12 - 1));
                    g.setColour (Palette::inkSoft);
                    g.setFont (page.assets.baskItalic (17.0f));
                    g.drawText (names.joinIntoString ("  "), d.removeFromBottom (24.0f), Justification::centred, false);
                }
            }
            else
            {
                g.setColour (Palette::inkSoft);
                g.setFont (page.assets.baskItalic (17.0f));
                // on a phone the progressions are on their own tab
                const bool browserElsewhere = page.isMobileLayout() && page.getMobileLayout().browser.isEmpty();
                g.drawFittedText (browserElsewhere ? "Pick a progression in LIBRARY, use KEY CHORDS, or type a chord below."
                                                   : "Pick a progression on the right, use KEY CHORDS, or type a chord below.",
                                  diagramArea.reduced (10, 30), Justification::centred, 3);
            }

            g.setColour (panelInk.withAlpha (0.9f));
            g.setFont (page.assets.cinzel (18.0f));
            g.drawText ("PROGRESSION", trayLabel.toFloat(), Justification::centredLeft, false);
            g.setFont (page.assets.baskItalic (15.0f));
            g.setColour (Palette::inkSoft);
            g.drawText ("drag to reorder", trayLabel.toFloat(), Justification::centredRight, false);

            if (entryError)
            {
                g.setColour (Colour (0xffb0301c));
                g.setFont (page.assets.baskItalic (16.0f));
                g.drawText ("Unknown chord", entry.getBounds().toFloat().translated (0, -19.0f).withHeight (19.0f), Justification::centredLeft, false);
            }
        }

    private:
        SongMapperPage& page;
    public:
        TrayGrid grid;
    private:
        KButton guitarBtn, pianoBtn;
        MenuBox voicing;
        TextEditor entry;
        KButton keyChords, playBtn, addBtn, saveBtn, clearBtn;
        ExpandButton expandBtn;
        Rectangle<int> nameArea, diagramArea, trayLabel;
        bool entryError = false;

        // ---- playable diagram: click a fret, string marker or key to hear just that note ----
        Rectangle<float> guitarRect() const
        {
            // 150 x 142 on the desktop; keeps that shape when a phone gives the diagram more room
            const float h = (float) diagramArea.getHeight() - 6.0f;
            return diagramArea.toFloat().withSizeKeepingCentre (jmin ((float) diagramArea.getWidth() - 50.0f, h * 150.0f / 142.0f), h);
        }
        Rectangle<float> keyboardRect() const { return diagramArea.toFloat().reduced (2.0f, 26.0f); }
        Rectangle<float> flashRect;
        bool flashRound = false;
        String flashName;

        struct DiagramHit { int note = -1; Rectangle<float> mark; bool round = false; };

        DiagramHit hitTestDiagram (Point<float> p) const
        {
            DiagramHit hit;
            const auto& c = page.focused;
            if (! c.isValid()) return hit;
            if (page.song.guitar)
            {
                const auto v = sm::guitarVoicing (c, page.song.style);
                if (! v.valid) return hit;
                const auto area = guitarRect();
                const auto geo = guitarGeometry (v, area, false);
                const int base = geo.base;
                const auto fretGrid = geo.grid;
                const float sw = geo.sw, fh = geo.fh;
                const int s = (int) std::lround ((p.x - fretGrid.getX()) / sw);
                if (s < 0 || s > 5 || std::abs (p.x - (fretGrid.getX() + (float) s * sw)) > sw * 0.55f) return hit;
                if (p.y < area.getY() - 2.0f || p.y > fretGrid.getBottom() + 2.0f) return hit;
                int fret;
                Point<float> centre;
                if (p.y < fretGrid.getY())
                {
                    fret = 0;                                              // above the nut: the open string
                    centre = { fretGrid.getX() + (float) s * sw, area.getY() + geo.topMarks * 0.45f };
                }
                else
                {
                    const int row = jlimit (0, 4, (int) ((p.y - fretGrid.getY()) / fh));
                    fret = base + row;
                    centre = { fretGrid.getX() + (float) s * sw, fretGrid.getY() + ((float) row + 0.5f) * fh };
                }
                hit.note = sm::standardTuning[(size_t) s] + fret;
                const float r = jmin (sw, fh) * 0.42f;
                hit.mark = Rectangle<float> (centre.x - r, centre.y - r, 2 * r, 2 * r);
                hit.round = true;
                return hit;
            }
            // piano: black keys sit on top of the white ones
            const auto notes = page.song.notesFor (c);
            int low = 36, whites = 21;
            keyboardRange (notes, 3, low, whites);
            const auto kb = keyboardRect();
            if (! kb.contains (p)) return hit;
            static const int whiteSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
            const float ww = kb.getWidth() / (float) whites;
            for (int i = 0; i < whites - 1; ++i)
            {
                const int step = whiteSteps[i % 7];
                if (step == 4 || step == 11) continue;
                Rectangle<float> k (kb.getX() + (float) (i + 1) * ww - ww * 0.32f, kb.getY(), ww * 0.64f, kb.getHeight() * 0.6f);
                if (k.contains (p)) { hit.note = low + 12 * (i / 7) + step + 1; hit.mark = k; return hit; }
            }
            const int i = jlimit (0, whites - 1, (int) ((p.x - kb.getX()) / ww));
            hit.note = low + 12 * (i / 7) + whiteSteps[i % 7];
            hit.mark = Rectangle<float> (kb.getX() + (float) i * ww, kb.getY(), ww, kb.getHeight());
            return hit;
        }

    public:
        void mouseDown (const MouseEvent& e) override
        {
            page.takeFocus();
            if (nameArea.contains (e.getPosition()) && page.focused.isValid())
            {
                page.auditionChord (page.focused);                         // the chord name plays the whole chord
                return;
            }
            const auto hit = hitTestDiagram (e.position);
            if (hit.note < 0) return;
            page.auditionNote (hit.note);
            flashRect = hit.mark;
            flashRound = hit.round;
            flashName = utf8 (sm::pcName (hit.note % 12, page.useFlats())) + String (hit.note / 12 - 1);
            repaint();
            startTimer (700);
        }

        void mouseMove (const MouseEvent& e) override
        {
            const bool over = hitTestDiagram (e.position).note >= 0 || (nameArea.contains (e.getPosition()) && page.focused.isValid());
            setMouseCursor (over ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
        }

        void paintOverChildren (Graphics& g) override
        {
            if (flashRect.isEmpty()) return;
            Path p;
            if (flashRound) p.addEllipse (flashRect);
            else            p.addRectangle (flashRect);
            DropShadow (Palette::blue, 10, {}).drawForPath (g, p);
            g.setColour (Palette::blue.withAlpha (0.55f));
            g.fillPath (p);
            g.setColour (Colour (0xff0a3480));
            g.setFont (page.assets.bask (16.0f));
            g.drawText (flashName, diagramArea.toFloat().removeFromBottom (20.0f).removeFromLeft (60.0f), Justification::centredLeft, false);
        }

    private:
        void timerCallback() override
        {
            stopTimer();
            flashRect = {};
            repaint();
        }
    };

    //==============================================================================
    // Centre panel: song structure & chord timeline
    //==============================================================================
    // Small brass button with a "drop into DAW" arrow; the owning row handles the drag
    class MidiDragButton : public Component, public SettableTooltipClient
    {
    public:
        MidiDragButton() { setMouseCursor (MouseCursor::DraggingHandCursor); }

        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.5f);
            const auto c = r.getCentre();
            const float R = jmin (r.getWidth(), r.getHeight()) * 0.5f;
            ColourGradient ring (Palette::goldLight, c.x, c.y - R, Palette::goldDark, c.x, c.y + R, false);
            g.setGradientFill (ring);
            g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
            const float r2 = R - 3.0f;
            ColourGradient lens (Colour (0xff4b3e32), c.x - r2 * 0.3f, c.y - r2 * 0.4f, Colour (0xff070504), c.x + r2, c.y + r2, true);
            g.setGradientFill (lens);
            g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);

            const float u = r2 / 10.0f;
            Path arrow;
            arrow.startNewSubPath (c.x, c.y - 6.0f * u);
            arrow.lineTo (c.x, c.y + 2.0f * u);
            arrow.startNewSubPath (c.x - 4.0f * u, c.y - 1.5f * u);
            arrow.lineTo (c.x, c.y + 2.5f * u);
            arrow.lineTo (c.x + 4.0f * u, c.y - 1.5f * u);
            arrow.startNewSubPath (c.x - 5.5f * u, c.y + 2.5f * u);
            arrow.lineTo (c.x - 5.5f * u, c.y + 5.5f * u);
            arrow.lineTo (c.x + 5.5f * u, c.y + 5.5f * u);
            arrow.lineTo (c.x + 5.5f * u, c.y + 2.5f * u);
            g.setColour (hover ? Palette::goldLight : Palette::gold);
            g.strokePath (arrow, PathStrokeType (jmax (1.4f, 1.6f * u), PathStrokeType::curved, PathStrokeType::rounded));
        }
        void mouseEnter (const MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const MouseEvent&) override  { hover = false; repaint(); }

    private:
        bool hover = false;
    };

    // A light "add something" button drawn like the dashed + chip
    class GhostButton : public Button
    {
    public:
        GhostButton (Assets& a, const String& label) : Button (label), assets (a) { setMouseCursor (MouseCursor::PointingHandCursor); }
        void paintButton (Graphics& g, bool highlighted, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            if (highlighted || down)
            {
                g.setColour (down ? Colour (0xff6b4520).withAlpha (0.12f) : Colours::white.withAlpha (0.35f));
                g.fillRoundedRectangle (r, 7.0f);
            }
            const float dashes[] = { 5.0f, 4.0f };
            Path p, dp;
            p.addRoundedRectangle (r, 7.0f);
            PathStrokeType (1.2f).createDashedStroke (dp, p, dashes, 2);
            g.setColour (Colour (0xff8a6a3c).withAlpha (highlighted ? 0.95f : 0.7f));
            g.fillPath (dp);
            g.setColour (highlighted ? panelInk : Palette::inkSoft);
            g.setFont (assets.baskItalic (fontSize));
            g.drawFittedText (getButtonText(), r.reduced (6.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.8f);
        }
        float fontSize = 17.5f;
    private:
        Assets& assets;
    };

    //==============================================================================
    // A bass or lead row: a small piano roll whose rows are the notes of the key
    //==============================================================================
    class LaneView : public Component, public SettableTooltipClient, private ScrollBar::Listener
    {
    public:
        static constexpr int headerH = 36, nameStripH = 17, barH = 9, gutterW = 38, numRows = 15;
        static int rollH() { return isTouchUI() ? 262 : 206; }      // taller rows for fingers
        static constexpr int maxRows = 20;     // 15 notes of the key, plus room for chord notes from outside it
        static int heightFor() { return headerH + 2 + rollH() + barH + 4; }

        LaneView (SongMapperPage& p, int sectionIndex, sm::Lane l)
            : page (p), section (sectionIndex), lane (l), loopBox (p.assets), fillBox (p.assets),
              removeBtn (p.assets, String (CharPointer_UTF8 ("\xc3\x97")), KButton::Look::Round)
        {
            setComponentID ("lane" + String (sectionIndex) + (l == sm::Lane::Bass ? "bass" : "lead"));
            addAndMakeVisible (loopBox);
            if (lane == sm::Lane::Bass) addAndMakeVisible (fillBox);
            addAndMakeVisible (dawBtn);
            addAndMakeVisible (removeBtn);
            addChildComponent (hbar);
            hbar.setAutoHide (false);
            hbar.addListener (this);
            setWantsKeyboardFocus (true);
            setTooltip (String ("Click to add a note (and hear it), drag a note to move it, drag its right end to change its length. ")
                        + "Double-click or Delete removes it; right-click for sharp, flat, length and more. Hold Alt for 16ths. "
                        + "The rows are the notes of the key, plus any chord notes outside it (their names are blue); "
                        + "gold rows are in the chord playing there.");

            loopBox.fontSize = 15.5f;
            loopBox.setTooltip ("How long this row's pattern is before it repeats through the section");
            loopBox.buildMenu = [this] (PopupMenu& m)
            {
                const int cur = line().loopBars;
                for (int b : { 1, 2, 4, 8 })
                    m.addItem (b, String (b) + (b == 1 ? " bar" : " bars"), b <= sec().bars, cur == b);
                m.addItem (100, "The whole section (no repeat)", true, cur == 0);
            };
            loopBox.onResult = [this] (int r) { page.song.setLineLoop (section, lane, r == 100 ? 0 : r); };

            fillBox.fontSize = 15.5f;
            fillBox.text = "FILL";
            fillBox.setTooltip ("Write a bass line from this section's chords (replaces the notes in this row)");
            fillBox.buildMenu = [this] (PopupMenu& m)
            {
                const bool any = ! sec().chords.empty();
                for (int i = 0; i < (int) sm::BassFill::NumFills; ++i)
                    m.addItem (i + 1, sm::bassFillName ((sm::BassFill) i), any);
                if (! any) m.addSectionHeader ("(add chords to this section first)");
            };
            fillBox.onResult = [this] (int r) { page.song.fillBass (section, (sm::BassFill) (r - 1)); };

            dawBtn.setTooltip (String ("Drag onto a track in your DAW to drop just this row of this section as MIDI"));
            dawBtn.addMouseListener (this, false);
            removeBtn.fontSize = 18.0f;
            removeBtn.setTooltip ("Hide this row (its notes are kept: + " + String (lane == sm::Lane::Bass ? "bass" : "lead") + " line brings it back)");
            removeBtn.onClick = [this] { page.song.setLineEnabled (section, lane, false); };
        }

        const sm::Section& sec() const
        {
            static const sm::Section none;
            return isPositiveAndBelow (section, (int) page.song.sections.size()) ? page.song.sections[(size_t) section] : none;
        }
        const sm::Line& line() const { return lane == sm::Lane::Lead ? sec().lead : sec().bass; }
        const std::vector<sm::Note>& notes() const { return editing ? working : line().notes; }
        bool valid() const { return isPositiveAndBelow (section, (int) page.song.sections.size()); }

        // called whenever the song changes
        void sync()
        {
            const auto& ln = line();
            loopBox.text = ln.loopBars > 0 ? "LOOP " + String (ln.loopBars) + (ln.loopBars == 1 ? " BAR" : " BARS") : String ("LOOP SECTION");
            loopBox.repaint();
            if (! isPositiveAndBelow (selected, (int) ln.notes.size())) selected = -1;
            updateScrollRange();
            repaint();
        }

        void resized() override
        {
            auto h = getLocalBounds().removeFromTop (headerH).reduced (0, 3);
            removeBtn.setBounds (h.removeFromRight (28).withSizeKeepingCentre (28, 28));
            h.removeFromRight (3);
            dawBtn.setBounds (h.removeFromRight (28).withSizeKeepingCentre (28, 28));
            h.removeFromRight (8);
            if (lane == sm::Lane::Bass)
            {
                fillBox.setBounds (h.removeFromRight (78));
                h.removeFromRight (5);
            }
            loopBox.setBounds (h.removeFromRight (jmin (140, h.getWidth() - 62)));
            labelArea = h;
            rollArea = getLocalBounds().withTrimmedTop (headerH + 2).withHeight (rollH());
            hbar.setBounds (Rectangle<int> ((int) gridX0(), rollArea.getBottom() + 1, (int) gridW(), barH - 1));
            updateScrollRange();
        }

        // ---- geometry ----
        static bool inScale (int pitch, const sm::Key& k)
        {
            static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 }, minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
            const int* sc = k.minor ? minor : major;
            const int d = ((pitch - k.tonic) % 12 + 12) % 12;
            for (int i = 0; i < 7; ++i) if (sc[i] == d) return true;
            return false;
        }

        // notes of the chords in this row's loop that are not in the key (they get rows of their own,
        // so a chord from outside the key can always be followed, e.g. an E major chord in A minor)
        std::array<bool, 12> outsideChordTones (const sm::Key& k) const
        {
            std::array<bool, 12> extra {};
            const auto& s = sec();
            const int n = (int) s.chords.size();
            if (n == 0) return extra;
            const double L = loopBeats(), bpb = page.song.beatsPerBar();
            const double len = s.barsPerChord <= 0.0f ? s.bars * bpb / n : s.barsPerChord * bpb;
            if (len <= 0.0) return extra;
            for (int i = 0; (double) i * len < L - 1.0e-6 && i < 4096; ++i)
            {
                const auto& c = s.chords[(size_t) (i % n)];
                auto pcs = c.pitchClasses();
                if (c.bass >= 0) pcs.push_back (c.bass);
                for (int pc : pcs)
                    if (! inScale (pc, k)) extra[(size_t) (((pc % 12) + 12) % 12)] = true;
            }
            return extra;
        }

        std::vector<int> rowPitches() const
        {
            const auto k = page.currentKey();
            const auto extra = outsideChordTones (k);
            // bass: from E1 up (where the bass fills write); lead: from the key note around middle C
            const int start = jlimit (0, 100, (lane == sm::Lane::Bass ? 28 : 55 + ((k.tonic - 55) % 12 + 12) % 12) + 12 * rangeShift);
            // the same span as 15 notes of the key, with any outside chord notes added in between (up to maxRows)
            std::vector<int> rows;
            int inKey = 0;
            for (int pitch = start; inKey < numRows && (int) rows.size() < maxRows && pitch < 128; ++pitch)
            {
                const bool k1 = inScale (pitch, k);
                if (k1 || extra[(size_t) (pitch % 12)]) rows.push_back (pitch);
                if (k1) ++inKey;
            }
            rowCount = jmax (numRows, (int) rows.size());
            return rows;
        }
        Rectangle<float> rowsArea() const { return rollArea.toFloat().withTrimmedTop ((float) nameStripH); }
        float rowH() const                { return rowsArea().getHeight() / (float) rowCount; }
        float gridX0() const              { return (float) rollArea.getX() + (float) gutterW + 2.0f; }
        float gridW() const               { return jmax (10.0f, (float) rollArea.getRight() - 3.0f - gridX0()); }
        double loopBeats() const          { return jmax (0.5, page.song.lineLoopBeats (section, lane)); }
        double pixelsPerBeat() const      { return jmax ((double) gridW() / loopBeats(), 22.0); }   // long loops scroll sideways
        float contentW() const            { return (float) (loopBeats() * pixelsPerBeat()); }
        float xFor (double beat) const    { return gridX0() + (float) (beat * pixelsPerBeat()) - scrollX; }
        double beatAt (float x) const     { return jlimit (0.0, loopBeats(), (double) (x - gridX0() + scrollX) / pixelsPerBeat()); }
        static double snapTo (double b, double step) { return std::round (b / step) * step; }

        // vertical position of a pitch in rows (half-way between rows for notes outside the key)
        float rowPos (int pitch, const std::vector<int>& rows) const
        {
            if (rows.empty()) return 0.0f;
            if (pitch < rows.front()) return -0.6f;
            if (pitch > rows.back()) return (float) rowCount - 0.4f;
            for (int i = 0; i < (int) rows.size(); ++i)
            {
                if (rows[(size_t) i] == pitch) return (float) i;
                if (i + 1 < (int) rows.size() && pitch > rows[(size_t) i] && pitch < rows[(size_t) i + 1]) return (float) i + 0.5f;
            }
            return 0.0f;
        }
        float yForRow (float r) const { return rowsArea().getBottom() - (r + 1.0f) * rowH(); }
        int rowAt (float y) const     { rowPitches(); return jlimit (0, rowCount - 1, (int) ((rowsArea().getBottom() - y) / rowH())); }

        Rectangle<float> noteRect (const sm::Note& n, const std::vector<int>& rows) const
        {
            const float r = rowPos (n.pitch, rows);
            const float x0 = xFor (n.start), x1 = xFor (jmin (loopBeats(), n.start + n.length));
            return { x0 + 0.5f, yForRow (r) + 0.5f, jmax (4.0f, x1 - x0 - 1.0f), rowH() - 1.0f };
        }

        int noteAt (Point<float> p, bool& onEnd) const
        {
            if (p.x < gridX0() || p.x > gridX0() + gridW()) return -1;
            const auto rows = rowPitches();
            const auto& ns = notes();
            for (int i = (int) ns.size() - 1; i >= 0; --i)
            {
                if (ns[(size_t) i].start >= loopBeats() - 1.0e-6) continue;
                const auto r = noteRect (ns[(size_t) i], rows);
                if (r.expanded (isTouchUI() ? 2.0f : 0.0f, isTouchUI() ? 3.0f : 1.0f).contains (p))
                {
                    // a finger needs a bigger handle on the note's end
                    onEnd = p.x > r.getRight() - (isTouchUI() ? jmin (18.0f, r.getWidth() * 0.45f) : jmin (7.0f, r.getWidth() * 0.35f));
                    return i;
                }
            }
            return -1;
        }

        // ---- painting ----
        void paint (Graphics& g) override
        {
            if (! valid()) return;
            const auto rows = rowPitches();
            const auto k = page.currentKey();
            const bool muted = lane == sm::Lane::Bass ? page.song.muteBass : page.song.muteLead;

            // engraved rule above the row, then its name
            g.setColour (Colour (0xff6b4520).withAlpha (0.35f));
            g.fillRect (2.0f, 0.0f, (float) getWidth() - 4.0f, 1.0f);
            g.setColour (Colours::white.withAlpha (0.45f));
            g.fillRect (2.0f, 1.0f, (float) getWidth() - 4.0f, 1.0f);
            auto la = labelArea.toFloat().withTrimmedLeft (2.0f);
            g.setColour (panelInk);
            g.setFont (page.assets.cinzel (19.0f));
            const String title = lane == sm::Lane::Bass ? "BASS" : "LEAD";
            g.drawText (title, la, Justification::centredLeft, false);
            if (muted)
            {
                g.setColour (Colour (0xff9a2a14));
                g.setFont (page.assets.baskItalic (16.0f));
                g.drawText ("muted", la.withTrimmedLeft (textWidth (page.assets.cinzel (19.0f), title) + 6.0f), Justification::centredLeft, false);
            }

            // the roll: a recessed parchment well
            const auto ra = rollArea.toFloat();
            const auto rowsR = rowsArea();
            const auto grid = Rectangle<float> (gridX0(), ra.getY(), gridW(), ra.getHeight());
            {
                ColourGradient well (Colour (0xffe9d4a6), ra.getX(), ra.getY(), Colour (0xfff6e8c6), ra.getX(), ra.getY() + 18.0f, false);
                g.setGradientFill (well);
                g.fillRoundedRectangle (ra, 5.0f);
            }
            {
                Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (grid.getSmallestIntegerContainer());
                // rows: the key note's rows a little darker, the others faintly striped
                for (int r = 0; r < (int) rows.size(); ++r)
                {
                    const float y = yForRow ((float) r);
                    const bool tonic = ((rows[(size_t) r] - k.tonic) % 12 + 12) % 12 == 0;
                    if (tonic)       g.setColour (Colour (0xff8a5a18).withAlpha (0.13f));
                    else if (r % 2)  g.setColour (Colour (0xff8a5a18).withAlpha (0.04f));
                    else             continue;
                    g.fillRect (grid.getX(), y, grid.getWidth(), rowH());
                }
                // the chord under each part of the roll: its notes tinted gold, its name along the top
                const auto& s = sec();
                const int n = (int) s.chords.size();
                const double L = loopBeats(), bpb = page.song.beatsPerBar();
                g.setColour (Colour (0xff6b4520).withAlpha (0.10f));
                g.fillRect (grid.getX(), ra.getY(), grid.getWidth(), (float) nameStripH);
                if (n > 0)
                {
                    const double len = s.barsPerChord <= 0.0f ? s.bars * bpb / n : s.barsPerChord * bpb;
                    for (int i = 0; (double) i * len < L - 1.0e-6; ++i)
                    {
                        const auto& c = s.chords[(size_t) (i % n)];
                        const auto pcs = c.pitchClasses();
                        const float x0 = xFor (i * len), x1 = xFor (jmin (L, (i + 1) * len));
                        if (x1 < grid.getX() || x0 > grid.getRight()) continue;
                        for (int r = 0; r < (int) rows.size(); ++r)
                            if (std::find (pcs.begin(), pcs.end(), rows[(size_t) r] % 12) != pcs.end())
                            {
                                g.setColour (Colour (0xffe0a526).withAlpha (0.36f));
                                g.fillRect (x0, yForRow ((float) r), x1 - x0, rowH());
                            }
                        g.setColour (Palette::inkSoft);
                        g.setFont (page.assets.baskItalic (15.0f));
                        const float tx = jmax (x0, grid.getX());
                        g.drawText (page.chordText (c), Rectangle<float> (tx + 3.0f, ra.getY(), x1 - tx - 4.0f, (float) nameStripH),
                                    Justification::centredLeft, true);
                    }
                }
                // time grid: 8ths, beats, bars
                for (double b = 0.0; b <= L + 1.0e-6; b += 0.5)
                {
                    const float x = xFor (b);
                    if (x < grid.getX() - 2.0f || x > grid.getRight() + 2.0f) continue;
                    const bool bar = std::abs (std::remainder (b, bpb)) < 1.0e-6, beat = std::abs (std::remainder (b, 1.0)) < 1.0e-6;
                    g.setColour (Colour (0xff6b4520).withAlpha (bar ? 0.55f : (beat ? 0.24f : 0.09f)));
                    g.fillRect (x - (bar ? 0.6f : 0.4f), bar ? ra.getY() : rowsR.getY(), bar ? 1.2f : 0.8f, bar ? ra.getHeight() : rowsR.getHeight());
                }
                // notes
                const auto& ns = notes();
                for (int i = 0; i < (int) ns.size(); ++i)
                {
                    const auto& note = ns[(size_t) i];
                    if (note.start >= L - 1.0e-6) continue;
                    const auto nr = noteRect (note, rows);
                    if (nr.getRight() < grid.getX() || nr.getX() > grid.getRight()) continue;
                    const float rp = rowPos (note.pitch, rows);
                    const bool offKey = std::abs (rp - std::floor (rp)) > 0.01f || rp < 0.0f || rp >= (float) rowCount;
                    const bool playing = ! editing && page.isNotePlaying (section, lane, i);
                    if (playing)
                    {
                        Path gp;
                        gp.addRoundedRectangle (nr, 3.0f);
                        DropShadow (Palette::blue, 8, {}).drawForPath (g, gp);
                    }
                    const Colour top = playing ? Colour (0xff8fd0ff) : (offKey ? Colour (0xffd0583c) : Colour (0xff4a3624));
                    const Colour bot = playing ? Colour (0xff1c5fb5) : (offKey ? Colour (0xff7a2414) : Colour (0xff140d07));
                    ColourGradient ng (top, nr.getX(), nr.getY(), bot, nr.getX(), nr.getBottom(), false);
                    g.setGradientFill (ng);
                    g.fillRoundedRectangle (nr, 2.5f);
                    g.setColour (i == selected ? Palette::goldLight : (offKey ? Colour (0xffffb08a) : Palette::goldDark));
                    g.drawRoundedRectangle (nr, 2.5f, i == selected ? 1.8f : 0.9f);
                    if (nr.getWidth() > 9.0f)   // grip on the right end
                    {
                        g.setColour (Palette::gold.withAlpha (0.75f));
                        g.fillRect (nr.getRight() - 3.5f, nr.getY() + 2.0f, 1.0f, nr.getHeight() - 4.0f);
                    }
                }
                // playhead
                const double ph = page.sectionPlayhead (section);
                if (ph >= 0.0)
                {
                    const float x = xFor (std::fmod (ph, L));
                    g.setColour (Palette::blue.withAlpha (0.85f));
                    g.fillRect (x - 0.75f, ra.getY(), 1.5f, ra.getHeight());
                }
            }
            // gutter: note names
            const auto gut = ra.withWidth ((float) gutterW);
            ColourGradient gg (Colour (0xff2e2621), gut.getX(), gut.getY(), Colour (0xff0b0806), gut.getRight(), gut.getY(), false);
            g.setGradientFill (gg);
            g.fillRoundedRectangle (gut, 4.0f);
            for (int r = 0; r < (int) rows.size(); ++r)
            {
                const int pitch = rows[(size_t) r];
                const bool tonic = ((pitch - k.tonic) % 12 + 12) % 12 == 0;
                const bool outside = ! inScale (pitch, k);     // a chord note from outside the key
                g.setColour (tonic ? Palette::goldLight : (outside ? Colour (0xff9fcbff) : Palette::cream.withAlpha (0.72f)));
                const float fs = jmin (12.0f, rowH() + 1.0f);     // smaller when there are extra rows
                g.setFont (tonic ? page.assets.bask (fs + 0.5f) : page.assets.baskPlain (fs));
                g.drawText (utf8 (sm::pcName (pitch % 12, page.useFlats())) + String (pitch / 12 - 1),
                            Rectangle<float> (gut.getX() + 1.0f, yForRow ((float) r), gut.getWidth() - 2.0f, rowH()), Justification::centred, false);
            }
            // arrows at the top of the gutter move the rows up or down an octave
            const auto arrows = gut.withHeight ((float) nameStripH);
            g.setColour (Palette::gold);
            Path up, down;
            const float ax = arrows.getCentreX(), ay = arrows.getCentreY();
            up.addTriangle (ax - 8.0f, ay - 3.5f, ax - 12.0f, ay + 2.5f, ax - 4.0f, ay + 2.5f);
            down.addTriangle (ax + 8.0f, ay + 3.5f, ax + 4.0f, ay - 2.5f, ax + 12.0f, ay - 2.5f);
            g.fillPath (up);
            g.fillPath (down);
            g.setColour (Colour (0xff6b4520));
            g.drawRoundedRectangle (ra, 5.0f, 1.2f);
            if (hasKeyboardFocus (false) && selected >= 0)
            {
                g.setColour (Palette::gold.withAlpha (0.5f));
                g.drawRoundedRectangle (ra.reduced (1.0f), 5.0f, 1.0f);
            }
        }

        // ---- editing ----
        void mouseMove (const MouseEvent& e) override
        {
            bool onEnd = false;
            const int hit = noteAt (e.position, onEnd);
            setMouseCursor (hit >= 0 ? (onEnd ? MouseCursor::LeftRightResizeCursor : MouseCursor::DraggingHandCursor)
                                     : (rollArea.contains (e.getPosition()) && e.position.x >= gridX0() ? MouseCursor::CrosshairCursor
                                                                                                        : MouseCursor::NormalCursor));
        }

        void mouseDown (const MouseEvent& e) override
        {
            dragStarted = false;
            resizing = false;
            pendingTap = false;
            sideScroll = false;
            hold.cancel();
            setViewportIgnoreDragFlag (false);
            if (e.eventComponent == &dawBtn) return;
            if (! valid() || ! rollArea.contains (e.getPosition())) return;
            grabKeyboardFocus();
            const auto p = e.position;
            if (isTouchUI() && ! e.mods.isPopupMenu() && p.x >= gridX0() - 1.0f && p.y >= rowsArea().getY())
            {
                bool end = false;
                const int touched = noteAt (p, end);
                if (touched < 0)
                {
                    // empty roll on a touch screen: a tap adds a note, a swipe scrolls (the song, or a long row sideways),
                    // press and hold for the row's menu
                    pendingTap = true;
                    scrollStart = scrollX;
                    hold.onLongPress = [this] { pendingTap = false; showRollMenu(); };
                    hold.mouseDown (e);
                    return;
                }
                // a note: it moves or stretches with the finger (the song doesn't scroll), press and hold for its menu
                setViewportIgnoreDragFlag (true);
                hold.onLongPress = [this, touched]
                {
                    if (resizing) return;
                    editing = false;
                    mode = Mode::None;
                    dragNote = -1;
                    selected = touched;
                    repaint();
                    showNoteMenu (touched);
                };
                hold.mouseDown (e);
            }
            if (p.x < gridX0() - 1.0f)
            {
                // the octave arrows at the top of the gutter; a note name plays that note
                if (p.y < (float) rollArea.getY() + (float) nameStripH)
                {
                    rangeShift = p.x < (float) rollArea.getX() + (float) gutterW * 0.5f ? jmin (rangeShift + 1, 2) : jmax (rangeShift - 1, -2);
                    repaint();
                }
                else
                {
                    const auto rows = rowPitches();
                    const int row = rowAt (p.y);
                    if (row < (int) rows.size()) page.auditionNote (rows[(size_t) row], soundFor());
                }
                return;
            }
            if (p.y < rowsArea().getY()) return;      // the chord-name strip
            const auto rows = rowPitches();
            bool onEnd = false;
            const int hit = noteAt (p, onEnd);
            if (e.mods.isPopupMenu())
            {
                if (hit >= 0) { selected = hit; repaint(); showNoteMenu (hit); }
                else showRollMenu();
                return;
            }
            working = line().notes;
            const double step = e.mods.isAltDown() ? 0.25 : 0.5;
            if (hit >= 0)
            {
                if (e.getNumberOfClicks() > 1)
                {
                    working.erase (working.begin() + hit);
                    selected = -1;
                    editing = true;
                    commit();
                    return;
                }
                selected = dragNote = hit;
                mode = onEnd ? Mode::Resize : Mode::Move;
                grabBeat = beatAt (p.x) - working[(size_t) hit].start;
                grabRow = rowAt (p.y);
                lastPitch = working[(size_t) hit].pitch;
                page.auditionNote (lastPitch, soundFor());
            }
            else
            {
                // a new note on the clicked row; drag right to set its length
                const int row = rowAt (p.y);
                const double L = loopBeats();
                const double start = jlimit (0.0, jmax (0.0, L - step), std::floor (beatAt (p.x) / step + 1.0e-6) * step);
                sm::Note n;
                n.start = start;
                n.length = jmax (step, jmin (newLength, L - start));
                n.pitch = row < (int) rows.size() ? rows[(size_t) row] : 60;
                n.velocity = std::abs (std::remainder (start, 1.0)) < 1.0e-6 ? 0.84f : 0.72f;
                working.push_back (n);
                selected = dragNote = (int) working.size() - 1;
                mode = Mode::Resize;
                grabBeat = 0.0;
                grabRow = row;
                lastPitch = n.pitch;
                page.auditionNote (n.pitch, soundFor());
            }
            editing = true;
            repaint();
        }

        void mouseDrag (const MouseEvent& e) override
        {
            hold.mouseDrag (e);
            if (pendingTap)
            {
                // a swipe that starts on empty roll: sideways scrolls a long row, anything else scrolls the song
                if (e.getDistanceFromDragStart() <= 8) return;
                const auto d = e.getOffsetFromDragStart();
                if (! sideScroll && hbar.isVisible() && std::abs (d.x) > std::abs (d.y))
                {
                    sideScroll = true;
                    setViewportIgnoreDragFlag (true);
                }
                if (sideScroll)
                    hbar.setCurrentRangeStart ((double) scrollStart - (double) d.x);
                else
                    pendingTap = false;          // the song list scrolls instead
                return;
            }
            if (hold.fired()) return;
            if (e.eventComponent == &dawBtn)
            {
                if (isTouchUI() || dragStarted || e.getDistanceFromDragStart() < 6) return;
                dragStarted = true;
                const auto f = page.midiFile (section, sm::laneBit (lane));
                if (f.existsAsFile())
                    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
                return;
            }
            if (! editing || ! isPositiveAndBelow (dragNote, (int) working.size())) return;
            if (e.getDistanceFromDragStart() < 3 && ! resizing) return;
            auto& n = working[(size_t) dragNote];
            const double L = loopBeats(), step = e.mods.isAltDown() ? 0.25 : 0.5;
            if (mode == Mode::Resize)
            {
                resizing = true;
                const double end = jlimit (n.start + step, L, snapTo (beatAt (e.position.x), step));
                n.length = end - n.start;
            }
            else if (mode == Mode::Move)
            {
                resizing = true;
                n.start = jlimit (0.0, jmax (0.0, L - step), snapTo (beatAt (e.position.x) - grabBeat, step));
                n.length = jmax (step, jmin (n.length, L - n.start));
                const auto rows = rowPitches();
                const int row = rowAt (e.position.y);
                if (row != grabRow && row < (int) rows.size())
                {
                    grabRow = row;
                    n.pitch = rows[(size_t) row];
                    if (n.pitch != lastPitch) { lastPitch = n.pitch; page.auditionNote (n.pitch, soundFor()); }
                }
            }
            repaint();
        }

        void mouseUp (const MouseEvent& e) override
        {
            hold.mouseUp();
            if (isTouchUI() && e.eventComponent == &dawBtn && e.getDistanceFromDragStart() < 8)
            {
                page.shareMidi (section, sm::laneBit (lane));
                return;
            }
            if (pendingTap)
            {
                pendingTap = false;
                if (! sideScroll && ! hold.fired() && e.getDistanceFromDragStart() <= 8)
                    addNoteAt (e.mouseDownPosition);
                sideScroll = false;
                setViewportIgnoreDragFlag (false);
                return;
            }
            setViewportIgnoreDragFlag (false);
            if (hold.fired())
            {
                editing = false;
                return;
            }
            if (e.eventComponent == &dawBtn || ! editing) return;
            if (mode == Mode::Resize && resizing && isPositiveAndBelow (dragNote, (int) working.size()))
                newLength = working[(size_t) dragNote].length;     // new notes take the last length you set
            commit();
        }

        void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
        {
            // sideways scrolling for long patterns; up/down scrolling goes to the song list
            if (hbar.isVisible() && (std::abs (w.deltaX) > 0.0f || e.mods.isShiftDown()))
            {
                const float d = std::abs (w.deltaX) > 0.0f ? w.deltaX : w.deltaY;
                hbar.setCurrentRangeStart (hbar.getCurrentRangeStart() - d * 160.0);
                return;
            }
            Component::mouseWheelMove (e, w);
        }

        bool keyPressed (const KeyPress& key) override
        {
            if (! isPositiveAndBelow (selected, (int) line().notes.size())) return false;
            const int code = key.getKeyCode();
            if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
            {
                editNote (selected, {}, true);
                return true;
            }
            if (code == KeyPress::upKey || code == KeyPress::downKey)
            {
                const int dir = code == KeyPress::upKey ? 1 : -1;
                if (key.getModifiers().isShiftDown())
                    editNote (selected, [dir] (sm::Note& n) { n.pitch = jlimit (0, 127, n.pitch + dir); });     // a semitone
                else
                    editNote (selected, [this, dir] (sm::Note& n) { n.pitch = nextInKey (n.pitch, dir); });     // the next note of the key
                return true;
            }
            if (code == KeyPress::leftKey || code == KeyPress::rightKey)
            {
                const double d = (code == KeyPress::rightKey ? 0.5 : -0.5) * (key.getModifiers().isAltDown() ? 0.5 : 1.0);
                const double L = loopBeats();
                editNote (selected, [d, L] (sm::Note& n) { n.start = jlimit (0.0, jmax (0.0, L - 0.25), n.start + d); n.length = jmin (n.length, L - n.start); });
                return true;
            }
            return false;
        }

        void focusLost (FocusChangeType) override { repaint(); }

    private:
        enum class Mode { None, Move, Resize };

        // a tap on an empty part of the roll (touch screens): a note there, as long as the last one you set
        void addNoteAt (Point<float> p)
        {
            if (! valid()) return;
            const auto rows = rowPitches();
            const double L = loopBeats(), step = 0.5;
            const int row = rowAt (p.y);
            sm::Note n;
            n.start = jlimit (0.0, jmax (0.0, L - step), std::floor (beatAt (p.x) / step + 1.0e-6) * step);
            n.length = jmax (step, jmin (newLength, L - n.start));
            n.pitch = row < (int) rows.size() ? rows[(size_t) row] : 60;
            n.velocity = std::abs (std::remainder (n.start, 1.0)) < 1.0e-6 ? 0.84f : 0.72f;
            working = line().notes;
            working.push_back (n);
            selected = (int) working.size() - 1;
            editing = true;
            page.auditionNote (n.pitch, soundFor());
            commit();
        }

        int soundFor() const { return lane == sm::Lane::Bass ? 1 : -1; }

        // the next row up or down: a note of the key, or a chord note from outside it (as the rows show)
        int nextInKey (int pitch, int dir) const
        {
            const auto k = page.currentKey();
            const auto extra = outsideChordTones (k);
            for (int p = pitch + dir; p >= 0 && p <= 127; p += dir)
                if (inScale (p, k) || extra[(size_t) (p % 12)]) return p;
            return pitch;
        }

        void scrollBarMoved (ScrollBar*, double newRangeStart) override
        {
            scrollX = (float) newRangeStart;
            repaint();
        }

        void updateScrollRange()
        {
            const double total = contentW(), vis = gridW();
            const bool needed = total > vis + 0.5;
            hbar.setVisible (needed);
            hbar.setRangeLimits (0.0, jmax (total, vis), dontSendNotification);
            hbar.setCurrentRange (needed ? (double) scrollX : 0.0, vis, dontSendNotification);
            scrollX = (float) hbar.getCurrentRangeStart();
        }

        void reselect (const std::optional<sm::Note>& sel)
        {
            selected = -1;
            if (! sel) return;
            const auto& ns = line().notes;
            for (int i = 0; i < (int) ns.size(); ++i)
                if (ns[(size_t) i] == *sel) { selected = i; break; }
        }

        void commit()
        {
            std::optional<sm::Note> sel;
            if (isPositiveAndBelow (selected, (int) working.size())) sel = working[(size_t) selected];
            editing = false;
            resizing = false;
            mode = Mode::None;
            dragNote = -1;
            if (working != line().notes)
                page.song.setLineNotes (section, lane, working);      // sorts the notes, so find the selected one again
            reselect (sel);
            repaint();
        }

        void editNote (int idx, std::function<void (sm::Note&)> fn, bool remove = false)
        {
            working = line().notes;
            if (! isPositiveAndBelow (idx, (int) working.size())) return;
            if (remove)
            {
                working.erase (working.begin() + idx);
                selected = -1;
            }
            else
            {
                fn (working[(size_t) idx]);
                selected = idx;
                page.auditionNote (working[(size_t) idx].pitch, soundFor());
            }
            editing = true;
            commit();
        }

        void showNoteMenu (int idx)
        {
            PopupMenu m, len;
            m.setLookAndFeel (&getLookAndFeel());
            const double bpb = page.song.beatsPerBar();
            len.addItem (20, "A 16th");
            len.addItem (21, "An 8th");
            len.addItem (22, "A beat");
            len.addItem (23, "Two beats");
            len.addItem (24, "A whole bar");
            m.addItem (1, "Play");
            m.addSubMenu ("Length", len);
            m.addItem (2, String (CharPointer_UTF8 ("Sharpen  \xe2\x99\xaf  (a semitone up)")));
            m.addItem (3, String (CharPointer_UTF8 ("Flatten  \xe2\x99\xad  (a semitone down)")));
            m.addItem (4, "Octave up");
            m.addItem (5, "Octave down");
            m.addSeparator();
            m.addItem (6, "Delete");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, idx, bpb, safe = SafePointer<LaneView> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0 || ! isPositiveAndBelow (idx, (int) line().notes.size())) return;
                                 const double L = loopBeats();
                                 if (r == 1)      page.auditionNote (line().notes[(size_t) idx].pitch, soundFor());
                                 else if (r == 2) editNote (idx, [] (sm::Note& n) { n.pitch = jmin (127, n.pitch + 1); });
                                 else if (r == 3) editNote (idx, [] (sm::Note& n) { n.pitch = jmax (0, n.pitch - 1); });
                                 else if (r == 4) editNote (idx, [] (sm::Note& n) { n.pitch = jmin (127, n.pitch + 12); });
                                 else if (r == 5) editNote (idx, [] (sm::Note& n) { n.pitch = jmax (0, n.pitch - 12); });
                                 else if (r == 6) editNote (idx, {}, true);
                                 else if (r >= 20 && r <= 24)
                                 {
                                     const double l = r == 20 ? 0.25 : (r == 21 ? 0.5 : (r == 22 ? 1.0 : (r == 23 ? 2.0 : bpb)));
                                     newLength = l;
                                     editNote (idx, [l, L] (sm::Note& n) { n.length = jmax (0.25, jmin (l, L - n.start)); });
                                 }
                             });
        }

        void showRollMenu()
        {
            PopupMenu m, fill;
            m.setLookAndFeel (&getLookAndFeel());
            if (lane == sm::Lane::Bass)
            {
                for (int i = 0; i < (int) sm::BassFill::NumFills; ++i) fill.addItem (10 + i, sm::bassFillName ((sm::BassFill) i));
                m.addSubMenu ("Fill from the chords", fill, ! sec().chords.empty());
            }
            m.addItem (1, "Clear this row", ! line().notes.empty());
            m.addItem (2, "Hide this row (the notes are kept)");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, safe = SafePointer<LaneView> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 if (r == 1) page.song.setLineNotes (section, lane, {});
                                 else if (r == 2) page.song.setLineEnabled (section, lane, false);
                                 else if (r >= 10) page.song.fillBass (section, (sm::BassFill) (r - 10));
                             });
        }

        SongMapperPage& page;
        int section;
        sm::Lane lane;
        MenuBox loopBox, fillBox;
        MidiDragButton dawBtn;
        KButton removeBtn;
        ScrollBar hbar { false };
        Rectangle<int> labelArea, rollArea;
        int rangeShift = 0;
        mutable int rowCount = numRows;     // rows on show (set by rowPitches)
        float scrollX = 0.0f;
        std::vector<sm::Note> working;
        bool editing = false, dragStarted = false, resizing = false;
        int dragNote = -1, selected = -1, lastPitch = -1, grabRow = -1;
        Mode mode = Mode::None;
        double grabBeat = 0.0, newLength = 1.0;
        LongPress hold;
        bool pendingTap = false, sideScroll = false;     // touch: waiting to see if a press on empty roll is a tap or a swipe
        float scrollStart = 0.0f;
    };

    class SectionRow : public Component, public DragAndDropTarget
    {
    public:
        SectionRow (SongMapperPage& p, int idx)
            : page (p), index (idx), bars (p.assets), perChord (p.assets),
              playBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xb6")), KButton::Look::Round),
              removeBtn (p.assets, String (CharPointer_UTF8 ("\xc3\x97")), KButton::Look::Round),
              addBass (p.assets, "+  bass line"), addLead (p.assets, "+  lead line")
        {
            setComponentID ("section" + String (idx));
            for (Component* c : { (Component*) &bars, (Component*) &perChord, (Component*) &playBtn, (Component*) &dawBtn, (Component*) &removeBtn })
                addAndMakeVisible (c);
            addChildComponent (addBass);
            addChildComponent (addLead);
            addBass.setTooltip ("Add a bass row to this section (filled from the chords' roots to start you off)");
            addLead.setTooltip ("Add a lead row to this section for a melody or riff");
            addBass.onClick = [this] { page.song.setLineEnabled (index, sm::Lane::Bass, true); };
            addLead.onClick = [this] { page.song.setLineEnabled (index, sm::Lane::Lead, true); };

            bars.fontSize = 16.5f;
            perChord.fontSize = 16.5f;
            bars.setTooltip ("Length: how many bars this section is (this is what lands in your DAW)");
            perChord.setTooltip ("How long each chord lasts. The chords repeat until the section is full; SPREAD shares the bars between them.");
            bars.buildMenu = [this] (PopupMenu& m)
            {
                for (int b : { 1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64 })
                    m.addItem (b, String (b) + (b == 1 ? " bar" : " bars"), true, section().bars == b);
            };
            bars.onResult = [this] (int r) { page.song.setSectionBars (index, r); };
            perChord.buildMenu = [this] (PopupMenu& m)
            {
                const float cur = section().barsPerChord;
                m.addItem (1, "Half a bar per chord", true, std::abs (cur - 0.5f) < 0.01f);
                m.addItem (2, "1 bar per chord", true, std::abs (cur - 1.0f) < 0.01f);
                m.addItem (3, "2 bars per chord", true, std::abs (cur - 2.0f) < 0.01f);
                m.addItem (4, "4 bars per chord", true, std::abs (cur - 4.0f) < 0.01f);
                m.addItem (5, "Spread evenly over the section", true, cur <= 0.0f);
            };
            perChord.onResult = [this] (int r)
            {
                const float v[] = { 0.5f, 1.0f, 2.0f, 4.0f, 0.0f };
                page.song.setSectionBarsPerChord (index, v[r - 1]);
            };
            playBtn.fontSize = 16.0f;
            playBtn.setTooltip ("Play this section");
            playBtn.onClick = [this] { if (page.playingSection == index) page.stopPlayback(); else page.playSong (index); };
            dawBtn.setTooltip ("Drag this onto your DAW to drop just this section as MIDI (its bass and lead rows come as their own tracks)");
            dawBtn.addMouseListener (this, false);
            removeBtn.fontSize = 22.0f;
            removeBtn.setTooltip ("Delete this section");
            removeBtn.onClick = [this] { page.song.removeSection (index); };
            setRepaintsOnMouseActivity (false);
        }

        bool valid() const { return isPositiveAndBelow (index, (int) page.song.sections.size()); }
        const sm::Section& section() const
        {
            static const sm::Section none;
            return valid() ? page.song.sections[(size_t) index] : none;   // rows are rebuilt asynchronously after edits
        }

        static int chipsEnd (int numChords, int width)
        {
            const int perRow = jmax (1, (width - 16) / (chipW + chipGap));
            const int rowsNeeded = (numChords + 1 + perRow - 1) / perRow;
            return headerH + 8 + rowsNeeded * (chipH + chipGap);
        }

        static int heightFor (const sm::Section& s, int width, bool showLines)
        {
            int h = chipsEnd ((int) s.chords.size(), width);
            if (showLines)
            {
                if (s.bass.enabled) h += LaneView::heightFor();
                if (s.lead.enabled) h += LaneView::heightFor();
                if (! s.bass.enabled || ! s.lead.enabled) h += addRowH;
            }
            return h + 8;
        }

        // create / remove the bass and lead rows to match the song
        void syncLanes()
        {
            const bool show = page.song.showLines && valid();
            auto syncOne = [this, show] (std::unique_ptr<LaneView>& view, sm::Lane lane)
            {
                const bool want = show && (lane == sm::Lane::Bass ? section().bass.enabled : section().lead.enabled);
                if (want && view == nullptr)
                {
                    view = std::make_unique<LaneView> (page, index, lane);
                    addAndMakeVisible (*view);
                }
                else if (! want && view != nullptr)
                    view.reset();
                if (view != nullptr) view->sync();
            };
            syncOne (bassView, sm::Lane::Bass);
            syncOne (leadView, sm::Lane::Lead);
            addBass.setVisible (show && ! section().bass.enabled);
            addLead.setVisible (show && ! section().lead.enabled);
        }

        void repaintLanes()
        {
            if (bassView != nullptr) bassView->repaint();
            if (leadView != nullptr) leadView->repaint();
        }

        Rectangle<float> chipRect (int i) const
        {
            const int perRow = jmax (1, (getWidth() - 16) / (chipW + chipGap));
            return { 8.0f + (float) ((i % perRow) * (chipW + chipGap)), (float) (headerH + 8 + (i / perRow) * (chipH + chipGap)), (float) chipW, (float) chipH };
        }

        int chipAt (Point<float> p) const
        {
            for (int i = 0; i < (int) section().chords.size(); ++i) if (chipRect (i).contains (p)) return i;
            return -1;
        }

        int insertPosFor (Point<float> p) const
        {
            const int n = (int) section().chords.size();
            for (int i = 0; i < n; ++i)
            {
                auto r = chipRect (i);
                if (p.y < r.getBottom() + chipGap * 0.5f && p.x < r.getCentreX() && p.y > r.getY() - chipGap) return i;
            }
            return n;
        }

        void resized() override
        {
            // line 1: name and the round buttons; line 2: LENGTH and PER CHORD
            auto hd = getLocalBounds().removeFromTop (headerH).reduced (7, 5);
            auto h = hd.removeFromTop (38);
            auto round = [&h] { return h.removeFromRight (32).withSizeKeepingCentre (32, 32); };
            removeBtn.setBounds (round());
            h.removeFromRight (3);
            dawBtn.setBounds (round());
            h.removeFromRight (3);
            playBtn.setBounds (round());
            h.removeFromRight (6);
            nameArea = h;
            hd.removeFromTop (2);
            auto line2 = hd.removeFromTop (34).withTrimmedLeft (4);
            bars.setBounds (line2.removeFromLeft (112));
            line2.removeFromLeft (6);
            perChord.setBounds (line2.removeFromLeft (jmin (150, line2.getWidth())));
            if (nameEditor != nullptr)
                nameEditor->setBounds (nameArea.withTrimmedLeft (18).withSizeKeepingCentre (nameArea.getWidth() - 18, 30));

            // bass / lead rows under the chords, then the buttons that add the missing ones
            int y = chipsEnd ((int) section().chords.size(), getWidth());
            for (auto* v : { bassView.get(), leadView.get() })
                if (v != nullptr)
                {
                    v->setBounds (8, y, getWidth() - 16, LaneView::heightFor());
                    y += LaneView::heightFor();
                }
            auto addRow = Rectangle<int> (8, y + 2, getWidth() - 16, addRowH - 6);
            for (auto* b : { (Button*) &addBass, (Button*) &addLead })
                if (b->isVisible())
                {
                    b->setBounds (addRow.removeFromLeft (128));
                    addRow.removeFromLeft (6);
                }
        }

        void paint (Graphics& g) override
        {
            if (! valid()) return;
            const auto& s = section();
            auto r = getLocalBounds().toFloat().reduced (2.0f, 2.0f);
            const bool selected = page.song.selectedSection == index;
            const bool playing = page.playingSection == index;

            r = r.withTrimmedBottom (2.0f).withTrimmedRight (1.0f);
            if (selected)
            {
                Path gp;
                gp.addRoundedRectangle (r, 9.0f);
                DropShadow (Palette::gold.withAlpha (0.9f), 9, {}).drawForPath (g, gp);
            }
            paintCard (g, page.assets, r, 9.0f, Colour (0xfffaefd4), Colour (0xffe8d1a2),
                       selected ? Palette::goldDark : Colour (0xff7a5a2c), selected ? 2.4f : 1.1f, selected ? 0.6f : 1.0f);
            if (dropHighlight)
            {
                g.setColour (Palette::blue.withAlpha (0.12f));
                g.fillRoundedRectangle (r, 9.0f);
            }

            // header band with an engraved rule under it
            auto hr = r.reduced (1.5f).withHeight ((float) headerH - 3.0f);
            {
                Graphics::ScopedSaveState save (g);
                Path hp;
                hp.addRoundedRectangle (hr.getX(), hr.getY(), hr.getWidth(), hr.getHeight(), 8.0f, 8.0f, true, true, false, false);
                g.reduceClipRegion (hp);
                ColourGradient band (Colour (0xff6b4520).withAlpha (selected ? 0.22f : 0.13f), hr.getX(), hr.getY(),
                                     Colour (0xff6b4520).withAlpha (0.05f), hr.getX(), hr.getBottom(), false);
                g.setGradientFill (band);
                g.fillRect (hr);
            }
            g.setColour (Colour (0xff6b4520).withAlpha (0.45f));
            g.fillRect (hr.getX() + 6.0f, hr.getBottom(), hr.getWidth() - 12.0f, 1.0f);
            g.setColour (Colours::white.withAlpha (0.5f));
            g.fillRect (hr.getX() + 6.0f, hr.getBottom() + 1.0f, hr.getWidth() - 12.0f, 1.0f);

            auto nameR = nameArea.toFloat().withTrimmedLeft (4.0f);
            g.setColour (playing ? chipBlue : panelInk);
            Font nf = page.assets.bask (23.0f);
            while (textWidth (nf, s.name) > nameR.getWidth() && nf.getHeight() > 12.0f) nf = nf.withHeight (nf.getHeight() - 1.0f);
            g.setFont (nf);
            if (nameEditor == nullptr)
                g.drawText (s.name, nameR, Justification::centredLeft, false);

            bars.text = String (s.bars) + (s.bars == 1 ? " BAR" : " BARS");
            perChord.text = s.barsPerChord <= 0.0f ? String ("SPREAD OUT")
                          : (s.barsPerChord < 1.0f ? String (CharPointer_UTF8 ("\xc2\xbd BAR EACH"))
                                                   : String ((int) s.barsPerChord) + ((int) s.barsPerChord == 1 ? " BAR EACH" : " BARS EACH"));
            playBtn.lit = playing;

            // chord chips
            for (int i = 0; i < (int) s.chords.size(); ++i)
            {
                auto cr = chipRect (i);
                const bool isPlaying = playing && page.playingChord == i;
                if (isPlaying)
                {
                    Path p;
                    p.addRoundedRectangle (cr, 7.0f);
                    DropShadow (Palette::blue, 10, {}).drawForPath (g, p);
                }
                paintCard (g, page.assets, cr, 6.5f, isPlaying ? Colour (0xffeef8ff) : Colour (0xfffff8e8),
                           isPlaying ? Colour (0xffc4e4ff) : Colour (0xffead6ae),
                           isPlaying ? Palette::blue : Colour (0xff7a5a2c), isPlaying ? 2.0f : 1.0f, isPlaying ? 0.0f : 0.8f, 0.15f);
                Font cf = page.assets.bask (22.0f);
                const String name = page.chordText (s.chords[(size_t) i]);
                while (textWidth (cf, name) > cr.getWidth() - 6.0f && cf.getHeight() > 11.0f) cf = cf.withHeight (cf.getHeight() - 1.0f);
                g.setColour (isPlaying ? chipBlue : panelInk);
                g.setFont (cf);
                g.drawText (name, cr, Justification::centred, false);
                if (insertIndex == i)
                {
                    g.setColour (Palette::blue);
                    g.fillRect (cr.getX() - 4.0f, cr.getY() + 2.0f, 3.0f, cr.getHeight() - 4.0f);
                }
            }
            auto plus = chipRect ((int) s.chords.size());
            const float dashes[] = { 5.0f, 4.0f };
            Path pp, dp;
            pp.addRoundedRectangle (plus.reduced (1.0f), 7.0f);
            PathStrokeType (1.2f).createDashedStroke (dp, pp, dashes, 2);
            g.setColour (Colour (0xff8a6a3c).withAlpha (0.7f));
            g.fillPath (dp);
            g.setColour (Palette::inkSoft);
            g.setFont (page.assets.bask (26.0f));
            g.drawText ("+", plus, Justification::centred, false);
            if (insertIndex == (int) s.chords.size())
            {
                g.setColour (Palette::blue);
                g.fillRect (plus.getX() - 4.0f, plus.getY() + 2.0f, 3.0f, plus.getHeight() - 4.0f);
            }
            if (s.chords.empty())
            {
                g.setColour (Palette::inkSoft.withAlpha (0.85f));
                g.setFont (page.assets.baskItalic (16.5f));
                g.drawFittedText ("drop chords or a progression here", Rectangle<float> (plus.getRight() + 8.0f, plus.getY(), r.getRight() - plus.getRight() - 12.0f, plus.getHeight()).toNearestInt(),
                            Justification::centredLeft, 2, 0.9f);
            }
        }

        void mouseDown (const MouseEvent& e) override
        {
            pressedChip = -1;
            dragStarted = false;
            hold.cancel();
            if (e.eventComponent == &dawBtn || ! valid()) return;
            if (isTouchUI() && ! e.mods.isPopupMenu())
            {
                // a touch: a swipe scrolls the song, a tap acts when the finger lifts, press and hold opens the menu
                const auto p = e.position;
                hold.onLongPress = [this, p]
                {
                    if (! valid()) return;
                    page.song.selectedSection = index;
                    if (const int chip = chipAt (p); chip >= 0)
                    {
                        page.focusChord (section().chords[(size_t) chip], false);
                        showChipMenu (chip);
                    }
                    else
                        showSectionMenu();
                    page.song.changed();
                };
                hold.mouseDown (e);
                return;
            }
            press (e);
        }

        void mouseUp (const MouseEvent& e) override
        {
            hold.mouseUp();
            if (! isTouchUI() || hold.fired() || e.getDistanceFromDragStart() > 8) return;
            if (e.eventComponent == &dawBtn) { page.shareMidi (index); return; }
            if (valid()) press (e);      // a tap
        }

        // what a click (or a tap) does
        void press (const MouseEvent& e)
        {
            page.takeFocus();

            page.song.selectedSection = index;
            const auto& s = section();
            const auto p = e.position;

            if (nameArea.contains (e.getPosition()))
            {
                grabbedHeader = true;
                if (e.mods.isPopupMenu()) showSectionMenu();
                else if (e.getNumberOfClicks() > 1) startRename();
                page.song.changed();
                return;
            }
            grabbedHeader = false;

            const int chip = chipAt (p);
            if (chip >= 0)
            {
                pressedChip = chip;
                if (e.mods.isPopupMenu()) showChipMenu (chip);
                else page.focusChord (s.chords[(size_t) chip]);
                page.song.changed();
                return;
            }
            if (chipRect ((int) s.chords.size()).contains (p))
            {
                showAddMenu();
                return;
            }
            page.song.changed();
        }

        void mouseDrag (const MouseEvent& e) override
        {
            hold.mouseDrag (e);
            if (isTouchUI()) return;         // on a touch screen a swipe scrolls the song (the menus move things)
            if (dragStarted || e.getDistanceFromDragStart() < 6) return;

            if (e.eventComponent == &dawBtn)
            {
                dragStarted = true;
                const auto f = page.midiFile (index);
                if (f.existsAsFile())
                    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
                return;
            }
            if (grabbedHeader)
            {
                dragStarted = true;
                auto d = makeDescription ("section");
                d.getDynamicObject()->setProperty ("index", index);
                startChipDrag (this, page.assets, d, section().name);
                return;
            }
            if (pressedChip >= 0 && pressedChip < (int) section().chords.size())
            {
                dragStarted = true;
                auto d = makeDescription ("chord");
                d.getDynamicObject()->setProperty ("chords", String (section().chords[(size_t) pressedChip].id()));
                d.getDynamicObject()->setProperty ("from", "section");
                d.getDynamicObject()->setProperty ("section", index);
                d.getDynamicObject()->setProperty ("index", pressedChip);
                startChipDrag (this, page.assets, d, page.chordText (section().chords[(size_t) pressedChip]));
            }
        }

        void showAddMenu()
        {
            PopupMenu m;
            m.setLookAndFeel (&getLookAndFeel());
            if (! page.song.tray.empty())
            {
                m.addItem (1, "Add all progression chords (" + String ((int) page.song.tray.size()) + ")");
                if (page.focused.isValid()) m.addItem (2, "Add " + page.chordText (page.focused));
                m.addSeparator();
            }
            auto chords = std::make_shared<std::vector<sm::Chord>>();
            PopupMenu keyMenu;
            page.fillChordMenu (keyMenu, 1000, *chords);
            m.addSubMenu ("Key chords", keyMenu);
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, chords, safe = SafePointer<SectionRow> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 if (r == 1) page.song.insertChords (index, page.song.tray);
                                 else if (r == 2) page.song.insertChords (index, { page.focused });
                                 else if (const int i = SongMapperPage::chordMenuIndex (r, 1000, *chords); i >= 0) page.song.insertChords (index, { (*chords)[(size_t) i] });
                             });
        }

        void showChipMenu (int chip)
        {
            PopupMenu m, change;
            m.setLookAndFeel (&getLookAndFeel());
            auto chords = std::make_shared<std::vector<sm::Chord>>();
            page.fillChordMenu (change, 1000, *chords);
            const int count = (int) section().chords.size();
            m.addItem (1, "Play");
            m.addSubMenu ("Change to", change);
            m.addItem (2, "Duplicate");
            m.addItem (3, "Copy to progression");
            m.addItem (5, String (CharPointer_UTF8 ("Move left  \xe2\x97\x82")), chip > 0);
            m.addItem (6, String (CharPointer_UTF8 ("Move right  \xe2\x96\xb8")), chip < count - 1);
            m.addSeparator();
            m.addItem (4, "Remove");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, chip, chords, safe = SafePointer<SectionRow> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0 || chip >= (int) section().chords.size()) return;
                                 const auto c = section().chords[(size_t) chip];
                                 if (r == 5) { page.song.moveChord (index, chip, index, chip - 1); return; }
                                 if (r == 6) { page.song.moveChord (index, chip, index, chip + 2); return; }     // an insertion point
                                 if (r == 1) page.auditionChord (c);
                                 else if (r == 2) page.song.insertChords (index, { c }, chip + 1);
                                 else if (r == 3) page.song.insertTray (c);
                                 else if (r == 4) page.song.removeChord (index, chip);
                                 else if (const int i = SongMapperPage::chordMenuIndex (r, 1000, *chords); i >= 0) page.song.replaceChord (index, chip, (*chords)[(size_t) i]);
                             });
        }

        void showSectionMenu()
        {
            PopupMenu m, rename;
            m.setLookAndFeel (&getLookAndFeel());
            const char* names[] = { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Hook", "Bridge", "Solo", "Breakdown", "Build", "Drop", "Interlude", "Outro" };
            for (int i = 0; i < 13; ++i) rename.addItem (100 + i, names[i]);
            m.addItem (6, String (CharPointer_UTF8 ("Rename\xe2\x80\xa6")));
            m.addSubMenu ("Rename to", rename);
            m.addItem (1, "Duplicate section");
            m.addItem (2, "Move up", index > 0);
            m.addItem (3, "Move down", index < (int) page.song.sections.size() - 1);
            m.addItem (4, "Clear chords", ! section().chords.empty());
            m.addItem (9, "Practice these chords (big diagrams)", ! section().chords.empty());
            m.addSeparator();
            m.addItem (7, "Bass line", true, section().bass.enabled);
            m.addItem (8, "Lead line", true, section().lead.enabled);
            m.addSeparator();
            m.addItem (5, "Delete section");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, safe = SafePointer<SectionRow> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 const char* nm[] = { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Hook", "Bridge", "Solo", "Breakdown", "Build", "Drop", "Interlude", "Outro" };
                                 if (r >= 100) page.song.renameSection (index, page.song.nextSectionName (nm[r - 100]));
                                 else if (r == 1) page.song.duplicateSection (index);
                                 else if (r == 2) page.song.moveSection (index, index - 1);
                                 else if (r == 3) page.song.moveSection (index, index + 1);
                                 else if (r == 4) page.song.clearSection (index);
                                 else if (r == 5) page.song.removeSection (index);
                                 else if (r == 6) startRename();
                                 else if (r == 9) page.showPractice (index);
                                 else if (r == 7 || r == 8)
                                 {
                                     const auto lane = r == 7 ? sm::Lane::Bass : sm::Lane::Lead;
                                     if (! page.song.showLines) { page.song.showLines = true; page.song.setLineEnabled (index, lane, true); page.song.changed(); }
                                     else page.song.setLineEnabled (index, lane, ! page.song.line (index, lane).enabled);
                                 }
                             });
        }

        // drag & drop
        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            const auto t = d.description["type"].toString();
            return t == "chord" || t == "progression" || t == "section";
        }
        void itemDragEnter (const SourceDetails&) override { dropHighlight = true; repaint(); }
        void itemDragMove (const SourceDetails& d) override
        {
            if (auto* vp = findParentComponentOfClass<Viewport>())
            {
                const auto vpPos = vp->getLocalPoint (this, d.localPosition);
                vp->autoScroll (vpPos.x, vpPos.y, 40, 10);
            }
            const int pos = d.description["type"].toString() == "section" ? -1 : insertPosFor (d.localPosition.toFloat());
            if (pos != insertIndex) { insertIndex = pos; repaint(); }
        }
        void itemDragExit (const SourceDetails&) override { dropHighlight = false; insertIndex = -1; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            dropHighlight = false;
            const int at = insertIndex < 0 ? (int) section().chords.size() : insertIndex;
            insertIndex = -1;
            const auto type = d.description["type"].toString();
            if (type == "section")
            {
                const int from = (int) d.description["index"];
                int to = index;
                if (d.localPosition.y > getHeight() / 2 && from > index) ++to;
                if (d.localPosition.y < getHeight() / 2 && from < index) --to;
                page.song.moveSection (from, to);
                return;
            }
            if (type == "chord" && d.description["from"].toString() == "section")
            {
                page.song.moveChord ((int) d.description["section"], (int) d.description["index"], index, at);
                return;
            }
            page.song.insertChords (index, chordsFromVar (d.description), at);
        }

        void startRename()
        {
            nameEditor = std::make_unique<TextEditor>();
            auto& ed = *nameEditor;
            ed.setFont (page.assets.bask (21.0f));
            ed.setColour (TextEditor::textColourId, Palette::cream);
            ed.setColour (TextEditor::highlightColourId, Palette::blue.withAlpha (0.45f));
            ed.setColour (CaretComponent::caretColourId, Palette::goldLight);
            ed.setIndents (6, 4);
            ed.setText (section().name, false);
            addAndMakeVisible (ed);
            resized();
            ed.selectAll();
            ed.grabKeyboardFocus();
            ed.onReturnKey = [this] { finishRename (true); };
            ed.onFocusLost = [this] { finishRename (true); };
            ed.onEscapeKey = [this] { finishRename (false); };
            repaint();
        }

        void finishRename (bool keep)
        {
            if (nameEditor == nullptr || renameDone) return;
            renameDone = true;
            const auto text = nameEditor->getText().trim();
            if (keep && text.isNotEmpty() && text != section().name)
                page.song.renameSection (index, text.substring (0, 40));
            // the editor is still inside its own callback: remove it afterwards
            MessageManager::callAsync ([safe = SafePointer<SectionRow> (this)]
            {
                if (safe == nullptr) return;
                safe->nameEditor.reset();
                safe->renameDone = false;
                safe->repaint();
            });
        }

        static constexpr int headerH = 86, chipW = 66, chipH = 40, chipGap = 6, addRowH = 36;

    private:
        SongMapperPage& page;
        int index;
        MenuBox bars, perChord;
        KButton playBtn;
        MidiDragButton dawBtn;
        KButton removeBtn;
        GhostButton addBass, addLead;
        std::unique_ptr<LaneView> bassView, leadView;
        Rectangle<int> nameArea;
        std::unique_ptr<TextEditor> nameEditor;
        bool renameDone = false;
        int pressedChip = -1, insertIndex = -1;
        bool dragStarted = false, grabbedHeader = false, dropHighlight = false;
        LongPress hold;
    };

    class SectionList : public Component, public DragAndDropTarget
    {
    public:
        explicit SectionList (SongMapperPage& p) : page (p), addSection (p.assets, "+   add a section")
        {
            addSection.fontSize = 19.0f;
            addSection.setTooltip ("Add a song section (Intro, Verse, Chorus...)");
            addSection.onClick = [this] { if (onAddSection) onAddSection (&addSection); };
            addAndMakeVisible (addSection);
        }

        std::function<void (Component*)> onAddSection;

        // dropping below the last section makes a new section (or moves a section to the end)
        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            const auto t = d.description["type"].toString();
            return t == "chord" || t == "progression" || t == "section";
        }
        void itemDragEnter (const SourceDetails&) override { dropHint = true; repaint(); }
        void itemDragExit (const SourceDetails&) override  { dropHint = false; repaint(); }
        void itemDragMove (const SourceDetails& d) override
        {
            if (auto* vp = findParentComponentOfClass<Viewport>())
            {
                const auto vpPos = vp->getLocalPoint (this, d.localPosition);
                vp->autoScroll (vpPos.x, vpPos.y, 40, 10);
            }
        }
        void itemDropped (const SourceDetails& d) override
        {
            dropHint = false;
            repaint();
            const auto t = d.description["type"].toString();
            if (t == "section")
            {
                page.song.moveSection ((int) d.description["index"], (int) page.song.sections.size() - 1);
                return;
            }
            const auto chords = chordsFromVar (d.description);
            if (chords.empty()) return;
            if (t == "chord" && d.description["from"].toString() == "section")
            {
                const int from = (int) d.description["section"];
                const int idx = page.song.addSection (page.song.nextSectionName ("Verse"), 8);
                page.song.moveChord (from, (int) d.description["index"], idx, 0);
                return;
            }
            const int idx = page.song.addSection (page.song.nextSectionName ("Verse"), 8);
            page.song.insertChords (idx, chords);
            page.song.selectedSection = idx;
        }

        void paint (Graphics& g) override
        {
            if (! dropHint) return;
            auto r = getLocalBounds().toFloat().withTop ((float) contentBottom).reduced (2.0f, 4.0f);
            if (r.getHeight() < 20.0f) return;
            r = r.withHeight (jmin (r.getHeight(), 80.0f));
            const float dashes[] = { 6.0f, 5.0f };
            Path p, dp;
            p.addRoundedRectangle (r, 9.0f);
            PathStrokeType (1.6f).createDashedStroke (dp, p, dashes, 2);
            g.setColour (Palette::blue.withAlpha (0.1f));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (Palette::blueDeep);
            g.fillPath (dp);
            g.setFont (page.assets.baskItalic (18.0f));
            g.drawText ("drop here to make a new section", r, Justification::centred, false);
        }

        void rebuild (int width)
        {
            const int n = (int) page.song.sections.size();
            if ((int) rows.size() != n)
            {
                rows.clear();
                for (int i = 0; i < n; ++i)
                {
                    rows.push_back (std::make_unique<SectionRow> (page, i));
                    addAndMakeVisible (*rows.back());
                }
            }
            int y = 0;
            for (int i = 0; i < n; ++i)
            {
                const int h = SectionRow::heightFor (page.song.sections[(size_t) i], width, page.song.showLines);
                rows[(size_t) i]->syncLanes();
                rows[(size_t) i]->setBounds (0, y, width, h);
                rows[(size_t) i]->resized();
                rows[(size_t) i]->repaint();
                y += h + 6;
            }
            addSection.setBounds (2, y + 2, width - 4, 46);
            y += 54;
            contentBottom = y;
            int minHeight = 10;
            if (auto* vp = findParentComponentOfClass<Viewport>()) minHeight = vp->getMaximumVisibleHeight();
            setSize (width, jmax (y + 90, minHeight));    // room below the last section to drop a new one
            repaint();
        }

        void repaintRows() { for (auto& r : rows) r->repaint(); }
        int rowTop (int section) const { return isPositiveAndBelow (section, (int) rows.size()) ? rows[(size_t) section]->getY() : 0; }
        void repaintLanes (int section)
        {
            if (isPositiveAndBelow (section, (int) rows.size())) rows[(size_t) section]->repaintLanes();
        }

    private:
        SongMapperPage& page;
        std::vector<std::unique_ptr<SectionRow>> rows;
        GhostButton addSection;
        int contentBottom = 0;
        bool dropHint = false;
    };

    // One part of the song (chords, bass or lead): click to mute it, drag it into the DAW for that part of the whole song
    class LanePill : public Component, public SettableTooltipClient
    {
    public:
        LanePill (SongMapperPage& p, sm::Lane l) : page (p), lane (l)
        {
            setMouseCursor (MouseCursor::PointingHandCursor);
            const String part = lane == sm::Lane::Chords ? "chords" : (lane == sm::Lane::Bass ? "bass line" : "lead line");
            setTooltip ("Click to mute or unmute the " + part + " when you play the song. "
                        "Drag it onto a track in your DAW to drop the " + part + " of the whole song as MIDI.");
        }

        bool muted() const
        {
            return lane == sm::Lane::Chords ? page.song.muteChords : (lane == sm::Lane::Bass ? page.song.muteBass : page.song.muteLead);
        }

        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.5f);
            const bool off = muted();
            const bool hidden = lane != sm::Lane::Chords && ! page.song.showLines;
            const bool empty = ! page.song.laneHasNotes (-1, lane);
            ColourGradient body (Colour (0xff2e2621), r.getX(), r.getY(), Colour (0xff0b0806), r.getX(), r.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (r, 7.0f);
            if (isMouseOver() || down)
            {
                g.setColour (down ? Colours::black.withAlpha (0.25f) : Colours::white.withAlpha (0.07f));
                g.fillRoundedRectangle (r, 7.0f);
            }
            ColourGradient border (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, r.getX(), r.getBottom(), false);
            g.setGradientFill (border);
            g.drawRoundedRectangle (r, 7.0f, 1.8f);

            // lamp: lit blue when this part plays
            const float lr = 5.0f;
            const auto lc = Point<float> (r.getX() + 15.0f, r.getCentreY());
            const bool lit = ! off && ! hidden;
            if (lit)
            {
                Path glow;
                glow.addEllipse (lc.x - lr, lc.y - lr, lr * 2, lr * 2);
                DropShadow (Palette::blue, 7, {}).drawForPath (g, glow);
            }
            ColourGradient lamp (lit ? Colour (0xffd8f3ff) : Colour (0xff4b3e32), lc.x - 1.5f, lc.y - 2.0f,
                                 lit ? Colour (0xff1c6fd0) : Colour (0xff120c07), lc.x + lr, lc.y + lr, true);
            g.setGradientFill (lamp);
            g.fillEllipse (lc.x - lr, lc.y - lr, lr * 2, lr * 2);
            g.setColour (Palette::goldDark);
            g.drawEllipse (lc.x - lr, lc.y - lr, lr * 2, lr * 2, 1.0f);

            // drag arrow on the right
            const float ax = r.getRight() - 15.0f, ay = r.getCentreY();
            Path arrow;
            arrow.startNewSubPath (ax, ay - 5.0f);
            arrow.lineTo (ax, ay + 2.0f);
            arrow.startNewSubPath (ax - 3.5f, ay - 1.0f);
            arrow.lineTo (ax, ay + 2.5f);
            arrow.lineTo (ax + 3.5f, ay - 1.0f);
            arrow.startNewSubPath (ax - 5.0f, ay + 2.5f);
            arrow.lineTo (ax - 5.0f, ay + 5.0f);
            arrow.lineTo (ax + 5.0f, ay + 5.0f);
            arrow.lineTo (ax + 5.0f, ay + 2.5f);
            g.setColour (Palette::gold.withAlpha (empty ? 0.4f : 0.9f));
            g.strokePath (arrow, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));

            const String name = lane == sm::Lane::Chords ? "CHORDS" : (lane == sm::Lane::Bass ? "BASS" : "LEAD");
            g.setColour (lit ? enamelText : enamelText.withAlpha (0.45f));
            g.setFont (page.assets.cinzel (18.0f));
            g.drawFittedText (name, r.withTrimmedLeft (24.0f).withTrimmedRight (22.0f).toNearestInt(), Justification::centred, 1, 0.6f);
            if (off)
            {
                g.setColour (Colour (0xffd0583c).withAlpha (0.85f));
                const auto t = r.withTrimmedLeft (24.0f).withTrimmedRight (22.0f);
                g.fillRect (t.getX(), t.getCentreY() - 0.6f, t.getWidth(), 1.3f);
            }
        }

        void mouseEnter (const MouseEvent&) override { repaint(); }
        void mouseExit (const MouseEvent&) override  { repaint(); }
        void mouseDown (const MouseEvent& e) override
        {
            down = true;
            dragged = false;
            repaint();
            // touch screens: press and hold shares this part of the song as MIDI (a tap mutes it)
            hold.onLongPress = [this] { down = false; repaint(); page.shareMidi (-1, sm::laneBit (lane)); };
            hold.mouseDown (e);
        }
        void mouseDrag (const MouseEvent& e) override
        {
            hold.mouseDrag (e);
            if (isTouchUI()) return;
            if (dragged || e.getDistanceFromDragStart() < 6) return;
            dragged = true;
            down = false;
            repaint();
            const auto f = page.midiFile (-1, sm::laneBit (lane));
            if (f.existsAsFile())
                DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        }
        void mouseUp (const MouseEvent& e) override
        {
            hold.mouseUp();
            down = false;
            repaint();
            if (dragged || hold.fired() || ! getLocalBounds().contains (e.getPosition())) return;
            auto& m = lane == sm::Lane::Chords ? page.song.muteChords : (lane == sm::Lane::Bass ? page.song.muteBass : page.song.muteLead);
            m = ! m;
            page.song.changed();
        }

    private:
        SongMapperPage& page;
        sm::Lane lane;
        bool down = false, dragged = false;
        LongPress hold;
    };

    class TimelinePanel : public Component
    {
    public:
        explicit TimelinePanel (SongMapperPage& p)
            : page (p), list (p),
              playBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xb6 PLAY SONG"))), stopBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xa0 STOP"))),
              loopBtn (p.assets, "LOOP"), linesBtn (p.assets, "LINES"),
              chordsPill (p, sm::Lane::Chords), bassPill (p, sm::Lane::Bass), leadPill (p, sm::Lane::Lead)
        {
            viewport.setViewedComponent (&list, false);
            viewport.setComponentID ("timelineViewport");
            viewport.setScrollBarsShown (true, false);
            viewport.setScrollBarThickness (10);
            viewport.setScrollOnDragMode (isTouchUI() ? Viewport::ScrollOnDragMode::all : Viewport::ScrollOnDragMode::nonHover);
            for (Component* c : { (Component*) &viewport, (Component*) &playBtn, (Component*) &stopBtn, (Component*) &loopBtn, (Component*) &linesBtn,
                                  (Component*) &chordsPill, (Component*) &bassPill, (Component*) &leadPill })
                addAndMakeVisible (c);
            list.onAddSection = [this] (Component* from) { showAddSectionMenu (from); };

            playBtn.setTooltip ("Play the whole song in order");
            playBtn.onClick = [this] { page.playSong (-1); };
            stopBtn.onClick = [this] { page.stopPlayback(); };
            loopBtn.setTooltip ("Loop playback");
            loopBtn.onClick = [this] { page.song.loopPlayback = ! page.song.loopPlayback; page.song.changed(); };
            linesBtn.setTooltip ("Show or hide the Bass and Lead rows. While they are hidden, only the chords are played and dragged.");
            linesBtn.onClick = [this] { page.song.showLines = ! page.song.showLines; page.song.changed(); };
        }

        void showAddSectionMenu (Component* target = nullptr)
        {
            PopupMenu m;
            m.setLookAndFeel (&getLookAndFeel());
            const char* names[] = { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Hook", "Bridge", "Solo", "Breakdown", "Build", "Drop", "Interlude", "Outro" };
            for (int i = 0; i < 13; ++i) m.addItem (i + 1, names[i]);
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (target != nullptr ? target->getScreenBounds() : getScreenBounds()).withStandardItemHeight (menuItemHeight()),
                             [this, safe = SafePointer<TimelinePanel> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 const char* nm[] = { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Hook", "Bridge", "Solo", "Breakdown", "Build", "Drop", "Interlude", "Outro" };
                                 const int bars = (r == 1 || r == 13) ? 4 : 8;
                                 page.song.addSection (page.song.nextSectionName (nm[r - 1]), bars);
                                 refresh();
                                 viewport.setViewPosition (0, list.getHeight());
                             });
        }

        void resized() override
        {
            auto r = panelContent (getLocalBounds());
            auto t = r.removeFromTop (40);
            const int unit = (t.getWidth() - 15) / 20;
            playBtn.setBounds (t.removeFromLeft (unit * 7));
            t.removeFromLeft (5);
            stopBtn.setBounds (t.removeFromLeft (unit * 5));
            t.removeFromLeft (5);
            loopBtn.setBounds (t.removeFromLeft (unit * 4));
            t.removeFromLeft (5);
            linesBtn.setBounds (t);
            for (auto* b : { &playBtn, &stopBtn, &loopBtn, &linesBtn }) b->fontSize = 18.0f;
            r.removeFromTop (8);
            auto f = r.removeFromBottom (40);
            const int pw = (f.getWidth() - 10) / 3;
            chordsPill.setBounds (f.removeFromLeft (pw + 6));
            f.removeFromLeft (5);
            bassPill.setBounds (f.removeFromLeft (pw - 3));
            f.removeFromLeft (5);
            leadPill.setBounds (f);
            r.removeFromBottom (8);
            viewport.setBounds (r);
            refresh();
        }

        void showSection (int index)
        {
            viewport.setViewPosition (0, list.rowTop (index));
        }

        void refresh()
        {
            list.rebuild (viewport.getWidth() - viewport.getScrollBarThickness() - 2);
            for (auto* pill : { &chordsPill, &bassPill, &leadPill }) pill->repaint();
            repaint();
        }

        void paint (Graphics& g) override
        {
            if (page.isMobileLayout()) paintFramedPanel (g, page.assets, getLocalBounds().toFloat(), page.getMobileLayout().hideTitles ? String() : String ("SONG STRUCTURE & CHORD TIMELINE"));
            loopBtn.lit = page.song.loopPlayback;
            linesBtn.lit = page.song.showLines;
            playBtn.lit = page.isPlaying() && page.playScope == -1;

            if (page.song.sections.empty())
            {
                g.setColour (Palette::inkSoft);
                g.setFont (page.assets.baskItalic (19.0f));
                g.drawFittedText ("No sections yet. Click \"+ add a section\", or drop a progression here.", viewport.getBounds().reduced (20).withTrimmedTop (60), Justification::centredTop, 3);
            }
        }

    private:
        SongMapperPage& page;
    public:
        SectionList list;
        Viewport viewport;
    private:
        KButton playBtn, stopBtn, loopBtn, linesBtn;
        LanePill chordsPill, bassPill, leadPill;
    };

    //==============================================================================
    // Right panel: progression browser
    //==============================================================================
    class ProgRow : public Component, public SettableTooltipClient
    {
    public:
        ProgRow (SongMapperPage& p, String nm, String rom, String gen, bool fav)
            : page (p), name (std::move (nm)), roman (std::move (rom)), genre (std::move (gen)), isFavouriteList (fav),
              playBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xb6")), KButton::Look::Round),
              starBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x98\x85")), KButton::Look::Round),
              useBtn (p.assets, "USE")
        {
            addAndMakeVisible (playBtn);
            addAndMakeVisible (starBtn);
            addAndMakeVisible (useBtn);
            playBtn.fontSize = 16.0f;
            starBtn.fontSize = 19.0f;
            useBtn.fontSize = 16.0f;
            playBtn.setTooltip ("Preview");
            starBtn.setTooltip ("Add to / remove from favourites");
            useBtn.setTooltip ("Load into the progression on the left");
            playBtn.onClick = [this] { page.playProgression (chords()); };
            starBtn.onClick = [this] { page.favourites->toggle ({ name, roman, genre }); page.song.changed(); };
            useBtn.onClick = [this]
            {
                const auto c = chords();
                page.song.setTray (c);
                if (! c.empty()) page.focusChord (c.front(), false);
            };
            setTooltip ("Drag onto a song section to add all its chords. Double-click adds them to the selected section; right-click for more.");
            setMouseCursor (MouseCursor::DraggingHandCursor);
        }

        std::vector<sm::Chord> chords() const { return sm::resolveProgression (sm::splitNumerals (roman.toStdString()), page.currentKey()); }

        void resized() override
        {
            // line 1: name, play, star - line 2: numerals - line 3: chords, USE
            auto r = getLocalBounds().reduced (9, 7);
            auto l1 = r.removeFromTop (32);
            starBtn.setBounds (l1.removeFromRight (32).withSizeKeepingCentre (32, 32));
            l1.removeFromRight (6);
            playBtn.setBounds (l1.removeFromRight (32).withSizeKeepingCentre (32, 32));
            useBtn.setBounds (r.removeFromBottom (30).removeFromRight (62).translated (0, 1));
        }

        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f).withTrimmedBottom (2.0f);
            paintCard (g, page.assets, r, 8.0f, Colour (0xfffaf0d6), Colour (0xffe9d4a8), Colour (0xff7a5a2c), 1.0f);
            starBtn.lit = page.favourites->contains (roman);

            auto text = r.reduced (8.0f, 5.0f);
            auto l1 = text.removeFromTop (32.0f).withTrimmedRight (78.0f).withTrimmedLeft (2.0f);
            auto l3 = text.removeFromBottom (30.0f).withTrimmedRight (68.0f).withTrimmedLeft (2.0f);
            auto l2 = text.withTrimmedLeft (2.0f);

            g.setColour (panelInk);
            g.setFont (page.assets.bask (21.0f));
            g.drawFittedText (name, l1.toNearestInt(), Justification::centredLeft, 1, 0.75f);

            g.setColour (Palette::inkSoft);
            Font rf = page.assets.baskItalic (17.5f);
            const String numerals = roman.replace ("b", CharPointer_UTF8 ("\xe2\x99\xad")).replace ("#", CharPointer_UTF8 ("\xe2\x99\xaf"))
                                         .replace (" ", CharPointer_UTF8 (" \xe2\x80\x93 "));
            while (textWidth (rf, numerals) > l2.getWidth() && rf.getHeight() > 10.0f) rf = rf.withHeight (rf.getHeight() - 0.5f);
            g.setFont (rf);
            g.drawText (numerals, l2, Justification::centredLeft, false);

            StringArray names;
            for (auto& c : chords()) names.add (page.chordText (c));
            g.setColour (chipBlue);
            Font cf = page.assets.bask (19.0f);
            const String line = names.joinIntoString (CharPointer_UTF8 (" \xc2\xb7 "));
            while (textWidth (cf, line) > l3.getWidth() && cf.getHeight() > 10.0f) cf = cf.withHeight (cf.getHeight() - 0.5f);
            g.setFont (cf);
            g.drawText (line, l3, Justification::centredLeft, false);
        }

        void mouseDown (const MouseEvent& e) override
        {
            dragStarted = false;
            page.takeFocus();
            if (e.mods.isPopupMenu()) { showMenu(); return; }
            hold.onLongPress = [this] { showMenu(); };     // touch screens: press and hold
            hold.mouseDown (e);
        }

        void mouseUp (const MouseEvent&) override { hold.mouseUp(); }

        void mouseDoubleClick (const MouseEvent&) override
        {
            page.addToSelectedSection (chords());
        }

        void showMenu()
        {
            PopupMenu m;
            m.setLookAndFeel (&getLookAndFeel());
            m.addItem (1, "Play");
            m.addItem (2, "Use as the progression (left panel)");
            m.addItem (3, "Add to the selected section");
            m.addItem (4, "Add as a new section");
            m.addSeparator();
            m.addItem (5, page.favourites->contains (roman) ? "Remove from favourites" : "Add to favourites");
            kcui::showMenu (m, getLookAndFeel(), PopupMenu::Options().withMousePosition().withStandardItemHeight (menuItemHeight()),
                             [this, safe = SafePointer<ProgRow> (this)] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 const auto c = chords();
                                 if (r == 1) page.playProgression (c);
                                 else if (r == 2) { page.song.setTray (c); if (! c.empty()) page.focusChord (c.front(), false); }
                                 else if (r == 3) page.addToSelectedSection (c);
                                 else if (r == 4)
                                 {
                                     const int idx = page.song.addSection (page.song.nextSectionName ("Verse"), 8);
                                     page.song.insertChords (idx, c);
                                 }
                                 else if (r == 5) { page.favourites->toggle ({ name, roman, genre }); page.song.changed(); }
                             });
        }
        void mouseDrag (const MouseEvent& e) override
        {
            hold.mouseDrag (e);
            if (isTouchUI()) return;         // a swipe scrolls the list on a touch screen (USE and the menu add it)
            if (dragStarted || e.getDistanceFromDragStart() < 6) return;
            dragStarted = true;
            auto d = makeDescription ("progression");
            d.getDynamicObject()->setProperty ("chords", chordsToVarString (chords()));
            StringArray names;
            for (auto& c : chords()) names.add (page.chordText (c));
            startChipDrag (this, page.assets, d, names.joinIntoString (" "));
        }

    private:
        SongMapperPage& page;
        String name, roman, genre;
        bool isFavouriteList;
        KButton playBtn, starBtn, useBtn;
        bool dragStarted = false;
        LongPress hold;
    };

    class BrowserPanel : public Component
    {
    public:
        explicit BrowserPanel (SongMapperPage& p) : page (p), genreBox (p.assets), styleBox (p.assets),
                                                    favBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x98\x85")), KButton::Look::Round)
        {
            addAndMakeVisible (genreBox);
            addAndMakeVisible (styleBox);
            addAndMakeVisible (favBtn);
            addAndMakeVisible (viewport);
            viewport.setViewedComponent (&content, false);
            viewport.setScrollBarsShown (true, false);
            viewport.setScrollBarThickness (10);
            viewport.setScrollOnDragMode (isTouchUI() ? Viewport::ScrollOnDragMode::all : Viewport::ScrollOnDragMode::nonHover);

            genreBox.fontSize = 22.0f;
            genreBox.setTooltip ("Genre");
            genreBox.buildMenu = [this] (PopupMenu& m)
            {
                const auto& lib = sm::library();
                for (int i = 0; i < (int) lib.size(); ++i)
                    m.addItem (i + 1, lib[(size_t) i].name, true, ! page.song.showFavourites && page.song.genre == i);
                m.addSeparator();
                m.addItem (1000, String (CharPointer_UTF8 ("\xe2\x98\x85 Favourites")), true, page.song.showFavourites);
            };
            genreBox.onResult = [this] (int r)
            {
                if (r == 1000) page.song.showFavourites = true;
                else { page.song.showFavourites = false; page.song.genre = r - 1; page.song.genreStyle = 0; }
                page.song.changed();
            };
            styleBox.fontSize = 19.0f;
            styleBox.setTooltip ("Style within the genre");
            styleBox.buildMenu = [this] (PopupMenu& m)
            {
                const auto& g = genre();
                for (int i = 0; i < (int) g.styles.size(); ++i)
                    m.addItem (i + 1, g.styles[(size_t) i].name, true, page.song.genreStyle == i);
            };
            styleBox.onResult = [this] (int r) { page.song.genreStyle = r - 1; page.song.changed(); };
            favBtn.setTooltip ("Show your favourite progressions");
            favBtn.fontSize = 20.0f;
            favBtn.onClick = [this] { page.song.showFavourites = ! page.song.showFavourites; page.song.changed(); };
        }

        const sm::Genre& genre() const
        {
            const auto& lib = sm::library();
            return lib[(size_t) jlimit (0, (int) lib.size() - 1, page.song.genre)];
        }

        void resized() override
        {
            auto r = panelContent (getLocalBounds());
            auto g = r.removeFromTop (46);
            favBtn.setBounds (g.removeFromRight (44).withSizeKeepingCentre (42, 42));
            g.removeFromRight (6);
            genreBox.setBounds (g);
            r.removeFromTop (6);
            styleBox.setBounds (r.removeFromTop (42));
            infoArea = r.removeFromTop (30);
            viewport.setBounds (r);
            refresh();
        }

        void refresh()
        {
            rows.clear();
            std::vector<std::tuple<String, String, String>> items;
            if (page.song.showFavourites)
            {
                for (const auto& f : page.favourites->all()) items.emplace_back (f.name, f.roman, f.genre);
            }
            else
            {
                const auto& g = genre();
                const auto& st = g.styles[(size_t) jlimit (0, (int) g.styles.size() - 1, page.song.genreStyle)];
                for (const auto& p : st.progressions) items.emplace_back (String (p.name), String (p.roman), String (g.name));
            }
            const int w = viewport.getWidth() - viewport.getScrollBarThickness() - 2;
            const int cols = jmax (1, w / 290), gap = 6;          // a wide panel (tablet, phone on its side) shows two or three across
            const int cw = (w - gap * (cols - 1)) / cols;
            int i = 0;
            for (auto& [n, rmn, gn] : items)
            {
                rows.push_back (std::make_unique<ProgRow> (page, n, rmn, gn, page.song.showFavourites));
                content.addAndMakeVisible (*rows.back());
                rows.back()->setBounds ((i % cols) * (cw + gap), (i / cols) * 98, cw, 94);
                ++i;
            }
            const int y = ((i + cols - 1) / cols) * 98;
            content.setSize (w, jmax (y, 10));
            repaint();
        }

        void paint (Graphics& g) override
        {
            if (page.isMobileLayout()) paintFramedPanel (g, page.assets, getLocalBounds().toFloat(), page.getMobileLayout().hideTitles ? String() : String ("PROGRESSION BROWSER"));
            const auto& gen = genre();
            genreBox.text = page.song.showFavourites ? String (CharPointer_UTF8 ("\xe2\x98\x85 FAVOURITES")) : String (gen.name).toUpperCase();
            styleBox.text = page.song.showFavourites ? String ("ALL") : String (gen.styles[(size_t) jlimit (0, (int) gen.styles.size() - 1, page.song.genreStyle)].name).toUpperCase();
            styleBox.setEnabled (! page.song.showFavourites);
            favBtn.lit = page.song.showFavourites;

            g.setColour (Palette::inkSoft);
            g.setFont (page.assets.baskItalic (17.5f));
            g.drawFittedText ("Chords shown in " + page.keyText().toLowerCase().replace (" major", " major").replace (" minor", " minor"),
                              infoArea, Justification::centred, 1, 0.8f);
            if (rows.empty())
            {
                g.setFont (page.assets.baskItalic (18.0f));
                g.drawFittedText (page.song.showFavourites ? String (CharPointer_UTF8 ("No favourites yet. Tap \xe2\x98\x85 on any progression, or SAVE your own."))
                                                           : String ("No progressions here."),
                                  viewport.getBounds().reduced (16), Justification::centredTop, 3);
            }
        }

        void repaintRows() { for (auto& r : rows) r->repaint(); }

    private:
        SongMapperPage& page;
        MenuBox genreBox, styleBox;
        KButton favBtn;
        Viewport viewport;
        Component content;
        std::vector<std::unique_ptr<ProgRow>> rows;
        Rectangle<int> infoArea;
    };

    //==============================================================================
    // Brass medallion you drag into the DAW
    //==============================================================================
    class DragMedallion : public Component, public SettableTooltipClient
    {
    public:
        explicit DragMedallion (SongMapperPage& p) : page (p)
        {
            setMouseCursor (MouseCursor::DraggingHandCursor);
            setTooltip ("Drag this onto a track in your DAW to drop the whole song as MIDI chords. Click to save a .mid file instead.");
        }

        void paint (Graphics& g) override
        {
            // the brass ring and the parchment behind the glass are part of the page art
            const auto c = getLocalBounds().toFloat().getCentre();
            const float r2 = 74.0f;
            if (hover)
            {
                ColourGradient warm (Colour (0x55fff1c0), c.x, c.y, Colour (0x00fff1c0), c.x + r2, c.y, true);
                g.setGradientFill (warm);
                g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
            }

            // chord-bars icon (a little MIDI clip)
            const float k = r2 / 80.0f;     // the artwork below was drawn for an 80 px glass
            Rectangle<float> clip (c.x - 44.0f * k, c.y - 50.0f * k, 88.0f * k, 40.0f * k);
            g.setColour (Colour (0xff2a1a0c).withAlpha (0.12f));
            g.fillRoundedRectangle (clip, 5.0f);
            g.setColour (Colour (0xff2a1a0c));
            g.drawRoundedRectangle (clip, 5.0f, 1.6f);
            const int pattern[4][3] = { { 0, 2, 4 }, { 1, 3, 5 }, { 0, 3, 5 }, { 1, 2, 4 } };
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 3; ++j)
                    g.fillRoundedRectangle (clip.getX() + 6.0f * k + i * 20.5f * k, clip.getBottom() - 8.0f * k - pattern[i][j] * 5.0f * k, 17.0f * k, 3.6f * k, 1.5f);

            g.setColour (Palette::ink);
            g.setFont (page.assets.cinzel (20.0f * k));
            // a touch screen has no DAW to drag into: tapping shares the MIDI instead
            g.drawText (isTouchUI() ? "SHARE SONG" : "DRAG SONG", Rectangle<float> (c.x - r2, c.y - 2.0f * k, 2 * r2, 22.0f * k), Justification::centred, false);
            g.drawText (isTouchUI() ? "AS MIDI" : "TO DAW", Rectangle<float> (c.x - r2, c.y + 19.0f * k, 2 * r2, 22.0f * k), Justification::centred, false);
            g.setColour (Palette::inkSoft);
            g.setFont (page.assets.baskItalic (17.0f * k));
            const int bars = page.song.totalBars();
            const int secs = (int) std::lround (bars * page.song.beatsPerBar() * 60.0 / jmax (1.0, page.effectiveBpm()));
            g.drawFittedText (String (bars) + " bars" + String (CharPointer_UTF8 ("  \xc2\xb7  ")) + String (secs / 60) + ":" + String (secs % 60).paddedLeft ('0', 2),
                              Rectangle<float> (c.x - r2 * 0.78f, c.y + 42.0f * k, 1.56f * r2, 20.0f * k).toNearestInt(), Justification::centred, 1, 0.8f);

            // the glass: a soft reflection over everything, and a bright edge when hovered
            {
                Graphics::ScopedSaveState save (g);
                Path glassClip;
                glassClip.addEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
                g.reduceClipRegion (glassClip);
                ColourGradient glass (Colours::white.withAlpha (0.30f), c.x - r2 * 0.45f, c.y - r2 * 0.75f,
                                      Colours::transparentWhite, c.x - r2 * 0.1f, c.y - r2 * 0.05f, true);
                g.setGradientFill (glass);
                g.fillEllipse (c.x - r2 * 1.2f, c.y - r2 * 1.25f, r2 * 1.6f, r2 * 1.2f);
                ColourGradient inner (Colours::black.withAlpha (0.0f), c.x, c.y, Colours::black.withAlpha (0.28f), c.x + r2, c.y, true);
                g.setGradientFill (inner);
                g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
            }
            if (hover)
            {
                g.setColour (Palette::goldLight.withAlpha (0.75f));
                g.drawEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2, 2.2f);
            }
        }

        void mouseEnter (const MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const MouseEvent&) override  { hover = false; repaint(); }
        void mouseDown (const MouseEvent&) override  { dragStarted = false; }
        void mouseDrag (const MouseEvent& e) override
        {
            if (isTouchUI() || dragStarted || e.getDistanceFromDragStart() < 6) return;
            dragStarted = true;
            const auto f = page.midiFile (-1);
            if (f.existsAsFile())
                DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        }
        void mouseUp (const MouseEvent& e) override
        {
            if (! dragStarted && e.getDistanceFromDragStart() < 6 && e.mouseWasClicked())
                page.exportMidi (-1);
        }

    private:
        SongMapperPage& page;
        bool hover = false, dragStarted = false;
    };

    //==============================================================================
    // Practice view: the chords big enough to play from, opened with the expand button
    //==============================================================================
    class PracticeView : public Component, private Timer
    {
    public:
        explicit PracticeView (SongMapperPage& p)
            : page (p),
              closeBtn (p.assets, String (CharPointer_UTF8 ("\xc3\x97")), KButton::Look::Round),
              playBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"))), loopBtn (p.assets, "LOOP"),
              guitarBtn (p.assets, "GUITAR"), pianoBtn (p.assets, "PIANO"), voicing (p.assets), sourceBox (p.assets),
              prevBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x97\x80")), KButton::Look::Round),
              nextBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x96\xb6")), KButton::Look::Round),
              smallerBtn (p.assets, String (CharPointer_UTF8 ("\xe2\x88\x92")), KButton::Look::Round),
              biggerBtn (p.assets, "+", KButton::Look::Round)
        {
            for (Component* c : { (Component*) &closeBtn, (Component*) &playBtn, (Component*) &loopBtn, (Component*) &guitarBtn,
                                  (Component*) &pianoBtn, (Component*) &voicing, (Component*) &sourceBox, (Component*) &prevBtn, (Component*) &nextBtn,
                                  (Component*) &smallerBtn, (Component*) &biggerBtn })
                addAndMakeVisible (c);
            smallerBtn.fontSize = biggerBtn.fontSize = 28.0f;
            smallerBtn.setTooltip ("Smaller diagrams - more chords on a page (-)");
            biggerBtn.setTooltip ("Bigger diagrams - fewer chords on a page (+)");
            smallerBtn.onClick = [this] { changeSize (1); };
            biggerBtn.onClick  = [this] { changeSize (-1); };
            setWantsKeyboardFocus (true);

            closeBtn.fontSize = 30.0f;
            closeBtn.setTooltip ("Close the practice view (Esc)");
            closeBtn.onClick = [this] { page.closePractice(); };
            playBtn.fontSize = 19.0f;
            playBtn.setTooltip ("Play these chords - the one sounding lights up so you can play along");
            playBtn.onClick = [this] { togglePlay(); };
            loopBtn.fontSize = 19.0f;
            loopBtn.setTooltip ("Keep playing them round");
            loopBtn.onClick = [this] { page.song.loopPlayback = ! page.song.loopPlayback; page.song.changed(); repaint(); };
            guitarBtn.fontSize = pianoBtn.fontSize = 18.0f;
            guitarBtn.onClick = [this] { page.song.guitar = true; page.song.changed(); };
            pianoBtn.onClick  = [this] { page.song.guitar = false; page.song.changed(); };

            voicing.fontSize = 19.0f;
            voicing.setTooltip ("How the chords are voiced (Standard, Open, Triad, Barre, Power, Shell, Inversion, Color)");
            voicing.buildMenu = [this] (PopupMenu& m)
            {
                for (int i = 0; i < (int) sm::GuitarStyle::NumStyles; ++i)
                {
                    const auto st = (sm::GuitarStyle) i;
                    if (! page.song.guitar && st == sm::GuitarStyle::Barre) continue;
                    m.addItem (i + 1, styleName (st, page.song.guitar), true, page.song.style == st);
                }
            };
            voicing.onResult = [this] (int r) { page.song.style = (sm::GuitarStyle) (r - 1); page.song.changed(); };

            sourceBox.fontSize = 19.0f;
            sourceBox.setTooltip ("Which chords to practice: the progression, one section of the song, or just the chord you picked");
            sourceBox.buildMenu = [this] (PopupMenu& m)
            {
                m.addItem (1, "The progression (" + String ((int) page.song.tray.size()) + " chords)", ! page.song.tray.empty(), source == sourceProgression);
                if (page.focused.isValid())
                    m.addItem (2, "Just " + page.chordText (page.focused), true, source == sourceChord);
                m.addSeparator();
                m.addSectionHeader ("Song sections");
                for (int i = 0; i < (int) page.song.sections.size(); ++i)
                {
                    const auto& s = page.song.sections[(size_t) i];
                    m.addItem (100 + i, s.name + "  (" + String ((int) s.chords.size()) + " chords)", ! s.chords.empty(), source == i);
                }
            };
            sourceBox.onResult = [this] (int r) { setSource (r == 1 ? sourceProgression : (r == 2 ? sourceChord : r - 100)); };

            prevBtn.fontSize = nextBtn.fontSize = 18.0f;
            prevBtn.onClick = [this] { turnPage (-1); };
            nextBtn.onClick = [this] { turnPage (1); };
            startTimerHz (15);
        }

        enum : int { sourceAuto = -3, sourceProgression = -2, sourceChord = -1 };

        void setSource (int s)
        {
            if (s == sourceAuto)
            {
                const int sel = page.song.selectedSection;
                if (! page.song.tray.empty()) s = sourceProgression;
                else if (page.focused.isValid()) s = sourceChord;
                else if (isPositiveAndBelow (sel, (int) page.song.sections.size()) && ! page.song.sections[(size_t) sel].chords.empty()) s = sel;
                else s = sourceProgression;
            }
            source = s;
            selected = -1;
            pageIndex = 0;
            flashRect = {};
            refresh();
        }

        void refresh()
        {
            if (source >= (int) page.song.sections.size()) source = sourceProgression;
            const int pages = numPages();
            pageIndex = jlimit (0, jmax (0, pages - 1), pageIndex);
            repaint();
        }

        std::vector<sm::Chord> chords() const
        {
            if (source == sourceProgression) return page.song.tray;
            if (isPositiveAndBelow (source, (int) page.song.sections.size())) return page.song.sections[(size_t) source].chords;
            if (page.focused.isValid()) return { page.focused };
            return {};
        }

        int numPages() const { return jmax (1, ((int) chords().size() + perPage() - 1) / perPage()); }

        // how many cards share a page: fewer = bigger. Kept per instrument on the page, so it is remembered.
        static constexpr int pageSizes[] = { 1, 2, 3, 4, 6, 8, 12 };
        static constexpr int numPageSizes = (int) (sizeof (pageSizes) / sizeof (pageSizes[0]));
        int& sizeSetting() const { return page.practiceSize[page.song.guitar ? 1 : 0]; }
        int perPage() const { return pageSizes[jlimit (0, numPageSizes - 1, sizeSetting())]; }

        void changeSize (int step)
        {
            const int firstShown = pageIndex * perPage();
            sizeSetting() = jlimit (0, numPageSizes - 1, sizeSetting() + step);
            pageIndex = firstShown / perPage();      // keep the chords you were looking at in view
            refresh();
        }

        // one keyboard range for every chord here, so the keys line up from card to card
        void keyboardSpan (int& low, int& whites) const
        {
            int lo = 128, hi = -1;
            for (const auto& c : chords())
                for (int n : page.song.notesFor (c)) { lo = jmin (lo, n); hi = jmax (hi, n); }
            if (hi < 0) { lo = 60; hi = 72; }
            auto isWhite = [] (int n) { const int pc = n % 12; return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11; };
            low = lo - 1;                            // a white key of room below the lowest note...
            while (! isWhite (low)) --low;
            int top = hi + 1;                        // ...and above the highest
            while (! isWhite (top)) ++top;
            whites = 0;
            for (int n = low; n <= top; ++n) whites += isWhite (n) ? 1 : 0;
            whites = jmax (whites, 10);
        }

        int playingIndex() const
        {
            if (source == sourceProgression) return page.playingTray;
            if (source >= 0 && page.playingSection == source) return page.playingChord;
            return -1;
        }

        bool playingThis() const
        {
            if (! page.isPlaying()) return false;
            if (source == sourceProgression) return page.playScope == -2 && page.playingTray >= 0;
            if (source >= 0) return page.playScope == source || (page.playScope == -1 && page.playingSection == source);
            return false;
        }

        void togglePlay()
        {
            if (playingThis()) { page.stopPlayback(); return; }
            if (source == sourceProgression)  page.playTray();
            else if (source >= 0)             page.playSong (source);
            else if (page.focused.isValid())  page.auditionChord (page.focused);
        }

        void turnPage (int d)
        {
            pageIndex = jlimit (0, numPages() - 1, pageIndex + d);
            flashRect = {};
            repaint();
        }

        //==========================================================================
        void resized() override
        {
            if (getHeight() < 520 && getWidth() > getHeight())
            {
                resizedShort();
                return;
            }
            if (getWidth() < 760)
            {
                resizedNarrow();
                return;
            }
            narrow = false;
            frame = getLocalBounds().toFloat().reduced (22.0f);
            auto r = frame.reduced (28.0f, 20.0f).toNearestInt();
            auto row1 = r.removeFromTop (52);
            closeBtn.setBounds (row1.removeFromRight (50).withSizeKeepingCentre (48, 48));
            row1.removeFromRight (12);
            loopBtn.setBounds (row1.removeFromRight (100).reduced (0, 4));
            row1.removeFromRight (8);
            playBtn.setBounds (row1.removeFromRight (140).reduced (0, 4));
            titleArea = row1;
            r.removeFromTop (8);
            auto row2 = r.removeFromTop (44);
            biggerBtn.setBounds (row2.removeFromRight (46).withSizeKeepingCentre (44, 44));
            sizeLabel = row2.removeFromRight (78);
            smallerBtn.setBounds (row2.removeFromRight (46).withSizeKeepingCentre (44, 44));
            row2.removeFromRight (12);
            sourceBox.setBounds (row2.removeFromLeft (jlimit (200, 320, row2.getWidth() - 440)));
            row2.removeFromLeft (16);
            guitarBtn.setBounds (row2.removeFromLeft (112).reduced (0, 2));
            row2.removeFromLeft (6);
            pianoBtn.setBounds (row2.removeFromLeft (100).reduced (0, 2));
            row2.removeFromLeft (10);
            voicing.setBounds (row2.removeFromLeft (jmin (180, row2.getWidth())));
            r.removeFromTop (14);
            auto foot = r.removeFromBottom (48);
            nextBtn.setBounds (foot.removeFromRight (48).withSizeKeepingCentre (44, 44));
            pageLabel = foot.removeFromRight (110);
            prevBtn.setBounds (foot.removeFromRight (48).withSizeKeepingCentre (44, 44));
            hintArea = foot;
            r.removeFromBottom (10);
            gridArea = r;
        }

        // a phone held upright: the controls in rows above the cards, size and pages below them
        void resizedNarrow()
        {
            narrow = true;
            frame = getLocalBounds().toFloat().reduced (6.0f);
            auto r = frame.reduced (12.0f, 10.0f).toNearestInt();
            auto row1 = r.removeFromTop (46);
            closeBtn.setBounds (row1.removeFromRight (46).withSizeKeepingCentre (44, 44));
            titleArea = row1;
            r.removeFromTop (4);
            auto row2 = r.removeFromTop (42);
            const int third = (row2.getWidth() - 12) / 3;
            playBtn.setBounds (row2.removeFromLeft (third).reduced (0, 2));
            row2.removeFromLeft (6);
            loopBtn.setBounds (row2.removeFromLeft (third * 3 / 4).reduced (0, 2));
            row2.removeFromLeft (6);
            sourceBox.setBounds (row2);
            r.removeFromTop (6);
            auto row3 = r.removeFromTop (42);
            guitarBtn.setBounds (row3.removeFromLeft (row3.getWidth() * 3 / 10).reduced (0, 2));
            row3.removeFromLeft (4);
            pianoBtn.setBounds (row3.removeFromLeft (row3.getWidth() * 4 / 10).reduced (0, 2));
            row3.removeFromLeft (6);
            voicing.setBounds (row3);
            r.removeFromTop (10);
            auto foot = r.removeFromBottom (46);
            smallerBtn.setBounds (foot.removeFromLeft (46).withSizeKeepingCentre (42, 42));
            sizeLabel = foot.removeFromLeft (54);
            biggerBtn.setBounds (foot.removeFromLeft (46).withSizeKeepingCentre (42, 42));
            nextBtn.setBounds (foot.removeFromRight (46).withSizeKeepingCentre (44, 44));
            pageLabel = foot.removeFromRight (64);
            prevBtn.setBounds (foot.removeFromRight (46).withSizeKeepingCentre (44, 44));
            r.removeFromBottom (2);
            hintArea = r.removeFromBottom (38);
            r.removeFromBottom (4);
            gridArea = r;
        }

        // a phone on its side: the controls in a column on the left, the cards fill the rest
        void resizedShort()
        {
            narrow = true;
            frame = getLocalBounds().toFloat().reduced (6.0f);
            auto r = frame.reduced (12.0f, 8.0f).toNearestInt();
            auto col = r.removeFromLeft (jlimit (200, 260, r.getWidth() / 4));
            r.removeFromLeft (12);
            const int rowH = jlimit (34, 44, (col.getHeight() - 12) / 8);
            auto row1 = col.removeFromTop (rowH + 4);
            closeBtn.setBounds (row1.removeFromRight (44).withSizeKeepingCentre (rowH, rowH));
            titleArea = row1;
            auto line = [&] { col.removeFromTop (4); return col.removeFromTop (rowH); };
            auto row2 = line();
            playBtn.setBounds (row2.removeFromLeft (row2.getWidth() * 6 / 10).reduced (0, 1));
            row2.removeFromLeft (6);
            loopBtn.setBounds (row2.reduced (0, 1));
            sourceBox.setBounds (line());
            auto row4 = line();
            guitarBtn.setBounds (row4.removeFromLeft (row4.getWidth() / 2 - 3).reduced (0, 1));
            row4.removeFromLeft (6);
            pianoBtn.setBounds (row4.reduced (0, 1));
            voicing.setBounds (line());
            auto row6 = line();
            smallerBtn.setBounds (row6.removeFromLeft (rowH + 2).withSizeKeepingCentre (rowH, rowH));
            biggerBtn.setBounds (row6.removeFromRight (rowH + 2).withSizeKeepingCentre (rowH, rowH));
            sizeLabel = row6;
            auto row7 = line();
            prevBtn.setBounds (row7.removeFromLeft (rowH + 2).withSizeKeepingCentre (rowH, rowH));
            nextBtn.setBounds (row7.removeFromRight (rowH + 2).withSizeKeepingCentre (rowH, rowH));
            pageLabel = row7;
            hintArea = col.reduced (0, 4);
            gridArea = r;
        }

        // the cards of this page, laid out as big as they can be
        std::vector<Rectangle<float>> cardRects (int n) const
        {
            std::vector<Rectangle<float>> out;
            if (n <= 0) return out;
            const auto area = gridArea.toFloat();
            const float gap = 18.0f, aspect = page.song.guitar ? 0.78f : 2.3f;     // the shape a card likes to be
            int bestCols = 1;
            float best = 0.0f;
            for (int cols = 1; cols <= n; ++cols)
            {
                const int rows = (n + cols - 1) / cols;
                const float cw = (area.getWidth() - gap * (float) (cols - 1)) / (float) cols;
                const float ch = (area.getHeight() - gap * (float) (rows - 1)) / (float) rows;
                const float usable = jmin (cw, ch * aspect);
                if (usable > best + 0.5f) { best = usable; bestCols = cols; }
            }
            const int cols = bestCols, rows = (n + cols - 1) / cols;
            float cw = (area.getWidth() - gap * (float) (cols - 1)) / (float) cols;
            float ch = (area.getHeight() - gap * (float) (rows - 1)) / (float) rows;
            cw = jmin (cw, ch * aspect * 1.25f);            // don't let a card get much wider than it needs
            ch = jmin (ch, cw / aspect * 1.35f);
            const float gridH = ch * (float) rows + gap * (float) (rows - 1);
            float y = area.getY() + (area.getHeight() - gridH) * 0.5f;
            for (int row = 0, i = 0; row < rows; ++row)
            {
                const int inRow = jmin (cols, n - i);
                const float rowW = cw * (float) inRow + gap * (float) (inRow - 1);
                float x = area.getX() + (area.getWidth() - rowW) * 0.5f;
                for (int c = 0; c < inRow; ++c, ++i)
                {
                    out.push_back ({ x, y, cw, ch });
                    x += cw + gap;
                }
                y += ch + gap;
            }
            return out;
        }

        struct CardLayout
        {
            Rectangle<float> name, longName, diagram, notes;
            GuitarGeometry geo;
            float scale = 1.0f;
            int low = 48, whites = 14;      // keyboard
        };

        CardLayout layoutFor (Rectangle<float> r, const sm::Chord& c) const
        {
            CardLayout L;
            auto inner = r.reduced (jmax (10.0f, r.getWidth() * (page.song.guitar ? 0.06f : 0.04f)), jmax (8.0f, r.getHeight() * 0.035f));
            const float nameH = jlimit (36.0f, 78.0f, r.getHeight() * 0.12f);
            L.name = inner.removeFromTop (nameH);
            L.longName = inner.removeFromTop (jmax (20.0f, nameH * 0.48f));
            L.notes = inner.removeFromBottom (jlimit (24.0f, 46.0f, nameH * 0.6f));
            inner.removeFromBottom (inner.getHeight() * 0.03f);
            if (page.song.guitar)
            {
                const float d = jmin (inner.getWidth(), inner.getHeight());
                L.diagram = inner.withSizeKeepingCentre (d, d);
                L.scale = d / 150.0f;
                L.geo = guitarGeometry (sm::guitarVoicing (c, page.song.style), L.diagram, false, L.scale);
            }
            else
            {
                keyboardSpan (L.low, L.whites);
                const float h = jmin (inner.getHeight(), inner.getWidth() / (float) L.whites * 5.5f);    // keys about 5.5 times as long as wide
                L.diagram = inner.withSizeKeepingCentre (inner.getWidth(), h);
                L.scale = h / 102.0f;
            }
            return L;
        }

        //==========================================================================
        void paint (Graphics& g) override
        {
            g.fillAll (Colours::black.withAlpha (0.62f));
            paintCard (g, page.assets, frame, 18.0f, Colour (0xfffaefd4), Colour (0xffe6cf9f), Palette::goldDark, 3.0f, 1.0f, 0.2f);
            {
                // a brass rim
                ColourGradient rim (Palette::goldLight, frame.getX(), frame.getY(), Palette::goldDark, frame.getX(), frame.getBottom(), false);
                g.setGradientFill (rim);
                g.drawRoundedRectangle (frame.reduced (1.5f), 18.0f, 5.0f);
                g.setColour (Colour (0xff6b4520).withAlpha (0.5f));
                g.drawRoundedRectangle (frame.reduced (6.0f), 14.0f, 1.2f);
            }

            guitarBtn.lit = page.song.guitar;
            pianoBtn.lit = ! page.song.guitar;
            loopBtn.lit = page.song.loopPlayback;
            const bool playing = playingThis();
            playBtn.lit = playing;
            playBtn.setButtonText (playing ? String (CharPointer_UTF8 ("\xe2\x96\xa0 STOP")) : String (CharPointer_UTF8 ("\xe2\x96\xb6 PLAY")));
            voicing.text = shortStyleName (page.song.guitar || page.song.style != sm::GuitarStyle::Barre ? page.song.style : sm::GuitarStyle::Standard, page.song.guitar);
            sourceBox.text = source == sourceProgression ? String ("THE PROGRESSION")
                           : (isPositiveAndBelow (source, (int) page.song.sections.size()) ? page.song.sections[(size_t) source].name.toUpperCase()
                                                                                           : String ("THIS CHORD"));
            playBtn.setEnabled (! chords().empty());

            // title
            auto t = titleArea.toFloat();
            g.setColour (panelInk);
            {
                Font tf = page.assets.cinzel (narrow ? 24.0f : 32.0f);
                while (textWidth (tf, "CHORD PRACTICE") > t.getWidth() - 4.0f && tf.getHeight() > 12.0f) tf = tf.withHeight (tf.getHeight() - 1.0f);
                g.setFont (tf);
                const float tw = textWidth (tf, "CHORD PRACTICE") + 12.0f;
                g.drawText ("CHORD PRACTICE", t.removeFromLeft (narrow ? tw : 360.0f), Justification::centredLeft, false);
            }
            g.setColour (Palette::inkSoft);
            g.setFont (page.assets.baskItalic (19.0f));
            if (t.getWidth() > 60.0f)
                g.drawFittedText ("in " + page.keyText().toLowerCase(), t.toNearestInt(), Justification::centredLeft, 1, 0.8f);

            const auto all = chords();
            const int first = pageIndex * perPage();
            const int n = jlimit (0, perPage(), (int) all.size() - first);
            const auto rects = cardRects (n);
            const int now = playingIndex();
            const auto order = displayOrder ((int) all.size());     // while dragging: as it will be when dropped
            for (int k = 0; k < n; ++k)
            {
                const int idx = order[(size_t) (first + k)];
                if (dragging && idx == pressIndex)
                    paintDropSlot (g, rects[(size_t) k]);
                else
                    paintChordCard (g, rects[(size_t) k], all[(size_t) idx], idx == now, idx == selected);
            }

            if (all.empty())
            {
                g.setColour (Palette::inkSoft);
                g.setFont (page.assets.baskItalic (24.0f));
                g.drawFittedText ("No chords here yet. Add some to the progression or a section,\nor pick another set of chords above.",
                                  gridArea, Justification::centred, 3);
            }

            // flash on the note you clicked
            if (! flashRect.isEmpty())
            {
                Path p;
                if (flashRound) p.addEllipse (flashRect); else p.addRectangle (flashRect);
                DropShadow (Palette::blue, 14, {}).drawForPath (g, p);
                g.setColour (Palette::blue.withAlpha (0.6f));
                g.fillPath (p);
            }

            // the card being dragged, under the mouse
            if (dragging && isPositiveAndBelow (pressIndex, (int) all.size()) && n > 0)
            {
                const auto r = rects[0].withPosition (dragPos - grabOffset);
                Path p;
                p.addRoundedRectangle (r, 12.0f);
                DropShadow (Colours::black.withAlpha (0.55f), 26, { 6, 10 }).drawForPath (g, p);
                Graphics::ScopedSaveState save (g);
                g.addTransform (AffineTransform::rotation (-0.02f, r.getCentreX(), r.getCentreY()));
                paintChordCard (g, r, all[(size_t) pressIndex], false, true);
            }

            g.setColour (Palette::inkSoft);
            g.setFont (page.assets.baskItalic (19.0f));
            const String dot (CharPointer_UTF8 ("  \xc2\xb7  "));
            String hint = isTouchUI() ? "Tap a chord, a dot or a key to hear it" : "Click a chord, or one dot or key, to hear it";
            if (canReorder()) hint += dot + (isTouchUI() ? "drag a card to move it" : "drag a card to move it");
            if (! isTouchUI()) hint += dot + String (CharPointer_UTF8 ("+ / \xe2\x88\x92 size")) + dot + "Esc closes";
            if (hintArea.getHeight() >= 30)
                g.drawFittedText (hint, hintArea, narrow ? Justification::centred : Justification::centredLeft, narrow ? 4 : 2, 0.8f);
            g.setColour (panelInk);
            g.setFont (page.assets.cinzel (18.0f));
            g.drawText ("SIZE", sizeLabel.toFloat(), Justification::centred, false);
            smallerBtn.setEnabled (sizeSetting() < numPageSizes - 1);
            biggerBtn.setEnabled (sizeSetting() > 0);

            const int pages = numPages();
            prevBtn.setVisible (pages > 1);
            nextBtn.setVisible (pages > 1);
            if (pages > 1)
            {
                g.setColour (panelInk);
                g.setFont (page.assets.cinzel (19.0f));
                g.drawText (String (pageIndex + 1) + " / " + String (pages), pageLabel.toFloat(), Justification::centred, false);
            }
        }

        void paintChordCard (Graphics& g, Rectangle<float> r, const sm::Chord& c, bool playing, bool sel)
        {
            if (playing)
            {
                Path p;
                p.addRoundedRectangle (r, 12.0f);
                DropShadow (Palette::blue, 18, {}).drawForPath (g, p);
            }
            paintCard (g, page.assets, r, 12.0f, playing ? Colour (0xffeef8ff) : Colour (0xfffffaec),
                       playing ? Colour (0xffcfe8ff) : Colour (0xffeedcb6),
                       playing ? Palette::blue : (sel ? Palette::goldDark : Colour (0xff7a5a2c)), playing ? 3.0f : (sel ? 2.4f : 1.2f),
                       playing ? 0.0f : 0.9f, 0.16f);
            const auto L = layoutFor (r, c);

            const String name = page.chordText (c);
            Font nf = page.assets.bask (L.name.getHeight() * 0.95f);
            while (textWidth (nf, name) > L.name.getWidth() && nf.getHeight() > 14.0f) nf = nf.withHeight (nf.getHeight() - 1.0f);
            g.setColour (playing ? chipBlue : panelInk);
            g.setFont (nf);
            g.drawText (name, L.name, Justification::centred, false);

            String sub = "(" + utf8 (c.longName (page.useFlats())) + ")";
            if (page.song.guitar)
            {
                const auto v = sm::guitarVoicing (c, page.song.style);
                if (v.shown.isValid() && v.shown.quality != c.quality) sub += "  - plays " + page.chordText (v.shown);
            }
            g.setColour (Palette::inkSoft);
            g.setFont (page.assets.baskItalic (jmax (17.0f, L.longName.getHeight() * 0.78f)));
            g.drawFittedText (sub, L.longName.toNearestInt(), Justification::centred, 1, 0.7f);

            const float noteFont = jlimit (17.0f, 30.0f, L.notes.getHeight() * 0.72f);
            if (page.song.guitar)
            {
                const auto v = sm::guitarVoicing (c, page.song.style);
                const auto fingers = fingeringFor (v);
                drawGuitarDiagram (g, page.assets, v, L.diagram, false, L.scale, fingers.data());
                // the note on each string, under the diagram
                g.setFont (page.assets.baskItalic (noteFont));
                for (int s = 0; s < 6; ++s)
                {
                    const int f = v.frets[(size_t) s];
                    const float x = L.geo.grid.getX() + (float) s * L.geo.sw;
                    g.setColour (f < 0 ? Palette::inkSoft.withAlpha (0.6f) : panelInk);
                    const String label = f < 0 ? String ("x") : utf8 (sm::pcName ((sm::standardTuning[(size_t) s] + f) % 12, page.useFlats()));
                    g.drawText (label, Rectangle<float> (x - L.geo.sw * 0.5f, L.notes.getY(), L.geo.sw, L.notes.getHeight()), Justification::centred, false);
                }
            }
            else
            {
                const auto notes = page.song.notesFor (c);
                drawKeyboard (g, L.diagram, notes, L.low, L.whites, L.scale);
                StringArray names;
                for (int nn : notes) names.add (utf8 (sm::pcName (nn % 12, page.useFlats())) + String (nn / 12 - 1));
                g.setColour (panelInk);
                g.setFont (page.assets.baskItalic (noteFont));
                g.drawFittedText (names.joinIntoString ("   "), L.notes.toNearestInt(), Justification::centred, 1, 0.7f);
            }
        }

        //==========================================================================
        // which note is under the mouse in a card (-1 = none)
        int noteAt (const CardLayout& L, const sm::Chord& c, Point<float> p, Rectangle<float>& mark, bool& round) const
        {
            if (page.song.guitar)
            {
                const auto v = sm::guitarVoicing (c, page.song.style);
                if (! v.valid) return -1;
                const auto& geo = L.geo;
                const int s = (int) std::lround ((p.x - geo.grid.getX()) / geo.sw);
                if (s < 0 || s > 5 || std::abs (p.x - (geo.grid.getX() + (float) s * geo.sw)) > geo.sw * 0.55f) return -1;
                if (p.y < geo.area.getY() - 2.0f || p.y > geo.grid.getBottom() + 2.0f) return -1;
                int fret;
                Point<float> centre;
                if (p.y < geo.grid.getY())
                {
                    fret = 0;
                    centre = { geo.grid.getX() + (float) s * geo.sw, geo.area.getY() + geo.topMarks * 0.45f };
                }
                else
                {
                    const int row = jlimit (0, 4, (int) ((p.y - geo.grid.getY()) / geo.fh));
                    fret = geo.base + row;
                    centre = { geo.grid.getX() + (float) s * geo.sw, geo.grid.getY() + ((float) row + 0.5f) * geo.fh };
                }
                const float rr = jmin (geo.sw, geo.fh) * 0.42f;
                mark = Rectangle<float> (centre.x - rr, centre.y - rr, 2 * rr, 2 * rr);
                round = true;
                return sm::standardTuning[(size_t) s] + fret;
            }
            const auto kb = L.diagram;
            if (! kb.contains (p)) return -1;
            const float ww = kb.getWidth() / (float) L.whites;
            for (int i = 0; i < L.whites - 1; ++i)
            {
                const int white = whiteKeyNote (L.low, i);
                if (white % 12 == 4 || white % 12 == 11) continue;
                Rectangle<float> k (kb.getX() + (float) (i + 1) * ww - ww * 0.32f, kb.getY(), ww * 0.64f, kb.getHeight() * 0.6f);
                if (k.contains (p)) { mark = k; round = false; return white + 1; }
            }
            const int i = jlimit (0, L.whites - 1, (int) ((p.x - kb.getX()) / ww));
            mark = Rectangle<float> (kb.getX() + (float) i * ww, kb.getY(), ww, kb.getHeight());
            round = false;
            return whiteKeyNote (L.low, i);
        }

        //==========================================================================
        // rearranging: drag a card, right-click it, or Shift + arrow keys. It changes the progression or section itself.
        bool canReorder() const
        {
            return (source == sourceProgression || isPositiveAndBelow (source, (int) page.song.sections.size())) && chords().size() > 1;
        }

        // move the chord at 'from' so that it ends up at 'to'
        void moveCard (int from, int to)
        {
            const int count = (int) chords().size();
            if (! canReorder() || ! isPositiveAndBelow (from, count)) return;
            to = jlimit (0, count - 1, to);
            if (from == to) return;
            if (source == sourceProgression) page.song.moveTray (from, to);
            else                             page.song.moveChord (source, from, source, to > from ? to + 1 : to);   // that one takes an insertion point
            selected = to;
            pageIndex = jlimit (0, numPages() - 1, to / perPage());
            refresh();
        }

        // the order the cards are shown in: as they are, or while dragging, as they will be after the drop
        std::vector<int> displayOrder (int count) const
        {
            std::vector<int> order ((size_t) count);
            std::iota (order.begin(), order.end(), 0);
            if (dragging && isPositiveAndBelow (pressIndex, count) && isPositiveAndBelow (dropIndex, count) && dropIndex != pressIndex)
            {
                order.erase (order.begin() + pressIndex);
                order.insert (order.begin() + dropIndex, pressIndex);
            }
            return order;
        }

        // which place on this page the mouse is over (the nearest card), -1 when off the cards
        int slotAt (Point<float> p) const
        {
            const int count = (int) chords().size();
            const int first = pageIndex * perPage();
            const int n = jlimit (0, perPage(), count - first);
            const auto rects = cardRects (n);
            if (n == 0 || ! gridArea.toFloat().expanded (30.0f).contains (p)) return -1;
            int best = -1;
            float bestDist = std::numeric_limits<float>::max();
            for (int k = 0; k < n; ++k)
            {
                const float d = rects[(size_t) k].getCentre().getDistanceSquaredFrom (p);
                if (d < bestDist) { bestDist = d; best = first + k; }
            }
            return best;
        }

        void paintDropSlot (Graphics& g, Rectangle<float> r)
        {
            g.setColour (Palette::gold.withAlpha (0.14f));
            g.fillRoundedRectangle (r, 12.0f);
            Path outline, dashed;
            outline.addRoundedRectangle (r.reduced (2.0f), 12.0f);
            const float dashes[] = { 10.0f, 7.0f };
            PathStrokeType (2.5f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (Palette::goldDark);
            g.fillPath (dashed);
        }

        void mouseDrag (const MouseEvent& e) override
        {
            hold.mouseDrag (e);
            if (hold.fired() || pressIndex < 0 || ! canReorder() || e.mods.isPopupMenu()) return;
            if (! dragging)
            {
                if (e.getDistanceFromDragStart() < 7) return;
                dragging = true;
                dropIndex = pressIndex;
                flashRect = {};
                setMouseCursor (MouseCursor::DraggingHandCursor);
            }
            dragPos = e.position;
            if (const int s = slotAt (dragPos); s >= 0) dropIndex = s;
            repaint();
        }

        void mouseUp (const MouseEvent&) override
        {
            hold.mouseUp();
            if (dragging)
            {
                dragging = false;
                setMouseCursor (MouseCursor::NormalCursor);
                const int from = pressIndex, to = dropIndex;
                pressIndex = dropIndex = -1;
                if (from >= 0 && to >= 0 && from != to) moveCard (from, to);
                else { selected = from; repaint(); }
            }
            pressIndex = -1;
            edgeDir = 0;
        }

        void showCardMenu (int index)
        {
            const int count = (int) chords().size();
            const bool movable = canReorder();
            PopupMenu m;
            m.setLookAndFeel (&getLookAndFeel());
            m.addItem (1, "Play " + page.chordText (chords()[(size_t) index]));
            m.addSeparator();
            m.addItem (2, String (CharPointer_UTF8 ("Move left  (Shift+\xe2\x86\x90)")), movable && index > 0);
            m.addItem (3, String (CharPointer_UTF8 ("Move right  (Shift+\xe2\x86\x92)")), movable && index < count - 1);
            m.addItem (4, "Move to the start", movable && index > 0);
            m.addItem (5, "Move to the end", movable && index < count - 1);
            if (! movable)
                m.addSectionHeader ("Pick the progression or a section to rearrange");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (Rectangle<int> (Desktop::getMousePosition(), Desktop::getMousePosition()).expanded (1))
                                                 .withStandardItemHeight (menuItemHeight()),
                             [safe = SafePointer<PracticeView> (this), index] (int r)
                             {
                                 if (safe == nullptr || r == 0) return;
                                 const auto list = safe->chords();
                                 if (! isPositiveAndBelow (index, (int) list.size())) return;
                                 if (r == 1) safe->page.focusChord (list[(size_t) index]);
                                 else if (r == 2) safe->moveCard (index, index - 1);
                                 else if (r == 3) safe->moveCard (index, index + 1);
                                 else if (r == 4) safe->moveCard (index, 0);
                                 else if (r == 5) safe->moveCard (index, (int) list.size() - 1);
                             });
        }

        void mouseDown (const MouseEvent& e) override
        {
            grabKeyboardFocus();
            dragging = false;
            pressIndex = -1;
            const auto all = chords();
            const int first = pageIndex * perPage();
            const int n = jlimit (0, perPage(), (int) all.size() - first);
            const auto rects = cardRects (n);
            for (int k = 0; k < n; ++k)
            {
                if (! rects[(size_t) k].contains (e.position)) continue;
                const auto& c = all[(size_t) (first + k)];
                selected = first + k;
                if (e.mods.isPopupMenu())
                {
                    repaint();
                    showCardMenu (first + k);
                    return;
                }
                pressIndex = first + k;
                grabOffset = e.position - rects[(size_t) k].getPosition();
                dragPos = e.position;
                hold.onLongPress = [this, idx = first + k]
                {
                    if (dragging) return;
                    pressIndex = -1;
                    showCardMenu (idx);
                };
                hold.mouseDown (e);
                Rectangle<float> mark;
                bool round = false;
                const int note = noteAt (layoutFor (rects[(size_t) k], c), c, e.position, mark, round);
                if (note >= 0)
                {
                    page.auditionNote (note);
                    flashRect = mark;
                    flashRound = round;
                    flashUntil = Time::getMillisecondCounter() + 700;
                }
                else
                {
                    flashRect = {};
                    page.focusChord (c, ! playingThis());     // plays it (unless these chords are playing already), and shows it in the small diagram too
                }
                repaint();
                return;
            }
        }

        bool keyPressed (const KeyPress& key) override
        {
            if (key.isKeyCode (KeyPress::escapeKey))
            {
                if (dragging) { dragging = false; pressIndex = dropIndex = -1; setMouseCursor (MouseCursor::NormalCursor); repaint(); return true; }
                page.closePractice();
                return true;
            }
            const bool shift = key.getModifiers().isShiftDown();
            const bool hasSelection = isPositiveAndBelow (selected, (int) chords().size());
            if (key.isKeyCode (KeyPress::leftKey))
            {
                if (shift && hasSelection) moveCard (selected, selected - 1); else turnPage (-1);
                return true;
            }
            if (key.isKeyCode (KeyPress::rightKey))
            {
                if (shift && hasSelection) moveCard (selected, selected + 1); else turnPage (1);
                return true;
            }
            // undo / redo and the manual work here as on the page
            const auto mods = key.getModifiers();
            if ((mods.isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'Y')) || key.isKeyCode (KeyPress::F1Key))
                return page.keyPressed (key);
            const auto ch = key.getTextCharacter();
            if (ch == '+' || ch == '=') { changeSize (-1); return true; }
            if (ch == '-' || ch == '_') { changeSize (1); return true; }
            if (key.isKeyCode (KeyPress::spaceKey) && page.proc.wrapperType == AudioProcessor::wrapperType_Standalone)
            {
                togglePlay();
                return true;
            }
            return false;
        }

        int getSource() const noexcept { return source; }
        int getPageIndex() const noexcept { return pageIndex; }
        int lastClickedNote() const noexcept { return page.lastAuditionedNote; }

    private:
        void timerCallback() override
        {
            if (dragging)
            {
                // hold a dragged card over the page arrows (or the edge of the cards) to turn the page
                int dir = 0;
                const auto grid = gridArea.toFloat();
                if (numPages() > 1)
                {
                    if (prevBtn.getBounds().toFloat().expanded (10.0f).contains (dragPos) || dragPos.x < grid.getX() + 12.0f) dir = -1;
                    if (nextBtn.getBounds().toFloat().expanded (10.0f).contains (dragPos) || dragPos.x > grid.getRight() - 12.0f) dir = 1;
                }
                const auto t = Time::getMillisecondCounter();
                if (dir != edgeDir) { edgeDir = dir; edgeSince = t; }
                else if (dir != 0 && t - edgeSince > 650)
                {
                    const int before = pageIndex;
                    pageIndex = jlimit (0, numPages() - 1, pageIndex + dir);
                    edgeSince = t;
                    if (pageIndex != before)
                    {
                        const int first = pageIndex * perPage();
                        const int last = jmin ((int) chords().size(), first + perPage()) - 1;
                        dropIndex = dir > 0 ? first : last;
                        repaint();
                    }
                }
                return;     // and don't follow the playback while a card is in the hand
            }

            // follow the playback: light the chord that is sounding and turn to its page
            const int now = playingIndex();
            if (now >= 0 && now / perPage() != pageIndex)
                pageIndex = jlimit (0, numPages() - 1, now / perPage());
            if (! flashRect.isEmpty() && Time::getMillisecondCounter() > flashUntil) flashRect = {};
            const String sig = String (now) + "/" + String (pageIndex) + "/" + String ((int) playingThis()) + "/" + String ((int) flashRect.isEmpty())
                             + "/" + String ((int) chords().size()) + "/" + String ((int) page.song.guitar) + "/" + String ((int) page.song.style)
                             + "/" + (page.focused.isValid() ? String (page.focused.id()) : String());
            if (sig != lastSig)
            {
                lastSig = sig;
                repaint();
            }
        }

        SongMapperPage& page;
        KButton closeBtn, playBtn, loopBtn, guitarBtn, pianoBtn;
        MenuBox voicing, sourceBox;
        KButton prevBtn, nextBtn, smallerBtn, biggerBtn;
        Rectangle<float> frame, flashRect;
        Rectangle<int> titleArea, gridArea, hintArea, pageLabel, sizeLabel;
        int source = sourceProgression, pageIndex = 0, selected = -1;
        bool flashRound = false;
        uint32 flashUntil = 0;
        String lastSig;
        LongPress hold;
        bool narrow = false;
        // dragging a card
        int pressIndex = -1, dropIndex = -1, edgeDir = 0;
        bool dragging = false;
        Point<float> dragPos, grabOffset;
        uint32 edgeSince = 0;
    };

    //==============================================================================
    // The page's painted artwork (map, lid with title, brass panels, plates, porthole), cached as an image
    //==============================================================================
    class MapperBackdrop : public Component
    {
    public:
        explicit MapperBackdrop (Assets& a) : assets (a)
        {
            setInterceptsMouseClicks (false, false);
            setBufferedToImage (true);
            setOpaque (true);
        }
        void paint (Graphics& g) override
        {
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            if (assets.mapperBackground.isValid())
                g.drawImage (assets.mapperBackground, getLocalBounds().toFloat());
            else
                g.drawImage (assets.background, getLocalBounds().toFloat());
        }
    private:
        Assets& assets;
    };

    //==============================================================================
    // The page
    //==============================================================================
    SongMapperPage::SongMapperPage (KeyCompassProcessor& p, Assets& a)
        : proc (p), assets (a), song (p.getSongMap()),
          switchButton (a, String (CharPointer_UTF8 ("\xe2\x97\x82 KEY COMPASS")), KButton::Look::Plaque),
          tapButton (a, "TAP TEMPO", KButton::Look::Plaque),
          captureButton (a, "CAPTURE CHORDS", KButton::Look::Plaque),
          songButton (a, String (CharPointer_UTF8 ("SONG \xe2\x96\xbe")), KButton::Look::Plaque),
          helpButton (a, "?", KButton::Look::Round),
          undoButton (a, String (CharPointer_UTF8 ("\xe2\x86\xb6")), KButton::Look::Round),
          redoButton (a, String (CharPointer_UTF8 ("\xe2\x86\xb7")), KButton::Look::Round),
          playStyleBox (a, { "Block", "Pulse", "Strum", "Arpeggio" },
                           { sm::playStyleDescription (sm::PlayStyle::Block), sm::playStyleDescription (sm::PlayStyle::Pulse),
                             sm::playStyleDescription (sm::PlayStyle::Strum), sm::playStyleDescription (sm::PlayStyle::Arpeggio) }),
          meterBox (a, { "4/4", "3/4", "6/8" }, { "four beats to a bar", "waltz time", "two swaying beats" }),
          octaveBox (a, { "-2 OCT", "-1 OCT", "NORMAL", "+1 OCT", "+2 OCT" }, 2),
          lockKey (a, RoundToggle::Icon::Lock)
    {
        setSize (Layout::designWidth, Layout::designHeight);
        setInterceptsMouseClicks (false, true);

        backdrop = std::make_unique<MapperBackdrop> (assets);
        addAndMakeVisible (*backdrop);
        header = std::make_unique<HeaderView> (*this);
        chordPanel = std::make_unique<ChordPanel> (*this);
        timeline = std::make_unique<TimelinePanel> (*this);
        browser = std::make_unique<BrowserPanel> (*this);
        medallion = std::make_unique<DragMedallion> (*this);

        // tooltips for anything inside a panel appear beside the panel, not over the work in it
        for (Component* c : { (Component*) chordPanel.get(), (Component*) timeline.get(), (Component*) browser.get() })
            TipWindow::markTipArea (*c);

        for (Component* c : { (Component*) header.get(), (Component*) chordPanel.get(), (Component*) timeline.get(), (Component*) browser.get(),
                              (Component*) medallion.get(), (Component*) &switchButton, (Component*) &tapButton, (Component*) &captureButton,
                              (Component*) &songButton, (Component*) &helpButton, (Component*) &volume, (Component*) &playStyleBox, (Component*) &meterBox,
                              (Component*) &octaveBox, (Component*) &lockKey, (Component*) &clearSong })
            addAndMakeVisible (c);

        playStyleBox.setTooltip ("How chords are played, in the preview and in the MIDI you drag into your DAW:\n"
                                 "BLOCK: held chords.  PULSE: straight 8ths.  STRUM: a down-up guitar strum.  ARPEGGIO: one note at a time.");
        playStyleBox.onChange = [this] (int i) { song.playStyle = (sm::PlayStyle) i; song.changed(); };
        meterBox.setTooltip ("Time signature of the song (also written into the MIDI)");
        meterBox.onChange = [this] (int i) { song.timeSignature = i; song.changed(); };
        octaveBox.setTooltip ("Moves the chords up or down by octaves, in the preview and in the MIDI. The bass and lead rows keep their own notes. Double-click for normal.");
        octaveBox.onChange = [this] (int i) { song.octave = i - 2; song.changed(); };
        lockKey.setTooltip ("Lock Key: keep the Song Mapper in this key. Off, it follows what Key Compass hears, or the key your chords are in");
        lockKey.onClick = [this] { setLockKey (lockKey.getToggleState()); };
        clearSong.setTooltip ("Clear Song: removes the chords and the bass and lead notes from every section and keeps the sections (Ctrl+Z brings them back)");
        clearSong.setMouseCursor (MouseCursor::PointingHandCursor);
        clearSong.onClick = [this] { stopPlayback(); song.clearAllChords(); };
        syncControls();

        header->setTooltip ("Click KEY to choose a key, or to let it follow (what Key Compass hears, or the key of your chords). Drag or scroll the TEMPO, or click its bottom line for options.");
        switchButton.setTooltip ("Back to the compass");
        switchButton.onClick = [this] { stopPlayback(); if (onSwitchToCompass) onSwitchToCompass(); };
        tapButton.setTooltip ("Tap in time to set the tempo");
        tapButton.onClick = [this] { tapTempo(); };
        captureButton.setTooltip ("Copy the chords Key Compass heard into the progression");
        captureButton.onClick = [this] { captureDetectedChords(); };
        songButton.setTooltip ("Undo / redo, start a new song from a template, rename the song, save a MIDI file");
        setWantsKeyboardFocus (true);
        songButton.onClick = [this] { showSongMenu(); };
        helpButton.fontSize = 26.0f;
        helpButton.setTooltip ("Open the manual (F1). It opens as a PDF in your PDF viewer or web browser.");
        helpButton.onClick = [] { openManual(); };

        volume.setSliderStyle (Slider::LinearHorizontal);
        volume.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.0, 0.01);
        volume.setValue (song.auditionVolume, dontSendNotification);
        volume.setColour (Slider::trackColourId, Palette::gold);
        volume.setColour (Slider::backgroundColourId, Colour (0xff17110c));
        volume.setColour (Slider::thumbColourId, Palette::goldLight);
        volume.setTooltip ("Audition volume");
        volume.onValueChange = [this]
        {
            song.auditionVolume = (float) volume.getValue();
            proc.getAudition().setVolume (song.auditionVolume);
        };

        // phone / tablet layouts: undo and redo either side of the KEY / TEMPO plaque (there is no keyboard)
        for (auto* b : { &undoButton, &redoButton })
        {
            b->fontSize = 26.0f;
            addChildComponent (*b);
        }
        undoButton.onClick = [this] { song.undo(); };
        redoButton.onClick = [this] { song.redo(); };

        song.addChangeListener (this);
        if (! song.tray.empty()) focused = song.tray[(size_t) jmax (0, song.selectedTray)];
        resized();
    }

    SongMapperPage::~SongMapperPage()
    {
        song.removeChangeListener (this);
    }

    void SongMapperPage::setMobileLayout (const MobileLayout& l)
    {
        mobile = l;
        const bool m = mobile.active;
        // what the painted page carries is not used on a phone: the app has tabs and a settings page instead
        for (Component* c : { (Component*) backdrop.get(), (Component*) medallion.get(), (Component*) &switchButton, (Component*) &tapButton,
                              (Component*) &captureButton, (Component*) &songButton, (Component*) &helpButton, (Component*) &volume,
                              (Component*) &playStyleBox, (Component*) &meterBox, (Component*) &octaveBox, (Component*) &lockKey,
                              (Component*) &clearSong })
            c->setVisible (! m);
        undoButton.setVisible (m && ! mobile.header.isEmpty());
        redoButton.setVisible (m && ! mobile.header.isEmpty());
        for (Component* c : { (Component*) header.get(), (Component*) chordPanel.get(), (Component*) timeline.get(), (Component*) browser.get() })
        {
            c->setTransform ({});
            c->setVisible (true);
        }
        resized();
        refreshAll();
    }

    void SongMapperPage::layoutMobile()
    {
        const float s = jmax (0.2f, mobile.scale);
        auto place = [s] (Component& c, Rectangle<int> area)
        {
            c.setVisible (! area.isEmpty());
            if (area.isEmpty()) return;
            c.setTransform ({});
            c.setBounds (0, 0, roundToInt ((float) area.getWidth() / s), roundToInt ((float) area.getHeight() / s));
            c.setTransform (AffineTransform::scale (s).translated ((float) area.getX(), (float) area.getY()));
        };
        place (*chordPanel, mobile.chords);
        place (*timeline, mobile.timeline);
        place (*browser, mobile.browser);

        // the KEY / TEMPO plaque keeps its shape, with round undo / redo buttons either side
        auto h = mobile.header;
        undoButton.setVisible (! h.isEmpty());
        redoButton.setVisible (! h.isEmpty());
        header->setVisible (! h.isEmpty());
        if (! h.isEmpty())
        {
            const int btn = jmin (h.getHeight(), 52);
            undoButton.setBounds (h.removeFromLeft (btn).withSizeKeepingCentre (btn, btn));
            redoButton.setBounds (h.removeFromRight (btn).withSizeKeepingCentre (btn, btn));
            h.reduce (6, 0);
            const float hs = jmin ((float) h.getWidth() / (float) headerArea.getWidth(), (float) h.getHeight() / (float) headerArea.getHeight());
            const int w = roundToInt ((float) headerArea.getWidth() * hs), ht = roundToInt ((float) headerArea.getHeight() * hs);
            header->setTransform ({});
            header->setBounds (0, 0, headerArea.getWidth(), headerArea.getHeight());
            header->setTransform (AffineTransform::scale (hs).translated ((float) (h.getCentreX() - w / 2), (float) (h.getCentreY() - ht / 2)));
        }
        header->toFront (false);
        undoButton.toFront (false);
        redoButton.toFront (false);
        undoButton.setEnabled (song.canUndo());
        redoButton.setEnabled (song.canRedo());
    }

    void SongMapperPage::resized()
    {
        if (header == nullptr) return;
        if (mobile.active)
        {
            layoutMobile();
            return;
        }
        backdrop->setBounds (getLocalBounds());
        header->setBounds (headerArea);
        chordPanel->setBounds (chordArea);
        timeline->setBounds (timelineArea);
        browser->setBounds (browserArea);
        medallion->setBounds (medallionArea);
        switchButton.setBounds (20, 30, 226, 50);
        tapButton.setBounds (20, 86, 226, 46);
        captureButton.setBounds (20, 138, 226, 46);
        songButton.setBounds (20, 190, 226, 46);
        helpButton.setBounds (812, 22, 48, 48);
        volume.setBounds (914, 213, 146, 30);
        playStyleBox.setBounds (Layout::modeBox);
        meterBox.setBounds (Layout::responseBox);
        octaveBox.setBounds (Layout::tuningBox);
        lockKey.setBounds (Layout::lockButton);
        clearSong.setBounds (Layout::clearButton);
    }

    void SongMapperPage::visibilityChanged()
    {
        if (isVisible()) startTimerHz (30);
        else
        {
            stopTimer();
            closePractice();
        }
    }

    void SongMapperPage::paint (Graphics&)
    {
        // everything static is in the backdrop (rendered artwork); see MapperBackdrop
    }

    //==============================================================================
    SongMapperPage::KeySource SongMapperPage::keySource (sm::Key* key, bool ignoreLock) const
    {
        // Key Compass only hears what comes into the track (an instrument, a mic, MIDI), never the
        // Song Mapper's own playback, so with nothing coming in the key is read from the song's chords.
        static constexpr double heardHoldMs = 4000.0;     // keep following the ears through short pauses
        const auto snap = proc.getEngine().getSnapshot();
        const double now = Time::getMillisecondCounterHiRes();
        if (snap.hasKey && snap.signalPresent) lastHeardMs = now;
        const bool hearing = snap.hasKey && now - lastHeardMs < heardHoldMs;

        auto fromIndex = [] (int i) { return sm::Key { i % 12, i >= 12 }; };
        sm::Key k { 0, false }, chordKey;
        KeySource src = KeySource::None;
        if (song.keyOverride >= 0 && ! ignoreLock)  { k = fromIndex (song.keyOverride); src = KeySource::Set; }
        else if (hearing)                           { k = { snap.key.tonic, snap.key.minor }; src = KeySource::Heard; }
        else if (song.keyFromChords (chordKey))     { k = chordKey; src = KeySource::Chords; }
        else if (snap.hasKey)                       { k = { snap.key.tonic, snap.key.minor }; src = KeySource::Heard; }
        else if (song.lastSetKey >= 0)              { k = fromIndex (song.lastSetKey); src = KeySource::Kept; }
        if (key != nullptr) *key = k;
        return src;
    }

    sm::Key SongMapperPage::currentKey() const
    {
        sm::Key k;
        keySource (&k);
        return k;
    }

    bool SongMapperPage::keyIsDetected() const
    {
        return keySource() == KeySource::Heard;
    }

    bool SongMapperPage::keyKnown() const
    {
        return keySource() != KeySource::None;
    }

    bool SongMapperPage::keyUsesFlats (int tonic, bool minor) noexcept
    {
        // F Bb Eb Ab Db major / D G C F Bb Eb minor are written with flats
        static const bool majorFlat[12] = { false, true, false, true, false, true, false, false, true, false, true, false };
        static const bool minorFlat[12] = { true, false, true, true, false, true, false, true, false, false, true, false };
        tonic = ((tonic % 12) + 12) % 12;
        return minor ? minorFlat[tonic] : majorFlat[tonic];
    }

    bool SongMapperPage::useFlats() const
    {
        if (flats) return true;
        if (! keyKnown()) return false;
        const auto k = currentKey();
        return keyUsesFlats (k.tonic, k.minor);
    }

    String SongMapperPage::keyText() const
    {
        if (! keyKnown()) return "C MAJOR";
        const auto k = currentKey();
        return utf8 (sm::pcName (k.tonic, useFlats())) + (k.minor ? " MINOR" : " MAJOR");
    }

    double SongMapperPage::effectiveBpm() const
    {
        const double host = proc.getHostBpm();
        return (song.followHostTempo && host > 0.0) ? host : song.manualBpm;
    }

    bool SongMapperPage::tempoFromHost() const
    {
        return song.followHostTempo && proc.getHostBpm() > 0.0;
    }

    void SongMapperPage::focusChord (const sm::Chord& c, bool audition)
    {
        focused = c;
        if (audition) auditionChord (c);
        chordPanel->repaint();
    }

    void SongMapperPage::auditionNote (int midiNote, int sound)
    {
        proc.getAudition().setVolume (song.auditionVolume);
        lastAuditionedNote = midiNote;
        const auto instrument = sound == 1 ? sm::Instrument::Bass
                              : (sound == 0 ? sm::Instrument::Piano
                                            : (sound == 2 || song.guitar ? sm::Instrument::Guitar : sm::Instrument::Piano));
        proc.getAudition().playChord ({ midiNote }, instrument, sound == 1 ? 1.2 : 1.6);
    }

    void SongMapperPage::auditionChord (const sm::Chord& c)
    {
        proc.getAudition().setVolume (song.auditionVolume);
        proc.getAudition().playChord (song.playedNotes (c), song.guitar, 1.8);
    }

    void SongMapperPage::playSong (int onlySection)
    {
        playScope = onlySection < 0 ? -1 : onlySection;
        proc.getAudition().setVolume (song.auditionVolume);
        proc.getAudition().playSequence (song.buildSequence (onlySection, playLanes(), true), effectiveBpm(), song.loopPlayback, song.guitar);
    }

    void SongMapperPage::refreshPlayback()
    {
        // an edit while the song (or a section) plays: carry on playing with the change, from the same place
        if (! isPlaying()) return;
        if (playScope == -2)
        {
            // the progression playing (not a browser preview): carry on with it as it is now (rearranged, say)
            const auto* seq = proc.getAudition().currentSequence();
            const bool trayPlaying = seq != nullptr && std::any_of (seq->events.begin(), seq->events.end(),
                                                                    [] (const sm::SequenceEvent& e) { return e.trayIndex >= 0; });
            if (! trayPlaying) return;
            if (song.tray.empty()) { stopPlayback(); return; }
            proc.getAudition().updateSequence (song.buildTraySequence(), effectiveBpm(), song.loopPlayback, song.guitar);
            return;
        }
        if (playScope < -1) return;
        if (playScope >= (int) song.sections.size())
        {
            stopPlayback();
            return;
        }
        proc.getAudition().updateSequence (song.buildSequence (playScope, playLanes(), true), effectiveBpm(), song.loopPlayback, song.guitar);
    }

    void SongMapperPage::playTray()
    {
        playScope = -2;
        proc.getAudition().setVolume (song.auditionVolume);
        proc.getAudition().playSequence (song.buildTraySequence(), effectiveBpm(), song.loopPlayback, song.guitar);
    }

    void SongMapperPage::playProgression (const std::vector<sm::Chord>& chords)
    {
        playScope = -2;
        proc.getAudition().setVolume (song.auditionVolume);
        proc.getAudition().playSequence (song.sequenceFor (chords), effectiveBpm(), false, song.guitar);
    }

    bool SongMapperPage::isNotePlaying (int section, sm::Lane lane, int noteIndex) const
    {
        const int key = (section << 16) | ((int) lane << 12) | (noteIndex & 0xfff);
        return std::find (playingNotes.begin(), playingNotes.end(), key) != playingNotes.end();
    }

    double SongMapperPage::sectionPlayhead (int section) const
    {
        if (section < 0 || section != playingSection || ! isPlaying() || playScope < -1) return -1.0;
        const auto* seq = proc.getAudition().currentSequence();
        if (seq == nullptr) return -1.0;
        double offset = 0.0;
        return seq->sectionAt (proc.getAudition().positionBeats(), &offset) == section ? offset : -1.0;
    }

    void SongMapperPage::stopPlayback()
    {
        proc.getAudition().stop();
    }

    bool SongMapperPage::isPlaying() const
    {
        return proc.getAudition().isSequencePlaying();
    }

    void SongMapperPage::tapTempo()
    {
        const double now = Time::getMillisecondCounterHiRes() / 1000.0;
        if (! taps.empty() && now - taps.back() > 2.0) taps.clear();
        taps.push_back (now);
        if (taps.size() > 5) taps.erase (taps.begin());
        if (taps.size() >= 2)
        {
            const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
            song.manualBpm = jlimit (30.0, 300.0, std::round (60.0 / avg));
            song.followHostTempo = false;
            song.changed();
        }
    }

    void SongMapperPage::captureDetectedChords()
    {
        const auto snap = proc.getEngine().getSnapshot();
        std::vector<sm::Chord> out;
        for (int i = 0; i < snap.historySize; ++i)
        {
            const auto& h = snap.history[(size_t) i];
            if (h.kind != kc::HistoryEntry::Kind::Chord || h.chord.isSingleNote || ! h.chord.isValid()) continue;
            static const char* const map[] = { "maj", "m", "dim", "aug", "sus2", "sus4", "7", "maj7", "m7", "5" };
            sm::Chord c;
            c.root = h.chord.root;
            c.quality = sm::qualityIndex (map[(int) h.chord.type]);
            c.bass = h.chord.bass;
            if (out.empty() || out.back() != c) out.push_back (c);
        }
        if (out.size() > 8) out.erase (out.begin(), out.end() - 8);
        if (out.empty()) return;
        song.setTray (out);
        focusChord (out.back(), false);
    }

    void SongMapperPage::addToSelectedSection (const std::vector<sm::Chord>& chords)
    {
        if (chords.empty()) return;
        if (song.sections.empty()) song.addSection ("Verse 1", 8);
        const int s = juce::isPositiveAndBelow (song.selectedSection, (int) song.sections.size()) ? song.selectedSection : 0;
        song.insertChords (s, chords);
    }

    File SongMapperPage::midiFile (int onlySection, int laneMask) const
    {
        return song.writeMidiFile (onlySection, effectiveBpm(), File::getSpecialLocation (File::tempDirectory).getChildFile ("KeyCompass"),
                                   laneMask < 0 ? visibleLanes() : laneMask);
    }

    void SongMapperPage::setLockKey (bool lock)
    {
        if (lock)
        {
            const auto k = currentKey();
            song.setKeyOverride (k.tonic + (k.minor ? 12 : 0));
        }
        else
            song.setKeyOverride (-1);
        song.changed();
    }

    void SongMapperPage::shareMidi (int onlySection, int laneMask)
    {
        const auto f = midiFile (onlySection, laneMask);
        if (! f.existsAsFile()) return;
        shareBox = ContentSharer::shareFilesScoped ({ URL (f) }, [] (bool, const String&) {}, this);
    }

    String SongMapperPage::songFileXml() const
    {
        ValueTree root ("KeyCompassSong");
        root.setProperty ("version", KC_VERSION_STRING, nullptr);
        root.appendChild (song.toValueTree(), nullptr);
        return root.toXmlString();
    }

    File SongMapperPage::songFileFor (const String& name) const
    {
        auto safe = File::createLegalFileName (name.trim()).trim();
        if (safe.isEmpty()) safe = "Song";
        return File::getSpecialLocation (File::tempDirectory).getChildFile ("KeyCompass").getChildFile (safe + ".kcsong");
    }

    void SongMapperPage::shareSongFile()
    {
        auto f = songFileFor (song.songName);
        f.getParentDirectory().createDirectory();
        if (! f.replaceWithText (songFileXml())) return;
        shareBox = ContentSharer::shareFilesScoped ({ URL (f) }, [] (bool, const String&) {}, this);
    }

    void SongMapperPage::saveSongFile()
    {
        if (isTouchUI()) { shareSongFile(); return; }
        const auto start = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (songFileFor (song.songName).getFileName());
        chooser = std::make_unique<FileChooser> ("Save the song (to open it in Key Compass on another computer or phone)", start, "*.kcsong");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                              [safe = SafePointer<SongMapperPage> (this)] (const FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  auto dest = fc.getResult();
                                  if (dest == File()) return;
                                  if (! dest.hasFileExtension ("kcsong")) dest = dest.withFileExtension ("kcsong");
                                  dest.replaceWithText (safe->songFileXml());
                              });
    }

    void SongMapperPage::openSongFile()
    {
        // on Android the picker hands back a content:// address, so the file is read through its URL
        chooser = std::make_unique<FileChooser> ("Open a Key Compass song",
                                                 File::getSpecialLocation (File::userDocumentsDirectory),
                                                 isTouchUI() ? String ("*") : String ("*.kcsong"));
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [safe = SafePointer<SongMapperPage> (this)] (const FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  const auto url = fc.getURLResult();
                                  if (url.isEmpty()) return;
                                  String text;
                                  if (url.isLocalFile())
                                      text = url.getLocalFile().loadFileAsString();
                                  else if (auto in = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)))
                                      text = in->readEntireStreamAsString();
                                  if (! safe->loadSongXml (text))
                                      AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, "Open song",
                                                                        "That file isn't a Key Compass song (.kcsong).");
                              });
    }

    bool SongMapperPage::loadSongXml (const String& text)
    {
        const auto xml = parseXML (text);
        if (xml == nullptr) return false;
        const auto tree = ValueTree::fromXml (*xml);
        const auto map = tree.hasType ("SongMap") ? tree : tree.getChildWithName ("SongMap");
        if (! map.isValid()) return false;
        stopPlayback();
        closePractice();
        song.checkpoint();                 // Undo brings back the sections and chords you had before
        song.fromValueTree (map);
        focused = song.tray.empty() ? sm::Chord() : song.tray[(size_t) jlimit (0, (int) song.tray.size() - 1, song.selectedTray)];
        song.changed();
        return true;
    }

    void SongMapperPage::exportMidi (int onlySection)
    {
        if (isTouchUI()) { shareMidi (onlySection); return; }
        const auto src = midiFile (onlySection);
        chooser = std::make_unique<FileChooser> ("Save MIDI file", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (src.getFileName()), "*.mid");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                              [src] (const FileChooser& fc)
                              {
                                  auto dest = fc.getResult();
                                  if (dest == File()) return;
                                  if (! dest.hasFileExtension ("mid")) dest = dest.withFileExtension ("mid");
                                  src.copyFileTo (dest);
                              });
    }

    void SongMapperPage::saveTrayAsFavourite()
    {
        if (song.tray.empty()) return;
        const auto k = currentKey();
        StringArray rn, names;
        for (auto& c : song.tray)
        {
            rn.add (sm::chordToRoman (c, k.tonic));
            names.add (c.name (false));
        }
        favourites->add ({ names.joinIntoString (" - "), rn.joinIntoString (" "), "My progressions" });
        song.showFavourites = true;
        song.changed();
    }

    void SongMapperPage::fillChordMenu (PopupMenu& m, int idBase, std::vector<sm::Chord>& out) const
    {
        const auto k = currentKey();
        auto add = [&] (PopupMenu& menu, const sm::Chord& c, const String& roman)
        {
            menu.addItem (idBase + (int) out.size(), chordText (c) + "   (" + roman + ")");
            out.push_back (c);
        };
        m.addSectionHeader ("In " + keyText().toLowerCase());
        for (auto& c : sm::diatonicChords (k, false)) add (m, c, utf8 (sm::chordToRoman (c, k.tonic)));
        PopupMenu sev, borrowed, roots;
        for (auto& c : sm::diatonicChords (k, true)) add (sev, c, utf8 (sm::chordToRoman (c, k.tonic)));
        m.addSubMenu ("Sevenths", sev);
        const char* const borrowedMajor[] = { "iv", "bVII", "bVI", "bIII", "II7", "III7", "VI7" };
        const char* const borrowedMinor[] = { "V", "V7", "IV", "bII", "viidim7", "I" };
        if (k.minor) for (auto* n : borrowedMinor) add (borrowed, sm::romanToChord (n, k.tonic), n);
        else         for (auto* n : borrowedMajor) add (borrowed, sm::romanToChord (n, k.tonic), n);
        m.addSubMenu ("Borrowed & secondary", borrowed);
        // any chord: root -> quality
        for (int pc = 0; pc < 12; ++pc)
        {
            PopupMenu q;
            for (int qi = 0; qi < (int) sm::qualities().size(); ++qi)
            {
                sm::Chord c;
                c.root = pc;
                c.quality = qi;
                q.addItem (idBase + (int) out.size(), chordText (c));
                out.push_back (c);
            }
            roots.addSubMenu (utf8 (sm::pcName (pc, useFlats())), q);
        }
        m.addSubMenu ("Any chord", roots);
    }

    void SongMapperPage::showSongMenu()
    {
        PopupMenu m, templates;
        m.setLookAndFeel (&getLookAndFeel());
        const String cmd = (SystemStats::getOperatingSystemType() & SystemStats::MacOSX) != 0 ? "Cmd" : "Ctrl";
        m.addItem (1, "Undo", song.canUndo());
        m.addItem (2, "Redo", song.canRedo());
        m.addSeparator();
        templates.addItem (11, "Pop  (Intro, Verse, Pre-Chorus, Chorus x3, Bridge, Outro)");
        templates.addItem (12, "Rock  (Intro, Verse, Chorus, Verse, Chorus, Solo, Chorus, Outro)");
        templates.addItem (13, "Hip-hop  (Intro, Hook, Verse, Hook, Verse, Hook, Outro)");
        templates.addItem (14, "EDM  (Intro, Build, Drop, Breakdown, Build, Drop, Outro)");
        templates.addItem (15, "Worship  (Intro, Verse, Chorus, Verse, Chorus, Bridge, Chorus, Outro)");
        templates.addItem (16, "Country  (Intro, Verse, Chorus, Verse, Chorus, Bridge, Chorus, Outro)");
        templates.addSeparator();
        templates.addItem (19, "Empty song");
        m.addSubMenu ("New song from a template", templates);
        m.addItem (3, String (CharPointer_UTF8 ("Rename song\xe2\x80\xa6  (")) + song.songName + ")");
        m.addItem (4, String (CharPointer_UTF8 (isTouchUI() ? "Share the song as MIDI\xe2\x80\xa6" : "Save MIDI file\xe2\x80\xa6")));
        m.addItem (7, String (CharPointer_UTF8 (isTouchUI() ? "Share the song file\xe2\x80\xa6  (open it on your PC)"
                                                            : "Save song file\xe2\x80\xa6  (open it on your phone)")));
        m.addItem (8, String (CharPointer_UTF8 ("Open a song file\xe2\x80\xa6")));
        m.addItem (6, isTouchUI() ? "Open the manual" : "Open the manual  (F1)");
        m.addSeparator();
        m.addItem (5, "Clear the progression", ! song.tray.empty());
        if (! isTouchUI()) m.addSectionHeader (cmd + "+Z undoes, " + cmd + "+Shift+Z redoes");
        m.addSectionHeader (versionText());
        auto target = songButton.isShowing() ? songButton.getScreenBounds() : Rectangle<int> (Desktop::getMousePosition(), Desktop::getMousePosition()).expanded (2);
        showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (target).withStandardItemHeight (menuItemHeight()),
                         [this, safe = SafePointer<SongMapperPage> (this)] (int r)
                         {
                             if (safe == nullptr || r == 0) return;
                             if (r == 1) { song.undo(); return; }
                             if (r == 2) { song.redo(); return; }
                             if (r == 3) { renameSong(); return; }
                             if (r == 4) { exportMidi (-1); return; }
                             if (r == 5) { song.clearTray(); return; }
                             if (r == 6) { openManual(); return; }
                             if (r == 7) { saveSongFile(); return; }
                             if (r == 8) { openSongFile(); return; }
                             std::vector<std::pair<String, int>> t;
                             if (r == 11) t = { { "Intro", 4 }, { "Verse", 8 }, { "Pre-Chorus", 4 }, { "Chorus", 8 }, { "Verse", 8 }, { "Pre-Chorus", 4 }, { "Chorus", 8 }, { "Bridge", 8 }, { "Chorus", 8 }, { "Outro", 4 } };
                             if (r == 12) t = { { "Intro", 4 }, { "Verse", 8 }, { "Chorus", 8 }, { "Verse", 8 }, { "Chorus", 8 }, { "Solo", 8 }, { "Chorus", 8 }, { "Outro", 4 } };
                             if (r == 13) t = { { "Intro", 4 }, { "Hook", 8 }, { "Verse", 16 }, { "Hook", 8 }, { "Verse", 16 }, { "Hook", 8 }, { "Outro", 4 } };
                             if (r == 14) t = { { "Intro", 8 }, { "Build", 8 }, { "Drop", 16 }, { "Breakdown", 8 }, { "Build", 8 }, { "Drop", 16 }, { "Outro", 8 } };
                             if (r == 15) t = { { "Intro", 4 }, { "Verse", 8 }, { "Chorus", 8 }, { "Verse", 8 }, { "Chorus", 8 }, { "Bridge", 8 }, { "Chorus", 8 }, { "Outro", 4 } };
                             if (r == 16) t = { { "Intro", 4 }, { "Verse", 16 }, { "Chorus", 8 }, { "Verse", 16 }, { "Chorus", 8 }, { "Bridge", 8 }, { "Chorus", 8 }, { "Outro", 4 } };
                             if (r >= 11 && r <= 19)
                             {
                                 stopPlayback();
                                 song.resetSections (t);
                             }
                         });
    }

    void SongMapperPage::renameSong()
    {
        auto* w = new AlertWindow ("Rename song", "This name is used for the MIDI you drag into your DAW.", MessageBoxIconType::NoIcon, this);
        w->setLookAndFeel (&getLookAndFeel());
        w->addTextEditor ("name", song.songName, "Name:");
        w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([safe = SafePointer<SongMapperPage> (this), w] (int result)
        {
            if (result == 1 && safe != nullptr)
            {
                const auto name = w->getTextEditorContents ("name").trim().substring (0, 60);
                if (name.isNotEmpty())
                {
                    safe->song.songName = name;
                    safe->song.changed();
                }
            }
        }), true);
    }

    bool SongMapperPage::keyPressed (const KeyPress& key)
    {
        const auto mods = key.getModifiers();
        if (mods.isCommandDown() && key.getKeyCode() == 'Z')
        {
            if (mods.isShiftDown()) song.redo(); else song.undo();
            return true;
        }
        if (mods.isCommandDown() && key.getKeyCode() == 'Y')
        {
            song.redo();
            return true;
        }
        if (key.isKeyCode (KeyPress::F1Key))
        {
            openManual();
            return true;
        }
        if (key.isKeyCode (KeyPress::spaceKey) && proc.wrapperType == AudioProcessor::wrapperType_Standalone)   // in a DAW, space belongs to the DAW
        {
            if (isPlaying()) stopPlayback(); else playSong (-1);
            return true;
        }
        return false;
    }

    void SongMapperPage::takeFocus()
    {
        if (isShowing() && ! hasKeyboardFocus (true))
            grabKeyboardFocus();
    }

    void SongMapperPage::setFlats (bool f)
    {
        if (f == flats) return;
        flats = f;
        refreshAll();
    }

    void SongMapperPage::refreshAll()
    {
        timeline->refresh();
        browser->refresh();
        if (practice != nullptr) practice->refresh();
        repaint();
    }

    void SongMapperPage::showPractice (int source)
    {
        if (practice == nullptr) practice = std::make_unique<PracticeView> (*this);
        // above everything on the page, including the shared plates the editor owns
        auto* host = getParentComponent() != nullptr ? getParentComponent() : this;
        if (practiceHost) if (auto* h = practiceHost()) host = h;
        host->addAndMakeVisible (*practice);
        if (practiceHost && host != this && host != getParentComponent())
            practice->setBounds (host->getLocalBounds());       // the app sizes it to fit the screen (see MobileEditor)
        else
            practice->setBounds (host == this ? getLocalBounds() : getBounds());
        practice->setSource (source);
        practice->toFront (true);
        practice->grabKeyboardFocus();
    }

    void SongMapperPage::closePractice()
    {
        if (practice == nullptr) return;
        practice->setVisible (false);
        // it may be closing from inside one of its own buttons: delete it afterwards
        MessageManager::callAsync ([safe = SafePointer<SongMapperPage> (this)]
        {
            if (safe == nullptr) return;
            safe->practice.reset();
            safe->takeFocus();
        });
    }

    Component* SongMapperPage::practiceView() const noexcept { return practice.get(); }

    int SongMapperPage::practiceSource() const
    {
        return practice != nullptr && practice->isVisible() ? practice->getSource() : -100;
    }

    void SongMapperPage::syncControls()
    {
        playStyleBox.setIndex ((int) song.playStyle);
        meterBox.setIndex (song.timeSignature);
        octaveBox.setIndex (song.octave + 2);
        lockKey.setToggleState (song.keyOverride >= 0, dontSendNotification);
    }

    void SongMapperPage::changeListenerCallback (ChangeBroadcaster*)
    {
        syncControls();
        undoButton.setEnabled (song.canUndo());
        redoButton.setEnabled (song.canRedo());
        if (song.selectedTray >= 0 && song.selectedTray < (int) song.tray.size() && ! focused.isValid())
            focused = song.tray[(size_t) song.selectedTray];
        volume.setValue (song.auditionVolume, dontSendNotification);
        refreshAll();
        refreshPlayback();
    }

    void SongMapperPage::showSection (int index)
    {
        timeline->showSection (index);
    }

    void SongMapperPage::timerCallback()
    {
        updatePlaybackDisplay();

        // key / tempo changes coming from the detector or the DAW
        sm::Key k;
        const auto src = keySource (&k);
        const int keyIdx = (src != KeySource::None ? k.tonic + (k.minor ? 12 : 0) : -1) + 100 * (int) src;   // a new source changes the header too
        const double bpm = effectiveBpm();
        if (keyIdx != lastKeyIndex || std::abs (bpm - lastBpm) > 0.01)
        {
            const bool tempoChanged = std::abs (bpm - lastBpm) > 0.01 && lastBpm > 0.0;
            lastKeyIndex = keyIdx;
            lastBpm = bpm;
            if (tempoChanged) refreshPlayback();
            repaint();      // chord spellings, browser chords, header, timings
            timeline->list.repaintRows();
            browser->repaintRows();
            chordPanel->grid.repaint();
        }
    }

    void SongMapperPage::updatePlaybackDisplay()
    {
        // playback highlight, from the play position: the chord sounding, the bass / lead notes sounding, the section
        int ps = -1, pc = -1, pt = -1;
        std::vector<int> notesNow;
        auto& au = proc.getAudition();
        const auto* seq = au.currentSequence();
        const bool playing = au.isSequencePlaying() && seq != nullptr;
        if (playing)
        {
            const double pos = au.positionBeats();
            const sm::SequenceEvent* lastChord = nullptr;
            for (const auto& e : seq->events)
            {
                if (e.startBeat > pos + 1.0e-9) break;      // events are in time order
                if (e.lane == 0)
                    lastChord = &e;
                else if (pos < e.startBeat + e.lengthBeats && e.noteIndex >= 0 && e.section >= 0)
                    notesNow.push_back ((e.section << 16) | (e.lane << 12) | (e.noteIndex & 0xfff));
            }
            if (playScope >= -1)
                ps = seq->sectionAt (pos);
            if (lastChord != nullptr)
            {
                if (lastChord->trayIndex >= 0) pt = lastChord->trayIndex;
                else if (lastChord->section >= 0 && lastChord->section == ps) pc = lastChord->chordIndex;
            }
        }
        const bool chordsChanged = ps != playingSection || pc != playingChord || pt != playingTray;
        playingSection = ps;
        playingChord = pc;
        playingTray = pt;
        playingNotes = std::move (notesNow);
        if (chordsChanged)
        {
            timeline->list.repaintRows();
            timeline->repaint();
            chordPanel->repaint();
            chordPanel->grid.repaint();
        }
        else if (playing && ps >= 0)
            timeline->list.repaintLanes (ps);       // the playhead moves through the bass / lead rows
    }
}
