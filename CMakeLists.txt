cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

project(dolphinjet LANGUAGES C CXX)

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

# The DolRecomp host tool (build-time code generator, not linked into
# dolphinjet) plus dr_cpu, the small register-file/memory runtime its plain
# C backend targets. DOLRECOMP_ENABLE_LLVM stays off: the LLVM object
# backend is a separate, heavier codegen path this port does not use - see
# docs/recompilation.md's "DolRecomp runtime contract" section for why the
# plain C backend needs no external runtime (no ModernGekko, no LLVM) at
# all, only this dr_cpu-based host-call bridge into Aurora.
add_subdirectory(extern/dolrecomp EXCLUDE_FROM_ALL)

include(extern/aurora/cmake/AuroraCopyRuntimeDLLs.cmake)

include(files.cmake)

source_group("Port" FILES ${PORT_FILES})

# Recompiled game code (docs/recompilation.md) derives from a copyrighted
# disc image and is never committed - see .gitignore. If
# tools/port/recompile.py has already been run against a real GMSP01
# dump, its output lands in generated/ and gets picked up here
# automatically; nothing to edit in this file to build it. Without that
# dump - the normal case for a fresh checkout, and for CI - this glob is
# empty and dolphinjet builds exactly as before (menu shell only).
file(GLOB DOLPHINJET_GENERATED_GAME_FILES "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.c")
if (DOLPHINJET_GENERATED_GAME_FILES)
    message(STATUS "dolphinjet: found generated/ - building recompiled game code")
    add_library(game_recompiled STATIC ${DOLPHINJET_GENERATED_GAME_FILES})
    target_include_directories(game_recompiled PRIVATE extern/dolrecomp/src "${CMAKE_CURRENT_SOURCE_DIR}/generated")
    target_link_libraries(game_recompiled PUBLIC dr_cpu)
endif ()

add_executable(dolphinjet ${PORT_FILES})
target_compile_definitions(dolphinjet PRIVATE TARGET_PC NON_MATCHING)
target_include_directories(dolphinjet PRIVATE include extern extern/aurora/include extern/aurora/include/dolphin extern/dolrecomp/src)
target_link_libraries(dolphinjet PRIVATE
        aurora::main aurora::core aurora::card aurora::dvd aurora::gx aurora::mtx aurora::os aurora::si aurora::vi aurora::pad
        dr_cpu
        absl::flat_hash_map nlohmann_json::nlohmann_json fmt::fmt)
if (TARGET game_recompiled)
    target_link_libraries(dolphinjet PRIVATE game_recompiled)
    target_compile_definitions(dolphinjet PRIVATE DOLPHINJET_HAVE_RECOMPILED_GAME)
endif ()
if (TARGET rmlui_debugger)
    target_link_libraries(dolphinjet PRIVATE rmlui_debugger)
endif ()

if (WIN32)
    target_link_libraries(dolphinjet PRIVATE ws2_32)
endif ()

aurora_copy_runtime_dlls(dolphinjet dolphinjet)

# res/ holds the fonts and RmlUi (.rcss) stylesheets the menu shell needs at
# runtime; ship it next to the executable the same way Marioparty4 does.
add_custom_command(TARGET dolphinjet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:dolphinjet>/res"
        COMMENT "Copying menu resources (fonts, RmlUi stylesheets)")
