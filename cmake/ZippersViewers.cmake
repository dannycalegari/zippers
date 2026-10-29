# SPDX-License-Identifier: GPL-3.0-or-later
#
# The GLUT viewers (option ZIPPERS_VIEWERS, default ON).
#
# With the option ON, this module checks that GLUT and OpenGL are there and defines the target
#
#     zippers::gl
#
# for the viewer programs to link against.
#   macOS:  the GLUT and OpenGL frameworks of the system, linked exactly as the old makefiles linked them
#           (-framework GLUT -framework OpenGL), with no other compile or link flags.
#   Linux:  freeglut, OpenGL and GLU, found with CMake's own FindGLUT and FindOpenGL.
# If they cannot be found, configuring stops with a message that says what to install.
#
# With the option OFF nothing is searched for and zippers::gl does not exist, so a viewer target must be created
# only inside  if(ZIPPERS_VIEWERS) ... endif().  For example:
#
#     if(ZIPPERS_VIEWERS)
#       add_executable(s3_zipper3 zipper3.cc)
#       target_compile_definitions(s3_zipper3 PRIVATE ZIPPER3_VIEWER)
#       target_link_libraries(s3_zipper3 PRIVATE ZLIB::ZLIB zippers::gl)
#     endif()

include_guard(GLOBAL)

if(ZIPPERS_VIEWERS)
  add_library(zippers_gl INTERFACE)
  add_library(zippers::gl ALIAS zippers_gl)

  if(APPLE)
    # (find_package(GLUT) would also add -isystem/-iframework flags to every compile line and link Cocoa as well.)
    find_library(ZIPPERS_GLUT_FRAMEWORK GLUT)
    find_library(ZIPPERS_OPENGL_FRAMEWORK OpenGL)
    mark_as_advanced(ZIPPERS_GLUT_FRAMEWORK ZIPPERS_OPENGL_FRAMEWORK)
    if(NOT ZIPPERS_GLUT_FRAMEWORK OR NOT ZIPPERS_OPENGL_FRAMEWORK)
      message(FATAL_ERROR
        "ZIPPERS_VIEWERS is ON, but the GLUT or OpenGL framework was not found.\n"
        "Install the Xcode command line tools (xcode-select --install), or configure with -DZIPPERS_VIEWERS=OFF "
        "to build only the programs that need no window.")
    endif()
    message(STATUS "Found GLUT: ${ZIPPERS_GLUT_FRAMEWORK}")
    message(STATUS "Found OpenGL: ${ZIPPERS_OPENGL_FRAMEWORK}")
    target_link_libraries(zippers_gl INTERFACE "-framework GLUT" "-framework OpenGL")
  else()
    find_package(OpenGL)
    find_package(GLUT)
    if(NOT OpenGL_FOUND OR NOT GLUT_FOUND)
      message(FATAL_ERROR
        "ZIPPERS_VIEWERS is ON, but GLUT or OpenGL was not found.\n"
        "On Ubuntu or Debian: sudo apt-get install freeglut3-dev libglu1-mesa-dev\n"
        "To build only the programs that need no window, configure with -DZIPPERS_VIEWERS=OFF.")
    endif()
    target_link_libraries(zippers_gl INTERFACE GLUT::GLUT OpenGL::GL)
    if(TARGET OpenGL::GLU)
      target_link_libraries(zippers_gl INTERFACE OpenGL::GLU)
    endif()
  endif()
endif()
