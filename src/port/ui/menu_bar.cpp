// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm

#include "menu_bar.hpp"

#include <RmlUi/Core.h>

#include "localization.hpp"
#include "modal.hpp"
#include "port/main.h"
#include "settings.hpp"
#include "ui.hpp"
#include "window.hpp"

#include <cmath>

namespace sms::ui {
namespace {

    const Rml::String kDocumentSource = R"RML(
<rml>
<head>
    <link type="text/rcss" href="res/rml/tabbing.rcss" />
    <link type="text/rcss" href="res/rml/popup.rcss" />
</head>
<body>
    <popup id="popup" />
</body>
</rml>
)RML";

}

MenuBar::MenuBar()
    : Document(kDocumentSource)
    , mRoot(mDocument->GetElementById("popup"))
{
    mTabBar = std::make_unique<TabBar>(mRoot,
        TabBar::Props {
            .onClose = [this] { hide(false); },
            .autoSelect = false,
        });
    // `prelaunch=true` is what actually shows the Prelaunch tab (disc image
    // path, language, graphics backend) - the default-constructed
    // SettingsWindow() used here before defaulted it to false, so F1 could
    // never reach it: there was no way through the UI to set or change
    // which disc image boots. That default made sense for a genuine
    // in-game pause menu, but there is no such state yet - port_main()
    // calls try_boot_game() unconditionally and immediately on startup
    // (portmain.cpp), before any menu interaction is possible, so by the
    // time F1 can be pressed the game has already tried to boot with
    // whatever discPath was already configured. Revisit this once boot is
    // gated behind an explicit action (see the disk-usage finding in
    // docs/port_bootstrap.md) rather than firing on process start - at that
    // point this should reflect whether a game is actually running instead
    // of being hardcoded.
    mTabBar->add_tab(ui_translate("Settings"), [this] { push(std::make_unique<SettingsWindow>(true)); });

    mTabBar->add_tab(ui_translate("Quit"), [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal &modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props {
            .title = ui_translate("Quit"),
            .bodyRml = ui_translate("Are you sure you want to quit?"),
            .actions =
                {
                    ModalAction {
                        .label = ui_translate("No"),
                        .onPressed = [dismiss](Modal &modal) { dismiss(modal); },
                    },
                    ModalAction {
                        .label = ui_translate("Yes"),
                        .onPressed =
                            [dismiss](Modal &modal) {
                                dismiss(modal);
                                sms::IsRunning = false;
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });

    // Hide document after transition completion
    listen(mRoot, Rml::EventId::Transitionend, [this](Rml::Event &event) {
        if (event.GetTargetElement() == mRoot && !mRoot->HasAttribute("open") && Document::visible()) {
            Document::hide(mPendingClose);
        }
    });
}

void MenuBar::show()
{
    Document::show();
    mRoot->SetAttribute("open", "");
    mTabBar->set_active_tab(-1);
    if (!mTabBar->focus_tab(mFocusedTabIndex)) {
        mTabBar->focus();
    }
}

void MenuBar::hide(bool close)
{
    mFocusedTabIndex = mTabBar->focused_tab_index();
    mRoot->RemoveAttribute("open");
    if (close) {
        mPendingClose = true;
    }
}

void MenuBar::update()
{
    update_safe_area();
    Document::update();
}

void MenuBar::update_safe_area() noexcept
{
    if (mDocument == nullptr || mTabBar == nullptr) {
        return;
    }

    Rml::Context *context = mDocument->GetContext();
    Insets safeInsets = safe_area_insets(context);
    safeInsets = {
        0.0f,
        std::round(safeInsets.right),
        0.0f,
        std::round(safeInsets.left),
    };
    if (safeInsets == mTabBarPadding) {
        return;
    }

    mTabBarPadding = safeInsets;
    auto *tabBar = mTabBar->root();
    tabBar->SetProperty(Rml::PropertyId::PaddingRight, Rml::Property(safeInsets.right, Rml::Unit::PX));
    tabBar->SetProperty(Rml::PropertyId::PaddingLeft, Rml::Property(safeInsets.left, Rml::Unit::PX));
    if (auto *close = tabBar->QuerySelector("close")) {
        close->SetProperty(
            Rml::PropertyId::Right, Rml::Property(safeInsets.right + 8.0f * context->GetDensityIndependentPixelRatio(), Rml::Unit::PX));
    }
}

bool MenuBar::visible() const
{
    return mRoot->HasAttribute("open");
}

bool MenuBar::handle_nav_command(Rml::Event &event, NavCommand cmd)
{
    if (cmd == NavCommand::Cancel && visible()) {
        hide(false);
        return true;
    }
    return Document::handle_nav_command(event, cmd);
}

bool MenuBar::focus()
{
    return mTabBar->focus();
}

} // namespace sms::ui
