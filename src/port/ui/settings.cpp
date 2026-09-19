// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm

#include "settings.hpp"

#include "bool_button.hpp"
#include "controller_config.hpp"
#include "graphics_tuner.hpp"
#include "localization.hpp"
#include "number_button.hpp"
#include "pane.hpp"
#include "string_button.hpp"
#include "ui.hpp"

#include "port/config.hpp"
#include "port/settings.h"

#include <algorithm>
#include <array>
#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/vi.h>

namespace sms::ui {
namespace {

    // The original GameCube framebuffer is 640x480 for both NTSC and PAL SMS;
    // the window opens at twice that until the player resizes it.
    constexpr uint32_t kBaseWidth = 640;
    constexpr uint32_t kBaseHeight = 480;

    struct LanguageChoice {
        const char *name;
        GameLanguage value;
    };

    constexpr std::array kLanguageChoices = {
        LanguageChoice { "English", GameLanguage::English },
        LanguageChoice { "French", GameLanguage::French },
    };

    constexpr std::array kFpsOverlayCornerNames = {
        "Top Left",
        "Top Right",
        "Bottom Left",
        "Bottom Right",
    };

    constexpr std::array kTargetFrameRates = {
        60,
        90,
        120,
        144,
        165,
        240,
    };

    bool try_parse_backend(std::string_view backend, AuroraBackend &outBackend)
    {
        if (backend == "auto") { outBackend = BACKEND_AUTO; return true; }
        if (backend == "d3d12") { outBackend = BACKEND_D3D12; return true; }
        if (backend == "metal") { outBackend = BACKEND_METAL; return true; }
        if (backend == "vulkan") { outBackend = BACKEND_VULKAN; return true; }
        if (backend == "opengl") { outBackend = BACKEND_OPENGL; return true; }
        if (backend == "opengles") { outBackend = BACKEND_OPENGLES; return true; }
        if (backend == "webgpu") { outBackend = BACKEND_WEBGPU; return true; }
        if (backend == "null") { outBackend = BACKEND_NULL; return true; }
        return false;
    }

    std::string_view backend_name(AuroraBackend backend)
    {
        switch (backend) {
            default: return "Auto";
            case BACKEND_D3D12: return "D3D12";
            case BACKEND_METAL: return "Metal";
            case BACKEND_VULKAN: return "Vulkan";
            case BACKEND_OPENGL: return "OpenGL";
            case BACKEND_OPENGLES: return "OpenGL ES";
            case BACKEND_WEBGPU: return "WebGPU";
            case BACKEND_NULL: return "Null";
        }
    }

    std::string_view backend_id(AuroraBackend backend)
    {
        switch (backend) {
            default: return "auto";
            case BACKEND_D3D12: return "d3d12";
            case BACKEND_METAL: return "metal";
            case BACKEND_VULKAN: return "vulkan";
            case BACKEND_OPENGL: return "opengl";
            case BACKEND_OPENGLES: return "opengles";
            case BACKEND_WEBGPU: return "webgpu";
            case BACKEND_NULL: return "null";
        }
    }

    std::vector<AuroraBackend> available_backends()
    {
        std::vector<AuroraBackend> backends;
        backends.emplace_back(BACKEND_AUTO);
        size_t backendCount = 0;
        const AuroraBackend *raw = aurora_get_available_backends(&backendCount);
        for (size_t i = 0; i < backendCount; ++i) {
            if (raw[i] != BACKEND_NULL) {
                backends.emplace_back(raw[i]);
            }
        }
        return backends;
    }

    AuroraBackend configured_backend()
    {
        AuroraBackend configuredBackend = BACKEND_AUTO;
        const auto configuredId = getSettings().backend.graphicsBackend.getValue();
        if (!try_parse_backend(configuredId, configuredBackend)) {
            configuredBackend = BACKEND_AUTO;
        }
        return configuredBackend;
    }

