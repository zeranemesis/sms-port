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
git -C extern/aurora apply --check ../../patches/aurora-fifo-stream.patch
git -C extern/aurora apply ../../patches/aurora-fifo-stream.patch
git -C extern/aurora apply --check ../../patches/aurora-gx-diagnostics.patch
git -C extern/aurora apply ../../patches/aurora-gx-diagnostics.patch
```

Two more have been added since that list was first written:

- `patches/aurora-fifo-stream.patch` - `lib/gx/fifo.cpp` and
  `lib/gx/command_processor.hpp`: `drain()` carries an incomplete trailing
  command over to the next buffer instead of discarding it, because this port
  cuts the FIFO mid-command by construction (see the FIFO section below).
- `patches/aurora-gx-diagnostics.patch` - the GX/THP measurement probes, and
  now also the command processor's half of the same FIFO fix. The fix and the
  probes share `lib/gx/command_processor.cpp`, and these patches are per-file
  diffs, so they cannot be separated without hand-splitting hunks; the split
  is by file, not by purpose.

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

## 2026-09-19: `dolphinjet.exe` actually run for the first time on this machine, two runaway-log bugs found and fixed

The build in `build-msvc/RelWithDebInfo/dolphinjet.exe` (compiled by an
earlier session, still current against today's source) was launched for the
first time this session, in response to a direct request to build on the
already-ported Party-Board-style menu. **It already works**: `MenuBar`'s tab
bar (Settings/Quit) opens on F1, a real toast notification fires
("NO CONTROLLER ASSIGNED - Configure controller port 1 in Settings"), and
activating the Settings tab (Enter/click, not just keyboard focus - the tab
bar highlighting a tab and *activating* it are different things, which cost
a false "the panel is blank" diagnosis at first) opens a real, fully
populated `SettingsWindow`: Fullscreen, Default Window Size, VSync, target
frame rate, Lock 4:3, Adaptive Widescreen HUD, Pause on Focus Lost, FPS
counter, Internal Resolution (Auto 1280x960), Shadow Resolution - screenshot
evidence, not just reading the code. This whole shell is confirmed
functional end to end, not just "should build."

**What running it also showed**: `game_recompiled.lib` (DolRecomp's own
recompilation of the real GMSP01 dump, built by an earlier session,
separate from and unrelated to the ModernGekko-Template work elsewhere in
this session) is linked into `dolphinjet.exe` and starts executing
immediately on launch - `recomp_boot.cpp`'s boot path fires for real, not
gated behind any menu action yet. It hits exactly the `OSReport("bootrom")`
polling loop documented right above as the current blocker.

**Two real bugs, not one**, both in unconditional per-call logging with no
rate limit, found because they filled disk fast enough to matter:
- `recomp_host.cpp`'s `dispatch()` logged every *miss* on an unresolved
  host-call address - not every distinct address, every single hit. Against
  the stuck bootrom loop (thousands of calls/sec at a handful of PCs) this
  produced a 1.4GB log file in under two minutes. Fixed: an
  `std::unordered_set<u32>` dedupes by address, so each one is still logged
  exactly once (that's the actionable signal - which `HostCallEntry` to add
  next), just not once per occurrence.
- `recomp_dolphin_sdk.cpp`'s `host_call_os_report()` had the same shape:
  every `OSReport` call logged unconditionally. The stuck loop calls it with
  the *same* text from the *same* call site continuously - confirmed by
  sampling the log mid-file (`grep` at the head looked fine; the real spam,
  15.5 million lines of it, was further in). Fixed the same way, deduping on
  `(message, pc)` so a call site whose message actually changes each time
  (a frame counter, say) still logs every distinct value, only exact
  repeats are suppressed.

Both fixes verified by rebuilding and rerunning: log growth went from
~470MB/15s to a flat 53KB that stayed flat over a further 15s. Neither fix
touches *why* the bootrom loop is stuck - that's still the DI content gap
described above - only how loudly it fails while stuck.

**Unrelated but urgent, found while investigating the first log file**: the
machine's `C:` drive was at 470GB/476GB used, 1.9GB free, before any of this
session's own files were cleaned up - a pre-existing condition, not caused
by this session, but close enough to full that the runaway log almost made
it worse. Cleaned up ~4.9GB of this session's own reproducible scratch data
(the downloaded LLVM archive and its extracted tree under
`moderngekko-tools/`, safe to re-fetch per `docs/recompilation.md`'s own
recipe) to bring it to 6.6GB free - still tight, worth Valentin's own
attention on what else is using the other ~470GB, since that's pre-existing
data on his machine, not this session's to decide about.

## F1 menu: the Prelaunch tab existed in code but was never reachable

Requested follow-up to the above: continue building out the F1 menu now
that it's confirmed to actually run. `settings.cpp`'s `SettingsWindow` has
always had three tabs - `Prelaunch` (disc image path, language, graphics
backend), `Video`, `Input` - but `Prelaunch` is only added when constructed
with `prelaunch=true`, and `SettingsWindow`'s default argument is `false`.
`menu_bar.cpp`'s F1 tab bar constructed it with no argument at all, so every
run through the actual menu (not just reading the code) landed on Video as
the first tab, with no way to see or change the disc path from the UI -
confirmed by screenshot before and after the fix, not just inferred from
the source.

The honest reason this default was wrong rather than just unlucky:
`prelaunch=true` is clearly meant to distinguish "configuring before a game
is running" from "a live in-game pause menu" (it also gates whether
resolution changes in `GraphicsTuner` apply immediately or need a restart,
and suppresses one nav-fallback path). But `port_main()`
(`portmain.cpp`) calls `try_boot_game()` unconditionally and immediately on
process start, before any menu interaction is possible - there is currently
no window in the app's actual lifetime where "before the game boots" and
"F1 is reachable" overlap. So the `false` default was never correct for the
one call site that exists; it just silently ate the Prelaunch tab. Fixed by
passing `true` from `menu_bar.cpp` for now, with a comment there pointing at
this section: once boot is gated behind an explicit action instead of
firing on startup, this should reflect real game-running state instead of
being hardcoded true. Checked that this doesn't regress the pause-menu
"Cancel closes the window" path - that's handled unconditionally in
`Window::handle_nav_command` before the `mSuppressNavFallback` check that
`prelaunch` also affects, so it's unaffected either way.

Verified end to end after rebuilding: F1 -> Settings now opens on
**Prelaunch** by default, showing the real configured values (`Disc Image:
D:\dolphin\Super Mario Sunshine [GMSP01].iso`, `Language: English`,
`Graphics Backend: Auto`) - not placeholders, the actual `config.json`
content - with Video and Input as the next two tabs, exactly matching what
this section already described before it was actually run.

## 2026-09-19: the boot actually comes up - and three of this document's own conclusions were wrong

The longest bring-up session so far. It took the port from "parked in `PPCHalt`
on frame 0" to "the game runs its own main loop, streams data off the disc, and
reaches its intro movie". Recorded in the order the defects were found, because
the order is the argument: each one was only visible once the previous was gone.

### The `sc` off-by-one - and what it says about the "bootrom loop"

`handle_system_call` added 4 to `srr0` on the stated premise that it pointed AT
the `sc` instruction. It does not. `ppc_system_call_exception`
(`extern/dolrecomp/src/cpu/cpu.c`) raises the exception with `cia + 4u`, which
is what the architecture specifies for System Call - unlike traps, which do
leave SRR0 on the faulting instruction. The extra bump skipped exactly one
instruction after *every* `sc` in the game.

That is invisible almost everywhere and fatal in one place: `PPCSync()` is
`sc; blr` at 0x80339C38, and `PPCHalt` - an unconditional infinite loop -
starts four bytes later at 0x80339C40. Skipping `PPCSync`'s `blr` fell straight
into `PPCHalt`. Measured as `pc=0x80339C44`, with `lr` pointing back into
`__ARChecksize`, which contains no `PPCHalt` call of its own.

### Interrupts, and the defect that hid behind them

Nothing in this port ever raised an interrupt, because the DOL contains no
exception-vector code - the SDK copies its low-memory stubs into place at
runtime, so a static recompiler never translated anything at 0x500. That
blocked the boot twice in unrelated subsystems: the scheduler idling in
`SelectThread`'s `while (RunQueueBits == 0) ;`, and later the game's own audio
driver busy-waiting in `DSPSendCommands2`.

The second one is why a narrow workaround was not enough - a busy-wait in game
code is not a scheduler idle point, so there is no safe moment to special-case.
`recomp_interrupt.cpp` now does what the hardware does: save the interrupted
register file into the guest `OSContext` at `__OSCurrentContext` and jump to
`__OSDispatchInterrupt` (0x8033DF04). No return path is needed - every path
through that function ends in `OSLoadContext(context)`, and it brackets the
handler with `OSDisableScheduler`/`OSEnableScheduler`/`__OSReschedule` itself.

Once delivery existed, it exposed a much worse defect underneath. The
FP-unavailable handler re-enabled FP and resumed at `srr0` **without an rfi**,
so `exception_msr()`'s clearing of EE, IR, DR and RI was never undone: the
first floating-point instruction a thread ever executed disabled its interrupts
permanently. Measured at 172,801 out of 172,801 sampled slice boundaries with
`msr=0x00003000` - exactly FP|ME, an exception-entry MSR with FP added back. A
guest stack back-chain walk put the blocked thread in JAudio's audio thread.
This defect had been latent since the first boot attempt and was unobservable
without interrupts to notice it.

### Guest time had no sub-frame resolution

`CPUState::timebase` was advanced once per host frame from the host's wall
clock. That gets the long-run rate right and the resolution exactly backwards:
inside a frame guest time stood still, and between frames it jumped ~16.6ms.
The SDK's hardware-calibration loops measure *microseconds* - `__AI_SRC_INIT`
accepts a gap below 28.5us or between 34.5us and 39.0us and spins forever on
anything else. Guest time is now derived from consumed cycles and synced at
every MMIO access, which is where a polling loop observes the world.

### This port was supposed to be the apploader, and was not

**The `OSReport("bootrom")` message this document has speculated about for
several sections is not a disc check and not a BS2 version check.** It is
`DVDInit` (`src/dolphin/dvd/dvd.c:82`) branching on `bootInfo->magic`: neither
0xE5207C22 (booted via JTAG) nor 0xD15EA5E (booted from bootrom) means
"first time in bootrom", and that third branch is what prints it.

`OSBootInfo` lives at 0x80000000 and is written by the IPL/apploader, never by
the DOL. A calloc'd guest RAM answered 0, so the SDK took that branch, ran the
full drive bring-up, was answered with zeros, and shut the drive down.

The same class of gap explained the audio hang: `OS_BUS_CLOCK` is
`__OSBusClock`, a *variable* at 0x800000F8, so with it at zero every
`OSNanosecondsToTicks` in the SDK collapsed to 0 and `__AI_SRC_INIT` could
never satisfy bounds that were all zero. The interval it was measuring had been
correct the whole time - traced at 1271 ticks at 32kHz and 843 at 48kHz,
exactly the two sample periods.

`install_boot_info` now writes what the apploader writes: the disc ID, the
magic, and the file system table read off the disc, with `arenaHi` lowered to
protect it. The log line changed from `bootrom` to `app booted from bootrom`.

### The DVD had never transferred a byte

The DI registers were only shadowed, which let the guest believe transfers
completed while nothing moved - so the game ran on zeros, and
`JAIData::initData` looped forever on a count read out of a sound-info file
that never arrived. Two details had to be read rather than assumed:

- **DISR's transfer-complete bit is bit 4, not bit 2.**
  `__DVDInterruptHandler` maps `intr & 0x10` to cause 1 and `intr & 4` to cause
  2, and `cbForStateBusy` treats `intType & 1` as success and `& 2` as error.
  Setting bit 2 announced a drive failure on every successful transfer -
  measured as `DVDLowStopMotor` being issued immediately after the inquiry.
- **`DILENGTH` must count down to zero**, because `cbForStateBusy` computes
  `transferredSize += currTransferSize - __DIRegs[6]`.

### Vertex arrays carry pointers, which cannot be forwarded

`GXSetArray` is the one GX call that cannot work by forwarding FIFO bytes: what
it puts in the FIFO is a *pointer*. Aurora has no guest RAM to walk, rejects
the CP array-base registers outright, and offers `GX_LOAD_AURORA_ARRAYBASE`
carrying a host pointer instead. Until it was bridged, every indexed vertex
attribute was dropped - which is essentially all geometry. The texture path
plausibly has the same shape and is **not** bridged; that is the open question
at the time of writing, not a verified conclusion.

### Three measurement errors, recorded because they cost real time

1. **A probe rate-limited by its own subject.** The draw counter logged one in
   every 20,000 draws, and 11,730 frames x 1 quad falls under that threshold,
   so the second report never came and the result read as "one draw in 90
   seconds". Nothing rate-limited by the thing it measures can tell "rare"
   apart from "just below the threshold". It is wall-clock gated now.
2. **An inference reported as a finding.** "Stuck in `APP_STATE_BOOT`" was
   derived from a flat screen and a sleeping scheduler, and written up as fact.
   The guest-state probe (`src/port/recomp_probe.cpp`) then showed `appState`
   reaching NLOGO and then DONE, with the setup thread terminated,
   `arcBufNLogo` loaded and the DVD layer healthy. The game is in its intro
   movie. A screenshot taken at 45s showed the post-logo state; the logo phase
   is over by ~3s, and the draw burst recorded during it - 1180 quads in one
   second - was the logo being drawn all along.
3. **Five runaway logs, two of them after "fixing" it.** Deduplicating the
   unmapped-access warning by address bounded the spin-loop case it was written
   for, but a wild pointer writes to a *different* address every time, so every
   write is a first - 377MB. The number of distinct addresses is capped now,
   and past that point the message says plainly that this is memory corruption
   rather than a missing register.

### Where it actually stands

The game runs its own main loop, presents a frame every frame, performs real
disc reads, reaches `APP_STATE_DONE` and creates a `TMovieDirector`. `stderr`
is clean. Still open, and labelled rather than glossed:

- **The intro movie is stalled** - `ReadThread`, `VideoDecodeThread` and
  `AudioDecodeThread` are all WAITING. The AI DMA completion interrupt, which
  this port does not raise, is the leading suspect. **UNTESTED.**
- **No geometry is visible yet.** Textures are the next thing to check.
- **Alarms never fire.** `OSAlarm` is driven solely by the decrementer, and
  `mtspr 22` is not translated - it lands in the instruction fallback and is
  stored in a plain cell. Not what stalled the DVD, but it will bite.
- **Audio is not implemented.** The DSP mailbox handshake is simulated from the
  protocol's CPU side; no microcode runs.
- **SI (0xCC006400) is unmapped**, so the guest's PAD path reports
  `PAD_ERR_NO_CONTROLLER` on all four ports. `PADRead`/`PADInit` remain
  unbridged - the "Deliberately not attempted yet" note earlier in this
  document still stands.

## 2026-09-19 (later): the intro movie renders - and four sampling errors

The movie plays. Mario and Toadsworth, aboard the plane, in colour, drawn from
the game's own recompiled code. Getting there took two real fixes and exposed a
habit worth naming.

### The two fixes

**AI DMA completion.** The audio DMA engine lives in the DSP register block
(`__DSPRegs[24]/[25]` for the address, `[27]` for the length in 32-byte blocks
and its enable bit - `src/dolphin/ai/ai.c:51-64`), and nothing ever completed
its transfers. That froze everything downstream: `__AIDHandler` never ran, so
`syncAudio` never posted, so `Kernel::updateDac` and `MixAudio` never ran, so
`curAudioNumber` stayed at 0, so `PlayControl`'s
`curVideoNumber - curAudioNumber <= 1` (`THPPlayer.c:524`) was false forever and
the three THP worker threads deadlocked in a ring. The completion interval is
derived from the hardware - block count x 32 bytes at the DSP sample rate,
stereo 16-bit - and measures 708,750 timebase ticks for a 70-block buffer,
which is exactly 560 frames at 32kHz.

**The Gekko locked cache.** The THP decoder refuses to run without it
(`if (!(PPCMfhid2() & 0x10000000)) goto _err_lc_not_enabled;`,
`src/dolphin/thp/THPDec.c:49`), does its IDCT into a 16KB scratchpad at
0xE0000000, and moves each decoded row out with a DMA programmed through
`mtspr 922/923`. Neither the scratchpad nor the DMA existed: writes to
0xE0000000 hit the unmapped-MMIO path and were dropped, and the SPR writes were
filed in a storage map. The decoder wrote into the void while reporting
success. Decoded planes went from `nonZero=0` to a mean luminance of 122-164.

### Four sampling errors, all the same shape

Worth recording together, because each one cost real time and each looked like
a finding rather than an artefact:

1. The draw probe logged one in every 20,000 draws. 11,730 frames x 1 quad is
   under that threshold, so it reported "one draw in 90 seconds".
2. The plane probe hashed 64 of 286,720 bytes and reported empty planes when
   they were merely empty *at those 64 points*.
3. The texture content signature samples those same 64 points, which is not a
   sound change detector for a video frame.
4. A single screenshot per run reported a black screen for a movie that had
   been rendering all along. The run does not reach the same point at the same
   wall-clock time twice, so one sample per run measures nothing.

The lesson is the same in all four: a probe rate-limited by, or sparser than,
the thing it measures cannot distinguish absence from a gap between samples.

### Still open

Horizontal bands of corrupted pixels cross every frame. They are **not**
letterbox rows - they cut through Mario and the cabin. The locked-cache DMA
added the same day is the first suspect, since it is what moves each decoded
macroblock row into main memory, and per-row corruption is exactly the shape a
partially-wrong block count would produce.

## 2026-09-19 (later still): the noise bands, traced back three stages

The bands are thin horizontal strips of full-saturation random RGB that cut
through the picture. Each one begins at an arbitrary x and runs to the right
edge, and they sit at different heights from frame to frame. Four measurements,
each one narrowing the previous, and the first of them was wrong.

### 1. A mean per tile row said guest RAM was clean. It was the wrong statistic.

An I8 texture is tiled 8x4, so one tile row is `width*4` bytes covering
scanlines 4r..4r+3 - the same granularity the locked-cache DMA works at.
Profiling tile-row means found only the expected `zeroRows=4`, which is
448-432=16 scanlines of padding: the texture is 640x448, the movie is 640x432.

That reading was too strong, and it was recorded in a commit before it was
checked. Random bytes average 128, and this movie's own tile rows average
100-150. **A mean cannot see noise whose mean matches the picture.** Fifth
sampling error of this session, same family as the other four.

### 2. Roughness sees it immediately, in all three planes at once.

Mean `|b[i+1]-b[i]|` over horizontal neighbours inside each tile: a natural
image sits in the single digits, uniform random bytes average 85.3. Measured on
every frame: average 7-18, **maximum 84-139 on exactly one tile row**.

And the rows line up across planes - Y row 49 with U row 25 and V row 24, Y row
95 with U 46 and V 47, Y 38 with U 19 and V 19. The same picture region is
garbage in Y, U and V simultaneously, which is why the bands are full-saturation
colour rather than a luminance artefact.

### 3. The locked-cache DMA did deliver those rows.

A ring of recent store transfers, queried for the noisy row and for a control
row of real picture (the row whose roughness is closest to the frame average -
the *smoothest* row is a constant-coloured border and proves nothing):
`covered=true` for both, every time. The transfer is not dropping anything.

Its block-count encoding was also re-derived instruction by instruction from
`LCLoadBlocks`/`LCStoreBlocks` (`OSCache.c:513-545`) and matches.

### 4. The locked cache already held the noise, and the guest wrote it there.

Measuring roughness of the DMA *source* before each copy: 72 stores in 40
seconds carrying roughness 60-125, at `lc=0xE0000000/0xE0001000/0xE0002000`
(the Y work buffer) and `0xE0002800/0xE0003200` (U and V) - the
`__THPLCWork640` layout. Whole buffers, not tails.

Counting guest writes per 32-byte line since that line last left:
**`unwrittenLines=0` in all 72 cases.** Every byte was written by the guest.

### Where that leaves it

The decoder wrote noise. This is not a memory-plumbing defect in the port's
locked cache, its DMA, or the texture bridge - all three are now measured to be
faithful. It is the decode itself producing garbage.

The shape points at the Huffman bitstream. `__THPHuffDecodeMCU640`
(`THPDec.c:1570-1584`) counts MCUs down to a restart marker and resets the three
`predDC` values there. A bitstream desync therefore corrupts everything from the
error to the next restart marker and then recovers by itself - which is exactly
"one band, bounded, at a different height each frame". The decoder's inner loops
are hand-written PowerPC asm, so the next step is to find which instruction's
semantics differ under recompilation, not to add more probes around memory.

### Recorded as FAIL, not yet investigated

A 75-second run ended at ~60s with `aurora::gx::fifo: draw vertex data overrun:
need 80 bytes at pos 854, have 854`. Shorter runs (40-45s) have not reproduced
it. It is a separate defect from the bands and it is not cleared.

## 2026-09-20: the bands were the FPU context, and the probes proved it

The previous section ended by saying the decoder wrote noise and the next step
was the Huffman path. That was the right conclusion from the evidence and the
wrong guess about the cause. A parallel session working in the same tree had
already found it: **the port never performed the SDK's lazy FPU context
handoff.**

The FP-unavailable trap added earlier in this port sets `MSR[FP]` and resumes,
which is what lets floating-point code run at all. What it does *not* do is what
the real `__OSFPUnavailableHandler` also does - save the previous owner's FPRs,
paired singles and FPSCR into its `OSContext` and load the new owner's. So when
another thread borrowed the FPU, the THP decode thread's floating-point state
was silently replaced underneath it, and the IDCT in flight produced garbage for
whatever macroblocks it was working on. The bitstream was never desynchronised;
the arithmetic was.

That explains every property of the bands that had been measured and none of
which a memory-plumbing fault accounted for: one band per frame rather than
sustained corruption, a different height each time, Y/U/V wrong in the same
picture region simultaneously, every affected line demonstrably written by the
guest, and the locked cache and its DMA both faithful.

### The instrumentation became the regression test

The probes built while hunting the cause are what confirms the fix, with no new
measurement invented after the fact:

| measurement | before | after |
| --- | --- | --- |
| locked-cache stores carrying noise | 72 in 40 s | **0 in 45 s** |
| max tile-row roughness, any plane | 84-139 | **0.5-2.5** |
| movie still advancing | `v/a=668/666` | `v/a=514/512`, revision 1→120 |
| bands on screen | 4-5 per frame | **none** |

The low roughness was checked rather than accepted: a plane that has gone flat
would report the same thing. It has not - `nonZero=94%`, mean 163-167, min 0,
max 255, texture revision climbing. The frames sampled are simply smooth ones
(sky and cloud), and a busy frame - Peach against the cabin's map screen, full
of texture detail - is clean too.

Screenshots are the criterion here, not the counters, and they agree.

## 2026-09-20: the FIFO overrun is a cut, not corruption

`draw vertex data overrun: need 80 bytes at pos N, have N` killed two runs. It
is not random, and the second occurrence is what showed that.

### The two crashes carry the same signature

```
run 1  pos  835- 853: 5e b7 70 09 08 80 b9 dc ee f5 08 90 3b 9d ce e7 [80] 00 04
run 2  pos 13184-13202: 5e b7 70 09 08 80 b9 dc ee f5 08 90 3b 9d ce e7 [80] 00 04
```

Byte for byte identical, at two unrelated FIFO positions. Decoded, the tail is
`08 80 <u32>` = LOAD_CP_REG VAT_B[0], `08 90 <u32>` = LOAD_CP_REG VAT_C[0], then
`80 00 04` = draw quads, vertex format 0, four vertices - 4 x 20 = 80 bytes of
vertex data. `have N` is the position where those 80 bytes should start, so the
buffer ended **exactly** on the draw header. That is a cut, not corruption.

### Why the port cuts there

`portmain.cpp` runs `step_game(&g_gameCpu, kGameBlocksPerFrame)` and then
`aurora_end_frame()`. The guest is stepped for a fixed budget of *translated
blocks*, so the budget expires at an arbitrary guest instruction - including one
between the store that writes a draw's opcode into the write-gather pipe and the
stores that write its vertices. `aurora_end_frame()` then calls
`gx::fifo::drain()`, which did:

```cpp
process(detail::sBufferData, detail::sBufferSize, true);
detail::sBufferSize = 0;
```

- process the buffer whole, abort on a short command, and discard the rest.

Real hardware never sees this. The GP reads the FIFO as a stream and waits for
the remainder; there is no end of buffer to run off. Aurora's software FIFO is a
buffer that is consumed wholesale once per frame, and that difference is the
entire defect.

### The fix is to make it a stream again

`process_stream()` runs the same processing but, when a command runs past the
end of the buffer, stops at that command's first byte and reports the position.
`drain()` keeps those bytes and hands them to the next drain:

```cpp
const uint32_t consumed = process_stream(data, size, true);
if (consumed < size) { memmove(data, data + consumed, size - consumed); }
```

Every truncation guard in the command processor is unchanged outside stream
mode, so display lists - which are complete by construction - still assert
exactly as before.

A tail that never completes would grow without bound and silently swallow every
later command, so that is warned about once rather than left to look like a
rendering bug.

### The regression test

`dolphinjet --recomp-fifo-stream-self-test` is deterministic, unlike the crash
it stands in for: it feeds `process_stream` three NOPs followed by a LOAD_BP_REG
missing half its payload and checks that exactly 3 bytes are consumed, that
completing the command consumes all of it, and that a buffer holding nothing but
an incomplete command consumes nothing at all.

It exercises the carry-over mechanism rather than the draw path specifically. A
draw needs live GX and graphics state that a headless test cannot stand up
honestly, and pretending otherwise would make the test prove less than it looks
like it proves.

## 2026-09-20: the game was running at the monitor's refresh rate

The AI audio path reporting "dropped N queued bytes to bound latency" is what
exposed this. It was not an audio defect.

`portmain.cpp` called `step_game` once per host frame, and the host frame is
paced by `aurora_begin_frame()`, i.e. by vsync. The heartbeat measured **+144
steps per second** on a 144Hz display. Meanwhile `step_game` advances the guest
clock by exactly `kCyclesPerFrame = kCpuClockHz / 60` per call
(`recomp_boot.cpp:757`) and raises one vertical retrace per call.

So the guest lived 2.4 seconds per real second. Unplayable, and it handed the
32kHz audio device 2.4x more samples than it could consume - hence the drops.
It would also have been *too slow* on a 50Hz display, which is the same defect
seen from the other side.

The loop now accumulates real elapsed time and runs whole guest frames out of
it, keeping the remainder so the long-run rate does not drift, with catch-up
capped at four frames so one hitch cannot start a spiral.

Measured over 60 s, before and after:

| | before | after |
| --- | --- | --- |
| host frames/s | +144 | +144 (unchanged) |
| **guest steps/s** | **+144** | **+60** |
| audio drop warnings | 4 (log is capped) | **0** |

The 60 is what the clock model already assumes, so this makes the loop
self-consistent with it. It is **not** established as correct: GMSP01 is the
PAL disc, and the real retrace rate is whatever the guest programs into VI.
Deriving both this and `kCyclesPerFrame` from the VI registers instead of from a
constant is a separate question and is deliberately left open.

### What this then made visible

With the guest paced to real time, the THP movie advances about 5 frames per
guest second. That rate is unchanged by the pacing - before the fix it was 11.4
frames per *real* second, which is the same 4.8 per *guest* second - so this is
a pre-existing defect the speed bug was hiding, not something introduced here.
A THP intro is not a 5fps movie. Not yet investigated; the movie's own header
frame rate is the thing to read first.

### The movie's rate, measured rather than guessed at

Reading `ActivePlayer.header` out of guest memory (`ActivePlayer` at
`0x803E3B20` per `config/GMSP01/symbols.txt:25160`, `THPHeader` after the 0x3C
`DVDFileInfo`, so `frameRate` at +0x4C and `numFrames` at +0x50): the intro is
**29.97fps, 2816 frames** - 94 seconds of NTSC video.

It was advancing at 5.4 frames per guest second, i.e. 18% speed.

The data already contained its own discriminator: `decodedQueue` sat at 0-1 out
of three texture sets. A consumer starved of decoded frames means the decoder is
behind, not that `PlayControl`'s audio gate is holding frames back - if the gate
were the limit, the queue would be full and waiting. With the process using only
38% of one core, the port was not compute-limited; it was limited by the budget
it gives the guest.

`kGameBlocksPerFrame` was 4096, described in its own comment as "a conservative
starting guess, not a tuned constant... expect to revisit once this has actually
been run once". Changing only that constant:

| blocks/frame | movie fps | guest steps/s | CPU (1 core) | audio drops |
| --- | --- | --- | --- | --- |
| 4096 | 5.4 | 60 | 38% | 0 |
| 16384 | **15.4** | 55-63 | 93% | 0 |
| 32768 | 14.6 | 44-60 | 98% | 0 |

16384 gives the same decode throughput as 32768 without letting the guest fall
behind real time, so that is what is set. Interrupt latency is unaffected: the
per-slice *cycle* budget is `kCyclesPerFrame / 64` either way, only the block
count per slice changes.

**This is not solved, and the constant is not the fix.** 15.4fps against a
declared 29.97 means the guest still does not finish a frame's work inside its
budget, and the honest answer is not a bigger number - it is to stop stepping
when the guest has *finished*, i.e. when it blocks in the scheduler's idle path
waiting for the next retrace, with the block count demoted to a safety cap. The
heartbeat already shows that idle pc. That is the next piece of work, and until
it is done the movie runs at about half speed and the right constant is
machine-specific, which is its own argument against keeping one.

## 2026-09-20: the per-frame budget was counting the wrong thing

The comment on `step_game`'s slice loop said a slice ends "on whichever budget
runs out first: the cycle budget... or the block count". That was not true.
`dolrecomp_run_blocks` (`generated/generated.h:621`) is eight lines long and
never looks at `downcount`:

```c
while (max_blocks == 0u || blocks < max_blocks) {
    if (!dolrecomp_call(ctx, ctx->pc)) return 0;
    if (ctx->exception) return 0;
    blocks++;
}
```

So the block count was the *only* stop condition, and the cycle budget only fed
the clock. Instrumenting how much guest time a frame actually bought:

```
frame budget: guest time advanced 620008 of 675000 ticks (92%) | 64/64 slices hit the block budget
frame budget: guest time advanced 258019 of 675000 ticks (38%) | 64/64 slices hit the block budget
frame budget: guest time advanced  45901 of 675000 ticks ( 7%) | 64/64 slices hit the block budget
```

Every slice of every frame ran out of blocks, and a frame bought between **7%
and 92%** of a real GameCube frame. Block length varies enormously - a tight
loop the recompiler extracted into one block burns thousands of cycles, a run of
straight-line code burns a handful - so the block count is a poor proxy for
time. The guest was permanently cut off mid-frame at an unpredictable point.

`run_blocks_until_budget_spent` now stops on `downcount`, which is what models a
frame, and the block count becomes the safety cap its own comment always called
it.

### The cap is not optional, and finding that out cost a stall

Raising the cap to 1,048,576 at the same time as adding the cycle check -
two variables at once, against this project's own rule - produced frames that
advanced exactly **675,125 of 675,000 ticks (100%)** with **0 of 64** slices
stopping on blocks. Correct, and it stalled: after 150s the game sat at
`appState=3 (NLOGO)` with `piCause=0x144` (VI, DSP and DI all pending and never
acknowledged) and a DI transfer frozen at `curr=0x8000 done=0x0`.

The previous run had already isolated it, because that one kept the old 256
blocks/slice cap alongside the cycle check and ran fine. So the cycle check is
sound and the missing cap is what stalls: something reached through
`dolrecomp_call` - a host-call bridge or a replacement - returns without
decrementing `downcount`, so the slice never ends and interrupt delivery, which
only happens between slices, is starved. **Which** callee is not yet identified;
that is worth knowing rather than papering over with a cap.

### Where it is left

| | blocks/slice | guest time per frame | guest steps/s | movie fps |
| --- | --- | --- | --- | --- |
| block budget only | 256 | 7-92% | 60 | 15.4 |
| cycle budget + cap | 2048 | **69-71%** | 36 | 12 |
| cycle budget, no cap | 16384 | 100% | 32 | stalls |

The middle row is what is set. It is the more principled of the two working
rows - the guest's clock and the work it does now agree to within a third,
instead of varying by a factor of thirteen frame to frame - and the game reaches
the movie and plays it normally. It is also honestly slower on the clock the
player sees: 36 retraces per second instead of 60, so the game runs at about
60% speed, where the old row ran at 60 retraces of partial work, which is slow
motion with an incoherent clock rather than a faster game.

Neither row is the answer. The port executes guest code at roughly half the rate
a GameCube does on one host thread, and that is a performance problem, not a
budgeting one. The two concrete leads are the callee that does not decrement
`downcount` (above), and the guest's idle spin: with the cycle budget the guest
now burns real host cycles inside `SelectThread`'s `while (RunQueueBits == 0);`
loop (`src/dolphin/os/OSThread.c`), which costs nothing on hardware. Skipping
ahead when `RunQueueBits` is zero is free time, and `RunQueueBits` is a single
guest global, so detecting it needs no pc heuristics.

### The callee that spent no cycles was our own bridges

Naming it took one bounded probe: record `downcount` either side of every
`dolrecomp_call` and log the first occurrence of each address that spends
nothing. Sixteen distinct addresses, resolved against
`config/GMSP01/symbols.txt`:

```
__OSInitSystemCall  OSReport  PADInit  PADRead  PADSetAnalogMode
PADControlMotor     GXSetArray          PPCMtwpar
Config24MB+0x2C ... +0x60
```

Every one of the interesting ones is a **host-call bridge this port installed**.
A bridge replaces a whole guest subroutine with host code and returns without
touching `downcount`, so a stretch of them spends no slice budget at all. The
`Config24MB` cluster is the instruction fallback (`mtspr`/`mfspr`), which
advanced `pc` without charging for the instruction it emulated.

Both now charge: 100 cycles per bridged call, one cycle per emulated
instruction. The 100 is explicitly **not** a timing model - the replaced
function's real cost is unknown and this does not pretend to estimate it. It is
a forward-progress guarantee, and at 0.2us it is 1/81,000th of a frame, too
small to distort the clock at any plausible call rate.

The effect is larger than "no longer stalls", because a slice that ended on the
block cap **threw away its remaining cycles** - the next `refill_slice_budget`
overwrites `downcount` rather than adding to it. Recovering that budget:

| | guest time per frame | slices stopped by the block cap |
| --- | --- | --- |
| before | 69-71% | 37-41 of 64 |
| after | **97-99%** | **0-4 of 64** |

Same 36 guest steps/s and same host CPU, so guest work done per host second rose
about 40%. The block cap is finally what it was always described as: a safety
net that almost never fires.

Sixteen addresses still spend nothing, but they are now ordinary game blocks at
mid-function offsets (`__THPDecompressiMCURowNxN+0x19F0`,
`drawChar_scale__10JUTResFontFffffib+0x210`), i.e. blocks whose counted cost is
zero rather than bridges that bypass counting. `downcount` is a real cycle
counter - dolrecomp emits `ctx->downcount -= block_cycles[i]` per basic block
(`extern/dolrecomp/src/backend/emitter.c:1914,1969`) - so these are sparse and
interleaved with blocks that do charge, which is why they cannot wedge a slice.

## 2026-09-20: fast-forwarding the guest's idle spin

With the cycle budget in place the guest genuinely sits in `SelectThread`'s

```c
if (RunQueueBits == 0) {
    OSSetCurrentContext(&IdleContext);
    do { OSEnableInterrupts(); while (RunQueueBits == 0) ; OSDisableInterrupts(); }
    while (RunQueueBits == 0);
    OSClearContext(&IdleContext);
}
```

(`src/dolphin/os/OSThread.c`). That loop costs a GameCube nothing and costs this
port full host cycles to simulate, for a wait.

### The obvious condition is wrong, and it deadlocks the boot

`RunQueueBits == 0` alone is **not** idleness. Before `__OSThreadInit` runs there
is no scheduler and nothing to count, so the condition holds from the very first
frame. Measured: "64 slices idle-skipped" on every frame, CPU down to 8.5%, and
the game frozen at `appState=0 (WAIT)` having executed nothing at all. The CPU
number on its own looked like a spectacular win, which is exactly why it was
checked against the game's state before being believed.

The exact signal is the context: `SelectThread` does
`OSSetCurrentContext(&IdleContext)` immediately before the spin and
`OSClearContext(&IdleContext)` immediately after, so `__OSCurrentContext ==
&IdleContext` means the scheduler is up *and* has nothing to run, which cannot
be true before it exists. `IdleContext` is at `.bss:0x803FA558` with size
`0x2C8`, matching `sizeof(OSContext)` exactly - a useful check that it is the
object it claims to be. `RunQueueBits == 0` is kept as the second half, since a
non-zero value means the spin is about to exit anyway.

A slice is never skipped when an interrupt was just delivered: that slice has a
handler to run, and the handler is what makes a thread runnable.
`deliver_pending_interrupts` now returns whether it dispatched.

### What it buys

The clock is unaffected: spending the slice's budget without executing leaves
the next `refill_slice_budget` to account for exactly the cycles it would have,
so guest time passes at the same rate and every time-driven device completion
lands where it would have. Only the host work of simulating a wait is skipped.

Skipped slices per frame, over 90 seconds: **62 of 64** on eight sampled frames,
0 on eight others, and 21/58/52 elsewhere - the game alternates between frames
that are almost entirely idle and frames that are fully busy. Guest steps went
from 36/s to 36-40/s.

**A caveat on the numbers, because the obvious comparison is invalid here.** The
"guest time advanced per frame" percentage depends on what the movie is
decoding, and two runs are never at the same point in it. Comparing that
percentage across builds therefore measures the scene as much as the change, and
any such comparison in this document's earlier tables is weaker than it looks.
Skipped-slice counts and steps/s are the figures that can be compared.

## 2026-09-20: where the host second actually goes

Two leads were named at the end of the budgeting work. Both were measured; one
was wrong.

### Per-block dispatch overhead: measured, and it does not matter

`dolrecomp_call` runs a host-call lookup on every single block -
`recomp_host.cpp`'s `dispatch()` is an `unordered_map::find` - before reaching
the translated code, which looked like a plausible per-block tax. Measuring
guest cycles per block says otherwise: **290-514 cycles per block** in typical
frames (one outlier frame at 68, with 85,312 small blocks). At roughly 20,000
blocks per frame and 36 frames a second that is ~720,000 lookups per second,
which at any believable hash cost is about 1% of a core.

So the dispatch is not the bottleneck and optimising it would have been work
spent on nothing. Recorded because a lead that turns out to be wrong is worth
the same as one that turns out to be right, as long as it was measured.

### The host loop around the guest: also small, but it exposed a real defect

Timing `step_game` against wall time: **93-94% of every second was inside it**.
The presenting, FIFO draining and UI around it cost about 6%. The bottleneck is
the translated code itself, which is a code-generation problem rather than
anything the frame loop can fix.

But the same heartbeat showed the host frame count had collapsed to **+9 per
second**. The pacing loop ran catch-up as a `while`, so when the guest was
behind it could execute up to four guest frames back to back with no present in
between - and a guest frame costs about 26ms of host time here, so that is up to
104ms of frozen window. The guest was fine; the picture was not, and nothing in
the guest-side numbers would ever have shown it.

Stepping at most once per host iteration lets the host draw between guest
frames. The debt accumulator is unchanged and still capped, so nothing is lost:

| | before | after |
| --- | --- | --- |
| host frames presented/s | 9 | **32** |
| guest steps/s | 36 | 32 |
| step_game share of wall time | 93-94% | 89-91% |

11% less guest work for 3.5x more frames actually seen, and `frames == steps`
now, which is the relationship that should hold: every guest frame presented
exactly once, none dropped and none presented twice.

The game is still in slow motion - 32 guest frames per second against 60 - but
it is now smooth slow motion rather than a picture updating four times less
often than the guest produces it.

## 2026-09-20: profiling the translated code, and what the profile does not say

Budgeting, scheduling and per-block dispatch have all been measured and ruled
out, which leaves the translated code itself. Profiling it needed care about
*what* is being weighted: sampling at slice boundaries would favour whatever
runs when a budget expires, and counting blocks would weight a three-instruction
block the same as a three-thousand-cycle loop. So the profile is keyed on each
block's entry pc and weighted by the guest cycles that block actually spent -
a profile of time. Addresses are resolved against `config/GMSP01/symbols.txt`
offline, keeping the hot path to one hash update.

Over 80 seconds, 1025 distinct block entries, 2.28 billion guest cycles:

| share | function |
| --- | --- |
| 36.0% | `__THPDecompressiMCURowNxN+0xB0` |
| 25.9% | `__THPHuffDecodeDCTCompY` |
| 4.9% | `__THPHuffDecodeDCTCompU` |
| 4.3% | `SelectThread+0x134` |
| 3.9% | `__THPHuffDecodeDCTCompV` |
| 3.4% | `__THPDecompressiMCURowNxN+0x17A8` |
| 2.4% | `getFontType__10JUTResFontCFv` |
| 1.3% | `GXBegin` |

**About 74% of every guest cycle is the THP video decoder.** That is not a
defect - a software JPEG-ish decoder is why THP needs the locked cache and
paired singles on real hardware too.

### The caveat matters more than the numbers

This is a profile of **the intro movie**, which is 2816 frames at 29.97fps: 94
seconds of a game that is meant to last many hours. Optimising against it would
be optimising the title sequence. A gameplay profile will look nothing like
this, and nothing here justifies work on the THP path until that profile exists.

Two smaller things are worth noting because they are *not* movie-specific:

- `SelectThread+0x134` still takes 4.3%. The idle fast-forward only decides at
  slice boundaries, so up to a slice of spin survives each time.
- `getFontType`, `getAscent` and `getDescent` together take 4.7%. They are
  one-line accessors, and at that size the cost is the per-block machinery
  around them rather than the work they do - which is the shape of overhead a
  recompiler leaves behind, and the one place this profile hints at something
  general.

### Which exposes that nothing has ever run past the intro

The movie is 94 seconds of content and the port runs at about half speed, so it
needs roughly three minutes of wall clock to finish. Every measurement run so
far has been 40-150 seconds. **No run has ever reached the far side of the
title sequence**, so what happens after it is entirely untested - not working,
not broken, untested.

## 2026-09-20: input works, the intro is skippable, and gameplay is a black screen

### First, a correction

The previous section said no run had ever reached the far side of the title
sequence, and a reading of one run's final log lines was taken as a deadlock:
the movie frozen at frame 1704 of 2816 with 97% of cycles in `SelectThread`'s
idle spin. **That was wrong.** Those were the last lines of a process being shut
down by the harness, not a hang. Plotting the movie's frame counter across the
whole run shows it advancing steadily throughout - 0, 202, 370, 528, 687, 828,
986, 1164, 1350, 1536, 1626, 1704 - with no plateau anywhere. The run simply
ended before the movie did.

### The intro is skippable, and A reaches the guest

Pressing A skips it, and three attempts failed before one worked - each failure
for a different reason in the *harness*, not in the port:

1. `SendKeys` posts `WM_CHAR`, which SDL does not read. Replaced with
   `keybd_event`.
2. Windows refuses `SetForegroundWindow` from a background process, so the
   keys went to whatever had focus. Aurora reads the keyboard through
   `SDL_GetKeyboardState`, which is only fed by window messages, so focus is
   not optional. Fixed with the `AttachThreadInput` sequence, and the harness
   now *says* whether it got focus - a negative result without focus proves
   nothing.
3. The key still never arrived, because `keybd_event` with a virtual-key code
   is logical and `SDL_SCANCODE_Z` is physical. On this machine's French AZERTY
   layout Windows maps `VK_Z` to the key that types "z", which sits where QWERTY
   has W, so SDL saw `SDL_SCANCODE_W` and the binding never matched. Sending the
   set-1 scancode `0x2C` with `KEYEVENTF_SCANCODE` names the physical key and
   makes the layout irrelevant.

With that, `PADRead: port 1 buttons 0x0100 err=0` - `PAD_BUTTON_A` in the
guest's own `PADStatus`. **Input is proven end to end for the first time**:
keyboard to SDL to Aurora to the recompiled game. Worth stating plainly because
every earlier signal was indirect: the notice disappearing only said Aurora was
willing to answer, not that a key travelled.

The harness is kept as `tools/port/dolphinjet_skip_intro.ps1`.

### Which reaches gameplay, and a new frontier

On the A press the THP player closes (`open=0`) and the app state becomes
**`appState=5 (GAMEPLAY)`**. The guest runs at a full 60 steps per second and
the host presents at 144.

And the screen is black. A profile taken in that state, over 130 seconds:

| share | function |
| --- | --- |
| 51.1% | `SelectThread+0x138` |
| 45.8% | `SelectThread+0x134` |
| 1.5% | `mixDSP__Q28JASystem6DSPBufFl` |
| 1.0% | `vframeWork__Q28JASystem6KernelFv` |

**96.9% idle spin, 61 distinct block entries in the whole window**, every thread
`WAITING`, no DVD command executing, `piCause=0`, no errors. Only the audio
kernel is doing anything at all. The game enters gameplay and then does nothing.

This is the frontier now, and unlike everything before it, it is reachable in
about twenty seconds and reproducible on demand. It is **untested territory
turned into a testable defect**, which is the whole point of being able to press
A.

## 2026-09-20: the black gameplay screen was GXWaitDrawDone

Resolving what each waiting thread was parked on named it immediately:

```
prio 16  DefaultThread   attend sur 0x804060FC = FinishQueue [sbss]
prio 12  0x803e27a0      attend sur FreeAudioBufferQueue
prio  8  ...             attend sur JKRDecomp / JKRAramStream / JKRAram
prio  3  ...             attend sur JASystem::Dvd
prio  2  ...             attend sur JASystem::AudioThread
```

`FinishQueue` has exactly one sleeper anywhere in the SDK - `GXWaitDrawDone`
(`src/dolphin/gx/GXMisc.c:94`) - so the main game thread being on it is a proof,
not an inference.

The chain: on hardware `GXSetDrawDone` clears `DrawDone` and writes BP register
`0x45000002`; the GP later raises PE_FINISH (PI cause bit `0x400`,
`OSInterrupt.c:368`), whose handler sets `DrawDone` and wakes `FinishQueue`.
This port **bridges `GXSetDrawDone` away** to an Aurora no-op, so the guest's own
body never runs, and it does not model PE_FINISH at all - but `GXWaitDrawDone`
is **not** bridged, so the guest's own copy runs and sleeps forever. The
existing comment on that bridge had even predicted where the trouble would
surface, without naming the consequence.

The bridge now sets the guest's `DrawDone` byte (`.sbss:0x804060F8`). That is
more faithful than bridging `GXWaitDrawDone` as well: the guest's own wait code
still runs and finds the work already finished, which is what it would find on
hardware if the interrupt beat it to the check. Aurora renders synchronously, so
by the time the bridge returns there is genuinely nothing to wait for.

**What it does not model**, stated because it will matter later: the PE interrupt
itself. `DrawDone` stays 1 rather than cycling per frame, so nothing would notice
a GP that really had fallen behind, and `GXSetDrawDoneCallback`/`TokenCB` never
fire. Both need the PE register block at `0xCC001000` and PI cause bit `0x400`.

### Which immediately reached two new defects

With the main thread running, the game starts a DVD read (`state=1 (BUSY)
offset=0x210c1040 length=0x8000` - loading a level) and begins submitting real
geometry. It gets further than anything before it, and then:

```
[ERROR] aurora::gx::fifo: CP_REG_ARRAYBASE_ID is not supported on Aurora.
                          Use GX_LOAD_AURORA_ARRAYBASE instead.   (x3)
