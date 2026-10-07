include_guard(GLOBAL)
include(FetchContent)
get_filename_component(_sms_partyboard_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# An immutable upstream revision is shared with the existing PartyBoard port.
# Offline builders can pass -DSMS_RMLUI_SOURCE_DIR=/path/to/RmlUi.
set(SMS_RMLUI_SOURCE_DIR "" CACHE PATH "Optional existing RmlUi 6.3 source tree")
set(SMS_FREETYPE_SOURCE_DIR "" CACHE PATH "Optional existing FreeType 2.14.3 source tree")
function(sms_add_partyboard_backend)
    set(BUILD_SHARED_LIBS OFF)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    if(NOT TARGET Freetype::Freetype)
        if(SMS_FREETYPE_SOURCE_DIR)
            add_subdirectory("${SMS_FREETYPE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/partyboard-freetype" EXCLUDE_FROM_ALL)
        else()
            FetchContent_Declare(sms_freetype
                URL https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.gz
                URL_HASH SHA256=e61b31ab26358b946e767ed7eb7f4bb2e507da1cfefeb7a8861ace7fd5c899a1
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
            FetchContent_MakeAvailable(sms_freetype)
        endif()
        if(NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype ALIAS freetype)
        endif()
    endif()
    # RmlUi accepts an already supplied target. Avoid rediscovering a system
    # import library that would introduce font DLL dependencies into the port.
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
    set(RMLUI_SAMPLES OFF)
    set(RMLUI_TESTS OFF)
    set(RMLUI_FONT_ENGINE freetype)
    set(RMLUI_PRECOMPILED_HEADERS OFF)
    set(RMLUI_LUA_BINDINGS OFF)
    set(RMLUI_SVG_PLUGIN OFF)
    set(RMLUI_LOTTIE_PLUGIN OFF)
    if(SMS_RMLUI_SOURCE_DIR)
        set(_rml_source "${SMS_RMLUI_SOURCE_DIR}")
        add_subdirectory("${_rml_source}" "${CMAKE_BINARY_DIR}/partyboard-rmlui" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(sms_rmlui
            URL https://github.com/mikke89/RmlUi/archive/f9b8c9e2935d5df2c7dff2c190d3968e99b0c3dc.tar.gz
            URL_HASH SHA256=42b830abc2509a8c07cf02ba3313505de3a4642725654f489402022d8fefdd79
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(sms_rmlui)
        set(_rml_source "${sms_rmlui_SOURCE_DIR}")
    endif()
    if(NOT TARGET SDL2::SDL2)
        find_package(SDL2 REQUIRED CONFIG)
    endif()
    add_library(sms_partyboard_backend STATIC
        "${_sms_partyboard_root}/platform/frontend/partyboard_backend.cpp"
        "${_rml_source}/Backends/RmlUi_Platform_SDL.cpp"
        "${_rml_source}/Backends/RmlUi_Renderer_GL3.cpp")
    target_compile_features(sms_partyboard_backend PUBLIC cxx_std_17)
    target_compile_definitions(sms_partyboard_backend PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_include_directories(sms_partyboard_backend
        PUBLIC "${_sms_partyboard_root}/platform/frontend"
        PRIVATE "${_rml_source}/Backends"
                "${_sms_partyboard_root}/platform/gx/src")
    target_link_libraries(sms_partyboard_backend PUBLIC RmlUi::Core SDL2::SDL2 PRIVATE ${CMAKE_DL_LIBS})
endfunction()
sms_add_partyboard_backend()
