#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PortableSettings.h"

#include <cmath>

namespace
{
constexpr auto engageParameterId = "engage";
constexpr auto triggerModeParameterId = "triggerMode";
constexpr auto downEnabledParameterId = "downEnabled";
constexpr auto upEnabledParameterId = "upEnabled";
constexpr auto downSpeedParameterId = "downSpeed";
constexpr auto upSpeedParameterId = "upSpeed";
constexpr auto muteAtParameterId = "muteAt";
constexpr auto timingModeParameterId = "timingMode";
constexpr auto downSyncDivisionParameterId = "downSyncDivision";
constexpr auto upSyncDivisionParameterId = "upSyncDivision";
constexpr auto envelopeEnabledParameterId = "envelopeEnabled";
constexpr auto downCurveParameterId = "downCurve";
constexpr auto upCurveParameterId = "upCurve";
constexpr auto driveParameterId = "drive";
constexpr auto wowParameterId = "wow";
constexpr auto flutterParameterId = "flutter";
constexpr auto fluxParameterId = "flux";
constexpr auto mixParameterId = "mix";

juce::String envelopeXParameterId (int pointIndex)
{
    return "envX" + juce::String (pointIndex);
}

juce::String envelopeYParameterId (int pointIndex)
{
    return "envY" + juce::String (pointIndex);
}

juce::StringArray makeSyncDivisionNames()
{
    return { "4 BAR", "2 BAR", "1 BAR", "1/2", "1/3",
             "1/4", "1/6", "1/8", "1/16", "1/24", "1/32",
             "1/48", "1/64" };
}

float shapeTransition (float progress, int curveIndex) noexcept
{
    const auto p = juce::jlimit (0.0f, 1.0f, progress);

    switch (curveIndex)
    {
        case 1:  return p * p;                         // Gentle start.
        case 2:  return 1.0f - (1.0f - p) * (1.0f - p); // Steep start.
        case 3:  return p * p * (3.0f - 2.0f * p);    // Smooth S-curve.
        default: return p;
    }
}
}

TapeStopperAudioProcessor::TapeStopperAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    const auto portableSettings = TapeStopperPortableSettings::load();
    fullSpeedMuteEnabled.store (portableSettings.fullSpeedMute,
                                std::memory_order_relaxed);
    buttonDisplayReversed.store (portableSettings.reversedButtonDisplay,
                                 std::memory_order_relaxed);
    waveformDisplayEnabled.store (portableSettings.waveformDisplay,
                                  std::memory_order_relaxed);

    for (auto& sample : waveformSamples)
        sample.store (0.0f, std::memory_order_relaxed);

    envelopeEnabledValue = parameters.getRawParameterValue (envelopeEnabledParameterId);

    for (int point = 1; point < numEnvelopePoints - 1; ++point)
        envelopeXValues[static_cast<size_t> (point - 1)]
            = parameters.getRawParameterValue (envelopeXParameterId (point));

    for (int point = 0; point < numEnvelopePoints; ++point)
        envelopeYValues[static_cast<size_t> (point)]
            = parameters.getRawParameterValue (envelopeYParameterId (point));
}

