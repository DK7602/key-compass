// Key Compass - shared assets
#include "Theme.h"
#include "BinaryData.h"

namespace kcui
{
    Assets::Assets()
    {
        cinzelBold     = juce::Typeface::createSystemTypefaceFor (BinaryData::CinzelBold_ttf, (size_t) BinaryData::CinzelBold_ttfSize);
        baskBold       = juce::Typeface::createSystemTypefaceFor (BinaryData::BaskervilleBold_ttf, (size_t) BinaryData::BaskervilleBold_ttfSize);
        baskBoldItalic = juce::Typeface::createSystemTypefaceFor (BinaryData::BaskervilleBoldItalic_ttf, (size_t) BinaryData::BaskervilleBoldItalic_ttfSize);
        baskRegular    = juce::Typeface::createSystemTypefaceFor (BinaryData::BaskervilleRegular_ttf, (size_t) BinaryData::BaskervilleRegular_ttfSize);

        background = juce::ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
        face       = juce::ImageCache::getFromMemory (BinaryData::face_jpg, BinaryData::face_jpgSize);
        mapperBackground = juce::ImageCache::getFromMemory (BinaryData::mapper_bg_jpg, BinaryData::mapper_bg_jpgSize);
        plaque     = juce::ImageCache::getFromMemory (BinaryData::plaque_png, BinaryData::plaque_pngSize);
    }

    float textWidth (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }

    void drawGlowingText (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                          juce::Rectangle<float> area, juce::Justification just,
                          juce::Colour textColour, juce::Colour glowColour, int glowRadius, float glowStrength)
    {
        juce::GlyphArrangement ga;
        ga.addFittedText (font, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1, 0.75f);
        juce::Path p;
        ga.createPath (p);

        if (glowStrength > 0.01f && glowRadius > 0)
        {
            // wide halo + tighter halo (+ a hot core when strength > 1) = neon-style glow
            const juce::DropShadow wide (glowColour.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, glowStrength)), glowRadius, {});
            wide.drawForPath (g, p);
            if (glowStrength > 1.0f)
                wide.drawForPath (g, p);   // second pass = brighter halo
            juce::DropShadow (glowColour.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, glowStrength * 0.85f)),
                              juce::jmax (2, (int) ((float) glowRadius * 0.5f)), {}).drawForPath (g, p);
            if (glowStrength > 1.0f)
                juce::DropShadow (glowColour.brighter (0.8f).withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, glowStrength - 1.0f)),
                                  juce::jmax (2, glowRadius / 5), {}).drawForPath (g, p);
        }

        g.setColour (textColour);
        g.fillPath (p);
    }
}
