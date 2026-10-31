#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_gui_window: the window of zippers view itself, ON THE SCREEN (label gui; make test-gui). It is skipped (exit 77)
unless ZIPPERS_GUI_TESTS=1, so that a plain ctest never opens a window.

For each geometry, zview opens its window and is driven by a short key feed (ZIPPERS_KEYS, one token every
ZIPPERS_KEYDELAY = 150 ms): the arrows, a drag, the zoom keys and the scroll wheel, a resize, the keys of the geometry
(S2 c, S3 b and the pairs n l, S4 h j), and <G>, which reads the picture area back into GRAB_NN.png; it ends with q.
The test checks that zview exits with status 0, that each grab is a PNG of the window's square, not blank, and with the
colours of the layers shown (S2 Z+ blue and Z- red on white, with the black silhouette; S3 and S4 their blue and red,
faded by the fogs, and the black crosses of t+ and t-; S3's linked pair). About half a minute.

usage: test_gui_window.py BUILD      (Python 3.6 or later, standard library only)
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

BUILD = sys.argv[1] if len(sys.argv) > 1 else "build"
ZVIEW = os.path.join(BUILD, "apps", "gui", "zview")
SKIP = 77

n_checks = 0
n_bad = 0


def check(ok, what):
    global n_checks, n_bad
    n_checks += 1
    print("  %s: %s" % ("ok" if ok else "FAIL", what))
    if not ok:
        n_bad += 1
    return ok


def read_png(fn):
    """the width, height and RGB bytes of a PNG of 8-bit RGB rows with filter 0 (as zview writes its grabs)"""
    with open(fn, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    i, w, h, idat = 8, 0, 0, b""
    while i < len(data):
        n, kind = struct.unpack(">I4s", data[i:i + 8])
        body = data[i + 8:i + 8 + n]
        if kind == b"IHDR":
            w, h, depth, colour = struct.unpack(">IIBB", body[:10])
            if depth != 8 or colour != 2:
                return None
        elif kind == b"IDAT":
            idat += body
        i += 12 + n
    raw = zlib.decompress(idat)
    row = 1 + 3 * w
    if len(raw) != row * h or any(raw[y * row] != 0 for y in range(h)):
        return None
    rgb = b"".join(raw[y * row + 1:(y + 1) * row] for y in range(h))
    return w, h, rgb


def colours(rgb):
    """of every other pixel: those that are not white, and how many are bluish, reddish, greenish and dark"""
    n = blue = red = green = dark = 0
    for k in range(0, len(rgb) - 2, 6):
        r, g, b = rgb[k], rgb[k + 1], rgb[k + 2]
        if r > 245 and g > 245 and b > 245:
            continue
        n += 1
        if b > r + 40 and b > g + 20:
            blue += 1
        if r > b + 40 and r > g + 40:
            red += 1
        if g > r + 40 and g > b + 40:
            green += 1
        if r < 60 and g < 60 and b < 60:
            dark += 1
    return n, blue, red, green, dark


CASES = [
    ("S2", ["fig8", "-R", "2", "-a", "0.01"], "<G><L><U><drag 40,25>z<scroll -1><s700x500><G>c<wait><G>c<G>q",
     [("blue", "red", "dark"), ("blue", "red", "dark"), ("blue", "red", "green", "dark"), ("green", "dark")]),
    ("S3", ["cs", "-a", "0.05"], "<G><L><U>w<drag -30,10>zx<scroll 1>b<G>bbb+<wait><G>q",
     [("blue", "red", "dark"), ("blue", "red"), ("blue", "red")]),
    ("S3 pairs", ["cs", "-a", "0.05", "--pairs", "linked_pairs.txt"], "<G>n<G>l<G>q",
     [("blue", "red"), ("blue", "red"), ("blue", "red")]),
    ("S4", ["imm", "-a", "0.05"], "<G>hhjj<drag 20,0><R>s<G>HHJJz<G>q",
     [("blue", "red", "dark"), ("blue", "red"), ("blue", "red")]),
]


def main():
    if os.environ.get("ZIPPERS_GUI_TESTS") != "1":
        print("test_gui_window: skipped (it opens a window on the screen: set ZIPPERS_GUI_TESTS=1, as make test-gui does)")
        return SKIP
    if not os.access(ZVIEW, os.X_OK):
        print("test_gui_window: %s is not built" % ZVIEW)
        return 1
    root = tempfile.mkdtemp(prefix="zippers_test_gui_window_")
    try:
        for name, args, keys, want in CASES:
            print("%s: zview %s, keys %s" % (name, " ".join(args), keys))
            d = os.path.join(root, name.replace(" ", "_"))
            os.mkdir(d)
            env = dict(os.environ)
            env.pop("ZIPPERS_NO_WINDOW", None)
            env.pop("ZIPPERS_DATA", None)
            env["ZIPPERS_KEYS"] = keys
            env["ZIPPERS_KEYDELAY"] = "150"
            try:
                p = subprocess.run([os.path.abspath(ZVIEW)] + args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, universal_newlines=True, cwd=d, env=env, timeout=120)
                code, err = p.returncode, p.stderr
            except subprocess.TimeoutExpired:
                code, err = -1, "timed out after 120 s"
            check(code == 0 and "refused" not in err, "exit 0, nothing refused (exit %d: %s)" % (code, err[-500:]))
            for k, kinds in enumerate(want):
                fn = os.path.join(d, "GRAB_%02d.png" % (k + 1))
                png = read_png(fn) if os.path.exists(fn) else None
                if not check(png is not None, "%s: a PNG of RGB rows" % os.path.basename(fn)):
                    continue
                w, h, rgb = png
                n, blue, red, green, dark = colours(rgb)
                got = {"blue": blue, "red": red, "green": green, "dark": dark}
                ok = w == h and n > 0.001 * w * h and all(got[c] > 10 for c in kinds)
                check(ok, "%s: %d x %d, %d pixels drawn: blue %d, red %d, green %d, dark %d (wanted %s)" % (
                    os.path.basename(fn), w, h, n, blue, red, green, dark, ", ".join(kinds)))
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("test_gui_window: %d checks, %d fail" % (n_checks, n_bad))
    return 1 if n_bad else 0


if __name__ == "__main__":
    sys.exit(main())
