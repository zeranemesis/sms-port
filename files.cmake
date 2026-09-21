# The Aurora bootstrap and Party-Board-style menu shell (see
# docs/port_bootstrap.md). No SMS game code is listed here yet: src/game and
# src/REL exist for the decompilation-matching workflow (configure.py/ninja/
# objdiff, see AGENTS.md) and are not yet wired into this CMake build. That
# happens once recompiled and/or decompiled game code can provide a boot
# entry point - see docs/recompilation.md for the chosen route.
set(PORT_FILES
        src/port/config.cpp
        src/port/entry.cpp
        src/port/io.cpp
        src/port/portmain.cpp
        src/port/recomp_boot.cpp
    src/port/recomp_crash.cpp
        src/port/recomp_card.cpp
        src/port/recomp_dolphin_sdk.cpp
        src/port/recomp_exi.cpp
        src/port/recomp_gx_fifo.cpp
        src/port/recomp_host.cpp
        src/port/recomp_interrupt.cpp
        src/port/recomp_pad.cpp
        src/port/recomp_probe.cpp
        src/port/settings.cpp

        src/port/ui/bool_button.cpp
        src/port/ui/bool_button.hpp
        src/port/ui/button.cpp
        src/port/ui/button.hpp
        src/port/ui/compat.cpp
        src/port/ui/component.cpp
        src/port/ui/component.hpp
        src/port/ui/controller_config.cpp
        src/port/ui/controller_config.hpp
        src/port/ui/document.cpp
        src/port/ui/document.hpp
        src/port/ui/event.cpp
        src/port/ui/event.hpp
        src/port/ui/graphics_tuner.cpp
        src/port/ui/graphics_tuner.hpp
        src/port/ui/input.cpp
        src/port/ui/input.hpp
        src/port/ui/localization.hpp
        src/port/ui/menu_bar.cpp
        src/port/ui/menu_bar.hpp
        src/port/ui/modal.cpp
        src/port/ui/modal.hpp
        src/port/ui/nav_types.hpp
        src/port/ui/number_button.cpp
        src/port/ui/number_button.hpp
        src/port/ui/overlay.cpp
        src/port/ui/overlay.hpp
        src/port/ui/pane.cpp
        src/port/ui/pane.hpp
        src/port/ui/select_button.cpp
        src/port/ui/select_button.hpp
        src/port/ui/settings.cpp
        src/port/ui/settings.hpp
        src/port/ui/string_button.cpp
        src/port/ui/string_button.hpp
        src/port/ui/tab_bar.cpp
        src/port/ui/tab_bar.hpp
        src/port/ui/ui.cpp
        src/port/ui/ui.hpp
        src/port/ui/window.cpp
        src/port/ui/window.hpp
)
