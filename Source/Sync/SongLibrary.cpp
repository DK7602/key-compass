#include "SongLibrary.h"
#include "../Version.h"

namespace kcsync
{
    using namespace juce;

    namespace
    {
        // settings that belong to one device, not to the song (see SongMap::toValueTree)
        const char* const localSettings[] = { "genre", "genreStyle", "favs", "volume", "loop", "showLines", "mutes",
                                              "selectedSection", "followHostTempo", "songId", "libStamp" };

        bool sameTree (const ValueTree& a, const ValueTree& b)
        {
            if (a.getType() != b.getType() || a.getNumProperties() != b.getNumProperties()
                || a.getNumChildren() != b.getNumChildren())
                return false;
            for (int i = 0; i < a.getNumProperties(); ++i)
            {
                const auto name = a.getPropertyName (i);
                if (! b.hasProperty (name) || a[name].toString() != b[name].toString())
                    return false;
            }
            for (int i = 0; i < a.getNumChildren(); ++i)
                if (! sameTree (a.getChild (i), b.getChild (i)))
                    return false;
            return true;
        }

        // through XML and back, so numbers and text compare the same way wherever the tree came from
        ValueTree normalised (const ValueTree& t)
        {
            if (auto xml = t.createXml())
                return ValueTree::fromXml (*xml);
            return {};
        }
    }

    SongLibrary::SongLibrary (const File& f) : folder (f)
    {
        folder.createDirectory();
        rescan();
    }

    SongLibrary::~SongLibrary()
    {
        *alive = false;
    }

    File SongLibrary::defaultFolder()
    {
        return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("KeyCompass").getChildFile ("Songs");
    }

    File SongLibrary::fileFor (const String& id) const
    {
        return folder.getChildFile (File::createLegalFileName (id) + ".kcsong");
    }

    //==============================================================================
    bool SongLibrary::readFile (const File& file, Entry& e, ValueTree* song) const
    {
        const auto xml = parseXML (file);
        if (xml == nullptr || ! xml->hasTagName ("KeyCompassSong")) return false;
        e.id = xml->getStringAttribute ("id");
        if (e.id.isEmpty()) return false;
        e.name = xml->getStringAttribute ("name");
        e.device = xml->getStringAttribute ("device");
        e.modified = xml->getStringAttribute ("modified").getLargeIntValue();
        e.rev = xml->getIntAttribute ("rev");
        e.dirty = xml->getBoolAttribute ("dirty");
        e.deleted = xml->getBoolAttribute ("deleted");
        if (song != nullptr)
            if (auto* s = xml->getChildByName ("SongMap"))
                *song = ValueTree::fromXml (*s);
        return true;
    }

    bool SongLibrary::write (const Entry& e, const ValueTree& song)
    {
        XmlElement root ("KeyCompassSong");
        root.setAttribute ("version", KC_VERSION_STRING);
        root.setAttribute ("id", e.id);
        root.setAttribute ("name", e.name);
        root.setAttribute ("modified", String (e.modified));
        root.setAttribute ("rev", e.rev);
        root.setAttribute ("dirty", e.dirty ? 1 : 0);
        root.setAttribute ("deleted", e.deleted ? 1 : 0);
        root.setAttribute ("device", e.device);
        if (! e.deleted && song.isValid())
            if (auto xml = song.createXml())
                root.addChildElement (xml.release());

        const auto file = fileFor (e.id);
        const auto tmp = file.getSiblingFile (file.getFileNameWithoutExtension() + ".tmp");
        if (! root.writeTo (tmp) || ! tmp.moveFileTo (file))
        {
            tmp.deleteFile();
            return false;
        }
        fileTimes[e.id] = file.getLastModificationTime();
        return true;
    }

    void SongLibrary::rescan()
    {
        bool changed = false;
        {
            const ScopedLock sl (lock);
            std::set<String> seen;
            for (const auto& entry : RangedDirectoryIterator (folder, false, "*.kcsong", File::findFiles))
            {
                const auto file = entry.getFile();
                const auto id = file.getFileNameWithoutExtension();
                seen.insert (id);
                const auto time = file.getLastModificationTime();
                auto t = fileTimes.find (id);
                if (t != fileTimes.end() && t->second == time && entries.count (id) > 0) continue;
                Entry e;
                if (readFile (file, e, nullptr) && e.id == id)
                {
                    entries[id] = e;
                    fileTimes[id] = time;
                    changed = true;
                }
            }
            for (auto it = entries.begin(); it != entries.end();)
            {
                if (seen.count (it->first) == 0)
                {
                    fileTimes.erase (it->first);
                    it = entries.erase (it);
                    changed = true;
                }
                else ++it;
            }
        }
        if (changed) announce();
    }

