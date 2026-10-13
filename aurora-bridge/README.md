# aurora-bridge — the Aurora renderer for the PC port

`sms_aurora.dll` puts [Aurora](https://github.com/encounter/aurora) (GX on WebGPU through Dawn: Direct3D 12, Vulkan, Direct3D 11) behind a small C interface (`sms_aurora.h`).
The game itself is still built with MinGW. Aurora needs MSVC, so it lives in this DLL.

The GX front end stays the port's own: `platform/gx` encodes the API, parses every command stream and decodes vertices.
With `-DSMS_GX_BACKEND=aurora`, `platform/gx/src/gx_aurora.cpp` replaces the OpenGL renderer and the SDL2 window. It:

- forwards the BP, CP and XF register writes to Aurora's command processor;
- sends draws with the vertices already decoded (format 0, every attribute direct, floats big-endian);
- sends textures and TLUTs as `GX_LOAD_AURORA_TEXOBJ` and `GX_LOAD_AURORA_TLUT` with host pointers, because Aurora does not follow the physical addresses carried by J3D display lists.

## Building

1. Aurora: `third_party/aurora`, a clone of encounter/aurora at 5143394 plus `aurora-sms.patch` (hardware-accurate fixes the game needs).
2. The DLL, with MSVC and Ninja.
   The dependency sources are taken from an existing Aurora build (`SMS_AURORA_DEPS`, by default the Mario Party 4 build), so nothing is downloaded:
   ```
   vcvars64.bat
   cmake -S aurora-bridge -B build/aurora-bridge -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
   cmake --build build/aurora-bridge --target sms_aurora
   ```
3. The game, with MinGW, in its own build directory:
   ```
   cmake -S . -B build/windows-64-pal-aurora -G Ninja -DSMS_REGION=GMSP01 -DSMS_GX_BACKEND=aurora
   cmake --build build/windows-64-pal-aurora --target sms
   ```
   The DLLs (`sms_aurora.dll`, `webgpu_dawn.dll`, `SDL3.dll`, the DXC compiler) are copied next to `sms.exe`.

## Running

- `SMS_AURORA_BACKEND=d3d12|vulkan|d3d11|auto` (default: d3d12).
- `SMS_GX_STATS=120` prints the median frame time every 120 frames.
- Debugging:
  - `SMS_AURORA_TEXLOG=1` logs the textures.
  - `SMS_AURORA_XFDBG=l|t|L|c` turns lighting off, or logs texgens, lights or TEV colours.
  - `AURORA_SHADER_DEBUG=srtg|all` prints Aurora's shader configurations.

## Not done yet

- Pixel metrics (Delfino's pollution count), and EFB peeks: the sun's lens flare reads the depth buffer.
- The port's extensions: widescreen, HUD panes, name tags, the PartyBoard menu (F1).
- VI timing beyond presenting at every display copy.
