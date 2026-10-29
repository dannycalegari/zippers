#!/usr/bin/env python3
"""make_movie.py -- the one command that makes the 2D half of the zipper film.

  python3 make_movie.py [--timing music/timing_REC.json] [--audio REC.wav] [--out FILM.mp4]
                        [--board storyboard.json] [--work DIR] [-j N] [--contact SHEET.png]
                        [--force] [--no-check]

It reads storyboard.json (every time in it is a musical landmark, a bar 'barN' or a half-note
beat 'barN.B', never a clock time), a timing map (landmark -> seconds in one recording, written
by music/align.py) and that recording, and then

  1. renders every shot with the frame engine (bin/zmovie) into WORK/raw/SHOT/ (frame k of the
     film shows at k/30 s; a shot is re-rendered only when its generated shot file changes);
  2. composes the film frame by frame (crossfades centred on their landmarks, overlays,
     captions) into WORK/final/;
  3. encodes at 30 fps (bin/encode_movie) and adds the recording (AAC, passed through by
     bin/add_audio; the encoder's B-frame delay and the AAC priming are both trimmed by edit
     lists, so film time 0 is audio time 0);
  4. checks the result (unless --no-check): every transition frame against its landmark time,
     the frames actually decoded from the MP4 at those times, the soundtrack decoded from the
     MP4 against the recording (A/V offset), and the onsets of landmark-keyed animation
     events found in the pixels;
  5. writes a contact sheet of 24 evenly spaced frames of the encoded film.

The film runs from the start of the recording to the storyboard's 'film_end' landmark
(C2next: the first new note after the cadence on the downbeat of bar 46).  The last shot holds
its final picture under the cadence chord; the 3D half begins at C2next.

Re-timing for another recording is therefore:

  python3 music/align.py NEW.wav music/timing_NEW.json --last-bar 46
  python3 make_movie.py --timing music/timing_NEW.json --audio NEW.wav --out NEW_2d_half.mp4

(if NEW.wav is the whole fugue, --last-bar 101 also times the second half's landmarks).
Frames, WAVs and AAC files stay in WORK (the scratch directory), never in the repository.

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import argparse, json, math, os, re, shutil, subprocess, sys, time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'src'))
import render_film as RF          # noqa: E402  (shot files, rendering, composing)
import caption as CAP             # noqa: E402
from cut_audio import read_wav, write_wav   # noqa: E402

BIN = os.path.join(HERE, 'bin')
SCRATCH = RF.SCRATCH
FPS = RF.FPS


def run(cmd, capture=False):
    r = subprocess.run(cmd, capture_output=capture, text=True)
    if r.returncode:
        sys.exit('failed: %s\n%s' % (' '.join(cmd), (r.stdout or '') + (r.stderr or '') if capture else ''))
    return r.stdout if capture else None


# ---------------------------------------------------------------- the soundtrack
def make_soundtrack(audio, offset, dur, fade_out, out_wav):
    """the recording on the film's clock: film time t = audio time t + offset (timing map)."""
    x, rate = read_wav(audio)
    n = int(round((dur + 0.1) * rate))          # a little beyond the picture: add_audio trims to it
    y = np.zeros((n, x.shape[1]), dtype=np.float64)
    s0 = int(round(offset * rate))                 # film sample of audio sample 0
    a0, b0 = max(0, -s0), max(0, s0)
    m = min(len(x) - a0, n - b0)
    if m <= 0:
        sys.exit('the audio does not cover the film')
    y[b0:b0 + m] = x[a0:a0 + m]
    if len(x) - a0 < int(dur * rate):
        print('WARNING: the recording ends %.3f s before the picture' % (dur - (len(x) - a0) / rate))
    ne = int(round(dur * rate))
    if fade_out > 0:                               # a short fade ending with the picture (no click)
        nf = int(round(fade_out * rate))
        w = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, nf))
        y[ne - nf:ne] *= w[:, None]
        y[ne:] = 0
    write_wav(out_wav, np.clip(np.round(y), -32768, 32767).astype(np.int16), rate)
    return rate


