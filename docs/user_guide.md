# User guide

How to use the programs of the suite: the command `zippers`, every option of each of its commands, the files the
programs read and write, and what the programs check and what they only measure. The short introduction is the
`README.md`; the measurements and the traps of the code are in `docs/developer_guide.md`; the options of the programs
themselves are in `apps/README.md`; the tests are in `tests/README.md`.

A statement of this guide is one of three kinds, and says which when it matters: **checked** by a program (it prints a
verdict, or refuses), **tested** by a named test of `tests/`, or **measured** (a number from a run, with its source;
nothing proves it).

## 1. Getting started

**Building.** The `README.md` has the requirements (CMake 3.20, a C++17 compiler, zlib) and the commands: `make`
builds into `build/`, `make test` runs the fast tests, `make install PREFIX=P` installs. `make JOBS=2` limits the
build to two compiles at a time (four at once take about 1.7 GB). The programs run from the build folder as they are:
`build/apps/cli/zippers`. The examples below say `zippers`; give the path of the build, or put `P/bin` on `PATH`.

**The first commands.**

```sh
zippers list
```

lists the examples. `zippers list EXAMPLE` says, for one example, which program each command runs and where each data
file is found.

```sh
zippers draw fig8 -R 3 -o fig8.png
```

```text
monodromy fig8, order 2: tr a = 1.780776+0.624811i, tr c = 2.135779+0.000000i
wrote fig8.png
Z+: 14390 leaves, 101 polylines ; Z-: 13944 leaves, 110 polylines
```

`zippers check` runs the exact checks of the examples (21 of them, about 25 seconds); section 7 says what they certify.

**The form of a command.** `zippers [--dry-run] COMMAND [EXAMPLE] [options]`. `zippers --help` lists the commands,
`zippers --version` prints the version, and `zippers COMMAND --help` lists the options of a command (for `draw` and
`search`, `zippers draw EXAMPLE --help` lists those of that example only). The options keep the names, the meanings
and the defaults that they have in the programs. `zippers` checks every option against its table (the option's name
for the command and the sphere of the example, and the number of values that follow it) and refuses anything else
with exit status 1 and a message:

```sh
zippers draw fig8 --pole 1 2
```

```text
zippers: draw: --pole is not an option for fig8 (S2); it is one for cartwright-steger (S3)
```

A value may begin with `-` only as a number (`--yaw -30`) or be `-` itself; `--yaw --pitch` is refused. Otherwise
`zippers` replaces itself by the program, so the exit status and the output are the program's. **`--dry-run`**, before
the command, prints the command line of the program and runs nothing. Use it to see what an option becomes, or to
look at a heavy run before making it:

```sh
zippers --dry-run draw RRL -n 3 --format png -o a.png
```

```text
BUILD/apps/s2/zipper -m RRL -n 3 -f png -o a.png
```

(`BUILD` stands for the folder of the programs, which `zippers` prints in full. It runs the programs of its own build,
or those of `../libexec/zippers/` beside it when installed; never a program found in `PATH`.)

**Memory and time.** The commands shown with their output in this guide are cheap: each took under 10 seconds on an
Apple M2 (the times in the outputs vary). Those marked heavy are not. A picture in S³ or S⁴ at the defaults takes a
fraction of a second; a larger `-R` and a smaller `-a` make it larger, and `--cap` (section 7) bounds it.

## 2. The examples

Three examples, one for each sphere at infinity. The mathematics is in the paper cited in the `README.md`; this
section says what each example takes and what it reads.

| example | sphere | what it is | data files | commands |
|---|---|---|---|---|
| `fig8`, or a word such as `RRL` | S² | a hyperbolic 3-orbifold that fibers over the circle; the word in `R` and `L` is the monodromy | none: the arcs are computed from the monodromy | `draw`, `film`, `check` |
| `cartwright-steger` (`cs`) | S³ | the Cartwright–Steger surface, with φ = 1,0 | `data/s3/` | `draw`, `search`, `link`, `check` |
| `imm-m5` (`imm`) | S⁴ | the fibered hyperbolic 5-manifold of Italiano–Martelli–Migliorini, with φ = h/2 | `data/s4/` | `draw`, `search`, `check` |

### S²: `fig8`, words in R and L

`fig8` is the figure-eight monodromy `RL`, with its cusped character built in. Any word in `R` and `L` containing both
letters is an example (`RRL`, `RLLRL`; lower case too); a proper power such as `RLRL` draws its root `RL`, the same
zippers. **`-n N`** is the order of the cone point, an integer from 2 to 1000000 (default 2), or `inf` (or `0`) for the
cusp.

```sh
zippers draw RRL -n 3 -R 3 -a 0.02 -o rrl.png
```

```text
monodromy RRL: phi(a)=ab phi(b)=babab ; cusped char heuristically selected (verify the picture)
monodromy RRL, order 3: tr a = 1.718622+0.399951i, tr c = 2.104329+0.067386i
wrote rrl.png
Z+: 4830 leaves, 83 polylines ; Z-: 2636 leaves, 40 polylines
```

The cusped character of a word other than `fig8` is chosen by a heuristic, and the program says so, as above: look at
the picture. The ball of the group (`-R`, default 6) may hold at most 16 million elements, and a larger `-R` is
refused with the largest one for that monodromy and order:

```sh
zippers draw fig8 -R 9 -o r9.png
```

```text
monodromy fig8, order 2: tr a = 1.780776+0.624811i, tr c = 2.135779+0.000000i
-R 9: the root ball would have about 8.6e+07 elements, more than the limit of 16000000; the largest -R for this monodromy and order is about 8.16
```

`zippers draw fig8 --polylines F` also records that largest value (`maxR`, section 5). The wheel (`-C`) is the green
circle that separates Z⁺ from Z⁻ (the CaTherine wheel).

### S³: `cartwright-steger`

`cs` is the Cartwright–Steger surface. Its data are in `data/s3/` (the table of files is in `data/README.md`). The
default file of `draw` is `subst_4.00_9.50_t3_r1.txt`; `draw --pairs` and `link` read `subst_4.00_9.50_t3.txt`, since the
linked pairs are loops of its base arcs and other roofs give other curves. `--phi 1,0` is checked and dropped: it is the
only φ there is.

```sh
zippers draw cs -a 0.05 -R 2.554 --polylines cs.poly -o cs.png
```

```text
BUILD/data/s3/subst_4.00_9.50_t3_r1.txt: |S| = 36 (18 pairs), roofs checked exactly; t^+ and t^- found
Z+: R=2.55 acc=0.05: 19 roots, 70872 branches, 19 skipped, 62451 leaves, 873 polylines, 225 spikes, 0 forced, depth 6, 3 split through one-letter roofs
Z-: R=2.55 acc=0.05: 19 roots, 56483 branches, 8 skipped, 50198 leaves, 463 polylines, 235 spikes, 0 forced, depth 6
built in 0.20s
wrote cs.poly
wrote cs.png
```

