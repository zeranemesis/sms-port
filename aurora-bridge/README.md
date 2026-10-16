# aurora-bridge â€” the Aurora renderer for the PC port

`sms_aurora.dll` puts [Aurora](https://github.com/encounter/aurora) (GX on WebGPU through Dawn: Direct3D 12, Vulkan, Direct3D 11) behind a small C interface (`sms_aurora.h`).
The game itself is still built with MinGW. Aurora needs MSVC, so it lives in this DLL.

The GX front end stays the port's own: `platform/gx` encodes the API, parses every command stream and decodes vertices.
With `-DSMS_GX_BACKEND=aurora`, `platform/gx/src/gx_aurora.cpp` replaces the OpenGL renderer and the SDL2 window. It:

- forwards the BP, CP and XF register writes to Aurora's command processor;
- sends draws with the vertices already decoded (format 0, every attribute direct, floats big-endian);
- sends textures and TLUTs as `GX_LOAD_AURORA_TEXOBJ` and `GX_LOAD_AURORA_TLUT` with host pointers, because Aurora does not follow the physical addresses carried by J3D display lists.

## Building

1. Aurora: `third_party/aurora`, a clone of encounter/aurora at 5143394 plus `aurora-sms.patch`, a `git am` series: the hardware-accurate fixes the game needs, then the menu rendering, pixel metrics, EFB copy read-back and the SDK viewport offset, then Mailbox presentation with vsync (the game paces its own frames; `SMS_PRESENT_FIFO=1` brings FIFO back).
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

The 16 October correctness fixes render pollution masks at their full native
size in a temporary data target, retain per-counter pixel areas, and restore
the main EFB after GXCopyTex. This avoids repeated 512-to-448-row clipping and
premature pollution completion at increased display resolution. Necessary
readbacks use a bounded checkpoint under pool pressure rather than increasing
the pool; GPU failure can still reject a request without changing CPU RAM.
Corona's water receives synchronous big-endian Z24X8 data, and its orthographic
depth pass keeps logical copy coordinates in widescreen. Audio output stops
before static destruction. These changes preserve the game clock and do not
certify locked 60 FPS or every stage/episode. Evidence and limits are in
`artifacts/aurora-final-20261016/RESULTATS.md`.

`aurora-state-20261016.patch` is a cumulative Aurora integration snapshot against
`e2b6057d1584832ea8b05ce7103d0ec3654db863`, including earlier local optimizations
and the new correctness work. Apply it to that clean base instead of stacking
the historical EFB/VI/request-ID patches. Existing modified checkouts already
contain the changes. The native bridge/platform sources and `decomp-patches`
remain part of the main repository; rebuild and deploy the DLL and EXE together.

- `SMS_AURORA_BACKEND=d3d12|vulkan|d3d11|auto` (default: d3d12).
- `SMS_GX_STATS=120` prints the median frame time every 120 frames.
- `Jouer-Aurora.cmd` leaves these synchronous reports off by default: their writes caused periodic long frames. Error/startup logs remain enabled. Set `SMS_GX_STATS=120` explicitly for diagnosis.
- `SMS_GX_PROFILE=<file>` records per-frame draw/texture preparation and EFB peek CPU timings; draw includes texture time. Fine timers add overhead and should be used only for diagnosis.
- `SMS_GX_PROFILE_LIGHT=1` keeps primitive/query counts and EFB duration without the per-primitive draw/texture clocks; zero draw/texture durations in this mode mean unmeasured.
- `SMS_GX_STOP_AFTER=<copy count>` ends a bounded diagnostic run through normal shutdown. Unset for play.
- `SMS_GX_PREP_CACHE=0` disables the per-map texture-entry and vertex-stride preparation caches for comparisons. Texture/palette invalidation remains active with either setting.
- `SMS_GX_STATE_CACHE=0` disables the new palette/CP/XF caches for comparisons. Unchanged palettes use a byte comparison against the current TMEM, so direct API writes and overlapping FIFO palette loads are still observed. Only identical XF memory spans and effective matrix indices are suppressed; viewport/projection register commands remain intact.
- `SMS_AURORA_OPTIMIZE_INLINING=ON` (CMake option in `aurora-bridge`) uses Release `/Ob2` inlining for Aurora GX in the normal `RelWithDebInfo` build, retaining symbols and the existing floating-point model. Set it to `OFF` and rebuild the DLL for comparison. See `artifacts/aurora-cpu-20261014/RESULTS.md` for validation and scoped observations.
- `SMS_VI_PRECISE_SLEEP=1` uses a Windows high-resolution waitable timer for VI only, with the original Sleep path as fallback. Aurora's launcher enables it; `0` selects the legacy path. Audio timing and global Windows timer resolution are unchanged.
- `SMS_AURORA_EFB_FULL=1` forces full-image EFB checkpoints. Normally, groups that previously requested only one point use a 32x32 tile, with a same-revision full fallback on a tile miss. Dense groups keep full images.
- `SMS_AURORA_EFB_COMPARE=1` compares sparse reads against a full capture before any game draw can intervene. This performs extra GPU waits and is only for diagnosis.
- `SMS_AURORA_PEEK_PROFILE=<file>` separates staging preparation, staging rotation, worker backlog, command submission and synchronous EFB readback. The historical `clone` column now measures rotation without copying the uploaded prefix.
- `SMS_AURORA_FRAME_PROFILE=<file>` separates frame-slot, staging and setup waits at frame start.
- `SMS_AURORA_WORKER_PROFILE=<file>` times render-worker jobs by frame/pass/type.
- `SMS_AURORA_PIPELINE_PROFILE=<file>` times compilation and pending-pipeline waits; cached lookups do not acquire extra clocks.
  These CPU-only diagnostics add no GPU fences and are off by default.
- `SMS_GX_VERTEX_TIMERS=1` enables the FIFO vertex-loader timers explicitly. Aurora's ordinary frame statistics no longer enable these unused per-primitive timers.
- `SMS_VI_LOG=1` logs the first 120 scanout requests (XFB address, copy serial and black state).
- Debugging:
  - `SMS_AURORA_TEXLOG=1` logs the textures.
  - `SMS_GX_PEEK_LOG=1` logs the first 64 colour/depth reads, their checkpoint and CPU duration.
  - `SMS_AURORA_XFDBG=l|t|L|c` turns lighting off, or logs texgens, lights or TEV colours.
  - `AURORA_SHADER_DEBUG=srtg|all` prints Aurora's shader configurations.

## Renderer work beyond the menu


- The native frontend polls completed copy results before forcing a pressure
  checkpoint. Queued requests and pending dumps still count toward the same
  bounded capacity; request IDs and CPU-write protection are unchanged.
  Two targeted Noki runs measured about 51 intervals/s against 40 in the
  reference, with no copy-drop warnings. This is not a guarantee of constant
  60 FPS across the game. See
  `artifacts/aurora-cpu-next-20261016/RESULTATS.md` for validation and limits.

- Copy-pool pressure submits its sealed prefix without an unrelated depth
  readback. The recording thread services completion events only while the
  render worker is idle, then admits a copy once an actual slot is available.
  The pool remains bounded at 16 slots; request identities, writeback ordering,
  exact depth copies and visual quality are preserved.
  See `artifacts/aurora-frame-waits-20261016/RESULTATS.md` for measurements and limits.

- Startup waits for all known cached shader pipelines before starting the game.
  Window events remain responsive and the window title shows preparation progress.
  New configurations are still learned during gameplay and prepared on subsequent
  launches; a first visit can therefore still compile new shaders.
  `SMS_AURORA_PRECOMPILE=0` restores background-only startup for diagnostics.
  See `artifacts/aurora-precompile-20261016/RESULTATS.md` for validation.

- The episode-selection gradient uses the PC patch `zzzz-port-select-gradient-depth.patch`: its quad sits one float ULP inside the orthographic depth boundary to avoid disappearing in Aurora. Game timing and the original decomp sources are unchanged. See `artifacts/visual-fixes-20261015/RESULTS.md`.

- Identified texture readbacks use the new `sab_copy_tex_request` /
  `sab_poll_copy_readback_request` API. The request ID is preserved through GPU
  mapping; dropped/failed copies produce failure results. The game rejects
  mismatched, older or CPU-obsoleted results before conversion and writeback.
  The old SAB exports remain wrappers for older clients; do not mix old and new
  polling consumers in one process. Rebuild and deploy the DLL and game together.
  `aurora-copy-request-id.patch` records the additional Aurora changes after the
  existing EFB/VI patches. Validation is in `artifacts/visual-fixes-20261015`.
- The portal destination preview is a THP video. The PC game patch
  `zzzz-port-gate-preview-movies.patch` prepares and plays this looping texture
  independently of `skip_movies`; cinematic skipping no longer suppresses it.
  `Jouer-Aurora.cmd` also clears inherited movie-skip and scripted-input overrides.
  The Soleil bounce endpoint is defined at phase 4 by
  `zzzz-port-select-shine-height-boundary.patch`, without changing its timer.
  Controlled portal-to-selection captures and performance measurements are in
  `artifacts/aurora-hud-perf-20261016/RESULTATS.md`.
- Sub-viewports return their existing widescreen mapping before computing vertex
  bounds that would be discarded. This reduces CPU work for data/pollution draws
  without changing pixels, readbacks or the simulation clock.

- Pixel metrics (Delfino's pollution count) and data copies are read back into the game's RAM. EFB colour/depth peeks now use synchronous GPU checkpoints at the game's query, cached until another draw/copy or frame. They return GX ARGB and Z24, including reversed-depth conversion. MSAA probes read sample zero from both attachments. This preserves query ordering without using a previous frame, but the first read of each group introduces a GPU wait; profiling and visual comparison remain necessary. See `artifacts/aurora-efb-peek-20261013/RESULTS.md`.
- `aurora-efb-vi.patch` applies to Aurora baseline `e2b6057d1584832ea8b05ce7103d0ec3654db863` (the three commits in `aurora-sms.patch`). It includes EFB peeks, staging rotation and VI scanout; use it instead of the older `aurora-efb-peek.patch`. Existing checkouts already contain these changes.
- Menu rendering, live changes, pause/resume and widescreen/HUD mapping require runtime observation across scenes and GPUs.
- Display copies retain a GPU image per XFB address and apply the GX colour/alpha/depth clear masks. VI selects the visible copy or opaque black; unchanged scanout is not requested again at every retrace. Pending game work can be submitted without presenting it. EFB contents survive these boundaries and menu pause. Exact VI filtering/interlacing, XFB CPU readback (`GXPC_ReadXFB`), and longer scene-transition validation remain unfinished. See `artifacts/aurora-vi-20261013/RESULTS.md`.
- EFB checkpoints resume appending in a fresh mapped staging slot after the already uploaded prefix, preserving shared GPU buffer offsets. Prefixes are aligned to four bytes and checked after worker synchronization; recording does not read or upload the skipped prefix. This removes a measured 19â€“23 ms mapped-memory clone at Delfino without replacing the current EFB read with stale data. See `artifacts/aurora-perf-20261014/RESULTS.md` for the scoped performance observations and limits.
- Remaining performance changes and comparisons are in `artifacts/aurora-opt-20261014/RESULTS.md`: optional synchronous reports, adaptive EFB tiles with full-reference comparison, preparation caches, precise VI waits, and suppression of redundant matrix/texture state updates. They do not certify constant 60 FPS across the game.
