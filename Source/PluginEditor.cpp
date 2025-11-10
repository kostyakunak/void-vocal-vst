/*
  ==============================================================================

   AudioPluginDemo Editor - постепенная модификация
   Этап 0: Завершен - GUI для всех параметров

  ==============================================================================
*/

#include "PluginEditor.h"
#include "HelpTooltip.h"

//==============================================================================
JuceDemoPluginAudioProcessorEditor::JuceDemoPluginAudioProcessorEditor (JuceDemoPluginAudioProcessor& owner)
    : AudioProcessorEditor (owner),
      flowAttachment    (owner.state, "flow", flowSlider),
      meltAttachment    (owner.state, "melt", meltSlider),
      ghostAttachment   (owner.state, "ghost", ghostSlider),
      depthAttachment   (owner.state, "depth", depthSlider),
      clarityAttachment (owner.state, "clarity", claritySlider),
      gravityAttachment (owner.state, "gravity", gravitySlider),
      energyAttachment  (owner.state, "energy", energySlider),
      mixAttachment     (owner.state, "mix", mixSlider),
      outputAttachment  (owner.state, "output", outputSlider)
{
    // Setup all sliders with modern styling
    juce::Slider* sliders[] = { &flowSlider, &meltSlider, &ghostSlider, &depthSlider,
                                &claritySlider, &gravitySlider, &energySlider,
                                &mixSlider, &outputSlider };
    
    for (auto* slider : sliders)
    {
        addAndMakeVisible (slider);
        slider->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        slider->setTextValueSuffix ("");
        slider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff4A90E2));
        slider->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2A2A2A));
        slider->setColour (juce::Slider::thumbColourId, juce::Colour (0xffFFFFFF));
        slider->setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffE0E0E0));
        slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1A1A1A));
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3A3A3A));
    }
    
    // Mix slider uses 0-100% range
    mixSlider.setTextValueSuffix ("%");
    
    // Output slider uses 0-200% range (0-2.0)
    outputSlider.setTextValueSuffix ("x");
    
    // Clarity slider uses -50% to +50% range
    claritySlider.setTextValueSuffix ("%");
    
    // Setup all labels
    juce::Label* labels[] = { &flowLabel, &meltLabel, &ghostLabel, &depthLabel,
                             &clarityLabel, &gravityLabel, &energyLabel,
                             &mixLabel, &outputLabel };
    
    // Mark parameters that are still stubs (not yet implemented)
    // Active: Flow, Depth, Ghost, Clarity, Energy, Mix, Output
    // Stubs: Melt (GranularEngine), Gravity (DynamicLayer)
    // Clarity РЕАЛИЗОВАН (SpectralEngine) - убрали из stubs!
    bool isStub[] = { false, true, false, false,  // Flow, Melt, Ghost, Depth
                      false, true, false,          // Clarity, Gravity, Energy
                      false, false };             // Mix, Output
    
    for (int i = 0; i < 9; ++i)
    {
        labels[i]->setFont (juce::FontOptions (13.0f).withStyle ("bold"));
        labels[i]->setJustificationType (juce::Justification::centred);
        if (isStub[i])
        {
            // Gray out stub parameters
            labels[i]->setColour (juce::Label::textColourId, juce::Colour (0xff666666));
            labels[i]->setText (labels[i]->getText() + " (stub)", juce::dontSendNotification);
        }
        else
        {
            labels[i]->setColour (juce::Label::textColourId, juce::Colour (0xffE0E0E0));
        }
        addAndMakeVisible (labels[i]);
    }
    
    // Disable and gray out stub sliders
    if (isStub[1]) // Melt
    {
        meltSlider.setEnabled (false);
        meltSlider.setAlpha (0.5f);
    }
    // Clarity РЕАЛИЗОВАН - не блокируем!
    // if (isStub[4]) // Clarity - убрали, теперь работает
    if (isStub[5]) // Gravity
    {
        gravitySlider.setEnabled (false);
        gravitySlider.setAlpha (0.5f);
    }

    addAndMakeVisible (timecodeDisplayLabel);
    timecodeDisplayLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    
    // Setup help buttons with descriptions
    setupHelpButtons();
    timecodeDisplayLabel.setColour (juce::Label::textColourId, juce::Colour (0xff888888));
    timecodeDisplayLabel.setJustificationType (juce::Justification::centred);

    setResizeLimits (560, 500, 1000, 800);
    setResizable (true, owner.wrapperType != juce::AudioProcessor::wrapperType_AudioUnitv3);

    lastUIWidth .referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("width",  nullptr));
    lastUIHeight.referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("height", nullptr));

    // Default size for modern UI: 3 rows of 3 sliders each
    auto defaultWidth = 560;
    auto defaultHeight = 500;
    auto savedWidth = static_cast<int> (lastUIWidth.getValue());
    auto savedHeight = static_cast<int> (lastUIHeight.getValue());
    
    if (savedWidth < 600 || savedHeight < 400)
    {
        // Use default size for new 9-slider layout
        setSize (defaultWidth, defaultHeight);
    }
    else
    {
        setSize (savedWidth, savedHeight);
    }

    lastUIWidth. addListener (this);
    lastUIHeight.addListener (this);

    updateTrackProperties();

    startTimerHz (30);
}

