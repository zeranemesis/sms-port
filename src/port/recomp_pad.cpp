#include "port/recomp_pad.h"

#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <dolphin/pad.h>

#include <array>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace sms::recomp::pad {
namespace {

aurora::Module Log("sms::recomp::pad");

// PADStatus has 11 bytes of *fields* (include/dolphin/pad.h:47-58) but it
// contains a u16, so it aligns to 2 and the compiler sizes it at 12. The array
// stride is what matters here, and it is not a deduction - the game's own
// linker map states it:
//
//     mPadStatus__10JUTGamePad = .bss:0x803FBBF4; // size:0x30
//     (config/GMSP01/symbols.txt:28810)
//
// That array is `PADStatus mPadStatus[4]` (src/JSystem/JUtility/JUTGamePad.cpp)
// and 0x30 is 48, i.e. 4 x 12.
//
// This was 11, which is right for channel 0 and wrong for every other: channels
// 1-3 landed at +11/+22/+33 instead of +12/+24/+36, so their `button` halfword
// was written to an odd address and every field after it was displaced.
constexpr u32 kGuestPadStatusSize = 12;
constexpr u32 kGuestPadStatusCount = 4;
constexpr u32 kGuestOutputSize = kGuestPadStatusSize * kGuestPadStatusCount;
static_assert(kGuestOutputSize == 0x30u, "must match mPadStatus__10JUTGamePad's size:0x30");

bool guest_range_writable(const CPUState *cpu, u32 address, u32 size)
{
    if (address < GC_RAM_BASE) {
        return false;
    }
    const u64 offset = static_cast<u64>(address) - GC_RAM_BASE;
    return offset + size <= cpu->ram_size;
}

void write_guest_status(CPUState *cpu, u32 address, const PADStatus &status)
{
    // The exact guest layout is documented by the decomp's
    // include/dolphin/pad.h: button at 0, four sticks at 2..5, four analog
    // values at 6..9 and signed error at 10.  Aurora's extButton is not in
    // the GameCube ABI and is intentionally not copied.
    mem_write16(cpu, address, status.button);
    mem_write8(cpu, address + 2, static_cast<u8>(status.stickX));
    mem_write8(cpu, address + 3, static_cast<u8>(status.stickY));
    mem_write8(cpu, address + 4, static_cast<u8>(status.substickX));
    mem_write8(cpu, address + 5, static_cast<u8>(status.substickY));
    mem_write8(cpu, address + 6, status.triggerLeft);
    mem_write8(cpu, address + 7, status.triggerRight);
    mem_write8(cpu, address + 8, status.analogA);
    mem_write8(cpu, address + 9, status.analogB);
    mem_write8(cpu, address + 10, static_cast<u8>(status.err));
}

bool host_call_pad_init(CPUState *cpu, u32)
{
    const bool ok = PADInit();

    // Without this the port is unplayable on a machine with no gamepad, which
    // is the machine it is developed on. Aurora's PADRead reports
    // PAD_ERR_NO_CONTROLLER for a port with no physical controller, no virtual
    // pad and no keyboard bindings (extern/aurora/lib/dolphin/pad/pad.cpp:694),
    // and the game then sees four dead ports - the on-screen "NO CONTROLLER
    // ASSIGNED" notice.
    //
    // Activating alone is not enough: PADKeyboardState's mappings are
    // value-initialised, not defaulted, so an active port with an all-zero
    // table reports PAD_ERR_NONE and no buttons. PADClearKeyBindings is what
    // installs g_defaultKeys/g_defaultKeyAxis, so it has to come first.
    //
    // This does not overwrite a configuration the player made: Aurora loads
    // keyboard_bindings.dat lazily inside PADRead (pad.cpp:678), which runs
    // after PADInit, so a saved file still wins over these defaults.
    constexpr u32 kKeyboardPort = 0;
    PADClearKeyBindings(kKeyboardPort);
    PADSetKeyboardActive(kKeyboardPort, TRUE);

    cpu->gpr[3] = ok ? 1u : 0u;
    return true;
}

bool host_call_pad_read(CPUState *cpu, u32)
{
    const u32 guestStatus = cpu->gpr[3];
    if (!guest_range_writable(cpu, guestStatus, kGuestOutputSize)) {
        // Returning here used to set r3 = 0 and report success, which is the
        // worst of both: nothing is written, and 0 is PADRead's "no channel is
        // resetting" answer, so the guest reads four stale or zeroed structs
        // and believes them. Hand the call back instead - dolrecomp falls
        // through to the guest's own PADRead, which at least fails honestly
        // against its own uninitialised state.
        //
        // Bounded to one line per distinct pointer AND a hard count, because
        // PADRead runs once per frame: an unbounded warning here is 60
        // lines/second forever, which is how this project has produced five
        // runaway logs already.
        static std::unordered_set<u32> warned;
        if (warned.size() < 8 && warned.insert(guestStatus).second) {
            Log.warn("PADRead guest output {:#010x} is outside guest RAM - not bridged", guestStatus);
        }
        return false;
    }

    std::array<PADStatus, kGuestPadStatusCount> hostStatus {};
    const u32 result = PADRead(hostStatus.data());
    for (u32 i = 0; i < kGuestPadStatusCount; ++i) {
        write_guest_status(cpu, guestStatus + i * kGuestPadStatusSize, hostStatus[i]);
    }
    cpu->gpr[3] = result;
    return true;
}

bool host_call_pad_reset(CPUState *cpu, u32)
{
    cpu->gpr[3] = PADReset(cpu->gpr[3]) ? 1u : 0u;
    return true;
}

bool host_call_pad_recalibrate(CPUState *cpu, u32)
{
    cpu->gpr[3] = PADRecalibrate(cpu->gpr[3]) ? 1u : 0u;
    return true;
}

bool host_call_pad_control_motor(CPUState *cpu, u32)
{
    PADControlMotor(cpu->gpr[3], cpu->gpr[4]);
    return true;
}

bool host_call_pad_set_analog_mode(CPUState *cpu, u32)
{
    PADSetAnalogMode(cpu->gpr[3]);
    return true;
}

} // namespace

