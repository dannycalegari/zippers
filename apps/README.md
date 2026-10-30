# apps

The programs built on the shared library `src/`.

| folder | programs |
|---|---|
| `s2/` | `zipper` (without its window), the film engine `zmovie`, and its checks `ztest` |
| `s3/` | `chz` (the kernel search), `zipper3` (without its window), `link3` (linking numbers) |
| `s4/` | `chz5` (the kernel search), `zipper5`, and `film5`, the film's ending on S⁴ (a window) |
| `cli/` | `zippers`, the command line of the suite: it runs the programs above by name (below) |
| `tools/` | the measuring programs, which change nothing in `src/` and are **not** part of the reference runs: `strays` (how far the true curves stray from the drawn chords; with `--window --cull`, inside the window of a zoomed view), `islands5` (the islands of the finite S⁴ zippers), `overdraw` (the ink drawn twice, with and without the renderer's marking), `roofs` (the measures of a system of roofs), and `cull` (how far a branch's curve strays from the points of its cull test, in units of its size; a picture with culling against the same picture without it, in the window; the focus centres; the density of the focus ball; the cusp points of the tips). The options of each are in the comment at the top of its source |

Each program of `s2/`, `s3/` and `s4/` has the command line of its legacy original (`legacy/`), and is listed in
`tests/reference/programs_apps.json`, which holds it to the reference runs of the original: what it prints and writes
is byte for byte the legacy program's, except for the differences below. The windows (`zipper -v`, `zipper3 -v`) and
the S² film pipeline's scripts are in `legacy/`. `docs/developer_guide.md` has the measurements and the traps behind
the options below.

## `zippers`, the command line

`zippers COMMAND [EXAMPLE] [options]` runs the programs above by the names of the examples: `fig8` or any word in R
and L (S², with `-n N`), `cartwright-steger` or `cs` (S³), `imm-m5` or `imm` (S⁴).

| command | runs |
|---|---|
| `list [EXAMPLE]` | (itself) the examples, and where each default file is found |
| `draw EXAMPLE [options]` | `zipper -m EXAMPLE` (S²), `zipper3` (S³) or `zipper5` (S⁴) |
| `view EXAMPLE` | nothing: the window is not in this version |
| `search EXAMPLE [C R [TPHI]]` | `chz kernel C R [TPHI]` (S³), `chz5 kernel C R RHO [TPHI]` (S⁴; RHO is `--rho`, default 3); without numbers the search that made the example's default file |
| `link cartwright-steger` | `link3`, with `-a 0.01 --nrel 20 --hr 2.6` unless given (the scan that made the stored pairs) |
| `film SHOT.txt` | `zmovie`, with the arguments unchanged |
| `check [EXAMPLE ...]` | the exact checks of the examples, 21 runs of the programs (about 25 s), each in a temporary folder of its own and without `ZIPPERS_DATA`, so that they read the suite's own data; interrupted (Ctrl-C, `SIGTERM`, `SIGHUP`, or `SIGPIPE` when what reads its output stops, as `\| head`), it stops the running check, removes the folder, and ends with the signal; a signal ignored when it starts (`SIGHUP` under `nohup`) stays ignored |

It checks every option against its table (the option's name, for the command and the sphere of the example, and the
number of values after it, none of which may look like an option: `-` followed by a letter or by a second `-`; a
negative number such as `--yaw -30`, and `-` itself as in `--only -`, are values), refuses anything else with exit
status 1 and a message, and passes the options on
unchanged, in the same order, but for a few translations: the S² example becomes `-m WORD`; `--format FMT` becomes
`zipper`'s `-f FMT` (in `zippers`, `-f` is always a substitution file); `search`'s numbers and `--rho`; `--phi 1,0`,
the only φ of `cartwright-steger` in this version, is checked and dropped. Then it replaces itself by the program
(`execv`), so the exit status and the output are the program's. `--dry-run` prints the program's command line instead,
and `zippers COMMAND --help` the options. It runs the programs of its own build when it runs from its build folder, and
those of `../libexec/zippers/` when it is installed, never any program found in `PATH`. The top of `cli/zippers.cc`
says more.

## How the programs find their data files

`zipper3`, `zipper5`, `link3`, `film5` and the tools find their substitution file (the default, or that of `-f`), and
`zipper3 --pairs FILE` its file of linked pairs, by the search of `src/core/data_files.h`:
1. a name that contains `/` is read as given, and nothing else is tried;
2. the working folder;
3. the folders of the environment variable `ZIPPERS_DATA` (separated by `:`): in each, `s3/NAME` (or `s4/NAME`),
   then `NAME`;
