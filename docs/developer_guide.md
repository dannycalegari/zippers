# Developer guide

For those who change the code: how it is built, how to add an example or a geometry, and then what has been measured
and the traps, by part of the code. Each measurement section names the code it is about; the comments of the code
point here where a fact is too long for them. The user's side of the programs is in `docs/user_guide.md` and in the
READMEs of `apps/`, `legacy/` and `film/`, and the tests are described in `tests/README.md`.

Contents: [the architecture](#the-architecture), [one picture, from the command line to the file](#one-picture-from-the-command-line-to-the-file),
[adding an example](#adding-an-example), [adding a geometry](#adding-a-geometry), [the tests](#the-tests-to-add),
then the measurements and traps: [building and testing](#building-and-testing), [the renderer](#the-renderer-srcrender),
[the examples](#the-examples-srcexamples), [the roofs of `data/`](#the-roofs-of-data).

## The architecture

### The folders

| folder | what it holds |
|---|---|
| `src/` | the shared library, header only: programs and tests link the CMake target `zippers::core`, which puts `src/` on the include path. `src/README.md` has one line per header |
| `apps/` | the programs (`apps/s2/`, `apps/s3/`, `apps/s4/`), `zippers` (`apps/cli/`), the window `zview` with its model, its jobs and its drawing (`apps/gui/`), and the measuring tools (`apps/tools/`) |
| `tests/` | the tests and the reference runs (`tests/README.md`) |
| `data/` | the substitution files (`data/s3/`, `data/s4/`) and the file of linked pairs; installed to `share/zippers/` |
| `legacy/` | the programs as they were before the shared library, with their makefiles, kept as the specification: moved code must reproduce them bit for bit (`legacy/README.md`) |
| `film/` | the tools that make the joined film (Swift tools, scripts) |
| `docs/` | the user guide and this guide |

### The folders of `src/`

- **`src/geometry/`: the backends.** `backend.h` says what a geometry backend must provide and checks it
  (`is_backend<B>()`); `sphere.h` has `SpherePoint<N>`, `chord` and `segment_distance`; `pu21/` is `PU21` (PU(2,1)
  over Z[zeta_12], the sphere S³, with the matrix type `M3`); `on1/on1.h` is `ON1<n>` (O(n,1) over (1/2)Z; `ON1<5>`
  is S⁴). The S² programs do not use a backend: they keep the renderer of `legacy/s2/zipper.cc` (`apps/s2/zipper.cc`).
- **`src/core/`.** `hash.h` (`fingerprint`, the renderer's key of a branch), `keyset.h` (`KeySet`), `timing.h`,
  `memory.h`, `sha256.h`, and `data_files.h` (`find_data_file`, `find_data`: where a data file is found).
- **`src/store/store.h`.** `Store<B, Label>`: exact group elements, packed, with a hash table on their keys. The
  searches and the root balls are built on it.
- **`src/examples/`: an example is a data header, a search policy and a render policy.** For Cartwright-Steger (namespace `cs`) and
  IMM's M5 (namespace `imm`): `cartwright_steger.h` and `imm_m5.h` (the group's data, its words, balls, roots and
  `shell_radii`); `*_search.h` (the search policy: what `chz` and `chz5` each do differently); `*_render.h` (the
  render policy `RenderPolicy`: the example's file dialect, words, roots, tips and look-ahead); and the exact checks of
  the root balls (`cartwright_steger_roots.h`, the end of `imm_m5.h`, both behind `--stats`).
- **`src/search/`.** `kernel_search.h` is the block-filling kernel search, written once as `kernel_search(Policy&, ...)`
  over a search policy; `roof_choice.h` chooses the roofs (`--roofs`) and measures a system of roofs.
- **`src/render/`: the renderer.** `substitution.h` reads a substitution file (`load_substitution`, a
  `SubstitutionDialect` per example); `renderer.h` has `Letters<B>`, `Side<B>` (one zipper), `make_side` and
  `Renderer<B>` (the polylines of a zipper); `world.h` has `World<B>`, `prepare<P>` and `build<P>`; `cull.h`
  (`CullWindow<N>`), `tips.h` (the tips of S⁴'s parabolic chains), and `arcs.h` (`arc_points` and `loop_points`: complete
  arcs and loops, for the analysis).
- **`src/view/`: the views.** `s3.h` and `s4.h` are the cameras and the PNG painters of S³ and S⁴ (`s3::write_png`,
  `s4::write_png5`), with the windows of zoomed views (`s3::window3`, `s4::window5`); `s2.h` is the camera of S²;
  `options.h` is what `--colour` and `--viewmatrix` share.
- **`src/output/`: the outputs.** `png.h` (`png::write_rgb` and the other writers), `raster.h` (the depth-sorted
  segment painter `raster::paint_segments`), `polylines.h` (the polyline file), `session.h` (the session file).
- **`src/analysis/`.** The separation of Z⁺ and Z⁻ (`separation.h`), relations and their loops (`relations.h`), linking
  numbers (`linking.h`), and the linked pairs (`pairs.h`).

The dependencies run one way: a backend knows nothing of the renderer; the renderer, the store and the search are
templates over a backend and (for the search and the render) over an example's policy; the views and the analysis use
the renderer's output; the programs of `apps/` put them together. Every inner loop is a template instantiated for one
backend, so there are no virtual calls.

### The programs

`apps/s3/` has `chz` (the kernel search), `zipper3` (the pictures) and `link3` (linking numbers); `apps/s4/` has `chz5`,
`zipper5` and `film5`; `apps/s2/` has `zipper`, `zmovie` and `ztest`. Each program of `apps/s2/` to `apps/s4/` has
the command line of its legacy original; those that have reference runs (all but `ztest` and `film5`) are listed in
`tests/reference/programs_apps.json`. `apps/cli/zippers.cc`
is `zippers`: it checks the options against its table (`OPTIONS`), and replaces itself by the program (`execv`).
`apps/gui/jobs.h` runs `zippers draw` as child processes for a window. `apps/tools/` holds the measuring programs
(`strays`, `overdraw`, `roofs`, `cull`, `islands5`), which change nothing in `src/`.

## One picture, from the command line to the file

The picture of the default S³ example, `zippers draw cartwright-steger -o PIC.png`, with the place of each step.

1. **`zippers`** (`apps/cli/zippers.cc`). `main` (line 1237) reads the command; `draw` (1015) finds the example
   (`find_example`, 182), checks every option against `OPTIONS` (`parse`, 408), and builds the program's argument list
   (`pass_on`, 465; `program_of`, 351, names `zipper3` for S³). `run` (476) replaces `zippers` by that program
   (`execv`, 492), so everything after it is the program's.
2. **The program** (`apps/s3/zipper3.cc`). `main` (344) calls `cs::setup` (346: the group's matrices,
   `src/examples/cartwright_steger.h:58`) and reads its options.
3. **The data file.** `find_data` (`zipper3.cc:486`; `src/core/data_files.h:244`, over `find_data_file`, 191) looks in
   the working folder, then `ZIPPERS_DATA`, then the data of the build or install (`apps/README.md`, "How the programs
   find their data files").
4. **`prepare<cs::RenderPolicy>`** (`zipper3.cc:492`; `src/render/world.h:106`). It reads the file
   with `load_substitution` (`src/render/substitution.h:52`, in the policy's dialect), turns each word into an exact
   element with `P::word` (`parse_word`, `cartwright_steger.h:171`), finds each letter's inverse (it stops if the
   letters are not closed under inversion), and builds the two zippers with `make_side` (`src/render/renderer.h:201`),
   which checks every roof exactly. Then it calls the policy's hook `P::sides` (the look-ahead of S³, the tips of S⁴)
   and `P::prepared` (the line it prints).
5. **The view** (`zipper3.cc:495`): `s3::init_view` (`src/view/s3.h:145`) puts the camera's pole at the attracting
   point of t (`W.plus.V`); with a zoom, `s3::window3` (`s3.h:164`) makes the window that culling and the focus ball use.
6. **The roots and the build.** `build<cs::RenderPolicy>` (`zipper3.cc:526`; `world.h:137`) asks the policy for the
   roots (`P::roots`, which calls `roots_info`, `cartwright_steger.h:205`: the elements of the lattice in B(R), nearest
   first), makes a `Renderer<B>` for Z⁺ and one for Z⁻, and calls `Renderer::draw` (`renderer.h:421`) for each root and
   each base arc.
7. **The renderer.** `draw` keeps a stack of branches; `visit` (`renderer.h:454`) maps a branch's ends by its element to
   the sphere (`B::to_double`, `B::act`, `B::boundary`), splits it into its roof's children while its chord exceeds the
   accuracy (the children's keys, `B::product_key` and `fingerprint`, keep a branch from being drawn twice), and
   otherwise emits the leaf (`emit`, 392) as a segment of a polyline. The output is the polylines of the renderer:
   points of S³ as floats, with the root and the letter of each.
8. **The polylines file, if asked** (`zipper3.cc:573`): `write_polylines` (the same file, 229) gives them to
   `polylines::write` (`src/output/polylines.h`).
9. **The painter.** `s3::write_png` (`zipper3.cc:607`; `src/view/s3.h:238`) projects every point (`s3::eye`, 130:
   stereographic projection from the pole, then the rotation of R³), fogs it by depth, and collects `raster::Segment`s;
   `raster::paint_segments` (`src/output/raster.h:99`) sorts them by depth and draws them at twice the size,
   `raster::downsample_rgb` halves it, and `png::write_rgb` (`src/output/png.h`) writes the file.

S⁴ is the same with `zipper5` (`apps/s4/zipper5.cc`, `main` at 249), `imm::RenderPolicy`, `s4::write_png5`
(`src/view/s4.h:198`) and `find_roots5`. In S² the program `apps/s2/zipper.cc` has its own renderer.

## Adding an example

An example is a lattice Γ in the isometries of a backend, with a homomorphism φ: Γ → Z and its kernel K. It is drawn
(`zipper3`, `zipper5`), found (`chz`, `chz5`) and checked. The model is Cartwright-Steger; every item below names where
it is done there.

### What an example supplies

1. **The group's data** (`src/examples/cartwright_steger.h`, namespace `cs`): `setup()` builds the generators as exact
   backend elements; `parse_word` reads a word of the files; `geogens(rho)` gives the generators of Γ's ball;
   `roots_info(R)` (and `find_roots`) lists the elements of Γ in B(R), nearest first, with their distances;
   `shell_radii` lists the radii at which the roots change; `focus_centre3` gives a lattice element near a point of the
   sphere, for the focus ball of a zoomed view. The group must act freely, so that an element's `B::key` determines it.
2. **A search policy** (`cartwright_steger_search.h`, `cs::SearchPolicy`): the class that the
   comment at the top of `src/search/kernel_search.h` lists member by member (the small ball, `pick_t`, `kernel_indices`,
   the K-store's labels, `write_file`, ...). `kernel_search` does the rest, and the program (`apps/s3/chz.cc`) only
   builds the policy and calls it.
3. **A render policy** (`cartwright_steger_render.h`, `cs::RenderPolicy`): `B`, the file `dialect`
   (`SubstitutionDialect`: the number of fields before a letter's word, and whether a `member` section is skipped),
   `word`, `sides` (the hook that sets the tips or the look-ahead; empty where none applies), `prepared` (the line
   printed) and `roots`. Its list is the comment at the top of `src/render/world.h`.
4. **The root-ball check** (`cartwright_steger_roots.h`, `stats_lines`): `--stats` makes the search of the roots again
   and compares it with the roots drawn, bit for bit; a check of the descent alone passes a search that skipped a step
   (see "The root balls" below).
5. **Substitution files** (`data/s3/`): the kernel search writes them; they are the example's data, with their
   hashes in sessions.

### Where it is registered

- **The programs.** The draw program (`apps/s3/zipper3.cc`) includes the policy and calls `prepare<cs::RenderPolicy>`
  and `build<cs::RenderPolicy>`; the search program calls `kernel_search`. The programs name their example's policies directly (`cs::RenderPolicy`
  in `zipper3.cc`), so a new example gets programs of its own, with a target in the CMake file of their folder
  (`apps/s3/CMakeLists.txt`). `link3` and the tools use the same policies (`apps/tools/*.cc`).
- **`zippers`** (`apps/cli/zippers.cc`): `find_example` (the names of the example), `program_of` (which program draws
  it and searches), the `OPTIONS` table (which options it takes, for which spheres; `program_of` and `OPTIONS` are
  keyed by sphere, not by example, so a second example on one sphere needs them keyed by example), `checks_of` (its exact checks, run
  by `zippers check`), `list` (its default files) and the usage texts. The same names are in
  `src/output/session.h`: `sphere_of_example` and the `KEYS` table (the session's keys and their defaults per sphere).
- **The data files** (`data/s3/`): the file, and its line in `data/README.md`; the default files are named in
  `apps/s3/zipper3.cc` (`DEFAULT_SUBST`), in the `KEYS` table of `src/output/session.h`, and in `zippers`' `list`.
  `data/s3` and `data/s4` are installed as folders (`CMakeLists.txt`): a new folder needs its own `install` line, and `find_data` takes the folder's name as its `sphere` argument
  (`src/core/data_files.h`).
- **The tests and the references** (below, "The tests to add"): `tests/reference/runs.json` (the run), and the same run in
  `tests/reference/programs_apps.json` (the program that makes it) and `tests/reference/zippers_runs.json` (its
  `zippers` command line, or the reason it has none); a session in `tests/sessions/`; the unit tests. These tests and
  documents name the examples too, and must learn a new one: `tests/unit/test_default_files.cc`, `test_zippers_cli.py`,
  `test_install.py`, `test_gui_session.cc` and `test_gui_jobs.cc`; `README.md`, `apps/README.md` and
  `docs/user_guide.md`.
- **A new header** of `src/` gets its line in `src/README.md`.

### What must be checked

The tests of the two existing examples compare the moved code with the legacy programs bit for bit (`test_search_cs.cc`,
`test_render_cs.cc`, `test_pu21.cc`, ...); a new example has no legacy original, so its checks are exact ones, stored
data and independent computations:
- `prepare` stops on a roof that fails the exact check (`make_side`) or on letters not closed under inversion.
- The backend satisfies `is_backend`, and its operations are tested against exact values (`test_pu21.cc`, `test_on1.cc`
  are the models, there against the legacy functions).
- The root balls: `--stats` makes the search of the roots again and checks it exactly, and the checks themselves are
  tested (`test_roots_cs.cc`, `test_roots_imm.cc`); the curves of a smaller R are the first polylines of those of a
  larger one (`test_shells.cc`).
- The search writes its stored files again, byte for byte on the reference machine (a reference run of `chz kernel`),
  and its certificate (every roof found, every generator in the group generated) is printed by the program.
- A picture: a reference run, byte for byte on the reference machine, with a `portable` result that does not depend on
  rounding (the count of letters, for instance); see "Building and testing" below. `zippers check` runs the exact checks.
- Where a word format can fail silently, a reader test (`test_reader_imm.cc` is the model: the word "1" is facet 1 and
  also the empty word of `chz5`).

## Adding a geometry

A geometry is a family of isometries with an action on a sphere at infinity S^{N-1} ⊂ R^N. It is a backend
(`src/geometry/backend.h`), and everything above it is templated, so the renderer, the store and the search need no
change. The models are `PU21` (`src/geometry/pu21/pu21.h`, S³) and `ON1<n>` (`src/geometry/on1/on1.h`, S⁴ for n = 5).

### What a backend provides

The comment at the top of `backend.h` lists it with the legacy function each member replaces; in short:
- **Types and constants:** `Elem` (an exact element, trivially copyable), `N`, `Point` (= `SpherePoint<N>`), `key_size`,
  `Key` (an `std::array<int64_t, key_size>` that determines the element: Γ acts freely, so the image of the base point
  does), `packed_size`, `DMat` and `DVec`.
- **Exact operations:** `identity`, `mul`, `inv`, `normal`, `eq`, `same`, `max_coefficient` (the growth guard),
  `key`, `product_key` (the key of a product, without the whole product), `displacement` and `key_displacement`
  (d(o, g o)), and `pack`, `unpack` and `packed_key` (the store's layout).
- **Double operations:** `to_double`, `act`, `boundary` (a null vector to its point of the sphere), `attracting` (the
  fixed point of a loxodromic element).

The header of the backend ends with `static_assert(zippers::is_backend<B>(), "...")` (as `pu21.h` and `on1.h` do);
each requirement that fails gives its own message. `is_backend` checks the shape, not the arithmetic: the exactness
and the agreement of `key`, `product_key` and the products is checked by the backend's test.

### What else a new sphere needs

- **A view and a painter.** `src/view/s3.h` and `s4.h` are the models: the camera (`ViewState`, `View5`: a frame and a
  pole, stereographic projection to R³ by `eye`/`eye5`), `init_view`, the window of a zoom (`window3`, `window5`: a
  `CullWindow<N>`), and `write_png`/`write_png5`, which project the renderer's polylines and give segments to
  `raster::paint_segments`. `s3::write_png` has `static_assert(B::N == 4, ...)`; `s4::write_png5` is for `ON1<5>`.
- **The tips** (`src/render/tips.h`) are for backends with parabolic elements: `find_tips` and `set_tips` take
  `Side<ON1<n>>`. A cocompact lattice needs none (`cs::RenderPolicy::sides` uses the look-ahead only).
- **The polyline file** (`src/output/polylines.h`) has the key `sphere N` for N = 2, 3 or 4, so a sphere of another
  dimension needs the format extended and its version raised.
- **The session** (`src/output/session.h`): `sphere_of_example`, `sphere_bit` and the per-sphere defaults of `KEYS`.
- **`zippers`:** the `Sphere` enum, `OPTIONS` and `program_of`.
- **The checks of the root balls and of the kernel search** are the example's, not the geometry's.

### What to test

`tests/unit/test_backend_contract.cc` checks `is_backend` with a mock; `test_pu21.cc` and `test_on1.cc` hold the real
backends bit for bit to their legacy originals (random elements, every operation); `test_sphere.cc` holds `chord` and
`segment_distance`. A new backend needs its own test of this kind, and, if it has a view, tests of the view and the
painter (`test_view_s3.cc`, `test_output_s3.cc`).

## The tests to add

`tests/README.md` has the table of tests, and says how to run them. For an example: a unit test of its policy against
what it replaces or an independent computation (`test_search_*`, `test_render_*`, `test_roots_*`), a reference run
(`runs.json`, with `programs_apps.json` and `zippers_runs.json`: `run_references.py --complete` fails if a run is
named by neither table), and a session in `tests/sessions/` (held by `test_gui_session`). For a geometry: the backend's
test, the view's test, and the contract test for the new shape. Register a unit test with `zippers_unit_test` in
`tests/unit/CMakeLists.txt`.

## Building and testing

- **Floating point.** The outputs of the programs are byte for byte reproducible only on the kind of machine where the
  reference runs were recorded: macOS on Apple silicon, with the recording's compiler (AppleClang 21.0.0.21000101).
  Elsewhere `tests/run_references.py` compares only the results that do not depend on rounding (`tests/README.md`).
  - After an update of Xcode the compiler changes, and the reference test falls back to portable mode, and says so.
    Run it with `--mode exact` to see what changed; if outputs differ, record the references again from a clean
    build of the same sources with the new compiler.
  - The version of macOS is not part of the mode's decision. After an update of macOS the test stays exact, and a
    changed system maths library could change outputs for reasons unrelated to the code. If exact mode then fails,
    build the sources the references were recorded from and compare them with `--mode exact` too: if they differ as
    well, the system changed, and the references must be recorded again.
- **An x86_64 Python builds x86_64 programs.** On a Mac with Apple silicon, a build started from a script that an
  x86_64 Python runs (for instance Intel Homebrew's, under Rosetta) compiles x86_64 programs, whose floating point
  differs in the last bits from the arm64 references. Build from the shell, or from an arm64 Python
  (`/usr/bin/python3`), or prefix the build with `arch -arm64`.
- **Tests that compare with a stored file.** On Linux the S³ kernel search chooses other words for the same roofs (in
  `chz kernel 4 9 1`, 28 cancelling junctions against 20 on the reference machine), so its file differs from the stored
  one. A fast test may compare a program's output with a STORED file byte for byte only under
  `#if defined(__APPLE__) && defined(__aarch64__)`, with a portable check in the `#else` branch (`test_roof_choice`
  does so); comparing two runs on the same machine is fine anywhere. Off the reference machine the reference harness
  does not compare an S³ kernel file's hash either.
- **Memory.** Four compiles at once take about 1.7 GB (`make JOBS=2` halves it). The heavy runs: the `link3` scan of the
  references (8.3 minutes, 0.46 GB); `strays` on S⁴ pictures at ACC 0.01 (870 s and 1.1 GB without the tips, 1,331 s and
  2.3 GB with them); `strays` on S³ at the first shell at ACC 0.01 (1.8 GB).
- **The S² film engine.** `zmovie --frames A:B` renders A to B inclusive; `zmovie -j N` prints its summary lines in a
  varying order (its frames are identical). A build outside the repository points the git-ignored link
  `legacy/s2/movie/bin` at itself; a `make` in the repository points it back.

## The renderer (`src/render/`)

### The two orientations of a branch

The renderer keys a branch (h, x) and its reversal (h·s_x, x⁻¹) alike, as one curve traced both ways (`branch_key`).
That is exact when the two are the same arc, and they need not be:
- For an ordinary letter, `make_side` builds the roof of x⁻¹ as the inverse word of the roof of x, so the two
  orientations agree level by level as long as no involution letter occurs below them.
- An involution letter c (s_c = s_c⁻¹) has one roof for both orientations. Unless that roof is its own inverse word
  (`self_inverse`, `src/search/roof_choice.h`), the two orientations of an involution arc are different arcs with the
  same ends. In S⁴'s `subst5_2.30_4.50_t1.txt` none of the 8 involution roofs is its own inverse word, and a vertex of
  α₀ (Z⁺) lies 0.317 from s₀·α₀. Ordinary letters inherit this wherever an involution occurs below them (letter 11:
  0.124 and 0.137).
- In `_t2` and `_t3` one involution of Z⁺ each has a self-inverse roof. In `subst5_2.30_4.50_t1_r0.txt` (the default
  of `zipper5`) every involution roof is self-inverse, so there the two orientations of every involution arc are the
  same arc (gap at most 3.3e-14, against 0.17 to 0.82 in `_t1`). In S³ (and S²) the two orientations coincide.

So the renderer draws whichever orientation it reaches first, and which branches are shared, and the order and the
direction of the points, depend on the drawing order. The picture loses nothing beyond 2 ACC (`overdraw --mode
missing`). Compare pictures, or decide whether two arcs touch, from the drawn curves, not from shared keys. Never mark
the other orientation as drawn: `--reverse` (of `strays` and `overdraw`) drops curve.

### The roofs that the renderer reads

The S⁴ files hold a roof for every letter, but `make_side` builds the roof of a letter that is not the representative
of its inverse pair as the inverse word of the representative's, and never reads the file's. In
`subst5_2.30_4.50_t1.txt` the two differ for 31 of 36 such letters in Z⁺ and 24 of 36 in Z⁻. Read the roofs from
`Side::roof`, never from the file (`apps/tools/roofs` checks both).

### Complete arcs in S⁴

Complete S⁴ arcs retrace themselves at every scale: many roofs begin and end with the involution 0, so `arc_points` on
Z⁻ at ACC 0.01 makes 87.7 million leaves where the picture has 294 thousand. Anything built on complete arcs (loops,
linking numbers) explodes in S⁴: measure first. `strays --dedup` measures arcs with each oriented branch once.

### Tips

In S⁴ a leaf deep in a parabolic chain is accepted on its chord while its curve runs out to the cusp: without the
tips the strays of the drawing are spikes into the cusps, whose length in units of ACC grows like ACC^(-1/2). With the
tips (`src/render/tips.h`: each leaf of a letter on a parabolic cycle of the roofs is drawn through the cusp points of
its cycles) the largest stray of the drawing is at most 1.64 ACC, and nothing strays beyond 2 ACC (`apps/tools/strays`,
on S⁴ `_t1` at R = 0 and ACC 0.04, 0.02 and 0.01). In S³ the strays are small and bounded (Π is cocompact: no
parabolic element, no tips).

The S⁴ group Γ = ker μ is not torsion free: it has index 16 in the right-angled Coxeter group W, and 8 of the 80
letters are involutions. So an element that fixes a null vector with eigenvalue 1 may be elliptic, not parabolic.
`tips.h` excludes elliptic elements directly: P = (G − 2)²e₆ lies in im(g − 1), and g² is used for the screw
parabolics.

### The look-ahead through one-letter roofs (S³)

At a letter whose roof is one letter the first split has no inner vertex, so the one-level look-ahead tests nothing;
the look-ahead (b) (`through_split`) looks through the chain of one-letter roofs. Four letters of each side of the
stored S³ files have a one-letter roof. It changes no S³ reference run (at ACC 0.01 on `subst_4.00_9.50_t3.txt` it
never splits a branch), but `zipper3 -R 2.554 -a 0.04` draws 72,167 leaves of Z⁺ with it and 72,169 without. No test
on the stored files reaches a chain of one-letter roofs of two steps; `test_cull` builds one of three steps by hand.

### Marking and overdraw

Distinct branches overlap where consecutive roofs cancel (the arc goes out along a piece and back along it), and
there the ink is drawn twice: the keys cannot remove it. `apps/tools/overdraw` measures it, by keys and without them:
on the pictures of the stored files it is 21 to 23% in S³ Z⁺, 13 to 15% in Z⁻, 15 to 18% in S⁴ Z⁺ and 65 to 72% in
S⁴ Z⁻, mostly backtracking at cancelling junctions.

Marking (`--overlap L`) is a measured trade, not a proved one (there are no enclosures in S³ and S⁴): at L = 1 it
saves 1 to 2 points of overdraw (6 in S⁴ Z⁻) and 1 to 5% of the leaves, for 1.2 to 3.5 times the time and up to 4.5
times the memory. What holds is a subset BY KEYS: every leaf drawn with marking has the key of a leaf drawn without
it. Not always the same leaf bit for bit: on the S³ file `_t1`, 55 to 82 leaves of 400 to 600 thousand are drawn from
their other end (the same key, the same floats in reverse order), and on `_t3` at ACC 0.5, 16 of 1641 leaves at
R = 2.554.

The roofs chosen by the kernel search's `--roofs` remove most of the overdraw (below, "The roofs of `data/`").

### Culling

Culling (`--cull`, on by default for a zoomed still) drops a branch when all its points lie more than K times its size
beyond one face of the window. The window is the screen square and the clip ball pulled back exactly, as five
half-spaces of R^N; it is a tube through the depth, so culling saves about zoom², not more. There are no enclosures:
the margin is a measured bet, not a bound.
- **The strays, and K = 16** (`CULL_K`). Measured with `cull --mode margin` on the default files, among the branches
  that K = 16 can cull (size below 1/16): at most 3.3 sizes on `subst_4.00_9.50_t3_r1.txt` up to R = 4.5 (a safety
  factor of 4.82 or more), and 1.4 sizes on `subst5_2.30_4.50_t1_r0.txt` up to R = 2.1 (11 or more). K = 8 leaves only
  1.26 on `_t3_r1` at R = 3.99 and 4.5, where whole base arcs of far roots stray 6.3 sizes. Larger R is unmeasured,
  and no window was built to find a loss.
- **S³ `_t1` needs `--cullk 32`:** its branches stray up to 8.9 sizes (its roofs make arcs whose first-split vertices
  do not represent the curve), and K = 6 already loses segments there. Its picture strays too, independently of
  culling: leaf strays up to 4.94 ACC (Z⁺) and 3.17 ACC (Z⁻), against 1.37 and 1.30 for `_t3`.
- **A branch's size must count its tips,** or S⁴'s parabolic chains stray more and more as ACC shrinks: 1.6, 2.5 and
  3.3 sizes at ACC 0.04, 0.02 and 0.01 without them, at most 1.13 with them (Z⁻ of `_t1`).
- **What it saves.** In every comparison made (zooms 1 to 1000, both geometries, R up to 2.554, accuracies down to
  0.001 on the sphere, with marking, the focus ball and forced leaves; 24 million segments of windows) the segments
  drawn in the window are the same with and without culling. At zoom 100 at the same accuracy, culling with K = 8
  draws 76 times fewer leaves in S³ and 10 times fewer at the S⁴ cusp e₅; K = 16 keeps 1.06 to 1.8 times as many leaves
  as K = 8 in S³, and 1.2 to 3.8 times as many in S⁴, for a still at the plain accuracy.
- **The focus ball** (`--focus`, on by default for a zoomed still) is h·B(R), with h an element of the lattice. At a
  cusp that is a vertex of P5 (e₅ and the others) it is empty: the orbit point nearest the focus point is o itself, at
  every zoom. At another cusp the geodesic towards it ends in the cusp of one tile, whose orbit points stay at a
  bounded height, so d(h·o, o_F) grows like L and the ball is far from the view. Cusp pictures come from culling with
  `--zoomacc`.
- **Comparing pictures.** `test_cull_programs` compares culled and unculled pictures byte for byte. The painter's sort
  is not stable, so this holds only because the order came out the same in the cases tested. In S⁴ with the focus ball
  it fails (tens to a couple of hundred pixels, the same segments), and the tests avoid those cases. Elsewhere, if it
  fails, compare the segments (`cull --mode window`).

## The examples (`src/examples/`)

### The S⁴ word reader

`chz5` writes "1" for the empty word, and "1" is also facet 1. The legacy reader (`parse_fword` of
`legacy/s4/zipcore5.h`) skips every token "1", so it drops facet 1 from every word: it reads 19 of the 80 generators
of every stored file, and t of the `_t2` and `_t3` files, as their images under the homomorphism of W that kills r₁,
and draws 47 base arcs where the data have 44. `imm::parse_fword` reads facet 1; `legacy/` keeps the old reader as
the record of what the legacy programs drew. `test_reader_imm` holds the reader to `chz5`'s data.

Two bugs of the renderer would be caught only by `test_render_imm`'s comparison with the legacy `Renderer5` (given the
legacy reader's letters): the S⁴ reference pictures do not see them.

### The root balls

`zipper3 --stats`, `link3 --stats` and `zipper5 --stats` make the search of the roots again and check it exactly
(`cartwright_steger_roots.h`, the end of `imm_m5.h`). A check of the descent alone (every element but 1 has a strictly
nearer neighbour) passes a search that skipped a step; the checks test the closure of the store under the steps inside
the ball as well.

## The roofs of `data/`

The kernel search's `--roofs` chooses the roofs of the file's t (`src/search/roof_choice.h`): self-inverse roofs for
the involution letters, then a greedy pass that lowers the cancelling junctions. The stored files of the legacy
programs have, among the junctions of their roofs, 58 of 160 and 42 of 146 cancelling (S³ `_t3`, up and down), and
96 of 468 and 248 of 423 (S⁴ `_t1`; 225 of them at an involution letter); no involution roof of the S⁴ file is
self-inverse. What the files of `data/` change:
- **S⁴ `subst5_2.30_4.50_t1_r0.txt`** (the same t as `_t1`, the roofs at no extra length): overdraw 15/79% becomes
  10/5% at ACC 0.02 (Z⁺/Z⁻), 20% fewer leaves, and the two orientations of every involution arc are the same arc.
- **S³ `subst_4.00_9.50_t3_r1.txt`** (one letter of excess): overdraw 21/14% becomes 10/0.4%, but the strays of Z⁻ reach
  1.94 sizes, and there are 3 to 15% more leaves. `_t3_r0`: 15% fewer leaves, overdraw 23% in Z⁺ and 4% in Z⁻.
  `_t1_r0` brings the strays of `_t1` from 8.9 sizes down to 2.3.
- **Without involution letters** (`--noinv`) the certificate fails at c = 2.3. The facet counts mod 2, a homomorphism
  W → (Z/2)¹⁶, of the other 72 letters span no involution letter's (`test_roof_choice` checks both).
