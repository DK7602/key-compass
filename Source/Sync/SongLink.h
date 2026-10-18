// Key Compass - the song open in an editor, tied to My Songs.
//
// SyncHub: one per program (shared by every plug-in window): the My Songs folder and cloud sync.
// SongLink: one per Key Compass (each processor): saves the open song into My Songs a moment after each
// change, opens other songs from there, and follows changes made to it on other devices.
#pragma once

#include "CloudSync.h"

namespace kcsync
{
    class SongLink;

    class SyncHub
    {
    public:
        SyncHub();
        ~SyncHub();
        SongLibrary& getLibrary() noexcept           { return *library; }
        CloudSync& getCloud() noexcept               { return *cloud; }
        const juce::String& getDevice() const noexcept { return device; }

        static juce::String serverUrl();
        // tests: a folder / server to use instead of the real ones (set before the first SyncHub is made)
        static juce::File& folderOverride();
        static CloudSync::Transport& transportOverride();
        static juce::String& deviceOverride();

        // the songs open right now, so two windows never write over each other's song
        void add (SongLink* l)                       { links.addIfNotAlreadyThere (l); }
        void remove (SongLink* l)                    { links.removeFirstMatchingValue (l); }
        bool isOpenElsewhere (const juce::String& id, const SongLink* except) const;
        juce::String yourDevice (const juce::String& d) const;      // "your phone", "another device"...

    private:
        juce::SharedResourcePointer<sm::FavouritesStore> favourites;
        juce::String device;
        std::unique_ptr<SongLibrary> library;
        std::unique_ptr<CloudSync> cloud;
        juce::Array<SongLink*> links;
        JUCE_DECLARE_NON_COPYABLE (SyncHub)
    };

    class SongLink : private juce::ChangeListener, private juce::Timer, private SongLibrary::Listener
    {
    public:
        explicit SongLink (sm::SongMap&);
        ~SongLink() override;

        SyncHub& getHub() noexcept                   { return *hub; }
        const juce::String& currentId() const noexcept { return song.songId; }

        // The host or the app has just put back a saved song. If My Songs has a newer version of it (changed
        // on another device since), either load that (askFirst = false: the phone app) or ask (onNewerVersion).
        void loadedFromState (bool askFirst);
        bool hasNewerVersion() const noexcept        { return newerFrom.isNotEmpty(); }
        juce::String newerDescription() const;       // "your phone"
        void loadNewer();                            // take the newer version (Undo brings this one back)
        void keepThis();                             // keep this one; the newer version stays in My Songs as a copy

        void flush();                                // save a change that is still waiting
        void openSong (const juce::String& id);      // the open song is saved first
        void newSong (const std::vector<std::pair<juce::String, int>>& layout);
        void importedSong();                         // a .kcsong file was just opened into the song
        void rename (const juce::String& id, const juce::String& newName);

        std::function<void (const juce::String&)> onNotice;     // a short message for the screen
        std::function<void()> onNewerVersion;                   // ask: newer version or this one?
        std::function<void()> onSongReplaced;                   // about to load another song (stop playback...)

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;
        void songChangedRemotely (const RemoteChange&) override;
        void saveNow();
        void loadFromLibrary (const Entry&, bool keepUndo);
        void notice (const juce::String& text) { if (onNotice) onNotice (text); }

        sm::SongMap& song;
        juce::SharedResourcePointer<SyncHub> hub;
        juce::String newerFrom;
        JUCE_DECLARE_NON_COPYABLE (SongLink)
    };
}
