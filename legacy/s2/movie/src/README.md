# The frame engine of the zipper film

Tools for the 2D half of the zipper film: `zmovie` renders shots of zippers, Catherine wheels
and base arcs to PNG frames; `caption.py` puts captions on them; `zmovie --assemble` joins
shots with crossfades; the Swift tools encode the frames and add the music.

The drawing is the zipper program's own. `zmovie.cc` `#include`s `zipper_snapshot.cc`, an
unmodified copy of `../../zipper.cc` (`main` renamed by `#define main zipper_main`), and calls
its functions. `make check-snapshot` says whether the copy still equals the program's source.
Neither `zipper.cc` nor the program's binary is touched.

| file | what |
|---|---|
| `zmovie.cc` | the frame engine (C++) |
| `ztest.cc` | checks of the engine against the program |
| `zipper_snapshot.cc` | the copy of `zipper.cc` |
| `caption.py` | captions in the 3D film's style, with fades |
| `cut_audio.py` | cut a clip from a WAV, with fades (for tests) |
| `encode_movie.swift`, `add_audio.swift` | the encoder and the soundtrack tool (in this repository: `film/swift/`) |
| `probe_movie.swift` | what a film holds: tracks, frame counts, time ranges, edit lists |
| `grab_frames.swift` | save a film's frames at given times, to look at them |
| `Makefile` | builds everything into `../bin` |

## Build

```bash
cd movie/src && make            # bin/zmovie, ztest, encode_movie, add_audio, probe_movie, grab_frames
make check-snapshot             # is zipper_snapshot.cc still the program's zipper.cc?
```

C++ is built with the Command Line Tools' `clang++` and SDK (`-std=c++17 -O3 -Wall -Wextra`),
Swift with `swiftc -sdk .../MacOSX26.5.sdk` (the default SDK does not match the compiler).
Python needs `numpy` and `PIL`.

## zmovie

```bash
zmovie SHOT.txt [key=value ...] [-o DIR] [-j N] [--frames A:B] [--quiet]
zmovie --assemble TIMELINE.txt OUTDIR
```

* `-o DIR`: the frame directory (else the shot's `out`). Frames are `frame_NNNNN.png`
  (`prefix`, numbered from `first`).
* `-j N`: N processes. A static scene is built once and then forked (shared copy-on-write,
  frames interleaved); a dynamic one (a T-morph or `build = perframe`) is forked first, and
  each process takes a contiguous block of frames, so that builds can be reused.
* `--frames A:B`: only these frames.
* `key=value`: overrides a key of the shot file, e.g. `frames=240` or `timing=other.txt`.

### How a shot works

The scene is built with the program's own code: the holonomy (`solve_character`,
`build_rep_from_char`), the framing (`conformal_center`), the root ball (`make_ball_gens`,
`enumerate_ball`), and `Renderer::setup` and `Renderer::draw_arc` for each root in the ball's
order, exactly as `Renderer::draw_roots` calls it. Two things differ:

* **Tags.** Each polyline is tagged with its root's distance `d(o, g.o)` (the ball's sort
  key) and its arc (root, letter). A polyline never spans two roots (`open` is reset before
  each `draw_arc`).
