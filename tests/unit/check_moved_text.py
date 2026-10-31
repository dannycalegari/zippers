#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""check_moved_text: every function of src/output/png.h and src/output/raster.h, and the S2 camera of src/view/s2.h, is its
legacy original, token for token.

The moved code changes no expression. The unit tests test_output_*.cc show
that the moved functions BEHAVE like the legacy ones; this check shows that they ARE the legacy ones, up to the mechanical
changes that the headers list: `static` is `inline`, `vector` and `string` are written `std::vector` and `std::string`,
the names are new, and the statements the legacy `write_png` functions have inline became functions.

Both texts are cut into C++ tokens (comments and white space dropped), the mechanical changes are undone in the NEW tokens,
and the two token lists must be equal. A statement the legacy code has inside a larger function must appear, token for
token, inside the new function. Prints one line per pair; the exit status is 1 if any pair differs.

usage: check_moved_text.py [REPO_ROOT]          (Python 3.6 or later, standard library only)
"""
import os
import re
import sys

REPO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")

Z3, Z4 = "legacy/s3/zipcore.h", "legacy/s4/zipcore5.h"
P4 = "legacy/s4/zipper5.cc"
P2, FILM = "legacy/s2/zipper.cc", "legacy/s2/movie/src/zmovie.cc"


def read(path):
    with open(os.path.join(REPO, path), encoding="utf-8") as f:
        return f.read()


TOKEN = re.compile(r"""[A-Za-z_]\w*|\d[\w.]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|::|\S""")


def tokens(text):
    text = re.sub(r"//[^\n]*", "", text)                      # line comments
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)          # block comments
    return TOKEN.findall(text)


def function_text(src, header):
    """From `header` to the brace that closes the function body."""
    i = src.index(header)
    depth, k = 0, src.index("{", i)
    while True:
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
        k += 1


def between(src, start, end):
    """From the first `start` to the end of the first `end` after it."""
    i = src.index(start)
    return src[i:src.index(end, i) + len(end)]


def undo(toks, renames):
    """The mechanical changes, undone in a list of tokens."""
    out, i = [], 0
    while i < len(toks):
        t = toks[i]
        if t == "std" and i + 2 < len(toks) and toks[i + 1] == "::" and toks[i + 2] in ("vector", "string"):
            out.append(toks[i + 2])                              # std::vector -> vector, std::string -> string
            i += 3
            continue
        out.append("static" if t == "inline" else renames.get(t, t))
        i += 1
    return out


def replace_seq(toks, old, new):
    """Every occurrence of the token sequence `old` replaced by `new`."""
    out, i = [], 0
    while i < len(toks):
        if toks[i:i + len(old)] == old:
            out.extend(new)
            i += len(old)
        else:
            out.append(toks[i])
            i += 1
    return out


def contains(hay, needle):
    n = len(needle)
    return any(hay[i:i + n] == needle for i in range(len(hay) - n + 1))


png, ras = read("src/output/png.h"), read("src/output/raster.h")
failures = 0


def report(ok, name):
    global failures
    print(("  same     " if ok else "  DIFFERS  ") + name)
    if not ok:
        failures += 1


print("png.h: whole functions")
for new_header, new_name, old_name, old_header, files in [
    ("inline void chunk(FILE* f", "chunk", "png_chunk", "static void png_chunk(", [Z3, Z4, P2]),
    ("inline bool write_rgb(", "write_rgb", "write_png_rgb", "static bool write_png_rgb(", [Z3, Z4]),
    ("inline bool write_rows(", "write_rows", "write_png_raw", "static bool write_png_raw(", [P2]),
    ("inline bool write_rgb_sub(", "write_rgb_sub", "png_write", "static bool png_write(", [FILM]),
    ("inline bool read(", "read", "png_read", "static bool png_read(", [FILM]),
]:
    new = undo(tokens(function_text(png, new_header)), {new_name: old_name, "chunk": "png_chunk"})
    for f in files:
        report(new == tokens(function_text(read(f), old_header)), "png::%s  =  %s  (%s)" % (new_name, old_name, f))

print("raster.h: whole functions")
new = undo(tokens(function_text(ras, "inline void line_rgb(")), {"line_rgb": "ras_line"})
old3 = tokens(function_text(read(Z3), "static void ras_line("))
old4 = tokens(function_text(read(P4), "static void ras_line("))
report(new == old4, "raster::line_rgb  =  ras_line  (%s)" % P4)
# The S3 copy names the loop variables of the brush dy, dx (they hide those of the Bresenham loop, which is harmless); the S4
# copy names them ey, ex. So the two legacy texts must differ in exactly the 8 tokens of the brush loops, and in no other.
diff = [(a, b) for a, b in zip(old3, old4) if a != b]
report(len(old3) == len(old4) and len(diff) == 8 and all(d in (("dy", "ey"), ("dx", "ex")) for d in diff),
       "raster::line_rgb  =  ras_line  (%s): the S3 copy differs from the S4 copy in 8 tokens, the names of its brush variables" % Z3)
new = undo(tokens(function_text(ras, "inline void line_index(")), {"line_index": "ras_line"})
report(new == tokens(function_text(read(P2), "static void ras_line(")), "raster::line_index  =  ras_line  (%s)" % P2)

print("raster.h: the statements the legacy write_png functions have inline")


def struct_tokens(src, name):
    return tokens(re.search(r"struct\s+" + name + r"\s*\{[^}]*\};", src).group(0))


new_seg = ["Seg" if t == "Segment" else t for t in struct_tokens(ras, "Segment")]
report(new_seg == struct_tokens(read(Z3), "Seg"), "raster::Segment  =  struct Seg of write_png  (%s)" % Z3)
report(new_seg == struct_tokens(read(P4), "Seg"), "raster::Segment  =  struct Seg of write_png5  (%s)" % P4)

paint = undo(tokens(function_text(ras, "inline void paint_segments(")), {"line_rgb": "ras_line", "Segment": "Seg", "th": "thick"})
for f in (Z3, P4):
    report(contains(paint, tokens(between(read(f), "std::sort(segs.begin()", "s.c, thick);"))),
           "raster::paint_segments contains the sort and the drawing of the painter in %s" % f)

cross = undo(tokens(function_text(ras, "inline void cross(")), {"line_rgb": "ras_line"})
cross_s3 = replace_seq(cross, [",", "th", ")"], [")"])                 # S3: the brush is left out (0 by default)
cross_s4 = replace_seq(cross, ["th"], ["1"])                           # S4: brush 1
report(contains(cross_s3, tokens(between(read(Z3), "const uint8_t blk[3]", "cx, cy + 4*SS, blk);"))),
       "raster::cross contains the two lines of the marks in %s (brush 0)" % Z3)
report(contains(cross_s4, tokens(between(read(P4), "const uint8_t blk[3]", "cx, cy + 4*SS, blk, 1);"))),
       "raster::cross contains the two lines of the marks in %s (brush 1)" % P4)

down = undo(tokens(function_text(ras, "inline std::vector<uint8_t> downsample_rgb(")), {})
for f in (Z3, P4):
    report(contains(down, tokens(between(read(f), "vector<uint8_t> out((size_t)size*size*3);", "(uint8_t)(sum/(SS*SS));\n    }"))),
           "raster::downsample_rgb contains the 2x2 loops of %s" % f)

pal = undo(tokens(function_text(ras, "inline std::vector<uint8_t> downsample_palette_rows(")), {})
report(contains(pal, tokens(between(read(P2), "const size_t rowlen = 1 + (size_t)W*3;", "(uint8_t)(sum/(SS*SS));\n        }\n    }"))),
       "raster::downsample_palette_rows contains the 2x2 loops of write_png in %s" % P2)

print("view/s2.h: the S2 camera, from the program and its window (%s)" % P2)
s2h, leg2 = read("src/view/s2.h"), read(P2)
report(struct_tokens(s2h, "View") == struct_tokens(leg2, "View"), "s2::View  =  View")
for name, ret in [("make_view", "View"), ("mul3", "View"), ("rotx", "View"), ("roty", "View"), ("wrap180", "double"),
                  ("view_angles", "void"), ("fmt_num", "string"), ("export_name", "string")]:
    new_ret = "std::string" if ret == "string" else ret
    new = undo(tokens(function_text(s2h, "inline %s %s(" % (new_ret, name))), {})
    report(new == tokens(function_text(leg2, "static %s %s(" % (ret, name))), "s2::%s  =  %s" % (name, name))
# command_angles: the statements of the window's print_command that choose the angles
ca = undo(tokens(function_text(s2h, "inline bool command_angles(")), {})
report(contains(ca, tokens("view_angles(gview, y, p, r);")) and "view_angles(gview, y, p, r);" in leg2,
       "s2::command_angles contains print_command's call of view_angles")
report(contains(ca, tokens(between(leg2, "for (double t : {1e-12, 0.0}) {", "if (e <= 1e-3*px) break;\n        }"))),
       "s2::command_angles contains print_command's loop over the two tolerances")
# export_framing: the statements of the window's key 'p' that frame the picture, the window's variables renamed
ef = undo(tokens(function_text(s2h, "inline void export_framing(")), {})
for st in ["if (o.size < 1024) o.size = 1024;", "o.zoom = opt->zoom * (double)o.size / ((o.size - 4.0) * 1.1);",
           "o.size = std::min(o.size, PNG_MAX_SIZE); o.zoom = std::min(o.zoom, MAX_ZOOM);"]:
    t = tokens(st)
    for old, new_ in [(["o", ".", "size"], ["size"]), (["o", ".", "zoom"], ["zoom"]), (["opt", "->", "zoom"], ["window_zoom"]),
                      (["PNG_MAX_SIZE"], ["max_size"]), (["MAX_ZOOM"], ["max_zoom"])]:
        t = replace_seq(t, old, new_)
    report(st in leg2 and contains(ef, t), "s2::export_framing contains  %s  (the key 'p')" % st)

print("\n%d pair(s) differ" % failures)
sys.exit(1 if failures else 0)
