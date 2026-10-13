# aurora-bridge — the Aurora renderer for the PC port

`sms_aurora.dll` puts [Aurora](https://github.com/encounter/aurora) (GX on WebGPU through Dawn: Direct3D 12, Vulkan, Direct3D 11) behind a small C interface (`sms_aurora.h`).
The game itself is still built with MinGW. Aurora needs MSVC, so it lives in this DLL.

The GX front end stays the port's own: `platform/gx` encodes the API, parses every command stream and decodes vertices.
With `-DSMS_GX_BACKEND=aurora`, `platform/gx/src/gx_aurora.cpp` replaces the OpenGL renderer and the SDL2 window. It:

- forwards the BP, CP and XF register writes to Aurora's command processor;
- sends draws with the vertices already decoded (format 0, every attribute direct, floats big-endian);
- sends textures and TLUTs as `GX_LOAD_AURORA_TEXOBJ` and `GX_LOAD_AURORA_TLUT` with host pointers, because Aurora does not follow the physical addresses carried by J3D display lists.

## Building

1. Aurora: `third_party/aurora`, a clone of encounter/aurora at 5143394 plus `aurora-sms.patch`, a `git am` series: the hardware-accurate fixes the game needs, then the menu rendering, pixel metrics, EFB copy read-back and the SDK viewport offset.
   The local checkout already includes them (branch `sms-port`).
2. The DLL, with MSVC and Ninja.
   The dependency sources are taken from an existing Aurora build (`SMS_AURORA_DEPS`, by default the Mario Party 4 build), so nothing is downloaded:
   ```
   vcvars64.bat
   cmake -S aurora-bridge -B build/aurora-bridge -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
   cmake --build build/aurora-bridge --target sms_aurora
   ```
3. The game, with MinGW, in its own build directory:
   ```
   cmake -S . -B build/windows-64-pal-aurora -G Ninja -DSMS_REGION=GMSP01 -DSMS_GX_BACKEND=aurora -DSMS_PARTYBOARD_UI=ON -DSMS_GX_BUILD_TESTS=OFF
   cmake --build build/windows-64-pal-aurora --target sms
   ```
   The DLLs (`sms_aurora.dll`, `webgpu_dawn.dll`, `SDL3.dll`, the DXC compiler) are copied next to `sms.exe`.

`Construire-Aurora.cmd` performs both compiler phases with the installed Visual Studio x64 tools and MSYS2, then refreshes the runtime DLLs even when the game executable does not need relinking.

## PartyBoard menu

Run `Jouer-Aurora.cmd`. Open the original Sunshine PartyBoard menu with **F1** or the controller **Guide** button. The game pauses while its retained image and the menu continue to render. Mouse, wheel, keyboard, text input, controller directions and stick navigation use the Aurora window.

The shared `partyboard_menu.cpp` and `res/partyboard` provide the same eight tabs as OpenGL: combined video/graphics, audio, controllers, game, saves, mods, achievements and updates. There is one copy of the frontend services in the MinGW game. Only plain C data and opaque renderer handles cross the MSVC DLL boundary; SDL3 calls stay in that DLL to avoid collisions with the game's SDL2.

Graphics changes are queued for the next frame outside GPU recording, after synchronizing the render worker. Resolution, supported MSAA (off/4x), FXAA, anisotropy, brightness, aspect, presentation filter, widescreen/HUD and window settings are connected to Aurora. FXAA uses only the current frame. Texture packs use Aurora's Dolphin PNG/DDS replacement registry, including palettes, selected mod priorities and `.sms-disabled` folders. Language and scene file replacements retain the existing next-level behavior. Save restores retain the existing next-process behavior.

Updates select a Windows 64 PAL **Aurora** ZIP with a published SHA-256 digest, and check `renderer: "aurora"` plus `sms_aurora.dll` before installation. A published OpenGL package is not selected by this build. No Aurora release is published by this change.

RetroAchievements login and the official set 6049 retain their existing implementation. Native memory mapping for achievement unlocks remains incomplete. Online play is excluded. 360 Hz interpolation is not added by this menu migration.

The DLL and native executable are compiled locally. F1 and Resume were observed in game after fixing an input-reset recursion. A subsequent debugger run reached 1,700 game frames with 1x/5x resolution changes, MSAA 4x, FXAA and a normal exit after fixing reentrant GPU readback callbacks. This limited run does not certify all scenes or graphics devices. Build logs, snapshots and reports are in `artifacts/aurora-menu-20261013`; see `RUNTIME-FIXES.md` for the evidence and remaining limits. `Jouer-Aurora.cmd` now retains per-session logs in `artifacts/aurora-runtime`.

## Running

- `SMS_AURORA_BACKEND=d3d12|vulkan|d3d11|auto` (default: d3d12).
- `SMS_GX_STATS=120` prints the median frame time every 120 frames.
- Debugging:
  - `SMS_AURORA_TEXLOG=1` logs the textures.
  - `SMS_AURORA_XFDBG=l|t|L|c` turns lighting off, or logs texgens, lights or TEV colours.
  - `AURORA_SHADER_DEBUG=srtg|all` prints Aurora's shader configurations.

## Renderer work beyond the menu

- Pixel metrics (Delfino's pollution count) match the OpenGL port, and data copies are read back into the game's RAM. EFB peeks (the sun's glare test) and custom name-tag drawing remain separate renderer work.
- Menu rendering, live changes, pause/resume and widescreen/HUD mapping require runtime observation across scenes and GPUs.
- VI timing beyond presenting at every display copy.
