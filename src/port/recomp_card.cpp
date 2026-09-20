#include "port/recomp_card.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <dolphin/card.h>
#include <dolphin/dvd.h>

#include <cstring>
#include <unordered_map>
#include <unordered_set>
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
bool g_cardReady[2] = {};

// The unbridged CARD functions are deliberately allowed to continue into the
// generated SDK code.  Keeping their names here lets their first dynamic use
// be identified without fabricating a host ABI for CARDFileInfo, callbacks,
// or guest buffers before a real game path has demanded one.
std::unordered_map<u32, const char *> g_observedCallNames;
std::unordered_set<u32> g_observedCalls;

bool observe_unbridged_card_call(CPUState *, u32 address)
{
    if (g_observedCalls.insert(address).second) {
        const auto it = g_observedCallNames.find(address);
        Log.info("observed unbridged {} at {:#010x}; continuing in generated CARD SDK",
            it != g_observedCallNames.end() ? it->second : "CARD call", address);
    }
    return false;
}

bool host_call_card_init(CPUState *cpu, u32)
{
    // The GameCube SDK signature is CARDInit(void); its current game/maker
    // come from the disc ID that CARDInit installs into its control blocks.
    // Aurora's PC adaptation needs those fields explicitly, so obtain them
    // from its already-mounted disc instead of treating volatile r3/r4 as
    // strings.  The latter created card paths with random bytes.
    const DVDDiskID *diskId = DVDGetCurrentDiskID();
    char game[sizeof(diskId->gameName) + 1] = "GMSP";
    char maker[sizeof(diskId->company) + 1] = "01";
    if (diskId != nullptr) {
        std::memcpy(game, diskId->gameName, sizeof(diskId->gameName));
        std::memcpy(maker, diskId->company, sizeof(diskId->company));
    } else {
        Log.warn("CARDInit without a mounted disc; falling back to the GMSP01 target ID");
    }
    CARDInit(game, maker);
    g_cardInitialized = true;
    g_cardReady[0] = false;
    g_cardReady[1] = false;
    Log.info("CARDInit from mounted disc (game=\"{}\", maker=\"{}\")", game, maker);
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

bool host_call_card_check(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    cpu->gpr[3] = static_cast<u32>(CARDCheck(chan));
    if (chan >= 0 && chan < 2) {
        g_cardReady[chan] = static_cast<s32>(cpu->gpr[3]) == CARD_RESULT_READY;
    }
    Log.info("CARDCheck(chan={}) -> {}", chan, static_cast<s32>(cpu->gpr[3]));
    return true;
}

bool host_call_card_open(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const std::string fileName = read_guest_cstring(cpu, cpu->gpr[4], CARD_FILENAME_MAX);
    const u32 guestFileInfo = cpu->gpr[5];

    // CARDFileInfo's scalar fields have the same GameCube byte offsets in
    // Aurora, but a guest address is never a host pointer.  Marshal through
    // a host temporary and write its documented 20-byte guest layout back.
    CARDFileInfo fileInfo {};
    const s32 auroraResult = CARDOpen(chan, fileName.c_str(), &fileInfo);
    // CardGciFolder currently reports NOCARD for a missing named file even
    // after its CARDCheck has established that the card itself is ready.
    // The GameCube contract distinguishes those cases: Sunshine needs
    // NOFILE to enter its first-save creation flow.
    const bool missingFile = auroraResult == CARD_RESULT_NOCARD
        && chan >= 0 && chan < 2 && g_cardReady[chan];
    const s32 result = missingFile ? CARD_RESULT_NOFILE : auroraResult;
    if (result == CARD_RESULT_READY) {
        mem_write32(cpu, guestFileInfo + 0x00u, static_cast<u32>(fileInfo.chan));
        mem_write32(cpu, guestFileInfo + 0x04u, static_cast<u32>(fileInfo.fileNo));
        mem_write32(cpu, guestFileInfo + 0x08u, static_cast<u32>(fileInfo.offset));
        mem_write32(cpu, guestFileInfo + 0x0Cu, static_cast<u32>(fileInfo.length));
        mem_write16(cpu, guestFileInfo + 0x10u, fileInfo.iBlock);
    }
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDOpen(chan={}, file=\"{}\") -> {}{}", chan, fileName, result,
        missingFile ? " (normalized missing file)" : "");
    return true;
}

bool host_call_card_free_blocks(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const u32 bytesAddr = cpu->gpr[4];
    const u32 filesAddr = cpu->gpr[5];
    s32 bytes = 0;
    s32 files = 0;
    const s32 result = CARDFreeBlocks(chan, &bytes, &files);
    if (result == CARD_RESULT_READY) {
        mem_write32(cpu, bytesAddr, static_cast<u32>(bytes));
        mem_write32(cpu, filesAddr, static_cast<u32>(files));
    }
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDFreeBlocks(chan={}) -> {} (bytes={} files={})", chan, result, bytes, files);
    return true;
}

bool host_call_card_create(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const std::string fileName = read_guest_cstring(cpu, cpu->gpr[4], CARD_FILENAME_MAX);
    const u32 size = cpu->gpr[5];
    const u32 guestFileInfo = cpu->gpr[6];
    if (size == 0 || size > CARD_MAX_SIZE) {
        // CardGciFolder allocates the complete GCI payload before it can
        // report capacity.  Do the SDK-level size validation at the guest
        // boundary so a corrupt register cannot turn into a multi-gigabyte
        // host allocation.
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_INSSPACE);
        Log.warn("CARDCreate(chan={}, file=\"{}\", bytes={}) exceeds card capacity", chan, fileName, size);
        return true;
    }
    CARDFileInfo fileInfo {};
    const s32 result = CARDCreate(chan, fileName.c_str(), size, &fileInfo);
    if (result == CARD_RESULT_READY) {
        // Sunshine's synchronous create contract only establishes chan and
        // fileNo; the subsequent CARDWrite/CARDClose path consumes those two
        // fields.  Keep the rest of the guest-owned record untouched.
        mem_write32(cpu, guestFileInfo + 0x00u, static_cast<u32>(fileInfo.chan));
        mem_write32(cpu, guestFileInfo + 0x04u, static_cast<u32>(fileInfo.fileNo));
    }
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDCreate(chan={}, file=\"{}\", bytes={}) -> {}", chan, fileName, size, result);
    return true;
}

