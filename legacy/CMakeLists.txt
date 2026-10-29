# SPDX-License-Identifier: GPL-3.0-or-later
#
# legacy/: the three programs as they were before the suite, copied from their old folders (README.md lists the few
# lines changed since) and built with exactly the flags of their old makefiles (kept next to the sources as
# makefile.original and movie/src/Makefile.original). The binaries keep their old names, in one build folder per
# program:
#
#   s2/   the 3-manifold program: zipper, and the film engine zmovie, ztest     build/legacy/s2/
#   s3/   the Cartwright-Steger surface: chz, zipper3, link3                    build/legacy/s3/
#   s4/   the 5-manifold of Italiano-Martelli-Migliorini: chz5, zipper5, film5  build/legacy/s4/
#
# Each folder's CMakeLists.txt lists its targets and their flags.

add_subdirectory(s2)
add_subdirectory(s3)
add_subdirectory(s4)