# ---------------------------------------------------------------- checks
def ncc_lag(ref, seg, maxlag):
    """lag (samples) of seg inside ref, ref = audio[i - maxlag : i + len(seg) + maxlag]."""
    c = np.correlate(ref, seg, 'valid')
    e = np.sqrt(np.convolve(ref * ref, np.ones(len(seg)), 'valid')) * np.sqrt((seg * seg).sum()) + 1e-9
    c = c / e
    j = int(np.argmax(c))
    return j - maxlag, float(c[j])


def check_audio(film, ref_wav, work, dur):
    dec = os.path.join(work, 'check_audio.wav')
    run([os.path.join(BIN, 'extract_audio'), film, dec], capture=True)
    a, ra = read_wav(ref_wav)
    b, rb = read_wav(dec)
    if ra != rb:
        return dict(ok=False, note='sample rates differ (%d, %d)' % (ra, rb))
    a = a.astype(np.float64).mean(1)
    b = b.astype(np.float64).mean(1)
    lags, w, L = [], 8192, 4410                      # 0.19 s windows, search +-100 ms
    for t in np.arange(1.0, dur - 0.5, 2.0):
        i = int(t * ra)
        seg = b[i:i + w]
        if (seg * seg).mean() < 1e4:
            continue
        lag, c = ncc_lag(a[i - L:i + w + L], seg, L)
        if c > 0.9:
            lags.append(-lag / ra)                     # > 0: the film's audio is late
    lags = np.array(lags)
    med = float(np.median(lags))
    tail = len(b) / rb
    return dict(windows=len(lags), median_ms=1000 * med, max_abs_ms=1000 * float(np.abs(lags).max()),
                frac_exact=float(np.mean(np.abs(lags) < 0.5e-3)), decoded_s=tail, ok=abs(med) < 1.0 / FPS / 2)


def grab(film, times, d):
    """decode the film at these times (s): one grab_frames call per time (its file names
    keep only 0.1 s, so frames less than 0.1 s apart would overwrite each other)."""
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    got = []
    for i, t in enumerate(times):
        tmp = os.path.join(d, 'tmp')
        os.makedirs(tmp, exist_ok=True)
        out = run([os.path.join(BIN, 'grab_frames'), film, tmp, '%.6f' % t], capture=True)
        m = re.search(r'(\S+)\s+\(asked ([\d.]+) s, got ([\d.]+) s\)', out)
        if not m:
            sys.exit('grab_frames: no frame at %.3f s' % t)
        fn = os.path.join(d, 'g_%04d.png' % i)
        os.rename(m.group(1), fn)
        got.append((fn, float(m.group(2)), float(m.group(3))))
    return got


def load_png(fn):
    return np.asarray(Image.open(fn).convert('RGB')).astype(np.float32)


def check_frames(film, final, ks, work):
    """for each k: decode the MP4 a quarter of a frame after k/fps and find which composed frame it is."""
    got = grab(film, [(k + 0.25) / FPS for k in ks], os.path.join(work, 'check_grab'))
    res = []
    for k, (fn, asked, actual) in zip(ks, got):
        g = load_png(fn)
        es = []
        for d in (-2, -1, 0, 1, 2):
            p = os.path.join(final, 'frame_%05d.png' % (k + d))
            if os.path.exists(p):
                es.append((float(np.abs(load_png(p) - g).mean()), d))
        es.sort()
        e0 = dict((d, e) for e, d in es)[0]
        # frame k, or a tie with it (a still picture: neighbouring frames are the same image)
        shown = k if e0 <= 1.02 * es[0][0] + 0.05 else k + es[0][1]
        res.append(dict(k=k, shown=shown, err=e0, next=min(e for e, d in es if d != 0), pts=actual))
    return res


def red_ink(fn):
    im = load_png(fn)
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    return int(((r - g > 60) & (r - b > 60)).sum())


def wheel_ink(fn):
    im = load_png(fn)
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    return int(((g - r > 50) & (g - b > 50)).sum())


