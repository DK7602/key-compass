#!/bin/bash
# Key Compass on an Android emulator (run by the GitHub workflow): install, open, tap through the tabs,
# turn it on its side, and keep a screenshot of each step plus the system log in <out>.
#   android-smoke-test.sh KeyCompass.apk <out folder>
apk="$1"; out="${2:-smoke}"
mkdir -p "$out"
pkg=com.keycompass.app

shot() {
    sleep "${2:-4}"
    adb exec-out screencap -p > "$out/$1.png"
    if command -v convert > /dev/null; then convert "$out/$1.png" -resize 50% -quality 82 "$out/$1.jpg" && rm "$out/$1.png"; fi
    echo "shot $1"
}
alive() {
    if adb shell pidof "$pkg" > /dev/null; then echo "running after $1"; else echo "NOT RUNNING after $1" | tee -a "$out/result.txt"; fi
}

adb install -r "$apk" 2>&1 | tee "$out/install.txt"
adb shell pm grant "$pkg" android.permission.RECORD_AUDIO || true
adb logcat -c
adb shell am start -W -n "$pkg/com.rmsl.juce.JuceActivity" | tee "$out/start.txt"
shot 1-start 25
alive "start"

# screen size in pixels and density, to find the tabs (along the bottom on an upright phone)
size=$(adb shell wm size | grep -o '[0-9]*x[0-9]*' | tail -1)
w=${size%x*}; h=${size#*x}
dpi=$(adb shell wm density | grep -o '[0-9]*' | tail -1)
echo "screen ${w}x${h} at ${dpi} dpi" | tee -a "$out/result.txt"
navbar=$(( 48 * dpi / 160 ))
tabY=$(( h - navbar - 35 * dpi / 160 ))
tab() { adb shell input tap $(( w * (2 * $1 + 1) / 10 )) "$tabY"; }

tab 1; shot 2-chords
tab 2; shot 3-song
tab 3; shot 4-library
tab 4; shot 5-settings
tab 0; shot 6-compass
alive "the tabs"

# press and hold the first progression card on the chords tab (its menu)
tab 1; sleep 3
adb shell input swipe $(( w * 16 / 100 )) $(( h * 47 / 100 )) $(( w * 16 / 100 )) $(( h * 47 / 100 )) 900
shot 7-card-menu 2
adb shell input keyevent KEYCODE_BACK
sleep 2
alive "a menu"

# on its side
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
shot 8-sideways 6
adb shell settings put system user_rotation 0
shot 9-upright 6
alive "turning"

# away and back (the app saves itself and stops the microphone, then starts again)
adb shell input keyevent KEYCODE_HOME
sleep 4
adb shell am start -n "$pkg/com.rmsl.juce.JuceActivity" > /dev/null
shot 10-back 8
alive "leaving and coming back"

adb logcat -d > "$out/logcat.txt"
grep -E "FATAL|AndroidRuntime|libjuce_jni|DEBUG   :|Abort|SIGSEGV|JNI DETECTED" "$out/logcat.txt" | head -80 > "$out/crashes.txt"
grep -iE "juce|keycompass|oboe|AudioRecord|AAudio" "$out/logcat.txt" | head -300 > "$out/app-log.txt"
echo "done" >> "$out/result.txt"
cat "$out/result.txt"
