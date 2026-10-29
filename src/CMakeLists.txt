# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/: the shared library of the suite. It is header only. Programs and tests link the INTERFACE target
# zippers::core, which puts src/ on the include path, so they write for example
#     #include "geometry/backend.h"
# It adds no compile flags: each program keeps the flags of its legacy original.

add_library(zippers_core INTERFACE)
add_library(zippers::core ALIAS zippers_core)
target_include_directories(zippers_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
