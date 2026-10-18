#!/bin/bash
# Key Compass on an Android emulator (run by the GitHub workflow): install, open, tap through the tabs,
# turn it on its side, and keep a screenshot of each step plus the system log in <out>.
#   android-smoke-test.sh KeyCompass.apk <out folder>
apk="$1"; out="${2:-smoke}"
mkdir -p "$out"
pkg=com.keycompass.app

shot() {
    sleep "${2:-4}"
    clear_dialogs
    adb exec-out screencap -p > "$out/$1.png"
    if command -v convert > /dev/null; then convert "$out/$1.png" -resize 50% -quality 82 "$out/$1.jpg" && rm "$out/$1.png"; fi
    echo "shot $1"
}
# the emulator's own apps sometimes freeze on a slow test machine, and their "isn't responding" box would
# swallow the taps: close that app (never Key Compass - if it is Key Compass, that's a failure to report)
clear_dialogs() {
    local who
    who=$(adb shell dumpsys window windows 2> /dev/null | grep -o "Application \(Not Responding\|Error\): [a-zA-Z0-9._:]*" | head -1)
    [ -z "$who" ] && return
    local app=${who##*: }
    if [ "${app%%:*}" = "$pkg" ]; then
        echo "KEY COMPASS DIALOG: $who" | tee -a "$out/result.txt"
        adb shell screencap -p > "$out/dialog-$(date +%s).png"
    else
        echo "closed a system box: $who" >> "$out/result.txt"
        adb shell am force-stop "${app%%:*}"
    fi
    sleep 2
}
alive() {
    if adb shell pidof "$pkg" > /dev/null; then echo "running after $1"; else echo "NOT RUNNING after $1" | tee -a "$out/result.txt"; fi
}

# a phone that has just started up keeps changing its own settings for a minute (navigation bar, themes),
# which would close any app that is open: wait for it to settle first
until [ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = "1" ]; do sleep 2; done
sleep 45
adb shell input keyevent KEYCODE_WAKEUP
adb shell settings put global hide_error_dialogs 1 || true
adb shell wm dismiss-keyguard 2> /dev/null || true

adb install -r "$apk" 2>&1 | tee "$out/install.txt"
adb shell pm grant "$pkg" android.permission.RECORD_AUDIO || true
sleep 10
adb logcat -c
adb logcat -b events -c 2> /dev/null || true
# open it the way the launcher does (the activity's class depends on the JUCE settings)
adb shell cmd package resolve-activity --brief -c android.intent.category.LAUNCHER "$pkg" | tee "$out/start.txt"
adb shell monkey -p "$pkg" -c android.intent.category.LAUNCHER 1 >> "$out/start.txt" 2>&1
shot 1-start 25
alive "start"

# screen size in pixels and density, to find the tabs (along the bottom on an upright phone)
size=$(adb shell wm size | grep -o '[0-9]*x[0-9]*' | tail -1)
w=${size%x*}; h=${size#*x}
dpi=$(adb shell wm density | grep -o '[0-9]*' | tail -1)
echo "screen ${w}x${h} at ${dpi} dpi" | tee -a "$out/result.txt"
navbar=$(( 48 * dpi / 160 ))
# gesture navigation (mode 2) leaves only a thin handle at the bottom
[ "$(adb shell settings get secure navigation_mode | tr -d '\r')" = "2" ] && navbar=$(( 24 * dpi / 160 ))
tabY=$(( h - navbar - 35 * dpi / 160 ))
tab() { clear_dialogs; adb shell input tap $(( w * (2 * $1 + 1) / 10 )) "$tabY"; }

tab 1; shot 2-chords
tab 2; shot 3-song
tab 3; shot 4-library
tab 4; shot 5-settings
tab 0; shot 6-compass
alive "the tabs"

# press and hold the first progression card on the chords tab (its menu)
tab 1; sleep 3
clear_dialogs
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

# settings people change while an app is open: font size, dark mode, display size (none may close the app)
adb shell settings put system font_scale 1.3
shot 11-big-font 6
adb shell settings put system font_scale 1.0
sleep 3
alive "changing the font size"
adb shell cmd uimode night yes
shot 12-dark-mode 6
adb shell cmd uimode night no
sleep 3
alive "changing dark mode"
adb shell wm density 480
shot 13-display-size 6
adb shell wm density reset
shot 14-display-reset 6
alive "changing the display size"

# away and back (the app saves itself and stops the microphone, then starts again)
adb shell input keyevent KEYCODE_HOME
sleep 4
adb shell monkey -p "$pkg" -c android.intent.category.LAUNCHER 1 > /dev/null 2>&1
shot 10-back 8
alive "leaving and coming back"

adb logcat -d > "$out/logcat.txt"
# why each window of the app opened or closed (finish reasons, restarts, process deaths)
adb logcat -b events -d 2> /dev/null | grep -E "keycompass|am_proc_died|am_kill" | grep -E "wm_|am_" > "$out/events.txt" || true
grep -E "FATAL|AndroidRuntime|libjuce_jni|DEBUG   :|Abort|SIGSEGV|JNI DETECTED" "$out/logcat.txt" | head -80 > "$out/crashes.txt"
grep -iE "juce|keycompass|oboe|AudioRecord|AAudio" "$out/logcat.txt" | head -300 > "$out/app-log.txt"
echo "done" >> "$out/result.txt"
cat "$out/result.txt"
