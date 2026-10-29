"""Minimal **kern parser for the 4-spine WTC II fugue 22 (BWV 891) file.
Returns notes as dicts: voice (0=bass..3=soprano), bar, onset (quarter notes from start),
dur (quarters), midi, step (diatonic number), name."""
import re
from fractions import Fraction as F

LET = 'cdefgab'
PC = {'c':0,'d':2,'e':4,'f':5,'g':7,'a':9,'b':11}
VOICES = ['bass','tenor','alto','soprano']

def parse_tok(tok):
    """one kern token -> (dur Fraction quarters, rest?, midi, step, tie_start, tie_end)"""
    m = re.match(r'^([\[\(]*)(\d+)(\.*)([a-gA-Gr]+)([#\-n]*)(.*)$', tok)
    if not m: return None
    pre, d, dots, p, acc, post = m.groups()
    base = F(4, int(d)); dur = base; add = base
    for _ in dots: add /= 2; dur += add
    if p[0] == 'r': return dict(dur=dur, rest=True)
    L = p[0].lower(); n = len(p)
    octv = 3 + n if p[0].islower() else 4 - n   # c = C4, cc = C5, C = C3, CC = C2
    a = acc.count('#') - acc.count('-')
    midi = 12*(octv+1) + PC[L] + a
    step = 7*octv + LET.index(L)
    return dict(dur=dur, rest=False, midi=midi, step=step, name=p+acc,
                tie_start='[' in pre, tie_end=']' in post, tie_mid='_' in post)

def parse(path):
    lines = open(path).read().splitlines()
    t = [F(0)]*4; bar = 0; notes = []; bars = {}
    open_ties = [None]*4; pending = [F(0)]*4  # time at which each spine is next free
    cur = F(0)
    for ln in lines:
        if not ln or ln.startswith('!') or ln.startswith('*'): continue
        f = ln.split('\t')
        if f[0].startswith('='):
            mm = re.match(r'=(\d+)', f[0])
            if mm:
                bar = int(mm.group(1)); bars[bar] = max(pending)
            continue
        # data line: onset = current time = min over spines of pending (kern lines are time slices)
        now = None
        for v, tok in enumerate(f):
            if tok == '.': continue
            now = pending[v]; break
        for v, tok in enumerate(f):
            if tok == '.': continue
            for sub in tok.split(' '):
                r = parse_tok(sub)
                if r is None: continue
                d = r['dur']
                if not r['rest']:
                    if (r['tie_end'] or r.get('tie_mid')) and open_ties[v] is not None:
                        open_ties[v]['dur'] += d
                        if r['tie_end']: open_ties[v] = None
                    else:
                        n = dict(voice=v, bar=bar, onset=pending[v], dur=d, midi=r['midi'],
                                 step=r['step'], name=r['name'])
                        notes.append(n)
                        if r['tie_start']: open_ties[v] = n
            pending[v] += parse_tok(tok.split(' ')[0])['dur']
    total = max(pending)
    return notes, bars, total

if __name__ == '__main__':
    import sys
    notes, bars, total = parse(sys.argv[1])
    print(len(notes), 'notes', len(bars), 'bars, total quarters', total)
    bad = [b for b in bars if bars[b] != (b-1)*6]
    print('bars not at 6*(b-1) quarters:', bad[:10])
