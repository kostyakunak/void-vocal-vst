/*
  ==============================================================================

   SpectralEngine - Spectral processing для Iceberg
   Этап 1: Настоящий формант-шифт через спектральное перемаппирование

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

//==============================================================================
class SpectralEngine
{
public:
    SpectralEngine();
    ~SpectralEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    // Parameter control (normalized)
    void setClarity (float clarity);    // -0.5 to +0.5: баланс верхов/низов
    void setDepth (float depth);        // 0.0-1.0: формант-сдвиг вниз + спектральная "темнота"
    void setFlow (float flow);          // 0.0-1.0: LFO на форманты (для Motion Mod)

private:
    void updateFilters();
    void processFormantShift (juce::AudioBuffer<float>& buffer, int channel);
    
    // EQ для спектрального баланса
    // Используем ProcessorDuplicator для раздельных состояний фильтров на каждый канал
    // Это устраняет стерео-смещение при изменении Clarity
    using IIR = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    template<typename F> using Dup = juce::dsp::ProcessorDuplicator<F, Coeffs>;
    
    juce::dsp::ProcessorChain<
        Dup<IIR>,  // High-shelf для воздуха (Clarity - верха)
        Dup<IIR>,  // Low-mid для "гула" (Depth, когда формант-шифт выкл)
        Dup<IIR>,  // Формант F1 (200-800 Hz) - Clarity управляет сдвигом
        Dup<IIR>,  // Формант F2 (800-3000 Hz) - Clarity управляет сдвигом
        Dup<IIR>   // Формант F3 (2000-4000 Hz) - Clarity управляет сдвигом
    > eqChain;

    // FFT для формант-шифта (оптимизировано)
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftBuffer;           // FFT буфер (фиксированный размер)
    std::vector<float> windowBuffer;       // Окно (фиксированный размер)
    std::vector<float> inputBuffer;        // Входной буфер для накопления
    std::vector<float> outputBuffer;       // Выходной буфер для overlap-add
    std::vector<float> overlapBuffer;      // Overlap-add буфер (фиксированный размер)
    
    int fftSize = 2048;
    int hopSize = 512;
    int overlapSize = fftSize - hopSize;   // 1536 samples
    int inputBufferPos = 0;                // Позиция в inputBuffer
    int outputBufferPos = 0;               // Позиция в outputBuffer

    // Parameters (normalized)
    float clarityParam = 0.0f;  // -0.5 to +0.5
    float depthParam = 0.0f;    // 0.0 to 1.0
    float flowParam = 0.0f;     // 0.0 to 1.0
    
    // Smoothed parameters
    juce::LinearSmoothedValue<float> claritySmoother, depthSmoother, flowSmoother;
    
    // LFO для формант-модуляции (в противофазе с Motion Mod gain)
    float formantLfoPhase = 0.5f;  // 90° offset от gain LFO
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;
    
    // Iceberg-оптимизированные настройки
    static constexpr float HIGH_SHELF_FREQ = 8000.0f;  // Воздух в верхней части
    static constexpr float LOW_MID_FREQ = 400.0f;      // Формантовый гул (fallback)
    static constexpr float LOW_MID_Q = 1.5f;
    static constexpr float MAX_AIR_BOOST = 7.0f;       // Макс подъем воздуха (+7 дБ) - УМЕРЕННЫЙ
    static constexpr float MAX_LOW_MID_BOOST = 4.0f;    // Макс подъем гула (+4 дБ)
    
    // Формант-шифт настройки (для Iceberg)
    static constexpr float FORMANT_SHIFT_SEMITONES = -0.3f;  // -0.3 полутона вниз (скрытый параметр)
    static constexpr float FORMANT_LFO_HZ = 0.05f;          // 0.05 Hz LFO для модуляции
    static constexpr float FORMANT_LFO_DEPTH = 0.15f;       // ±0.15 полутона модуляция
    static constexpr float FORMANT_F1_MIN = 200.0f;          // F1 диапазон (Hz)
    static constexpr float FORMANT_F1_MAX = 800.0f;
    static constexpr float FORMANT_F2_MIN = 800.0f;          // F2 диапазон (Hz)
    static constexpr float FORMANT_F2_MAX = 3000.0f;
    static constexpr float FORMANT_F3_MIN = 2000.0f;         // F3 диапазон (Hz)
    static constexpr float FORMANT_F3_MAX = 4000.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralEngine)
};



