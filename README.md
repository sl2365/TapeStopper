# TapeStopper

[![Release](https://img.shields.io/github/v/release/sl2365/TapeStopper?style=for-the-badge-square&logo=github&logoColor=white&color=purple)](https://github.com/sl2365/TapeStopper/releases/latest/download/TapeStopper.rar)
[![Release Date](https://img.shields.io/github/release-date/sl2365/TapeStopper?style=for-the-badge-square&logo=github&logoColor=white&color=yellow)](https://github.com/sl2365/TapeStopper/releases)

[![Latest Asset Downloads](https://img.shields.io/github/downloads/sl2365/TapeStopper/latest/TapeStopper.rar?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-latest&displayAssetName=false&color=blue)](https://github.com/sl2365/TapeStopper/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/sl2365/TapeStopper/total?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-total&color=blue)](https://github.com/sl2365/TapeStopper/releases)

[![Commits Since Release](https://img.shields.io/github/commits-since/sl2365/TapeStopper/latest?style=for-the-badge-square&logo=github&logoColor=white&color=green)](https://github.com/sl2365/TapeStopper/activity)
[![Last Commit](https://img.shields.io/github/last-commit/sl2365/TapeStopper?style=for-the-badge-square&logo=github&logoColor=white&color=green)](https://github.com/sl2365/TapeStopper/activity)

TapeStopper is a portable Windows x64 VST3 audio effect for tape-style slowdown and startup transitions. It combines independently timed DOWN and UP motion with tempo sync, an editable downward pitch envelope, a live waveform display, transition curves, saturation, wow, flutter, irregular flux instability, wet/dry mix, threshold muting and portable user presets.

The interface is inspired by the TapeStop effect by TBT (Daniel Lind) and SLowER by DiVerSe. I was unable to find any sites remaining for either, only posts made on KVR audio.

## Current version

Stage 8.0 is a functional development build. The plug-in uses the EP-style slowdown/startup behaviour; the earlier experimental TD mode is intentionally not included.

## Features

- Buffered tape slowdown and startup processing.
- Momentary and Toggle operation for the main Play button.
- Independently enabled DOWN and UP transitions.
- Separate DOWN and UP transition times.
- FREE timing from 0.05 to 8.00 seconds.
- Host-tempo SYNC timing from 4 bars to 1/64 notes.
- Independent Linear, Gentle, Steep and S-Curve transition shapes.
- Editable 11-point downward pitch envelope with a range of -12 to +12 semitones.
- Optional red live-output waveform behind the envelope graph.
- Adjustable Mute At transition threshold.
- Tape-style Drive, Wow and Flutter controls.
- Strong FLUX control for irregular pitch warble and playback errors during DOWN and UP. It fluctuates around the envelope-shaped pitch when the envelope is enabled, or around the normal tape curve when it is bypassed.
- Dry/processed Mix control.
- Full Speed Mute option for send-effect use.
- Normal or Reversed main-button icon display.
- Portable INI presets with INIT, previous/next, rename, Save, Save As and Load.
- Resizable 800 x 300 interface from 75% to 200%, with a fixed aspect ratio.
- Portable settings and presets stored beside the VST3.
- Mono and stereo audio layouts.

For a detailed description of every control, see [Instructions.ini](Instructions.ini).

## Quick use

1. Load `TapeStopper.vst3` as an audio effect.
2. Leave `PLAY: M` selected to use the large blue button momentarily, or select `PLAY: T` for toggle operation.
3. Set the DOWN time with the green marker using the left mouse button.
4. Set the UP time with the red marker using the right mouse button.
5. Press the large blue button to slow and stop the audio. Release it, or press it again in Toggle mode, to return to full speed.

The green DOWN marker and red UP marker can be adjusted only on the timing track. The readout underneath the track is informational and cannot move the markers.

## Timing modes

In FREE mode, the two timing markers select continuous transition times between 0.05 and 8.00 seconds. Moving a marker left produces a longer/slower transition; moving it right produces a shorter/faster transition.

In SYNC mode, the markers snap independently to:

`4 BAR`, `2 BAR`, `1 BAR`, `1/2`, `1/3`, `1/4`, `1/6`, `1/8`, `1/16`, `1/24`, `1/32`, `1/48`, `1/64`

SYNC timing follows tempo information supplied by the host. FREE and SYNC values are retained separately when switching modes.

## Envelope

The 11-point envelope adds pitch movement to the DOWN transition only. Time runs from left to right and the centre line represents zero additional pitch. Moving a point upward adds pitch; moving it downward subtracts pitch.

- The first and last points move vertically only.
- The middle nine points move horizontally and vertically but cannot cross each other.
- Double-clicking a point resets that point to zero semitones without changing its time position.
- `ENV RESET` resets the pitch of all eleven points to zero while preserving their time positions.
- `ENVELOPE: OFF` leaves the edited curve visible but prevents it from affecting the sound.

The UP transition has its own curve control but does not use the pitch envelope.

## Presets and portable data

User presets are saved as INI files in:

```text
Data\Presets
```

The folder is created beside the loaded VST3. Presets store the sound controls, timing choices, direction enables, Play mode and all envelope points. They do not store the temporary pressed state of the large Play button or the global Setup options.

Portable editor and Setup options are saved in:

```text
Data\Settings.ini
```

This stores the editor scale, Full Speed Mute setting, Normal/Reversed button display choice and Waveform display choice. No settings or presets are written to the Windows user profile.

## Building

The supplied build script expects this directory arrangement:

```text
_Projects\TapeStopper\
_Tools\cmake\_4.4.2\
_Tools\JUCE\_8.0.15\
```

Visual Studio 18 2026 with the x64 C++ tools is required.

1. Open the TapeStopper Project Root in File Explorer.
2. Double-click `- Build.bat`.
3. Wait for the build summary to report PASS.
4. Load `dist\TapeStopper.vst3` in the host.

The build creates a portable single-file Windows x64 VST3 and preserves an existing `dist\Data` folder. It does not install the plug-in elsewhere. Detailed build output is written to `Results.log`.

## Project files

- `source` - C++ and CMake source files.
- `- Build.bat` - one-click Windows x64 Release build.
- `Instructions.ini` - complete control reference.
- `README-dev.md` - detailed stage behaviour and development notes.

## Development status

TapeStopper is still under development. Its bundle ID and four-character VST identity codes are provisional and should be replaced with permanent approved identifiers before a compatibility-sensitive public release. Changing those identifiers later will cause hosts to recognise the plug-in as a different product.
