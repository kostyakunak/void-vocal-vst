/*
  ==============================================================================

   HarmonicGlide - PsychoCore #2 для Platina
   Психоакустический кирпич: микро-сдвиг гармоник в ответ на динамику

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

//==============================================================================
class HarmonicGlide
{
public:
    HarmonicGlide();
    ~HarmonicGlide() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameter control (normalized 0.0-1.0)
    void setEnergy (float energy);    // Чувствительность к громкости (0.0 = выкл, 1.0 = макс)
    void setFlow (float flow);       // Скорость реакции (0.0 = медленно, 1.0 = быстро)

private:
    // RMS-follower для отслеживания энергии
    float rmsValue = 0.0f;
    float rmsTarget = 0.0f;
    static constexpr float RMS_ATTACK_TIME_MS = 50.0f;      // Быстрая атака (50 мс)
    static constexpr float RMS_RELEASE_TIME_MS = 450.0f;    // Медленный релиз (450 мс) - основное время реакции
    
    // Параметры питч-шифта
    static constexpr float MAX_SHIFT_CENTS = 3.0f;          // Максимальный сдвиг: ±3 цента
    static constexpr float MIN_SHIFT_CENTS = 2.0f;          // Минимальный сдвиг: ±2 цента
    
    // Delay line для питч-шифта (микро-сдвиг через задержку с интерполяцией)
    static constexpr int MAX_DELAY_SAMPLES = 512;           // Максимальная задержка для питч-шифта
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int delayWritePosL = 0;
    int delayWritePosR = 0;
    
    // Текущий питч-шифт (в центах)
    float currentPitchShiftCents = 0.0f;
    float targetPitchShiftCents = 0.0f;
    
    // Состояние для отслеживания изменения RMS
    float previousRMS = 0.0f;
    float smoothedDelta = 0.0f;
    
    // Сглаживание питч-шифта (для плавности)
    juce::LinearSmoothedValue<float> pitchShiftSmoother;
    static constexpr float PITCH_SHIFT_SMOOTH_TIME_MS = 100.0f;  // Плавное изменение питч-шифта
    
    // Parameters (normalized 0.0-1.0)
    float energyParam = 0.0f;
    float flowParam = 0.0f;
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> energySmoother, flowSmoother;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;
    
    // Вспомогательные функции
    float calculateRMS (const juce::AudioBuffer<float>& buffer);
    float centsToDelaySamples (float cents);
    float getDelayedSample (const std::vector<float>& delayBuffer, int writePos, float delaySamples);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicGlide)
};

