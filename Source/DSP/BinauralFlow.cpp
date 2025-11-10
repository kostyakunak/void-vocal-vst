/*
  ==============================================================================

   BinauralFlow - Психоакустический кирпич для Iceberg
   Реализация с учётом психоакустики для максимально стильного эффекта

  ==============================================================================
*/

#include "BinauralFlow.h"
#include <algorithm>

//==============================================================================
BinauralFlow::BinauralFlow()
    : randomGenerator (std::random_device{}()),
      jitterDistribution (-MAX_JITTER_MS, MAX_JITTER_MS)
{
    // Initialize delay buffers
    delayBufferL.setSize (1, MAX_DELAY_SAMPLES);
    delayBufferR.setSize (1, MAX_DELAY_SAMPLES);
    delayBufferL.clear();
    delayBufferR.clear();
    
    // Initialize smoothers (30ms smoothing для плавности)
    flowSmoother.reset (44100.0, 0.03f);
    depthSmoother.reset (44100.0, 0.03f);
    ghostSmoother.reset (44100.0, 0.03f);
    
    flowSmoother.setCurrentAndTargetValue (0.0f);
    depthSmoother.setCurrentAndTargetValue (0.0f);
    ghostSmoother.setCurrentAndTargetValue (0.0f);
    
    // Initialize random jitter
    updateRandomJitter();
}

//==============================================================================
void BinauralFlow::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = (int) spec.maximumBlockSize;
    numChannels = static_cast<int> (spec.numChannels);
    
    // Prepare high-pass filters для фазовой модуляции (только верха)
    // Используем два фильтра (L и R) для независимой обработки
    auto highPassCoeffs = Coeffs::makeHighPass (
        sampleRate, HIGH_PASS_FREQ, HIGH_PASS_Q);
    *highPassChain.get<0>().state = *highPassCoeffs;
    *highPassChain.get<1>().state = *highPassCoeffs;
    
    highPassChain.prepare (spec);
    
    // Reset delay buffers
    delayBufferL.clear();
    delayBufferR.clear();
    writePosL = 0;
    writePosR = 0;
    
    // Reset smoothers with new sample rate
    flowSmoother.reset (sampleRate, 0.03f);
    depthSmoother.reset (sampleRate, 0.03f);
    ghostSmoother.reset (sampleRate, 0.03f);
    
    reset();
}

//==============================================================================
void BinauralFlow::reset()
{
    delayBufferL.clear();
    delayBufferR.clear();
    writePosL = 0;
    writePosR = 0;
    
    lfoPhase = 0.0f;
    phaseModL = 0.0f;
    phaseModR = 0.0f;
    phaseModPhase = 0.0f;
    
    highPassChain.reset();
    
    flowSmoother.setCurrentAndTargetValue (0.0f);
    depthSmoother.setCurrentAndTargetValue (0.0f);
    ghostSmoother.setCurrentAndTargetValue (0.0f);
    
    updateRandomJitter();
    jitterUpdateCounter = 0.0f;
}

//==============================================================================
void BinauralFlow::setFlow (float flow)
{
    flowParam = juce::jlimit (0.0f, 1.0f, flow);
    flowSmoother.setTargetValue (flowParam);
}

void BinauralFlow::setDepth (float depth)
{
    depthParam = juce::jlimit (0.0f, 1.0f, depth);
    depthSmoother.setTargetValue (depthParam);
}

void BinauralFlow::setGhost (float ghost)
{
    ghostParam = juce::jlimit (0.0f, 1.0f, ghost);
    ghostSmoother.setTargetValue (ghostParam);
}

//==============================================================================
float BinauralFlow::getDelayedSample (float* delayBuffer, int writePos, float delaySamples, int bufferSize)
{
    // КРИТИЧНО: writePos указывает на позицию, куда мы ЗАПИШЕМ следующий семпл
    // Значит, последний записанный семпл находится в (writePos - 1 + bufferSize) % bufferSize
    // Задержанный семпл находится на delaySamples назад от последнего записанного
    
    // Вычисляем позицию чтения: от последнего записанного семпла отнимаем задержку
    float readPos = static_cast<float> (writePos) - 1.0f - delaySamples;
    
    // Обработка wrap-around (правильная нормализация)
    while (readPos < 0.0f)
        readPos += static_cast<float> (bufferSize);
    while (readPos >= static_cast<float> (bufferSize))
        readPos -= static_cast<float> (bufferSize);
    
    // Линейная интерполяция для fractional delay
    int readPosInt = static_cast<int> (std::floor (readPos));
    float fraction = readPos - static_cast<float> (readPosInt);
    
    // Нормализуем индексы для wrap-around
    readPosInt = (readPosInt + bufferSize) % bufferSize;
    int readPosNext = (readPosInt + 1) % bufferSize;
    
    // Безопасное чтение с проверкой границ
    float sample1 = delayBuffer[readPosInt];
    float sample2 = delayBuffer[readPosNext];
    
    // Линейная интерполяция
    return sample1 + (sample2 - sample1) * fraction;
}

