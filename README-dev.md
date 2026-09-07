# TapeStopper - Stage 6.1

This is the first functional Windows x64 VST3 stage.

## Stage 6.1 behaviour

- Builds a native Windows x64 VST3 audio effect with C++17 and JUCE 8.0.15.
- Passes audio through unchanged at full speed when the new tape-character controls are at their neutral defaults.
- Implements the first EP-mode buffered tape slowdown and startup engine.
- Implements Momentary/M and Toggle/T operation for the main button.
- Implements independent Up and Down enable buttons.
- Keeps both direction buttons grey while motion is inactive.
- Highlights only Down in green while the blue marker moves right, or only Up in red while it moves left.
- Styles the small rectangular controls after the original Tape Stop buttons, with a metallic gradient, inset bevel and dark outline.
- Shows enabled-but-idle direction buttons in light silver and disabled directions in a clearly darker grey.
- Shows the PLAY T/M button in the normal light-silver enabled style and sizes it to match UP/DOWN.
- Places FREE/SYNC beneath PLAY T/M, with every button in that control stack the same size.
- Replaces the old SETUP button and top-right stage text with a plain white cog, without a button background or border.
- Makes the cog toggle the middle panel between the timing/envelope controls and Setup options.
- Highlights Button Normal/Reversed in blue when active, distinguishing the display option from the green audio-option highlight.
- Draws Mute At as a custom cyan-to-deep-blue knob matching the main Play button.
- Adds Full Speed Mute for send-effect use, with a short smoothed transition into and out of silence.
- Adds Normal/Reversed Play-button display without changing the trigger behaviour.
- Saves GUI scale, Full Speed Mute and button display mode in the portable `Data\Settings.ini` file.
- Makes the PLAY triangle slightly narrower and rounds its three corners.
- Implements independently adjustable green Down and red Up speed markers.
- Extends the FREE timing range from 0.05 to 8.00 seconds.
- Preserves the measured 20% marker default at approximately 0.71 seconds while extending the slow end of the curve.
- Shows a blue transition-position indicator.
- Adds a functional FREE/SYNC button that switches between continuous and host-tempo timing.
- Snaps the green Down and red Up markers independently to 4 BAR, 2 BAR, 1 BAR, 1/2, 1/3, 1/4, 1/6, 1/8, 1/16, 1/24, 1/32, 1/48 or 1/64 in SYNC mode.
- Shows seconds below the timing bar in FREE mode and the selected divisions in SYNC mode.
- Restricts timing-marker clicks and drags to the visible slider track; the DOWN/UP time readout below it is non-interactive.
- Retains the continuous Free values separately when switching between timing modes.
- Replaces the former bottom information text with working Down Curve, Up Curve, Drive, Wow, Flutter and Mix controls.
- Provides independent Linear, Gentle, Steep and S-Curve shapes for downward and upward transitions.
- Adds smoothly adjustable tape-style saturation with Drive from 0%-100%.
- Adds slow 0.33 Hz Wow and faster 6.5 Hz Flutter using smoothly blended variable-delay modulation.
- Adds a 0%-100% dry/processed Mix control.
- Preserves the established sound by defaulting both curves to Linear, Drive/Wow/Flutter to 0% and Mix to 100%.
- Retains EP as the fixed slowdown/startup engine while the former TD mode remains deferred.
- Adds an ENVELOPE ON/OFF button and an editable 11-point envelope for downward motion.
- Locks the first and last envelope points horizontally while allowing vertical pitch adjustment.
- Allows the middle nine points to move in time and pitch without crossing adjacent points.
- Resets any envelope point to the zero-semitone centre line when it is double-clicked, without changing its time position.
- Adds ENV RESET to return all eleven pitch points to zero semitones while preserving their time positions.
- Makes the envelope graph a few pixels taller while retaining its rounded corners and timing-bar alignment.
- Aligns the rounded envelope graph with the start and end positions of the blue timing indicator.
- Applies a neutral-centred pitch offset from -12 to +12 semitones around the normal downward tape curve.
- Safely limits positive modulation at normal playback speed so the read head cannot overtake live input.
- Defaults the envelope to a neutral flat line and OFF, preserving the Stage 3 sound until enabled.
- Adds an automatable Mute At knob, with a 0%-100% transition threshold.
- Displays the value as a single non-editable label such as `MUTE AT: 90%`; the knob remains the only direct editor control.
- Applies Mute At during downward and upward travel and smooths the gain edge over approximately 5 ms to prevent clicks.
- Adds a compact preset panel beneath the main Play button with a preset-name display, previous/next arrows, NAME and MENU.
- Cycles through INIT and saved presets one at a time with the arrows or the mouse wheel over the preset name.
- Provides SAVE, SAVE AS and a LOAD submenu containing INIT and every saved user preset.
- Opens the Save As and Rename preset dialogues centrally over the plug-in interface.
- Allows NAME to rename a saved preset; when INIT is selected it creates a newly named preset instead.
- Saves portable preset files to `Data\Presets` beside the loaded VST3 and scans that folder whenever its list is opened or stepped.
- Stores every sound/control parameter and all envelope points in a preset, while excluding the transient main Play state and the global Setup options.
- Includes all six new bottom-panel parameters in saved presets and loads Stage 5.x presets with safe defaults for them.
- Retains the current preset name in DAW project state.
- Saves and restores the Stage 6.1 parameters and all envelope points in DAW project state.
- Opens at 800 x 300 pixels and resizes from 600 x 225 through 1600 x 600 while preserving the aspect ratio.
- Creates `Data\Settings.ini` beside the portable VST3 and restores the last GUI scale when the editor is reopened.
- Produces the portable single-file output `dist\TapeStopper.vst3`.
- Does not install the plug-in elsewhere.
- Preserves an existing `dist\Data` folder.

TD mode is intentionally deferred while the envelope is evaluated. Setup continues to use the requested toggleable middle-panel view rather than a separate window.

## Provisional identity warning

The Stage 6.1 bundle ID and four-character VST identity codes are development placeholders. They must be replaced with approved permanent identifiers before any public build. Changing them later will make hosts see the permanent build as a different plug-in, so Stage 6.1 must not be treated as a compatibility release.

## Build

Double-click `- Build.bat` in the TapeStopper Project Root. The batch file uses the shared tools beside the project:

```text
..\_Tools\cmake\_4.4.2\bin\cmake.exe
..\_Tools\JUCE\_8.0.15\CMakeLists.txt
```

The required generator is Visual Studio 18 2026, x64.