[FATAL] aurora::gx::fifo: unsupported primitive type 192
```

- **CP array bases.** Aurora rejects CP registers 0xA0-0xAF
  (`command_processor.cpp:1309`) because a guest physical address means nothing
  to it; it wants `GX_LOAD_AURORA_ARRAYBASE` with a host pointer. This port does
  bridge `GXSetArray` and emit that, but the guest re-sends the CP registers
  itself from its own shadow state, so the bridge alone is not enough.
- **Primitive 192.** `0xC0` is not a valid GX opcode at all (draws are
  0x80-0xB8), so the FIFO parser is out of sync rather than meeting something
  merely unimplemented. Gameplay is the first time real geometry has ever gone
  through this path, and `recomp_gx_fifo.cpp`'s size-8 FIFO write - the
  `stfd`/`psq_st` split - carries its own comment saying the split direction is
  "a best guess from big-endian byte order, not confirmed". It is now exercised
  for the first time. Not established as the cause; it is the first thing to
  check, and the next step is a hex dump around this FATAL like the one the
  draw-overrun path already has.

Recorded as **FAIL**, reproducible in about twenty seconds with
`tools/port/dolphinjet_skip_intro.ps1`.

## 2026-09-20: the FIFO desync was an indexed XF load consuming one byte too many

The desync's first bad byte is never where the fault is, so the parser now keeps
a trail of the last twelve commands and dumps it. That named the cause in one
run:

```
[pos 7450 op 10 +13]  [pos 7463 op 20 +6]  [pos 7469 op 00 +1]  [pos 7470 op 13 +135177]
```

Two things are wrong there. `0x13` is not a GX opcode at all, but
`cmd & CP_OPCODE_MASK` turns it into `0x10` (LOAD_XF_REG), whose garbage header
then consumed **135,177 bytes** and left the cursor inside vertex data - the
mask hides an invalid byte rather than reporting it. And the real fault is one
command earlier: `op 20` (LOAD_INDX_A) consumed **6** bytes.

The game's own emitter says what the format is:

```c
GX_WRITE_U8(0x20);
GX_WRITE_U32(reg);        // src/dolphin/gx/GXTransform.c:177
```

One opcode byte and one u32 - **five bytes**, with `reg` carrying the offset in
bits 0-11, length-1 in bits 12-15 and a 16-bit index in bits 16-31. Aurora read
the index as a single byte, read `addrLen` from bytes 1-2 instead of the low
half at 2-3, and consumed five payload bytes instead of four. One byte too many
per indexed load, and the stream never recovers.

Measured after the fix: **zero desyncs**, where every run before it died on
`unsupported primitive type 192`.

### Which exposed the next one, as fixing a parse usually does

The indexed load now actually dereferences the array it names -
`array.data + srcArrayIdx * array.stride` - and `array.data` is null, because
Aurora rejects the CP array-base registers 0xA0-0xAF that would have set it
(`command_processor.cpp:1309`; it wants `GX_LOAD_AURORA_ARRAYBASE` with a host
pointer, since a guest physical address means nothing to it). The run now ends
in a hard crash with no FATAL line, which is what reading from null plus an
offset looks like.

So the two defects were linked: the stream desynchronised before it ever got far
enough to use an array base, and fixing the parse is what made the missing base
matter. The port does bridge `GXSetArray`, but the guest re-sends the CP
registers itself from its own shadow state, so the bridge alone never covered
this path.

Recorded as **FAIL**, reproducible in about twenty seconds.

### Two more in the same handler, and the array bases

Fixing the parse made indexed loads actually execute, which exposed the rest of
the same handler.

**An out-of-bounds array index.** `GX_POS_MTX_ARRAY + (opcode - (CP_CMD_LOAD_INDX_A / 0x08))`
puts the division on the constant instead of the difference: for opcode `0x20`
that is `21 + (0x20 - 4)` = **49**, indexing a 26-element array. It read a
garbage base pointer and stride and dereferenced them - a hard crash with no
FATAL line, which is what the run showed. It should be
`GX_POS_MTX_ARRAY + ((opcode - CP_CMD_LOAD_INDX_A) / 0x08)`, giving 0-3 for the
PosMtx, NrmMtx, TexMtx and Light arrays. An array with no base is now reported
once instead of dereferenced.

**The array bases themselves.** Aurora refused CP registers 0xA0-0xAF outright
because they hold a guest physical address it cannot read. Bridging `GXSetArray`
does not cover that path: the guest re-sends those registers from its own shadow
state on every dirty-state flush. Measured in one 70-second gameplay run:
**351,846 rejections**, unbounded - the sixth runaway log this project would
have had.

Aurora now asks the host to translate, through a resolver the port installs
(`install_array_base_resolver`), and the mapping lands correctly on the matrix
arrays too: `GX_VA_POS + 12` is `GX_POS_MTX_ARRAY`, +13 NRM, +14 TEX, +15 LIGHT.
The remaining complaint is bounded to 32 distinct addresses.

### Where that leaves gameplay

| | before | after |
| --- | --- | --- |
| FIFO desyncs | every run | **0** |
| array-base rejections | 351,846 | **0** |
| crash | hard crash, no FATAL | **none over 70 s** |
| guest steps/s | 44 | **60** |

`appState=5 (GAMEPLAY)`, full speed, no errors left in stderr beyond two
memory-card lines. **And the screen is still black.** Three real defects are
gone and the frame still renders nothing, so the cause is further along and is
not yet identified. Recorded as such rather than as progress that looks like a
conclusion.

One thing worth knowing for the next step: the 15,510 "unresolved host call"
warnings are **not** failures. `dispatch()` logs a miss and returns false, and
`dolrecomp_call` then falls through to `dolrecomp_call_original`, which handles
the address normally. Every unbridged address produces one, so the count is
noise, not a signal.

## 2026-09-20: the screen is not black any more

`GX_CMD_CALL_DL` is now executed the way the GP executes it — the pointer is
followed and the list runs in place — through the same host resolver the CP
array-base registers use, generalised rather than duplicated.
`GXCallDisplayList` is deliberately **not** bridged: the guest body has to keep
running, because it does `__GXSetDirtyState()` and `__GXSendFlushPrim()` before
emitting the opcode, and bridging it away is exactly the mistake already made
on `GXSetDrawDone`.

A display list is complete by construction, so it is processed outside stream
mode with the stream flags saved and restored around the call - otherwise a
truncation inside a list would tell the outer FIFO it had been cut and make it
carry over bytes that were never its own. Depth is bounded at two.

### Getting there needed a crash reporter, because guessing had run out

Following display lists brought back a crash that left no message at all, and
no command-line debugger is installed on this machine (`cdb`, `windbg`, `ntsd`
all absent). Three fixes were attempted on hypotheses and none was the cause,
which is three more than this project's rules allow.

`src/port/recomp_crash.cpp` now reports faults in-process: a vectored exception
handler with DbgHelp symbolisation on Windows, POSIX signals with `backtrace()`
elsewhere, plus `SetUnhandledExceptionFilter`, `SIGABRT` and `std::set_terminate`
- the last three matter because the actual failure was an `abort()`, which a
vectored handler never sees. Release builds now carry `/Zi` so the stack has
names. It **reports and does not recover**: continuing after a fault turns a
located crash into an unlocated one later.

It named the cause on the first run:

```
#4  aurora::gfx::push+0x89          (lib/gfx/common.cpp:1474)
#5  aurora::gx::fifo::handle_draw_unmerged  (command_processor.cpp:2077)
#6  aurora::gx::fifo::draw_prim
#9  aurora::gx::fifo::call_display_list
#12 aurora::gx::fifo::drain
```

### The cause was mine

`command_processor.cpp:2077` is `push_storage(array.data, array.size)` - Aurora
uploads a whole vertex array. And `array.size` is what my own resolver reports:
a vertex array has no length register on the hardware, so it returned
*everything from the base to the end of guest RAM*, up to 24MB. The storage
staging buffer is 8MB, and `ByteBuffer::resize()` calls `abort()` when a
non-owned buffer is asked to grow (`lib/gfx/common.hpp:143-156`). It only ever
fired once display lists executed, because until then no indexed array was used
to draw.

The derivable bound was sitting right there: the vertex descriptor says whether
the attribute is indexed with 8 or 16 bits, so the highest byte the GP could
reach is `(maxIndex + 1) * stride`. That is what is uploaded now, and a clamp is
reported once per attribute.

Two further latent faults were fixed on the way, neither of them the cause:
`get_last_draw_command()` called `back()` on a possibly-empty vector — undefined
behaviour reached by the first draw of a pass once the state stops being dirty,
which only real geometry causes — and the indexed XF load could name a byte
16MB past its base.

### Measured

| | before | after |
| --- | --- | --- |
| display lists discarded | 512,094 | **0** |
| draws submitted | 3,731/s | **79,387/s** |
| vertices submitted | 59,444/s | **628,858/s** |
| crash over 70 s | yes, silent | **none** |
| screen | black | **the file-select / options menu, rendered** |

The screenshot is the acceptance criterion and it is met: blue menu panels, the
word "Corrupt" on a save slot, "OPTIONS", the arrow. "Corrupt" is consistent
with the memory card not working, which is Phase 4 of the plan.

## 2026-09-20: what renders, what does not, and a probe that lied

With display lists executing, the file-select menu renders. Pressing A past it
gives a black screen again, with **80,000 draws and 640,000 vertices submitted
per second** and no DVD activity - so it is not a loading screen and the
geometry is arriving.

### The projection was never the problem, and my probe said otherwise

`projType` looked pinned at 1 (orthographic), which would explain a 3D scene
projected into nothing. Aurora only applies a projection write when it starts at
the block's first register and covers all seven (`command_processor.cpp`), and
partial writes were dropped with no log, so a probe was added for them.

It reported four ignored projection writes and four ignored viewport writes per
frame - and every one was a **false positive of the probe itself**. `handle_xf`
loops over every register a write covers (`for (i = 0; i < count; i++) { reg =
xfAddr + i; switch (reg) }`), so one perfectly normal 7-register write enters the
projection case seven times with offsets 0..6, and only offset 0 does the work.
That is correct by design: the whole block is read at once when offset 0 is seen.

Corrected to fire only on the write's first register:

| | before correction | after |
| --- | --- | --- |
| projection writes ignored | 4 per frame | **0** |
| viewport writes ignored | 4 per frame | **0** |

And both projections are applied, perspective included:
`projection applied: type 0 (perspective)`. The earlier "projType is always 1"
reading came from the one-shot THP quad probe, not from the truth.

**Recorded because it nearly caused a fix to the wrong thing.** A probe coarser
or blunter than its subject has now produced a false reading six times in this
project; this is the first time the false reading was an invented defect rather
than a missed one.

### What is left

`numTevStages` never exceeds 1, across the whole run and every distinct
`genMode` value. Super Mario Sunshine's materials use more than one stage for
almost everything, and J3D sets them through material display lists which now
execute - so either those particular lists are not being reached, or the BP
genMode writes inside them are not landing. That is the next measurement, not a
conclusion.

## 2026-09-20: why the save says "Corrupt"

The game shows "Corrupt" on its save slot, and the log said only:

```
Failed to open file: super_mario_sunshine
Failed to write 8192 bytes to card
Failed to close file at idx: 0
```

### What the file itself says

`%APPDATA%\dolphinjet\DolphinJet\EUR\Card A\01-GMSP-super_mario_sunshine.gci`
is 57,408 bytes with a **perfectly valid header** - `GMSP01`,
`super_mario_sunshine`, seven blocks at offset 0x38, and 64 + 7 x 8192 = 57,408
exactly - and **every one of its 57,344 data bytes is zero**. Looking at the
size would have said the save was fine; only looking at the contents says it is
empty. `tools/port/inspect_gci.py` now does that in one command.

So `CARDCreate` succeeded and the write after it did not. The game then reads
back zeros and calls the slot corrupt, which is correct of it.

### The bridges were not at fault

Both `host_call_card_open` and `host_call_card_write`
(`src/port/recomp_card.cpp`) marshal correctly - host temporary, guest layout
written back through `mem_write32`, guest buffer copied byte by byte. They were
committed as "not verified"; they are now verified and they are sound.

### The cause

`CardGciFolder::getFile()` returns a file **only when it is marked open**
(`extern/aurora/lib/card/CardGciFolder.cpp`):

```cpp
auto file = &m_files[idx];
if (file->opened)
  return file;
