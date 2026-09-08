# TapeStopper - Stage 11.3

This is the first functional Windows x64 VST3 stage.

## Stage 11.3 behaviour

- Builds a native Windows x64 VST3 audio effect with C++17 and JUCE 8.0.15.
- Passes audio through unchanged at full speed when the new tape-character controls are at their neutral defaults.
- Implements the first EP-mode buffered tape slowdown and startup engine.
- Implements Momentary/M and Toggle/T operation for the main button.
- Adds a separate RETRIG button that jumps instantly to stopped speed and then runs the current UP transition.
- Makes RETRIG independent of the Play M/T mode and every DOWN setting, while using the current UP enable, time and curve.
- Keeps Mute At and the tape-character processing active during the UP recovery, and highlights RETRIG blue while that recovery is active.
- Releases an engaged main Play control when RETRIG is clicked, while a later normal Play action cancels the automatic cycle cleanly.
- Implements independent Up and Down enable buttons.
- Keeps both direction buttons grey while motion is inactive.
- Highlights only Down in green while the blue marker moves right, or only Up in red while it moves left.
- Styles the small rectangular controls after the original Tape Stop buttons, with a metallic gradient, inset bevel and dark outline.
- Shows enabled-but-idle direction buttons in light silver and disabled directions in a clearly darker grey.
- Shows the PLAY T/M button in the normal light-silver enabled style and sizes it to match UP/DOWN.
- Places FREE/SYNC beneath PLAY T/M, with every button in that control stack the same size.
- Replaces the old SETUP button and top-right stage text with a plain cog, without a button background or border.
- Aligns the visible Setup cog artwork, rather than its invisible button bounds, with the right edge of the Envelope/Mute At panel and matches its colour to the TAPESTOPPER title text.
- Makes the cog toggle the middle panel between the timing/envelope controls and Setup options.
- Highlights Button Normal/Reversed in blue when active, distinguishing the display option from the green audio-option highlight.
- Makes both Setup Display buttons the same width as the Setup Audio button.
- Draws Mute At as a custom cyan-to-deep-blue knob matching the main Play button.
- Adds Full Speed Mute for send-effect use, with a short smoothed transition into and out of silence.
- Adds Normal/Reversed Play-button display without changing the trigger behaviour.
- Saves GUI scale, Full Speed Mute, button display mode and waveform display mode in the portable `Data\Settings.ini` file.
- Makes the PLAY triangle slightly narrower and rounds its three corners.
- Implements independently adjustable green Down and red Up speed markers.
- Extends the FREE timing range from 0.05 to 8.00 seconds.
- Preserves the measured 20% marker default at approximately 0.71 seconds while extending the slow end of the curve.
- Shows a blue transition-position indicator.
- Adds a functional FREE/SYNC button that switches between continuous and host-tempo timing.
- Snaps the green Down and red Up markers independently to 4 BAR, 2 BAR, 1 BAR, 1/2, 1/2T, 1/4, 1/4T, 1/8, 1/16, 1/16T, 1/32, 1/32T or 1/64 in SYNC mode.
- Shows seconds below the timing bar in FREE mode and the selected divisions in SYNC mode.
- Restricts timing-marker clicks and drags to the visible slider track; the DOWN/UP time readout below it is non-interactive.
- Retains the continuous Free values separately when switching between timing modes.
- Replaces the two fixed-shape curve buttons with Filter Amount and Volume Amount controls.
- Adds independent Pitch, Filter and Volume curve pairs, each with five vertically draggable control points equally spaced at 1/6 intervals across the transition.
- Uses smooth Catmull-Rom interpolation so the five points can create linear, gentle, steep, S-shaped and non-monotonic curves with finer control.
- Edits DOWN with square points and the left mouse button, and UP with circular points and the right mouse button.
- Adds four compact graph tabs with separately clickable enable LEDs for Pitch, Filter, Volume and the global Envelope.
- Uses blue for Pitch, orange for Filter, purple for Volume and the established green for Envelope.
- Distinguishes every direction in the graph: Pitch uses cyan DOWN/blue UP, Filter uses orange DOWN/brown UP, and Volume uses violet DOWN/indigo UP.
- Keeps the red transition trace visible in all four graph views.
- Makes Pitch bypass independent, allowing Filter-only or Volume-only triggered processing.
- Adds smoothed low-pass Filter and processed-level Volume curves with separate 0%-100% Amount controls.
- Corrects the Filter path so the selected curve directly controls logarithmic cutoff movement from the open frequency toward 120 Hz; Filter Amount now sets depth without a second blend that weakened the early and middle sweep.
- Reorganises all seven bottom knobs into equal spacing and abbreviates the first two labels to FLTR AMT and VOL AMT.
- Adds a persistent SEQ view button and a separately clickable blue enable LED.
- Replaces the seven-knob display with a 64-step editor while SEQ view is open, without changing whether the sequencer is enabled.
- Displays the 64 binary Play states as two rows of 32 and supports click-drag pattern painting.
- Drives the main Play state from the active step while the host transport is running and returns to manual Play control when sequencing is disabled.
- Provides sequencer Sync rates of 1/2, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T, 1/64, 1/64T and 1/128, plus Free step durations from 25-2000 ms.
- Renames the main timing-bar triplet values to matching T notation without changing their actual durations.
- Clamps sequencer Rate, Length and Offset adjustment at their end values instead of wrapping.
- Enlarges only the SEQ view-button text and darkens the coloured graph-tab text for clearer reading.
- Moves FLTR AMT, VOL AMT, DRIVE, WOW and FLUTTER controls and labels together onto 97-pixel centre spacing; FLUX remains fixed and MIX moves left over its unchanged label.
- Adds 1-64 step Length, 0-63 step Offset and a Reset control that clears the entire pattern.
- Highlights the current step and dims steps outside the selected Length.
- Adds smoothly adjustable tape-style saturation with Drive from 0%-100%.
- Adds slow 0.33 Hz Wow and faster 6.5 Hz Flutter using smoothly blended variable-delay modulation.
- Adds a 0%-100% Flux control that applies smoothly changing random pitch instability and occasional stronger playback errors during Down and Up only.
- Applies Flux as relative pitch instability after the optional envelope pitch calculation: it follows the envelope when On and the normal tape curve when Off.
- Raises maximum Flux pitch depth to approximately 2.5 times the Stage 7.0 range, based on comparison recordings.
- Uses a shared Flux movement for both channels, avoiding unwanted stereo-image wandering.
- Safely clamps the final envelope-plus-Flux tape speed so the read head cannot overtake the live write head.
- Adds a 0%-100% dry/processed Mix control.
- Preserves the established basic sound by defaulting all six simple curves to Linear, Pitch to On, Filter/Volume to Off, Drive/Wow/Flutter/Flux to 0% and Mix to 100%.
- Retains EP as the fixed slowdown/startup engine while the former TD mode remains deferred.
- Retains the editable 11-point global Envelope for downward pitch/scratch modulation as a separate graph view.
- Locks the first and last envelope points horizontally while allowing vertical pitch adjustment.
- Allows the middle nine points to move in time and pitch without crossing adjacent points.
- Resets any envelope point to the zero-semitone centre line when it is double-clicked, without changing its time position.
- Makes the reset button contextual: CURVE RESET linearises both directions of the selected simple curve, while ENV RESET centres all eleven global Envelope points.
- Makes the envelope graph a few pixels taller while retaining its rounded corners and timing-bar alignment.
- Aligns the rounded envelope graph with the start and end positions of the blue timing indicator.
- Replaces the fast rolling audio waveform with a persistent red transition trace similar in purpose to the Yum display.
- Uses horizontal position for progress through the complete DOWN or UP transition and vertical position for effective tape speed/pitch.
- Captures the actual speed after the envelope and Flux calculations, so their pitch movement is visible in the trace.
- Leaves the completed transition visible until the next DOWN or UP movement begins.
- Adds a blue-highlighted WAVEFORM ON/OFF option to Setup and stops transition-trace capture entirely when it is Off.
- Applies a neutral-centred pitch offset from -12 to +12 semitones around the normal downward tape curve.
- Safely limits positive modulation at normal playback speed so the read head cannot overtake live input.
- Defaults the envelope to a neutral flat line and OFF, preserving the Stage 3 sound until enabled.
- Adds an automatable Mute At knob, with a 0%-100% transition threshold.
- Displays the value as a single non-editable label such as `MUTE AT: 90%`; the knob remains the only direct editor control.
- Applies Mute At during downward and upward travel and smooths the gain edge over approximately 5 ms to prevent clicks.
- Adds a compact preset panel beneath the main Play button with a preset-name display, previous/next arrows, NAME and MENU.
- Enlarges the four preset-button labels and makes the preset-name background read from the buttons' unchanged background-colour definition.
- Cycles through INIT and saved presets one at a time with the arrows or the mouse wheel over the preset name.
- Provides SAVE, SAVE AS and a LOAD submenu containing INIT and every saved user preset.
- Opens the Save As and Rename preset dialogues centrally over the plug-in interface.
- Allows NAME to rename a saved preset; when INIT is selected it creates a newly named preset instead.
- Saves portable preset files to `Data\Presets` beside the loaded VST3 and scans that folder whenever its list is opened or stepped.
- Stores every sound/control parameter and all envelope points in a preset, while excluding the transient main Play state and the global Setup options.
- Stores the Filter/Volume Amount controls, all six five-point curves and all four enable states in presets.
- Retains the current preset name in DAW project state.
- Centres the SEQ button and LED vertically in the bottom panel and increases the horizontal gap between them.
- Narrows the four graph-tab buttons while preserving each LED-to-button gap and adding clearer spacing between neighbouring pairs.
- Makes all five sequencer controls the same width and doubles the space between them for a more consistent control row.
- Saves and restores the Stage 11.3 parameters, curves, Envelope points and sequencer pattern in DAW project state. RETRIG remains a transient action rather than a preset value.
- Opens at 800 x 300 pixels and resizes from 600 x 225 through 1600 x 600 while preserving the aspect ratio.
- Creates `Data\Settings.ini` beside the portable VST3 and restores the last GUI scale when the editor is reopened.
- Produces the portable single-file output `dist\TapeStopper.vst3`.
- Does not install the plug-in elsewhere.
- Preserves an existing `dist\Data` folder.

TD mode is intentionally deferred while the envelope is evaluated. Setup continues to use the requested toggleable middle-panel view rather than a separate window.

## Provisional identity warning

The Stage 11.3 bundle ID and four-character VST identity codes are development placeholders. They must be replaced with approved permanent identifiers before any public build. Changing them later will make hosts see the permanent build as a different plug-in, so Stage 11.3 must not be treated as a compatibility release.

## Build

Double-click `- Build.bat` in the TapeStopper Project Root. The batch file uses the shared tools beside the project:

```text
..\_Tools\cmake\_4.4.2\bin\cmake.exe
..\_Tools\JUCE\_8.0.15\CMakeLists.txt
```

The required generator is Visual Studio 18 2026, x64.
