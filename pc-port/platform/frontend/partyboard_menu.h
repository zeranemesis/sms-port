#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace Rml { class Context; class Event; }
namespace sms_frontend {
struct MenuBindings {
    std::function<std::string(const char*,const char*)> getSetting;
    std::function<void(const char*,const std::string&)> setSetting;
    std::function<std::string(int)> getBinding;
    std::function<void(int,const std::string&)> setBinding;
    std::function<bool()> onResume;
    std::function<void()> onQuit;
    std::function<bool()> onSave;
    std::string resourceDirectory = "res/partyboard";
    std::vector<std::string> bindingLabels;
    std::vector<std::string> availableMods;
    std::function<std::vector<std::string>()> refreshMods;
    std::function<void()> onModsInstalled;
};
// RmlUi DOM, original PartyBoard CSS and original font resources; no ImGui.
class PartyBoardMenu {
public:
    PartyBoardMenu(Rml::Context* context, const MenuBindings& bindings);
    ~PartyBoardMenu();
    PartyBoardMenu(const PartyBoardMenu&) = delete;
    PartyBoardMenu& operator=(const PartyBoardMenu&) = delete;
    bool available() const;
    bool visible() const;
    void show();
    void hide();
    void update();
    void handleEvent(Rml::Event& event);
    bool waitingForBinding() const;
    // Backend passes the PAD-compatible key name; Escape cancels capture.
    bool captureBinding(const std::string& key_name);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace sms_frontend
