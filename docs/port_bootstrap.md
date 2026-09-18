# Aurora port bootstrap (DolphinJet)

What exists now, on top of `README.port.md` and `docs/recompilation.md`: a
CMake build alongside the existing `configure.py`/`ninja` decomp workflow
(which is untouched and still tracks matching progress), bringing up an
Aurora window with a Party-Board-style (Marioparty4) menu shell. **No SMS
game code is linked in yet** - this is the platform/menu layer the
recompiled and/or decompiled game code plugs into next.

## What this is modeled on

`zeranemesis/Marioparty4` ("Party Board") is a mature Aurora-based decomp
port with a full RmlUi menu (Settings/Achievements/Play Online/Quit, tabbed
options, controller config, graphics tuner). Its `src/port/ui/` is
overwhelmingly generic - RmlUi/SDL glue with no Mario Party game-state
coupling - so most of it is ported here near-verbatim (namespace
`partyboard::ui` -> `sms::ui`): `document`, `component`, `window`, `pane`,
`tab_bar`, the button family, `modal`, `event`, `nav_types`, `input`,
`overlay`, `graphics_tuner`, `controller_config`, and the core `ui`
subsystem. `menu_bar` and `settings` are rewritten for SMS's (currently much
smaller) settings surface rather than copied, since Marioparty4's versions
are full of its own game/achievements/netplay/cheats hooks. Dropped
entirely for now: achievements, the standalone ISO-picker `Prelaunch`
document (folded into a plain "Prelaunch" Settings tab with a text field
instead - see below), and anything netplay-related.

## Why the game code isn't linked in yet

Per `README.port.md`, SMS decomp is far less complete than Marioparty4's
(≈18% vs. Marioparty4 being close to fully matched), so directly compiling
`src/game` the way Marioparty4's CMake does would leave ~2,489 functions
undefined at link time. `docs/recompilation.md` already chose the way past
that - static recompilation of the original PowerPC binary (DolRecomp +
ModernGekko), hybridized with matched decomp code over time - but vendoring
that toolchain (DolRecomp, ModernGekko, its LLVM 19/20 backend) and wiring
its generated C output into this CMake build is a separate, substantial
piece of work, not attempted in this pass. `CMakeLists.txt` and
`files.cmake` only list the port/menu layer (`PORT_FILES`); adding
`GAME_FILES`/recompiled sources is the next phase.

## Aurora submodule patches

`extern/aurora` is pinned to the same commit Marioparty4 uses
(`5143394...`), with two **uncommitted, working-tree** patches applied
locally, exactly like Marioparty4 keeps `extern/aurora`/`extern/musyx`:

- `patches/aurora-render-fixes.patch` - copied unchanged from Marioparty4
  (GX texture/palette correctness fixes; game-agnostic).
- `patches/aurora-port-fixes.patch` - the generic subset of Marioparty4's
  `aurora-partyboard.patch`: the WebGPU present-mode fix (prefer Immediate
  over Mailbox when VSync is off) and the GameCube-adapter SDL/HIDAPI hints
  in `lib/window.cpp`. The rest of that patch (a `PartyBoard_`-prefixed
  test-automation input bridge reading `MarioPartyRD\Party Board\...`
  paths) is Party-Board-specific test harness code and was **not** ported.

Because these are working-tree changes, not commits, CI must apply them at
checkout - add an "Apply port dependency patches" step to
`.github/workflows/*.yml` that runs, in order:

```bash
git -C extern/aurora apply --check patches/aurora-port-fixes.patch
git -C extern/aurora apply patches/aurora-port-fixes.patch
git -C extern/aurora apply --check patches/aurora-render-fixes.patch
git -C extern/aurora apply patches/aurora-render-fixes.patch
```

before configuring, exactly as Marioparty4's `build.yml` does. This
repository does not have that CI step wired up yet.

## Assets

`res/` carries only generic, redistributable fonts (Inter, Alegreya SC,
Noto Mono, Material Symbols Rounded - all OFL/Apache-licensed) and the
RmlUi stylesheets (`res/rml/*.rcss`) needed by the menu, copied from
Marioparty4 with its Mario-Party-branded font (`N64 Party`) and its
commercial Japanese font (`FOT-NewRodin Pro`) both replaced by Inter.
Marioparty4's actual brand assets (`icon.png`, `logo.png`,
`prelaunch-bg.png`, the N64 Party and FOT-NewRodin font files) were not
copied at all.

## What the menu currently has

F1 (or the gamepad Menu button) opens the popup with two tabs: **Settings**
and **Quit**. Settings has:

- **Prelaunch**: Disc Image (a plain text field for the GMSP01 image path -
  no file picker or hash verification yet, unlike Marioparty4's
  `iso_validate`-backed picker), Language (English/French, with a small
  `ui_translate` table for the strings this menu actually uses), Graphics
  Backend.
- **Video**: fullscreen/window size, VSync, frame rate target, 4:3 lock /
  adaptive widescreen, pause-on-focus-lost, FPS counter, internal
  resolution and shadow resolution tuners.
- **Input**: Configure Controller (opens the full binding/rumble-test
  window, ported unchanged from Marioparty4), Allow Background Input.

Settings persist to `config.json` under the OS preferences directory
(`SDL_GetPrefPath("dolphinjet", "DolphinJet")`) via the same layered `ConfigVar`
system Marioparty4 uses (`include/port/config_var.hpp`, `config.hpp`,
`src/port/config.cpp` - ported near-verbatim, only the CVar map's container
changed from Abseil to `std::unordered_map` since nothing here depends on
its performance characteristics).

## Verification

**What was checked in this sandbox:** `cmake -S . -B build` parses this
repository's own `CMakeLists.txt`/`files.cmake` correctly, resolves
`nlohmann_json` via `FetchContent`, and correctly hands off into
`extern/aurora`'s own CMake (`AURORA_ENABLE_RMLUI`/`AURORA_ENABLE_DVD`
options take effect, `aurora::*` targets resolve). It then fails
**downloading Aurora's own transitive dependencies** (abseil-cpp, and
before that a prebuilt Dawn package) - GitHub's archive/release download
endpoints return `403` through this session's outbound proxy, even though
plain `git clone`/`fetch` over HTTPS to `github.com` (used for the
submodules) works fine. That is a sandbox network-policy limitation, not a
problem in this repository's build files: **the actual compile of this
port layer's ~30 files, and a real boot, are unverified.**

To actually verify, on a machine with normal GitHub access:

```bash
git submodule update --init --recursive
git -C extern/aurora apply patches/aurora-port-fixes.patch
git -C extern/aurora apply patches/aurora-render-fixes.patch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

This should produce a `dolphinjet` executable that opens a window and shows
the empty Party-Board-style menu on F1 - there is no game to boot yet, so
that is the actual milestone, not a placeholder for one. No disc image is
needed for this; one *is* needed for the next phase (wiring in recompiled
or decompiled game code) as it always has been in `recompilation.md`.
