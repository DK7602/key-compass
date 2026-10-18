// Key Compass - audio processor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/KeyEngine.h"
#include "SongMapper/SongModel.h"
#include "SongMapper/Audition.h"
#include "Sync/SongLink.h"

#ifndef JucePlugin_Name
 #define JucePlugin_Name "Key Compass"      // the Android app is not built as a plug-in
#endif

class KeyCompassProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::ChangeListener
{
public:
    KeyCompassProcessor();
    ~KeyCompassProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    // "infinite" keeps hosts (e.g. Cubase's silence suspend) processing so the Song Mapper audition is heard on a silent track
    double getTailLengthSeconds() const override         { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    kc::KeyEngine& getEngine() noexcept                  { return engine; }
    juce::AudioProcessorValueTreeState& getState() noexcept { return parameters; }

    float getEditorScale() const noexcept                { return editorScale.load(); }
    void setEditorScale (float s) noexcept               { editorScale.store (s); }
    float getMapperScale() const noexcept                { return mapperScale.load(); }
    void setMapperScale (float s) noexcept               { mapperScale.store (s); }
    int getEditorPage() const noexcept                   { return editorPage.load(); }
    void setEditorPage (int p) noexcept                  { editorPage.store (p); }
    int getMobileTab() const noexcept                    { return mobileTab.load(); }   // the Android app's tab
    void setMobileTab (int t) noexcept                   { mobileTab.store (t); }

    // Song Mapper
    sm::SongMap& getSongMap() noexcept                   { return songMap; }
    kcsync::SongLink& getSongLink() noexcept             { return *songLink; }     // My Songs and cloud sync
    sm::AuditionEngine& getAudition() noexcept           { return audition; }
    double getHostBpm() const noexcept                   { return hostBpm.load(); }   // 0 = no tempo from the host

    static constexpr const char* modeId     = "mode";
    static constexpr const char* responseId = "response";
    static constexpr const char* tuningId   = "tuning";
    static constexpr const char* lockId     = "lock";
    static constexpr const char* flatsId    = "flats";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void syncEngineParameters();

    class AnalysisThread : public juce::Thread
    {
    public:
        explicit AnalysisThread (KeyCompassProcessor& p) : juce::Thread ("Key Compass analysis"), owner (p) {}
        void run() override
        {
            while (! threadShouldExit())
            {
                owner.syncEngineParameters();
                owner.engine.process();
                wait (8);
            }
        }
    private:
        KeyCompassProcessor& owner;
    };

    kc::KeyEngine engine;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* responseParam = nullptr;
    std::atomic<float>* tuningParam = nullptr;
    std::atomic<float>* lockParam = nullptr;

    std::vector<float> monoBuffer;
    std::atomic<float> editorScale { 0.0f }, mapperScale { 0.0f };    // window size per page; 0 = choose automatically
    std::atomic<int> editorPage { 0 };                                  // 0 = compass, 1 = song mapper
    std::atomic<int> mobileTab { 0 };                                   // Android app: 0 compass, 1 chords, 2 song, 3 library, 4 more

    sm::SongMap songMap;
    std::unique_ptr<kcsync::SongLink> songLink;     // after songMap: goes first
    sm::AuditionEngine audition;
    std::atomic<double> hostBpm { 0.0 };
    juce::CriticalSection pendingSongLock;
    juce::ValueTree pendingSong, lastSongCopy;
    AnalysisThread analysisThread { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyCompassProcessor)
};
