# SPDX-License-Identifier: GPL-3.0-or-later
#
# film/: the Swift film tools (macOS, option ZIPPERS_FILM), one copy for both films, built with `swiftc -O` as in the
# old makefiles, into build/film/swift/:
#
#   target              binary                          source
#   film_encode_movie   build/film/swift/encode_movie   swift/encode_movie.swift    PNG frames -> MP4
#   film_add_audio      build/film/swift/add_audio      swift/add_audio.swift       put a soundtrack on a film
#   film_probe_movie    build/film/swift/probe_movie    swift/probe_movie.swift     what is in an MP4
#   film_grab_frames    build/film/swift/grab_frames    swift/grab_frames.swift     frames of an MP4 as PNG
#   film_extract_audio  build/film/swift/extract_audio  swift/extract_audio.swift   the audio of an MP4 as WAV
#
# The S2 film pipeline finds them through legacy/s2/movie/bin (see legacy/s2/CMakeLists.txt). The old S2 film
# Makefile (legacy/s2/movie/src/Makefile.original) passed `-sdk` with a Command Line Tools SDK, as a workaround for a
# half-installed toolchain; the S3/S4 makefile did not. To pass one, configure with -DZIPPERS_SWIFT_SDK=<path>.
# The Python scripts of film/ are run by hand or by make_joined_film.sh (see README.md).

if(NOT ZIPPERS_FILM)
  return()
endif()

find_program(ZIPPERS_SWIFTC swiftc)
if(NOT ZIPPERS_SWIFTC)
  message(FATAL_ERROR
    "ZIPPERS_FILM is ON, but swiftc was not found.\n"
    "Install the Xcode command line tools (xcode-select --install), or configure with -DZIPPERS_FILM=OFF.")
endif()
set(ZIPPERS_SWIFT_SDK "" CACHE PATH "SDK for swiftc -sdk (empty: swiftc's default)")

set(_film_sdk)
if(ZIPPERS_SWIFT_SDK)
  set(_film_sdk -sdk "${ZIPPERS_SWIFT_SDK}")
endif()

set(_film_swift_dir "${CMAKE_CURRENT_BINARY_DIR}/swift")
file(MAKE_DIRECTORY "${_film_swift_dir}")
foreach(_tool encode_movie add_audio probe_movie grab_frames extract_audio)
  add_custom_command(
    OUTPUT "${_film_swift_dir}/${_tool}"
    COMMAND "${ZIPPERS_SWIFTC}" -O ${_film_sdk} "${CMAKE_CURRENT_SOURCE_DIR}/swift/${_tool}.swift"
            -o "${_film_swift_dir}/${_tool}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/swift/${_tool}.swift"
    WORKING_DIRECTORY "${_film_swift_dir}"
    COMMENT "Building the Swift film tool ${_tool}"
    VERBATIM)
  add_custom_target(film_${_tool} ALL DEPENDS "${_film_swift_dir}/${_tool}")
endforeach()
