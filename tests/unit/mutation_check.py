#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""mutation_check: do the unit tests of src/output/ catch a bug?

Puts ONE deliberate bug at a time into a copy of src/output/png.h or raster.h (a changed constant, a dropped test, a
swapped member...), builds test_output_s3, test_output_s4 and test_output_s2 against the copy, runs them, and reports
whether at least one of them fails (or does not compile). A bug that no test notices is a hole in the tests.

It first does a CONTROL run with no bug at all: the three tests must build and pass with this script's own compile
lines, or the results would mean nothing.

This is a development tool, not a ctest test: it builds 3 x (1 + the number of bugs) programs (about 8 minutes with
--jobs 2). It needs a configured build folder (`make` or `cmake -S . -B build`), because test_output_s2 includes the
snapshot of zipper.cc that legacy/s2/CMakeLists.txt makes there.

usage: python3 tests/unit/mutation_check.py [--build DIR] [--jobs N] [--keep] [NAME ...]
       (no names: every bug; the compiler is $CXX, default c++; Python 3.6 or later, standard library only)
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.realpath(os.path.join(HERE, "..", ".."))

# (name, file, old, new, nth occurrence (1-based) of `old`)
M = [
 ("chunk_crc_seed",        "png.h", "crc32(0, (const Bytef*)type, 4)", "crc32(1, (const Bytef*)type, 4)", 1),
 ("chunk_len_byte3",       "png.h", "(uint8_t)(len>>24)", "(uint8_t)(len>>25)", 1),
 ("chunk_no_data_crc",     "png.h", "if (len) crc = crc32(crc, (const Bytef*)data, (uInt)len);", "", 1),
 ("rgb_default_level",     "png.h", "int W, int H, int level = 6)", "int W, int H, int level = 5)", 1),
 ("rgb_filter_byte",       "png.h", "raw.push_back(0);", "raw.push_back(1);", 1),
 ("rgb_ihdr_ctype",        "png.h", "(uint8_t)H, 8, 2, 0, 0, 0 };", "(uint8_t)H, 8, 6, 0, 0, 0 };", 1),
 ("rgb_returns_fclose",    "png.h", "    fclose(f);\n    return true;", "    return fclose(f) == 0;", 1),
 ("rows_level",            "png.h", "raw.data(), (uLong)raw.size(), 6)", "raw.data(), (uLong)raw.size(), 9)", 1),
 ("rows_no_free",          "png.h", "std::vector<uint8_t>().swap(raw);", "", 1),
 ("rows_returns_true",     "png.h", "return fclose(f) == 0;", "fclose(f); return true;", 1),
 ("rows_free_late",        "png.h", "    std::vector<uint8_t>().swap(raw);\n    FILE* f = fopen(fn, \"wb\"); if (!f) return false;", "    FILE* f = fopen(fn, \"wb\"); if (!f) return false;\n    std::vector<uint8_t>().swap(raw);", 1),
 ("sub_filter_byte",       "png.h", "row[0] = 1;", "row[0] = 0;", 1),
 ("sub_first_pixel",       "png.h", "(i >= 3 ? src[i-3] : 0)", "(i >= 3 ? src[i-3] : 1)", 1),
 ("sub_tmp_name",          "png.h", "std::string tmp = fn + \".tmp\";", "std::string tmp = fn + \".tmp2\";", 1),
 ("sub_ignores_fclose",    "png.h", "if (fclose(f) != 0) return false;", "fclose(f);", 1),
 ("sub_no_rename",         "png.h", "return rename(tmp.c_str(), fn.c_str()) == 0;", "return true;", 1),
 ("read_paeth_tie",        "png.h", "(pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c)", "(pa < pb && pa < pc) ? a : (pb <= pc ? b : c)", 1),
 ("read_average",         "png.h", "case 3: v += (a + b)/2; break;", "case 3: v += (a + b + 1)/2; break;", 1),
 ("read_sub_filter",      "png.h", "case 1: v += a; break;", "case 1: v += b; break;", 1),
 ("read_rgba_rounding",    "png.h", "(q[k]*al + 255*(255 - al) + 127)/255", "(q[k]*al + 255*(255 - al))/255", 1),
 ("read_grey_rounding",    "png.h", "g = (q[0]*al + 255*(255 - al) + 127)/255", "g = (q[0]*al + 255*(255 - al))/255", 1),
 ("read_interlace",        "png.h", "if (depth != 8 || !bpp || inter)", "if (depth != 8 || !bpp)", 1),
 ("read_length_check",     "png.h", " || rl != raw.size()", "", 1),
 ("read_first_idat_only",  "png.h", "else if (type == \"IDAT\") idat.insert", "else if (type == \"IDAT\" && idat.empty()) idat.insert", 1),
 ("read_message",          "png.h", "unsupported PNG (8-bit RGB/RGBA/grey only)", "unsupported PNG", 1),
 ("read_bad_filter",       "png.h", "default: return false;", "default: break;", 1),
 ("read_chunk_bounds",     "png.h", "if (p + 12 + len > d.size()) return false;", "if (p + 12 + len > d.size() + 1) return false;", 1),
 ("line_brush_end",        "raster.h", "for (int ey = -th; ey <= th; ey++)", "for (int ey = -th; ey < th; ey++)", 1),
 ("line_brush_x",          "raster.h", "for (int ex = -th; ex <= th; ex++)", "for (int ex = -th; ex < th; ex++)", 1),
 ("line_colour",           "raster.h", "p[1] = c[1];", "p[1] = c[0];", 1),
 ("line_rgb_err_x",        "raster.h", "if (e2 >= dy) { err += dy; x0 += sx; }", "if (e2 > dy) { err += dy; x0 += sx; }", 1),
 ("line_rgb_err_y",        "raster.h", "if (e2 <= dx) { err += dx; y0 += sy; }", "if (e2 < dx) { err += dx; y0 += sy; }", 1),
 ("line_rgb_stride",       "raster.h", "&img[((size_t)y*W + x)*3]", "&img[((size_t)y*H + x)*3]", 1),
 ("line_index_err_y",      "raster.h", "if (e2 <= dx) { err += dx; y0 += sy; }", "if (e2 < dx) { err += dx; y0 += sy; }", 2),
 ("line_index_colour",     "raster.h", "img[(size_t)y0*W + x0] = col;", "img[(size_t)y0*W + x0] = (uint8_t)(col + 1);", 1),
 ("line_index_bounds",     "raster.h", "(unsigned)y0 < (unsigned)H", "(unsigned)y0 <= (unsigned)H", 1),
 ("paint_stable_sort",     "raster.h", "std::sort(segs.begin()", "std::stable_sort(segs.begin()", 1),
 ("paint_comparator_le",   "raster.h", ": a.z < b.z; });", ": a.z <= b.z; });", 1),
 ("paint_round_x0",        "raster.h", "(int)std::lround(s.x0), (int)std::lround(s.y0)", "(int)s.x0, (int)std::lround(s.y0)", 1),
 ("paint_prio_reversed",   "raster.h", "a.prio < b.prio", "a.prio > b.prio", 1),
 ("paint_z_reversed",      "raster.h", ": a.z < b.z; });", ": a.z > b.z; });", 1),
 ("paint_ignores_brush",   "raster.h", "s.c, th);", "s.c, 0);", 1),
 ("cross_left_arm",        "raster.h", "cx - 4*SS, cy, cx + 4*SS, cy", "cx - 3*SS, cy, cx + 4*SS, cy", 1),
 ("cross_down_arm",        "raster.h", "cx, cy - 4*SS, cx, cy + 4*SS", "cx, cy - 4*SS, cx, cy + 3*SS", 1),
 ("cross_colour",          "raster.h", "const uint8_t blk[3] = {0,0,0};", "const uint8_t blk[3] = {0,0,1};", 1),
 ("down_rounding",         "raster.h", "out[((size_t)y*size + x)*3 + c] = (uint8_t)(sum/(SS*SS));", "out[((size_t)y*size + x)*3 + c] = (uint8_t)((sum + SS)/(SS*SS));", 1),
 ("down_channel",          "raster.h", "(x*SS+dx))*3 + c];", "(x*SS+dx))*3 + (c+1)%3];", 1),
 ("down_row_order",        "raster.h", "for (int dy = 0; dy < SS; ++dy) for (int dx = 0; dx < SS; ++dx) sum += img[", "for (int dy = 0; dy < SS - 1; ++dy) for (int dx = 0; dx < SS; ++dx) sum += img[", 1),
 ("pal_filter_byte",       "raster.h", "row[0] = 0;", "row[0] = 1;", 1),
 ("pal_channel",           "raster.h", "(x*SS+dx)]][c];", "(x*SS+dx)]][(c+1)%3];", 1),
 ("pal_rounding",          "raster.h", "row[1 + (size_t)x*3 + c] = (uint8_t)(sum/(SS*SS));", "row[1 + (size_t)x*3 + c] = (uint8_t)((sum + 1)/(SS*SS));", 1),
 ("segment_member_order",  "raster.h", "struct Segment { float x0, y0, x1, y1, z; uint8_t c[3]; int prio; };", "struct Segment { float z, x0, y0, x1, y1; uint8_t c[3]; int prio; };", 1),
 ("segment_prio_first",    "raster.h", "struct Segment { float x0, y0, x1, y1, z; uint8_t c[3]; int prio; };", "struct Segment { float x0, y0, x1, y1, z; int prio; uint8_t c[3]; };", 1),
]


