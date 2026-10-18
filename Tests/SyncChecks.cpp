// Cloud sync checks (Linux test build): two devices syncing through the sync server running on this machine
// (node cloud/test/local-server.mjs <port>). The "phone" is this program's own My Songs and open song
// (SyncHub / SongLink, as the app uses them); the "PC" is a second library with its own sync engine.
//   KeyCompassMobileShots <out> sync <port>
#include "../Source/PluginProcessor.h"
#include "../Source/Sync/SongLink.h"
#include <iostream>

#pragma GCC diagnostic ignored "-Wfloat-equal"     // the tempos compared are whole numbers

using namespace juce;
using namespace kcsync;

namespace synctest
{
    std::atomic<bool> online { true };

    // a bare-bones HTTP client for 127.0.0.1 (the test build has no web library)
    CloudSync::Transport localTransport (int port)
    {
        return [port] (const String& op, const String& code, const String& body) -> CloudSync::Reply
        {
            if (! online) return {};
            StreamingSocket s;
            if (! s.connect ("127.0.0.1", port, 3000)) return {};
            const auto bytes = body.toRawUTF8();
            const auto length = (int) body.getNumBytesAsUTF8();
            const String head = "POST /v1/" + op + " HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                                "Authorization: Bearer " + code + "\r\nContent-Length: " + String (length) + "\r\nConnection: close\r\n\r\n";
            s.write (head.toRawUTF8(), (int) head.getNumBytesAsUTF8());
            s.write (bytes, length);
            MemoryBlock reply;
            char buffer[8192];
            for (;;)
            {
                const int n = s.read (buffer, (int) sizeof (buffer), true);
                if (n <= 0) break;
                reply.append (buffer, (size_t) n);
            }
            const auto text = reply.toString();
            const auto json = JSON::parse (text.fromFirstOccurrenceOf ("\r\n\r\n", false, false));
            if (! json.isObject()) return {};
            return { true, json };
        };
    }

    // before the first Key Compass is made: My Songs in a test folder, the local server, called "phone"
    void prepare (int port, const File& root)
    {
        root.deleteRecursively();
        root.createDirectory();
        SyncHub::folderOverride() = root.getChildFile ("phone").getChildFile ("Songs");
        SyncHub::deviceOverride() = "phone";
        SyncHub::transportOverride() = port > 0 ? localTransport (port)
                                                : CloudSync::Transport ([] (const String&, const String&, const String&) { return CloudSync::Reply(); });
    }

