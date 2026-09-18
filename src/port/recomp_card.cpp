#include "port/recomp_card.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <dolphin/card.h>

#include <cstring>
#include <vector>

namespace sms::recomp::card {
namespace {

aurora::Module Log("sms::recomp::card");

// Set only inside host_call_card_init, after the real CARDInit() call
// returns - deliberately independent of Aurora's own internal
// `Initialized` flag (card.cpp), so this stays correct regardless of
// whether some other, unrelated code path already initialized Aurora's
// CARD module first.
bool g_cardInitialized = false;

bool host_call_card_init(CPUState *cpu, u32)
{
    const std::string game = read_guest_cstring(cpu, cpu->gpr[3]);
    const std::string maker = read_guest_cstring(cpu, cpu->gpr[4]);
    CARDInit(game.c_str(), maker.c_str());
    g_cardInitialized = true;
    Log.info("CARDInit(\"{}\", \"{}\")", game, maker);
    return true;
}

bool host_call_card_mount(CPUState *cpu, u32)
{
    // workArea (r4) and detachCallback (r5) are [[maybe_unused]] in
    // Aurora's real CARDMount - verified reading card.cpp - so they are
    // never dereferenced/called and can be passed through as null/ignored
    // without any guest<->host pointer translation risk.
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    cpu->gpr[3] = static_cast<u32>(CARDMount(chan, nullptr, nullptr));
    Log.info("CARDMount(chan={}) -> {}", chan, static_cast<s32>(cpu->gpr[3]));
    return true;
}

bool host_call_card_mount_async(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    cpu->gpr[3] = static_cast<u32>(CARDMountAsync(chan, nullptr, nullptr, nullptr));
    Log.info("CARDMountAsync(chan={}) -> {}", chan, static_cast<s32>(cpu->gpr[3]));
    return true;
}

bool host_call_card_probe_ex(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const u32 memSizeAddr = cpu->gpr[4];
    const u32 sectorSizeAddr = cpu->gpr[5];

    if (!g_cardInitialized) {
        Log.warn("CARDProbeEx(chan={}) called before CARDInit - returning CARD_RESULT_NOCARD without touching Aurora's CARD state", chan);
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_NOCARD);
        return true;
    }

    s32 memSize = 0;
    s32 sectorSize = 0;
    const s32 result = CARDProbeEx(chan, &memSize, &sectorSize);
    // memSizeAddr/sectorSizeAddr are guest addresses, not valid host
    // pointers - write the results back through guest memory rather than
    // handing Aurora's real out-params directly to the guest.
    mem_write32(cpu, memSizeAddr, static_cast<u32>(memSize));
    mem_write32(cpu, sectorSizeAddr, static_cast<u32>(sectorSize));
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDProbeEx(chan={}) -> {} (memSize={} sectorSize={})", chan, result, memSize, sectorSize);
    return true;
}

} // namespace

void register_known_card_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    for (size_t i = 0; i < count; ++i) {
        const char *name = addresses[i].name;
        HostCallFn fn = nullptr;
        if (std::strcmp(name, "CARDInit") == 0) {
            fn = &host_call_card_init;
        } else if (std::strcmp(name, "CARDMount") == 0) {
            fn = &host_call_card_mount;
        } else if (std::strcmp(name, "CARDMountAsync") == 0) {
            fn = &host_call_card_mount_async;
        } else if (std::strcmp(name, "CARDProbeEx") == 0) {
            fn = &host_call_card_probe_ex;
        }
        if (fn) {
            entries.push_back({ addresses[i].address, name, fn });
        }
        // The rest of the CARD family (CARDRead/CARDWrite, CARDFormat,
        // CARDCheck, CARDGetStatus...) is deliberately left unregistered
        // until a real call site is observed - same "don't guess ahead of
        // a real call site" discipline as PADRead
        // (include/port/recomp_dolphin_sdk.h).
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

} // namespace sms::recomp::card
