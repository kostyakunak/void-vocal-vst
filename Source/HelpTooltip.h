/*
  ==============================================================================

   HelpTooltip - Красивые подсказки для параметров
   Стиль: красноречиво, для звукорежиссеров и обычных пользователей

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Макрос для явного создания UTF-8 строк из строковых литералов
// Гарантирует правильную обработку кириллицы независимо от настроек компилятора
#define UTF8_STRING(str) juce::String::fromUTF8(str, static_cast<int>(sizeof(str) - 1))

//==============================================================================
class HelpTooltip : public juce::Component
{
public:
    HelpTooltip (const juce::String& title, const juce::String& description);
    ~HelpTooltip() override = default;

    void paint (juce::Graphics& g) override;

private:
    juce::String titleText;
    juce::String descriptionText;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpTooltip)
};

//==============================================================================
class HelpButton : public juce::Component
{
public:
    HelpButton();
    ~HelpButton() override = default;

    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void setHelpText (const juce::String& title, const juce::String& description);

private:
    bool isHovered = false;
    juce::String helpTitle;
    juce::String helpDescription;
    
    void showHelpWindow();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpButton)
};


