# SPDX-License-Identifier: GPL-3.0-or-later
#
# S3, the Cartwright-Steger surface, on the shared library src/:
#
#   target           binary                    source               legacy original                            flags                           libraries
#   apps_s3_chz      build/apps/s3/chz         apps/s3/chz.cc       legacy/s3/chz.cc                           -O2 -std=c++17 -Wall -Wextra
#   apps_s3_zipper3  build/apps/s3/zipper3     apps/s3/zipper3.cc   legacy/s3/zipper3.cc (its nogui build)     -O2 -std=c++17 -Wall -Wextra    zlib
#   apps_s3_link3    build/apps/s3/link3       apps/s3/link3.cc     legacy/s3/link3.cc                         -O2 -std=c++17 -Wall -Wextra    zlib
#
# Compiled and linked exactly as the legacy original (legacy/s3/CMakeLists.txt explains each setting below), with
# src/ on the include path (zippers::core, header only, which adds no flags). -std=c++17 comes from the top level.
# chz writes its substitution file into the working directory. zipper3 and link3 find theirs by the search of
# src/core/data_files.h, which CMake gives the places of (zippers_data_places): the working folder first, then
# ZIPPERS_DATA, then data/ of the source tree (or, installed, share/zippers/). By default zipper3 reads
# subst_4.00_9.50_t3_r1.txt, and with --pairs subst_4.00_9.50_t3.txt, which is also link3's (both in data/s3/).
# zipper3 is the batch build of the legacy program (the legacy target s3_zipper3_nogui, without ZIPPER3_VIEWER): there is no
# window in apps/.
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

add_executable(apps_s3_chz chz.cc)
target_compile_options(apps_s3_chz PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_s3_chz PRIVATE -O2)
target_link_libraries(apps_s3_chz PRIVATE zippers::core)
set_target_properties(apps_s3_chz PROPERTIES OUTPUT_NAME chz RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_s3_zipper3 zipper3.cc)
target_compile_options(apps_s3_zipper3 PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_s3_zipper3 PRIVATE -O2)
target_link_libraries(apps_s3_zipper3 PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_s3_zipper3 PROPERTIES OUTPUT_NAME zipper3 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(apps_s3_link3 link3.cc)
target_compile_options(apps_s3_link3 PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_s3_link3 PRIVATE -O2)
target_link_libraries(apps_s3_link3 PRIVATE zippers::core ZLIB::ZLIB)
set_target_properties(apps_s3_link3 PROPERTIES OUTPUT_NAME link3 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

zippers_data_places(apps_s3_zipper3 "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers")
zippers_data_places(apps_s3_link3 "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers")

# installed into libexec/zippers/, where zippers runs them
install(TARGETS apps_s3_chz apps_s3_zipper3 apps_s3_link3 RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/zippers")
