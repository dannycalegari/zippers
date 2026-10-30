# SPDX-License-Identifier: GPL-3.0-or-later
#
# The places of the data files, for the search of src/core/data_files.h.
#
#     zippers_data_places(TARGET INSTALL_FOLDER)
#
# gives TARGET the three compile definitions that data_files.h reads (ZIPPERS_DATA_PLACES):
#   ZIPPERS_BUILD_FOLDER    the folder in which the build puts TARGET (its RUNTIME_OUTPUT_DIRECTORY, which must be set);
#   ZIPPERS_SOURCE_DATA     the data/ folder of the source tree;
#   ZIPPERS_INSTALLED_DATA  the installed data folder, share/zippers/ (CMAKE_INSTALL_DATADIR), as a path relative to
#                           INSTALL_FOLDER, the folder into which TARGET is installed (an absolute path under the
#                           install prefix, such as ${CMAKE_INSTALL_FULL_LIBEXECDIR}/zippers).
# The relative path does not depend on the prefix, so an installed tree can be moved, and installed with another prefix
# (cmake --install --prefix). Needs GNUInstallDirs.

function(zippers_data_places target install_folder)
  get_target_property(_dir ${target} RUNTIME_OUTPUT_DIRECTORY)
  if(NOT _dir)
    message(FATAL_ERROR "zippers_data_places: ${target} has no RUNTIME_OUTPUT_DIRECTORY")
  endif()
  file(RELATIVE_PATH _rel "${install_folder}" "${CMAKE_INSTALL_FULL_DATADIR}/zippers")
  target_compile_definitions(${target} PRIVATE
    "ZIPPERS_BUILD_FOLDER=\"${_dir}\""
    "ZIPPERS_SOURCE_DATA=\"${PROJECT_SOURCE_DIR}/data\""
    "ZIPPERS_INSTALLED_DATA=\"${_rel}\"")
endfunction()
