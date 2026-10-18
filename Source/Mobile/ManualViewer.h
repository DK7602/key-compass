// Key Compass - the manual inside the Android app. Android has no "open this PDF" that works for an app's own
// files, so the pages are drawn with Android's own PDF renderer and shown here, one under the other.
#pragma once

#include "../UI/SongMapperView.h"

class ManualViewer : public juce::Component, private juce::Timer
{
public:
    // draws the pages of a document (Android's PdfRenderer; the desktop tests use a stand-in)
    struct Renderer
    {
        virtual ~Renderer() = default;
        virtual int pageCount() = 0;
        virtual juce::Point<int> pageSize (int page) = 0;                // in PDF points
        virtual juce::Image render (int page, int widthInPixels) = 0;
    };
    static std::unique_ptr<Renderer> openPdf (const juce::File&);        // nullptr where it isn't possible

    ManualViewer (kcui::Assets&, std::unique_ptr<Renderer>);
    ~ManualViewer() override;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setZoom (float newZoom);
    float getZoom() const noexcept { return zoom; }

private:
    class Pages;
    void timerCallback() override;

    kcui::Assets& assets;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Pages> pages;
    juce::Viewport viewport;
    kcui::KButton closeButton, smallerButton, biggerButton;
    juce::Rectangle<int> titleArea;
    float zoom = 1.0f;
};