(`BUILD/data` is the folder `data/` of the source tree, or `share/zippers/` of an install; a file found in the working
folder is named as given.) `-R 0` draws the base arcs only; the larger `-R`, the more translates of them. The first
shells of translates are at R = 2.553, 3.326, 3.983; `-R 2.554` draws the first (section 7 on shells).

**The linked pairs.** `data/s3/linked_pairs.txt` holds 13 pairs of loops, one in Z⁺ and one in Z⁻, whose linking
number is +1 or −1, each written as the words of its two loops and the element h that translates the second. It was
written by `link3 --export` from a scan, and `zippers check` verifies the numbers again (section 7). Draw pair K with
its context:

```sh
zippers draw cs --pairs linked_pairs.txt --pairindex 2 --context -o pair.png
```

```text
BUILD/data/s3/subst_4.00_9.50_t3.txt: |S| = 36 (18 pairs), roofs checked exactly; t^+ and t^- found
Z+: R=0.00 acc=0.01: 1 roots, 100317 branches, 0 skipped, 88067 leaves, 3513 polylines, 526 spikes, 0 forced, depth 8
Z-: R=0.00 acc=0.01: 1 roots, 89104 branches, 0 skipped, 78351 leaves, 2125 polylines, 741 spikes, 0 forced, depth 7
built in 0.23s
BUILD/data/s3/linked_pairs.txt: 13 linked pairs
built the 13 pairs of loops in 0.94s
pair 2: lk +1   gamma_w, w = 4 17 23 (Z+)   h.delta_w', w' = 1 3 31 12 7 (Z-), phi(h) = 1
wrote pair.png
```

A single pair can also be computed by `link` (section 3.5): `zippers link cs --pair 4 10 1 --nproj 4`.

### S⁴: `imm-m5`

`imm` is the 5-manifold. Its default file is `subst5_2.30_4.50_t1_r0.txt`. The sphere S⁴ is projected to R⁴ and the
picture shows three of the four coordinates; the fourth, **the hidden direction**, is shown as a fog that fades the
curves towards white with the distance of the hidden coordinate from 0 (a factor from 1 down to 0.12, on the
scale `--hidfog`, default 1.5). `--turn D` and `--turn2 D` rotate R⁴ in the plane of the hidden direction and the third, or the first,
visible axis, so as to look at the picture along another direction.

```sh
zippers draw imm -o imm.png
```

```text
Z+: 42 tips on 40 letters (the cusps of the closed paths of its roofs, of length <= 4, each verified exactly)
Z-: 42 tips on 42 letters (the cusps of the closed paths of its roofs, of length <= 4, each verified exactly)
BUILD/data/s4/subst5_2.30_4.50_t1_r0.txt: |S| = 80 (44 base arcs), roofs checked exactly; t^+ and t^- found
Z+: R=0.00 acc=0.02: 1 roots, 70976 branches, 0 skipped, 60273 leaves, 1617 polylines, 0 spikes, 0 forced, depth 12, 66348 points at tips
Z-: R=0.00 acc=0.02: 1 roots, 88823 branches, 0 skipped, 75986 leaves, 905 polylines, 4 spikes, 0 forced, depth 12, 50839 points at tips
built in 0.06s
wrote imm.png
```

The group Γ of S⁴ has torsion and cusps; the drawing handles the cusps by drawing the leaves of parabolic chains through
their cusp points (the "tips"). `--centre 0,0,0,0,1,1` centres the view at the cusp point given by a null vector of
R^{5,1}; five numbers give a point of S⁴ instead.

## 3. The commands

`zippers COMMAND [EXAMPLE] [options]`. In the tables the **spheres** column says for which examples an option exists:
2 for S², 3 for S³, 4 for S⁴, "all" for the three. "Default" is the program's default; a pair such as "S² 6; S³, S⁴ 0" is
one default for each. Unless said, a number option takes a decimal number.

### 3.1 `list`

`zippers list` prints the examples; `zippers list EXAMPLE` prints, for the example, each command with its program and
the data files it reads and where the search found them, and the places the search looks in (section 6). It runs no
program and takes no options.

### 3.2 `draw`

`zippers draw EXAMPLE [options] [-o FILE]` runs `zipper -m EXAMPLE` (S²), `zipper3` (S³) or `zipper5` (S⁴).
`zippers draw --session FILE [options]` takes the example and the options from a session file (section 5).
Differences from the programs' own command lines: the S² example becomes `-m WORD`; **`--format FMT`** of `zippers` is
`-f FMT` of `zipper` (in `zippers`, `-f` is always a substitution file, S³ and S⁴ only); `--phi 1,0` is dropped.

Without `-o`, an S² drawing writes EPS to the standard output (refused when that is a terminal; `--polylines` alone
writes no picture), and S³ and S⁴ build the zippers and print what they print (`--check`, `--stats`) but write no
picture.

**Options of all three spheres.**

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `--session FILE` | a session file | all | | take the example and the options from it; first, in place of the example |
| `-o FILE` | a file name | all | | write the picture: PNG; S² also EPS |
| `-R R` | radius | all | S² 6; S³, S⁴ 0 | the radius of the ball of the group whose translates are drawn |
| `-a ACC` | number > 0 | all | S² 0.004; S³ 0.01; S⁴ 0.02 | the accuracy: the largest chord on the sphere |
| `-s SIZE` | integer | all | S² 512; S³ 900; S⁴ 840 | the size, in pixels (PNG) or EPS points |
| `--zoom F` | number | all | 1 | magnify about the centre |
| `--yaw D` | degrees | all | 0 | turn the view about the vertical axis |
| `--pitch D` | degrees | all | S² 0; S³ 20; S⁴ 15 | turn it about the horizontal axis |
| `--stats` | none | all | | the checks (S²: the counts and the exactness checks; S³, S⁴: the root balls) |
| `--overlap L` | 0 to 16 | all | S² 1; S³, S⁴ 0 | mark the sub-branches of each leaf down to L levels as drawn |
| `--nocull` | none | all | | with `--zoom`: no culling by default |
| `--nofocus` | none | all | | with `--zoom`: no focus ball by default |
| `--polylines FILE` | a file name | all | | write the curves as data (section 5); S² without `-o`: no picture |
| `--colour L=R,G,B` | name and three numbers in [0, 1] | all | | the colour of L; once for each L; given twice, the last counts |

**Options of S² only** (`zipper`).

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `-n N` | 2 to 1000000, `inf` or 0 | 2 | 2 | the order of the cone point; `inf` is the cusp |
| `--format FMT` | `eps` or `png` | 2 | png if `-o` ends in `.png`, else eps | the format of the picture |
| `-w WIDTH` | points | 2 | 0 | the line width in points; 0 is the thinnest |
| `-C` | none | 2 | | also draw the CaTherine wheel |
| `--wheel` | none | 2 | | the same as `-C` |
| `--wheelres N` | 8 to 10^7 | 2 | 10000000 | the finest edge π/N of the wheel's mesh |
| `--timing` | none | 2 | | print the time of each phase (on the standard error) |

