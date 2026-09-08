#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <memory>

class TapeStopperAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit TapeStopperAudioProcessorEditor (TapeStopperAudioProcessor&);
    ~TapeStopperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateTriggerModeText();
    void updateTimingModeText();
    void updateMuteAtText();
    void updateBottomControlText();
    void updateMotionButtonColours();
    void updateRetriggerButtonColour();
    void savePortableSettings();
    void showSetupPanel (bool shouldShow);
    void showSequencerPanel (bool shouldShow);

    TapeStopperAudioProcessor& processor;
    std::unique_ptr<juce::LookAndFeel_V4> smallButtonLookAndFeel;
    std::unique_ptr<juce::LookAndFeel_V4> muteAtLookAndFeel;

    juce::TextButton setupButton { "SETUP" };
    juce::TextButton upButton { "UP" };
    juce::TextButton downButton { "DOWN" };
    juce::TextButton triggerModeButton;
    juce::TextButton timingModeButton;
    juce::TextButton retriggerButton { "RETRIG" };
    juce::TextButton envelopeResetButton { "ENV RESET" };
    juce::TextButton sequencerViewButton { "SEQ" };
    juce::Slider muteAtSlider;
    juce::Slider filterAmountSlider;
    juce::Slider volumeAmountSlider;
    juce::Slider driveSlider;
    juce::Slider wowSlider;
    juce::Slider flutterSlider;
    juce::Slider fluxSlider;
    juce::Slider mixSlider;
    juce::Label muteAtValueLabel;
    juce::Label filterAmountValueLabel;
    juce::Label volumeAmountValueLabel;
    juce::Label driveValueLabel;
    juce::Label wowValueLabel;
    juce::Label flutterValueLabel;
    juce::Label fluxValueLabel;
    juce::Label mixValueLabel;

    std::unique_ptr<juce::Component> mainTrigger;
    std::unique_ptr<juce::Component> timingBar;
    std::unique_ptr<juce::Component> envelopeEditor;
    std::unique_ptr<juce::Component> setupPanel;
    std::unique_ptr<juce::Component> presetSection;
    std::unique_ptr<juce::Component> sequencerEnableLed;
    std::unique_ptr<juce::Component> sequencerPanel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> upAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> downAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> muteAtAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        filterAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        volumeAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fluxAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    int observedEditorWidth = 0;
    int savedEditorWidth = 0;
    int stableResizeTicks = 0;
    bool showingSetup = false;
    bool retriggerGestureActive = false;
    bool showingSequencer = false;

    static constexpr int designWidth = 800;
    static constexpr int designHeight = 300;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeStopperAudioProcessorEditor)
};
