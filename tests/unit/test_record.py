#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_record: the recorder of the references (tests/run_references.py --record) writes no commit hash.

The recorder is run on a temporary copy, never on the repository's references: a folder outside the repository with
a copy of tests/run_references.py and a runs.json that keeps two light runs (s3_chz_selftest and s4_chz5_selftest),
made into a git repository with one commit. In that copy:
  1. a whole recording (--record --once) writes the machine block, whose "programs" must say the state of the
     checkout ("a clean checkout", "a checkout with uncommitted changes" or "a checkout of unknown state");
  2. after a second commit, a recording of one run (--record --once --only s4_chz5_selftest) adds a line to
     "also_recorded", which must say the same;
and neither runs.json may hold the hash of a commit of the copy, whole or in its short form (7 characters). The
repository's tests/reference/ must be byte for byte as before. It needs git and the build's legacy/s3/chz and
legacy/s4/chz5 (the programs of the two runs). Python 3 standard library only.

usage: test_record.py REPO BUILD
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

RUNS = ['s3_chz_selftest', 's4_chz5_selftest']
STATES = ('a clean checkout', 'a checkout with uncommitted changes', 'a checkout of unknown state')


def digest(folder):
    """The SHA-256 of every file under folder, by relative path."""
    out = {}
    for root, _, files in os.walk(folder):
        for f in files:
            p = os.path.join(root, f)
            with open(p, 'rb') as fh:
                out[os.path.relpath(p, folder)] = hashlib.sha256(fh.read()).hexdigest()
    return out


def git(copy, *args):
    """git in the copy, with a name of its own and an empty e-mail (so that no configuration is needed), no signing."""
    r = subprocess.run(['git', '-C', copy, '-c', 'user.name=test_record', '-c', 'user.email=',
                        '-c', 'commit.gpgsign=false'] + list(args),
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL, universal_newlines=True)
    if r.returncode != 0:
        raise RuntimeError('git %s: %s' % (' '.join(args), r.stderr.strip()))
    return r.stdout.strip()


def record(copy, build, extra):
    cmd = [sys.executable, os.path.join(copy, 'tests', 'run_references.py'), '--record', '--once', '--build', build,
           '--work', os.path.join(copy, 'work')] + extra
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                       universal_newlines=True)
    return r.returncode, r.stdout


def main():
    if len(sys.argv) != 3:
        print('usage: test_record.py REPO BUILD')
        return 2
    repo, build = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    refs = os.path.join(repo, 'tests', 'reference')
    if not shutil.which('git'):
        print('test_record: FAILED: git is needed, and there is none')
        return 1
    before = digest(refs)
    bad = []
    copy = tempfile.mkdtemp(prefix='zippers_test_record_')
    try:
        os.makedirs(os.path.join(copy, 'tests', 'reference'))
        shutil.copyfile(os.path.join(repo, 'tests', 'run_references.py'), os.path.join(copy, 'tests', 'run_references.py'))
        with open(os.path.join(refs, 'runs.json'), encoding='utf-8') as f:
            doc = json.load(f)
        doc['runs'] = [r for r in doc['runs'] if r['id'] in RUNS]
        if [r['id'] for r in doc['runs']] != RUNS:
            print('test_record: FAILED: runs.json does not have the runs %s' % ', '.join(RUNS))
            return 1
        runs_json = os.path.join(copy, 'tests', 'reference', 'runs.json')
        with open(runs_json, 'w', encoding='utf-8') as f:
            json.dump(doc, f, indent=1, ensure_ascii=False)
            f.write('\n')
        git(copy, 'init', '-q')
        git(copy, 'add', '-A')
        git(copy, 'commit', '-q', '-m', 'the copy')
        hashes = [git(copy, 'rev-parse', 'HEAD')]

        def check_hashes(text, when):
            for h in hashes:
                for form in (h, h[:7]):
                    if form in text:
                        bad.append('%s: runs.json holds the commit hash %s' % (when, form))

        # 1. a whole recording: the machine block
        st, out = record(copy, build, [])
        with open(runs_json, encoding='utf-8') as f:
            text = f.read()
        programs = json.loads(text).get('machine', {}).get('programs', '')
        print('the whole recording: exit %d; "programs": "%s"' % (st, programs))
        if st != 0 or 'wrote ' not in out:
            bad.append('the whole recording: exit %d:\n%s' % (st, out))
        if not programs.endswith(STATES):
            bad.append('the whole recording: "programs" does not end with the state of the checkout: "%s"' % programs)
        check_hashes(text, 'the whole recording')

        # 2. a recording of one run: a line of also_recorded
        git(copy, 'add', '-A')
        git(copy, 'commit', '-q', '-m', 'the first recording')
        hashes.append(git(copy, 'rev-parse', 'HEAD'))
        st, out = record(copy, build, ['--only', RUNS[1]])
        with open(runs_json, encoding='utf-8') as f:
            text = f.read()
        also = json.loads(text).get('machine', {}).get('also_recorded', [])
        print('the recording of %s: exit %d; "also_recorded": %s' % (RUNS[1], st, json.dumps(also)))
        if st != 0 or 'wrote ' not in out:
            bad.append('the recording of one run: exit %d:\n%s' % (st, out))
        if len(also) != 1 or not any(s in also[0] for s in STATES) or not also[0].endswith(': ' + RUNS[1]):
            bad.append('the recording of one run: "also_recorded" is not one line with the state of the checkout and '
                       'the run: %s' % json.dumps(also))
        check_hashes(text, 'the recording of one run')
    except RuntimeError as e:
        bad.append(str(e))
    finally:
        shutil.rmtree(copy, ignore_errors=True)
    if digest(refs) != before:
        bad.append("the repository's tests/reference/ changed")
    for b in bad:
        print('FAILED: ' + b)
    print('test_record: %s' % ('%d failure(s)' % len(bad) if bad else
                               'no commit hash in either recording, and the references of the repository unchanged'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
