/*
  ==============================================================================

   SpectralEngine - Spectral processing для Iceberg
   Этап 1: Настоящий формант-шифт через спектральное перемаппирование

  ==============================================================================
*/

#include "SpectralEngine.h"

//==============================================================================
SpectralEngine::SpectralEngine()
    // ВРЕМЕННО: FFT отключен (вызывает зависание)
    // : fft (std::make_unique<juce::dsp::FFT> (static_cast<int> (std::log2 (fftSize))))
{
    // ВРЕМЕННО: FFT буферы отключены (вызывают зависание)
    // fftBuffer.resize (fftSize * 2, 0.0f);
    // windowBuffer.resize (fftSize, 0.0f);
    // inputBuffer.resize (fftSize, 0.0f);
    // outputBuffer.resize (fftSize, 0.0f);
    // overlapBuffer.resize (overlapSize, 0.0f);
    
    // Create Hann window для плавного перекрытия
    // for (int i = 0; i < fftSize; ++i)
    // {
    //     windowBuffer[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
    // }
    
    // Initialize smoothers (30ms smoothing)
    claritySmoother.reset (44100.0, 0.03f);
    depthSmoother.reset (44100.0, 0.03f);
    flowSmoother.reset (44100.0, 0.03f);
    
    claritySmoother.setCurrentAndTargetValue (0.0f);
    depthSmoother.setCurrentAndTargetValue (0.0f);
    flowSmoother.setCurrentAndTargetValue (0.0f);
    
    inputBufferPos = 0;
    outputBufferPos = 0;
}

//==============================================================================
void SpectralEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = (int) spec.maximumBlockSize;
    numChannels = static_cast<int> (spec.numChannels);
    
    // Prepare EQ chain - КРИТИЧНО: prepare должен вызываться перед reset
    // Это гарантирует, что фильтры инициализируются правильно для стерео
    eqChain.prepare (spec);
    
    // Убеждаемся, что все фильтры в цепочке сброшены и имеют одинаковое состояние
    eqChain.reset();
    
    // ВРЕМЕННО: FFT буферы отключены
    // std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    // std::fill (outputBuffer.begin(), outputBuffer.end(), 0.0f);
    // std::fill (overlapBuffer.begin(), overlapBuffer.end(), 0.0f);
    inputBufferPos = 0;
    outputBufferPos = 0;
    
    // Reset smoothers with new sample rate
    claritySmoother.reset (sampleRate, 0.03f);
    depthSmoother.reset (sampleRate, 0.03f);
    flowSmoother.reset (sampleRate, 0.03f);
    
    reset();
}

//==============================================================================
void SpectralEngine::reset()
{
    eqChain.reset();
    claritySmoother.setCurrentAndTargetValue (0.0f);
    depthSmoother.setCurrentAndTargetValue (0.0f);
    flowSmoother.setCurrentAndTargetValue (0.0f);
    formantLfoPhase = 0.5f;
    
    // ВРЕМЕННО: FFT буферы отключены
    // std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    // std::fill (outputBuffer.begin(), outputBuffer.end(), 0.0f);
    // std::fill (overlapBuffer.begin(), overlapBuffer.end(), 0.0f);
    // std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
    inputBufferPos = 0;
    outputBufferPos = 0;
    
    updateFilters();
}

//==============================================================================
void SpectralEngine::setClarity (float clarity)
{
    auto newClarity = juce::jlimit (-0.5f, 0.5f, clarity);
    if (std::abs (newClarity - clarityParam) > 0.0001f)
    {
        clarityParam = newClarity;
        claritySmoother.setTargetValue (clarityParam);
        updateFilters();  // Обновляем фильтры сразу при изменении параметра
    }
}

void SpectralEngine::setDepth (float depth)
{
    depthParam = juce::jlimit (0.0f, 1.0f, depth);
}

void SpectralEngine::setFlow (float flow)
{
    flowParam = juce::jlimit (0.0f, 1.0f, flow);
    flowSmoother.setTargetValue (flowParam);
}

