#include "runtime.h"
#include "entrances.h"
#include "classic.h"
#include "actions.h"
#include "nozzles.h"
#include "rewards.h"
#include "presentation.h"
#include "../../gx/include/gx_frontend_language.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>

namespace {
bool active = false, ready = false, entered = false;
bool classic = false;
std::uint32_t seed = 1;
std::vector<std::uint16_t> scenes;
sms_randomizer::EntranceSeed layout;
std::filesystem::path profile;
std::atomic<int> nozzle{-1};
std::atomic<unsigned> unlocks{0x7fffff};
std::atomic<unsigned> language{0};
std::array<std::uint32_t, 120> rewards{};
std::array<bool, 120> collected{};
std::filesystem::path visual_assets;
}

extern "C" int port_randomizer_prepare()
{
    port_randomizer_set_language(sms_frontend::languageIndex(std::getenv("SMS_LANGUAGE")));
    const char* mode = std::getenv("SMS_RANDOMIZER_MODE");
    if (!mode || !*mode || std::string(mode) == "0" || std::string(mode) == "off")
        return 1;
    if (std::string(mode) != "entrances" && std::string(mode) != "classic" && std::string(mode) != "classic-test") {
        std::fprintf(stderr, "[randomizer] Unsupported mode\n");
        return 0;
    }
#if defined(__ANDROID__) || defined(SMS_ECLIPSE) || defined(SMS_ARCHIPELAGO)
    std::fprintf(stderr, "[randomizer] This preview requires standard Sunshine on PC\n");
    return 0;
#else
    const char* value = std::getenv("SMS_RANDOMIZER_SEED");
    if (value && *value) {
        for (const char* p = value; *p; ++p)
            if (*p < '0' || *p > '9') {
                std::fprintf(stderr, "[randomizer] Seed must be an unsigned 32-bit integer\n");
                return 0;
            }
        char* end = nullptr;
        errno = 0;
        const unsigned long long parsed = std::strtoull(value, &end, 10);
        if (errno || *end || parsed > UINT32_MAX)
            return 0;
        seed = static_cast<std::uint32_t>(parsed);
    }
    std::error_code error;
    const char* settings = std::getenv("SMS_SETTINGS");
    const auto root = settings ? std::filesystem::absolute(std::filesystem::u8path(settings), error).parent_path()
                               : std::filesystem::current_path(error);
    if (error)
        return 0;
    classic = std::string(mode) == "classic" || std::string(mode) == "classic-test";
    profile = root / "saves" / (classic ? "randomizer-classic-test-v1" : "randomizer-native-v1") / std::to_string(seed);
    visual_assets = root / "rom" / "randomizer-beta-0.3.6";
    const auto card = profile / "card-a";
    std::filesystem::create_directories(card, error);
    if (error) {
        std::fprintf(stderr, "[randomizer] Cannot create isolated save: %s\n", error.message().c_str());
        return 0;
    }
    const std::string path = card.u8string();
#ifdef _WIN32
    if (_putenv_s("SMS_SAVE_DIR", path.c_str()) != 0)
        return 0;
#else
    if (setenv("SMS_SAVE_DIR", path.c_str(), 1) != 0)
        return 0;
#endif
    active = true;
    unlocks.store(classic ? 0x400 : 0x7fffff);
    std::fprintf(stderr, "[randomizer] %s, seed %u, isolated card %s\n",
                 classic ? "Classic integration test v1 (incomplete gameplay port)" : "Native entrance preview v1",
                 seed, path.c_str());
    return 1;
#endif
}

extern "C" int port_randomizer_active() { return active; }
extern "C" int port_randomizer_ready() { return ready; }
extern "C" void port_randomizer_add_scene(unsigned scene)
{
    if (active && !ready && scene <= 0xffff)
        scenes.push_back(static_cast<std::uint16_t>(scene));
}
extern "C" int port_randomizer_finish_scenes()
{
    if (!active || ready)
        return 1;
    try {
        std::ostringstream contents;
        if (classic) {
            std::fprintf(stderr, "[randomizer] Enumerated %zu scene archives:", scenes.size());
            for (const auto scene : scenes) std::fprintf(stderr, " %04x", scene);
            std::fputc('\n', stderr);
            const auto generated = sms_randomizer::make_classic_seed(seed, scenes);
            layout.routes = std::map<std::uint16_t, std::uint16_t>(generated.routes.begin(), generated.routes.end());
            rewards = generated.rewards;
            contents << "Sunshine classic integration test v1 (beta-0.3.6 generator)\nseed " << seed << '\n';
            for (const auto& route : generated.routes)
                contents << route.first << ' ' << route.second << '\n';
            contents << "rewards\n";
            for (unsigned shine = 0; shine < rewards.size(); ++shine)
                contents << shine << ' ' << rewards[shine] << '\n';
        } else {
            layout = sms_randomizer::make_entrance_seed(seed, scenes);
            contents << "Sunshine native entrance preview v1\nseed " << seed << '\n';
            for (const auto& route : layout.routes)
                contents << route.first << ' ' << route.second << '\n';
        }
        const auto path = profile / "entrances.txt";
        if (std::filesystem::exists(path)) {
            std::ifstream previous(path);
            std::ostringstream saved;
            saved << previous.rdbuf();
            if (!previous || saved.str() != contents.str())
                throw std::runtime_error("This seed's saved routes differ from the current game data; choose a new seed");
        } else {
            std::ofstream manifest(path);
            manifest << contents.str();
            if (!manifest)
                throw std::runtime_error("Cannot write seed manifest");
        }
        ready = true;
        std::fprintf(stderr, "[randomizer] %zu native routes generated (%s)\n", layout.routes.size(),
                     classic ? "original beta-0.3.6 generator" : "preview graph validated");
        return 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[randomizer] %s\n", error.what());
        return 0;
    }
}
extern "C" int port_randomizer_route(unsigned scene, unsigned current_scene)
{
    if (!active || !ready || scene > 0xffff)
        return static_cast<int>(scene);
    // Death and retry reload the current destination rather than shuffling again.
    if (entered && scene == current_scene)
        return static_cast<int>(scene);
    entered = true;
    const auto destination = (scene >> 8) == 1 ? 0x0108 : layout.destination(static_cast<std::uint16_t>(scene));
    std::fprintf(stderr, "[randomizer] route %02x,%02x -> %02x,%02x\n", scene >> 8, scene & 255,
                 destination >> 8, destination & 255);
    return destination;
}
extern "C" void port_randomizer_request_nozzle(int value)
{
    if (active && (value == 1 || value == 4 || value == 5) && port_randomizer_can_use_nozzle(value))
        nozzle.store(value);
}
extern "C" int port_randomizer_take_nozzle() { return active ? nozzle.exchange(-1) : -1; }