//==============================================================================
void JuceDemoPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Modern dark gradient background
    juce::ColourGradient gradient (juce::Colour (0xff1A1A2E), 0.0f, 0.0f,
                                   juce::Colour (0xff16213E), 0.0f, static_cast<float> (getHeight()),
                                   false);
    g.setGradientFill (gradient);
    g.fillAll();
    
    // Title bar with plugin name
    auto titleArea = getLocalBounds().removeFromTop (40);
    g.setColour (juce::Colour (0xff0F0F1E));
    g.fillRect (titleArea);
    
    // Plugin name and version
    g.setColour (juce::Colour (0xffE0E0E0));
    g.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
    
    // Build version string with date for tracking changes
    auto versionText = UTF8_STRING("VØID Engine v1.4.6 - Build ") + 
                       juce::Time::getCompilationDate().toString (true, true, false, true);
    
    g.drawText (versionText, titleArea.reduced (12, 0),
                juce::Justification::left, true);
    
    // Smaller version info on the right
    g.setColour (juce::Colour (0xff888888));
    g.setFont (juce::FontOptions (11.0f));
    auto buildInfo = juce::String ("Stage 1 - DSP Core");
    g.drawText (buildInfo, titleArea.reduced (12, 0),
                juce::Justification::right, true);
    
    // Subtle divider line
    g.setColour (juce::Colour (0xff3A3A4E).withAlpha (0.5f));
    g.drawLine (0.0f, static_cast<float> (titleArea.getBottom()),
                static_cast<float> (getWidth()), static_cast<float> (titleArea.getBottom()),
                1.0f);
}

void JuceDemoPluginAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    
    // Title bar (already painted, but we skip it in layout)
    r.removeFromTop (40);
    
    // Timecode display at top
    timecodeDisplayLabel.setBounds (r.removeFromTop (24).reduced (8, 4));
    r.removeFromTop (8);

    // Arrange sliders: 3 rows of 3 sliders each
    const int sliderSize = 100;  // Larger sliders
    const int labelHeight = 24;
    const int spacing = 20;  // More spacing
    const int margin = 20;  // Outer margin
    
    auto sliderArea = r.reduced (margin, margin);
    
    // Row 1: Flow, Melt, Ghost (3 sliders)
    auto row1 = sliderArea.removeFromTop (sliderSize + labelHeight + 8);
    auto flowArea = row1.removeFromLeft (sliderSize);
    flowLabel.setBounds (flowArea.removeFromTop (labelHeight));
    flowSlider.setBounds (flowArea);
    row1.removeFromLeft (spacing);
    
    auto meltArea = row1.removeFromLeft (sliderSize);
    meltLabel.setBounds (meltArea.removeFromTop (labelHeight));
    meltSlider.setBounds (meltArea);
    row1.removeFromLeft (spacing);
    
    auto ghostArea = row1.removeFromLeft (sliderSize);
    ghostLabel.setBounds (ghostArea.removeFromTop (labelHeight));
    ghostSlider.setBounds (ghostArea);
    
    sliderArea.removeFromTop (spacing);
    
    // Row 2: Depth, Clarity, Gravity (3 sliders)
    auto row2 = sliderArea.removeFromTop (sliderSize + labelHeight + 8);
    auto depthArea = row2.removeFromLeft (sliderSize);
    depthLabel.setBounds (depthArea.removeFromTop (labelHeight));
    depthSlider.setBounds (depthArea);
    row2.removeFromLeft (spacing);
    
    auto clarityArea = row2.removeFromLeft (sliderSize);
    clarityLabel.setBounds (clarityArea.removeFromTop (labelHeight));
    claritySlider.setBounds (clarityArea);
    row2.removeFromLeft (spacing);
    
    auto gravityArea = row2.removeFromLeft (sliderSize);
    gravityLabel.setBounds (gravityArea.removeFromTop (labelHeight));
    gravitySlider.setBounds (gravityArea);
    
    sliderArea.removeFromTop (spacing);
    
    // Row 3: Energy, Mix, Output (3 sliders)
    auto row3 = sliderArea.removeFromTop (sliderSize + labelHeight + 8);
    auto energyArea = row3.removeFromLeft (sliderSize);
    energyLabel.setBounds (energyArea.removeFromTop (labelHeight));
    energySlider.setBounds (energyArea);
    row3.removeFromLeft (spacing);
    
    auto mixArea = row3.removeFromLeft (sliderSize);
    mixLabel.setBounds (mixArea.removeFromTop (labelHeight));
    mixSlider.setBounds (mixArea);
    row3.removeFromLeft (spacing);
    
    auto outputArea = row3.removeFromLeft (sliderSize);
    outputLabel.setBounds (outputArea.removeFromTop (labelHeight));
    outputSlider.setBounds (outputArea);

    // Position help buttons next to labels
    positionHelpButtons();
    
    lastUIWidth  = getWidth();
    lastUIHeight = getHeight();
}

