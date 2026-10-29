# SPDX-License-Identifier: GPL-3.0-or-later
#
# S4, the fibered hyperbolic 5-manifold of Italiano-Martelli-Migliorini, on the shared library src/:
#
#   target            binary                    source              legacy original        flags                           libraries
#   apps_s4_chz5      build/apps/s4/chz5        apps/s4/chz5.cc     legacy/s4/chz5.cc      -O2 -std=c++17 -Wall -Wextra
#   apps_s4_zipper5   build/apps/s4/zipper5     apps/s4/zipper5.cc  legacy/s4/zipper5.cc   -O2 -std=c++17 -Wall -Wextra    zlib
#   apps_s4_film5     build/apps/s4/film5       apps/s4/film5.cc    legacy/s4/film5.cc     -O2 -std=c++17 -Wall -Wextra    zlib, GLUT, OpenGL
#
# Compiled and linked exactly as the legacy original (legacy/s4/CMakeLists.txt explains each setting below), with
# src/ on the include path (zippers::core, header only, which adds no flags). -std=c++17 comes from the top level.
# chz5 writes its substitution file into the working directory; zipper5 reads one from there (by default
# subst5_2.30_4.50_t1_r0.txt of data/s4/) and writes its picture where -o says. film5 (the film's ending, a GLUT window)
# is built only with ZIPPERS_VIEWERS, as the legacy one; it reads its substitution file from the working directory too
# (by default subst5_2.30_4.50_t1.txt of legacy/s4/: the film keeps the file of the legacy ending).
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

add_executable(apps_s4_chz5 chz5.cc)
target_compile_options(apps_s4_chz5 PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_s4_chz5 PRIVATE -O2)
target_link_libraries(apps_s4_chz5 PRIVATE zippers::core)
set_target_properties(apps_s4_chz5 PROPERTIES OUTPUT_NAME chz5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_s4_zipper5 zipper5.cc)
target_compile_options(apps_s4_zipper5 PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_s4_zipper5 PRIVATE -O2)
target_link_libraries(apps_s4_zipper5 PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_s4_zipper5 PROPERTIES OUTPUT_NAME zipper5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(ZIPPERS_VIEWERS)
  add_executable(apps_s4_film5 film5.cc)
  target_compile_options(apps_s4_film5 PRIVATE -O2 -Wall -Wextra)
  target_link_options(apps_s4_film5 PRIVATE -O2)
  target_link_libraries(apps_s4_film5 PRIVATE zippers::core ZLIB::ZLIB zippers::gl)
  set_target_properties(apps_s4_film5 PROPERTIES OUTPUT_NAME film5 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()
