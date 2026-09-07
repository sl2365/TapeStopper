#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class TapeStopperAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class MotionDirection
    {
        inactive,
        down,
        up
    };

    TapeStopperAudioProcessor();
    ~TapeStopperAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    float getVisualPosition() const noexcept;
    MotionDirection getMotionDirection() const noexcept;
    bool isFullSpeedMuteEnabled() const noexcept;
    bool isButtonDisplayReversed() const noexcept;
    void setFullSpeedMuteEnabled (bool enabled) noexcept;
    void setButtonDisplayReversed (bool reversed) noexcept;

    static float speedControlToSeconds (float controlValue) noexcept;
    static float syncDivisionToSeconds (int divisionIndex, float bpm) noexcept;
    static juce::String syncDivisionName (int divisionIndex);
    static constexpr int numSyncDivisions = 13;
    static constexpr int numEnvelopePoints = 11;

private:
    enum class MotionState
    {
        fullSpeed,
        slowing,
        stopped,
        speeding,
        reentering
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void beginSlowdown (bool enabled);
    void beginSpeedup (bool enabled);
    void beginReentry();
    void advanceMotionState();
    float readTapeSample (int channel) const noexcept;
    float readCharacterSample (int channel, double delayInSamples) const noexcept;

    juce::AudioProcessorValueTreeState parameters;

    std::vector<std::vector<float>> tapeBuffer;
    int tapeBufferSize = 0;
    int writePosition = 0;
    double readPosition = 0.0;
    double currentSampleRate = 48000.0;

    std::vector<std::vector<float>> characterBuffer;
    int characterBufferSize = 0;
    int characterWritePosition = 0;
    double wowPhase = 0.0;
    double flutterPhase = 0.0;
    float currentDriveAmount = 0.0f;
    float currentWowAmount = 0.0f;
    float currentFlutterAmount = 0.0f;
    float currentMixAmount = 1.0f;
    float characterSmoothingAmount = 1.0f;

    MotionState motionState = MotionState::fullSpeed;
    float currentSpeed = 1.0f;
    float transitionStartSpeed = 1.0f;
    int transitionPosition = 0;
    int transitionLength = 1;
    int transitionCurveIndex = 0;
    int reentryPosition = 0;
    int reentryLength = 1;
    bool previousEngage = false;
    float currentMuteGain = 1.0f;
    float muteSmoothingAmount = 1.0f;

    std::atomic<float>* envelopeEnabledValue = nullptr;
    std::array<std::atomic<float>*, numEnvelopePoints - 2> envelopeXValues {};
    std::array<std::atomic<float>*, numEnvelopePoints> envelopeYValues {};

    std::atomic<float> visualPosition { 0.0f };
    std::atomic<MotionDirection> motionDirection { MotionDirection::inactive };
    std::atomic<float> currentBpm { 120.0f };
    std::atomic<bool> fullSpeedMuteEnabled { false };
    std::atomic<bool> buttonDisplayReversed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeStopperAudioProcessor)
};
