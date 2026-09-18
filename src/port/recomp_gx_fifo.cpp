#include "port/recomp_gx_fifo.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"
#include "aurora/lib/gx/fifo.hpp"

#include <dolphin/gx/GXFifo.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXManage.h>

#include <cstring>
#include <vector>

namespace sms::recomp::gx_fifo {
namespace {

aurora::Module Log("sms::recomp::gx_fifo");

// The real GX write-gather-pipe on hardware. Verified: dr_cpu's
// mem_write8/16/32/64 (extern/dolrecomp/src/cpu/cpu.c) already route any
// store to an address resolve_addr() can't map into guest RAM through
// CPUState::external_write - this range being outside GC_RAM_BASE/MEM1/
// MEM2 means every FIFO store the recompiled game executes reaches this
// handler as soon as it's registered, no other CPU-side change needed.
constexpr u32 kFifoBase = 0xCC008000u;
constexpr u32 kFifoEnd = 0xCC009000u;

void write(CPUState *, u32 /*addr*/, u64 value, u8 size)
{
    // The write-gather pipe ignores the specific address within the range
    // on real hardware (and aurora::gx::fifo's write_u8/u16/u32 API takes
    // no address either) - only the value and size matter.
    //
    // `value` here is dr_cpu's plain host-native integer (verified reading
    // mem_write32: it passes the same `value` it would otherwise
    // write_be32() into mapped RAM, not pre-byte-swapped) - exactly what
    // aurora::gx::fifo::write_u16/write_u32 expect, since they apply their
    // own bswap() internally before appending to the FIFO buffer
    // (extern/aurora/lib/gx/fifo.hpp).
    switch (size) {
        case 1:
            aurora::gx::fifo::write_u8(static_cast<u8>(value));
            break;
        case 2:
            aurora::gx::fifo::write_u16(static_cast<u16>(value));
            break;
        case 4:
            aurora::gx::fifo::write_u32(static_cast<u32>(value));
            break;
        case 8:
            // stfd/psq_st path - not yet exercised against a real run, so
            // the split direction (high word first) is a best guess from
            // big-endian byte order, not confirmed.
            aurora::gx::fifo::write_u32(static_cast<u32>(value >> 32));
            aurora::gx::fifo::write_u32(static_cast<u32>(value));
            break;
        default:
            Log.warn("FIFO write: unexpected size {}", size);
            break;
    }
}

} // namespace

void install()
{
    register_mmio_range({
        .base = kFifoBase,
        .end = kFifoEnd,
        .name = "GX FIFO",
        .read = nullptr, // write-only on real hardware; a read here is a
                          // signal something unexpected is happening, let
                          // the generic dispatcher's miss-log path catch it
        .write = &write,
    });
}

namespace {

// The host GXFifoObj* GXInit() really returns (extern/aurora/lib/dolphin/
// gx/GXManage.cpp - GXInit already calls aurora::gx::fifo::init() and
// wires both GXSetCPUFifo/GXSetGPFifo to it internally, verified reading
// that function). The guest never needs the real value - GXFifoObj is an
// opaque `u8 pad[128]` (extern/aurora/include/dolphin/gx/GXFifo.h) that
// Aurora's own GXSetCPUFifo/GXSetGPFifo never dereference (`CPUFifo =
// fifo;` is a plain pointer store) - so it's safe to hand the guest a
// fabricated sentinel instead and translate it back here.
GXFifoObj *g_realFifoObj = nullptr;

// Not a valid guest RAM address (see GC_RAM_BASE/GC_MAIN_RAM_SIZE,
// extern/dolrecomp/src/cpu/cpu.h) - deliberately, so if anything ever did
// try to dereference it as a guest pointer, it would be caught immediately
// as out of range rather than silently reading garbage.
constexpr u32 kFakeFifoHandle = 0xFEEDF1F0u;

bool host_call_gx_init(CPUState *cpu, u32)
{
    g_realFifoObj = GXInit(nullptr, 0);
    cpu->gpr[3] = kFakeFifoHandle;
    Log.info("GXInit -> fifo initialized (fake guest handle {:#010x})", kFakeFifoHandle);
    return true;
}

bool host_call_gx_set_cpu_fifo(CPUState *, u32)
{
    // The guest-supplied handle (r3) is intentionally ignored - see
    // g_realFifoObj's comment. Even a guest value that isn't
    // kFakeFifoHandle is safe, since it's never dereferenced either way.
    GXSetCPUFifo(g_realFifoObj);
    Log.info("GXSetCPUFifo (real fifo object)");
    return true;
}

bool host_call_gx_set_gp_fifo(CPUState *, u32)
{
    GXSetGPFifo(g_realFifoObj);
    Log.info("GXSetGPFifo (real fifo object)");
    return true;
}

bool host_call_gx_set_draw_done(CPUState *, u32)
{
    // This SDK revision's actual symbol is GXSetDrawDone (no callback
    // argument - confirmed present in generated/generated_symbols.h,
    // unlike GXSetDrawDoneCallback, which this dump doesn't have at all).
    // Aurora's real GXSetDrawDone() just synchronously invokes whatever
    // callback GXSetDrawDoneCallback registered (extern/aurora/lib/
    // dolphin/gx/GXManage.cpp) - none has been registered through this
    // bridge, so today this is a safe no-op plus a log line.
    // GXWaitDrawDone (real address 0x80355CBC per generated_symbols.h)
    // has no Aurora implementation at all and isn't bridged yet - expect
    // it to show up as an unresolved-call log miss if/when reached.
    GXSetDrawDone();
    Log.info("GXSetDrawDone");
    return true;
}

bool host_call_gx_draw_done(CPUState *, u32)
{
    GXDrawDone();
    Log.info("GXDrawDone");
    return true;
}

bool host_call_gx_flush(CPUState *, u32)
{
    GXFlush();
    Log.info("GXFlush");
    return true;
}

bool host_call_gx_copy_disp(CPUState *cpu, u32)
{
    // dest (r3) is a guest framebuffer address - not forwarded, since
    // Aurora's real GXCopyDisp is currently an empty stub regardless
    // (verified reading GXFrameBuffer.cpp) and never touches it. clear
    // (r4) is scalar and safe to pass through.
    const GXBool clear = static_cast<GXBool>(cpu->gpr[4]);
    GXCopyDisp(nullptr, clear);
    Log.info("GXCopyDisp(clear={})", static_cast<int>(clear));
    return true;
}

} // namespace

void register_known_gx_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    for (size_t i = 0; i < count; ++i) {
        const char *name = addresses[i].name;
        HostCallFn fn = nullptr;
        if (std::strcmp(name, "GXInit") == 0) {
            fn = &host_call_gx_init;
        } else if (std::strcmp(name, "GXSetCPUFifo") == 0) {
            fn = &host_call_gx_set_cpu_fifo;
        } else if (std::strcmp(name, "GXSetGPFifo") == 0) {
            fn = &host_call_gx_set_gp_fifo;
        } else if (std::strcmp(name, "GXSetDrawDone") == 0) {
            fn = &host_call_gx_set_draw_done;
        } else if (std::strcmp(name, "GXDrawDone") == 0) {
            fn = &host_call_gx_draw_done;
        } else if (std::strcmp(name, "GXFlush") == 0) {
            fn = &host_call_gx_flush;
        } else if (std::strcmp(name, "GXCopyDisp") == 0) {
            fn = &host_call_gx_copy_disp;
        }
        if (fn) {
            entries.push_back({ addresses[i].address, name, fn });
        }
        // Everything else in the GX family relies purely on install()'s
        // external_write forwarding above - only functions that configure
        // Aurora's fifo module or callback wiring, rather than writing
        // FIFO bytes themselves, need a bridge here at all.
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

} // namespace sms::recomp::gx_fifo
