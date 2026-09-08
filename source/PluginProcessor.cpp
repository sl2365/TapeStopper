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
constexpr auto pitchCurveEnabledParameterId = "pitchCurveEnabled";
constexpr auto filterCurveEnabledParameterId = "filterCurveEnabled";
constexpr auto volumeCurveEnabledParameterId = "volumeCurveEnabled";
constexpr auto filterAmountParameterId = "filterAmount";
constexpr auto volumeAmountParameterId = "volumeAmount";
constexpr auto driveParameterId = "drive";
constexpr auto wowParameterId = "wow";
constexpr auto flutterParameterId = "flutter";
constexpr auto fluxParameterId = "flux";
constexpr auto mixParameterId = "mix";
constexpr auto sequencerEnabledParameterId = "sequencerEnabled";
constexpr auto sequencerClockModeParameterId = "sequencerClockMode";
constexpr auto sequencerResolutionParameterId = "sequencerResolution";
constexpr auto sequencerFreeRateParameterId = "sequencerFreeRate";
constexpr auto sequencerLengthParameterId = "sequencerLength";
constexpr auto sequencerOffsetParameterId = "sequencerOffset";

juce::String sequencerStepParameterId (int stepIndex)
{
    return "seqStep" + juce::String (stepIndex + 1);
}

juce::String envelopeXParameterId (int pointIndex)
{
    return "envX" + juce::String (pointIndex);
}

juce::String envelopeYParameterId (int pointIndex)
{
    return "envY" + juce::String (pointIndex);
}

juce::String curvePointParameterId (const juce::String& target,
                                    const juce::String& direction,
                                    int pointIndex)
{
    return target + direction + "Curve" + juce::String (pointIndex + 1);
}

juce::StringArray makeSyncDivisionNames()
{
    return { "4 BAR", "2 BAR", "1 BAR", "1/2", "1/2T",
             "1/4", "1/4T", "1/8", "1/16", "1/16T", "1/32",
             "1/32T", "1/64" };
}

juce::StringArray makeSequencerResolutionNames()
{
    return { "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T",
             "1/32", "1/32T", "1/64", "1/64T", "1/128" };
}

