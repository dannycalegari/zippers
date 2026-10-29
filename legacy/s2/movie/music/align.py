#!/usr/bin/env python3
"""Align the score of Bach, Fugue in B-flat minor BWV 891 (WTC II/22) to a recording,
and write a timing map  landmark -> seconds  for the film.

    python3 align.py AUDIO.wav OUT_timing.json [--first-bar 1] [--last-bar 46]
                     [--landmarks landmarks.json] [--offset 0.0]

AUDIO.wav  : 16-bit PCM WAV (any rate, mono or stereo).  The audio must START at (or a
             little before) the first note of --first-bar; it may stop anywhere at or
             after the downbeat of --last-bar (open end).
--offset   : seconds to add to every time (e.g. if AUDIO.wav is an excerpt starting at
             t0 of the film's soundtrack, pass t0).

Method (numpy only):
  1. STFT (93 ms Hann window, 5.8 ms hop) -> semitone filterbank MIDI 28..100.
  2. Onset envelope = half-wave rectified spectral flux of log semitone energies.
  3. Tempo estimate = autocorrelation of the onset envelope.
  4. Independent beat tracking (Ellis-style dynamic programming at the quarter period).
  5. Score-informed alignment: DTW between audio chroma (+ chroma onset) and a chroma
     rendering of the **kern score (wtc2f22.krn) with piano-like decay.
  6. Refinement: every distinct score onset is matched to a peak of the onset envelope
     within +-50 ms of the DTW prediction; bar lines are then obtained by a Gaussian-
     weighted local linear fit (sigma = 3 quarter notes) to the matched onsets, averaged
     with the onset found exactly on the bar line when the two agree.
  7. Constant bias correction: flux peaks precede the sample-level hammer attack by
     a constant time, the median of the measured offsets, estimated again for each recording.
Confidence per bar line: 'high' / 'medium' / 'low' from the local fit residual, the
presence of an attack on the downbeat and agreement with the score-free beat tracker.
"""
import sys, json, wave, argparse, os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from kern import parse

QPB = 6           # quarter notes per bar (3/2)


def load_wav(path):
    w = wave.open(path)
    sr, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth()
    assert sw == 2, '16-bit PCM expected'
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32)
    x = x.reshape(-1, ch).mean(1) / 32768.0
    return x, sr


def features(x, sr):
    N = int(round(4096 * sr / 44100)); H = int(round(256 * sr / 44100))
    nf = 1 + (len(x) - N) // H
    win = np.hanning(N).astype(np.float32)
    freqs = np.fft.rfftfreq(N, 1 / sr)
    mid = np.arange(28, 101); fc = 440 * 2 ** ((mid - 69) / 12)
    fb = np.zeros((len(mid), len(freqs)), np.float32)
    for i, f in enumerate(fc):
        sel = (freqs >= f * 2 ** (-1 / 24)) & (freqs < f * 2 ** (1 / 24))
        if sel.sum() == 0: sel[np.argmin(abs(freqs - f))] = True
        fb[i] = sel
    P = np.empty((nf, len(mid)), np.float32); rms = np.empty(nf, np.float32)
    for a in range(0, nf, 2000):
        b = min(nf, a + 2000)
        idx = np.arange(N)[None, :] + H * np.arange(a, b)[:, None]
        fr = x[idx]
        P[a:b] = np.abs(np.fft.rfft(fr * win, axis=1)) @ fb.T
        rms[a:b] = np.sqrt((fr ** 2).mean(1))
    t = (np.arange(nf) * H + N / 2) / sr
    L = np.log1p(100 * P)
    flux = np.maximum(np.diff(L, axis=0, prepend=L[:1]), 0).sum(1)
    k = max(1, int(0.5 / (H / sr)))
    o = np.maximum(flux - np.convolve(flux, np.ones(k) / k, 'same'), 0)
    o /= o.max()
    return t, P, mid, o, rms


def tempo(o, dt):
    oc = o - o.mean()
    n = len(oc); F = np.fft.rfft(oc, 2 * n)
    ac = np.fft.irfft(F * np.conj(F))[:n]; ac /= ac[0]
    lags = np.arange(n) * dt
    s = (lags > 0.2) & (lags < 0.6)          # quarter-note range
    return lags[np.argmax(ac * s)]