return nullptr;
```

`openFile()` sets `gciFile.opened = true` before handing back a handle.
`createFile()` does not - it pushes the new entry with `opened = false` and
returns a handle to it anyway. Every later use of that handle therefore finds
nothing.

Exhibited by moving the empty save aside so a create would happen again, with
the error messages taught to name their result:

```
Failed to write 8192 bytes to card at offset 0 (fileNo 0): result -3
Failed to close file at idx: 0 - result -4
```

`-3` is `NOCARD` and `-4` is `NOFILE` (`extern/aurora/lib/card/Util.hpp:58-70`),
and both come from that same null `getFile`. The freshly created file is all
zeros again, so this reproduces on demand rather than being a one-off.

Recorded before the fix, which lands separately.

### The fix, and what it measures

`createFile()` now pushes its entry with `opened = true`, the same thing
`openFile()` already does, because it hands back a usable handle.

| | before | after |
| --- | --- | --- |
| non-zero data bytes | **0** of 57,344 | **4,236** (7.39%) |
| block 0 | 0 | 4,224 |
| blocks 1-6 | 0 each | 2 each |
| write result | -3 (NOCARD) | no error |
| close result | -4 (NOFILE) | no error |

The two remaining `Failed to open file` lines are the expected first-open before
a create: `CARDOpen` returns -4 (NOFILE), which is what makes the game enter its
save-creation flow, and the bridge normalises Aurora's NOCARD to NOFILE for
exactly that reason.

### A harness problem worth recording

The screenshot harness captured the **whole screen**, not the game window, so a
run photographed unrelated things the developer had open. That is a privacy
defect in the tooling regardless of what happens to be on screen. It now
captures only the DolphinJet window rectangle via `GetWindowRect`, and the
full-screen captures from these runs were deleted.

## 2026-09-20: where the 80,000 draws a second actually go

A counter that separates **submitted** from **issued to the GPU**, and names
where the rest went. It checks its own arithmetic in the line it prints, so a
counter that is wrong says so instead of being believed - six probes in this
project have already produced a false reading by being coarser than their
subject, and one invented a defect outright.

Measured in gameplay:

```
draw fate 1s: submitted=6566 reached_render=6566 (dropped_before_render=0)
              | pipeline_not_ready=0 issued=6566 of which without_texture=1885
