# complex hyperbolic zipper

Draws approximations to the zippers Z⁺ (blue) and Z⁻ (red) on the 3-sphere at infinity of the
**Cartwright–Steger surface** X = Π\B². The homomorphism is φ: Π → ℤ, the first coordinate of
H₁(Π) = ℤ². Its kernel is finitely generated, and it comes from a circle-valued Morse function
whose critical points (three of them) all have index 2.

The zippers are those of Calegari–Loukidou, *Zippers* (arXiv:2411.15610), constructed by the
staircase/block-filling method of §4. For the group and the homomorphism see
[README_for_distribution.md](README_for_distribution.md); the group's data are in `csgroup.h` (the
generators of Cartwright–Koziarz–Yeung over ℤ[ζ₁₂]) and the conventions at the top of `chz.cc`.

## Build

```bash
make
```

This builds `chz`, the block-filling search, and `zipper3`, the renderer and viewer. It needs a
C++17 compiler, zlib, and the macOS GLUT/OpenGL frameworks. `make nogui` builds `zipper3`
without the viewer.

## Run

```bash
./zipper3 -v
```

This opens the interactive viewer, starting with the 18 base arcs of each zipper.

```bash
./zipper3 -R 3 -a 0.005 -o picture.png
```

This writes a PNG instead. The PNG is depth-cued: far lines fade toward white.

| option | meaning | default |
|---|---|---|
| `-R R` | roots: the translates g·α for d(o, g·o) ≤ R. Shells of new translates start at 2.553, 3.326, 3.983, … | 0 |
| `-a ACC` | accuracy: maximal chord on the unit S³ | 0.01 |
| `--only +` / `--only -` | draw only Z⁺ or only Z⁻ | both |
| `--clip RHO` | clip the stereographic picture to the ball of radius RHO | 3 |
| `--zoom F`, `--yaw D`, `--pitch D` | view | 1, 0, 20 |
| `--pole D1 D2` | move the pole of the stereographic projection (degrees) | 0 0 |
| `--check` | print the smallest distance between the drawn Z⁺ and Z⁻ (it must stay > 0) | |
| `-f FILE` | substitution data from `chz kernel` | `subst_4.00_9.50_t3.txt` |
| `-s SIZE` | image or window size | 900 |

**Viewer controls**

- **mouse drag:** rotate ℝ³.
- **arrow keys, `w`/`s`:** move the pole of the projection. This rotates S³ itself, so the picture
  changes non-rigidly as you fly through it.
- **`z`/`x`:** zoom in / out.
- **`+`/`-`:** add / remove the next shell of translates. This rebuilds.
- **`]`/`[`:** finer / coarser accuracy. This rebuilds.
- **`c`:** cycle Z⁺ / Z⁻ / both.
- **`b`:** change the clip radius.
- **`r`:** reset the view.
- **`p`:** save a PNG of the window.
- **`q`:** quit.

**Linked pairs.**

```bash
./zipper3 -v --pairs linked_pairs.txt
```

This shows the verified linked pairs of loops from `link3` instead of the zippers:
- a loop γ_w ⊂ Z⁺ (blue) and a loop h·δ_{w′} ⊂ Z⁻ (red);
- `n`/`N` step through the pairs, and `l` shows the base arcs faintly behind them;
- the terminal prints each pair's linking number and relations.

With `-o FILE.png --pairindex K` it writes a picture of pair K instead.

**Cost.** At accuracy 0.01:

| R | roots | build time | leaves per zipper | memory |
|---|---|---|---|---|
| 0 | 1 | 0.3 s | 0.09M | |
| 3 | 19 | 1 s | 0.6M | |
| 4 | 331 | 6 s | 3.5–3.9M | 0.7 GB |

The leaf count grows by about 3× per halving of the accuracy.

## The picture

S³ is the unit sphere of ℂ², the visual sphere of the basepoint o, the fixed point of the
order-288 stabiliser in Cartwright–Steger's lattice. It is shown by stereographic projection.

By default t⁺ is at the centre. The contact plane at t⁺ is horizontal, and its Reeb direction
(i·t⁺) is vertical. The fixed points t⁺ and t⁻ are marked. Near t⁺, t contracts the Reeb
direction like e^(−nℓ) but the contact plane only like e^(−nℓ/2), with a rotation. So pieces of
arc near t⁺ are flattened into the contact plane, which gives the fans of "needles" there.

## Linking numbers (`link3`)

```bash
./link3 selftest
```

This tests the linking-number code against the Gauss integral on known links.

```bash
./link3 -a 0.01 --nrel 20 --hr 2.6
```

This is the scan:
- it finds short relations w in K exactly;
- it builds the loops γ_w ⊂ Z⁺ and h·δ_{w′} ⊂ Z⁻ for h ∈ B(2.6);
- it computes all their linking numbers;
- it re-verifies the nonzero ones at finer accuracy, in more projections, and under translation.

```bash
./link3 --nrel 20 --hr 2.6 --export link_run2.txt
```

This writes the verified pairs to `linked_pairs.txt`.

`--pair A B H` computes one pair in many projections, `--hinfo` lists the translates h with
φ(h) and their words, and `link3` with no arguments lists all options.

## How it works

1. **The roofs.** `chz kernel 4 9.5 3` finds the roofs, i.e. the substitution t⁻¹st = x₁⋯x_k.
   - S is the 36 kernel elements of displacement ≤ 4.
   - t has φ(t) = 3.
   - The search is exact, in ℤ[ζ₁₂], and comes with a certificate that S generates ker φ.
2. **The arcs.** The arc α_s of Z⁺ from t⁺ to s·t⁺ is self-similar:

   α_s = t·(α_{x₁} ∪ x₁α_{x₂} ∪ … ∪ x₁⋯x_{k−1}α_{x_k}).

   - A branch (h, s) is split into (h·t·x₁⋯x_{i−1}, x_i).
   - For s⁻¹ the roof is the inverse word of the roof of s. So a branch and its reversal have
     one canonical exact key, and each branch is drawn once.
   - Z⁻ is the same with t⁻¹ and the down-roofs.
3. **Splitting.** As in the S² program: split until the chord is below the accuracy, with a
   one-level look-ahead for spikes. The roots are taken nearest first.
