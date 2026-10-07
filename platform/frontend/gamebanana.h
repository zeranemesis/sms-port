#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace sms_frontend {
struct BananaMod { int id=0; std::string name, author, category, url; };
struct BananaFile { int id=0; std::string name; uint64_t bytes=0; bool zip=false; };
enum class BananaState { Idle, Loading, Ready, Downloading, Installed, Unsupported, Failed };
struct BananaStatus {
    BananaState state=BananaState::Idle;
    std::string message, installed_mod, installed_path;
    std::vector<BananaMod> mods;
    std::vector<BananaFile> files;
    int page=1, total=0, selected_mod=0;
};
// Network operations are asynchronous. poll returns a thread-safe snapshot.
void gamebanana_catalogue(int page=1);
void gamebanana_files(int mod_id);
// Fresh API metadata is checked before download. Installs into mods only;
// never overwrites assets, saves, existing installations or executable code.
void gamebanana_install(int mod_id, int file_id);
struct InstalledMod { std::string id,name; bool textures=false,mixed=false,enabled=true; };
std::vector<InstalledMod> gamebanana_installed();
bool gamebanana_enable_texture(const std::string& id,bool enabled);
BananaStatus gamebanana_status();
}
