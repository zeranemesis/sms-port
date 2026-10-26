# Aurora must hold SDL's actual activity mutex around Android surface access.
# SDL's shared-library ABI normally hides these two internal entry points.
# Extend this vendored build's export map without changing their implementation.
if(NOT TARGET SDL3-shared)
  message(FATAL_ERROR "Android Aurora requires the vendored shared SDL3 target")
endif()
get_target_property(_sms_sdl_source SDL3-shared SOURCE_DIR)
file(READ "${_sms_sdl_source}/src/dynapi/SDL_dynapi.sym" _sms_sdl_symbols)
set(_sms_sdl_marker "    # extra symbols go here (don't modify this line)")
string(FIND "${_sms_sdl_symbols}" "${_sms_sdl_marker}" _sms_sdl_marker_at)
if(_sms_sdl_marker_at EQUAL -1)
  message(FATAL_ERROR "SDL export-map layout changed; review Android surface locking")
endif()
string(REPLACE "${_sms_sdl_marker}"
  "    Android_LockActivityMutex;\n    Android_UnlockActivityMutex;\n${_sms_sdl_marker}"
  _sms_sdl_symbols "${_sms_sdl_symbols}")
set(_sms_sdl_map "${CMAKE_CURRENT_BINARY_DIR}/sms-sdl-exports.sym")
file(CONFIGURE OUTPUT "${_sms_sdl_map}" CONTENT "${_sms_sdl_symbols}" @ONLY)
get_target_property(_sms_sdl_options SDL3-shared LINK_OPTIONS)
list(FILTER _sms_sdl_options EXCLUDE REGEX "version-script")
list(APPEND _sms_sdl_options "-Wl,--version-script=${_sms_sdl_map}")
set_property(TARGET SDL3-shared PROPERTY LINK_OPTIONS "${_sms_sdl_options}")
set_property(TARGET SDL3-shared APPEND PROPERTY LINK_DEPENDS "${_sms_sdl_map}")
# All other internals in this file remain local through the explicit export map.
set_property(SOURCE "${_sms_sdl_source}/src/core/android/SDL_android.c"
  TARGET_DIRECTORY SDL3-shared APPEND PROPERTY COMPILE_OPTIONS -fvisibility=default)