//==============================================================================
void SpectralEngine::updateFilters()
{
    // Используем прямое значение параметра (не smoothed), чтобы фильтры обновлялись сразу
    // Smoother нужен только для плавности, но для обновления фильтров используем актуальное значение
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    auto clarity = clarityParam;
    
    // Clarity: "Хрустальный блеск" vs "Мутный лёд" - ПРАВИЛЬНЫЙ ПОДХОД
    // Проблема была: слишком агрессивный boost усиливал шумы, а не гармоники
    // Решение: более тонкий, музыкальный подход
    
    auto clarityCurved = clarity * 2.0f;  // -1.0 to +1.0
    
    // High-shelf: УМЕРЕННЫЙ boost для "воздуха" (не шум!)
    // При +50%: +7 дБ @ 8 кГц - заметнее, но не шумно
    // При -50%: -7 дБ @ 8 кГц - "мутный лёд"
    auto airGainDb = clarityCurved * 7.0f;  // ±7 дБ (+20% от ±6)
    auto airGainLinear = juce::Decibels::decibelsToGain (airGainDb);
    
    // High-shelf filter (воздух) - для верхов
    // Более широкий Q (0.7) для плавности и минимальных фазовых искажений
    auto highShelfCoeffs = Coeffs::makeHighShelf (
        sampleRate, HIGH_SHELF_FREQ, 0.7f, airGainLinear);  // Q=0.7 для баланса плавности и фаз
    *eqChain.get<0>().state = *highShelfCoeffs;
    
    // Формант-сдвиг через резонансные фильтры (F1, F2, F3) - УМЕРЕННЫЙ
    // При -50%: форманты сдвигаются вниз (мутный лёд)
    // При +50%: форманты сдвигаются вверх (хрустальный блеск)
    auto formantShiftRatio = 1.0f + clarityCurved * 0.35f;  // ±35% сдвиг (+20% от ±30%)
    
    // F1: 200-800 Hz (базовый формант) - ЛЕГКИЙ ЭФФЕКТ
    // УМЕНЬШЕН Q для минимизации фазовых искажений (меньше стерео-смещения)
    auto f1Center = (FORMANT_F1_MIN + FORMANT_F2_MIN) / 2.0f;  // ~500 Hz
    auto f1Shifted = f1Center * formantShiftRatio;
    auto f1Gain = clarityCurved > 0.0f 
        ? 1.0f + clarityCurved * 0.35f  // При +50%: +35% boost
        : 1.0f - std::abs(clarityCurved) * 0.35f;  // При -50%: -35%
    auto f1Coeffs = Coeffs::makePeakFilter (
        sampleRate, f1Shifted, 1.2f, f1Gain);  // Q=1.2 (было 2.0) - менее фазовых искажений
    *eqChain.get<2>().state = *f1Coeffs;
    
    // F2: 800-3000 Hz (основной формант речи) - СРЕДНИЙ ЭФФЕКТ
    auto f2Center = (FORMANT_F2_MIN + FORMANT_F2_MAX) / 2.0f;  // ~1900 Hz
    auto f2Shifted = f2Center * formantShiftRatio;
    auto f2Gain = clarityCurved > 0.0f 
        ? 1.0f + clarityCurved * 0.45f  // При +50%: +45% boost
        : 1.0f - std::abs(clarityCurved) * 0.45f;  // При -50%: -45%
    auto f2Coeffs = Coeffs::makePeakFilter (
        sampleRate, f2Shifted, 1.2f, f2Gain);  // Q=1.2 (было 1.8) - менее фазовых искажений
    *eqChain.get<3>().state = *f2Coeffs;
    
    // F3: 2000-4000 Hz (высокий формант, "блеск") - КЛЮЧЕВОЙ, НО УМЕРЕННЫЙ
    auto f3Center = (FORMANT_F3_MIN + FORMANT_F3_MAX) / 2.0f;  // ~3000 Hz
    auto f3Shifted = f3Center * formantShiftRatio;
    auto f3Gain = clarityCurved > 0.0f 
        ? 1.0f + clarityCurved * 0.55f  // При +50%: +55% boost (~+4 дБ)
        : 1.0f - std::abs(clarityCurved) * 0.55f;  // При -50%: -55% (~-4 дБ)
    auto f3Coeffs = Coeffs::makePeakFilter (
        sampleRate, f3Shifted, 1.0f, f3Gain);  // Q=1.0 (было 1.5) - минимальные фазовые искажения
    *eqChain.get<4>().state = *f3Coeffs;
    
    // Low-mid bell filter (Depth - для "темноты" подо льдом)
    auto depth = depthSmoother.getCurrentValue();
    if (depth < 0.1f)
    {
        auto depthCurved = std::pow (depth * 10.0f, 1.3f);  // Scale для малых значений
        auto lowMidGainDb = depthCurved * MAX_LOW_MID_BOOST;
        auto lowMidGainLinear = juce::Decibels::decibelsToGain (lowMidGainDb);
        auto lowMidCoeffs = Coeffs::makePeakFilter (
            sampleRate, LOW_MID_FREQ, LOW_MID_Q, lowMidGainLinear);
        *eqChain.get<1>().state = *lowMidCoeffs;
    }
    else
    {
        // Когда Depth большой, отключаем low-mid EQ (глубина создаётся через реверб)
        auto lowMidCoeffs = Coeffs::makePeakFilter (
            sampleRate, LOW_MID_FREQ, LOW_MID_Q, 1.0f);
        *eqChain.get<1>().state = *lowMidCoeffs;
    }
}

