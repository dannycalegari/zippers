# tests

The tests are registered with CTest in `tests/CMakeLists.txt`. Each one carries a label, which says when it runs:

| test | label | what it does | time on an Apple M2 |
|---|---|---|---|
| `smoke` | `fast` | checks the layout of the repository, and that the copies in `data/` of three files of `legacy/` are those files, byte for byte | under 1 s |
| `reference_fast` | `fast` | re-runs the 45 fast reference runs (all but the `link3` scan) and compares them with `reference/` | about 47 s |
| `reference` | `reference` | re-runs all 46 reference runs and compares them with `reference/` | about 9 min |
| `test_*`, `check_*` (in `unit/`) | `fast` | unit tests of the shared library `src/`: most compare moved code with its legacy original, bit for bit (`check_*`: token for token); `test_reader_imm` holds the S⁴ word reader to `chz5`'s data; `test_tips_imm` checks the tips of the S⁴ zippers exactly and that drawing through them changes no leaf, and `test_lookthrough_cs` the look-ahead through one-letter roofs in S³; `test_roots_imm` and `test_roots_cs` the descent checks of the S⁴ and S³ root balls (`test_roots_cs` also runs `zipper3` and `link3`); `test_record` that the recorder (`run_references.py --record`, run on a temporary copy) writes no commit hash; the others, the options of the programs (`tests/unit/CMakeLists.txt` says what each one does) | a few minutes together |
| `reference_apps_fast`, `reference_apps` | `fast`, `reference` | the same reference runs, made with the new programs of `apps/`, through `reference/programs_apps.json` | as above |
| `reference_zippers_fast`, `reference_zippers` | `fast`, `reference` | the same reference runs, made through the command line `zippers`, with the command lines of `reference/zippers_runs.json` (42 of the 46 runs; the other 4 are listed there with their reasons) | about 46 s, 9 min |
| `test_zippers_cli` (in `unit/`) | `fast` | `zippers --dry-run`: the program command line that `zippers` would run, for every option of its table, its translations and defaults, its refusals (a value that looks like an option among them) and its usage texts; the quoting of the printed line, read back by `sh -c`; an S² picture without `-o` on a terminal; and four messages of the programs (`zipper5 -h`, `zipper -v`, `ztest` with an unknown word, `link3` when a picture cannot be written) | about 4 s |
| `test_zippers_check` (in `unit/`) | `fast` | `zippers check` itself, with shell scripts in the place of its programs (beside a copy of `zippers`): a failure in each check's output reported as FAIL, exit status 1; the checks run without `ZIPPERS_DATA`; an interrupted `zippers check` (SIGINT, SIGTERM, SIGHUP) stops its running check, removes its temporary folder and ends by the signal, and a signal ignored when it starts (SIGHUP under `nohup`) stays ignored; when what reads its output stops (`\| head`), it ends by SIGPIPE and removes the folder | about 10 s |
| `test_zippers_tables` (in `unit/`) | `fast` | the option table of `zippers` against the programs, both ways: every option it passes on is accepted by its program, and every option of the programs' own parsers is in the table | about 6 s |
| `test_default_files` (in `unit/`) | `fast` | which data files the programs, the tools and `zippers` read, and how they find them: the working folder first, then `ZIPPERS_DATA`, then the data of the source tree; an entry of the name that is not a readable file (a folder, a file that cannot be read, a link to nothing) stops the search; copies of the programs outside the build say where they looked | about 10 s |
| `test_install` (in `unit/`) | `fast` | `cmake --install` into `build/install_test/` (nowhere else): the installed files, the installed `zippers` run from an empty folder (`list`, three pictures byte for byte as the build's programs draw them, `check`), the installed tree moved, an install with `DESTDIR` | about 27 s |

- `make test` (and the automatic runs on GitHub) run the tests labelled `fast`: `ctest --test-dir build -L fast`.
- The test labelled `reference` is slow and is run by hand: `ctest --test-dir build -L reference`.
- CTest shows a test's output only when it fails. To see the table of a passing test, add `-V`.

## The reference runs (`reference/`)

These are the outputs of a fixed set of runs of the legacy programs (`legacy/`), recorded on 2026-10-25 on the
reference machine (below). The programs of `apps/` must reproduce them.

**Three runs were added later**, on 2026-10-26, for paths that no other run exercised, each recorded from the same
legacy program (byte for byte the binary of the first recording): `s4_chz5_kernel_steep` (`chz5 kernel 2.3 4.2 3 2`:
a steep t and the descent), and `s3_zipper3_pair`, `s3_zipper3_pair_context` (`zipper3 --pairs`, with and without
`--context`: the only runs that draw a linked pair). `runs.json` says so in their `note`s and in its machine block.

**The S⁴ pictures are recorded from the new `zipper5`** (2026-10-26). The S⁴ word reader of `legacy/s4/zipcore5.h`
skips every token `1`, which `chz5` writes for the empty word but which is also facet 1, so the legacy `zipper5` (and
`film5`) draws from misread letters: 19 of the 80 generators of every stored file, and t of the `_t2` and `_t3` files,
are read as their images under the homomorphism that kills r₁, and it draws 47 base arcs where the data have 44. The
reader of `src/` reads facet 1, and `legacy/` keeps the old one as the record of what was drawn. So:
- `s4_zipper5_base` and `s4_zipper5_r18` were recorded again, from `apps/s4/zipper5` (their `program` now): the
  same commands, other pictures (`|S| = 80 (44 base arcs)`). Their `note`s give the SHA-256 of the legacy pictures,
  which the legacy `zipper5` still draws;
- `s4_zipper5_t2` (`zipper5 -f subst5_2.30_4.50_t2.txt`) was added and recorded from it: it pins the reading of t,
  whose word in that file contains facet 1 (the other two runs read the `_t1` file, whose t has none);
- each of the three has a portable result, the count of letters and of base arcs (`80 | 44`).

So no reference run holds the legacy `zipper5` any more; the unit test `test_render_imm` still compares the renderer
with the legacy one, giving both the legacy reading (`unit/test_render_imm.cc`).

**The S⁴ pictures were recorded again with the tips** (2026-10-26). `zipper5` draws each leaf whose letter lies on a
parabolic cycle of the roofs through the cusp points of the cycles, exact points of the true curve, so that the spikes
into the cusps reach their tips (`src/render/tips.h`). The leaves are the same; the pictures, and the text (two lines
for the tips, and ", N points at tips" in each line of the build), are not. `s4_zipper5_base`, `s4_zipper5_r18` and
`s4_zipper5_t2` were recorded again from `apps/s4/zipper5`; their `note`s give the SHA-256 of the pictures before.
`test_render_imm` gives the renderer no tips (its own policy), so it still compares it with the legacy one.

**The S³ programs look ahead through one-letter roofs** (the look-ahead (b) of `src/render/renderer.h`): `zipper3` and
`link3` also split a branch whose letter has a one-letter roof when its only child's first split strays from the
chord. No S³ reference run changed: each, made with the new programs, is byte for byte what was recorded (at ACC 0.01
on `subst_4.00_9.50_t3.txt` the look-ahead through a roof never splits a branch). `test_render_cs`, `test_view_s3` and
`test_analysis_s3` use a policy without it, so they still compare with the legacy code.

**The defaults.** `zipper5` reads `subst5_2.30_4.50_t1_r0.txt` (`data/s4/`) and `zipper3` reads
`subst_4.00_9.50_t3_r1.txt` (`data/s3/`) when `-f` is not given; `zipper3 --pairs` and `link3` read
`subst_4.00_9.50_t3.txt`, and `film5` reads `subst5_2.30_4.50_t1.txt`. A still of a zoomed view (`-o` with `--zoom`
above 1) has culling (K = 16) and the focus ball by default, as if `--cull --focus 1` had been given. So:
- the five runs that use a legacy file (`s3_zipper3_r0`, `s3_zipper3_r1`, `s3_zipper3_check`, `s4_zipper5_base`,
  `s4_zipper5_r18`) name it, with `-f subst_4.00_9.50_t3.txt` or `-f subst5_2.30_4.50_t1.txt` as their first arguments;
  their outputs are byte for byte the recorded ones, with the legacy programs and with the map (`--programs`). The two
  `--pairs` runs give no `-f`: they test that `--pairs` reads `subst_4.00_9.50_t3.txt` by default;
- seven runs, on the defaults and with no `-f`, with the file in `data/` among their `inputs`, are recorded from the
  new programs (the machine block's `also_recorded` has a line for each recording): `s4_zipper5_base_r0` and
  `s4_zipper5_r18_r0` (the commands of `s4_zipper5_base` and `s4_zipper5_r18`), `s3_zipper3_t3r1_base`,
  `s3_zipper3_t3r1_shell` and `s3_zipper3_t3r1_check` (the commands of `s3_zipper3_r0`, `s3_zipper3_r1` and
  `s3_zipper3_check`), and `s3_zipper3_t3r1_zoom` and `s4_zipper5_zoom_r0`, stills of a view zoomed to 100 with no `--cull`
  or `--focus`, which test the defaults of a zoomed still. Each has a portable result: the count of letters (and of base
  arcs, or pairs), and for the stills the two notes that the program prints to say that culling and the focus ball are on.
  They are in the fast subset (0.06 to 1.6 s, up to 0.29 GB each).

The script `run_references.py` re-runs every run in a fresh folder and compares what it produces with what was
recorded. It prints a table, then the details of every difference, and exits with status 1 if anything differs or a
run fails, and 0 otherwise. It needs only Python 3 and its standard library (tried with 3.9 and 3.14).

```sh
make                                             # build first
python3 tests/run_references.py                  # all 46 runs, in build/reference_work/all/
python3 tests/run_references.py --subset fast    # the 45 fast runs (all but the scan)
python3 tests/run_references.py --no-heavy       # all but the 8-minute link3 scan
python3 tests/run_references.py --only s3_chz_kernel,s4_chz5_kernel
python3 tests/run_references.py --mode portable  # only the comparisons that are made on Linux (see below)
```

`--programs MAP.json` makes the runs with the new programs that the map names instead of the legacy ones, and
compares them with the same references; a run whose program is already one of the new programs (the S⁴ pictures of
`apps/s4/zipper5`) is made as it is; the runs whose programs it does not name are left out, and listed.
`reference/programs_apps.json` is the map of `apps/`. With `--complete`, a run that the map leaves out is a failure;
CTest's `reference_apps_fast` and `reference_apps` use it, since every program is mapped. Recording a subset of the
runs (`--record --only ...`) keeps the machine block of the first recording and adds a dated line to its
`also_recorded` list.

`--zippers RUNS.json` makes the runs through the command line `zippers` (`apps/cli/zippers` of the build folder): the
table `reference/zippers_runs.json` gives, for each run that has one, the `zippers` command line that does what the
recorded program did (for instance `draw fig8 -n 2 --memlimit 2 -o fig8_n2.eps` for `zipper -m fig8 -n 2 --memlimit 2
-o fig8_n2.eps`), and, under `without`, the reason of each run that has none (the three selftests, which `zippers check`
runs with verdicts of its own, and `chz5 ball 3`). The runs are compared with the same references. With `--complete`, a
run that the table names neither way is a failure; CTest's `reference_zippers_fast` and `reference_zippers` use it.
`zippers` replaces itself by the program it runs, so its runs are those of the program: byte for byte in exact mode.

The programs run with the environment of the script, with `LC_ALL=C` and without `ZIPPERS_DATA`. They look for their
data files in the working folder first (`src/core/data_files.h`), where every run has its `inputs`, under their plain
names, so what they print and write is what they printed and wrote when they read the working folder only. A file
missing from a run's `inputs` would be found in `data/` of the source tree instead, and printed with its full path.

`--build DIR` names another build folder, and `--work DIR` another scratch folder. The outputs of the last test stay
in the scratch folder, one folder per run, so a difference can be looked at.

### What is in `reference/`

- **`runs.json`**, the list from which the test re-runs everything. For each run:
  - `program`, a path in the build folder, and `args`, its arguments;
  - `inputs`: the files of the repository copied into the run's folder first (the programs of S³ and S⁴ look for their
    data files in the working folder first); `mkdirs`: empty folders made there first;
  - `timeout`, in seconds;
  - `fast`: whether the run belongs to the fast subset; `heavy`: whether it is a heavy run (minutes, or much memory:
    below);
  - `portable`: the patterns that pick out the results compared on every machine (below), and `portable_files`;
  - `same_as`: an output file that must be byte for byte a file of the repository, such as the substitution file that
    `chz kernel 4 9.5 3` writes; `stdout_same_as`: the same for the standard output;
  - `expect`: what the run produced when it was recorded: the exit status; the SHA-256 of its standard output and
    standard error, with the timings masked; the SHA-256 and size of every file it wrote; the values its portable
    patterns picked out; and, for information, its time, its peak memory and the SHA-256 of the program;
  - `note`, `title`: for people.
  - `machine` (at the top): the system, processor and C++ compiler of the recording (`system`, `processor`,
    `compiler`: the script reads these to choose its mode), the day (`recorded`, printed), what was recorded from what
    (`programs`, `note`), and a line for each later recording of a subset (`also_recorded`, which `--record --only`
    extends).
- **`<run>/stdout.txt`, `<run>/stderr.txt`**: the text the run printed, with the timings masked.
- **`<run>/small_*.jpg`**: a copy of every picture the run made, at most 256 pixels wide, for people to look at
  (JPEG: the 31 copies are 7 to 40 kB, 20 kB on average).
  The pictures themselves are not kept, only their SHA-256 and size (51 kB to 2.4 MB, 0.23 MB in the median), and
  every one of them can be made again with the program that `runs.json` names, which every build builds (`legacy/`,
  and `apps/s4/zipper5` for the S⁴ pictures).
- **`<run>/files/`**: a copy of every text file under 200 kB that a run writes and that is not already in the
  repository. At present there is one, the substitution file of `s4_chz5_kernel_steep`; the other two substitution
  files and `linked_pairs.txt` that the runs write are byte for byte the ones in `legacy/`.

**Timings.** Before text is compared, every number that comes after a space or "(" and is followed by "s" (seconds)
is replaced by `<time>`: `built in <time>s`, `(<time>s)`, `<time> s/frame`. The rest of the line is still compared.
"12 spikes", "+1 sep" and the "1 s" of "(t^-1 s t)" are left alone.

### Which comparisons are made where

The outputs are only expected to be byte for byte the same on the kind of machine where they were recorded: macOS on
Apple Silicon, with the same C++ compiler (`AppleClang 21.0.0.21000101`, from Xcode 26.6). Another compiler, or
another processor, can change the last bits of floating-point results, and then a few pixels or a printed digit. So
the script works in one of two modes:

| comparison | exact mode | portable mode |
|---|---|---|
| the exit status | yes | yes |
| the names of the output files | yes | yes |
| the SHA-256 of every output file | yes | only `linked_pairs.txt` of `s3_link3_export` (exact data: words in the group) |
| the text output, timings masked | yes | no |
| the portable results (below) | yes | yes |

With `--mode auto` (the default) the script reads the system, the processor and the compiler from the build folder
(the files CMake writes into `build/CMakeFiles/`). If all three are those of the recording, it compares in exact mode,
otherwise in portable mode. It prints which. `--mode exact` and `--mode portable` choose the mode by hand.

| where | what runs | mode |
|---|---|---|
| the reference machine (`make test`, `ctest -L fast`) | `smoke`, `reference_fast` (45 runs) | exact |
| the reference machine, by hand (`ctest -L reference`) | `reference` (46 runs) | exact |
| GitHub's Linux runner (`ctest -L fast`) | `smoke`, `reference_fast` | portable |
| GitHub's macOS runner (`ctest -L fast`) | `smoke`, `reference_fast` | portable, since its Xcode has another compiler |
| the reference machine after an update of Xcode | as above | portable; `--mode exact` shows what changed |

**The portable results**, which do not depend on floating-point details:

| run | what is compared on every machine |
|---|---|
| `s3_chz_selftest` | the 19 exact checks in ℤ[ζ₁₂] ("u' unitary for F': yes", "u^3 = zeta^0 I", ...), without the two printed distances |
| `s3_link3_selftest` | the linking numbers of the four test links in four random projections ("-1/-1", ...), without the Gauss integrals and the crossing counts |
| `s4_chz5_selftest` | its one line ("160 orthogonal pairs, cocycle on every square from all 16 states; ok") |
| `s4_chz5_ball3` | "(mu, h) inconsistencies 0" |
| `s3_chz_kernel` | the certificate: \|S_c\| = 36 generators, "f-inconsistencies 0" twice, the roofs up and down (36 / 36), the three "a_i t^k in <S_c>: yes", "CERTIFICATE that S_c generates K: YES", the steep roofs (36 / 36), the name of the file it writes |
| `s4_chz5_kernel` | the certificate: "(mu, h) inconsistencies 0", \|S_c\| = 80 generators, the roofs up and down (80 / 80), the 192 / 192 Schreier generators, "CERTIFICATE that S_c generates K: YES", the name of the file it writes |
| `s4_chz5_kernel_steep` | as `s4_chz5_kernel`, and the steep roofs (80 / 80 up and down); not the counts of the descent, which is best first by float distances |
| `s4_zipper5_base`, `s4_zipper5_r18`, `s4_zipper5_t2`, `s4_zipper5_base_r0`, `s4_zipper5_r18_r0` | the count of letters and of base arcs that `zipper5` reads from its substitution file ("\|S\| = 80 (44 base arcs)"): exact, and 47 with the legacy reader |
| `s3_zipper3_t3r1_base`, `s3_zipper3_t3r1_shell`, `s3_zipper3_t3r1_check` | the count of letters and of pairs that `zipper3` reads from its substitution file ("\|S\| = 36 (18 pairs)") |
| `s3_zipper3_t3r1_zoom`, `s4_zipper5_zoom_r0` | the same count, and the two notes that the program prints when a zoomed still has culling and the focus ball by default ("note: culling is on by default for a zoomed still (K = 16; --nocull turns it off)", "note: the focus ball is on by default ...") |
| `s3_zipper3_pair`, `s3_zipper3_pair_context` | the number of linked pairs read, and the description of the pair drawn (its linking number, its words, φ(h)) |
| `s3_link3_export` | the 13 pairs it writes, and `linked_pairs.txt` itself (by SHA-256): this checks that the relation and translate numbers used below name the same elements of the group |
| `s3_link3_pair_*` (13 runs) | the linking number in each of the 20 projections, and that the "A over B" and "B over A" counts agree |
| `s3_link3_scan` (in the `reference` test only) | the 13 nonzero pairs with their linking numbers; for each, "STABLE", its linking numbers after the translations, and "ALL AGREE"; and the totals (7600 pairs tested, 13 nonzero, 0 over/under disagreements, 0 pairs where two projections disagree) |

The other runs (the other pictures, `zipper3 --check`, the film engine) have no portable results.
In portable mode they are run, and their exit status and the names of their files are compared, but nothing else
(the table says `n/c`, not compared). They are in the fast subset too (they take at most 2 s and 0.35 GB each), so
`make test` on the reference machine compares every picture of every program, byte for byte.

Some of these results could in principle still depend on floating-point details, through a comparison with a cut-off
(a distance just at a radius). The margins are wide: the generators of S³ have d = 3.33 and 3.98 against the cut-off
4, those of S⁴ have d up to 2.2924 against 2.3, and the linked loops are at least 0.0197 apart, against the accuracy
0.01. The numbering of the translates h (`--pair A B H`) comes from sorting by distance in single precision, with
ties broken by the order in which the enumeration found them; `s3_link3_export` would show if it changed. (The 18
translates of `--hr 2.6` all lie at the same distance 2.5534, so if another standard library orders them otherwise,
`s3_link3_export` and the 13 pair runs fail together: a false failure, never a false pass.)

This was tried on the reference machine with a second build made with `-ffp-contract=off` (no fused multiply-add),
which changes the last bits of many floating-point results. In exact mode 7 of the 34 light runs then differ: four S²
pictures in EPS, `cusp.png`, the 4 film frames, two numbers of `--stats` ("1.98e-14" becomes "4.2e-14"), and the
Gauss integral of the split circles in `link3 selftest` ("+0.0000" becomes "-0.0000"). In portable mode all of them
agree.

### Heavy runs

One run is heavy: `s3_link3_scan`, the scan that made `legacy/s3/link_run2.txt`. It takes about 8 minutes and 0.5 GB.
It is in the `reference` test, not in `reference_fast`; `--no-heavy` leaves it out. Every other run takes under 8 s
and 0.35 GB. The S² runs carry the hidden option `--memlimit 2` (a drawing limit of 2 GB); none of them comes near it.

### Recording the references again

```sh
python3 tests/run_references.py --record                         # every run twice; they must agree
python3 tests/run_references.py --record --once --only s3_link3_scan
```

This rewrites `runs.json` and the folders of `reference/`. It needs `sips` (macOS) and Ghostscript for the small
copies of the pictures. Record only on the reference machine, from a clean build of the programs that the references
are to describe, built from the shell (on a Mac with Apple silicon, a build started from an x86_64 Python compiles
x86_64 programs, whose floating point differs: `docs/developer_guide.md`). Do not record again to make a test pass:
find out first why the output differs from the reference. A change that is meant to change outputs is recorded again
openly: say which runs and why, in their `note`s and here.

A full recording rewrites the machine block with the day and the state of the checkout; a recording of a few runs
(`--only`) keeps the block and adds a dated line to `also_recorded`. Say in the block's `note` what was recorded from
what.

### Notes on the runs

- `zipper` and `zipper3` are run from their builds without the viewer (`build/legacy/s2/nogui/zipper`,
  `build/legacy/s3/nogui/zipper3`), which exist in every configuration. Their outputs are byte for byte those of the
  viewer builds.
- The film engine's run `zmovie rot_n2.txt --frames 0:3` also has `-o rot_n2`, since the shot file names no output
  folder. `--frames 0:3` renders frames 0 to 3, so it writes 4 frames.
- `s3_link3_selftest` is the selftest of the linking-number code, and `s3_link3_export` and `s3_link3_scan` are how
  `linked_pairs.txt` and `link_run2.txt` were made (`legacy/s3/README.md`).