def first_change(final, k0, k1, fn, thresh):
    """the first frame in [k0, k1) where fn(frame) exceeds fn(frame k0 - 1) by thresh pixels."""
    base = fn(os.path.join(final, 'frame_%05d.png' % (k0 - 1)))
    for k in range(k0 - 1, k1):
        if fn(os.path.join(final, 'frame_%05d.png' % k)) - base > thresh:
            return k
    return None


# ---------------------------------------------------------------- contact sheet
def bar_beat(tmap, t):
    bars = sorted((v, int(k[3:])) for k, v in tmap.items() if re.fullmatch(r'bar\d+', k))
    bt = sorted((v, k) for k, v in tmap.items() if re.fullmatch(r'bar\d+\.\d+', k))
    lab = '-'
    for v, k in bt:
        if v <= t + 1e-6:
            lab = k[3:]
    if t < bars[0][0]:
        lab = 'before bar 1'
    else:
        b, s = lab.split('.')
        lab = 'bar %s, beat %s' % (b, s)
    return lab


def contact_sheet(film, B, tmap, nf, out, work, n=24, cols=6, size=300):
    ks = [int((i + 0.5) * nf / n) for i in range(n)]
    got = grab(film, [(k + 0.25) / FPS for k in ks], os.path.join(work, 'contact_grab'))
    font = ImageFont.truetype(CAP.FONT, 13)
    lab = 36
    rows = (n + cols - 1) // cols
    W = Image.new('RGB', (cols * (size + 8) + 8, rows * (size + lab + 8) + 8), 'white')
    d = ImageDraw.Draw(W)
    for i, (k, (fn, _, _)) in enumerate(zip(ks, got)):
        x = 8 + (i % cols) * (size + 8)
        y = 8 + (i // cols) * (size + lab + 8)
        W.paste(Image.open(fn).convert('RGB').resize((size, size), Image.LANCZOS), (x, y))
        d.rectangle([x - 1, y - 1, x + size, y + size], outline=(190, 190, 190))
        shot = [s['id'] for s in B['shots'] if s['F0'] <= k < s['F1']][0]
        d.text((x, y + size + 4), '%s   frame %d' % (shot, k), font=font, fill=(0, 0, 0))
        d.text((x, y + size + 19), '%.2f s   %s' % (k / FPS, bar_beat(tmap, k / FPS)), font=font, fill=(90, 90, 90))
    W.save(out, optimize=True)


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--board', default=os.path.join(HERE, 'storyboard.json'))
    ap.add_argument('--timing', default=None, help='timing map (default: the storyboard\'s "timing", if it names one)')
    ap.add_argument('--audio', default=None, help='the recording (default: the timing map\'s audio_file)')
    ap.add_argument('--out', default=None, help='the film (default: movie/zippers_2d_first_half_REC.mp4)')
    ap.add_argument('--work', default=os.path.join(SCRATCH, 'frames') if SCRATCH else None, help='the scratch folder (default: $SCRATCH/frames)')
    ap.add_argument('--contact', default=None, help='contact sheet (default: next to the film, _contact.png)')
    ap.add_argument('-j', type=int, default=6)
    ap.add_argument('--force', action='store_true', help='re-render every shot')
    ap.add_argument('--no-check', action='store_true')
    ap.add_argument('--check-only', action='store_true', help='only check an existing film (and remake the contact sheet)')
    ap.add_argument('--audio-fade-out', type=float, default=0.04, help='seconds, ending with the picture')
    a = ap.parse_args()
    T0 = time.time()

    B0 = json.load(open(a.board))
    timing = os.path.abspath(a.timing or os.path.join(HERE, B0.get('timing') or sys.exit('no timing map: pass --timing')))
    TJ = json.load(open(timing))
    RF.TMAP = RF.load_timing(timing)
    tmap = RF.TMAP
    audio = a.audio or TJ.get('audio_file')
    if not audio or not os.path.exists(audio):
        sys.exit('no audio: pass --audio')
    if os.path.basename(audio) != os.path.basename(TJ.get('audio_file', audio)):
        print('WARNING: the timing map was made from %s, not %s' % (TJ.get('audio_file'), audio))
    rec = re.sub(r'^timing_', '', os.path.splitext(os.path.basename(timing))[0])
    film = os.path.abspath(a.out or os.path.join(HERE, 'zippers_2d_first_half_%s.mp4' % rec))
    contact = a.contact or os.path.splitext(film)[0] + '_contact.png'
    work = a.work or sys.exit('no scratch folder: pass --work DIR, or set SCRATCH')
    os.makedirs(work, exist_ok=True)

    B = RF.load_board(a.board)
    shots = B['shots']
    end_lm = B.get('film_end')
    if end_lm:           # the last shot runs on to the next note, holding its picture
        if end_lm not in tmap:
            sys.exit('the timing map has no landmark %s (re-run music/align.py with the current landmarks.json)' % end_lm)
        shots[-1]['F1'] = shots[-1]['R1'] = RF.frame_at(RF.time_of(end_lm))
    nf = shots[-1]['F1']
    dur = nf / FPS
    print('storyboard %s\ntiming %s\naudio %s\n%d shots, frames 0..%d (%.3f s)' % (a.board, timing, audio, len(shots), nf - 1, dur))
    for s in shots:
        print('  %-14s %-8s -> %-8s frames %4d..%4d  %5.2f s  in: %s' % (
            s['id'], s['start'], s['end'] if s is not shots[-1] or not end_lm else end_lm,
            s['F0'], s['F1'] - 1, (s['F1'] - s['F0']) / FPS, 'crossfade %d' % s['xin'] if s['xin'] else 'cut'))

    if a.check_only:
        final = os.path.join(work, 'final')
        wav = os.path.join(work, 'music.wav')
        R = checks(B, shots, tmap, film, final, wav, work, dur, end_lm)
        contact_sheet(film, B, tmap, nf, contact, work)
        print('\n'.join(R + ['contact sheet %s' % contact]))
        return

    # 1. render
    t1 = time.time()
    ovp = RF.render_overlays(B, work, timing, a.force)
    for s in shots:
        RF.render_shot(B, s, work, timing, a.j, a.force)
    t_render = time.time() - t1

    # 2. compose
    t1 = time.time()
    caps = RF.caption_list(B)
    for c in caps:
        if CAP.text_mask(c[4], 840, 840, c[5])[1][3] > 830:
            print('WARNING: caption too wide: %s' % c[4])
    final = os.path.join(work, 'final')
    if os.path.isdir(final):
        shutil.rmtree(final)
    os.makedirs(final)
    ctx = dict(B=B, work=work, ov=ovp, caps=caps, outdir=final)
    import multiprocessing as mp
    with mp.get_context('spawn').Pool(a.j, initializer=RF._init_worker, initargs=(tmap, ctx)) as pool:
        for i, _ in enumerate(pool.imap_unordered(RF._compose_k, range(nf), chunksize=8)):
            if i % 500 == 0:
                print('composed %d / %d' % (i, nf), flush=True)
    t_compose = time.time() - t1

    # 3. encode, then the music
    t1 = time.time()
    video = os.path.join(work, 'video.mp4')
    run([os.path.join(BIN, 'encode_movie'), final, video, str(FPS), str(B.get('bitrate_mbit', 8))])
    wav = os.path.join(work, 'music.wav')
    m4a = os.path.join(work, 'music.m4a')
    make_soundtrack(audio, TJ.get('offset_s', 0.0), dur, a.audio_fade_out, wav)
    if os.path.exists(m4a):
        os.remove(m4a)
    run(['afconvert', '-f', 'm4af', '-d', 'aac', '-b', '256000', wav, m4a])
    run([os.path.join(BIN, 'add_audio'), video, m4a, film, '0', '-1'])
    t_encode = time.time() - t1
    probe = run([os.path.join(BIN, 'probe_movie'), film], capture=True)
    print(probe)

    report = ['film %s' % film, 'timing %s' % timing, 'audio %s' % audio,
              'frames %d (%.3f s), last frame %d at %.3f s' % (nf, dur, nf - 1, (nf - 1) / FPS),
              'render %.0f s, compose %.0f s, encode + audio %.0f s' % (t_render, t_compose, t_encode)]
    if not a.no_check:
        report += checks(B, shots, tmap, film, final, wav, work, dur, end_lm)
        contact_sheet(film, B, tmap, nf, contact, work)
        report.append('contact sheet %s' % contact)
    report.append('total %.0f s; film %.1f MB' % (time.time() - T0, os.path.getsize(film) / 1e6))
    txt = '\n'.join(report) + '\n'
    open(os.path.join(work, 'make_movie_report.txt'), 'w').write(txt + '\n' + probe)
    print(txt)


def checks(B, shots, tmap, film, final, wav, work, dur, end_lm):
    R = ['', 'SYNC (frame k shows during [k/30, (k+1)/30) s; a landmark at t falls on frame round(30 t))']
    # (a) the schedule: each transition against its landmark
    rows, ks = [], []
    for s in shots[1:]:
        t = RF.time_of(s['start'])
        k = s['F0']
        if s['xin']:
            centre = k - 0.5               # the crossfade's 50% point lies between frames k-1 and k
            R.append('  %-14s %-6s %8.3f s  crossfade %d frames, frames %d..%d, 50%% at %.3f s: %+.1f ms' % (
                s['id'], s['start'], t, s['xin'], s['R0'], s['F0'] + s['xin'] // 2 - 1, centre / FPS,
                1000 * (centre / FPS - t)))
            ks += [k - 1, k]
        else:
            R.append('  %-14s %-6s %8.3f s  cut: first frame %d starts at %.3f s: %+.1f ms' % (
                s['id'], s['start'], t, k, k / FPS, 1000 * (k / FPS - t)))
            ks += [k - 1, k]
        rows.append(abs(k / FPS - t) <= 1.0 / FPS)
    if end_lm:
        t = RF.time_of(end_lm)
        R.append('  end            %-6s %8.3f s  film ends after frame %d, at %.3f s: %+.1f ms' % (
            end_lm, t, shots[-1]['F1'] - 1, shots[-1]['F1'] / FPS, 1000 * (shots[-1]['F1'] / FPS - t)))
    R.append('  all transitions within one frame of their landmark: %s' % all(rows))
    # (b) what the MP4 actually shows at those frames
    fr = check_frames(film, final, sorted(set(ks + list(range(0, shots[-1]['F1'], 211)) + [shots[-1]['F1'] - 1])), work)
    bad = [r for r in fr if r['shown'] != r['k']]
    R.append('  decoded from the MP4 at (k+0.25)/30 for %d frames (around every transition, and every 211th): %s' % (
        len(fr), 'every one is composed frame k (or identical to it)' if not bad else 'MISMATCH %s' % bad))
    R.append('    (mean |decoded - composed frame k| / 255, then the best of k-2..k+2 other than k:)')
    R.append('    ' + '  '.join('%d: %.1f/%.1f' % (r['k'], r['err'], r['next']) for r in fr))
    # (c) the soundtrack against the recording
    A = check_audio(film, wav, work, dur)
    R.append('  soundtrack decoded from the MP4 vs the recording: offset %+.2f ms (median of %d windows; '
             '%.0f%% exactly 0; max |offset| %.1f ms); decoded length %.3f s' % (
                 A['median_ms'], A['windows'], 100 * A['frac_exact'], A['max_abs_ms'], A['decoded_s']))
    # (d) landmark-keyed animation inside shots, found in the pixels
    ev = [('A1', 'red pen starts (Z- reveal)', red_ink, 30),
          ('ST1b', 'red base arc starts (second stretto voice)', red_ink, 30),
          ('ST2b', 'red Z- starts (second stretto voice)', red_ink, 30)]
    for lm, what, fn, th in ev:
        if lm not in tmap:
            continue
        t = tmap[lm]
        k = RF.frame_at(t)
        f = first_change(final, k - 3, k + 12, fn, th)
        R.append('  %-6s %8.3f s (frame %d) %s: first visible change at frame %s (%s)' % (
            lm, t, k, what, f, '%+d frames' % (f - k) if f is not None else 'none found'))
    return R


if __name__ == '__main__':
    main()
