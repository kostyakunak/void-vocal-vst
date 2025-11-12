/*
  ==============================================================================

   OfflineRenderer - Офлайн-рендеринг плагина через командную строку
   Для тестирования без DAW

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

//==============================================================================
class OfflineRenderer
{
public:
    OfflineRenderer();
    ~OfflineRenderer() = default;

    bool renderFile (const juce::String& inputFile,
                     const juce::String& outputFile,
                     const juce::String& presetParams = "");

private:
    std::unique_ptr<JuceDemoPluginAudioProcessor> processor;
    
    bool loadAudioFile (const juce::String& filePath, juce::AudioBuffer<float>& buffer, double& sampleRate);
    bool saveAudioFile (const juce::String& filePath, const juce::AudioBuffer<float>& buffer, double sampleRate);
    void parsePresetParams (const juce::String& params);
};

