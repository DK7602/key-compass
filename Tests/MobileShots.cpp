// Key Compass - draws the Android app's screens at phone and tablet sizes into PNG files (developer test, Linux).
//   KeyCompassMobileShots <output folder> [name filter]
// Each shot sets the screen size (with the phone's status and navigation bars), picks a tab, sometimes does
// something on the screen, and saves what it shows.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Mobile/MobileEditor.h"

using namespace juce;

namespace
{
    // a few seconds of F# minor (F#m - D - A - E) into the detector, so the compass has something to show
    void feedTestAudio (KeyCompassProcessor& proc)
    {
        const double sr = 48000.0;
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        const int chords[4][3] = { { 54, 57, 61 }, { 50, 54, 57 }, { 57, 61, 64 }, { 52, 56, 59 } };
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        double phase[3] = { 0, 0, 0 };
        const int blocksPerChord = (int) (sr * 0.9 / 512);
        for (int round = 0; round < 2; ++round)
            for (int c = 0; c < 4; ++c)
                for (int b = 0; b < blocksPerChord; ++b)
                {
                    buf.clear();
                    for (int k = 0; k < 3; ++k)
                    {
                        const double f = 440.0 * std::pow (2.0, (chords[c][k] - 69) / 12.0);
                        for (int i = 0; i < 512; ++i)
                        {
                            const float v = 0.12f * (float) (std::sin (phase[k]) + 0.4 * std::sin (2.0 * phase[k]) + 0.2 * std::sin (3.0 * phase[k]));
                            buf.addSample (0, i, v);
                            buf.addSample (1, i, v);
                            phase[k] += MathConstants<double>::twoPi * f / sr;
                        }
                    }
                    proc.processBlock (buf, midi);
                    if (b % 8 == 0) Thread::sleep (2);
                }
        Thread::sleep (300);
    }

    // the song the user was working on: F#m G#dim A E, with a bass line and a lead
    void makeSong (KeyCompassProcessor& proc)
    {
        auto& song = proc.getSongMap();
        song.resetSections ({ { "Intro", 4 }, { "Verse", 8 }, { "Chorus", 8 }, { "Bridge", 8 } });
        auto chords = [] (const char* text)
        {
            std::vector<sm::Chord> out;
            for (auto& t : StringArray::fromTokens (text, " ", ""))
                out.push_back (sm::parseChord (t.toStdString()));
            return out;
        };
        song.setTray (chords ("F#m D A E"));
        song.insertChords (0, chords ("F#m G#dim A E"));
        song.insertChords (1, chords ("F#m D A E F#m D Bm C#"));
        song.insertChords (2, chords ("D A E F#m"));
        song.setLineEnabled (1, sm::Lane::Bass, true);
        song.fillBass (1, sm::BassFill::RootFifth);
        song.setLineEnabled (1, sm::Lane::Lead, true);
        std::vector<sm::Note> lead;
        const int pitches[] = { 66, 69, 68, 66, 64, 66, 69, 73 };
        for (int i = 0; i < 8; ++i) lead.push_back ({ i * 1.0, 1.0, pitches[i], 0.8f });
        song.setLineNotes (1, sm::Lane::Lead, lead);
        song.selectedSection = 1;
        song.selectedTray = 0;
        song.songName = "Desert Song";
        song.changed();
    }

    // stands in for Android's PDF renderer: lettered pages
    struct FakePdf : ManualViewer::Renderer
    {
        int pageCount() override                  { return 5; }
        Point<int> pageSize (int) override        { return { 612, 792 }; }
        Image render (int page, int w) override
        {
            const int h = w * 792 / 612;
            Image img (Image::ARGB, w, h, true);
            Graphics g (img);
            g.fillAll (Colours::white);
            g.setColour (Colours::black);
            g.setFont (FontOptions ((float) w / 14.0f, Font::bold));
            g.drawText ("Key Compass - page " + String (page + 1), 0, h / 12, w, h / 10, Justification::centred);
            g.setFont (FontOptions ((float) w / 40.0f));
            for (int i = 0; i < 30; ++i)
                g.drawText ("Lorem ipsum dolor sit amet, consectetur adipiscing elit - line " + String (i + 1),
                            w / 12, h / 5 + i * h / 45, w, h / 45, Justification::centredLeft);
            return img;
        }
    };

    TextEditor* findTextEditor (Component& root)
    {
        if (auto* t = dynamic_cast<TextEditor*> (&root)) return root.isShowing() ? t : nullptr;
        for (auto* c : root.getChildren())
            if (auto* t = findTextEditor (*c)) return t;
        return nullptr;
    }

