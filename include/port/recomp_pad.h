// Bridges the subset of the GameCube PAD API which the recompiled game uses
// to Aurora's SDL-backed controller layer.  The original guest PADStatus is
// 11 bytes (include/dolphin/pad.h); Aurora's TARGET_PC version has an extra
// host-only extButton field, so PADRead must marshal field-by-field instead
// of passing a guest pointer to Aurora.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

#include <cstddef>

namespace sms::recomp::pad {

struct NamedAddress {
    const char *name;
    u32 address;
};

void register_known_pad_calls(const NamedAddress *addresses, size_t count);

// Covers the guest/host ABI seam without needing a disc image or a physical
// controller.  The real controller path is exercised by a live DolphinJet
// run after Aurora has initialized SDL input.
bool run_pad_self_test();

} // namespace sms::recomp::pad