//==============================================================================
void SpectralEngine::processFormantShift (juce::AudioBuffer<float>& buffer, int channel)
{
    auto numSamples = buffer.getNumSamples();
    auto* channelData = buffer.getWritePointer (channel);
    
    // Вычисляем формант-сдвиг от Clarity (в полутонах)
    auto clarityCurved = clarityParam * 2.0f;  // -1.0 to +1.0
    auto formantShiftSemitones = clarityCurved * 3.0f;  // ±3 полутона (документация: ±2-6 пт)
    auto formantShiftRatio = std::pow (2.0f, formantShiftSemitones / 12.0f);  // Частотный коэффициент
    
    // Если сдвиг нулевой, формант-шифт не нужен
    if (std::abs (formantShiftRatio - 1.0f) < 0.001f)
        return;
    
    // Обрабатываем каждый семпл с overlap-add
    for (int i = 0; i < numSamples; ++i)
    {
        // Добавляем семпл во входной буфер
        inputBuffer[inputBufferPos] = channelData[i];
        inputBufferPos++;
        
        // Когда набрали достаточно семплов для FFT, обрабатываем
        if (inputBufferPos >= hopSize)
        {
            // Копируем inputBuffer в fftBuffer с окном
            for (int j = 0; j < fftSize; ++j)
            {
                fftBuffer[j * 2] = inputBuffer[j] * windowBuffer[j];  // Real
                fftBuffer[j * 2 + 1] = 0.0f;                           // Imaginary
            }
            
            // Forward FFT
            fft->performRealOnlyForwardTransform (fftBuffer.data());
            
            // Спектральное перемаппирование (формант-сдвиг)
            // Создаем новый спектр со сдвинутыми частотами
            std::vector<float> shiftedSpectrum (fftSize * 2, 0.0f);
            
            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                // Вычисляем новую частоту после сдвига
                float originalFreq = static_cast<float> (bin) * sampleRate / fftSize;
                float shiftedFreq = originalFreq * formantShiftRatio;
                float shiftedBin = shiftedFreq * fftSize / sampleRate;
                
                // Интерполируем между бинами
                int binLow = static_cast<int> (std::floor (shiftedBin));
                int binHigh = static_cast<int> (std::ceil (shiftedBin));
                float fraction = shiftedBin - binLow;
                
                if (binHigh < fftSize / 2)
                {
                    // Линейная интерполяция
                    float realLow = fftBuffer[binLow * 2];
                    float imagLow = fftBuffer[binLow * 2 + 1];
                    float realHigh = fftBuffer[binHigh * 2];
                    float imagHigh = fftBuffer[binHigh * 2 + 1];
                    
                    shiftedSpectrum[bin * 2] = realLow * (1.0f - fraction) + realHigh * fraction;
                    shiftedSpectrum[bin * 2 + 1] = imagLow * (1.0f - fraction) + imagHigh * fraction;
                }
            }
            
            // Копируем обратно в fftBuffer
            std::copy (shiftedSpectrum.begin(), shiftedSpectrum.end(), fftBuffer.begin());
            
            // Inverse FFT
            fft->performRealOnlyInverseTransform (fftBuffer.data());
            
            // Применяем окно и добавляем в outputBuffer с overlap-add
            for (int j = 0; j < fftSize; ++j)
            {
                float sample = fftBuffer[j * 2] * windowBuffer[j];
                
                if (j < overlapSize)
                {
                    // Overlap-add с предыдущим окном
                    sample += overlapBuffer[j];
                }
                
                if (j < hopSize)
                {
                    // Выводим первые hopSize семплов
                    outputBuffer[j] = sample;
                }
                else if (j < overlapSize)
                {
                    // Сохраняем для следующего overlap
                    overlapBuffer[j] = sample;
                }
            }
            
            // Сдвигаем inputBuffer влево на hopSize
            std::copy (inputBuffer.begin() + hopSize, inputBuffer.end(), inputBuffer.begin());
            std::fill (inputBuffer.begin() + (fftSize - hopSize), inputBuffer.end(), 0.0f);
            inputBufferPos -= hopSize;
        }
        
        // Выводим семпл из outputBuffer
        if (outputBufferPos < hopSize && inputBufferPos < hopSize)
        {
            channelData[i] = outputBuffer[outputBufferPos];
            outputBufferPos++;
        }
    }
    
    // Сбрасываем позиции для следующего блока
    if (outputBufferPos >= hopSize)
        outputBufferPos = 0;
}

