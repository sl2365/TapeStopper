#include "PluginEditor.h"
#include "PortableSettings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace
{
constexpr auto engageParameterId = "engage";
constexpr auto retriggerParameterId = "retrigger";
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
constexpr auto currentPresetNameProperty = "currentPresetName";
const juce::Identifier directionButtonProperty { "isDirectionButton" };
const juce::Identifier settingsCogProperty { "isSettingsCog" };
const juce::Identifier largerButtonTextProperty { "hasLargerButtonText" };

juce::Colour presetButtonBackgroundColour() noexcept
{
    return juce::Colour (0xffdadad5);
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

juce::String sequencerStepParameterId (int stepIndex)
{
    return "seqStep" + juce::String (stepIndex + 1);
}

class TapeStopButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override
    {
        if (static_cast<bool> (button.getProperties().getWithDefault
                                   (settingsCogProperty, false)))
            return;

        const auto isDirectionButton = static_cast<bool>
                                       (button.getProperties().getWithDefault
                                           (directionButtonProperty, false));
        const auto isDisabled = ! button.isEnabled()
                                || (isDirectionButton && ! button.getToggleState());

        auto base = isDisabled ? juce::Colour (0xff747676) : backgroundColour;

        if (isButtonDown)
            base = base.darker (0.18f);
        else if (isMouseOverButton && ! isDisabled)
            base = base.brighter (0.07f);

        const auto area = button.getLocalBounds().toFloat().reduced (0.75f);
        juce::ColourGradient fill (base.brighter (0.32f), area.getX(), area.getY(),
                                   base.darker (0.18f), area.getX(), area.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (area, 2.0f);

        g.setColour (juce::Colour (0xff3b3d3d));
        g.drawRoundedRectangle (area, 2.0f, 1.5f);

        const auto inner = area.reduced (2.0f);
        g.setColour (juce::Colours::white.withAlpha (isDisabled ? 0.25f : 0.58f));
        g.drawLine (inner.getX(), inner.getY(), inner.getRight(), inner.getY(), 1.0f);
        g.drawLine (inner.getX(), inner.getY(), inner.getX(), inner.getBottom(), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.drawLine (inner.getX(), inner.getBottom(), inner.getRight(), inner.getBottom(), 1.0f);
        g.drawLine (inner.getRight(), inner.getY(), inner.getRight(), inner.getBottom(), 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool isMouseOverButton, bool isButtonDown) override
    {
        const auto isDirectionButton = static_cast<bool>
                                       (button.getProperties().getWithDefault
                                           (directionButtonProperty, false));
        const auto isDisabled = ! button.isEnabled()
                                || (isDirectionButton && ! button.getToggleState());

        g.setColour (isDisabled ? juce::Colour (0xff242626) : juce::Colour (0xff171919));

        if (static_cast<bool> (button.getProperties().getWithDefault
                                   (settingsCogProperty, false)))
        {
            const auto area = button.getLocalBounds().toFloat();
            const auto centre = area.getCentre();
            const auto outerRadius = juce::jmin (area.getWidth(), area.getHeight())
                                     * 0.342105f;
            const auto innerRadius = outerRadius * 0.73f;
            juce::Path cog;

            for (int step = 0; step < 32; ++step)
            {
                const auto angle = -juce::MathConstants<float>::halfPi
                                   + juce::MathConstants<float>::twoPi
                                         * static_cast<float> (step) / 32.0f;
                const auto toothStep = step % 4;
                const auto radius = toothStep == 0 || toothStep == 1
                                        ? outerRadius : innerRadius;
                const juce::Point<float> point
                    { centre.x + std::cos (angle) * radius,
                      centre.y + std::sin (angle) * radius };

                if (step == 0)
                    cog.startNewSubPath (point);
                else
                    cog.lineTo (point);
            }

            cog.closeSubPath();
            cog.setUsingNonZeroWinding (false);
            cog.addEllipse (juce::Rectangle<float> (outerRadius * 0.70f,
                                                    outerRadius * 0.70f)
                                .withCentre (centre));

            auto cogColour = juce::Colour (0xffd7d9d9);
            if (isButtonDown)
                cogColour = cogColour.withAlpha (0.68f);
            else if (isMouseOverButton)
                cogColour = cogColour.brighter (0.10f);

            g.setColour (cogColour);
            g.fillPath (cog);
            return;
        }

        const auto useLargerText = static_cast<bool>
            (button.getProperties().getWithDefault (largerButtonTextProperty, false));
        g.setFont (juce::FontOptions (useLargerText ? 13.5f : 12.0f,
                                      juce::Font::bold));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (5, 2),
                          juce::Justification::centred, 1);
    }
};

class TapeStopKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosition, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        auto area = juce::Rectangle<float> (static_cast<float> (x),
                                            static_cast<float> (y),
                                            static_cast<float> (width),
                                            static_cast<float> (height)).reduced (5.0f);
        const auto diameter = juce::jmin (area.getWidth(), area.getHeight());
        const auto knob = area.withSizeKeepingCentre (diameter, diameter);

        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillEllipse (knob.translated (1.5f, 2.0f));

        g.setColour (juce::Colour (0xff0b347d));
        g.fillEllipse (knob);

        const auto face = knob.reduced (3.0f);
        juce::ColourGradient fill (juce::Colour (0xff5bd4ff),
                                   face.getX(), face.getY(),
                                   juce::Colour (0xff075ed6),
                                   face.getX(), face.getBottom(), false);
        g.setGradientFill (fill);
        g.fillEllipse (face);

        g.setColour (juce::Colours::white.withAlpha (0.42f));
        g.drawEllipse (face.reduced (0.5f), 1.0f);

        const auto angle = rotaryStartAngle
                           + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
        const auto centre = face.getCentre();
        const auto radius = face.getWidth() * 0.5f;
        const auto pointerStart = juce::Point<float>
                                  { centre.x + std::sin (angle) * radius * 0.12f,
                                    centre.y - std::cos (angle) * radius * 0.12f };
        const auto pointerEnd = juce::Point<float>
                                { centre.x + std::sin (angle) * radius * 0.72f,
                                  centre.y - std::cos (angle) * radius * 0.72f };

        g.setColour (juce::Colour (0xffbaf4ff));
        g.drawLine (juce::Line<float> { pointerStart, pointerEnd }, 3.0f);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (pointerEnd));

        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f,
                                0.0f, rotaryStartAngle, angle, true);
        g.strokePath (valueArc, juce::PathStrokeType (1.5f));
    }
};

void drawPanel (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (0xffb7b9b8));
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (juce::Colour (0xff484b4d));
    g.drawRoundedRectangle (area, 5.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawLine (area.getX() + 4.0f, area.getY() + 4.0f,
                area.getRight() - 4.0f, area.getY() + 4.0f, 1.0f);
}

std::vector<juce::String> makePresetParameterIds()
{
    std::vector<juce::String> ids {
        triggerModeParameterId,
        downEnabledParameterId,
        upEnabledParameterId,
        downSpeedParameterId,
        upSpeedParameterId,
        muteAtParameterId,
        timingModeParameterId,
        downSyncDivisionParameterId,
        upSyncDivisionParameterId,
        pitchCurveEnabledParameterId,
        filterCurveEnabledParameterId,
        volumeCurveEnabledParameterId,
        filterAmountParameterId,
        volumeAmountParameterId,
        driveParameterId,
        wowParameterId,
        flutterParameterId,
        fluxParameterId,
        mixParameterId,
        envelopeEnabledParameterId,
        sequencerEnabledParameterId,
        sequencerClockModeParameterId,
        sequencerResolutionParameterId,
        sequencerFreeRateParameterId,
        sequencerLengthParameterId,
        sequencerOffsetParameterId
    };

    for (const auto& target : { juce::String ("pitch"), juce::String ("filter"),
                                juce::String ("volume") })
        for (const auto& direction : { juce::String ("Down"), juce::String ("Up") })
            for (int point = 0; point < TapeStopperAudioProcessor::numCurveControlPoints;
                 ++point)
                ids.push_back (curvePointParameterId (target, direction, point));

    for (int point = 1; point < TapeStopperAudioProcessor::numEnvelopePoints - 1; ++point)
        ids.push_back (envelopeXParameterId (point));

    for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
        ids.push_back (envelopeYParameterId (point));

    for (int step = 0; step < TapeStopperAudioProcessor::numSequencerSteps; ++step)
        ids.push_back (sequencerStepParameterId (step));

    return ids;
}

const auto presetParameterIds = makePresetParameterIds();

bool isBackwardCompatibleOptionalPresetParameter (const juce::String& parameterId)
{
    return parameterId == driveParameterId
           || parameterId == wowParameterId
           || parameterId == flutterParameterId
           || parameterId == fluxParameterId
           || parameterId == mixParameterId;
}

juce::StringPairArray readIniText (const juce::String& text)
{
    juce::StringPairArray values (true);
    juce::StringArray lines;
    lines.addLines (text);

    for (auto line : lines)
    {
        line = line.trim();

        if (line.isEmpty() || line.startsWithChar (';') || line.startsWithChar ('#')
            || line.startsWithChar ('['))
            continue;

        const auto separator = line.indexOfChar ('=');
        if (separator <= 0)
            continue;

        values.set (line.substring (0, separator).trim(),
                    line.substring (separator + 1).trim());
    }

    return values;
}

juce::String legalPresetName (juce::String requestedName)
{
    requestedName = requestedName.trim();
    if (requestedName.endsWithIgnoreCase (".ini"))
        requestedName = requestedName.dropLastCharacters (4).trim();

    return juce::File::createLegalFileName (requestedName).trim();
}

juce::File getPresetDirectory()
{
    const auto dataFolder = TapeStopperPortableSettings::getDataFolder();
    return dataFolder == juce::File() ? juce::File()
                                      : dataFolder.getChildFile ("Presets");
}

class PresetButton final : public juce::TextButton
{
public:
    explicit PresetButton (const juce::String& text = {}) : juce::TextButton (text) {}

    void paintButton (juce::Graphics& g, bool isMouseOverButton,
                      bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        auto background = presetButtonBackgroundColour();

        if (isMouseOverButton)
            background = background.brighter (0.10f);
        if (isButtonDown)
            background = background.darker (0.12f);

        g.setColour (background);
        g.fillRoundedRectangle (bounds, 1.5f);
        g.setColour (juce::Colour (0xff686864));
        g.drawRoundedRectangle (bounds, 1.5f, 1.0f);
        g.setColour (juce::Colour (0xff242424));
        g.setFont (juce::FontOptions (getHeight() * 0.58f, juce::Font::bold));
        g.drawFittedText (getButtonText(), getLocalBounds().reduced (1, 0),
                          juce::Justification::centred, 1, 0.82f);
    }
};

class PresetNameDisplay final : public juce::Component
{
public:
    void setText (const juce::String& newText)
    {
        if (text != newText)
        {
            text = newText;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (presetButtonBackgroundColour());
        g.fillRoundedRectangle (bounds, 1.8f);
        g.setColour (juce::Colour (0xff686864));
        g.drawRoundedRectangle (bounds, 1.8f, 1.0f);
        g.setColour (juce::Colour (0xff242424));
        g.setFont (juce::FontOptions (getHeight() * 0.54f, juce::Font::bold));
        g.drawFittedText (text, getLocalBounds().reduced (3, 0),
                          juce::Justification::centred, 1, 0.72f);
    }

    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& wheel) override
    {
        const auto movement = std::abs (wheel.deltaY) >= std::abs (wheel.deltaX)
                                  ? wheel.deltaY : wheel.deltaX;

        if (movement != 0.0f && onStep != nullptr)
            onStep (movement > 0.0f ? -1 : 1);
    }

    std::function<void (int)> onStep;

private:
    juce::String text { "INIT" };
};

class PresetSectionComponent final : public juce::Component
{
public:
    explicit PresetSectionComponent (TapeStopperAudioProcessor& owner)
        : processor (owner), presetDirectory (getPresetDirectory())
    {
        previousButton.setTooltip ("Load the previous preset");
        nextButton.setTooltip ("Load the next preset");
        nameButton.setTooltip ("Rename the current preset, or save INIT under a new name");
        menuButton.setTooltip ("Save, Save As, or load a preset");

        previousButton.onClick = [this] { selectRelativePreset (-1); };
        nextButton.onClick = [this] { selectRelativePreset (1); };
        nameButton.onClick = [this]
        {
            showPresetNamePrompt (currentPresetFile.existsAsFile());
        };
        menuButton.onClick = [this] { showPresetMenu(); };
        nameDisplay.onStep = [this] (int delta) { selectRelativePreset (delta); };

        for (auto* component : { static_cast<juce::Component*> (&nameDisplay),
                                 static_cast<juce::Component*> (&previousButton),
                                 static_cast<juce::Component*> (&nextButton),
                                 static_cast<juce::Component*> (&nameButton),
                                 static_cast<juce::Component*> (&menuButton) })
            addAndMakeVisible (*component);

        refreshPresetFiles();
        restorePresetLabelFromState();
    }

