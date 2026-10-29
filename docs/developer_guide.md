# Developer guide

For those who change the code: what has been measured, and the traps. Each section names the code it is about; the
comments of the code point here where a fact is too long for them. The user's side of the programs is in the
READMEs of `apps/`, `legacy/` and `film/`, and the tests are described in `tests/README.md`.

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
