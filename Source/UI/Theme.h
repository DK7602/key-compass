// Key Compass - shared assets, colours and layout (all layout values are in "design" pixels,
// i.e. pixels of the 1090 x 1390 painted background; the editor scales everything uniformly).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kcui
{
    //==============================================================================
    struct Assets
    {
        Assets();

        juce::Typeface::Ptr cinzelBold, baskBold, baskBoldItalic, baskRegular;
        juce::Image background, face;
        juce::Image mapperBackground, plaque;     // Song Mapper page art (1.5x / 2x design size)
        juce::Image mobilePanel, mobileMap;       // phone / tablet layouts: a blank framed panel (1.5x), the old map

        juce::Font cinzel (float height) const          { return juce::Font (juce::FontOptions (cinzelBold).withHeight (height)); }
        juce::Font bask (float height) const            { return juce::Font (juce::FontOptions (baskBold).withHeight (height)); }
        juce::Font baskItalic (float height) const      { return juce::Font (juce::FontOptions (baskBoldItalic).withHeight (height)); }
        juce::Font baskPlain (float height) const       { return juce::Font (juce::FontOptions (baskRegular).withHeight (height)); }
    };

    //==============================================================================
    namespace Palette
    {
        const juce::Colour ink          { 0xff2a1a0c };
        const juce::Colour inkSoft      { 0xff6b4d2c };
        const juce::Colour gold         { 0xffe2b04a };
        const juce::Colour goldLight    { 0xfff9dc8f };
        const juce::Colour goldDark     { 0xff8a5a18 };
        const juce::Colour goldDim      { 0xffb99650 };
        const juce::Colour cream        { 0xfff4e7c9 };
        const juce::Colour parchment    { 0xfff5e6c4 };
        const juce::Colour blue         { 0xff3fa9ff };
        const juce::Colour blueLight    { 0xffa9e0ff };
        const juce::Colour blueDeep     { 0xff0b2c63 };
        const juce::Colour red          { 0xffd23a24 };
        const juce::Colour amber        { 0xffffb35c };
        const juce::Colour plateTop     { 0xff2a2119 };
        const juce::Colour plateBottom  { 0xff0d0a07 };
    }

    //==============================================================================
    namespace Layout
    {
        constexpr int designWidth  = 1090;
        constexpr int designHeight = 1390;

        // Compass dial
        constexpr float dialCx = 544.0f, dialCy = 911.0f;
        constexpr float faceRadius = 263.0f;
        constexpr float tickOuter = 251.0f, tickInner = 236.0f;
        constexpr float wedgeOuter = 233.0f, wedgeInner = 160.0f;
        constexpr float letterRadius = 197.0f;
        constexpr float roseRing = 150.0f;

        // Lid (display window)
        const juce::Rectangle<float> keyText        { 383.0f, 256.0f, 331.0f, 86.0f };
        const juce::Rectangle<float> runnerUpArea   { 492.0f, 334.0f, 262.0f, 30.0f };
        const juce::Rectangle<float> confidenceBar  { 341.0f, 371.0f, 329.0f, 50.0f };
        const juce::Rectangle<float> percentBox     { 674.0f, 372.0f, 80.0f, 47.0f };
        const juce::Rectangle<float> familyBox      { 346.0f, 453.0f, 397.0f, 43.0f };
        const juce::Rectangle<float> historyLabel   { 470.0f, 499.0f, 156.0f, 25.0f };
        const juce::Rectangle<float> historyBox     { 390.0f, 523.0f, 310.0f, 40.0f };
        const juce::Rectangle<int>   lidArea        { 330, 240, 440, 332 };

        // Controls (cover the painted placeholders)
        const juce::Rectangle<int> modeBox      { 44, 897, 148, 50 };
        const juce::Rectangle<int> responseBox  { 38, 1053, 170, 54 };
        const juce::Rectangle<int> tuningBox    { 894, 898, 154, 49 };
        const juce::Rectangle<int> lockButton   { 922 - 33, 1092 - 33, 66, 66 };
        const juce::Rectangle<int> flatsButton  { 1018 - 33, 1092 - 33, 66, 66 };
        const juce::Rectangle<int> clearButton  { 895, 1158, 141, 73 };
    }

    //==============================================================================
    // Point at angle (radians, clockwise from 12 o'clock) and radius around a centre
    inline juce::Point<float> polar (juce::Point<float> centre, float angle, float radius) noexcept
    {
        return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
    }

    // Draws text as a path with a soft glow behind it
    void drawGlowingText (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                          juce::Rectangle<float> area, juce::Justification just,
                          juce::Colour textColour, juce::Colour glowColour, int glowRadius, float glowStrength);

    float textWidth (const juce::Font& font, const juce::String& text);

    // Touch screens (the Android app): no hover tooltips, press-and-hold instead of right-click,
    // bigger menu rows. On by default on phones and tablets; the desktop tests can switch it on.
    bool& touchUIFlag() noexcept;
    inline bool isTouchUI() noexcept { return touchUIFlag(); }
    inline int menuItemHeight() noexcept { return isTouchUI() ? 46 : 30; }
}
