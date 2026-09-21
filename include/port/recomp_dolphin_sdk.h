// Dolphin SDK host-call trampolines for the DolRecomp -> Aurora bridge
// (see include/port/recomp_host.h for the mechanism these plug into).
//
// Each function here has the signature recomp::HostCallFn expects and does
// the actual marshalling between DolRecomp's guest CPUState/memory and a
// real Aurora call. Only functions marshalled carefully enough to trust are
// here. PADInit/PADRead live in recomp_pad instead: their PADStatus argument
// needs guest-memory marshalling, since Aurora's TARGET_PC struct has an
// extra host-only `extButton` field.
//
// What is here is limited to what's safe without that struct knowledge:
// scalar arguments only, since DolRecomp's CPUState register file already
// holds those as correctly-decoded host-native values (see
// docs/recompilation.md's "Contrat d'execution de DolRecomp" section), and
// NUL-terminated guest strings, which have no endianness to get wrong.
#pragma once

#include "recomp_host.h"

#include <cstddef>
#include <string>

namespace sms::recomp::dolphin_sdk {

// A minimal OSReport bridge: reads the format string from guest memory at
// r3 and logs it, substituting %s (a further guest string pointer), %d/%u/%x
// (a further GPR treated as decimal/unsigned/hex), %c and %% from the
// registers a real `OSReport(fmt, ...)` call would have used per the
// PowerPC EABI (r4-r10, in order, one register per substitution regardless
// of its width - the original game never passes a variadic argument wider
// than a GPR/single float to OSReport). Any other conversion specifier is
// copied through literally rather than guessed at. Floating-point
// specifiers (%f/%g) are not handled: those would read from the FPR file
// (f1-f8) instead of the GPR file, tracked separately from the integer
// argument index, and OSReport call sites using them were not common
// enough in the original SDK to justify guessing at without a real one to
// test against.
bool host_call_os_report(CPUState *cpu, u32 address);

// Formats exactly what host_call_os_report would log, without touching a
// CPUState's host_call plumbing or Aurora's logger - the seam
// run_dolphin_sdk_self_test() exercises directly.
std::string format_os_report(CPUState *cpu, u32 formatGuestAddr);

// Registers every trampoline in this file under its Dolphin SDK symbol
// name. `addresses` maps a symbol name (as it would appear in
// generated_symbols.h's DOLRECOMP_SYMBOL_<name>, e.g. "OSReport") to the
// address dolrecomp resolved it to for this specific disc image - there is
// no such table yet without a real GMSP01 dol/map (tools/port/recompile.py
// documents how to get one). A name with no matching trampoline here is
// silently skipped, not an error: this file's coverage is expected to grow
// well behind the SDK's actual symbol list.
struct NamedAddress {
    const char *name;
    u32 address;
};
void register_known_dolphin_sdk_calls(const NamedAddress *addresses, size_t count);

// Exercises format_os_report()'s specifier handling against a synthetic
// CPUState and guest memory buffer this test owns - no disc image, no real
// Aurora link, and nothing from a real game involved.
bool run_dolphin_sdk_self_test();

} // namespace sms::recomp::dolphin_sdk