```

Three things this settles:

- **The pipeline cache is not the problem.** `pipeline_not_ready = 0` across the
  whole run. `find_pipeline_impl` returning a reference for a merely-queued
  pipeline, and `render()` dropping such draws in silence, was the leading
  suspect from the exploration. It never fires here.
- **Essentially every draw reaches the GPU.** submitted and issued track each
  other to within a handful per second.
- **The 80,000 figure was a different thing.** The older `draw 1s:` probe counts
  every `handle_draw`; this one counts pushed draw commands, and merging takes
  80,000 down to ~6,550. Both are right and they measure different things -
  worth stating, because comparing them would look like a catastrophic loss
  that is not there.

What it does surface: **1,880 of 6,550 issued draws a second - 28% - carry no
texture bind group at all**, so every texture unit samples zero. That is
visually black and is indistinguishable from a correct shader without this
count. It is not the whole story though, since the other 72% are issued with
textures and the screen is still black.

### The self-check earned its place immediately

Its first run flagged one inconsistent line, `submitted=6501
reached_render=6506`. That was the check being wrong, not the counters: a draw
submitted in one frame is rendered in the next, so a one-second delta window can
straddle that boundary. The invariant that actually holds - every draw reaching
`render()` either loses its pipeline or is issued - is exact over cumulative
totals, and the check now uses those.

## 2026-09-20: the model-view matrices were never the problem

The 2D menu renders and the 3D world does not, and the 3D world is the only
thing that needs model-view matrices. They arrive through indexed XF loads, and
a rejection there was reported only `#ifndef NDEBUG` - silent in every Release
build these measurements are taken with. So it was counted.

