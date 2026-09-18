#include "port/recomp_dolphin_sdk.h"

#include "aurora/lib/logging.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace sms::recomp::dolphin_sdk {
namespace {

aurora::Module Log("sms::recomp::dolphin_sdk");

// OSReport call sites in practice never need more variadic arguments than
// this, and capping means a corrupt or unexpected format string can't walk
// the marshaller past the end of the EABI's register-passed argument
// window (r3-r10; anything beyond spills to the stack, which OSReport
// never needed in the original SDK).
constexpr u32 kFirstVariadicGpr = 4;
constexpr u32 kLastVariadicGpr = 10;
constexpr size_t kMaxFormatLength = 4096;

u32 next_variadic_gpr(CPUState *cpu, u32 &gprIndex)
{
    if (gprIndex > kLastVariadicGpr) {
        return 0;
    }
    return cpu->gpr[gprIndex++];
}

} // namespace

std::string format_os_report(CPUState *cpu, u32 formatGuestAddr)
{
    std::string out;
    u32 gprIndex = kFirstVariadicGpr;
    u32 addr = formatGuestAddr;

    for (size_t i = 0; i < kMaxFormatLength; ++i) {
        const u8 c = mem_read8(cpu, addr++);
        if (c == 0) {
            break;
        }
        if (c != '%') {
            out.push_back(static_cast<char>(c));
            continue;
        }

        const u8 spec = mem_read8(cpu, addr++);
        switch (spec) {
            case 0:
                out.push_back('%');
                return out;
            case '%':
                out.push_back('%');
                break;
            case 's':
                out += read_guest_cstring(cpu, next_variadic_gpr(cpu, gprIndex));
                break;
            case 'c':
                out.push_back(static_cast<char>(next_variadic_gpr(cpu, gprIndex)));
                break;
            case 'd':
            case 'i':
                out += std::to_string(static_cast<s32>(next_variadic_gpr(cpu, gprIndex)));
                break;
            case 'u':
                out += std::to_string(next_variadic_gpr(cpu, gprIndex));
                break;
            case 'x':
            case 'X': {
                char buf[16];
                std::snprintf(buf, sizeof(buf), spec == 'x' ? "%x" : "%X", next_variadic_gpr(cpu, gprIndex));
                out += buf;
                break;
            }
            default:
                // An unhandled conversion (commonly %f/%g, which read the FPR
                // file instead - see the header comment) is copied through
                // literally rather than guessed at.
                out.push_back('%');
                out.push_back(static_cast<char>(spec));
                break;
        }
    }
    return out;
}

bool host_call_os_report(CPUState *cpu, u32 address)
{
    (void)address;
    const std::string message = format_os_report(cpu, cpu->gpr[3]);
    Log.info("{} (called from pc={:#010x} lr={:#010x})", message, cpu->pc, cpu->lr);
    return true;
}

void register_known_dolphin_sdk_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    for (size_t i = 0; i < count; ++i) {
        if (std::strcmp(addresses[i].name, "OSReport") == 0) {
            entries.push_back({ addresses[i].address, "OSReport", &host_call_os_report });
        }
        // More trampolines get a branch here as they're written - see the
        // header comment for why PADRead/PADInit aren't among them yet.
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

bool run_dolphin_sdk_self_test()
{
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("self-test: cpu_init failed");
        return false;
    }

    constexpr u32 kFormatAddr = GC_RAM_BASE + 0x1000u;
    constexpr u32 kStringArgAddr = GC_RAM_BASE + 0x2000u;

    const auto write_guest_cstring = [&cpu](u32 addr, const char *text) {
        for (; *text != '\0'; ++text, ++addr) {
            mem_write8(&cpu, addr, static_cast<u8>(*text));
        }
        mem_write8(&cpu, addr, 0);
    };

    write_guest_cstring(kFormatAddr, "Hello %s, %d/%u/%x/%%!");
    write_guest_cstring(kStringArgAddr, "world");

    cpu.gpr[4] = kStringArgAddr; // %s
    cpu.gpr[5] = static_cast<u32>(-5); // %d
    cpu.gpr[6] = 42; // %u
    cpu.gpr[7] = 0xBEEFu; // %x

    const std::string result = format_os_report(&cpu, kFormatAddr);
    cpu_free(&cpu);

    const std::string expected = "Hello world, -5/42/beef/%!";
    if (result != expected) {
        Log.error("self-test: expected '{}', got '{}'", expected, result);
        return false;
    }

    Log.info("self-test: OSReport format-string bridge matches expected output");
    return true;
}

} // namespace sms::recomp::dolphin_sdk
