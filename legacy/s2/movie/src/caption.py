#!/usr/bin/env python3
"""caption.py -- overlay captions on a sequence of frames, in the style of the 3D zipper film.

The 3D film (zipper3 --movie) draws its captions with GLUT's 18-pixel Helvetica bitmap font
at raster position (14, 16) from the bottom left, in grey 0.1.  Measured on its frames: cap
height 14 px (rows 810-823 of 840), text starting at column 15, colour (25-27)/255.  Here the
same font (Helvetica regular, 18 px, antialiased) is drawn with its baseline at y = H - 16 and
its origin at x = 14, in (0.1, 0.1, 0.1); the antialiased coverage is raised to the power 0.5
(weight), which gives the same total ink as the bitmap font.  Text widths agree to 1% for short
captions; long ones come out up to 3% narrower (the bitmap font's integer advances).

usage:
  caption.py IN_DIR OUT_DIR SPEC.txt        captions from a spec file
  caption.py IN_DIR OUT_DIR --text "..." [--start F] [--end F] [--fade-in N] [--fade-out N] [--halo H]

IN_DIR holds the frames (*.png, sorted by name; frame k is the k-th file); OUT_DIR receives
all of them, captioned (OUT_DIR may equal IN_DIR: the frames are then rewritten in place,
only those with a caption).  SPEC.txt lines ('#' comments):

  START END FADE_IN FADE_OUT  text of the caption
  @ x y size grey                     optional: change position/size/colour for later lines
  @panel P                            optional: a soft-edged white rectangle of opacity P (0..1)
                                      behind later captions (the film uses 0.82, with the halo)
  @halo H                             optional: a soft white backing of opacity H (0..1) behind
                                      later captions, for legibility over dense pictures (the
                                      3D film has none: default 0)

START and END are frame indices (inclusive; END may be 'end'); FADE_IN/FADE_OUT are numbers
of frames (0: no fade).  The opacity ramps smoothly (cubic) from 0 to 1 over the first
FADE_IN frames and back to 0 over the last FADE_OUT frames.  Overlapping captions are all
drawn (fade one out while the next fades in, or place them at different heights).

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import os, sys, shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFont

FONT = '/System/Library/Fonts/Helvetica.ttc'
STYLE = dict(x=14, y_from_bottom=16, size=18, grey=0.1, weight=0.5, halo=0.0, panel=0.0)
PANEL_PAD = (8, 5)        # the panel's margin around the glyphs (x, y), in pixels
PANEL_FEATHER = 5         # its edges ramp smoothly to 0 over this many pixels


def smooth(s):
    s = min(1.0, max(0.0, s))
    return s * s * (3 - 2 * s)


def caption_alpha(k, start, end, fin, fout):
    if k < start or k > end:
        return 0.0
    a = 1.0
    if fin > 0:
        a = min(a, smooth((k - start + 1) / (fin + 1)))
    if fout > 0:
        a = min(a, smooth((end - k + 1) / (fout + 1)))
    return a


_masks = {}


def text_mask(text, W, H, st):
    key = (text, W, H, st['x'], st['y_from_bottom'], st['size'], st.get('weight', 1.0))  # (halo: applied at overlay)
    if key not in _masks:
        font = ImageFont.truetype(FONT, st['size'], index=0)
        im = Image.new('L', (W, H), 0)
        ImageDraw.Draw(im).text((st['x'], H - st['y_from_bottom']), text, font=font, fill=255, anchor='ls')
        m = (np.asarray(im).astype(np.float32) / 255.0) ** st.get('weight', 1.0)
        ys, xs = np.nonzero(m > 0)
        box = (max(0, ys.min() - 6), min(H, ys.max() + 7), max(0, xs.min() - 6), min(W, xs.max() + 7)) if len(ys) else (0, 0, 0, 0)
        # the halo: the glyphs dilated by 2 px, then blurred (box filter, radius 2, twice)
        h = m.copy()
        for _ in range(2):
            g = h.copy()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    g = np.maximum(g, np.roll(np.roll(h, dy, 0), dx, 1))
            h = g
        for _ in range(2):
            g = np.zeros_like(h)
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    g += np.roll(np.roll(h, dy, 0), dx, 1)
            h = np.minimum(1.0, g / 25.0 * 1.6)
        _masks[key] = (m, box, h)
    return _masks[key]


_panels = {}


def panel_mask(text, W, H, st):
    """a soft-edged white rectangle behind the caption: over the full-frame
    close-ups (shots 5, 6, 9) the glyph halo alone left the text hard to read.  On white it is
    invisible, so captions over white look exactly as in the 3D film."""
    m, (y0, y1, x0, x1), _ = text_mask(text, W, H, st)
    key = (text, W, H, st['x'], st['y_from_bottom'], st['size'])
    if key not in _panels:
        ys, xs = np.nonzero(m > 0.05)
        if not len(ys):
            _panels[key] = (None, (0, 0, 0, 0))
            return _panels[key]
        px, py, f = PANEL_PAD[0], PANEL_PAD[1], PANEL_FEATHER
        a0, a1 = max(0, ys.min() - py - f), min(H, ys.max() + 1 + py + f)
        b0, b1 = max(0, xs.min() - px - f), min(W, xs.max() + 1 + px + f)
        yy = np.arange(a0, a1)[:, None].astype(np.float32)
        xx = np.arange(b0, b1)[None, :].astype(np.float32)
        dy = np.maximum(0, np.maximum((ys.min() - py) - yy, yy - (ys.max() + py)))
        dx = np.maximum(0, np.maximum((xs.min() - px) - xx, xx - (xs.max() + px)))
        d = np.sqrt(dx * dx + dy * dy)
        s = np.clip(1 - d / f, 0, 1)
        _panels[key] = (s * s * (3 - 2 * s), (a0, a1, b0, b1))
    return _panels[key]


def overlay(img, text, alpha, st):
    """img: H x W x 3 float array in [0,1], modified in place."""
    H, W = img.shape[:2]
    m, (y0, y1, x0, x1), halo = text_mask(text, W, H, st)
    if alpha <= 0 or y1 <= y0:
        return
    if st.get('panel', 0) > 0:
        pm, (a0, a1, b0, b1) = panel_mask(text, W, H, st)
        if pm is not None:
            b = alpha * st['panel'] * pm[:, :, None]
            img[a0:a1, b0:b1] = img[a0:a1, b0:b1] * (1 - b) + b
    if st.get('halo', 0) > 0:
        b = alpha * st['halo'] * halo[y0:y1, x0:x1, None]
        img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - b) + b
    a = alpha * m[y0:y1, x0:x1, None]
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a) + st['grey'] * a


def parse_spec(path, nframes):
    caps, st = [], dict(STYLE)
    for ln, line in enumerate(open(path, encoding='utf-8'), 1):
        line = line.split('#', 1)[0].rstrip('\n')
        if not line.strip():
            continue
        f = line.split(None, 4)
        if f[0] == '@halo':
            st = dict(st, halo=float(f[1]))
            continue
        if f[0] == '@panel':
            st = dict(st, panel=float(f[1]))
            continue
        if f[0] == '@':
            st = dict(st, x=int(f[1]), y_from_bottom=int(f[2]), size=int(f[3]), grey=float(f[4]))
            continue
        if len(f) < 5:
            sys.exit('%s:%d: want START END FADE_IN FADE_OUT text' % (path, ln))
        end = nframes - 1 if f[1] == 'end' else int(f[1])
        caps.append((int(f[0]), end, int(f[2]), int(f[3]), f[4].strip(), dict(st)))
    return caps


def main(argv):
    if len(argv) < 4:
        sys.exit(__doc__)
    ind, outd = argv[1], argv[2]
    files = sorted(f for f in os.listdir(ind) if f.endswith('.png') and not f.startswith('.'))
    if not files:
        sys.exit('no frames in ' + ind)
    n = len(files)
    if argv[3] == '--text':
        args = dict(start='0', end='end', fade_in='0', fade_out='0', halo='0')
        text, i = argv[4], 5
        while i < len(argv):
            k = argv[i].lstrip('-').replace('-', '_')
            args[k] = argv[i + 1]
            i += 2
        end = n - 1 if args['end'] == 'end' else int(args['end'])
        caps = [(int(args['start']), end, int(args['fade_in']), int(args['fade_out']), text, dict(STYLE, halo=float(args['halo'])))]
    else:
        caps = parse_spec(argv[3], n)
    inplace = os.path.abspath(ind) == os.path.abspath(outd)
    os.makedirs(outd, exist_ok=True)
    for k, fn in enumerate(files):
        live = [(c, caption_alpha(k, *c[:4])) for c in caps]
        live = [(c, a) for c, a in live if a > 0]
        src, dst = os.path.join(ind, fn), os.path.join(outd, fn)
        if not live:
            if not inplace:
                shutil.copyfile(src, dst)
            continue
        img = np.asarray(Image.open(src).convert('RGB')).astype(np.float32) / 255.0
        for c, a in live:
            overlay(img, c[4], a, c[5])
        Image.fromarray(np.clip(img * 255.0 + 0.5, 0, 255).astype(np.uint8)).save(dst, compress_level=3)
    print('captioned %d frames (%d captions) into %s' % (n, len(caps), outd))


if __name__ == '__main__':
    main(sys.argv)