First reading:

```
indexed XF 1s: posMtx=0 texMtx=0 nrmMtx=8426 light=0 rejected=8426
```

`nrmMtx` and `rejected` identical, every second, and `posMtx` zero. Not a
coincidence: `copy_xf_data`'s position-matrix branch **writes the matrix and
then falls through the whole if-chain to the final `return false`** - it is the
only branch missing its `return true`. Nothing broke, because the only consumer
was a debug-only log, but a probe built on that return value counted every
position matrix as rejected, which reads as "model-view matrices never arrive"
when they always had. Seventh false reading of this project, and the second
caused by the instrument rather than by the thing measured.

With the `return true` restored:

```
indexed XF 1s: posMtx=7689 texMtx=0 nrmMtx=7689 light=0 rejected=0
               | pnMtx[0] row0=(0.074 -0.091 0.993 -194.530)
```

One position and one normal matrix per draw, nothing rejected - and the line
also prints the matrix the draws are actually using, because a count cannot
tell a real view matrix from a zeroed one. It is a plausible view matrix with a
real translation, and it changes every second.

**So matrices are not the cause.** Together with the draw-fate counter, that
now rules out: the pipeline cache, draws failing to reach the GPU, the
projection, the viewport, and the model-view matrices. What remains untested is
the per-draw raster state - depth, alpha, culling, blending, scissor - and
which render pass the draws land in.

