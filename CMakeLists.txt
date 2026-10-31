# SPDX-License-Identifier: GPL-3.0-or-later
#
# zippers: the top level of the build.
#
# This file sets up what the whole suite shares: the C++ standard, the options, zlib, the optional viewers and window,
# the tests and the install. It sets NO warning flags and NO optimisation flags of its own. Each program sets its own, so that
# the imported programs (legacy/) build exactly as their old makefiles did. Settings that programs may share are
# offered as targets to link against (zippers::warnings, zippers::gl) and as modules in cmake/.
#
# Normally this is run through the Makefile (`make`, `make test`, `make install`, `make clean`, `make sanitize`); see
# README.md.

cmake_minimum_required(VERSION 3.20)

# Building inside the source folder would write a generated Makefile over the wrapper Makefile of this repository.
get_filename_component(_zippers_src "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(_zippers_bin "${CMAKE_BINARY_DIR}" REALPATH)
if(_zippers_src STREQUAL _zippers_bin)
  message(FATAL_ERROR
    "Do not build inside the source folder. Run `make`, or `cmake -S . -B build && cmake --build build`.")
endif()

project(zippers
  VERSION 0.9
  DESCRIPTION "Zippers on the spheres at infinity of hyperbolic manifolds"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---- where an install puts things: CMake's standard folders (bin/, libexec/, share/, share/doc/zippers/) ------------
include(GNUInstallDirs)
include(ZippersData)       # zippers_data_places: the places of the data files, for src/core/data_files.h

# build/compile_commands.json lists the exact compile line of every file (useful for comparing with an old makefile)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- the language: C++17, with the platform's default C++ compiler ------------------------------------------------
# Without compiler extensions, so that the compile line says -std=c++17, as the old makefiles did (not -std=gnu++17).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- the build type: Release unless asked otherwise ---------------------------------------------------------------
# Note for the imported programs (legacy/): CMake's Release type puts "-O3 -DNDEBUG" at the start of every compile line,
# before the flags a target sets itself. A program that must keep exactly the flags of its old makefile can begin its
# CMakeLists.txt with   set(CMAKE_CXX_FLAGS_RELEASE "")   which affects only that folder. (-DCMAKE_BUILD_TYPE=None
# gives a build with no build-type flags at all.)
get_property(_zippers_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _zippers_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---- options -------------------------------------------------------------------------------------------------------
option(ZIPPERS_VIEWERS  "Build the GLUT viewers (needs GLUT and OpenGL)" ON)
option(ZIPPERS_SANITIZE "Build with the address and undefined-behaviour sanitizers (make sanitize)" OFF)
if(APPLE)
  option(ZIPPERS_FILM "Build the Swift film tools (macOS only; film/CMakeLists.txt reads this)" ON)
else()
  option(ZIPPERS_FILM "Build the Swift film tools (macOS only; film/CMakeLists.txt reads this)" OFF)
endif()

include(ZippersSanitize)   # applies ZIPPERS_SANITIZE to everything created below
include(ZippersWarnings)   # defines zippers::warnings (applied to nothing by itself)

# ---- libraries -----------------------------------------------------------------------------------------------------
find_package(ZLIB REQUIRED)  # provides ZLIB::ZLIB
include(ZippersViewers)      # with ZIPPERS_VIEWERS=ON: finds GLUT and OpenGL, defines zippers::gl
include(ZippersWindow)       # ZIPPERS_WINDOW (AUTO, ON, OFF): finds GLFW and OpenGL, defines zippers::window

# ---- tests and programs --------------------------------------------------------------------------------------------
enable_testing()

# The shared library: header only, the target zippers::core.
add_subdirectory(src)

# The new programs on the shared library (if apps/CMakeLists.txt exists).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/CMakeLists.txt")
  add_subdirectory(apps)
endif()

# The legacy programs and the film tools (each if its folder has a CMakeLists.txt).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/legacy/CMakeLists.txt")
  add_subdirectory(legacy)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/film/CMakeLists.txt")
  add_subdirectory(film)
endif()

add_subdirectory(tests)

# ---- install (cmake --install build --prefix P, or make install PREFIX=P) ------------------------------------------
# P/bin/zippers and P/libexec/zippers/ (the programs that zippers runs) are installed by apps/; here the data and the
# documents. Nothing of legacy/, film/ or apps/tools/ is installed.
install(DIRECTORY data/s3 data/s4 DESTINATION "${CMAKE_INSTALL_DATADIR}/zippers")
install(FILES data/README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/zippers")
install(FILES README.md COPYING CITATION.cff DESTINATION "${CMAKE_INSTALL_DOCDIR}")

# ---- summary -------------------------------------------------------------------------------------------------------
message(STATUS "zippers: build type '${CMAKE_BUILD_TYPE}', C++${CMAKE_CXX_STANDARD} with ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "zippers: viewers ${ZIPPERS_VIEWERS}, sanitizers ${ZIPPERS_SANITIZE}, film tools ${ZIPPERS_FILM}")
if(ZIPPERS_WINDOW_BUILT)
  message(STATUS "zippers: window ON")
else()
  message(STATUS "zippers: window OFF (${ZIPPERS_WINDOW_WHY})")
endif()
