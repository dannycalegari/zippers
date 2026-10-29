#!/usr/bin/env python3
"""Audition clips around landmarks (keep them out of the repository: the recording is copyrighted).
    python3 make_clips.py AUDIO.wav timing_X.json OUTDIR
For each landmark L writes OUTDIR/L.wav (about 3 s before to 3 s after) and OUTDIR/L_tick.wav
(same, with a short click exactly at the landmark time), plus OUTDIR/all_barlines_tick.wav
(the whole recording with a click on every bar line, louder on landmark bars)."""
import sys, os, json, wave
import numpy as np

audio, timing, outdir = sys.argv[1:4]
os.makedirs(outdir, exist_ok=True)
w = wave.open(audio); sr, ch = w.getframerate(), w.getnchannels()
x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, ch).astype(np.float32)
T = json.load(open(timing)); off = T.get('offset_s', 0.0)

def click(n_amp=0.35):
    n = int(0.025 * sr); tt = np.arange(n) / sr
    return (n_amp * 32767 * np.sin(2 * np.pi * 2000 * tt) * np.exp(-tt / 0.004)).astype(np.float32)

def add_click(y, i, amp=0.35):
    c = click(amp); j = min(len(y), i + len(c))
    if 0 <= i < len(y): y[i:j] += c[:j - i, None]

def write(path, y):
    y = np.clip(y, -32768, 32767).astype(np.int16)
    o = wave.open(path, 'wb'); o.setnchannels(ch); o.setsampwidth(2); o.setframerate(sr)
    o.writeframes(y.tobytes()); o.close()

for name, L in T['landmarks'].items():
    t = L['t'] - off
    a = max(0.0, t - 3.0); b = min(len(x) / sr, a + 6.0); a = max(0.0, b - 6.0)
    y = x[int(a * sr):int(b * sr)].copy()
    f = int(0.05 * sr); ramp = np.linspace(0, 1, f)[:, None]
    y[:f] *= ramp; y[-f:] *= ramp[::-1]
    write(os.path.join(outdir, name + '.wav'), y)
    yt = y.copy(); add_click(yt, int((t - a) * sr)); write(os.path.join(outdir, name + '_tick.wav'), yt)
    print('%-6s bar %2d beat %s  t=%7.3f  clip %6.2f-%6.2f s  (landmark at %.2f s into clip)' % (name, L['bar'], L['beat'], L['t'], a, b, t - a))

y = x.copy()
lmbars = {L['bar'] for L in T['landmarks'].values()}
for bar, tb in T['bars'].items():
    add_click(y, int((tb - off) * sr), 0.45 if int(bar) in lmbars else 0.18)
write(os.path.join(outdir, 'all_barlines_tick.wav'), y)
print('wrote', outdir)