4. the data of the program: for a program in its build folder, `data/` of the source tree it was built from; for an
   installed program, `share/zippers/` of its prefix (found from the program's own place, so an installed tree can be
   moved).

The first place that holds an entry of the name is the one used: if that entry is not a readable file (a folder, a file
that cannot be read), the program stops with exit status 1, and says what is there and where; it never passes over it
to the next place. A file found in the working folder keeps its name as given, so what the program prints is what it
printed when it read the working folder only, and no reference run changes; a file found elsewhere is named by the path
at which it was found (in the line `FILE: |S| = ...`): a full path, or, through a relative folder of `ZIPPERS_DATA`, a
relative one. When the file is found nowhere, the program says where it looked, and exits with status 1.

## Installing

`cmake --install build --prefix P` (or `make install PREFIX=P`) puts `zippers` in `P/bin/`, the seven programs it runs
(`zipper`, `zmovie`, `chz`, `zipper3`, `link3`, `chz5`, `zipper5`) in `P/libexec/zippers/`, and the files of `data/` in
`P/share/zippers/`. `ztest`, `film5` and the tools are not installed.

## Differences from the legacy programs

- **The S⁴ word reader.** `zipper5` reads facet 1 in the words of the substitution files, which the legacy word reader
  dropped, so its pictures differ from the legacy `zipper5`'s (44 base arcs, not 47).
- **The tips, in S⁴.** `zipper5` draws each leaf of a letter on a parabolic cycle of the roofs through the cusp points
  of its cycles (`src/render/tips.h`), so that the spikes into the cusps are drawn to their tips. The same leaves are
  drawn, with more points.
- **The look-ahead through one-letter roofs, in S³.** `zipper3` and `link3` take the renderer's look-ahead through
  one-letter roofs. None of their reference runs changes, but some pictures at other settings do:
  `zipper3 -R 2.554 -a 0.04` draws 72,167 leaves of Z⁺, where the legacy program draws 72,169.
- **The checks of the root balls** (`--stats`). `zipper5 --stats` makes its search of the roots again and prints the
  result of exact checks on it: the search is closed under the 16 facet reflections inside the searched ball, and every
  element has a strictly nearer facet neighbour but 1 (`src/examples/imm_m5.h`). `zipper3 --stats` and `link3 --stats`
  print the result of the checks of `src/examples/cartwright_steger_roots.h`, which accept search radii up to 6 (every
  radius in use), with a second line when R is within 10⁻⁶ of a shell. Without `--stats` nothing printed changes.
  `link3 --pair` refuses indices out of range, which the legacy program read past the end.
- **The data files** are found by the search above; the legacy programs read the working folder only.
- **The messages.** `zipper5` has a usage text (`-h`, and after an unknown option), where the legacy program had none.
  `link3` says "cannot write FILE" for a picture it cannot write (the legacy program said "wrote FILE" all the same:
  the scan writes its pictures into the folder `pictures/`, which must exist). `zipper -v` says that this build has no
  window, and where the original program's window is built. `ztest` refuses a word other than `same`, `depth`, `frame`
  and `all`, with exit status 1 (the legacy program ran nothing and exited with 0).
- **`film5`**, the film's S⁴ ending, draws with the corrected reader and the tips, and takes its credit line from
  `--credit TEXT` (none without it). Compiled with the legacy reader and no tips it writes the legacy `film5`'s frames
  byte for byte (checked on four frames). Its pens, letters 19 and 42, both contain facet 1, so they changed most: the
  legacy reader read both as `4 9 12`.
- **Marking** (`--overlap L`). `zipper3` and `zipper5` take `--overlap L` (0..16; default 0: off), the renderer's
  marking, as the S² program does: when a leaf is accepted, the keys of its sub-branches down to L levels are marked as
  drawn, so that they are skipped when met again. **With L = 0 nothing printed or written changes** (`zipper3`'s usage
  text has one more line); with L ≥ 1 lines of marking counts follow the build lines. With no enclosures in S³ and S⁴
  it is a measured trade, not a proved one: at L = 1 it saves 1 to 5% of the leaves and costs 1.2 to 3.5 times the
  time and up to 4.5 times the memory. The renderer's marking has a second mode, `reverse` (the other orientation of
  an involution branch is marked too), reached only through `--reverse` of the tools `strays` and `overdraw`, **for
  measurement only**: in S⁴ the other orientation can be another arc, and the picture then loses real curve.
