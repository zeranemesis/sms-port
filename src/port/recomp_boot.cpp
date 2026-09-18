#include "port/recomp_boot.h"

// generated/generated.h and generated/generated_symbols.h only exist once
// tools/port/recompile.py has run against a real GMSP01 dump (see
// CMakeLists.txt's DOLPHINJET_GENERATED_GAME_FILES glob) - guard this
// file's real content on the same macro CMakeLists.txt defines exactly
// when that's true, so a fresh checkout / CI without a dump still builds
// dolphinjet (menu shell only) against the stub below.
#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME

#include "port/recomp_dolphin_sdk.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <dolphin/dvd.h>

// Like extern/dolrecomp's own cpu.h (see recomp_host.h), generated.h has no
// extern "C" guard of its own: every func_<address> it declares is plain C
// (chunk_*.c is compiled by the C compiler in the game_recompiled target),
// but including it unguarded from this .cpp file C++-mangles those
// declarations, so none of them match the plain-C symbols the linker
// actually has - verified as the real cause of a build's worth of
// "LNK2001: unresolved external symbol func_<address>" errors, one per
// chunk, the first time this file was written without this wrapper.
extern "C" {
#include "generated.h"
#include "generated_symbols.h"
}

#include <cstring>
#include <iterator>
#include <unordered_map>

