# RetroAchievements native PAL integration

The menu uses the official rcheevos v12.5.0 `rc_client`, not local substitute
achievements. Login is an asynchronous HTTPS request through WinHTTP; callbacks
are delivered on the game thread by `pump()`. Passwords and returned session
tokens are never written to settings or disk. Logging out invalidates pending
callbacks. The API response is bounded to 16 MiB; credential requests never
follow redirects or use plaintext HTTP.

The official client identifies the selected disc, then checks that the returned
game is **6049**. ISO/CISO are read through the native GCDisc API and hashed by
rcheevos. Extracted PAL folders use boot.bin, bi2.bin, apploader.img and main.dol
to reconstruct the bytes used by the same official hash algorithm. This path
is accepted only when its computed hash equals the published original PAL
identifier `58d598ccfa63fb89b81e8ead70f2dbdb`. Other extracted revisions or
modified executable images are rejected rather than impersonated.

Sources:
- https://retroachievements.org/game/6049
- https://retroachievements.org/game/6049/hashes
- https://github.com/RetroAchievements/rcheevos/tree/v12.5.0

## Current supported behavior and remaining work

Account login, official game identification, server achievement descriptions
and existing account unlocks are implemented. **New unlock evaluation is not
implemented.** The native port does not preserve GameCube addresses, object
layouts, pointer sizes or big-endian state. Its arena beginning at 0x80000000
must never be treated as an emulator memory image. No validated translator
for the set's console-state conditions exists yet. `read_memory()` returns a
failed read (zero bytes) for these unsupported addresses; the client remains
in spectator mode, `game_tick()` does not evaluate or submit achievements.
The menu must display this limitation explicitly.

Implementing unlocks requires obtaining the authentic condition definitions,
mapping their PAL console addresses to live native game fields (including
indirect pointers), reconstructing big-endian console bytes with pinned field
offsets, then validating each achievement condition against original gameplay.
Only after that may `game_tick()` call `rc_client_do_frame()` once per logic
tick and spectator mode be removed. Display/presentation ticks are insufficient.
Hardcore requires separate client approval and must remain disabled.

Root integration: include `cmake/retroachievements.cmake`, link
`sms_retroachievements`, exclude its C++ files from the platform source glob,
call `initialize()/pump()/shutdown()` on the UI thread, and feed credentials
from transient password inputs to `login()`. Clear the password input
immediately after submitting. `set_disc_path()` supports changing the selected
source; `SMS_DISC_IMAGE` then `SMS_DISC_ROOT` are used at initialization.
