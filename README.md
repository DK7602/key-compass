# Key Compass

A key, chord and note finder with a song sketchpad (the **Song Mapper**), as a VST3 plug-in and app for Windows,
and as an app for Android phones and tablets.

## Android

**Install:** open [the latest release](../../releases/latest) on your phone and tap **KeyCompass.apk**, then *Open*.
The first time, Android asks you to allow installs from your browser: allow it, then tap *Install*.
Android 8 or newer. A new version installs over the old one and keeps your song.

Every push to `main` builds a new `KeyCompass.apk` with GitHub Actions (`.github/workflows/android.yml`)
and puts it on the release page.

## Windows (VST3 + standalone)

Download the repository as a zip, extract it, and double-click **Build and Install.bat**
(it needs Visual Studio with "Desktop development with C++"). See *HOW TO INSTALL AND USE.txt*.

## My Songs and cloud sync

Every song you work on is kept in **My Songs** (*SONG menu → My songs* on the PC, *SETTINGS → MY SONGS* on the phone).
**Cloud sync** keeps My Songs and the starred progressions the same on every device with the same sync code:
*My Songs → CLOUD SYNC → TURN ON SYNC* on one device, then type the code it shows on the others.

The sync server is a Cloudflare Worker with a D1 database (`cloud/`), deployed by `.github/workflows/cloud.yml`
(it needs the `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` secrets). `node cloud/test/local-server.mjs`
runs the same server locally, with Node's SQLite standing in for D1.

Without sync, a song still moves as a file: *SETTINGS → SHARE SONG FILE* on the phone, *SONG menu → Open a song file*
on the PC (and *Save song file* / *OPEN SONG FILE* the other way).

## How it's put together

| Folder | |
| --- | --- |
| `Source/Engine` | key, chord and pitch detection (no JUCE dependency) |
| `Source/SongMapper` | the song model, chord theory, progression library, sampled guitar and bass |
| `Source/UI` | the compass and Song Mapper pages (shared by every platform) |
| `Source/Mobile` | the Android app: audio and saving, the phone / tablet layouts, the in-app manual |
| `Source/Sync` | My Songs (the song folder), cloud sync, and the link between the open song and both |
| `cloud/` | the sync server (Cloudflare Worker + D1), a local stand-in and its checks |
| `Android/make_jucer.py` | writes the Projucer project the Android build is made from |
| `Tests/MobileShots.cpp` | draws the app at phone and tablet sizes and runs touch checks (Linux) |
| `Tests/SyncChecks.cpp` | two devices syncing through the local server: edits, clashes, deletes, offline |

Built with [JUCE](https://juce.com) 8.0.15. Guitar and bass sounds from tonejs-instruments (CC BY 3.0);
fonts Cinzel and Libre Baskerville (SIL OFL) - see *HOW TO INSTALL AND USE.txt*.
