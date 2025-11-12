/*
  ==============================================================================

   HarmonicGlide - PsychoCore #2 для Platina
   Психоакустический кирпич: микро-сдвиг гармоник в ответ на динамику

  ==============================================================================
*/

#include "HarmonicGlide.h"

//==============================================================================
HarmonicGlide::HarmonicGlide()
{
    // Инициализация delay buffers
    delayBufferL.resize (MAX_DELAY_SAMPLES, 0.0f);
    delayBufferR.resize (MAX_DELAY_SAMPLES, 0.0f);
    
    // Инициализация smoothers
    energySmoother.reset (44100.0, 0.03f);
    flowSmoother.reset (44100.0, 0.03f);
    pitchShiftSmoother.reset (44100.0, PITCH_SHIFT_SMOOTH_TIME_MS / 1000.0f);
    
    energySmoother.setCurrentAndTargetValue (0.0f);
    flowSmoother.setCurrentAndTargetValue (0.0f);
    pitchShiftSmoother.setCurrentAndTargetValue (0.0f);
    
    delayWritePosL = 0;
    delayWritePosR = 0;
    rmsValue = 0.0f;
    rmsTarget = 0.0f;
    currentPitchShiftCents = 0.0f;
    targetPitchShiftCents = 0.0f;
    previousRMS = 0.0f;
    smoothedDelta = 0.0f;
}

//==============================================================================
void HarmonicGlide::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = (int) spec.maximumBlockSize;
    numChannels = static_cast<int> (spec.numChannels);
    
    // Пересоздаём delay buffers если нужно
    if (delayBufferL.size() < MAX_DELAY_SAMPLES)
    {
        delayBufferL.resize (MAX_DELAY_SAMPLES, 0.0f);
        delayBufferR.resize (MAX_DELAY_SAMPLES, 0.0f);
    }
    
    // Reset delay buffers
    std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0f);
    
    // Reset smoothers with new sample rate
    energySmoother.reset (sampleRate, 0.03f);
    flowSmoother.reset (sampleRate, 0.03f);
    pitchShiftSmoother.reset (sampleRate, PITCH_SHIFT_SMOOTH_TIME_MS / 1000.0f);
    
    reset();
}

//==============================================================================
void HarmonicGlide::reset()
{
    std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0f);
    
    delayWritePosL = 0;
    delayWritePosR = 0;
    
    rmsValue = 0.0f;
    rmsTarget = 0.0f;
    currentPitchShiftCents = 0.0f;
    targetPitchShiftCents = 0.0f;
    previousRMS = 0.0f;
    smoothedDelta = 0.0f;
    
    energySmoother.setCurrentAndTargetValue (energyParam);
    flowSmoother.setCurrentAndTargetValue (flowParam);
    pitchShiftSmoother.setCurrentAndTargetValue (0.0f);
}

//==============================================================================
void HarmonicGlide::setEnergy (float energy)
{
    energyParam = juce::jlimit (0.0f, 1.0f, energy);
    energySmoother.setTargetValue (energyParam);
}

void HarmonicGlide::setFlow (float flow)
{
    flowParam = juce::jlimit (0.0f, 1.0f, flow);
    flowSmoother.setTargetValue (flowParam);
}

//==============================================================================
float HarmonicGlide::calculateRMS (const juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    auto numCh = buffer.getNumChannels();
    
    if (numSamples == 0 || numCh == 0)
        return 0.0f;
    
    float sumSquared = 0.0f;
    int totalSamples = 0;
    
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* channelData = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            sumSquared += channelData[i] * channelData[i];
            totalSamples++;
        }
    }
    
    if (totalSamples == 0)
        return 0.0f;
    
    return std::sqrt (sumSquared / totalSamples);
}

//==============================================================================
float HarmonicGlide::centsToDelaySamples (float cents)
{
    // Конвертируем центы в коэффициент изменения частоты
    // cents = 1200 * log2(f_new / f_old)
    // f_new / f_old = 2^(cents/1200)
    
    if (std::abs (cents) < 0.01f)
        return 0.0f;
    
    // Для микро-сдвига используем упрощённый подход:
    // Положительные центы (вверх) = уменьшаем задержку (ускоряем)
    // Отрицательные центы (вниз) = увеличиваем задержку (замедляем)
    
    // Для микро-сдвига в 2-3 цента изменение задержки будет очень маленьким
    // Используем линейную аппроксимацию: 1 цент ≈ 0.000833 изменения частоты
    // Для питч-шифта через delay: delay_change ≈ -cents * period / 1200
    
    // Базовый период для средней частоты (например, 440 Гц)
    float baseFreq = 440.0f;
    float basePeriod = sampleRate / baseFreq;
    
    // Изменение задержки пропорционально центам
    // Для сдвига вверх (положительные центы) уменьшаем задержку
    float delayChange = -cents * basePeriod / 1200.0f;
    
    // Ограничиваем изменение задержки разумными пределами
    return juce::jlimit (-10.0f, 10.0f, delayChange);
}

//==============================================================================
float HarmonicGlide::getDelayedSample (const std::vector<float>& delayBuffer, int writePos, float delaySamples)
{
    // Вычисляем позицию чтения (назад от writePos)
    float readPos = writePos - delaySamples;
    
    // Обрабатываем wrap-around
    while (readPos < 0.0f)
        readPos += MAX_DELAY_SAMPLES;
    while (readPos >= MAX_DELAY_SAMPLES)
        readPos -= MAX_DELAY_SAMPLES;
    
    // Линейная интерполяция
    int readPosInt = static_cast<int> (readPos);
    float fraction = readPos - readPosInt;
    
    int readPosNext = (readPosInt + 1) % MAX_DELAY_SAMPLES;
    
    float sample1 = delayBuffer[readPosInt];
    float sample2 = delayBuffer[readPosNext];
    
    return sample1 + fraction * (sample2 - sample1);
}

