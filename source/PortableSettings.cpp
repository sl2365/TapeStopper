#include "PortableSettings.h"

#include <array>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

juce::File TapeStopperPortableSettings::getDataFolder()
{
#if JUCE_WINDOWS
    std::array<wchar_t, 32768> modulePath {};
    const auto moduleHandle = static_cast<HMODULE>
                              (juce::Process::getCurrentModuleInstanceHandle());

    if (moduleHandle == nullptr)
        return {};

    const auto pathLength = ::GetModuleFileNameW
                            (moduleHandle, modulePath.data(),
                             static_cast<DWORD> (modulePath.size()));

    if (pathLength == 0 || pathLength >= modulePath.size() - 1)
        return {};

    const juce::File moduleFile { juce::String (modulePath.data()) };
    auto folder = moduleFile.getParentDirectory();

    for (int depth = 0; depth < 6; ++depth)
    {
        if (folder.hasFileExtension (".vst3"))
            return folder.getParentDirectory().getChildFile ("Data");

        const auto parent = folder.getParentDirectory();
        if (parent == folder)
            break;
        folder = parent;
    }

    return moduleFile.getParentDirectory().getChildFile ("Data");
#else
    return {};
#endif
}

juce::File TapeStopperPortableSettings::getSettingsFile()
{
    const auto dataFolder = getDataFolder();
    return dataFolder == juce::File() ? juce::File()
                                      : dataFolder.getChildFile ("Settings.ini");
}

TapeStopperPortableSettings TapeStopperPortableSettings::load()
{
    TapeStopperPortableSettings settings;
    const auto settingsFile = getSettingsFile();

    if (! settingsFile.existsAsFile())
        return settings;

    juce::StringArray lines;
    lines.addLines (settingsFile.loadFileAsString());
    juce::String section;

    for (const auto& untrimmedLine : lines)
    {
        const auto line = untrimmedLine.trim();

        if (line.startsWithChar ('[') && line.endsWithChar (']'))
        {
            section = line.substring (1, line.length() - 1).trim();
            continue;
        }

        const auto separator = line.indexOfChar ('=');
        if (separator < 0)
            continue;

        const auto key = line.substring (0, separator).trim();
        const auto value = line.substring (separator + 1).trim();

        if (section.equalsIgnoreCase ("GUI"))
        {
            if (key.equalsIgnoreCase ("ScalePercent"))
                settings.guiScalePercent = juce::jlimit (75, 200, value.getIntValue());
            else if (key.equalsIgnoreCase ("ButtonDisplay"))
                settings.reversedButtonDisplay = value.equalsIgnoreCase ("Reversed");
        }
        else if (section.equalsIgnoreCase ("Audio")
                 && key.equalsIgnoreCase ("FullSpeedMute"))
        {
            settings.fullSpeedMute = value.getIntValue() != 0
                                     || value.equalsIgnoreCase ("On")
                                     || value.equalsIgnoreCase ("True");
        }
    }

    return settings;
}

bool TapeStopperPortableSettings::save() const
{
    const auto settingsFile = getSettingsFile();
    if (settingsFile == juce::File())
        return false;

    const auto dataFolder = settingsFile.getParentDirectory();
    if (! dataFolder.isDirectory() && dataFolder.createDirectory().failed())
        return false;

    juce::String text;
    text << "; TapeStopper portable settings\r\n"
         << "[GUI]\r\n"
         << "ScalePercent=" << juce::jlimit (75, 200, guiScalePercent) << "\r\n"
         << "ButtonDisplay=" << (reversedButtonDisplay ? "Reversed" : "Normal")
         << "\r\n\r\n"
         << "[Audio]\r\n"
         << "FullSpeedMute=" << (fullSpeedMute ? 1 : 0) << "\r\n";

    return settingsFile.replaceWithText (text);
}
