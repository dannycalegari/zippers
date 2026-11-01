# apps

The programs built on the shared library `src/`.

| folder | programs |
|---|---|
| `s2/` | `zipper` (without its window), the film engine `zmovie`, and its checks `ztest` |
| `s3/` | `chz` (the kernel search), `zipper3` (without its window), `link3` (linking numbers) |
| `s4/` | `chz5` (the kernel search), `zipper5`, and `film5`, the film's ending on S⁴ (a window) |
| `cli/` | `zippers`, the command line of the suite: it runs the programs above by name (below) |
| `gui/` | the window's program `zview` (GLFW and OpenGL 3.3; without GLFW only `--headless`) and what it stands on: its drawing (`draw.h`, OpenGL only); and with no OpenGL, its model (`model.h`), the state and the handlers of the keys and the mouse; its key feed (`feed.h`); its jobs (`jobs.h`), `zippers draw` run as child processes, one at a time (below) |
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
| `draw --session FILE [options]` | the same, with the example and the options of a session file (below) |
| `view [EXAMPLE] [options]` | `zview`, the window on the curves of `draw EXAMPLE [options]` (by default `fig8 -n 2 -R 6`), or `view --session FILE [options]`; a build without GLFW has no window, and `zview` says so (below) |
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

### Session files

A session file (format 1, `src/output/session.h`) holds the draw command of a picture with every option written out,
so that `zippers draw --session FILE -o PIC` remakes the picture:

```
zippers session 1
made_by = zippers 1.0, Darwin arm64, AppleClang 21.0.0.21000101
example = cartwright-steger
phi = 1,0
f = subst_4.00_9.50_t3_r1.txt
sha256.f = (64 hexadecimal digits)
R = 2.554
a = 0.05
...
colour = Z+=0.05,0.15,0.85
```

