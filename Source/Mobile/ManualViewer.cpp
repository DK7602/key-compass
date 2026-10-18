// Key Compass - the manual inside the Android app (see ManualViewer.h)
#if defined (__ANDROID__)
 #define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#endif
#include "ManualViewer.h"

using namespace juce;

//==============================================================================
#if JUCE_ANDROID
namespace juce
{
   #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
     STATICMETHOD (open, "open", "(Ljava/io/File;I)Landroid/os/ParcelFileDescriptor;") \
     METHOD (close, "close", "()V")
    DECLARE_JNI_CLASS (KcParcelFd, "android/os/ParcelFileDescriptor")
   #undef JNI_CLASS_MEMBERS

   #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
     METHOD (constructor,  "<init>",       "(Landroid/os/ParcelFileDescriptor;)V") \
     METHOD (getPageCount, "getPageCount", "()I") \
     METHOD (openPage,     "openPage",     "(I)Landroid/graphics/pdf/PdfRenderer$Page;") \
     METHOD (close,        "close",        "()V")
    DECLARE_JNI_CLASS (KcPdfRenderer, "android/graphics/pdf/PdfRenderer")
   #undef JNI_CLASS_MEMBERS

   #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
     METHOD (getWidth,  "getWidth",  "()I") \
     METHOD (getHeight, "getHeight", "()I") \
     METHOD (render,    "render",    "(Landroid/graphics/Bitmap;Landroid/graphics/Rect;Landroid/graphics/Matrix;I)V") \
     METHOD (close,     "close",     "()V")
    DECLARE_JNI_CLASS (KcPdfPage, "android/graphics/pdf/PdfRenderer$Page")
   #undef JNI_CLASS_MEMBERS

   #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
     METHOD (eraseColor, "eraseColor", "(I)V")
    DECLARE_JNI_CLASS (KcBitmapOps, "android/graphics/Bitmap")
   #undef JNI_CLASS_MEMBERS
}

namespace
{
    bool javaFailed (JNIEnv* env)
    {
        if (! env->ExceptionCheck()) return false;
        env->ExceptionClear();
        return true;
    }

    class AndroidPdf final : public ManualViewer::Renderer
    {
    public:
        bool open (const File& file)
        {
            auto* env = getEnv();
            LocalRef<jobject> javaFile (env->NewObject (JavaFile, JavaFile.constructor, javaString (file.getFullPathName()).get()));
            if (javaFailed (env) || javaFile == nullptr) return false;
            constexpr jint modeReadOnly = 0x10000000;
            LocalRef<jobject> fd (env->CallStaticObjectMethod (KcParcelFd, KcParcelFd.open, javaFile.get(), modeReadOnly));
            if (javaFailed (env) || fd == nullptr) return false;
            LocalRef<jobject> r (env->NewObject (KcPdfRenderer, KcPdfRenderer.constructor, fd.get()));
            if (javaFailed (env) || r == nullptr)
            {
                env->CallVoidMethod (fd.get(), KcParcelFd.close);
                javaFailed (env);
                return false;
            }
            descriptor = GlobalRef (fd);
            pdf = GlobalRef (r);
            const int n = env->CallIntMethod (pdf.get(), KcPdfRenderer.getPageCount);
            if (javaFailed (env)) return false;
            for (int i = 0; i < n; ++i)
            {
                LocalRef<jobject> page (env->CallObjectMethod (pdf.get(), KcPdfRenderer.openPage, (jint) i));
                if (javaFailed (env) || page == nullptr) { sizes.push_back ({ 612, 792 }); continue; }
                sizes.push_back ({ env->CallIntMethod (page.get(), KcPdfPage.getWidth), env->CallIntMethod (page.get(), KcPdfPage.getHeight) });
                env->CallVoidMethod (page.get(), KcPdfPage.close);
                javaFailed (env);
            }
            return ! sizes.empty();
        }

        ~AndroidPdf() override
        {
            auto* env = getEnv();
            if (pdf.get() != nullptr)        { env->CallVoidMethod (pdf.get(), KcPdfRenderer.close); javaFailed (env); }
            if (descriptor.get() != nullptr) { env->CallVoidMethod (descriptor.get(), KcParcelFd.close); javaFailed (env); }
        }

        int pageCount() override               { return (int) sizes.size(); }
        Point<int> pageSize (int i) override    { return isPositiveAndBelow (i, (int) sizes.size()) ? sizes[(size_t) i] : Point<int> (612, 792); }