    ~PresetSectionComponent() override
    {
        presetNameWindow.reset();
    }

    void resized() override
    {
        const auto sx = static_cast<float> (getWidth()) / designWidth;
        const auto sy = static_cast<float> (getHeight()) / designHeight;
        const auto scaled = [sx, sy] (int x, int y, int width, int height)
        {
            return juce::Rectangle<int> (juce::roundToInt (x * sx),
                                         juce::roundToInt (y * sy),
                                         juce::roundToInt (width * sx),
                                         juce::roundToInt (height * sy));
        };

        nameDisplay.setBounds (scaled (0, 0, 145, 20));
        previousButton.setBounds (scaled (0, 23, 24, 18));
        nextButton.setBounds (scaled (28, 23, 24, 18));
        nameButton.setBounds (scaled (56, 23, 42, 18));
        menuButton.setBounds (scaled (102, 23, 43, 18));
    }

private:
    void refreshPresetFiles()
    {
        presetFiles.clear();
        if (presetDirectory == juce::File())
            return;

        presetDirectory.createDirectory();

        const auto files = presetDirectory.findChildFiles (juce::File::findFiles,
                                                            false, "*.ini");
        for (const auto& file : files)
            presetFiles.push_back (file);

        std::sort (presetFiles.begin(), presetFiles.end(),
                   [] (const juce::File& first, const juce::File& second)
                   {
                       return first.getFileNameWithoutExtension().compareIgnoreCase
                                  (second.getFileNameWithoutExtension()) < 0;
                   });
    }

    void restorePresetLabelFromState()
    {
        const auto storedName = processor.getValueTreeState().state
                                    .getProperty (currentPresetNameProperty, "INIT")
                                    .toString().trim();

        if (storedName.isEmpty() || storedName.equalsIgnoreCase ("INIT"))
        {
            setCurrentPreset ("INIT", {});
            return;
        }

        for (const auto& file : presetFiles)
        {
            if (file.getFileNameWithoutExtension().equalsIgnoreCase (storedName))
            {
                setCurrentPreset (file.getFileNameWithoutExtension(), file);
                return;
            }
        }

        setCurrentPreset (storedName, {});
    }

    void setCurrentPreset (const juce::String& presetName, const juce::File& presetFile)
    {
        currentPresetName = presetName.isNotEmpty() ? presetName : "INIT";
        currentPresetFile = presetFile;
        nameDisplay.setText (currentPresetName);
        processor.getValueTreeState().state.setProperty (currentPresetNameProperty,
                                                         currentPresetName, nullptr);
    }

    void selectRelativePreset (int delta)
    {
        refreshPresetFiles();
        const auto presetCount = static_cast<int> (presetFiles.size()) + 1;
        auto currentIndex = 0;
        auto currentWasFound = currentPresetName.equalsIgnoreCase ("INIT")
                               && currentPresetFile == juce::File();

        if (currentPresetFile != juce::File())
        {
            for (int index = 0; index < static_cast<int> (presetFiles.size()); ++index)
            {
                if (presetFiles[static_cast<size_t> (index)] == currentPresetFile)
                {
                    currentIndex = index + 1;
                    currentWasFound = true;
                    break;
                }
            }
        }

        if (! currentWasFound)
        {
            loadInitialPreset();
            return;
        }

        auto nextIndex = (currentIndex + delta) % presetCount;
        if (nextIndex < 0)
            nextIndex += presetCount;

        if (nextIndex == 0)
            loadInitialPreset();
        else
            loadPresetFile (presetFiles[static_cast<size_t> (nextIndex - 1)]);
    }

    void loadInitialPreset()
    {
        auto& parameterState = processor.getValueTreeState();

        for (const auto& parameterId : presetParameterIds)
        {
            if (auto* parameter = parameterState.getParameter (parameterId))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
                parameter->endChangeGesture();
            }
        }

        setCurrentPreset ("INIT", {});
    }

    bool applyPresetValues (const juce::StringPairArray& values,
                            const juce::String& sourceName)
    {
        auto& parameterState = processor.getValueTreeState();
        const auto presetFormatVersion = values.getValue ("FormatVersion", "0")
                                               .getIntValue();
        std::vector<std::pair<juce::RangedAudioParameter*, float>> pendingValues;
        pendingValues.reserve (presetParameterIds.size());

        for (const auto& parameterId : presetParameterIds)
        {
            auto* parameter = parameterState.getParameter (parameterId);
            if (parameter == nullptr)
            {
                showPresetError ("The preset is incomplete or incompatible:\n" + sourceName);
                return false;
            }

            if (! values.containsKey (parameterId))
            {
                if (isBackwardCompatibleOptionalPresetParameter (parameterId))
                {
                    pendingValues.emplace_back (parameter, parameter->getDefaultValue());
                    continue;
                }

                showPresetError ("The preset is incomplete or incompatible:\n" + sourceName);
                return false;
            }

            auto actualValue = values[parameterId].getDoubleValue();
            if (! std::isfinite (actualValue))
            {
                showPresetError ("The preset contains an invalid value:\n" + sourceName);
                return false;
            }

            if (presetFormatVersion >= 4 && presetFormatVersion <= 5)
            {
                if ((parameterId == downSyncDivisionParameterId
                     || parameterId == upSyncDivisionParameterId)
                    && actualValue >= 8.0)
                    actualValue += 1.0;
                else if (parameterId == sequencerResolutionParameterId
                         && actualValue >= 1.0)
                    actualValue += 2.0;
            }

            pendingValues.emplace_back
                (parameter, juce::jlimit (0.0f, 1.0f,
                    parameter->convertTo0to1 (static_cast<float> (actualValue))));
        }

        for (const auto& pendingValue : pendingValues)
        {
            pendingValue.first->beginChangeGesture();
            pendingValue.first->setValueNotifyingHost (pendingValue.second);
            pendingValue.first->endChangeGesture();
        }

        return true;
    }

    bool loadPresetFile (const juce::File& file)
    {
        if (! file.existsAsFile())
        {
            showPresetError ("The selected preset file no longer exists.");
            return false;
        }

        const auto values = readIniText (file.loadFileAsString());
        if (! applyPresetValues (values, file.getFileName()))
            return false;

        auto presetName = values.getValue
                          ("Name", file.getFileNameWithoutExtension()).trim();
        if (presetName.isEmpty())
            presetName = file.getFileNameWithoutExtension();

        setCurrentPreset (presetName, file);
        return true;
    }

    bool savePresetFile (const juce::File& file, const juce::String& presetName)
    {
        if (presetDirectory == juce::File()
            || presetDirectory.createDirectory().failed())
        {
            showPresetError ("The Data\\Presets folder could not be created.");
            return false;
        }

        juce::String contents;
        contents << "; TapeStopper user preset\r\n"
                 << "[TapeStopperPreset]\r\n"
                 << "Name=" << presetName << "\r\n"
                 << "FormatVersion=6\r\n";

        auto& parameterState = processor.getValueTreeState();
        for (const auto& parameterId : presetParameterIds)
        {
            auto* parameter = parameterState.getParameter (parameterId);
            if (parameter == nullptr)
            {
                showPresetError ("An internal preset parameter could not be found.");
                return false;
            }

            const auto actualValue = parameter->convertFrom0to1 (parameter->getValue());
            contents << parameterId << "=" << juce::String (actualValue, 9) << "\r\n";
        }

        if (! file.replaceWithText (contents))
        {
            showPresetError ("The preset could not be written:\n" + file.getFullPathName());
            return false;
        }

        return true;
    }

    void saveCurrentPreset()
    {
        if (! currentPresetFile.existsAsFile())
        {
            showPresetNamePrompt (false);
            return;
        }

        if (savePresetFile (currentPresetFile, currentPresetName))
        {
            refreshPresetFiles();
            setCurrentPreset (currentPresetName, currentPresetFile);
        }
    }

    void savePresetAs (const juce::String& requestedName)
    {
        const auto presetName = legalPresetName (requestedName);
        if (presetName.isEmpty())
        {
            showPresetError ("Please enter a preset name.");
            return;
        }

        if (presetName.equalsIgnoreCase ("INIT"))
        {
            showPresetError ("INIT is reserved for the built-in initial preset.");
            return;
        }

        const auto targetFile = presetDirectory.getChildFile (presetName + ".ini");
        if (targetFile.existsAsFile())
        {
            showPresetError ("A preset with that name already exists.\n"
                             "Use SAVE to replace the currently selected preset.");
            return;
        }

        if (savePresetFile (targetFile, presetName))
        {
            refreshPresetFiles();
            setCurrentPreset (presetName, targetFile);
        }
    }

    void renameCurrentPreset (const juce::String& requestedName)
    {
        if (! currentPresetFile.existsAsFile())
        {
            savePresetAs (requestedName);
            return;
        }

        const auto presetName = legalPresetName (requestedName);
        if (presetName.isEmpty())
        {
            showPresetError ("Please enter a preset name.");
            return;
        }

        if (presetName.equalsIgnoreCase ("INIT"))
        {
            showPresetError ("INIT is reserved for the built-in initial preset.");
            return;
        }

        if (presetName.equalsIgnoreCase
                (currentPresetFile.getFileNameWithoutExtension()))
        {
            setCurrentPreset (currentPresetFile.getFileNameWithoutExtension(),
                              currentPresetFile);
            return;
        }

        const auto targetFile = presetDirectory.getChildFile (presetName + ".ini");
        if (targetFile.existsAsFile())
        {
            showPresetError ("A preset with that name already exists.");
            return;
        }

        if (! currentPresetFile.moveFileTo (targetFile))
        {
            showPresetError ("The preset file could not be renamed.");
            return;
        }

        currentPresetFile = targetFile;
        if (! savePresetFile (targetFile, presetName))
            return;

        refreshPresetFiles();
        setCurrentPreset (presetName, targetFile);
    }

    void showPresetNamePrompt (bool renameExistingPreset)
    {
        auto* centreComponent = getParentComponent();
        if (centreComponent == nullptr)
            centreComponent = this;

        presetNameWindow = std::make_unique<juce::AlertWindow>
            (renameExistingPreset ? "Rename Preset" : "Save Preset As",
             renameExistingPreset ? "Enter the new preset name."
                                  : "Enter a name for this preset.",
             juce::MessageBoxIconType::NoIcon, centreComponent);

        auto* window = presetNameWindow.get();
        window->addTextEditor ("presetName",
                               renameExistingPreset ? currentPresetName : "New Preset",
                               "Name:");
        window->addButton (renameExistingPreset ? "Rename" : "Save", 1,
                           juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0);
        window->centreAroundComponent (centreComponent,
                                       window->getWidth(), window->getHeight());

        auto* nameEditor = window->getTextEditor ("presetName");
        juce::Component::SafePointer<juce::AlertWindow> safeWindow (window);

        if (nameEditor != nullptr)
        {
            nameEditor->setReturnKeyStartsNewLine (false);
            nameEditor->setSelectAllWhenFocused (true);
            nameEditor->onReturnKey = [safeWindow]
            {
                if (auto* activeWindow = safeWindow.getComponent())
                    activeWindow->exitModalState (1);
            };
        }

        juce::Component::SafePointer<PresetSectionComponent> safeSection (this);
        window->enterModalState
            (true,
             juce::ModalCallbackFunction::create
                ([safeSection, renameExistingPreset] (int result)
                 {
                     if (auto* section = safeSection.getComponent())
                     {
                         auto name = juce::String();
                         if (result == 1 && section->presetNameWindow != nullptr)
                             name = section->presetNameWindow
                                        ->getTextEditorContents ("presetName");

                         section->presetNameWindow.reset();

                         if (result == 1)
                         {
                             if (renameExistingPreset)
                                 section->renameCurrentPreset (name);
                             else
                                 section->savePresetAs (name);
                         }
                     }
                 }),
             false);

        juce::MessageManager::callAsync ([safeSection]
        {
            if (auto* section = safeSection.getComponent())
            {
                if (section->presetNameWindow != nullptr)
                {
                    auto* centre = section->getParentComponent();
                    if (centre == nullptr)
                        centre = section;

                    auto* dialog = section->presetNameWindow.get();
                    dialog->centreAroundComponent (centre,
                                                   dialog->getWidth(),
                                                   dialog->getHeight());
                }
            }
        });

        if (nameEditor != nullptr)
        {
            nameEditor->grabKeyboardFocus();
            nameEditor->selectAll();
        }
    }

    void showPresetMenu()
    {
        refreshPresetFiles();

        constexpr int savePresetId = 1;
        constexpr int savePresetAsId = 2;
        constexpr int initialPresetId = 1000;
        constexpr int firstUserPresetId = 2000;

        juce::PopupMenu loadMenu;
        loadMenu.addItem (initialPresetId, "INIT", true,
                          currentPresetName.equalsIgnoreCase ("INIT")
                              && currentPresetFile == juce::File());

        if (! presetFiles.empty())
            loadMenu.addSeparator();

        for (int index = 0; index < static_cast<int> (presetFiles.size()); ++index)
        {
            const auto& file = presetFiles[static_cast<size_t> (index)];
            loadMenu.addItem (firstUserPresetId + index,
                              file.getFileNameWithoutExtension(), true,
                              file == currentPresetFile);
        }

        juce::PopupMenu menu;
        menu.addItem (savePresetId, "SAVE");
        menu.addItem (savePresetAsId, "SAVE AS...");
        menu.addSeparator();
        menu.addSubMenu ("LOAD", loadMenu);

        juce::Component::SafePointer<PresetSectionComponent> safeSection (this);
        menu.showMenuAsync
            (juce::PopupMenu::Options().withTargetComponent (&menuButton),
             [safeSection, savePresetId, savePresetAsId,
              initialPresetId, firstUserPresetId] (int result)
             {
                 auto* section = safeSection.getComponent();
                 if (section == nullptr || result == 0)
                     return;

                 if (result == savePresetId)
                     section->saveCurrentPreset();
                 else if (result == savePresetAsId)
                     section->showPresetNamePrompt (false);
                 else if (result == initialPresetId)
                     section->loadInitialPreset();
                 else if (result >= firstUserPresetId)
                 {
                     const auto index = result - firstUserPresetId;
                     if (index >= 0
                         && index < static_cast<int> (section->presetFiles.size()))
                         section->loadPresetFile
                             (section->presetFiles[static_cast<size_t> (index)]);
                 }
             });
    }

    void showPresetError (const juce::String& message) const
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "TapeStopper Presets", message);
    }

    TapeStopperAudioProcessor& processor;
    juce::File presetDirectory;
    std::vector<juce::File> presetFiles;
    juce::File currentPresetFile;
    juce::String currentPresetName { "INIT" };
    PresetNameDisplay nameDisplay;
    PresetButton previousButton { "<" };
    PresetButton nextButton { ">" };
    PresetButton nameButton { "NAME" };
    PresetButton menuButton { "MENU" };
    std::unique_ptr<juce::AlertWindow> presetNameWindow;
    static constexpr int designWidth = 145;
    static constexpr int designHeight = 41;
};

class MainTriggerComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit MainTriggerComponent (TapeStopperAudioProcessor& owner)
        : processor (owner),
          engageParameter (owner.getValueTreeState().getParameter (engageParameterId)),
          triggerModeValue (owner.getValueTreeState().getRawParameterValue
                            (triggerModeParameterId))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    ~MainTriggerComponent() override
    {
        if (gestureActive && engageParameter != nullptr)
            engageParameter->endChangeGesture();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        const auto manualEngaged = engageParameter != nullptr
                                   && engageParameter->getValue() >= 0.5f;
        const auto sequencerControlling = processor.getCurrentSequencerStep() >= 0;
        const auto engaged = sequencerControlling
                                 ? processor.isSequencerGateActive() : manualEngaged;
        const auto hoverLift = isMouseOver() ? 0.10f : 0.0f;

        juce::ColourGradient fill (juce::Colour (0xff5bd4ff).brighter (hoverLift),
                                   bounds.getX(), bounds.getY(),
                                   juce::Colour (0xff075ed6).brighter (hoverLift),
                                   bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (juce::Colour (0xff0b347d));
        g.drawRoundedRectangle (bounds, 7.0f, 5.0f);

        g.setColour (juce::Colour (0xffbaf4ff));
        const auto showStopIcon = engaged != processor.isButtonDisplayReversed();

        if (showStopIcon)
        {
            const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.34f;
            g.fillRoundedRectangle (bounds.withSizeKeepingCentre (side, side), 2.0f);
        }
        else
        {
            const juce::Point<float> upperLeft
                { bounds.getX() + bounds.getWidth() * 0.37f,
                  bounds.getY() + bounds.getHeight() * 0.27f };
            const juce::Point<float> lowerLeft
                { upperLeft.x, bounds.getBottom() - bounds.getHeight() * 0.27f };
            const juce::Point<float> right
                { bounds.getX() + bounds.getWidth() * 0.72f, bounds.getCentreY() };
            const auto between = [] (juce::Point<float> from,
                                     juce::Point<float> to, float amount)
            {
                return juce::Point<float> { from.x + (to.x - from.x) * amount,
                                            from.y + (to.y - from.y) * amount };
            };

            constexpr auto cornerAmount = 0.12f;
            const auto start = between (upperLeft, right, cornerAmount);
            const auto beforeRight = between (right, upperLeft, cornerAmount);
            const auto afterRight = between (right, lowerLeft, cornerAmount);
            const auto beforeLower = between (lowerLeft, right, cornerAmount);
            const auto afterLower = between (lowerLeft, upperLeft, cornerAmount);
            const auto beforeUpper = between (upperLeft, lowerLeft, cornerAmount);

            juce::Path play;
            play.startNewSubPath (start);
            play.lineTo (beforeRight);
            play.quadraticTo (right.x, right.y, afterRight.x, afterRight.y);
            play.lineTo (beforeLower);
            play.quadraticTo (lowerLeft.x, lowerLeft.y, afterLower.x, afterLower.y);
            play.lineTo (beforeUpper);
            play.quadraticTo (upperLeft.x, upperLeft.y, start.x, start.y);
            play.closeSubPath();
            g.fillPath (play);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isLeftButtonDown() || engageParameter == nullptr
            || processor.getCurrentSequencerStep() >= 0)
            return;

        engageParameter->beginChangeGesture();
        gestureActive = true;

        const auto toggleMode = triggerModeValue != nullptr
                                && triggerModeValue->load() >= 0.5f;
        const auto newValue = toggleMode ? engageParameter->getValue() < 0.5f : true;
        engageParameter->setValueNotifyingHost (newValue ? 1.0f : 0.0f);

        if (toggleMode)
        {
            engageParameter->endChangeGesture();
            gestureActive = false;
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! gestureActive || engageParameter == nullptr)
            return;

        engageParameter->setValueNotifyingHost (0.0f);
        engageParameter->endChangeGesture();
        gestureActive = false;
        repaint();
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    TapeStopperAudioProcessor& processor;
    juce::RangedAudioParameter* engageParameter = nullptr;
    std::atomic<float>* triggerModeValue = nullptr;
    bool gestureActive = false;
};

class TimingBarComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    TimingBarComponent (TapeStopperAudioProcessor& owner,
                        juce::AudioProcessorValueTreeState& state)
        : processor (owner),
          downParameter (state.getParameter (downSpeedParameterId)),
          upParameter (state.getParameter (upSpeedParameterId)),
          downValue (state.getRawParameterValue (downSpeedParameterId)),
          upValue (state.getRawParameterValue (upSpeedParameterId)),
          downSyncParameter (state.getParameter (downSyncDivisionParameterId)),
          upSyncParameter (state.getParameter (upSyncDivisionParameterId)),
          downSyncValue (state.getRawParameterValue (downSyncDivisionParameterId)),
          upSyncValue (state.getRawParameterValue (upSyncDivisionParameterId)),
          timingModeValue (state.getRawParameterValue (timingModeParameterId))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        startTimerHz (30);
    }

    ~TimingBarComponent() override
    {
        if (activeParameter != nullptr)
            activeParameter->endChangeGesture();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bar = getBarBounds();
        g.setColour (juce::Colour (0xff171b17));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (juce::Colour (0xff3c4140));
        g.drawRoundedRectangle (bar, 3.0f, 2.0f);

        const auto usableWidth = juce::jmax (1.0f, bar.getWidth() - 8.0f);
        const auto markerX = [&bar, usableWidth] (float value)
        {
            return bar.getX() + 4.0f + usableWidth * juce::jlimit (0.0f, 1.0f, value);
        };

        const auto isSynced = timingModeValue != nullptr && timingModeValue->load() >= 0.5f;
        const auto downDivision = juce::jlimit
                                  (0, TapeStopperAudioProcessor::numSyncDivisions - 1,
                                   downSyncValue != nullptr
                                       ? juce::roundToInt (downSyncValue->load()) : 5);
        const auto upDivision = juce::jlimit
                                (0, TapeStopperAudioProcessor::numSyncDivisions - 1,
                                 upSyncValue != nullptr
                                     ? juce::roundToInt (upSyncValue->load()) : 5);
        const auto down = isSynced
                              ? static_cast<float> (downDivision)
                                    / static_cast<float>
                                        (TapeStopperAudioProcessor::numSyncDivisions - 1)
                              : (downValue != nullptr ? downValue->load() : 0.20f);
        const auto up = isSynced
                            ? static_cast<float> (upDivision)
                                  / static_cast<float>
                                      (TapeStopperAudioProcessor::numSyncDivisions - 1)
                            : (upValue != nullptr ? upValue->load() : 0.20f);
        const auto position = processor.getVisualPosition();

        g.setColour (juce::Colour (0xff39d458));
        g.fillRect (markerX (down) - 1.5f, bar.getY() + 2.0f, 3.0f, bar.getHeight() - 4.0f);
        g.setColour (juce::Colour (0xffff3939));
        g.fillRect (markerX (up) - 1.5f, bar.getY() + 2.0f, 3.0f, bar.getHeight() - 4.0f);
        g.setColour (juce::Colour (0xff73b8d1));
        g.fillRect (markerX (position) - 1.0f, bar.getY() + 2.0f, 2.0f, bar.getHeight() - 4.0f);

        juce::String downText;
        juce::String upText;

        if (isSynced)
        {
            downText = TapeStopperAudioProcessor::syncDivisionName (downDivision);
            upText = TapeStopperAudioProcessor::syncDivisionName (upDivision);
        }
        else
        {
            const auto downSeconds = TapeStopperAudioProcessor::speedControlToSeconds (down);
            const auto upSeconds = TapeStopperAudioProcessor::speedControlToSeconds (up);
            downText = juce::String (downSeconds, downSeconds >= 1.0f ? 2 : 3) + " s";
            upText = juce::String (upSeconds, upSeconds >= 1.0f ? 2 : 3) + " s";
        }

        g.setColour (juce::Colour (0xff343637));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("DOWN " + downText + "    /    UP " + upText,
                    getLocalBounds().removeFromBottom (18), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! getBarBounds().contains (event.position))
            return;

        const auto isSynced = timingModeValue != nullptr && timingModeValue->load() >= 0.5f;
        activeParameter = isSynced
                              ? (event.mods.isRightButtonDown()
                                     ? upSyncParameter : downSyncParameter)
                              : (event.mods.isRightButtonDown() ? upParameter : downParameter);

        if (activeParameter != nullptr)
        {
            activeParameter->beginChangeGesture();
            updateFromMouse (event);
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (activeParameter != nullptr)
            updateFromMouse (event);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (getBarBounds().contains (event.position)
                            ? juce::MouseCursor::LeftRightResizeCursor
                            : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (activeParameter != nullptr)
        {
            activeParameter->endChangeGesture();
            activeParameter = nullptr;
        }
    }

private:
    juce::Rectangle<float> getBarBounds() const
    {
        return getLocalBounds().toFloat().withTrimmedBottom (19.0f).reduced (1.0f);
    }

    void timerCallback() override
    {
        repaint();
    }

    void updateFromMouse (const juce::MouseEvent& event)
    {
        const auto width = static_cast<float> (juce::jmax (1, getWidth() - 10));
        const auto value = juce::jlimit (0.0f, 1.0f, (event.position.x - 5.0f) / width);
        const auto isSyncParameter = activeParameter == downSyncParameter
                                     || activeParameter == upSyncParameter;

        if (isSyncParameter)
        {
            const auto division = juce::jlimit
                                  (0, TapeStopperAudioProcessor::numSyncDivisions - 1,
                                   juce::roundToInt
                                       (value * (TapeStopperAudioProcessor::numSyncDivisions - 1)));
            activeParameter->setValueNotifyingHost
                (activeParameter->convertTo0to1 (static_cast<float> (division)));
        }
        else
        {
            activeParameter->setValueNotifyingHost (value);
        }

        repaint();
    }

    TapeStopperAudioProcessor& processor;
    juce::RangedAudioParameter* downParameter = nullptr;
    juce::RangedAudioParameter* upParameter = nullptr;
    std::atomic<float>* downValue = nullptr;
    std::atomic<float>* upValue = nullptr;
    juce::RangedAudioParameter* downSyncParameter = nullptr;
    juce::RangedAudioParameter* upSyncParameter = nullptr;
    std::atomic<float>* downSyncValue = nullptr;
    std::atomic<float>* upSyncValue = nullptr;
    std::atomic<float>* timingModeValue = nullptr;
    juce::RangedAudioParameter* activeParameter = nullptr;
};

class EnvelopeEditorComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    EnvelopeEditorComponent (TapeStopperAudioProcessor& owner,
                             juce::AudioProcessorValueTreeState& state)
        : processor (owner)
    {
        for (int point = 1; point < TapeStopperAudioProcessor::numEnvelopePoints - 1; ++point)
        {
            xParameters[static_cast<size_t> (point - 1)]
                = state.getParameter (envelopeXParameterId (point));
            xValues[static_cast<size_t> (point - 1)]
                = state.getRawParameterValue (envelopeXParameterId (point));
        }

        for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
        {
            yParameters[static_cast<size_t> (point)]
                = state.getParameter (envelopeYParameterId (point));
            yValues[static_cast<size_t> (point)]
                = state.getRawParameterValue (envelopeYParameterId (point));
        }

        const std::array<juce::String, 3> targetNames
            { juce::String ("pitch"), juce::String ("filter"),
              juce::String ("volume") };
        for (int target = 0; target < 3; ++target)
            for (int direction = 0; direction < 2; ++direction)
                for (int point = 0;
                     point < TapeStopperAudioProcessor::numCurveControlPoints; ++point)
                {
                    const auto parameterId = curvePointParameterId
                        (targetNames[static_cast<size_t> (target)],
                         direction == 0 ? "Down" : "Up", point);
                    curveParameters[static_cast<size_t> (target)]
                                   [static_cast<size_t> (direction)]
                                   [static_cast<size_t> (point)]
                        = state.getParameter (parameterId);
                    curveValues[static_cast<size_t> (target)]
                               [static_cast<size_t> (direction)]
                               [static_cast<size_t> (point)]
                        = state.getRawParameterValue (parameterId);
                }

        const std::array<const char*, 4> enabledIds
            { pitchCurveEnabledParameterId, filterCurveEnabledParameterId,
              volumeCurveEnabledParameterId, envelopeEnabledParameterId };
        for (int view = 0; view < 4; ++view)
        {
            enabledParameters[static_cast<size_t> (view)]
                = state.getParameter (enabledIds[static_cast<size_t> (view)]);
            enabledValues[static_cast<size_t> (view)]
                = state.getRawParameterValue (enabledIds[static_cast<size_t> (view)]);
        }

        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        startTimerHz (30);
    }

    ~EnvelopeEditorComponent() override
    {
        endActiveGestures();
    }

    void paint (juce::Graphics& g) override
    {
        const auto graph = getGraphBounds();
        g.setColour (juce::Colour (0xff08120d));
        g.fillRoundedRectangle (graph, 3.0f);
        g.setColour (juce::Colour (0xff22583a));
        g.drawRoundedRectangle (graph, 3.0f, 1.0f);

        if (processor.isWaveformDisplayEnabled())
        {
            std::array<float, TapeStopperAudioProcessor::waveformSampleCount>
                waveform {};
            processor.copyWaveformSamples (waveform);

            juce::Path waveformPath;
            auto pathActive = false;
            for (int sample = 0; sample < TapeStopperAudioProcessor::waveformSampleCount;
                 ++sample)
            {
                const auto speed = waveform[static_cast<size_t> (sample)];
                if (speed < 0.0f)
                {
                    pathActive = false;
                    continue;
                }

                const auto x = graph.getX() + graph.getWidth()
                               * static_cast<float> (sample)
                               / static_cast<float>
                                   (TapeStopperAudioProcessor::waveformSampleCount - 1);
                const auto y = graph.getBottom() - 2.0f
                               - juce::jlimit (0.0f, 1.0f, speed)
                                     * (graph.getHeight() - 4.0f);

                if (! pathActive)
                {
                    waveformPath.startNewSubPath (x, y);
                    pathActive = true;
                }
                else
                    waveformPath.lineTo (x, y);
            }

            g.setColour (juce::Colour (0xffff3d3d).withAlpha (0.72f));
            g.strokePath (waveformPath, juce::PathStrokeType (1.6f));
        }

        const auto viewColour = getViewColour (selectedView);
        const auto verticalDivisions = selectedView == envelopeView
                                           ? TapeStopperAudioProcessor::numEnvelopePoints - 1
                                           : TapeStopperAudioProcessor::numCurveControlPoints + 1;
        g.setColour (viewColour.darker (0.58f).withAlpha (0.70f));
        for (int division = 1; division < verticalDivisions; ++division)
        {
            const auto x = graph.getX() + graph.getWidth()
                           * static_cast<float> (division)
                           / static_cast<float> (verticalDivisions);
            g.drawVerticalLine (juce::roundToInt (x), graph.getY(), graph.getBottom());
        }
        for (int division = 1; division < 4; ++division)
        {
            const auto y = graph.getY() + graph.getHeight()
                           * static_cast<float> (division) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        const auto selectedEnabled = isViewEnabled (selectedView);
        const auto curveAlpha = selectedEnabled ? 1.0f : 0.42f;

        if (selectedView == envelopeView)
        {
            g.setColour (juce::Colour (0xff34764e));
            g.drawLine (graph.getX(), graph.getCentreY(), graph.getRight(),
                        graph.getCentreY(), 1.4f);

            std::array<juce::Point<float>, TapeStopperAudioProcessor::numEnvelopePoints>
                points;
            for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints;
                 ++point)
                points[static_cast<size_t> (point)] = getPointPosition (point, graph);

            juce::Path curve;
            curve.startNewSubPath (points.front());
            for (int point = 1; point < TapeStopperAudioProcessor::numEnvelopePoints;
                 ++point)
                curve.lineTo (points[static_cast<size_t> (point)]);

            g.setColour (viewColour.withAlpha (curveAlpha));
            g.strokePath (curve, juce::PathStrokeType (2.0f));

            for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints;
                 ++point)
            {
                const auto size = point == activePoint ? 8.0f : 6.0f;
                const auto handle = juce::Rectangle<float> (size, size)
                                        .withCentre (points[static_cast<size_t> (point)]);
                g.setColour (point == activePoint ? juce::Colour (0xffffff72)
                                                  : juce::Colour (0xffb9ffc8)
                                                        .withAlpha (curveAlpha));
                g.fillRect (handle);
                g.setColour (juce::Colour (0xff0a3317));
                g.drawRect (handle, 1.0f);
            }

            if (activePoint >= 0)
            {
                const auto semitones = (0.5f - getPointY (activePoint)) * 24.0f;
                const auto prefix = semitones > 0.0f ? "+" : "";
                drawActiveReadout (g, graph,
                                   "ENV " + juce::String (activePoint + 1) + "  "
                                       + prefix + juce::String (semitones, 1) + " st");
            }
        }
        else
        {
            const auto downCurve = createCurvePath (selectedView, downDirection, graph);
            const auto upCurve = createCurvePath (selectedView, upDirection, graph);
            const auto downColour = getCurveColour (selectedView, downDirection);
            const auto upColour = getCurveColour (selectedView, upDirection);
            g.setColour (upColour.withAlpha (0.88f * curveAlpha));
            g.strokePath (upCurve, juce::PathStrokeType (3.6f));
            g.setColour (downColour.withAlpha (curveAlpha));
            g.strokePath (downCurve, juce::PathStrokeType (1.8f));

            for (int direction = 0; direction < 2; ++direction)
                for (int point = 0;
                     point < TapeStopperAudioProcessor::numCurveControlPoints; ++point)
                {
                    const auto position = getCurvePointPosition
                        (selectedView, direction, point, graph);
                    const auto active = direction == activeCurveDirection
                                        && point == activeCurvePoint;
                    const auto handle = juce::Rectangle<float>
                                        (active ? 8.0f : 6.0f,
                                         active ? 8.0f : 6.0f).withCentre (position);
                    g.setColour (active ? juce::Colour (0xffffff72)
                                        : (direction == downDirection
                                               ? downColour : upColour)
                                              .withAlpha (curveAlpha));
                    if (direction == downDirection)
                        g.fillRect (handle);
                    else
                        g.fillEllipse (handle);
                    g.setColour (juce::Colour (0xff101616));
                    if (direction == downDirection)
                        g.drawRect (handle, 1.0f);
                    else
                        g.drawEllipse (handle, 1.0f);
                }

            g.setColour (downColour.withAlpha (0.88f));
            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            g.drawText ("L: DOWN", graph.toNearestInt().reduced (5, 3),
                        juce::Justification::topLeft, false);
            g.setColour (upColour.withAlpha (0.88f));
            g.drawText ("R: UP", graph.toNearestInt().reduced (5, 3),
                        juce::Justification::topRight, false);

            if (activeCurvePoint >= 0)
            {
                const auto value = getCurvePointY
                    (selectedView, activeCurveDirection, activeCurvePoint);
                drawActiveReadout
                    (g, graph,
                     juce::String (activeCurveDirection == downDirection ? "DOWN " : "UP ")
                         + juce::String (activeCurvePoint + 1) + "  "
                         + juce::String (juce::roundToInt (value * 100.0f)) + "%");
            }
        }

        paintTabs (g);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.getNumberOfClicks() > 1)
            return;

        if (getTabArea().contains (event.position))
        {
            endActiveGestures();
            const auto tab = tabAtPosition (event.position);
            if (tab < 0)
                return;

            if (getLedBounds (tab).expanded (4.0f).contains (event.position))
                toggleViewEnabled (tab);
            else
                selectedView = tab;
            repaint();
            return;
        }

        if (selectedView != envelopeView)
        {
            const auto direction = event.mods.isRightButtonDown()
                                       ? upDirection : downDirection;
            if (! event.mods.isLeftButtonDown() && ! event.mods.isRightButtonDown())
                return;
            const auto closestPoint = findClosestCurvePoint
                                      (event.position, direction);
            if (closestPoint < 0)
                return;

            activeCurveDirection = direction;
            activeCurvePoint = closestPoint;
            if (auto* parameter = getCurveParameter
                                  (selectedView, direction, closestPoint))
                parameter->beginChangeGesture();
            updateActiveCurvePoint (event.position);
            return;
        }

        if (! event.mods.isLeftButtonDown())
            return;

        const auto closestPoint = findClosestPoint (event.position);

        if (closestPoint < 0)
            return;

        activePoint = closestPoint;
        if (auto* xParameter = getXParameter (activePoint))
            xParameter->beginChangeGesture();
        if (auto* yParameter = yParameters[static_cast<size_t> (activePoint)])
            yParameter->beginChangeGesture();
        updateActivePoint (event.position);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! getGraphBounds().contains (event.position))
            return;

        if (selectedView != envelopeView)
        {
            resetCurveDirection (selectedView,
                                 event.mods.isRightButtonDown()
                                     ? upDirection : downDirection);
            return;
        }

        if (! event.mods.isLeftButtonDown())
            return;

        const auto point = findClosestPoint (event.position);
        if (point < 0)
            return;

        endActiveGestures();

        if (auto* yParameter = yParameters[static_cast<size_t> (point)])
        {
            yParameter->beginChangeGesture();
            yParameter->setValueNotifyingHost (yParameter->convertTo0to1 (0.5f));
            yParameter->endChangeGesture();
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (activeCurvePoint >= 0)
            updateActiveCurvePoint (event.position);
        else if (activePoint >= 0)
            updateActivePoint (event.position);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (getTabArea().contains (event.position)
                            ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::CrosshairCursor);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endActiveGestures();
    }

    void resetCurrentView()
    {
        endActiveGestures();
        if (selectedView == envelopeView)
        {
            for (auto* parameter : yParameters)
                setParameterValue (parameter, 0.5f);
        }
        else
        {
            resetCurveDirection (selectedView, downDirection);
            resetCurveDirection (selectedView, upDirection);
        }
        repaint();
    }

    juce::String getResetButtonText() const
    {
        return selectedView == envelopeView ? "ENV RESET" : "CURVE RESET";
    }

