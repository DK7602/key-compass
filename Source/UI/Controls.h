// Key Compass - custom controls and look-and-feel
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace kcui
{
    //==============================================================================
    class CompassLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit CompassLookAndFeel (Assets& a);

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                const juce::Colour* textColour) override;
        juce::Font getPopupMenuFont() override;
        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                        int& idealWidth, int& idealHeight) override;

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

        void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                            int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
        int getDefaultScrollbarWidth() override { return 10; }
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                               float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
        void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
        void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

        // The area the next tooltip must not cover (what it describes, or the whole panel it belongs to),
        // in the tooltip window's parent coordinates. Set by TipWindow just before the tooltip is placed.
        juce::Rectangle<int> tooltipAvoid;

        // where a tooltip of this size goes: beside 'avoid', never over it, inside 'area'
        using TipMeasure = std::function<juce::Point<int> (int maxWidth)>;
        static juce::Rectangle<int> placeTooltip (juce::Point<int> mouse, juce::Rectangle<int> avoid, juce::Rectangle<int> area,
                                                  const TipMeasure& measure);

    private:
        Assets& assets;
    };

    //==============================================================================
    // Tooltips that keep out of the way: shown beside the control - or beside the whole panel it sits in
    // (any ancestor with the property "kcTipArea") - never on top of it. Once you click something, its tip
    // stays hidden until the mouse moves to something else.
    class TipWindow : public juce::TooltipWindow
    {
    public:
        TipWindow (juce::Component* parent, CompassLookAndFeel& lf, int delayMs);
        ~TipWindow() override;
        juce::String getTipFor (juce::Component&) override;
        juce::Rectangle<int> areaToAvoid (juce::Component&) const;     // in this window's parent coordinates
        static void markTipArea (juce::Component& c) { c.getProperties().set ("kcTipArea", true); }

    private:
        struct ClickWatcher : juce::MouseListener
        {
            juce::Component::SafePointer<juce::Component> pressed;
            void mouseDown (const juce::MouseEvent& e) override { pressed = e.eventComponent; }
        };
        ClickWatcher watcher;
        CompassLookAndFeel& laf;
    };

    //==============================================================================
    // Dark enamel box with a value and a chevron; click for a menu of choices
    class ChoiceBox : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChoiceBox (Assets&, juce::RangedAudioParameter& param, juce::StringArray displayNames,
                   juce::StringArray menuDescriptions);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    private:
        Assets& assets;
        juce::StringArray names, descriptions;
        int index = 0;
        juce::ParameterAttachment attachment;
    };

    //==============================================================================
    // "< 440.0 Hz >" reference pitch box: click arrows, drag, scroll, double-click to reset
    class TuningBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        TuningBox (Assets&, juce::RangedAudioParameter& param);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    private:
        void timerCallback() override;
        void step (float amount);
        int zoneAt (juce::Point<float> p) const;   // -1 left arrow, 1 right arrow, 0 middle

        Assets& assets;
        juce::RangedAudioParameter& parameter;
        float value = 440.0f;
        juce::ParameterAttachment attachment;
        int pressedZone = 0;
        float dragStartValue = 440.0f;
        bool dragging = false;
        float repeatStep = 0.0f;
    };

    //==============================================================================
    // The same enamel boxes, for settings that are not plug-in parameters (Song Mapper page)
    class PlainChoiceBox : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        PlainChoiceBox (Assets&, juce::StringArray displayNames, juce::StringArray menuDescriptions);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }
        void setIndex (int i)                { i = juce::jlimit (0, names.size() - 1, i); if (i != index) { index = i; repaint(); } }
        int getIndex() const noexcept        { return index; }
        std::function<void (int)> onChange;

    private:
        Assets& assets;
        juce::StringArray names, descriptions;
        int index = 0;
    };

    // "< value >" stepper in the Tuning box style: click the arrows, scroll, or double-click to reset
    class PlainStepper : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        PlainStepper (Assets&, juce::StringArray valueNames, int defaultIndex);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }
        void setIndex (int i)                { i = juce::jlimit (0, names.size() - 1, i); if (i != index) { index = i; repaint(); } }
        int getIndex() const noexcept        { return index; }
        std::function<void (int)> onChange;

    private:
        void step (int d);
        Assets& assets;
        juce::StringArray names;
        int index = 0, defaultIndex = 0, pressedZone = 0;
    };

    //==============================================================================
    // Round glass button (Lock Key / Flats) that glows blue when on
    class RoundToggle : public juce::Button
    {
    public:
        enum class Icon { Lock, Flat };
        RoundToggle (Assets&, Icon);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool hitTest (int x, int y) override;

    private:
        Assets& assets;
        Icon icon;
    };

    //==============================================================================
    // Invisible button over the painted CLEAR HISTORY plate (adds hover / press feedback)
    class PlateButton : public juce::Button
    {
    public:
        PlateButton() : juce::Button ("Clear History") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };
}
