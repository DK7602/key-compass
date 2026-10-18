#include "MySongsView.h"
#include "../PluginProcessor.h"

namespace kcui
{
    using namespace juce;

    namespace
    {
        String utf8 (const char* s)   { return String (CharPointer_UTF8 (s)); }

        String ago (int64 ms)
        {
            const auto now = Time::getCurrentTime();
            const auto then = Time (ms);
            const auto secs = (now.toMilliseconds() - ms) / 1000;
            if (secs < 60)     return "just now";
            if (secs < 3600)   return String (secs / 60) + " min ago";
            if (secs < 86400 && then.getDayOfMonth() == now.getDayOfMonth()) return String (secs / 3600) + " h ago";
            if (secs < 2 * 86400) return "yesterday";
            if (secs < 7 * 86400) return String (secs / 86400) + " days ago";
            return then.formatted (then.getYear() == now.getYear() ? "%d %b" : "%d %b %Y");
        }

        int textHeight (const String& text, const Font& font, int width)
        {
            AttributedString s;
            s.append (text, font, Colours::black);
            s.setJustification (Justification::topLeft);
            TextLayout layout;
            layout.createLayout (s, (float) jmax (20, width));
            return (int) std::ceil (layout.getHeight()) + 2;
        }
    }

    //==============================================================================
    class MySongsView::SongRow : public Component
    {
    public:
        SongRow (MySongsView& v, const kcsync::Entry& e) : entry (e), view (v)
        {
            press.onLongPress = [this] { view.songMenu (entry, *this); };
            setComponentID ("song:" + e.name);
        }

        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            const bool current = entry.id == view.link.currentId();
            const bool hot = isMouseOverOrDragging() && ! isTouchUI();
            g.setColour (current ? Palette::gold.withAlpha (0.30f) : Colour (0xff6b4520).withAlpha (hot ? 0.13f : 0.06f));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (Colour (0xff6b4520).withAlpha (current ? 0.75f : 0.32f));
            g.drawRoundedRectangle (r, 7.0f, current ? 1.7f : 1.0f);

            auto t = r.reduced (12.0f, 7.0f);
            if (current)
            {
                auto tag = t.removeFromRight (64.0f).withSizeKeepingCentre (60.0f, 24.0f);
                g.setColour (Palette::goldDark.withAlpha (0.9f));
                g.fillRoundedRectangle (tag, 5.0f);
                g.setColour (Palette::goldLight);
                g.setFont (view.assets.cinzel (12.5f));
                g.drawText ("OPEN", tag, Justification::centred, false);
                t.removeFromRight (6.0f);
            }
            g.setColour (Palette::ink);
            g.setFont (view.assets.bask (19.0f));
            g.drawFittedText (entry.name.isEmpty() ? String ("Untitled") : entry.name,
                              t.removeFromTop (t.getHeight() * 0.56f).toNearestInt(), Justification::bottomLeft, 1, 0.75f);
            g.setColour (Palette::inkSoft);
            g.setFont (view.assets.baskItalic (14.0f));
            g.drawFittedText (view.describe (entry), t.toNearestInt(), Justification::topLeft, 1, 0.75f);
        }

        void mouseDown (const MouseEvent& e) override
        {
            press.mouseDown (e);
            if (e.mods.isPopupMenu()) view.songMenu (entry, *this);
        }
        void mouseDrag (const MouseEvent& e) override   { press.mouseDrag (e); }
        void mouseUp (const MouseEvent& e) override
        {
            press.mouseUp();
            if (press.fired() || e.mods.isPopupMenu() || e.getDistanceFromDragStart() > 8) return;
            view.link.openSong (entry.id);
            if (view.onClose) view.onClose();
        }
        void mouseEnter (const MouseEvent&) override    { repaint(); }
        void mouseExit (const MouseEvent&) override     { repaint(); }

        kcsync::Entry entry;