The values that the program checks: `-s` is 16 to 100000 for EPS and 16 to 16384 for PNG (a pixel takes about 7 bytes
of memory), `--zoom` is above 0 and at most 1e7 (points are single precision, so above about 1e5 the curves show steps),
and a proper power of a word draws its root. Three more options of `zipper` are for testing and do not appear in the
usage text of `zippers`, though `zippers` accepts them:

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `--memlimit GB` | 0.2 to 64 | 2 | 4; 6 with `--stats` | the memory at which drawing stops (section 7) |
| `--roll D` | degrees | 2 | 0 | turn the view about the line of sight |
| `--nochains` | none | 2 | | split parabolic chains step by step, not as chains |

**Options of S³ and S⁴** (`zipper3`, `zipper5`).

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `-f FILE` | a substitution file | 3 4 | S³ `subst_4.00_9.50_t3_r1.txt` (with `--pairs`, `subst_4.00_9.50_t3.txt`); S⁴ `subst5_2.30_4.50_t1_r0.txt` | the substitution file, looked for as in section 6 |
| `--only +\|-` | `+` or `-` | 3 4 | both | draw only Z⁺ or only Z⁻ |
| `--clip RHO` | radius | 3 4 | 3 | clip the picture to the ball of radius RHO (section 4) |
| `--thick N` | integer | 3 4 | 0 | thicker lines |
| `--cap N` | leaves | 3 4 | S³ 8e6; S⁴ 2e7 | stop after N leaves per zipper (section 7) |
| `--centre P` | S³: x1,...,x4; S⁴: x1,...,x5 or x1,...,x6 | 3 4 | the default view | centre the view at the point P |
| `--cull` | none | 3 4 | on for a zoomed still | draw only the branches that may meet the window |
| `--cullk K` | number | 3 4 | 16 | the margin factor of culling; implies `--cull` |
| `--zoomacc` | none | 3 4 | | with culling: the accuracy ACC/zoom on the sphere |
| `--focus M` | number | 3 4 | 1 for a zoomed still | with `--zoom`: a second ball of roots, at M log(zoom) towards the centre |
| `--viewmatrix N1,...` | S³: 25 numbers; S⁴: 34 | 3 4 | | the view as its matrices (section 4) |

**Options of S³ only** (`zipper3`).

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `--pole D1 D2` | two angles, degrees | 3 | 0 0 | move the pole of the projection, along E1 then E2 |
| `--check` | none | 3 | | print the smallest distance between the drawn Z⁺ and Z⁻ |
| `--pairs FILE` | a file of linked pairs | 3 | | show the linked pairs of FILE instead of the zippers |
| `--pairindex K` | integer | 3 | 0 | with `--pairs` and `-o`: draw pair K |
| `--context` | none | 3 | | with `--pairs`: the base arcs faintly behind the pair |
| `--phi 1,0` | `1,0` | 3 | | checked and dropped: the only φ |

**Options of S⁴ only** (`zipper5`).

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `--turn D` | degrees | 4 | 0 | turn the hidden direction, in the plane of the third visible axis |
| `--turn2 D` | degrees | 4 | 0 | the same, in the plane of the first visible axis |
| `--hidfog H` | number | 4 | 1.5 | the fog scale of the hidden coordinate |

`--stats` prints the checks of the root balls in S³ and S⁴ (one line, on the standard error) and, in S², the counts and
the exactness checks of section 7. `--check` (S³) prints a distance: for the default picture,

```sh
zippers draw cs --check
```

```text
check: min distance Z+ to Z- (vertex to segment, on S^3) = 0.02089  (0.08s)  at Z+ point (0.3969 0.0129 0.8824 -0.2522)
```

is its last line. It is a measurement on the drawn chords, not a bound for the curves.

Options not available through `zippers draw`: the original programs' windows (`-v`, `--view`, `--snap`, `--movie`,
`--moviescript`, `--moviestart`, `--moviescale`; the window is `zippers view`, section 3.3) and the aliases `-d` (of `-R`), `--nest` (of `--overlap`)
and the obsolete `-e`. `zippers --table` prints every option of every program with what `zippers` does with it.

### 3.3 `view`

