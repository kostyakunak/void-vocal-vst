/*
  ==============================================================================

   SpaceEngine - Reverb/Space модуль
   Этап 1: Реализация с оптимизацией для мужского вокала

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class SpaceEngine
{
public:
    SpaceEngine();
    ~SpaceEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameter control (normalized 0.0-1.0)
    void setDepth (float depth);      // 0.0 = close, 1.0 = deep space
    void setFlow (float flow);        // 0.0 = static, 1.0 = moving
    void setGhost (float ghost);      // 0.0 = no reflections, 1.0 = dense reflections

private:
    void updateParameters();

    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    
    // Pre-delay for male vocal clarity (100-150 ms optimal)
    // Using simple delay buffers for now (can be improved with proper DelayLine later)
    juce::AudioBuffer<float> predelayBufferL, predelayBufferR;
    int predelayWritePosL = 0, predelayWritePosR = 0;
    int predelayReadPosL = 0, predelayReadPosR = 0;
    
    // Stereo width control
    float stereoWidth = 1.0f;
    
    // Parameters (normalized 0.0-1.0)
    float depthParam = 0.0f;
    float flowParam = 0.0f;
    float ghostParam = 0.0f;
    
    // Smoothed parameters to prevent clicks
    juce::LinearSmoothedValue<float> depthSmoother, flowSmoother, ghostSmoother;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;
    
    // Male vocal optimized settings
    // Уменьшено для устранения заметной задержки: 20-120 мс вместо 100-200 мс
    static constexpr float MIN_PREDELAY_MS = 20.0f;   // Минимальная задержка (незаметна)
    static constexpr float MAX_PREDELAY_MS = 120.0f; // Максимальная задержка (для глубины)
    static constexpr float MIN_DECAY_SEC = 1.0f;
    static constexpr float MAX_DECAY_SEC = 20.0f;     // Iceberg can go up to 20 sec
    static constexpr float MIN_DAMPING = 0.3f;        // Less damping for male voice (lower frequencies)
    static constexpr float MAX_DAMPING = 0.7f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEngine)
};

