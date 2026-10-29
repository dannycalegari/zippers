#!/usr/bin/env python3
"""render_film.py -- render the 2D half of the zipper film from storyboard.json.

  python3 render_film.py [--timing T.json] [--audio A.wav] [--work DIR] [-j N]
                         [--only ID,ID] [--stills] [--film OUT.mp4] [--no-encode] [--force]

Every time in storyboard.json is a musical landmark (a name from music/landmarks.json, a bar
'bar12' or a half-note beat 'bar12.2' of the timing map), never a clock time.  The timing map
(--timing T.json, or the storyboard's "timing") turns landmarks into seconds; re-timing to
another recording is: python3 music/align.py NEW.wav music/timing_NEW.json --last-bar 46,
then  python3 render_film.py --timing music/timing_NEW.json --audio NEW.wav.

Steps:
  1. each shot is written as a zmovie shot file (WORK/shots/ID.txt) and rendered to
     WORK/raw/ID/frame_NNNNN.png, numbered by the frame of the film (frame k shows at k/fps);
     a crossfade of N frames centred on a landmark extends both shots by N/2 frames;
  2. overlays (stills composited over a shot, e.g. the ghost arcs of the T-morph) are
     rendered once each (WORK/overlays) and multiplied onto the shot's frames;
  3. the film is composed frame by frame: crossfades, overlays, captions (in the 3D film's
     style, caption.py) -> WORK/final;
  4. encode_movie, then the music (AAC, passed through by add_audio).
--stills renders only the stills listed per shot (WORK/stills) and the contact sheets
(storyboard/ next to this file).  Frames, WAVs and AAC files stay in WORK (the scratch
directory), never in the repository.

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import argparse, hashlib, json, math, os, shutil, subprocess, sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, 'bin')
SRC = os.path.join(HERE, 'src')
sys.path.insert(0, SRC)
import caption as CAP  # noqa: E402

SCRATCH = os.environ.get('SCRATCH')    # the scratch folder (frames, WAVs, AAC files), from the
                                       # environment: --work defaults to SCRATCH/film2d; without
                                       # SCRATCH, --work is needed

# ---------------------------------------------------------------- timing (as zmovie reads it)
TMAP = {}
FPS = 30


def load_timing(path):
    d = json.load(open(path))
    off = d.get('offset_s', 0.0)
    m = {}
    L = d.get('landmarks', {})
    if isinstance(L, dict):
        for k, v in L.items():
            m[k] = (v['t'] if isinstance(v, dict) else v) + off
    else:
        for x in L:
            m[x['name']] = x['t'] + off
    for k, v in d.get('bars', {}).items():
        m['bar' + k] = v + off
    for x in d.get('beats', []):
        m['bar%d.%s' % (x['bar'], ('%g' % x['beat']))] = x['t'] + off
    return m


def time_of(tok):
    tok = str(tok).lstrip('@')
    try:
        return float(tok)
    except ValueError:
        pass
    name, off = tok, 0.0
    p = max(tok.rfind('+'), tok.rfind('-'))
    if p > 0:
        try:
            off = float(tok[p + 1:]) * (-1 if tok[p] == '-' else 1)
            name = tok[:p]
        except ValueError:
            pass
    if name not in TMAP:
        sys.exit('unknown landmark %r' % name)
    return TMAP[name] + off


def frame_at(t):
    return int(math.floor(t * FPS + 0.5))


# ---------------------------------------------------------------- tracks (overlay opacities)
def smooth(s):
    s = min(1.0, max(0.0, s))
    return s * s * (3 - 2 * s)


def track(spec, k):
    """value at film frame k of a track 'K:v[:ease] ...' whose keys are @landmarks."""
    spec = str(spec).split()
    if len(spec) == 1 and ':' not in spec[0]:
        return float(spec[0])
    keys = []
    for x in spec:
        f = x.split(':')
        keys.append((frame_at(time_of(f[0])), float(f[1]), f[2] if len(f) > 2 else 'smooth'))
    if k <= keys[0][0]:
        return keys[0][1]
    for (f0, v0, _), (f1, v1, e) in zip(keys, keys[1:]):
        if k < f1:
            s = (k - f0) / max(1, f1 - f0)
            s = {'linear': s, 'hold': 0.0}.get(e, smooth(s))
            return v0 + (v1 - v0) * s
    return keys[-1][1]


# ---------------------------------------------------------------- the storyboard
def load_board(path):
    B = json.load(open(path))
    shots = B['shots']
    for i, s in enumerate(shots):
        s['F0'] = frame_at(time_of(s['start']))
        s['F1'] = frame_at(time_of(s['end']))
        if s['F1'] <= s['F0']:
            sys.exit('shot %s: empty' % s['id'])
        if i and s['F0'] != shots[i - 1]['F1']:
            sys.exit('shot %s does not start where %s ends' % (s['id'], shots[i - 1]['id']))
        tr = s.get('transition_in', {'type': 'cut'})
        s['xin'] = int(tr.get('frames', 0)) if tr['type'] == 'crossfade' else 0
        if s['xin'] % 2:
            sys.exit('shot %s: crossfade frames must be even' % s['id'])
    for i, s in enumerate(shots):
        s['xout'] = shots[i + 1]['xin'] if i + 1 < len(shots) else 0
        s['R0'] = s['F0'] - s['xin'] // 2          # the rendered span [R0, R1)
        s['R1'] = s['F1'] + s['xout'] // 2
    return B


def engine_keys(B, scene, extra):
    d = {}
    d.update(B.get('defaults', {}))
    if scene:
        d.update(B['scenes'][scene])
    d.update(extra or {})
    return d


def shot_file(B, s, work, timing):
    d = engine_keys(B, s.get('scene'), s.get('engine'))
    lines = ['# generated by render_film.py from storyboard.json: shot %s' % s['id'],
             'timing = %s' % timing, 'fps = %d' % FPS,
             'start = %.6f' % (s['R0'] / FPS), 'end = %.6f' % (s['R1'] / FPS)]
    lines += ['%s = %s' % (k, v) for k, v in d.items()]
    p = os.path.join(work, 'shots', s['id'] + '.txt')
    txt = '\n'.join(lines) + '\n'
    return p, txt


def still_file(B, ov, work, timing, name):
    d = engine_keys(B, ov.get('scene'), ov.get('engine'))
    lines = ['# overlay still %s' % name, 'timing = %s' % timing, 'frames = 1']
    lines += ['%s = %s' % (k, v) for k, v in d.items()]
    return os.path.join(work, 'overlays', name + '.txt'), '\n'.join(lines) + '\n'


def write_if_changed(p, txt):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    old = open(p).read() if os.path.exists(p) else None
    if old != txt:
        open(p, 'w').write(txt)
        return True
    return False


def run(cmd):
    r = subprocess.run(cmd)
    if r.returncode:
        sys.exit('failed: ' + ' '.join(cmd))


def render_shot(B, s, work, timing, jobs, force, frames=None):
    p, txt = shot_file(B, s, work, timing)
    changed = write_if_changed(p, txt)
    out = os.path.join(work, 'raw', s['id'])
    stamp = os.path.join(out, '.spec')
    h = hashlib.md5(txt.encode()).hexdigest()
    if frames is None:
        if not force and not changed and os.path.exists(stamp) and open(stamp).read() == h:
            return out
        if os.path.isdir(out):
            shutil.rmtree(out)
        os.makedirs(out)
        print('shot %s: frames %d..%d' % (s['id'], s['R0'], s['R1'] - 1), flush=True)
        run([os.path.join(BIN, 'zmovie'), p, '-o', out, '-j', str(jobs), '--quiet'])
        open(stamp, 'w').write(h)
    else:
        os.makedirs(out, exist_ok=True)
        if os.path.exists(stamp) and open(stamp).read() != h:   # stale frames of an old spec
            shutil.rmtree(out)
            os.makedirs(out)
        for k in frames:
            fn = os.path.join(out, 'frame_%05d.png' % k)
            if os.path.exists(fn) and not force and os.path.exists(stamp + '.partial') \
                    and open(stamp + '.partial').read() == h:
                continue
            r = k - s['R0']
            run([os.path.join(BIN, 'zmovie'), p, '-o', out, '--frames', '%d:%d' % (r, r), '--quiet'])
        open(stamp + '.partial', 'w').write(h)
    return out


def render_overlays(B, work, timing, force):
    paths = {}
    for s in B['shots']:
        for j, ov in enumerate(s.get('overlays', [])):
            name = '%s_%d' % (s['id'], j)
            p, txt = still_file(B, ov, work, timing, name)
            changed = write_if_changed(p, txt)
            od = os.path.join(work, 'overlays', name)
            fn = os.path.join(od, 'frame_00000.png')
            if force or changed or not os.path.exists(fn):
                os.makedirs(od, exist_ok=True)
                run([os.path.join(BIN, 'zmovie'), p, '-o', od, '--quiet'])
            paths[name] = fn
    return paths


# ---------------------------------------------------------------- composing a frame
_ov_cache = {}


def ov_image(fn):
    if fn not in _ov_cache:
        _ov_cache[fn] = np.asarray(Image.open(fn).convert('RGB')).astype(np.float32) / 255.0
    return _ov_cache[fn]


def shot_frame(ctx, s, k):
    fn = os.path.join(ctx['work'], 'raw', s['id'], 'frame_%05d.png' % k)
    img = np.asarray(Image.open(fn).convert('RGB')).astype(np.float32) / 255.0
    for j, ov in enumerate(s.get('overlays', [])):
        a = track(ov['opacity'], k)
        if a > 1e-4:
            g = ov_image(ctx['ov']['%s_%d' % (s['id'], j)])
            img = img * (1.0 - a * (1.0 - g))       # multiply: ink over ink darkens, as pen lines
    return img


def caption_list(B):
    caps = []
    for s in B['shots']:
        for c in s.get('captions', []):
            st = dict(CAP.STYLE)
            st.update(B.get('caption_style', {}))
            if c.get('line', 1) == 2:
                st['y_from_bottom'] = st['y_from_bottom'] + B.get('caption_line_gap', 24)
            f0, f1 = frame_at(time_of(c['start'])), frame_at(time_of(c['end'])) - 1
            caps.append((f0, f1, int(c.get('fade_in', 12)), int(c.get('fade_out', 12)), c['text'], st))
    return caps


_CTX = None


def _init_worker(tmap, ctx):
    global TMAP, _CTX
    TMAP = tmap
    _CTX = ctx


def _compose_k(k):
    return compose((_CTX, k))


def compose(args):
    ctx, k = args
    B = ctx['B']
    live = [s for s in B['shots'] if s['R0'] <= k < s['R1']]
    if len(live) == 1:
        img = shot_frame(ctx, live[0], k)
    else:
        a, b = live[0], live[1]
        n = b['xin']
        w = smooth((k - b['R0'] + 1) / (n + 1))
        img = shot_frame(ctx, a, k) * (1 - w) + shot_frame(ctx, b, k) * w
    for c in ctx['caps']:
        al = CAP.caption_alpha(k, *c[:4])
        if al > 0:
            CAP.overlay(img, c[4], al, c[5])
    out = Image.fromarray(np.clip(img * 255.0 + 0.5, 0, 255).astype(np.uint8))
    out.save(os.path.join(ctx['outdir'], 'frame_%05d.png' % k), compress_level=1)
    return k


def tail_frames(ctx, last, n):
    """the preview's tail: the last frame, fading to white (the 3D film starts here)."""
    img = np.asarray(Image.open(os.path.join(ctx['outdir'], 'frame_%05d.png' % last)).convert('RGB')).astype(np.float32) / 255
    for i in range(n):
        w = smooth((i + 1) / n)
        o = img * (1 - w) + w
        Image.fromarray(np.clip(o * 255 + 0.5, 0, 255).astype(np.uint8)).save(
            os.path.join(ctx['outdir'], 'frame_%05d.png' % (last + 1 + i)), compress_level=1)


