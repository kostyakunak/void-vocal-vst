/*
  ==============================================================================

   AudioPluginDemo - постепенная модификация для VoidEngine
   Этап 0: ЗАВЕРШЕН - все параметры и заглушки модулей добавлены

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
JuceDemoPluginAudioProcessor::JuceDemoPluginAudioProcessor()
    : AudioProcessor (getBusesProperties()),
      state (*this, nullptr, "state",
             { 
                 // Universal parameters (0.0 - 1.0, normalized)
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "flow", 1 }, "Flow", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "melt", 1 }, "Melt", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "ghost", 1 }, "Ghost", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "depth", 1 }, "Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "clarity", 1 }, "Clarity", juce::NormalisableRange<float> (-0.5f, 0.5f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gravity", 1 }, "Gravity", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "energy", 1 }, "Energy", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
                 std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (0.0f, 2.0f), 2.0f)
             })
{
    state.state.addChild ({ "uiState", { { "width",  400 }, { "height", 200 } }, {} }, -1, nullptr);
}

//==============================================================================
bool JuceDemoPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    if (! mainInput.isDisabled() && mainInput != mainOutput)
        return false;

    if (mainOutput.size() > 2)
        return false;

    return true;
}

void JuceDemoPluginAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    // Setup process spec
    processSpec.sampleRate = newSampleRate;
    processSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    processSpec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    // Initialize parameter smoothers (30ms smoothing time)
    const float smoothingTimeMs = 30.0f;
    const float smoothingTimeInSeconds = smoothingTimeMs / 1000.0f;
    
    flowSmoother.reset (newSampleRate, smoothingTimeInSeconds);
    meltSmoother.reset (newSampleRate, smoothingTimeInSeconds);
    ghostSmoother.reset (newSampleRate, smoothingTimeInSeconds);
    depthSmoother.reset (newSampleRate, smoothingTimeInSeconds);
    claritySmoother.reset (newSampleRate, smoothingTimeInSeconds);
    gravitySmoother.reset (newSampleRate, smoothingTimeInSeconds);
    energySmoother.reset (newSampleRate, smoothingTimeInSeconds);
    mixSmoother.reset (newSampleRate, smoothingTimeInSeconds);
    outputSmoother.reset (newSampleRate, smoothingTimeInSeconds);

    // Set initial values
    flowSmoother.setCurrentAndTargetValue (0.0f);
    meltSmoother.setCurrentAndTargetValue (0.0f);
    ghostSmoother.setCurrentAndTargetValue (0.0f);
    depthSmoother.setCurrentAndTargetValue (0.0f);
    claritySmoother.setCurrentAndTargetValue (0.0f);
    gravitySmoother.setCurrentAndTargetValue (0.0f);
    energySmoother.setCurrentAndTargetValue (0.0f);
    mixSmoother.setCurrentAndTargetValue (0.0f);
    outputSmoother.setCurrentAndTargetValue (2.0f);
    
    // Prepare DSP modules
    granularEngine.prepare (processSpec);
    spectralEngine.prepare (processSpec);
    binauralFlow.prepare (processSpec);  // После Granular, перед Reverb
    harmonicGlide.prepare (processSpec);  // Психоакустический кирпич для Platina
    spaceEngine.prepare (processSpec);
    dynamicLayer.prepare (processSpec);
    motionMod.prepare (processSpec);
    
    reset();
}

void JuceDemoPluginAudioProcessor::releaseResources()
{
}

void JuceDemoPluginAudioProcessor::reset()
{
    // Reset DSP modules
    granularEngine.reset();
    spectralEngine.reset();
    binauralFlow.reset();
    harmonicGlide.reset();
    spaceEngine.reset();
    dynamicLayer.reset();
    motionMod.reset();
}

//==============================================================================
void JuceDemoPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    jassert (! isUsingDoublePrecision());
    process (buffer, midiMessages);
}

void JuceDemoPluginAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    jassert (isUsingDoublePrecision());
    process (buffer, midiMessages);
}

//==============================================================================
juce::AudioProcessorEditor* JuceDemoPluginAudioProcessor::createEditor()
{
    return new ::JuceDemoPluginAudioProcessorEditor (*this);
}

//==============================================================================
void JuceDemoPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xmlState = state.copyState().createXml())
        copyXmlToBinary (*xmlState, destData);
}

void JuceDemoPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        state.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
void JuceDemoPluginAudioProcessor::updateTrackProperties (const TrackProperties& properties)
{
    {
        const juce::ScopedLock sl (trackPropertiesLock);
        trackProperties = properties;
    }

    juce::MessageManager::callAsync ([this]
    {
        if (auto* editor = dynamic_cast<::JuceDemoPluginAudioProcessorEditor*> (getActiveEditor()))
             editor->updateTrackProperties();
    });
}

JuceDemoPluginAudioProcessor::TrackProperties JuceDemoPluginAudioProcessor::getTrackProperties() const
{
    const juce::ScopedLock sl (trackPropertiesLock);
    return trackProperties;
}

//==============================================================================
void JuceDemoPluginAudioProcessor::SpinLockedPosInfo::set (const juce::AudioPlayHead::PositionInfo& newInfo)
{
    const juce::SpinLock::ScopedTryLockType lock (mutex);

    if (lock.isLocked())
        info = newInfo;
}

juce::AudioPlayHead::PositionInfo JuceDemoPluginAudioProcessor::SpinLockedPosInfo::get() const noexcept
{
    const juce::SpinLock::ScopedLockType lock (mutex);
    return info;
}

//==============================================================================

void JuceDemoPluginAudioProcessor::updateCurrentTimeInfoFromHost()
{
    const auto newInfo = [&]
    {
        if (auto* ph = getPlayHead())
            if (auto result = ph->getPosition())
                return *result;

        return juce::AudioPlayHead::PositionInfo{};
    }();

    lastPosInfo.set (newInfo);
}

JuceDemoPluginAudioProcessor::BusesProperties JuceDemoPluginAudioProcessor::getBusesProperties()
{
    return BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuceDemoPluginAudioProcessor();
}
