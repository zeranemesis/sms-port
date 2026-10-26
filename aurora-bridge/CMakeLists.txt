# sms_aurora.dll: Aurora (GX on WebGPU) built with MSVC, behind a C interface
# the MinGW build of the game links against. See README.md.
cmake_minimum_required(VERSION 3.25)
project(sms_aurora_bridge LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

# Reuse the dependency sources that the Mario Party 4 build already fetched,
# so configuring never downloads anything.
set(SMS_AURORA_DEPS "C:/Users/BEAVSN/Documents/Marioparty4/build/x-windows-ci-msvc/_deps"
    CACHE PATH "FetchContent _deps directory to take dependency sources from")
foreach(dep abseil-cpp dawn_prebuilt fmt freetype imgui json png rmlui sdl3_prebuilt sqlite3 tracy xxhash zlib zstd)
  if(ANDROID AND dep MATCHES "^(dawn_prebuilt|sdl3_prebuilt)$")
    continue() # Desktop binary packages cannot be reused for Android.
  endif()
  string(TOUPPER "${dep}" dep_upper)
  if(EXISTS "${SMS_AURORA_DEPS}/${dep}-src")
    set(FETCHCONTENT_SOURCE_DIR_${dep_upper} "${SMS_AURORA_DEPS}/${dep}-src" CACHE PATH "" FORCE)
  endif()
endforeach()

set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI ON CACHE BOOL "" FORCE)
set(AURORA_DAWN_PROVIDER package CACHE STRING "" FORCE)
if(ANDROID)
  set(AURORA_DAWN_LINKAGE static CACHE STRING "" FORCE)
  set(AURORA_SDL3_PROVIDER vendor CACHE STRING "" FORCE)
else()
  set(AURORA_DAWN_LINKAGE shared CACHE STRING "" FORCE)
  set(AURORA_SDL3_PROVIDER package CACHE STRING "" FORCE)
endif()
set(AURORA_SDL3_LINKAGE shared CACHE STRING "" FORCE)
if(MSVC)
  add_compile_options(/bigobj)
endif()
if(MSVC OR ANDROID)
  # The host game uses MinGW. Never discover its libraries/CRT headers from
  # PATH or pkg-config while building this separate MSVC DLL.
  set(CMAKE_DISABLE_FIND_PACKAGE_SQLite3 TRUE)
  set(CMAKE_DISABLE_FIND_PACKAGE_zstd TRUE)
  set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)
  if(ANDROID)
    set(PKG_CONFIG_FOUND FALSE)
    set(PkgConfig_FOUND FALSE)
  endif()
endif()
add_subdirectory(../third_party/aurora aurora EXCLUDE_FROM_ALL)
if(ANDROID)
  include(android-sdl.cmake)
endif()

# RelWithDebInfo otherwise uses /Ob1, which leaves Aurora's small FIFO,
# vertex and buffer helpers out of line in the per-primitive hot path.
# Keep symbols and the existing floating-point model while matching Release
# inlining. The option permits an identical-build A/B comparison.
option(SMS_AURORA_OPTIMIZE_INLINING "Use release inlining in the Aurora GX renderer" ON)
if(MSVC AND SMS_AURORA_OPTIMIZE_INLINING)
  target_compile_options(aurora_gx PRIVATE $<$<CONFIG:RelWithDebInfo>:/Ob2>)
endif()

add_library(sms_aurora SHARED bridge.cpp ui.cpp live.cpp sms_aurora.h sms_aurora_ui.h sms_aurora_settings.h)
if(ANDROID)
  target_sources(sms_aurora PRIVATE audio.cpp android_lifecycle.cpp)
endif()
target_include_directories(sms_aurora PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/aurora/lib"
    "${SMS_AURORA_DEPS}/rmlui-src/Backends")
target_link_libraries(sms_aurora PRIVATE aurora::core aurora::gx aurora::vi)
if(ANDROID)
  option(SMS_ANDROID_OPTIMIZE_RENDERER "Optimize ARM64 Aurora CPU command processing" ON)
  if(SMS_ANDROID_OPTIMIZE_RENDERER)
    foreach(_sms_renderer_target aurora_core aurora_gx sms_aurora)
      target_compile_options(${_sms_renderer_target} PRIVATE -O3)
    endforeach()
  endif()
endif()

if(WIN32)
  target_link_libraries(sms_aurora PRIVATE dbghelp)
endif()
if(ANDROID)
  # The game owns GX symbols too. Export only the bridge ABI so ELF symbol
  # interposition cannot route Aurora's internal GX calls back into the game.
  target_link_options(sms_aurora PRIVATE
    "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/android-exports.map")
  set_property(TARGET sms_aurora APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/android-exports.map")
endif()
# Next to the DLL: the runtime DLLs it needs (Dawn, SDL3).
include(../third_party/aurora/cmake/AuroraCopyRuntimeDLLs.cmake OPTIONAL RESULT_VARIABLE _copy_dlls)
if(_copy_dlls AND COMMAND aurora_copy_runtime_dlls)
  aurora_copy_runtime_dlls(sms_aurora)
endif()