    private:
        MySongsView& view;
        LongPress press;
    };

    //==============================================================================
    MySongsView::MySongsView (SongMapperPage& p)
        : page (p), link (p.proc.getSongLink()), hub (link.getHub()), assets (p.assets),
          newButton (p.assets, "+ NEW SONG", KButton::Look::Enamel),
          cloudButton (p.assets, "CLOUD SYNC", KButton::Look::Enamel),
          closeButton (p.assets, utf8 ("\xc3\x97"), KButton::Look::Round),
          backButton (p.assets, utf8 ("\xe2\x80\xb9 MY SONGS"), KButton::Look::Enamel),
          turnOnButton (p.assets, "TURN ON SYNC", KButton::Look::Enamel),
          joinButton (p.assets, "JOIN", KButton::Look::Enamel),
          copyButton (p.assets, "COPY CODE", KButton::Look::Enamel),
          syncNowButton (p.assets, "SYNC NOW", KButton::Look::Enamel),
          turnOffButton (p.assets, utf8 ("TURN OFF\xe2\x80\xa6"), KButton::Look::Enamel)
    {
        setWantsKeyboardFocus (true);
        for (Component* c : { (Component*) &newButton, (Component*) &cloudButton, (Component*) &closeButton, (Component*) &backButton,
                              (Component*) &turnOnButton, (Component*) &joinButton, (Component*) &copyButton,
                              (Component*) &syncNowButton, (Component*) &turnOffButton, (Component*) &listViewport,
                              (Component*) &codeEditor })
            addChildComponent (c);
        closeButton.fontSize = 28.0f;
        for (auto* b : { &newButton, &cloudButton, &backButton, &turnOnButton, &joinButton, &copyButton, &syncNowButton, &turnOffButton })
            b->fontSize = 18.0f;

        listViewport.setViewedComponent (&listContent, false);
        listViewport.setScrollBarsShown (true, false);
        listViewport.setScrollBarThickness (8);
        if (isTouchUI()) listViewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::all);

        codeEditor.setTextToShowWhenEmpty ("the code from your other device", Palette::goldDim);
        codeEditor.setColour (TextEditor::backgroundColourId, Colour (0xff17110c));
        codeEditor.setColour (TextEditor::textColourId, Palette::cream);
        codeEditor.setColour (TextEditor::outlineColourId, Palette::goldDark);
        codeEditor.setColour (TextEditor::focusedOutlineColourId, Palette::gold);
        codeEditor.setColour (CaretComponent::caretColourId, Palette::goldLight);
        codeEditor.setFont (assets.bask (19.0f));
        codeEditor.setIndents (8, 8);
        codeEditor.setInputRestrictions (32, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz- ");
        codeEditor.onReturnKey = [this] { joinButton.triggerClick(); };

        closeButton.onClick = [this] { if (onClose) onClose(); };
        cloudButton.onClick = [this] { showCloud (true); };
        backButton.onClick = [this] { showCloud (false); };
        newButton.onClick = [this] { chooseNewSong(); };

        std::weak_ptr<bool> token = alive;
        auto finished = [this, token] (const String& success)
        {
            return [this, token, success] (bool ok, const String& message)
            {
                if (auto a = token.lock(); ! a || ! *a) return;
                setBusy (false, ok ? success : message);
            };
        };
        turnOnButton.onClick = [this, finished]
        {
            setBusy (true, utf8 ("Connecting\xe2\x80\xa6"));
            hub.getCloud().turnOnNew (finished ("Sync is on. Now type this code into Key Compass on your other devices."));
        };
        joinButton.onClick = [this, finished]
        {
            const auto typed = codeEditor.getText();
            if (kcsync::CloudSync::normaliseCode (typed).isEmpty())
            {
                setBusy (false, "That doesn't look like a sync code. It has 20 letters and numbers, like 7H3Q-M9XD-2PLA-W6RT-K4NE.");
                return;
            }
            setBusy (true, utf8 ("Connecting\xe2\x80\xa6"));
            hub.getCloud().join (typed, finished ("Joined. The songs from your other devices are on their way."));
        };
        copyButton.onClick = [this]
        {
            SystemClipboard::copyTextToClipboard (hub.getCloud().getCode());
            setBusy (false, "Copied.");
        };
        syncNowButton.onClick = [this] { hub.getCloud().syncNow(); };
        turnOffButton.onClick = [this, finished]
        {
            PopupMenu m;
            m.addItem (1, "Turn off on this device  (your songs stay here and in the cloud)");
            m.addItem (2, "Turn off and delete the songs in the cloud  (they stay on this device)");
            showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (turnOffButton.getScreenBounds())
                                                               .withStandardItemHeight (menuItemHeight()),
                      [this, finished, safe = SafePointer<MySongsView> (this)] (int r)
                      {
                          if (safe == nullptr || r == 0) return;
                          if (r == 1)
                          {
                              setBusy (true);
                              hub.getCloud().turnOff (false, finished ("Sync is off on this device."));
                              return;
                          }
                          AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::NoIcon)
                                                      .withTitle ("Delete the songs in the cloud?")
                                                      .withMessage ("Your songs stay on this device. Other devices keep the songs they already have, "
                                                                    "but stop syncing until sync is turned on again with a new code.")
                                                      .withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                                                  [this, finished, safe] (int choice)
                                                  {
                                                      if (safe == nullptr || choice != 1) return;
                                                      setBusy (true, utf8 ("Deleting\xe2\x80\xa6"));
                                                      hub.getCloud().turnOff (true, finished ("Sync is off, and the songs in the cloud are deleted."));
                                                  });
                      });
        };

        link.ensureSaved();                       // the open song is always in the list
        hub.getLibrary().addListener (this);
        hub.getCloud().addChangeListener (this);
        hub.getLibrary().rescan();
        hub.getCloud().syncNow();
        startTimer (30000);
        refresh();
    }

    MySongsView::~MySongsView()
    {
        *alive = false;
        hub.getCloud().removeChangeListener (this);
        hub.getLibrary().removeListener (this);
    }

    //==============================================================================
    Rectangle<float> MySongsView::panelArea() const
    {
        auto r = getLocalBounds().toFloat();
        if (roomy())                 // the PC, an upright tablet: a panel in the middle of the page
            return r.withSizeKeepingCentre (jmin (r.getWidth() - 60.0f, 640.0f), jmin (r.getHeight() - 60.0f, 1000.0f));
        return r.reduced (3.0f);
    }

    Rectangle<int> MySongsView::innerArea() const
    {
        // the framed panel's parchment (as the Song Mapper's panels)
        return panelArea().toNearestInt().withTrimmedLeft (27).withTrimmedRight (22).withTrimmedTop (44).withTrimmedBottom (27);
    }

    String MySongsView::describe (const kcsync::Entry& e) const
    {
        String s;
        if (e.id == link.currentId()) s << "Open now" << utf8 ("  \xc2\xb7  ");
        s << "changed " << ago (e.modified);
        if (e.device.isNotEmpty() && e.device != hub.getDevice()) s << " on your " << e.device;
        if (hub.getCloud().isOn() && e.dirty) s << utf8 ("  \xc2\xb7  not synced yet");
        return s;
    }

    void MySongsView::showCloud (bool cloud)
    {
        cloudPage = cloud;
        cloudMessage = {};
        refresh();
    }

    bool MySongsView::handleBack()
    {
        if (cloudPage) showCloud (false);
        else if (onClose) onClose();
        return true;
    }

    bool MySongsView::keyPressed (const KeyPress& k)
    {
        if (k == KeyPress::escapeKey) return handleBack();
        return false;
    }

    void MySongsView::mouseDown (const MouseEvent& e)
    {
        // a click outside the panel (the PC) closes it
        if (! panelArea().contains (e.position) && onClose) onClose();
    }

    void MySongsView::setBusy (bool b, const String& message)
    {
        busy = b;
        cloudMessage = message;
        refresh();
    }

    Component* MySongsView::findRow (const String& songName)
    {
        for (auto* r : rows)
            if (r->entry.name == songName) return r;
        return nullptr;
    }

    //==============================================================================
    void MySongsView::refresh()
    {
        const bool on = hub.getCloud().isOn();
        for (Component* c : { (Component*) &newButton, (Component*) &cloudButton, (Component*) &listViewport })
            c->setVisible (! cloudPage);
        closeButton.setVisible (true);
        backButton.setVisible (cloudPage);
        for (Component* c : { (Component*) &turnOnButton, (Component*) &codeEditor, (Component*) &joinButton })
            c->setVisible (cloudPage && ! on);
        for (Component* c : { (Component*) &copyButton, (Component*) &syncNowButton, (Component*) &turnOffButton })
            c->setVisible (cloudPage && on);
        for (auto* b : { &turnOnButton, &joinButton, &copyButton, &syncNowButton, &turnOffButton })
            b->setEnabled (! busy);
        codeEditor.setEnabled (! busy);
        cloudButton.lit = on;

        if (! cloudPage)
        {
            const auto position = listViewport.getViewPosition();
            rows.clear();
            for (const auto& e : hub.getLibrary().list())
                listContent.addAndMakeVisible (rows.add (new SongRow (*this, e)));
            layoutList();
            listViewport.setViewPosition (position);
        }
        else
            layoutCloud();
        repaint();
    }

    void MySongsView::resized()
    {
        if (cloudPage) layoutCloud();
        else           layoutList();
    }

    void MySongsView::layoutList()
    {
        auto r = innerArea().reduced (4, 2);
        auto top = r.removeFromTop (50);
        closeButton.setBounds (top.removeFromRight (46).withSizeKeepingCentre (44, 44));
        top.removeFromRight (8);
        const int half = (top.getWidth() - 8) / 2;
        newButton.setBounds (top.removeFromLeft (half).reduced (0, 3));
        top.removeFromLeft (8);
        cloudButton.setBounds (top.reduced (0, 3));
        r.removeFromTop (8);
        statusArea = r.removeFromBottom (58).withTrimmedBottom (12).reduced (24, 0);    // clear of the frame's corner ornaments
        listViewport.setBounds (r);
        const int w = r.getWidth() - listViewport.getScrollBarThickness() - 2;
        int y = 0;
        for (auto* row : rows)
        {
            row->setBounds (0, y, w, 64);
            y += 69;
        }
        listContent.setSize (w, jmax (y, 10));
    }

    void MySongsView::layoutCloud()
    {
        words.clear();
        auto r = innerArea().reduced (8, 2);
        auto top = r.removeFromTop (50);
        closeButton.setBounds (top.removeFromRight (46).withSizeKeepingCentre (44, 44));
        top.removeFromRight (8);
        backButton.setBounds (top.removeFromLeft (jmin (190, top.getWidth())).reduced (0, 3));
        r.removeFromTop (12);
        const int w = r.getWidth();
        auto say = [&] (const String& text, int style)
        {
            const auto font = style == 0 ? assets.cinzel (17.0f) : style == 1 ? assets.baskPlain (16.5f) : assets.baskItalic (15.5f);
            const int h = textHeight (text, font, w);
            words.push_back ({ r.removeFromTop (h), text, style });
            r.removeFromTop (style == 0 ? 8 : 12);
        };
        auto button = [&] (Component& c, int h = 50)
        {
            c.setBounds (r.removeFromTop (h));
            r.removeFromTop (12);
        };
        auto& cloud = hub.getCloud();
        codeBox = {};
        if (! cloud.isOn())
        {
            say ("KEEP YOUR SONGS THE SAME EVERYWHERE", 0);
            say ("Turn on cloud sync here, then type the code it gives you into Key Compass on your other devices: "
                 "your PC, phone or tablet. Your songs and starred progressions stay the same on all of them, "
                 "and you can keep working offline: it catches up when you're connected.", 1);
            button (turnOnButton);
            r.removeFromTop (6);
            say ("Turned it on already on another device? Type the code it shows:", 1);
            button (codeEditor, 48);
            button (joinButton);
        }
        else
        {
            say ("YOUR SYNC CODE", 0);
            codeBox = r.removeFromTop (64);
            r.removeFromTop (12);
            button (copyButton, 46);
            say ("Type this code into Key Compass on your other devices (My Songs, then Cloud Sync). "
                 "Keep it to yourself: anyone with the code can see and change your songs.", 1);
            say ("Status: " + cloud.describe(), 2);
            const int half = (w - 10) / 2;
            auto row = r.removeFromTop (50);
            syncNowButton.setBounds (row.removeFromLeft (half));
            row.removeFromLeft (10);
            turnOffButton.setBounds (row);
            r.removeFromTop (14);
        }
        if (cloudMessage.isNotEmpty())
            say (cloudMessage, 3);
    }

    //==============================================================================
    void MySongsView::paint (Graphics& g)
    {
        if (roomy()) g.fillAll (Colours::black.withAlpha (0.55f));
        else                  g.fillAll (Colour (0xff120d09));
        paintFramedPanel (g, assets, panelArea(), cloudPage ? "CLOUD SYNC" : "MY SONGS");

        if (cloudPage)
        {
            paintCloud (g);
            return;
        }
        if (rows.isEmpty())
        {
            g.setColour (Palette::inkSoft);
            g.setFont (assets.baskItalic (17.0f));
            g.drawFittedText ("No songs yet. Everything you make in the Song Mapper is kept here.",
                              listViewport.getBounds().reduced (12, 0).withHeight (90), Justification::centred, 3);
        }
        auto& cloud = hub.getCloud();
        g.setColour (Palette::inkSoft);
        g.setFont (assets.baskItalic (15.0f));
        g.drawFittedText ("Cloud sync: " + (cloud.isOn() ? cloud.describe()
                                                         : utf8 ("off \xe2\x80\x93 CLOUD SYNC puts these songs on your other devices")),
                          statusArea.reduced (4, 2), Justification::centred, 2, 0.8f);
    }

    void MySongsView::paintCloud (Graphics& g)
    {
        for (const auto& wd : words)
        {
            const auto font = wd.style == 0 ? assets.cinzel (17.0f) : wd.style == 1 ? assets.baskPlain (16.5f) : assets.baskItalic (15.5f);
            const auto colour = wd.style == 3 ? Colour (0xff8a2a14) : wd.style == 2 ? Palette::inkSoft : Palette::ink;
            AttributedString s;
            s.append (wd.text, font, colour);
            s.setJustification (Justification::topLeft);
            s.draw (g, wd.area.toFloat());
        }
        if (! codeBox.isEmpty())
        {
            const auto box = codeBox.toFloat();
            g.setColour (Colour (0xff17110c));
            g.fillRoundedRectangle (box, 8.0f);
            g.setColour (Palette::goldDark);
            g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.5f);
            g.setColour (Palette::goldLight);
            Font f = assets.cinzel (26.0f);
            const auto code = hub.getCloud().getCode();
            while (textWidth (f, code) > box.getWidth() - 20.0f && f.getHeight() > 12.0f) f = f.withHeight (f.getHeight() - 0.5f);
            g.setFont (f);
            g.drawText (code, box, Justification::centred, false);
        }
    }

    //==============================================================================
    void MySongsView::songMenu (const kcsync::Entry& e, Component& target)
    {
        const bool current = e.id == link.currentId();
        PopupMenu m;
        m.addItem (1, current ? "Open  (it's open now)" : "Open", ! current);
        m.addItem (2, utf8 ("Rename\xe2\x80\xa6"));
        m.addItem (3, "Duplicate");
        if (current) m.addItem (4, utf8 (isTouchUI() ? "Share the song file\xe2\x80\xa6" : "Save the song file\xe2\x80\xa6"));
        m.addSeparator();
        m.addItem (5, current ? String ("Delete  (open another song first)") : utf8 ("Delete\xe2\x80\xa6"), ! current);
        showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (target.getScreenBounds())
                                                           .withStandardItemHeight (menuItemHeight()),
                  [this, e, safe = SafePointer<MySongsView> (this)] (int r)
                  {
                      if (safe == nullptr || r == 0) return;
                      auto& lib = hub.getLibrary();
                      if (r == 1)
                      {
                          link.openSong (e.id);
                          if (onClose) onClose();
                      }
                      else if (r == 2) renameSong (e);
                      else if (r == 3) lib.duplicate (e.id, lib.uniqueName (e.name + " copy"), hub.getDevice());
                      else if (r == 4) { if (isTouchUI()) page.shareSongFile(); else page.saveSongFile(); }
                      else if (r == 5) deleteSong (e);
                  });
    }

    void MySongsView::renameSong (const kcsync::Entry& e)
    {
        auto* w = new AlertWindow ("Rename song", {}, MessageBoxIconType::NoIcon, this);
        w->setLookAndFeel (&getLookAndFeel());
        w->addTextEditor ("name", e.name, "Name:");
        w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([safe = SafePointer<MySongsView> (this), w, id = e.id] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            const auto name = w->getTextEditorContents ("name").trim().substring (0, 60);
            if (name.isNotEmpty()) safe->link.rename (id, name);
        }), true);
    }

    void MySongsView::deleteSong (const kcsync::Entry& e)
    {
        String message = "Delete \"" + e.name + "\"?";
        if (hub.getCloud().isOn()) message << "\n\nIt goes from My Songs on all your devices.";
        AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::NoIcon).withTitle ("Delete song")
                                    .withMessage (message).withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                                [safe = SafePointer<MySongsView> (this), id = e.id] (int r)
                                {
                                    if (r == 1 && safe != nullptr)
                                        safe->hub.getLibrary().remove (id, safe->hub.getDevice());
                                });
    }

    void MySongsView::chooseNewSong()
    {
        PopupMenu m;
        page.addTemplateItems (m);
        showMenu (m, getLookAndFeel(), PopupMenu::Options().withTargetScreenArea (newButton.getScreenBounds())
                                                           .withStandardItemHeight (menuItemHeight()),
                  [this, safe = SafePointer<MySongsView> (this)] (int r)
                  {
                      if (safe == nullptr || r == 0) return;
                      if (page.startNewSong (r) && onClose) onClose();
                  });
    }

    //==============================================================================
    void NoticeBanner::show (const String& text, Component& host, Rectangle<int> area, float scale)
    {
        message = text;
        host.addAndMakeVisible (this);
        setTransform (AffineTransform::scale (scale));
        const auto a = area.toFloat() / scale;
        const float w = jmin (a.getWidth() - 16.0f, 520.0f);
        const int h = textHeight (text, assets.bask (16.0f), (int) w - 28) + 24;
        setBounds (Rectangle<float> (a.getCentreX() - w / 2.0f, a.getY() + 8.0f, w, (float) jmax (48, h)).toNearestInt());
        toFront (false);
        startTimer (7000);
        repaint();
    }

    void NoticeBanner::paint (Graphics& g)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (ColourGradient (Palette::plateTop, 0.0f, r.getY(), Palette::plateBottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (Palette::gold);
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.6f);
        AttributedString s;
        s.append (message, assets.bask (16.0f), Palette::cream);
        s.setJustification (Justification::centred);
        s.draw (g, r.reduced (14.0f, 10.0f));
    }
}