    const Rml::String kInternalResolutionHelpText =
        "Configure the resolution used for rendering the game. Higher values are more demanding on your graphics hardware.";
    const Rml::String kShadowResolutionHelpText =
        "Configure the shadow-map resolution. Higher values improve shadow quality but increase GPU and memory usage.";

    struct ConfigBoolProps {
        Rml::String key;
        Rml::String icon;
        Rml::String helpText;
        std::function<void(bool)> onChange;
        std::function<bool()> isDisabled;
    };

    SelectButton &config_bool_select(Pane &leftPane, Pane &rightPane, ConfigVar<bool> &var, ConfigBoolProps props)
    {
        auto &button = leftPane.add_child<BoolButton>(BoolButton::Props {
            .key = std::move(props.key),
            .icon = std::move(props.icon),
            .getValue = [&var] { return var.getValue(); },
            .setValue =
                [&var, callback = std::move(props.onChange)](bool value) {
                    if (value == var.getValue()) {
                        return;
                    }
                    var.setValue(value);
                    config::Save();
                    if (callback) {
                        callback(value);
                    }
                },
            .isDisabled = std::move(props.isDisabled),
            .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        });
        leftPane.register_control(button, rightPane, [helpText = std::move(props.helpText)](Pane &pane) {
            pane.clear();
            pane.add_rml(helpText);
        });
        return button;
    }

