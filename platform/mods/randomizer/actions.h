#pragma once

#include <cstdint>

namespace sms_randomizer {

struct ActionDecision {
    std::uint32_t status;
    bool allowed = true;
    bool clear_holder = false;
};

// Unlock indices and status values follow the original v0.3.6 callback 0x5704.
// The caller still applies the game's ordinary status-transition checks.
inline ActionDecision filter_action(std::uint32_t unlocks, std::uint32_t status,
                                    bool is_player = true)
{
    ActionDecision result{status};
    if (!is_player)
        return result;
    const auto owns = [unlocks](unsigned bit) { return (unlocks & (1u << bit)) != 0; };
    if ((!owns(15) && status == 0x00810446) ||
        (!owns(3) && (status == 0x383 || status == 0x384))) {
        result.allowed = false;
    } else if (!owns(7) && status == 0x8a7) {
        result.status = 0x88c;
    } else if ((!owns(5) && (status == 0x895 || status == 0x896)) ||
               (!owns(2) && status == 0x887)) {
        result.status = 0x02000880;
    } else if (!owns(4) && status == 0x441) {
        result.allowed = false;
    } else if (!owns(0) && status == 0x02000881) {
        result.status = owns(1) ? 0x882 : 0x02000880;
    } else if (!owns(8) && status == 0x008008a9) {
        result.allowed = false;
    } else if (!owns(1) && status == 0x882) {
        result.status = 0x02000880;
    } else if (!owns(22) && (status == 0x00200346 || status == 0x00200349 ||
                            status == 0x08200348 || status == 0x3000036c ||
                            status == 0x3000036b || status == 0x10100341)) {
        result.allowed = false;
        result.clear_holder = true;
    } else if (!owns(6) && status == 0x02000889) {
        result.allowed = false;
    }
    return result;
}

} // namespace sms_randomizer
