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
  string(TOUPPER "${dep}" dep_upper)
  if(EXISTS "${SMS_AURORA_DEPS}/${dep}-src")
    set(FETCHCONTENT_SOURCE_DIR_${dep_upper} "${SMS_AURORA_DEPS}/${dep}-src" CACHE PATH "" FORCE)
  endif()
endforeach()

set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "" FORCE)
set(AURORA_DAWN_PROVIDER package CACHE STRING "" FORCE)
set(AURORA_DAWN_LINKAGE shared CACHE STRING "" FORCE)
set(AURORA_SDL3_PROVIDER package CACHE STRING "" FORCE)
set(AURORA_SDL3_LINKAGE shared CACHE STRING "" FORCE)
if(MSVC)
  add_compile_options(/bigobj)
endif()
add_subdirectory(../third_party/aurora aurora EXCLUDE_FROM_ALL)

add_library(sms_aurora SHARED bridge.cpp sms_aurora.h)
target_link_libraries(sms_aurora PRIVATE aurora::core aurora::gx aurora::vi dbghelp)
# Next to the DLL: the runtime DLLs it needs (Dawn, SDL3).
include(../third_party/aurora/cmake/AuroraCopyRuntimeDLLs.cmake OPTIONAL RESULT_VARIABLE _copy_dlls)
if(_copy_dlls AND COMMAND aurora_copy_runtime_dlls)
  aurora_copy_runtime_dlls(sms_aurora)
endif()
