/*
  ==============================================================================

   GranularEngine - заглушка модуля
   Этап 0: Базовая структура без обработки

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class GranularEngine
{
public:
    GranularEngine();
    ~GranularEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularEngine)
};