void JuceDemoPluginAudioProcessorEditor::setupHelpButtons()
{
    HelpButton* buttons[] = { &flowHelpButton, &meltHelpButton, &ghostHelpButton, &depthHelpButton,
                             &clarityHelpButton, &gravityHelpButton, &energyHelpButton,
                             &mixHelpButton, &outputHelpButton };
    
    for (auto* button : buttons)
    {
        addAndMakeVisible (button);
        button->setHelpText ("", ""); // Will be set below
    }
    
    // Flow - управляет скоростью движения звука
    flowHelpButton.setHelpText (
        UTF8_STRING("Flow — Скорость движения"),
        UTF8_STRING(
            "Flow управляет скоростью движения звука в пространстве.\n\n"
            "• При 0% — звук статичен, эффект выключен\n"
            "• При 50% — медленное «дыхание» (LFO ~0.05 Гц, цикл ~20 сек)\n"
            "• При 100% — заметное «дыхание океана» (LFO ~0.08 Гц, цикл ~12.5 сек)\n\n"
            "Влияет на:\n"
            "• BinauralFlow: скорость LFO для фазовой модуляции (0.03-0.08 Гц)\n"
            "• MotionMod: частота LFO для панорамы/громкости (требует Energy > 0%)\n"
            "• SpaceEngine: ширина стерео-поля реверба\n\n"
            "💡 BinauralFlow работает БЕЗ панорамы — создаёт «дыхание пространства» через фазовые сдвиги.\n\n"
            "Создаёт ощущение «плывущего пространства», как дыхание холода."
        )
    );
    
    // Energy - амплитуда модуляции
    energyHelpButton.setHelpText (
        UTF8_STRING("Energy — Сила движения"),
        UTF8_STRING(
            "Energy контролирует силу движения звука.\n\n"
            "• При 0% — нет движения (даже если Flow > 0%)\n"
            "• При 50% — умеренное движение панорамы и громкости\n"
            "• При 100% — максимальная амплитуда движения\n\n"
            "Влияет на: силу модуляции панорамы (±28%) и громкости (±10%), минимальную частоту LFO.\n\n"
            "💡 Работает даже при Flow = 0% — создаёт очень медленное движение.\n\n"
            "Максимальный эффект: Flow = 100% + Energy = 100%."
        )
    );
    
    // Ghost - реверб и фазовая модуляция
    ghostHelpButton.setHelpText (
        UTF8_STRING("Ghost — Отражения и фазовая модуляция"),
        UTF8_STRING(
            "Ghost добавляет отражения голоса и фазовую модуляцию на верхах.\n\n"
            "• При 0% — нет реверба, нет фазовой модуляции на верхах\n"
            "• При 50% — умеренные отражения, лёгкая фазовая модуляция\n"
            "• При 100% — максимальная плотность отражений, полная фазовая модуляция\n\n"
            "Влияет на:\n"
            "• SpaceEngine: wet level реверба (плотность отражений)\n"
            "• BinauralFlow: фазовая модуляция на верхах (5-12 кГц, ±5-10°)\n\n"
            "💡 BinauralFlow: Ghost создаёт ощущение «эхо, обтекающего голову» через фазовую модуляцию только на высоких частотах.\n\n"
            "Создаёт ощущение «эха замерзшего дыхания», холодные отражения в пространстве."
        )
    );
    
    // Depth - размер пространства
    depthHelpButton.setHelpText (
        UTF8_STRING("Depth — Глубина пространства"),
        UTF8_STRING(
            "Depth управляет размером и глубиной пространства.\n\n"
            "• При 0% — маленькая комната, близкий звук\n"
            "• При 50% — среднее пространство\n"
            "• При 100% — огромная «бездна», глубокий звук\n\n"
            "Влияет на:\n"
            "• BinauralFlow: амплитуда фазового сдвига (5-10 градусов)\n"
            "• SpaceEngine: размер комнаты (room size), предзадержка реверба\n"
            "• SpectralEngine: низко-средние частоты (low-mid bell filter)\n\n"
            "💡 BinauralFlow: Depth = 0% → минимальный фазовый сдвиг (5°), Depth = 100% → максимальный (10°).\n\n"
            "Создаёт ощущение «холодной дали», от близкого звука до «глубоко подо льдом»."
        )
    );
    
    // Melt - будет для гранул
    meltHelpButton.setHelpText (
        UTF8_STRING("Melt — Растворение формы"),
        UTF8_STRING(
            "Melt смешивает сухой и обработанный сигнал, создавая эффект «таяния».\n\n"
            "• При 0% — только сухой сигнал\n"
            "• При 50% — баланс между сухим и обработанным\n"
            "• При 100% — полностью обработанный звук, «ледяной туман»\n\n"
            "Влияет на: dry/wet mix, размывание спектра.\n\n"
            "⚠️ Пока в разработке (stub).\n\n"
            "Создаёт ощущение «смешения хвоста с оригиналом», как ледяной туман."
        )
    );
    
    // Clarity - спектральный баланс (SpectralEngine)
    clarityHelpButton.setHelpText (
        UTF8_STRING("Clarity — Чистота и блеск"),
        UTF8_STRING(
            "Clarity контролирует баланс между мутностью и яркостью спектра.\n\n"
            "• При -50% — мутный звук, «лёд без блеска» (снижение верхов до -6 дБ @ 8 кГц)\n"
            "• При 0% — нейтральный баланс, без изменений\n"
            "• При +50% — хрустальный блеск, прозрачность (подъем верхов до +6 дБ @ 8 кГц)\n\n"
            "Влияет на: high-shelf EQ (8 кГц), баланс верхов и формант, яркость спектра.\n\n"
            "💡 Работает независимо от других параметров — можно использовать с любыми настройками.\n\n"
            "Взаимодействие:\n"
            "• Clarity +50% + Depth высокий = хрустальный блеск + глубина (идеальный Iceberg)\n"
            "• Clarity -50% + Ghost высокий = мутный туман с отражениями\n"
            "• Clarity +30% + Flow 50% = блестящее «дыхание» пространства\n\n"
            "Работает через SpectralEngine: high-shelf фильтр для «воздуха» в верхней части спектра.\n\n"
            "Создаёт ощущение от «мутного льда» до «хрустального блеска» — контроль прозрачности звука."
        )
    );
    
    // Gravity - будет для динамики
    gravityHelpButton.setHelpText (
        UTF8_STRING("Gravity — Масса и плотность"),
        UTF8_STRING(
            "Gravity усиливает ощущение «массы» звука, его плотность.\n\n"
            "• При 0% — лёгкий, невесомый звук\n"
            "• При 50% — умеренная плотность\n"
            "• При 100% — максимальная «масса под водой»\n\n"
            "Влияет на: компрессию, сатурацию, низкие частоты.\n\n"
            "⚠️ Пока в разработке (stub).\n\n"
            "Создаёт ощущение «силы притяжения к низу», как масса под водой."
        )
    );
    
    // Mix - сухой/мокрый
    mixHelpButton.setHelpText (
        UTF8_STRING("Mix — Сухой / Обработанный"),
        UTF8_STRING(
            "Mix контролирует баланс между оригинальным и обработанным сигналом.\n\n"
            "• При 0% — только сухой сигнал (без эффектов)\n"
            "• При 50% — баланс 50/50\n"
            "• При 100% — только обработанный сигнал\n\n"
            "Влияет на: финальный dry/wet mix всего плагина.\n\n"
            "💡 Используй для точной настройки количества эффекта в миксе."
        )
    );
    
    // Output - выходная громкость
    outputHelpButton.setHelpText (
        UTF8_STRING("Output — Выходная громкость"),
        UTF8_STRING(
            "Output контролирует финальную громкость выходного сигнала.\n\n"
            "• При 0.0x — без звука\n"
            "• При 1.0x — нормальная громкость (0 дБ)\n"
            "• При 2.0x — удвоенная громкость (+6 дБ)\n\n"
            "Влияет на: финальную громкость после всех эффектов.\n\n"
            "💡 Используй для компенсации громкости или усиления эффекта."
        )
    );
}

