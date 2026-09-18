// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm

#pragma once
#include "window.hpp"

namespace sms::ui {

class SettingsWindow : public Window {
public:
    explicit SettingsWindow(bool prelaunch = false);

protected:
    bool mPrelaunch;
};

} // namespace sms::ui
