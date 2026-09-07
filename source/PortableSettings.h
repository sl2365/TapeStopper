#pragma once

#include <JuceHeader.h>

struct TapeStopperPortableSettings
{
    int guiScalePercent = 100;
    bool fullSpeedMute = false;
    bool reversedButtonDisplay = false;
    bool waveformDisplay = true;

    static TapeStopperPortableSettings load();
    static juce::File getDataFolder();
    bool save() const;

private:
    static juce::File getSettingsFile();
};
