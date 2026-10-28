#include "../localization.h"
#include <cassert>
#include <cstring>
#include <cstdio>

int main()
{
    using namespace sms_randomizer;
    unsigned messages = 0, labels = 0;
    for (unsigned language = 0; language < 5; ++language) {
        for (unsigned id = 0; id < static_cast<unsigned>(Text::Count); ++id) {
            const char* translation = text(static_cast<Text>(id), language);
            assert(translation && translation[0]);
            ++labels;
        }
        for (unsigned bit = 0; bit < 23; ++bit) {
            const auto reward = 1u << bit;
            assert(unlock_name(reward, language) && unlock_name(reward, language)[0]);
            char message[128];
            assert(reward_message(reward, language, message, sizeof(message)));
            assert(std::strlen(message) < sizeof(message));
            char too_small[2] = {'x', 'y'};
            assert(!reward_message(reward, language, too_small, sizeof(too_small)) && !too_small[0]);
            ++messages;
        }
    }
    char message[128];
    assert(reward_message(1u << 19, 2, message, sizeof(message)));
    assert(std::strcmp(message, "D\xc9" "BLOQU\xc9 : BUSE STATIONNAIRE") == 0);
    assert(reward_message(1u << 19, 1, message, sizeof(message)));
    assert(std::strcmp(message, "FREIGESCHALTET: SCHWEBED\xdc" "SE") == 0);
    assert(reward_message(1u << 19, 4, message, sizeof(message)));
    assert(std::strcmp(message, "SBLOCCATO: SPRUZZOPLANO") == 0);
    assert(reward_message(1u << 19, 999, message, sizeof(message)));
    assert(std::strcmp(message, "UNLOCKED: HOVER NOZZLE") == 0);
    assert(!reward_message(3, 2, message, sizeof(message)) && !message[0]);
    assert(!reward_message(1u << 19, 2, nullptr, 0));
    assert(!*text(static_cast<Text>(999), 0));
    std::printf("%u translated menu labels and %u HUD messages passed; language fallback, accents and bounds checked\n", labels, messages);
}