## 2026-09-20: the 3D state is healthy, and the screen is still black

### Correcting an earlier claim

"`numTevStages` never exceeds 1" was **wrong**. It came from the `genMode`
probe, which only ever captured the early 2D values. Dumping each distinct
raster configuration the perspective draws actually use gives **24 of them**,
with up to **5 TEV stages and 4 texgens**. Eighth false reading in this project,
and the third caused by the instrument rather than the thing measured.

### What the 3D draws look like

```
3D raster state: colorUpdate=true depthCompare=true depthUpdate=true depthFunc=3
                 cull=2 blend=1 alphaComp0=7 alphaComp1=7 tevStages=5 texGens=4
```

`colorUpdate` true, a normal depth function, alpha compare ALWAYS on the bulk of
them, real multi-stage materials. A handful have `colorUpdate=false`, which is
what a depth-only pass looks like and is expected. Nothing here rejects a draw.

### And the passes

```
draw passes 1s: onscreen=3861 offscreen=0 no_pass=0 | passes this frame=1
```

Every draw lands in the single onscreen pass. No offscreen pass is ever
created, so the "rendered into a target that is never resolved" theory - which
`GXCopyTex` not being bridged made plausible - does not apply.

### Where that leaves it

Ruled out **by measurement**, not by argument: the pipeline cache, draws failing
to reach the GPU, the projection, the viewport, the model-view matrices, the
per-draw raster state, and the render pass. The window is genuinely black -
four captures over 70 seconds are byte-identical, the window is frontmost, and
the capture is now window-only.

