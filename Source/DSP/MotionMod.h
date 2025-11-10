/*
  ==============================================================================

   MotionMod - Modulation Core модуль
   Этап 1: Реализация LFO для панорамы/громкости (оптимизировано для мужского вокала)

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

//==============================================================================
class MotionMod
{
public:
    MotionMod();
    ~MotionMod() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameter control (normalized 0.0-1.0)
    void setFlow (float flow);        // 0.0 = static, 1.0 = moving
    void setEnergy (float energy);    // 0.0 = subtle, 1.0 = pronounced

private:
    // LFO state
    float lfoPhasePan = 0.0f;        // Pan LFO phase (0-2π)
    float lfoPhaseGain = 0.5f;      // Gain LFO phase (offset by 90° for anti-phase)
    
    // Envelope follower for transient protection (prevents clicks on attacks)
    float envelopeFollower = 0.0f;
    static constexpr float ENVELOPE_ATTACK = 0.001f;   // Fast attack (1ms)
    static constexpr float ENVELOPE_RELEASE = 0.05f;   // Slow release (50ms)
    
    // LFO fade-in to prevent clicks on startup
    float lfoFadeIn = 0.0f;
    static constexpr float LFO_FADE_IN_TIME = 0.5f;   // 500ms fade-in
    
    // Parameters (normalized 0.0-1.0)
    float flowParam = 0.0f;
    float energyParam = 0.0f;
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> flowSmoother, energySmoother;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;
    
    // Male vocal optimized settings - согласно концепции Iceberg
    // Концепция: "медленное дыхание", "плывущее пространство"
    // Увеличено для заметности: Flow=0% → почти статично, Flow=100% → заметное дыхание
    // Energy влияет на минимальную частоту: даже при Flow=0% есть движение, если Energy>0%
    static constexpr float BASE_MIN_LFO_HZ = 0.03f;    // Базовое минимальное значение (33 sec cycle)
    static constexpr float ENERGY_MIN_LFO_HZ = 0.02f;  // Добавка от Energy (Energy=100% → +0.02 Hz)
    static constexpr float MAX_LFO_HZ = 0.5f;          // Заметное дыхание (2 sec cycle) - более заметное!
    static constexpr float LFO_CURVE = 1.8f;           // Нелинейная кривая (flow^1.8) - плавное ускорение
    static constexpr float MAX_PAN_AMPLITUDE = 0.28f;  // ±28% pan (заметное, но не слишком агрессивное)
    static constexpr float MAX_GAIN_AMPLITUDE = 0.1f;  // ±10% gain (заметное дыхание громкости)
    static constexpr float TWO_PI = 6.28318530718f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotionMod)
};

