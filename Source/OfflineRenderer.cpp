/*
  ==============================================================================

   OfflineRenderer - Офлайн-рендеринг плагина через командную строку

  ==============================================================================
*/

#include "OfflineRenderer.h"
#include <iostream>

//==============================================================================
OfflineRenderer::OfflineRenderer()
{
    processor = std::make_unique<JuceDemoPluginAudioProcessor>();
}

//==============================================================================
bool OfflineRenderer::loadAudioFile (const juce::String& filePath, juce::AudioBuffer<float>& buffer, double& sampleRate)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (juce::File (filePath)));
    
    if (reader == nullptr)
    {
        std::cerr << "Ошибка: Не удалось загрузить файл " << filePath << std::endl;
        return false;
    }
    
    sampleRate = reader->sampleRate;
    auto numChannels = static_cast<int> (reader->numChannels);
    auto numSamples = static_cast<int> (reader->lengthInSamples);
    
    buffer.setSize (numChannels, numSamples, false, true, true);
    
    if (! reader->read (&buffer, 0, numSamples, 0, true, true))
    {
        std::cerr << "Ошибка: Не удалось прочитать аудио данные" << std::endl;
        return false;
    }
    
    return true;
}

//==============================================================================
bool OfflineRenderer::saveAudioFile (const juce::String& filePath, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    juce::File outputFile (filePath);
    outputFile.deleteFile();
    
    std::unique_ptr<juce::AudioFormatWriter> writer;
    
    if (filePath.endsWith (".wav"))
    {
        writer.reset (formatManager.findFormatForFileExtension ("wav")->createWriterFor (
            new juce::FileOutputStream (outputFile),
            sampleRate,
            buffer.getNumChannels(),
            16,
            {},
            0));
    }
    else
    {
        std::cerr << "Ошибка: Неподдерживаемый формат файла" << std::endl;
        return false;
    }
    
    if (writer == nullptr)
    {
        std::cerr << "Ошибка: Не удалось создать writer для " << filePath << std::endl;
        return false;
    }
    
    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
    {
        std::cerr << "Ошибка: Не удалось записать аудио данные" << std::endl;
        return false;
    }
    
    return true;
}

//==============================================================================
void OfflineRenderer::parsePresetParams (const juce::String& params)
{
    // Парсим параметры в формате: "flow=0.5,energy=0.7,mix=0.5"
    juce::StringArray paramList;
    paramList.addTokens (params, ",", "");
    
    for (auto& param : paramList)
    {
        auto keyValue = param.trim().upToFirstOccurrenceOf ("=", false, false);
        auto value = param.trim().fromFirstOccurrenceOf ("=", false, false);
        
        float floatValue = value.getFloatValue();
        
        if (keyValue == "flow")
            processor->state.getParameter ("flow")->setValueNotifyingHost (floatValue);
        else if (keyValue == "energy")
            processor->state.getParameter ("energy")->setValueNotifyingHost (floatValue);
        else if (keyValue == "mix")
            processor->state.getParameter ("mix")->setValueNotifyingHost (floatValue);
        else if (keyValue == "depth")
            processor->state.getParameter ("depth")->setValueNotifyingHost (floatValue);
        else if (keyValue == "ghost")
            processor->state.getParameter ("ghost")->setValueNotifyingHost (floatValue);
        else if (keyValue == "clarity")
        {
            // Clarity: -0.5 to 0.5, нормализуем в 0.0-1.0
            float normalized = (floatValue + 0.5f) / 1.0f;
            processor->state.getParameter ("clarity")->setValueNotifyingHost (normalized);
        }
        else if (keyValue == "output")
        {
            // Output: 0.0 to 2.0, нормализуем в 0.0-1.0 для setValueNotifyingHost
            float normalized = floatValue / 2.0f;
            processor->state.getParameter ("output")->setValueNotifyingHost (normalized);
        }
    }
}

//==============================================================================
bool OfflineRenderer::renderFile (const juce::String& inputFile,
                                   const juce::String& outputFile,
                                   const juce::String& presetParams)
{
    std::cout << "🎵 Офлайн-рендеринг VØID Engine" << std::endl;
    std::cout << "   Входной файл: " << inputFile << std::endl;
    std::cout << "   Выходной файл: " << outputFile << std::endl;
    
    // Загружаем аудио файл
    juce::AudioBuffer<float> audioBuffer;
    double sampleRate = 44100.0;
    
    if (! loadAudioFile (inputFile, audioBuffer, sampleRate))
        return false;
    
    std::cout << "   Загружено: " << audioBuffer.getNumChannels() 
              << " каналов, " << audioBuffer.getNumSamples() 
              << " семплов, " << sampleRate << " Гц" << std::endl;
    
    // Подготавливаем процессор
    processor->prepareToPlay (sampleRate, 512);
    
    // Устанавливаем параметры ПЕРЕД установкой Output по умолчанию
    // (чтобы если в параметрах указан output, он не перезаписывался)
    if (presetParams.isNotEmpty())
    {
        std::cout << "   Параметры: " << presetParams << std::endl;
        parsePresetParams (presetParams);
    }
    
    // Устанавливаем Output = 2.0 по умолчанию (только если не указан в параметрах)
    // Проверяем, был ли output в параметрах
    bool outputSet = presetParams.containsIgnoreCase ("output=");
    if (!outputSet)
    {
        // Output=2.0 → нормализованное = 1.0
        processor->state.getParameter ("output")->setValueNotifyingHost (1.0f);
    }
    
    // Обрабатываем аудио блоками
    const int blockSize = 512;
    int numSamples = audioBuffer.getNumSamples();
    int numChannels = audioBuffer.getNumChannels();
    
    juce::MidiBuffer midiBuffer;
    
    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        int samplesToProcess = juce::jmin (blockSize, numSamples - pos);
        
        juce::AudioBuffer<float> block (numChannels, samplesToProcess);
        
        for (int ch = 0; ch < numChannels; ++ch)
            block.copyFrom (ch, 0, audioBuffer, ch, pos, samplesToProcess);
        
        processor->processBlock (block, midiBuffer);
        
        for (int ch = 0; ch < numChannels; ++ch)
            audioBuffer.copyFrom (ch, pos, block, ch, 0, samplesToProcess);
    }
    
    // Сохраняем результат
    if (! saveAudioFile (outputFile, audioBuffer, sampleRate))
        return false;
    
    std::cout << "✅ Рендеринг завершён!" << std::endl;
    return true;
}

