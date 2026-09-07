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
constexpr auto mixParameterId = "mix";
constexpr auto currentPresetNameProperty = "currentPresetName";
const juce::Identifier directionButtonProperty { "isDirectionButton" };
const juce::Identifier settingsCogProperty { "isSettingsCog" };

juce::String envelopeXParameterId (int pointIndex)
{
    return "envX" + juce::String (pointIndex);
}

juce::String envelopeYParameterId (int pointIndex)
{
    return "envY" + juce::String (pointIndex);
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
            const auto outerRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.31f;
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

            auto cogColour = juce::Colours::white;
            if (isButtonDown)
                cogColour = cogColour.withAlpha (0.68f);
            else if (isMouseOverButton)
                cogColour = cogColour.brighter (0.10f);

            g.setColour (cogColour);
            g.fillPath (cog);
            return;
        }

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
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
        downCurveParameterId,
        upCurveParameterId,
        driveParameterId,
        wowParameterId,
        flutterParameterId,
        mixParameterId,
        envelopeEnabledParameterId
    };

    for (int point = 1; point < TapeStopperAudioProcessor::numEnvelopePoints - 1; ++point)
        ids.push_back (envelopeXParameterId (point));

    for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
        ids.push_back (envelopeYParameterId (point));

    return ids;
}

const auto presetParameterIds = makePresetParameterIds();

