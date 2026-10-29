# SPDX-License-Identifier: GPL-3.0-or-later
#
# apps/: the programs on the shared library src/, with the command lines and the outputs of their legacy
# originals. Each is listed in tests/reference/programs_apps.json, which holds it to the reference runs of its legacy
# original (run_references.py --programs; CTest's reference_apps_fast and reference_apps).
#
#   s2/   zipper (its batch build), zmovie, ztest
#   s3/   chz (its kernel search in src/search/), zipper3 and link3
#   s4/   chz5 (likewise), zipper5

add_subdirectory(s2)
add_subdirectory(s3)
add_subdirectory(s4)
add_subdirectory(tools)
