#!/usr/bin/env python3
"""captions.py -- the film's captions, typeset with LaTeX (so the mathematics is set properly:
S^3, Z^+, d(o, g.o) <= R, ...) and laid on caption-free frames, centred at the bottom, with a
soft white halo.  One pass over the whole film, so the three parts share one caption style.

    python3 captions.py --frames DIR2D DIR3D DIR4D --timing2d T.json --script3d SCRIPT
                        --landmarks4d LANDMARKS --out OUTDIR [--storyboard2d FILE] [--credit FILE]
                        [--font cm|cmbright|pazo] [--color 2c3e50] [--first K] [--last K] [--jobs N]
    python3 captions.py ... --sample K OUT.png      one frame

The caption texts and times come from:
  the 2D half: the S^2 program's storyboard.json (read only; or --storyboard2d) on the 2D half's
               timing map (--timing2d), via the S^2 pipeline's own render_film.caption_list;
  the 3D half: the script made by make_script.py (--script3d; one caption per segment), which
               begins where the 2D half ends (the storyboard's film_end);
  the ending:  the landmarks made by make_landmarks4d.py (--landmarks4d), as in film5; it begins
               at START, and the film ends at END.
  the credit:  --credit FILE, a file holding the LaTeX source of the credit line, shown from CRED
               to END; there is no credit without it.
Each plain-text caption is mapped to its LaTeX source in TEX below; a caption without an entry
is an error, so nothing is set in the wrong font.

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import os, sys, json, hashlib, subprocess, tempfile, shutil, argparse
import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
M2D = os.path.join(PROJ, 'legacy', 's2', 'movie')
W = H = 840

TEX = {
    # the 2D half
    'Monodromy RL: the figure-eight knot complement, a punctured-torus bundle':
        r'Monodromy $RL$: the figure-eight knot complement, a punctured-torus bundle',
    'Z+ (blue) is the union of the translates of two arcs, from the cusp point V to a·V and to b·V':
        r'$Z^+$ (blue) is the union of the translates of two arcs, from the cusp point $V$ to $a\cdot V$ and to $b\cdot V$',
    'Z− (red) joins the same points by two other arcs: at the cusp, the monodromy c fixes only V':
        r'$Z^-$ (red) joins the same points by two other arcs: at the cusp, the monodromy $c$ fixes only $V$',
    'Filling the cusp: the arcs change as the cone angle 2π/n grows from 0 to π':
        r'Filling the cusp: the arcs change as the cone angle $2\pi/n$ grows from $0$ to $\pi$',
    'n = ∞': r'$n=\infty$', 'n = 12': r'$n=12$', 'n = 6': r'$n=6$', 'n = 4': r'$n=4$', 'n = 3': r'$n=3$', 'n = 2': r'$n=2$',
    'Cone order 2: adding the translates of the arcs by all g with d(o, g·o) ≤ R, up to R = 5':
        r'Cone order 2: adding the translates of the arcs by all $g$ with $d(o,g\cdot o)\le R$, up to $R=5$',
    'Cone order 12: near the cusp, the two fixed points of c (where the arcs start) come close':
        r'Cone order 12: near the cusp, the two fixed points of $c$ (where the arcs start) come close',
    'At the cusp they merge into one parabolic fixed point, where Z+ and Z− meet':
        r'At the cusp they merge into one parabolic fixed point, where $Z^+$ and $Z^-$ meet',
    'Monodromy RRL, fiber a torus with a cone point of order 3: Z+ and Z− out to R = 4.5':
        r'Monodromy $RRL$, fiber a torus with a cone point of order 3: $Z^+$ and $Z^-$ out to $R=4.5$',
    'Monodromy RLLRL, fiber a torus with a cone point of order 4':
        r'Monodromy $RLLRL$, fiber a torus with a cone point of order 4',
    'Its Catherine wheel (green): one circle separating the drawn Z+ from the drawn Z−':
        r'Its Catherine wheel (green): one circle separating the drawn $Z^+$ from the drawn $Z^-$',
    'Monodromy RL again, cone order 2: the Catherine wheel, traced along its length':
        r'Monodromy $RL$ again, cone order 2: the Catherine wheel, traced along its length',
    # the 3D half
    'The Cartwright-Steger surface, a complex hyperbolic surface: its zippers lie in S^3':
        r'The Cartwright--Steger surface, a complex hyperbolic surface: its zippers lie in $S^3$',
    'The 18 base arcs of the zippers Z+ (blue) and Z- (red), in a stereographic view of S^3 centred at t+':
        r'The 18 base arcs of the zippers $Z^+$ (blue) and $Z^-$ (red), in a stereographic view of $S^3$ centred at $t^+$',
    'Rotating S^3 itself: the pole of the stereographic projection moves':
        r'Rotating $S^3$ itself: the pole of the stereographic projection moves',
    'Adding the translates by the 18 shortest group elements':
        r'Adding the translates by the 18 shortest group elements',
    'Adding the next shell of translates: the zippers fill in':
        r'Adding the next shell of translates: the zippers fill in',
    'Flying through S^3 among the translates': r'Flying through $S^3$ among the translates',
    'A loop in Z+ and a loop in Z-, from the same relation of the kernel: linking number -1':
        r'A loop in $Z^+$ and a loop in $Z^-$, from the same relation of the kernel: linking number $-1$',
    'The same pair against the faint base arcs': r'The same pair against the faint base arcs',
    'Another linked pair: linking number +1': r'Another linked pair: linking number $+1$',
    'Linking number -1': r'Linking number $-1$',
    'Linking number +1': r'Linking number $+1$',
    'Linking number -1, against the base arcs': r'Linking number $-1$, against the base arcs',
    'The zippers of the Cartwright-Steger surface': r'The zippers of the Cartwright--Steger surface',
    # the ending
    'ENDING1': r'A hyperbolic 5-manifold that fibers over the circle (Italiano--Martelli--Migliorini): its zippers lie in $S^4$',
    'ENDING2': r'The shadow of $S^4$ in $\mathbb{R}^3$, turning in the fourth dimension',
    # 'CREDITS': the credit line, from --credit
}

FONTS = {'cm': '', 'cmbright': r'\usepackage{cmbright}', 'pazo': r'\usepackage{mathpazo}'}


def render_tex(src, font, cache, dpi=125, ss=4, width_in=6.55):
    """the caption as an alpha mask (PIL 'L'), cached by its source"""
    key = hashlib.sha1(('%s|%s|%d|%g' % (font, src, dpi, width_in)).encode()).hexdigest()[:16]
    fn = os.path.join(cache, key + '.png')
    if os.path.exists(fn):
        return Image.open(fn).convert('L')
    tex = (r'\documentclass[12pt,border=1.5pt,varwidth=%gin]{standalone}' '\n'
           r'\usepackage{amssymb}' '\n%s\n'
           r'\begin{document}\centering %s\end{document}' '\n') % (width_in, FONTS[font], src)
    d = tempfile.mkdtemp()
    open(os.path.join(d, 'c.tex'), 'w').write(tex)
    r = subprocess.run(['pdflatex', '-interaction=nonstopmode', 'c.tex'], cwd=d, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(os.path.join(d, 'c.pdf')):
        sys.exit('LaTeX failed on: %s\n%s' % (src, r.stdout[-1500:]))
    subprocess.run(['gs', '-q', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha', '-r%d' % (dpi*ss),
                    '-dTextAlphaBits=4', '-dGraphicsAlphaBits=4', '-sOutputFile=c.png', 'c.pdf'], cwd=d, check=True)
    im = Image.open(os.path.join(d, 'c.png')).convert('RGBA')
    a = im.split()[3]
    a = a.resize((max(1, a.width // ss), max(1, a.height // ss)), Image.LANCZOS)
    a.save(fn)
    shutil.rmtree(d)
    return a


# ---------------------------------------------------------------- the caption timeline

def timeline(inp):
    """[(f0, f1, fade_in, fade_out, key, line)] for the whole film, line 2 above line 1; and the
    parts: the first frame of the 3D half, the first frame of the ending, the last frame"""
    caps = []
    # the 2D half, from the S^2 program's storyboard (read only)
    sys.path.insert(0, M2D)
    import render_film as RF
    B = json.load(open(inp['storyboard2d']))
    RF.TMAP = RF.load_timing(inp['timing2d'])
    for (f0, f1, fi, fo, text, st) in RF.caption_list(B):
        caps.append((f0, f1, fi, fo, text, 2 if st.get('y_from_bottom', 16) > 20 else 1))
    # the 3D half begins where the 2D half ends: make_movie.py runs it on to the storyboard's film_end
    k3 = RF.frame_at(RF.time_of(B.get('film_end') or B['shots'][-1]['end']))
    # the 3D half: one caption per segment of the script
    k = k3
    for ln in open(inp['script3d']):
        if ln.startswith('#') or not ln.strip():
            continue
        f = ln.split(None, 6)
        n = int(f[0])
        cap = f[6].strip() if len(f) > 6 else '-'
        if cap != '-':
            caps.append((k, k + n - 1, 8, 6, cap, 1))
        k += n
    # the ending
    L = {}
    for ln in open(inp['landmarks4d']):
        a, b = ln.split()
        L[a] = float(b)
    fr = lambda t: int(round(30*t))
    caps.append((fr(L['START']), fr(L['FINAL']) - 1, 12, 9, 'ENDING1', 1))
    caps.append((fr(L['FINAL']), fr(L['FADE1']) - 1, 12, 15, 'ENDING2', 1))
    if inp['credit']:
        caps.append((fr(L['CRED']), fr(L['END']) - 1, 12, 15, 'CREDITS', 1))
    return caps, (k3, fr(L['START']), fr(L['END']) - 1)


def active(caps, k):
    out = []
    for (f0, f1, fi, fo, key, line) in caps:
        if f0 <= k <= f1:
            a = min(1.0, (k - f0 + 1)/max(1, fi), (f1 - k + 1)/max(1, fo))
            out.append((key, line, max(0.0, a)))
    return out


# ---------------------------------------------------------------- compositing

def compose(img, items, font, color, cache, bottom=12, top=14, halo=0.85, panel=0.85, weight=0.8):
    """items: [(key, line, opacity)]; line 1 at the bottom centre, line 2 at the top centre.
    Behind the text: a soft-edged white panel (invisible on white; it keeps the captions legible
    over the dense close-ups) and a white halo.  The coverage is raised to the power `weight`,
    which gives Computer Modern's thin strokes a little more ink."""
    x = np.asarray(img.convert('RGB')).astype(np.float32)
    col = np.array([int(color[i:i+2], 16) for i in (0, 2, 4)], np.float32)
    for key, line, op in items:
        if key not in TEX:
            sys.exit('no LaTeX source for the caption: %r' % key)
        m = render_tex(TEX[key], font, cache)
        y0 = H - bottom - m.height if line == 1 else top          # line 2 (the cone order): top centre
        x0 = (W - m.width)//2
        a = (np.asarray(m).astype(np.float32)/255)**weight
        # the panel: the text's box with a margin, feathered
        PX, PY, F = 10, 4, 6
        bw, bh = m.width + 2*(PX + F), m.height + 2*(PY + F)
        pm = Image.new('L', (bw, bh), 0)
        pm.paste(255, (F, F, bw - F, bh - F))
        pm = np.asarray(pm.filter(ImageFilter.GaussianBlur(F/2))).astype(np.float32)/255*panel*op
        px0, py0 = x0 - PX - F, y0 - PY - F
        ya, yb, xa, xb = max(0, py0), min(H, py0 + bh), max(0, px0), min(W, px0 + bw)
        sl = x[ya:yb, xa:xb]
        pp = pm[ya - py0:yb - py0, xa - px0:xb - px0]
        sl[:] = sl*(1 - pp[..., None]) + 255*pp[..., None]
        # the halo, then the text
        hm = np.asarray(m.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(1.6))).astype(np.float32)/255
        hm = np.minimum(1, 1.6*hm)*halo*op
        a = a*op
        sl = x[y0:y0 + m.height, x0:x0 + m.width]
        sl[:] = sl*(1 - hm[..., None]) + 255*hm[..., None]
        sl[:] = sl*(1 - a[..., None]) + col*a[..., None]
    return Image.fromarray(np.clip(x + 0.5, 0, 255).astype(np.uint8))