        Image render (int index, int w) override
        {
            if (! isPositiveAndBelow (index, (int) sizes.size()) || w < 8) return {};
            const auto sz = sizes[(size_t) index];
            const int h = jmax (8, roundToInt ((double) w * sz.y / jmax (1, sz.x)));
            auto* env = getEnv();
            LocalRef<jobject> page (env->CallObjectMethod (pdf.get(), KcPdfRenderer.openPage, (jint) index));
            if (javaFailed (env) || page == nullptr) return {};
            LocalRef<jobject> config (env->CallStaticObjectMethod (AndroidBitmapConfig, AndroidBitmapConfig.valueOf, javaString ("ARGB_8888").get()));
            LocalRef<jobject> bitmap (env->CallStaticObjectMethod (AndroidBitmap, AndroidBitmap.createBitmap, (jint) w, (jint) h, config.get()));
            Image img;
            if (! javaFailed (env) && bitmap != nullptr)
            {
                env->CallVoidMethod (bitmap.get(), KcBitmapOps.eraseColor, (jint) 0xffffffff);      // the PDF draws on white paper
                constexpr jint renderForDisplay = 1;
                env->CallVoidMethod (page.get(), KcPdfPage.render, bitmap.get(), (jobject) nullptr, (jobject) nullptr, renderForDisplay);
                if (! javaFailed (env))
                {
                    LocalRef<jintArray> pixels (env->NewIntArray (w * h));
                    env->CallVoidMethod (bitmap.get(), AndroidBitmap.getPixels, pixels.get(), 0, (jint) w, 0, 0, (jint) w, (jint) h);
                    if (! javaFailed (env))
                    {
                        img = Image (Image::ARGB, w, h, false);
                        Image::BitmapData data (img, Image::BitmapData::writeOnly);
                        auto* src = env->GetIntArrayElements (pixels.get(), nullptr);
                        // Android's ARGB ints are the same as JUCE's ARGB pixels (and the page is opaque)
                        for (int y = 0; y < h; ++y)
                            std::memcpy (data.getLinePointer (y), src + (size_t) y * (size_t) w, (size_t) w * 4);
                        env->ReleaseIntArrayElements (pixels.get(), src, JNI_ABORT);
                    }
                }
                env->CallVoidMethod (bitmap.get(), AndroidBitmap.recycle);
                javaFailed (env);
            }
            env->CallVoidMethod (page.get(), KcPdfPage.close);
            javaFailed (env);
            return img;
        }

    private:
        GlobalRef descriptor, pdf;
        std::vector<Point<int>> sizes;
    };
}

std::unique_ptr<ManualViewer::Renderer> ManualViewer::openPdf (const File& f)
{
    auto pdf = std::make_unique<AndroidPdf>();
    if (! pdf->open (f)) return {};
    return pdf;
}
#else
std::unique_ptr<ManualViewer::Renderer> ManualViewer::openPdf (const File&)
{
    return {};
}
#endif

//==============================================================================
// The pages, one under the other; each is drawn when it scrolls into view and forgotten when far away
//==============================================================================
class ManualViewer::Pages : public Component
{
public:
    Pages (Renderer& r, Viewport& v) : renderer (r), viewport (v)
    {
        for (int i = 0; i < renderer.pageCount(); ++i)
            images.push_back ({});
    }

    static constexpr int gap = 12;

    void layout (int width)
    {
        rects.clear();
        pageWidth = width;
        int y = gap;
        for (int i = 0; i < renderer.pageCount(); ++i)
        {
            const auto sz = renderer.pageSize (i);
            const int h = roundToInt ((double) width * sz.y / jmax (1, sz.x));
            rects.push_back ({ gap, y, width, h });
            y += h + gap;
        }
        setSize (width + 2 * gap, y);
        for (auto& im : images) if (im.isValid() && std::abs (im.getWidth() - wantedPixels()) > 4) im = {};
        repaint();
    }

    int wantedPixels() const
    {
        float scale = 1.0f;
        if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
            scale = (float) d->scale;
        return jlimit (200, 2600, roundToInt ((float) pageWidth * jmax (1.0f, scale)));
    }

    // draws one missing page that is in view (or close to it); false when nothing is missing
    bool renderNext()
    {
        const auto visible = viewport.getViewArea().expanded (0, viewport.getHeight());
        for (int i = 0; i < (int) rects.size(); ++i)
        {
            const bool near = rects[(size_t) i].intersects (visible);
            if (! near)
            {
                // keep the memory down: pages far away are drawn again when they come back
                if (images[(size_t) i].isValid() && ! rects[(size_t) i].intersects (visible.expanded (0, viewport.getHeight() * 2)))
                    images[(size_t) i] = {};
                continue;
            }
            if (! images[(size_t) i].isValid())
            {
                images[(size_t) i] = renderer.render (i, wantedPixels());
                repaint (rects[(size_t) i]);
                return true;
            }
        }
        return false;
    }