Everything the port can see about these draws is healthy. What has **not** been
looked at is the vertex data itself: the positions the shader reads out of the
indexed arrays. The arrays are resolved through this port's own resolver and
indexed by the guest's indices, so a wrong base or stride would place every
vertex somewhere impossible while leaving every piece of state above perfectly
valid. That is the next measurement.

## 2026-09-20: nothing rasterises, and that changes the question

### The vertex data is fine

Dumped raw rather than decoded in-place, because a decoder written next to the
thing it measures can be wrong in the same way:

```
3D vtx dump: prim=0x98 vtxCount=10 vtxSize=10 | POS desc=3 cnt=1 type=3 frac=8
             | array stride=6
array first 32 bytes: f7 54 fa ad 1a 0d  fa 94 fd be 12 5b ...
```

`desc=3` is GX_INDEX16, `cnt=1` XYZ, `type=3` s16, `frac=8`, and a stride of 6
is exactly 3 x s16 - all consistent. Decoding the array's first vertex by hand:
`0xf754, 0xfaad, 0x1a0d` over 256 gives **(-8.67, -5.32, 26.05)**, plausible
model-space coordinates.

### The decisive test

With the fragment shader forced to return solid magenta, and separately with
clip coordinates clamped into the visible volume, the window contains **zero
non-black pixels** across every capture. Pipelines still compile and draws are
still issued (`pipeline_not_ready=0`, `issued=3778`), so this is not a broken
shader.

