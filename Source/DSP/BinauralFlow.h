/*
  ==============================================================================

   BinauralFlow - Психоакустический кирпич для Iceberg
   Создаёт иллюзию движения через микро-задержки L/R и фазовую модуляцию

   Психоакустика:
   - ITD (Interaural Time Difference) 0.3-0.6 мс создаёт ощущение движения
   - Мозг очень чувствителен к изменениям ITD, но не замечает статическую задержку
   - Медленный LFO (0.03-0.08 Гц) имитирует естественное "дыхание" пространства
   - Фазовая модуляция только на верхах (5-12 кГц) для избежания фейзинга
   - Случайный джиттер добавляет естественность

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <random>

//==============================================================================
class BinauralFlow
{
public:
    BinauralFlow();
    ~BinauralFlow() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameter control (normalized 0.0-1.0)
    void setFlow (float flow);        // 0.0 = static, 1.0 = full movement
    void setDepth (float depth);      // 0.0 = subtle, 1.0 = pronounced
    void setGhost (float ghost);      // 0.0 = no phase mod, 1.0 = full phase mod

private:
    // Fractional delay line с линейной интерполяцией
    float getDelayedSample (float* delayBuffer, int writePos, float delaySamples, int bufferSize);
    
    // Фазовая модуляция для верхов (5-12 кГц)
    void applyPhaseModulation (float* leftChannel, float* rightChannel, int numSamples);
    
    // Обновление случайного джиттера
    void updateRandomJitter();
    
    // Delay buffers для L и R каналов
    // Размер: достаточно для максимальной задержки (1 мс при 48 кГц = 48 семплов)
    static constexpr int MAX_DELAY_SAMPLES = 64;  // 1.45 мс при 44.1 кГц
    juce::AudioBuffer<float> delayBufferL, delayBufferR;
    int writePosL = 0, writePosR = 0;
    
    // LFO для модуляции задержки
    float lfoPhase = 0.0f;
    
    // Фазовая модуляция (для верхов)
    float phaseModL = 0.0f;
    float phaseModR = 0.0f;
    float phaseModPhase = 0.0f;
    
    // High-pass фильтр для фазовой модуляции (только верха 5-12 кГц)
    using IIR = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    template<typename F> using Dup = juce::dsp::ProcessorDuplicator<F, Coeffs>;
    juce::dsp::ProcessorChain<Dup<IIR>, Dup<IIR>> highPassChain;  // L и R
    
    // Случайный джиттер (обновляется раз в несколько секунд)
    float randomJitterL = 0.0f;
    float randomJitterR = 0.0f;
    float jitterUpdateCounter = 0.0f;
    std::mt19937 randomGenerator;
    std::uniform_real_distribution<float> jitterDistribution;
    
    // Parameters (normalized 0.0-1.0)
    float flowParam = 0.0f;
    float depthParam = 0.0f;
    float ghostParam = 0.0f;
    
    // Smoothed parameters для плавности
    juce::LinearSmoothedValue<float> flowSmoother, depthSmoother, ghostSmoother;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;
    
    // Психоакустические константы (согласно main.md:513-590)
    static constexpr float MIN_DELAY_MS = 0.3f;      // Минимальная задержка (мс)
    static constexpr float MAX_DELAY_MS = 0.6f;      // Максимальная задержка (мс)
    static constexpr float MIN_LFO_HZ = 0.03f;       // Минимальная частота LFO (33 сек цикл)
    static constexpr float MAX_LFO_HZ = 0.08f;       // Максимальная частота LFO (12.5 сек цикл)
    static constexpr float MAX_JITTER_MS = 0.1f;    // Максимальный случайный джиттер (мс)
    static constexpr float JITTER_UPDATE_SEC = 3.0f; // Обновление джиттера раз в 3 секунды
    static constexpr float PHASE_MOD_FREQ_HZ = 0.05f; // Частота фазовой модуляции (20 сек цикл)
    static constexpr float PHASE_MOD_DEG_MIN = 5.0f;  // Минимальная фазовая модуляция (градусы)
    static constexpr float PHASE_MOD_DEG_MAX = 10.0f; // Максимальная фазовая модуляция (градусы)
    static constexpr float HIGH_PASS_FREQ = 5000.0f; // Частота среза для фазовой модуляции (5 кГц)
    static constexpr float HIGH_PASS_Q = 0.7f;       // Q фильтра
    
    static constexpr float TWO_PI = 6.28318530718f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BinauralFlow)
};

