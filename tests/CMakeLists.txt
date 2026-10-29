# SPDX-License-Identifier: GPL-3.0-or-later
#
# The tests are registered with CTest. Every test carries a label that says when it runs:
#
#   fast        a few minutes at most. `make test` and the automatic runs on GitHub run exactly these
#               (ctest -L fast).
#   reference   slow; re-runs the fixed runs of tests/reference/ and compares the outputs.
#               Run by hand.
#
# Every test also gets a time limit (TIMEOUT, in seconds).

# smoke: the tree has the layout of the skeleton, COPYING is the GPL 3, CITATION.cff names the licence and the paper,
# and git tracks no recordings (tests/smoke.sh).
add_test(NAME smoke
         COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/smoke.sh" "${PROJECT_SOURCE_DIR}")
set_tests_properties(smoke PROPERTIES LABELS fast TIMEOUT 60)

# The reference runs: run_references.py re-runs the runs listed in reference/runs.json, each in a fresh folder
# under build/reference_work/, and compares what they produce with what was recorded. See README.md here for
# which comparisons are made on which machine: on the reference machine (macOS, Apple Silicon, the compiler of the
# recording) everything is compared, elsewhere (Linux, GitHub's macOS) only the results that do not depend on
# floating-point details.
#   reference_fast (label fast):   every run but the 8-minute link3 scan: the selftests, the certificates, the
#                                  linking numbers and every picture; about 47 s.
#   reference (label reference):   all the runs, among them the 8-minute link3 scan; about 9 minutes, one run at a time.
# The new programs of apps/ are held to the same references through the map reference/programs_apps.json
# (run_references.py --programs). Their tests exist once the map names a program, so that they never pass with
# nothing to compare:
#   reference_apps_fast (label fast):   the fast runs whose programs the map names;
#   reference_apps (label reference):   all the runs whose programs the map names.
# Every program is mapped, so both are run with --complete: a run that the map leaves out is a failure.
set(_zippers_apps_map "${CMAKE_CURRENT_SOURCE_DIR}/reference/programs_apps.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_zippers_apps_map}")
file(READ "${_zippers_apps_map}" _zippers_apps_json)
string(JSON _zippers_apps_n ERROR_VARIABLE _zippers_apps_err LENGTH "${_zippers_apps_json}" map)
if(_zippers_apps_err)
  message(FATAL_ERROR "zippers: ${_zippers_apps_map}: ${_zippers_apps_err}")
endif()
set(_zippers_ref_tests reference_fast reference)
if(_zippers_apps_n GREATER 0)
  list(APPEND _zippers_ref_tests reference_apps_fast reference_apps)
endif()

find_package(Python3 3.6 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  set(_zippers_ref "${CMAKE_CURRENT_SOURCE_DIR}/run_references.py")
  add_test(NAME reference_fast
           COMMAND "${Python3_EXECUTABLE}" "${_zippers_ref}" --build "${PROJECT_BINARY_DIR}" --subset fast
                   --work "${PROJECT_BINARY_DIR}/reference_work/fast")
  set_tests_properties(reference_fast PROPERTIES LABELS fast TIMEOUT 600)
  add_test(NAME reference
           COMMAND "${Python3_EXECUTABLE}" "${_zippers_ref}" --build "${PROJECT_BINARY_DIR}" --subset all
                   --work "${PROJECT_BINARY_DIR}/reference_work/all")
  set_tests_properties(reference PROPERTIES LABELS reference TIMEOUT 3600 RUN_SERIAL TRUE)
  if(_zippers_apps_n GREATER 0)
    add_test(NAME reference_apps_fast
             COMMAND "${Python3_EXECUTABLE}" "${_zippers_ref}" --build "${PROJECT_BINARY_DIR}" --subset fast
                     --programs "${_zippers_apps_map}" --complete --work "${PROJECT_BINARY_DIR}/reference_work/apps_fast")
    set_tests_properties(reference_apps_fast PROPERTIES LABELS fast TIMEOUT 600)
    add_test(NAME reference_apps
             COMMAND "${Python3_EXECUTABLE}" "${_zippers_ref}" --build "${PROJECT_BINARY_DIR}" --subset all
                     --programs "${_zippers_apps_map}" --complete --work "${PROJECT_BINARY_DIR}/reference_work/apps_all")
    set_tests_properties(reference_apps PROPERTIES LABELS reference TIMEOUT 3600 RUN_SERIAL TRUE)
  endif()
else()
  # Without Python 3 the reference tests cannot run. They are registered anyway, as tests that fail and say why, so
  # that `make test` cannot pass without them.
  list(JOIN _zippers_ref_tests ", " _zippers_ref_list)
  message(WARNING "zippers: no Python 3 (3.6 or later) found, so the reference tests (tests/run_references.py) "
                  "cannot run; ${_zippers_ref_list} will fail")
  foreach(_t ${_zippers_ref_tests})
    add_test(NAME ${_t}
             COMMAND sh -c "echo '${_t}: needs Python 3 (3.6 or later) to run tests/run_references.py, and CMake found none' >&2; exit 1")
    if(_t MATCHES "_fast$")
      set_tests_properties(${_t} PROPERTIES LABELS fast TIMEOUT 60)
    else()
      set_tests_properties(${_t} PROPERTIES LABELS reference TIMEOUT 60)
    endif()
  endforeach()
endif()

# Unit tests of the shared library src/, label fast.
add_subdirectory(unit)
