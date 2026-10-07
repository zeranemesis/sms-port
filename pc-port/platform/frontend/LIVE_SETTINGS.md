# Native PAL frontend changes — 7 October 2026

## Rendering and input

Video and graphics share one PartyBoard RmlUi tab. The fidelity submenu applies
an original GameCube profile or the same original resources at 3x resolution.
No sharpening slider or contrast sharpening is applied by the renderer.

Resolution/MSAA attachment changes run on the GL/game thread at the paused
presentation boundary. They retain color/depth and XFB caches, and roll back
attachments on allocation/copy failure. Window settings, postprocessing,
anisotropy, texture-pack enable state, audio volume, bindings and camera options
apply without restarting. Changes to 30/60 gameplay rate update the display,
fader and pad-repeat timing. Packs downloaded through GameBanana are rescanned
with synchronization against the decode worker.

## Scene resources

Language writes the original PAL option flag; the next scene load uses the
existing load2DResource2Aram path. The current scene's HUD archives are retained.
File-mod and HD-movie selection updates the pending environment; the native
scene-load hook restores the original FST and mounts the requested overlays at
the next stage resource load. It never overwrites the extracted disc files.
A scene reload/transition is required for these existing loaded resources,
although restarting the process is not required.

## Online services

GameBanana catalogue, file selection, download and compatible ZIP installation
use game 5798. Unsupported console patches or executable mods are refused;
see GAMEBANANA.md. Compatibility of arbitrary assets with PAL still depends
on the mod author's format and version.

The official RetroAchievements rc_client handles login and account achievement
lists for game 6049, including identification of the actual extracted PAL disc.
Passwords are not persisted. Achievement evaluation and submission remain
suspended: a validated GameCube-address-to-native-state translator is missing.
The frontend explicitly reports this. There are no fabricated achievement awards.

## Validation scope

Compilation logs: build/windows-64-pal/partyboard-live-pal-build.log and
partyboard-live-final-build.log. No automated tests or account login were run.
Visual/runtime behavior and installed third-party assets need user observation.

Final MinGW build succeeded for GMSP01/x64. Executable: `C:/sms-pal-port/build/windows-64-pal/sms.exe`. Original executable retained as `sms-before-live-settings-20261007.exe`. Runtime observations and RA login remain pending.
