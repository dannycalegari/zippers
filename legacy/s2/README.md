# zipper

Draws the **zippers** `Z⁺` (blue) and `Z⁻` (red) on the sphere at infinity `S²∞` of a
hyperbolic 3-orbifold `M` that fibers over the circle, with fiber a torus with one cone
point of order `n` (for `n = ∞` a punctured torus, and `M` is cusped). Zippers are defined
in Danny Calegari and Ino Loukidou, *Zippers*,
[arXiv:2411.15610](https://arxiv.org/abs/2411.15610): two disjoint, path-connected,
`π₁`-invariant subsets of `S²∞` carrying the stable and unstable laminations. The picture
shows the front hemisphere, viewed orthographically; the black circle is the equator. With
`-C` it also draws a **Catherine wheel**: one circle separating the drawn `Z⁺` from the
drawn `Z⁻`.

The original `zcircle` took 3 minutes and wrote 384 MB for `fig8`, order 2, accuracy 0.004;
this program takes 0.25 s and writes 0.5 MB at `-R 5`.

## Build

```bash
make            # ./zipper: EPS and PNG output, and the interactive viewer
make nogui      # the same without the viewer (needs only a C++17 compiler and zlib)
```

The viewer uses the macOS GLUT and OpenGL frameworks. If the compiler cannot find the C++
headers (a half-installed Xcode update), the Makefile falls back to the Command Line Tools
`clang++`. `make examples` writes a few pictures; `make zcircle` builds the original.

## Usage

```bash
./zipper [options] -o out.eps     # EPS (to stdout without -o)
./zipper [options] -o out.png     # PNG (the format follows the extension, or -f)
./zipper [options] -v             # interactive window
```

| option | meaning | default |
|---|---|---|
| `-m NAME` | monodromy: `fig8`, or a positive word in `R`, `L` such as `RRL` | `fig8` |
| `-n N` | cone order: an integer `2..10⁶`, or `inf` (or `0`) for the cusp | `2` |
| `-a ACC` | accuracy: each chord is at most `ACC` long and within `ACC` of its curve | `0.004` |
| `-R RADIUS` | radius of the hyperbolic ball of group elements drawn (denser when larger) | `6` |
| `-s SIZE` | EPS points (`16..100000`) or PNG pixels (`16..16384`, about 7 bytes each) | `512` |
| `-w WIDTH` | line width in points (`0`: the thinnest line) | `0` |
| `--yaw D`, `--pitch D` | rotate the view (degrees) | `0` |
| `--zoom F` | magnify the view about its centre, `0 < F ≤ 10⁷`, adding detail | `1` |
| `-C` | also draw the Catherine wheel (green) | off |
| `-f FMT` | `eps` or `png` | from `-o` |
| `-o FILE` | output file | stdout |
| `-v` | open the viewer | off |

Less common: `--overlap L` (`0..16`, default 1, see *Overlaps*), `--wheelres N` (finest wheel
mesh edge `π/N`, default `10⁷`), `--stats` (diagnostics and exactness checks), `--timing`,
and, for testing, `--nofocus`, `--nocull`, `--nochains`; `-e TOL` is ignored. A proper
power such as `RLRL = (RL)²` draws its root (same zippers); `RL` is `fig8`.

A radius whose ball would exceed 16 million elements (about `-R 8.16` for `fig8 -n 2`) is
refused, with the largest radius allowed for that monodromy and order. So is `-C` beyond
650 000 elements (about `-R 6.55` for `fig8 -n 2`, `-R 6.75` for `-n inf`), or 55 000
(about `-R 5.5`) for `60 ≤ n ≲ 3100`, where the zippers come very close at many points.
Drawing the zippers stops at a memory limit of 4 GB (6 GB with `--stats`). The wheel's mesh
has its own budget of 3.5 GB, so `-C` runs can peak at about 4.3 GB. Exit status: 0
success, 1 error, 2 a false merge found by `--stats`, 3 the picture was written but is
incomplete (the memory limit).

```bash
./zipper -m fig8 -n 2 -o fig8_n2.eps              # the paper's example
./zipper -m fig8 -n inf -s 1200 -o cusp.png       # the cusped figure-8 knot complement
./zipper -m RRL -n 3 -o rrl.eps                   # another monodromy and order
./zipper -n 2 -a 0.001 -R 7 -o fine.eps           # finer and denser
./zipper -n inf -a 0.0005 --yaw 42 --zoom 6 -s 900 -o petal.png   # zoom on a cusp point
./zipper -n 2 -R 5 -C -o wheel.png                # with the Catherine wheel
```

## The viewer (`-v`)

* arrow keys: rotate, about axes fixed on the screen;
* `+` / `-`: change `R` by 0.5 and recompute (refused beyond the limits above);
* `z` / `x`: zoom in or out by 1.5, from 0.1 to `10⁷`. Beyond `10⁵` the curves show
  single-precision steps, in the window and in the export alike;
* `c`: zippers → zippers and wheel → wheel → zippers (`-C` starts with the wheel);
* `p`: save the view as a PNG framed like the window, named after `-o` (else `zipper.png`),
  never overwriting a file (`STEM_1.png`, …). It keeps the window's `-s` (at least 1024
  pixels) and `-a`, so when zoomed in, long branches are straight chords. `p` also prints a
  command that recomputes the view with `-a` divided by the zoom (down to `10⁻⁶`). It may
  use the hidden `--roll D`, which turns the picture in its plane;