# test program, the legacy include folder (relative to the repository), more include folders (relative to the build folder)
TESTS = [("test_output_s3", "legacy/s3", []),
         ("test_output_s4", "legacy/s4", []),
         ("test_output_s2", "legacy/s2/movie/src", ["legacy/s2/movie/snapshot"])]


def build_and_run(inc, build, test, legacy, extra, out):
    """Build one test against the headers in `inc` and run it; returns (status, the lines that matter)."""
    os.makedirs(out, exist_ok=True)
    exe = os.path.join(out, test)
    cmd = [os.environ.get("CXX", "c++"), "-I" + inc, "-I" + os.path.join(REPO, legacy)] + \
          ["-I" + os.path.join(build, e) for e in extra] + \
          ["-I" + os.path.join(REPO, "src"), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wno-unused-function",
           "-o", exe, os.path.join(HERE, test + ".cc"), "-lz"]
    c = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if c.returncode != 0:
        return "does not compile", c.stderr[-400:]
    try:
        r = subprocess.run([exe], cwd=out, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    except subprocess.TimeoutExpired:
        return "TIMEOUT", ""
    lines = [l for l in r.stdout.splitlines() if "DIFFERENT" in l or "comparisons with the legacy" in l]
    return ("pass" if r.returncode == 0 else "FAIL (exit %d)" % r.returncode), "\n".join(lines[:3])


def with_mutation(work, mutation):
    """Copy the headers into work/<name>/inc/output and put the bug in; returns the include folder."""
    name, fn, old, new, nth = mutation
    inc = os.path.join(work, name, "inc")
    os.makedirs(os.path.join(inc, "output"))
    for f in ("png.h", "raster.h"):
        shutil.copy(os.path.join(REPO, "src", "output", f), os.path.join(inc, "output", f))
    path = os.path.join(inc, "output", fn)
    with open(path) as f:
        s = f.read()
    i = -1
    for _ in range(nth):
        i = s.index(old, i + 1)                    # a ValueError here means the header no longer has this text
    with open(path, "w") as f:
        f.write(s[:i] + new + s[i + len(old):])
    return inc


def main():
    ap = argparse.ArgumentParser(description="do the unit tests of src/output/ catch a bug?")
    ap.add_argument("names", nargs="*", help="the bugs to try (default: all): " + ", ".join(m[0] for m in M))
    ap.add_argument("--build", default=os.path.join(REPO, "build"), help="the configured build folder (default: build)")
    ap.add_argument("--jobs", type=int, default=2, help="programs built and run at a time (default 2)")
    ap.add_argument("--keep", action="store_true", help="keep the work folder")
    a = ap.parse_args()
    build = os.path.realpath(a.build)
    if not os.path.isfile(os.path.join(build, "legacy", "s2", "movie", "snapshot", "zipper_snapshot.cc")):
        sys.exit("%s: no legacy/s2/movie/snapshot/zipper_snapshot.cc: configure the build first (make)" % build)
    todo = [m for m in M if not a.names or m[0] in a.names]
    if a.names and len(todo) != len(a.names):
        sys.exit("unknown bug: " + ", ".join(n for n in a.names if n not in [m[0] for m in M]))
    work = tempfile.mkdtemp(prefix="zippers_mutation_")
    try:
        # the control: the headers as they are
        control = os.path.join(work, "control")
        shutil.copytree(os.path.join(REPO, "src", "output"), os.path.join(control, "inc", "output"))
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            res = list(ex.map(lambda t: build_and_run(os.path.join(control, "inc"), build, t[0], t[1], t[2], os.path.join(control, t[0])), TESTS))
        for (test, _, _), (status, info) in zip(TESTS, res):
            print("control  %-16s %s   %s" % (test, status, info.splitlines()[-1] if info else ""), flush=True)
        if any(status != "pass" for status, _ in res):
            sys.exit("the control run does not pass: the results below would mean nothing")

        def one(m):
            inc = with_mutation(work, m)
            r = {t[0]: build_and_run(inc, build, t[0], t[1], t[2], os.path.join(work, m[0], t[0])) for t in TESTS}
            shutil.rmtree(inc, ignore_errors=True)
            return m[0], r

        survivors = []
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            for name, r in ex.map(one, todo):
                caught = [t.replace("test_output_", "") + (" (compile)" if st == "does not compile" else "") for t, (st, _) in r.items() if st != "pass"]
                print("%-24s %s" % (name, "caught by " + ", ".join(caught) if caught else "NOT CAUGHT  <<<<<<<<<<<<<<"), flush=True)
                if not caught:
                    survivors.append(name)
        print("\n%d bugs, %d caught, %d not caught%s" % (len(todo), len(todo) - len(survivors), len(survivors), ": " + ", ".join(survivors) if survivors else ""))
        return 1 if survivors else 0
    finally:
        if a.keep:
            print("work folder kept:", work)
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
