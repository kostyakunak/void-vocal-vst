/*
  ==============================================================================

   MotionMod - Modulation Core модуль
   Этап 1: Реализация LFO для панорамы/громкости (оптимизировано для мужского вокала)

  ==============================================================================
*/

#include "MotionMod.h"

//==============================================================================
MotionMod::MotionMod()
{
    // Initialize smoothers (30ms smoothing)
    flowSmoother.reset (44100.0, 0.03f);
    energySmoother.reset (44100.0, 0.03f);
    
    flowSmoother.setCurrentAndTargetValue (0.0f);
    energySmoother.setCurrentAndTargetValue (0.0f);
}

//==============================================================================
void MotionMod::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = (int) spec.maximumBlockSize;
    numChannels = static_cast<int> (spec.numChannels);
    
    // Reset smoothers with new sample rate
    flowSmoother.reset (sampleRate, 0.03f);
    energySmoother.reset (sampleRate, 0.03f);
    
    reset();
}

//==============================================================================
void MotionMod::reset()
{
    lfoPhasePan = 0.0f;
    lfoPhaseGain = 0.5f;  // 90° offset for anti-phase
    envelopeFollower = 0.0f;
    lfoFadeIn = 0.0f;  // Start with fade-in
}

//==============================================================================
void MotionMod::setFlow (float flow)
{
    flowParam = juce::jlimit (0.0f, 1.0f, flow);
    flowSmoother.setTargetValue (flowParam);
}

void MotionMod::setEnergy (float energy)
{
    energyParam = juce::jlimit (0.0f, 1.0f, energy);
    energySmoother.setTargetValue (energyParam);
}

//==============================================================================
void MotionMod::process (juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    
    if (numChannels < 2 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    flowSmoother.skip (numSamples);
    energySmoother.skip (numSamples);
    
    // Get current smoothed values
    auto flow = flowSmoother.getCurrentValue();
    auto energy = energySmoother.getCurrentValue();
    
    // Calculate LFO frequency with non-linear curve
    // Flow=0% → почти статично, Flow=100% → заметное дыхание (0.5 Hz = 2 sec cycle)
    // Energy влияет на минимальную частоту: даже при Flow=0% есть движение, если Energy>0%
    // Используем нелинейную кривую flow^1.8 для плавного ускорения
    auto flowCurved = std::pow (flow, LFO_CURVE);
    
    // Минимальная частота зависит от Energy: базовое значение + добавка от Energy
    // Energy=0% → MIN = 0.03 Hz (33 sec), Energy=100% → MIN = 0.05 Hz (20 sec)
    auto minLfoHz = BASE_MIN_LFO_HZ + ENERGY_MIN_LFO_HZ * energy;
    
    // Максимальная частота фиксирована (0.5 Hz = 2 sec cycle - заметно!)
    // Flow контролирует интерполяцию между min и max
    auto lfoFreq = minLfoHz + (MAX_LFO_HZ - minLfoHz) * flowCurved;
    auto lfoPhaseIncrement = TWO_PI * lfoFreq / sampleRate;
    
    // Energy controls amplitude (increased for testing/male voice)
    // Energy=0% → нет движения, Energy=100% → полная амплитуда
    auto panAmplitude = MAX_PAN_AMPLITUDE * energy;
    auto gainAmplitude = MAX_GAIN_AMPLITUDE * energy;
    
    // КРИТИЧНО: Если Energy = 0, то панорамы и громкости не должно быть вообще
    // Даже если Flow > 0, без Energy эффект не должен работать
    if (energy < 0.001f)
        return;  // Выключаем MotionMod полностью при Energy = 0
    
    // Skip processing only if both Flow and Energy are zero
    if (flow < 0.001f)
        return;  // Если Flow = 0, тоже выключаем
    
    // Process stereo channels
    auto* leftChannel = buffer.getWritePointer (0);
    auto* rightChannel = buffer.getWritePointer (1);
    
    // Update LFO fade-in (gradual ramp-up to prevent clicks)
    auto fadeInIncrement = static_cast<float> (numSamples) / (sampleRate * LFO_FADE_IN_TIME);
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Update envelope follower for transient detection
        // Use average of L+R channels to detect signal level
        auto signalLevel = std::abs (leftChannel[sample]) + std::abs (rightChannel[sample]);
        signalLevel *= 0.5f;  // Average
        
        // Envelope follower: fast attack, slow release
        if (signalLevel > envelopeFollower)
            envelopeFollower += (signalLevel - envelopeFollower) * ENVELOPE_ATTACK;
        else
            envelopeFollower += (signalLevel - envelopeFollower) * ENVELOPE_RELEASE;
        
        // Calculate LFO values
        auto panLFO = std::sin (lfoPhasePan) * panAmplitude;  // -1 to +1, scaled
        auto gainLFO = std::sin (lfoPhaseGain) * gainAmplitude;  // -1 to +1, scaled
        
        // Update LFO phases
        lfoPhasePan += lfoPhaseIncrement;
        lfoPhaseGain += lfoPhaseIncrement;
        
        // Wrap phases to [0, 2π)
        if (lfoPhasePan >= TWO_PI)
            lfoPhasePan -= TWO_PI;
        if (lfoPhaseGain >= TWO_PI)
            lfoPhaseGain -= TWO_PI;
        
        // Update LFO fade-in (prevents clicks on startup)
        if (lfoFadeIn < 1.0f)
        {
            lfoFadeIn += fadeInIncrement;
            if (lfoFadeIn > 1.0f)
                lfoFadeIn = 1.0f;
        }
        
        // Reduce modulation on transients (when envelope is high)
        // This prevents clicks on attacks
        auto transientFactor = 1.0f - std::min (envelopeFollower * 2.0f, 0.7f);  // Reduce up to 70% on attacks
        transientFactor = std::max (transientFactor, 0.3f);  // Keep at least 30% modulation
        
        // Apply fade-in and transient protection
        panLFO *= lfoFadeIn * transientFactor;
        gainLFO *= lfoFadeIn * transientFactor;
        
        // Apply panning (panLFO: -1 = left, +1 = right)
        // Convert to left/right gains with smoother curve
        // Updated for MAX_PAN_AMPLITUDE = 0.28 (±28%) - заметное, но не слишком агрессивное
        auto panAmount = panLFO;  // Already scaled to ±0.28 by MAX_PAN_AMPLITUDE
        auto leftGain = 1.0f - panAmount;   // 0.72 to 1.28 range
        auto rightGain = 1.0f + panAmount;  // 0.72 to 1.28 range
        // Clamp to prevent over-amplification
        leftGain = juce::jlimit (0.6f, 1.4f, leftGain);
        rightGain = juce::jlimit (0.6f, 1.4f, rightGain);
        
        // Apply gain modulation with smoother curve (use tanh for smoother transition)
        // This prevents clicks from sudden gain changes
        // Updated for MAX_GAIN_AMPLITUDE = 0.1 (±10%) - заметное дыхание громкости
        auto gainMod = 1.0f + gainLFO;  // Direct use: ±10% range (0.9-1.1)
        gainMod = std::tanh (gainMod * 2.2f) * 0.1f + 1.0f;  // Smooth curve, map to 0.9-1.1 range
        
        // Apply both pan and gain modulation
        leftChannel[sample] *= leftGain * gainMod;
        rightChannel[sample] *= rightGain * gainMod;
    }
}