* `q` or `Esc`: quit.

## How it works

**Holonomy.** A positive word in `R: (a,b) ↦ (a, ba)` and `L: (a,b) ↦ (ab, b)` is the
monodromy `φ` (`fig8 = RL`). The fiber character `(tr a, tr b, tr ab)` is a fixed point of
the trace map of `φ` on the level set `tr[a,b] = −2cos(π/n)`. It is found at the cusp
(`fig8`: `(3 ± i√3)/2`) and continued in `n` by Newton's method. `C = ρ(c)` solves
`C ρ(g) C⁻¹ = ρ(φ(g))`. A cheap screen (loxodromic generators, the orbit count of a small
ball) rejects plainly non-discrete solutions.

**Orientation.** A character and its complex conjugate give mirror-image orbifolds. At the
cusp `P = ρ([a,b])` and `C` are commuting parabolics, translations by `t_P` and `t_C` once
their fixed point is at `∞`. The convention is `Im(t_C/t_P) < 0`, inherited by continuation;
for `n ≥ 3`, with `C` conjugated to `z ↦ λ²z`, `|λ| > 1`, it says that `P` rotates by
`+2π/n`. This is the chirality of the original program and the paper (`fig8 -n 2`:
`tr a = 1.780776+0.624811i`). `fig8ref` draws the original program's matrices.

**Branches with exact identity.** Each zipper is the union of the translates `g·α_y` of two
arcs `α_a`, `α_b`, where `α_y` runs from the attracting fixed point `V` of `c⁻¹` (of `c` for
`Z⁻`) to `y·V` and is defined by self-similarity: `α_y = c⁻¹·α_{φ(y)}`. These arcs are the
branches of the lifted train track. Splitting the branch `(h, y)` along `φ(y) = x₁⋯x_k`
gives the consecutive branches `(h c⁻¹ x₁⋯x_{i−1}, x_i)`. A branch is identified by the pair
*(group element, letter)*, never by its geometry. The element is keyed by the image of a
generic point `o′` of `H³`, rounded to hyperbolic boxes of size `10⁻²`. Distinct orbit
points are much farther apart: the least `d(o′, g·o′)`, `g ≠ 1`, is 0.79–0.99 for `fig8`
and `RRL`, and 0.15–0.39 for thin words such as `R¹⁰L` and `R¹⁴L`, always at least 15
boxes. A branch met again (inside a larger branch drawn earlier) is skipped, so each branch
is drawn once, and its leaves come out in order, which makes the polylines.

**The ball.** An exactly invariant finite picture is impossible, so the roots are the group
elements with `d(o, g·o) ≤ R` (`cosh d = ‖ρ(g)‖²/2`), drawn nearest first. The orbit
equidistributes for the Patterson–Sullivan measure, which is round, so the density is even.
The ball is found **completely**. A search through `a, b, c` inside it would miss up to
about 19% of `B(6)` for `fig8` (at `n = 6`), more for thin words. The search uses a set `S`
of 10–36 elements with the *descent property*: every `g` outside a seed ball `B(o, K)` has a
neighbour `gs` (`s ∈ S`) nearer to `o`, so a best-first search need only follow steps that
do not decrease the distance. By the hyperbolic law of cosines, `gs` is nearer than `g`
exactly when the direction of `g⁻¹·o` lies within `arccos(tanh(ℓ_s/2) coth d)` of the
direction of `s·o` (`ℓ_s = d(o, s·o)`, `d = d(o, g·o)`). These caps grow with `d`, so if
they cover the sphere of directions at `d = K`, the property holds for every `d ≥ K`. This
is checked on a grid of 21 600 cells; `K` starts at 3 and grows by 0.5 while cells stay
uncovered. Directions within `0.04` of a cusp point (for `n ≥ 12`, a near-cusp point)
cannot be covered: there the nearer neighbour is a (nearly) parabolic step whose gain tends
to zero. These cells are tolerated, so there completeness is checked, not proved. In all
106 balls tested (`R` up to 6.5, zoom up to `10⁶`) the roots are exactly the ball found by
an independent search with geometric deduplication.