//==============================================================================
void HarmonicGlide::process (juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    
    auto numSamples = buffer.getNumSamples();
    
    if (numChannels == 0 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    energySmoother.skip (numSamples);
    flowSmoother.skip (numSamples);
    pitchShiftSmoother.skip (numSamples);
    
    auto energy = energySmoother.getCurrentValue();
    auto flow = flowSmoother.getCurrentValue();
    
    // Если Energy = 0, эффект выключен
    if (energy < 0.001f)
    {
        // Просто пропускаем сигнал без изменений
        return;
    }
    
    // Вычисляем RMS текущего блока
    float blockRMS = calculateRMS (buffer);
    
    // Обновляем RMS-follower с медленным релизом
    // Attack: быстро (50 мс), Release: медленно (450 мс) - основное время реакции
    float attackCoeff = std::exp (-1.0f / (RMS_ATTACK_TIME_MS * 0.001f * sampleRate));
    float releaseCoeff = std::exp (-1.0f / (RMS_RELEASE_TIME_MS * 0.001f * sampleRate));
    
    if (blockRMS > rmsTarget)
    {
        // Attack: быстрое отслеживание роста
        rmsTarget = blockRMS;
        rmsValue = rmsTarget + (rmsValue - rmsTarget) * attackCoeff;
    }
    else
    {
        // Release: медленное отслеживание спада
        rmsTarget = blockRMS;
        rmsValue = rmsTarget + (rmsValue - rmsTarget) * releaseCoeff;
    }
    
    // Нормализуем RMS (0.0 - 1.0) для вычисления питч-шифта
    // Используем логарифмическую шкалу для более естественного восприятия
    float normalizedRMS = rmsValue;
    if (normalizedRMS > 0.0001f)
    {
        normalizedRMS = std::log10 (normalizedRMS * 1000.0f + 1.0f) / std::log10 (1001.0f);
    }
    else
    {
        normalizedRMS = 0.0f;
    }
    
    // Вычисляем целевой питч-шифт на основе RMS и параметра Energy
    // При росте RMS → положительный сдвиг (вверх)
    // При спаде RMS → отрицательный сдвиг (вниз)
    // Energy управляет чувствительностью (амплитудой сдвига)
    
    // Базовый сдвиг: от -MIN до +MAX центов в зависимости от RMS
    // Используем центрированную RMS (относительно предыдущего значения)
    float rmsDelta = normalizedRMS - previousRMS;
    previousRMS = normalizedRMS;
    
    // Сглаживаем delta для плавности
    float deltaSmoothCoeff = std::exp (-1.0f / (RMS_RELEASE_TIME_MS * 0.001f * sampleRate));
    smoothedDelta = rmsDelta + (smoothedDelta - rmsDelta) * deltaSmoothCoeff;
    
    // Вычисляем целевой питч-шифт: положительный при росте, отрицательный при спаде
    float shiftRange = MIN_SHIFT_CENTS + (MAX_SHIFT_CENTS - MIN_SHIFT_CENTS) * energy;
    targetPitchShiftCents = smoothedDelta * shiftRange * 100.0f;  // Усиливаем чувствительность
    
    // Ограничиваем целевой сдвиг
    targetPitchShiftCents = juce::jlimit (-MAX_SHIFT_CENTS, MAX_SHIFT_CENTS, targetPitchShiftCents);
    
    // Flow управляет скоростью реакции (влияет на сглаживание питч-шифта)
    float smoothTime = PITCH_SHIFT_SMOOTH_TIME_MS * (1.0f - flow * 0.7f);  // При Flow=1.0 быстрее на 30%
    pitchShiftSmoother.reset (sampleRate, smoothTime / 1000.0f);
    pitchShiftSmoother.setTargetValue (targetPitchShiftCents);
    
    // Получаем текущий сглаженный питч-шифт
    currentPitchShiftCents = pitchShiftSmoother.getCurrentValue();
    
    // Конвертируем центы в задержку в семплах
    float delaySamples = centsToDelaySamples (currentPitchShiftCents);
    
    // Обрабатываем каждый канал
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        auto& delayBuffer = (ch == 0) ? delayBufferL : delayBufferR;
        int& writePos = (ch == 0) ? delayWritePosL : delayWritePosR;
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Читаем задержанный семпл (для питч-шифта)
            float delayedSample = getDelayedSample (delayBuffer, writePos, delaySamples);
            
            // Записываем текущий семпл в delay buffer
            delayBuffer[writePos] = channelData[i];
            
            // Применяем питч-шифт: смешиваем оригинал с задержанной версией
            // Для микро-сдвига используем лёгкое смешивание
            float mixAmount = std::abs (currentPitchShiftCents) / MAX_SHIFT_CENTS * 0.3f;  // Макс 30% смешивания
            channelData[i] = channelData[i] * (1.0f - mixAmount) + delayedSample * mixAmount;
            
            // Обновляем позицию записи
            writePos = (writePos + 1) % MAX_DELAY_SAMPLES;
        }
    }
}