private:
    static constexpr int pitchView = 0;
    static constexpr int filterView = 1;
    static constexpr int volumeView = 2;
    static constexpr int envelopeView = 3;
    static constexpr int downDirection = 0;
    static constexpr int upDirection = 1;

    void timerCallback() override
    {
        repaint();
    }

    juce::Rectangle<float> getGraphBounds() const
    {
        return getLocalBounds().toFloat().withTrimmedBottom (22.0f)
                               .reduced (5.0f, 3.0f);
    }

    juce::Rectangle<float> getTabArea() const
    {
        auto area = getLocalBounds().toFloat();
        return area.removeFromBottom (21.0f);
    }

    juce::Rectangle<float> getTabBounds (int tab) const
    {
        const auto area = getTabArea();
        constexpr auto gap = 5.0f;
        const auto width = (area.getWidth() - gap * 3.0f) / 4.0f;
        return { area.getX() + (width + gap) * static_cast<float> (tab),
                 area.getY(), width, area.getHeight() };
    }

    juce::Rectangle<float> getLedBounds (int tab) const
    {
        const auto tabBounds = getTabBounds (tab);
        return juce::Rectangle<float> (8.0f, 8.0f)
            .withCentre ({ tabBounds.getX() + 8.0f, tabBounds.getCentreY() });
    }

    int tabAtPosition (juce::Point<float> position) const
    {
        for (int tab = 0; tab < 4; ++tab)
            if (getTabBounds (tab).contains (position))
                return tab;
        return -1;
    }

    static juce::Colour getViewColour (int view)
    {
        const std::array<juce::Colour, 4> colours
            { juce::Colour (0xff36a9ff), juce::Colour (0xffff9e3d),
              juce::Colour (0xffc779ff), juce::Colour (0xff49ff70) };
        return colours[static_cast<size_t> (juce::jlimit (0, 3, view))];
    }

    static juce::Colour getCurveColour (int view, int direction)
    {
        const std::array<std::array<juce::Colour, 2>, 3> colours
            {{ { juce::Colour (0xff35e7ff), juce::Colour (0xff3278ff) },
               { juce::Colour (0xffff9e3d), juce::Colour (0xffa96232) },
               { juce::Colour (0xffd76cff), juce::Colour (0xff5840bd) } }};
        return colours[static_cast<size_t> (juce::jlimit (pitchView, volumeView, view))]
                      [static_cast<size_t> (juce::jlimit (downDirection, upDirection,
                                                         direction))];
    }

    bool isViewEnabled (int view) const
    {
        const auto* value = enabledValues[static_cast<size_t>
                                          (juce::jlimit (0, 3, view))];
        return value != nullptr && value->load() >= 0.5f;
    }

    void toggleViewEnabled (int view)
    {
        if (auto* parameter = enabledParameters[static_cast<size_t>
                                                (juce::jlimit (0, 3, view))])
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getValue() < 0.5f
                                                   ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
    }

    void paintTabs (juce::Graphics& g) const
    {
        const std::array<juce::String, 4> names
            { juce::String ("PITCH"), juce::String ("FILTER"),
              juce::String ("VOLUME"), juce::String ("ENVELOPE") };

        for (int tab = 0; tab < 4; ++tab)
        {
            const auto tabBounds = getTabBounds (tab);
            const auto led = getLedBounds (tab);
            const auto button = tabBounds.withTrimmedLeft (14.0f).reduced (1.5f, 2.0f);
            const auto colour = getViewColour (tab);
            const auto selected = tab == selectedView;

            g.setColour (juce::Colour (0xff303335));
            g.fillEllipse (led.expanded (1.2f));
            g.setColour (isViewEnabled (tab) ? colour : colour.withAlpha (0.20f));
            g.fillEllipse (led);

            juce::ColourGradient fill
                (selected ? juce::Colour (0xffd6e7f2) : juce::Colour (0xffd0d1cf),
                 button.getX(), button.getY(),
                 selected ? juce::Colour (0xff859aaa) : juce::Colour (0xff8f9291),
                 button.getX(), button.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (button, 2.0f);
            g.setColour (selected ? juce::Colour (0xff168bd4)
                                  : juce::Colour (0xff4b4e50));
            g.drawRoundedRectangle (button, 2.0f, selected ? 1.6f : 1.0f);
            g.setColour (colour.darker (0.55f));
            g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
            g.drawFittedText (names[static_cast<size_t> (tab)],
                              button.toNearestInt().reduced (2, 0),
                              juce::Justification::centred, 1, 0.72f);
        }
    }

    float getCurvePointY (int view, int direction, int point) const
    {
        const auto* value = curveValues[static_cast<size_t> (view)]
                                      [static_cast<size_t> (direction)]
                                      [static_cast<size_t> (point)];
        return value != nullptr ? juce::jlimit (0.0f, 1.0f, value->load())
                                : static_cast<float> (point + 1)
                                      / static_cast<float>
                                          (TapeStopperAudioProcessor::numCurveControlPoints
                                           + 1);
    }

    juce::RangedAudioParameter* getCurveParameter (int view, int direction,
                                                    int point) const
    {
        if (view < pitchView || view > volumeView
            || direction < downDirection || direction > upDirection
            || point < 0 || point >= TapeStopperAudioProcessor::numCurveControlPoints)
            return nullptr;
        return curveParameters[static_cast<size_t> (view)]
                              [static_cast<size_t> (direction)]
                              [static_cast<size_t> (point)];
    }

    std::array<float, TapeStopperAudioProcessor::numCurveControlPoints>
    getCurveControls (int view, int direction) const
    {
        std::array<float, TapeStopperAudioProcessor::numCurveControlPoints> result {};
        for (int point = 0; point < TapeStopperAudioProcessor::numCurveControlPoints;
             ++point)
            result[static_cast<size_t> (point)] = getCurvePointY (view, direction, point);
        return result;
    }

    static float evaluateDisplayCurve
        (const std::array<float, TapeStopperAudioProcessor::numCurveControlPoints>& controls,
         float position)
    {
        constexpr auto numValues = TapeStopperAudioProcessor::numCurveControlPoints + 2;
        std::array<float, numValues> values {};
        values.front() = 0.0f;
        values.back() = 1.0f;
        for (int point = 0; point < TapeStopperAudioProcessor::numCurveControlPoints;
             ++point)
            values[static_cast<size_t> (point + 1)]
                = juce::jlimit (0.0f, 1.0f,
                                controls[static_cast<size_t> (point)]);

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
        return juce::jlimit
            (0.0f, 1.0f,
             0.5f * ((2.0f * p1) + (-p0 + p2) * t
                     + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                     + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
    }

    juce::Path createCurvePath (int view, int direction,
                                juce::Rectangle<float> graph) const
    {
        const auto controls = getCurveControls (view, direction);
        juce::Path path;
        for (int sample = 0; sample <= 96; ++sample)
        {
            const auto x = static_cast<float> (sample) / 96.0f;
            const auto point = juce::Point<float>
                { graph.getX() + graph.getWidth() * x,
                  graph.getY() + graph.getHeight()
                                     * evaluateDisplayCurve (controls, x) };
            if (sample == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        return path;
    }

    juce::Point<float> getCurvePointPosition (int view, int direction, int point,
                                              juce::Rectangle<float> graph) const
    {
        const auto x = static_cast<float> (point + 1)
                       / static_cast<float>
                           (TapeStopperAudioProcessor::numCurveControlPoints + 1);
        return { graph.getX() + graph.getWidth() * x,
                 graph.getY() + graph.getHeight()
                                    * getCurvePointY (view, direction, point) };
    }

    int findClosestCurvePoint (juce::Point<float> position, int direction) const
    {
        const auto graph = getGraphBounds();
        auto closestDistance = 14.0f;
        auto closestPoint = -1;
        for (int point = 0; point < TapeStopperAudioProcessor::numCurveControlPoints;
             ++point)
        {
            const auto distance = position.getDistanceFrom
                (getCurvePointPosition (selectedView, direction, point, graph));
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestPoint = point;
            }
        }
        return closestPoint;
    }

    void updateActiveCurvePoint (juce::Point<float> position)
    {
        if (activeCurvePoint < 0)
            return;
        const auto graph = getGraphBounds();
        const auto value = juce::jlimit
            (0.0f, 1.0f, (position.y - graph.getY()) / graph.getHeight());
        if (auto* parameter = getCurveParameter
                              (selectedView, activeCurveDirection, activeCurvePoint))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        repaint();
    }

    static void setParameterValue (juce::RangedAudioParameter* parameter, float value)
    {
        if (parameter == nullptr)
            return;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }

    void resetCurveDirection (int view, int direction)
    {
        endActiveGestures();
        for (int point = 0; point < TapeStopperAudioProcessor::numCurveControlPoints;
             ++point)
            setParameterValue (getCurveParameter (view, direction, point),
                               static_cast<float> (point + 1)
                                   / static_cast<float>
                                       (TapeStopperAudioProcessor::numCurveControlPoints
                                        + 1));
        repaint();
    }

    static void drawActiveReadout (juce::Graphics& g, juce::Rectangle<float> graph,
                                   const juce::String& text)
    {
        g.setColour (juce::Colour (0xffd7ffe0));
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText (text, graph.toNearestInt().reduced (5, 3),
                    juce::Justification::bottomRight, false);
    }

    int findClosestPoint (juce::Point<float> position) const
    {
        const auto graph = getGraphBounds();
        auto closestDistance = 12.0f;
        auto closestPoint = -1;

        for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
        {
            const auto distance = position.getDistanceFrom (getPointPosition (point, graph));
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestPoint = point;
            }
        }

        return closestPoint;
    }

    float getPointX (int point) const
    {
        if (point <= 0)
            return 0.0f;
        if (point >= TapeStopperAudioProcessor::numEnvelopePoints - 1)
            return 1.0f;

        const auto* value = xValues[static_cast<size_t> (point - 1)];
        return value != nullptr ? juce::jlimit (0.0f, 1.0f, value->load())
                                : static_cast<float> (point)
                                      / static_cast<float>
                                          (TapeStopperAudioProcessor::numEnvelopePoints - 1);
    }

    float getPointY (int point) const
    {
        const auto* value = yValues[static_cast<size_t> (point)];
        return value != nullptr ? juce::jlimit (0.0f, 1.0f, value->load()) : 0.5f;
    }

    juce::Point<float> getPointPosition (int point, juce::Rectangle<float> graph) const
    {
        return { graph.getX() + graph.getWidth() * getPointX (point),
                 graph.getY() + graph.getHeight() * getPointY (point) };
    }

    juce::RangedAudioParameter* getXParameter (int point) const
    {
        if (point <= 0 || point >= TapeStopperAudioProcessor::numEnvelopePoints - 1)
            return nullptr;
        return xParameters[static_cast<size_t> (point - 1)];
    }

    void updateActivePoint (juce::Point<float> position)
    {
        const auto graph = getGraphBounds();
        const auto requestedY = juce::jlimit
                                (0.0f, 1.0f, (position.y - graph.getY()) / graph.getHeight());

        if (auto* yParameter = yParameters[static_cast<size_t> (activePoint)])
            yParameter->setValueNotifyingHost (yParameter->convertTo0to1 (requestedY));

        if (auto* xParameter = getXParameter (activePoint))
        {
            constexpr auto minimumSpacing = 0.015f;
            const auto minimum = getPointX (activePoint - 1) + minimumSpacing;
            const auto maximum = getPointX (activePoint + 1) - minimumSpacing;
            const auto rawX = (position.x - graph.getX()) / graph.getWidth();
            const auto requestedX = maximum >= minimum
                                        ? juce::jlimit (minimum, maximum, rawX)
                                        : 0.5f * (minimum + maximum);
            xParameter->setValueNotifyingHost (xParameter->convertTo0to1 (requestedX));
        }

        repaint();
    }

    void endActiveGestures()
    {
        if (activeCurvePoint >= 0)
        {
            if (auto* parameter = getCurveParameter
                                  (selectedView, activeCurveDirection, activeCurvePoint))
                parameter->endChangeGesture();
            activeCurvePoint = -1;
            activeCurveDirection = -1;
        }

        if (activePoint >= 0)
        {
            if (auto* xParameter = getXParameter (activePoint))
                xParameter->endChangeGesture();
            if (auto* yParameter = yParameters[static_cast<size_t> (activePoint)])
                yParameter->endChangeGesture();
            activePoint = -1;
        }
        repaint();
    }

    TapeStopperAudioProcessor& processor;
    std::array<juce::RangedAudioParameter*, TapeStopperAudioProcessor::numEnvelopePoints - 2>
        xParameters {};
    std::array<std::atomic<float>*, TapeStopperAudioProcessor::numEnvelopePoints - 2>
        xValues {};
    std::array<juce::RangedAudioParameter*, TapeStopperAudioProcessor::numEnvelopePoints>
        yParameters {};
    std::array<std::atomic<float>*, TapeStopperAudioProcessor::numEnvelopePoints>
        yValues {};
    std::array<std::array<std::array<juce::RangedAudioParameter*,
                                    TapeStopperAudioProcessor::numCurveControlPoints>, 2>, 3>
        curveParameters {};
    std::array<std::array<std::array<std::atomic<float>*,
                                    TapeStopperAudioProcessor::numCurveControlPoints>, 2>, 3>
        curveValues {};
    std::array<juce::RangedAudioParameter*, 4> enabledParameters {};
    std::array<std::atomic<float>*, 4> enabledValues {};
    int selectedView = envelopeView;
    int activePoint = -1;
    int activeCurvePoint = -1;
    int activeCurveDirection = -1;
};

class SequencerEnableLedComponent final : public juce::Component,
                                          public juce::SettableTooltipClient,
                                          private juce::Timer
{
public:
    explicit SequencerEnableLedComponent (juce::AudioProcessorValueTreeState& state)
        : parameter (state.getParameter (sequencerEnabledParameterId)),
          value (state.getRawParameterValue (sequencerEnabledParameterId))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Enable or disable sequencer control of the main Play button");
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto enabled = value != nullptr && value->load() >= 0.5f;
        g.setColour (juce::Colour (0xff343738));
        g.fillEllipse (area.expanded (1.5f));
        g.setColour (enabled ? juce::Colour (0xff35c6ff)
                             : juce::Colour (0xff35c6ff).withAlpha (0.20f));
        g.fillEllipse (area);
        if (enabled)
        {
            g.setColour (juce::Colour (0xffbcefff).withAlpha (0.75f));
            g.fillEllipse (area.reduced (2.5f));
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isLeftButtonDown() || parameter == nullptr)
            return;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getValue() < 0.5f ? 1.0f : 0.0f);
        parameter->endChangeGesture();
        repaint();
    }

private:
    void timerCallback() override { repaint(); }

    juce::RangedAudioParameter* parameter = nullptr;
    std::atomic<float>* value = nullptr;
};

