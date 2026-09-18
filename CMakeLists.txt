cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

project(smsport LANGUAGES C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_cmake")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

if ((CMAKE_SYSTEM_NAME STREQUAL Linux) OR APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_RPATH "$ORIGIN")
elseif (MSVC)
    add_compile_options(/bigobj)
endif ()

include(FetchContent)

FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Aurora provides the GX->Vulkan/Metal/D3D12 rendering layer, the Dolphin-SDK
# platform shims (PAD/VI/OS/CARD/DVD), and (with RmlUi enabled) the UI
# renderer the Party-Board-style menu is built on. It also brings in fmt and
# abseil, which the port layer links against directly below.
set(AURORA_ENABLE_DVD ON)
set(AURORA_ENABLE_RMLUI ON CACHE BOOL "Enable RmlUi UI support" FORCE)

# extern/aurora and extern/libco are pinned submodule commits; two local,
# uncommitted patches apply on top (patches/aurora-port-fixes.patch,
# patches/aurora-render-fixes.patch - see docs/port_bootstrap.md for why they
# are not committed into the submodule itself). CI must apply them at
# checkout, the same way Marioparty4's build.yml does for its own patches.
add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)

include(extern/aurora/cmake/AuroraCopyRuntimeDLLs.cmake)

include(files.cmake)

source_group("Port" FILES ${PORT_FILES})

add_executable(smsport ${PORT_FILES})
target_compile_definitions(smsport PRIVATE TARGET_PC NON_MATCHING)
target_include_directories(smsport PRIVATE include extern extern/aurora/include extern/aurora/include/dolphin)
target_link_libraries(smsport PRIVATE
        aurora::main aurora::core aurora::card aurora::dvd aurora::gx aurora::mtx aurora::os aurora::si aurora::vi aurora::pad
        absl::flat_hash_map nlohmann_json::nlohmann_json fmt::fmt)
if (TARGET rmlui_debugger)
    target_link_libraries(smsport PRIVATE rmlui_debugger)
endif ()

if (WIN32)
    target_link_libraries(smsport PRIVATE ws2_32)
endif ()

aurora_copy_runtime_dlls(smsport smsport)

# res/ holds the fonts and RmlUi (.rcss) stylesheets the menu shell needs at
# runtime; ship it next to the executable the same way Marioparty4 does.
add_custom_command(TARGET smsport POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:smsport>/res"
        COMMENT "Copying menu resources (fonts, RmlUi stylesheets)")