# ---------------------------------------------------------------- contact sheets
def contact_sheet(B, s, files, out, size=280):
    font = ImageFont.truetype(CAP.FONT, 13)
    lab = 34
    W = Image.new('RGB', (len(files) * (size + 6) + 6, size + lab + 6), 'white')
    d = ImageDraw.Draw(W)
    for i, (fn, label) in enumerate(files):
        im = Image.open(fn).convert('RGB').resize((size, size), Image.LANCZOS)
        x = 6 + i * (size + 6)
        W.paste(im, (x, 6))
        d.rectangle([x - 1, 5, x + size, 6 + size], outline=(190, 190, 190))
        d.text((x, size + 10), label, font=font, fill=(0, 0, 0))
    W.save(out, optimize=True)


def describe_time(tok):
    t = time_of(tok)
    return '%s (%.2f s)' % (str(tok).lstrip('@'), t)


# ---------------------------------------------------------------- main
def main():
    global TMAP
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--board', default=os.path.join(HERE, 'storyboard.json'))
    ap.add_argument('--timing', default=None)
    ap.add_argument('--audio', default=None, help='the recording, a 16-bit PCM WAV (to encode the film)')
    ap.add_argument('--work', default=os.path.join(SCRATCH, 'film2d') if SCRATCH else None)
    ap.add_argument('--film', default=None)
    ap.add_argument('-j', type=int, default=6)
    ap.add_argument('--only', default=None)
    ap.add_argument('--stills', action='store_true')
    ap.add_argument('--no-encode', action='store_true')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--tail', type=float, default=None, help='preview tail (s) after the last landmark: '
                    'hold the last frame and fade to white (default: up to the end of the audio)')
    a = ap.parse_args()
    B = json.load(open(a.board))
    timing = os.path.abspath(a.timing or os.path.join(HERE, B.get('timing') or sys.exit('no timing map: pass --timing')))
    TMAP = load_timing(timing)
    B = load_board(a.board)
    work = a.work or sys.exit('no scratch folder: pass --work DIR, or set SCRATCH')
    os.makedirs(work, exist_ok=True)
    shots = B['shots']
    only = set(a.only.split(',')) if a.only else None
    print('timing %s: %d shots, frames 0..%d (%.3f s)' % (timing, len(shots), shots[-1]['F1'] - 1, shots[-1]['F1'] / FPS))
    for s in shots:
        print('  %-14s %-8s -> %-8s frames %4d..%4d  %5.2f s  in: %s' % (
            s['id'], s['start'], s['end'], s['F0'], s['F1'] - 1, (s['F1'] - s['F0']) / FPS,
            'crossfade %d' % s['xin'] if s['xin'] else 'cut'))
    ovp = render_overlays(B, work, timing, a.force)
    caps = caption_list(B)
    for c in caps:   # a caption must fit in the frame
        m, box, _ = CAP.text_mask(c[4], 840, 840, c[5])
        if box[3] > 830:
            print('WARNING: caption too wide (%d px): %s' % (box[3], c[4]))

    if a.stills:
        sd = os.path.join(work, 'stills')
        os.makedirs(sd, exist_ok=True)
        sheets = os.path.join(HERE, 'storyboard')
        os.makedirs(sheets, exist_ok=True)
        ctx = dict(B=B, work=work, ov=ovp, caps=caps, outdir=sd)
        for s in shots:
            if only and s['id'] not in only:
                continue
            ks = [(frame_at(time_of(t)), t) for t in s.get('stills', [])]
            ks = [(min(max(k, s['F0']), s['F1'] - 1), t) for k, t in ks]
            render_shot(B, s, work, timing, a.j, a.force, frames=[k for k, _ in ks])
            files = []
            for k, t in ks:
                img = shot_frame(ctx, s, k)
                for c in caps:
                    al = CAP.caption_alpha(k, *c[:4])
                    if al > 0:
                        CAP.overlay(img, c[4], al, c[5])
                fn = os.path.join(sd, '%s_%05d.png' % (s['id'], k))
                Image.fromarray(np.clip(img * 255 + 0.5, 0, 255).astype(np.uint8)).save(fn)
                files.append((fn, '%s  (%.2f s, frame %d)' % (str(t).lstrip('@'), k / FPS, k)))
            contact_sheet(B, s, files, os.path.join(sheets, '%s.png' % s['id']))
            print('stills of %s: %s' % (s['id'], ', '.join(f for f, _ in files)))
        return

    for s in shots:
        if only and s['id'] not in only:
            continue
        render_shot(B, s, work, timing, a.j, a.force)
    if only:
        return
    final = os.path.join(work, 'final')
    if os.path.isdir(final):
        shutil.rmtree(final)
    os.makedirs(final)
    nf = shots[-1]['F1']
    ctx = dict(B=B, work=work, ov=ovp, caps=caps, outdir=final)
    import multiprocessing as mp
    # spawn, not fork: forked workers hung on the overlay frames (macOS)
    with mp.get_context('spawn').Pool(a.j, initializer=_init_worker, initargs=(TMAP, ctx)) as pool:
        for i, _ in enumerate(pool.imap_unordered(_compose_k, range(nf), chunksize=8)):
            if i % 250 == 0:
                print('composed %d / %d' % (i, nf), flush=True)
    tail = a.tail
    if tail is None:
        tail = B.get('preview_tail_s', 0.0)
    nt = int(round(tail * FPS))
    if nt > 0:
        tail_frames(ctx, nf - 1, nt)
    print('final frames: %d (%.3f s) in %s' % (nf + nt, (nf + nt) / FPS, final))
    if a.no_encode:
        return
    film = a.film or os.path.join(HERE, B.get('film', 'film_2d_half.mp4'))
    video = os.path.join(work, 'video.mp4')
    run([os.path.join(BIN, 'encode_movie'), final, video, str(FPS), str(B.get('bitrate_mbit', 8))])
    dur = (nf + nt) / FPS
    wav = os.path.join(work, 'music.wav')
    m4a = os.path.join(work, 'music.m4a')
    run(['python3', os.path.join(SRC, 'cut_audio.py'), a.audio or sys.exit('no audio: pass --audio'), wav, '0', '%.3f' % (dur + 0.1), '0',
         '%.3f' % B.get('audio_fade_out_s', 0.0)])
    if os.path.exists(m4a):
        os.remove(m4a)
    run(['afconvert', '-f', 'm4af', '-d', 'aac', '-b', '256000', wav, m4a])
    run([os.path.join(BIN, 'add_audio'), video, m4a, film, '0', '-1'])
    run([os.path.join(BIN, 'probe_movie'), film])


if __name__ == '__main__':
    main()