void JuceDemoPluginAudioProcessorEditor::positionHelpButtons()
{
    const int buttonSize = 20;
    const int buttonOffsetX = 5;
    const int buttonOffsetY = 2;
    
    // Position help buttons next to their labels
    auto positionButton = [&] (HelpButton& button, juce::Label& label)
    {
        auto labelBounds = label.getBounds();
        button.setBounds (labelBounds.getRight() + buttonOffsetX,
                        labelBounds.getY() + buttonOffsetY,
                        buttonSize, buttonSize);
    };
    
    positionButton (flowHelpButton, flowLabel);
    positionButton (meltHelpButton, meltLabel);
    positionButton (ghostHelpButton, ghostLabel);
    positionButton (depthHelpButton, depthLabel);
    positionButton (clarityHelpButton, clarityLabel);
    positionButton (gravityHelpButton, gravityLabel);
    positionButton (energyHelpButton, energyLabel);
    positionButton (mixHelpButton, mixLabel);
    positionButton (outputHelpButton, outputLabel);
}

void JuceDemoPluginAudioProcessorEditor::timerCallback()
{
    updateTimecodeDisplay (getProcessor().lastPosInfo.get());
}

void JuceDemoPluginAudioProcessorEditor::hostMIDIControllerIsAvailable (bool controllerIsAvailable)
{
    juce::ignoreUnused (controllerIsAvailable);
}

