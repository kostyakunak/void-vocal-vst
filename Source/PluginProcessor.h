/*
  ==============================================================================

   AudioPluginDemo - постепенная модификация для VoidEngine
   Этап 0: ЗАВЕРШЕН - все параметры и заглушки модулей добавлены

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <type_traits>
#include "DSP/GranularEngine.h"
#include "DSP/SpectralEngine.h"
#include "DSP/SpaceEngine.h"
#include "DSP/DynamicLayer.h"
#include "DSP/MotionMod.h"
#include "DSP/BinauralFlow.h"

//==============================================================================
/** As the name suggest, this class does the actual audio processing. */
class JuceDemoPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    JuceDemoPluginAudioProcessor();
    ~JuceDemoPluginAudioProcessor() override = default;

    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void prepareToPlay (double newSampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool supportsDoublePrecisionProcessing() const override { return true; }

    //==============================================================================
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override;

    //==============================================================================
    bool hasEditor() const override                                   { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    const juce::String getName() const override                             { return "AudioPluginDemo"; }
    bool acceptsMidi() const override                                 { return false; }
    bool producesMidi() const override                                { return false; }
    double getTailLengthSeconds() const override                      { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                     { return 0; }
    int getCurrentProgram() override                                  { return 0; }
    void setCurrentProgram (int) override                             {}
    const juce::String getProgramName (int) override                        { return "None"; }
    void changeProgramName (int, const juce::String&) override              {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    void updateTrackProperties (const TrackProperties& properties) override;

    TrackProperties getTrackProperties() const;

    class SpinLockedPosInfo
    {
    public:
        void set (const juce::AudioPlayHead::PositionInfo& newInfo);
        juce::AudioPlayHead::PositionInfo get() const noexcept;

    private:
        juce::SpinLock mutex;
        juce::AudioPlayHead::PositionInfo info;
    };

    //==============================================================================
    // These properties are public so that our editor component can access them

    SpinLockedPosInfo lastPosInfo;
    juce::AudioProcessorValueTreeState state;

private:
    //==============================================================================
    class JuceDemoPluginAudioProcessorEditor;

    //==============================================================================
    template <typename FloatType>
    void process (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages);

    juce::CriticalSection trackPropertiesLock;
    TrackProperties trackProperties;

    void updateCurrentTimeInfoFromHost();

    static BusesProperties getBusesProperties();

    // Parameter smoothing (to prevent clicks)
    juce::LinearSmoothedValue<float> flowSmoother, meltSmoother, ghostSmoother, 
                                     depthSmoother, claritySmoother, gravitySmoother,
                                     energySmoother, mixSmoother, outputSmoother;

    // DSP Modules
    GranularEngine granularEngine;
    SpectralEngine spectralEngine;
    SpaceEngine spaceEngine;
    DynamicLayer dynamicLayer;
    MotionMod motionMod;
    BinauralFlow binauralFlow;  // Психоакустический кирпич для Iceberg
    
    juce::dsp::ProcessSpec processSpec;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceDemoPluginAudioProcessor)
};

//==============================================================================
// Template implementations

template <typename FloatType>
void JuceDemoPluginAudioProcessor::process (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // In case we have more outputs than inputs, we'll clear any output
    // channels that didn't contain input data
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    // Update parameter smoothers with current values
    flowSmoother.setTargetValue (state.getParameter ("flow")->getValue());
    meltSmoother.setTargetValue (state.getParameter ("melt")->getValue());
    ghostSmoother.setTargetValue (state.getParameter ("ghost")->getValue());
    depthSmoother.setTargetValue (state.getParameter ("depth")->getValue());
    claritySmoother.setTargetValue (state.getParameter ("clarity")->getValue());
    gravitySmoother.setTargetValue (state.getParameter ("gravity")->getValue());
    energySmoother.setTargetValue (state.getParameter ("energy")->getValue());
    mixSmoother.setTargetValue (state.getParameter ("mix")->getValue());
    outputSmoother.setTargetValue (state.getParameter ("output")->getValue());

    // Save dry signal for mixing
    juce::AudioBuffer<FloatType> dryBuffer (numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    // Create wet buffer for processing
    juce::AudioBuffer<FloatType> wetBuffer (numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        wetBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    // Process through DSP modules (stubs for now - pass through unchanged)
    // Processing chain: Granular -> Spectral -> Space -> Dynamic -> Motion
    if (numChannels > 0 && numSamples > 0)
    {
        // Convert to float buffer for processing (modules use float for now)
        juce::AudioBuffer<float> floatBuffer (numChannels, numSamples);
        
        if constexpr (std::is_same_v<FloatType, float>)
        {
            floatBuffer.makeCopyOf (wetBuffer);
        }
        else
        {
            // Convert from double to float
            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* src = wetBuffer.getReadPointer (channel);
                auto* dst = floatBuffer.getWritePointer (channel);
                for (int sample = 0; sample < numSamples; ++sample)
                    dst[sample] = static_cast<float> (src[sample]);
            }
        }
        
        // Update module parameters from CURRENT parameter values
        // Modules will handle their own smoothing internally
        // We pass the target values directly, not smoothed values
        auto flowValue = static_cast<float> (state.getParameter ("flow")->getValue());
        auto depthValue = static_cast<float> (state.getParameter ("depth")->getValue());
        auto ghostValue = static_cast<float> (state.getParameter ("ghost")->getValue());
        auto energyValue = static_cast<float> (state.getParameter ("energy")->getValue());
        auto clarityValue = static_cast<float> (state.getParameter ("clarity")->getValue());
        
        // Update SpaceEngine parameters (module will smooth internally)
        spaceEngine.setDepth (depthValue);
        spaceEngine.setFlow (flowValue);
        spaceEngine.setGhost (ghostValue);
        
        // Update SpectralEngine parameters (module will smooth internally)
        spectralEngine.setClarity (clarityValue);
        spectralEngine.setDepth (depthValue);
        spectralEngine.setFlow (flowValue);
        
        // Update MotionMod parameters (module will smooth internally)
        motionMod.setFlow (flowValue);
        motionMod.setEnergy (energyValue);
        
        // Update BinauralFlow parameters (module will smooth internally)
        binauralFlow.setFlow (flowValue);
        binauralFlow.setDepth (depthValue);
        binauralFlow.setGhost (ghostValue);
        
        // Process through modules
        // Processing chain: Granular -> Spectral -> BinauralFlow -> Space -> Dynamic -> Motion
        granularEngine.process (floatBuffer);
        spectralEngine.process (floatBuffer);
        binauralFlow.process (floatBuffer);  // Психоакустический кирпич для Iceberg
        spaceEngine.process (floatBuffer);
        dynamicLayer.process (floatBuffer);
        motionMod.process (floatBuffer);
        
        // Convert back to FloatType and write to wet buffer
        if constexpr (std::is_same_v<FloatType, float>)
        {
            wetBuffer.makeCopyOf (floatBuffer);
        }
        else
        {
            // Convert from float to double
            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* src = floatBuffer.getReadPointer (channel);
                auto* dst = wetBuffer.getWritePointer (channel);
                for (int sample = 0; sample < numSamples; ++sample)
                    dst[sample] = static_cast<FloatType> (src[sample]);
            }
        }
    }

    // Apply dry/wet mix with per-sample smoothing
    // Process all channels with the same smoothed mix/output values
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get current smoothed values (smoother updates internally with getNextValue)
        auto currentMix = static_cast<FloatType> (mixSmoother.getNextValue());
        auto currentOutput = static_cast<FloatType> (outputSmoother.getNextValue());
        
        // Apply to all channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* dry = dryBuffer.getReadPointer (channel);
            auto* wet = wetBuffer.getReadPointer (channel);
            auto* out = buffer.getWritePointer (channel);
            
            // Mix dry and wet: out = dry * (1 - mix) + wet * mix
            auto mixed = dry[sample] * (static_cast<FloatType> (1.0) - currentMix) + 
                        wet[sample] * currentMix;
            
            // Apply output gain
            out[sample] = mixed * currentOutput;
        }
    }

    updateCurrentTimeInfoFromHost();
}