`zippers view [EXAMPLE] [options]` (by default `fig8 -n 2 -R 6`), or `zippers view --session FILE [options]`, opens a
window on the curves of `zippers draw EXAMPLE [options]`. Its options are those of `draw`, less `--polylines`,
`--format`, `--stats`, `--check` and `--timing`; `-o STEM` is the stem of the pictures that `p` saves (by default the
example's name). It runs `zview`, the window's program, which runs `zippers draw` itself, in the background, for the
curves it shows and for the pictures it saves: the keys work while a computation runs, and a new request replaces the
one running. The window's title says the example, R, the zoom, and whether something is being computed; the messages
(what each key did, what was saved) are printed on the standard error.

The window shows what `p` saves: the same projection, the same colours and fogs, the front hemisphere in S² with the
silhouette circle, the depth order of the pictures (in S⁴ the painter's order, which mixes depth and the hidden
direction), S³'s linked pair over its context, and the crosses of t⁺ and t⁻. The shorter side of the window shows what
a picture shows across its width.

**The keys.** A key that does nothing in the geometry shown says so.

| key | S² | S³ | S⁴ |
|---|---|---|---|
| ← → ↑ ↓ | rotate the sphere 6° about the screen's axes | move the pole 4° toward the screen's axes | as S³ |
| `w` `s` | — | move the pole 4° toward / away from the viewer | as S³ |
| `+` `=` | R + 0.5 (refused at once beyond the program's limit) | the next shell of roots | as S³ |
| `-` `_` | R − 0.5, at least 1 | the previous shell | as S³ |
| `]` `[` | the accuracy ÷ 1.5, × 1.5 | as S² | as S² |
| `z` `Z`, `x` `X` | zoom × 1.5, ÷ 1.5, within [0.1, 10⁷] | as S² | as S² |
| `c` `C` | the zippers → the zippers and the wheel → the wheel | both zippers → Z⁺ → Z⁻ | as S³ |
| `b` | — | the clip radius 3 → 6 → 12 → 1.5 | as S³ |
| `r` | the view of the start again | as S² | as S² |
| `n` `N`, `l` | — | with `--pairs`: the next, the previous linked pair; the zippers faintly behind it | — |
| `h` `H`, `j` `J` | — | — | turn the hidden direction ±5° in the plane of the horizontal axis and the hidden one (`--turn2`), in that of the depth axis and the hidden one (`--turn`) |
| `p` `P` | save the picture of the view | as S² | as S² |
| `f` | the curves of this view in full detail; `f` again, or any change of the view, returns to the whole picture | as S² | as S² |
| `?` | print the keys | as S² | as S² |
| `q` Esc | quit | as S² | as S² |

The shells of S³ and S⁴ up to the R already computed are shown at once, with no computation (their curves are the
first ones of those of a larger R).

**The mouse.** A drag with the left button rotates, 0.4° a point about the screen's axes: in S² the sphere, in S³ and
S⁴ the space R³ of the picture. The scroll wheel zooms, × 1.5 a notch.

**`p`** writes `NAME.png` and `NAME.zsession`, with NAME the stem, else `stem_1`, `stem_2`, ...: never a file that
exists. The picture is made by `zippers draw --session NAME.zsession -o NAME.png`, which `p` prints, so the command
remakes it exactly; when the view is zoomed it also prints the command of the picture at full detail. The picture's
size is `-s`, not the window's.

**Without a window.** A build without GLFW (README) has no window: `zippers view` checks its options, says so, and
exits with status 1; an installation without it says that it has no `zview`. `zippers draw` makes the pictures.

### 3.4 `search`

`zippers search EXAMPLE [C R [TPHI]] [options]` runs the search that writes a substitution file and checks, with a
certificate, that the substitution generates the kernel: `chz kernel C R [TPHI]` (S³) or `chz5 kernel C R RHO [TPHI]`
(S⁴). C is the step radius, R the radius of the ball searched, TPHI the value of φ(t) (default 1). **Without numbers
it runs the search that made the example's default file**, with its options unless options are given:
`chz kernel 4 9.5 3 --roofs --jexcess 1` for `cs` and `chz5 kernel 2.3 4.5 3 --roofs` for `imm`:

```sh
zippers --dry-run search cs
```

```text
zippers search: without numbers, the search of the default file subst_4.00_9.50_t3_r1.txt: chz kernel 4 9.5 3 --roofs --jexcess 1
BUILD/apps/s3/chz kernel 4 9.5 3 --roofs --jexcess 1
```

The first line is on the standard error. The search writes its file into the working folder, replacing a file of the
same name. A search takes seconds (`zippers check` runs both with their certificates, 5 to 7 s each); the words of
the roofs it chooses can depend on the platform (section 7).

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `--tlist` | none | 3 4 | | list the candidates for t, and stop |
| `--tcand I` | integer | 3 4 | | take candidate I for t (`--tlist` numbers them) |
| `--noinv` | none | 3 4 | | S_c without its involutions |
| `--roofs` | none | 3 4 | | choose the roofs: self-inverse involution roofs, then fewer cancelling junctions |
| `--excess E` | integer | 3 4 | 2 | with `--roofs`: the excess of a self-inverse roof |
| `--jexcess e` | integer | 3 4 | 0 | with `--roofs`: the excess of the junction pass |
| `--rho RHO` | number | 4 | 3 | the ball B(max(RHO, C)) of the search; it is the third number of `chz5 kernel` |
| `--phi 1,0` | `1,0` | 3 | | checked and dropped |

In S⁴, `--noinv` makes the certificate fail at C = 2.3: the facet counts of the other letters span no involution
letter's (checked by `test_roof_choice`). The files of `data/` were made by `search` with the options listed in `data/README.md`.

### 3.5 `link`

`zippers link cartwright-steger [options]` runs `link3`, the linking numbers of loops of relations in Z⁺ and Z⁻. It
adds `-a 0.01 --nrel 20 --hr 2.6`, the settings of the scan that made the stored pairs, unless they are given, and makes
the folder `pictures/` that `link3` writes into. **Heavy without `--pair`, `--hinfo` or `--export`: the scan takes about 8
minutes and 0.5 GB** (the reference scan: `docs/developer_guide.md`). The cheap forms:

```sh
zippers link cs --pair 4 10 1 --nproj 4
```

```text
BUILD/data/s3/subst_4.00_9.50_t3.txt: |S| = 36 (18 pairs), roofs checked exactly; t^+ and t^- found
pair w4 w10 h1 at acc 0.01: 62351 + 28303 points, separation 0.1702
  projection  0: lk +1  (A over B and B over A agree)  crossings 836  pole distance 1.0574
  projection  1: lk +1  (A over B and B over A agree)  crossings 1532  pole distance 1.0594
  projection  2: lk +1  (A over B and B over A agree)  crossings 196  pole distance 1.1502
  projection  3: lk +1  (A over B and B over A agree)  crossings 352  pole distance 1.1519
```

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `-f FILE` | a substitution file | 3 | `subst_4.00_9.50_t3.txt` | the substitution file |
| `-a ACC` | number | 3 | `zippers`: 0.01 | the accuracy of the loops |
| `--rr R` | radius | 3 | 7.5 | find the relations inside the ball B(R) |
| `--maxlen N` | integer | 3 | 6 | the longest relation |
| `--nrel N` | integer | 3 | `zippers`: 20 | use the first N relations |
| `--hr R` | radius | 3 | `zippers`: 2.6 | translate the Z⁻ loops by the elements of B(R) |
| `--stats` | none | 3 | | check the translates (closure, descent; exact) |
| `--seed N` | integer | 3 | 1 | the random seed of the projections |
| `--pair A B H` | three integers | 3 | | the single pair (γ_wA, h_H δ_wB), in `--nproj` projections |
| `--nproj N` | integer | 3 | 20 | the projections of `--pair` |
| `--png STEM` | a name | 3 | | with `--pair`: three pictures `STEM_yaw000.png`, ... |
| `--hinfo` | none | 3 | | list the translates h, their φ and their words |
| `--export FILE` | a scan's output | 3 | | write the verified pairs of a scan's output FILE to `linked_pairs.txt` |
| `--phi 1,0` | `1,0` | 3 | | checked and dropped |

`--export` reads the output of a scan, saved in a file. `--hinfo` lists h0 (the identity)
and the translates of B(`--hr`), with their distance, φ and word. The numbering of the translates h comes from sorting
by distance in single precision, with ties in the order of the enumeration (`tests/README.md`).

### 3.6 `film`

`zippers film SHOT.txt [key=value ...] [options]` runs `zmovie`, the frame engine of the S² film, with its arguments
unchanged: it renders the frames of a shot (a file of `key = value` lines: the monodromy, the order, the view and the
parameters as functions of the frame) into a folder of PNG frames. `key=value` on the command line overrides a key of
the shot. **Rendering writes a folder of PNG frames and can take a long time and much memory: look first with
`--dry-run`.** A shot of the repository, `legacy/s2/movie/tests/shots/rot_n2.txt` (fig8, order 2, R = 6), renders its
frames 0 to 3 in under 2 seconds:

```sh
zippers film legacy/s2/movie/tests/shots/rot_n2.txt --frames 0:3 -o out
```

```text
built: fig8 T=2 R=6 a=0.004: 183371 roots, Z+ 636521 points / 144834 polylines, Z- 687258 / 158092, 1.04 s
job 0: 4 frames: render 0.118 s/frame, png 0.030 s/frame
shot: 4 frames in 1.6 s
```

The four frames are `out/frame_00000.png` to `out/frame_00003.png`; the times vary. The shot format is described in
`legacy/s2/movie/src/README.md`.

| option | values | spheres | default | meaning |
|---|---|---|---|---|
| `-o DIR` | a folder | 2 | the shot's `out` key | the folder of the frames |
| `-j N` | integer | 2 | | render with N processes |
| `--frames A:B` | two integers | 2 | all | render frames A to B inclusive |
| `--quiet` | none | 2 | | print less |
| `--timing` | none | 2 | | print timings (accepted, not in the usage text) |
| `--assemble TIMELINE.txt OUTDIR` | two names | 2 | | instead of a shot: assemble frame folders into a sequence with crossfades, by a timeline; first on the command line (accepted, not in the usage text) |

With `-j N` the summary lines of the processes come in a varying order, and the frames are identical
(`docs/developer_guide.md`). The film's ending in S⁴ is `film5`, a window; `zippers` does not run it. The Swift film tools (macOS) are in `film/`; its `README.md`
describes them.

### 3.7 `check`

`zippers check [EXAMPLE ...]` runs the exact checks of the examples named (all by default). Section 7 lists them. It
takes no options. Each check runs in a temporary folder of its own (under `TMPDIR`), without `ZIPPERS_DATA`, so that it
reads the suite's own data. Interrupted (Ctrl-C), it stops the running check and removes the folder. It exits with status
0 if all pass, and 1 otherwise. For `fig8`:

```sh
zippers check fig8
```

```text
zippers check: 1 exact checks, each in a folder of its own under TMPDIR/zippers_check_XXXXXX
  S2  the exactness checks of a drawing (zipper --stats)                                     pass (0.4 s)
zippers check: all 1 checks pass
```

## 4. Pictures

**Formats.** S² pictures are EPS (the default, and on the standard output without `-o`) or PNG (by `-o X.png` or
`--format png`). S³ and S⁴ pictures are PNG, software rendered, with a depth cue, and need `-o`. The size `-s` is in
EPS points or in pixels. Every picture is of the curves: Z⁺ in blue and Z⁻ in red by default.

**The view.** In S², `--yaw`, `--pitch` and (hidden) `--roll` rotate the sphere (the rotation is Rz(roll) Ry(yaw)
Rx(pitch)). In S³ the sphere is projected stereographically from a pole to R³; the default view puts the fixed point
t⁺ at the centre, `--pole D1 D2` moves the pole by D1 degrees along the first axis of the frame and then D2 along the
second, `--yaw` and `--pitch` rotate R³, and `--centre P` centres the view at the point P. In S⁴ the sphere is projected
to R⁴; `--yaw` and `--pitch` rotate the three visible coordinates, `--turn` and `--turn2` mix the hidden coordinate
in, and `--centre P` takes five numbers (a point, normalised) or six (a null vector x1,...,x6 of R^{5,1}, the point
x1/x6, ..., x5/x6, which may be a cusp).

**The clip radius.** `--clip RHO` (S³, S⁴) clips the picture to the ball of radius RHO in the stereographic image (R³ for S³, R⁴ for S⁴);
the default is 3.

**The exact view.** `--viewmatrix N1,...` gives the view as its matrices, comma-separated: in S³ the frame of R⁴ (16
numbers, row by row) and the rotation of R³ (9), 25 in all; in S⁴ the frame of R⁵ (25) and the rotation of R³ (9), 34
in all, the hidden direction staying that of `--turn` and `--turn2`. It replaces `--yaw`, `--pitch`, `--pole` and
`--centre`; `--zoom`, `--clip` and `--hidfog` are as usual. The view is the one given, bit for bit: the `viewmatrix` of a
polyline file's header (section 5) draws the same picture again. Each matrix must be orthonormal to within 1e-9 (the
largest entry of |M Mᵀ − I|). In S³ both must have determinant +1, since a determinant −1 would draw the mirror image,
whose linking numbers have the other sign. In S⁴ either determinant is accepted (the fog is even in the hidden
coordinate). **Tested** by `test_polylines`.

**The zoom.** `--zoom F` magnifies about the centre of the view. In S² it adds detail down to the zoom's scale (the
program accepts F up to 1e7, but see the limit of single precision above); for the ball `-R` it also takes a second one
about a point pushed towards the view (the focus ball), and draws only what meets the window (culling). In S³ and S⁴ a
zoomed still behaves as if `--cull --focus 1` were given, with K = 16, and says so on the standard error:

