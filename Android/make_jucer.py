#!/usr/bin/env python3
"""Writes Android/KeyCompassMobile.jucer from the files in Source/ and Resources/.

The Projucer turns that project into the Android Studio build (Android/Builds/Android).
Run from anywhere:  python3 Android/make_jucer.py   (the GitHub workflow runs it before every build)
"""
import glob, os, re

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)

version = re.search(r'KC_VERSION_STRING\s+"([^"]+)"', open('Source/Version.h').read()).group(1)
major, minor, patch = (int(x) for x in version.split('.'))
version_code = major * 10000 + minor * 100 + patch      # 1.4.0 -> 10400; must go up with every release

src = sorted(f for f in glob.glob('Source/**/*.*', recursive=True)
             if f.endswith(('.cpp', '.h')) and not f.startswith('Source/Tests'))
res = ['Resources/background.jpg', 'Resources/face.jpg', 'Resources/mapper_bg.jpg', 'Resources/plaque.png',
       'Resources/fonts/CinzelBold.ttf', 'Resources/fonts/BaskervilleBold.ttf', 'Resources/fonts/BaskervilleBoldItalic.ttf',
       'Resources/fonts/BaskervilleRegular.ttf', 'Resources/manual/KeyCompassManual.pdf', 'Resources/samples/samples.txt']
res += sorted(glob.glob('Resources/samples/*.ogg')) + sorted(glob.glob('Resources/mobile/*.*'))
modules = ['juce_audio_basics', 'juce_audio_devices', 'juce_audio_formats', 'juce_audio_processors',
           'juce_audio_processors_headless', 'juce_audio_utils', 'juce_core', 'juce_data_structures', 'juce_events',
           'juce_graphics', 'juce_gui_basics', 'juce_gui_extra']

out = []
w = out.append
w('<?xml version="1.0" encoding="UTF-8"?>\n')
w('<!-- Key Compass for Android - written by Android/make_jucer.py (edit that, not this). The GitHub workflow\n'
  '     re-saves it with the Projucer into Android/Builds/Android and builds it with Gradle. JUCE goes in ../JUCE. -->\n')
w(f'<JUCERPROJECT id="KcMob1" name="Key Compass" projectType="guiapp" version="{version}" bundleIdentifier="com.keycompass.app"\n'
  '              companyName="Key Compass" cppLanguageStandard="17" jucerFormatVersion="1"\n'
  '              defines="KC_MOBILE=1" addUsingNamespaceToJuceHeader="0" useAppConfig="1" maxBinaryFileSize="4194304">\n')
w('  <MAINGROUP id="KcMain" name="Key Compass">\n')
w('    <GROUP id="KcSrc" name="Source">\n')
for i, f in enumerate(src):
    w(f'      <FILE id="Ksrc{i:03d}" name="{os.path.basename(f)}" compile="{1 if f.endswith(".cpp") else 0}" resource="0" file="../{f}"/>\n')
w('    </GROUP>\n')
w('    <GROUP id="KcRes" name="Resources">\n')
for i, f in enumerate(res):
    w(f'      <FILE id="Kres{i:03d}" name="{os.path.basename(f)}" compile="0" resource="1" file="../{f}"/>\n')
w('      <FILE id="KcIcon" name="KeyCompassIcon.png" compile="0" resource="0" file="../Resources/KeyCompassIcon.png"/>\n')
w('    </GROUP>\n')
w('  </MAINGROUP>\n')
w('  <JUCEOPTIONS JUCE_WEB_BROWSER="0" JUCE_USE_CURL="0" JUCE_STRICT_REFCOUNTEDPOINTER="1" JUCE_USE_ANDROID_OBOE="1"\n'
  '               JUCE_USE_OGGVORBIS="1" JUCE_USE_FLAC="0" JUCE_USE_MP3AUDIOFORMAT="0"/>\n')
w('  <MODULES>\n')
for m in modules:
    w(f'    <MODULE id="{m}" showAllCode="1" useLocalCopy="0" useGlobalPath="0"/>\n')
w('  </MODULES>\n')
w('  <EXPORTFORMATS>\n')
w(f'    <ANDROIDSTUDIO targetFolder="Builds/Android" androidMinimumSDK="26" androidTargetSDK="35"\n'
  f'                   androidVersionCode="{version_code}" androidScreenOrientation="unspecified"\n'
  '                   microphonePermissionNeeded="1" androidEnableContentSharing="1" androidPushNotifications="0"\n'
  '                   androidExternalReadNeeded="0" androidExternalWriteNeeded="0" androidReadMediaAudioPermission="0"\n'
  '                   androidReadMediaImagesPermission="0" androidReadMediaVideoPermission="0"\n'
  '                   androidBluetoothNeeded="0" androidInternetNeeded="0" bigIcon="KcIcon" smallIcon="KcIcon">\n')
w('      <CONFIGURATIONS>\n')
w('        <CONFIGURATION isDebug="1" name="Debug" androidArchitectures="arm64-v8a" recommendedWarnings="LLVM"/>\n')
w('        <CONFIGURATION isDebug="0" name="Release" androidArchitectures="armeabi-v7a arm64-v8a" optimisation="3" recommendedWarnings="LLVM"/>\n')
w('      </CONFIGURATIONS>\n')
w('      <MODULEPATHS>\n')
for m in modules:
    w(f'        <MODULEPATH id="{m}" path="../JUCE/modules"/>\n')
w('      </MODULEPATHS>\n')
w('    </ANDROIDSTUDIO>\n')
w('  </EXPORTFORMATS>\n')
w('</JUCERPROJECT>\n')

open('Android/KeyCompassMobile.jucer', 'w', newline='\n').write(''.join(out))
print(f'Android/KeyCompassMobile.jucer: version {version} (code {version_code}), {len(src)} source files, {len(res)} resources')