bool guest_range_valid(const CPUState *cpu, u32 address, u32 bytes)
{
    if (address < GC_RAM_BASE) {
        return false;
    }
    const u64 offset = static_cast<u64>(address) - GC_RAM_BASE;
    return offset + bytes <= cpu->ram_size;
}

bool read_guest_file_info(CPUState *cpu, u32 address, CARDFileInfo *fileInfo)
{
    constexpr u32 kGuestCardFileInfoSize = 0x12u;
    if (!guest_range_valid(cpu, address, kGuestCardFileInfoSize)) {
        Log.warn("CARDFileInfo at {:#010x} is outside guest RAM", address);
        return false;
    }
    fileInfo->chan = static_cast<s32>(mem_read32(cpu, address + 0x00u));
    fileInfo->fileNo = static_cast<s32>(mem_read32(cpu, address + 0x04u));
    fileInfo->offset = static_cast<s32>(mem_read32(cpu, address + 0x08u));
    fileInfo->length = static_cast<s32>(mem_read32(cpu, address + 0x0Cu));
    fileInfo->iBlock = mem_read16(cpu, address + 0x10u);
    return true;
}

bool host_call_card_close(CPUState *cpu, u32)
{
    CARDFileInfo fileInfo {};
    if (!read_guest_file_info(cpu, cpu->gpr[3], &fileInfo)) {
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_FATAL_ERROR);
        return true;
    }
    const s32 result = CARDClose(&fileInfo);
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDClose(chan={}, fileNo={}) -> {}", fileInfo.chan, fileInfo.fileNo, result);
    return true;
}