def source_frame(k, dirs, parts):
    d2, d3, d4 = dirs
    d = d2 if k < parts[0] else (d3 if k < parts[1] else d4)
    return Image.open(os.path.join(d, 'frame_%05d.png' % k))


def read_inputs(a):
    """the files the captions come from, and the credit's LaTeX source (None without --credit)"""
    timing2d = a.timing2d
    if not timing2d:
        named = json.load(open(a.storyboard2d)).get('timing')
        if not named:
            sys.exit('no timing map for the 2D half: pass --timing2d')
        timing2d = os.path.join(os.path.dirname(os.path.abspath(a.storyboard2d)), named)
    credit = None
    if a.credit:
        credit = open(a.credit).read().strip()
        if not credit:
            sys.exit('%s: the credit file is empty' % a.credit)
    return dict(storyboard2d=a.storyboard2d, timing2d=timing2d, script3d=a.script3d, landmarks4d=a.landmarks4d,
                credit=credit)


def _work(args):
    ks, dirs, out, font, color, cache, inp = args
    if inp['credit']:
        TEX['CREDITS'] = inp['credit']
    caps, parts = timeline(inp)
    for k in ks:
        compose(source_frame(k, dirs, parts), active(caps, k), font, color, cache).save(os.path.join(out, 'frame_%05d.png' % k), compress_level=1)
    return len(ks)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--frames', nargs=3, required=True, help='caption-free frames: the 2D half, the 3D half, the ending')
    ap.add_argument('--storyboard2d', default=os.path.join(M2D, 'storyboard.json'), help="the 2D half's storyboard")
    ap.add_argument('--timing2d', default=None, help="the 2D half's timing map (default: the storyboard's, if it names one)")
    ap.add_argument('--script3d', required=True, help="the 3D half's script (make_script.py)")
    ap.add_argument('--landmarks4d', required=True, help="the ending's landmarks (make_landmarks4d.py)")
    ap.add_argument('--credit', default=None, help='a file holding the LaTeX source of the credit line (no credit without it)')
    ap.add_argument('--out', default=None)
    ap.add_argument('--sample', nargs=2, default=None, metavar=('K', 'OUT.png'))
    ap.add_argument('--font', default='cm')
    ap.add_argument('--color', default='2c3e50')
    ap.add_argument('--cache', default=os.path.join(tempfile.gettempdir(), 'zipper_captions'))
    ap.add_argument('--first', type=int, default=0)
    ap.add_argument('--last', type=int, default=None, help='the last frame (default: the last of the film, END less one)')
    ap.add_argument('--jobs', type=int, default=4)
    a = ap.parse_args()
    inp = read_inputs(a)
    if inp['credit']:
        TEX['CREDITS'] = inp['credit']
    os.makedirs(a.cache, exist_ok=True)
    caps, parts = timeline(inp)
    for key in sorted({c[4] for c in caps}):          # typeset every caption once, before the workers start
        if key not in TEX:
            sys.exit('no LaTeX source for the caption: %r' % key)
        render_tex(TEX[key], a.font, a.cache)
    if a.sample:
        k = int(a.sample[0])
        compose(source_frame(k, a.frames, parts), active(caps, k), a.font, a.color, a.cache).save(a.sample[1])
        print('wrote', a.sample[1], [c[0][:40] for c in active(caps, k)])
        return
    if not a.out:
        sys.exit('no output folder: pass --out OUTDIR (or --sample K OUT.png)')
    os.makedirs(a.out, exist_ok=True)
    ks = list(range(a.first, (parts[2] if a.last is None else a.last) + 1))
    chunks = [(ks[i::a.jobs], a.frames, a.out, a.font, a.color, a.cache, inp) for i in range(a.jobs)]
    import multiprocessing as mp
    with mp.get_context('spawn').Pool(a.jobs) as pool:
        n = sum(pool.map(_work, chunks))
    print('wrote %d frames to %s' % (n, a.out))


if __name__ == '__main__':
    main()
