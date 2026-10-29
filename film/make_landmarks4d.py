#!/usr/bin/env python3
"""make_landmarks4d.py -- the musical landmarks of the ending on S^4 (film5): the last six bars, from a
timing map made by align.py (bars 46-101).

    python3 make_landmarks4d.py TIMING.json OUT.txt [--final SECONDS]

--final: the time of the last chord, in seconds.  Without it, the bar line of bar 101 from the
timing map; but align.py extrapolates that bar line through the ritardando, so it can come early.
Pass the attack of the chord, measured on the recording.

Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
"""
import json, sys

if len(sys.argv) < 3:
    sys.exit('usage: make_landmarks4d.py TIMING.json OUT.txt [--final SECONDS]')
T = json.load(open(sys.argv[1]))
beats = {(b['bar'], b['beat']): b['t'] for b in T['beats']}
final = beats[(101, 1)]
if '--final' in sys.argv:
    final = float(sys.argv[sys.argv.index('--final') + 1])
L = [('START', beats[(96, 1)]),         # bar 96: the last phrase, the cut from the 3D film
     ('B96', beats[(96, 1)]),           # the subject in the right hand, in sixths: the blue pens start
     ('B96b', beats[(96, 2)]),          # the inversion in the left hand, in thirds, a beat later: the red pens
     ('B98', beats[(98, 1)]),           # the right hand's run: the blue pens speed up
     ('B98b', beats[(98, 2)]),          # the left hand's run: the red pens speed up
     ('B99c', beats[(99, 3)]),          # the blue pens arrive
     ('B100', beats[(100, 1)]),         # the red pens arrive; the base arcs come in
     ('B100b', beats[(100, 2)]),        # the translates come in
     ('FINAL', final),                  # the last chord: everything drawn; the hidden direction turns
     ('FADE0', final + 3.6),            # the picture fades to white while the chord dies away
     ('FADE1', final + 5.3),
     ('CRED', final + 5.44),            # the credits, in the silence
     ('END', final + 9.44)]
with open(sys.argv[2], 'w') as o:
    for n, t in L:
        o.write('%s %.3f\n' % (n, t))
        print('%-6s %8.3f s  frame %d' % (n, t, round(30 * t)))