* **No joining.** `draw_roots` ends with `join_polylines`, which only concatenates polylines
  that meet; it is skipped (it would mix the roots' polylines).

Each frame then projects the polylines (the program's `make_view` / `vrot`, front hemisphere
only, orthographic) and rasterises them.

### The shot description

Plain `key = value` lines; `#` starts a comment; a later line wins. Unknown keys get a
warning.

**Frames and output**

| key | default | meaning |
|---|---|---|
| `frames` | 1 | number of frames |
| `fps` | 30 | frame rate (for `timing`) |
| `timing` | — | a timing map: a JSON map as written by `movie/music/align.py` (e.g. `movie/music/timing_REC.json`), or lines `NAME SECONDS` (see *Timing* below) |
| `start`, `end` | — | the shot's place in the film: landmarks or seconds; then `frames = round(fps·end) − round(fps·start)` and `first = round(fps·start)` |
| `out` | — | the frame directory (or `-o`) |
| `prefix`, `first` | `frame_`, 0 | file names `prefixNNNNN.png`, numbered from `first` |
| `size` (or `width`, `height`) | 840 | frame size |
| `supersample` | 3 | samples per pixel along each axis |
| `radius_px` | `size/2 − 2` = 418 | radius of the sphere at zoom 1, in pixels (as the program's PNG) |
| `background` | `1 1 1` | RGB |
| `fog` | 0.35 | lines fade towards white by up to this fraction at the rim: the opacity is `1 − fog·(1 − Z)`, `Z` the depth on the front hemisphere (an imitation of the 3D film's depth fog; 0: the program's flat colours) |
| `png_level` | 3 | zlib level of the PNGs |

**The scene**

| key | default | meaning |
|---|---|---|
| `monodromy` | `fig8` | as `-m`: `fig8` or an R/L word |
| `order` | 2 | as `-n`: an integer, or `inf` for the cusp |
| `T` | `order` | the cone parameter, a track: `tr[a,b] = −2cos(π/T)`, `T = inf` the cusp. A non-integer T is allowed only with `R = 0` (the base arcs: those groups are not discrete, so the discreteness screen is skipped) |
| `R` | 6 | the radius of the root ball (`-R`); `R = 0` draws the base arcs alone |
| `accuracy` | 0.004 | `-a` (for `build = perframe`, at zoom 1) |
| `overlap` | 1 | `--overlap` |
| `wheel` | 0 | 1: compute the Catherine wheel (`-C`; static scenes only; the program's limits apply) |
| `wheelres` | 10⁷ | `--wheelres` |
| `center` | `native` | the framing, see below |
| `build` | `once` | `once`, or `perframe`: rebuild every frame for the frame's zoom and view, as `zipper --zoom F -a ACC/F` would (detail grows with the zoom) |
| `cull` | 0 (`once`), 1 (`perframe`) | view culling as the program's EPS/PNG output (with `once`: for the view at `build_frame`, default 0, at zoom `build_zoom`) |
| `focus` | 1 | with a zoomed, culled build, add the ball about the point pushed towards the view (`--nofocus` = 0) |
| `accuracy_min`, `accuracy_zoom_exponent` | 10⁻⁶, 1 | `perframe`: the accuracy is `max(accuracy_min, accuracy / zoom^exponent)` |
| `zoom_step` | 0 | `perframe`: build only at zooms `zoom_step^k` (rounded down: the culled window still holds the view); e.g. 1.1 cuts 120 builds to 35, at the cost of a small change at each new level |

**The view and the animation** (all are tracks)

| key | default | meaning |
|---|---|---|
| `yaw`, `pitch`, `roll` | 0 | degrees, as `--yaw`, `--pitch`, `--roll`: `make_view(yaw, pitch, roll)` |
| `look_at` | — | `plus` (V⁺, the attracting fixed point of c⁻¹, where the Z⁺ base arcs start; a cusp point at `order = inf`), `minus` (V⁻), `mid`, `arcs` (the centroid of the base arcs), or `x y z`. Then `yaw` and `pitch` are offsets from the angles that put that point at the centre (followed every frame, e.g. through a T-morph) |
| `zoom` | 1 | magnification about the centre (interpolated in log zoom) |
| `depth` | `inf` | the ball shown: polylines whose root has `d(o, g.o) ≤ depth` are drawn fully, those up to `depth + depth_fade` fade in (smoothly), the rest are hidden. Applies to the layers with `.depth = 1` (Z⁺ and Z⁻ by default) |
| `depth_fade` | 0.25 | width of the fade, in units of R |

**Layers**, drawn in the order of `layers` (default `circle zplus zminus wheel arcs_plus
arcs_minus`), each composited over the frame with its opacity:

| layer | what | colour | width (px) | opacity |
|---|---|---|---|---|
| `circle` | the equator (the program's black circle) | 0 0 0 | 1 | 0 |
| `zplus` | Z⁺ | 0.05 0.15 0.85 | 1 | 1 |
| `zminus` | Z⁻ | 0.85 0.08 0.08 | 1 | 1 |
| `wheel` | the Catherine wheel | 0 0.6 0 | 1.5 | 1 if `wheel = 1` |
| `arcs_plus`, `arcs_minus` | the base arcs (the root 1) of Z⁺, Z⁻, highlighted | as Z± | 2.5 | 0 |

Per layer `L`: `L.color = r g b`, `L.width = px`, `L.opacity` (track), `L.reveal` (track,
0..1: each arc is drawn up to this fraction of its length on the sphere, from its start; for
the base arcs, from V±), `L.pen = px` (a dot of this radius at the reveal front),
`L.depth = 0|1`. The colours are the 3D film's (`zipper3.cc`); its faint context colours
(0.70 0.75 0.95 and 0.95 0.72 0.72) are Z± at opacity of about 1/3, and its highlighted
pairs are drawn 2.5 px wide.

**Tracks.** A constant (`yaw = 30`) or keys `K:value[:ease]`, where `K` is a frame number, a
percentage of the shot (`50%`; `0%` is frame 0, `100%` the last frame), `end`, or a landmark
`@NAME` / `@NAME+0.5` / `@NAME-0.25` (seconds) of the timing map. Before the first key and
after the last, the value holds. The easing of the segment that ends at a key: `smooth`
(cubic ease in-out, the default), `smoother` (quintic), `sine`, `linear`, `in`, `out`,
`hold` (jump at the key). Values may be `inf` (T, depth). `zoom` is interpolated in log
zoom, `T` in 1/T (so `T = inf` is reached smoothly).

```
yaw   = 0:0 100%:360:linear            # one full turn
zoom  = 0:1 30%:1 100%:20              # hold, then ease in and out to zoom 20
depth = @S2:0 @S3:6                    # the ball grows from the second entry to the third
T     = 0:2 20%:2 90%:3                # a T-morph (base arcs, R = 0)
```

**Framing (`center`)**. The program conjugates the holonomy so that the basepoint is the
conformal barycentre of a small orbit sample (`conformal_center`). For a lattice almost any
point is such a centre (the visual measure from any point is round), so the result is decided
by the sample: between consecutive orders it moves the basepoint by 0.34 to 0.75 in H³, and at
non-integer T (non-discrete groups) it jumps by up to 400 px between nearby T. The unconjugated
holonomy (the normal form of `reconstruct_AB`) is continuous in T (checked from T = 2 to the
cusp: no branch switch). So:

* `native`: the program's framing at each T. Use this for integer T (it is what `zipper` draws);
  not for a T-morph.
* `ref T0`: the framing of T0 for every T. Smooth; the frames at other integer T differ from the
  program's by a Möbius map. Full zippers at another integer order can be built with the same
  `ref T0`, so that a cut from a T-morph to them does not jump.
* `interp T0 T1`: `D^s · Mc(T0)`, with `D = Mc(T1) Mc(T0)⁻¹` and `s = (k − k(T0))/(k(T1) −
  k(T0))` clamped to [0, 1], k = tr[a,b]. Smooth, and exactly the program's frame at both ends,
  so a morph can start and end on full zipper shots drawn with `native`.
* `none`: the unconjugated normal form.

Measured (`ztest frame`, fig8, 60 steps of 1/T, largest step of a vertex of the base arcs'
splitting to depth 4, in pixels of an 840 frame):

| framing | T 2 → 3 | T 3 → ∞ |
|---|---|---|
| `native` | 403 px (jumps) | 325 px (jumps) |
| `ref 2` (`ref 3` for the second leg) | 3.6 px | 6.4 px |
| `interp 2 3` (`interp 3 inf`) | 8.9 px | 9.9 px |
| `none` | 2.0 px | 3.7 px |

### Checks (`ztest`)

* `ztest same`: the tagged build against the program's own `prepare` + `build_zippers`
  (with `join_polylines`), compared as multisets of segments (float endpoints, exact): fig8 at
  n = 2 (R 5), 3 (R 6), ∞ (R 5) and RRL n = 3 (R 5): **identical** (0 segments differ;
  178 000 to 564 000 segments per zipper).
* `ztest depth`: the polylines with tag ≤ R′ of a build at R = 6 against the program's own
  build at R′: fig8 n = 2 at R′ = 2, 3, 4, 5, 5.5, n = 3 at 3, 4.5, 5.25, n = ∞ at 4, 5:
  **identical** in every case (0 segments differ either way; 0 pixels differ when both are
  rasterised). The roots of B(R′) come first in the ball's order (nearest first), so the
  drawing state when each of them is drawn is the same as in the program's run at R′; the test
  confirms that the ball search orders them identically (ties included).
* `ztest frame`: the table above.

### Costs

Apple M1 (8 cores), 840×840, supersample 3, PNG level 3. "Render" is projecting and
rasterising one frame; "build" is the program's computation.

| shot (`movie/tests/shots`) | build | per frame (1 process) | 120 frames, `-j 4` | memory per process |
|---|---|---|---|---|
| `rot_n2`: fig8 n=2 R=6, rotating (1.3 M points) | 1.2 s once | render 0.13 s + PNG 0.03 s | 6.6 s | 97 MB |
| `depth_n2`: depth growth to R=6 | 1.2 s once | 0.05 s + 0.013 s (fewer lines early) | 3.2 s | 97 MB |
| `tmorph`: T 2 → 3, base arcs, a = 0.002 | 0.044 s per frame | + 0.011 s render | 5.5 s (1 process) | 34 MB |
| `zoom_cusp`: into V⁺ at n = ∞ to zoom 20, `perframe` | 1.3–2.3 s per frame | + 0.19 s render | 59 s | 270 MB |
| the same with `zoom_step = 1.1` (35 builds) | | | 25 s | 270 MB |
| `zoom_cusp_once`: one build at a = 2·10⁻⁴, culled to the zoom-1 view | 30 s once | 1.6 s render (42 M points) | 82 s | 2.4 GB |

So zooms should be rebuilt per frame: it is faster, uses a tenth of the memory, and each frame
is exactly the program's `--zoom` picture (with its focus ball, so the zoomed window keeps its
density; one build at the final accuracy has no focus ball and is much sparser at zoom 20).
Consecutive per-frame builds differ little: at a fixed view, builds for zoom 19.8 and 20 differ
in 0.5% of the pixels (by more than 32/255), builds for 19 and 20 in 3%. The depth growth has
no pops: the change between consecutive frames varies smoothly (largest ratio of consecutive
changes 1.09 in the demo).

## Captions: `caption.py`

```bash
python3 caption.py IN_DIR OUT_DIR SPEC.txt
python3 caption.py IN_DIR OUT_DIR --text "..." [--start F] [--end F] [--fade-in N] [--fade-out N] [--halo H]
```

SPEC lines: `START END FADE_IN FADE_OUT text` (frame indices in the sorted order of IN_DIR's
PNGs, `END` may be `end`; fades in frames, smooth). `@ x y size grey` changes the style and
`@halo H` adds a soft white backing behind later captions (legible over a dense sphere; the 3D
film has none). Overlapping captions are all drawn. OUT_DIR = IN_DIR rewrites in place.

The style is measured on the 3D film's frames (GLUT's 18 px Helvetica bitmap at raster position
(14, 16), grey 0.1): cap height 14 px (rows 810–823), first column 15, colour 25–27/255. Here:
Helvetica regular 18 px from `/System/Library/Fonts/Helvetica.ttc`, origin x = 14, baseline 16 px
above the bottom, grey 0.1, coverage raised to the power 0.5 (the same total ink as the bitmap
font). Short captions agree in width to 1 px; long ones come out up to 3% narrower (the bitmap
font's integer advances).

## Assembling shots, with crossfades: `zmovie --assemble`

```
# TIMELINE.txt
clip DIR [FIRST LAST]     # the frames FIRST..LAST (sorted PNGs; default all) of DIR
xfade N [ease]            # the next clip overlaps the last N frames of this one, crossfaded
hold N                    # repeat the last frame N times
blank N [r g b]           # N plain frames (default white)
```

`zmovie --assemble TIMELINE.txt OUTDIR` writes `OUTDIR/frame_00000.png ...`. A crossfade
overlaps the clips (the result is N frames shorter than the sum); the weight of the new clip
rises smoothly over the N frames (`ease`: `smooth` or `linear`, ...). Caption after
assembling, so that captions do not crossfade with the pictures.

## Encoding and sound

```bash
bin/encode_movie FRAMEDIR video.mp4 30 5                       # H.264, 5 Mbit/s
afconvert -f m4af -d aac -b 256000 music.wav music.m4a         # AAC first (see below)
bin/add_audio video.mp4 music.m4a film.mp4 START -1            # pass-through: no re-encoding
bin/probe_movie film.mp4                                       # duration, tracks, frames, sync
bin/grab_frames film.mp4 DIR 0.5 2.0 ...                       # frames at these times, to look at
```

The encoding test, `movie/tests/encode_test.sh` (60 frames of `rot_n2` and 2 s of
the music (`$WAV`), all in the scratch directory): `encode_movie` gives 60 frames, 840×840,
30 fps, 2.000 s (1.9 MB at this density, 7.4 Mbit/s: the asked 5 Mbit/s is an average the
encoder exceeds on dense frames). What matters for sync:

* **Video.** H.264 with B-frames: the first frame's presentation time in the track is 0.0667 s
  (two frames of reordering delay), and an edit list maps it to film time 0. AVFoundation,
  QuickTime and browsers honour it. Frame k is shown from k/30 s.
* **Audio, AAC.** The encoder's priming (2112 samples = 0.0479 s) is trimmed by an edit list,
  so the music starts exactly at film time 0 (`START` seconds into the file). In pass-through,
  `add_audio` ends the audio track 0.0479 s before the picture (2.0 s of video, 1.9521 s of
  audio; the 3D film ends in the same way). The start is
  unaffected; only the last 48 ms are lost, inside the baked fade-out of the first half. For a
  clip that ends on music, give the audio a little spare (the demo cuts 6.1 s for a 6 s film).
* **Audio, WAV.** `add_audio` in pass-through puts LPCM into the MP4 (1411 kbit/s). AVFoundation
  plays it, but LPCM in MP4 is non-standard and other players (and YouTube) may reject it:
  convert to AAC with `afconvert` first. With `FADE ≥ 0` `add_audio` re-encodes everything
  (`AVAssetExportPresetHighestQuality`): the video is H.264-encoded a second time, the audio
  becomes 138 kbit/s AAC, and it lasts the full 2.000 s.
* `cut_audio.py IN.wav OUT.wav START DUR [FADE_IN [FADE_OUT]]` cuts a clip with cosine fades (it
  reads the data chunk directly: `afconvert`'s stereo WAVs are WAVE_FORMAT_EXTENSIBLE).

The recording is under copyright: WAVs, AAC files and clips stay in the scratch directory;
only the films themselves carry the music.

## Timing: landmarks, not clock time

Every transition should be keyed to a musical landmark. Keep one timing map per recording,
give each shot `start = NAME` and `end = NAME`, and write its keys as `@NAME` or as
percentages. Re-timing to another recording is then `timing=other_map.json` and a re-render.

`zmovie` reads two formats. A JSON map as written by `movie/music/align.py`
(`timing_REC.json`): every entry of `landmarks` (`S1`, `A1`, ..., `C2`, with `t`), every bar
of `bars` as `bar12` (its downbeat), every beat of `beats` as `bar12.2` (bar 12, beat 2), all
plus `offset_s`. Or plain text, lines `NAME SECONDS`. A name may carry an offset in seconds:
`@bar12.2-0.25`, `end = C2+0.1`. For example, `start = S2` and `end = A2` give the frames from
round(30 S2) to round(30 A2) - 1. Frames lie on the film's
absolute grid (frame k at k/30 s; a landmark at t falls on frame round(30 t)), so consecutive
shots tile exactly; each shot's files are numbered by their frame in the film.

## The demo

`movie/tests/make_engine_demo.sh` renders `tests/shots/engine_demo.txt` (fig8 n = 3: the base arcs
unfold, then the ball grows from R = 0 to 6 while the sphere turns; 180 frames), captions it
(`tests/engine_demo_captions.txt`), encodes it and adds the first 6 s of the music (0.7 s
fade-out): `movie/tests/engine_demo.mp4`, 6.000 s, 180 frames, 840×840, 30 fps, 4.0 MB. The whole
script takes 12 s.

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