    template <typename T>
    void graphics_tuner_control(Window &window, Pane &leftPane, Pane &rightPane, ConfigVar<T> &var, const GraphicsTunerProps &props, bool prelaunch)
    {
        leftPane.register_control(leftPane
                                      .add_select_button({
                                          .key = props.title,
                                          .getValue =
                                              [&var, option = props.option] {
                                                  return format_graphics_setting_value(option, static_cast<int>(var.getValue()));
                                              },
                                          .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
                                          .submit = false,
                                      })
                                      .on_nav_command([&window, props, prelaunch](Rml::Event &, NavCommand cmd) {
                                          if (cmd == NavCommand::Confirm || cmd == NavCommand::Left || cmd == NavCommand::Right) {
                                              window.push(std::make_unique<GraphicsTuner>(props, prelaunch));
                                              return true;
                                          }
                                          return false;
                                      }),
            rightPane, [helpText = props.helpText](Pane &pane) {
                pane.clear();
                pane.add_text(helpText);
            });
    }

} // namespace

SettingsWindow::SettingsWindow(bool prelaunch)
    : mPrelaunch(prelaunch)
{
    if (prelaunch) {
        mSuppressNavFallback = true;
        add_tab(ui_translate("Prelaunch"), [this](Rml::Element *content) {
            auto &leftPane = add_child<Pane>(content, Pane::Type::Controlled);
            auto &rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

            leftPane.register_control(
                leftPane.add_child<StringButton>(StringButton::Props {
                    .key = ui_translate("Disc Image"),
                    .getValue = [] { return Rml::String { getSettings().backend.discPath.getValue() }; },
                    .setValue = [](Rml::String value) {
                        getSettings().backend.discPath.setValue(std::string { value });
                        config::Save();
                    },
                }),
                rightPane, [](Pane &pane) {
                    pane.clear();
                    pane.add_rml("Path to a legally-dumped GMSP01 disc image (ISO/RVZ/etc). "
                                 "Changes require a restart.<br/><br/>"
                                 "This is a plain text field for now; a native file picker with "
                                 "disc-hash verification (as Marioparty4's iso_validate does) is "
                                 "a follow-up once the recompiled/decompiled game can actually load a disc.");
                });

            leftPane.register_control(
                leftPane.add_select_button({
                    .key = ui_translate("Language"),
                    .getValue =
                        [] {
                            return getSettings().game.language.getValue() == GameLanguage::French
                                ? kLanguageChoices[1].name
                                : kLanguageChoices[0].name;
                        },
                    .isModified = [] { return getSettings().game.language.getValue() != getSettings().game.language.getDefaultValue(); },
                }),
                rightPane, [](Pane &pane) {
                    for (const auto &choice : kLanguageChoices) {
                        pane.add_button({
                                            .text = choice.name,
                                            .isSelected = [value = choice.value] { return getSettings().game.language.getValue() == value; },
                                        })
                            .on_pressed([value = choice.value] {
                                getSettings().game.language.setValue(value);
                                config::Save();
                            });
                    }
                });

            leftPane.register_control(
                leftPane.add_select_button({
                    .key = ui_translate("Graphics Backend"),
                    .getValue = [] { return Rml::String { backend_name(configured_backend()) }; },
                    .isModified = [] { return getSettings().backend.graphicsBackend.getValue() != getSettings().backend.graphicsBackend.getDefaultValue(); },
                }),
                rightPane, [](Pane &pane) {
                    for (const auto backend : available_backends()) {
                        pane.add_button({
                                            .text = Rml::String { backend_name(backend) },
                                            .isSelected = [backend] { return configured_backend() == backend; },
                                        })
                            .on_pressed([backend] {
                                getSettings().backend.graphicsBackend.setValue(std::string { backend_id(backend) });
                                config::Save();
                            });
                    }
                    pane.add_rml("<br/>Changes require a restart.");
                });
        });
    }

    add_tab(ui_translate("Video"), [this](Rml::Element *content) {
        auto &leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto &rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(ui_translate("Display"));

        leftPane.register_control(leftPane.add_button(ui_translate("Toggle Fullscreen")).on_pressed([] {
            getSettings().video.enableFullscreen.setValue(!getSettings().video.enableFullscreen);
            VISetWindowFullscreen(getSettings().video.enableFullscreen);
            config::Save();
        }),
            rightPane, [](Pane &pane) { pane.clear(); });
        leftPane.register_control(leftPane.add_button(ui_translate("Restore Default Window Size")).on_pressed([] {
            getSettings().video.enableFullscreen.setValue(false);
            VISetWindowFullscreen(false);
            VISetWindowSize(kBaseWidth * 2, kBaseHeight * 2);
            VICenterWindow();
        }),
            rightPane, [](Pane &pane) { pane.clear(); });
        config_bool_select(leftPane, rightPane, getSettings().video.enableVsync,
            {
                .key = ui_translate("Enable VSync"),
                .helpText = "Synchronizes the frame rate to your monitor's refresh rate.",
                .onChange = [](bool value) { aurora_enable_vsync(value); },
            });
        leftPane.register_control(leftPane.add_select_button({
                                      .key = ui_translate("Frame Rate"),
                                      .getValue = [] {
                                          return Rml::String { std::to_string(getSettings().video.targetFrameRate.getValue()) + " FPS" };
                                      },
                                      .isModified = [] {
                                          return getSettings().video.targetFrameRate.getValue()
                                              != getSettings().video.targetFrameRate.getDefaultValue();
                                      },
                                  }),
            rightPane, [](Pane &pane) {
                for (const int frameRate : kTargetFrameRates) {
                    pane.add_button({
                                        .text = Rml::String { std::to_string(frameRate) + " FPS" },
                                        .isSelected = [frameRate] {
                                            return getSettings().video.targetFrameRate.getValue() == frameRate;
                                        },
                                    })
                        .on_pressed([frameRate] {
                            getSettings().video.targetFrameRate.setValue(frameRate);
                            config::Save();
                        });
                }
            });
        config_bool_select(leftPane, rightPane, getSettings().video.lockAspectRatio,
            {
                .key = ui_translate("Lock 4:3 Aspect Ratio"),
                .helpText = "Lock the game's aspect ratio to the original.",
                .onChange = [](bool value) {
                    if (value) {
                        getSettings().video.enableAdaptiveWidescreen.setValue(false);
                    }
                    AuroraSetViewportPolicy(value ? AURORA_VIEWPORT_FIT : AURORA_VIEWPORT_STRETCH);
                    config::Save();
                },
            });
        config_bool_select(leftPane, rightPane, getSettings().video.enableAdaptiveWidescreen,
            {
                .key = ui_translate("Adaptive Widescreen HUD"),
                .helpText = "Uses the window's aspect ratio without stretching the 3D scene.",
                .onChange = [](bool value) {
                    if (value) {
                        getSettings().video.lockAspectRatio.setValue(false);
                    }
                    AuroraSetViewportPolicy(value || !getSettings().video.lockAspectRatio.getValue()
                            ? AURORA_VIEWPORT_STRETCH
                            : AURORA_VIEWPORT_FIT);
                    config::Save();
                },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.pauseOnFocusLost,
            {
                .key = ui_translate("Pause on Focus Lost"),
            });
        leftPane.register_control(leftPane.add_select_button({
                                      .key = ui_translate("Show FPS Counter"),
                                      .getValue =
                                          [] {
                                              if (!getSettings().video.enableFpsOverlay.getValue()) {
                                                  return Rml::String { "Off" };
                                              }
                                              const int idx = getSettings().video.fpsOverlayCorner.getValue();
                                              return Rml::String { kFpsOverlayCornerNames[idx] };
                                          },
                                      .isModified =
                                          [] {
                                              const auto &enable = getSettings().video.enableFpsOverlay;
                                              const auto &corner = getSettings().video.fpsOverlayCorner;
                                              return enable.getValue() != enable.getDefaultValue()
                                                  || (enable.getValue() && corner.getValue() != corner.getDefaultValue());
                                          },
                                  }),
            rightPane, [](Pane &pane) {
                pane.add_button({
                                    .text = "Off",
                                    .isSelected = [] { return !getSettings().video.enableFpsOverlay.getValue(); },
                                })
                    .on_pressed([] {
                        getSettings().video.enableFpsOverlay.setValue(false);
                        config::Save();
                    });
                for (int i = 0; i < static_cast<int>(kFpsOverlayCornerNames.size()); ++i) {
                    pane
                        .add_button({
                            .text = kFpsOverlayCornerNames[i],
                            .isSelected
                            = [i] { return getSettings().video.enableFpsOverlay.getValue() && getSettings().video.fpsOverlayCorner.getValue() == i; },
                        })
                        .on_pressed([i] {
                            getSettings().video.enableFpsOverlay.setValue(true);
                            getSettings().video.fpsOverlayCorner.setValue(i);
                            config::Save();
                        });
                }
            });

        leftPane.add_section(ui_translate("Resolution"));
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.internalResolutionScale,
            GraphicsTunerProps {
                .option = GraphicsOption::InternalResolution,
                .title = ui_translate("Internal Resolution"),
                .helpText = kInternalResolutionHelpText,
                .valueMin = 0,
                .valueMax = 12,
                .defaultValue = 0,
            },
            mPrelaunch);
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.shadowResolutionMultiplier,
            GraphicsTunerProps {
                .option = GraphicsOption::ShadowResolution,
                .title = ui_translate("Shadow Resolution"),
                .helpText = kShadowResolutionHelpText,
                .valueMin = 1,
                .valueMax = 8,
                .defaultValue = 1,
            },
            mPrelaunch);
    });

    add_tab(ui_translate("Input"), [this](Rml::Element *content) {
        auto &leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto &rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(ui_translate("Controller"));
        leftPane.register_control(
            leftPane.add_button(ui_translate("Configure Controller")).on_pressed([this] { push(std::make_unique<ControllerConfigWindow>()); }),
            rightPane, [](Pane &pane) {
                pane.clear();
                pane.add_text("Open controller binding configuration.");
            });
        config_bool_select(leftPane, rightPane, getSettings().game.allowBackgroundInput,
            {
                .key = ui_translate("Allow Background Input"),
                .helpText = "Allow controller input even when the game window is not focused.",
                .onChange = [](bool value) { aurora_set_background_input(value); },
            });
    });
}

} // namespace sms::ui
