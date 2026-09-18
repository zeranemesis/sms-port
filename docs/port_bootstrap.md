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
undefined at link time. `docs/recompilation.md` chose the way past that -
static recompilation of the original PowerPC binary - and that toolchain is
now partially wired in (see "Recompilation pipeline" below), but there is
still no generated game code to link: that requires an actual GMSP01 dump,
which nobody running this in CI or this sandbox has. `CMakeLists.txt` and
`files.cmake` only list the port/menu layer (`PORT_FILES`); adding
generated/recompiled sources once they exist is the next step.

## Recompilation pipeline (DolRecomp, no ModernGekko)

`extern/dolrecomp` (`ExpansionPak/DolRecomp`, pinned submodule) is now
vendored and wired into `CMakeLists.txt` (`add_subdirectory(extern/dolrecomp
EXCLUDE_FROM_ALL)`, `DOLRECOMP_ENABLE_LLVM` left off). This is a real,
locally-verified change, unlike the Aurora side of this build: DolRecomp's
plain `--backend c` has no network-fetched dependencies (no zlib/LLVM
required), so `cmake -S extern/dolrecomp -B build && cmake --build build
--target dolrecomp` was actually run and succeeded in this sandbox.

`docs/recompilation.md` had already flagged that the LLVM backend ModernGekko
needs "cannot be fixed locally" on this project's own Windows setup (DIA SDK
path issue) - and separately, that ModernGekko brings its own Dolphin-derived
video/audio/HLE runtime, which is not "tout sous Aurora." **This port does
not use ModernGekko.** Instead, `src/port/recomp_host.cpp` (new,
`include/port/recomp_host.h`) hooks DolRecomp's plain-C-backend contract
directly: generated functions are `void func_<address>(CPUState* ctx)`
operating on a register file DolRecomp's own `dr_cpu` library provides, and
`CPUState::host_call` fires whenever generated code jumps to an address
DolRecomp didn't translate - which is exactly what happens at a Dolphin SDK
call (`GXSetVtxDesc`, `PADRead`, `OSReport`, ...) identified by its address
from the game's MAP file. `recomp_host.cpp` is the dispatch table for that
hook, meant to redirect those addresses into Aurora's own GX/PAD/OS/VI
implementations - the same ones Marioparty4's decompiled C already calls.