```sh
zippers draw cs --zoom 20 -a 0.02 -R 2.554 --centre 1,0,0,0 -o z.png
```

```text
BUILD/data/s3/subst_4.00_9.50_t3_r1.txt: |S| = 36 (18 pairs), roofs checked exactly; t^+ and t^- found
note: culling is on by default for a zoomed still (K = 16; --nocull turns it off)
note: the focus ball is on by default for a zoomed still (--focus 1; --nofocus turns it off)
focus: L = 2.9957 (1 log zoom); h o at d(o, h o) = 4.8231, d(h o, o_F) = 2.2879 (h: 2 steps of the walk)
Z+: R=2.55 acc=0.02: 38 roots, 227775 branches, 38 skipped, 59614 leaves, 7231 polylines, 558 spikes, 0 forced, depth 7, 6 split through one-letter roofs, 141532 culled
Z-: R=2.55 acc=0.02: 38 roots, 170537 branches, 16 skipped, 39069 leaves, 5264 polylines, 486 spikes, 0 forced, depth 7, 3 split through one-letter roofs, 112758 culled
built in 0.44s
focus: 19 roots in B(R), 19 more from h B(R)
wrote z.png
```

- **Culling** drops a branch when all its points lie more than K times its size beyond a face of the window (the screen
  square and the clip ball, pulled back to the sphere). `--cull` asks for it elsewhere, `--cullk K` sets K (K = 16;
  **use `--cullk 32` with the S³ file `subst_4.00_9.00_t1.txt`** (in `legacy/s3/`), whose branches stray further),
  `--nocull` turns the default off. It saves about zoom². **It is a measured bet, not a proof** (section 7).
- **The focus ball** adds the roots h·B(R) about a lattice element h near the point M log(zoom) from the base point
  towards the view, since the roots of B(R) alone do not reach a zoomed window. `--focus M` sets M, `--nofocus` turns the
  default off. At a cusp that is a vertex of the S⁴ fundamental polytope it adds nothing (h = 1), and elsewhere the
  ball can be far from the view (`docs/developer_guide.md`, "Culling").
- **`--zoomacc`** draws at accuracy ACC/zoom on the sphere, so that the accuracy on the screen is that of zoom 1; it
  needs culling (the whole sphere at that accuracy costs about zoom² times as much).
- **Refusals.** `--nocull` is refused with `--cull`, `--cullk` and `--zoomacc`, and `--nofocus` with `--focus`. `--focus`
  needs `--zoom` above 1. `--cull` is refused with `--pairs`. At zoom 1, without `-o`, and with `--pairs`, culling and
  the focus ball are off unless asked for.

