# SPDX-License-Identifier: GPL-3.0-or-later
#
# Unit tests of src/ (label fast). Most compare code moved into src/ with its legacy original, bit for bit, so they
# include the legacy header (from legacy/s3 or legacy/s4) and are built with the flags of the legacy programs (-O2),
# under which the results must agree. The legacy headers define many functions a test does not use,
# hence -Wno-unused-function.

set(CMAKE_CXX_FLAGS_RELEASE "")

# zippers_unit_test(NAME SOURCE [LEGACY_DIR [DEFINITIONS...]])
function(zippers_unit_test name source)
  add_executable(${name} ${source})
  target_compile_options(${name} PRIVATE -O2 -Wall -Wextra -Wno-unused-function)
  target_link_libraries(${name} PRIVATE zippers::core ZLIB::ZLIB)
  if(ARGC GREATER 2)
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/${ARGV2}")
  endif()
  if(ARGC GREATER 3)
    list(SUBLIST ARGV 3 -1 _defs)
    target_compile_definitions(${name} PRIVATE ${_defs})
  endif()
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS fast TIMEOUT 120)
endfunction()

# The PNG encoder and the rasteriser (src/output/png.h, raster.h) against the three legacy writers. One test program
# per legacy source, since the legacy sources cannot be included together; output_test_support.h is what they share.
zippers_unit_test(test_output_s3 test_output_s3.cc legacy/s3)
zippers_unit_test(test_output_s4 test_output_s4.cc legacy/s4)
# The S2 test includes the film engine zmovie.cc (with ZMOVIE_NO_MAIN, as ztest.cc does), which includes a snapshot of
# zipper.cc: legacy/s2/CMakeLists.txt makes it in the build folder, and the test finds it there.
zippers_unit_test(test_output_s2 test_output_s2.cc legacy/s2/movie/src)
target_include_directories(test_output_s2 PRIVATE "${PROJECT_BINARY_DIR}/legacy/s2/movie/snapshot")
# The moved functions are the legacy ones, token for token (the tests above show that they behave like them). It needs
# Python 3, as the reference tests do (tests/CMakeLists.txt finds it and warns if it is missing).
if(Python3_Interpreter_FOUND)
  add_test(NAME check_output_moved_text
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/check_moved_text.py" "${PROJECT_SOURCE_DIR}")
else()
  # registered anyway, failing and saying why, as the reference tests are (tests/CMakeLists.txt)
  add_test(NAME check_output_moved_text
           COMMAND sh -c "echo 'check_output_moved_text: needs Python 3, and CMake found none' >&2; exit 1")
endif()
set_tests_properties(check_output_moved_text PROPERTIES LABELS fast TIMEOUT 60)

# The generic renderer (src/render/) with each example's render policy, against the legacy renderers of
# legacy/s3/zipcore.h and legacy/s4/zipcore5.h; the S4 view and painter (src/view/s4.h) against legacy/s4/zipper5.cc,
# which test_render_imm includes. render_test_support.h is what the two share. ZIPPERS_REPO lets them read the
# substitution files of legacy/.
zippers_unit_test(test_render_cs test_render_cs.cc legacy/s3 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
zippers_unit_test(test_render_imm test_render_imm.cc legacy/s4 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

# The S4 word reader (examples/imm_m5.h, parse_fword, corrected to read facet 1) against the data: for each
# substitution file of legacy/s4, chz5's S_c and t, recomputed with its search policy (examples/imm_m5_search.h). It
# uses src/ only, no legacy code; ZIPPERS_REPO lets it read the files. (test_render_imm gives both renderers the
# legacy reading, so that it compares renderers.)
zippers_unit_test(test_reader_imm test_reader_imm.cc)
target_compile_definitions(test_reader_imm PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

# The tips of the S4 zippers (src/render/tips.h), checked exactly, in arc order, and drawn through without a
# change of leaves; and none in S3. It uses src/ only; ZIPPERS_REPO lets it read the substitution files.
zippers_unit_test(test_tips_imm test_tips_imm.cc)
target_compile_definitions(test_tips_imm PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

# The descent checks of the S4 root balls (examples/imm_m5.h, the line of zipper5 --stats): the polytope and its
# reflections exactly, the checks on W n B(2.6), certify_roots5 against find_roots5, checks that fail when they should,
# enumerate6 against an independent enumeration (lattice points walked down to P5), the counts.
# It uses src/ only.
zippers_unit_test(test_roots_imm test_roots_imm.cc)

# The look-ahead through one-letter roofs in S3 (src/render/renderer.h): nothing changes at R = 0 for the file
# of the programs, the rule written again, and what it changes elsewhere. It uses src/ only; ZIPPERS_REPO lets it read
# the substitution files.
zippers_unit_test(test_lookthrough_cs test_lookthrough_cs.cc)
target_compile_definitions(test_lookthrough_cs PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

# The S3 view and painter (src/view/s3.h), the separation (src/analysis/separation.h) and the linked pairs
# (src/analysis/pairs.h) against legacy/s3/zipcore.h and zipper3.cc (its batch build), which test_view_s3 includes; the
# relations (src/analysis/relations.h) and the linking numbers (src/analysis/linking.h) against legacy/s3/link3.cc,
# which test_analysis_s3 includes. ZIPPERS_REPO lets them read the substitution files and linked_pairs.txt.
zippers_unit_test(test_view_s3 test_view_s3.cc legacy/s3 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
zippers_unit_test(test_analysis_s3 test_analysis_s3.cc legacy/s3 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

zippers_unit_test(test_backend_contract test_backend_contract.cc)
zippers_unit_test(test_sphere_s3 test_sphere.cc legacy/s3 ZIPPERS_TEST_N=4)
zippers_unit_test(test_sphere_s4 test_sphere.cc legacy/s4 ZIPPERS_TEST_N=5)

# The backends PU21 and ON1<5>, the store and the two examples, against legacy/s3 and legacy/s4. ZIPPERS_REPO
# lets them read the substitution files of legacy/.
zippers_unit_test(test_pu21 test_pu21.cc legacy/s3 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
zippers_unit_test(test_on1 test_on1.cc legacy/s4 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

# The generic kernel search (src/search/kernel_search.h) with each example's policy, against the kernel modes of
# legacy/s3/chz.cc and legacy/s4/chz5.cc (copied into the tests), and the descent against the legacy express.
zippers_unit_test(test_search_cs test_search_cs.cc legacy/s3)
zippers_unit_test(test_search_imm test_search_imm.cc legacy/s4)

# The memory functions (src/core/memory.h) against legacy/s2/zipper.cc, which test_memory includes (with its main
# renamed). They read the system, so the test compares what the two see, and their texts token for token; ZIPPERS_REPO
# lets it read the texts.
zippers_unit_test(test_memory test_memory.cc legacy/s2 "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
# The S2 programs of apps/s2/ are the legacy files with exactly the changes listed in check_s2_moved_text.py, token for
# token (Python 3).
if(Python3_Interpreter_FOUND)
  add_test(NAME check_s2_moved_text
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/check_s2_moved_text.py" "${PROJECT_SOURCE_DIR}")
else()
  add_test(NAME check_s2_moved_text
           COMMAND sh -c "echo 'check_s2_moved_text: needs Python 3, and CMake found none' >&2; exit 1")
endif()
set_tests_properties(check_s2_moved_text PROPERTIES LABELS fast TIMEOUT 60)

# The recorder of the references (tests/run_references.py --record) writes no commit hash. It is run on a temporary copy
# of itself and of runs.json (two light runs), made into a git repository, never on the repository's references
# (Python 3; it needs git, and the build's legacy/s3/chz and legacy/s4/chz5).
if(Python3_Interpreter_FOUND)
  add_test(NAME test_record
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/test_record.py" "${PROJECT_SOURCE_DIR}"
                   "${PROJECT_BINARY_DIR}")
else()
  add_test(NAME test_record
           COMMAND sh -c "echo 'test_record: needs Python 3, and CMake found none' >&2; exit 1")
endif()
set_tests_properties(test_record PROPERTIES LABELS fast TIMEOUT 120)

# The descent checks of the S3 root balls (examples/cartwright_steger_roots.h, the lines of zipper3 --stats): the
# interval arithmetic, the ball columns and the cell radius the covering stands on; exact enumerations; the descent
# checks, and that they fail when they should; certify_roots against roots_info; store checks that fail when they
# should; the counts of B(6); and a count mod 3, an independent exact test of B(6). It uses src/ only. It also runs
# zipper3 --stats and link3 --stats themselves (on the substitution file of the references), so it depends on the two
# programs.
zippers_unit_test(test_roots_cs test_roots_cs.cc)
if(TARGET apps_s3_zipper3 AND TARGET apps_s3_link3)
  add_dependencies(test_roots_cs apps_s3_zipper3 apps_s3_link3)
  target_compile_definitions(test_roots_cs PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\""
                             "ZIPPERS_ZIPPER3=\"$<TARGET_FILE:apps_s3_zipper3>\"" "ZIPPERS_LINK3=\"$<TARGET_FILE:apps_s3_link3>\"")
endif()

# The renderer's marking (src/render/renderer.h), written again from its definition and compared bit for bit,
# and the subset property; the parts of the overdraw measure (apps/tools/overdraw.h) on cases with known answers. It
# uses src/ and apps/tools/ only; ZIPPERS_REPO lets it read the substitution files.
zippers_unit_test(test_overlap test_overlap.cc)
target_include_directories(test_overlap PRIVATE "${PROJECT_SOURCE_DIR}/apps/tools")
target_compile_definitions(test_overlap PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")

# The --overlap option of zipper3 and zipper5 takes effect (no other test runs the programs with the option). The
# programs themselves are run, with and without the option, and what they print is compared with what the library
# computes in the test with the same marking; so it depends on the two programs (as test_roots_cs does). It uses src/
# only; ZIPPERS_REPO lets it read the substitution files.
zippers_unit_test(test_overlap_programs test_overlap_programs.cc)
target_compile_definitions(test_overlap_programs PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
if(TARGET apps_s3_zipper3 AND TARGET apps_s4_zipper5)
  add_dependencies(test_overlap_programs apps_s3_zipper3 apps_s4_zipper5)
  target_compile_definitions(test_overlap_programs PRIVATE "ZIPPERS_ZIPPER3=\"$<TARGET_FILE:apps_s3_zipper3>\""
                             "ZIPPERS_ZIPPER5=\"$<TARGET_FILE:apps_s4_zipper5>\"")
endif()
# The deep zoom in the library: a World and a Renderer start without culling and without a focus ball (the programs turn
# them on for a zoomed still: test_cull_programs); the window pulled back (exactly the points the painters show); nothing
# lost in the window with culling (the tips, the look-ahead through one-letter roofs, marking, forced leaves); culling
# effective at zoom 100; the points of the cull test; the focus centres; the focus ball (B(R) first, de-duplicated by key);
# the same with a turned view (S4's hidden-direction rotation U is not the identity); culling through build itself (both
# zippers get the window); the value of K (the default is 16, and the margin of the cull test at its boundary); the S3 focus
# walk against the nearest orbit point, and its products of two generators; a chain of three one-letter roofs. It uses src/
# only; ZIPPERS_REPO lets it read the substitution files.
zippers_unit_test(test_cull test_cull.cc)
target_compile_definitions(test_cull PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
# The deep zoom options of zipper3 and zipper5 (--centre, --cull, --cullk, --nocull, --zoomacc, --focus, --nofocus) take
# effect, and culling and the focus ball are on by default for a still of a zoomed view (-o with --zoom above 1) and off
# elsewhere: the programs themselves are run, and what they print is compared with what the library computes for the same
# view (as test_overlap_programs does for --overlap), so it depends on the two programs. It checks the default K as a number,
# both zippers' lines culled, a turned view (and the picture with --cull is the picture without it, byte for byte), --stats
# and zipper3 --check with the defaults and with --cull --focus 1.
zippers_unit_test(test_cull_programs test_cull_programs.cc)
target_compile_definitions(test_cull_programs PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
if(TARGET apps_s3_zipper3 AND TARGET apps_s4_zipper5)
  add_dependencies(test_cull_programs apps_s3_zipper3 apps_s4_zipper5)
  target_compile_definitions(test_cull_programs PRIVATE "ZIPPERS_ZIPPER3=\"$<TARGET_FILE:apps_s3_zipper3>\""
                             "ZIPPERS_ZIPPER5=\"$<TARGET_FILE:apps_s4_zipper5>\"")
endif()
# The margin mode of the tool cull prints a column for the default margin factor CULL_K, and its window mode and the window mode
# of the tool strays use CULL_K when --cullk is not given, with nothing lost in the window on the default files (the tools are run
# on light cases).
if(TARGET apps_tools_cull)
  add_dependencies(test_cull_programs apps_tools_cull)
  target_compile_definitions(test_cull_programs PRIVATE "ZIPPERS_CULL=\"$<TARGET_FILE:apps_tools_cull>\"")
endif()
if(TARGET apps_tools_strays)
  add_dependencies(test_cull_programs apps_tools_strays)
  target_compile_definitions(test_cull_programs PRIVATE "ZIPPERS_STRAYS=\"$<TARGET_FILE:apps_tools_strays>\"")
endif()
# The options of the presentation in the kernel search (search/kernel_search.h, search/roof_choice.h): the words
# and THE FACT on a toy alphabet, the junctions of the stored files, the candidates for t, and chz and chz5 themselves
# with --tlist, --tcand, --roofs and --noinv on light cases (so it depends on the two programs, as test_roots_cs does).
# It uses src/ only; ZIPPERS_REPO lets it read the substitution files.
zippers_unit_test(test_roof_choice test_roof_choice.cc)
target_compile_definitions(test_roof_choice PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
if(TARGET apps_s3_chz AND TARGET apps_s4_chz5)
  add_dependencies(test_roof_choice apps_s3_chz apps_s4_chz5)
  target_compile_definitions(test_roof_choice PRIVATE "ZIPPERS_CHZ=\"$<TARGET_FILE:apps_s3_chz>\""
                             "ZIPPERS_CHZ5=\"$<TARGET_FILE:apps_s4_chz5>\"")
endif()
# Which data files each program, tool and zippers reads, and how they find them (the search of src/core/data_files.h):
# the defaults (zipper3 the _t3_r1 file, with --pairs and link3 the _t3 file and linked_pairs.txt; zipper5 the _t1_r0 file;
# the tools follow the programs; film5 _t1, all in data/), that -f wins, the working folder first, ZIPPERS_DATA, the data of
# the source tree for the programs of the build, and the message of copies outside the build that find nothing. The
# programs, the tools and zippers themselves are run, in folders that hold only the files chosen, so it depends on them all
# (film5 only when it is built, with --list, which opens no window). It compares names, never a stored file; ZIPPERS_REPO
# lets it read the files that it copies.
zippers_unit_test(test_default_files test_default_files.cc)
target_compile_definitions(test_default_files PRIVATE "ZIPPERS_REPO=\"${PROJECT_SOURCE_DIR}\"")
if(TARGET apps_s3_zipper3 AND TARGET apps_s3_link3 AND TARGET apps_s4_zipper5 AND TARGET apps_tools_roofs AND
   TARGET apps_tools_cull AND TARGET apps_tools_strays AND TARGET apps_tools_overdraw AND TARGET apps_tools_islands5 AND
   TARGET apps_cli_zippers)
  add_dependencies(test_default_files apps_s3_zipper3 apps_s3_link3 apps_s4_zipper5 apps_tools_roofs apps_tools_cull
                   apps_tools_strays apps_tools_overdraw apps_tools_islands5 apps_cli_zippers)
  target_compile_definitions(test_default_files PRIVATE
                             "ZIPPERS_ZIPPER3=\"$<TARGET_FILE:apps_s3_zipper3>\"" "ZIPPERS_LINK3=\"$<TARGET_FILE:apps_s3_link3>\""
                             "ZIPPERS_ZIPPER5=\"$<TARGET_FILE:apps_s4_zipper5>\"" "ZIPPERS_ROOFS=\"$<TARGET_FILE:apps_tools_roofs>\""
                             "ZIPPERS_CULL=\"$<TARGET_FILE:apps_tools_cull>\"" "ZIPPERS_STRAYS=\"$<TARGET_FILE:apps_tools_strays>\""
                             "ZIPPERS_OVERDRAW=\"$<TARGET_FILE:apps_tools_overdraw>\""
                             "ZIPPERS_ISLANDS5=\"$<TARGET_FILE:apps_tools_islands5>\""
                             "ZIPPERS_CLI=\"$<TARGET_FILE:apps_cli_zippers>\"")
  if(TARGET apps_s4_film5)
    add_dependencies(test_default_files apps_s4_film5)
    target_compile_definitions(test_default_files PRIVATE "ZIPPERS_FILM5=\"$<TARGET_FILE:apps_s4_film5>\"")
  endif()
endif()

# zippers, the command line (apps/cli/zippers), and its install (Python 3):
#   test_zippers_cli      with --dry-run, the program command line that zippers would run for every option of its table,
#                         its translations and defaults, its refusals and its usage texts; an S2 picture without -o on a
#                         terminal; and four messages of the programs (zipper5 -h, zipper -v, ztest with an unknown word,
#                         link3 when a picture cannot be written);
#   test_zippers_tables   the option table of zippers against the programs, both ways: each option it passes on is
#                         accepted by its program, and each option of the programs' parsers is in the table;
#   test_zippers_check    zippers check itself, with stand-ins for its programs (shell scripts beside a copy of zippers):
#                         a failure in each check's output reported as FAIL, exit status 1; the checks run without
#                         ZIPPERS_DATA; an interrupted check (SIGINT, SIGTERM, SIGHUP) ends by the signal, its running
#                         check stopped and its temporary folder removed; a signal ignored when zippers starts stays
#                         ignored; and when what reads its output stops, it ends by SIGPIPE, its folder removed;
#   test_install          cmake --install into build/install_test/ (nowhere else), the list of installed files, the
#                         installed zippers run from an empty folder (list, three pictures byte for byte as the build's
#                         programs draw them, check), the installed tree moved, and an install with DESTDIR.
if(Python3_Interpreter_FOUND)
  add_test(NAME test_zippers_cli
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/test_zippers_cli.py" "${PROJECT_BINARY_DIR}")
  add_test(NAME test_zippers_tables
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/test_zippers_tables.py" "${PROJECT_SOURCE_DIR}"
                   "${PROJECT_BINARY_DIR}")
  add_test(NAME test_zippers_check
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/test_zippers_check.py" "${PROJECT_SOURCE_DIR}"
                   "${PROJECT_BINARY_DIR}")
  add_test(NAME test_install
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/test_install.py" "${CMAKE_COMMAND}"
                   "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}" "${CMAKE_INSTALL_BINDIR}" "${CMAKE_INSTALL_LIBEXECDIR}"
                   "${CMAKE_INSTALL_DATADIR}" "${CMAKE_INSTALL_DOCDIR}")
else()
  foreach(_t test_zippers_cli test_zippers_tables test_zippers_check test_install)
    add_test(NAME ${_t} COMMAND sh -c "echo '${_t}: needs Python 3, and CMake found none' >&2; exit 1")
  endforeach()
endif()
set_tests_properties(test_zippers_cli test_zippers_tables test_zippers_check PROPERTIES LABELS fast TIMEOUT 300)
set_tests_properties(test_install PROPERTIES LABELS fast TIMEOUT 600)
