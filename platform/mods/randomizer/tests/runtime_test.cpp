#include "../runtime.h"
#include "../rules.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string mode = argv[1];
    const int prepared = port_randomizer_prepare();
    if (mode == "invalid") {
        assert(!prepared);
        assert(!port_randomizer_active());
        return 0;
    }
    assert(prepared);
    if (mode == "off") {
        assert(!port_randomizer_active());
        assert(port_randomizer_route(0x0200, 0xffff) == 0x0200);
        unsigned status = 0x10100341;
        int clear_holder = -1;
        assert(port_randomizer_filter_action(&status, 1, &clear_holder));
        assert(status == 0x10100341 && !clear_holder);
        assert(port_randomizer_unlocks() == 0x7fffff);
        assert(port_randomizer_can_use_nozzle(4));
        assert(port_randomizer_can_switch_backup());
        assert(port_randomizer_fludd_available());
        return 0;
    }
    assert(port_randomizer_active());
    assert(std::string(std::getenv("SMS_SAVE_DIR")).find("randomizer-native-v1") != std::string::npos);
    std::set<unsigned> scenes;
    for (const auto& rule : sms_randomizer::level_rules) {
        scenes.insert(rule.scene);
        if (rule.kind == sms_randomizer::RuleKind::Exit)
            scenes.insert(rule.target);
    }
    for (unsigned scene : scenes)
        port_randomizer_add_scene(scene);
    if (mode == "changed") {
        assert(!port_randomizer_finish_scenes());
        assert(!port_randomizer_ready());
        return 0;
    }
    assert(port_randomizer_finish_scenes());
    assert(port_randomizer_ready());
    const int start = port_randomizer_route(0, 0);
    assert(start != 0);
    assert(port_randomizer_route(start, start) == start);
    assert(port_randomizer_route(0x0100, start) == 0x0108);
    port_randomizer_request_nozzle(999);
    assert(port_randomizer_take_nozzle() == -1);
    port_randomizer_request_nozzle(5);
    assert(port_randomizer_take_nozzle() == 5);
    assert(port_randomizer_take_nozzle() == -1);
}
