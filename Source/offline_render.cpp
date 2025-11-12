/*
  ==============================================================================

   offline_render - Консольное приложение для офлайн-рендеринга плагина

  ==============================================================================
*/

#include "OfflineRenderer.h"
#include <juce_core/juce_core.h>
#include <iostream>

//==============================================================================
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI scopedJuce;
    if (argc < 3)
    {
        std::cout << "Использование: offline_render <input.wav> <output.wav> [параметры]" << std::endl;
        std::cout << std::endl;
        std::cout << "Параметры (опционально, через запятую):" << std::endl;
        std::cout << "  flow=0.5      - Скорость движения (0.0-1.0)" << std::endl;
        std::cout << "  energy=0.7    - Сила движения (0.0-1.0)" << std::endl;
        std::cout << "  mix=0.5       - Mix (0.0-1.0)" << std::endl;
        std::cout << "  depth=0.5     - Depth (0.0-1.0)" << std::endl;
        std::cout << "  ghost=0.3     - Ghost (0.0-1.0)" << std::endl;
        std::cout << "  clarity=0.0   - Clarity (-0.5-0.5)" << std::endl;
        std::cout << std::endl;
        std::cout << "Примеры:" << std::endl;
        std::cout << "  offline_render input.wav output.wav" << std::endl;
        std::cout << "  offline_render input.wav output.wav flow=0.5,energy=0.7,mix=0.5" << std::endl;
        return 1;
    }
    
    juce::String inputFile = argv[1];
    juce::String outputFile = argv[2];
    juce::String params = argc > 3 ? argv[3] : "";
    
    OfflineRenderer renderer;
    
    if (renderer.renderFile (inputFile, outputFile, params))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