    //==============================================================================
    std::vector<Entry> SongLibrary::list() const
    {
        std::vector<Entry> out;
        {
            const ScopedLock sl (lock);
            for (const auto& [id, e] : entries)
                if (! e.deleted) out.push_back (e);
        }
        std::sort (out.begin(), out.end(), [] (const Entry& a, const Entry& b) { return a.modified > b.modified; });
        return out;
    }

    Entry SongLibrary::find (const String& id) const
    {
        const ScopedLock sl (lock);
        auto it = entries.find (id);
        return it != entries.end() ? it->second : Entry();
    }

    ValueTree SongLibrary::load (const String& id) const
    {
        const ScopedLock sl (lock);
        Entry e;
        ValueTree song;
        if (id.isNotEmpty() && readFile (fileFor (id), e, &song) && ! e.deleted)
            return song;
        return {};
    }

    String SongLibrary::dataFor (const String& id) const
    {
        const auto song = load (id);
        if (auto xml = song.isValid() ? song.createXml() : nullptr)
            return xml->toString (XmlElement::TextFormat().withoutHeader().singleLine());
        return {};
    }

    std::vector<Entry> SongLibrary::pending() const
    {
        std::vector<Entry> out;
        const ScopedLock sl (lock);
        for (const auto& [id, e] : entries)
            if (e.dirty) out.push_back (e);
        return out;
    }

    //==============================================================================
    Entry SongLibrary::save (const String& id, const ValueTree& songTree, const String& device)
    {
        const auto song = normalised (songOnly (songTree));
        Entry e;
        {
            const ScopedLock sl (lock);
            auto it = entries.find (id);
            if (it != entries.end())
            {
                e = it->second;
                if (! e.deleted && e.name == song["name"].toString() && sameSong (load (id), song))
                    return e;                                  // nothing new
            }
            const auto now = Time::currentTimeMillis();
            e.id = id;
            e.name = song["name"].toString();
            e.device = device;
            e.modified = jmax (now, e.modified + 1);
            e.dirty = true;
            e.deleted = false;
            if (! write (e, song)) return entries.count (id) > 0 ? entries[id] : Entry();
            entries[id] = e;
        }
        changedLocally();
        return e;
    }

    void SongLibrary::rename (const String& id, const String& newName, const String& device)
    {
        auto song = load (id);
        if (! song.isValid()) return;
        song.setProperty ("name", newName, nullptr);
        save (id, song, device);
    }

    String SongLibrary::duplicate (const String& id, const String& newName, const String& device)
    {
        auto song = load (id);
        if (! song.isValid()) return {};
        song = song.createCopy();
        song.setProperty ("name", newName, nullptr);
        const auto copyId = newId();
        save (copyId, song, device);
        return copyId;
    }

    void SongLibrary::remove (const String& id, const String& device)
    {
        {
            const ScopedLock sl (lock);
            auto it = entries.find (id);
            if (it == entries.end() || it->second.deleted) return;
            auto e = it->second;
            if (e.rev == 0)
            {
                // never reached the cloud: nobody else needs to hear about it
                fileFor (id).deleteFile();
                fileTimes.erase (id);
                entries.erase (it);
            }
            else
            {
                e.deleted = true;
                e.dirty = true;
                e.device = device;
                e.modified = jmax (Time::currentTimeMillis(), e.modified + 1);
                if (write (e, {})) entries[id] = e;
            }
        }
        changedLocally();
    }

    //==============================================================================
    void SongLibrary::applyRemote (const Entry& remote, const String& songXml, RemoteChange::What what, bool notify)
    {
        {
            const ScopedLock sl (lock);
            if (remote.deleted)
            {
                fileFor (remote.id).deleteFile();
                fileTimes.erase (remote.id);
                entries.erase (remote.id);
            }
            else
            {
                const auto song = parseSong (songXml);
                if (! song.isValid()) return;
                auto e = remote;
                e.dirty = false;
                if (e.name.isEmpty()) e.name = song["name"].toString();
                if (! write (e, song)) return;
                entries[e.id] = e;
            }
        }
        announce();
        if (notify)
            notifyRemote ({ remote.deleted ? RemoteChange::What::deleted : what, remote.id, {}, remote.device });
    }

