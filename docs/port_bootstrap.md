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
