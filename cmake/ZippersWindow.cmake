# SPDX-License-Identifier: GPL-3.0-or-later
#
# The window of `zippers view` (the cache string ZIPPERS_WINDOW: AUTO, the default; ON; OFF).
#
#   AUTO   the window is built when GLFW 3.3 or later and OpenGL are found; otherwise everything else is built as
#          without them, and a status line says why there is no window;
#   ON     they are required: configuring stops, with a message that says what to install, when they are not found;
#   OFF    nothing is searched for, and there is no window.
#
# GLFW is found first as a CMake package (find_package(glfw3 3.3 CONFIG), the imported target glfw: Homebrew's and
# Debian's packages install one), else through pkg-config (glfw3 >= 3.3). OpenGL is CMake's FindOpenGL (OpenGL::GL: the
# framework on macOS, libGL on Linux).
#
# It sets ZIPPERS_WINDOW_BUILT (ON or OFF) and ZIPPERS_WINDOW_WHY (why there is no window), and with the window it
# defines the target
#
#     zippers::window
#
# (GLFW and OpenGL) for the window's program to link against. Without GLFW, zview is built all the same, with its
# headless mode only (apps/gui/).

include_guard(GLOBAL)

set(ZIPPERS_WINDOW AUTO CACHE STRING "Build the window of zippers view: AUTO (when GLFW 3.3+ and OpenGL are found), ON, OFF")
set_property(CACHE ZIPPERS_WINDOW PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${ZIPPERS_WINDOW}" _zippers_window)
if(NOT _zippers_window MATCHES "^(AUTO|ON|OFF)$")
  message(FATAL_ERROR "ZIPPERS_WINDOW is '${ZIPPERS_WINDOW}': it is AUTO, ON or OFF")
endif()

set(ZIPPERS_WINDOW_BUILT OFF)
set(ZIPPERS_WINDOW_WHY "")
if(_zippers_window STREQUAL "OFF")
  set(ZIPPERS_WINDOW_WHY "ZIPPERS_WINDOW is OFF")
else()
  set(_zippers_glfw "")
  find_package(glfw3 3.3 CONFIG QUIET)
  if(TARGET glfw)
    set(_zippers_glfw glfw)
    set(_zippers_glfw_how "GLFW ${glfw3_VERSION} (CMake package)")
  else()
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(ZIPPERS_GLFW QUIET IMPORTED_TARGET glfw3>=3.3)
      if(ZIPPERS_GLFW_FOUND)
        set(_zippers_glfw PkgConfig::ZIPPERS_GLFW)
        set(_zippers_glfw_how "GLFW ${ZIPPERS_GLFW_VERSION} (pkg-config)")
      endif()
    endif()
  endif()
  find_package(OpenGL QUIET)
  if(_zippers_glfw AND TARGET OpenGL::GL)
    add_library(zippers_window INTERFACE)
    add_library(zippers::window ALIAS zippers_window)
    target_link_libraries(zippers_window INTERFACE ${_zippers_glfw} OpenGL::GL)
    set(ZIPPERS_WINDOW_BUILT ON)
    message(STATUS "zippers: the window with ${_zippers_glfw_how} and OpenGL")
  else()
    if(NOT _zippers_glfw)
      set(ZIPPERS_WINDOW_WHY "GLFW 3.3 or later was not found")
    else()
      set(ZIPPERS_WINDOW_WHY "OpenGL was not found")
    endif()
    if(_zippers_window STREQUAL "ON")
      message(FATAL_ERROR
        "ZIPPERS_WINDOW is ON, but ${ZIPPERS_WINDOW_WHY}.\n"
        "On macOS with Homebrew: brew install glfw. On Ubuntu or Debian: sudo apt-get install libglfw3-dev.\n"
        "To build without the window, configure with -DZIPPERS_WINDOW=AUTO or OFF.")
    endif()
    message(STATUS "zippers: no window (${ZIPPERS_WINDOW_WHY}); zview is built with its headless mode only")
  endif()
endif()