//==============================================================================
void SpectralEngine::process (juce::AudioBuffer<float>& buffer)
{
    // КРИТИЧНО: отключаем денормалы для предотвращения асимметрии на разных каналах
    juce::ScopedNoDenormals noDenormals;
    
    auto numSamples = buffer.getNumSamples();
    
    if (numChannels == 0 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    claritySmoother.skip (numSamples);
    depthSmoother.skip (numSamples);
    flowSmoother.skip (numSamples);
    
    // Обновляем smoothed значения для плавности
    claritySmoother.setTargetValue (clarityParam);
    depthSmoother.setTargetValue (depthParam);
    
    // Update filters if parameters changed (проверяем прямое значение параметра)
    static float lastClarity = -999.0f;
    static float lastDepth = -999.0f;
    
    // Обновляем фильтры если значения изменились (используем прямое значение, не smoothed)
    if (std::abs (clarityParam - lastClarity) > 0.0001f ||
        std::abs (depthParam - lastDepth) > 0.0001f)
    {
        lastClarity = clarityParam;
        lastDepth = depthParam;
        updateFilters();
    }
    
    // Process formant shift for each channel (before EQ)
    // ВРЕМЕННО ОТКЛЮЧЕНО - FFT вызывает зависание (нужна оптимизация)
    // Формант-шифт управляется Clarity (не Depth!)
    // auto clarity = claritySmoother.getCurrentValue();
    // if (std::abs (clarity) > 0.01f)  // Если Clarity не нулевой
    // {
    //     for (int ch = 0; ch < numChannels; ++ch)
    //         processFormantShift (buffer, ch);
    // }
    
    // Process through EQ chain (after formant shift)
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    eqChain.process (context);
}

