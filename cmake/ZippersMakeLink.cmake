# SPDX-License-Identifier: GPL-3.0-or-later
#
# Make a symbolic link, at build time:
#
#     cmake -DLINK=<path of the link> -DTARGET=<what it points to> -P ZippersMakeLink.cmake
#
# TARGET is stored as given (it may be relative to the folder of LINK). An existing link is replaced if it points
# elsewhere; a real file or folder at LINK is left alone, with a warning. legacy/s2/CMakeLists.txt uses this for
# legacy/s2/movie/bin, the folder in which the S2 film pipeline looks for its binaries.

if(NOT DEFINED LINK OR NOT DEFINED TARGET)
  message(FATAL_ERROR "usage: cmake -DLINK=<link> -DTARGET=<target> -P ZippersMakeLink.cmake")
endif()

if(IS_SYMLINK "${LINK}")
  file(READ_SYMLINK "${LINK}" _zippers_old)
  if(_zippers_old STREQUAL TARGET)
    return()
  endif()
  file(REMOVE "${LINK}")
elseif(EXISTS "${LINK}")
  message(WARNING "${LINK} exists and is not a link, so it is left as it is (it should be a link to ${TARGET}).")
  return()
endif()

file(CREATE_LINK "${TARGET}" "${LINK}" SYMBOLIC)
message(STATUS "${LINK} -> ${TARGET}")
