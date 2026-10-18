// Key Compass - custom controls and look-and-feel
#include "Controls.h"

namespace kcui
{
    using namespace juce;

    //==============================================================================
    static void paintEnamelBox (Graphics& g, Rectangle<float> r, bool hover)
    {
        ColourGradient body (Colour (0xff2e2621), r.getX(), r.getY(), Colour (0xff0b0806), r.getX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, 9.0f);

        g.setColour (Colours::white.withAlpha (0.055f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.46f).reduced (4.0f, 3.0f), 6.0f);

        ColourGradient border (Palette::goldLight, r.getX(), r.getY(), Palette::goldDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (r, 9.0f, hover ? 2.8f : 2.1f);

        g.setColour (Palette::goldDark.withAlpha (0.65f));
        g.drawRoundedRectangle (r.reduced (4.5f), 6.0f, 0.9f);
    }

    //==============================================================================
    CompassLookAndFeel::CompassLookAndFeel (Assets& a) : assets (a)
    {
        setColour (PopupMenu::backgroundColourId, Colour (0xff17110c));
        setColour (PopupMenu::textColourId, Palette::cream);
        setColour (PopupMenu::highlightedBackgroundColourId, Palette::gold);
        setColour (PopupMenu::highlightedTextColourId, Palette::ink);
        setColour (TooltipWindow::backgroundColourId, Colour (0xf01a130d));
        setColour (TooltipWindow::textColourId, Palette::cream);
        setColour (TooltipWindow::outlineColourId, Palette::goldDark);
    }

    void CompassLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
    {
        g.fillAll (Colour (0xff17110c));
        g.setColour (Palette::goldDark);
        g.drawRect (0, 0, width, height, 2);
    }

    void CompassLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive,
                                                bool isHighlighted, bool isTicked, bool, const String& text,
                                                const String& shortcutKeyText, const Drawable*, const Colour*)
    {
        if (isSeparator)
        {
            g.setColour (Palette::goldDark.withAlpha (0.6f));
            g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1));
            return;
        }

        auto r = area.reduced (4, 1).toFloat();
        Colour textColour = Palette::cream.withAlpha (isActive ? 1.0f : 0.4f);

        if (isHighlighted && isActive)
        {
            g.setColour (Palette::gold.withAlpha (0.92f));
            g.fillRoundedRectangle (r, 4.0f);
            textColour = Palette::ink;
        }

        auto tickArea = r.removeFromLeft (26.0f);
        if (isTicked)
        {
            const auto c = tickArea.getCentre();
            Path d;
            d.addQuadrilateral (c.x, c.y - 5.0f, c.x + 5.0f, c.y, c.x, c.y + 5.0f, c.x - 5.0f, c.y);
            g.setColour (isHighlighted ? Palette::ink : Palette::gold);
            g.fillPath (d);
        }

        g.setColour (textColour);
        g.setFont (assets.cinzel (18.5f));
        g.drawText (text, (shortcutKeyText.isNotEmpty() ? r.removeFromLeft (r.getWidth() * 0.6f) : r).toNearestInt(), Justification::centredLeft, true);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (textColour.withMultipliedAlpha (0.75f));
            g.setFont (assets.baskItalic (16.0f));
            g.drawText (shortcutKeyText, r.withTrimmedRight (10.0f).toNearestInt(), Justification::centredRight, true);
        }
    }

    Font CompassLookAndFeel::getPopupMenuFont()
    {
        return assets.cinzel (18.5f);
    }

    void CompassLookAndFeel::getIdealPopupMenuItemSize (const String& text, bool isSeparator, int standardMenuItemHeight,
                                                        int& idealWidth, int& idealHeight)
    {
        if (isSeparator)
        {
            idealWidth = 50;
            idealHeight = 10;
            return;
        }
        idealHeight = jmax (30, standardMenuItemHeight);
        idealWidth = (int) textWidth (assets.cinzel (18.5f), text) + 70;
    }

    static TextLayout makeTooltipLayout (const Font& font, const String& text, float maxWidth)
    {
        AttributedString s;
        s.setJustification (Justification::centredLeft);
        s.append (text, font, Palette::cream);
        TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, maxWidth);
        return tl;
    }

    Rectangle<int> CompassLookAndFeel::placeTooltip (Point<int> mouse, Rectangle<int> avoid, Rectangle<int> area, const TipMeasure& measure)
    {
        constexpr int gap = 8, minSide = 170, maxW = 360;
        area = area.reduced (4);
        if (avoid.isEmpty()) avoid = Rectangle<int> (mouse.x - 14, mouse.y - 14, 28, 28);
        avoid = avoid.expanded (2);

        auto right = [&]() -> Rectangle<int>
        {
            const int space = area.getRight() - (avoid.getRight() + gap);
            if (space < minSide) return {};
            const auto s = measure (jmin (maxW, space));
            if (s.y > area.getHeight() || s.x > space) return {};
            return { avoid.getRight() + gap, jlimit (area.getY(), area.getBottom() - s.y, mouse.y - s.y / 2), s.x, s.y };
        };
        auto left = [&]() -> Rectangle<int>
        {
            const int space = (avoid.getX() - gap) - area.getX();
            if (space < minSide) return {};
            const auto s = measure (jmin (maxW, space));
            if (s.y > area.getHeight() || s.x > space) return {};
            return { avoid.getX() - gap - s.x, jlimit (area.getY(), area.getBottom() - s.y, mouse.y - s.y / 2), s.x, s.y };
        };
        auto below = [&]() -> Rectangle<int>
        {
            const auto s = measure (jmin (maxW, area.getWidth()));
            if (avoid.getBottom() + gap + s.y > area.getBottom()) return {};
            return { jlimit (area.getX(), area.getRight() - s.x, mouse.x - s.x / 2), avoid.getBottom() + gap, s.x, s.y };
        };
        auto above = [&]() -> Rectangle<int>
        {
            const auto s = measure (jmin (maxW, area.getWidth()));
            if (avoid.getY() - gap - s.y < area.getY()) return {};
            return { jlimit (area.getX(), area.getRight() - s.x, mouse.x - s.x / 2), avoid.getY() - gap - s.y, s.x, s.y };
        };

        // big areas (a piano roll, a panel): beside them, on the roomier side first; small controls: under or over them
        const bool big = avoid.getWidth() > 160 || avoid.getHeight() > 100;
        const bool rightFirst = area.getRight() - avoid.getRight() >= avoid.getX() - area.getX();
        std::vector<std::function<Rectangle<int>()>> order;
        if (big)  order = { rightFirst ? std::function<Rectangle<int>()> (right) : left, rightFirst ? std::function<Rectangle<int>()> (left) : right, below, above };
        else      order = { below, above, right, left };
        for (auto& f : order)
        {
            const auto r = f();
            if (! r.isEmpty()) return r;
        }

        // nowhere beside it: the corner of the window farthest from the mouse
        const auto s = measure (jmin (maxW, area.getWidth()));
        const int x = mouse.x > area.getCentreX() ? area.getX() : area.getRight() - s.x;
        const int y = mouse.y > area.getCentreY() ? area.getY() : area.getBottom() - s.y;
        return { x, y, s.x, s.y };
    }

    Rectangle<int> CompassLookAndFeel::getTooltipBounds (const String& tipText, Point<int> screenPos, Rectangle<int> parentArea)
    {
        const auto font = assets.baskPlain (17.0f);
        const auto r = placeTooltip (screenPos, tooltipAvoid, parentArea, [&] (int maxWidth)
        {
            const auto tl = makeTooltipLayout (font, tipText, (float) maxWidth - 22.0f);
            return Point<int> ((int) std::ceil (tl.getWidth()) + 22, (int) std::ceil (tl.getHeight()) + 14);
        });
        tooltipAvoid = {};
        return r;
    }

    void CompassLookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
    {
        const auto b = Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (Colour (0xf61a130d));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (Palette::goldDark);
        g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

        const auto tl = makeTooltipLayout (assets.baskPlain (17.0f), text, (float) width - 20.0f);
        tl.draw (g, b.reduced (10.0f, 7.0f));
    }

    //==============================================================================
    TipWindow::TipWindow (Component* parent, CompassLookAndFeel& lf, int delayMs)
        : TooltipWindow (parent, delayMs), laf (lf)
    {
        setLookAndFeel (&laf);
        Desktop::getInstance().addGlobalMouseListener (&watcher);
    }

    TipWindow::~TipWindow()
    {
        Desktop::getInstance().removeGlobalMouseListener (&watcher);
        setLookAndFeel (nullptr);
    }

    String TipWindow::getTipFor (Component& c)
    {
        // something you just clicked keeps quiet until the mouse moves on
        if (watcher.pressed != nullptr && watcher.pressed.getComponent() != &c)
            watcher.pressed = nullptr;
        if (watcher.pressed.getComponent() == &c)
            return {};

        const auto tip = TooltipWindow::getTipFor (c);
        if (tip.isNotEmpty())
            laf.tooltipAvoid = areaToAvoid (c);
        return tip;
    }

    Rectangle<int> TipWindow::areaToAvoid (Component& c) const
    {
        // keep clear of the control and of the panel (work area) it belongs to
        Component* zone = &c;
        for (auto* a = &c; a != nullptr; a = a->getParentComponent())
            if (a->getProperties().contains ("kcTipArea")) { zone = a; break; }

        if (auto* parent = getParentComponent())
        {
            auto visible = [parent] (Component& comp)
            {
                auto r = parent->getLocalArea (&comp, comp.getLocalBounds());
                for (auto* a = comp.getParentComponent(); a != nullptr && a != parent; a = a->getParentComponent())
                    r = r.getIntersection (parent->getLocalArea (a, a->getLocalBounds()));
                return r;
            };
            return visible (c).getUnion (visible (*zone));
        }
        return c.getScreenBounds().getUnion (zone->getScreenBounds());
    }

    void CompassLookAndFeel::drawCornerResizer (Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
    {
        const float lineThickness = jmin ((float) w, (float) h) * 0.08f;
        g.setColour (Palette::gold.withAlpha (isMouseOver || isMouseDragging ? 0.95f : 0.55f));
        for (float i = 0.0f; i < 1.0f; i += 0.3f)
            g.drawLine ((float) w * i, (float) h + 1.0f, (float) w + 1.0f, (float) h * i, lineThickness);
    }

    //==============================================================================
    ChoiceBox::ChoiceBox (Assets& a, RangedAudioParameter& param, StringArray displayNames, StringArray menuDescriptions)
        : assets (a),
          names (std::move (displayNames)),
          descriptions (std::move (menuDescriptions)),
          attachment (param, [this] (float v)
                      {
                          index = jlimit (0, names.size() - 1, (int) std::lround (v));
                          repaint();
                      }, nullptr)
    {
        attachment.sendInitialUpdate();
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    static void paintChoiceFace (Graphics& g, Assets& assets, Rectangle<float> r, const String& text, bool hover)
    {
        paintEnamelBox (g, r, hover);

        auto textArea = r.reduced (12.0f, 4.0f).withTrimmedRight (24.0f);
        g.setColour (Palette::cream);
        g.setFont (text.containsAnyOf ("0123456789") ? assets.bask (r.getHeight() * 0.50f) : assets.cinzel (r.getHeight() * 0.50f));
        g.drawFittedText (text.toUpperCase(), textArea.toNearestInt(), Justification::centred, 1, 0.75f);

        const float cx = r.getRight() - 24.0f, cy = r.getCentreY();
        Path chevron;
        chevron.startNewSubPath (cx - 7.5f, cy - 3.5f);
        chevron.lineTo (cx, cy + 4.5f);
        chevron.lineTo (cx + 7.5f, cy - 3.5f);
        g.setColour (Palette::gold);
        g.strokePath (chevron, PathStrokeType (3.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    static void showChoiceMenu (Component& owner, const StringArray& names, const StringArray& descriptions, int current,
                                std::function<void (int)> chosen)
    {
        PopupMenu menu;
        menu.setLookAndFeel (&owner.getLookAndFeel());
        for (int i = 0; i < names.size(); ++i)
        {
            PopupMenu::Item item (names[i]);
            item.itemID = i + 1;
            item.isTicked = (i == current);
            item.shortcutKeyDescription = descriptions[i];
            menu.addItem (item);
        }
        const auto screen = owner.getScreenBounds();
        menu.showMenuAsync (PopupMenu::Options().withTargetScreenArea (screen)
                                                .withMinimumWidth (jmax (screen.getWidth(), 300))
                                                .withStandardItemHeight (30),
                            [safe = Component::SafePointer<Component> (&owner), chosen] (int result)
                            {
                                if (safe != nullptr && result > 0)
                                    chosen (result - 1);
                            });
    }

    void ChoiceBox::paint (Graphics& g)
    {
        paintChoiceFace (g, assets, getLocalBounds().toFloat().reduced (1.5f), names[index], isMouseOver (true));
    }

    void ChoiceBox::mouseDown (const MouseEvent& e)
    {
        if (! e.mods.isLeftButtonDown() && ! e.mods.isPopupMenu())
            return;

        showChoiceMenu (*this, names, descriptions, index, [this] (int i)
        {
            attachment.setValueAsCompleteGesture ((float) i);
        });
    }

    //==============================================================================
    TuningBox::TuningBox (Assets& a, RangedAudioParameter& param)
        : assets (a), parameter (param),
          attachment (param, [this] (float v) { value = v; repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        setMouseCursor (MouseCursor::UpDownResizeCursor);
    }

    int TuningBox::zoneAt (Point<float> p) const
    {
        if (p.x < 42.0f) return -1;
        if (p.x > (float) getWidth() - 42.0f) return 1;
        return 0;
    }

    void TuningBox::paint (Graphics& g)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.5f);
        paintEnamelBox (g, r, isMouseOver (true));

        auto arrow = [&] (float cx, bool left, bool pressed)
        {
            const float cy = r.getCentreY(), s = 8.5f;
            Path p;
            if (left) p.addTriangle (cx - s * 0.6f, cy, cx + s * 0.6f, cy - s, cx + s * 0.6f, cy + s);
            else      p.addTriangle (cx + s * 0.6f, cy, cx - s * 0.6f, cy - s, cx - s * 0.6f, cy + s);
            g.setColour (pressed ? Palette::goldLight : Palette::gold);
            g.fillPath (p);
        };
        arrow (r.getX() + 20.0f, true, pressedZone == -1);
        arrow (r.getRight() - 20.0f, false, pressedZone == 1);

        g.setColour (std::abs (value - 440.0f) < 0.05f ? Palette::cream : Palette::goldLight);
        g.setFont (assets.bask (r.getHeight() * 0.50f));
        g.drawFittedText (String (value, 1) + " Hz", r.reduced (34.0f, 2.0f).toNearestInt(), Justification::centred, 1, 0.8f);
    }

    void TuningBox::step (float amount)
    {
        const float v = jlimit (415.0f, 466.0f, std::round ((value + amount) * 10.0f) / 10.0f);
        attachment.setValueAsCompleteGesture (v);
    }

    void TuningBox::mouseDown (const MouseEvent& e)
    {
        pressedZone = zoneAt (e.position);
        if (pressedZone != 0)
        {
            repeatStep = (e.mods.isShiftDown() ? 0.1f : 1.0f) * (float) pressedZone;
            step (repeatStep);
            startTimer (420);
        }
        else
        {
            dragging = true;
            dragStartValue = value;
            attachment.beginGesture();
        }
        repaint();
    }

    void TuningBox::mouseDrag (const MouseEvent& e)
    {
        if (! dragging) return;
        const float perPixel = e.mods.isShiftDown() ? 0.01f : 0.1f;
        const float v = jlimit (415.0f, 466.0f, std::round ((dragStartValue - (float) e.getDistanceFromDragStartY() * perPixel) * 10.0f) / 10.0f);
        attachment.setValueAsPartOfGesture (v);
    }

    void TuningBox::mouseUp (const MouseEvent&)
    {
        stopTimer();
        if (dragging)
        {
            attachment.endGesture();
            dragging = false;
        }
        pressedZone = 0;
        repaint();
    }

    void TuningBox::mouseDoubleClick (const MouseEvent& e)
    {
        if (zoneAt (e.position) == 0)
            attachment.setValueAsCompleteGesture (440.0f);
    }

    void TuningBox::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
    {
        const float d = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (std::abs (d) < 1.0e-6f) return;
        const float dir = d > 0.0f ? 1.0f : -1.0f;
        step (dir * (e.mods.isShiftDown() ? 0.1f : 1.0f));
    }

    void TuningBox::timerCallback()
    {
        step (repeatStep);
        startTimer (70);
    }

    //==============================================================================
    RoundToggle::RoundToggle (Assets& a, Icon i) : Button (i == Icon::Lock ? "Lock Key" : "Flats"), assets (a), icon (i)
    {
        setClickingTogglesState (true);
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    bool RoundToggle::hitTest (int x, int y)
    {
        return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) < (float) getWidth() * 0.5f;
    }

    void RoundToggle::paintButton (Graphics& g, bool highlighted, bool down)
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f - 3.0f;
        const bool on = getToggleState();

        Path disc;
        disc.addEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
        if (on)
            DropShadow (Palette::blue.withAlpha (0.85f), 12, {}).drawForPath (g, disc);

        ColourGradient ring (Palette::goldLight, c.x, c.y - R, Palette::goldDark, c.x, c.y + R, false);
        g.setGradientFill (ring);
        g.fillPath (disc);
        g.setColour (Colour (0xff2a1806));
        g.drawEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R, 1.0f);

        const float r2 = R - 5.0f;
        ColourGradient lens = on ? ColourGradient (Colour (0xffd8f3ff), c.x - r2 * 0.3f, c.y - r2 * 0.35f,
                                                   Colour (0xff0a2a5e), c.x + r2, c.y + r2, true)
                                 : ColourGradient (Colour (0xff4b3e32), c.x - r2 * 0.3f, c.y - r2 * 0.35f,
                                                   Colour (0xff070504), c.x + r2, c.y + r2, true);
        if (on) lens.addColour (0.45, Colour (0xff2b89ec));
        g.setGradientFill (lens);
        g.fillEllipse (c.x - r2, c.y - r2, 2.0f * r2, 2.0f * r2);

        g.setColour (Colours::white.withAlpha (on ? 0.32f : 0.10f));
        g.fillEllipse (c.x - r2 * 0.55f, c.y - r2 * 0.82f, r2 * 1.1f, r2 * 0.52f);

        if (highlighted || down)
        {
            g.setColour (down ? Colours::black.withAlpha (0.22f) : Colours::white.withAlpha (0.08f));
            g.fillEllipse (c.x - r2, c.y - r2, 2.0f * r2, 2.0f * r2);
        }

        const Colour iconColour = on ? Colours::white : Palette::gold;
        g.setColour (iconColour);

        if (icon == Icon::Lock)
        {
            const float bw = 20.0f, bh = 15.0f, top = c.y - 1.5f;
            g.fillRoundedRectangle (c.x - bw * 0.5f, top, bw, bh, 2.5f);
            g.setColour (on ? Colour (0xff1c5fb5) : Colour (0xff1a120b));
            g.fillEllipse (c.x - 2.2f, top + 4.0f, 4.4f, 4.4f);
            g.fillRect (c.x - 1.0f, top + 7.0f, 2.0f, 4.5f);

            const float lift = on ? 0.0f : 5.0f, sr = 6.5f, legH = 5.0f;
            Path shackle;
            shackle.startNewSubPath (c.x - sr, top);
            shackle.lineTo (c.x - sr, top - legH - lift);
            shackle.addCentredArc (c.x, top - legH - lift, sr, sr, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi);
            shackle.lineTo (c.x + sr, on ? top : top - legH - lift + 2.0f);
            g.setColour (iconColour);
            g.strokePath (shackle, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        else
        {
            g.setFont (assets.bask (36.0f));
            g.drawText (String (CharPointer_UTF8 ("\xe2\x99\xad")), Rectangle<float> (c.x - 20.0f, c.y - 22.0f, 40.0f, 40.0f).toNearestInt(),
                        Justification::centred, false);
        }
    }

    //==============================================================================
    void PlateButton::paintButton (Graphics& g, bool highlighted, bool down)
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        if (down)
        {
            g.setColour (Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (r, 10.0f);
        }
        else if (highlighted)
        {
            g.setColour (Colours::white.withAlpha (0.09f));
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (Palette::goldLight.withAlpha (0.5f));
            g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 1.5f);
        }
    }
}

//==============================================================================
namespace kcui
{
    void CompassLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                            int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown)
    {
        using namespace juce;
        auto track = Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f, 2.0f);
        g.setColour (Colour (0xff3a2a1a).withAlpha (0.25f));
        g.fillRoundedRectangle (track, jmin (track.getWidth(), track.getHeight()) * 0.5f);

        Rectangle<float> thumb = vertical ? Rectangle<float> ((float) x, (float) thumbStart, (float) width, (float) thumbSize)
                                          : Rectangle<float> ((float) thumbStart, (float) y, (float) thumbSize, (float) height);
        thumb = thumb.reduced (2.0f, 2.0f);
        if (thumb.isEmpty()) return;
        ColourGradient grad (Palette::goldLight, thumb.getX(), thumb.getY(), Palette::goldDark, thumb.getRight(), thumb.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (thumb, jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
        g.setColour (Colour (0xff3a2208).withAlpha (isMouseDown ? 0.9f : (isMouseOver ? 0.7f : 0.5f)));
        g.drawRoundedRectangle (thumb, jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f, 1.0f);
    }

    void CompassLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                               float, juce::Slider::SliderStyle, juce::Slider&)
    {
        using namespace juce;
        const float cy = (float) y + (float) height * 0.5f;
        Rectangle<float> track ((float) x + 4.0f, cy - 4.0f, (float) width - 8.0f, 8.0f);
        g.setColour (Colour (0xff120c07));
        g.fillRoundedRectangle (track, 4.0f);
        ColourGradient fill (Palette::blueDeep, track.getX(), cy, Palette::blue, sliderPos, cy, false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (track.withRight (jmax (track.getX() + 8.0f, sliderPos)), 4.0f);
        g.setColour (Palette::goldDark);
        g.drawRoundedRectangle (track, 4.0f, 1.2f);

        const float r = jmin ((float) height * 0.42f, 12.0f);
        ColourGradient knob (Palette::goldLight, sliderPos - r * 0.4f, cy - r * 0.6f, Palette::goldDark, sliderPos + r, cy + r, true);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (sliderPos - r + 1.0f, cy - r + 2.0f, 2 * r, 2 * r);
        g.setGradientFill (knob);
        g.fillEllipse (sliderPos - r, cy - r, 2 * r, 2 * r);
        g.setColour (Colour (0xff3a2208));
        g.drawEllipse (sliderPos - r, cy - r, 2 * r, 2 * r, 1.2f);
    }

    void CompassLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& te)
    {
        using namespace juce;
        auto r = Rectangle<int> (width, height).toFloat().reduced (0.5f);
        ColourGradient body (Colour (0xff2e2621), 0, 0, Colour (0xff0b0806), 0, (float) height, false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, 7.0f);
        ignoreUnused (te);
    }

    void CompassLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& te)
    {
        using namespace juce;
        auto r = Rectangle<int> (width, height).toFloat().reduced (1.0f);
        ColourGradient border (Palette::goldLight, 0, 0, Palette::goldDark, 0, (float) height, false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (r, 7.0f, te.hasKeyboardFocus (true) ? 2.4f : 1.6f);
    }
}

//==============================================================================
namespace kcui
{
    PlainChoiceBox::PlainChoiceBox (Assets& a, StringArray displayNames, StringArray menuDescriptions)
        : assets (a), names (std::move (displayNames)), descriptions (std::move (menuDescriptions))
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    void PlainChoiceBox::paint (Graphics& g)
    {
        paintChoiceFace (g, assets, getLocalBounds().toFloat().reduced (1.5f), names[index], isMouseOver (true));
    }

    void PlainChoiceBox::mouseDown (const MouseEvent& e)
    {
        if (! e.mods.isLeftButtonDown() && ! e.mods.isPopupMenu())
            return;
        showChoiceMenu (*this, names, descriptions, index, [this] (int i)
        {
            setIndex (i);
            if (onChange) onChange (index);
        });
    }

    PlainStepper::PlainStepper (Assets& a, StringArray valueNames, int defIndex)
        : assets (a), names (std::move (valueNames)), index (defIndex), defaultIndex (defIndex)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    void PlainStepper::paint (Graphics& g)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.5f);
        paintEnamelBox (g, r, isMouseOver (true));
        auto arrow = [&] (float cx, bool left, bool pressed, bool enabled)
        {
            const float cy = r.getCentreY(), s = 8.5f;
            Path p;
            if (left) p.addTriangle (cx - s * 0.6f, cy, cx + s * 0.6f, cy - s, cx + s * 0.6f, cy + s);
            else      p.addTriangle (cx + s * 0.6f, cy, cx - s * 0.6f, cy - s, cx - s * 0.6f, cy + s);
            g.setColour ((pressed ? Palette::goldLight : Palette::gold).withMultipliedAlpha (enabled ? 1.0f : 0.35f));
            g.fillPath (p);
        };
        arrow (r.getX() + 20.0f, true, pressedZone == -1, index > 0);
        arrow (r.getRight() - 20.0f, false, pressedZone == 1, index < names.size() - 1);
        const String text = names[index];
        g.setColour (index == defaultIndex ? Palette::cream : Palette::goldLight);
        g.setFont (text.containsAnyOf ("0123456789") ? assets.bask (r.getHeight() * 0.48f) : assets.cinzel (r.getHeight() * 0.46f));
        g.drawFittedText (text, r.reduced (34.0f, 2.0f).toNearestInt(), Justification::centred, 1, 0.75f);
    }

    void PlainStepper::step (int d)
    {
        const int i = jlimit (0, names.size() - 1, index + d);
        if (i == index) return;
        index = i;
        repaint();
        if (onChange) onChange (index);
    }

    void PlainStepper::mouseDown (const MouseEvent& e)
    {
        pressedZone = e.position.x < 42.0f ? -1 : (e.position.x > (float) getWidth() - 42.0f ? 1 : 0);
        if (pressedZone == 0)
        {
            // click in the middle: a menu of all values
            showChoiceMenu (*this, names, {}, index, [this] (int i) { step (i - index); });
            return;
        }
        step (pressedZone);
        repaint();
    }

    void PlainStepper::mouseUp (const MouseEvent&)            { pressedZone = 0; repaint(); }
    void PlainStepper::mouseDoubleClick (const MouseEvent& e)
    {
        if (e.position.x >= 42.0f && e.position.x <= (float) getWidth() - 42.0f)
            step (defaultIndex - index);
    }
    void PlainStepper::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
    {
        const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX;
        if (std::abs (d) > 1.0e-6f) step (d > 0 ? 1 : -1);
    }
}
