// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm

#pragma once

#include "button.hpp"
#include "document.hpp"
#include "tab_bar.hpp"

#include <memory>

namespace sms::ui {

class MenuBar : public Document {
public:
    MenuBar();

    MenuBar(const MenuBar &) = delete;
    MenuBar &operator=(const MenuBar &) = delete;

    void show() override;
    void hide(bool close) override;
    void update() override;
    bool focus() override;
    bool visible() const override;

protected:
    bool handle_nav_command(Rml::Event &event, NavCommand cmd) override;

private:
    void update_safe_area() noexcept;

    Rml::Element *mRoot;
    std::unique_ptr<TabBar> mTabBar;
    Insets mTabBarPadding;
    int mFocusedTabIndex = -1;
};

} // namespace sms::ui
