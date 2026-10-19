cmake_minimum_required (VERSION 3.22)

project (KeyCompass VERSION 1.5.1 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    # Link the C++ runtime statically so the plug-in doesn't need the Visual C++ Redistributable.
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

# ---------------------------------------------------------------------------------------------
# JUCE (the build script downloads it into ./JUCE)
set (KC_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Location of the JUCE 8 framework")
if (NOT EXISTS "${KC_JUCE_DIR}/CMakeLists.txt")
    message (FATAL_ERROR "JUCE was not found at ${KC_JUCE_DIR}. Run 'Build and Install.bat', or put JUCE 8 in that folder.")
endif ()
add_subdirectory ("${KC_JUCE_DIR}" JUCE)

set (KC_FORMATS VST3 Standalone CACHE STRING "Plug-in formats to build")
option (KC_LINUX_TEST "Developer build for Linux without audio devices / X11 extensions" OFF)

# ---------------------------------------------------------------------------------------------
juce_add_plugin (KeyCompass
    COMPANY_NAME                "Key Compass"
    PRODUCT_NAME                "Key Compass"
    VERSION                     1.5.1
    PLUGIN_MANUFACTURER_CODE    KcMp
    PLUGIN_CODE                 Kcmp
    FORMATS                     ${KC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Analyzer
    BUNDLE_ID                   com.keycompass.plugin
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/KeyCompassIcon.png"
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/KeyCompassIconSmall.png"
    DESCRIPTION                 "Key, chord and note finder")

# recorded guitar / bass samples (Resources/samples, CC BY 3.0 - see the README there)
file (GLOB KC_SAMPLE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/samples/*.ogg")

juce_add_binary_data (KeyCompassAssets
    SOURCES
        ${KC_SAMPLE_FILES}
        Resources/samples/samples.txt
        Resources/manual/KeyCompassManual.pdf
        Resources/background.jpg
        Resources/face.jpg
        Resources/mapper_bg.jpg
        Resources/plaque.png
        Resources/mobile/panel.jpg
        Resources/mobile/map.jpg
        Resources/fonts/CinzelBold.ttf
        Resources/fonts/BaskervilleBold.ttf
        Resources/fonts/BaskervilleBoldItalic.ttf
        Resources/fonts/BaskervilleRegular.ttf)

set (KC_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Theme.cpp
    Source/UI/Controls.cpp
    Source/UI/CompassViews.cpp
    Source/Engine/KeyEngine.cpp
    Source/Engine/MusicTheory.cpp
    Source/SongMapper/ChordTheory.cpp
    Source/SongMapper/ProgressionLibrary.cpp
    Source/SongMapper/SongModel.cpp
    Source/SongMapper/SampleLibrary.cpp
    Source/SongMapper/Audition.cpp
    Source/UI/SongMapperView.cpp
    Source/UI/Manual.cpp
    Source/UI/MySongsView.cpp
    Source/Sync/SongLibrary.cpp
    Source/Sync/CloudSync.cpp
    Source/Sync/SongLink.cpp)

target_sources (KeyCompass PRIVATE ${KC_SOURCES})

set (KC_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if (KC_LINUX_TEST)
    list (APPEND KC_DEFINITIONS JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif ()

target_compile_definitions (KeyCompass PUBLIC ${KC_DEFINITIONS})

target_link_libraries (KeyCompass
    PRIVATE
        KeyCompassAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------------------------
# Developer-only (Linux): renders the screens to PNG files with test audio (not needed to use the plug-in)
if (KC_LINUX_TEST)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/SnapshotMain.cpp")
        juce_add_console_app (KeyCompassSnapshot PRODUCT_NAME "KeyCompassSnapshot")
        target_sources (KeyCompassSnapshot PRIVATE Tests/SnapshotMain.cpp ${KC_SOURCES})
        target_compile_definitions (KeyCompassSnapshot PRIVATE ${KC_DEFINITIONS}
            JucePlugin_Name="Key Compass" JucePlugin_Build_Standalone=0 JUCE_STANDALONE_APPLICATION=1)
        target_link_libraries (KeyCompassSnapshot PRIVATE
            KeyCompassAssets juce::juce_audio_processors juce::juce_gui_extra juce::juce_audio_formats
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif ()

    # the Android app's screens, drawn at phone and tablet sizes (Tests/MobileShots.cpp)
    juce_add_console_app (KeyCompassMobileShots PRODUCT_NAME "KeyCompassMobileShots")
    target_sources (KeyCompassMobileShots PRIVATE Tests/MobileShots.cpp Tests/SyncChecks.cpp Source/Mobile/MobileEditor.cpp Source/Mobile/ManualViewer.cpp ${KC_SOURCES})
    target_compile_definitions (KeyCompassMobileShots PRIVATE ${KC_DEFINITIONS}
        KC_MOBILE=1 JucePlugin_Build_Standalone=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries (KeyCompassMobileShots PRIVATE
        KeyCompassAssets juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra juce::juce_audio_formats
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif ()