namespace sms::recomp {
namespace {

aurora::Module Log("sms::recomp::boot");

// dr_cpu's plain C backend implements the well-known architectural SPRs
// (LR, CTR, XER...) inline in generated code, and a few Gekko-specific
// ones natively in CPUState (hid2 - see cpu.h), but falls back to
// CPUState::instruction_fallback for others - verified empirically: the
// first real boot attempt against the GMSP01 dump hit "mfhid0 r3" at
// 0x8033b90c (early OS/cache-init code any GameCube game runs before
// touching game logic) and, with no fallback installed, that became an
// unhandled PPC_PROGRAM_ILLEGAL exception at vector 0x700 - there being no
// exception-vector code in the translated DOL for the CPU to jump to
// (real hardware's IPL would have installed a handler there; nothing here
// emulates the IPL boot sequence).
//
// HID0 and its kin (L2CR, WPAR, MMCR*, THRM*, ICTC...) configure hardware
// (caches, performance counters, thermal sensors) this host doesn't model
// and whose values the game never inspects for actual logic decisions -
// only real GameCube diagnostic/init code reads them back, typically just
// to OR in a documented bit and write it back. Treating every such SPR as
// a plain read/write-back storage cell (last value written, 0 initially)
// is the same stubbing real hardware-abstraction layers commonly do for
// this exact class of register - not a guess at what the register does,
// a decision to not emulate what it does because nothing here depends on
// it doing anything.
std::unordered_map<u32, u32> &unmodeled_spr_storage()
{
    static std::unordered_map<u32, u32> storage;
    return storage;
}

// Decodes only what's needed to tell mfspr/mtspr apart from every other
// instruction that might reach this fallback (see the PowerPC ISA's
// XFX-form encoding: primary opcode 31, spr split across bits 11-20 with
// its two 5-bit halves swapped, secondary opcode in bits 21-30) - verified
// by hand-decoding 0x7C70FAA6 ("mfhid0 r3" per dolrecomp's own decoder
// comment) to primary=31, rD=3, spr=1008 (HID0), secondary=339 (mfspr),
// matching the documented encoding exactly.
void handle_instruction_fallback(CPUState *cpu, u32 raw, u32 cia)
{
    const u32 primaryOp = raw >> 26;
    const u32 rD_rS = (raw >> 21) & 0x1Fu; // bits 6-10 (X-form rD/rS, XFX-form rD/rS)
    const u32 rA = (raw >> 16) & 0x1Fu;    // bits 11-15 (X-form rA; XFX-form spr low 5 bits)
    const u32 rB = (raw >> 11) & 0x1Fu;    // bits 16-20 (X-form rB; XFX-form spr high 5 bits)
    const u32 secondaryOp = (raw >> 1) & 0x3FFu; // bits 21-30

    if (primaryOp == 31 && secondaryOp == 339) { // mfspr
        // XFX-form's spr field packs its two 5-bit halves in reverse order
        // (bits 11-15 = low 5 bits, bits 16-20 = high 5 bits) - rA/rB above
        // are exactly those two fields, already split out.
        const u32 spr = (rB << 5) | rA;
        cpu->gpr[rD_rS] = unmodeled_spr_storage()[spr];
        cpu->pc = cia + 4;
        return;
    }
    if (primaryOp == 31 && secondaryOp == 467) { // mtspr
        const u32 spr = (rB << 5) | rA;
        unmodeled_spr_storage()[spr] = cpu->gpr[rD_rS];
        cpu->pc = cia + 4;
        return;
    }

    // Cache-management hints (dcbf/dcbst/dcbi/dcbt/dcbtst/icbi): no cache is
    // modeled here, so there is nothing for them to do - verified against
    // dr_cpu's own ppc_cache_control() (extern/dolrecomp/src/cpu/cpu.c),
    // whose default behavior with no cpu->cache_control installed is
    // exactly a no-op for all of these (bar dcbi in user mode, a privilege
    // exception dolphinjet's supervisor-mode CPUState never triggers).
    // dcbz is different and NOT safe to no-op: real GameCube code uses it
    // to fast-zero 32-byte-aligned buffers, an effect games can observe.
    if (primaryOp == 31 &&
        (secondaryOp == 86 ||  // dcbf
         secondaryOp == 54 ||  // dcbst
         secondaryOp == 470 || // dcbi
         secondaryOp == 278 || // dcbt
         secondaryOp == 246 || // dcbtst
         secondaryOp == 982    // icbi
        )) {
        cpu->pc = cia + 4;
        return;
    }
    if (primaryOp == 31 && secondaryOp == 1014) { // dcbz
        const u32 ea = ((rA != 0 ? cpu->gpr[rA] : 0u) + cpu->gpr[rB]) & ~0x1Fu;
        for (u32 offset = 0; offset < 32; offset += 4) {
            mem_write32(cpu, ea + offset, 0);
        }
        cpu->pc = cia + 4;
        return;
    }

    Log.error("instruction_fallback: unhandled raw={:#010x} at pc={:#010x} (primary={} secondary={})", raw, cia, primaryOp, secondaryOp);
    ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL, cia);
}

// The `sc` (system call) instruction always raises PPC_EXC_SYSTEM_CALL,
// architecturally - real hardware has no default handler of its own; the
// IPL/BS2 bootrom installs a minimal one at the vector (0xC00) before ever
// jumping to a game, matching this port's dr_cpu-based CPUState having no
// such vector translated either (DolRecomp translated the DOL, not the
// IPL). Verified hitting this for real, inside DCFlushRange
// (generated_symbols.h: DOLRECOMP_SYMBOL_DCFlushRange 0x8033B80C, size
// 0x34, our fault address 0x8033b83c falls inside that range) - a GameCube
// SDK cache-flush routine, not application code expecting a real syscall
// ABI, consistent with `sc` here being the documented "debugger trap
// point" convention: harmless to skip when nothing is attached to catch
// it. `ppc_rfi` (extern/dolrecomp/src/cpu/cpu.c) restores msr from srr1
// and sets pc = srr0 exactly - since srr0 was left pointing AT the `sc`
// itself (not past it, unlike some architectures' trap instructions),
// calling it unmodified would fault on the same instruction again forever;
// bumping srr0 past the 4-byte `sc` first is the standard PowerPC
// exception-handler convention for "continue after this trap".
bool handle_system_call(CPUState *cpu, u32 /*address*/)
{
    cpu->srr0 += 4;
    ppc_rfi(cpu, cpu->pc);
    return true;
}

// GameCube DOL header: 7 text + 11 data sections, all big-endian u32 at
// fixed offsets - verified against extern/dolrecomp's own parser
// (extern/dolrecomp/src/frontend/container/dol.c, dol_load()). That parser
// reads from a file path via fopen, but DVDGetDOLLocation hands back the
// raw bytes nod already read off the mounted disc image, not a path - so
// this reads the same fixed layout directly out of that in-memory buffer
// instead of writing it to a temp file just to reuse dol_load().
u32 read_be32(const u8 *base, u32 offset)
{
    return (u32(base[offset]) << 24) | (u32(base[offset + 1]) << 16) |
           (u32(base[offset + 2]) << 8) | u32(base[offset + 3]);
}

bool load_dol_section(CPUState *cpu, const u8 *dol, u32 dolSize, const char *label, int index,
    u32 offset, u32 address, u32 size)
{
    if (size == 0) {
        return true;
    }
    if (u64(offset) + size > dolSize) {
        Log.error("DOL {} section {} (offset={:#x} size={:#x}) runs past the {}-byte DOL", label, index, offset, size, dolSize);
        return false;
    }
    if (address < GC_RAM_BASE || u64(address - GC_RAM_BASE) + size > cpu->ram_size) {
        Log.error("DOL {} section {} (address={:#x} size={:#x}) falls outside the {}MB guest RAM", label, index, address, size, cpu->ram_size / (1024 * 1024));
        return false;
    }
    std::memcpy(cpu->ram + (address - GC_RAM_BASE), dol + offset, size);
    return true;
}

bool load_dol_into_ram(CPUState *cpu, const u8 *dol, u32 dolSize)
{
    if (dolSize < 0x100) {
        Log.error("DOL too small to hold a header ({} bytes)", dolSize);
        return false;
    }

    for (int i = 0; i < 7; ++i) {
        const u32 offset = read_be32(dol, 0x00 + u32(i) * 4);
        const u32 address = read_be32(dol, 0x48 + u32(i) * 4);
        const u32 size = read_be32(dol, 0x90 + u32(i) * 4);
        if (!load_dol_section(cpu, dol, dolSize, "text", i, offset, address, size)) {
            return false;
        }
    }
    for (int i = 0; i < 11; ++i) {
        const u32 offset = read_be32(dol, 0x1C + u32(i) * 4);
        const u32 address = read_be32(dol, 0x64 + u32(i) * 4);
        const u32 size = read_be32(dol, 0xAC + u32(i) * 4);
        if (!load_dol_section(cpu, dol, dolSize, "data", i, offset, address, size)) {
            return false;
        }
    }

    // BSS is left as-is: cpu_init() calloc's cpu->ram, so it already reads
    // as zero, matching what a fresh GameCube boot's BSS clear produces.
    return true;
}

} // namespace

