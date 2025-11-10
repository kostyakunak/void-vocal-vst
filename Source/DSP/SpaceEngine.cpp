/*
  ==============================================================================

   SpaceEngine - Reverb/Space модуль
   Этап 1: Реализация с оптимизацией для мужского вокала

  ==============================================================================
*/

#include "SpaceEngine.h"
#include <cmath>

//==============================================================================
SpaceEngine::SpaceEngine()
{
    // Initialize pre-delay buffers (max size for 48kHz)
    auto maxDelaySamples = static_cast<int> (MAX_PREDELAY_MS * 0.001 * 48000.0);
    predelayBufferL.setSize (1, maxDelaySamples);
    predelayBufferR.setSize (1, maxDelaySamples);
    predelayBufferL.clear();
    predelayBufferR.clear();
    // Initialize reverb parameters for male vocal
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.5f;
    reverbParams.wetLevel = 0.33f;
    reverbParams.dryLevel = 0.4f;
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0.0f;
    
    // Initialize smoothers (30ms smoothing)
    depthSmoother.reset (44100.0, 0.03f);
    flowSmoother.reset (44100.0, 0.03f);
    ghostSmoother.reset (44100.0, 0.03f);
    
    depthSmoother.setCurrentAndTargetValue (0.0f);
    flowSmoother.setCurrentAndTargetValue (0.0f);
    ghostSmoother.setCurrentAndTargetValue (0.0f);
    
    // Initialize parameters to zero (no reverb effect)
    depthParam = 0.0f;
    flowParam = 0.0f;
    ghostParam = 0.0f;
    updateParameters();  // This will set wetLevel=0, roomSize=0.1
}

//==============================================================================
void SpaceEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = (int) spec.maximumBlockSize;
    numChannels = static_cast<int> (spec.numChannels);
    
    // Prepare reverb
    reverb.prepare (spec);
    
    // Prepare pre-delay buffers (enough for max pre-delay)
    auto maxDelaySamples = static_cast<int> (MAX_PREDELAY_MS * 0.001 * sampleRate);
    predelayBufferL.setSize (1, maxDelaySamples);
    predelayBufferR.setSize (1, maxDelaySamples);
    predelayBufferL.clear();
    predelayBufferR.clear();
    predelayWritePosL = 0;
    predelayWritePosR = 0;
    predelayReadPosL = 0;
    predelayReadPosR = 0;
    
    // Reset smoothers with new sample rate
    depthSmoother.reset (sampleRate, 0.03f);
    flowSmoother.reset (sampleRate, 0.03f);
    ghostSmoother.reset (sampleRate, 0.03f);
    
    reset();
}

//==============================================================================
void SpaceEngine::reset()
{
    reverb.reset();
    predelayBufferL.clear();
    predelayBufferR.clear();
    predelayWritePosL = 0;
    predelayWritePosR = 0;
    predelayReadPosL = 0;
    predelayReadPosR = 0;
}

//==============================================================================
void SpaceEngine::setDepth (float depth)
{
    depthParam = juce::jlimit (0.0f, 1.0f, depth);
    updateParameters();
}

void SpaceEngine::setFlow (float flow)
{
    flowParam = juce::jlimit (0.0f, 1.0f, flow);
    updateParameters();
}

void SpaceEngine::setGhost (float ghost)
{
    ghostParam = juce::jlimit (0.0f, 1.0f, ghost);
    updateParameters();
}

//==============================================================================
void SpaceEngine::updateParameters()
{
    // Update smoothed values
    depthSmoother.setTargetValue (depthParam);
    flowSmoother.setTargetValue (flowParam);
    ghostSmoother.setTargetValue (ghostParam);
    
    // Depth controls: decay time, pre-delay, room size
    // Нелинейная кривая для более заметных изменений на больших значениях
    // Depth=0% → маленькая комната, Depth=100% → бездна
    // ВАЖНО: Depth работает только если Ghost > 0 (реверб включен)
    auto depth = depthSmoother.getCurrentValue();
    auto depthCurved = std::pow (depth, 1.3f);  // Менее агрессивная кривая для более заметных изменений
    auto predelayMs = MIN_PREDELAY_MS + (MAX_PREDELAY_MS - MIN_PREDELAY_MS) * depthCurved;
    // Room size: 0.1 (маленькая комната) → 0.95 (огромная) для более заметной разницы
    // Сделано более заметным: 0.1 вместо 0.05 для лучшего контраста
    auto roomSize = 0.1f + 0.85f * depthCurved;  // 0.1 to 0.95 (более заметный диапазон)
    
    // Flow controls: movement (LFO will be in MotionMod, here we adjust width and damping)
    // Нелинейная кривая для более заметного эффекта на больших значениях
    auto flow = flowSmoother.getCurrentValue();
    auto flowCurved = std::pow (flow, 1.4f);  // Плавное ускорение эффекта
    stereoWidth = 0.25f + 0.75f * flowCurved;  // 0.25 to 1.0 (более заметное изменение ширины)
    // Also reduce damping with flow for more movement
    
    // Ghost controls: wet level (reflections density)
    // When ghost=0, wetLevel should be 0 (no reverb effect)
    // When ghost>0, make it full wet (1.0) so we can do dry/wet mix in processor
    // IMPORTANT: We want 100% wet from reverb, then we do dry/wet mix ourselves in processor
    auto ghost = ghostSmoother.getCurrentValue();
    auto wetLevel = ghost;  // 0.0 to 1.0 (completely off when ghost=0, full wet when ghost=1)
    auto dryLevel = 0.0f;  // Always 0 - we do dry/wet mix in processor, not here
    
    // Damping: less for male voice (lower frequencies need less HF damping)
    // Also affected by Flow for more movement (используем flowCurved для согласованности)
    // Male vocal optimized: 0.2-0.7 (wider range, more noticeable)
    auto baseDamping = MIN_DAMPING + (MAX_DAMPING - MIN_DAMPING) * (1.0f - depthCurved * 0.5f);
    auto damping = baseDamping * (1.0f - flowCurved * 0.3f);  // Reduce damping with flow for more brightness
    damping = juce::jlimit (MIN_DAMPING, MAX_DAMPING, damping);
    
    // Update reverb parameters
    reverbParams.roomSize = roomSize;
    reverbParams.damping = damping;
    reverbParams.wetLevel = wetLevel;
    reverbParams.dryLevel = dryLevel;
    reverbParams.width = stereoWidth;
    
    // Note: pre-delay is applied manually in process() using predelayMs
    // predelayMs is calculated above and used in process() per-sample
    
    reverb.setParameters (reverbParams);
}

