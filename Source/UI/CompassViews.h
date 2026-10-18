// Key Compass - display layers
//  StaticLayer : painted background + parchment face + tick ring + compass rose (cached)
//  DialLayer   : key wedge, note letters, needle, live chord plate
//  LidLayer    : detected key, confidence, key family, chord history
#pragma once

#include "Theme.h"
#include "UiState.h"

namespace kcui
{
    class StaticLayer : public juce::Component
    {
    public:
        explicit StaticLayer (Assets& a);
        void paint (juce::Graphics&) override;

    private:
        void paintTickRing (juce::Graphics&, juce::Point<float> c);
        void paintRose (juce::Graphics&, juce::Point<float> c);
        Assets& assets;
    };

    //==============================================================================
    class DialLayer : public juce::Component
    {
    public:
        DialLayer (Assets& a, const UiState& s);
        void paint (juce::Graphics&) override;

        bool showPlate = true;   // turned off only when rendering the app icon

    private:
        void paintWedge (juce::Graphics&, juce::Point<float> c);
        void paintLetters (juce::Graphics&, juce::Point<float> c);
        void paintNeedle (juce::Graphics&, juce::Point<float> c);
        void paintHub (juce::Graphics&, juce::Point<float> c);
        void paintPlate (juce::Graphics&, juce::Point<float> c, bool textPass);

        Assets& assets;
        const UiState& state;
        juce::Point<float> origin;
    };

    //==============================================================================
    class LidLayer : public juce::Component
    {
    public:
        LidLayer (Assets& a, const UiState& s);
        void paint (juce::Graphics&) override;

    private:
        void paintKey (juce::Graphics&);
        void paintRunnerUp (juce::Graphics&);
        void paintConfidence (juce::Graphics&);
        void paintFamily (juce::Graphics&);
        void paintHistory (juce::Graphics&);
        static void paintCreamBox (juce::Graphics&, juce::Rectangle<float>);

        Assets& assets;
        const UiState& state;
    };
}
