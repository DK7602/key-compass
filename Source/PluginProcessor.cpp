// Key Compass - audio processor
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KeyCompassProcessor::KeyCompassProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "KeyCompass", createLayout())
{
    modeParam     = parameters.getRawParameterValue (modeId);
    responseParam = parameters.getRawParameterValue (responseId);
    tuningParam   = parameters.getRawParameterValue (tuningId);
    lockParam     = parameters.getRawParameterValue (lockId);

    monoBuffer.resize (4096);
    syncEngineParameters();
    lastSongCopy = songMap.toValueTree();
    songMap.addChangeListener (this);
    analysisThread.startThread (juce::Thread::Priority::low);
}

KeyCompassProcessor::~KeyCompassProcessor()
{
    cancelPendingUpdate();
    songMap.removeChangeListener (this);
    analysisThread.stopThread (2000);
}

juce::AudioProcessorValueTreeState::ParameterLayout KeyCompassProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { modeId, 1 }, "Mode",
                                                        StringArray { "Track", "Mono", "Song" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { responseId, 1 }, "Response",
                                                        StringArray { "Fast", "Balanced", "Steady" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { tuningId, 1 }, "Tuning A4",
                                                       NormalisableRange<float> (415.0f, 466.0f, 0.1f), 440.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { lockId, 1 }, "Lock Key", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { flatsId, 1 }, "Flats", false));
    return layout;
}

void KeyCompassProcessor::syncEngineParameters()
{
    engine.setMode ((kc::AnalysisMode) juce::jlimit (0, 2, (int) std::lround (modeParam->load())));
    engine.setResponse ((kc::ResponseSpeed) juce::jlimit (0, 2, (int) std::lround (responseParam->load())));
    engine.setReferencePitch (tuningParam->load());
    engine.setLocked (lockParam->load() > 0.5f);
}

//==============================================================================
void KeyCompassProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    monoBuffer.resize ((size_t) juce::jmax (samplesPerBlock, 4096));
    engine.prepare (sampleRate);
    audition.prepare (sampleRate);
}

bool KeyCompassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void KeyCompassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());

    // MIDI first (timestamps are relative to this block)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = meta.samplePosition;

        if (m.isNoteOn())                                    engine.pushMidi (m.getNoteNumber(), juce::jmax (0.01f, m.getFloatVelocity()), pos);
        else if (m.isNoteOff())                              engine.pushMidi (m.getNoteNumber(), 0.0f, pos);
        else if (m.isAllNotesOff() || m.isAllSoundOff())     engine.pushAllNotesOff (pos);
        else if (m.isSustainPedalOn())                       engine.pushSustainPedal (true, pos);
        else if (m.isSustainPedalOff())                      engine.pushSustainPedal (false, pos);
    }

    // Mono mix of the input for analysis (the audio itself passes through untouched)
    for (int start = 0; start < numSamples; start += (int) monoBuffer.size())
    {
        const int n = juce::jmin ((int) monoBuffer.size(), numSamples - start);
        float* mono = monoBuffer.data();

        if (numIn <= 0)
        {
            std::fill (mono, mono + n, 0.0f);
        }
        else
        {
            juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0, start), n);
            for (int ch = 1; ch < numIn; ++ch)
                juce::FloatVectorOperations::add (mono, buffer.getReadPointer (ch, start), n);
            if (numIn > 1)
                juce::FloatVectorOperations::multiply (mono, 1.0f / (float) numIn, n);
        }

        engine.pushAudio (mono, n);
    }

    for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // As a standalone app it listens to the microphone; never play that back (feedback).
    if (wrapperType == wrapperType_Standalone)
        buffer.clear();

    // Host tempo for the Song Mapper
    double bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    hostBpm.store (bpm > 1.0 ? bpm : 0.0);

    // Song Mapper audition sound (adds to whatever is in the buffer)
    audition.render (buffer, 0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* KeyCompassProcessor::createEditor()
{
    return new KeyCompassEditor (*this);
}

void KeyCompassProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    // "compassScale" replaces the old "editorScale", which builds up to 1.3.2 always saved as the smallest size
    state.setProperty ("compassScale", editorScale.load(), nullptr);

    const auto snap = engine.getSnapshot();
    state.setProperty ("savedKey", snap.hasKey ? snap.key.index() : -1, nullptr);
    state.setProperty ("savedConfidence", snap.confidence, nullptr);
    state.setProperty ("mapperScale", mapperScale.load(), nullptr);
    state.setProperty ("editorPage", editorPage.load(), nullptr);

    // The song lives on the message thread; hosts usually ask for state there too.
    juce::ValueTree song;
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        song = songMap.toValueTree();
    else
    {
        const juce::ScopedLock sl (pendingSongLock);
        song = pendingSong.isValid() ? pendingSong.createCopy() : lastSongCopy.createCopy();
    }
    if (song.isValid())
        state.appendChild (song, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KeyCompassProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            auto song = tree.getChildWithName ("SongMap").createCopy();
            tree.removeChild (tree.getChildWithName ("SongMap"), nullptr);
            parameters.replaceState (tree);

            const float cs = (float) (double) tree.getProperty ("compassScale", 0.0);
            editorScale.store (cs > 0.0f ? juce::jlimit (0.35f, 1.2f, cs) : 0.0f);
            const float ms = (float) (double) tree.getProperty ("mapperScale", 0.0);
            mapperScale.store (ms > 0.0f ? juce::jlimit (0.35f, 1.2f, ms) : 0.0f);
            editorPage.store (juce::jlimit (0, 1, (int) tree.getProperty ("editorPage", 0)));

            if (song.isValid())
            {
                {
                    const juce::ScopedLock sl (pendingSongLock);
                    pendingSong = song;
                }
                if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                    handleAsyncUpdate();
                else
                    triggerAsyncUpdate();
            }

            const int savedKey = (int) tree.getProperty ("savedKey", -1);
            if (savedKey >= 0 && savedKey < 24)
                engine.restoreKey (kc::KeyId::fromIndex (savedKey), (float) (double) tree.getProperty ("savedConfidence", 0.0));

            syncEngineParameters();
        }
    }
}

void KeyCompassProcessor::handleAsyncUpdate()
{
    juce::ValueTree song;
    {
        const juce::ScopedLock sl (pendingSongLock);
        song = pendingSong;
        pendingSong = {};
    }
    if (song.isValid())
    {
        audition.stop();
        songMap.fromValueTree (song);
        songMap.changed();
    }
}

void KeyCompassProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto copy = songMap.toValueTree();
    const juce::ScopedLock sl (pendingSongLock);
    lastSongCopy = copy;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyCompassProcessor();
}
