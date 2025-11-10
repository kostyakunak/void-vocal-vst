/*
  ==============================================================================

   GranularEngine - заглушка модуля
   Этап 0: Базовая структура без обработки

  ==============================================================================
*/

#include "GranularEngine.h"

//==============================================================================
GranularEngine::GranularEngine()
{
}

//==============================================================================
void GranularEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = (int) spec.maximumBlockSize;
    reset();
}

//==============================================================================
void GranularEngine::reset()
{
    // TODO: Reset granular buffers, grains, etc.
}

//==============================================================================
void GranularEngine::process (juce::AudioBuffer<float>& buffer)
{
    // TODO: Implement granular processing
    // For now, pass through unchanged
    juce::ignoreUnused (buffer);
}

