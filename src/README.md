# src

The shared library of the suite. It is header only: programs and tests link the CMake target `zippers::core`, which
puts this folder on the include path (`#include "geometry/backend.h"`).

| path | what |
|---|---|
| `geometry/backend.h` | what a geometry backend must provide, with the legacy function each member replaces, and `is_backend<B>()` |
| `geometry/sphere.h` | points of S^{N-1} in R^N, and the chord and point-to-segment distances |
| `geometry/pu21/` | PU(2,1) over Z[zeta_12] (`zeta12.h`, `pu21.h`): the backend `PU21` of S^3 |
| `geometry/on1/on1.h` | O(n,1) over (1/2)Z, stored doubled: the backend `ON1<n>` (n = 5 for S^4) |
| `core/` | `hash.h` (splitmix, the renderer's fingerprint), `keyset.h`, `timing.h`, `memory.h` (the resident memory of the process, and giving freed memory back: the S² memory guard's), `sha256.h` (SHA-256, FIPS 180-4: the hashes of the data files that a session names) |
| `store/store.h` | `Store<B, Label>`: exact group elements with a hash table on their keys |
| `examples/` | `cartwright_steger.h` (namespace `cs`) and `imm_m5.h` (namespace `imm`): the data of the two lattices, words, balls, roots, and the radii at which the roots change (`shell_radii`); `cartwright_steger_roots.h` and the end of `imm_m5.h`: the descent checks of the root balls |
| `examples/*_search.h` | the two search policies: what `chz` and `chz5` each do differently (sorts, membership, messages, files) |
| `examples/*_render.h` | the two render policies: each example's file dialect, words, roots, tips and look-ahead, and messages |
| `output/png.h` | the PNG writers (RGB; filtered rows; the film engine's, with the Sub filter) and the reader |
| `output/raster.h` | lines (with a brush; in a palette image), the depth-sorted segment painter, marks, 2x2 downsampling |
| `output/polylines.h` | the polyline file (format 1): the curves of a picture as data, with a text header; its writer, which `zipper`, `zipper3` and `zipper5` use (`--polylines`), and which writes under a temporary name and renames (through a symbolic link, to its target), so that a write that fails leaves nothing behind; `write_problem`, why a file cannot be written; and its reader, which checks every count against the size of the file and every number for being finite |
| `output/session.h` | the session file (format 1): a picture's draw command with every option written out, its defaults per sphere, the reader and the writer, the options given after a session, and the hashes of its data files; read by `zippers draw --session`, and the format a window writes beside its pictures |
| `search/kernel_search.h` | the block-filling kernel search (Calegari-Loukidou, Thm 4.9), once, over an example's search policy; its options |
| `search/roof_choice.h` | the choice of the roofs (the kernel search's `--roofs`), and the measures of a system of roofs |
| `render/` | one renderer for S³ and S⁴ over the backend (`Renderer<B>`), substitution files, letters, sides, tips, culling, complete arcs and loops, `World<B>` |
| `view/s4.h`, `view/s3.h` | the S⁴ and S³ cameras and their PNG painters (namespaces `s4`, `s3`), and the windows of zoomed views; in S⁴ the pole moved toward any direction |
| `view/s2.h` | the S² camera (namespace `s2`): the view of the S² program (`make_view`), and the turns, angles and framing of a window on it; the names of a window's pictures, which never overwrite a file (`export_name`, and `export_stem` for a picture and its session side by side) |
| `view/options.h` | what `--colour` and `--viewmatrix` of the three drawing programs share: a colour, and the orthonormality and the determinant of a view's matrices |
| `analysis/` | the separation of Z⁺ and Z⁻, relations and their loops, linking numbers, linked pairs |

Every function moved here from a legacy program reproduces it bit for bit, unless its header says otherwise;
`tests/unit/` checks this. A program that includes `output/png.h` must also link zlib (`ZLIB::ZLIB`), as the legacy
programs do. `docs/developer_guide.md` has the measurements and the traps of this code.
