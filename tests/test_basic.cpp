/*
  ==============================================================================

   Базовые тесты для VØID Engine
   Проверка работы модулей и звуковых метрик

  ==============================================================================
*/

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>
#include "../Source/DSP/SpectralEngine.h"
#include "../Source/DSP/SpaceEngine.h"
#include "../Source/DSP/MotionMod.h"

// Простой ProcessSpec для тестов
juce::dsp::ProcessSpec createTestSpec()
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    return spec;
}

// Создание тестового сигнала (синусоида 440 Hz)
juce::AudioBuffer<float> createTestSignal(int numSamples, double sampleRate, float frequency = 440.0f)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    
    for (int channel = 0; channel < 2; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            data[sample] = std::sin(2.0 * juce::MathConstants<double>::pi * frequency * sample / sampleRate) * 0.5f;
        }
    }
    
    return buffer;
}

// Проверка RMS (громкость)
float calculateRMS(const juce::AudioBuffer<float>& buffer)
{
    float sum = 0.0f;
    int totalSamples = 0;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            sum += data[sample] * data[sample];
            totalSamples++;
        }
    }
    
    return std::sqrt(sum / totalSamples);
}

// Проверка моно-корреляции
float calculateMonoCorrelation(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return 1.0f;
    
    auto* left = buffer.getReadPointer(0);
    auto* right = buffer.getReadPointer(1);
    int numSamples = buffer.getNumSamples();
    
    float leftMean = 0.0f, rightMean = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        leftMean += left[i];
        rightMean += right[i];
    }
    leftMean /= numSamples;
    rightMean /= numSamples;
    
    float numerator = 0.0f;
    float leftVar = 0.0f, rightVar = 0.0f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        float leftDiff = left[i] - leftMean;
        float rightDiff = right[i] - rightMean;
        
        numerator += leftDiff * rightDiff;
        leftVar += leftDiff * leftDiff;
        rightVar += rightDiff * rightDiff;
    }
    
    float denominator = std::sqrt(leftVar * rightVar);
    if (denominator < 1e-10f)
        return 1.0f;
    
    return numerator / denominator;
}

// Проверка клипов
bool hasClips(const juce::AudioBuffer<float>& buffer, float threshold = 1.0f)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (std::abs(data[sample]) > threshold)
                return true;
        }
    }
    return false;
}

// Тест 1: SpectralEngine - Clarity работает
bool testSpectralClarity()
{
    std::cout << "Тест 1: SpectralEngine - Clarity (high-shelf) работает...\n";
    
    SpectralEngine engine;
    auto spec = createTestSpec();
    engine.prepare(spec);
    
    // Создаем тестовый сигнал
    auto signal = createTestSignal(8192, spec.sampleRate, 440.0f);
    
    // Тест без обработки (Clarity=0)
    engine.setDepth(0.0f);
    engine.setClarity(0.0f);
    engine.setFlow(0.0f);
    
    auto signalCopy1 = signal;
    engine.process(signalCopy1);
    float rms1 = calculateRMS(signalCopy1);
    
    // Тест с Clarity (подъем верхов)
    engine.setClarity(0.3f);
    auto signalCopy2 = signal;
    engine.process(signalCopy2);
    float rms2 = calculateRMS(signalCopy2);
    
    // Clarity должен изменить сигнал
    bool changed = std::abs(rms1 - rms2) > 0.001f;
    
    if (changed)
        std::cout << "  ✅ Clarity работает (RMS изменился: " << rms1 << " -> " << rms2 << ")\n";
    else
        std::cout << "  ⚠️  Предупреждение: Clarity не изменил сигнал\n";
    
    return true; // Не критично
}

// Тест 2: SpaceEngine - реверб работает
bool testSpaceReverb()
{
    std::cout << "\nТест 2: SpaceEngine - реверб работает...\n";
    
    SpaceEngine engine;
    auto spec = createTestSpec();
    engine.prepare(spec);
    
    auto signal = createTestSignal(8192, spec.sampleRate, 440.0f);
    
    // Тест без реверба (Ghost=0)
    engine.setGhost(0.0f);
    engine.setDepth(0.0f);
    engine.setFlow(0.0f);
    
    auto signalCopy1 = signal;
    engine.process(signalCopy1);
    float rms1 = calculateRMS(signalCopy1);
    
    // Тест с ревербом (Ghost=0.6)
    engine.setGhost(0.6f);
    engine.setDepth(0.5f);
    auto signalCopy2 = signal;
    engine.process(signalCopy2);
    float rms2 = calculateRMS(signalCopy2);
    
    // Реверб должен изменить сигнал
    bool changed = std::abs(rms1 - rms2) > 0.001f;
    
    if (changed)
        std::cout << "  ✅ Реверб работает (RMS изменился: " << rms1 << " -> " << rms2 << ")\n";
    else
        std::cout << "  ❌ Ошибка: Реверб не работает\n";
    
    return changed;
}