- **The lines** are `KEY = VALUE`, with `#` for comments. The draw keys are the options of `zippers draw` without their
  dashes, with their values as on a command line (`pole = 10 5`; a flag is `C = yes` or `C = no`; one `colour` line for
  each colour). Every option of the picture is written, its default included, so that a later version that changes a
  default still remakes the picture. For a picture (`-o`), a zoomed still's culling and focus ball are written as the
  options they stand for. A command line without `-o` (curves) is written as it is: its session, replayed without
  `-o`, gives the command's curves. Replayed with `-o`, it makes a still with the culling and focus ball of the
  `zippers` that replays it (at a zoom above 1), which a later version may change; to keep those of today, make the
  session from the command line with `-o`. In S² `--stats` is a draw key (`stats = yes`): the memory of its tables
  counts against the drawing's limit, so it can change a picture that reaches the limit. The limit is written as the
  command had it: 6 GB with `--stats` (zipper's default then), else 4. A picture whose drawing reaches the limit can
  still differ from run to run, since zipper's guard reads the memory the process holds. In S³ and S⁴ `--stats` only
  prints. The outputs
  (`-o`, `--polylines`), the options that only print (`--check`, `--timing`, and `--stats` in S³ and S⁴) and
  `--format` are not draw keys. Numbers are the shortest decimals that read back to the same doubles.
- **`sha256.f`** is the SHA-256 of the substitution file that the data search found when the session was written, and
  `sha256.pairs` that of the file of `--pairs`. `zippers draw --session` finds the files as always, and refuses one
  whose hash differs, saying which file it found, where, and both hashes; so a file of the same name in the working
  folder cannot change the picture silently.
- **Options given after `--session`** replace the session's: a key by its name, a colour by its colour's name; the view
  by `--viewmatrix` and the view by the angles replace each other (`--yaw` after a session of a matrix drops the
  matrix, and the angles not given are then the programs' defaults); `--nocull` drops the session's culling; `-f` (and
  `--pairs`) replace the file and its check.
- **Versions:** a later format is refused ("this session is of format 2, written by a newer zippers; this one reads
  format 1"); an unknown draw key is an error, since it would change the picture; the window's own keys (`window.`)
  are kept, and an unknown one is ignored with a warning.
- `zippers --dry-run draw --session FILE` prints the program's command line. `tests/sessions/` holds sessions of each
  sphere and of a pair picture; `tests/unit/test_gui_session.cc` checks that each still remakes its picture.

### The jobs of a window

`gui/jobs.h` runs `zippers draw` as child processes for a window (with no OpenGL and no window of its own), so that the
curves it shows and the pictures it saves are `zippers draw` commands' outputs: a curves job writes `--polylines` into
the jobs' own temporary folder and is read back in a thread; a new request for curves ends a running curves job and
starts 0.3 s after the last request (`cancel_curves` ends a curves job and leaves the pictures); a picture job is ended only by a cancel or by quitting, which remove its partial
files; one child runs at a time; the child's standard error is kept and its memory sampled 4 times a second; a memory
limit (by default 40% of the machine's memory) is passed to S² jobs as `--memlimit` and ends S³ and S⁴ jobs that pass
it; a child is ended by `SIGTERM`, then `SIGKILL`, and always waited for, and inherits no descriptor but its standard
streams. The jobs' folder is removed when they end, and, when they handle the signals (`handle_signals`: one `Jobs` at a
time), on `SIGINT`, `SIGTERM` and `SIGHUP`; without it a signal that ends the process leaves the folder. The top of
`gui/jobs.h` says more.

### The window's model and keys

`gui/model.h` is the window's state and the handlers of its keys and its mouse, with no OpenGL and no window, so that
they are the same with a window and without one. The curves it shows are those of `zippers draw ... --polylines`, run by
the jobs, for the whole sphere; the first ones give the start view, the program's own (the header of the polyline
file). The keys:

| key | S² | S³ | S⁴ |
|---|---|---|---|
| ← → ↑ ↓ | rotate the sphere 6° about the screen's axes | move the pole 4° toward the screen's axes | as S³ |
| `w` `s` | — | the pole toward, away from the viewer | as S³ |
| `+` `=` `-` `_` | R ± 0.5 (R ≥ 1; `+` checked at once against the program's limit), new curves | the next, the previous shell of roots: within the R of the curves shown, their first polylines (no job); beyond it, or in detail, new curves | as S³ |
| `]` `[` | the accuracy ÷ 1.5, × 1.5: new curves | as S² | as S² |
| `z` `Z` `x` `X`, the scroll wheel | the zoom × 1.5, ÷ 1.5, in [0.1, 10⁷] | as S² | as S² |
| `c` `C` | the zippers → and the wheel → the wheel | both → Z⁺ → Z⁻ | as S³ |
| `b` | — | the clip radius 3 → 6 → 12 → 1.5 | as S³ |
| `r` | the start view | as S² (with a pair: its view) | as S² |
| `n` `N` `l` | — | with `--pairs`: the next, the previous linked pair; the zippers faintly behind it | — |
| `h` `H` `j` `J` | — | — | turn the hidden direction ±5°: `--turn2` (with the horizontal axis), `--turn` (with the depth axis) |
| `p` `P` | save the picture of the view: `NAME.png` and `NAME.zsession`, never overwriting a file | as S² | as S² |
| `f` | the curves of this view in detail (culled, at the accuracy on the screen); `f` again, or a change of the view, for the whole picture | as S² (not of a pair) | as S² |
| `?` | the keys | as S² | as S² |
| `q` Esc | quit; a picture not yet written is removed | as S² | as S² |
| a drag | rotate, 0.4° a pixel | rotate R³, 0.4° a pixel | as S³ |

A key that does nothing in the geometry shown says so; Tab does nothing (the window has no panel).

**`p`** writes the session of the view first (`src/output/session.h`), then runs `zippers draw --session NAME.zsession
-o NAME.png` as a picture job, so that the picture is the output of the command it prints, byte for byte; zoomed, it
also prints the command of the full detail (S²: `-a` divided by the zoom, down to 10⁻⁶; S³, S⁴: `--zoomacc`). NAME is
the stem of `-o` (by default the example's name), then `NAME_1`, `NAME_2`, ... S²'s picture frames the window's square
(at least 1024 pixels, `s2::export_framing`) at the angles of `s2::command_angles`; S³ and S⁴ give the view as
`--viewmatrix`, S⁴ with `--turn` and `--turn2`. A session also holds the window's own keys (`window.stem`,
`window.size`, `window.layers`, and in S² `window.zoom` and `window.view`), so that `zippers view --session NAME.zsession`
opens on the same state. In S² with the wheel alone shown, the picture draws the zippers with the wheel (no command draws
the wheel alone).

**The jobs' memory limit** is the jobs' default, 40% of the machine's memory.

### The window

`gui/zview.cc` opens a GLFW window with an OpenGL 3.3 core context and draws it with `gui/draw.h`. GLFW's character
callback gives the keys (with shift applied), its key callback the arrows, Esc and Tab; the drag, the scroll wheel (a
notch at a time) and the window's size are in the window's points, and the picture is drawn in the framebuffer's
pixels (twice the points on a Retina screen). The loop waits for events at most 0.1 s, takes the jobs' results, and
draws again only when something changed. The title shows the example, R, the zoom and whether a job runs; the messages
are on the standard error. On macOS GLFW is kept from changing the working folder (`GLFW_COCOA_CHDIR_RESOURCES`).

`gui/draw.h`: one vertex buffer for each layer, uploaded once for each set of curves, and each segment an instanced
quad, whose vertex shader projects both ends as the painters do (S²: the front hemisphere; S³: `s3::eye` and its fog;
S⁴: `s4::eye5`, its two fogs, and the painter's sort key `z + 10 fh` as the depth), drops it unless both ends pass, and
widens it to the layer's width (1 point; S³'s linked pair 2.5, over its context, the depth buffer cleared between the
two). S²'s silhouette circle and the crosses of t⁺ and t⁻ are drawn as the pictures draw them. Lines are smoothed by
4x multisampling. Its OpenGL functions are loaded through `glfwGetProcAddress`, so the same code runs on macOS and
Linux and `draw.cc` links no OpenGL library. The view's change needs no pass over the points; beyond a zoom of about
10⁵ the single precision of the GPU shows as steps (the pictures are computed in double).

The build: `ZIPPERS_WINDOW` (`AUTO`, the default, builds the window when GLFW 3.3 or later and OpenGL are found; `ON`
requires them; `OFF`), `cmake/ZippersWindow.cmake`. With the window `zview` is installed into `libexec/zippers/`.

**`zview --headless`** (for the tests; not in the usage texts) runs the model and its jobs with no window: it computes
the first curves, feeds the tokens of the environment variable `ZIPPERS_KEYS` to the handlers (`gui/feed.h`: one
character, `<L>` `<R>` `<U>` `<D>`, `<E>`, `<T>`, `<W>` a pause of `ZIPPERS_KEYDELAY` ms, `<sWxH>`, `<drag DX,DY>`,
`<scroll N>`, `<wait>` until no job runs, `<S>` the state into `STATE_NN.zsession`; `<G>` is refused), and exits at
their end, or at `q` or Esc, with status 1 if a token was refused. `zview` is built in every build and needs no window
system for it. The same tokens drive the window when `ZIPPERS_KEYS` is set, one every `ZIPPERS_KEYDELAY` ms once the
first curves have come, `<G>` reading the picture area back into `GRAB_NN.png`; `zview` then exits at their end. With
`ZIPPERS_NO_WINDOW` set (for the tests), `zview` without `--headless` checks its options and exits with status 1 without
opening the window. Without GLFW `zview` is not installed: `zippers view` finds it as it finds the programs, and an
installed `zippers view` says that the installation has no window.

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

## The curves as data, the exact view, the colours

Three options of `zipper`, `zipper3` and `zipper5` (and of `zippers draw`), which change nothing when they are not given:

- **`--polylines FILE`** writes the curves that the program drew as a polyline file (format 1, `src/output/polylines.h`):
  a text header of `KEY VALUE` lines, from `zippers polylines 1` to `data`, then little-endian blocks. Each layer holds,
  for each polyline, its first point, its root and its letter, and then the points as the renderer stores them (floats),
  so that a reader has exactly the points that the program drew; the file ends with the distances of the roots.
  - The layers: `Z+`, `Z-`; with `zipper -C`, `wheel`; with `zipper3 --pairs`, `pair.Z+` and `pair.Z-`, whose polyline
    k is the loop of pair k. In S³ and S⁴ every polyline records its root and its letter; in S² none (−1: the polylines
    are joined across roots).
  - The header: `program`, `command` (the command line), `example`, the parameters (`order`, `file`, `R`, `acc`, `cap`,
    `overlap`, `only`, ...), whether the curves were culled to the view (`cull`: S² `on` or `off`, S³ and S⁴ the margin
    factor or `off`) and the focus ball (`focus`), and the view. S²: `view` (the rotation, row by row), `yaw`, `pitch`,
    `roll`, `zoom`, and the largest R that the program accepts for the example (`maxR`; `maxR.focus` with the focus
    ball; `maxR.wheel` with `-C`). S³: `viewmatrix` (the 25 numbers of `--viewmatrix`), `zoom`, `clip`, the marks
    (`mark t+ ...`, `mark t- ...`), and with `--pairs`, `pairs` and for each pair K `pair.K.pole`, `pair.K.clip`,
    `pair.K.lk`, `pair.K.line`. S⁴: `viewmatrix` (34 numbers), `turn`, `turn2`, `U` (the rotation of R⁴ that they give),
    `zoom`, `clip`, `hidfog`, the marks. In all three, for each layer, `capped NAME yes` or `capped NAME no`: whether
    its curves are incomplete because a cap was reached (S³ and S⁴: the renderer's cap of leaves, `--cap`; S²: the
    memory limit of the drawing, or the cap of the root search; the wheel when a zipper is).
  - The `file` key gives the data file as the program found it: its plain name when it was found in the working folder,
    and otherwise its full path.
  - `zipper` without `-o` then writes no picture (not even EPS on the standard output); with `-o` it writes both.
    `zipper3` and `zipper5` write the file after the build (and `--pairs`, `--check`, `--stats`), before any picture.
    All three refuse a FILE that they cannot write (a folder of the name, a folder that does not exist or may not be
    written in) before they compute anything, and at the start an argument that holds a line break, which the
    header's command line cannot hold.
  - The file is written under a temporary name in its folder and renamed at the end: a write that fails leaves nothing
    behind, an older file of the name stays as it was, and the message gives the system's reason. So the folder must
    be one that may be written in, even when the file itself may be written; such a file is refused, and the message
    says so. A name that is a symbolic link is written through: its target is written (the temporary name beside the
    target), and the link stays a link. A program ended while it writes leaves its temporary file, `FILE.tmpPID_K`
    (PID its process), beside the name.
  - In S³ and S⁴ the polylines come in the order of their roots, nearest first, and without the focus ball the curves
    of a smaller R are exactly the first polylines of those of a larger R; `-R` exactly at a shell's radius includes
    that shell (on the default files, at every shell up to R = 5.5 in S³ at ACC 0.1 and up to 3.33 at the default 0.01,
    and up to 2.48 in S⁴ at ACC 0.05 and at the default 0.02: `tests/unit/test_shells.cc`). The shells are `cs::shell_radii` (S³) and `imm::shell_radii` (S⁴).
    With the focus ball the roots of fh B(R) come after all of B(R), so there they are not.
- **`--viewmatrix N1,...`** (`zipper3`, `zipper5`): the view as its matrices, comma-separated: in S³ the frame Q of R⁴
  (16 numbers, row by row) and the rotation of R³ (9); in S⁴ the frame of R⁵ (25) and the rotation of R³ (9), the
  hidden direction staying that of `--turn` and `--turn2`. It replaces the view of `--yaw`, `--pitch`, `--pole` and
  `--centre`, which it does not go with; `--zoom`, `--clip` and `--hidfog` are as usual. The view is then the one given,
  bit for bit: the `viewmatrix` of a polyline file's header draws the picture of the zippers in that view again. The
  frame and the rotation must be orthonormal to within 1e-9 (the largest entry of |M Mᵀ − I|), which leaves room for
  the rounding of millions of turns and refuses a number mistyped anywhere but in its last few digits.
  - In S³ both must have determinant +1: a frame or a rotation of determinant −1 would draw the mirror image, on which
    every linking number has the other sign, and no option or turn makes one.
  - In S⁴ either determinant is accepted. The program's own frames have determinant −1, and the picture does not see
    the sign of the hidden coordinate (its fog is even in it), so a reflection of the frame or of the rotation draws
    what some rotation of the frame draws.
  - With `zipper3 --pairs -o` the pair is drawn in the view given, with `--zoom` and `--clip`, in place of its own. Its
    own view, which the picture without `--viewmatrix` has, is the frame of the pair's pole (`pair.K.pole`), its clip
    radius (`pair.K.clip`) and zoom 1; the header's `viewmatrix` is still the view of the zippers.
- **`--colour L=R,G,B`**: the colour of what L names, three numbers in [0, 1], one option for each L. The names are
  the polyline file's layer names, and the zippers behind a pair:
  - `Z+` and `Z-`, the zippers: `zipper` `0,0,1` and `1,0,0`; `zipper3` and `zipper5` `0.05,0.15,0.85` and
    `0.85,0.08,0.08`;
  - `wheel`, the wheel (`zipper -C`): `0,0.6,0`;
  - `pair.Z+` and `pair.Z-`, the loops of a pair (`zipper3 --pairs -o`): the zippers' colours;
  - `context.Z+` and `context.Z-`, the zippers drawn faintly behind a pair (`--context`): `0.70,0.75,0.95` and
    `0.95,0.72,0.72`.

  A name that the picture does not draw is accepted and changes nothing (the zippers' colours in a pair picture, the
  wheel's without `-C`); the names of another program are refused. When one is given twice, the last one counts. The
  defaults given as options draw the same pictures, byte for byte.

## Installing

`cmake --install build --prefix P` (or `make install PREFIX=P`) puts `zippers` in `P/bin/`, the seven programs it runs
(`zipper`, `zmovie`, `chz`, `zipper3`, `link3`, `chz5`, `zipper5`) in `P/libexec/zippers/`, and the files of `data/` in
`P/share/zippers/`. `ztest`, `film5`, `zview` and the tools are not installed.

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
- **`--polylines`, `--viewmatrix` and `--colour`** (above) are new. Without them nothing that the programs print or write
  changes; their usage texts have a few more lines.
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
