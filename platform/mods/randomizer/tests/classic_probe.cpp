#include "../classic.h"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) return 1;
    const std::string flags = argc == 3 ? argv[2] : "";
    std::set<std::uint16_t> unique{0x100, 0x101, 0x102, 0x105, 0x106, 0x107, 0x108,
                                 0x109, 0xf00, 0x3400, 0x3c00};
    for (const auto& rule : sms_randomizer::level_rules) {
        unique.insert(rule.scene);
        if (rule.kind == sms_randomizer::RuleKind::Exit) unique.insert(rule.target);
    }
    if (flags == "pal" || flags == "pal-all") unique.insert(0x0c01);
    try {
        const auto result = sms_randomizer::make_classic_seed(
            static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)),
            std::vector<std::uint16_t>(unique.begin(), unique.end()), flags == "all" || flags == "pal-all");
        for (const auto& route : result.routes)
            std::printf("route %u %u\n", route.first, route.second);
        for (unsigned shine = 0; shine < result.rewards.size(); ++shine)
            std::printf("reward %u %u\n", shine, result.rewards[shine]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
