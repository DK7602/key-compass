// Key Compass - My Songs: every song you work on, kept in one folder shared by every Key Compass on this
// device (each plug-in window and the app). The song open in an editor saves itself here as you work
// (see SongLink), and cloud sync (CloudSync) keeps the folder the same on all your devices.
//
// One file per song: <id>.kcsong, holding the song plus what sync needs to know about it:
//   <KeyCompassSong version="1.5.0" id="..." name="..." modified="ms" rev="3" dirty="1" deleted="0" device="PC">
//     <SongMap .../>
//   </KeyCompassSong>
// 'rev' is the cloud's version number this copy is based on (0 = never uploaded); 'dirty' = changed here
// since. A deleted song stays as a small file with deleted="1" until the cloud has heard about it.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace kcsync
{
    struct Entry
    {
        juce::String id, name, device;
        juce::int64 modified = 0;          // when the song last changed (ms since 1970)
        int rev = 0;
        bool dirty = false, deleted = false;
        bool isValid() const noexcept      { return id.isNotEmpty(); }
    };

    // Something another device did to a song (told to SongLibrary::Listener on the message thread)
    struct RemoteChange
    {
        enum class What { added, updated, deleted, forked };
        What what = What::updated;
        juce::String id;
        juce::String newId;                // forked: this device's unsent edits were moved to this new song
        juce::String device;               // the other device ("phone", "PC"...)
    };

    class SongLibrary
    {
    public:
        explicit SongLibrary (const juce::File& folder);
        ~SongLibrary();
        static juce::File defaultFolder();
        const juce::File& getFolder() const noexcept   { return folder; }

        std::vector<Entry> list() const;                        // the songs (not deleted), newest first
        Entry find (const juce::String& id) const;              // invalid if unknown; deleted ones have deleted = true
        juce::ValueTree load (const juce::String& id) const;    // the song: a "SongMap" tree (invalid if none)
        juce::String dataFor (const juce::String& id) const;    // the song as XML text, as it goes to the cloud
        std::vector<Entry> pending() const;                     // changed here, not yet in the cloud (deletes too)

        // ---- changes made on this device ----
        Entry save (const juce::String& id, const juce::ValueTree& song, const juce::String& device);
        void rename (const juce::String& id, const juce::String& newName, const juce::String& device);
        juce::String duplicate (const juce::String& id, const juce::String& newName, const juce::String& device);
        void remove (const juce::String& id, const juce::String& device);

        // ---- changes from the cloud ----
        void applyRemote (const Entry& e, const juce::String& songXml, RemoteChange::What what, bool notify = true);
        void uploaded (const juce::String& id, int rev, juce::int64 modifiedWhenSent);
        void setRev (const juce::String& id, int rev);           // a clash was settled: our copy now follows this rev
        void notifyRemote (const RemoteChange&);
        void forgetCloud();                                      // sync turned off: nothing counts as uploaded any more

        void rescan();                                           // pick up files another program changed (desktop)

        // ---- songs as trees ----
        // The song without the settings that belong to one device (volume, mutes, the genre being browsed,
        // the selected section...), so turning the volume down on the phone doesn't change the song on the PC.
        static juce::ValueTree songOnly (const juce::ValueTree& songMapTree);
        static juce::ValueTree withLocalSettings (const juce::ValueTree& song, const juce::ValueTree& current);
        static bool sameSong (const juce::ValueTree& a, const juce::ValueTree& b);
        static juce::ValueTree parseSong (const juce::String& xml);   // a SongMap tree from cloud data or a .kcsong file
        static juce::String newId();
        juce::String uniqueName (const juce::String& wanted, const juce::String& exceptId = {}) const;

        struct Listener
        {
            virtual ~Listener() = default;
            virtual void libraryChanged() {}
            virtual void songChangedRemotely (const RemoteChange&) {}
        };
        void addListener (Listener* l)       { listeners.add (l); }
        void removeListener (Listener* l)    { listeners.remove (l); }
        std::function<void()> onLocalChange;    // set by the sync engine: there is something to upload

    private:
        juce::File fileFor (const juce::String& id) const;
        bool readFile (const juce::File&, Entry&, juce::ValueTree* song) const;
        bool write (const Entry&, const juce::ValueTree& song);
        void changedLocally();
        void announce();

        juce::File folder;
        mutable juce::CriticalSection lock;
        std::map<juce::String, Entry> entries;
        std::map<juce::String, juce::Time> fileTimes;
        juce::ListenerList<Listener> listeners;
        std::shared_ptr<bool> alive = std::make_shared<bool> (true);

        JUCE_DECLARE_NON_COPYABLE (SongLibrary)
    };
}