**Colours.** `--colour L=R,G,B`, three numbers in [0, 1], once for each layer name L. The names are the layers of the
polyline file and the zippers behind a pair:

| name | what | default |
|---|---|---|
| `Z+`, `Z-` | the zippers | S² `0,0,1` and `1,0,0`; S³ and S⁴ `0.05,0.15,0.85` and `0.85,0.08,0.08` |
| `wheel` | the CaTherine wheel (S² only, `-C`) | `0,0.6,0` |
| `pair.Z+`, `pair.Z-` | the loops of a pair (S³ only, `--pairs`) | the zippers' colours |
| `context.Z+`, `context.Z-` | the zippers behind a pair (S³ only, `--context`) | `0.70,0.75,0.95` and `0.95,0.72,0.72` |

A name that the picture does not draw is accepted and changes nothing; a name of another program is refused. The
defaults given as options draw the same picture, byte for byte (**tested** by `test_polylines` and
`test_polylines_s2`).

## 5. The curves as data, and sessions

### The polyline file

`--polylines FILE` writes the curves that the program drew as a file (format 1, `src/output/polylines.h`): a text
header of `KEY VALUE` lines from `zippers polylines 1` to `data`, then little-endian blocks. Each layer (`Z+`, `Z-`;
`wheel` with `-C`; `pair.Z+`, `pair.Z-` with `--pairs`) holds for each polyline its first point, its root and its letter
(−1 in S², where polylines are joined across roots), and the points as the renderer stores them (floats): a reader has
exactly the points that were drawn. The file ends with the distances of the roots. Some lines of the header of
`zippers draw fig8 -R 3 --polylines f8.poly`:

```text
view 1 0 0 0 1 -0 -0 0 1
zoom 1
maxR 8.160953126990613
maxR.focus 7.81437953671064
maxR.wheel 6.559267307824495
capped Z+ no
capped Z- no
```

The header holds the program and its command line, the example, the parameters (`order`,
`file`, `R`, `acc`, `cap`, `overlap`, `only`), whether culling and the focus ball were on, and the view: in S² `view`,
`yaw`, `pitch`, `roll`, `zoom` and the largest R the program accepts (`maxR`, `maxR.focus`, `maxR.wheel`); in S³ and S⁴
`viewmatrix` (25 and 34 numbers, section 4), `zoom`, `clip`, and in S⁴ `turn`, `turn2`, `U` and `hidfog`; for each layer,
**`capped NAME yes`** if its curves are incomplete because a cap was reached (the cap of leaves, or in S² the memory
limit), else `capped NAME no`. The `file` line is the data file's plain name if it was found in the working folder,
and otherwise its full path.

Rules, **tested** by `test_polylines` and `test_polylines_s2`:
- S² without `-o` writes no picture, only the file; with `-o`, both.
- The file is refused before anything is computed if it cannot be written (a folder of the name, a folder that does
  not exist or may not be written in) and so is an argument holding a line break.
- It is written under a temporary name in its folder and renamed at the end: a write that fails leaves nothing and an
  older file as it was. A symbolic link is written through. A program ended while writing leaves `FILE.tmpPID_K`.
- In S³ and S⁴ the polylines come in the order of their roots, nearest first. Without the focus ball the curves of a
  smaller R are exactly the first polylines of those of a larger R, and `-R` exactly at a shell's radius includes that
  shell (**tested** by `test_shells`, section 7). With the focus ball they are not.

### Session files

A session file (format 1, `src/output/session.h`) holds the `draw` command of a picture with every option written
out. `zippers draw --session FILE -o PIC` remakes the picture. The window's `p` writes one beside each picture it saves
(`zippers view`, above); `tests/sessions/` holds one of each sphere and one of a pair picture, and a session is a text
file that can also be written by hand:

```text
zippers session 1
made_by = zippers 1.0, Darwin arm64, AppleClang 21.0.0.21000101
example = cartwright-steger
phi = 1,0
f = subst_4.00_9.50_t3_r1.txt
sha256.f = 0b89d5579b75a7ef562bf8f891c0d223a9472bc5f617c57799e2ed4ea2f1e64e
R = 2.554
a = 0.05
zoom = 3
...
colour = Z+=0.05,0.15,0.85
```

- Lines are `KEY = VALUE`, with `#` for comments. A draw key is an option of `zippers draw` without its dashes, with the
  value as on a command line (`pole = 10 5`); a flag is `C = yes` or `C = no`; one `colour` line for each colour. The
  outputs (`-o`, `--polylines`), the options that only print (`--check`, `--timing`, and `--stats` in S³ and S⁴) and
  `--format` are not draw keys. Numbers are the shortest decimals that read back to the same doubles.
- Every option of the picture is written, its default included, so that a session remakes its picture even if a default
  changes. For a picture a zoomed still's culling and focus ball are written as the options they stand for. A session
  of a command without `-o` is written as it is, and replayed without `-o` gives that command's curves; replayed with
  `-o` it makes a still with the culling and focus ball of the `zippers` that replays it, which are not written.
- **`sha256.f`** is the SHA-256 of the substitution file that the search for data files found when the session was
  written, and `sha256.pairs` that of the file of `--pairs`. `zippers draw --session` finds the files as usual and refuses
  a file whose hash differs, saying which file it found, where, and both hashes, so a file of the same name in the
  working folder cannot change the picture silently:

  ```text
  zippers: draw --session bad.zsession: the substitution file subst_4.00_9.50_t3_r1.txt, found at BUILD/data/s3/subst_4.00_9.50_t3_r1.txt, has the SHA-256 0b89...; the session's is fb89... (it was written with another file of that name)
  ```

  (The hashes are written out in full by the program.) The exit status is 1.
- **Options after `--session`** replace the session's: a key by its name, a colour by its colour's name. The view by
  `--viewmatrix` and the view by angles replace each other: `--yaw` after a session of a matrix drops the matrix, and the
  angles not given are then the programs' defaults. `--nocull` drops the session's culling; `-f` (and `--pairs`) replace
  the file and its check. A session of a zoomed still keeps its focus ball: with `--zoom 1` the program refuses
  (`--focus needs --zoom F with F > 1`), so give `--nocull --nofocus` along with it.
