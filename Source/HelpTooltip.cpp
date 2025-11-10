/*
  ==============================================================================

   HelpTooltip - Красивые подсказки для параметров

  ==============================================================================
*/

#include "HelpTooltip.h"
#include <memory>

//==============================================================================
HelpTooltip::HelpTooltip (const juce::String& title, const juce::String& description)
    : titleText (title), descriptionText (description)
{
    // Calculate height based on text content
    // Простая оценка: считаем количество переносов строк и примерную ширину
    juce::Font font (juce::FontOptions (13.0f));
    auto textWidth = 400.0f - 32.0f; // Width minus padding
    auto lineHeight = font.getHeight() + 4.0f; // 4px spacing between lines
    
    // Подсчитываем количество строк (по \n)
    int numLines = 1;
    for (auto i = 0; i < descriptionText.length(); ++i)
    {
        if (descriptionText[i] == '\n')
            numLines++;
    }
    
    // Добавляем строки, которые переносятся по ширине (примерная оценка)
    auto avgCharWidth = font.getStringWidthFloat ("M");
    auto charsPerLine = static_cast<int> (textWidth / avgCharWidth);
    auto totalChars = descriptionText.length();
    auto wrappedLines = (totalChars / charsPerLine) + 1;
    numLines = juce::jmax (numLines, wrappedLines);
    
    auto textHeight = static_cast<float> (numLines) * lineHeight;
    auto minHeight = 150.0f;
    auto height = juce::jmax (minHeight, 60.0f + textHeight);
    
    setSize (400, static_cast<int> (height));
}

void HelpTooltip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Фон с градиентом (холодные тона, как Iceberg)
    juce::ColourGradient gradient (juce::Colour (0xff1A2A3A).darker (0.3f),
                                   bounds.getTopLeft().toFloat(),
                                   juce::Colour (0xff2A3A4A).brighter (0.1f),
                                   bounds.getBottomLeft().toFloat(),
                                   false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds.toFloat(), 12.0f);
    
    // Рамка
    g.setColour (juce::Colour (0xff4A90E2).withAlpha (0.6f));
    g.drawRoundedRectangle (bounds.toFloat(), 12.0f, 2.0f);
    
    // Заголовок - drawText правильно обрабатывает UTF-8 если строка в UTF-8
    auto titleBounds = bounds.removeFromTop (45);
    g.setColour (juce::Colour (0xffE0E0E0));
    g.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
    g.drawText (titleText, titleBounds.reduced (16, 8),
                juce::Justification::centredLeft, true);
    
    // Описание - используем TextLayout напрямую для правильного UTF-8
    // Создаём TextLayout из AttributedString вручную
    juce::AttributedString attributedText;
    attributedText.append (descriptionText, juce::FontOptions (13.0f), juce::Colour (0xffB0B0B0));
    attributedText.setJustification (juce::Justification::topLeft);
    attributedText.setWordWrap (juce::AttributedString::WordWrap::byWord);
    
    // Создаём TextLayout вручную
    juce::TextLayout textLayout;
    textLayout.createLayoutWithBalancedLineLengths (attributedText, static_cast<float> (bounds.getWidth() - 32));
    textLayout.draw (g, bounds.reduced (16, 20).toFloat());
}

//==============================================================================
HelpButton::HelpButton()
{
    setSize (20, 20);
}

void HelpButton::setHelpText (const juce::String& title, const juce::String& description)
{
    helpTitle = title;
    helpDescription = description;
}

void HelpButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    
    // Фон круга
    auto bgColour = isHovered ? juce::Colour (0xff4A90E2).withAlpha (0.3f)
                               : juce::Colour (0xff4A90E2).withAlpha (0.15f);
    g.setColour (bgColour);
    g.fillEllipse (bounds);
    
    // Рамка
    g.setColour (juce::Colour (0xff4A90E2).withAlpha (isHovered ? 0.8f : 0.5f));
    g.drawEllipse (bounds, 1.5f);
    
    // Знак вопроса
    g.setColour (juce::Colour (0xffE0E0E0));
    g.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
    g.drawText ("?", bounds, juce::Justification::centred, true);
}

void HelpButton::mouseEnter (const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void HelpButton::mouseExit (const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
}

void HelpButton::mouseDown (const juce::MouseEvent&)
{
    showHelpWindow();
}

void HelpButton::showHelpWindow()
{
    if (helpTitle.isEmpty())
        return;
    
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;
    
    auto tooltip = std::make_unique<HelpTooltip> (helpTitle, helpDescription);
    
    auto screenBounds = getScreenBounds();
    juce::CallOutBox::launchAsynchronously (std::move (tooltip),
                                            screenBounds,
                                            parent->getTopLevelComponent());
    // CallOutBox сам управляет жизненным циклом tooltip
}