    void SongLibrary::uploaded (const String& id, int rev, int64 modifiedWhenSent)
    {
        {
            const ScopedLock sl (lock);
            auto it = entries.find (id);
            if (it == entries.end()) return;
            auto e = it->second;
            if (e.deleted)
            {
                if (e.modified == modifiedWhenSent)
                {
                    fileFor (id).deleteFile();         // the cloud has the delete now
                    fileTimes.erase (id);
                    entries.erase (it);
                }
                return;
            }
            const auto song = load (id);
            e.rev = rev;
            if (e.modified == modifiedWhenSent) e.dirty = false;     // else it changed again while it was on its way
            if (write (e, song)) entries[id] = e;
        }
        announce();
    }

    void SongLibrary::setRev (const String& id, int rev)
    {
        const ScopedLock sl (lock);
        auto it = entries.find (id);
        if (it == entries.end()) return;
        auto e = it->second;
        e.rev = rev;
        if (write (e, load (id))) entries[id] = e;
    }

    void SongLibrary::forgetCloud()
    {
        {
            const ScopedLock sl (lock);
            for (auto it = entries.begin(); it != entries.end();)
            {
                auto e = it->second;
                if (e.deleted)
                {
                    fileFor (e.id).deleteFile();
                    fileTimes.erase (e.id);
                    it = entries.erase (it);
                    continue;
                }
                const auto song = load (e.id);
                e.rev = 0;
                e.dirty = true;
                if (write (e, song)) it->second = e;
                ++it;
            }
        }
        announce();
    }

    //==============================================================================
    void SongLibrary::changedLocally()
    {
        announce();
        if (onLocalChange) onLocalChange();
    }

    void SongLibrary::announce()
    {
        std::weak_ptr<bool> token = alive;
        MessageManager::callAsync ([this, token]
        {
            if (auto a = token.lock(); a && *a)
                listeners.call ([] (Listener& l) { l.libraryChanged(); });
        });
    }

    void SongLibrary::notifyRemote (const RemoteChange& rc)
    {
        std::weak_ptr<bool> token = alive;
        MessageManager::callAsync ([this, token, rc]
        {
            if (auto a = token.lock(); a && *a)
                listeners.call ([&rc] (Listener& l) { l.songChangedRemotely (rc); });
        });
    }

    //==============================================================================
    ValueTree SongLibrary::songOnly (const ValueTree& t)
    {
        auto copy = t.createCopy();
        for (auto* name : localSettings)
            copy.removeProperty (name, nullptr);
        return copy;
    }

    ValueTree SongLibrary::withLocalSettings (const ValueTree& song, const ValueTree& current)
    {
        auto copy = song.createCopy();
        for (auto* name : localSettings)
            if (current.hasProperty (name))
                copy.setProperty (name, current[name], nullptr);
        return copy;
    }

    bool SongLibrary::sameSong (const ValueTree& a, const ValueTree& b)
    {
        if (! a.isValid() || ! b.isValid()) return false;
        return sameTree (normalised (songOnly (a)), normalised (songOnly (b)));
    }

    ValueTree SongLibrary::parseSong (const String& xmlText)
    {
        const auto xml = parseXML (xmlText);
        if (xml == nullptr) return {};
        const auto tree = ValueTree::fromXml (*xml);
        return tree.hasType ("SongMap") ? tree : tree.getChildWithName ("SongMap");
    }

    String SongLibrary::newId()
    {
        return Uuid().toDashedString();
    }

    String SongLibrary::uniqueName (const String& wanted, const String& exceptId) const
    {
        const auto songs = list();
        auto taken = [&] (const String& n)
        {
            for (const auto& e : songs)
                if (e.id != exceptId && e.name.equalsIgnoreCase (n)) return true;
            return false;
        };
        if (! taken (wanted)) return wanted;
        for (int i = 2;; ++i)
            if (! taken (wanted + " " + String (i))) return wanted + " " + String (i);
    }
}
