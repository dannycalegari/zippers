# SPDX-License-Identifier: GPL-3.0-or-later
#
# cli/: zippers, the command line of the suite (zippers.cc). It checks and translates its options, then runs one of the
# programs of apps/ in its place (execv); the top of zippers.cc says how.
#
#   target            binary                   source               flags                           libraries
#   apps_cli_zippers  build/apps/cli/zippers   apps/cli/zippers.cc  -O2 -std=c++17 -Wall -Wextra    -
#
# It is given, as compile definitions:
#   - the paths of the programs in this build (ZIPPERS_PROGRAM_<name>), which it runs when it runs from this folder;
#   - ZIPPERS_INSTALLED_PROGRAMS, libexec/zippers/ relative to bin/, where it finds them when it is installed;
#   - the places of the data files (zippers_data_places, cmake/ZippersData.cmake), for zippers list;
#   - ZIPPERS_VERSION, the version of the project.
# It depends on the seven programs, so that building zippers builds them. Installed into bin/.
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

add_executable(apps_cli_zippers zippers.cc)
target_compile_options(apps_cli_zippers PRIVATE -O2 -Wall -Wextra)
target_link_options(apps_cli_zippers PRIVATE -O2)
target_link_libraries(apps_cli_zippers PRIVATE zippers::core)
set_target_properties(apps_cli_zippers PROPERTIES OUTPUT_NAME zippers RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
zippers_data_places(apps_cli_zippers "${CMAKE_INSTALL_FULL_BINDIR}")

set(_zippers_programs
  zipper  apps_s2_zipper
  zmovie  apps_s2_zmovie
  chz     apps_s3_chz
  zipper3 apps_s3_zipper3
  link3   apps_s3_link3
  chz5    apps_s4_chz5
  zipper5 apps_s4_zipper5)
set(_defs)
set(_deps)
while(_zippers_programs)
  list(POP_FRONT _zippers_programs _name _target)
  list(APPEND _defs "ZIPPERS_PROGRAM_${_name}=\"$<TARGET_FILE:${_target}>\"")
  list(APPEND _deps ${_target})
endwhile()
file(RELATIVE_PATH _rel "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers")
target_compile_definitions(apps_cli_zippers PRIVATE ${_defs}
  "ZIPPERS_INSTALLED_PROGRAMS=\"${_rel}\"" "ZIPPERS_VERSION=\"${PROJECT_VERSION}\"")
add_dependencies(apps_cli_zippers ${_deps})

install(TARGETS apps_cli_zippers RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