    void pump (int ms)    { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    bool waitFor (std::function<bool()> condition, int timeoutMs = 15000)
    {
        const auto end = Time::getMillisecondCounter() + (uint32) timeoutMs;
        while (! condition())
        {
            if (Time::getMillisecondCounter() > end) return false;
            pump (20);
        }
        return true;
    }

    ValueTree songTree (const String& name, double bpm)
    {
        sm::SongMap m;
        m.resetSections ({ { "Verse", 8 }, { "Chorus", 8 } });
        m.songName = name;
        m.manualBpm = bpm;
        return m.toValueTree();
    }

    double bpmOf (const ValueTree& t)      { return (double) t.getProperty ("bpm", 0.0); }

    int run (KeyCompassProcessor& proc, int port, const File& root)
    {
        int failed = 0;
        auto check = [&] (const String& what, bool ok)
        {
            std::cout << (ok ? "  ok   " : "FAILED ") << what << std::endl;
            if (! ok) ++failed;
        };

        auto& link = proc.getSongLink();
        auto& hub = link.getHub();
        auto& phoneLib = hub.getLibrary();
        auto& phoneCloud = hub.getCloud();
        auto& song = proc.getSongMap();
        SharedResourcePointer<sm::FavouritesStore> phoneFavs;
        phoneFavs->replaceAll ({});
        String lastNotice;
        link.onNotice = [&] (const String& t) { lastNotice = t; };

        sm::FavouritesStore pcFavs (root.getChildFile ("pc").getChildFile ("favourites.json"));
        SongLibrary pcLib (root.getChildFile ("pc").getChildFile ("Songs"));
        CloudSync pcCloud (pcLib, pcFavs, localTransport (port), root.getChildFile ("pc").getChildFile ("sync.json"), "PC");

        auto settle = [&]
        {
            phoneCloud.waitUntilIdleForTests (15000);
            pcCloud.waitUntilIdleForTests (15000);
            pump (250);
        };
        auto syncBoth = [&] (bool pcFirst)
        {
            if (pcFirst) { pcCloud.syncNow(); pcCloud.waitUntilIdleForTests (15000); pump (150); }
            phoneCloud.syncNow(); phoneCloud.waitUntilIdleForTests (15000); pump (150);
            if (! pcFirst) { pcCloud.syncNow(); pcCloud.waitUntilIdleForTests (15000); }
            pump (250);
        };

        // ---- My Songs on one device ----
        song.songName = "Phone Song";
        song.manualBpm = 97.0;
        song.changed();
        pump (1800);
        const auto phoneSongId = song.songId;
        check ("the open song saves itself into My Songs", phoneSongId.isNotEmpty() && phoneLib.find (phoneSongId).name == "Phone Song"
                                                          && bpmOf (phoneLib.load (phoneSongId)) == 97.0);
        const auto stamp = phoneLib.find (phoneSongId).modified;
        song.auditionVolume = 0.21f;
        song.genre = 3;
        song.changed();
        pump (1800);
        check ("the volume and the genre shown aren't changes to the song", phoneLib.find (phoneSongId).modified == stamp);

        // ---- turning sync on, and a second device joining ----
        bool done = false, ok = false;
        String message;
        auto finished = [&] (bool o, const String& m) { done = true; ok = o; message = m; };
        check ("sync starts off", ! phoneCloud.isOn() && phoneCloud.getStatus() == CloudSync::Status::off);
        phoneCloud.turnOnNew (finished);
        waitFor ([&] { return done; });
        settle();
        check ("turn sync on (a new code)", ok && phoneCloud.isOn() && CloudSync::normaliseCode (phoneCloud.getCode()).length() == 20);
        check ("the phone is up to date", phoneCloud.getStatus() == CloudSync::Status::upToDate);
        check ("the phone's song went up", phoneLib.find (phoneSongId).rev > 0 && ! phoneLib.find (phoneSongId).dirty);

        done = false;
        pcCloud.join ("not a code", finished);
        check ("a mistyped code is refused straight away", done && ! ok && message.contains ("20 letters"));
        done = false;
        pcCloud.join ("2222-3333-4444-5555-6666", finished);
        waitFor ([&] { return done; });
        check ("a code nobody has is refused", ! ok && message.contains ("isn't right"));

        const auto pcSongId = SongLibrary::newId();
        pcLib.save (pcSongId, songTree ("PC Song", 120.0), "PC");
        pcFavs.add ({ "Four chords", "I V vi IV", "Pop" });
        done = false;
        pcCloud.join (phoneCloud.getCode().toLowerCase().replace ("-", " "), finished);
        waitFor ([&] { return done; });
        settle();
        check ("the PC joins with the code (typed in lower case, with spaces)", ok && pcCloud.isOn());
        check ("the PC gets the phone's song", SongLibrary::sameSong (pcLib.load (phoneSongId), phoneLib.load (phoneSongId)));
        syncBoth (false);
        check ("the phone gets the PC's song", phoneLib.find (pcSongId).isValid() && bpmOf (phoneLib.load (pcSongId)) == 120.0);
        check ("and the PC's starred progression", phoneFavs->contains ("I V vi IV"));
        check ("both have the same songs", phoneLib.list().size() == 2 && pcLib.list().size() == 2);

        // ---- a change on one device reaches the song open on the other ----
        auto t = pcLib.load (phoneSongId);
        t.setProperty ("bpm", 140.0, nullptr);
        pcLib.save (phoneSongId, t, "PC");
        syncBoth (true);
        check ("a change made on the PC reaches the song open on the phone", song.songId == phoneSongId && song.manualBpm == 140.0);
        check ("the phone says where it came from", lastNotice.contains ("your PC"));
        check ("the phone's own volume setting stays", std::abs (song.auditionVolume - 0.21f) < 0.001f);

        // ---- changed in both places while offline: nothing is lost ----
        online = false;
        song.manualBpm = 150.0;
        song.changed();
        pump (1800);
        phoneCloud.syncNow();
        phoneCloud.waitUntilIdleForTests (15000);
        pump (150);
        check ("offline: the status says so", phoneCloud.getStatus() == CloudSync::Status::offline);
        check ("offline: the change is kept, waiting", phoneLib.find (phoneSongId).dirty && bpmOf (phoneLib.load (phoneSongId)) == 150.0);
        t = pcLib.load (phoneSongId);
        t.setProperty ("bpm", 160.0, nullptr);
        pcLib.save (phoneSongId, t, "PC");
        online = true;
        lastNotice = {};
        syncBoth (true);
        check ("both changed: the version that reached the cloud first keeps the song", bpmOf (phoneLib.load (phoneSongId)) == 160.0);
        String copyId;
        for (const auto& e : phoneLib.list())
            if (e.name == "Phone Song (phone copy)") copyId = e.id;
        check ("and the phone's edits became a copy", copyId.isNotEmpty() && bpmOf (phoneLib.load (copyId)) == 150.0);
        check ("which is what stays open on the phone", song.songId == copyId && song.manualBpm == 150.0 && song.songName == "Phone Song (phone copy)");
        check ("the phone explains", lastNotice.contains ("also changed"));
        syncBoth (false);
        check ("the PC gets the copy too", pcLib.find (copyId).isValid() && pcLib.list().size() == 3);

        // ---- deleting ----
        pcLib.remove (pcSongId, "PC");
        syncBoth (true);
        check ("a song deleted on the PC goes from the phone", ! phoneLib.find (pcSongId).isValid());
        check ("and from the PC's folder once the cloud knows", ! pcLib.find (pcSongId).isValid());

        // ---- starred progressions changed on both while offline: merged ----
        online = false;
        phoneFavs->add ({ "Sensitive", "vi IV I V", "Pop" });
        pcFavs.add ({ "Lift", "IV I V vi", "Pop" });
        online = true;
        syncBoth (true);
        syncBoth (true);
        check ("stars added on both devices end up on both", phoneFavs->contains ("IV I V vi") && phoneFavs->contains ("vi IV I V")
                                                             && pcFavs.contains ("IV I V vi") && pcFavs.contains ("vi IV I V"));
        pcFavs.remove ("I V vi IV");
        syncBoth (true);
        check ("unstarring reaches the other device", ! phoneFavs->contains ("I V vi IV"));

        // ---- opening an older copy (the app restarted with an old save; a DAW project from last week) ----
        MemoryBlock oldState;
        proc.getStateInformation (oldState);
        const auto openId = song.songId;
        t = pcLib.load (openId);
        t.setProperty ("bpm", 171.0, nullptr);
        pcLib.save (openId, t, "PC");
        syncBoth (true);
        proc.setStateInformation (oldState.getData(), (int) oldState.getSize());
        pump (300);
        check ("the phone app opening an older copy takes the newer version", song.songId == openId && song.manualBpm == 171.0);

        song.fromValueTree (ValueTree::fromXml (*AudioProcessor::getXmlFromBinary (oldState.getData(), (int) oldState.getSize()))
                                .getChildWithName ("SongMap"));
        song.manualBpm = 150.0;
        link.loadedFromState (true);
        check ("in a DAW project it asks first", link.hasNewerVersion() && link.newerDescription() == "your PC");
        link.keepThis();
        pump (300);
        String fromPc;
        for (const auto& e : phoneLib.list())
            if (e.name.contains ("(from PC)")) fromPc = e.id;
        check ("keep this one: it stays, and the newer one is kept as a copy", ! link.hasNewerVersion() && song.manualBpm == 150.0
                                                                             && bpmOf (phoneLib.load (openId)) == 150.0
                                                                             && fromPc.isNotEmpty() && bpmOf (phoneLib.load (fromPc)) == 171.0);
        settle();

        // ---- My Songs actions ----
        const auto before = song.songId;
        link.newSong ({ { "Intro", 4 }, { "Verse", 8 } });
        pump (300);
        check ("a new song gets its own place in My Songs", song.songId != before && phoneLib.find (song.songId).name.startsWith ("New song")
                                                            && phoneLib.find (before).isValid());
        link.openSong (before);
        check ("open a song from My Songs", song.songId == before && song.manualBpm == 150.0);
        link.rename (before, "Renamed");
        check ("rename the open song", song.songName == "Renamed" && phoneLib.find (before).name == "Renamed");

        // ---- turning sync off ----
        done = false;
        pcCloud.turnOff (false, finished);
        waitFor ([&] { return done; });
        check ("turn sync off on the PC", ok && ! pcCloud.isOn() && pcLib.pending().size() == pcLib.list().size());
        const auto code = phoneCloud.getCode();
        done = false;
        phoneCloud.turnOff (true, finished);
        waitFor ([&] { return done; });
        const auto gone = localTransport (port) ("join", CloudSync::normaliseCode (code), "{}");
        check ("turn off and delete the cloud copy", ok && ! phoneCloud.isOn() && gone.reached && gone.json["error"].toString() == "unknown-code");

        link.onNotice = nullptr;
        std::cout << (failed == 0 ? "all sync checks passed" : String (failed) + " sync checks FAILED") << std::endl;
        return failed;
    }
}