**No fragment is rasterised at all.** Everything measured so far - pipelines,
draws reaching the GPU, matrices, projection, viewport, raster state, render
pass, vertex data - was necessary but not sufficient, and saying the geometry
was "healthy" was reading a set of green lights as a conclusion.

### Two things that are not the cause

`empty_total=0`: no draw is issued with a zero vertex or index count.

The counter's self-check fired on every line and was right to: `report_draw_fate`
ran at the *top* of `render()`, counting the in-flight draw in `reached_render`
before it had been classified as issued or dropped, so the invariant was off by
exactly one for ever. Ninth false reading here, fourth from the instrument. It
reports after classification now and is consistent.

### The new lead

The render target changes size mid-run:

```
draw passes 1s: ... | scissor=(0,0 1280x896) viewport=(4,4 1280x896)
draw passes 1s: ... | scissor=(0,0 512x512)  viewport=(4,4 512x512)
```

512x512 is a render-to-texture size, not the framebuffer, and the pass is still
reported as onscreen with one pass per frame. A viewport offset of (4,4) is odd
too. That is where to look next - not at another piece of per-draw state, all
of which is now measured.

## 2026-09-20: proving the "nothing rasterises" result, and nearly not proving it

The previous section claimed no fragment is rasterised, on the strength of a
forced-magenta fragment shader leaving the screen black. That claim was made
**without a positive control**, and it nearly rested on a false premise.

### The cache the deletions never touched

Before each shader test the pipeline cache was deleted from
`%APPDATA%\dolphinjet\DolphinJet\`. That path does not exist. The game writes
its caches to
`AppData\Local\Packages\OpenAI.Codex_.../LocalCache\Roaming\dolphinjet\`, a
store-redirected path inherited from the environment it is launched in, and
`dawn_cache.db` there was being updated during the runs. Dawn caches compiled
shaders, so every shader-level test could have been measuring the *previous*
build.

Deleting both caches at the real path and repeating: still zero magenta.

### The control that actually settles it

Running the same magenta build with **no input at all**, so the intro movie
plays:

| | magenta pixels |
| --- | --- |
| intro (no input) | **1,130,038** |
| after pressing A past the menu | **0** |

The instrument is proven to work by the first row, and the second row is
therefore a real measurement rather than an assumption. Tenth false reading
avoided, and the first one caught by asking "does my instrument do anything at
all?" before believing its silence.

### What is now established

The same pipeline that renders the intro entirely renders **not one fragment**
after the game leaves the file-select menu, while submitting ~3,800 draws a
second whose pipelines are ready, whose matrices, projection, viewport, raster
state, vertex data and render pass are all verified, none of which are empty
and only 12% of which have their colour write mask closed.

Draws are issued and nothing is rasterised. That is the shape of a transform
producing degenerate or out-of-range clip coordinates for every vertex, and the
next measurement is the clip-space output itself rather than its inputs - all
of which have now been checked one at a time and are individually valid.

## 2026-09-20: the clip-space depth is outside WebGPU's range

Redoing the vertex shader's own transform on the CPU, with the same matrices
and a known vertex, and in both multiplication conventions - no GPU readback
needed, which is what made it worth doing before the heavier probe:

```
3D cpu transform: vtx0=(-8.672 -5.324 26.051) pnMtx=0
   clip(row) = (-27.267 42.003 -1.000 w=64.967) -> ndc (-0.420 0.647 -0.015)
   proj rows: (1.524 0 0 0) (0 2.050 0 0) (0 0 -0.000 -10.000) (0 0 -1.000 0)
```

The row-vector convention - the one the generated WGSL uses - gives a sane
x and y: **ndc (-0.420, 0.647)** is comfortably inside the visible square. The
column-vector alternative gives (0.012, -0.002), nearly degenerate, so the
convention in the shader is the right one.

**But ndc z is -0.015.** OpenGL and the GameCube clip depth to [-1, 1];
**WebGPU clips to [0, 1]**. A vertex at z = -0.015 is outside the clip volume
and is discarded before rasterisation.

And it is not one unlucky vertex. In that projection `m2[2]` is zero, so clip z
does not depend on the vertex at all - it is `m3[2]` = -1.0 for **every** vertex
in the draw, giving a negative ndc z for all of them. That is precisely the
shape of what has been measured all along: draws issued, pipelines ready, every
input valid, and not one fragment rasterised.

Nothing in the generated vertex shader remaps depth: it is
`out.pos = vec4f(mv_pos, 1.0) * ubuf.proj;` and no more.

**Stated as a lead, not a conclusion.** What is measured is one vertex of one
draw landing at ndc z = -0.015 with a projection whose z row makes that constant.
What is not yet measured is whether every 3D draw shares that projection, and
why the 2D paths - which do render - survive it.