**The stopping rule and its guarantee.** A branch `(g, x)` is a leaf when its chord on `S²`
is at most `ACC` **and** its curve provably lies within `ACC` of the chord. For the second,
each base arc `α_x` is enclosed once in a few round discs `D`. The curve of `(g, x)` lies in
the cap `g·D`, computed exactly, and the farthest point of that cap from the chord bounds the
distance. One disc is not enough: along a chain `Qᵏ(α)` shrinking to a fixed point `p` of
`Q` (parabolic at the cusp), the images `Qᵏ(D)` of a disc around `p` do not shrink. So each
chain gets a region adapted to it: with `p` at `∞` and `Q(z) = μz + β`, a half-plane that
`Q` maps into itself (`μ` real), a few rotated half-planes (a spiral), or a disc avoiding
the repelling fixed point. So every chord is within `ACC` of its curve and vice versa.
`--stats` checks this on refined curves: at most `1.01×` the accuracy, except `1.11×` in
the zoom-300 cusp crop, where rounding in long products matters.

**Cusps and spikes.** Chains into a parabolic fixed point shrink only like `1/depth`. One
kind is the chain of first (or last) sub-branches. The other is a spike: a middle child of
a branch `x` is again `x`, mapped to it by some `Q`, and repeating this gives a chain that
ends at the fixed point of `Q`. These chains are walked directly, one product per level,
and keyed as ordinary splitting would key them. A run of tiny side branches is drawn as one
chord when all of them provably lie within `ACC/2` of it.

**Overlaps.** A train path can lie inside another branch without being its sub-branch, so a
leaf may overlap finer pieces of the same curve from another root. `--overlap L` marks the
sub-branches up to `L` levels below a leaf as drawn when they provably lie within `ACC` of its
chord. The remaining overdraw is invisible: 6% of the length at `L = 1`, 2% at `L = 2` (for
1.5× the time).

**Zoom.** Only branches whose cap meets the visible square are split, and the ball is also
taken about a point pushed towards the view by `log(zoom)` (at most `R − 1`), so the magnified
region keeps its density. That ball is complete too (descent works about any centre). `-a`
is measured on the sphere, so use about `-a 0.004/F` with `--zoom F` to add detail.

**The Catherine wheel** is the set `d(·, Z⁻) = d(·, Z⁺)` for the drawn zippers, found on a
triangulation of the sphere refined where they come close. A piece of a zipper cut off from
its side is joined to it by a *bridge* (a path away from the other zipper), and stray
pockets are merged, so the result is one closed curve. Each segment lies in its own mesh
triangle, so the curve is embedded (up to single-precision rounding). The drawn zippers
meet only where they come closer than `ACC` resolves (finite `n`) or at parabolic points
(`n = ∞`); there no circle can avoid them, and a note says so.

## Costs

Apple M1, `-s 512` EPS. Time, peak memory (footprint; the maximum RSS is 10–17% more), file
size.

| run | time | memory | EPS |
|---|---|---|---|
| `-n 2` (defaults) / `-n 3` / `-n 30` / `-n inf` | 0.8 / 0.7 / 1.0 / 1.0 s | 60 / 55 / 80 / 80 MB | 2.4 / 2.1 / 3.0 / 3.2 MB |
| `-n 2 -R 5` | 0.25 s | 27 MB | 0.5 MB |
| `-m RRL -n 3` / `-m RRL -n 30` / `-m R¹⁰L -n 5` | 1.0 / 1.4 / 3.9 s | 70 / 105 / 360 MB | 1.9 / 2.6 / 2.0 MB |
| `-n 2 -a 0.001 -R 7` | 7.4 s | 0.5 GB | 19 MB |
| `-n inf -a 0.001` / `-a 2e-4` | 4.4 / 32 s | 0.3 / 2.4 GB | 11 / 65 MB |
| `-n 2 -C` / `-n 4 -C` / `-n inf -C` | 4.7 / 9.3 / 5.7 s | 0.9 / 1.4 / 0.7 GB | 3.1 / 3.2 / 3.8 MB |
| cusp crop `--zoom 40 -a 1e-4` | 5 s | 0.4 GB | 5.6 MB |
| the same at `--zoom 100 -a 4e-5` / `--zoom 300 -a 1.3e-5` | 15 / 46 s | 1.3 / 2.5 GB | 3.1 / 8.7 MB |

The number of leaves grows like `e^{2R}` and `1/ACC` asymptotically. The time grows more
slowly at these sizes: for `fig8 -n 2` it is 0.16 / 0.24 / 0.80 / 4.4 s at `R = 4 / 5 / 6
/ 7`, and at `R = 6` each halving of `-a` costs 1.3–1.6×. A zoomed view costs about as much
as an unzoomed one, except near cusps: there, with `-a` scaled with the zoom, it grows about
like the zoom (a spike of size `s` needs about `s/ACC` levels).