bool isStageSixPresetParameter (const juce::String& parameterId)
{
    return parameterId == downCurveParameterId
           || parameterId == upCurveParameterId
           || parameterId == driveParameterId
           || parameterId == wowParameterId
           || parameterId == flutterParameterId
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
        auto background = juce::Colour (0xffdadad5);

        if (isMouseOverButton)
            background = background.brighter (0.10f);
        if (isButtonDown)
            background = background.darker (0.12f);

        g.setColour (background);
        g.fillRoundedRectangle (bounds, 1.5f);
        g.setColour (juce::Colour (0xff686864));
        g.drawRoundedRectangle (bounds, 1.5f, 1.0f);
        g.setColour (juce::Colour (0xff242424));
        g.setFont (juce::FontOptions (getHeight() * 0.44f, juce::Font::bold));
        g.drawFittedText (getButtonText(), getLocalBounds().reduced (1, 0),
                          juce::Justification::centred, 1, 0.70f);
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
        g.setColour (juce::Colour (0xffedede8));
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
                if (isStageSixPresetParameter (parameterId))
                {
                    pendingValues.emplace_back (parameter, parameter->getDefaultValue());
                    continue;
                }

                showPresetError ("The preset is incomplete or incompatible:\n" + sourceName);
                return false;
            }

            const auto actualValue = values[parameterId].getDoubleValue();
            if (! std::isfinite (actualValue))
            {
                showPresetError ("The preset contains an invalid value:\n" + sourceName);
                return false;
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
                 << "FormatVersion=2\r\n";

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
        const auto engaged = engageParameter != nullptr && engageParameter->getValue() >= 0.5f;
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
        if (! event.mods.isLeftButtonDown() || engageParameter == nullptr)
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
    explicit EnvelopeEditorComponent (juce::AudioProcessorValueTreeState& state)
        : enabledValue (state.getRawParameterValue (envelopeEnabledParameterId))
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

        for (int division = 1; division < TapeStopperAudioProcessor::numEnvelopePoints - 1;
             ++division)
        {
            const auto x = graph.getX() + graph.getWidth()
                           * static_cast<float> (division)
                           / static_cast<float> (TapeStopperAudioProcessor::numEnvelopePoints - 1);
            g.drawVerticalLine (juce::roundToInt (x), graph.getY(), graph.getBottom());
        }

        g.setColour (juce::Colour (0xff34764e));
        g.drawLine (graph.getX(), graph.getCentreY(), graph.getRight(),
                    graph.getCentreY(), 1.4f);

        std::array<juce::Point<float>, TapeStopperAudioProcessor::numEnvelopePoints> points;
        for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
            points[static_cast<size_t> (point)] = getPointPosition (point, graph);

        juce::Path curve;
        curve.startNewSubPath (points.front());
        for (int point = 1; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
            curve.lineTo (points[static_cast<size_t> (point)]);

        const auto isEnabled = enabledValue != nullptr && enabledValue->load() >= 0.5f;
        g.setColour (juce::Colour (0xff49ff70).withAlpha (isEnabled ? 1.0f : 0.48f));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
        {
            const auto size = point == activePoint ? 8.0f : 6.0f;
            const auto handle = juce::Rectangle<float> (size, size)
                                    .withCentre (points[static_cast<size_t> (point)]);
            g.setColour (point == activePoint ? juce::Colour (0xffffff72)
                                              : juce::Colour (0xffb9ffc8));
            g.fillRect (handle);
            g.setColour (juce::Colour (0xff0a3317));
            g.drawRect (handle, 1.0f);
        }

        if (activePoint >= 0)
        {
            const auto semitones = (0.5f - getPointY (activePoint)) * 24.0f;
            const auto prefix = semitones > 0.0f ? "+" : "";
            g.setColour (juce::Colour (0xffd7ffe0));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ("POINT " + juce::String (activePoint + 1) + "  " + prefix
                            + juce::String (semitones, 1) + " st",
                        graph.toNearestInt().reduced (5, 3),
                        juce::Justification::topRight, false);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isLeftButtonDown() || event.getNumberOfClicks() > 1)
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
        if (activePoint >= 0)
            updateActivePoint (event.position);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endActiveGestures();
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    juce::Rectangle<float> getGraphBounds() const
    {
        return getLocalBounds().toFloat().reduced (5.0f, 4.0f);
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
        if (activePoint < 0)
            return;

        if (auto* xParameter = getXParameter (activePoint))
            xParameter->endChangeGesture();
        if (auto* yParameter = yParameters[static_cast<size_t> (activePoint)])
            yParameter->endChangeGesture();
        activePoint = -1;
        repaint();
    }

    std::atomic<float>* enabledValue = nullptr;
    std::array<juce::RangedAudioParameter*, TapeStopperAudioProcessor::numEnvelopePoints - 2>
        xParameters {};
    std::array<std::atomic<float>*, TapeStopperAudioProcessor::numEnvelopePoints - 2>
        xValues {};
    std::array<juce::RangedAudioParameter*, TapeStopperAudioProcessor::numEnvelopePoints>
        yParameters {};
    std::array<std::atomic<float>*, TapeStopperAudioProcessor::numEnvelopePoints>
        yValues {};
    int activePoint = -1;
};

class SetupPanelComponent final : public juce::Component
{
public:
    SetupPanelComponent (TapeStopperAudioProcessor& owner,
                         juce::LookAndFeel* buttonLookAndFeel,
                         std::function<void()> settingsChanged)
        : processor (owner), onSettingsChanged (std::move (settingsChanged))
    {
        for (auto* button : { &fullSpeedMuteButton, &buttonDisplayButton })
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

        fullSpeedMuteButton.setToggleState (processor.isFullSpeedMuteEnabled(),
                                             juce::dontSendNotification);
        buttonDisplayButton.setToggleState (processor.isButtonDisplayReversed(),
                                             juce::dontSendNotification);

        fullSpeedMuteButton.setTooltip
            ("Mute the dry output at full speed for send-effect use");
        buttonDisplayButton.setTooltip
            ("Swap the Play and Stop icons without changing the audio behaviour");

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

        updateButtonText();
    }

    ~SetupPanelComponent() override
    {
        fullSpeedMuteButton.setLookAndFeel (nullptr);
        buttonDisplayButton.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto scale = static_cast<float> (getWidth()) / designWidth;
        g.addTransform (juce::AffineTransform::scale (scale));

        g.setColour (juce::Colour (0xff343637));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("AUDIO", 10, 3, 170, 20, juce::Justification::centredLeft);
        g.drawText ("DISPLAY", 10, 78, 170, 20, juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText ("Mutes normal full-speed output; transitions remain audible.",
                          190, 24, 164, 38, juce::Justification::centredLeft, 2);
        g.drawFittedText ("Swaps the Play and Stop icons only.",
                          205, 100, 149, 28, juce::Justification::centredLeft, 2);
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
        buttonDisplayButton.setBounds (scaled (10, 102, 185, 28));
    }

private:
    void updateButtonText()
    {
        fullSpeedMuteButton.setButtonText
            (fullSpeedMuteButton.getToggleState() ? "FULL SPEED MUTE: ON"
                                                   : "FULL SPEED MUTE: OFF");
        buttonDisplayButton.setButtonText
            (buttonDisplayButton.getToggleState() ? "BUTTON: REVERSED"
                                                   : "BUTTON: NORMAL");
    }

    TapeStopperAudioProcessor& processor;
    std::function<void()> onSettingsChanged;
    juce::TextButton fullSpeedMuteButton;
    juce::TextButton buttonDisplayButton;
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
    envelopeButton.setLookAndFeel (smallButtonLookAndFeel.get());
    envelopeResetButton.setLookAndFeel (smallButtonLookAndFeel.get());
    downCurveButton.setLookAndFeel (smallButtonLookAndFeel.get());
    upCurveButton.setLookAndFeel (smallButtonLookAndFeel.get());

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
    envelopeButton.setColour (juce::TextButton::buttonColourId,
                              juce::Colour (0xffc2c4c3));
    envelopeButton.setColour (juce::TextButton::buttonOnColourId,
                              juce::Colour (0xff4fac61));
    envelopeResetButton.setColour (juce::TextButton::buttonColourId,
                                   juce::Colour (0xffc2c4c3));
    envelopeResetButton.setColour (juce::TextButton::buttonOnColourId,
                                   juce::Colour (0xffc2c4c3));
    downCurveButton.setColour (juce::TextButton::buttonColourId,
                               juce::Colour (0xffc2c4c3));
    downCurveButton.setColour (juce::TextButton::buttonOnColourId,
                               juce::Colour (0xffc2c4c3));
    upCurveButton.setColour (juce::TextButton::buttonColourId,
                             juce::Colour (0xffc2c4c3));
    upCurveButton.setColour (juce::TextButton::buttonOnColourId,
                             juce::Colour (0xffc2c4c3));

    downCurveButton.setTooltip
        ("Cycle the downward transition through Linear, Gentle, Steep and S-Curve");
    downCurveButton.onClick = [this]
    {
        auto& parameterState = processor.getValueTreeState();
        auto* parameter = parameterState.getParameter (downCurveParameterId);
        const auto* value = parameterState.getRawParameterValue (downCurveParameterId);
        if (parameter != nullptr && value != nullptr)
        {
            const auto next = (juce::roundToInt (value->load()) + 1) % 4;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost
                (parameter->convertTo0to1 (static_cast<float> (next)));
            parameter->endChangeGesture();
            updateBottomControlText();
        }
    };
    addAndMakeVisible (downCurveButton);

    upCurveButton.setTooltip
        ("Cycle the upward transition through Linear, Gentle, Steep and S-Curve");
    upCurveButton.onClick = [this]
    {
        auto& parameterState = processor.getValueTreeState();
        auto* parameter = parameterState.getParameter (upCurveParameterId);
        const auto* value = parameterState.getRawParameterValue (upCurveParameterId);
        if (parameter != nullptr && value != nullptr)
        {
            const auto next = (juce::roundToInt (value->load()) + 1) % 4;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost
                (parameter->convertTo0to1 (static_cast<float> (next)));
            parameter->endChangeGesture();
            updateBottomControlText();
        }
    };
    addAndMakeVisible (upCurveButton);

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

    envelopeButton.setClickingTogglesState (true);
    envelopeButton.setTooltip ("Enable the 11-point pitch envelope during downward motion");
    addAndMakeVisible (envelopeButton);
    envelopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                         (state, envelopeEnabledParameterId, envelopeButton);

    envelopeResetButton.setTooltip
        ("Reset every envelope point to zero semitones without changing its time");
    envelopeResetButton.onClick = [this]
    {
        auto& parameterState = processor.getValueTreeState();
        for (int point = 0; point < TapeStopperAudioProcessor::numEnvelopePoints; ++point)
        {
            if (auto* parameter = parameterState.getParameter
                                  (envelopeYParameterId (point)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.5f));
                parameter->endChangeGesture();
            }
        }
    };
    addAndMakeVisible (envelopeResetButton);

    mainTrigger = std::make_unique<MainTriggerComponent> (processor);
    timingBar = std::make_unique<TimingBarComponent> (processor, state);
    envelopeEditor = std::make_unique<EnvelopeEditorComponent> (state);
    presetSection = std::make_unique<PresetSectionComponent> (processor);
    setupPanel = std::make_unique<SetupPanelComponent>
                 (processor, smallButtonLookAndFeel.get(), [this]
                  {
                      savePortableSettings();
                  });
    addAndMakeVisible (*mainTrigger);
    addAndMakeVisible (*timingBar);
    addAndMakeVisible (*envelopeEditor);
    addAndMakeVisible (*presetSection);
    addAndMakeVisible (*setupPanel);
    setupPanel->setVisible (false);

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

    for (auto* slider : { &driveSlider, &wowSlider, &flutterSlider, &mixSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setLookAndFeel (muteAtLookAndFeel.get());
        slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (*slider);
    }

    driveSlider.setTooltip ("Tape-style saturation amount");
    wowSlider.setTooltip ("Slow tape-speed variation at 0.33 Hz");
    flutterSlider.setTooltip ("Fast tape-speed variation at 6.5 Hz");
    mixSlider.setTooltip ("Dry and processed signal balance");

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                      (state, driveParameterId, driveSlider);
    wowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (state, wowParameterId, wowSlider);
    flutterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                        (state, flutterParameterId, flutterSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (state, mixParameterId, mixSlider);

    for (auto* label : { &driveValueLabel, &wowValueLabel,
                         &flutterValueLabel, &mixValueLabel })
    {
        label->setColour (juce::Label::textColourId, juce::Colour (0xff343637));
        label->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centred);
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*label);
    }

    updateTriggerModeText();
    updateTimingModeText();
    updateEnvelopeButtonText();
    updateMuteAtText();
    updateBottomControlText();
    updateMotionButtonColours();
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
    savePortableSettings();
    stopTimer();
    setupButton.setLookAndFeel (nullptr);
    upButton.setLookAndFeel (nullptr);
    downButton.setLookAndFeel (nullptr);
    triggerModeButton.setLookAndFeel (nullptr);
    timingModeButton.setLookAndFeel (nullptr);
    envelopeButton.setLookAndFeel (nullptr);
    envelopeResetButton.setLookAndFeel (nullptr);
    downCurveButton.setLookAndFeel (nullptr);
    upCurveButton.setLookAndFeel (nullptr);
    muteAtSlider.setLookAndFeel (nullptr);
    driveSlider.setLookAndFeel (nullptr);
    wowSlider.setLookAndFeel (nullptr);
    flutterSlider.setLookAndFeel (nullptr);
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
    updateEnvelopeButtonText();
    updateMuteAtText();
    updateBottomControlText();
    updateMotionButtonColours();
}

void TapeStopperAudioProcessorEditor::savePortableSettings()
{
    auto settings = TapeStopperPortableSettings::load();
    settings.guiScalePercent = juce::roundToInt
                               (100.0f * static_cast<float> (getWidth()) / designWidth);
    settings.fullSpeedMute = processor.isFullSpeedMuteEnabled();
    settings.reversedButtonDisplay = processor.isButtonDisplayReversed();
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

void TapeStopperAudioProcessorEditor::updateTimingModeText()
{
    const auto* mode = processor.getValueTreeState().getRawParameterValue (timingModeParameterId);
    timingModeButton.setButtonText (mode != nullptr && mode->load() >= 0.5f
                                        ? "SYNC"
                                        : "FREE");
}

void TapeStopperAudioProcessorEditor::updateEnvelopeButtonText()
{
    const auto* enabled = processor.getValueTreeState().getRawParameterValue
                          (envelopeEnabledParameterId);
    envelopeButton.setButtonText (enabled != nullptr && enabled->load() >= 0.5f
                                      ? "ENVELOPE: ON"
                                      : "ENVELOPE: OFF");
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
    const juce::StringArray curveNames { "LINEAR", "GENTLE", "STEEP", "S-CURVE" };

    const auto curveText = [&state, &curveNames] (const char* parameterId)
    {
        const auto* value = state.getRawParameterValue (parameterId);
        const auto index = juce::jlimit (0, curveNames.size() - 1,
                                         value != nullptr
                                             ? juce::roundToInt (value->load()) : 0);
        return curveNames[index];
    };

    const auto percentageText = [&state] (const char* parameterId,
                                          const juce::String& name)
    {
        const auto* value = state.getRawParameterValue (parameterId);
        const auto percentage = value != nullptr ? juce::roundToInt (value->load()) : 0;
        return name + ": " + juce::String (percentage) + "%";
    };

    downCurveButton.setButtonText ("DOWN: " + curveText (downCurveParameterId));
    upCurveButton.setButtonText ("UP: " + curveText (upCurveParameterId));
    driveValueLabel.setText (percentageText (driveParameterId, "DRIVE"),
                             juce::dontSendNotification);
    wowValueLabel.setText (percentageText (wowParameterId, "WOW"),
                           juce::dontSendNotification);
    flutterValueLabel.setText (percentageText (flutterParameterId, "FLUTTER"),
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

    g.setColour (juce::Colour (0xff343637));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (showingSetup ? "SETUP"
                             : "ENVELOPE - DOWNWARD PITCH +/- 12 SEMITONES",
                276, 195, 360, 15,
                juce::Justification::centred);

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

    setupButton.setBounds (scaled (758, 2, 27, 19));
    if (mainTrigger != nullptr)
        mainTrigger->setBounds (scaled (20, 45, 145, 110));
    if (presetSection != nullptr)
        presetSection->setBounds (scaled (20, 164, 145, 41));
    upButton.setBounds (scaled (180, 43, 60, 27));
    downButton.setBounds (scaled (180, 75, 60, 27));
    triggerModeButton.setBounds (scaled (180, 107, 60, 27));
    timingModeButton.setBounds (scaled (180, 139, 60, 27));
    if (timingBar != nullptr)
        timingBar->setBounds (scaled (274, 45, 364, 48));
    envelopeButton.setBounds (scaled (672, 43, 106, 27));
    envelopeResetButton.setBounds (scaled (672, 75, 106, 27));
    if (envelopeEditor != nullptr)
        envelopeEditor->setBounds (scaled (274, 94, 364, 103));
    if (setupPanel != nullptr)
        setupPanel->setBounds (scaled (274, 40, 364, 150));
    muteAtSlider.setBounds (scaled (682, 105, 86, 70));
    muteAtValueLabel.setFont (juce::FontOptions (12.0f * scale, juce::Font::bold));
    muteAtValueLabel.setBounds (scaled (670, 184, 110, 18));

    downCurveButton.setBounds (scaled (20, 240, 128, 29));
    upCurveButton.setBounds (scaled (156, 240, 128, 29));

    driveSlider.setBounds (scaled (311, 223, 70, 43));
    wowSlider.setBounds (scaled (435, 223, 70, 43));
    flutterSlider.setBounds (scaled (559, 223, 70, 43));
    mixSlider.setBounds (scaled (683, 223, 70, 43));

    for (auto* label : { &driveValueLabel, &wowValueLabel,
                         &flutterValueLabel, &mixValueLabel })
        label->setFont (juce::FontOptions (11.0f * scale, juce::Font::bold));

    driveValueLabel.setBounds (scaled (291, 266, 110, 16));
    wowValueLabel.setBounds (scaled (415, 266, 110, 16));
    flutterValueLabel.setBounds (scaled (539, 266, 110, 16));
    mixValueLabel.setBounds (scaled (663, 266, 110, 16));
}