void register_known_pad_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    for (size_t i = 0; i < count; ++i) {
        HostCallFn fn = nullptr;
        const char *name = addresses[i].name;
        if (std::strcmp(name, "PADInit") == 0) {
            fn = &host_call_pad_init;
        } else if (std::strcmp(name, "PADRead") == 0) {
            fn = &host_call_pad_read;
        } else if (std::strcmp(name, "PADReset") == 0) {
            fn = &host_call_pad_reset;
        } else if (std::strcmp(name, "PADRecalibrate") == 0) {
            fn = &host_call_pad_recalibrate;
        } else if (std::strcmp(name, "PADControlMotor") == 0) {
            fn = &host_call_pad_control_motor;
        } else if (std::strcmp(name, "PADSetAnalogMode") == 0) {
            fn = &host_call_pad_set_analog_mode;
        }
        if (fn) {
            entries.push_back({ addresses[i].address, name, fn });
        }
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

bool run_pad_self_test()
{
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("PAD self-test: cpu_init failed");
        return false;
    }

    constexpr u32 kGuestOutput = GC_RAM_BASE + 0x1000u;
    PADStatus source {};
    // A second channel, written through the same per-channel stride the real
    // bridge uses. The previous version of this test round-tripped one channel
    // only, which meant it passed while asserting the exact constant that was
    // wrong - a green test for a real bug. Channel 1 landing at +12 is the
    // property that was actually broken, so that is what gets asserted.
    PADStatus second {};
    source.button = PAD_BUTTON_A | PAD_TRIGGER_L;
    source.stickX = -42;
    source.stickY = 71;
    source.substickX = -3;
    source.substickY = 8;
    source.triggerLeft = 12;
    source.triggerRight = 34;
    source.analogA = 56;
    source.analogB = 78;
    source.err = PAD_ERR_NO_CONTROLLER;
    second.button = PAD_BUTTON_B;
    second.stickX = 99;
    second.err = PAD_ERR_NONE;

    for (u32 i = 0; i < kGuestPadStatusCount; ++i) {
        write_guest_status(&cpu, kGuestOutput + i * kGuestPadStatusSize, i == 1 ? second : source);
    }

    const bool layoutPreserved = mem_read16(&cpu, kGuestOutput) == source.button &&
                                 static_cast<s8>(mem_read8(&cpu, kGuestOutput + 2)) == source.stickX &&
                                 static_cast<s8>(mem_read8(&cpu, kGuestOutput + 3)) == source.stickY &&
                                 mem_read8(&cpu, kGuestOutput + 9) == source.analogB &&
                                 static_cast<s8>(mem_read8(&cpu, kGuestOutput + 10)) == source.err;

    // The stride assertion: channel 1 must start at +12, and its button must be
    // readable as an aligned halfword there.
    const bool stridePreserved = mem_read16(&cpu, kGuestOutput + 12) == second.button &&
                                 static_cast<s8>(mem_read8(&cpu, kGuestOutput + 12 + 2)) == second.stickX &&
                                 mem_read16(&cpu, kGuestOutput + 24) == source.button;
    cpu_free(&cpu);

    if (!layoutPreserved) {
        Log.error("PAD self-test: guest PADStatus marshalling does not preserve the GameCube layout");
        return false;
    }
    if (!stridePreserved) {
        Log.error("PAD self-test: per-channel stride is wrong - channel 1 must start at +12, "
                  "matching mPadStatus__10JUTGamePad's size:0x30 for four channels");
        return false;
    }
    Log.info("PAD self-test: 12-byte-stride guest PADStatus marshalling works across all four channels");
    return true;
}

} // namespace sms::recomp::pad