    void pump (int ms)
    {
        MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    bool save (Component& c, const File& file)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        file.deleteFile();
        FileOutputStream out (file);
        return out.openedOk() && PNGImageFormat().writeImageToStream (img, out);
    }

    // a tap (or a press and hold) at a point of the editor, as a finger would do it
    void touch (Component& root, Point<float> p, int holdMs = 0)
    {
        auto src = Desktop::getInstance().getMainMouseSource();
        auto* target = root.getComponentAt (p.toInt());
        if (target == nullptr) return;
        const auto local = target->getLocalPoint (&root, p);
        const auto now = Time::getCurrentTime();
        const MouseEvent down (src, local, ModifierKeys::leftButtonModifier, MouseInputSource::defaultPressure, 0, 0, 0, 0,
                               target, target, now, local, now, 1, false);
        target->mouseDown (down);
        if (holdMs > 0) pump (holdMs);
        const MouseEvent up (src, local, {}, MouseInputSource::defaultPressure, 0, 0, 0, 0, target, target, Time::getCurrentTime(),
                             local, now, 1, false);
        target->mouseUp (up);
        pump (60);
    }
}

//==============================================================================
// Touch checks: finger-like events through the window (Linux has no touch source, so the mouse stands in;
// the app behaves as on a touch screen because the touch switch is on), then look at the song
namespace
{
    Component* findID (Component& root, const String& id)
    {
        if (root.getComponentID() == id) return &root;
        for (auto* c : root.getChildren())
            if (auto* f = findID (*c, id)) return f;
        return nullptr;
    }

    struct Finger
    {
        ComponentPeer& peer;
        Point<float> at;
        void send (Point<float> p, bool down)
        {
            at = p;
            const auto mods = down ? ModifierKeys::currentModifiers.withFlags (ModifierKeys::leftButtonModifier)
                                   : ModifierKeys::currentModifiers.withoutMouseButtons();
            peer.handleMouseEvent (MouseInputSource::InputSourceType::mouse, p, mods, 1.0f, 0.0f, Time::currentTimeMillis());
        }
        void down (Point<float> p) { send (p, true); pump (30); }
        void move (Point<float> p, int steps = 8)
        {
            const auto from = at;
            for (int i = 1; i <= steps; ++i) { send (from + (p - from) * ((float) i / (float) steps), true); pump (16); }
        }
        void up() { send (at, false); pump (80); }
        void tap (Point<float> p) { down (p); up(); }
        void hold (Point<float> p, int ms = 700) { down (p); pump (ms); up(); }
    };

    // a point of a component, in the editor's (= the window's) coordinates
    Point<float> pointIn (Component& root, Component& c, float fx, float fy)
    {
        const auto local = Point<float> ((float) c.getWidth() * fx, (float) c.getHeight() * fy);
        return root.getLocalPoint (&c, local);
    }