int JuceDemoPluginAudioProcessorEditor::getControlParameterIndex (juce::Component& control)
{
    if (&control == &flowSlider) return 0;
    if (&control == &meltSlider) return 1;
    if (&control == &ghostSlider) return 2;
    if (&control == &depthSlider) return 3;
    if (&control == &claritySlider) return 4;
    if (&control == &gravitySlider) return 5;
    if (&control == &energySlider) return 6;
    if (&control == &mixSlider) return 7;
    if (&control == &outputSlider) return 8;

    return -1;
}

void JuceDemoPluginAudioProcessorEditor::updateTrackProperties()
{
    auto trackColour = getProcessor().getTrackProperties().colour;
    auto& lf = getLookAndFeel();

    backgroundColour = (trackColour.has_value() ? trackColour->withAlpha (1.0f).withBrightness (0.266f)
                                                : lf.findColour (juce::ResizableWindow::backgroundColourId));
    repaint();
}

//==============================================================================
JuceDemoPluginAudioProcessor& JuceDemoPluginAudioProcessorEditor::getProcessor() const
{
    return static_cast<JuceDemoPluginAudioProcessor&> (processor);
}

//==============================================================================
juce::String JuceDemoPluginAudioProcessorEditor::timeToTimecodeString (double seconds)
{
    auto millisecs = juce::roundToInt (seconds * 1000.0);
    auto absMillisecs = std::abs (millisecs);

    return juce::String::formatted ("%02d:%02d:%02d.%03d",
                                      millisecs / 3600000,
                                      (absMillisecs / 60000) % 60,
                                      (absMillisecs / 1000)  % 60,
                                      absMillisecs % 1000);
}

juce::String JuceDemoPluginAudioProcessorEditor::quarterNotePositionToBarsBeatsString (double quarterNotes, juce::AudioPlayHead::TimeSignature sig)
{
    if (sig.numerator == 0 || sig.denominator == 0)
        return "1|1|000";

    auto quarterNotesPerBar = (sig.numerator * 4 / sig.denominator);
    auto beats  = (fmod (quarterNotes, quarterNotesPerBar) / quarterNotesPerBar) * sig.numerator;

    auto bar    = ((int) quarterNotes) / quarterNotesPerBar + 1;
    auto beat   = ((int) beats) + 1;
    auto ticks  = ((int) (fmod (beats, 1.0) * 960.0 + 0.5));

    return juce::String::formatted ("%d|%d|%03d", bar, beat, ticks);
}

void JuceDemoPluginAudioProcessorEditor::updateTimecodeDisplay (const juce::AudioPlayHead::PositionInfo& pos)
{
    juce::MemoryOutputStream displayText;

    const auto sig = pos.getTimeSignature().orFallback (juce::AudioPlayHead::TimeSignature{});

    displayText << "[" << juce::SystemStats::getJUCEVersion() << "]   "
                << juce::String (pos.getBpm().orFallback (120.0), 2) << " bpm, "
                << sig.numerator << '/' << sig.denominator
                << "  -  " << timeToTimecodeString (pos.getTimeInSeconds().orFallback (0.0))
                << "  -  " << quarterNotePositionToBarsBeatsString (pos.getPpqPosition().orFallback (0.0), sig);

    if (pos.getIsRecording())
        displayText << "  (recording)";
    else if (pos.getIsPlaying())
        displayText << "  (playing)";

    timecodeDisplayLabel.setText (displayText.toString(), juce::dontSendNotification);
}

void JuceDemoPluginAudioProcessorEditor::valueChanged (juce::Value&)
{
    setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
}
