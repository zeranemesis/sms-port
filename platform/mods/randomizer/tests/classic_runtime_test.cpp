#include "../runtime.h"
#include "../rules.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <cstdlib>

int main(int argc, char** argv)
{
    assert(argc == 2);
    assert(port_randomizer_prepare());
    assert(port_randomizer_active() && port_randomizer_classic());
    assert(port_randomizer_unlocks() == 0x400);
    assert(!port_randomizer_fludd_available());
    assert(std::string(std::getenv("SMS_SAVE_DIR")).find("randomizer-classic-test-v1") != std::string::npos);
    std::set<unsigned> scenes{0x100, 0x101, 0x102, 0x105, 0x106, 0x107, 0x108,
                              0x109, 0xf00, 0x3400, 0x3c00};
    for (const auto& rule : sms_randomizer::level_rules) {
        scenes.insert(rule.scene);
        if (rule.kind == sms_randomizer::RuleKind::Exit) scenes.insert(rule.target);
    }
    for (const auto scene : scenes) port_randomizer_add_scene(scene);
    if (std::string(argv[1]) == "changed") {
        assert(!port_randomizer_finish_scenes());
        assert(!port_randomizer_ready());
        return 0;
    }
    assert(port_randomizer_finish_scenes() && port_randomizer_ready());
    unsigned model_size = 123;
    assert(!port_randomizer_shine_model(&model_size) && model_size == 0);
    assert(port_randomizer_route(0, 0) == 0x2f00); // Original seed 1 first destination.
    unsigned action = 0x02000881;
    int clear_holder = -1;
    assert(port_randomizer_filter_action(&action, 1, &clear_holder));
    assert(action == 0x02000880 && !clear_holder);
    action = 0x10100341;
    assert(!port_randomizer_filter_action(&action, 1, &clear_holder) && clear_holder);
    assert(port_randomizer_filter_action(&action, 0, &clear_holder) && !clear_holder);
    assert(port_randomizer_reward(2) == 0x80000);
    port_randomizer_set_language(2);
    assert(std::string(port_randomizer_reward_name(2)) == "BUSE STATIONNAIRE");
    assert(!port_randomizer_reward_name(0));
    const char* hover_names[] = {"HOVER NOZZLE", "SCHWEBEDÜSE", "BUSE STATIONNAIRE", "BOQUILLA DEL PLANEADOR", "SPRUZZOPLANO"};
    for (unsigned lang = 0; lang < 5; ++lang) {
        port_randomizer_set_language(lang);
        assert(port_randomizer_language() == lang);
        assert(std::string(port_randomizer_reward_name(2)) == hover_names[lang]);
        char message[128];
        assert(port_randomizer_reward_message(2, message, sizeof(message)) && message[0]);
        assert(!port_randomizer_reward_message(0, message, sizeof(message)) && !message[0]);
    }
    port_randomizer_set_language(999);
    assert(port_randomizer_language() == 0);
    assert(std::string(port_randomizer_reward_name(2)) == "HOVER NOZZLE");
    port_randomizer_request_nozzle(4);
    assert(port_randomizer_take_nozzle() == -1);
    port_randomizer_set_collected(2, 1);
    assert(port_randomizer_unlocks() == 0x80400);
    assert(port_randomizer_fludd_available() && port_randomizer_backup_nozzle() == 4);
    assert(port_randomizer_can_use_nozzle(4) && !port_randomizer_can_use_nozzle(0));
    assert(!port_randomizer_can_switch_backup());
    port_randomizer_request_nozzle(4);
    assert(port_randomizer_take_nozzle() == 4 && port_randomizer_take_nozzle() == -1);
    port_randomizer_set_collected(2, 0);
    assert(port_randomizer_unlocks() == 0x400);
    assert(!port_randomizer_fludd_available());
    assert(port_randomizer_reward(120) == 0);
    port_randomizer_set_collected(120, 1);
    assert(port_randomizer_unlocks() == 0x400);
    for (unsigned shine = 0; shine < 120; ++shine) port_randomizer_set_collected(shine, 1);
    assert(port_randomizer_unlocks() == 0x7fffff);
    assert(port_randomizer_can_use_nozzle(0) && port_randomizer_can_use_nozzle(5));
    assert(port_randomizer_can_switch_backup());
    action = 0x10100341;
    assert(port_randomizer_filter_action(&action, 1, &clear_holder) && !clear_holder);
    std::puts("Classic runtime: original routes, separate profile, collection, flag restoration, movement and nozzle gates passed");
}