//==============================================================================
void BinauralFlow::applyPhaseModulation (float* leftChannel, float* rightChannel, int numSamples)
{
    // Фазовая модуляция применяется только к верхам (5-12 кГц)
    // Создаём временные буферы для фильтрованных верхов
    juce::AudioBuffer<float> tempBuffer (2, numSamples);
    tempBuffer.clear();
    
    // Копируем входной сигнал
    for (int i = 0; i < numSamples; ++i)
    {
        tempBuffer.setSample (0, i, leftChannel[i]);
        tempBuffer.setSample (1, i, rightChannel[i]);
    }
    
    // Применяем high-pass фильтр (только верха проходят)
    juce::dsp::AudioBlock<float> block (tempBuffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    highPassChain.process (context);
    
    // Получаем отфильтрованные верха
    auto* filteredL = tempBuffer.getReadPointer (0);
    auto* filteredR = tempBuffer.getReadPointer (1);
    
    // Применяем фазовую модуляцию к верхам
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Вычисляем фазовый сдвиг (синусоидальная модуляция)
        float phaseShiftL = std::sin (phaseModPhase) * phaseModL;
        float phaseShiftR = std::sin (phaseModPhase + juce::MathConstants<float>::pi) * phaseModR;  // Противофазно
        
        // Применяем фазовый сдвиг через all-pass (упрощённо: через задержку)
        // Для малых фазовых сдвигов используем приближение через задержку
        // Фаза в градусах → задержка в семплах
        float delayL = phaseShiftL * sampleRate / (360.0f * 1000.0f);  // Конвертация градусов → мс → семплы
        float delayR = phaseShiftR * sampleRate / (360.0f * 1000.0f);
        
        // Простое приближение: смешиваем с задержанной версией
        // Для малых фазовых сдвигов это работает достаточно хорошо
        int delaySamplesL = static_cast<int> (std::round (delayL));
        int delaySamplesR = static_cast<int> (std::round (delayR));
        
        delaySamplesL = juce::jlimit (-2, 2, delaySamplesL);  // Ограничиваем диапазон
        delaySamplesR = juce::jlimit (-2, 2, delaySamplesR);
        
        // Применяем к верхам (смешиваем с задержанной версией)
        if (delaySamplesL != 0 && sample >= std::abs (delaySamplesL))
        {
            float delayedL = filteredL[sample - delaySamplesL];
            leftChannel[sample] = leftChannel[sample] - filteredL[sample] + delayedL * 0.3f;  // 30% смешивание
        }
        
        if (delaySamplesR != 0 && sample >= std::abs (delaySamplesR))
        {
            float delayedR = filteredR[sample - delaySamplesR];
            rightChannel[sample] = rightChannel[sample] - filteredR[sample] + delayedR * 0.3f;
        }
        
        // Обновляем фазу фазовой модуляции
        phaseModPhase += TWO_PI * PHASE_MOD_FREQ_HZ / sampleRate;
        if (phaseModPhase >= TWO_PI)
            phaseModPhase -= TWO_PI;
    }
}

//==============================================================================
void BinauralFlow::updateRandomJitter()
{
    // Обновляем случайный джиттер для естественности
    randomJitterL = jitterDistribution (randomGenerator);
    randomJitterR = jitterDistribution (randomGenerator);
}

