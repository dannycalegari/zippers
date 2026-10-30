# SPDX-License-Identifier: GPL-3.0-or-later
#
# apps/: the programs on the shared library src/, with the command lines and the outputs of their legacy
# originals. Each is listed in tests/reference/programs_apps.json, which holds it to the reference runs of its legacy
# original (run_references.py --programs; CTest's reference_apps_fast and reference_apps).
#
#   s2/   zipper (its batch build), zmovie, ztest
#   s3/   chz (its kernel search in src/search/), zipper3 and link3
#   s4/   chz5 (likewise), zipper5, film5
#   cli/  zippers, the command line of the suite, which runs the programs above
#
# Installed (cmake --install): zippers in bin/, and the seven programs it runs (zipper, zmovie, chz, zipper3, link3,
# chz5, zipper5) in libexec/zippers/. Not installed: ztest, film5 and the tools of tools/.

add_subdirectory(s2)
add_subdirectory(s3)
add_subdirectory(s4)
add_subdirectory(tools)
add_subdirectory(cli)
