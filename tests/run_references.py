#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""run_references.py: re-run the reference runs and compare their outputs with tests/reference/.

    run_references.py [--build DIR] [--work DIR] [--subset all|fast] [--no-heavy] [--only ID,...]
                      [--mode auto|exact|portable] [--programs MAP.json | --zippers RUNS.json] [--complete]
    run_references.py --record [--once] [--build DIR] [--work DIR] [--no-heavy] [--only ID,...]

tests/README.md explains the runs, the fields of runs.json, and which comparisons are made on which machine.

The runs are listed in tests/reference/runs.json. For each run it gives the program (a path in the build folder), its
arguments, the files of the repository copied into its working folder first, and what the run produced when the
references were recorded:
  - its exit status;
  - its standard output and standard error, with the timings masked (stored in tests/reference/<id>/);
  - the SHA-256 and the size of every file it wrote;
  - the "portable" results: the lines picked out by the run's portable patterns (certificates, linking numbers,
    selftest verdicts), and the hashes of the output files that are exact combinatorial data.

Each run happens in a fresh folder <work>/<id>/, which holds only the copied inputs. The outputs stay there afterwards.
The runs are made one at a time, each with its time limit.

The new programs: with --programs MAP.json, where MAP.json holds {"map": {legacy program: new program}}
(paths in the build folder), every run whose program the map names is made with the new program instead, with the
same arguments and inputs, and compared with the same references; a run whose program is already a new program (one
of the map's values: a run recorded from the new program itself, such as the S4 pictures of apps/s4/zipper5) is made as it
is; the other runs are left out, and listed. This is how the reorganised programs are held to the outputs of the
legacy ones. tests/reference/programs_apps.json is the map of apps/.

Through zippers: with --zippers RUNS.json, where RUNS.json holds {"runs": {run id: zippers' arguments}, "without":
{run id: why the run has no zippers form}}, every run that the table names is made with the command line of zippers
(apps/cli/zippers in the build folder) that it gives, with the same inputs, and compared with the same references. The
runs listed under "without" are left out, and listed with their reasons; with --complete a run that the table does not
name at all is a failure. tests/reference/zippers_runs.json is the table of zippers.

The programs run with the environment of this script, but LC_ALL=C and without ZIPPERS_DATA, so that the folders that
variable names cannot change what a program reads (the programs look in it after the working folder).

Which comparisons are made (--mode):
  exact      everything: exit status, the masked text, the hash of every output file, and the portable results.
  portable   only what does not depend on floating-point details: the exit status, the names of the output files,
             the portable results. The other comparisons are reported as "not compared".
  auto       exact if this machine is the kind on which the references were recorded (the same system, processor,
             and C++ compiler and version, read from the build folder), portable otherwise. This is the default.

It prints a table, then the details of every difference, and exits with status 0 if every comparison made agrees,
1 if any differs or a run failed (a missing program, a time limit, a crash), and 2 for a usage error.

--record runs each run twice (once with --once), checks that the two runs agree, and writes what they produced into
runs.json and tests/reference/: the masked text, a JPEG copy at most 256 pixels wide of every picture (made with sips
and Ghostscript), and a copy of every text file under 200 kB that is not already in the repository. Record only on
the reference machine, from a clean build of the commit that the references describe; never to make a test pass.
"""

import argparse
import datetime
import difflib
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
ZIPPERS = 'apps/cli/zippers'               # the command line of the suite, in the build folder (--zippers)
REFDIR = os.path.join(HERE, 'reference')
RUNS_JSON = os.path.join(REFDIR, 'runs.json')

# A timing: a number after a space or "(", followed by "s" (seconds) that does not begin a word: "built in 0.25s",
# "(0.11s)", "1.06 s", "0.009 s/frame". Not timings: "12 spikes", "+1 sep", and the "1 s" of "(t^-1 s t)".
TIMING = re.compile(r'(?<![^\s(])\d+(?:\.\d+)?(?=\s?s(?![A-Za-z]))')
TIMING_MASK = '<time>'

STORE_LIMIT = 200 * 1024      # text outputs under this size are kept in tests/reference/<id>/files/
THUMB_WIDTH = 256             # the pictures' small copies are at most this wide


def clean(text):
    """The text output with its timings masked."""
    return TIMING.sub(TIMING_MASK, text)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def sha256_text(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def read_text(path):
    with open(path, 'rb') as f:
        return f.read().decode('utf-8', errors='replace')


# ---- the machine ------------------------------------------------------------------------------------------------------

def build_machine(build):
    """The system, processor and C++ compiler of a CMake build folder (from the files CMake writes there)."""
    m = {'system': None, 'processor': None, 'compiler': None}
    for path in sorted(glob.glob(os.path.join(build, 'CMakeFiles', '*', 'CMakeSystem.cmake'))):
        text = read_text(path)
        s = re.search(r'set\(CMAKE_SYSTEM_NAME "([^"]*)"\)', text)
        p = re.search(r'set\(CMAKE_SYSTEM_PROCESSOR "([^"]*)"\)', text)
        if s:
            m['system'] = s.group(1)
        if p:
            m['processor'] = p.group(1)
    for path in sorted(glob.glob(os.path.join(build, 'CMakeFiles', '*', 'CMakeCXXCompiler.cmake'))):
        text = read_text(path)
        i = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]*)"\)', text)
        v = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]*)"\)', text)
        if i and v:
            m['compiler'] = i.group(1) + ' ' + v.group(1)
    return m


def checkout_state():
    """Whether the checkout the references are recorded from is clean (the records name programs, not commits)."""
    try:
        dirty = subprocess.run(['git', '-C', REPO, 'status', '--porcelain', '--untracked-files=no'],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True)
        if dirty.returncode == 0:
            return 'a checkout with uncommitted changes' if dirty.stdout.strip() else 'a clean checkout'
    except OSError:
        pass
    return 'a checkout of unknown state'


# ---- running one program ----------------------------------------------------------------------------------------------

def run_program(cmd, cwd, out_path, err_path, timeout):
    """Run cmd in cwd, stdout and stderr to files. Returns (exit status, seconds, peak RSS in bytes, timed out).
    The exit status is negative for a signal. The peak memory is the child's own (wait4)."""
    env = dict(os.environ)
    env['LC_ALL'] = 'C'
    env.pop('ZIPPERS_DATA', None)          # the programs' search for data files: the working folder and their own data
    with open(out_path, 'wb') as so, open(err_path, 'wb') as se:
        t0 = time.monotonic()
        p = subprocess.Popen(cmd, cwd=cwd, stdout=so, stderr=se, stdin=subprocess.DEVNULL, env=env)
        timed_out = False
        while True:
            pid, status, ru = os.wait4(p.pid, os.WNOHANG)
            if pid != 0:
                break
            if time.monotonic() - t0 > timeout:
                timed_out = True
                p.kill()
                pid, status, ru = os.wait4(p.pid, 0)
                break
            time.sleep(0.01)
        secs = time.monotonic() - t0
    if os.WIFEXITED(status):
        code = os.WEXITSTATUS(status)
    elif os.WIFSIGNALED(status):
        code = -os.WTERMSIG(status)
    else:
        code = -1
    p.returncode = code                     # already reaped by wait4
    rss = ru.ru_maxrss if sys.platform == 'darwin' else ru.ru_maxrss * 1024
    return code, secs, rss, timed_out


def prepare(run, build, work):
    """A fresh working folder with the run's inputs; returns (folder, command, {input name: sha256})."""
    d = os.path.join(work, run['id'])
    if os.path.lexists(d):
        shutil.rmtree(d)
    os.makedirs(d)
    copied = {}
    for rel in run.get('inputs', []):
        src = os.path.join(REPO, rel)
        dst = os.path.join(d, os.path.basename(rel))
        shutil.copyfile(src, dst)
        copied[os.path.basename(rel)] = sha256_file(dst)
    for sub in run.get('mkdirs', []):
        os.makedirs(os.path.join(d, sub))
    cmd = [os.path.join(build, run['program'])] + list(run['args'])
    return d, cmd, copied


def collect_outputs(d, copied, skip):
    """Every file in d (recursively) except the unchanged inputs and the given names: {relative path: (sha256, size)}."""
    outs = {}
    for root, dirs, files in os.walk(d):
        dirs.sort()
        for f in sorted(files):
            path = os.path.join(root, f)
            rel = os.path.relpath(path, d).replace(os.sep, '/')
            if rel in skip:
                continue
            h = sha256_file(path)
            if rel in copied and copied[rel] == h:
                continue
            outs[rel] = (h, os.path.getsize(path))
    return outs


def extract(checks, texts):
    """The portable results: for each check, the list of its pattern's matches (groups joined by ' | ')."""
    res = []
    for c in checks:
        rx = re.compile(c['regex'], re.M)
        vals = []
        for m in rx.finditer(texts[c['stream']]):
            g = m.groups()
            vals.append(' | '.join('' if x is None else x for x in g) if g else m.group(0))
        res.append(vals)
    return res


class Result:
    pass


def execute(run, build, work, workname=None):
    """Make one run; returns a Result with everything needed for comparing or recording."""
    r = Result()
    r.id = run['id']
    r.error = None
    prog = os.path.join(build, run['program'])
    if not os.path.isfile(prog) or not os.access(prog, os.X_OK):
        r.error = 'the program %s is missing (not built?)' % prog
        return r
    sub = dict(run)
    if workname:
        sub['id'] = workname
    d, cmd, copied = prepare(sub, build, work)
    r.dir = d
    r.cmd = cmd
    out_path = os.path.join(work, sub['id'] + '.stdout')
    err_path = os.path.join(work, sub['id'] + '.stderr')
    r.code, r.secs, r.rss, r.timed_out = run_program(cmd, d, out_path, err_path, run.get('timeout', 600))
    r.raw = {'stdout': read_text(out_path), 'stderr': read_text(err_path)}
    r.text = {k: clean(v) for k, v in r.raw.items()}
    r.outputs = collect_outputs(d, copied, set())
    r.portable = extract(run.get('portable', []), r.text)
    r.prog_sha = sha256_file(prog)
    if r.timed_out:
        r.error = 'stopped at the time limit of %s s' % run.get('timeout', 600)
    elif r.code < 0:
        r.error = 'killed by signal %d' % -r.code
    return r


# ---- comparing --------------------------------------------------------------------------------------------------------

def text_diff(expected, got, label, n=40):
    lines = list(difflib.unified_diff(expected.splitlines(), got.splitlines(),
                                      'reference ' + label, 'this run ' + label, lineterm='', n=1))
    if len(lines) > n:
        lines = lines[:n] + ['... (%d more lines of diff)' % (len(lines) - n)]
    return lines


def compare(run, r, exact):
    """Compare a Result with the run's reference. Returns (cells for the table, list of detail lines, verdict)."""
    exp = run.get('expect')
    details = []
    if exp is None:
        return {'text': '-', 'files': '-', 'portable': '-'}, ['no reference recorded for this run'], 'NO REFERENCE'
    if r.error:
        return {'text': '-', 'files': '-', 'portable': '-'}, [r.error], 'ERROR'
    bad = False
    cells = {}

    # the exit status: always
    if r.code != exp['exit']:
        bad = True
        details.append('exit status %d, reference %d' % (r.code, exp['exit']))

    # the text
    if exact:
        diffs = []
        for s in ('stdout', 'stderr'):
            if sha256_text(r.text[s]) != exp[s]['sha256']:
                diffs.append(s)
                ref = read_text(os.path.join(REFDIR, exp[s]['file']))
                details.append('%s differs (timings masked):' % s)
                details.extend('    ' + x for x in text_diff(ref, r.text[s], s))
        same_as = run.get('stdout_same_as') if exp.get('stdout_identical_to_same_as') else None
        if same_as and r.text['stdout'] != read_text(os.path.join(REPO, same_as)):
            diffs.append('stdout vs ' + os.path.basename(same_as))
            details.append('stdout differs from %s' % same_as)
        cells['text'] = 'same' if not diffs else 'DIFF ' + ','.join(diffs)
        bad = bad or bool(diffs)
    else:
        cells['text'] = 'n/c'

    # the output files: their names always; their hashes in exact mode (and the portable ones in either mode)
    names_now, names_ref = set(r.outputs), set(exp['outputs'])
    missing, extra = sorted(names_ref - names_now), sorted(names_now - names_ref)
    if missing:
        details.append('missing output files: ' + ', '.join(missing))
    if extra:
        details.append('unexpected output files: ' + ', '.join(extra))
    portable_files = set(run.get('portable_files', []))
    ndiff = 0
    ncomp = 0
    for name in sorted(names_now & names_ref):
        if not exact and name not in portable_files:
            continue
        ncomp += 1
        if r.outputs[name][0] != exp['outputs'][name]['sha256']:
            ndiff += 1
            details.append('%s differs: SHA-256 %s (%d bytes), reference %s (%d bytes)' % (
                name, r.outputs[name][0], r.outputs[name][1], exp['outputs'][name]['sha256'],
                exp['outputs'][name]['size']))
            stored = exp['outputs'][name].get('stored') or exp['outputs'][name].get('same_as')
            if stored and r.outputs[name][1] < STORE_LIMIT:
                base = REFDIR if exp['outputs'][name].get('stored') else REPO
                details.extend('    ' + x for x in text_diff(read_text(os.path.join(base, stored)),
                                                             read_text(os.path.join(r.dir, name)), name))
    if exact:          # the outputs that were byte for byte a file of the repository when recorded must still be
        for name, repo_file in sorted(run.get('same_as', {}).items()):
            if not exp['outputs'].get(name, {}).get('identical_to_same_as'):
                continue
            if name in r.outputs and r.outputs[name][0] != sha256_file(os.path.join(REPO, repo_file)):
                ndiff += 1
                details.append('%s is no longer identical to %s' % (name, repo_file))
    nfiles = len(names_ref)
    if missing or extra or ndiff:
        bad = True
        cells['files'] = 'DIFF %d of %d' % (ndiff + len(missing) + len(extra), nfiles)
    elif nfiles == 0:
        cells['files'] = '-'
    elif ncomp == nfiles:
        cells['files'] = 'same %d' % nfiles
    elif ncomp:
        cells['files'] = 'same %d, n/c %d' % (ncomp, nfiles - ncomp)
    else:
        cells['files'] = 'names %d, n/c' % nfiles

    # the portable results: always
    checks = run.get('portable', [])
    if checks:
        pdiff = 0
        for c, got, want in zip(checks, r.portable, exp['portable']):
            if got != want:
                pdiff += 1
                details.append('portable check /%s/ on %s differs (%d values, reference %d):' % (
                    c['regex'], c['stream'], len(got), len(want)))
                if len(got) == len(want):
                    bad_k = [k for k in range(len(want)) if got[k] != want[k]]
                    for k in bad_k[:10]:
                        details.append('    value %d: %r, reference %r' % (k + 1, got[k], want[k]))
                    if len(bad_k) > 10:
                        details.append('    ... and %d more' % (len(bad_k) - 10))
                else:
                    details.extend('    ' + x for x in difflib.unified_diff(want, got, 'reference', 'this run',
                                                                              lineterm='', n=0))
        nvals = sum(len(w) for w in exp['portable'])
        cells['portable'] = 'same %d' % nvals if not pdiff else 'DIFF %d of %d' % (pdiff, len(checks))
        bad = bad or bool(pdiff)
    else:
        cells['portable'] = '-'

    if bad:
        return cells, details, 'DIFFERENT'
    if not exact and not checks and not portable_files:
        return cells, details, 'ran'
    return cells, details, 'same'


# ---- recording --------------------------------------------------------------------------------------------------------

def png_width(path):
    with open(path, 'rb') as f:
        head = f.read(24)
    if head[:8] != b'\x89PNG\r\n\x1a\n':
        return None
    return int.from_bytes(head[16:20], 'big')


def eps_width(path):
    with open(path, 'rb') as f:
        head = f.read(4096).decode('latin-1')
    m = re.search(r'%%BoundingBox:\s*(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)', head)
    return float(m.group(3)) - float(m.group(1)) if m else None


def make_thumbnail(src, dst):
    """A copy of the picture src (PNG or EPS) at most THUMB_WIDTH pixels wide, as a JPEG of quality 60 (about 30 kB;
    the pictures are dense line drawings, which PNG compresses badly). Uses sips (macOS) and, for EPS, Ghostscript.
    Returns a note if the copy could not be made."""
    ext = os.path.splitext(src)[1].lower()
    if not shutil.which('sips'):
        return 'no sips (macOS) to make the small copy'
    tmp = None
    if ext == '.eps':
        gs = shutil.which('gs')
        w = eps_width(src)
        if not gs or not w:
            return 'no Ghostscript, or no bounding box, to make the small copy'
        tmp = dst + '.tmp.png'
        res = THUMB_WIDTH * 72.0 / w
        for _ in range(5):
            subprocess.run([gs, '-q', '-dSAFER', '-dBATCH', '-dNOPAUSE', '-dEPSCrop', '-sDEVICE=png16m',
                            '-dTextAlphaBits=4', '-dGraphicsAlphaBits=4', '-r%.4f' % res, '-sOutputFile=' + tmp, src],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
            if (png_width(tmp) or 0) <= THUMB_WIDTH:
                break
            res *= 0.99
        src = tmp
    elif ext != '.png':
        return 'not a PNG or EPS file'
    w = png_width(src)
    if w is None:
        return 'not a PNG'
    cmd = ['sips', '-s', 'format', 'jpeg', '-s', 'formatOptions', '60']
    if w > THUMB_WIDTH:
        cmd += ['--resampleWidth', str(THUMB_WIDTH)]
    subprocess.run(cmd + [src, '--out', dst], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    if tmp:
        os.remove(tmp)
    return None


def is_text(path):
    with open(path, 'rb') as f:
        data = f.read(STORE_LIMIT)
    if b'\0' in data:
        return False
    try:
        data.decode('utf-8')
    except UnicodeDecodeError:
        return False
    return True


def same_result(a, b):
    """Why two runs of the same command disagree, or None."""
    why = []
    if a.code != b.code:
        why.append('exit status %d and %d' % (a.code, b.code))
    for s in ('stdout', 'stderr'):
        if a.text[s] != b.text[s]:
            why.append(s + ' (timings masked)')
    if {k: v[0] for k, v in a.outputs.items()} != {k: v[0] for k, v in b.outputs.items()}:
        why.append('output files')
    return ', '.join(why) or None


def record(runs_doc, runs, build, work, once, partial=False):
    machine = build_machine(build)
    old = runs_doc.get('machine', {})
    if partial and old and any(machine[k] != old.get(k) for k in ('system', 'processor', 'compiler')):
        print('refusing to record some runs on %s %s, %s: the others were recorded on %s %s, %s; record them all again '
              'instead' % (machine['system'], machine['processor'], machine['compiler'], old.get('system'),
                           old.get('processor'), old.get('compiler')))
        return 2
    problems = 0
    for run in runs:
        print('recording %s: %s' % (run['id'], ' '.join([run['program']] + run['args'])), flush=True)
        a = execute(run, build, work)
        if a.error:
            print('  ERROR: ' + a.error)
            problems += 1
            continue
        if once:
            b = a
        else:
            b = execute(run, build, work, workname=run['id'] + '.again')
            why = same_result(a, b)
            if b.error or why:
                print('  NOT RECORDED: the two runs disagree: %s' % (b.error or why))
                problems += 1
                continue
            shutil.rmtree(b.dir)
        for s in ('stdout', 'stderr'):
            masked = sorted(set(TIMING.findall(a.raw[s])))
            if masked:
                lines = [x for x in a.raw[s].splitlines() if TIMING.search(x)]
                print('  %s: %d timing(s) masked, in %d line(s):' % (s, len(TIMING.findall(a.raw[s])), len(lines)))
                for x in lines[:6]:
                    print('      ' + x)
        rdir = os.path.join(REFDIR, run['id'])
        if os.path.isdir(rdir):
            shutil.rmtree(rdir)
        os.makedirs(rdir)
        exp = {'exit': a.code}
        for s in ('stdout', 'stderr'):
            rel = run['id'] + '/' + s + '.txt'
            with open(os.path.join(REFDIR, rel), 'w', encoding='utf-8', newline='') as f:
                f.write(a.text[s])
            exp[s] = {'file': rel, 'sha256': sha256_text(a.text[s]), 'lines': len(a.text[s].splitlines())}
        outs = {}
        for name, (h, size) in sorted(a.outputs.items()):
            o = {'sha256': h, 'size': size}
            path = os.path.join(a.dir, name)
            repo_file = run.get('same_as', {}).get(name)
            if repo_file:
                o['same_as'] = repo_file
                o['identical_to_same_as'] = (h == sha256_file(os.path.join(REPO, repo_file)))
                print('  %s %s %s' % (name, 'is byte for byte' if o['identical_to_same_as'] else 'DIFFERS from',
                                      repo_file))
            if os.path.splitext(name)[1].lower() in ('.png', '.eps'):
                thumb = run['id'] + '/small_' + os.path.splitext(name.replace('/', '_'))[0] + '.jpg'
                note = make_thumbnail(path, os.path.join(REFDIR, thumb))
                if note:
                    print('  no small copy of %s: %s' % (name, note))
                else:
                    o['small_copy'] = thumb
            elif size < STORE_LIMIT and is_text(path) and not o.get('identical_to_same_as'):
                stored = run['id'] + '/files/' + name
                os.makedirs(os.path.dirname(os.path.join(REFDIR, stored)), exist_ok=True)
                shutil.copyfile(path, os.path.join(REFDIR, stored))
                o['stored'] = stored
            outs[name] = o
        exp['outputs'] = outs
        exp['portable'] = a.portable
        same_as = run.get('stdout_same_as')
        if same_as:
            ok = a.text['stdout'] == read_text(os.path.join(REPO, same_as))
            exp['stdout_identical_to_same_as'] = ok
            print('  stdout %s %s' % ('is byte for byte' if ok else 'DIFFERS from', same_as))
        exp['seconds'] = round(min(a.secs, b.secs), 2)
        exp['peak_rss_mb'] = round(max(a.rss, b.rss) / 2**20, 1)
        exp['program_sha256'] = a.prog_sha
        exp['recorded_from'] = 'one run' if once else 'two runs that agree'
        run['expect'] = exp
        print('  recorded: exit %d, %.2f s, %.0f MB, %d output file(s)' % (a.code, exp['seconds'],
                                                                          exp['peak_rss_mb'], len(outs)), flush=True)
    old = runs_doc.get('machine', {})
    if partial and old:
        # a subset: keep the machine block of the first recording, and add a dated line for these runs
        line = '%s, from the programs named, built from %s: %s' % (datetime.date.today().isoformat(), checkout_state(),
                                                                  ', '.join(r['id'] for r in runs))
        added = list(old.get('also_recorded', []))
        added.append(line)
        old['also_recorded'] = added
        runs_doc['machine'] = old
    else:
        runs_doc['machine'] = {
            'system': machine['system'], 'processor': machine['processor'], 'compiler': machine['compiler'],
            'recorded': datetime.date.today().isoformat(),
            'programs': 'the programs that the runs name, built from %s' % checkout_state()}
    with open(RUNS_JSON, 'w', encoding='utf-8') as f:
        json.dump(runs_doc, f, indent=1, ensure_ascii=False)
        f.write('\n')
    print('wrote %s' % RUNS_JSON)
    return 1 if problems else 0


# ---- main -------------------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description='Re-run the reference runs of tests/reference/runs.json and compare.')
    ap.add_argument('--build', default=os.path.join(REPO, 'build'), help='the CMake build folder (default: build/)')
    ap.add_argument('--work', help='the scratch folder for the runs (default: <build>/reference_work/<subset>)')
    ap.add_argument('--subset', choices=('all', 'fast'), default='all', help='all runs, or the fast ones only')
    ap.add_argument('--mode', choices=('auto', 'exact', 'portable'), default='auto')
    ap.add_argument('--only', help='a comma-separated list of run ids')
    ap.add_argument('--no-heavy', action='store_true', help='leave out the heavy runs (over a few minutes)')
    ap.add_argument('--record', action='store_true', help='record the references (on the reference machine only)')
    ap.add_argument('--once', action='store_true', help='with --record: one run each instead of two that must agree')
    ap.add_argument('--complete', action='store_true',
                    help='with --programs: fail if the map leaves any selected run out (once every program is mapped); '
                         'with --zippers: fail if the table names neither a command line nor a reason for a run')
    ap.add_argument('--programs', metavar='MAP.json',
                    help='run the new programs named in this map instead of the legacy ones: only the runs '
                         'whose program the map names, or is already one of its new programs, are made, and they are '
                         'compared with the same references')
    ap.add_argument('--zippers', metavar='RUNS.json',
                    help='make the runs through zippers (apps/cli/zippers), with the command lines that this table '
                         'gives, and compare them with the same references; the runs it lists without one are left out')
    args = ap.parse_args()

    build = os.path.abspath(args.build)
    if not os.path.isdir(build):
        print('no build folder %s: build first (make), or give --build' % build)
        return 2
    with open(RUNS_JSON, encoding='utf-8') as f:
        doc = json.load(f)
    runs = doc['runs']
    if args.only:
        want = args.only.split(',')
        unknown = [x for x in want if x not in {r['id'] for r in runs}]
        if unknown:
            print('unknown run id(s): ' + ', '.join(unknown))
            return 2
        runs = [r for r in runs if r['id'] in want]
    elif args.subset == 'fast':
        runs = [r for r in runs if r.get('fast')]
    if args.no_heavy:
        runs = [r for r in runs if not r.get('heavy')]
    mapped_note = None
    if args.programs:
        if args.record:
            print('--record and --programs do not go together: the references are recorded from the programs that '
                  'runs.json names')
            return 2
        with open(args.programs, encoding='utf-8') as f:
            pmap = json.load(f).get('map', {})
        kept, left, own = [], [], []
        new_programs = set(pmap.values())
        for r in runs:
            if r['program'] in pmap:
                r = dict(r)
                r['legacy_program'] = r['program']
                r['program'] = pmap[r['program']]
                kept.append(r)
            elif r['program'] in new_programs:      # recorded from the new program itself: made as it is
                kept.append(r)
                own.append(r['id'])
            else:
                left.append(r['id'])
        if not kept:
            print('no run uses a program that %s maps (the map names %d program(s))' % (args.programs, len(pmap)))
            return 2
        if args.complete and left:
            print('--complete: %d run(s) are left out, since %s does not name their programs: %s'
                  % (len(left), args.programs, ', '.join(left)))
            return 1
        runs = kept
        mapped_note = ('programs: the new ones of %s, for %d run(s)%s; %d run(s) left out, since the map does not name '
                       'their programs yet%s' % (args.programs, len(kept),
                                              (' (%d of them recorded from the new program itself: %s)' %
                                               (len(own), ', '.join(own))) if own else '',
                                              len(left), (': ' + ', '.join(left)) if left else ''))
    if args.zippers:
        if args.record or args.programs:
            print('--zippers goes neither with --record nor with --programs')
            return 2
        with open(args.zippers, encoding='utf-8') as f:
            zdoc = json.load(f)
        zruns, without = zdoc.get('runs', {}), zdoc.get('without', {})
        kept, left, unnamed = [], [], []
        for r in runs:
            if r['id'] in zruns:
                r = dict(r)
                r['recorded_program'] = r['program']
                r['program'] = ZIPPERS
                r['args'] = list(zruns[r['id']])
                kept.append(r)
            elif r['id'] in without:
                left.append(r['id'])
            else:
                unnamed.append(r['id'])
        if args.complete and unnamed:
            print('--complete: %d run(s) have neither a zippers command line nor a reason in %s: %s'
                  % (len(unnamed), args.zippers, ', '.join(unnamed)))
            return 1
        if not kept:
            print('no selected run has a zippers command line in %s' % args.zippers)
            return 2
        runs = kept
        mapped_note = ('programs: zippers (%s), with the command lines of %s, for %d run(s); left out, with no zippers '
                       'form: %s%s' % (ZIPPERS, args.zippers, len(kept),
                                      '; '.join('%s (%s)' % (x, without[x]) for x in left) if left else 'none',
                                      ('; not named in the table: ' + ', '.join(unnamed)) if unnamed else ''))
    work = os.path.abspath(args.work or os.path.join(build, 'reference_work', 'record' if args.record else args.subset))
    os.makedirs(work, exist_ok=True)

    if args.record:
        return record(doc, runs, build, work, args.once, partial=len(runs) < len(doc['runs']))

    here, ref = build_machine(build), doc.get('machine', {})
    key = ('system', 'processor', 'compiler')
    same_machine = all(here[k] and here[k] == ref.get(k) for k in key)
    exact = args.mode == 'exact' or (args.mode == 'auto' and same_machine)
    print('reference machine: %s %s, %s (recorded %s)' % (ref.get('system'), ref.get('processor'),
                                                         ref.get('compiler'), ref.get('recorded')))
    print('this build:        %s %s, %s  (%s)' % (here['system'], here['processor'], here['compiler'], build))
    if exact:
        print('mode: exact (every output file by SHA-256, and the text with the timings masked)' +
              ('' if same_machine else '; NOTE: this is not the reference machine, so differences may be expected'))
    else:
        print('mode: portable (exit status, output file names, certificates, selftests and linking numbers only; '
              'the hashes and the full text are not compared%s)' % ('' if args.mode == 'portable' else
                                                                    ', since this is not the reference machine'))
    heavy = [r['id'] for r in runs if r.get('heavy')]
    if heavy:
        print('heavy runs (over a few minutes): ' + ', '.join(heavy))
    if mapped_note:
        print(mapped_note)
    print('working folder: %s' % work)
    print()

    fmt = '%-30s %-4s %8s %8s %4s  %-14s %-16s %-10s %s'
    print(fmt % ('run', 'set', 'seconds', 'peak MB', 'exit', 'text', 'files', 'portable', 'result'))
    all_details = []
    counts = {}
    same_prog = 0
    t_all = time.monotonic()
    for run in runs:
        r = execute(run, build, work)
        cells, details, verdict = compare(run, r, exact)
        counts[verdict] = counts.get(verdict, 0) + 1
        if getattr(r, 'prog_sha', None) and r.prog_sha == run.get('expect', {}).get('program_sha256'):
            same_prog += 1
        secs = '%.2f' % r.secs if hasattr(r, 'secs') else '-'
        mem = '%.0f' % (r.rss / 2**20) if hasattr(r, 'rss') else '-'
        code = str(r.code) if hasattr(r, 'code') else '-'
        print(fmt % (run['id'], 'fast' if run.get('fast') else '', secs, mem, code, cells['text'], cells['files'],
                     cells['portable'], verdict), flush=True)
        if details:
            all_details.append((run, details))
    print()
    print('%d run(s) in %.0f s: %s' % (len(runs), time.monotonic() - t_all,
                                       ', '.join('%d %s' % (n, v) for v, n in sorted(counts.items()))))
    print('(n/c: not compared in this mode; "ran": a run with nothing that is compared in this mode but its exit '
          'status and file names)')
    print('the program was byte for byte the one of the recording in %d of the %d runs' % (same_prog, len(runs)))
    for run, details in all_details:
        print()
        print('%s: %s' % (run['id'], ' '.join([run['program']] + run['args'])))
        for x in details:
            print('  ' + x)
    failed = sum(n for v, n in counts.items() if v not in ('same', 'ran'))
    print()
    print('RESULT: %s' % ('all the comparisons agree' if not failed else '%d run(s) differ or failed' % failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