bool boot_game(CPUState *cpu)
{
    DVDDiskID *diskId = DVDGetCurrentDiskID();
    if (diskId == nullptr) {
        Log.error("no disc mounted - open the configured disc image before booting");
        return false;
    }
    if (std::memcmp(diskId->gameName, "GMSP", sizeof(diskId->gameName)) != 0) {
        Log.error("mounted disc's game id doesn't start with GMSP - generated/ was built from a GMSP01 dump, this disc is not it");
        return false;
    }

    s32 dolSize = 0;
    const u8 *dol = DVDGetDOLLocation(&dolSize);
    if (dol == nullptr || dolSize <= 0) {
        Log.error("DVDGetDOLLocation returned no DOL");
        return false;
    }

    if (!cpu_init(cpu)) {
        Log.error("cpu_init failed");
        return false;
    }

    if (!load_dol_into_ram(cpu, dol, u32(dolSize))) {
        cpu_free(cpu);
        return false;
    }

    install_host_calls(cpu);
    cpu->instruction_fallback = &handle_instruction_fallback;
    // PPC_VECTOR_SYSTEM_CALL (sc) is handled directly in step_game(), not
    // through this table - see its own comment for why registering it
    // here like a normal host call doesn't work.

    static const dolphin_sdk::NamedAddress kKnownDolphinSdkCalls[] = {
        { "OSReport", DOLRECOMP_SYMBOL_OSReport },
    };
    dolphin_sdk::register_known_dolphin_sdk_calls(kKnownDolphinSdkCalls, std::size(kKnownDolphinSdkCalls));

    cpu->pc = DOLRECOMP_ENTRY_POINT;
    Log.info("boot: loaded {} byte DOL, entry={:#010x}", dolSize, cpu->pc);
    return true;
}

bool step_game(CPUState *cpu, unsigned maxBlocks)
{
    // dolrecomp_run_blocks (generated.h) stops as soon as CPUState::exception
    // is non-zero, *before* dispatching to the vector address it just set
    // cpu->pc to - verified: registering a host-call handler for
    // PPC_VECTOR_SYSTEM_CALL through recomp_host.h's normal table never
    // fired, because the loop bails out on ctx->exception first, so
    // dolrecomp_call(ctx, 0xC00) - where that handler would have been
    // looked up - is never reached. Handling PPC_EXC_SYSTEM_CALL here
    // instead, directly, then clearing the flag and re-running
    // dolrecomp_run_blocks for the same budget, is what actually reaches
    // handle_system_call. Capped retry count, not the block budget itself,
    // guards against a hypothetical sc-in-a-tight-loop from stalling a
    // frame - each retry already ran up to maxBlocks real blocks first.
    for (int retry = 0; retry < 64; ++retry) {
        if (dolrecomp_run_blocks(cpu, maxBlocks)) {
            return true;
        }
        if (cpu->exception != PPC_EXC_SYSTEM_CALL) {
            if (cpu->exception) {
                Log.warn("step_game: CPU exception {:#x} (program_exception cause={:#x}) at srr0={:#010x}, vector pc={:#010x}",
                    cpu->exception, cpu->program_exception, cpu->srr0, cpu->pc);
            } else {
                Log.warn("step_game: halted at pc={:#010x} (unresolved call)", cpu->pc);
            }
            return false;
        }
        cpu->exception = 0;
        handle_system_call(cpu, cpu->pc);
    }
    Log.warn("step_game: gave up after repeated system-call traps near pc={:#010x}", cpu->pc);
    return false;
}

} // namespace sms::recomp

#else // !DOLPHINJET_HAVE_RECOMPILED_GAME

namespace sms::recomp {

bool boot_game(CPUState *)
{
    return false;
}

bool step_game(CPUState *, unsigned)
{
    return false;
}

} // namespace sms::recomp

#endif
