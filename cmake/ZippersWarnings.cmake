# SPDX-License-Identifier: GPL-3.0-or-later
#
# zippers::warnings: the warning flags for new code.
#
# A target opts in with
#
#     target_link_libraries(<target> PRIVATE zippers::warnings)
#
# Nothing applies it automatically. The imported programs (legacy/) keep exactly the flags of their old makefiles,
# so they set their own and do not use this target.

include_guard(GLOBAL)

add_library(zippers_warnings INTERFACE)
add_library(zippers::warnings ALIAS zippers_warnings)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(zippers_warnings INTERFACE -Wall -Wextra)
endif()
