#!/usr/bin/env python3
"""make_script.py -- turn a storyboard whose segments start at bars of the fugue into a
zipper3 movie script (frame counts), using a timing map from align.py.

    python3 make_script.py STORYBOARD TIMING.json OUT_script.txt [--fps 30] [--start FRAME]

Frame k of the joined film is shown at k/FPS seconds, and a landmark at time t falls on frame
round(FPS t), so consecutive segments tile the film exactly.  Prints the first frame of the
segment list (for zipper3 --moviestart) and the first frame of the next part of the film.

--start FRAME: the first segment begins at this frame of the film, whatever its START says.
The joined film passes the frame where the part before ends, which comes from that part's own
timing map (film/make_joined_film.sh).

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import json, sys


def landmark_time(tok, T):
    """BAR.BEAT (beat may be fractional, e.g. 46.1.5) -> seconds, from the timing map T."""
    parts = tok.split('.')
    bar = int(parts[0])
    beat = float('.'.join(parts[1:])) if len(parts) > 1 else 1.0
    beats = {(b['bar'], b['beat']): b['t'] for b in T['beats']}
    b0 = int(beat)
    t0 = beats[(bar, b0)]
    if beat == b0:
        return t0
    nxt = (bar, b0 + 1) if b0 < 3 else (bar + 1, 1)
    return t0 + (beat - b0) * (beats[nxt] - t0)


def frame_of(tok, T, fps):
    if tok.startswith('frame:'):
        return int(tok[6:]), None
    t = landmark_time(tok, T)
    return int(round(fps * t)), t


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if len(args) < 3:
        sys.exit('usage: make_script.py STORYBOARD TIMING.json OUT_script.txt [--fps 30] [--start FRAME]')
    fps = 30
    if '--fps' in sys.argv:
        fps = int(sys.argv[sys.argv.index('--fps') + 1])
    sb, timing, out = args[:3]
    T = json.load(open(timing))
    segs, end = [], None
    for ln in open(sb):
        s = ln.strip()
        if not s or s.startswith('#'):
            continue
        f = s.split(None, 6)
        if f[0] == 'end':
            end = f[1]
            continue
        segs.append(f)
    if '--start' in sys.argv:
        segs[0][0] = 'frame:%d' % int(sys.argv[sys.argv.index('--start') + 1])
    starts = [frame_of(f[0], T, fps) for f in segs] + [frame_of(end, T, fps)]
    with open(out, 'w') as o:
        o.write('# made by make_script.py from %s and %s\n' % (sb, timing))
        for i, f in enumerate(segs):
            n = starts[i + 1][0] - starts[i][0]
            assert n > 0, 'segment %d has no frames' % (i + 1)
            o.write('%d %s %s %s %s %s %s\n' % (n, f[1], f[2], f[3], f[4], f[5], f[6] if len(f) > 6 else '-'))
            t = starts[i][1]
            print('%2d  %-11s frame %5d  %s  %4d frames (%5.2f s)  %s' % (
                i + 1, f[0], starts[i][0], ('%8.3f s' % t) if t is not None else '          ', n, n / fps,
                (f[6] if len(f) > 6 else '')[:60]))
    print('first frame %d; the next part begins at frame %d (%s, %.3f s)' % (
        starts[0][0], starts[-1][0], end, starts[-1][1] if starts[-1][1] is not None else starts[-1][0] / fps))


if __name__ == '__main__':
    main()
