#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace sms_frontend { namespace ra {
struct Achievement { uint32_t id; std::string title, description; unsigned points; bool unlocked; std::string progress; };
// All calls must run on the same game/UI thread. No credentials are persisted.
void initialize();
void shutdown();
void pump();
void game_tick();
void login(const std::string& user, const std::string& password);
void logout();
void set_disc_path(const std::string& path);
std::string status();
std::string username();
std::vector<Achievement> achievements();
bool evaluation_available();
} }
