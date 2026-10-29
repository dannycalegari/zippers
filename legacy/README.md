# legacy

The three programs as they were before the suite was built on its shared library (`src/`): the S² program
`zipper` with its film engine, the Cartwright–Steger programs on S³, and the S⁴ programs of the 5-manifold of
Italiano–Martelli–Migliorini.

They are copied from their old folders, and CMake builds them with exactly the flags of their old makefiles, which are
kept next to them as `makefile.original` (the one in `s3/` built S³ and S⁴ together). The READMEs inside the folders
are the old ones, so they describe the old builds; `s3/README.md` is rewritten where it pointed to a file that is not
here.

**The programs are as imported, except for the lines listed below.** Three changes let the programs build on Linux
(GCC and its standard library). The others remove names, paths of the machine they were written on, notes on how
the film was made, and the lengths and times measured on a recording, from comments, defaults and two credits; besides
the two credits, they change no picture. They are made within their lines, so no line number moved; the line numbers
are those of the files here.

| file | line | what changed | why |
|---|---|---|---|
| `s3/zipcore.h` | 14 | `#include <unordered_map>` added, which the file uses | to build with GCC's standard library |
| `s3/zipcore.h` | 69 | `std::max<i64>(m, std::llabs(...))` in `maxcoef` | on Linux `int64_t` is `long`, while `llabs` returns `long long` |
| `s4/chz5.cc` | 20 | `#include <array>` added, which the file uses | to build with GCC's standard library |
| `s3/zipper3.cc` | 2 | a pointer to a notes file of the old folder becomes "see README.md" | that file is not in this repository |
| `s3/link3.cc` | 2 | the same | the same |
| `s3/zipper3.cc` | 154, 170 | a performer's name, and the lengths of the films, removed from the comments of two built-in film scripts | the scripts' frame counts stay as they were |
| `s3/zipcore.h` | 517 | the lengths of the two films timed to a recording removed from a comment (the 58 s film keeps its length) | |
| `s3/README_for_distribution.md` | 98 | the length of the film timed to a recording removed | |
| `s3/zipper3.cc` | 184 | a performer's name removed from a credit (the caption of the last segment of `--moviescript 3`) | the caption drawn changes accordingly |
| `s4/film5.cc` | 243 | a performer's name removed from the credit | the caption drawn changes accordingly |
| `s2/movie/render_film.py` | 9 | the default timing map removed from the docstring | the timing map is an input (`--timing`) |
| `s2/movie/render_film.py` | 24 | the name of a file-sync service removed | |
| `s2/movie/render_film.py` | 39–41 | a default scratch path removed: `SCRATCH` comes from the environment | a path of the machine the pipeline was written on |
| `s2/movie/render_film.py` | 328 | the default recording (a file in that scratch path) removed | `--audio` is an input |
| `s2/movie/render_film.py` | 329 | `--work` defaults to `$SCRATCH/film2d`, and is needed without `SCRATCH` | as for line 39 |
| `s2/movie/render_film.py` | 340, 343, 418 | without a timing map, a scratch folder or a recording: a clear error | instead of the defaults removed |
| `s2/movie/make_movie.py` | 29 | the performer of another recording removed from an example | |
| `s2/movie/make_movie.py` | 35 | the name of a file-sync service removed | |
| `s2/movie/make_movie.py` | 225 | the default timing map removed from `--timing`'s help | the timing map is an input |
| `s2/movie/make_movie.py` | 228 | `--work` defaults to `$SCRATCH/frames`, and is needed without `SCRATCH` | a default scratch path removed (`render_film.py`, line 39) |
| `s2/movie/make_movie.py` | 239, 251 | without a timing map or a scratch folder: a clear error | instead of the defaults removed |
| `s2/movie/storyboard.json` | 4 | the default timing map (made from one recording) replaced by a note | the timing map is an input |
| `s2/movie/storyboard.json` | 6 | a performer's name removed from the default name of the film | |
| `s2/movie/storyboard.json` | 59, 99, 131, 321, 434, 534 | notes on earlier versions of the film removed from comments | |
| `s2/movie/storyboard.json` | 99 | a length measured on the recording removed from a comment | |
| `s2/movie/src/caption.py` | 95 | a note on an earlier version of the film removed from a docstring | |
| `s2/movie/src/README.md` | 48, 77, 259, 290, 293 | a performer's name removed from examples | |
| `s2/movie/src/README.md` | 269 | the lengths of the audio and the video of a film timed to a recording removed | |
| `s2/movie/src/README.md` | 293, 294 | the frames of an example, measured on a recording, replaced by the rule that gives them | |
| `s2/movie/src/README.md` | 19 | the name of another project's folder removed: the two tools are in `film/swift/` | |
| `s2/movie/music/align.py` | 26 | a performer's name, and a time measured on the recording, removed from a comment | |
| `s2/movie/music/make_clips.py` | 2 | the name of a file-sync service removed | |
| `s2/movie/tests/encode_test.sh` | 6, 7 | a default scratch path and the default recording removed: `SCRATCH` and `WAV` are needed | a path of the machine the tests were written on |
| `s2/movie/tests/make_engine_demo.sh` | 3 | the name of a file-sync service removed | |
| `s2/movie/tests/make_engine_demo.sh` | 7, 8 | as in `encode_test.sh` | as in `encode_test.sh` |

**The S⁴ word reader is kept as it was, and it drops facet 1.** `parse_fword` in `s4/zipcore5.h` skips every token
`1`, which `chz5` writes for the empty word but which is also facet 1, so the legacy `zipper5` and `film5` read 19 of
the 80 generators of every stored substitution file (and t, in the `_t2` and `_t3` files) as their images under the
homomorphism that kills r₁: they draw 47 base arcs where the data have 44. The reader of `src/` reads facet 1. The
programs here keep the old one, as the record of what they drew. The S⁴ reference pictures are those of
`apps/s4/zipper5` (`tests/README.md`). `chz5`, its certificate and the substitution files are not affected.

| folder | programs | binaries, after `make` | data |
|---|---|---|---|
| `s2/` | `zipper` (3-manifolds, on S²); the film engine `zmovie` and its checks `ztest` | `build/legacy/s2/zipper` (with the viewer, macOS only), `build/legacy/s2/nogui/zipper`, `build/legacy/s2/movie/bin/` | none |
| `s3/` | `chz`, `zipper3`, `link3` (the Cartwright–Steger surface, on S³) | `build/legacy/s3/`, and `build/legacy/s3/nogui/zipper3` without the viewer | `subst_*.txt`, `linked_pairs.txt` |
| `s4/` | `chz5`, `zipper5`, `film5` (the 5-manifold of Italiano–Martelli–Migliorini, on S⁴) | `build/legacy/s4/` | `subst5_*.txt` |

The S³ and S⁴ programs read their data from the working directory, so run them in their folder, for example

    cd legacy/s3 && ../../build/legacy/s3/zipper3 -R 3 -o picture.png

The S² film pipeline (`s2/movie/make_movie.py`, `render_film.py`, `tests/*.sh`) runs the binaries in `s2/movie/bin`.
Each build makes that a link (ignored by git) to `build/legacy/s2/movie/bin`, which holds `zmovie`, `ztest` and links
to the Swift tools of `film/`. Its scripts take the timing map (`--timing`), the recording (`--audio`, or `WAV` for the
tests) and the scratch folder (`--work`, or the environment variable `SCRATCH`) as inputs.
