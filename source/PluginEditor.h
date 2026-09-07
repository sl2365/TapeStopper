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
    void updateEnvelopeButtonText();
    void updateMuteAtText();
    void updateBottomControlText();
    void updateMotionButtonColours();
    void savePortableSettings();
    void showSetupPanel (bool shouldShow);

    TapeStopperAudioProcessor& processor;
    std::unique_ptr<juce::LookAndFeel_V4> smallButtonLookAndFeel;
    std::unique_ptr<juce::LookAndFeel_V4> muteAtLookAndFeel;

    juce::TextButton setupButton { "SETUP" };
    juce::TextButton upButton { "UP" };
    juce::TextButton downButton { "DOWN" };
    juce::TextButton triggerModeButton;
    juce::TextButton timingModeButton;
    juce::TextButton envelopeButton;
    juce::TextButton envelopeResetButton { "ENV RESET" };
    juce::TextButton downCurveButton;
    juce::TextButton upCurveButton;
    juce::Slider muteAtSlider;
    juce::Slider driveSlider;
    juce::Slider wowSlider;
    juce::Slider flutterSlider;
    juce::Slider mixSlider;
    juce::Label muteAtValueLabel;
    juce::Label driveValueLabel;
    juce::Label wowValueLabel;
    juce::Label flutterValueLabel;
    juce::Label mixValueLabel;

    std::unique_ptr<juce::Component> mainTrigger;
    std::unique_ptr<juce::Component> timingBar;
    std::unique_ptr<juce::Component> envelopeEditor;
    std::unique_ptr<juce::Component> setupPanel;
    std::unique_ptr<juce::Component> presetSection;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> upAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> downAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> envelopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> muteAtAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    int observedEditorWidth = 0;
    int savedEditorWidth = 0;
    int stableResizeTicks = 0;
    bool showingSetup = false;

    static constexpr int designWidth = 800;
    static constexpr int designHeight = 300;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeStopperAudioProcessorEditor)
};
