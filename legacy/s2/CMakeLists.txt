# SPDX-License-Identifier: GPL-3.0-or-later
#
# S2: the 3-manifold program zipper (zipper.cc) and its film engine (movie/src/zmovie.cc, ztest.cc), as imported (no
# line of these three files has changed), built with exactly the flags of the old makefiles (makefile.original,
# movie/src/Makefile.original):
#
#   target            binary                              flags                                  libraries
#   s2_zipper         build/legacy/s2/zipper              -std=c++17 -O3 -Wall -DZIPPER_VIEWER   zlib, GLUT, OpenGL
#   s2_zipper_nogui   build/legacy/s2/nogui/zipper        -std=c++17 -O3 -Wall                   zlib
#   s2_zmovie         build/legacy/s2/movie/bin/zmovie    -std=c++17 -O3 -Wall -Wextra           zlib
#   s2_ztest          build/legacy/s2/movie/bin/ztest     -std=c++17 -O3 -Wall -Wextra           zlib
#
# s2_zipper is the old `make`, s2_zipper_nogui the old `make nogui`. -std=c++17 comes from the top level. The viewer
# build needs the macOS GLUT framework, as before (zipper.cc includes <GLUT/glut.h>). The old film Makefile named the
# Command Line Tools' clang++ and SDK; here the engine is built with the default C++ compiler, like everything else.

# Make the compile and link lines those of the old makefiles:
#  - CMake's Release type would put -O3 -DNDEBUG in front of the flags below; the old makefiles had neither;
#  - the old makefiles compiled and linked in one command, so the linker driver saw the optimisation flag too
#    (each target passes it with target_link_options); without it, Apple's linker lays the binary out differently;
#  - on macOS CMake links executables with -Wl,-search_paths_first -Wl,-headerpad_max_install_names (from
#    CMAKE_CXX_LINK_FLAGS, or CMAKE_EXECUTABLE_CREATE_CXX_FLAGS under policy CMP0210 of CMake 4.3); the second
#    changes the binary (more room in its header), and the old makefiles had neither.
# With this, each binary is byte for byte the one the old makefiles build with the same compiler (for zmovie and ztest:
# the old film Makefile with CXX set to the default compiler, which was checked by building both and comparing).
set(CMAKE_CXX_FLAGS_RELEASE "")
if(APPLE)
  set(CMAKE_CXX_LINK_FLAGS "")
  set(CMAKE_EXECUTABLE_CREATE_CXX_FLAGS "")
endif()

# ---- zipper ---------------------------------------------------------------------------------------------------------
if(ZIPPERS_VIEWERS AND APPLE)
  add_executable(s2_zipper zipper.cc)
  target_compile_definitions(s2_zipper PRIVATE ZIPPER_VIEWER)
  target_compile_options(s2_zipper PRIVATE -O3 -Wall)
  target_link_options(s2_zipper PRIVATE -O3)
  target_link_libraries(s2_zipper PRIVATE ZLIB::ZLIB zippers::gl)
  set_target_properties(s2_zipper PROPERTIES
    OUTPUT_NAME zipper
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
elseif(ZIPPERS_VIEWERS)
  message(STATUS "zippers: the S2 viewer (s2_zipper) needs the macOS GLUT framework, as before; building s2_zipper_nogui only")
endif()

add_executable(s2_zipper_nogui zipper.cc)
target_compile_options(s2_zipper_nogui PRIVATE -O3 -Wall)
target_link_options(s2_zipper_nogui PRIVATE -O3)
target_link_libraries(s2_zipper_nogui PRIVATE ZLIB::ZLIB)
set_target_properties(s2_zipper_nogui PROPERTIES
  OUTPUT_NAME zipper
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/nogui")

# ---- the film engine ------------------------------------------------------------------------------------------------
# zmovie.cc includes "zipper_snapshot.cc": the program's zipper.cc, with main renamed by a #define. The old folder kept
# a copy under that name. Here CMake makes it from zipper.cc, byte for byte, when it configures (and again whenever
# zipper.cc changes), in a folder of its own on the include path of the two targets.
set(_s2_snapshot_dir "${CMAKE_CURRENT_BINARY_DIR}/movie/snapshot")
configure_file(zipper.cc "${_s2_snapshot_dir}/zipper_snapshot.cc" COPYONLY)

set(_s2_movie_bin "${CMAKE_CURRENT_BINARY_DIR}/movie/bin")
foreach(_prog zmovie ztest)
  add_executable(s2_${_prog} movie/src/${_prog}.cc)
  target_include_directories(s2_${_prog} PRIVATE "${_s2_snapshot_dir}")
  target_compile_options(s2_${_prog} PRIVATE -O3 -Wall -Wextra)
  target_link_options(s2_${_prog} PRIVATE -O3)
  target_link_libraries(s2_${_prog} PRIVATE ZLIB::ZLIB)
  set_target_properties(s2_${_prog} PROPERTIES
    OUTPUT_NAME ${_prog}
    RUNTIME_OUTPUT_DIRECTORY "${_s2_movie_bin}")
endforeach()

# ---- legacy/s2/movie/bin --------------------------------------------------------------------------------------------
# The film pipeline (movie/make_movie.py, movie/render_film.py, movie/tests/*.sh) runs its binaries from
# legacy/s2/movie/bin, as in the old folder. So
#  - build/legacy/s2/movie/bin holds zmovie and ztest, and links to the five Swift tools, which film/CMakeLists.txt
#    builds into build/film/swift/ (with ZIPPERS_FILM);
#  - every build makes legacy/s2/movie/bin a link to that folder (git ignores it). The link is relative when the build
#    folder is inside the repository, so that it survives moving or syncing the repository. A sanitizer build leaves
#    the link alone.
file(MAKE_DIRECTORY "${_s2_movie_bin}")
if(ZIPPERS_FILM)
  foreach(_tool encode_movie add_audio probe_movie grab_frames extract_audio)
    file(RELATIVE_PATH _rel "${_s2_movie_bin}" "${PROJECT_BINARY_DIR}/film/swift/${_tool}")
    file(CREATE_LINK "${_rel}" "${_s2_movie_bin}/${_tool}" SYMBOLIC)
  endforeach()
endif()

if(ZIPPERS_SANITIZE)
  message(STATUS "zippers: a sanitizer build, so legacy/s2/movie/bin is not made a link to it")
else()
  get_filename_component(_repo "${PROJECT_SOURCE_DIR}" REALPATH)
  get_filename_component(_here "${CMAKE_CURRENT_SOURCE_DIR}/movie" REALPATH)
  get_filename_component(_bin "${_s2_movie_bin}" REALPATH)
  string(FIND "${_bin}/" "${_repo}/" _pos)
  if(_pos EQUAL 0)
    file(RELATIVE_PATH _target "${_here}" "${_bin}")
  else()
    set(_target "${_bin}")
  endif()
  add_custom_target(s2_movie_bin_link ALL
    COMMAND "${CMAKE_COMMAND}" "-DLINK=${CMAKE_CURRENT_SOURCE_DIR}/movie/bin" "-DTARGET=${_target}"
            -P "${PROJECT_SOURCE_DIR}/cmake/ZippersMakeLink.cmake"
    VERBATIM)
  add_dependencies(s2_movie_bin_link s2_zmovie s2_ztest)
endif()