float evaluateCurve (const std::array<float,
                                      TapeStopperAudioProcessor::numCurveControlPoints>& controls,
                     float position) noexcept
{
    constexpr auto numValues = TapeStopperAudioProcessor::numCurveControlPoints + 2;
    std::array<float, numValues> values {};
    values.front() = 0.0f;
    values.back() = 1.0f;
    for (int point = 0; point < TapeStopperAudioProcessor::numCurveControlPoints;
         ++point)
        values[static_cast<size_t> (point + 1)]
            = juce::jlimit (0.0f, 1.0f, controls[static_cast<size_t> (point)]);

    const auto x = juce::jlimit (0.0f, 1.0f, position)
                   * static_cast<float> (numValues - 1);
    const auto segment = juce::jlimit
        (0, numValues - 2, static_cast<int> (std::floor (x)));
    const auto t = juce::jlimit (0.0f, 1.0f, x - static_cast<float> (segment));
    const auto p0 = values[static_cast<size_t> (juce::jmax (0, segment - 1))];
    const auto p1 = values[static_cast<size_t> (segment)];
    const auto p2 = values[static_cast<size_t> (segment + 1)];
    const auto p3 = values[static_cast<size_t>
                           (juce::jmin (numValues - 1, segment + 2))];
    const auto t2 = t * t;
    const auto t3 = t2 * t;
    const auto result = 0.5f * ((2.0f * p1)
                               + (-p0 + p2) * t
                               + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                               + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    return juce::jlimit (0.0f, 1.0f, result);
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
        sample.store (1.0f, std::memory_order_relaxed);

    envelopeEnabledValue = parameters.getRawParameterValue (envelopeEnabledParameterId);

    for (int point = 1; point < numEnvelopePoints - 1; ++point)
        envelopeXValues[static_cast<size_t> (point - 1)]
            = parameters.getRawParameterValue (envelopeXParameterId (point));

    for (int point = 0; point < numEnvelopePoints; ++point)
        envelopeYValues[static_cast<size_t> (point)]
            = parameters.getRawParameterValue (envelopeYParameterId (point));

    for (int point = 0; point < numCurveControlPoints; ++point)
    {
        const auto index = static_cast<size_t> (point);
        pitchDownCurveValues[index] = parameters.getRawParameterValue
                                      (curvePointParameterId ("pitch", "Down", point));
        pitchUpCurveValues[index] = parameters.getRawParameterValue
                                    (curvePointParameterId ("pitch", "Up", point));
        filterDownCurveValues[index] = parameters.getRawParameterValue
                                       (curvePointParameterId ("filter", "Down", point));
        filterUpCurveValues[index] = parameters.getRawParameterValue
                                     (curvePointParameterId ("filter", "Up", point));
        volumeDownCurveValues[index] = parameters.getRawParameterValue
                                       (curvePointParameterId ("volume", "Down", point));
        volumeUpCurveValues[index] = parameters.getRawParameterValue
                                     (curvePointParameterId ("volume", "Up", point));
    }

    for (int step = 0; step < numSequencerSteps; ++step)
        sequencerStepValues[static_cast<size_t> (step)]
            = parameters.getRawParameterValue (sequencerStepParameterId (step));
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
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { pitchCurveEnabledParameterId, 1 },
                 "Pitch Curve Enabled", true));
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { filterCurveEnabledParameterId, 1 },
                 "Filter Curve Enabled", false));
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { volumeCurveEnabledParameterId, 1 },
                 "Volume Curve Enabled", false));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { filterAmountParameterId, 1 }, "Filter Amount",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { volumeAmountParameterId, 1 }, "Volume Amount",
                 juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 100.0f));
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
                 "Global Envelope Enabled", false));
    layout.add (std::make_unique<juce::AudioParameterBool>
                (juce::ParameterID { sequencerEnabledParameterId, 1 },
                 "Sequencer Enabled", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { sequencerClockModeParameterId, 1 },
                 "Sequencer Clock", juce::StringArray { "Sync", "Free" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>
                (juce::ParameterID { sequencerResolutionParameterId, 1 },
                 "Sequencer Resolution", makeSequencerResolutionNames(), 4));
    layout.add (std::make_unique<juce::AudioParameterFloat>
                (juce::ParameterID { sequencerFreeRateParameterId, 1 },
                 "Sequencer Free Rate",
                 juce::NormalisableRange<float> { 25.0f, 2000.0f, 1.0f, 0.45f },
                 125.0f));
    layout.add (std::make_unique<juce::AudioParameterInt>
                (juce::ParameterID { sequencerLengthParameterId, 1 },
                 "Sequencer Length", 1, numSequencerSteps, numSequencerSteps));
    layout.add (std::make_unique<juce::AudioParameterInt>
                (juce::ParameterID { sequencerOffsetParameterId, 1 },
                 "Sequencer Offset", 0, numSequencerSteps - 1, 0));

    for (int step = 0; step < numSequencerSteps; ++step)
        layout.add (std::make_unique<juce::AudioParameterBool>
                    (juce::ParameterID { sequencerStepParameterId (step), 1 },
                     "Sequencer Step " + juce::String (step + 1), false));

    for (const auto& target : { juce::String ("pitch"), juce::String ("filter"),
                                juce::String ("volume") })
    {
        for (const auto& direction : { juce::String ("Down"), juce::String ("Up") })
        {
            for (int point = 0; point < numCurveControlPoints; ++point)
            {
                const auto defaultValue = static_cast<float> (point + 1)
                                          / static_cast<float>
                                              (numCurveControlPoints + 1);
                layout.add (std::make_unique<juce::AudioParameterFloat>
                            (juce::ParameterID
                                { curvePointParameterId (target, direction, point), 1 },
                             target.substring (0, 1).toUpperCase()
                                 + target.substring (1) + " " + direction
                                 + " Curve Point " + juce::String (point + 1),
                             juce::NormalisableRange<float> { 0.0f, 1.0f },
                             defaultValue));
            }
        }
    }

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

juce::String TapeStopperAudioProcessor::sequencerResolutionName (int resolutionIndex)
{
    return makeSequencerResolutionNames()
        [juce::jlimit (0, numSequencerResolutions - 1, resolutionIndex)];
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
    envelopeFilterStates.assign
        (static_cast<size_t> (juce::jmax (1, getTotalNumInputChannels())), 0.0f);

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
    transitionStartPosition = 0.0f;
    transitionPosition = 0;
    transitionLength = 1;
    reentryPosition = 0;
    reentryLength = juce::jmax (1, juce::roundToInt (currentSampleRate * 0.020));
    previousEngage = false;
    previousHostPlaying = false;
    freeSequencerSamplePosition = 0.0;
    currentMuteGain = fullSpeedMuteEnabled.load (std::memory_order_relaxed) ? 0.0f : 1.0f;
    muteSmoothingAmount = 1.0f - std::exp (-1.0f
                                           / static_cast<float> (currentSampleRate * 0.005));
    characterSmoothingAmount = 1.0f - std::exp
                               (-1.0f / static_cast<float> (currentSampleRate * 0.020));
    envelopeEffectSmoothingAmount = 1.0f - std::exp
                                    (-1.0f / static_cast<float>
                                                  (currentSampleRate * 0.005));
    currentEnvelopeFilterAmount = 0.0f;
    currentEnvelopeVolumeGain = 1.0f;
    currentPitchCurveMix = parameters.getRawParameterValue
                           (pitchCurveEnabledParameterId)->load() >= 0.5f ? 1.0f : 0.0f;
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
    retriggerRequested.store (false, std::memory_order_relaxed);
    retriggerActive.store (false, std::memory_order_relaxed);
    sequencerGateActive.store (false, std::memory_order_relaxed);
    currentSequencerStep.store (-1, std::memory_order_relaxed);
    for (auto& sample : waveformSamples)
        sample.store (1.0f, std::memory_order_relaxed);
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
    const auto startPosition = visualPosition.load (std::memory_order_relaxed);
    resetWaveformTrace (currentSpeed, startPosition);

    if (! enabled)
    {
        motionState = MotionState::stopped;
        currentSpeed = 0.0f;
        visualPosition.store (1.0f, std::memory_order_relaxed);
        motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
        recordWaveformTrace (0.0f, 1.0f);
        return;
    }

    transitionStartPosition = startPosition;
    transitionPosition = 0;

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
    transitionLength = juce::jmax
                       (1, juce::roundToInt
                               (fullLength * (1.0f - transitionStartPosition)));
    motionState = MotionState::slowing;
    motionDirection.store (MotionDirection::down, std::memory_order_relaxed);
}

void TapeStopperAudioProcessor::beginSpeedup (bool enabled)
{
    const auto startPosition = visualPosition.load (std::memory_order_relaxed);
    resetWaveformTrace (currentSpeed, startPosition);

    if (! enabled)
    {
        motionState = MotionState::fullSpeed;
        currentSpeed = 1.0f;
        readPosition = static_cast<double> (writePosition);
        visualPosition.store (0.0f, std::memory_order_relaxed);
        motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
        recordWaveformTrace (1.0f, 0.0f);
        return;
    }

    transitionStartPosition = startPosition;
    transitionPosition = 0;

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
    transitionLength = juce::jmax
                       (1, juce::roundToInt (fullLength * transitionStartPosition));
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
            const auto position = transitionStartPosition
                                  + (1.0f - transitionStartPosition) * progress;
            visualPosition.store (position, std::memory_order_relaxed);

            if (transitionPosition >= transitionLength)
            {
                motionState = MotionState::stopped;
                currentSpeed = 0.0f;
                visualPosition.store (1.0f, std::memory_order_relaxed);
                motionDirection.store (MotionDirection::inactive, std::memory_order_relaxed);
                recordWaveformTrace (0.0f, 1.0f);
            }
            break;
        }

        case MotionState::speeding:
        {
            ++transitionPosition;
            const auto progress = juce::jlimit (0.0f, 1.0f,
                                                static_cast<float> (transitionPosition)
                                                    / static_cast<float> (transitionLength));
            const auto position = transitionStartPosition * (1.0f - progress);
            visualPosition.store (position, std::memory_order_relaxed);

            if (transitionPosition >= transitionLength)
            {
                recordWaveformTrace (1.0f, 0.0f);
                beginReentry();
            }
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
                retriggerActive.store (false, std::memory_order_relaxed);
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

void TapeStopperAudioProcessor::resetWaveformTrace (float speed,
                                                     float position) noexcept
{
    if (! waveformDisplayEnabled.load (std::memory_order_relaxed))
        return;

    for (auto& sample : waveformSamples)
        sample.store (-1.0f, std::memory_order_relaxed);

    recordWaveformTrace (speed, position);
}

void TapeStopperAudioProcessor::recordWaveformTrace (float speed,
                                                      float position) noexcept
{
    if (! waveformDisplayEnabled.load (std::memory_order_relaxed))
        return;

    const auto index = juce::jlimit
                       (0, waveformSampleCount - 1,
                        juce::roundToInt (juce::jlimit (0.0f, 1.0f, position)
                                          * static_cast<float>
                                              (waveformSampleCount - 1)));
    waveformSamples[static_cast<size_t> (index)].store
        (juce::jlimit (0.0f, 1.0f, speed), std::memory_order_relaxed);
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

    auto hostPlaying = false;
    auto hasPpqPosition = false;
    auto blockPpqPosition = 0.0;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                currentBpm.store (static_cast<float> (*bpm), std::memory_order_relaxed);
            if (const auto ppq = position->getPpqPosition())
            {
                hasPpqPosition = true;
                blockPpqPosition = *ppq;
            }
            hostPlaying = position->getIsPlaying();
        }
    }

    const auto manualEngage = parameters.getRawParameterValue
                              (engageParameterId)->load() >= 0.5f;
    const auto downEnabled = parameters.getRawParameterValue (downEnabledParameterId)->load() >= 0.5f;
    const auto upEnabled = parameters.getRawParameterValue (upEnabledParameterId)->load() >= 0.5f;
    const auto muteAt = juce::jlimit (0.0f, 1.0f,
                                     parameters.getRawParameterValue (muteAtParameterId)->load()
                                         / 100.0f);
    const auto envelopeEnabled = envelopeEnabledValue != nullptr
                                 && envelopeEnabledValue->load() >= 0.5f;
    const auto pitchCurveEnabled = parameters.getRawParameterValue
                                   (pitchCurveEnabledParameterId)->load() >= 0.5f;
    const auto filterCurveEnabled = parameters.getRawParameterValue
                                    (filterCurveEnabledParameterId)->load() >= 0.5f;
    const auto volumeCurveEnabled = parameters.getRawParameterValue
                                    (volumeCurveEnabledParameterId)->load() >= 0.5f;
    const auto filterAmount = juce::jlimit
                              (0.0f, 1.0f,
                               parameters.getRawParameterValue
                                   (filterAmountParameterId)->load() / 100.0f);
    const auto volumeAmount = juce::jlimit
                              (0.0f, 1.0f,
                               parameters.getRawParameterValue
                                   (volumeAmountParameterId)->load() / 100.0f);
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
    const auto sequencerEnabled = parameters.getRawParameterValue
                                  (sequencerEnabledParameterId)->load() >= 0.5f;
    const auto sequencerUsesFreeClock = parameters.getRawParameterValue
                                        (sequencerClockModeParameterId)->load() >= 0.5f;
    const auto sequencerResolution = juce::jlimit
        (0, numSequencerResolutions - 1,
         juce::roundToInt (parameters.getRawParameterValue
                               (sequencerResolutionParameterId)->load()));
    const auto sequencerFreeRateMs = juce::jlimit
        (25.0f, 2000.0f,
         parameters.getRawParameterValue (sequencerFreeRateParameterId)->load());
    const auto sequencerLength = juce::jlimit
        (1, numSequencerSteps,
         juce::roundToInt (parameters.getRawParameterValue
                               (sequencerLengthParameterId)->load()));
    const auto sequencerOffset = juce::jlimit
        (0, numSequencerSteps - 1,
         juce::roundToInt (parameters.getRawParameterValue
                               (sequencerOffsetParameterId)->load()));
    std::array<bool, numSequencerSteps> sequencerSteps {};
    for (int step = 0; step < numSequencerSteps; ++step)
    {
        const auto* value = sequencerStepValues[static_cast<size_t> (step)];
        sequencerSteps[static_cast<size_t> (step)]
            = value != nullptr && value->load() >= 0.5f;
    }

    const auto sequencerControlsPlay = sequencerEnabled && hostPlaying;
    static constexpr std::array<double, numSequencerResolutions>
        sequencerQuarterNoteLengths
        { 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0,
          0.125, 1.0 / 12.0, 0.0625, 1.0 / 24.0, 0.03125 };

    if (! hostPlaying)
        freeSequencerSamplePosition = 0.0;
    else if (! previousHostPlaying)
        freeSequencerSamplePosition = 0.0;

    const auto loadCurve = [] (const auto& values)
    {
        std::array<float, numCurveControlPoints> result {};
        for (int point = 0; point < numCurveControlPoints; ++point)
        {
            const auto* value = values[static_cast<size_t> (point)];
            result[static_cast<size_t> (point)]
                = value != nullptr ? juce::jlimit (0.0f, 1.0f, value->load())
                                   : static_cast<float> (point + 1)
                                         / static_cast<float>
                                             (numCurveControlPoints + 1);
        }
        return result;
    };

    const auto pitchDownCurve = loadCurve (pitchDownCurveValues);
    const auto pitchUpCurve = loadCurve (pitchUpCurveValues);
    const auto filterDownCurve = loadCurve (filterDownCurveValues);
    const auto filterUpCurve = loadCurve (filterUpCurveValues);
    const auto volumeDownCurve = loadCurve (volumeDownCurveValues);
    const auto volumeUpCurve = loadCurve (volumeUpCurveValues);

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

    const auto getEnvelopeValue = [&envelopeX, &envelopeY] (float progress)
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
        return y1 + (y2 - y1) * amount;
    };

    if (retriggerRequested.exchange (false, std::memory_order_acq_rel))
    {
        retriggerActive.store (true, std::memory_order_relaxed);

        // RETRIG is a pulse rather than a DOWN transition: jump immediately
        // to stopped speed, then use only the current UP settings to recover.
        beginSlowdown (false);
        beginSpeedup (upEnabled);
        if (! upEnabled)
            retriggerActive.store (false, std::memory_order_relaxed);
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto effectiveEngage = manualEngage;

        if (sequencerControlsPlay)
        {
            double absoluteStep = 0.0;
            if (sequencerUsesFreeClock)
            {
                const auto stepSamples = juce::jmax
                    (1.0, currentSampleRate
                              * static_cast<double> (sequencerFreeRateMs) / 1000.0);
                absoluteStep = std::floor (freeSequencerSamplePosition / stepSamples);
            }
            else
            {
                const auto bpm = static_cast<double>
                                 (currentBpm.load (std::memory_order_relaxed));
                const auto samplePpq = hasPpqPosition
                                           ? blockPpqPosition
                                                 + static_cast<double> (sample) * bpm
                                                       / (60.0 * currentSampleRate)
                                           : freeSequencerSamplePosition * bpm
                                                 / (60.0 * currentSampleRate);
                absoluteStep = std::floor
                    (samplePpq
                     / sequencerQuarterNoteLengths
                           [static_cast<size_t> (sequencerResolution)]);
            }

            const auto rawStep = static_cast<std::int64_t> (absoluteStep);
            auto step = static_cast<int>
                ((rawStep + sequencerOffset) % static_cast<std::int64_t> (sequencerLength));
            if (step < 0)
                step += sequencerLength;
            currentSequencerStep.store (step, std::memory_order_relaxed);
            effectiveEngage = sequencerSteps[static_cast<size_t> (step)];
            sequencerGateActive.store (effectiveEngage, std::memory_order_relaxed);
        }
        else
        {
            currentSequencerStep.store (-1, std::memory_order_relaxed);
            sequencerGateActive.store (false, std::memory_order_relaxed);
        }

        if (effectiveEngage != previousEngage)
        {
            retriggerActive.store (false, std::memory_order_relaxed);

            if (effectiveEngage)
                beginSlowdown (downEnabled);
            else
                beginSpeedup (upEnabled);

            previousEngage = effectiveEngage;
        }

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
        const auto curveMotionActive = motionState == MotionState::slowing
                                       || motionState == MotionState::stopped
                                       || motionState == MotionState::speeding;
        const auto useDownCurves = motionState != MotionState::speeding;
        const auto pitchCurveAmount = curveMotionActive
                                          ? evaluateCurve
                                                (useDownCurves ? pitchDownCurve : pitchUpCurve,
                                                 transitionPositionNow)
                                          : 0.0f;
        const auto filterCurveAmount = curveMotionActive
                                           ? evaluateCurve
                                                 (useDownCurves ? filterDownCurve : filterUpCurve,
                                                  transitionPositionNow)
                                           : 0.0f;
        const auto volumeCurveAmount = curveMotionActive
                                           ? evaluateCurve
                                                 (useDownCurves ? volumeDownCurve : volumeUpCurve,
                                                  transitionPositionNow)
                                           : 0.0f;

        currentPitchCurveMix += ((pitchCurveEnabled ? 1.0f : 0.0f)
                                 - currentPitchCurveMix)
                                * envelopeEffectSmoothingAmount;
        const auto targetPitchSpeed = curveMotionActive
                                          ? 1.0f - pitchCurveAmount : 1.0f;
        currentSpeed += (targetPitchSpeed - currentSpeed)
                        * envelopeEffectSmoothingAmount;
        const auto targetEnvelopeFilterAmount = filterCurveEnabled
                                                     ? filterCurveAmount * filterAmount : 0.0f;
        const auto targetEnvelopeVolumeGain = volumeCurveEnabled
                                                  ? 1.0f - volumeCurveAmount * volumeAmount
                                                  : 1.0f;
        currentEnvelopeFilterAmount += (targetEnvelopeFilterAmount
                                        - currentEnvelopeFilterAmount)
                                       * envelopeEffectSmoothingAmount;
        currentEnvelopeVolumeGain += (targetEnvelopeVolumeGain
                                      - currentEnvelopeVolumeGain)
                                     * envelopeEffectSmoothingAmount;

        const auto openCutoff = juce::jmin (20000.0f,
                                            static_cast<float> (currentSampleRate * 0.45));
        const auto filterCutoff = openCutoff
                                  * std::pow (120.0f / openCutoff,
                                              currentEnvelopeFilterAmount);
        const auto envelopeFilterCoefficient = 1.0f
                                               - std::exp
                                                 (-juce::MathConstants<float>::twoPi
                                                  * filterCutoff
                                                  / static_cast<float> (currentSampleRate));

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

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            const auto input = buffer.getSample (channel, sample);
            auto wetOutput = input;

            if (currentPitchCurveMix > 0.000001f
                && motionState != MotionState::fullSpeed)
            {
                const auto tape = readTapeSample (channel);
                const auto pitchOutput = motionState == MotionState::reentering
                                             ? tape * (1.0f - reentryMix)
                                                   + input * reentryMix
                                             : tape;
                wetOutput += (pitchOutput - wetOutput) * currentPitchCurveMix;
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

            auto& envelopeFilterState = envelopeFilterStates
                                        [static_cast<size_t> (channel)];
            if (currentEnvelopeFilterAmount > 0.000001f)
            {
                envelopeFilterState += envelopeFilterCoefficient
                                       * (wetOutput - envelopeFilterState);
                wetOutput = envelopeFilterState;
            }
            else
                envelopeFilterState = wetOutput;

            wetOutput *= currentEnvelopeVolumeGain;

            const auto output = (input + (wetOutput - input) * currentMixAmount)
                                * currentMuteGain;
            buffer.setSample (channel, sample, output);
        }

        characterWritePosition = (characterWritePosition + 1) % characterBufferSize;

        wowPhase += juce::MathConstants<double>::twoPi * 0.33 / currentSampleRate;
        flutterPhase += juce::MathConstants<double>::twoPi * 6.5 / currentSampleRate;
        if (wowPhase >= juce::MathConstants<double>::twoPi)
            wowPhase -= juce::MathConstants<double>::twoPi;
        if (flutterPhase >= juce::MathConstants<double>::twoPi)
            flutterPhase -= juce::MathConstants<double>::twoPi;

        if (currentPitchCurveMix > 0.000001f
            && motionState != MotionState::fullSpeed)
        {
            auto readSpeed = currentSpeed;

            if (envelopeEnabled && motionState == MotionState::slowing)
            {
                const auto progress = transitionPositionNow;
                const auto semitones = (0.5f - getEnvelopeValue (progress)) * 24.0f;
                const auto pitchRatio = std::pow (2.0f, semitones / 12.0f);
                // The read head starts beside the live write head, so it must never
                // run faster than live playback and wrap into unwritten/stale audio.
                readSpeed = juce::jlimit (0.0f, 1.0f,
                                          readSpeed * pitchRatio);
            }

            // Flux follows the Pitch curve and the optional global Envelope.
            if (fluxMotionActive && currentFluxAmount > 0.000001f)
                readSpeed = juce::jlimit (0.0f, 1.0f,
                                          readSpeed * fluxPitchRatio);

            if (captureWaveform
                && (motionState == MotionState::slowing
                    || motionState == MotionState::speeding))
            {
                recordWaveformTrace
                    (1.0f + (readSpeed - 1.0f) * currentPitchCurveMix,
                     transitionPositionNow);
            }

            readPosition += readSpeed;
            while (readPosition >= tapeBufferSize)
                readPosition -= tapeBufferSize;
        }
        else if (captureWaveform
                 && (motionState == MotionState::slowing
                     || motionState == MotionState::speeding))
        {
            recordWaveformTrace (1.0f, transitionPositionNow);
        }

        writePosition = (writePosition + 1) % tapeBufferSize;

        if (currentPitchCurveMix <= 0.000001f
            || motionState == MotionState::fullSpeed)
            readPosition = static_cast<double> (writePosition);

        advanceMotionState();

        if (hostPlaying)
            freeSequencerSamplePosition += 1.0;
    }

    previousHostPlaying = hostPlaying;
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
    state.setProperty ("schemaVersion", 12, nullptr);

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

bool TapeStopperAudioProcessor::isRetriggerActive() const noexcept
{
    return retriggerActive.load (std::memory_order_relaxed);
}

bool TapeStopperAudioProcessor::isSequencerGateActive() const noexcept
{
    return sequencerGateActive.load (std::memory_order_relaxed);
}

int TapeStopperAudioProcessor::getCurrentSequencerStep() const noexcept
{
    return currentSequencerStep.load (std::memory_order_relaxed);
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
        const auto position = visualPosition.load (std::memory_order_relaxed);

        for (auto& sample : waveformSamples)
            sample.store (enabled && position <= 0.0001f ? 1.0f : -1.0f,
                          std::memory_order_relaxed);

        if (enabled && position > 0.0001f)
            recordWaveformTrace (1.0f - position, position);
    }
}

void TapeStopperAudioProcessor::requestRetrigger() noexcept
{
    retriggerRequested.store (true, std::memory_order_release);
}

void TapeStopperAudioProcessor::copyWaveformSamples
    (std::array<float, waveformSampleCount>& destination) const noexcept
{
    if (! isWaveformDisplayEnabled())
    {
        destination.fill (0.0f);
        return;
    }

    for (int sample = 0; sample < waveformSampleCount; ++sample)
        destination[static_cast<size_t> (sample)]
            = waveformSamples[static_cast<size_t> (sample)].load
                (std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeStopperAudioProcessor();
}