def beat_track(o, dt, period, alpha=100.0):
    """Ellis (2007) DP beat tracker. Returns beat times (s)."""
    P = period / dt
    n = len(o); score = o.copy(); back = -np.ones(n, int)
    lo, hi = int(round(P / 2)), int(round(2 * P))
    offs = np.arange(lo, hi + 1)
    pen = -alpha * np.log(offs / P) ** 2
    for i in range(hi, n):
        cand = score[i - offs] + pen
        j = np.argmax(cand); score[i] = o[i] + cand[j]; back[i] = i - offs[j]
    i = int(np.argmax(score[-int(2 * P):])) + n - int(2 * P)
    b = [i]
    while back[b[-1]] > 0: b.append(back[b[-1]])
    return np.array(b[::-1]) * dt


def score_chroma(notes, q0, q1, R):
    ns = int((q1 - q0) * R)
    Cs = np.zeros((ns, 12)); Os = np.zeros((ns, 12))
    for n in notes:
        a = int(round((float(n['onset']) - q0) * R))
        if a < 0 or a >= ns: continue
        b = min(ns, int(round((float(n['onset'] + n['dur']) - q0) * R)))
        env = np.exp(-np.arange(b - a) / (R * 2.0))
        for h, wt in ((0, 1.0), (12, 0.4), (19, 0.25), (24, 0.15)):
            Cs[a:b, (n['midi'] + h) % 12] += wt * env
        Os[a, n['midi'] % 12] += 1
    Os = np.apply_along_axis(lambda c: np.convolve(c, [.25, .5, 1, .5, .25], 'same'), 0, Os)
    return Cs, Os


def nrm(X): return X / (np.linalg.norm(X, axis=1, keepdims=True) + 1e-9)