juce::AudioProcessorValueTreeState::ParameterLayout
TapeStopperAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { engageParameterId, 1 }, "Start / Stop", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { triggerModeParameterId, 1 }, "Button Mode",
                 juce::StringArray { "Momentary", "Toggle" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { downEnabledParameterId, 1 }, "Down Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { upEnabledParameterId, 1 }, "Up Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { downSpeedParameterId, 1 }, "Down Speed",
                 juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.20f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { upSpeedParameterId, 1 }, "Up Speed",
                 juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.20f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { muteAtParameterId, 1 }, "Mute At",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 90.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { timingModeParameterId, 1 }, "Timing Mode",
                 juce::StringArray { "Free", "Sync" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { downSyncDivisionParameterId, 1 }, "Down Sync Division",
                 makeSyncDivisionNames(), 5));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { upSyncDivisionParameterId, 1 }, "Up Sync Division",
                 makeSyncDivisionNames(), 5));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { downCurveParameterId, 1 }, "Down Curve",
                 juce::StringArray { "Linear", "Gentle", "Steep", "S-Curve" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { upCurveParameterId, 1 }, "Up Curve",
                 juce::StringArray { "Linear", "Gentle", "Steep", "S-Curve" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { driveParameterId, 1 }, "Drive",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { wowParameterId, 1 }, "Wow",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { flutterParameterId, 1 }, "Flutter",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { fluxParameterId, 1 }, "Flux",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { mixParameterId, 1 }, "Mix",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { envelopeEnabledParameterId, 1 },
                 "Envelope Enabled", false));

    for (int point = 1; point < numEnvelopePoints - 1; ++point)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat>
                    (juce::ParameterID { envelopeXParameterId (point), 1 },
                     "Envelope Point " + juce::String (point + 1) + " Time",
                     juce::NormalisableRange<float> { 0.0f, 1.0f },
                     static_cast<float> (point) / static_cast<float> (numEnvelopePoints - 1)));
    }

    for (int point = 0; point < numEnvelopePoints; ++point)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat>
                    (juce::ParameterID { envelopeYParameterId (point), 1 },
                     "Envelope Point " + juce::String (point + 1) + " Pitch",
                     juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
    }

    return layout;
}

float TapeStopperAudioProcessor::speedControlToSeconds (float controlValue) noexcept
{
    const auto speed = juce::jlimit (0.0f, 1.0f, controlValue);
    // The curve reaches 8 seconds at the far-left position while retaining
    // the approved 20% default at approximately 0.71 seconds.
    return 0.05f + 7.95f * std::pow (1.0f - speed, 11.152854f);
}

float TapeStopperAudioProcessor::syncDivisionToSeconds (int divisionIndex,
                                                         float bpm) noexcept
{
    static constexpr float quarterNoteMultipliers[numSyncDivisions]
        { 16.0f, 8.0f, 4.0f, 2.0f, 4.0f / 3.0f,
          1.0f, 2.0f / 3.0f, 0.5f, 0.25f, 1.0f / 6.0f, 0.125f,
          1.0f / 12.0f, 0.0625f };

    const auto safeIndex = juce::jlimit (0, numSyncDivisions - 1, divisionIndex);
    const auto safeBpm = juce::jlimit (20.0f, 400.0f, bpm);
    return (60.0f / safeBpm) * quarterNoteMultipliers[safeIndex];
}

juce::String TapeStopperAudioProcessor::syncDivisionName (int divisionIndex)
{
    return makeSyncDivisionNames()[juce::jlimit (0, numSyncDivisions - 1, divisionIndex)];
}

void TapeStopperAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    tapeBufferSize = juce::jmax (2, juce::roundToInt (currentSampleRate * 30.0));
    tapeBuffer.assign (static_cast<size_t> (juce::jmax (1, getTotalNumInputChannels())),
                       std::vector<float> (static_cast<size_t> (tapeBufferSize), 0.0f));
    characterBufferSize = juce::jmax (2, juce::roundToInt (currentSampleRate * 0.030));
    characterBuffer.assign
        (static_cast<size_t> (juce::jmax (1, getTotalNumInputChannels())),
         std::vector<float> (static_cast<size_t> (characterBufferSize), 0.0f));

    writePosition = 0;
    readPosition = 0.0;
    characterWritePosition = 0;
    wowPhase = 0.0;
    flutterPhase = 0.0;
    currentFluxVariation = 0.0f;
    targetFluxVariation = 0.0f;
    fluxTargetSamplesRemaining = 0;
    fluxRandomState = 0x7f4a7c15u;
    motionState = MotionState::fullSpeed;
    currentSpeed = 1.0f;
    transitionStartSpeed = 1.0f;
    transitionPosition = 0;
    transitionLength = 1;
    transitionCurveIndex = 0;
    reentryPosition = 0;
    reentryLength = juce::jmax (1, juce::roundToInt (currentSampleRate * 0.020));
    previousEngage = false;
    currentMuteGain = fullSpeedMuteEnabled.load (std::memory_order_relaxed) ? 0.0f : 1.0f;
    muteSmoothingAmount = 1.0f - std::exp (-1.0f
                                           / static_cast<float> (currentSampleRate * 0.005));
    characterSmoothingAmount = 1.0f - std::exp
                               (-1.0f / static_cast<float> (currentSampleRate * 0.020));
    fluxVariationSmoothingAmount = 1.0f - std::exp
                                   (-1.0f / static_cast<float> (currentSampleRate * 0.008));
    currentDriveAmount = parameters.getRawParameterValue (driveParameterId)->load() / 100.0f;
    currentWowAmount = parameters.getRawParameterValue (wowParameterId)->load() / 100.0f;
    currentFlutterAmount = parameters.getRawParameterValue (flutterParameterId)->load() / 100.0f;
    currentFluxAmount = parameters.getRawParameterValue (fluxParameterId)->load() / 100.0f;
    currentMixAmount = parameters.getRawParameterValue (mixParameterId)->load() / 100.0f;
    visualPosition.store (0.0f, std::memory_order_relaxed);
    motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
    currentBpm.store (120.0f, std::memory_order_relaxed);
    waveformCaptureInterval = juce::jmax
                              (1, juce::roundToInt (currentSampleRate / 2000.0));
    waveformSamplesUntilCapture = waveformCaptureInterval;
    waveformWritePosition.store (0, std::memory_order_relaxed);
    for (auto& sample : waveformSamples)
        sample.store (0.0f, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::releaseResources()
{
}

bool TapeStopperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void TapeStopperAudioProcessor::beginSlowdown (bool enabled)
{
    if (! enabled)
    {
        motionState = MotionState::stopped;
        currentSpeed = 0.0f;
        visualPosition.store (1.0f, std::memory_order_relaxed);
        motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
        return;
    }

    transitionStartSpeed = currentSpeed;
    transitionPosition = 0;
    transitionCurveIndex = juce::roundToInt
                           (parameters.getRawParameterValue
                                (downCurveParameterId)->load());

    const auto isSynced = parameters.getRawParameterValue (timingModeParameterId)->load() >= 0.5f;
    const auto durationSeconds = isSynced
                                     ? syncDivisionToSeconds
                                           (juce::roundToInt (parameters.getRawParameterValue
                                               (downSyncDivisionParameterId)->load()),
                                            currentBpm.load (std::memory_order_relaxed))
                                     : speedControlToSeconds
                                           (parameters.getRawParameterValue
                                               (downSpeedParameterId)->load());
    const auto fullLength = durationSeconds * currentSampleRate;
    transitionLength = juce::jmax (1, juce::roundToInt (fullLength * currentSpeed));
    motionState = MotionState::slowing;
    motionDirection.store (MotionDirection::down, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::beginSpeedup (bool enabled)
{
    if (! enabled)
    {
        motionState = MotionState::fullSpeed;
        currentSpeed = 1.0f;
        readPosition = static_cast<double> (writePosition);
        visualPosition.store (0.0f, std::memory_order_relaxed);
        motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
        return;
    }

    transitionStartSpeed = currentSpeed;
    transitionPosition = 0;
    transitionCurveIndex = juce::roundToInt
                           (parameters.getRawParameterValue
                                (upCurveParameterId)->load());

    const auto isSynced = parameters.getRawParameterValue (timingModeParameterId)->load() >= 0.5f;
    const auto durationSeconds = isSynced
                                     ? syncDivisionToSeconds
                                           (juce::roundToInt (parameters.getRawParameterValue
                                               (upSyncDivisionParameterId)->load()),
                                            currentBpm.load (std::memory_order_relaxed))
                                     : speedControlToSeconds
                                           (parameters.getRawParameterValue
                                               (upSpeedParameterId)->load());
    const auto fullLength = durationSeconds * currentSampleRate;
    transitionLength = juce::jmax (1, juce::roundToInt (fullLength * (1.0f - currentSpeed)));
    motionState = MotionState::speeding;
    motionDirection.store (MotionDirection::up, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::beginReentry()
{
    currentSpeed = 1.0f;
    reentryPosition = 0;
    motionState = MotionState::reentering;
    motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::advanceMotionState()
{
    switch (motionState)
    {
        case MotionState::slowing:
        {
            ++transitionPosition;
            const auto progress = juce::jlimit (0.0f, 1.0f,
                                                static_cast<float> (transitionPosition)
                                                    / static_cast<float> (transitionLength));
            const auto shapedProgress = shapeTransition (progress, transitionCurveIndex);
            currentSpeed = transitionStartSpeed * (1.0f - shapedProgress);
            visualPosition.store (1.0f - currentSpeed, std::memory_order_relaxed);

            if (transitionPosition >= transitionLength)
            {
                motionState = MotionState::stopped;
                currentSpeed = 0.0f;
                visualPosition.store (1.0f, std::memory_order_relaxed);
                motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
            }
            break;
        }

        case MotionState::speeding:
        {
            ++transitionPosition;
            const auto progress = juce::jlimit (0.0f, 1.0f,
                                                static_cast<float> (transitionPosition)
                                                    / static_cast<float> (transitionLength));
            const auto shapedProgress = shapeTransition (progress, transitionCurveIndex);
            currentSpeed = transitionStartSpeed
                           + (1.0f - transitionStartSpeed) * shapedProgress;
            visualPosition.store (1.0f - currentSpeed, std::memory_order_relaxed);

            if (transitionPosition >= transitionLength)
                beginReentry();
            break;
        }

        case MotionState::reentering:
            ++reentryPosition;
            if (reentryPosition >= reentryLength)
            {
                motionState = MotionState::fullSpeed;
                currentSpeed = 1.0f;
                readPosition = static_cast<double> (writePosition);
                visualPosition.store (0.0f, std::memory_order_relaxed);
            }
            break;

        case MotionState::fullSpeed:
        case MotionState::stopped:
            break;
    }
}

float TapeStopperAudioProcessor::readTapeSample (int channel) const noexcept
{
    if (tapeBufferSize < 2 || channel < 0
        || channel >= static_cast<int> (tapeBuffer.size()))
        return 0.0f;

    const auto position = readPosition >= 0.0 ? readPosition
                                               : readPosition + tapeBufferSize;
    const auto first = static_cast<int> (position) % tapeBufferSize;
    const auto second = (first + 1) % tapeBufferSize;
    const auto fraction = static_cast<float> (position - std::floor (position));
    const auto& channelBuffer = tapeBuffer[static_cast<size_t> (channel)];

    return channelBuffer[static_cast<size_t> (first)]
           + fraction * (channelBuffer[static_cast<size_t> (second)]
                         - channelBuffer[static_cast<size_t> (first)]);
}

float TapeStopperAudioProcessor::readCharacterSample (int channel,
                                                       double delayInSamples) const noexcept
{
    if (characterBufferSize < 2 || channel < 0
        || channel >= static_cast<int> (characterBuffer.size()))
        return 0.0f;

    auto position = static_cast<double> (characterWritePosition) - delayInSamples;
    while (position < 0.0)
        position += characterBufferSize;
    while (position >= characterBufferSize)
        position -= characterBufferSize;

    const auto first = static_cast<int> (position) % characterBufferSize;
    const auto second = (first + 1) % characterBufferSize;
    const auto fraction = static_cast<float> (position - std::floor (position));
    const auto& channelBuffer = characterBuffer[static_cast<size_t> (channel)];

    return channelBuffer[static_cast<size_t> (first)]
           + fraction * (channelBuffer[static_cast<size_t> (second)]
                         - channelBuffer[static_cast<size_t> (first)]);
}

float TapeStopperAudioProcessor::nextFluxRandomUnit() noexcept
{
    // A small deterministic xorshift generator avoids allocations and locks on
    // the real-time audio thread. The upper 24 bits are mapped to 0.0 ... 1.0.
    fluxRandomState ^= fluxRandomState << 13;
    fluxRandomState ^= fluxRandomState >> 17;
    fluxRandomState ^= fluxRandomState << 5;
    return static_cast<float> ((fluxRandomState >> 8) & 0x00ffffffu)
           / static_cast<float> (0x00ffffffu);
}

void TapeStopperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();

    for (auto channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (tapeBufferSize < 2 || tapeBuffer.empty()
        || characterBufferSize < 2 || characterBuffer.empty())
        return;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                currentBpm.store (static_cast<float> (*bpm), std::memory_order_relaxed);
        }
    }

    const auto engage = parameters.getRawParameterValue (engageParameterId)->load() >= 0.5f;
    const auto downEnabled = parameters.getRawParameterValue (downEnabledParameterId)->load() >= 0.5f;
    const auto upEnabled = parameters.getRawParameterValue (upEnabledParameterId)->load() >= 0.5f;
    const auto muteAt = juce::jlimit (0.0f, 1.0f,
                                     parameters.getRawParameterValue (muteAtParameterId)->load()
                                         / 100.0f);
    const auto envelopeEnabled = envelopeEnabledValue != nullptr
                                 && envelopeEnabledValue->load() >= 0.5f;
    const auto targetDriveAmount = juce::jlimit
                                   (0.0f, 1.0f,
                                    parameters.getRawParameterValue (driveParameterId)->load()
                                        / 100.0f);
    const auto targetWowAmount = juce::jlimit
                                 (0.0f, 1.0f,
                                  parameters.getRawParameterValue (wowParameterId)->load()
                                      / 100.0f);
    const auto targetFlutterAmount = juce::jlimit
                                     (0.0f, 1.0f,
                                      parameters.getRawParameterValue
                                          (flutterParameterId)->load() / 100.0f);
    const auto targetFluxAmount = juce::jlimit
                                  (0.0f, 1.0f,
                                   parameters.getRawParameterValue
                                       (fluxParameterId)->load() / 100.0f);
    const auto targetMixAmount = juce::jlimit
                                 (0.0f, 1.0f,
                                  parameters.getRawParameterValue (mixParameterId)->load()
                                      / 100.0f);
    const auto captureWaveform = waveformDisplayEnabled.load
                                 (std::memory_order_relaxed);

    std::array<float, numEnvelopePoints> envelopeX {};
    std::array<float, numEnvelopePoints> envelopeY {};
    envelopeX.front() = 0.0f;
    envelopeX.back() = 1.0f;
    envelopeY.fill (0.5f);

    if (envelopeEnabled)
    {
        for (int point = 1; point < numEnvelopePoints - 1; ++point)
        {
            const auto* value = envelopeXValues[static_cast<size_t> (point - 1)];
            const auto requested = value != nullptr ? value->load() : 0.1f * point;
            envelopeX[static_cast<size_t> (point)]
                = juce::jlimit (envelopeX[static_cast<size_t> (point - 1)],
                                1.0f, requested);
        }

        for (int point = 0; point < numEnvelopePoints; ++point)
        {
            if (const auto* value = envelopeYValues[static_cast<size_t> (point)])
                envelopeY[static_cast<size_t> (point)]
                    = juce::jlimit (0.0f, 1.0f, value->load());
        }
    }

    const auto getEnvelopePitchRatio = [&envelopeX, &envelopeY] (float progress)
    {
        const auto position = juce::jlimit (0.0f, 1.0f, progress);
        auto segment = 0;

        while (segment < numEnvelopePoints - 2
               && position > envelopeX[static_cast<size_t> (segment + 1)])
            ++segment;

        const auto x1 = envelopeX[static_cast<size_t> (segment)];
        const auto x2 = envelopeX[static_cast<size_t> (segment + 1)];
        const auto width = juce::jmax (0.000001f, x2 - x1);
        const auto amount = juce::jlimit (0.0f, 1.0f, (position - x1) / width);
        const auto y1 = envelopeY[static_cast<size_t> (segment)];
        const auto y2 = envelopeY[static_cast<size_t> (segment + 1)];
        const auto y = y1 + (y2 - y1) * amount;
        const auto semitones = (0.5f - y) * 24.0f;
        return std::pow (2.0f, semitones / 12.0f);
    };

    if (engage != previousEngage)
    {
        if (engage)
            beginSlowdown (downEnabled);
        else
            beginSpeedup (upEnabled);

        previousEngage = engage;
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        currentDriveAmount += (targetDriveAmount - currentDriveAmount)
                              * characterSmoothingAmount;
        currentWowAmount += (targetWowAmount - currentWowAmount)
                            * characterSmoothingAmount;
        currentFlutterAmount += (targetFlutterAmount - currentFlutterAmount)
                                * characterSmoothingAmount;
        currentFluxAmount += (targetFluxAmount - currentFluxAmount)
                             * characterSmoothingAmount;
        currentMixAmount += (targetMixAmount - currentMixAmount)
                            * characterSmoothingAmount;

        for (int channel = 0; channel < inputChannels; ++channel)
            tapeBuffer[static_cast<size_t> (channel)][static_cast<size_t> (writePosition)]
                = buffer.getSample (channel, sample);

        const auto reentryMix = motionState == MotionState::reentering
                                    ? juce::jlimit (0.0f, 1.0f,
                                                   static_cast<float> (reentryPosition)
                                                       / static_cast<float> (reentryLength))
                                    : 0.0f;

        const auto transitionPositionNow = visualPosition.load (std::memory_order_relaxed);
        const auto fluxMotionActive = motionState == MotionState::slowing
                                      || motionState == MotionState::speeding;

        if (fluxMotionActive && currentFluxAmount > 0.000001f)
        {
            if (--fluxTargetSamplesRemaining <= 0)
            {
                const auto randomBipolar = nextFluxRandomUnit() * 2.0f - 1.0f;
                auto nextTarget = randomBipolar;
                const auto transitionDepth = std::sqrt
                                             (juce::jlimit (0.0f, 1.0f,
                                                            transitionPositionNow));
                const auto errorChance = 0.20f * currentFluxAmount
                                         * currentFluxAmount * transitionDepth;

                // Occasional negative excursions reproduce the short worn-tape
                // playback errors heard at the upper end of the reference control.
                if (nextFluxRandomUnit() < errorChance)
                    nextTarget -= 0.8f + 1.2f * nextFluxRandomUnit();

                targetFluxVariation = juce::jlimit (-2.25f, 1.25f, nextTarget);
                const auto intervalSeconds = 0.018f
                                             + 0.042f * nextFluxRandomUnit();
                fluxTargetSamplesRemaining = juce::jmax
                    (1, juce::roundToInt (currentSampleRate * intervalSeconds));
            }
        }
        else
        {
            targetFluxVariation = 0.0f;
            fluxTargetSamplesRemaining = 0;
        }

        currentFluxVariation += (targetFluxVariation - currentFluxVariation)
                                * fluxVariationSmoothingAmount;
        const auto fluxDepth = 0.20f + 0.80f
                               * std::sqrt (juce::jlimit (0.0f, 1.0f,
                                                         transitionPositionNow));
        const auto fluxSemitoneRange = 0.25f * currentFluxAmount
                                       + 3.50f * currentFluxAmount
                                                     * currentFluxAmount;
        const auto fluxPitchRatio = fluxMotionActive
                                        ? std::pow (2.0f,
                                                    currentFluxVariation * fluxDepth
                                                        * fluxSemitoneRange / 12.0f)
                                        : 1.0f;
        const auto wowDepthSamples = currentSampleRate * 0.008
                                     * static_cast<double> (currentWowAmount);
        const auto flutterDepthSamples = currentSampleRate * 0.001
                                         * static_cast<double> (currentFlutterAmount);
        const auto characterDelaySamples
            = currentSampleRate * 0.010
              + std::sin (wowPhase) * wowDepthSamples
              + std::sin (flutterPhase) * flutterDepthSamples;
        const auto characterMix = juce::jlimit
                                  (0.0f, 1.0f,
                                   juce::jmax (currentWowAmount, currentFlutterAmount));
        const auto muteApplies = motionState == MotionState::slowing
                                 || motionState == MotionState::stopped
                                 || motionState == MotionState::speeding;
        const auto muteAtThresholdReached = muteApplies
                                            && transitionPositionNow >= muteAt;
        const auto fullSpeedShouldMute = fullSpeedMuteEnabled.load
                                         (std::memory_order_relaxed)
                                         && motionState == MotionState::fullSpeed;
        const auto targetMuteGain = muteAtThresholdReached || fullSpeedShouldMute
                                        ? 0.0f : 1.0f;
        currentMuteGain += (targetMuteGain - currentMuteGain) * muteSmoothingAmount;

        auto waveformSample = 0.0f;

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            const auto input = buffer.getSample (channel, sample);
            auto wetOutput = input;

            if (motionState != MotionState::fullSpeed)
            {
                const auto tape = readTapeSample (channel);
                const auto tapeWithSpeedFade = tape * currentSpeed;
                wetOutput = motionState == MotionState::reentering
                                ? tape * (1.0f - reentryMix) + input * reentryMix
                                : tapeWithSpeedFade;
            }

            if (currentDriveAmount > 0.000001f)
            {
                const auto driveGain = 1.0f
                                       + 15.0f * currentDriveAmount
                                             * currentDriveAmount;
                const auto saturated = std::tanh (wetOutput * driveGain)
                                       / std::tanh (driveGain);
                wetOutput += (saturated - wetOutput) * currentDriveAmount;
            }

            characterBuffer[static_cast<size_t> (channel)]
                           [static_cast<size_t> (characterWritePosition)] = wetOutput;

            if (characterMix > 0.000001f)
            {
                const auto modulated = readCharacterSample
                                       (channel, characterDelaySamples);
                wetOutput += (modulated - wetOutput) * characterMix;
            }

            const auto output = (input + (wetOutput - input) * currentMixAmount)
                                * currentMuteGain;
            buffer.setSample (channel, sample, output);
            if (captureWaveform)
                waveformSample += output;
        }

        if (captureWaveform && inputChannels > 0
            && --waveformSamplesUntilCapture <= 0)
        {
            const auto sampleForDisplay = juce::jlimit
                                          (-1.0f, 1.0f,
                                           waveformSample
                                               / static_cast<float> (inputChannels));
            const auto position = waveformWritePosition.load
                                  (std::memory_order_relaxed);
            waveformSamples[static_cast<size_t> (position)].store
                (sampleForDisplay, std::memory_order_relaxed);
            waveformWritePosition.store
                ((position + 1) % waveformSampleCount, std::memory_order_release);
            waveformSamplesUntilCapture = waveformCaptureInterval;
        }

        characterWritePosition = (characterWritePosition + 1) % characterBufferSize;

        wowPhase += juce::MathConstants<double>::twoPi * 0.33 / currentSampleRate;
        flutterPhase += juce::MathConstants<double>::twoPi * 6.5 / currentSampleRate;
        if (wowPhase >= juce::MathConstants<double>::twoPi)
            wowPhase -= juce::MathConstants<double>::twoPi;
        if (flutterPhase >= juce::MathConstants<double>::twoPi)
            flutterPhase -= juce::MathConstants<double>::twoPi;

        if (motionState != MotionState::fullSpeed)
        {
            auto readSpeed = currentSpeed;

            if (envelopeEnabled && motionState == MotionState::slowing)
            {
                const auto progress = transitionPositionNow;
                // The read head starts beside the live write head, so it must never
                // run faster than live playback and wrap into unwritten/stale audio.
                readSpeed = juce::jlimit (0.0f, 1.0f,
                                          readSpeed * getEnvelopePitchRatio (progress));
            }

            // Flux always follows the pitch produced above. With the envelope
            // bypassed that pitch is the normal tape curve; with it enabled the
            // same instability fluctuates around the envelope-shaped pitch.
            if (fluxMotionActive && currentFluxAmount > 0.000001f)
                readSpeed = juce::jlimit (0.0f, 1.0f,
                                          readSpeed * fluxPitchRatio);

            readPosition += readSpeed;
            while (readPosition >= tapeBufferSize)
                readPosition -= tapeBufferSize;
        }

        writePosition = (writePosition + 1) % tapeBufferSize;

        if (motionState == MotionState::fullSpeed)
            readPosition = static_cast<double> (writePosition);

        advanceMotionState();
    }
}

juce::AudioProcessorEditor* TapeStopperAudioProcessor::createEditor()
{
    return new TapeStopperAudioProcessorEditor (*this);
}

bool TapeStopperAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String TapeStopperAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TapeStopperAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TapeStopperAudioProcessor::producesMidi() const
{
    return false;
}

bool TapeStopperAudioProcessor::isMidiEffect() const
{
    return false;
}

double TapeStopperAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TapeStopperAudioProcessor::getNumPrograms()
{
    return 1;
}

int TapeStopperAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TapeStopperAudioProcessor::setCurrentProgram (int)
{
}

const juce::String TapeStopperAudioProcessor::getProgramName (int)
{
    return "Init";
}

void TapeStopperAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void TapeStopperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("schemaVersion", 8, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeStopperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml (*xml);

        if (state.isValid() && state.hasType (parameters.state.getType()))
            parameters.replaceState (state);
    }
}

juce::AudioProcessorValueTreeState& TapeStopperAudioProcessor::getValueTreeState() noexcept
{
    return parameters;
}

float TapeStopperAudioProcessor::getVisualPosition() const noexcept
{
    return visualPosition.load (std::memory_order_relaxed);
}

TapeStopperAudioProcessor::MotionDirection
TapeStopperAudioProcessor::getMotionDirection() const noexcept
{
    return motionDirection.load (std::memory_order_relaxed);
}

bool TapeStopperAudioProcessor::isFullSpeedMuteEnabled() const noexcept
{
    return fullSpeedMuteEnabled.load (std::memory_order_relaxed);
}

bool TapeStopperAudioProcessor::isButtonDisplayReversed() const noexcept
{
    return buttonDisplayReversed.load (std::memory_order_relaxed);
}

bool TapeStopperAudioProcessor::isWaveformDisplayEnabled() const noexcept
{
    return waveformDisplayEnabled.load (std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::setFullSpeedMuteEnabled (bool enabled) noexcept
{
    fullSpeedMuteEnabled.store (enabled, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::setButtonDisplayReversed (bool reversed) noexcept
{
    buttonDisplayReversed.store (reversed, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::setWaveformDisplayEnabled (bool enabled) noexcept
{
    const auto previous = waveformDisplayEnabled.exchange
                          (enabled, std::memory_order_relaxed);

    if (previous != enabled)
    {
        waveformWritePosition.store (0, std::memory_order_relaxed);
        for (auto& sample : waveformSamples)
            sample.store (0.0f, std::memory_order_relaxed);
    }
}

void TapeStopperAudioProcessor::copyWaveformSamples
    (std::array<float, waveformSampleCount>& destination) const noexcept
{
    if (! isWaveformDisplayEnabled())
    {
        destination.fill (0.0f);
        return;
    }

    const auto writePositionNow = waveformWritePosition.load
                                  (std::memory_order_acquire);

    for (int sample = 0; sample < waveformSampleCount; ++sample)
    {
        const auto source = (writePositionNow + sample) % waveformSampleCount;
        destination[static_cast<size_t> (sample)]
            = waveformSamples[static_cast<size_t> (source)].load
                (std::memory_order_relaxed);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeStopperAudioProcessor();
}