class SequencerPanelComponent final : public juce::Component,
                                      public juce::SettableTooltipClient,
                                      private juce::Timer
{
public:
    SequencerPanelComponent (TapeStopperAudioProcessor& owner,
                             juce::AudioProcessorValueTreeState& state)
        : processor (owner),
          clockModeParameter (state.getParameter (sequencerClockModeParameterId)),
          resolutionParameter (state.getParameter (sequencerResolutionParameterId)),
          freeRateParameter (state.getParameter (sequencerFreeRateParameterId)),
          lengthParameter (state.getParameter (sequencerLengthParameterId)),
          offsetParameter (state.getParameter (sequencerOffsetParameterId)),
          clockModeValue (state.getRawParameterValue (sequencerClockModeParameterId)),
          resolutionValue (state.getRawParameterValue (sequencerResolutionParameterId)),
          freeRateValue (state.getRawParameterValue (sequencerFreeRateParameterId)),
          lengthValue (state.getRawParameterValue (sequencerLengthParameterId)),
          offsetValue (state.getRawParameterValue (sequencerOffsetParameterId))
    {
        for (int step = 0; step < TapeStopperAudioProcessor::numSequencerSteps; ++step)
        {
            stepParameters[static_cast<size_t> (step)]
                = state.getParameter (sequencerStepParameterId (step));
            stepValues[static_cast<size_t> (step)]
                = state.getRawParameterValue (sequencerStepParameterId (step));
        }
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("64-step Play sequencer: click or drag squares; right-click controls to step backwards");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto freeClock = getIntegerValue (clockModeValue, 0) != 0;
        const auto resolution = juce::jlimit
            (0, TapeStopperAudioProcessor::numSequencerResolutions - 1,
             getIntegerValue (resolutionValue, 6));
        const auto freeRate = juce::jlimit (25, 2000, getIntegerValue (freeRateValue, 125));
        const auto length = juce::jlimit
            (1, TapeStopperAudioProcessor::numSequencerSteps,
             getIntegerValue (lengthValue, TapeStopperAudioProcessor::numSequencerSteps));
        const auto offset = juce::jlimit
            (0, TapeStopperAudioProcessor::numSequencerSteps - 1,
             getIntegerValue (offsetValue, 0));

        drawControl (g, 0, freeClock ? "FREE" : "SYNC", freeClock);
        drawControl (g, 1,
                     freeClock ? "RATE " + juce::String (freeRate) + "ms"
                               : "RATE " + TapeStopperAudioProcessor::sequencerResolutionName
                                                (resolution),
                     false);
        drawControl (g, 2, "LEN " + juce::String (length), false);
        drawControl (g, 3, "OFF " + juce::String (offset), false);
        drawControl (g, 4, "RESET", false);

        g.setColour (juce::Colour (0xff353839));
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText ("64 STEP PLAY SEQUENCER", getInfoBounds().toNearestInt(),
                    juce::Justification::centredRight, false);

        const auto activeStep = processor.getCurrentSequencerStep();
        for (int step = 0; step < TapeStopperAudioProcessor::numSequencerSteps; ++step)
        {
            const auto bounds = getStepBounds (step);
            const auto on = stepValues[static_cast<size_t> (step)] != nullptr
                            && stepValues[static_cast<size_t> (step)]->load() >= 0.5f;
            const auto insideLength = step < length;
            auto fill = on ? juce::Colour (0xff168bd4) : juce::Colour (0xff555958);
            if (! insideLength)
                fill = fill.withAlpha (0.30f);
            else if (step / 4 % 2 != 0)
                fill = fill.brighter (0.06f);

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 1.4f);
            g.setColour (step == activeStep ? juce::Colour (0xffffff8a)
                                            : juce::Colour (0xff252829));
            g.drawRoundedRectangle (bounds, 1.4f, step == activeStep ? 2.0f : 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.position);
        if (step >= 0)
        {
            const auto currentlyOn = stepValues[static_cast<size_t> (step)] != nullptr
                                     && stepValues[static_cast<size_t> (step)]->load() >= 0.5f;
            paintStepsOn = ! currentlyOn;
            lastPaintedStep = -1;
            setStep (step, paintStepsOn);
            return;
        }

        const auto control = controlAt (event.position);
        if (control < 0)
            return;
        const auto direction = event.mods.isRightButtonDown() ? -1 : 1;
        adjustControl (control, direction);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.position);
        if (step >= 0 && step != lastPaintedStep)
            setStep (step, paintStepsOn);
    }

    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override
    {
        const auto control = controlAt (event.position);
        if (control >= 0 && control < 4 && wheel.deltaY != 0.0f)
            adjustControl (control, wheel.deltaY > 0.0f ? 1 : -1);
    }

