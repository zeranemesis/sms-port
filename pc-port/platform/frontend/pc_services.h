#pragma once
#include <string>
#include <vector>

namespace sms_frontend {
struct ServiceResult {
    bool ok;
    std::string message;
    std::string path;
    ServiceResult(bool success = false, const std::string& text = "", const std::string& location = "")
        : ok(success), message(text), path(location) {}
};
std::string save_directory();
// Call while the game thread is paused: CARD writes are synchronous.
ServiceResult backup_saves();
std::vector<std::string> save_backups();
// Creates a fresh card directory and records it for the NEXT process start.
// The currently mounted card and its files are never modified.
ServiceResult prepare_save_restore(const std::string& backup_path);
// Call before CARD initialization. Consumes a validated pending restore.
ServiceResult activate_pending_save_restore();

struct Achievement {
    std::string id, title, description;
    bool unlocked;
};
// Only pass counters read from the loaded game's TFlagManager, on its thread.
// Invalid/out-of-range counters are ignored. Loading an existing save counts.
void observe_game_progress(int shines, int blue_coins);
std::vector<Achievement> achievements();

enum class UpdateState { Idle, Checking, Available, UpToDate, Downloading, Ready, Failed };
struct UpdateStatus {
    UpdateState state;
    std::string version, asset_name, sha256, download_url, staged_path, message;
    UpdateStatus() : state(UpdateState::Idle) {}
};
// installed_tag is the build's release tag, if known; not a guessed timestamp.
void check_updates(const std::string& installed_tag);
std::string installed_release();
void download_update();
UpdateStatus update_status();
// Rechecks the archive and starts a hidden helper waiting for this process to
// exit. On success the UI must quit normally; it must not terminate threads.
// Requires a release ZIP with sms.exe, its DLLs and sms-build.json declaring
// {"region":"GMSP01","architecture":"x64"}. Originals retained for rollback.
ServiceResult prepare_update_installation(bool restart_after_install = true);
} // namespace sms_frontend