def dtw_open_end(C):
    """steps (1,1),(2,1),(1,2); start at (0,0); end at last audio frame, any score frame."""
    na, ns = C.shape; INF = 1e18
    D = np.full((na, ns), INF); B = np.zeros((na, ns), np.int8)
    D[0, 0] = C[0, 0]
    for i in range(1, na):
        c = C[i]
        m1 = np.full(ns, INF); m1[1:] = D[i - 1, :-1]
        m2 = np.full(ns, INF)
        if i >= 2: m2[1:] = D[i - 2, :-1]
        m3 = np.full(ns, INF); m3[2:] = D[i - 1, :-2]
        M = np.vstack([m1 + c, m2 + 2 * c, m3 + 2 * c])
        k = np.argmin(M, 0); D[i] = M[k, np.arange(ns)]; B[i] = k + 1
        if i < 3: D[i, 0] = C[i, 0]; B[i, 0] = 0      # tiny slack at the start
    i = na - 1; j = int(np.argmin(D[i] / (np.arange(ns) + i + 1)))
    path = [(i, j)]
    while i > 0 and j > 0:
        k = B[i, j]
        if k == 1: i, j = i - 1, j - 1
        elif k == 2: i, j = i - 2, j - 1
        elif k == 3: i, j = i - 1, j - 2
        else: break
        path.append((i, j))
    return np.array(path[::-1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('audio'); ap.add_argument('out')
    ap.add_argument('--first-bar', type=int, default=1)
    ap.add_argument('--last-bar', type=int, default=46)
    ap.add_argument('--landmarks', default=os.path.join(HERE, 'landmarks.json'))
    ap.add_argument('--offset', type=float, default=0.0)
    ap.add_argument('--recording', default='')
    a = ap.parse_args()

    notes, bars, total = parse(os.path.join(HERE, 'wtc2f22.krn'))
    x, sr = load_wav(a.audio)
    t, P, mid, o, rms = features(x, sr); dt = t[1] - t[0]

    # trim leading silence: first frame whose rms exceeds 2% of max
    i0 = int(np.argmax(rms > 0.02 * rms.max()))
    qper = tempo(o, dt)
    beats = beat_track(o, dt, qper)

    # ---- DTW on 4-frame (23 ms) blocks
    G = 4
    nb = (len(P) - i0) // G
    A = P[i0:i0 + nb * G].reshape(nb, G, -1).mean(1); ta = t[i0:i0 + nb * G].reshape(nb, G).mean(1)
    A = np.log1p(1000 * A / A.max())
    Ad = np.maximum(np.diff(A, axis=0, prepend=A[:1]), 0)
    Ca = np.zeros((nb, 12)); Oa = np.zeros((nb, 12))
    for k, m in enumerate(mid): Ca[:, m % 12] += A[:, k]; Oa[:, m % 12] += Ad[:, k]
    q0 = QPB * (a.first_bar - 1); q1 = QPB * min(101, a.last_bar + 4)
    R = 12
    Cs, Os = score_chroma(notes, q0, q1, R)
    C = (1 - nrm(Ca) @ nrm(Cs).T) + 0.5 * (1 - nrm(Oa) @ nrm(Os).T)
    path = dtw_open_end(C)
    qa = q0 + path[:, 1] / R; tq = ta[path[:, 0]]
    uq = np.unique(qa); ut = np.array([tq[qa == q].mean() for q in uq])

    # ---- refinement on onsets
    QEND = QPB * (a.last_bar - 1)
    qs = np.array(sorted({float(n['onset']) for n in notes if q0 <= float(n['onset']) <= QEND + 1e-6}))
    pk = np.array([i for i in range(1, len(o) - 1) if o[i] > o[i - 1] and o[i] >= o[i + 1] and o[i] > 0.04])
    pt, ps = t[pk], o[pk]
    pred = np.interp(qs, uq, ut)
    pred[0] = pt[np.argmax(pt >= t[i0] - 0.05)]          # first note = first attack
    for it in range(3):
        win = 0.07 if it == 0 else 0.05
        mq, mt, ms = [], [], []
        for q, tp in zip(qs, pred):
            s = np.where(abs(pt - tp) < win)[0]
            if len(s) == 0: continue
            k = s[np.argmax(ps[s] * np.exp(-((pt[s] - tp) / 0.03) ** 2))]
            mq.append(q); mt.append(pt[k]); ms.append(ps[k])
        mq, mt, ms = map(np.array, (mq, mt, ms))

        def fit(qc, sig=3.0):
            w = np.exp(-((mq - qc) / sig) ** 2) * np.sqrt(ms); sel = w > 1e-3
            X = np.vstack([np.ones(sel.sum()), mq[sel] - qc]).T; W = w[sel]
            c = np.linalg.solve(X.T @ (X * W[:, None]), X.T @ (W * mt[sel]))
            r = mt[sel] - X @ c
            return c[0], c[1], float(np.sqrt((W * r * r).sum() / W.sum())), int(sel.sum())
        pred = np.array([fit(q)[0] for q in qs])
        pred[0] = mt[0] if mq[0] == qs[0] else pred[0]

    # ---- bias: flux peak vs sample-level attack (steepest 5 ms rise of high-passed energy)
    y = np.diff(x, prepend=0); e = np.convolve(y ** 2, np.ones(int(0.002 * sr)) / int(0.002 * sr), 'same')
    le = 10 * np.log10(e + 1e-12); kk = int(0.005 * sr); rise = le[kk:] - le[:-kk]
    dd = []
    for tm in mt:
        lo_, hi_ = int((tm - 0.01) * sr), int((tm + 0.08) * sr)
        if lo_ < 0 or hi_ >= len(rise): continue
        dd.append((lo_ + np.argmax(rise[lo_:hi_]) + kk / 2) / sr - tm)
    bias = float(np.median(dd))

    # ---- bar lines
    # signed offset of the score-free DP beat tracker relative to the score-informed fits
    # (the DP tracker has its own constant lag; only deviations from its median count)
    def signed_beat(tt): j = np.argmin(abs(beats - tt)); return float(beats[j] - tt)
    beat_lag = float(np.median([signed_beat(fit(QPB * (b - 1))[0]) for b in range(a.first_bar, a.last_bar + 1)]))
    def nearest_beat(tt): return abs(signed_beat(tt) - beat_lag)
    barres = []
    for b in range(a.first_bar, a.last_bar + 1):
        qb = QPB * (b - 1)
        tf, slope, rr, nn = fit(qb)
        j = np.where(abs(mq - qb) < 1e-6)[0]
        tdir = float(mt[j[0]]) if len(j) else None
        if b == a.first_bar and tdir is not None: tb = tdir
        elif tdir is not None and abs(tdir - tf) < 0.04: tb = 0.5 * (tf + tdir)
        else: tb = tf
        tdtw = float(np.interp(qb, uq, ut))
        unc = max(0.005, rr / np.sqrt(max(nn, 1)) * 2) + (0.0 if tdir is not None else 0.02)
        db = nearest_beat(tb)
        if db > 0.05: unc += 0.02
        conf = 'high' if unc < 0.02 else ('medium' if unc < 0.04 else 'low')
        barres.append(dict(bar=b, t=tb + bias + a.offset, uncertainty_s=round(unc, 3), confidence=conf,
                           sec_per_quarter_local=round(slope, 4), fit_rms_ms=round(1000 * rr, 1),
                           attack_on_downbeat=tdir is not None, dtw_minus_final_ms=round(1000 * (tdtw - tb), 0),
                           beat_tracker_dist_ms=round(1000 * db, 0)))
    # half-note beat times (3 per bar) from the local fits
    beatres = []
    for b in range(a.first_bar, a.last_bar + 1):
        for bt in (1, 2, 3):
            qb = QPB * (b - 1) + 2 * (bt - 1)
            if qb > QEND: break
            tb = barres[b - a.first_bar]['t'] if bt == 1 else fit(qb)[0] + bias + a.offset
            beatres.append(dict(bar=b, beat=bt, t=round(tb, 3)))
    bt_times = {(r['bar'], r['beat']): r['t'] for r in beatres}

    def time_of(bar, beat):
        """bar (int), beat in half notes (1..3, may be fractional)"""
        qb = QPB * (bar - 1) + 2 * (beat - 1)
        if abs(beat - round(beat)) < 1e-9 and (bar, int(round(beat))) in bt_times:
            return bt_times[(bar, int(round(beat)))]
        return fit(qb)[0] + bias + a.offset

    out = dict(
        recording=a.recording or os.path.basename(a.audio),
        audio_file=os.path.abspath(a.audio), offset_s=a.offset,
        note='times in seconds from the start of the audio file (+offset); bar lines = downbeats; beats are half notes (3/2 metre)',
        method='align.py: chroma DTW against wtc2f22.krn, onset refinement, local linear fits; bias %+.3f s applied' % bias,
        tempo=dict(quarter_period_autocorr_s=round(float(qper), 4)),
        attack_bias_s=round(bias, 4),
        bars={str(r['bar']): round(r['t'], 3) for r in barres},
        bar_details=barres, beats=beatres, landmarks={})
    bl = [r['t'] for r in barres]
    bd = np.diff(bl)
    out['tempo'].update(mean_bar_s=round(float(bd[1:].mean()), 3), median_bar_s=round(float(np.median(bd)), 3),
                        min_bar_s=round(float(bd.min()), 3), max_bar_s=round(float(bd.max()), 3),
                        half_notes_per_min=round(3 * 60 / float(np.median(bd)), 1),
                        quarters_per_min=round(6 * 60 / float(np.median(bd)), 1))
    if os.path.exists(a.landmarks):
        L = json.load(open(a.landmarks))
        for lm in L['landmarks']:
            if a.first_bar <= lm['bar'] <= a.last_bar:
                bi = lm['bar'] - a.first_bar
                out['landmarks'][lm['name']] = dict(t=round(time_of(lm['bar'], lm['beat']), 3),
                                                     bar=lm['bar'], beat=lm['beat'],
                                                     confidence=barres[bi]['confidence'])
    json.dump(out, open(a.out, 'w'), indent=1)
    print('tempo: quarter %.4f s (autocorr); median bar %.3f s; %.1f half notes/min; bias %+.3f s'
          % (qper, np.median(bd), 3 * 60 / np.median(bd), bias))
    print('matched %d of %d score onsets; beat tracker beats %d, median lag %+.3f s' % (len(mq), len(qs), len(beats), beat_lag))
    for r in barres:
        print('bar %3d  %8.3f  %-6s unc %.3f  rms %4.1fms  dtw-final %+4.0fms  beatDP %3.0fms'
              % (r['bar'], r['t'], r['confidence'], r['uncertainty_s'], r['fit_rms_ms'],
                 r['dtw_minus_final_ms'], r['beat_tracker_dist_ms']))


if __name__ == '__main__':
    main()