No entries are registered in that table yet: they're keyed by runtime
address, and no address exists without running `dolrecomp` against a real
DOL and MAP (`tools/port/recompile.py` wraps that invocation once you have
one). What **is** verified, with no disc image needed - `dr_cpu` has none
either - is the mechanism itself: `sms::recomp::run_self_test()`
(`--recomp-hostcall-self-test`) builds a real `CPUState` via DolRecomp's own
`cpu_init`, installs the dispatcher, and exercises both the hit path (a
registered entry runs and sees the right register state) and the miss path
(an unregistered address is logged with the calling PC rather than crashing).
This was compiled and run standalone against real `dr_cpu` object code in
this sandbox (with the logging call swapped for a stub, since `aurora::Module`
needs the full Aurora dependency chain this sandbox can't fetch) and passed.
`dr_cpu` itself needs no `extern "C"` guard when built as C - but its
headers have none for C++ callers, unlike Aurora's `dolphin/*.h`, so
`recomp_host.h` wraps the include itself; skipping that produces
C++-name-mangled references to `dr_cpu`'s plain-C symbols and fails to link.

### A first real trampoline: OSReport

`src/port/recomp_dolphin_sdk.cpp`/`include/port/recomp_dolphin_sdk.h` add
the first actual Dolphin SDK bridge, `OSReport`. It's a reasonable first
target for two reasons: reading a NUL-terminated string from guest memory
has no endianness to get wrong (bytes are bytes), and DolRecomp's CPUState
register file already holds scalar arguments as correctly-decoded
host-native values (per the "Contrat d'exécution de DolRecomp" section of
`docs/recompilation.md`), so there's no struct layout to guess at either.
It implements a real, if partial, printf-style substitution
(`%s`/`%d`/`%i`/`%u`/`%x`/`%X`/`%c`/`%%`, reading each variadic argument
from `r4`-`r10` per the PowerPC EABI - `%f`/`%g` would need the separately-
tracked FPR file and aren't handled) and logs the formatted string through
`aurora::Module`.

**Deliberately not attempted yet:** `PADRead`/`PADInit`. Their `PADStatus*`
argument points at guest memory laid out per the *original* GameCube ABI,
which is not byte-identical to Aurora's own `PADStatus` (it has a
`TARGET_PC`-only `extButton` field, confirmed by reading
`extern/aurora/include/dolphin/pad.h`) - and nothing here has been checked
against a real disc's actual in-memory layout. `docs/recompilation.md`'s own
portability audit already names exactly this class of bug (endianness/
layout mismatches at a struct boundary) as the port's most serious silent-
corruption risk. Guessing at that marshalling without a real map/dol to
test it against would be writing precisely the kind of bug that audit
warns about, so `PADRead`/`PADInit` calls stay as unresolved (logged, not
crashed) host calls until there's something real to check the layout
against.

`register_known_dolphin_sdk_calls()` takes `{name, address}` pairs (still
nothing to populate them with, same as `recomp_host.cpp`'s table) and wires
matching trampolines - currently just `"OSReport"` - into the dispatch
table by name, so filling in real addresses later is a data problem, not a
code change.

**Verified the same way as the host-call mechanism itself:**
`format_os_report()` was compiled and linked against real `dr_cpu` object
code in this sandbox and its self-test
(`sms::recomp::dolphin_sdk::run_dolphin_sdk_self_test()`,
`--recomp-dolphin-sdk-self-test`) passed: given a guest format string
`"Hello %s, %d/%u/%x/%%!"` and register arguments, it produces exactly
`"Hello world, -5/42/beef/%!"`.

## Aurora submodule patches

`extern/aurora` is pinned to the same commit Marioparty4 uses
(`5143394...`), with three **uncommitted, working-tree** patches applied
locally, exactly like Marioparty4 keeps `extern/aurora`/`extern/musyx`:

- `patches/aurora-render-fixes.patch` - copied unchanged from Marioparty4
  (GX texture/palette correctness fixes; game-agnostic).
- `patches/aurora-port-fixes.patch` - the generic subset of Marioparty4's
  `aurora-partyboard.patch`: the WebGPU present-mode fix (prefer Immediate
  over Mailbox when VSync is off) and the GameCube-adapter SDL/HIDAPI hints
  in `lib/window.cpp`. The rest of that patch (a `PartyBoard_`-prefixed
  test-automation input bridge reading `MarioPartyRD\Party Board\...`
  paths) is Party-Board-specific test harness code and was **not** ported.
- `patches/aurora-dvd-os-link.patch` - two related fixes needed to link any
  `AURORA_ENABLE_DVD` consumer at all, verified building `dolphinjet`, both
  upstream Aurora gaps rather than anything port-specific:
  - `lib/dolphin/os/OSReport.cpp` - `OSReport`/`OSVReport`/`OSPanic`/
    `OSFatal` are fully written but sit inside `#if 0`/`#endif`, so nothing
    in Aurora defines them at all. `dolphin/os.h` declares them
    `DECL_WEAK` (`__declspec(weak)` on MSVC), which apparently doesn't
    tolerate a truly absent definition the way GCC/Clang weak symbols do:
    the real failure was `LNK2019: unresolved external symbol OSReport`
    from this port's own `io.cpp`, and `OSPanic` from `aurora_dvd.lib`
    (see below) - not a warning, not a null-call-skipped weak reference.
    Un-`#if 0`-ing restores Aurora's own already-written implementation
    unchanged.
  - `cmake/aurora_dvd.cmake` - `aurora_dvd` never declared it needs
    `aurora_os` (`lib/dolphin/dvd/dvd.cpp` calls `OSPanic`). Declared here
    explicitly rather than relying on a consumer's own link order to
    happen to put `aurora::os` early enough - `CMakeLists.txt` also puts
    `aurora::os` right after `aurora::core` in `dolphinjet`'s own list for
    the same reason (`io.cpp` calls `OSReport` directly), belt-and-braces
    since the OSReport.cpp gap above was confounding earlier ordering
    experiments at the time these were written.

Because these are working-tree changes, not commits, CI must apply them at
checkout - add an "Apply port dependency patches" step to
`.github/workflows/*.yml` that runs, in order:

```bash
git -C extern/aurora apply --check ../../patches/aurora-port-fixes.patch
git -C extern/aurora apply ../../patches/aurora-port-fixes.patch
git -C extern/aurora apply --check ../../patches/aurora-render-fixes.patch
git -C extern/aurora apply ../../patches/aurora-render-fixes.patch
git -C extern/aurora apply --check ../../patches/aurora-dvd-os-link.patch
git -C extern/aurora apply ../../patches/aurora-dvd-os-link.patch
```

`git -C <dir> apply <patch>` resolves `<patch>` relative to `<dir>`, not to
the caller's cwd - verified on this Windows checkout, where `patches/...`
(no `../../`) fails with "can't open patch". `.gitattributes` also now pins
`*.patch` to `text eol=lf`: with `core.autocrlf=true` (the common Windows
default), a working-tree patch file gets CRLF-normalized, and `git apply`
then fails with "corrupt patch" because a blank context line becomes `\r`
instead of empty.

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

**Update - since resolved:** the `403` described below was specific to
that sandboxed session's outbound proxy, not this repository. Run from an
unrestricted machine, every one of those downloads succeeds, `dolphinjet`
builds and links (after the fixes in "Booted against a real GMSP01 dump"
above), and has actually been run against a real GMSP01 dump - see that
section for what that run found. The rest of this section is kept as-is
for the historical record of what was and wasn't checked at the time.

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
problem in this repository's build files: **the actual compile of the menu
layer against Aurora, and a real boot, are unverified.**

`extern/dolrecomp` is the exception: it has no network-fetched dependencies
with `DOLRECOMP_ENABLE_LLVM` off, so it was actually configured and built in
this sandbox (`cmake -S extern/dolrecomp -B build && cmake --build build
--target dolrecomp`, real compiler output, real `dolrecomp` binary, no
network needed beyond the initial submodule clone). `src/port/recomp_host.cpp`
was likewise compiled and linked against `dr_cpu`'s real object code
(`cpu.c` compiled with `gcc -std=c11`, `recomp_host.cpp` with `g++
-std=c++20`, linked together - matching how CMake already builds them as
separate C/C++ targets) and `run_self_test()` passed, with `aurora::Module`
swapped for a stub logger since exercising the real one needs the
Aurora dependency chain above. This is the one piece of Phase 2 genuinely
verified end to end in this session, not just written.

To actually verify the rest, on a machine with normal GitHub access:

```bash
git submodule update --init --recursive
git -C extern/aurora apply ../../patches/aurora-port-fixes.patch
git -C extern/aurora apply ../../patches/aurora-render-fixes.patch
git -C extern/aurora apply ../../patches/aurora-dvd-os-link.patch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

This should produce a `dolphinjet` executable that opens a window and shows
the empty Party-Board-style menu on F1 - there is no game to boot yet, so
that is the actual milestone, not a placeholder for one. No disc image is
needed for this; one *is* needed for the next phase (wiring in recompiled
or decompiled game code) as it always has been in `recompilation.md`.

Run `dolphinjet --recomp-hostcall-self-test` to check the host-call bridge
independently of the menu - it needs no disc image, and passing does not
depend on Aurora's rendering backends initializing correctly, only on
`dr_cpu` and the dispatch table in `recomp_host.cpp`. Run `dolphinjet
--recomp-dolphin-sdk-self-test` the same way to check the OSReport
trampoline's format-string substitution. Both are also run in
`port-build.yml` once the build succeeds.

## Booted against a real GMSP01 dump - what actually happened

Everything in "What actually needs a disc image from here" (below) was
speculative when written - no dump, no test. This section replaces it with
what was actually measured running `dolphinjet.exe` against a real,
legally-owned PAL dump, end to end, on Windows/MSVC. **This is a SCRIPTED,
automated run (a timed launch, log inspection) - nobody has looked at a
rendered frame or touched a controller. That distinction matters and should
not get blurred by how far execution got.**

1. `tools/port/recompile.py` had two real bugs, both now fixed: it looked
   for `dolrecomp.exe` in the wrong place under a multi-config generator
   (Visual Studio), and it flattened `dolrecomp`'s own
   `generated/chunks/*.c` output in a way that broke every chunk's
   `#include "../generated.h"`. Re-running it against the real dump now
   reliably reproduces `docs/recompilation.md`'s numbers exactly: 890,680
   instructions, 0 unknown, 219 chunks.
2. The Aurora dependency download that blocked the previous (sandboxed)
   session is confirmed to be a sandbox network-policy limitation, not
   anything in this repository - it just works from an unrestricted
   network. `patches/aurora-dvd-os-link.patch` fixes two real upstream
   Aurora gaps found getting `dolphinjet` to actually link (see "Aurora
   submodule patches" above): `OSReport`/`OSPanic` are fully written but
   sit inside `#if 0`, and `aurora_dvd` never declared it needs
   `aurora_os`.
3. `src/port/portmain.cpp`'s `DOLPHINJET_HAVE_RECOMPILED_GAME` block is no
   longer a comment - `include/port/recomp_boot.h` / `src/port/recomp_boot.cpp`
   do exactly what it described: open the disc image from
   `Settings.backend.discPath` via `aurora_dvd_open`/`DVDInit`, pull the
   DOL directly from `DVDGetDOLLocation` (no `sys/main.dol` path lookup
   needed - `nod` already read it off the mounted image), load its
   text/data sections into a fresh `CPUState`'s guest RAM by hand (GameCube
   DOL header format, verified against `dr_frontend`'s own parser rather
   than reused from it, since that parser reads from a file path and this
   needs to read an in-memory buffer), install the host-call bridge, and
   set `pc` to `DOLRECOMP_ENTRY_POINT`. Confirmed: the loaded DOL size
   printed at boot (4,094,112 bytes) matches `dtk`'s own disc-info output
   exactly.
4. `generated/generated.h`'s chassis (`dolrecomp_find_original`,
   `dolrecomp_call`, `dolrecomp_run_blocks`) is real, generated, working
   code - not something this port needed to invent. `recomp_boot.cpp`
   drives it with a per-frame block budget
   (`kGameBlocksPerFrame` in `portmain.cpp`, currently an unmeasured
   starting guess, not a tuned constant).
5. Three real `dr_cpu` (DolRecomp's CPU runtime) gaps had to be closed
   before execution got anywhere, each found by running into it, not by
   guessing ahead of time - all in `recomp_boot.cpp`:
   - `mfhid0`/other `mfspr`/`mtspr` accesses to implementation-specific
     SPRs (HID0 etc. - cache/perf/thermal config registers with no
     software-visible effect worth modeling) aren't implemented in
     `dr_cpu`'s C backend and fall to `CPUState::instruction_fallback`,
     unset by default, which is a `PPC_PROGRAM_ILLEGAL` exception with
     nowhere to jump. Handled by decoding just enough of the raw
     instruction to treat any unmodeled SPR as a plain read/write-back
     storage cell.
   - `dcbf`/`dcbst`/`dcbi`/`dcbt`/`dcbtst`/`icbi` (cache-management hints)
     hit the same fallback path instead of `ppc_cache_control` (whose
     *default* behavior, with no `cache_control` callback installed, is
     already a safe no-op - verified in `cpu.c`). Now no-op'd directly.
     `dcbz` is different - it actually zeroes memory games can observe -
     and is implemented for real (8x `mem_write32`).
   - `sc` (system call) raises `PPC_EXC_SYSTEM_CALL` unconditionally, by
     PowerPC design; on real hardware, IPL/BS2 install a handler at its
     vector (0xC00) before the game ever runs, and the DOL doesn't
     include IPL code. Confirmed hitting this for real inside
     `DCFlushRange` (address falls exactly inside its
     `DOLRECOMP_SYMBOL_DCFlushRange`/`_SIZE_` range) - a GameCube SDK
     cache routine, not application code expecting a real syscall ABI,
     consistent with `sc` here being the documented "debugger trap point"
     convention. **Registering a handler for this through
     `recomp_host.h`'s normal host-call table does not work** -
     `dolrecomp_run_blocks` (`generated.h`) stops as soon as
     `CPUState::exception` is non-zero, *before* ever dispatching to the
     vector address it just set `pc` to, so the table is never consulted
     for it. `step_game()` handles it directly instead: clear
     `cpu->exception`, bump `srr0` past the 4-byte `sc` (real PPC
     convention - `ppc_rfi` returns to exactly `srr0`, so without this a
     handler loops on the same instruction forever), call `ppc_rfi`, retry.
6. **Execution now runs for a long time** - tens of millions of
   `dolrecomp_run_blocks` iterations across many seconds of wall-clock
   CPU work, not the handful of instructions before item 5's fixes - real
   GameCube OS/init code, not just the crt0 prologue.
7. **Current, concrete blocker**: sustained reads from `0xCC00500A` and
   nearby addresses. `0xCC0050xx` is GameCube EXI hardware register space
   (External Interface - memory card/GBA-link/serial bus), not part of
   the translated DOL, and not anything `host_call` or
   `instruction_fallback` intercepts either - these are plain `lhz`/`lwz`
   loads dr_cpu executes inline, which fall through `mem_read16`'s
   `resolve_addr` miss path straight to logging "unmapped" and returning
   0 unconditionally (verified in `cpu.c`). The real game code is almost
   certainly polling an EXI status/ready bit (memory card or controller
   pak detection is the standard GameCube boot-time culprit) that a
   constant 0 can never satisfy, hence the sustained loop rather than a
   crash. `CPUState::external_read`/`external_read32`
   (`extern/dolrecomp/src/cpu/cpu.h`) is the hook `mem_read16` already
   checks before falling back to the unmapped-warning path - the
   mechanism to use is not in question. What *is* still needed and was
   not attempted: the actual EXI register map and the specific bit
   pattern that reads as "channel present, no card/device attached" -
   real GameCube hardware documentation, not a guess from this session.
8. Audio still has no answer on any route - JAudio2 has no direct
   hardware calls to bridge (per the audit in `README.port.md`), so it
   needs its own design regardless of how far the EXI blocker above gets
   resolved.

## What actually needs a disc image from here (superseded, kept for history)

Written before any real GMSP01 dump was available in this environment -
every numbered item here has since either been done (see the section
above) or turned out to need something more specific than it anticipated.

1. Run `tools/port/recompile.py` against it to produce `generated/*.c` and
   `generated/generated_symbols.h`. `CMakeLists.txt` already globs
   `generated/*.c` into a `game_recompiled` target automatically when
   present (and defines `DOLPHINJET_HAVE_RECOMPILED_GAME` for
   `dolphinjet`) - nothing to edit there.
2. In `src/port/portmain.cpp`, the `#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME`
   block says exactly what's next: build a `CPUState`, install the
   host-call bridge, bind `sms::recomp::dolphin_sdk`'s trampolines to their
   real addresses from `generated_symbols.h`'s `DOLRECOMP_SYMBOL_*`
   constants, and drive the entry point. Left as a comment rather than
   code because the entry symbol and the actual set of `DOLRECOMP_SYMBOL_*`
   names are specific to that dump and unknowable in advance.
3. More trampolines in `recomp_dolphin_sdk.cpp` as real host-call misses
   get logged at step 2 - `PADRead`/`PADInit` in particular need their
   guest-memory `PADStatus` layout checked against that real dump first
   (see "A first real trampoline: OSReport" above for why).
4. Audio has no answer yet on any route - JAudio2 has no direct hardware
   calls to bridge the way GX/PAD/OS do (per the audit in
   `README.port.md`), so it needs its own design, not just more
   trampolines.

## EXI/CARD/GX bridging session - what was actually found running it

Follow-up to "Booted against a real GMSP01 dump" above. That session's
blocker (sustained reads from GameCube EXI hardware registers, `0xCC0050xx`)
turned out to need more than one fix, discovered the same way as before -
by running the real thing and reading the calling PC against
`generated/generated_symbols.h`, never by guessing ahead of a real run.

**What shipped**: `include/port/recomp_host.h`'s `MmioRangeHandler` table
(address-*range*-keyed, parallel to the existing address-*exact*-keyed
host-call table) plus `include/port/recomp_exi.h`, `recomp_card.h`,
`recomp_gx_fifo.h` (and their `.cpp`s) bridging EXI probes, CARD (a real,
working bridge to Aurora's GCI-folder-backed `card.cpp`), and the GX
write-gather-pipe (`external_write` on `[0xCC008000, 0xCC009000)` forwards
straight into `aurora::gx::fifo::write_u8/u16/u32` - Aurora's own,
already-implemented GX command decoder, not reimplemented here) plus its
control-plane calls (`GXInit`/`GXSetCPUFifo`/`GXSetGPFifo`/`GXSetDrawDone`/
`GXDrawDone`/`GXFlush`/`GXCopyDisp`). `--recomp-mmio-self-test` exercises
the new table without a disc image, same as the two pre-existing self-tests.

**Two real bugs found and fixed, both more general than EXI/CARD/GX**:

1. A no-op `EXIInit` stub (the obvious first guess) caused an *infinite
   loop calling EXIInit itself* in a real run - real `EXIInit` almost
   certainly writes an "initialized" flag into guest memory that other
   guest code polls afterward, and a stub with no guest-memory side effect
   can never satisfy that. Fixed by *not* bridging `EXIInit` at all and
   letting its own fully-translated body run instead (dolrecomp translated
   the whole DOL - there's a real implementation to fall back to, no need
   to guess at one).
2. `CPUState::timebase` (`extern/dolrecomp/src/cpu/cpu.h`) never advances
   on its own - only `mtspr`/`mftb` touch it at all. Any "wait N ticks"
   loop (found via `_OSInitAudioSystem` calling `OSGetTick()` in a tight
   loop that never terminated) spins forever with a frozen timebase,
   regardless of how much real wall-clock time passes. Fixed generally,
   not just for audio: `src/port/recomp_boot.cpp`'s `advance_timebase()`
   now advances it once per `step_game()` call by real elapsed wall-clock
   time, converted at `OS_TIMER_CLOCK` (40,500,000 Hz - not a guess, this
   codebase's own `include/dolphin/os.h`/`extern/aurora/include/dolphin/
   os.h` both define it as `OS_BUS_CLOCK/4` with `OS_BUS_CLOCK =
   162,000,000`). This should matter well beyond audio - anything using
   `OSGetTick`/`OSGetTime`-based delays anywhere in the game had the same
   problem before this fix.

**One hardware-register hypothesis corrected against real data**: the
`0xCC00500A` hang from the previous session is not EXI at all - the
logged PC lands inside `_OSInitAudioSystem`'s own symbol range
(`DOLRECOMP_SYMBOL__OSInitAudioSystem 0x8033B534`, size `0x1BC`), i.e. it
is the GameCube **DSP interface** (DSPCR and neighbors), polled during
audio bring-up. The real sequence, read off an actual run: write `0x8ac`,
read it back, write `0x8ad` (only bit 0 changes, 0->1), then spin reading
the same value forever - the standard "write a self-clearing reset bit,
poll until hardware clears it" pattern. `recomp_exi.cpp`'s shadow register
file (kept in that file/module for now rather than renamed mid-session)
now always reports bit 0 as clear on read, simulating an instantaneous
reset - this alone resolved the hang.

**Current, more precisely located blocker**: past both fixes above, the
same `_OSInitAudioSystem` now spins reading `0xCC005004` (a *different*
offset in the same DSP register block - almost certainly a CPU<->DSP
mailbox register) at `pc=0x8033b688`, always getting back `0`. This reads
as a real DSP firmware-upload/ready handshake - the kind of thing that
needs an actual (even if minimal) DSP core or HLE to answer correctly, not
a shadow-memory bit trick. This is the same "audio has no answer on any
route" gap already flagged in `README.port.md` and the original "What
actually needs a disc image from here" list below, now pinned to an exact
address and calling function rather than a general statement. **GX and
CARD bridging are wired and self-test clean, but have not been exercised
against a real run yet** - boot has not reached any GX or CARD call site
so far; both stay unverified beyond the self-tests until audio/DSP clears
enough for boot to reach that code. Every run in this session remains
SCRIPTED-level verification only (a timed launch, log inspection) - no
rendered frame has been seen.

### DSP mailbox: one step further, then the real wall

Following up on the blocker above (`0xCC005004` always reading `0`): the
GameCube DSP interface's mailbox registers are publicly documented as
`0xCC005000`/`0xCC005002` (CPU->DSP, high/low) and `0xCC005004`/`0xCC005006`
(DSP->CPU, high/low), with bit 15 of the high half conventionally meaning
"a response is ready" - `recomp_exi.cpp` now always reports that bit set on
reads of `0xCC005004`, leaving the actual response content (the low half,
`0xCC005006`, and the rest of the high half) at whatever was never written -
effectively `0`.

**Real run result**: this did move the hang - past the "wait for a
response" wait, to a *new* one a few instructions later, still inside
`_OSInitAudioSystem`, reading both mailbox halves in a loop
(`0xCC005004` -> `0x8000`, `0xCC005006` -> `0x0`, repeating). This reads
as validating the response's actual *content* against an expected value
(a DSP-firmware boot-acknowledgment code, most likely) - not just its
presence. That value is not known here (it would need either a real
hardware capture or the DSP IROM's own disassembly, neither available in
this session), and guessing at it would be exactly the kind of fix "based
on a plausible mechanism" this project's own methodology rules out. This
is the real, precise boundary of "audio has no answer on any route" -
not a vague statement anymore, but the literal missing piece.

## Breakthrough: past the DSP wall, using real Dolphin emulator source

Follow-up to the DSP mailbox section above. Real, freely-available reference
material was found and used - not guessed at - closing the gap that section
left open:

- **Dolphin emulator's own source (github.com/dolphin-emu/dolphin) documents
  the exact GameCube hardware this session was reverse-engineering from
  behavior alone.** `Source/Core/Core/HW/DSP.h`'s `UDSPControl` confirmed
  DSPReset is bit 0 (validating the earlier DSPCR fix after the fact) and
  named DSPHalt as bit 2. `Source/Core/Core/HW/DSPHLE/UCodes/INIT.cpp`
  gives the real DSP audio-init boot acknowledgment value Dolphin's HLE
  sends real games to satisfy their `_OSInitAudioSystem`-equivalent code:
  **`0x80544348`**. Using it (`recomp_exi.cpp`'s DSP mailbox read) plus
  simulating a *second*, DSP-self-initiated ready message once DSPHalt
  gets cleared (a real two-stage handshake, confirmed by reading the
  generated code directly - see the file's own comments) **cleared the
  DSP wall entirely** - `_OSInitAudioSystem` completes and boot proceeds
  well past it, into other real hardware.
- The same technique (fetch Dolphin's real register-layout headers rather
  than guess) was applied to two more hardware blocks discovered right
  after: **EXI** (`Source/Core/Core/HW/EXI/EXI_Channel.h` - base
  `0xCC006800`, 3 channels 0x14 bytes apart, status/control register
  layout with a self-clearing TSTART bit) and **DI, the Disc Interface**
  (`Source/Core/Core/HW/DVD/DVDInterface.h` - base `0xCC006000`, DISR/
  DICVR/DICR layout, also a self-clearing TSTART bit). Both are now
  handled in `recomp_exi.cpp` (kept in that file/module despite the name
  history - see its own header comment) with the real bit layout, not a
  guessed one.
- `DVDInit` itself was tried as a `host_call` bridge to Aurora's own real
  (no-op) `DVDInit()` - same pattern as `CARDInit`. It IS reached through
  a genuine cross-chunk call boundary (unlike `EXIInit`/
  `_OSInitAudioSystem`), so the bridge fires, but its caller then called
  it again in an immediate retry loop - the same "a no-op stub doesn't set
  the guest-side state the caller polls for" lesson as `EXIInit`. Reverted;
  `DVDInit`'s own translated body runs for real instead, which is what
  actually reaches the EXI/DI registers above.

**Current, new, more precise blocker**: past EXI and DI's completion
signaling, `DVDInit`'s caller loops calling `OSReport("bootrom")` -
almost certainly a disc/BS2-bootrom-version check reading an actual value
back from the disc via DI's immediate-data register (`DIIMMBUF`,
`recomp_exi.cpp`'s DI range +0x20), which this session's DI shadow always
reports as `0` since nothing here has produced a real disc-hardware
response value for it. Same category of gap as the DSP mailbox's response
*content* (as opposed to just its "ready" signal) - a specific expected
value neither guessed at nor found in the research done so far. GX/CARD
bridging (see the section above) remains wired, self-test clean, and
still unexercised by a real run - boot has not reached that code, now
for this new, different reason instead of the DSP one.
