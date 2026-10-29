#!/usr/bin/env python3
"""cut_audio.py -- cut a clip from a 16-bit PCM WAV, with optional (cosine) fades.

  cut_audio.py IN.wav OUT.wav START DURATION [FADE_IN [FADE_OUT]]     (seconds)

For short test clips of the soundtrack (e.g. the engine demo).  Reads the data chunk directly
(afconvert's stereo WAVs use WAVE_FORMAT_EXTENSIBLE, which Python's wave module rejects) and
writes a plain PCM WAV.  Keep the output in the scratch directory: the recording is under
copyright.

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import struct, sys
import numpy as np


def read_wav(path):
    d = open(path, 'rb').read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE':
        sys.exit(path + ': not a WAV file')
    p, fmt, data = 12, None, None
    while p + 8 <= len(d):
        cid, n = d[p:p + 4], struct.unpack('<I', d[p + 4:p + 8])[0]
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', d[p + 8:p + 24])
        elif cid == b'data':
            data = d[p + 8:p + 8 + n]
        p += 8 + n + (n & 1)
    tag, ch, rate, _, _, bits = fmt
    if bits != 16:
        sys.exit(path + ': only 16-bit PCM is supported')
    x = np.frombuffer(data, dtype='<i2').reshape(-1, ch)
    return x, rate


def write_wav(path, x, rate):
    ch = x.shape[1]
    b = x.astype('<i2').tobytes()
    hdr = b'RIFF' + struct.pack('<I', 36 + len(b)) + b'WAVE'
    hdr += b'fmt ' + struct.pack('<IHHIIHH', 16, 1, ch, rate, rate * ch * 2, ch * 2, 16)
    hdr += b'data' + struct.pack('<I', len(b))
    open(path, 'wb').write(hdr + b)


def main(a):
    if len(a) < 5:
        sys.exit(__doc__)
    x, rate = read_wav(a[1])
    t0, dur = float(a[3]), float(a[4])
    fi = float(a[5]) if len(a) > 5 else 0.0
    fo = float(a[6]) if len(a) > 6 else 0.0
    i0 = int(round(t0 * rate))
    y = x[i0:i0 + int(round(dur * rate))].astype(np.float64)
    n = len(y)
    g = np.ones(n)
    if fi > 0:
        k = min(n, int(round(fi * rate)))
        g[:k] = np.minimum(g[:k], 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / k))
    if fo > 0:
        k = min(n, int(round(fo * rate)))
        g[n - k:] = np.minimum(g[n - k:], 0.5 + 0.5 * np.cos(np.pi * (np.arange(k) + 1) / k))
    write_wav(a[2], np.clip(np.round(y * g[:, None]), -32768, 32767), rate)
    print('wrote %s: %.3f s from %.3f s, fades %.2f / %.2f s' % (a[2], n / rate, t0, fi, fo))


if __name__ == '__main__':
    main(sys.argv)
