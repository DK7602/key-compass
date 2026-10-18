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

## Moving a song between the phone and the PC

On the phone: *SETTINGS → SHARE SONG FILE*. On the PC: *SONG menu → Open a song file*.
The other way round: *SONG menu → Save song file*, then *SETTINGS → OPEN SONG FILE* on the phone.

## How it's put together

| Folder | |
| --- | --- |
| `Source/Engine` | key, chord and pitch detection (no JUCE dependency) |
| `Source/SongMapper` | the song model, chord theory, progression library, sampled guitar and bass |
| `Source/UI` | the compass and Song Mapper pages (shared by every platform) |
| `Source/Mobile` | the Android app: audio and saving, the phone / tablet layouts, the in-app manual |
| `Android/make_jucer.py` | writes the Projucer project the Android build is made from |
| `Tests/MobileShots.cpp` | draws the app at phone and tablet sizes and runs touch checks (Linux) |

Built with [JUCE](https://juce.com) 8.0.15. Guitar and bass sounds from tonejs-instruments (CC BY 3.0);
fonts Cinzel and Libre Baskerville (SIL OFL) - see *HOW TO INSTALL AND USE.txt*.
