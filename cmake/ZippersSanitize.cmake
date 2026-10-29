# SPDX-License-Identifier: GPL-3.0-or-later
#
# The ZIPPERS_SANITIZE option: build with the compiler's own checks for memory errors (AddressSanitizer) and for
# undefined behaviour (UndefinedBehaviorSanitizer). `make sanitize` configures a build with this option on, in
# build-sanitize/.
#
# Unlike the warning and optimisation flags, these go on every target, the imported programs included: a sanitizer
# build exists to check the whole suite. (-g and -fno-omit-frame-pointer make the reports readable.)
#
# Running a sanitized program: AddressSanitizer stops at its first error. UndefinedBehaviorSanitizer by default only
# prints what it finds and carries on, so the exit status stays 0. To make its first finding fatal, run the program
# with UBSAN_OPTIONS=halt_on_error=1.

include_guard(GLOBAL)

if(ZIPPERS_SANITIZE)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "ZIPPERS_SANITIZE needs GCC or Clang; the C++ compiler here is ${CMAKE_CXX_COMPILER_ID}.")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()
