# SPDX-License-Identifier: GPL-3.0-or-later
#
# S4: the fibered hyperbolic 5-manifold of Italiano-Martelli-Migliorini, its zippers on S^4: chz5 (the block-filling
# search), zipper5 (stills) and film5 (the film's ending, a GLUT window), as imported (../README.md lists the lines
# changed since), built with exactly the flags of the old makefile (legacy/s3/makefile.original, which built S3 and
# S4 together):
#
#   target       binary                     flags                           libraries
#   s4_chz5      build/legacy/s4/chz5       -O2 -std=c++17 -Wall -Wextra
#   s4_zipper5   build/legacy/s4/zipper5    -O2 -std=c++17 -Wall -Wextra    zlib
#   s4_film5     build/legacy/s4/film5      -O2 -std=c++17 -Wall -Wextra    zlib, GLUT, OpenGL
#
# -std=c++17 comes from the top level. The programs read their data from the working directory (by default
# subst5_2.30_4.50_t1.txt), so run them in this folder:   cd legacy/s4 && ../../build/legacy/s4/zipper5 -o still.png

# Make the compile and link lines those of the old makefile:
#  - CMake's Release type would put -O3 -DNDEBUG in front of the flags below; the old makefile had neither;
#  - the old makefile compiled and linked in one command, so the linker driver saw the optimisation flag too
#    (each target passes it with target_link_options); without it, Apple's linker lays the binary out differently;
#  - on macOS CMake links executables with -Wl,-search_paths_first -Wl,-headerpad_max_install_names (from
#    CMAKE_CXX_LINK_FLAGS, or CMAKE_EXECUTABLE_CREATE_CXX_FLAGS under policy CMP0210 of CMake 4.3); the second
#    changes the binary (more room in its header), and the old makefile had neither.
# With this, each binary is byte for byte the one the old makefile builds with the same compiler.
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

add_executable(s4_chz5 chz5.cc)
target_compile_options(s4_chz5 PRIVATE -O2 -Wall -Wextra)
target_link_options(s4_chz5 PRIVATE -O2)
set_target_properties(s4_chz5 PROPERTIES OUTPUT_NAME chz5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(s4_zipper5 zipper5.cc)
target_compile_options(s4_zipper5 PRIVATE -O2 -Wall -Wextra)
target_link_options(s4_zipper5 PRIVATE -O2)
target_link_libraries(s4_zipper5 PRIVATE ZLIB::ZLIB)
set_target_properties(s4_zipper5 PROPERTIES OUTPUT_NAME zipper5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(ZIPPERS_VIEWERS)
  add_executable(s4_film5 film5.cc)
  target_compile_options(s4_film5 PRIVATE -O2 -Wall -Wextra)
  target_link_options(s4_film5 PRIVATE -O2)
  target_link_libraries(s4_film5 PRIVATE ZLIB::ZLIB zippers::gl)
  set_target_properties(s4_film5 PROPERTIES OUTPUT_NAME film5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()
