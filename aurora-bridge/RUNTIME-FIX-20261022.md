# Sunshine: threaded Dawn and fullscreen correction

## Reported failure

CubeShelf's installed `cubeshelf-v0.3.0` Sunshine runtime crashed in
`webgpu_dawn.dll` with access violation `0xc0000005` on Intel Graphics,
driver 32.0.101.8826. Startup explicitly selected Direct3D 12. The native stack
reached `aurora::gx::render`, at the texture `SetBindGroup(2, ...)` call.
An exported Dawn function name mentioning Vulkan does not identify the backend.

## Corrections

1. `aurora-runtime-fixes-20261022.patch` preserves cached bind groups during
   resize. `GXPC_PollVI()` can pump SDL while a frame is recording. Resize
   previously called `clear_caches()`, destroying the cache even though unsealed
   draw commands retained its numeric identifiers. Synchronizing the worker
   cannot submit those commands. Their later lookup could dereference `end()`
   because the former `CHECK` was compiled out in Release. Cached groups now
   survive resize and are reclaimed by the existing 32-frame age limit, swept
   every 16 frames; only three frame slots can be in flight. Shutdown still
   drains the worker before clearing the cache. Missing lookups use an always
   active `ASSERT` for an explicit diagnostic.
2. The same patch requests
   `wgpu::FeatureName::ImplicitDeviceSynchronization` before device creation.
   Aurora shares the device across resource creation, pipeline compilation and
   the render worker. This feature enables Dawn's device mutex for those calls.
   It does not make a single command encoder safe for concurrent encoding;
   command encoding stays on its existing owner thread.
3. `bridge.cpp` handles F11 and Alt+Enter inside the shader preparation callback.
   It drains GX/render work before processing window events, ignores repeated
   keydown events and removes consumed shortcuts so they cannot toggle twice.

4. `SMS_AURORA_VALIDATE=1` enables Dawn validation and shader robustness for
   diagnostic runs. It does not change the backend.

Direct3D 12 remains the default. Neither workers nor shader preparation are
disabled. The Vulkan launch used during diagnosis is not this correction.

## Primary Dawn evidence

The bundled Dawn package uses revision
`13abc3bc8ea2d3c2050f9e77a12d012108ceee24`.

- [Device.cpp](https://github.com/google/dawn/blob/13abc3bc8ea2d3c2050f9e77a12d012108ceee24/src/dawn/native/Device.cpp#L441): device mutex creation depends on the requested feature.
- [PhysicalDevice.cpp](https://github.com/google/dawn/blob/13abc3bc8ea2d3c2050f9e77a12d012108ceee24/src/dawn/native/PhysicalDevice.cpp#L61): native adapters expose this feature.
- [Feature documentation](https://github.com/google/dawn/blob/13abc3bc8ea2d3c2050f9e77a12d012108ceee24/docs/dawn/features/implicit_device_synchronization.md): public device/child operations become synchronized, except command encoding. The documented implementation has coarse locking and possible reentrancy limitations; performance and runtime behavior need observation.

## Verification status

- Source: reproduced Aurora from all six commits in the tracked
  `aurora-sms.patch`; applied the synchronization correction.
- Build: MSVC RelWithDebInfo `sms_aurora` succeeded, including fullscreen and
  resource lifetime changes. The CMake configuration also succeeded.
- A synchronization-only Direct3D 12 run prepared all 4,843 pipelines in
  128.39 seconds and then reproduced the original access violation at the same
  Dawn offset (`0x4f550`), through GX texture `SetBindGroup`. Synchronization
  alone therefore did not resolve the reported crash.
- Resource lifetime correction: a Direct3D 12 run on the same Intel adapter,
  with `SMS_AURORA_VALIDATE=1` and `SMS_WINDOW_MODE=fullscreen`, completed all
  4,843 pipeline preparations in 170.16 seconds and remained running and
  responsive after shader preparation. No matching access violation, missing
  bind-group assertion or Dawn validation error was present at that checkpoint.
  This is a limited startup observation, not a full gameplay certification.
- Fullscreen shortcut behavior: the user confirmed both F11 transitions
  (fullscreen to windowed and back to fullscreen) work in the corrected run.
- Local deployment: the same tested DLLs were installed into CubeShelf's
  Sunshine runtime, with the previous DLLs and runtime state backed up.
  `local-patch.json` records file hashes and the backup location. The runtime
  is marked locally modified (`Verified=false`); no new official release is
  claimed. Diagnostic environment overrides were limited to the test process.
- No automated test suite was run.

Do not attribute the original crash conclusively to the synchronization defect
until the corrected Direct3D 12 runtime has been exercised. The stack identifies
the failing GPU call; it does not by itself prove the preceding race.
