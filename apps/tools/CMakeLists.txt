# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/: measuring programs on the shared library src/. They change nothing in src/ and have no reference runs;
# their outputs are measurements (docs/developer_guide.md has the ones the code relies on).
#
#   target              binary                     source                flags                           libraries
#   apps_tools_strays   build/apps/tools/strays    apps/tools/strays.cc  -O2 -std=c++17 -Wall -Wextra    zlib
#   apps_tools_islands5 build/apps/tools/islands5  apps/tools/islands5.cc -O2 -std=c++17 -Wall -Wextra   zlib
#   apps_tools_cull     build/apps/tools/cull      apps/tools/cull.cc    -O2 -std=c++17 -Wall -Wextra    zlib
#   apps_tools_roofs    build/apps/tools/roofs     apps/tools/roofs.cc   -O2 -std=c++17 -Wall -Wextra    -
#
#   strays: how far the true zipper curves of S^3 and S^4 stray from the drawn chords (the top of strays.cc).
#   islands5: the islands of the finite zippers of S^4, as a graph (vertices identified exactly) and as drawn
#   (through the branches the renderer draws once); the top of islands5.cc.
#   cull: the measurements of the deep zoom: how far a branch's curve strays from the points of its cull test,
#   in units of its size (to choose the margin factor K), and a picture with culling against the same picture without
#   it, in the window; the focus centres; the cusp points of the tips (the top of cull.cc).
#   roofs: the roofs of a substitution file measured: the exact check of every roof, the inverse words, the
#   self-inverse involution roofs (and whether the two orientations of a branch agree at every level), the lengths, the
#   cancelling junctions of theta and theta^2 and at every level, and the growth rate (the top of roofs.cc).
#
# Built like the other apps (apps/s3/CMakeLists.txt, apps/s4/CMakeLists.txt): without CMake's Release flags, at -O2,
# with src/ on the include path (zippers::core, header only). -std=c++17 comes from the top level.
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

add_executable(apps_tools_strays strays.cc)
target_compile_options(apps_tools_strays PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_tools_strays PRIVATE -O2)
target_link_libraries(apps_tools_strays PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_tools_strays PROPERTIES OUTPUT_NAME strays RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_tools_islands5 islands5.cc)
target_compile_options(apps_tools_islands5 PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_tools_islands5 PRIVATE -O2)
target_link_libraries(apps_tools_islands5 PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_tools_islands5 PROPERTIES OUTPUT_NAME islands5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_tools_overdraw overdraw.cc)
target_compile_options(apps_tools_overdraw PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_tools_overdraw PRIVATE -O2)
target_link_libraries(apps_tools_overdraw PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_tools_overdraw PROPERTIES OUTPUT_NAME overdraw RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_tools_cull cull.cc)
target_compile_options(apps_tools_cull PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_tools_cull PRIVATE -O2)
target_link_libraries(apps_tools_cull PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_tools_cull PROPERTIES OUTPUT_NAME cull RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_tools_roofs roofs.cc)
target_compile_options(apps_tools_roofs PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_tools_roofs PRIVATE -O2)
target_link_libraries(apps_tools_roofs PRIVATE zippers::core)
set_target_properties(apps_tools_roofs PROPERTIES OUTPUT_NAME roofs RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# The tools find their substitution file by the search of src/core/data_files.h, as zipper3 and zipper5 do
# (default_file.h); they are not installed, so in practice they read the working folder, ZIPPERS_DATA, or data/ of the
# source tree.
foreach(_tool strays islands5 overdraw cull roofs)
  zippers_data_places(apps_tools_${_tool} "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers")
endforeach()
