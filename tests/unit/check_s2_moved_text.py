#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""check_s2_moved_text: the S2 programs of apps/s2/ are the legacy files with exactly the changes listed below, and
nothing else (the changes are documented at the top of each program).

For each pair, the legacy text is rewritten by the list of changes below (each piece of legacy text must occur exactly
once; a block is cut from a unique start text to the first end text after it), and the result must equal the new file
token for token (C++ tokens: comments and white space do not count, so the paragraphs added to the header comments
and the comments left where code was cut are not compared). Anything else in a new file, anywhere, fails the check:
an output test cannot see a change that alters no output on this machine (a call of release_memory dropped, a buffer
freed later), and this check does. The moved functions themselves are checked by test_memory (src/core/memory.h) and
by the tests of src/output/.

When these programs are changed on purpose, the change is added to the lists below. Python 3 standard library only.

usage: check_s2_moved_text.py REPO
"""
import os, sys

REPO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')


def tokens(s):
    t, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c.isspace(): i += 1; continue
        if s.startswith('//', i):
            j = s.find('\n', i); i = n if j < 0 else j; continue
        if s.startswith('/*', i):
            j = s.find('*/', i + 2); i = n if j < 0 else j + 2; continue
        if c in '"\'':
            j = i + 1
            while j < n and s[j] != c: j += 2 if s[j] == '\\' else 1
            t.append(s[i:j + 1]); i = j + 1; continue
        if c.isalnum() or c == '_':
            j = i
            while j < n and (s[j].isalnum() or s[j] == '_'): j += 1
            t.append(s[i:j]); i = j; continue
        t.append(c); i += 1
    return t


def replace_once(text, old, new, what):
    k = text.count(old)
    if k != 1:
        raise SystemExit('%s: the legacy text of the change "%s" occurs %d times' % (what, old[:60].replace('\n', '\\n'), k))
    return text.replace(old, new)


def cut_block(text, start, end, new, what):
    k = text.count(start)
    if k != 1:
        raise SystemExit('%s: the start of the block "%s" occurs %d times' % (what, start[:60].replace('\n', '\\n'), k))
    a = text.index(start)
    b = text.find(end, a)
    if b < 0:
        raise SystemExit('%s: no end "%s" after "%s"' % (what, end[:40], start[:40]))
    return text[:a] + new + text[b + len(end):]


# ---- the changes of apps/s2/zipper.cc ---------------------------------------------------------------------------------
ZIPPER = [
    ('replace', '#ifdef __APPLE__\n#include <mach/mach.h>            // current resident memory (the drawing\'s memory guard)\n'
                '#include <malloc/malloc.h>\n#elif defined(__GLIBC__)\n#include <malloc.h>\n#endif\n',
                '#include "core/memory.h"\n'),
    ('replace', '#include <zlib.h>                 // PNG output\n', '#include "output/png.h"\n#include "output/raster.h"\n'),
    ('replace', '#ifdef ZIPPER_VIEWER\n#define GL_SILENCE_DEPRECATION\n#include <GLUT/glut.h>            // interactive on-screen viewer (macOS frameworks)\n'
                '#include <OpenGL/glu.h>\n#endif\n', ''),
    ('replace', 'typedef std::complex<double> cpx;\n',
                'typedef std::complex<double> cpx;\nusing zippers::resident_bytes;\nusing zippers::release_memory;\n'
                'namespace png = zippers::png;\nnamespace raster = zippers::raster;\n'),
    ('cut', 'static double resident_bytes() {', '    malloc_trim(0);\n#endif\n}\n', ''),
    ('cut', 'static void png_chunk(FILE* f, const char* type, const uint8_t* data, size_t len) {',
            '        if (e2 <= dx) { err += dx; y0 += sy; }\n    }\n}\n', ''),
    ('replace', '            ras_line(img, w, h, (int)std::lround(x0), (int)std::lround(y0),',
                '            raster::line_index(img, w, h, (int)std::lround(x0), (int)std::lround(y0),'),
    ('replace', '                ras_line(img, w, h, px0, py0, px1, py1, ci); px0=px1; py0=py1; }',
                '                raster::line_index(img, w, h, px0, py0, px1, py1, ci); px0=px1; py0=py1; }'),
    ('cut', '    const size_t rowlen = 1 + (size_t)W*3;\n    vector<uint8_t> raw((size_t)H*rowlen);\n    for (int y = 0; y < H; ++y) {\n'
            '        uint8_t* row = &raw[(size_t)y*rowlen]; row[0] = 0;\n        for (int x = 0; x < W; ++x) for (int c = 0; c < 3; ++c) {\n'
            '            int sum = 0;\n            for (int dy = 0; dy < SS; ++dy) for (int dx = 0; dx < SS; ++dx)\n'
            '                sum += pal[img[((size_t)(y*SS+dy))*w + (x*SS+dx)]][c];',
            '            row[1 + (size_t)x*3 + c] = (uint8_t)(sum/(SS*SS));\n        }\n    }\n',
            '    vector<uint8_t> raw = raster::downsample_palette_rows(img, pal, w, W, H, SS);\n'),
    ('replace', '    return write_png_raw(fn, raw, W, H);\n', '    return png::write_rows(fn, raw, W, H);\n'),
    ('cut', '#ifdef ZIPPER_VIEWER\n// The viewer\'s \'p\' export file:', '    glutMainLoop();\n}\n#endif\n', ''),
    ('replace', '#ifdef ZIPPER_VIEWER\n        run_viewer(opt, mon, rep, argc, argv);\n        return 0;\n#else\n'
                '        fprintf(stderr, "interactive viewer not built; rebuild with \'make\' (the default build has the viewer)\\n");\n'
                '        return 1;\n#endif\n',
                '        fprintf(stderr, "interactive viewer not built; rebuild with \'make\' (the default build has the viewer)\\n");\n'
                '        return 1;\n'),
]

# ---- the changes of apps/s2/zmovie.cc ---------------------------------------------------------------------------------
ZMOVIE = [
    ('replace', '#include "zipper_snapshot.cc"\n', '#include "zipper.cc"\n'),
    ('cut', 'static bool png_write(const string& fn, const vector<uint8_t>& rgb, int W, int H, int level) {',
            '            rgb[3*i] = rgb[3*i+1] = rgb[3*i+2] = (uint8_t)g;\n        }\n    }\n    return true;\n}\n', ''),
    ('replace', 'if (!png_write(outdir + "/" + name, rgb, sh.W, sh.H, sh.pngLevel))', 'if (!png::write_rgb_sub(outdir + "/" + name, rgb, sh.W, sh.H, sh.pngLevel))'),
    ('replace', 'if (!png_read(s.file, px, w, h))', 'if (!png::read(s.file, px, w, h))'),
    ('replace', 'if (!png_write(outdir + name, A, W, H, 3))', 'if (!png::write_rgb_sub(outdir + name, A, W, H, 3))'),
]

PAIRS = [('legacy/s2/zipper.cc', 'apps/s2/zipper.cc', ZIPPER),
         ('legacy/s2/movie/src/zmovie.cc', 'apps/s2/zmovie.cc', ZMOVIE),
         ('legacy/s2/movie/src/ztest.cc', 'apps/s2/ztest.cc', [])]


def main():
    bad = 0
    for leg, new, changes in PAIRS:
        text = open(os.path.join(REPO, leg)).read()
        for ch in changes:
            if ch[0] == 'replace': text = replace_once(text, ch[1], ch[2], leg)
            else: text = cut_block(text, ch[1], ch[2], ch[3], leg)
        want, got = tokens(text), tokens(open(os.path.join(REPO, new)).read())
        if want == got:
            print('%-22s = %-30s with its %2d documented changes: the same, %d tokens' % (new, leg, len(changes), len(got)))
        else:
            bad += 1
            i = next((k for k in range(min(len(want), len(got))) if want[k] != got[k]), min(len(want), len(got)))
            print('%-22s DIFFERS from %s with its %d changes, at token %d:\n  expected: %s\n  found:    %s' % (
                new, leg, len(changes), i, ' '.join(want[max(0, i - 8):i + 12]), ' '.join(got[max(0, i - 8):i + 12])))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