//==============================================================================
void BinauralFlow::process (juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    
    if (numChannels < 2 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    flowSmoother.skip (numSamples);
    depthSmoother.skip (numSamples);
    ghostSmoother.skip (numSamples);
    
    // Get current smoothed values
    auto flow = flowSmoother.getCurrentValue();
    auto depth = depthSmoother.getCurrentValue();
    auto ghost = ghostSmoother.getCurrentValue();
    
    // Если Flow = 0, эффект выключен (pass-through)
    if (flow < 0.001f)
        return;
    
    auto* leftChannel = buffer.getWritePointer (0);
    auto* rightChannel = buffer.getWritePointer (1);
    auto* delayL = delayBufferL.getWritePointer (0);
    auto* delayR = delayBufferR.getWritePointer (0);
    
    // Вычисляем параметры LFO
    // Flow управляет частотой LFO: 0.03-0.08 Гц (очень медленно!)
    // Нелинейная кривая для более заметного эффекта на больших значениях
    auto flowCurved = std::pow (flow, 1.5f);
    auto lfoFreq = MIN_LFO_HZ + (MAX_LFO_HZ - MIN_LFO_HZ) * flowCurved;
    auto lfoPhaseIncrement = TWO_PI * lfoFreq / sampleRate;
    
    // Depth управляет амплитудой задержки
    // Нелинейная кривая для более заметного эффекта
    auto depthCurved = std::pow (depth, 1.3f);
    auto delayAmplitudeMs = MIN_DELAY_MS + (MAX_DELAY_MS - MIN_DELAY_MS) * depthCurved;
    
    // Ghost управляет фазовой модуляцией
    auto ghostCurved = std::pow (ghost, 1.2f);
    auto phaseModAmplitudeDeg = PHASE_MOD_DEG_MIN + (PHASE_MOD_DEG_MAX - PHASE_MOD_DEG_MIN) * ghostCurved;
    
    // Обновляем амплитуды фазовой модуляции
    phaseModL = phaseModAmplitudeDeg;
    phaseModR = phaseModAmplitudeDeg;
    
    // Обновляем случайный джиттер (раз в несколько секунд)
    jitterUpdateCounter += static_cast<float> (numSamples) / sampleRate;
    if (jitterUpdateCounter >= JITTER_UPDATE_SEC)
    {
        updateRandomJitter();
        jitterUpdateCounter = 0.0f;
    }
    
    // КРИТИЧНО: Убираем задержки времени (ITD) - они создают панораму
    // Используем ТОЛЬКО фазовую модуляцию (IPD) через all-pass фильтры
    // Это единственный способ создать движение БЕЗ панорамы
    
    // Обновляем фазу LFO для фазовой модуляции
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Обновляем фазу LFO
        lfoPhase += lfoPhaseIncrement;
        if (lfoPhase >= TWO_PI)
            lfoPhase -= TWO_PI;
    }
    
    // КРИТИЧНО: Применяем фазовую модуляцию ко ВСЕМУ сигналу (не только верхам)
    // Это создает движение БЕЗ панорамы
    // Flow управляет интенсивностью фазовой модуляции
    if (flow > 0.001f)
    {
        // Вычисляем фазовый сдвиг через LFO
        float lfoValue = std::sin (lfoPhase);
        
        // Depth управляет амплитудой фазового сдвига
        // Конвертируем "задержку" в фазовый сдвиг (эквивалент для определенной частоты)
        // Для 5 кГц: 0.3 мс = 540 градусов фазы, 0.6 мс = 1080 градусов (эквивалент)
        float phaseShiftAmplitudeDeg = delayAmplitudeMs * 1800.0f;  // Примерная конвертация мс → градусы (для 5 кГц)
        phaseShiftAmplitudeDeg = juce::jlimit (5.0f, 10.0f, phaseShiftAmplitudeDeg);  // Ограничиваем 5-10 градусов
        
        // Модулируем фазовый сдвиг через LFO
        float phaseShiftL = lfoValue * phaseShiftAmplitudeDeg;
        float phaseShiftR = -lfoValue * phaseShiftAmplitudeDeg;  // Противофазно
        
        // Применяем фазовый сдвиг через упрощенный all-pass (через задержку на 1-2 семпла)
        // Это создает фазовый сдвиг без заметной задержки времени
        int delaySamplesL = static_cast<int> (std::round (phaseShiftL / 180.0f));  // Очень маленькая задержка
        int delaySamplesR = static_cast<int> (std::round (phaseShiftR / 180.0f));
        
        delaySamplesL = juce::jlimit (-1, 1, delaySamplesL);  // Максимум ±1 семпл
        delaySamplesR = juce::jlimit (-1, 1, delaySamplesR);
        
        // Применяем к сигналу (смешиваем с задержанной версией для фазового сдвига)
        for (int sample = 1; sample < numSamples - 1; ++sample)
        {
            if (delaySamplesL != 0)
            {
                float delayedL = leftChannel[sample - delaySamplesL];
                leftChannel[sample] = leftChannel[sample] * 0.7f + delayedL * 0.3f;  // Смешивание для фазового сдвига
            }
            
            if (delaySamplesR != 0)
            {
                float delayedR = rightChannel[sample - delaySamplesR];
                rightChannel[sample] = rightChannel[sample] * 0.7f + delayedR * 0.3f;
            }
        }
    }
    
    // Дополнительная фазовая модуляция на верхах (если Ghost > 0)
    if (ghost > 0.001f)
    {
        applyPhaseModulation (leftChannel, rightChannel, numSamples);
    }
}

