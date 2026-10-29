# SPDX-License-Identifier: GPL-3.0-or-later
#
# S2, the 3-manifold program and its film engine, on the shared library src/:
#
#   target           binary                  source              legacy original (target)                    flags                          libraries
#   apps_s2_zipper   build/apps/s2/zipper    apps/s2/zipper.cc   legacy/s2/zipper.cc (s2_zipper_nogui)         -std=c++17 -O3 -Wall           zlib
#   apps_s2_zmovie   build/apps/s2/zmovie    apps/s2/zmovie.cc   legacy/s2/movie/src/zmovie.cc (s2_zmovie)     -std=c++17 -O3 -Wall -Wextra   zlib
#   apps_s2_ztest    build/apps/s2/ztest     apps/s2/ztest.cc    legacy/s2/movie/src/ztest.cc (s2_ztest)       -std=c++17 -O3 -Wall -Wextra   zlib
#
# Compiled and linked exactly as the legacy originals (legacy/s2/CMakeLists.txt explains each setting below), with
# src/ on the include path (zippers::core, header only, which adds no flags). -std=c++17 comes from the top level.
# zipper is the batch build of the legacy program (the legacy target s2_zipper_nogui, without ZIPPER_VIEWER): there is
# no window in apps/. zmovie.cc includes zipper.cc of this folder (with main renamed), as the legacy engine
# includes its snapshot of the program, and ztest.cc includes zmovie.cc; both are found next to the file that includes
# them, so no snapshot is made. The film pipeline (legacy/s2/movie/*.py) still runs the legacy binaries.
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

add_executable(apps_s2_zipper zipper.cc)
target_compile_options(apps_s2_zipper PRIVATE -O3 -Wall)
target_link_options(apps_s2_zipper PRIVATE -O3)
target_link_libraries(apps_s2_zipper PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_s2_zipper PROPERTIES OUTPUT_NAME zipper RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

foreach(_prog zmovie ztest)
  add_executable(apps_s2_${_prog} ${_prog}.cc)
  target_compile_options(apps_s2_${_prog} PRIVATE -O3 -Wall -Wextra)
  target_link_options(apps_s2_${_prog} PRIVATE -O3)
  target_link_libraries(apps_s2_${_prog} PRIVATE zippers::core ZLIB::ZLIB)
  set_target_properties(apps_s2_${_prog} PROPERTIES OUTPUT_NAME ${_prog} RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