## Limitations

* For about one R/L word in five (9 of 45 cyclic classes tested, e.g. `LLRRLRR`) the solver
  finds no geometric cusped character, and says so (after up to a minute). Seeding it from
  the layered triangulation would fix this.
* Next to the cusp points completeness of the root ball is checked, not proved. For `R¹⁰L`
  and `R¹²L` at `n = 11` a few cells stay uncovered (`K = 5.5`, `6`), and beyond `R = K + 0.3`
  the program warns that a few roots may be missing (none were missing at `R = 6`).
* Points are stored in single precision: above zoom `10⁵` the curves show steps.
* Deep cusp crops are dominated by leaves below float size, which add no point but cost time
  and memory, up to the memory limit (exit status 3).
* The wheel's mesh has a budget (24 million vertices or 3.5 GB). Where the zippers come close
  at very many points (thin words such as `R¹⁰L`, `-n inf` beyond about `-R 6.4`) it is
  reached, and the wheel then crosses the zippers in places, with a warning.
* The wheel's points are single precision too. Where the zippers come within about `10⁻⁷`,
  a few points can coincide, and the wheel can cross itself locally at that scale. For
  `fig8 -C` this happens at most 2–3 times at `n = 3, 10, 30, 50, 59`; at `n = 3141`,
  `-R 5.5`, there are 7 repeated points and 9 crossings.
* `-m RL` draws the `fig8` picture up to sub-pixel differences (byte for byte at most
  orders). `-n inf` PNGs differ in a few pixels between compiler flags.

## Technical notes

* *Root ball.* `S` is chosen greedily from `B(o, K)` and the powers `h Pᵏ h⁻¹`,
  `0 < |k| ≤ min(n − 1, 100)`, for the `h` that make `h P h⁻¹` move `o` least (for
  `n ≈ 12..50` the hard directions lie along its axis). `K` is 3–4.5 in most tests, 5–6 for
  the thinnest words (`R¹⁴L -n 5`; `R¹⁰L`, `R¹²L` at `n = 11`). The seeds come from a plain
  search through `B(o, K + 2)` (a test needed a margin of at most 1.67). If cells stay
  uncovered, that search's elements without a nearer `S`-neighbour become seeds too, which
  keeps the search complete up to about `R = K + 0.3`. Ball sizes are estimated from
  `B(o, max(5, K + 1))`, about 15% high.
* *Keys.* Two routes to the same branch give key points about `10⁻¹¹` apart at the defaults,
  `10⁻⁵` at `-a 0.001`, and up to `10⁻²` for branches below float size deep in cusp crops
  (long products). The last can give one branch two keys, so it is drawn twice; distinct
  branches never share a key.
* *`--stats`* prints counts and two checks. A *false merge* (exit status 2) is a key met
  again with endpoints more than about 1% of the chord apart. There were none in any test:
  two routes to one branch agree to `10⁻¹³` at ordinary views, to 0.2% deep in a cusp crop.
  *Geometric duplicates* are identical leaf segments, from branches with two keys; they are
  harmless. There are none at the defaults, 4 + 3 in the zoom-100 cusp crop, 184 + 171 at
  zoom 300, and hundreds deeper (757 + 201 at the `RRL` tip at zoom 1000). Both checks skip
  chords below `10⁻⁷` (none at the defaults, 24 million in the zoom-100 crop). `--stats`
  also reports the largest stray of a leaf refined to `ACC/16`, the overdraw and whether
  descent is certified. It costs 2–3× the memory and 2–4× the time.
* *Memory limit.* The key tables check the process's memory before each new block (4 MB or
  more), the point arrays every 65 536 steps. Peaks at the 4 GB limit: 3.99–4.00 GB in deep
  cusp crops. The hidden `--memlimit GB` changes the limit, for testing.
* *Wheel.* Segments count as great-circle arcs. A triangle is split while a zipper can enter
  it or a hidden sign change is possible (`d(·,Z⁻) − d(·,Z⁺)` is 2-Lipschitz), down to `π/N`.
  Bridge searches stop after 200 million mesh edges or 10 million clearance queries. At
  `n = ∞` a crossing point stays at least 2% of its edge from the edge's ends (a shift of
  at most `1.3·10⁻³`); otherwise, where the zippers touch, several can fall on one vertex
  (1245 self-crossings for `fig8`). Independent checks found one closed curve with each
  zipper on its own side (finite `n`), and no self-crossing (`fig8`, `RRL` at `n = ∞`,
  `R = 4, 5, 6`).
* *Monodromies.* Named ones are in `monodromy_table()` in `zipper.cc`. For an R/L word the
  cusped character is the irreducible solution with a nondegenerate cusp that passes the
  screen and has the largest total imaginary part (ties: the orientation convention).