private:
    void timerCallback() override { repaint(); }

    juce::Rectangle<float> designRect (float x, float y, float width,
                                       float height) const
    {
        return { x * static_cast<float> (getWidth()) / designWidth,
                 y * static_cast<float> (getHeight()) / designHeight,
                 width * static_cast<float> (getWidth()) / designWidth,
                 height * static_cast<float> (getHeight()) / designHeight };
    }

    juce::Rectangle<float> getControlBounds (int control) const
    {
        constexpr auto buttonWidth = 72.0f;
        constexpr auto buttonGap = 8.0f;
        const auto safeControl = juce::jlimit (0, 4, control);
        const auto left = static_cast<float> (safeControl)
                          * (buttonWidth + buttonGap);
        return designRect (left, 0.0f, buttonWidth, 19.0f);
    }

    juce::Rectangle<float> getInfoBounds() const
    {
        return designRect (408.0f, 0.0f, designWidth - 408.0f, 19.0f);
    }

    juce::Rectangle<float> getStepBounds (int step) const
    {
        const auto row = step / 32;
        const auto column = step % 32;
        const auto columnWidth = designWidth / 32.0f;
        const auto cell = designRect (column * columnWidth, 22.0f + row * 18.0f,
                                      columnWidth, 17.0f);
        const auto side = juce::jmin (cell.getWidth() - 2.0f, cell.getHeight() - 2.0f);
        return juce::Rectangle<float> (side, side).withCentre (cell.getCentre());
    }

    int stepAt (juce::Point<float> position) const
    {
        for (int step = 0; step < TapeStopperAudioProcessor::numSequencerSteps; ++step)
            if (getStepBounds (step).expanded (1.0f).contains (position))
                return step;
        return -1;
    }

    int controlAt (juce::Point<float> position) const
    {
        for (int control = 0; control < 5; ++control)
            if (getControlBounds (control).contains (position))
                return control;
        return -1;
    }

    void drawControl (juce::Graphics& g, int control, const juce::String& text,
                      bool highlighted) const
    {
        const auto bounds = getControlBounds (control);
        juce::ColourGradient fill
            (highlighted ? juce::Colour (0xffc7e6f8) : juce::Colour (0xffdadbd8),
             bounds.getX(), bounds.getY(),
             highlighted ? juce::Colour (0xff6f9cb9) : juce::Colour (0xff8e9190),
             bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (highlighted ? juce::Colour (0xff168bd4)
                                 : juce::Colour (0xff4b4e50));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff202324));
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawFittedText (text, bounds.toNearestInt().reduced (2, 0),
                          juce::Justification::centred, 1, 0.70f);
    }

    static int getIntegerValue (const std::atomic<float>* value, int fallback)
    {
        return value != nullptr ? juce::roundToInt (value->load()) : fallback;
    }

    static void setActualValue (juce::RangedAudioParameter* parameter, float value)
    {
        if (parameter == nullptr)
            return;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }

    void adjustControl (int control, int direction)
    {
        if (control == 0)
        {
            setActualValue (clockModeParameter,
                            getIntegerValue (clockModeValue, 0) == 0 ? 1.0f : 0.0f);
        }
        else if (control == 1)
        {
            if (getIntegerValue (clockModeValue, 0) == 0)
            {
                const auto current = getIntegerValue (resolutionValue, 6);
                const auto count = TapeStopperAudioProcessor::numSequencerResolutions;
                setActualValue (resolutionParameter,
                                static_cast<float> (juce::jlimit
                                    (0, count - 1, current + direction)));
            }
            else
            {
                static constexpr std::array<int, 14> rates
                    { 25, 50, 75, 100, 125, 150, 200, 250, 333, 500,
                      750, 1000, 1500, 2000 };
                const auto current = getIntegerValue (freeRateValue, 125);
                auto index = 0;
                auto closest = std::abs (rates[0] - current);
                for (int candidate = 1; candidate < static_cast<int> (rates.size());
                     ++candidate)
                {
                    const auto distance = std::abs
                        (rates[static_cast<size_t> (candidate)] - current);
                    if (distance < closest)
                    {
                        closest = distance;
                        index = candidate;
                    }
                }
                index = juce::jlimit (0, static_cast<int> (rates.size()) - 1,
                                      index + direction);
                setActualValue (freeRateParameter,
                                static_cast<float> (rates[static_cast<size_t> (index)]));
            }
        }
        else if (control == 2)
        {
            const auto current = getIntegerValue
                (lengthValue, TapeStopperAudioProcessor::numSequencerSteps);
            const auto next = juce::jlimit
                (1, TapeStopperAudioProcessor::numSequencerSteps,
                 current + direction);
            setActualValue (lengthParameter, static_cast<float> (next));
        }
        else if (control == 3)
        {
            const auto current = getIntegerValue (offsetValue, 0);
            setActualValue (offsetParameter,
                            static_cast<float> (juce::jlimit
                                (0, TapeStopperAudioProcessor::numSequencerSteps - 1,
                                 current + direction)));
        }
        else if (control == 4)
        {
            for (auto* parameter : stepParameters)
                setActualValue (parameter, 0.0f);
        }
        repaint();
    }

    void setStep (int step, bool enabled)
    {
        if (step < 0 || step >= TapeStopperAudioProcessor::numSequencerSteps
            || step == lastPaintedStep)
            return;
        setActualValue (stepParameters[static_cast<size_t> (step)], enabled ? 1.0f : 0.0f);
        lastPaintedStep = step;
        repaint();
    }

    TapeStopperAudioProcessor& processor;
    juce::RangedAudioParameter* clockModeParameter = nullptr;
    juce::RangedAudioParameter* resolutionParameter = nullptr;
    juce::RangedAudioParameter* freeRateParameter = nullptr;
    juce::RangedAudioParameter* lengthParameter = nullptr;
    juce::RangedAudioParameter* offsetParameter = nullptr;
    std::atomic<float>* clockModeValue = nullptr;
    std::atomic<float>* resolutionValue = nullptr;
    std::atomic<float>* freeRateValue = nullptr;
    std::atomic<float>* lengthValue = nullptr;
    std::atomic<float>* offsetValue = nullptr;
    std::array<juce::RangedAudioParameter*, TapeStopperAudioProcessor::numSequencerSteps>
        stepParameters {};
    std::array<std::atomic<float>*, TapeStopperAudioProcessor::numSequencerSteps>
        stepValues {};
    bool paintStepsOn = false;
    int lastPaintedStep = -1;
    static constexpr float designWidth = 690.0f;
    static constexpr float designHeight = 58.0f;
};

class SetupPanelComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    SetupPanelComponent (TapeStopperAudioProcessor& owner,
                         juce::LookAndFeel* buttonLookAndFeel,
                         std::function<void()> settingsChanged)
        : processor (owner), onSettingsChanged (std::move (settingsChanged))
    {
        for (auto* button : { &fullSpeedMuteButton, &buttonDisplayButton,
                              &waveformDisplayButton })
        {
            button->setLookAndFeel (buttonLookAndFeel);
            button->setClickingTogglesState (true);
            button->setColour (juce::TextButton::buttonColourId,
                               juce::Colour (0xffc2c4c3));
            button->setColour (juce::TextButton::buttonOnColourId,
                               juce::Colour (0xff4fac61));
            addAndMakeVisible (*button);
        }

        buttonDisplayButton.setColour (juce::TextButton::buttonOnColourId,
                                       juce::Colour (0xff168bd4));
        waveformDisplayButton.setColour (juce::TextButton::buttonOnColourId,
                                         juce::Colour (0xff168bd4));

        fullSpeedMuteButton.setToggleState (processor.isFullSpeedMuteEnabled(),
                                             juce::dontSendNotification);
        buttonDisplayButton.setToggleState (processor.isButtonDisplayReversed(),
                                             juce::dontSendNotification);
        waveformDisplayButton.setToggleState (processor.isWaveformDisplayEnabled(),
                                               juce::dontSendNotification);

        fullSpeedMuteButton.setTooltip
            ("Mute the dry output at full speed for send-effect use");
        buttonDisplayButton.setTooltip
            ("Swap the Play and Stop icons without changing the audio behaviour");
        waveformDisplayButton.setTooltip
            ("Show or hide the persistent tape-speed trace behind the envelope");

        fullSpeedMuteButton.onClick = [this]
        {
            processor.setFullSpeedMuteEnabled (fullSpeedMuteButton.getToggleState());
            updateButtonText();
            if (onSettingsChanged)
                onSettingsChanged();
        };

        buttonDisplayButton.onClick = [this]
        {
            processor.setButtonDisplayReversed (buttonDisplayButton.getToggleState());
            updateButtonText();
            if (onSettingsChanged)
                onSettingsChanged();
        };

        waveformDisplayButton.onClick = [this]
        {
            processor.setWaveformDisplayEnabled
                (waveformDisplayButton.getToggleState());
            updateButtonText();
            if (onSettingsChanged)
                onSettingsChanged();
        };

        updateButtonText();
        startTimerHz (20);
    }

    ~SetupPanelComponent() override
    {
        stopTimer();
        fullSpeedMuteButton.setLookAndFeel (nullptr);
        buttonDisplayButton.setLookAndFeel (nullptr);
        waveformDisplayButton.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto scale = static_cast<float> (getWidth()) / designWidth;
        g.addTransform (juce::AffineTransform::scale (scale));

        g.setColour (juce::Colour (0xff343637));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("AUDIO", 10, 3, 170, 20, juce::Justification::centredLeft);
        g.drawText ("DISPLAY", 10, 65, 170, 20, juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText ("Mutes normal full-speed output; transitions remain audible.",
                          190, 24, 164, 38, juce::Justification::centredLeft, 2);
        g.drawFittedText ("Swaps the Play and Stop icons only.",
                          190, 85, 164, 27, juce::Justification::centredLeft, 2);
        g.drawFittedText ("Shows the tape-speed transition trace.",
                          190, 117, 164, 27, juce::Justification::centredLeft, 2);
    }

    void resized() override
    {
        const auto scale = static_cast<float> (getWidth()) / designWidth;
        const auto scaled = [scale] (int x, int y, int width, int height)
        {
            return juce::Rectangle<int> (juce::roundToInt (x * scale),
                                         juce::roundToInt (y * scale),
                                         juce::roundToInt (width * scale),
                                         juce::roundToInt (height * scale));
        };

        fullSpeedMuteButton.setBounds (scaled (10, 27, 170, 28));
        buttonDisplayButton.setBounds (scaled (10, 85, 170, 28));
        waveformDisplayButton.setBounds (scaled (10, 117, 170, 28));
    }

private:
    void timerCallback() override
    {
        const auto enabled = processor.isFullSpeedMuteEnabled();
        if (fullSpeedMuteButton.getToggleState() != enabled)
        {
            fullSpeedMuteButton.setToggleState (enabled, juce::dontSendNotification);
            updateButtonText();
        }
    }

    void updateButtonText()
    {
        fullSpeedMuteButton.setButtonText
            (fullSpeedMuteButton.getToggleState() ? "FULL SPEED MUTE: ON"
                                                   : "FULL SPEED MUTE: OFF");
        buttonDisplayButton.setButtonText
            (buttonDisplayButton.getToggleState() ? "BUTTON: REVERSED"
                                                   : "BUTTON: NORMAL");
        waveformDisplayButton.setButtonText
            (waveformDisplayButton.getToggleState() ? "WAVEFORM: ON"
                                                     : "WAVEFORM: OFF");
    }

    TapeStopperAudioProcessor& processor;
    std::function<void()> onSettingsChanged;
    juce::TextButton fullSpeedMuteButton;
    juce::TextButton buttonDisplayButton;
    juce::TextButton waveformDisplayButton;
    static constexpr int designWidth = 364;
};
}

TapeStopperAudioProcessorEditor::TapeStopperAudioProcessorEditor (TapeStopperAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto& state = processor.getValueTreeState();

    setOpaque (true);

    smallButtonLookAndFeel = std::make_unique<TapeStopButtonLookAndFeel>();
    muteAtLookAndFeel = std::make_unique<TapeStopKnobLookAndFeel>();
    setupButton.setLookAndFeel (smallButtonLookAndFeel.get());
    upButton.setLookAndFeel (smallButtonLookAndFeel.get());
    downButton.setLookAndFeel (smallButtonLookAndFeel.get());
    triggerModeButton.setLookAndFeel (smallButtonLookAndFeel.get());
    timingModeButton.setLookAndFeel (smallButtonLookAndFeel.get());
    retriggerButton.setLookAndFeel (smallButtonLookAndFeel.get());
    envelopeResetButton.setLookAndFeel (smallButtonLookAndFeel.get());
    sequencerViewButton.setLookAndFeel (smallButtonLookAndFeel.get());

    setupButton.setButtonText (juce::String());
    setupButton.getProperties().set (settingsCogProperty, true);
    setupButton.setClickingTogglesState (true);
    setupButton.setTooltip ("Show or hide the portable Setup options");
    setupButton.onClick = [this]
    {
        showSetupPanel (setupButton.getToggleState());
    };
    addAndMakeVisible (setupButton);

    upButton.setClickingTogglesState (true);
    upButton.getProperties().set (directionButtonProperty, true);
    addAndMakeVisible (upButton);

    downButton.setClickingTogglesState (true);
    downButton.getProperties().set (directionButtonProperty, true);
    addAndMakeVisible (downButton);

    triggerModeButton.setColour (juce::TextButton::buttonColourId,
                                 juce::Colour (0xffc2c4c3));
    triggerModeButton.setColour (juce::TextButton::buttonOnColourId,
                                 juce::Colour (0xffc2c4c3));
    timingModeButton.setColour (juce::TextButton::buttonColourId,
                                juce::Colour (0xffc2c4c3));
    timingModeButton.setColour (juce::TextButton::buttonOnColourId,
                                juce::Colour (0xffc2c4c3));
    envelopeResetButton.setColour (juce::TextButton::buttonColourId,
                                   juce::Colour (0xffc2c4c3));
    envelopeResetButton.setColour (juce::TextButton::buttonOnColourId,
                                   juce::Colour (0xffc2c4c3));
    sequencerViewButton.setColour (juce::TextButton::buttonColourId,
                                   juce::Colour (0xffc2c4c3));
    sequencerViewButton.setColour (juce::TextButton::buttonOnColourId,
                                   juce::Colour (0xff168bd4));

    upAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                    (state, upEnabledParameterId, upButton);
    downAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                      (state, downEnabledParameterId, downButton);

    triggerModeButton.onClick = [this]
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (triggerModeParameterId))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getValue() < 0.5f ? 1.0f : 0.0f);
            parameter->endChangeGesture();
            updateTriggerModeText();
        }
    };
    addAndMakeVisible (triggerModeButton);

    timingModeButton.setTooltip
        ("Switch between continuous seconds and host-tempo divisions");
    timingModeButton.onClick = [this]
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (timingModeParameterId))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getValue() < 0.5f ? 1.0f : 0.0f);
            parameter->endChangeGesture();
            updateTimingModeText();
        }
    };
    addAndMakeVisible (timingModeButton);

    retriggerButton.setTooltip
        ("Jump instantly to stopped speed, then run the current UP transition");
    retriggerButton.onStateChange = [this]
    {
        const auto pressed = retriggerButton.isDown();
        auto* retrigger = processor.getValueTreeState().getParameter
                          (retriggerParameterId);

        if (pressed && ! retriggerGestureActive)
        {
            if (auto* engage = processor.getValueTreeState().getParameter
                               (engageParameterId);
                engage != nullptr && engage->getValue() >= 0.5f)
            {
                engage->beginChangeGesture();
                engage->setValueNotifyingHost (0.0f);
                engage->endChangeGesture();
            }

            if (retrigger != nullptr)
            {
                retrigger->beginChangeGesture();
                retrigger->setValueNotifyingHost (1.0f);
            }

            retriggerGestureActive = true;
            processor.requestRetrigger();
        }
        else if (! pressed && retriggerGestureActive)
        {
            if (retrigger != nullptr)
            {
                retrigger->setValueNotifyingHost (0.0f);
                retrigger->endChangeGesture();
            }

            retriggerGestureActive = false;
        }
        updateRetriggerButtonColour();
    };
    addAndMakeVisible (retriggerButton);

    envelopeResetButton.setTooltip
        ("Reset both curves or all envelope points in the selected graph view");
    envelopeResetButton.onClick = [this]
    {
        if (auto* display = static_cast<EnvelopeEditorComponent*> (envelopeEditor.get()))
            display->resetCurrentView();
    };
    addAndMakeVisible (envelopeResetButton);

    sequencerViewButton.setClickingTogglesState (true);
    sequencerViewButton.getProperties().set (largerButtonTextProperty, true);
    sequencerViewButton.setTooltip
        ("Switch the bottom panel between tape controls and the 64-step sequencer");
    sequencerViewButton.onClick = [this]
    {
        showSequencerPanel (sequencerViewButton.getToggleState());
    };
    addAndMakeVisible (sequencerViewButton);

    mainTrigger = std::make_unique<MainTriggerComponent> (processor);
    timingBar = std::make_unique<TimingBarComponent> (processor, state);
    envelopeEditor = std::make_unique<EnvelopeEditorComponent> (processor, state);
    presetSection = std::make_unique<PresetSectionComponent> (processor);
    setupPanel = std::make_unique<SetupPanelComponent>
                 (processor, smallButtonLookAndFeel.get(), [this]
                  {
                      savePortableSettings();
                  });
    sequencerEnableLed = std::make_unique<SequencerEnableLedComponent> (state);
    sequencerPanel = std::make_unique<SequencerPanelComponent> (processor, state);
    addAndMakeVisible (*mainTrigger);
    addAndMakeVisible (*timingBar);
    addAndMakeVisible (*envelopeEditor);
    addAndMakeVisible (*presetSection);
    addAndMakeVisible (*setupPanel);
    addAndMakeVisible (*sequencerEnableLed);
    addAndMakeVisible (*sequencerPanel);
    setupPanel->setVisible (false);
    sequencerPanel->setVisible (false);

    muteAtSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    muteAtSlider.setLookAndFeel (muteAtLookAndFeel.get());
    muteAtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    muteAtSlider.setTooltip ("Mutes when the down/up marker reaches this position");
    addAndMakeVisible (muteAtSlider);
    muteAtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (state, muteAtParameterId, muteAtSlider);

    muteAtValueLabel.setColour (juce::Label::textColourId, juce::Colour (0xff343637));
    muteAtValueLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    muteAtValueLabel.setJustificationType (juce::Justification::centred);
    muteAtValueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (muteAtValueLabel);

    for (auto* slider : { &filterAmountSlider, &volumeAmountSlider,
                          &driveSlider, &wowSlider, &flutterSlider,
                          &fluxSlider, &mixSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setLookAndFeel (muteAtLookAndFeel.get());
        slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (*slider);
    }

    filterAmountSlider.setTooltip ("Maximum low-pass filtering reached by the curve");
    volumeAmountSlider.setTooltip ("Maximum volume reduction reached by the curve");
    driveSlider.setTooltip ("Tape-style saturation amount");
    wowSlider.setTooltip ("Slow tape-speed variation at 0.33 Hz");
    flutterSlider.setTooltip ("Fast tape-speed variation at 6.5 Hz");
    fluxSlider.setTooltip ("Irregular pitch and playback instability during DOWN and UP");
    mixSlider.setTooltip ("Dry and processed signal balance");

    filterAmountAttachment
        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
          (state, filterAmountParameterId, filterAmountSlider);
    volumeAmountAttachment
        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
          (state, volumeAmountParameterId, volumeAmountSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                      (state, driveParameterId, driveSlider);
    wowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (state, wowParameterId, wowSlider);
    flutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                        (state, flutterParameterId, flutterSlider);
    fluxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                     (state, fluxParameterId, fluxSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (state, mixParameterId, mixSlider);

    for (auto* label : { &filterAmountValueLabel, &volumeAmountValueLabel,
                         &driveValueLabel, &wowValueLabel,
                         &flutterValueLabel, &fluxValueLabel, &mixValueLabel })
    {
        label->setColour (juce::Label::textColourId, juce::Colour (0xff343637));
        label->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centred);
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*label);
    }

    updateTriggerModeText();
    updateTimingModeText();
    if (auto* display = static_cast<EnvelopeEditorComponent*> (envelopeEditor.get()))
        envelopeResetButton.setButtonText (display->getResetButtonText());
    updateMuteAtText();
    updateBottomControlText();
    updateMotionButtonColours();
    updateRetriggerButtonColour();
    showSequencerPanel (false);
    startTimerHz (30);

    // JUCE may call resized() while resizability and constraints are being set.
    // Do this only after all child components have been created.
    setResizable (true, true);
    setResizeLimits (600, 225, 1600, 600);

    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);

    const auto portableSettings = TapeStopperPortableSettings::load();
    setSize (juce::roundToInt (designWidth * portableSettings.guiScalePercent / 100.0f),
             juce::roundToInt (designHeight * portableSettings.guiScalePercent / 100.0f));
    savePortableSettings();
    observedEditorWidth = getWidth();
    savedEditorWidth = getWidth();
}