- **Versions.** A later format is refused ("this session is of format 2, written by a newer zippers; this one reads
  format 1"); an unknown draw key is an error, since it would change the picture; keys beginning `window.` are kept, and
  an unknown one is ignored with a warning.
- `zippers --dry-run draw --session FILE` prints the program's command line:

```sh
zippers --dry-run draw --session s3_cartwright_steger.zsession --yaw 60
```

```text
BUILD/apps/s3/zipper3 -f subst_4.00_9.50_t3_r1.txt --pairindex 0 -R 2.554 -a 0.05 -s 300 --zoom 3 --yaw 60 --pitch 20 --pole 10 5 --clip 3 --overlap 0 --cap 8000000 --thick 0 --cull --focus 1 --colour Z+=0.05,0.15,0.85 --colour Z-=0.85,0.08,0.08 --colour pair.Z+=0.05,0.15,0.85 --colour pair.Z-=0.85,0.08,0.08 --colour context.Z+=0.7,0.75,0.95 --colour context.Z-=0.95,0.72,0.72
```

`test_gui_session` **tests** that each stored session remakes the picture of its command line, byte for byte, on any
machine, the hashes, the refusals and the options after a session.

## 6. The data files

`zipper3`, `zipper5`, `link3`, the measuring tools and `zippers` find a data file (the default, or that of `-f`, and
the file of `--pairs`) by this search (`src/core/data_files.h`):
1. a name that contains `/` is read as given, and nothing else is tried;
2. the working folder;
3. the folders of the environment variable **`ZIPPERS_DATA`**, separated by `:`: in each, `s3/NAME` (or `s4/NAME`), then
   `NAME`;
4. the data of the program: `data/` of the source tree for a program of a build, `share/zippers/` of the prefix for an
   installed one (found from the program's own place, so an installed tree can be moved).

The first place that holds an entry of the name is used. If that entry is not a readable file (a folder, an unreadable
file, a link to nothing), the program stops with exit status 1, saying what is there, and never passes it over to the
next place. A file found in the working folder keeps its name as given; a file found elsewhere is named by the path
at which it was found. A file found nowhere stops the program, which says where it looked.
`zippers list EXAMPLE` shows where each default file is found and the places searched:

```sh
ZIPPERS_DATA=/some/folder zippers list imm
```

(the line `the data files are looked for in:` then shows `ZIPPERS_DATA (/some/folder)` instead of `ZIPPERS_DATA (not set)`.)
S² has no data file. **Tested** by `test_default_files`.

The files are in `data/` (`data/README.md` lists each with the search that made it). The defaults: `zipper3` reads
`subst_4.00_9.50_t3_r1.txt`; `zipper3 --pairs` and `link3` read `subst_4.00_9.50_t3.txt` and `linked_pairs.txt`;
`zipper5` reads `subst5_2.30_4.50_t1_r0.txt`. Each substitution file is written by the kernel search, `zippers search`;
a file written with `--roofs`, `--jexcess` or `--tcand` says so in its third line (`# options: ...`).

**`zippers search` and the files.** The search is how the data were made; the pictures of the stored files are the
reference pictures. `zippers search cs` without numbers writes `subst_4.00_9.50_t3_r1.txt` into the working folder, and
`zippers draw` run there then reads it in preference to the stored one (the working folder comes first). On another
platform it may choose other words for the same roofs (section 7), and its file then differs from the stored one.

## 7. What is checked, and the limits

### What `zippers check` certifies

`zippers check` makes 21 checks (all pass; about 25 s). Each verdict holds on every machine, since none depends on
rounding:

| sphere | check | what is certified |
|---|---|---|
| S² | the exactness checks of a drawing (`zipper -m fig8 -n 2 -R 5 --stats`) | see below |
| S³ | `chz selftest` | exact arithmetic of ℤ[ζ₁₂] and the lattice: 19 checks |
| S³ | `link3 selftest` | the linking-number code on four known links, in four random projections |
| S³ | `chz kernel 4 9.5 3` | the certificate that S_c generates the kernel, with the roofs |
| S³ | `zipper3 --stats` at R = 4.5 | the root balls (below) |
| S³ | `link3 --pair` for each of the 13 stored pairs | the linking number of the pair, in 20 projections, with "A over B" and "B over A" agreeing |
| S⁴ | `chz5 selftest` | the reflection group and the cocycle: "160 orthogonal pairs, cocycle on every square from all 16 states" |
| S⁴ | `chz5 kernel 2.3 4.5 3` | the certificate that S_c generates the kernel |
| S⁴ | `zipper5 --stats` at R = 2.1 | the root balls |

- **The root balls** (`--stats`, S³ and S⁴; `src/examples/cartwright_steger_roots.h`, the end of `src/examples/imm_m5.h`): the
  search of the roots is made again and checked exactly: the set is closed under the steps (the facet reflections in S⁴,
  16 of them) inside the searched ball, and every element but 1 has a strictly nearer neighbour. S³ accepts search radii up
  to 6, which covers every radius in use. A check of the descent alone would pass a search that skipped a step; the closure is
  checked too. `link3 --stats` makes the same check for its translates.
- **The substitution files** are checked when read: the program prints `roofs checked exactly` and `t^+ and t^- found`.
- **The S² exactness checks** (`--stats`): `FALSE-MERGE` (two branches with the same key that are not the same arc: the
  program exits with status 2), `GEOMETRIC-DUPLICATE` leaf segments, leaves that reached the depth cap, the spikes (leaves
  that stray more than twice the accuracy from their chord: measured by refining the leaf to accuracy/16), the overdraw
  (estimated from a sample of the pieces), and the parabolic chains. `--stats` costs two to four times the time and two to
  three times the memory. For `fig8 -R 3` it prints 0 false merges, 0 geometric duplicates and 0 spikes beyond twice the
  accuracy. The lines say which of these are exact and which are estimated from a sample.
- **The comparison of pictures** with recorded ones is not a `zippers check` but the reference test of the source tree
  (`make test`, `tests/run_references.py`): 46 recorded runs, 45 in the fast subset. Byte for byte on the machine of the
  recording (macOS on Apple silicon, one compiler), and only the portable results elsewhere (`tests/README.md`).

### What is measured, not certified

- **Culling is a measured bet, not a proof.** There are no enclosures. Among the branches that K = 16 can cull (size below
  1/16) the largest stray is 3.3 sizes on `subst_4.00_9.50_t3_r1.txt` up to R = 4.5 (a safety factor of 4.8), and 1.4
  sizes on `subst5_2.30_4.50_t1_r0.txt` up to R = 2.1 (11 or more): `cull --mode margin`, `docs/developer_guide.md`.
  **Larger R is unmeasured** (above 4.5 in S³, above 2.1 in S⁴). The S³ file `_t1` strays up to 8.9 sizes: use `--cullk 32`.
  In every comparison made (zooms 1 to 1000, R up to 2.554, accuracies down to 0.001) the segments drawn in the window
  were the same with and without culling; `test_cull_programs` compares culled and unculled pictures byte for byte
  (in S⁴ with the focus ball a few dozen pixels can differ, the same segments, since the painter orders them
  differently).
- **The strays.** A leaf is accepted on its chord. How far the true curve strays from it is measured by `strays` (`apps/tools/`),
  not certified. The largest stray of a picture is 1.37 ACC (Z⁺) and 1.30 ACC (Z⁻) in S³ (file `subst_4.00_9.50_t3.txt`), and with the tips at
  most 1.64 ACC in S⁴ (`_t1`, R = 0, ACC 0.04, 0.02 and 0.01), nothing straying beyond 2 ACC. In S³ `_t1` the strays reach 4.94 ACC.
  Without the tips the S⁴ spikes into the cusps grow like ACC^(-1/2) (`docs/developer_guide.md`, "Tips").
- **Marking** (`--overlap L`, L ≥ 1) is a measured trade: at L = 1 it saves 1 to 5% of the leaves for 1.2 to 3.5 times the
  time and up to 4.5 times the memory (S³, S⁴). What holds is a subset by keys, not leaf by leaf (`docs/developer_guide.md`,
  "Marking and overdraw").
- **Overdraw.** Where consecutive roofs cancel the ink is drawn twice, and no key removes it: 21 to 23% in S³ Z⁺, 13 to 15% in
  Z⁻, 15 to 18% in S⁴ Z⁺ and 65 to 72% in S⁴ Z⁻ on the stored files, measured by `overdraw`. The default files, whose roofs
  `--roofs` chose, have less (S³ 10% and 0.4%; S⁴ 10% and 5% at ACC 0.02).
- **The two orientations of an arc.** The renderer draws one orientation of a branch and its reversal; in S⁴ they can be different
  arcs with the same ends, except in the default file, where the two orientations of every involution arc are one arc
  (gap at most 3.3e-14). Compare drawn curves, not keys (`docs/developer_guide.md`, "The renderer").
- **The distance between Z⁺ and Z⁻** (`zipper3 --check`) is the smallest distance between the drawn chords, 0.02089 on the
  default picture: a measurement on the picture, not a bound.

### The limits

- **Memory of S² (the cap).** Drawing stops before the process holds 4 GB (6 GB with `--stats`; `--memlimit GB` changes it)
  with a warning ("drawing stopped at the memory limit; the picture is incomplete"), writes the picture, and exits with
  status 3. The polyline file says `capped Z+ yes`:

  ```sh
  zippers draw fig8 -R 6 -a 0.0005 --memlimit 0.2 --polylines m.poly -o m.png
  ```

  ```text
  monodromy fig8, order 2: tr a = 1.780776+0.624811i, tr c = 2.135779+0.000000i
  warning: Z+: drawing stopped at the memory limit (0.2 GB); the picture is incomplete (use a larger -a, or a smaller --zoom or -R)
  warning: Z-: drawing stopped at the memory limit (0.2 GB); the picture is incomplete (use a larger -a, or a smaller --zoom or -R)
  wrote m.poly
  wrote m.png
  Z+: 1588224 leaves, 20289 polylines ; Z-: 1278387 leaves, 12466 polylines
  ```

  (the exit status is 3, and the numbers of leaves vary). The guard reads the memory the process holds, so a picture whose
  drawing reaches the limit can differ from run to run, and the memory of `--stats` counts against it, so `--stats` can change
  such a picture.   **Tested** by `test_polylines_s2` at `--memlimit 0.2`.
- **The cap of S³ and S⁴** (`--cap N`: 8e6 and 2e7 leaves per zipper by default): the leaves beyond it are not drawn, and the
  header of a polyline file says `capped NAME yes`. In S⁴ complete arcs retrace themselves at every scale, so anything
  built on complete arcs explodes (87.7 million leaves where the picture has 294 thousand, on Z⁻ at ACC 0.01): measure
  first (`strays --dedup`).
- **The ball of S²**: `-R` is refused above the largest value for the monodromy and order (about 8.16 for `fig8`, order 2),
  and the zoomed ball and the wheel have their own (`maxR.focus`, `maxR.wheel`, in the header of a polyline file).
- **The shells that `-R` steps through.** The roots are the elements of the group within distance R of the base point; the set
  changes only at certain radii, the shells. In S³ the first are at R = 2.553, 3.326, 3.983, ... (`cs::shell_radii`; thirteen up to
  5.5), in S⁴ there are six up to 2.5 (`imm::shell_radii`). `-R` exactly at a shell's radius includes that shell, and the
  picture at a smaller R is exactly the first part of the one at a larger R (without the focus ball), on the default files and
  at the accuracies tried (S³: every shell up to 5.5 at ACC 0.1 and up to 3.33 at the default 0.01; S⁴: every shell up to 2.5
  at ACC 0.05 and at the default 0.02): **tested** by `test_shells`. Other files and accuracies are not tested.
- **The platform.** The outputs are reproduced byte for byte only on the recording's kind of machine (macOS on Apple silicon,
  AppleClang 21.0.0.21000101). Elsewhere the portable results are the same: the counts of letters, the certificates, the
  linking numbers. The S³ search chooses other words for the same roofs on Linux (28 cancelling junctions against 20 in
  `chz kernel 4 9 1`), so its file differs, and its certificate holds all the same. An x86_64 build on Apple silicon
  differs in the last bits (`docs/developer_guide.md`).
- **The reader of S⁴ words.** `zipper5` reads facet 1, which the legacy reader dropped; its pictures differ from the legacy
  program's (44 base arcs of the stored file, not 47).
- **The linking numbers** of `link3` are computed on the drawn chords at ACC 0.01 in 20 projections, with the two
  over/under counts agreeing; the pairs have separation at least 0.0197 against the accuracy 0.01. They are verified, not
  proved for the curves. The scan (`link3` without `--pair`) is heavy: about 8 minutes and 0.5 GB.
- **What has no check.** The window and the film's pictures are not checked by `zippers check`; the film engine is held by the
  reference runs (`zmovie`, 4 frames), not by a certificate. `zippers film` is run on the S² examples only.

## 8. The programs

`zippers` runs seven programs and no other. Their own options are in `apps/README.md` and in each program's usage
text (`-h`).

| program | sphere | run by | what it does |
|---|---|---|---|
| `zipper` | S² | `draw` of `fig8` and words | draws the zippers of a monodromy; EPS or PNG |
| `zmovie` | S² | `film` | the frame engine of the S² film |
| `chz` | S³ | `search cs`, `check` | the kernel search (`chz kernel`) and its selftest; `chz ball` is not run by `zippers` |
| `zipper3` | S³ | `draw cs` | draws the zippers, or linked pairs, as a PNG |
| `link3` | S³ | `link`, `check` | linking numbers of loops in Z⁺ and Z⁻; the scan, a single pair, the export |
| `chz5` | S⁴ | `search imm`, `check` | the kernel search and its selftest; `chz5 ball` is not run by `zippers` |
| `zipper5` | S⁴ | `draw imm` | draws the zippers as a PNG |

Not run by `zippers`: `ztest` (a measurement of the film engine against `zipper`, with no verdict), `film5` (the film's
ending on S⁴: a window), and the measuring tools of `apps/tools/` (`strays`, `islands5`, `overdraw`, `roofs`, `cull`),
which change nothing in `src/` and are not part of the reference runs; the options of each are in the comment at the top
of its source. `zipper -v` and `zipper3 -v` are the programs' windows, built from `legacy/`.
