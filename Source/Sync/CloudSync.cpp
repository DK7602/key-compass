#include "CloudSync.h"
#include <random>

namespace kcsync
{
    using namespace juce;

    namespace
    {
        const char* const codeAlphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";    // no 0/O, 1/I: easy to read and type

        Entry entryFrom (const var& item)
        {
            Entry e;
            e.id = item["id"].toString();
            e.name = item["name"].toString();
            e.device = item["device"].toString();
            e.modified = (int64) item["modified"];
            e.rev = (int) item["rev"];
            e.deleted = (bool) item["deleted"];
            return e;
        }

        var object (std::initializer_list<std::pair<const char*, var>> props)
        {
            auto* o = new DynamicObject();
            for (const auto& [name, value] : props)
                o->setProperty (name, value);
            return var (o);
        }
    }

    //==============================================================================
    CloudSync::Transport CloudSync::webTransport (const String& baseUrl)
    {
        return [baseUrl] (const String& op, const String& code, const String& body) -> Reply
        {
            int status = 0;
            const auto url = URL (baseUrl + "/v1/" + op).withPOSTData (body);
            const auto options = URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                                     .withExtraHeaders ("Content-Type: application/json\r\nAuthorization: Bearer " + code)
                                     .withConnectionTimeoutMs (8000)
                                     .withStatusCode (&status)
                                     .withNumRedirectsToFollow (0);
            const std::unique_ptr<InputStream> in (url.createInputStream (options));
            if (in == nullptr) return {};
            const auto json = JSON::parse (in->readEntireStreamAsString());
            if (! json.isObject()) return {};          // e.g. a Wi-Fi sign-in page: not the server
            return { true, json };
        };
    }

    CloudSync::CloudSync (SongLibrary& lib, sm::FavouritesStore& favs, Transport t, const File& state, const String& deviceName)
        : Thread ("Key Compass cloud sync"), library (lib), favourites (favs), transport (std::move (t)),
          stateFile (state), device (deviceName)
    {
        loadState();
        status = code.isNotEmpty() ? Status::working : Status::off;
        library.onLocalChange = [this] { syncNow(); };
        favourites.onUserChange = [this]
        {
            {
                const ScopedLock sl (lock);
                favouritesDirty = true;
            }
            saveState();
            syncNow();
        };
        startThread (Thread::Priority::low);
    }

    CloudSync::~CloudSync()
    {
        *alive = false;
        library.onLocalChange = nullptr;
        favourites.onUserChange = nullptr;
        signalThreadShouldExit();
        notify();
        stopThread (10000);         // a request already on its way gives up after 8 seconds
    }

    //==============================================================================
    String CloudSync::newCode()
    {
        std::random_device rd;      // the system's secure random numbers
        String c;
        for (int i = 0; i < 20; ++i)
            c << codeAlphabet[rd() % 32];
        return c;
    }

    String CloudSync::normaliseCode (const String& typed)
    {
        String c;
        for (auto ch : typed.toUpperCase())
            if (CharacterFunctions::isLetterOrDigit (ch)) c << ch;
        if (c.length() != 20) return {};
        for (auto ch : c)
            if (String (codeAlphabet).indexOfChar (ch) < 0) return {};
        return c;
    }

    String CloudSync::formatCode (const String& c)
    {
        String out;
        for (int i = 0; i < c.length(); i += 4)
            out << (i > 0 ? "-" : "") << c.substring (i, i + 4);
        return out;
    }

    bool CloudSync::isOn() const
    {
        const ScopedLock sl (lock);
        return code.isNotEmpty();
    }

    String CloudSync::getCode() const
    {
        const ScopedLock sl (lock);
        return formatCode (code);
    }

    CloudSync::Status CloudSync::getStatus() const
    {
        const ScopedLock sl (lock);
        return status;
    }

    String CloudSync::describe() const
    {
        const ScopedLock sl (lock);
        switch (status)
        {
            case Status::off:      return "Off";
            case Status::working:  return String (CharPointer_UTF8 ("Syncing\xe2\x80\xa6"));
            case Status::upToDate: return "Up to date";
            case Status::offline:  return String (CharPointer_UTF8 ("Offline \xe2\x80\x93 it catches up when you're connected"));
            case Status::badCode:
            case Status::problem:  return detail;
        }
        return {};
    }

    void CloudSync::setStatus (Status s, const String& d)
    {
        {
            const ScopedLock sl (lock);
            if (status == s && detail == d) return;
            status = s;
            detail = d;
        }
        sendChangeMessage();
    }