    int runChecks (KeyCompassProcessor& proc)
    {
        int failed = 0;
        auto check = [&failed] (bool ok, const String& what)
        {
            std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
            if (! ok) ++failed;
        };
        auto& song = proc.getSongMap();
        proc.setMobileTab (MobileEditor::chordsTab);
        MobileEditor ed (proc);
        ed.setSimulatedInsets ({ 36, 0, 24, 0 });
        ed.setBounds (0, 0, 412, 915);
        ed.addToDesktop (ComponentPeer::windowIgnoresKeyPresses);
        ed.setVisible (true);
        pump (300);
        auto* peer = ed.getPeer();
        if (peer == nullptr) { std::cout << "no window" << std::endl; return 1; }
        Finger f { *peer, {} };
        struct Spy : MouseListener
        {
            void mouseDown (const MouseEvent& e) override { if (verbose) std::cout << "    down on " << typeid (*e.eventComponent).name() << " " << e.getPosition().toString() << std::endl; }
            void mouseUp (const MouseEvent& e) override   { if (verbose) std::cout << "    up on " << typeid (*e.eventComponent).name() << std::endl; }
            bool verbose = SystemStats::getEnvironmentVariable ("KC_SPY", {}).isNotEmpty();
        } spy;
        ed.addMouseListener (&spy, true);
        std::cout << "  window at " << ed.getScreenBounds().toString() << ", showing " << (int) ed.isShowing() << std::endl;

        // --- the chords tab: tap a progression card, press and hold for its menu
        auto* grid = findID (ed, "trayGrid");
        check (grid != nullptr && grid->isShowing(), "chords tab shows the progression cards");
        if (grid != nullptr)
        {
            song.selectedTray = 0;
            f.tap (pointIn (ed, *grid, 0.375f, 0.25f));       // the second card (D)
            check (song.selectedTray == 1 && ed.getMapper().focused.id() == std::string ("D"), "a tap picks the D card");
            f.hold (pointIn (ed, *grid, 0.125f, 0.25f));
            check (PopupMenu::dismissAllActiveMenus(), "press and hold opens the card's menu");
            pump (100);
        }

        // --- the tabs
        ed.showTab (MobileEditor::songTab);
        pump (200);
        const auto tabY = 915.0f - 24.0f - 30.0f;
        f.tap ({ 412.0f * 0.7f, tabY });         // LIBRARY is the fourth of five
        check (ed.currentTab() == MobileEditor::libraryTab, "tapping LIBRARY in the tab bar");
        f.tap ({ 412.0f * 0.5f, tabY });
        check (ed.currentTab() == MobileEditor::songTab, "tapping SONG in the tab bar");
        pump (200);

        // --- the song tab: tap the bass roll to add a note, swipe it to scroll
        ed.getMapper().showSection (1);
        pump (200);
        auto* lane = findID (ed, "lane1bass");
        auto* vp = dynamic_cast<Viewport*> (findID (ed, "timelineViewport"));
        check (lane != nullptr && lane->isShowing() && vp != nullptr, "the verse's bass row is on screen");
        if (lane != nullptr && vp != nullptr)
        {
            const auto before = song.line (1, sm::Lane::Bass).notes.size();
            // near the top of the roll (high notes the fill doesn't use), early in the bar
            f.tap (pointIn (ed, *lane, 0.30f, 0.27f));
            check (song.line (1, sm::Lane::Bass).notes.size() == before + 1, "a tap on the roll adds a bass note");
            song.undo();
            pump (100);
            const auto startY = vp->getViewPositionY();
            auto* lane2 = findID (ed, "lane1bass");
            f.down (pointIn (ed, *lane2, 0.62f, 0.30f));
            f.move (f.at.translated (0.0f, -160.0f));
            f.up();
            pump (400);
            check (vp->getViewPositionY() > startY + 60, "a swipe up on the roll scrolls the song (" + String (startY) + " -> " + String (vp->getViewPositionY()) + ")");
            check (song.line (1, sm::Lane::Bass).notes.size() == before, "and adds no note");
        }

        // --- a chip in a section: tap shows the chord, press and hold for its menu
        pump (1500);                         // let the swipe's glide come to rest
        ed.getMapper().showSection (0);
        pump (200);
        if (auto* row = findID (ed, "section0"))
        {
            // the chips start under the 86 px header: F#m, G#dim, A, E
            const float sx = (float) row->getWidth(), sy = (float) row->getHeight();
            f.tap (ed.getLocalPoint (row, Point<float> (8.0f + 66.0f + 6.0f + 33.0f, 86.0f + 8.0f + 20.0f)));
            check (ed.getMapper().focused.id() == std::string ("G#dim"), "a tap on the G#dim chip shows it");
            f.hold (ed.getLocalPoint (row, Point<float> (8.0f + 33.0f, 86.0f + 8.0f + 20.0f)));
            check (PopupMenu::dismissAllActiveMenus(), "press and hold on a chip opens its menu");
            ignoreUnused (sx, sy);
            pump (100);
        }
        else check (false, "the intro's row");

        // --- practice view and the back button
        ed.getMapper().showPractice (-2);
        pump (200);
        check (ed.getMapper().isPracticeOpen(), "practice view opens");
        check (ed.handleBackButton() && ! ed.getMapper().isPracticeOpen(), "the back button closes it");
        pump (100);
        check (ed.handleBackButton() && ed.currentTab() == MobileEditor::compassTab, "back again goes to the compass");
        check (! ed.handleBackButton(), "back on the compass leaves the app");

        // --- song files
        const auto xml = ed.getMapper().songFileXml();
        song.clearAllChords();
        check (ed.getMapper().loadSongXml (xml) && song.sections.size() == 4 && song.sections[1].chords.size() == 8, "a song file loads back");
        check (! ed.getMapper().loadSongXml ("<html/>"), "something else is refused");
        song.undo();
        pump (100);
        song.redo();

        // --- rotate: a phone on its side, a tablet
        ed.setBounds (0, 0, 915, 412);
        pump (100);
        check (ed.currentLayout() == MobileEditor::Layout::Wide, "on its side: tabs down the left");
        ed.setBounds (0, 0, 800, 1280);
        pump (100);
        check (ed.currentLayout() == MobileEditor::Layout::Classic, "an upright tablet: the desktop pages");
        ed.setBounds (0, 0, 1280, 800);
        pump (100);
        check (ed.currentLayout() == MobileEditor::Layout::Wide && ed.panelsSideBySide() == 3, "a tablet on its side: three panels");
        ed.removeMouseListener (&spy);
        ed.removeFromDesktop();
        std::cout << (failed == 0 ? "all touch checks passed" : String (failed) + " touch checks FAILED") << std::endl;
        return failed;
    }
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    const String filter = argc > 2 ? String (argv[2]) : String();
    outDir.createDirectory();
    kcui::touchUIFlag() = true;

    AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Standalone);
    KeyCompassProcessor proc;
    AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Undefined);
    feedTestAudio (proc);
    makeSong (proc);
    if (filter == "checks")
        return runChecks (proc);

    struct Shot
    {
        String name;
        int w, h;
        BorderSize<int> bars;          // status bar, navigation bar (top, left, bottom, right)
        int tab;
        std::function<void (MobileEditor&)> action;
    };
    const BorderSize<int> phoneBars (36, 0, 24, 0), sideBars (24, 0, 20, 0), tabletBars (28, 0, 20, 0);
    using T = MobileEditor;
    std::vector<Shot> shots
    {
        { "phone_compass",   412, 915, phoneBars, T::compassTab, {} },
        { "phone_chords",    412, 915, phoneBars, T::chordsTab, {} },
        { "phone_song",      412, 915, phoneBars, T::songTab, {} },
        { "phone_library",   412, 915, phoneBars, T::libraryTab, {} },
        { "phone_settings",  412, 915, phoneBars, T::moreTab, {} },
        { "phone_practice",  412, 915, phoneBars, T::chordsTab, [] (MobileEditor& e) { e.getMapper().showPractice (-2); } },
        { "phone_piano",     412, 915, phoneBars, T::chordsTab, [&proc] (MobileEditor&) { proc.getSongMap().guitar = false; proc.getSongMap().changed(); } },
        { "phone_manual",    412, 915, phoneBars, T::moreTab, [] (MobileEditor& e) { e.showManualWith (std::make_unique<FakePdf>()); pump (600); } },
        { "phone_typing",    412, 915, phoneBars, T::chordsTab, [] (MobileEditor& e)
            {
                e.addToDesktop (0);
                e.setVisible (true);
                pump (200);
                if (auto* ed = findTextEditor (e)) ed->grabKeyboardFocus();
                e.setSimulatedInsets ({ 36, 0, 24, 0 }, 320);
                pump (100);
            } },
        { "side_compass",    915, 412, sideBars, T::compassTab, [&proc] (MobileEditor&) { proc.getSongMap().guitar = true; proc.getSongMap().changed(); } },
        { "side_chords",     915, 412, sideBars, T::chordsTab, {} },
        { "side_song",       915, 412, sideBars, T::songTab, {} },
        { "side_library",    915, 412, sideBars, T::libraryTab, {} },
        { "side_practice",   915, 412, sideBars, T::chordsTab, [] (MobileEditor& e) { e.getMapper().showPractice (-2); } },
        { "tablet_compass",  800, 1280, tabletBars, T::compassTab, {} },
        { "tablet_mapper",   800, 1280, tabletBars, T::songTab, {} },
        { "tabletside_compass", 1280, 800, tabletBars, T::compassTab, {} },
        { "tabletside_mapper",  1280, 800, tabletBars, T::songTab, {} },
        { "fold_chords",     673, 841, phoneBars, T::chordsTab, {} },
        { "small_phone",     360, 740, phoneBars, T::chordsTab, {} },
    };

    int failures = 0;
    for (const auto& s : shots)
    {
        if (filter.isNotEmpty() && ! s.name.contains (filter)) continue;
        proc.setMobileTab (s.tab);
        auto ed = std::make_unique<MobileEditor> (proc);
        ed->setSimulatedInsets (s.bars);
        ed->setSize (s.w, s.h);
        ed->showTab (s.tab);
        ed->tick (0.5f);
        pump (150);
        if (s.action) { s.action (*ed); pump (250); }
        ed->tick (0.5f);
        const auto f = outDir.getChildFile (s.name + ".png");
        if (! save (*ed, f)) { ++failures; std::cout << "FAILED " << f.getFullPathName() << std::endl; }
        else std::cout << "wrote " << f.getFileName() << "  (layout " << (int) ed->currentLayout() << ", panes " << ed->panelsSideBySide() << ")" << std::endl;
        ed->getMapper().closePractice();
        pump (50);
    }
    ignoreUnused (touch);
    return failures == 0 ? 0 : 1;
}
