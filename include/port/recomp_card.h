// Bridges GameCube CARD (memory card) SDK calls to Aurora's real, working
// implementation (extern/aurora/lib/dolphin/card.cpp - a genuine
// GCI-folder-backed virtual memory card, not a stub), unlike EXI (see
// include/port/recomp_exi.h) which has no real Aurora implementation at
// all.
//
// Safety ordering: Aurora's CARDProbeEx/CARDCheck/etc. dereference
// CardChannels[chan] (a std::unique_ptr constructed only inside
// CARDInit()) - calling them before CARDInit() has run null-derefs and
// crashes. This file tracks its own local "has CARDInit run" flag rather
// than trusting call order, since trampolines are added incrementally
// across sessions and it's easy to register CARDProbeEx before CARDInit
// happens to also be registered.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

#include <cstddef>

namespace sms::recomp::card {

struct NamedAddress {
    const char *name;
    u32 address;
};

// Same shape as sms::recomp::dolphin_sdk::register_known_dolphin_sdk_calls
// (include/port/recomp_dolphin_sdk.h) - one host_call_<Name> per bridged
// CARD function, matched by name against generated/generated_symbols.h's
// real addresses.
void register_known_card_calls(const NamedAddress *addresses, size_t count);

} // namespace sms::recomp::card