    //==============================================================================
    void CloudSync::syncNow()
    {
        wanted = true;
        notify();
    }

    void CloudSync::setForeground (bool f)
    {
        if (foreground.exchange (f) != f && f) syncNow();
    }

    void CloudSync::setPaused (bool p)
    {
        if (paused.exchange (p) != p && ! p) syncNow();
    }

    void CloudSync::post (std::function<void()> task)
    {
        {
            const ScopedLock sl (lock);
            tasks.push_back (std::move (task));
        }
        notify();
    }

    void CloudSync::waitUntilIdleForTests (int timeoutMs)
    {
        const auto end = Time::getMillisecondCounter() + (uint32) timeoutMs;
        for (;;)
        {
            bool idle;
            {
                const ScopedLock sl (lock);
                idle = tasks.empty() && ! busy && ! (wanted && code.isNotEmpty());
            }
            if (idle || Time::getMillisecondCounter() > end) return;
            Thread::sleep (20);
        }
    }

    void CloudSync::run()
    {
        while (! threadShouldExit())
        {
            std::deque<std::function<void()>> todo;
            {
                const ScopedLock sl (lock);
                todo.swap (tasks);
            }
            busy = ! todo.empty();
            for (auto& task : todo)
                if (! threadShouldExit()) task();

            // in the background (paused) there are no regular checks, but a change made just before still goes up
            if (isOn() && wanted.exchange (false))
            {
                busy = true;
                syncRound();
            }
            busy = false;

            bool more;
            {
                const ScopedLock sl (lock);
                more = ! tasks.empty();
            }
            if (more || threadShouldExit()) continue;
            const int interval = paused ? 3600000 : (foreground ? 30000 : 120000);
            if (! wait (interval) && ! paused)
                wanted = true;          // time for a regular check
        }
    }

    //==============================================================================
    CloudSync::Reply CloudSync::call (const String& op, const var& body)
    {
        String c;
        {
            const ScopedLock sl (lock);
            c = code;
        }
        if (c.isEmpty()) return {};
        return transport (op, c, JSON::toString (body, true));
    }

    bool CloudSync::handleRefusal (const Reply& r)
    {
        const auto error = r.json["error"].toString();
        if (error == "unknown-code" || error == "bad-code")
            setStatus (Status::badCode, "The cloud doesn't know this sync code any more. Turn sync off, then on again.");
        else
            setStatus (Status::problem, "The cloud said: " + error);
        return false;
    }

    void CloudSync::syncRound()
    {
        if (getStatus() != Status::upToDate) setStatus (Status::working);
        library.rescan();
        if (! pull()) return;
        if (! push()) return;
        {
            const ScopedLock sl (lock);
            lastSynced = Time::currentTimeMillis();
        }
        setStatus (Status::upToDate);
    }

    bool CloudSync::pull()
    {
        for (int page = 0; page < 200 && ! threadShouldExit(); ++page)
        {
            int64 since;
            {
                const ScopedLock sl (lock);
                since = seq;
            }
            const auto r = call ("changes", object ({ { "since", since } }));
            if (! r.reached) { setStatus (Status::offline); return false; }
            if (! (bool) r.json["ok"]) return handleRefusal (r);
            if (auto* items = r.json["items"].getArray())
                for (const auto& item : *items)
                    applyItem (item);
            {
                const ScopedLock sl (lock);
                seq = jmax (seq, (int64) r.json["seq"]);
            }
            saveState();
            if (! (bool) r.json["more"]) break;
        }
        return true;
    }

    void CloudSync::applyItem (const var& item)
    {
        const auto kind = item["kind"].toString();
        if (kind == "favourites") { applyFavourites (item); return; }
        if (kind != "song") return;

        const auto remote = entryFrom (item);
        const auto data = item["data"].toString();
        const auto local = library.find (remote.id);
        using What = RemoteChange::What;

        if (! local.isValid())
        {
            if (! remote.deleted) library.applyRemote (remote, data, What::added);
            return;
        }
        if (remote.rev <= local.rev) return;                       // we have this one (often our own upload)
        if (! local.dirty)
        {
            library.applyRemote (remote, data, local.deleted ? What::added : What::updated);
            return;
        }
        // changed here and somewhere else
        if (remote.deleted)  { library.setRev (remote.id, remote.rev); return; }   // our edits bring it back
        if (local.deleted)   { library.applyRemote (remote, data, What::added); return; }   // their edits beat our delete
        if (SongLibrary::sameSong (library.load (remote.id), SongLibrary::parseSong (data)))
        {
            library.uploaded (remote.id, remote.rev, local.modified);                 // the same change both ways
            return;
        }
        keepBoth (local, item);
    }