TapeStopperAudioProcessorEditor::~TapeStopperAudioProcessorEditor()
{
    if (retriggerGestureActive)
    {
        if (auto* retrigger = processor.getValueTreeState().getParameter
                              (retriggerParameterId))
        {
            retrigger->setValueNotifyingHost (0.0f);
            retrigger->endChangeGesture();
        }
        retriggerGestureActive = false;
    }

    savePortableSettings();
    stopTimer();
    setupButton.setLookAndFeel (nullptr);
    upButton.setLookAndFeel (nullptr);
    downButton.setLookAndFeel (nullptr);
    triggerModeButton.setLookAndFeel (nullptr);
    timingModeButton.setLookAndFeel (nullptr);
    retriggerButton.setLookAndFeel (nullptr);
    envelopeResetButton.setLookAndFeel (nullptr);
    sequencerViewButton.setLookAndFeel (nullptr);
    muteAtSlider.setLookAndFeel (nullptr);
    filterAmountSlider.setLookAndFeel (nullptr);
    volumeAmountSlider.setLookAndFeel (nullptr);
    driveSlider.setLookAndFeel (nullptr);
    wowSlider.setLookAndFeel (nullptr);
    flutterSlider.setLookAndFeel (nullptr);
    fluxSlider.setLookAndFeel (nullptr);
    mixSlider.setLookAndFeel (nullptr);
}

void TapeStopperAudioProcessorEditor::timerCallback()
{
    const auto currentWidth = getWidth();

    if (currentWidth != observedEditorWidth)
    {
        observedEditorWidth = currentWidth;
        stableResizeTicks = 0;
    }
    else if (currentWidth != savedEditorWidth && ++stableResizeTicks >= 15)
    {
        savePortableSettings();
        savedEditorWidth = currentWidth;
        stableResizeTicks = 0;
    }

    updateTriggerModeText();
    updateTimingModeText();
    if (auto* display = static_cast<EnvelopeEditorComponent*> (envelopeEditor.get()))
        envelopeResetButton.setButtonText (display->getResetButtonText());
    updateMuteAtText();
    updateBottomControlText();
    updateMotionButtonColours();
    updateRetriggerButtonColour();
}

void TapeStopperAudioProcessorEditor::savePortableSettings()
{
    auto settings = TapeStopperPortableSettings::load();
    settings.guiScalePercent = juce::roundToInt
                               (100.0f * static_cast<float> (getWidth()) / designWidth);
    settings.fullSpeedMute = processor.isFullSpeedMuteEnabled();
    settings.reversedButtonDisplay = processor.isButtonDisplayReversed();
    settings.waveformDisplay = processor.isWaveformDisplayEnabled();
    settings.save();
}

void TapeStopperAudioProcessorEditor::showSetupPanel (bool shouldShow)
{
    showingSetup = shouldShow;

    if (timingBar != nullptr)
        timingBar->setVisible (! showingSetup);
    if (envelopeEditor != nullptr)
        envelopeEditor->setVisible (! showingSetup);
    if (setupPanel != nullptr)
        setupPanel->setVisible (showingSetup);

    repaint();
}

void TapeStopperAudioProcessorEditor::showSequencerPanel (bool shouldShow)
{
    showingSequencer = shouldShow;
    sequencerViewButton.setToggleState (showingSequencer,
                                        juce::dontSendNotification);

    for (auto* component : { static_cast<juce::Component*> (&filterAmountSlider),
                             static_cast<juce::Component*> (&volumeAmountSlider),
                             static_cast<juce::Component*> (&driveSlider),
                             static_cast<juce::Component*> (&wowSlider),
                             static_cast<juce::Component*> (&flutterSlider),
                             static_cast<juce::Component*> (&fluxSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&filterAmountValueLabel),
                             static_cast<juce::Component*> (&volumeAmountValueLabel),
                             static_cast<juce::Component*> (&driveValueLabel),
                             static_cast<juce::Component*> (&wowValueLabel),
                             static_cast<juce::Component*> (&flutterValueLabel),
                             static_cast<juce::Component*> (&fluxValueLabel),
                             static_cast<juce::Component*> (&mixValueLabel) })
        component->setVisible (! showingSequencer);

    if (sequencerPanel != nullptr)
        sequencerPanel->setVisible (showingSequencer);
    repaint();
}

void TapeStopperAudioProcessorEditor::updateTimingModeText()
{
    const auto* mode = processor.getValueTreeState().getRawParameterValue (timingModeParameterId);
    timingModeButton.setButtonText (mode != nullptr && mode->load() >= 0.5f
                                        ? "SYNC"
                                        : "FREE");
}

void TapeStopperAudioProcessorEditor::updateMuteAtText()
{
    const auto* value = processor.getValueTreeState().getRawParameterValue
                        (muteAtParameterId);
    const auto percentage = value != nullptr ? juce::roundToInt (value->load()) : 90;
    muteAtValueLabel.setText ("MUTE AT: " + juce::String (percentage) + "%",
                              juce::dontSendNotification);
}

void TapeStopperAudioProcessorEditor::updateBottomControlText()
{
    auto& state = processor.getValueTreeState();

    const auto percentageText = [&state] (const char* parameterId,
                                          const juce::String& name)
    {
        const auto* value = state.getRawParameterValue (parameterId);
        const auto percentage = value != nullptr ? juce::roundToInt (value->load()) : 0;
        return name + ": " + juce::String (percentage) + "%";
    };

    filterAmountValueLabel.setText
        (percentageText (filterAmountParameterId, "FLTR AMT"),
         juce::dontSendNotification);
    volumeAmountValueLabel.setText
        (percentageText (volumeAmountParameterId, "VOL AMT"),
         juce::dontSendNotification);
    driveValueLabel.setText (percentageText (driveParameterId, "DRIVE"),
                             juce::dontSendNotification);
    wowValueLabel.setText (percentageText (wowParameterId, "WOW"),
                           juce::dontSendNotification);
    flutterValueLabel.setText (percentageText (flutterParameterId, "FLUTTER"),
                               juce::dontSendNotification);
    fluxValueLabel.setText (percentageText (fluxParameterId, "FLUX"),
                            juce::dontSendNotification);
    mixValueLabel.setText (percentageText (mixParameterId, "MIX"),
                           juce::dontSendNotification);
}

void TapeStopperAudioProcessorEditor::updateTriggerModeText()
{
    const auto* mode = processor.getValueTreeState().getRawParameterValue (triggerModeParameterId);
    triggerModeButton.setButtonText (mode != nullptr && mode->load() >= 0.5f
                                         ? "PLAY: T"
                                         : "PLAY: M");
}

void TapeStopperAudioProcessorEditor::updateMotionButtonColours()
{
    const auto motion = processor.getMotionDirection();
    const auto* upEnabledValue = processor.getValueTreeState().getRawParameterValue (upEnabledParameterId);
    const auto* downEnabledValue = processor.getValueTreeState().getRawParameterValue (downEnabledParameterId);
    const auto upEnabled = upEnabledValue != nullptr && upEnabledValue->load() >= 0.5f;
    const auto downEnabled = downEnabledValue != nullptr && downEnabledValue->load() >= 0.5f;
    const auto inactive = juce::Colour (0xffc2c4c3);
    const auto up = upEnabled && motion == TapeStopperAudioProcessor::MotionDirection::up
                        ? juce::Colour (0xffd05b52)
                        : inactive;
    const auto down = downEnabled && motion == TapeStopperAudioProcessor::MotionDirection::down
                          ? juce::Colour (0xff4fac61)
                          : inactive;

    upButton.setColour (juce::TextButton::buttonColourId, up);
    upButton.setColour (juce::TextButton::buttonOnColourId, up);
    downButton.setColour (juce::TextButton::buttonColourId, down);
    downButton.setColour (juce::TextButton::buttonOnColourId, down);
}

void TapeStopperAudioProcessorEditor::updateRetriggerButtonColour()
{
    const auto colour = processor.isRetriggerActive()
                            ? juce::Colour (0xff168bd4)
                            : juce::Colour (0xffc2c4c3);
    retriggerButton.setColour (juce::TextButton::buttonColourId, colour);
    retriggerButton.setColour (juce::TextButton::buttonOnColourId, colour);
}

void TapeStopperAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto scale = static_cast<float> (getWidth()) / designWidth;
    g.addTransform (juce::AffineTransform::scale (scale));

    juce::ColourGradient background (juce::Colour (0xffe3e4e2), 0.0f, 0.0f,
                                     juce::Colour (0xff777b7e), 0.0f,
                                     static_cast<float> (designHeight), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colour (0xff313538));
    g.fillRect (0, 0, designWidth, 23);
    g.setColour (juce::Colour (0xffd7d9d9));
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("TAPESTOPPER", 14, 0, 180, 23, juce::Justification::centredLeft);

    drawPanel (g, { 10.0f, 32.0f, 242.0f, 178.0f });
    drawPanel (g, { 262.0f, 32.0f, 388.0f, 178.0f });
    drawPanel (g, { 660.0f, 32.0f, 130.0f, 178.0f });

    drawPanel (g, { 10.0f, 220.0f, 780.0f, 68.0f });
}

void TapeStopperAudioProcessorEditor::resized()
{
    const auto scale = static_cast<float> (getWidth()) / designWidth;
    const auto scaled = [scale] (int x, int y, int width, int height)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * scale),
                                     juce::roundToInt (y * scale),
                                     juce::roundToInt (width * scale),
                                     juce::roundToInt (height * scale));
    };

    setupButton.setBounds (scaled (770, 2, 27, 19));
    if (mainTrigger != nullptr)
        mainTrigger->setBounds (scaled (20, 45, 145, 110));
    if (presetSection != nullptr)
        presetSection->setBounds (scaled (20, 164, 145, 41));
    upButton.setBounds (scaled (180, 43, 60, 27));
    downButton.setBounds (scaled (180, 75, 60, 27));
    triggerModeButton.setBounds (scaled (180, 107, 60, 27));
    timingModeButton.setBounds (scaled (180, 139, 60, 27));
    retriggerButton.setBounds (scaled (180, 171, 60, 27));
    if (timingBar != nullptr)
        timingBar->setBounds (scaled (274, 42, 364, 46));
    envelopeResetButton.setBounds (scaled (672, 43, 106, 27));
    if (envelopeEditor != nullptr)
        envelopeEditor->setBounds (scaled (274, 87, 364, 120));
    if (setupPanel != nullptr)
        setupPanel->setBounds (scaled (274, 40, 364, 150));
    muteAtSlider.setBounds (scaled (682, 92, 86, 76));
    muteAtValueLabel.setFont (juce::FontOptions (12.0f * scale, juce::Font::bold));
    muteAtValueLabel.setBounds (scaled (670, 184, 110, 18));

    sequencerViewButton.setBounds (scaled (20, 241, 52, 27));
    if (sequencerEnableLed != nullptr)
        sequencerEnableLed->setBounds (scaled (80, 246, 17, 17));
    if (sequencerPanel != nullptr)
        sequencerPanel->setBounds (scaled (98, 225, 682, 58));

    filterAmountSlider.setBounds (scaled (138, 223, 62, 43));
    volumeAmountSlider.setBounds (scaled (235, 223, 62, 43));
    driveSlider.setBounds (scaled (332, 223, 62, 43));
    wowSlider.setBounds (scaled (429, 223, 62, 43));
    flutterSlider.setBounds (scaled (526, 223, 62, 43));
    fluxSlider.setBounds (scaled (623, 223, 62, 43));
    mixSlider.setBounds (scaled (720, 223, 62, 43));

    for (auto* label : { &filterAmountValueLabel, &volumeAmountValueLabel,
                         &driveValueLabel, &wowValueLabel,
                         &flutterValueLabel, &fluxValueLabel, &mixValueLabel })
        label->setFont (juce::FontOptions (11.0f * scale, juce::Font::bold));

    filterAmountValueLabel.setBounds (scaled (122, 266, 94, 16));
    volumeAmountValueLabel.setBounds (scaled (219, 266, 94, 16));
    driveValueLabel.setBounds (scaled (316, 266, 94, 16));
    wowValueLabel.setBounds (scaled (413, 266, 94, 16));
    flutterValueLabel.setBounds (scaled (510, 266, 94, 16));
    fluxValueLabel.setBounds (scaled (607, 266, 94, 16));
    mixValueLabel.setBounds (scaled (712, 266, 78, 16));
}
