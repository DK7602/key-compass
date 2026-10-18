// Key Compass - display layers
#include "CompassViews.h"

namespace kcui
{
    using namespace juce;
    using kc::KeyId;

    static constexpr float pi = MathConstants<float>::pi;
    static constexpr float step30 = pi / 6.0f;

    static String toJuce (const std::string& s)  { return String (CharPointer_UTF8 (s.c_str())); }

    //==============================================================================
    // StaticLayer
    //==============================================================================
    StaticLayer::StaticLayer (Assets& a) : assets (a)
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);
    }

    void StaticLayer::paint (Graphics& g)
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (assets.background, Rectangle<float> (0, 0, (float) Layout::designWidth, (float) Layout::designHeight));

        const Point<float> c (Layout::dialCx, Layout::dialCy);
        const float R = Layout::faceRadius;

        // parchment face (replaces the painted face, which had a needle and letters baked in)
        {
            Graphics::ScopedSaveState save (g);
            Path circle;
            circle.addEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
            g.reduceClipRegion (circle);
            g.drawImage (assets.face, Rectangle<float> (c.x - R, c.y - R, 2.0f * R, 2.0f * R));
        }

        // shadow where the face sits under the bezel
        {
            ColourGradient shade (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.42f), c.x + R, c.y, true);
            shade.addColour ((R - 18.0f) / R, Colours::transparentBlack);
            g.setGradientFill (shade);
            g.fillEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
            g.setColour (Colour (0xff3a2410));
            g.drawEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R, 2.5f);
        }

        paintTickRing (g, c);
        paintRose (g, c);
    }

    void StaticLayer::paintTickRing (Graphics& g, Point<float> c)
    {
        const Colour ink = Palette::ink;
        g.setColour (ink.withAlpha (0.85f));
        g.drawEllipse (c.x - Layout::tickOuter, c.y - Layout::tickOuter, 2 * Layout::tickOuter, 2 * Layout::tickOuter, 1.4f);
        g.drawEllipse (c.x - Layout::tickInner, c.y - Layout::tickInner, 2 * Layout::tickInner, 2 * Layout::tickInner, 1.1f);

        for (int i = 0; i < 180; ++i)
        {
            if (i % 15 == 0) continue;            // note markers go there
            const float a = (float) i * 2.0f * pi / 180.0f;
            const bool major = (i % 5 == 0);
            const float len = major ? 10.0f : 5.5f;
            g.setColour (ink.withAlpha (major ? 0.85f : 0.6f));
            g.drawLine (Line<float> (polar (c, a, Layout::tickOuter), polar (c, a, Layout::tickOuter - len)), major ? 1.5f : 1.0f);
        }

        for (int p = 0; p < 12; ++p)
        {
            const float a = (float) p * step30;
            Path tri;
            const auto tip = polar (c, a, Layout::tickOuter + 2.0f);
            const auto l = polar (c, a - 0.028f, Layout::tickInner - 4.0f);
            const auto r = polar (c, a + 0.028f, Layout::tickInner - 4.0f);
            tri.addTriangle (tip, l, r);
            g.setColour (ink);
            g.fillPath (tri);
        }

        // inner boundary of the letter band
        g.setColour (ink.withAlpha (0.35f));
        g.drawEllipse (c.x - Layout::wedgeInner, c.y - Layout::wedgeInner, 2 * Layout::wedgeInner, 2 * Layout::wedgeInner, 1.0f);
    }

    void StaticLayer::paintRose (Graphics& g, Point<float> c)
    {
        const Colour ink = Palette::ink;

        // decorative degree ring
        const float r1 = Layout::roseRing, r0 = r1 - 7.0f;
        g.setColour (ink.withAlpha (0.55f));
        g.drawEllipse (c.x - r1, c.y - r1, 2 * r1, 2 * r1, 1.2f);
        g.drawEllipse (c.x - r0, c.y - r0, 2 * r0, 2 * r0, 0.9f);
        for (int i = 0; i < 72; ++i)
        {
            const float a = (float) i * 2.0f * pi / 72.0f;
            g.drawLine (Line<float> (polar (c, a, r0), polar (c, a, r1)), 0.8f);
        }

        struct Layer { int count; float offset, length, width; Colour light, dark; };
        const Layer layers[] = {
            { 8, pi / 8.0f, 74.0f,  11.0f, Colour (0xffcaa55a), Colour (0xff75501c) },
            { 4, pi / 4.0f, 104.0f, 15.0f, Colour (0xffdcb866), Colour (0xff875b20) },
            { 4, 0.0f,      136.0f, 19.0f, Colour (0xffeccd7e), Colour (0xff9b6c28) },
        };

        for (const auto& layer : layers)
        {
            for (int i = 0; i < layer.count; ++i)
            {
                const float a = layer.offset + (float) i * 2.0f * pi / (float) layer.count;
                const auto tip = polar (c, a, layer.length);
                const auto left = polar (c, a - pi / 4.0f, layer.width);
                const auto right = polar (c, a + pi / 4.0f, layer.width);

                Path lightHalf, darkHalf;
                lightHalf.addTriangle (c, tip, left);
                darkHalf.addTriangle (c, right, tip);
                g.setColour (layer.light);
                g.fillPath (lightHalf);
                g.setColour (layer.dark);
                g.fillPath (darkHalf);

                Path outline;
                outline.startNewSubPath (left);
                outline.lineTo (tip);
                outline.lineTo (right);
                outline.startNewSubPath (c);
                outline.lineTo (tip);
                g.setColour (Colour (0xff3b2710).withAlpha (0.9f));
                g.strokePath (outline, PathStrokeType (1.0f, PathStrokeType::mitered));
            }
        }

        g.setColour (Colour (0xff3b2710).withAlpha (0.7f));
        g.drawEllipse (c.x - 26.0f, c.y - 26.0f, 52.0f, 52.0f, 1.2f);
    }

    //==============================================================================
    // DialLayer
    //==============================================================================
    DialLayer::DialLayer (Assets& a, const UiState& s) : assets (a), state (s)
    {
        setInterceptsMouseClicks (false, false);
        const int R = (int) Layout::faceRadius;
        setBounds ((int) Layout::dialCx - R, (int) Layout::dialCy - R, 2 * R, 2 * R);
        origin = getPosition().toFloat();
    }

    void DialLayer::paint (Graphics& g)
    {
        g.addTransform (AffineTransform::translation (-origin.x, -origin.y));
        const Point<float> c (Layout::dialCx, Layout::dialCy);

        paintWedge (g, c);
        paintLetters (g, c);
        if (showPlate) paintPlate (g, c, false);
        paintNeedle (g, c);
        paintHub (g, c);
        if (showPlate) paintPlate (g, c, true);
    }

    void DialLayer::paintWedge (Graphics& g, Point<float> c)
    {
        const float alpha = state.wedgeAlpha;
        if (alpha < 0.01f) return;

        const float R2 = Layout::wedgeOuter, R1 = Layout::wedgeInner;
        const float span = 7.0f * step30;

        Path band;
        band.addPieSegment (c.x - R2, c.y - R2, 2 * R2, 2 * R2, state.wedgeAngle - span * 0.5f, state.wedgeAngle + span * 0.5f, R1 / R2);

        const Colour warm (0xffe0a53c);
        ColourGradient fill (warm.withAlpha (0.0f), c.x, c.y, warm.withAlpha (0.50f * alpha), c.x + R2, c.y, true);
        fill.addColour (R1 / R2, warm.withAlpha (0.16f * alpha));
        g.setGradientFill (fill);
        g.fillPath (band);

        g.setColour (Palette::goldDark.withAlpha (0.9f * alpha));
        g.strokePath (band, PathStrokeType (2.2f));

        Path outerArc;
        outerArc.addCentredArc (c.x, c.y, R2 - 2.5f, R2 - 2.5f, 0.0f, state.wedgeAngle - span * 0.5f + 0.01f, state.wedgeAngle + span * 0.5f - 0.01f, true);
        g.setColour (Palette::goldLight.withAlpha (0.55f * alpha));
        g.strokePath (outerArc, PathStrokeType (1.4f));

        if (state.snap.hasKey)
        {
            const float ta = UiState::tonicAngle (state.snap.key);
            Path tonic;
            tonic.addPieSegment (c.x - R2, c.y - R2, 2 * R2, 2 * R2, ta - step30 * 0.5f, ta + step30 * 0.5f, R1 / R2);
            g.setColour (Palette::blue.withAlpha (0.20f * state.keyGlow));
            g.fillPath (tonic);
        }
    }

    void DialLayer::paintLetters (Graphics& g, Point<float> c)
    {
        const auto& s = state.snap;
        const Font font = assets.bask (46.0f);

        for (int p = 0; p < 12; ++p)
        {
            const int pc = kc::pitchClassAtCircle (p);
            const auto pos = polar (c, (float) p * step30, Layout::letterRadius);
            const bool known = s.hasKey;
            const bool inKey = known && kc::isInKey (s.key, pc);          // incl. raised 7th in minor
            const int rel = known ? ((kc::circlePosition (pc) - kc::keyWedgeStart (s.key)) % 12 + 12) % 12 : 0;
            const bool inWedge = known && rel < 7;
            const bool tonic = known && s.key.tonic == pc;
            const float glow = state.noteGlow[(size_t) pc];

            if (tonic)
            {
                const float pulse = 0.85f + 0.15f * std::sin (state.time * 2.4f) * (s.signalPresent ? 1.0f : 0.0f);
                const float hr = 52.0f;
                ColourGradient halo (Colour (0xffe8f7ff).withAlpha (jmin (1.0f, 1.0f * state.keyGlow * pulse)), pos.x, pos.y,
                                     Palette::blue.withAlpha (0.0f), pos.x + hr, pos.y, true);
                halo.addColour (0.38, Palette::blueLight.withAlpha (jmin (1.0f, 1.0f * state.keyGlow * pulse)));
                halo.addColour (0.70, Palette::blue.withAlpha (0.55f * state.keyGlow * pulse));
                g.setGradientFill (halo);
                g.fillEllipse (pos.x - hr, pos.y - hr, 2.0f * hr, 2.0f * hr);
            }

            if (glow > 0.02f)
            {
                const Colour gc = (! known || inKey) ? Colour (0xffffd772) : Colour (0xffff5a3c);
                ColourGradient halo (gc.withAlpha (0.9f * glow), pos.x, pos.y, gc.withAlpha (0.0f), pos.x + 38.0f, pos.y, true);
                g.setGradientFill (halo);
                g.fillEllipse (pos.x - 38.0f, pos.y - 38.0f, 76.0f, 76.0f);
            }

            if (s.showRunnerUp && ! state.locked && s.runnerUp.tonic == pc && ! tonic)
            {
                Path ring, dashed;
                ring.addEllipse (pos.x - 32.0f, pos.y - 32.0f, 64.0f, 64.0f);
                const float dashes[] = { 7.0f, 5.0f };
                PathStrokeType (2.2f).createDashedStroke (dashed, ring, dashes, 2);
                g.setColour (Palette::blue.withAlpha (0.85f));
                g.fillPath (dashed);
            }

            Colour col = Palette::ink;
            if (known && ! inWedge) col = Palette::ink.withAlpha (0.40f + 0.60f * glow);
            if (glow > 0.35f)     col = (! known || inKey) ? Colour (0xff3d1f00) : Colour (0xff8a1206);
            if (tonic)            col = Colour (0xff08204a);

            g.setColour (col);
            g.setFont (font);
            g.drawText (toJuce (kc::pitchClassName (pc, state.flats)),
                        Rectangle<float> (pos.x - 50.0f, pos.y - 27.0f, 100.0f, 54.0f), Justification::centred, false);
        }
    }

    void DialLayer::paintNeedle (Graphics& g, Point<float> c)
    {
        const auto t = AffineTransform::rotation (state.needleAngle).translated (c.x, c.y);
        const float L = 166.0f, T = 60.0f, W = 11.5f;

        Path whole;
        whole.addQuadrilateral (0.0f, -L, W, 0.0f, 0.0f, T, -W, 0.0f);
        whole.applyTransform (t);
        DropShadow (Colours::black.withAlpha (0.45f), 6, { 4, 6 }).drawForPath (g, whole);

        auto tri = [&] (Point<float> a, Point<float> b, Point<float> d, Colour col)
        {
            Path p;
            p.addTriangle (a, b, d);
            p.applyTransform (t);
            g.setColour (col);
            g.fillPath (p);
        };

        const Colour tipLight = state.snap.hasKey ? Colour (0xffe8573f) : Colour (0xffc9876f);
        const Colour tipDark  = state.snap.hasKey ? Colour (0xff9e1d12) : Colour (0xff7d4a3a);
        tri ({ 0, -L }, { 0, 0 }, { -W, 0 }, tipLight);
        tri ({ 0, -L }, { W, 0 }, { 0, 0 }, tipDark);
        tri ({ -W, 0 }, { 0, 0 }, { 0, T }, Colour (0xff57534f));
        tri ({ 0, 0 }, { W, 0 }, { 0, T }, Colour (0xff1d1b1a));

        g.setColour (Colour (0xff1a0c06));
        g.strokePath (whole, PathStrokeType (1.0f, PathStrokeType::mitered));
    }

    void DialLayer::paintHub (Graphics& g, Point<float> c)
    {
        const float r = 17.0f;
        ColourGradient dome (Colour (0xfffff0b8), c.x - 5.0f, c.y - 6.0f, Colour (0xff5e3b10), c.x + 12.0f, c.y + 14.0f, true);
        dome.addColour (0.45, Colour (0xffc99434));
        g.setGradientFill (dome);
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (Colour (0xff2a1806));
        g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.3f);
        g.setColour (Colour (0xff7a5217));
        g.drawEllipse (c.x - 7.5f, c.y - 7.5f, 15.0f, 15.0f, 1.1f);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.fillEllipse (c.x - 8.0f, c.y - 9.0f, 6.0f, 4.0f);
    }

    void DialLayer::paintPlate (Graphics& g, Point<float> c, bool textPass)
    {
        const Rectangle<float> plate (c.x - 104.0f, c.y + 50.0f, 208.0f, 90.0f);
        const auto& s = state.snap;
        const bool mono = state.mode == kc::AnalysisMode::Mono;

        if (! textPass)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (plate.translated (2.0f, 4.0f).expanded (1.0f), 11.0f);
            ColourGradient body (Palette::plateTop, plate.getX(), plate.getY(), Palette::plateBottom, plate.getX(), plate.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (plate, 10.0f);
            ColourGradient border (Palette::goldLight, plate.getX(), plate.getY(), Palette::goldDark, plate.getX(), plate.getBottom(), false);
            g.setGradientFill (border);
            g.drawRoundedRectangle (plate, 10.0f, 2.0f);
            g.setColour (Palette::goldDark.withAlpha (0.6f));
            g.drawRoundedRectangle (plate.reduced (4.0f), 7.0f, 0.8f);
            return;
        }

        const float a = state.liveAlpha;
        auto inner = plate.reduced (12.0f, 7.0f);
        auto header = inner.removeFromTop (18.0f);
        auto footer = inner.removeFromBottom (20.0f);

        g.setFont (assets.cinzel (15.0f));
        g.setColour (Palette::goldDim.withMultipliedAlpha (a));
        g.drawText (mono ? "LIVE NOTE" : "LIVE CHORD", header, Justification::centredLeft, false);

        String right = s.sourceIsMidi ? "MIDI" : "AUDIO";
        if (mono && s.monoVoiced && ! s.sourceIsMidi)
            right = String (s.monoFrequency, 1) + " Hz";
        g.drawText (right, header, Justification::centredRight, false);

        // main reading
        String mainText = CharPointer_UTF8 ("\xe2\x80\x94");   // em dash
        String suffix;
        Colour mainColour = Palette::cream;

        if (mono)
        {
            if (s.monoNote >= 0)
                mainText = toJuce (kc::noteNameWithOctave (s.monoNote, state.flats));
        }
        else if (s.chord.isValid())
        {
            mainText = toJuce (kc::chordName (s.chord, state.flats));
            if (s.chord.isSingleNote) suffix = "note";

            if (s.hasKey && ! s.chord.isSingleNote)
            {
                bool diatonic = false;
                for (const auto& f : kc::keyFamily (s.key))
                    if (kc::triadClassIndex (f) == kc::triadClassIndex (s.chord)) diatonic = true;
                if (s.key.minor && s.chord.root == (s.key.tonic + 7) % 12 && kc::triadClassIndex (s.chord) == s.chord.root)
                    diatonic = true;   // major V in a minor key (harmonic minor)
                if (! diatonic) mainColour = Palette::amber;
            }
        }

        const bool haveReading = (mono ? s.monoNote >= 0 : s.chord.isValid());
        Font mainFont = assets.bask (40.0f);
        while (textWidth (mainFont, mainText) > inner.getWidth() - 50.0f && mainFont.getHeight() > 20.0f)
            mainFont = mainFont.withHeight (mainFont.getHeight() - 2.0f);

        if (suffix.isNotEmpty())
        {
            const float w = textWidth (mainFont, mainText);
            const Font sf = assets.cinzel (14.0f);
            const float sw = textWidth (sf, suffix) + 6.0f;
            const float x0 = inner.getCentreX() - (w + sw) * 0.5f;
            drawGlowingText (g, mainText, mainFont, { x0, inner.getY(), w + 2.0f, inner.getHeight() }, Justification::centredLeft,
                             mainColour.withMultipliedAlpha (a), Palette::gold, 8, 0.35f * a);
            g.setFont (sf);
            g.setColour (Palette::goldDim.withMultipliedAlpha (a));
            g.drawText (suffix, Rectangle<float> (x0 + w + 6.0f, inner.getY() + 8.0f, sw, inner.getHeight()), Justification::centredLeft, false);
        }
        else
        {
            drawGlowingText (g, mainText, mainFont, inner, Justification::centred,
                             mainColour.withMultipliedAlpha (haveReading ? a : 0.45f), Palette::gold, 8, haveReading ? 0.35f * a : 0.0f);
        }

        // footer: live notes, or the tuning meter in Mono mode
        if (mono)
        {
            if (s.monoNote < 0) return;
            const float cents = juce::jlimit (-50.0f, 50.0f, state.shownCents);
            auto meter = footer.withTrimmedRight (58.0f);
            const float y = meter.getCentreY() + 2.0f;
            const float x0 = meter.getX() + 4.0f, x1 = meter.getRight() - 4.0f, xm = (x0 + x1) * 0.5f;

            g.setColour (Palette::goldDim.withMultipliedAlpha (0.8f * a));
            g.drawLine (x0, y, x1, y, 1.2f);
            for (int i = -2; i <= 2; ++i)
            {
                const float x = xm + (float) i * (x1 - xm) * 0.5f;
                const float h = i == 0 ? 7.0f : 4.0f;
                g.drawLine (x, y - h, x, y + h * 0.4f, i == 0 ? 1.6f : 1.0f);
            }

            const float ac = std::abs (cents);
            const Colour pc = ac < 10.0f ? Colour (0xff7ee07a) : (ac < 25.0f ? Palette::amber : Colour (0xffff6a50));
            const float px = xm + cents / 50.0f * (x1 - xm);
            Path pointer;
            pointer.addTriangle (px, y + 1.0f, px - 5.0f, y - 8.0f, px + 5.0f, y - 8.0f);
            g.setColour (pc.withMultipliedAlpha (s.monoVoiced ? a : 0.4f));
            g.fillPath (pointer);

            g.setFont (assets.bask (16.0f));
            const int ci = (int) std::lround (cents);
            g.drawText ((ci > 0 ? "+" : "") + String (ci) + String (CharPointer_UTF8 ("\xc2\xa2")),
                        footer.removeFromRight (56.0f), Justification::centredRight, false);
        }
        else
        {
            StringArray live;
            for (int p = 0; p < 12; ++p)
            {
                const int pc = kc::pitchClassAtCircle (p);
                if (s.noteLevels[(size_t) pc] > 0.5f)
                    live.add (toJuce (kc::pitchClassName (pc, state.flats)));
            }
            g.setFont (assets.baskPlain (16.0f));
            g.setColour (Palette::cream.withMultipliedAlpha (0.75f * a));
            g.drawText (live.isEmpty() ? String() : live.joinIntoString (String (CharPointer_UTF8 ("  \xc2\xb7  "))),
                        footer, Justification::centred, false);
        }
    }

    //==============================================================================
    // LidLayer
    //==============================================================================
    LidLayer::LidLayer (Assets& a, const UiState& s) : assets (a), state (s)
    {
        setInterceptsMouseClicks (false, false);
        setBounds (Layout::lidArea);
    }

    void LidLayer::paint (Graphics& g)
    {
        g.addTransform (AffineTransform::translation ((float) -getX(), (float) -getY()));
        paintKey (g);
        paintRunnerUp (g);
        paintConfidence (g);
        paintFamily (g);
        paintHistory (g);
    }

    void LidLayer::paintCreamBox (Graphics& g, Rectangle<float> r)
    {
        ColourGradient fill (Colour (0xfffbf1db), r.getX(), r.getY(), Colour (0xffeedab2), r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (Colour (0xff9c7a45).withAlpha (0.75f));
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.3f);
        g.setColour (Colours::black.withAlpha (0.06f));
        g.drawRoundedRectangle (r.reduced (2.5f), 7.0f, 1.0f);
    }

    void LidLayer::paintKey (Graphics& g)
    {
        const auto& s = state.snap;
        const auto area = Layout::keyText;

        if (s.hasKey)
        {
            const String text = toJuce (kc::keyName (s.key, state.flats, true));
            Font f = assets.baskItalic (64.0f);
            while (textWidth (f, text) > area.getWidth() - 12.0f && f.getHeight() > 30.0f)
                f = f.withHeight (f.getHeight() - 2.0f);

            const float pulse = 1.0f + 0.12f * std::sin (state.time * 2.4f) * (s.signalPresent ? 1.0f : 0.0f);
            drawGlowingText (g, text, f, area, Justification::centred, Colour (0xff0b0e16),
                             Palette::blue, 22, 1.45f * state.keyGlow * pulse);
        }
        else
        {
            String text = "PLAY SOME MUSIC";
            if (s.signalPresent)
            {
                const int dots = (int) std::fmod (state.time * 2.5f, 4.0f);
                text = "LISTENING" + String::repeatedString (".", dots);
            }
            g.setColour (Palette::ink.withAlpha (0.72f));
            g.setFont (assets.cinzel (30.0f));
            g.drawText (text, area.withTrimmedLeft (s.signalPresent ? 12.0f : 0.0f), Justification::centred, false);
        }
    }

    void LidLayer::paintRunnerUp (Graphics& g)
    {
        const auto& s = state.snap;
        const auto area = Layout::runnerUpArea;

        if (state.locked && s.hasKey)
        {
            const float x = area.getRight() - 108.0f, y = area.getCentreY();
            g.setColour (Palette::goldDark);
            g.fillRoundedRectangle (x, y - 3.0f, 14.0f, 11.0f, 2.0f);
            Path shackle;
            shackle.addCentredArc (x + 7.0f, y - 3.0f, 4.5f, 5.5f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
            g.strokePath (shackle, PathStrokeType (2.2f));
            g.setFont (assets.cinzel (18.0f));
            g.drawText ("LOCKED", area.withTrimmedLeft (area.getWidth() - 88.0f), Justification::centredRight, false);
            return;
        }

        if (s.showRunnerUp && s.runnerUp.isValid() && ! state.locked)
        {
            g.setColour (Palette::inkSoft);
            g.setFont (assets.baskItalic (21.0f));
            g.drawText ("or " + toJuce (kc::keyName (s.runnerUp, state.flats, false)), area, Justification::centredRight, false);
            return;
        }

        // plain divider in place of the painted one
        const float y = area.getCentreY() + 2.0f, x0 = area.getX() + 24.0f, x1 = area.getRight() - 12.0f, xm = (x0 + x1) * 0.5f;
        g.setColour (Palette::ink.withAlpha (0.55f));
        g.drawLine (x0, y, xm - 12.0f, y, 1.2f);
        g.drawLine (xm + 12.0f, y, x1, y, 1.2f);
        Path d;
        d.addQuadrilateral (xm, y - 5.0f, xm + 7.0f, y, xm, y + 5.0f, xm - 7.0f, y);
        g.fillPath (d);
    }

    void LidLayer::paintConfidence (Graphics& g)
    {
        const auto& s = state.snap;
        const auto bar = Layout::confidenceBar;
        const float conf = juce::jlimit (0.0f, 1.0f, state.shownConfidence);

        // frame
        auto outer = bar.reduced (1.0f);
        g.setColour (Colour (0xff0e0b09));
        g.fillRoundedRectangle (outer, 12.0f);
        ColourGradient border (Palette::goldLight, outer.getX(), outer.getY(), Palette::goldDark, outer.getX(), outer.getBottom(), false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (outer, 12.0f, 2.2f);
        g.setColour (Palette::goldDark.withAlpha (0.7f));
        g.drawRoundedRectangle (outer.reduced (4.0f), 9.0f, 0.9f);

        auto track = bar.reduced (13.0f, 12.0f);
        g.setColour (Colour (0xff040303));
        g.fillRoundedRectangle (track, track.getHeight() * 0.5f);

        if (s.hasKey && conf > 0.005f)
        {
            const float w = juce::jmax (track.getHeight(), track.getWidth() * conf);
            const auto fillR = track.withWidth (w);
            Colour top, mid, bottom;
            if (conf >= 0.5f)       { top = Colour (0xff9adbff); mid = Colour (0xff2f8fff); bottom = Colour (0xff1450b8); }
            else if (conf >= 0.3f)  { top = Colour (0xffffe0a0); mid = Colour (0xffe8a23a); bottom = Colour (0xff9a5a12); }
            else                    { top = Colour (0xffffb09a); mid = Colour (0xffd8553a); bottom = Colour (0xff8a2412); }

            Path p;
            p.addRoundedRectangle (fillR, track.getHeight() * 0.5f);
            DropShadow (mid.withAlpha (0.55f), 10, {}).drawForPath (g, p);

            ColourGradient fg (top, fillR.getX(), fillR.getY(), bottom, fillR.getX(), fillR.getBottom(), false);
            fg.addColour (0.45, mid);
            g.setGradientFill (fg);
            g.fillPath (p);

            g.setColour (Colours::white.withAlpha (0.38f));
            g.fillRoundedRectangle (fillR.reduced (6.0f, 0.0f).withHeight (fillR.getHeight() * 0.28f).translated (0, 3.0f), 3.0f);
        }

        // percentage box
        const auto pb = Layout::percentBox;
        g.setColour (Colour (0xff0e0b09));
        g.fillRoundedRectangle (pb, 9.0f);
        ColourGradient pbBorder (Palette::goldLight, pb.getX(), pb.getY(), Palette::goldDark, pb.getX(), pb.getBottom(), false);
        g.setGradientFill (pbBorder);
        g.drawRoundedRectangle (pb, 9.0f, 2.0f);

        g.setColour (Palette::cream);
        g.setFont (assets.bask (27.0f));
        g.drawText (s.hasKey ? String ((int) std::lround (conf * 100.0f)) + "%" : String ("--"), pb, Justification::centred, false);
    }

    void LidLayer::paintFamily (Graphics& g)
    {
        const auto& s = state.snap;
        const auto box = Layout::familyBox;
        paintCreamBox (g, box);

        if (! s.hasKey)
        {
            g.setColour (Palette::ink.withAlpha (0.35f));
            g.setFont (assets.bask (22.0f));
            g.drawText (CharPointer_UTF8 ("\xe2\x80\x94"), box, Justification::centred, false);
            return;
        }

        const auto fam = kc::keyFamily (s.key);
        const int current = s.chord.isValid() ? kc::triadClassIndex (s.chord) : -1;

        StringArray names;
        for (const auto& c : fam) names.add (toJuce (kc::chordName (c, state.flats)));

        Font f = assets.bask (23.0f);
        const float gap = 26.0f;
        auto totalWidth = [&]
        {
            float t = gap * 5.0f;
            for (auto& n : names) t += textWidth (f, n);
            return t;
        };
        while (totalWidth() > box.getWidth() - 22.0f && f.getHeight() > 14.0f)
            f = f.withHeight (f.getHeight() - 1.0f);

        float x = box.getCentreX() - totalWidth() * 0.5f;
        for (int i = 0; i < 6; ++i)
        {
            const float w = textWidth (f, names[i]);
            const Rectangle<float> r (x, box.getY(), w + 2.0f, box.getHeight());
            const bool lit = current >= 0 && kc::triadClassIndex (fam[(size_t) i]) == current;

            if (lit)
                drawGlowingText (g, names[i], f, r, Justification::centredLeft, Colour (0xff0a3480),
                                 Palette::blue, 9, 1.3f * state.liveAlpha);
            else
                drawGlowingText (g, names[i], f, r, Justification::centredLeft, Palette::ink, {}, 0, 0.0f);

            x += w;
            if (i < 5)
            {
                g.setColour (Palette::ink.withAlpha (0.55f));
                g.fillRect (x + gap * 0.5f - 1.0f, box.getCentreY() - 11.0f, 2.0f, 22.0f);
                x += gap;
            }
        }
    }

    void LidLayer::paintHistory (Graphics& g)
    {
        const auto& s = state.snap;
        const bool mono = state.mode == kc::AnalysisMode::Mono;

        g.setColour (Palette::ink.withAlpha (0.92f));
        g.setFont (assets.bask (19.5f).withExtraKerningFactor (0.04f));
        g.drawText (mono ? "NOTES" : "CHORDS", Layout::historyLabel, Justification::centred, false);

        const auto box = Layout::historyBox;
        paintCreamBox (g, box);

        struct Item { String text; bool keyChange; bool italic; };
        std::vector<Item> items;
        for (int i = 0; i < s.historySize; ++i)
        {
            const auto& e = s.history[(size_t) i];
            switch (e.kind)
            {
                case kc::HistoryEntry::Kind::Chord:
                    if (mono) continue;
                    items.push_back ({ toJuce (kc::chordName (e.chord, state.flats)), false, e.chord.isSingleNote });
                    break;
                case kc::HistoryEntry::Kind::Note:
                    if (! mono) continue;
                    items.push_back ({ toJuce (kc::noteNameWithOctave (e.midiNote, state.flats)), false, false });
                    break;
                case kc::HistoryEntry::Kind::KeyChange:
                    items.push_back ({ toJuce (kc::keyShortName (e.key, state.flats)), true, false });
                    break;
            }
        }

        if (items.empty())
        {
            g.setColour (Palette::ink.withAlpha (0.35f));
            g.setFont (assets.bask (20.0f));
            g.drawText (CharPointer_UTF8 ("\xe2\x80\x94"), box, Justification::centred, false);
            return;
        }

        const Font chordFont = assets.bask (21.0f), italicFont = assets.baskItalic (21.0f), keyFont = assets.baskItalic (19.0f);
        const float arrowW = 24.0f, diamondW = 13.0f;
        auto widthOf = [&] (const Item& it)
        {
            if (it.keyChange) return textWidth (keyFont, it.text) + diamondW;
            return textWidth (it.italic ? italicFont : chordFont, it.text);
        };

        // take as many of the latest items as fit
        const float maxW = box.getWidth() - 22.0f;
        float used = 0.0f;
        int first = (int) items.size();
        while (first > 0)
        {
            const float w = widthOf (items[(size_t) first - 1]) + (first < (int) items.size() ? arrowW : 0.0f);
            if (used + w > maxW) break;
            used += w;
            --first;
        }

        float x = box.getCentreX() - used * 0.5f;
        const float cy = box.getCentreY();
        for (int i = first; i < (int) items.size(); ++i)
        {
            const auto& it = items[(size_t) i];
            const bool latest = (i == (int) items.size() - 1);
            const float w = widthOf (it);

            if (it.keyChange)
            {
                Path d;
                d.addQuadrilateral (x + 5.0f, cy - 6.0f, x + 10.0f, cy, x + 5.0f, cy + 6.0f, x, cy);
                g.setColour (Palette::blue);
                g.fillPath (d);
                g.setColour (Colour (0xff0a3480));
                g.setFont (keyFont);
                g.drawText (it.text, Rectangle<float> (x + diamondW, box.getY(), w, box.getHeight()), Justification::centredLeft, false);
            }
            else
            {
                const Colour col = latest && s.signalPresent ? Colour (0xff0a3480) : Palette::ink;
                g.setColour (col);
                g.setFont (it.italic ? italicFont : chordFont);
                g.drawText (it.text, Rectangle<float> (x, box.getY(), w + 4.0f, box.getHeight()), Justification::centredLeft, false);
            }
            x += w;

            if (i < (int) items.size() - 1)
            {
                const float ax0 = x + 6.0f, ax1 = x + arrowW - 6.0f;
                g.setColour (Palette::ink.withAlpha (0.8f));
                g.drawLine (ax0, cy, ax1 - 1.0f, cy, 1.6f);
                Path head;
                head.addTriangle (ax1 + 1.0f, cy, ax1 - 5.0f, cy - 4.0f, ax1 - 5.0f, cy + 4.0f);
                g.fillPath (head);
                x += arrowW;
            }
        }
    }
}