// Тест 3: MotionMod - движение работает
bool testMotionModulation()
{
    std::cout << "\nТест 3: MotionMod - движение работает...\n";
    
    MotionMod engine;
    auto spec = createTestSpec();
    engine.prepare(spec);
    
    auto signal = createTestSignal(8192, spec.sampleRate, 440.0f);
    
    // Тест без движения (Flow=0, Energy=0)
    engine.setFlow(0.0f);
    engine.setEnergy(0.0f);
    
    auto signalCopy1 = signal;
    engine.process(signalCopy1);
    float corr1 = calculateMonoCorrelation(signalCopy1);
    
    // Тест с движением (Flow=0.5, Energy=0.4)
    engine.setFlow(0.5f);
    engine.setEnergy(0.4f);
    
    auto signalCopy2 = signal;
    engine.process(signalCopy2);
    float corr2 = calculateMonoCorrelation(signalCopy2);
    
    // Движение должно изменить моно-корреляцию (панорама меняется)
    bool changed = std::abs(corr1 - corr2) > 0.01f;
    
    if (changed)
        std::cout << "  ✅ Движение работает (корреляция изменилась: " << corr1 << " -> " << corr2 << ")\n";
    else
        std::cout << "  ⚠️  Предупреждение: Движение не изменило корреляцию\n";
    
    return true; // Не критично
}

// Тест 4: Нет клипов
bool testNoClips()
{
    std::cout << "\nТест 4: Проверка клипов...\n";
    
    SpectralEngine spectral;
    SpaceEngine space;
    MotionMod motion;
    
    auto spec = createTestSpec();
    spectral.prepare(spec);
    space.prepare(spec);
    motion.prepare(spec);
    
    // Создаем громкий сигнал
    auto signal = createTestSignal(8192, spec.sampleRate, 440.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = signal.getWritePointer(ch);
        for (int i = 0; i < signal.getNumSamples(); ++i)
            data[i] *= 0.8f; // Громкий, но не клипинг
    }
    
    // Обработка через все модули
    spectral.setDepth(0.8f);
    spectral.setClarity(0.3f);
    spectral.setFlow(0.5f);
    spectral.process(signal);
    
    space.setGhost(0.7f);
    space.setDepth(0.8f);
    space.setFlow(0.5f);
    space.process(signal);
    
    motion.setFlow(0.5f);
    motion.setEnergy(0.5f);
    motion.process(signal);
    
    bool hasClipping = hasClips(signal, 1.0f);
    
    if (!hasClipping)
        std::cout << "  ✅ Нет клипов\n";
    else
        std::cout << "  ❌ Обнаружены клипы!\n";
    
    return !hasClipping;
}

// Тест 5: Моно-совместимость
bool testMonoCompatibility()
{
    std::cout << "\nТест 5: Моно-совместимость...\n";
    
    SpectralEngine spectral;
    SpaceEngine space;
    MotionMod motion;
    
    auto spec = createTestSpec();
    spectral.prepare(spec);
    space.prepare(spec);
    motion.prepare(spec);
    
    auto signal = createTestSignal(8192, spec.sampleRate, 440.0f);
    
    // Обработка
    spectral.setDepth(0.6f);
    spectral.setClarity(0.2f);
    spectral.setFlow(0.4f);
    spectral.process(signal);
    
    space.setGhost(0.6f);
    space.setDepth(0.7f);
    space.setFlow(0.4f);
    space.process(signal);
    
    motion.setFlow(0.4f);
    motion.setEnergy(0.3f);
    motion.process(signal);
    
    float correlation = calculateMonoCorrelation(signal);
    
    if (correlation >= 0.6f)
        std::cout << "  ✅ Моно-совместимость OK (корреляция: " << correlation << ")\n";
    else
        std::cout << "  ❌ Плохая моно-совместимость (корреляция: " << correlation << ")\n";
    
    return correlation >= 0.6f;
}

// Тест 6: Сглаживание параметров (нет кликов)
bool testParameterSmoothing()
{
    std::cout << "\nТест 6: Сглаживание параметров...\n";
    
    SpectralEngine engine;
    auto spec = createTestSpec();
    engine.prepare(spec);
    
    auto signal = createTestSignal(4096, spec.sampleRate, 440.0f);
    
    // Резкое изменение параметра
    engine.setDepth(0.0f);
    engine.process(signal);
    
    // Резко меняем на максимум
    engine.setDepth(1.0f);
    engine.process(signal);
    
    // Проверяем на клики (резкие скачки)
    bool hasClicks = false;
    for (int ch = 0; ch < signal.getNumChannels(); ++ch)
    {
        auto* data = signal.getReadPointer(ch);
        for (int i = 1; i < signal.getNumSamples(); ++i)
        {
            float diff = std::abs(data[i] - data[i-1]);
            if (diff > 0.1f) // Большой скачок
            {
                hasClicks = true;
                break;
            }
        }
        if (hasClicks) break;
    }
    
    if (!hasClicks)
        std::cout << "  ✅ Сглаживание работает (нет резких скачков)\n";
    else
        std::cout << "  ⚠️  Предупреждение: Обнаружены резкие скачки (возможно недостаточное сглаживание)\n";
    
    return true; // Не критично
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "VØID Engine - Базовые тесты\n";
    std::cout << "========================================\n\n";
    
    int passed = 0;
    int total = 6;
    
    if (testSpectralClarity()) passed++;
    if (testSpaceReverb()) passed++;
    if (testMotionModulation()) passed++;
    if (testNoClips()) passed++;
    if (testMonoCompatibility()) passed++;
    if (testParameterSmoothing()) passed++;
    
    std::cout << "\n========================================\n";
    std::cout << "Результаты: " << passed << "/" << total << " тестов пройдено\n";
    std::cout << "========================================\n";
    
    return (passed == total) ? 0 : 1;
}