//==============================================================================
void SpaceEngine::process (juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    
    if (numChannels < 2 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    depthSmoother.skip (numSamples);
    flowSmoother.skip (numSamples);
    ghostSmoother.skip (numSamples);
    
    // Get current smoothed values
    auto currentDepth = depthSmoother.getCurrentValue();
    auto currentFlow = flowSmoother.getCurrentValue();
    auto currentGhost = ghostSmoother.getCurrentValue();
    
    // If Ghost is zero (no reverb), skip processing entirely (pass through)
    // Depth alone doesn't enable reverb - Ghost controls wet level
    if (currentGhost < 0.001f)
        return;
    
    // Update reverb parameters if they changed significantly
    // (JUCE reverb doesn't support per-sample updates, so we update once per block)
    if (std::abs (currentDepth - depthParam) > 0.001f ||
        std::abs (currentFlow - flowParam) > 0.001f ||
        std::abs (currentGhost - ghostParam) > 0.001f)
    {
        depthParam = currentDepth;
        flowParam = currentFlow;
        ghostParam = currentGhost;
        updateParameters();
    }
    
    // Use curved depth for pre-delay calculation (consistent with updateParameters)
    auto depthCurved = std::pow (currentDepth, 1.5f);
    auto predelayMs = MIN_PREDELAY_MS + (MAX_PREDELAY_MS - MIN_PREDELAY_MS) * depthCurved;
    auto predelaySamplesInt = static_cast<int> (predelayMs * 0.001 * sampleRate);
    
    // Apply pre-delay to preserve male vocal clarity
    auto maxDelay = predelayBufferL.getNumSamples();
    
    if (predelaySamplesInt > 0 && predelaySamplesInt < maxDelay && numChannels >= 2)
    {
        auto* leftChannel = buffer.getWritePointer (0);
        auto* rightChannel = buffer.getWritePointer (1);
        auto* delayL = predelayBufferL.getWritePointer (0);
        auto* delayR = predelayBufferR.getWritePointer (0);
        
        // Update read position based on delay
        // Read position is (writePos - delaySamples) mod maxDelay
        predelayReadPosL = (predelayWritePosL - predelaySamplesInt + maxDelay) % maxDelay;
        predelayReadPosR = (predelayWritePosR - predelaySamplesInt + maxDelay) % maxDelay;
        
        // Simple circular buffer pre-delay
        // IMPORTANT: We need to write first, then read, to avoid reading zeros on first pass
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Save current sample before writing
            auto currentL = leftChannel[sample];
            auto currentR = rightChannel[sample];
            
            // Write current samples to delay buffer FIRST
            delayL[predelayWritePosL] = currentL;
            delayR[predelayWritePosR] = currentR;
            
            // Update write positions
            predelayWritePosL = (predelayWritePosL + 1) % maxDelay;
            predelayWritePosR = (predelayWritePosR + 1) % maxDelay;
            
            // Read delayed samples (from previous write)
            auto delayedL = delayL[predelayReadPosL];
            auto delayedR = delayR[predelayReadPosR];
            
            // Update read positions
            predelayReadPosL = (predelayReadPosL + 1) % maxDelay;
            predelayReadPosR = (predelayReadPosR + 1) % maxDelay;
            
            // Apply delayed signal (replace current with delayed)
            leftChannel[sample] = delayedL;
            rightChannel[sample] = delayedR;
        }
    }
    
    // Process through reverb
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}