- **The deep zoom.** `zipper3` and `zipper5` take `--centre P` (the view centred at the point P), `--cull` and
  `--cullk K` (draw only the branches whose curves may meet the window, the screen square and the clip ball pulled
  back to the sphere, with a margin of K times each branch's size; K = 16 by default), `--zoomacc` (with culling: draw
  at ACC/zoom on the sphere, so that the accuracy on the screen is that of zoom 1) and `--focus M` (a second root ball
  h·B(R), about an orbit point h·o near the point M log(zoom) from o towards the view; only with `--zoom` above 1).
  **Off by default, except for a zoomed still (below), and with them off the output is byte for byte the legacy
  program's** (the reference runs, and 12 more runs compared in the same way); `zipper3`'s usage text has six more
  lines (`zipper5`'s options are in the comment at the top of `zipper5.cc`). `--zoomacc` needs culling (which a zoomed
  still has by default), and `zipper3 --cull` is refused with `--pairs`. `--stats` with the focus ball checks B(R),
  the first roots, and says that the others are h·B(R); `zipper3 --check` with culling says that its minimum is over
  the culled picture.
  - **Culling is a measured bet, not a proof** (there are no enclosures). Among the branches that K = 16 can cull
    (size below 1/16), the largest stray is 3.3 sizes on `subst_4.00_9.50_t3_r1.txt` at R up to 4.5 (a safety factor
    of 4.8), and 1.4 sizes on `subst5_2.30_4.50_t1_r0.txt` at R up to 2.1 (11.2); R above 4.5 in S³ and 2.1 in S⁴ is
    unmeasured. It is **not enough for the S³ file `_t1`**, whose branches stray up to 8.9 sizes: use `--cullk 32`
    with it. Where it holds, the picture is made of the same segments, in a fraction of the leaves. The picture files
    are the same byte for byte in most cases; in S⁴ with the focus ball a few dozen pixels can differ, since the
    painter draws overlapping segments of equal depth in another order when culling changes the list
    (`docs/developer_guide.md`, "Culling").
  - **The focus ball** is h·B(R), with h an element of the lattice. It costs time in S³ (8.9 s against
    1.0 s at zoom 100). At a cusp that is a vertex of P5 (e₅ and the others) it adds no roots (h = 1); at another cusp
    point h ≠ 1, but the ball is far from the view.

## The defaults

- `zipper5` reads `subst5_2.30_4.50_t1_r0.txt` by default (`data/s4/`: `chz5 kernel 2.3 4.5 3 --roofs`, the same t as
  `_t1`, so the same view, with the roofs chosen at no extra length: the two orientations of every involution arc are
  one arc), and `zipper3` reads `subst_4.00_9.50_t3_r1.txt` (`data/s3/`: the roofs chosen at one letter of excess).
- **`zipper3 --pairs` and `link3` read `subst_4.00_9.50_t3.txt`** (the linked pairs are loops of its base arcs: with
  other roofs they are other curves), and **`film5` reads `subst5_2.30_4.50_t1.txt`** (the film's ending).
- The tools `overdraw`, `roofs`, `strays`, `cull` and `islands5` have the defaults of `zipper3` and `zipper5`.
- The default files are all in `data/` (`subst_4.00_9.50_t3.txt` and `subst5_2.30_4.50_t1.txt`, the files of the
  legacy programs, are copied there from `legacy/`, byte for byte). The programs find them by the search above, which
  begins with the working folder as the legacy programs did; when a file is found nowhere, the program says where it
  looked.
- At the defaults, every S⁴ picture and every S³ picture differs from the legacy program's (other base arcs). The
  reference runs of the defaults have ids of their own, and the runs of the legacy files give the file with `-f`
  (`tests/README.md`). `-f` with the legacy files draws the legacy pictures (but for the differences above).
- **A zoomed still has culling and the focus ball by default:** with `-o` and `--zoom` above 1, and no viewer and no
  `--pairs`, `zipper3` and `zipper5` behave as if `--cull --focus 1` were given (K = 16), and say so in a note on
  stderr; `--nocull` and `--nofocus` turn them off (and are refused with `--cull`, `--cullk`, `--zoomacc` and with
  `--focus`). At zoom 1, without `-o`, with `-v` or with `--pairs` they stay off. `--zoomacc` is accepted then, since
  culling is on.

The reference runs of `zipper5` are recorded from it (`tests/README.md`).