    void CloudSync::keepBoth (const Entry& local, const var& item)
    {
        // the cloud's version keeps the song's place; what was changed here becomes a copy (and gets uploaded)
        const auto remote = entryFrom (item);
        const auto copyName = library.uniqueName (local.name + " (" + device + " copy)");
        const auto copyId = library.duplicate (local.id, copyName, device);
        library.applyRemote (remote, item["data"].toString(), RemoteChange::What::updated, false);
        library.notifyRemote ({ RemoteChange::What::forked, local.id, copyId, remote.device });
    }

    void CloudSync::applyFavourites (const var& item)
    {
        const int rev = (int) item["rev"];
        bool dirty;
        {
            const ScopedLock sl (lock);
            if (rev <= favouritesRev) return;
            dirty = favouritesDirty;
        }
        const auto theirs = sm::FavouritesStore::parse (item["data"].toString());
        favourites.replaceAll (dirty ? sm::FavouritesStore::merge (favourites.all(), theirs) : theirs);
        {
            const ScopedLock sl (lock);
            favouritesRev = rev;
        }
        saveState();
    }

    bool CloudSync::push()
    {
        bool problem = false;
        for (int round = 0; round < 4 && ! threadShouldExit(); ++round)
        {
            bool again = false;
            for (const auto& e : library.pending())
            {
                if (threadShouldExit()) return false;
                const auto data = e.deleted ? String() : library.dataFor (e.id);
                if (! e.deleted && data.isEmpty()) continue;
                const auto r = call ("put", object ({ { "id", e.id }, { "kind", "song" }, { "baseRev", e.rev },
                                                      { "name", e.name }, { "modified", e.modified }, { "device", e.device },
                                                      { "deleted", e.deleted }, { "data", data } }));
                if (! r.reached) { setStatus (Status::offline); return false; }
                if ((bool) r.json["ok"])
                {
                    library.uploaded (e.id, (int) r.json["rev"], e.modified);
                    continue;
                }
                const auto error = r.json["error"].toString();
                if (error == "conflict")
                {
                    const auto item = r.json["item"];
                    again = true;
                    if (! item.isObject())                          // the cloud lost it: send it as new
                    {
                        library.setRev (e.id, 0);
                        continue;
                    }
                    const auto remote = entryFrom (item);
                    if (e.deleted)
                    {
                        if (remote.deleted) library.uploaded (e.id, remote.rev, e.modified);
                        else library.applyRemote (remote, item["data"].toString(), RemoteChange::What::added);
                    }
                    else if (remote.deleted)
                        library.setRev (e.id, remote.rev);
                    else if (SongLibrary::sameSong (library.load (e.id), SongLibrary::parseSong (item["data"].toString())))
                        library.uploaded (e.id, remote.rev, e.modified);
                    else
                        keepBoth (e, item);
                    continue;
                }
                if (error == "unknown-code" || error == "bad-code") return handleRefusal (r);
                problem = true;
                setStatus (Status::problem, error == "too-big"  ? "\"" + e.name + "\" is too big to sync"
                                          : error == "too-many" ? String ("There are too many songs to sync")
                                                                : "The cloud refused \"" + e.name + "\" (" + error + ")");
            }

            bool favDirty;
            int favRev;
            {
                const ScopedLock sl (lock);
                favDirty = favouritesDirty;
                favRev = favouritesRev;
            }
            if (favDirty)
            {
                const auto json = favourites.toJson();
                const auto r = call ("put", object ({ { "id", "favourites" }, { "kind", "favourites" }, { "baseRev", favRev },
                                                      { "name", "" }, { "modified", Time::currentTimeMillis() },
                                                      { "device", device }, { "deleted", false }, { "data", json } }));
                if (! r.reached) { setStatus (Status::offline); return false; }
                if ((bool) r.json["ok"])
                {
                    const ScopedLock sl (lock);
                    favouritesRev = (int) r.json["rev"];
                    if (favourites.toJson() == json) favouritesDirty = false;      // else starred again meanwhile
                }
                else if (r.json["error"].toString() == "conflict" && r.json["item"].isObject())
                {
                    const auto item = r.json["item"];
                    favourites.replaceAll (sm::FavouritesStore::merge (favourites.all(),
                                                                       sm::FavouritesStore::parse (item["data"].toString())));
                    const ScopedLock sl (lock);
                    favouritesRev = (int) item["rev"];
                    again = true;
                }
                else return handleRefusal (r);
                saveState();
            }
            if (! again) break;
        }
        return ! problem;
    }

