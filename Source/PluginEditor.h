/*
  ==============================================================================

   AudioPluginDemo Editor - постепенная модификация
   Этап 0: Завершен - GUI для всех параметров

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include "HelpTooltip.h"

//==============================================================================
/** This is the editor component that our filter will display. */
class JuceDemoPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer,
                                                  private juce::Value::Listener
{
public:
    explicit JuceDemoPluginAudioProcessorEditor (JuceDemoPluginAudioProcessor& owner);
    ~JuceDemoPluginAudioProcessorEditor() override = default;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    void hostMIDIControllerIsAvailable (bool controllerIsAvailable) override;
    int getControlParameterIndex (juce::Component& control) override;

    void updateTrackProperties();
    
    void setupHelpButtons();
    void positionHelpButtons();

private:
    void timerCallback() override;
    void valueChanged (juce::Value&) override;

    JuceDemoPluginAudioProcessor& getProcessor() const;

    static juce::String timeToTimecodeString (double seconds);
    static juce::String quarterNotePositionToBarsBeatsString (double quarterNotes, juce::AudioPlayHead::TimeSignature sig);
    void updateTimecodeDisplay (const juce::AudioPlayHead::PositionInfo& pos);

    juce::Label timecodeDisplayLabel;
    
    // Parameter labels
    juce::Label flowLabel { {}, "Flow:" },
                meltLabel { {}, "Melt:" },
                ghostLabel { {}, "Ghost:" },
                depthLabel { {}, "Depth:" },
                clarityLabel { {}, "Clarity:" },
                gravityLabel { {}, "Gravity:" },
                energyLabel { {}, "Energy:" },
                mixLabel { {}, "Mix:" },
                outputLabel { {}, "Output:" };
    
    // Parameter sliders
    juce::Slider flowSlider, meltSlider, ghostSlider, depthSlider,
                 claritySlider, gravitySlider, energySlider,
                 mixSlider, outputSlider;
    
    // Parameter attachments
    juce::AudioProcessorValueTreeState::SliderAttachment flowAttachment,
                                                          meltAttachment,
                                                          ghostAttachment,
                                                          depthAttachment,
                                                          clarityAttachment,
                                                          gravityAttachment,
                                                          energyAttachment,
                                                          mixAttachment,
                                                          outputAttachment;
    
    juce::Colour backgroundColour;

    juce::Value lastUIWidth, lastUIHeight;
    
    // Help buttons for each parameter
    HelpButton flowHelpButton, meltHelpButton, ghostHelpButton, depthHelpButton,
              clarityHelpButton, gravityHelpButton, energyHelpButton,
              mixHelpButton, outputHelpButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceDemoPluginAudioProcessorEditor)
};
