#include "SongLink.h"
#include "SyncConfig.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace kcsync
{
    using namespace juce;

    //==============================================================================
    SyncHub::SyncHub()
    {
        device = deviceOverride();
        if (device.isEmpty())
        {
           #if KC_MOBILE
            // a phone or a tablet, by the size of the screen (in dp)
            device = "phone";
            if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
                if (jmin (d->logicalBounds.getWidth(), d->logicalBounds.getHeight()) >= 600)
                    device = "tablet";
           #else
            device = (SystemStats::getOperatingSystemType() & SystemStats::MacOSX) != 0 ? "Mac" : "PC";
           #endif
        }
        const auto folder = folderOverride() != File() ? folderOverride() : SongLibrary::defaultFolder();
        library = std::make_unique<SongLibrary> (folder);
        auto transport = transportOverride() ? transportOverride() : CloudSync::webTransport (serverUrl());
        cloud = std::make_unique<CloudSync> (*library, *favourites, transport,
                                             folder.getParentDirectory().getChildFile ("sync.json"), device);
    }

    SyncHub::~SyncHub()
    {
        cloud.reset();          // stop the sync thread before the library goes
        library.reset();
    }

    String SyncHub::serverUrl()                              { return KC_SYNC_URL; }
    File& SyncHub::folderOverride()                          { static File f; return f; }
    CloudSync::Transport& SyncHub::transportOverride()       { static CloudSync::Transport t; return t; }
    String& SyncHub::deviceOverride()                        { static String d; return d; }

    bool SyncHub::isOpenElsewhere (const String& id, const SongLink* except) const
    {
        for (auto* l : links)
            if (l != except && l->currentId() == id) return true;
        return false;
    }

    String SyncHub::yourDevice (const String& d) const
    {
        return d.isEmpty() ? String ("another device") : "your " + d;
    }

    //==============================================================================
    SongLink::SongLink (sm::SongMap& s) : song (s)
    {
        hub->add (this);
        hub->getLibrary().addListener (this);
        song.addChangeListener (this);
    }

    SongLink::~SongLink()
    {
        song.removeChangeListener (this);
        flush();
        hub->getLibrary().removeListener (this);
        hub->remove (this);
    }

    void SongLink::changeListenerCallback (ChangeBroadcaster*)
    {
        // a moment after the last change (not on every drag step); not while asking which version to keep
        if (newerFrom.isEmpty())
            startTimer (1200);
    }

    void SongLink::timerCallback()
    {
        saveNow();
    }

    void SongLink::flush()
    {
        if (isTimerRunning()) saveNow();
    }

    void SongLink::ensureSaved()
    {
        if (song.songId.isEmpty() || isTimerRunning() || ! hub->getLibrary().find (song.songId).isValid())
            saveNow();
    }

    void SongLink::saveNow()
    {
        stopTimer();
        if (newerFrom.isNotEmpty()) return;
        auto& lib = hub->getLibrary();
        if (song.songId.isEmpty() || hub->isOpenElsewhere (song.songId, this))
        {
            // a new song (or a copy of a song open in another window): a place of its own in My Songs
            song.songId = SongLibrary::newId();
            const auto unique = lib.uniqueName (song.songName, song.songId);
            if (unique != song.songName)
            {
                song.songName = unique;
                song.changed();
            }
        }
        const auto e = lib.save (song.songId, song.toValueTree(), hub->getDevice());
        if (e.isValid()) song.libraryStamp = e.modified;
    }

    void SongLink::loadFromLibrary (const Entry& e, bool keepUndo)
    {
        const auto content = hub->getLibrary().load (e.id);
        if (! content.isValid()) return;
        song.fromValueTree (SongLibrary::withLocalSettings (content, song.toValueTree()), keepUndo);
        song.songId = e.id;
        song.libraryStamp = e.modified;
        song.changed();
    }

    //==============================================================================
    void SongLink::loadedFromState (bool askFirst)
    {
        stopTimer();
        newerFrom = {};
        auto& lib = hub->getLibrary();
        if (song.songId.isEmpty() || hub->isOpenElsewhere (song.songId, this)) { saveNow(); return; }

        const auto e = lib.find (song.songId);
        if (! e.isValid() || e.deleted) { saveNow(); return; }          // not in My Songs (any more): put it back
        if (SongLibrary::sameSong (lib.load (song.songId), song.toValueTree()))
        {
            song.libraryStamp = e.modified;
            return;
        }
        if (e.modified > song.libraryStamp)
        {
            // changed somewhere else since this copy was saved
            if (! askFirst)
            {
                loadFromLibrary (e, true);
                notice ("Updated from " + hub->yourDevice (e.device));
                return;
            }
            newerFrom = e.device.isEmpty() ? String ("?") : e.device;
            if (onNewerVersion) onNewerVersion();
            return;
        }
        saveNow();                                                       // this copy is the newer one
    }

    String SongLink::newerDescription() const
    {
        if (newerFrom == "?") return "another device";
        if (newerFrom == hub->getDevice()) return "another project or window on this " + newerFrom;
        return hub->yourDevice (newerFrom);
    }

    void SongLink::loadNewer()
    {
        if (newerFrom.isEmpty()) return;
        newerFrom = {};
        const auto e = hub->getLibrary().find (song.songId);
        if (! e.isValid() || e.deleted) { saveNow(); return; }
        if (onSongReplaced) onSongReplaced();
        loadFromLibrary (e, true);
    }

    void SongLink::keepThis()
    {
        if (newerFrom.isEmpty()) return;
        auto& lib = hub->getLibrary();
        const auto e = lib.find (song.songId);
        const auto who = newerFrom == "?" ? String ("other device") : newerFrom;
        newerFrom = {};
        if (e.isValid() && ! e.deleted)
            lib.duplicate (song.songId, lib.uniqueName (e.name + " (from " + who + ")"), e.device);
        saveNow();
    }

    void SongLink::openSong (const String& id)
    {
        if (id == song.songId && newerFrom.isEmpty()) return;
        flush();
        const auto e = hub->getLibrary().find (id);
        if (! e.isValid() || e.deleted) return;
        newerFrom = {};
        if (onSongReplaced) onSongReplaced();
        loadFromLibrary (e, false);
    }

    void SongLink::newSong (const std::vector<std::pair<String, int>>& layout)
    {
        flush();
        newerFrom = {};
        if (onSongReplaced) onSongReplaced();
        sm::SongMap fresh;
        fresh.resetSections (layout);
        // the new song plays on the same instrument, the same way
        fresh.guitar = song.guitar;
        fresh.style = song.style;
        fresh.playStyle = song.playStyle;
        fresh.octave = song.octave;
        auto tree = SongLibrary::songOnly (fresh.toValueTree());
        tree.setProperty ("name", hub->getLibrary().uniqueName ("New song"), nullptr);
        song.fromValueTree (SongLibrary::withLocalSettings (tree, song.toValueTree()), false);
        song.songId = SongLibrary::newId();
        song.libraryStamp = 0;
        song.changed();
        saveNow();
    }

    void SongLink::importedSong()
    {
        newerFrom = {};
        song.libraryStamp = 0;
        saveNow();
    }

    void SongLink::rename (const String& id, const String& newName)
    {
        if (id == song.songId)
        {
            song.songName = newName;
            song.changed();
            saveNow();
        }
        else
            hub->getLibrary().rename (id, newName, hub->getDevice());
    }

    //==============================================================================
    void SongLink::songChangedRemotely (const RemoteChange& rc)
    {
        if (rc.id != song.songId || rc.id.isEmpty()) return;
        auto& lib = hub->getLibrary();
        using What = RemoteChange::What;

        if (rc.what == What::added || rc.what == What::updated)
        {
            const auto e = lib.find (rc.id);
            if (! e.isValid()) return;
            if (isTimerRunning())
            {
                // a change made here hasn't been saved yet: keep it as a copy rather than lose either version
                stopTimer();
                song.songId = SongLibrary::newId();
                song.songName = lib.uniqueName (song.songName + " (" + hub->getDevice() + " copy)");
                song.changed();
                saveNow();
                notice ("This song was also changed on " + hub->yourDevice (rc.device) + ". Your version is now \""
                        + song.songName + "\"; the other one is in My Songs.");
                return;
            }
            if (SongLibrary::sameSong (lib.load (rc.id), song.toValueTree()))
            {
                song.libraryStamp = e.modified;
                return;
            }
            if (newerFrom.isNotEmpty()) return;          // already asking about it
            if (onSongReplaced) onSongReplaced();
            loadFromLibrary (e, true);
            notice ("Updated from " + hub->yourDevice (rc.device) + ". Undo brings back what was here.");
        }
        else if (rc.what == What::deleted)
        {
            notice ("This song was deleted on " + hub->yourDevice (rc.device) + ". It stays open here; change anything to keep it.");
        }
        else if (rc.what == What::forked)
        {
            const auto e = lib.find (rc.newId);
            if (! e.isValid()) return;
            song.songId = rc.newId;
            song.songName = e.name;
            song.libraryStamp = e.modified;
            song.changed();
            notice ("This song was also changed on " + hub->yourDevice (rc.device) + ". Your version is now \""
                    + e.name + "\"; the other one is in My Songs.");
        }
    }
}