extern "C" int port_randomizer_classic() { return active && classic; }
extern "C" unsigned port_randomizer_unlocks() { return unlocks.load(); }
extern "C" unsigned port_randomizer_reward(unsigned shine)
{
    return classic && ready && shine < rewards.size() ? rewards[shine] : 0;
}
extern "C" void port_randomizer_set_collected(unsigned shine, int value)
{
    if (!active || !classic || !ready || shine >= collected.size()) return;
    collected[shine] = value != 0;
    const auto next = sms_randomizer::collected_unlocks(rewards, collected, 0x400);
    const auto previous = unlocks.exchange(next);
    if (previous != next)
        std::fprintf(stderr, "[randomizer] Shine %u, unlock mask %06x -> %06x\n", shine, previous, next);
}
extern "C" int port_randomizer_filter_action(unsigned* status, int is_player, int* clear_holder)
{
    if (clear_holder) *clear_holder = 0;
    if (!status || !active || !classic) return 1;
    const auto result = sms_randomizer::filter_action(unlocks.load(), *status, is_player != 0);
    *status = result.status;
    if (clear_holder) *clear_holder = result.clear_holder;
    return result.allowed;
}
extern "C" int port_randomizer_can_use_nozzle(unsigned value)
{
    return !active || !classic || sms_randomizer::can_use_nozzle(unlocks.load(), value);
}
extern "C" int port_randomizer_can_switch_backup()
{
    return !active || !classic || sms_randomizer::can_switch_backup(unlocks.load());
}
extern "C" int port_randomizer_fludd_available()
{
    return !active || !classic || (unlocks.load() & ((1u << 17) | (1u << 19) | (1u << 20) | (1u << 21)));
}
extern "C" int port_randomizer_backup_nozzle()
{
    const auto mask = unlocks.load();
    return sms_randomizer::select_nozzles(mask & (1u << 17), mask & (1u << 19),
                                        mask & (1u << 20), mask & (1u << 21)).backup;
}
extern "C" void port_randomizer_player_loaded(int has_fludd)
{
    if (active && classic)
        std::fprintf(stderr, "[randomizer] Player loaded: unlocks %06x, FLUDD %s, language %u\n",
                     unlocks.load(), has_fludd ? "available" : "locked", language.load());
}
extern "C" const char* port_randomizer_unlock_name(unsigned reward)
{
    return sms_randomizer::unlock_name(reward, language.load());
}
extern "C" const char* port_randomizer_reward_name(unsigned shine)
{
    return sms_randomizer::unlock_name(port_randomizer_reward(shine), language.load());
}
extern "C" void port_randomizer_set_language(unsigned value) { language.store(sms_randomizer::language_index(value)); }
extern "C" unsigned port_randomizer_language() { return language.load(); }
extern "C" const char* port_randomizer_text(unsigned id)
{
    return sms_randomizer::text(static_cast<sms_randomizer::Text>(id), language.load());
}
extern "C" int port_randomizer_reward_message(unsigned shine, char* output, unsigned capacity)
{
    return sms_randomizer::reward_message(port_randomizer_reward(shine), language.load(), output, capacity);
}
extern "C" void* port_randomizer_shine_model(unsigned* size)
{
    if (size) *size = 0;
    if (!active || !classic) return nullptr;
    static bool attempted = false;
    static std::vector<unsigned char> model;
    if (!attempted) {
        attempted = true;
        const auto path = visual_assets / "unlockshine.bmd";
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        const auto length = input ? input.tellg() : std::streampos(-1);
        if (length >= 32 && length <= 1024 * 1024) {
            model.resize(static_cast<std::size_t>(length));
            input.seekg(0);
            input.read(reinterpret_cast<char*>(model.data()), model.size());
            const unsigned declared = (unsigned(model[8]) << 24) | (unsigned(model[9]) << 16) |
                                      (unsigned(model[10]) << 8) | model[11];
            if (!input || std::memcmp(model.data(), "J3D2bmd3", 8) || declared != model.size()) model.clear();
        }
        std::fprintf(stderr, "[randomizer] Reward Shine model: %s (%zu bytes)\n",
                     model.empty() ? "missing or invalid" : "original asset loaded", model.size());
    }
    if (model.empty()) return nullptr;
    if (size) *size = static_cast<unsigned>(model.size());
    return model.data();
}
extern "C" void port_randomizer_shine_color(unsigned char* rgb)
{
    if (!rgb) return;
    static const auto origin = std::chrono::steady_clock::now();
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - origin).count();
    const auto color = sms_randomizer::rainbow_color(static_cast<float>(std::fmod(seconds * 0.15, 1.0)));
    std::copy(color.begin(), color.end(), rgb);
}