    //==============================================================================
    void CloudSync::turnOnNew (Done done)
    {
        std::weak_ptr<bool> token = alive;
        post ([this, done, token]
        {
            const auto c = newCode();
            const auto r = transport ("create", c, "{}");
            const bool ok = r.reached && (bool) r.json["ok"];
            if (ok)
            {
                {
                    const ScopedLock sl (lock);
                    code = c;
                    seq = 0;
                    favouritesRev = 0;
                    favouritesDirty = ! favourites.all().empty();
                }
                library.forgetCloud();
                saveState();
                wanted = true;
            }
            const String message = ok ? String() : r.reached ? "The cloud said: " + r.json["error"].toString()
                                                             : String ("Couldn't reach the cloud. Check the internet connection and try again.");
            if (ok) setStatus (Status::working);
            MessageManager::callAsync ([token, done, ok, message] { if (auto a = token.lock(); a && *a && done) done (ok, message); });
        });
    }

    void CloudSync::join (const String& typed, Done done)
    {
        const auto c = normaliseCode (typed);
        if (c.isEmpty())
        {
            if (done) done (false, "That doesn't look like a sync code. It has 20 letters and numbers, like 7H3Q-M9XD-2PLA-W6RT-K4NE.");
            return;
        }
        std::weak_ptr<bool> token = alive;
        post ([this, c, done, token]
        {
            const auto r = transport ("join", c, "{}");
            const bool ok = r.reached && (bool) r.json["ok"];
            if (ok)
            {
                {
                    const ScopedLock sl (lock);
                    code = c;
                    seq = 0;
                    favouritesRev = 0;
                    favouritesDirty = ! favourites.all().empty();
                }
                library.forgetCloud();
                saveState();
                wanted = true;
                setStatus (Status::working);
            }
            String message;
            if (! r.reached)                                 message = "Couldn't reach the cloud. Check the internet connection and try again.";
            else if (r.json["error"].toString() == "unknown-code") message = "That code isn't right. Check it against the one shown on your other device.";
            else if (! ok)                                   message = "The cloud said: " + r.json["error"].toString();
            MessageManager::callAsync ([token, done, ok, message] { if (auto a = token.lock(); a && *a && done) done (ok, message); });
        });
    }

    void CloudSync::turnOff (bool deleteCloudCopy, Done done)
    {
        std::weak_ptr<bool> token = alive;
        post ([this, deleteCloudCopy, done, token]
        {
            if (deleteCloudCopy)
            {
                const auto r = call ("forget", object ({}));
                if (! r.reached || ! (bool) r.json["ok"])
                {
                    MessageManager::callAsync ([token, done]
                    {
                        if (auto a = token.lock(); a && *a && done)
                            done (false, "Couldn't reach the cloud to delete the songs there. Try again when you're online.");
                    });
                    return;
                }
            }
            {
                const ScopedLock sl (lock);
                code = {};
                seq = 0;
                favouritesRev = 0;
                favouritesDirty = false;
            }
            library.forgetCloud();
            saveState();
            setStatus (Status::off);
            MessageManager::callAsync ([token, done] { if (auto a = token.lock(); a && *a && done) done (true, {}); });
        });
    }

    //==============================================================================
    void CloudSync::loadState()
    {
        const auto json = JSON::parse (stateFile);
        const ScopedLock sl (lock);
        code = normaliseCode (json["code"].toString());
        seq = (int64) json["seq"];
        favouritesRev = (int) json["favouritesRev"];
        favouritesDirty = (bool) json["favouritesDirty"];
    }

    void CloudSync::saveState()
    {
        const ScopedLock sl (lock);
        auto state = object ({ { "code", code }, { "seq", seq }, { "favouritesRev", favouritesRev },
                               { "favouritesDirty", favouritesDirty } });
        stateFile.getParentDirectory().createDirectory();
        const auto tmp = stateFile.getSiblingFile (stateFile.getFileName() + ".tmp");
        if (tmp.replaceWithText (JSON::toString (state)))
            tmp.moveFileTo (stateFile);
    }
}
