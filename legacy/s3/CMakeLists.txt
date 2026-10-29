# SPDX-License-Identifier: GPL-3.0-or-later
#
# S3: the Cartwright-Steger surface, its zippers on S^3: chz (the block-filling search), zipper3 (renderer and viewer)
# and link3 (linking numbers), as imported (../README.md lists the lines changed since), built with exactly the flags
# of the old makefile (makefile.original):
#
#   target             binary                          flags                                     libraries
#   s3_chz             build/legacy/s3/chz             -O2 -std=c++17 -Wall -Wextra
#   s3_zipper3         build/legacy/s3/zipper3         -O2 -std=c++17 -Wall -Wextra -DZIPPER3_VIEWER   zlib, GLUT, OpenGL
#   s3_zipper3_nogui   build/legacy/s3/nogui/zipper3   -O2 -std=c++17 -Wall -Wextra              zlib
#   s3_link3           build/legacy/s3/link3           -O2 -std=c++17 -Wall -Wextra              zlib
#
# s3_zipper3_nogui is the old `make nogui`. -std=c++17 comes from the top level.
#
# The programs read their data from the working directory (by default subst_4.00_9.50_t3.txt), so run them in this
# folder, where the data are:   cd legacy/s3 && ../../build/legacy/s3/zipper3 -R 3 -o picture.png

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

add_executable(s3_chz chz.cc)
target_compile_options(s3_chz PRIVATE -O2 -Wall -Wextra)
target_link_options(s3_chz PRIVATE -O2)
set_target_properties(s3_chz PROPERTIES OUTPUT_NAME chz RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(ZIPPERS_VIEWERS)
  add_executable(s3_zipper3 zipper3.cc)
  target_compile_definitions(s3_zipper3 PRIVATE ZIPPER3_VIEWER)
  target_compile_options(s3_zipper3 PRIVATE -O2 -Wall -Wextra)
  target_link_options(s3_zipper3 PRIVATE -O2)
  target_link_libraries(s3_zipper3 PRIVATE ZLIB::ZLIB zippers::gl)
  set_target_properties(s3_zipper3 PROPERTIES OUTPUT_NAME zipper3 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

add_executable(s3_zipper3_nogui zipper3.cc)
target_compile_options(s3_zipper3_nogui PRIVATE -O2 -Wall -Wextra)
target_link_options(s3_zipper3_nogui PRIVATE -O2)
target_link_libraries(s3_zipper3_nogui PRIVATE ZLIB::ZLIB)
set_target_properties(s3_zipper3_nogui PROPERTIES
  OUTPUT_NAME zipper3
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/nogui")

add_executable(s3_link3 link3.cc)
target_compile_options(s3_link3 PRIVATE -O2 -Wall -Wextra)
target_link_options(s3_link3 PRIVATE -O2)
target_link_libraries(s3_link3 PRIVATE ZLIB::ZLIB)
set_target_properties(s3_link3 PROPERTIES OUTPUT_NAME link3 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