bool host_call_card_read(CPUState *cpu, u32)
{
    CARDFileInfo fileInfo {};
    const u32 guestFileInfo = cpu->gpr[3];
    const u32 guestBuffer = cpu->gpr[4];
    const s32 length = static_cast<s32>(cpu->gpr[5]);
    const s32 offset = static_cast<s32>(cpu->gpr[6]);
    if (!read_guest_file_info(cpu, guestFileInfo, &fileInfo) || length < 0
        || !guest_range_valid(cpu, guestBuffer, static_cast<u32>(length))) {
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_FATAL_ERROR);
        return true;
    }
    std::vector<u8> buffer(static_cast<size_t>(length));
    const s32 result = CARDRead(&fileInfo, buffer.data(), length, offset);
    if (result == CARD_RESULT_READY) {
        for (s32 i = 0; i < length; ++i) {
            mem_write8(cpu, guestBuffer + static_cast<u32>(i), buffer[static_cast<size_t>(i)]);
        }
    }
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDRead(chan={}, fileNo={}, bytes={}, offset={}) -> {}", fileInfo.chan, fileInfo.fileNo, length, offset, result);
    return true;
}

bool host_call_card_write(CPUState *cpu, u32)
{
    CARDFileInfo fileInfo {};
    const u32 guestFileInfo = cpu->gpr[3];
    const u32 guestBuffer = cpu->gpr[4];
    const s32 length = static_cast<s32>(cpu->gpr[5]);
    const s32 offset = static_cast<s32>(cpu->gpr[6]);
    if (!read_guest_file_info(cpu, guestFileInfo, &fileInfo) || length < 0
        || !guest_range_valid(cpu, guestBuffer, static_cast<u32>(length))) {
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_FATAL_ERROR);
        return true;
    }
    std::vector<u8> buffer(static_cast<size_t>(length));
    for (s32 i = 0; i < length; ++i) {
        buffer[static_cast<size_t>(i)] = mem_read8(cpu, guestBuffer + static_cast<u32>(i));
    }
    const s32 result = CARDWrite(&fileInfo, buffer.data(), length, offset);
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDWrite(chan={}, fileNo={}, bytes={}, offset={}) -> {}", fileInfo.chan, fileInfo.fileNo, length, offset, result);
    return true;
}

constexpr u32 kGuestCardStatSize = 0x6Cu;

void write_guest_card_stat(CPUState *cpu, u32 address, const CARDStat &stat)
{
    for (u32 i = 0; i < CARD_FILENAME_MAX; ++i) {
        mem_write8(cpu, address + i, static_cast<u8>(stat.fileName[i]));
    }
    mem_write32(cpu, address + 0x20u, stat.length);
    mem_write32(cpu, address + 0x24u, stat.time);
    for (u32 i = 0; i < 4; ++i) mem_write8(cpu, address + 0x28u + i, stat.gameName[i]);
    for (u32 i = 0; i < 2; ++i) mem_write8(cpu, address + 0x2Cu + i, stat.company[i]);
    mem_write8(cpu, address + 0x2Eu, stat.bannerFormat);
    mem_write32(cpu, address + 0x30u, stat.iconAddr);
    mem_write16(cpu, address + 0x34u, stat.iconFormat);
    mem_write16(cpu, address + 0x36u, stat.iconSpeed);
    mem_write32(cpu, address + 0x38u, stat.commentAddr);
    mem_write32(cpu, address + 0x3Cu, stat.offsetBanner);
    mem_write32(cpu, address + 0x40u, stat.offsetBannerTlut);
    for (u32 i = 0; i < CARD_ICON_MAX; ++i) mem_write32(cpu, address + 0x44u + i * 4u, stat.offsetIcon[i]);
    mem_write32(cpu, address + 0x64u, stat.offsetIconTlut);
    mem_write32(cpu, address + 0x68u, stat.offsetData);
}