    void paint (Graphics& g) override
    {
        const auto clip = g.getClipBounds();
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        for (int i = 0; i < (int) rects.size(); ++i)
        {
            const auto r = rects[(size_t) i];
            if (! r.intersects (clip)) continue;
            g.setColour (Colours::black.withAlpha (0.5f));
            g.fillRect (r.translated (2, 3));
            if (images[(size_t) i].isValid())
            {
                g.setOpacity (1.0f);
                g.drawImage (images[(size_t) i], r.toFloat());
            }
            else
            {
                g.setColour (Colours::white);
                g.fillRect (r);
                g.setColour (Colours::grey);
                g.setFont (FontOptions (16.0f));
                g.drawText ("Page " + String (i + 1), r, Justification::centred, false);
            }
        }
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (onDoubleTap) onDoubleTap (e.getPosition());
    }

    std::function<void (Point<int>)> onDoubleTap;

private:
    Renderer& renderer;
    Viewport& viewport;
    std::vector<Rectangle<int>> rects;
    std::vector<Image> images;
    int pageWidth = 100;
};

//==============================================================================
ManualViewer::ManualViewer (kcui::Assets& a, std::unique_ptr<Renderer> r)
    : assets (a), renderer (std::move (r)),
      closeButton (a, String (CharPointer_UTF8 ("\xc3\x97")), kcui::KButton::Look::Round),
      smallerButton (a, String (CharPointer_UTF8 ("\xe2\x88\x92")), kcui::KButton::Look::Round),
      biggerButton (a, "+", kcui::KButton::Look::Round)
{
    setOpaque (true);
    pages = std::make_unique<Pages> (*renderer, viewport);
    viewport.setViewedComponent (pages.get(), false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (6);
    viewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::all);
    addAndMakeVisible (viewport);
    pages->onDoubleTap = [this] (Point<int>) { setZoom (zoom > 1.2f ? 1.0f : 2.0f); };

    for (auto* b : { &closeButton, &smallerButton, &biggerButton })
        addAndMakeVisible (*b);
    closeButton.fontSize = 28.0f;
    smallerButton.fontSize = biggerButton.fontSize = 26.0f;
    closeButton.onClick = [this] { if (onClose) onClose(); };
    smallerButton.onClick = [this] { setZoom (zoom / 1.4f); };
    biggerButton.onClick = [this] { setZoom (zoom * 1.4f); };
    startTimerHz (20);
}

ManualViewer::~ManualViewer()
{
    stopTimer();
    viewport.setViewedComponent (nullptr, false);
}

void ManualViewer::setZoom (float z)
{
    const float old = zoom;
    zoom = jlimit (1.0f, 3.5f, z);
    if (std::abs (old - zoom) < 0.001f) return;
    // keep the middle of what you were reading in the middle
    const auto view = viewport.getViewArea();
    const double fx = (view.getCentreX()) / (double) jmax (1, pages->getWidth());
    const double fy = (view.getCentreY()) / (double) jmax (1, pages->getHeight());
    resized();
    viewport.setViewPosition (roundToInt (fx * pages->getWidth() - viewport.getWidth() / 2.0),
                              roundToInt (fy * pages->getHeight() - viewport.getHeight() / 2.0));
}

void ManualViewer::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (58).reduced (8, 6);
    closeButton.setBounds (bar.removeFromRight (46).withSizeKeepingCentre (44, 44));
    bar.removeFromRight (10);
    biggerButton.setBounds (bar.removeFromRight (46).withSizeKeepingCentre (42, 42));
    smallerButton.setBounds (bar.removeFromRight (46).withSizeKeepingCentre (42, 42));
    titleArea = bar;
    viewport.setBounds (r);
    const int w = jmax (100, roundToInt ((float) (viewport.getWidth() - 2 * Pages::gap) * zoom));
    pages->layout (w);
}

void ManualViewer::paint (Graphics& g)
{
    g.fillAll (Colour (0xff17110c));
    auto bar = getLocalBounds().removeFromTop (58).toFloat();
    ColourGradient body (Colour (0xff2e2621), bar.getX(), bar.getY(), Colour (0xff0b0806), bar.getX(), bar.getBottom(), false);
    g.setGradientFill (body);
    g.fillRect (bar);
    g.setColour (kcui::Palette::goldDark);
    g.fillRect (bar.removeFromBottom (2.0f));
    g.setColour (kcui::Palette::goldLight);
    g.setFont (assets.cinzel (20.0f));
    g.drawFittedText ("KEY COMPASS MANUAL", titleArea.reduced (6, 0), Justification::centredLeft, 1, 0.7f);
}

void ManualViewer::timerCallback()
{
    pages->renderNext();
}
