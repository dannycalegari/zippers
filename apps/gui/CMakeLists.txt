# SPDX-License-Identifier: GPL-3.0-or-later
#
# gui/: the window's program and what it stands on.
#
#   target           what                                                     source              flags              libraries
#   apps_gui_jobs    the jobs: zippers draw run as child processes (jobs.h)  jobs.cc             -O2 -Wall -Wextra  zippers::core, threads
#   apps_gui_model   the model: the state and the handlers (model.h), and    model.cc, feed.cc   -O2 -Wall -Wextra  apps_gui_jobs
#                    the key feed (feed.h)
#   apps_gui_draw    the drawing in OpenGL 3.3 (draw.h), its functions        draw.cc             -O2 -Wall -Wextra  apps_gui_model
#                    loaded at run time, so that it links no OpenGL library
#   apps_gui_zview   build/apps/gui/zview, the window's program: with GLFW    zview.cc            -O2 -Wall -Wextra  apps_gui_model; with
#                    the window and --headless, without it --headless only                                         GLFW apps_gui_draw,
#                                                                                                                  zippers::window, zlib
#                    (the model, the jobs and the feed, with no window)
#
# With the window (ZIPPERS_WINDOW_BUILT, cmake/ZippersWindow.cmake) zview is compiled with ZIPPERS_WINDOW and installed
# into libexec/zippers/; without it, it is not installed. zview is given, as compile definitions, the path of the
# build's zippers (ZIPPERS_ZIPPERS), bin/ relative to libexec/zippers/ (ZIPPERS_INSTALLED_RELATIVE), the places of the
# data files and the version. The tests are tests/unit/test_gui_jobs.cc, test_gui_model.cc, test_gui_draw.cc and
# test_zview.py, and with the window the test that opens it (test_gui_window, label gui: make test-gui).
find_package(Threads REQUIRED)
add_library(apps_gui_jobs STATIC jobs.cc)
target_compile_options(apps_gui_jobs PRIVATE -O2 -Wall -Wextra)
target_include_directories(apps_gui_jobs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(apps_gui_jobs PUBLIC zippers::core Threads::Threads)

add_library(apps_gui_model STATIC model.cc feed.cc)
target_compile_options(apps_gui_model PRIVATE -O2 -Wall -Wextra)
target_link_libraries(apps_gui_model PUBLIC apps_gui_jobs)

add_library(apps_gui_draw STATIC draw.cc)
target_compile_options(apps_gui_draw PRIVATE -O2 -Wall -Wextra)
target_link_libraries(apps_gui_draw PUBLIC apps_gui_model)

set(CMAKE_CXX_FLAGS_RELEASE "")
add_executable(apps_gui_zview zview.cc)
target_compile_options(apps_gui_zview PRIVATE -O2 -Wall -Wextra)
if(ZIPPERS_WINDOW_BUILT)
  target_link_libraries(apps_gui_zview PRIVATE apps_gui_draw zippers::window ZLIB::ZLIB)
  target_compile_definitions(apps_gui_zview PRIVATE ZIPPERS_WINDOW=1)
  install(TARGETS apps_gui_zview RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/zippers")
else()
  target_link_libraries(apps_gui_zview PRIVATE apps_gui_model)
endif()
set_target_properties(apps_gui_zview PROPERTIES OUTPUT_NAME zview RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
zippers_data_places(apps_gui_zview "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers")
file(RELATIVE_PATH _zview_to_bin "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers" "${CMAKE_INSTALL_FULL_BINDIR}")
target_compile_definitions(apps_gui_zview PRIVATE "ZIPPERS_ZIPPERS=\"$<TARGET_FILE:apps_cli_zippers>\""
                           "ZIPPERS_INSTALLED_RELATIVE=\"${_zview_to_bin}\"" "ZIPPERS_VERSION=\"${PROJECT_VERSION}\"")
