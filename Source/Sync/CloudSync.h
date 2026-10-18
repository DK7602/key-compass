// Key Compass - cloud sync: keeps My Songs (and the starred progressions) the same on every device that
// has the same sync code. Works in the background; the songs always live on the device first, so it is
// fine to be offline - it catches up when the connection comes back.
//
// Each round: download what other devices changed since last time, then upload what changed here.
// When the same song was changed in two places before they could sync, nothing is lost: the version
// already in the cloud keeps the song's name and place, and this device's edits become a copy
// ("Verse idea (PC copy)").
#pragma once

#include "SongLibrary.h"
#include <atomic>
#include <deque>
#include <functional>
#include "../SongMapper/SongModel.h"

namespace kcsync
{
    class CloudSync : private juce::Thread, public juce::ChangeBroadcaster
    {
    public:
        // One request to the server: POST <op> with a JSON body. 'reached' = the server answered with JSON.
        struct Reply { bool reached = false; juce::var json; };
        using Transport = std::function<Reply (const juce::String& op, const juce::String& code, const juce::String& body)>;
        static Transport webTransport (const juce::String& baseUrl);     // the real thing (https)

        CloudSync (SongLibrary&, sm::FavouritesStore&, Transport, const juce::File& stateFile, const juce::String& device);
        ~CloudSync() override;

        enum class Status { off, working, upToDate, offline, badCode, problem };
        Status getStatus() const;
        juce::String describe() const;                 // "Up to date - synced 2 min ago", "Offline - will catch up"...
        bool isOn() const;
        juce::String getCode() const;                  // "7H3Q-M9XD-2PLA-W6RT-K4NE", empty when off
        const juce::String& getDevice() const noexcept { return device; }

        static juce::String newCode();
        static juce::String normaliseCode (const juce::String& typed);   // "" if it can't be a code
        static juce::String formatCode (const juce::String& code);

        // setting up (call on the message thread; 'done' comes back on the message thread)
        using Done = std::function<void (bool ok, const juce::String& message)>;
        void turnOnNew (Done done);                                       // a new code for this device
        void join (const juce::String& typedCode, Done done);             // the code from another device
        void turnOff (bool deleteCloudCopy, Done done);

        void syncNow();                    // after an edit, on opening My Songs, coming back to the app...
        void setForeground (bool);         // someone is looking: check for changes more often
        void setPaused (bool);             // the phone app is in the background
        void waitUntilIdleForTests (int timeoutMs);

    private:
        void run() override;
        void syncRound();
        bool pull();
        bool push();
        void applyItem (const juce::var& item);
        void applyFavourites (const juce::var& item);
        void keepBoth (const Entry& local, const juce::var& remote);
        Reply call (const juce::String& op, const juce::var& body);
        bool handleRefusal (const Reply&);
        void setStatus (Status, const juce::String& detail = {});
        void loadState();
        void saveState();
        void post (std::function<void()> task);

        SongLibrary& library;
        sm::FavouritesStore& favourites;
        Transport transport;
        juce::File stateFile;
        juce::String device;

        mutable juce::CriticalSection lock;
        juce::String code;                 // normalised, "" = off
        juce::int64 seq = 0;               // the last change downloaded
        int favouritesRev = 0;
        bool favouritesDirty = false;
        Status status = Status::off;
        juce::String detail;
        juce::int64 lastSynced = 0;
        std::deque<std::function<void()>> tasks;
        std::atomic<bool> foreground { true }, paused { false }, wanted { true }, busy { false };
        std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    };
}
