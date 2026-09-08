#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
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
    bool isWaveformDisplayEnabled() const noexcept;
    bool isRetriggerActive() const noexcept;
    bool isSequencerGateActive() const noexcept;
    int getCurrentSequencerStep() const noexcept;
    void setFullSpeedMuteEnabled (bool enabled) noexcept;
    void setButtonDisplayReversed (bool reversed) noexcept;
    void setWaveformDisplayEnabled (bool enabled) noexcept;
    void requestRetrigger() noexcept;
    static constexpr int waveformSampleCount = 256;
    void copyWaveformSamples
        (std::array<float, waveformSampleCount>& destination) const noexcept;

    static float speedControlToSeconds (float controlValue) noexcept;
    static float syncDivisionToSeconds (int divisionIndex, float bpm) noexcept;
    static juce::String syncDivisionName (int divisionIndex);
    static juce::String sequencerResolutionName (int resolutionIndex);
    static constexpr int numSyncDivisions = 13;
    static constexpr int numSequencerSteps = 64;
    static constexpr int numSequencerResolutions = 11;
    static constexpr int numEnvelopePoints = 11;
    static constexpr int numCurveControlPoints = 5;

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
    void resetWaveformTrace (float speed, float position) noexcept;
    void recordWaveformTrace (float speed, float position) noexcept;
    float readTapeSample (int channel) const noexcept;
    float readCharacterSample (int channel, double delayInSamples) const noexcept;
    float nextFluxRandomUnit() noexcept;

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
    float currentFluxAmount = 0.0f;
    float currentMixAmount = 1.0f;
    float characterSmoothingAmount = 1.0f;
    std::vector<float> envelopeFilterStates;
    float currentEnvelopeFilterAmount = 0.0f;
    float currentEnvelopeVolumeGain = 1.0f;
    float currentPitchCurveMix = 1.0f;
    float envelopeEffectSmoothingAmount = 1.0f;

    float currentFluxVariation = 0.0f;
    float targetFluxVariation = 0.0f;
    float fluxVariationSmoothingAmount = 1.0f;
    int fluxTargetSamplesRemaining = 0;
    std::uint32_t fluxRandomState = 0x7f4a7c15u;

    MotionState motionState = MotionState::fullSpeed;
    float currentSpeed = 1.0f;
    float transitionStartPosition = 0.0f;
    int transitionPosition = 0;
    int transitionLength = 1;
    int reentryPosition = 0;
    int reentryLength = 1;
    bool previousEngage = false;
    float currentMuteGain = 1.0f;
    float muteSmoothingAmount = 1.0f;
    bool previousRetriggerParameterHigh = false;
    bool suppressEngageUntilReleased = false;

    std::atomic<float>* retriggerParameterValue = nullptr;
    std::atomic<float>* fullSpeedMuteParameterValue = nullptr;
    std::atomic<float>* envelopeEnabledValue = nullptr;
    std::array<std::atomic<float>*, numEnvelopePoints - 2> envelopeXValues {};
    std::array<std::atomic<float>*, numEnvelopePoints> envelopeYValues {};
    std::array<std::atomic<float>*, numCurveControlPoints> pitchDownCurveValues {};
    std::array<std::atomic<float>*, numCurveControlPoints> pitchUpCurveValues {};
    std::array<std::atomic<float>*, numCurveControlPoints> filterDownCurveValues {};
    std::array<std::atomic<float>*, numCurveControlPoints> filterUpCurveValues {};
    std::array<std::atomic<float>*, numCurveControlPoints> volumeDownCurveValues {};
    std::array<std::atomic<float>*, numCurveControlPoints> volumeUpCurveValues {};
    std::array<std::atomic<float>*, numSequencerSteps> sequencerStepValues {};

    double freeSequencerSamplePosition = 0.0;
    bool previousHostPlaying = false;

    std::atomic<float> visualPosition { 0.0f };
    std::atomic<MotionDirection> motionDirection { MotionDirection::inactive };
    std::atomic<float> currentBpm { 120.0f };
    std::atomic<bool> buttonDisplayReversed { false };
    std::atomic<bool> waveformDisplayEnabled { true };
    std::array<std::atomic<float>, waveformSampleCount> waveformSamples {};
    std::atomic<bool> retriggerRequested { false };
    std::atomic<bool> retriggerActive { false };
    std::atomic<bool> sequencerGateActive { false };
    std::atomic<int> currentSequencerStep { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeStopperAudioProcessor)
};