bool read_guest_card_stat(CPUState *cpu, u32 address, CARDStat *stat)
{
    if (!guest_range_valid(cpu, address, kGuestCardStatSize)) {
        Log.warn("CARDStat at {:#010x} is outside guest RAM", address);
        return false;
    }
    for (u32 i = 0; i < CARD_FILENAME_MAX; ++i) stat->fileName[i] = static_cast<char>(mem_read8(cpu, address + i));
    stat->length = mem_read32(cpu, address + 0x20u);
    stat->time = mem_read32(cpu, address + 0x24u);
    for (u32 i = 0; i < 4; ++i) stat->gameName[i] = mem_read8(cpu, address + 0x28u + i);
    for (u32 i = 0; i < 2; ++i) stat->company[i] = mem_read8(cpu, address + 0x2Cu + i);
    stat->bannerFormat = mem_read8(cpu, address + 0x2Eu);
    stat->iconAddr = mem_read32(cpu, address + 0x30u);
    stat->iconFormat = mem_read16(cpu, address + 0x34u);
    stat->iconSpeed = mem_read16(cpu, address + 0x36u);
    stat->commentAddr = mem_read32(cpu, address + 0x38u);
    stat->offsetBanner = mem_read32(cpu, address + 0x3Cu);
    stat->offsetBannerTlut = mem_read32(cpu, address + 0x40u);
    for (u32 i = 0; i < CARD_ICON_MAX; ++i) stat->offsetIcon[i] = mem_read32(cpu, address + 0x44u + i * 4u);
    stat->offsetIconTlut = mem_read32(cpu, address + 0x64u);
    stat->offsetData = mem_read32(cpu, address + 0x68u);
    return true;
}

bool host_call_card_get_status(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const s32 fileNo = static_cast<s32>(cpu->gpr[4]);
    const u32 guestStat = cpu->gpr[5];
    if (!guest_range_valid(cpu, guestStat, kGuestCardStatSize)) {
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_FATAL_ERROR);
        return true;
    }
    CARDStat stat {};
    const s32 result = CARDGetStatus(chan, fileNo, &stat);
    if (result == CARD_RESULT_READY) write_guest_card_stat(cpu, guestStat, stat);
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDGetStatus(chan={}, fileNo={}) -> {}", chan, fileNo, result);
    return true;
}

bool host_call_card_set_status(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const s32 fileNo = static_cast<s32>(cpu->gpr[4]);
    CARDStat stat {};
    if (!read_guest_card_stat(cpu, cpu->gpr[5], &stat)) {
        cpu->gpr[3] = static_cast<u32>(CARD_RESULT_FATAL_ERROR);
        return true;
    }
    const s32 result = CARDSetStatus(chan, fileNo, &stat);
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDSetStatus(chan={}, fileNo={}) -> {}", chan, fileNo, result);
    return true;
}

bool host_call_card_format(CPUState *cpu, u32)
{
    const s32 chan = static_cast<s32>(cpu->gpr[3]);
    const s32 result = CARDFormat(chan);
    if (chan >= 0 && chan < 2) {
        g_cardReady[chan] = result == CARD_RESULT_READY;
    }
    cpu->gpr[3] = static_cast<u32>(result);
    Log.info("CARDFormat(chan={}) -> {}", chan, result);
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
        } else if (std::strcmp(name, "CARDCheck") == 0) {
            fn = &host_call_card_check;
        } else if (std::strcmp(name, "CARDOpen") == 0) {
            fn = &host_call_card_open;
        } else if (std::strcmp(name, "CARDFreeBlocks") == 0) {
            fn = &host_call_card_free_blocks;
        } else if (std::strcmp(name, "CARDCreate") == 0) {
            fn = &host_call_card_create;
        } else if (std::strcmp(name, "CARDClose") == 0) {
            fn = &host_call_card_close;
        } else if (std::strcmp(name, "CARDRead") == 0) {
            fn = &host_call_card_read;
        } else if (std::strcmp(name, "CARDWrite") == 0) {
            fn = &host_call_card_write;
        } else if (std::strcmp(name, "CARDGetStatus") == 0) {
            fn = &host_call_card_get_status;
        } else if (std::strcmp(name, "CARDSetStatus") == 0) {
            fn = &host_call_card_set_status;
        } else if (std::strcmp(name, "CARDFormat") == 0) {
            fn = &host_call_card_format;
        }
        if (!fn) {
            g_observedCallNames[addresses[i].address] = name;
            fn = &observe_unbridged_card_call;
        }
        entries.push_back({ addresses[i].address, name, fn });
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

} // namespace sms::recomp::card
