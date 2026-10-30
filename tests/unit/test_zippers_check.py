#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_zippers_check: zippers check itself (its verdicts, its environment, its clean-up), with stand-ins for its programs.

A copy of zippers is put in T/bin/, beside a folder T/libexec/zippers/ of small shell scripts named like the programs
it runs (zipper, chz, link3, zipper3, chz5, zipper5); the copy, being outside its build, runs those (as an installed
zippers runs the programs of its libexec/). Each script answers as the file "mode" beside it says:
  - pass: the output that the real program gave in the reference runs (tests/reference/), so that every check passes;
  - fail: the same output with one change that the check must see (a "NO" for a "yes", one projection fewer, a
    "FAILED"; zipper exits with status 2), and exit status 0 but for zipper, so that only the reading of the output
    can find the failure;
  - wait: it writes its process number into the file "started", and waits;
  - hold: it writes its process number into the file "started", waits until the file "go" exists, then answers as pass.
Every script first stops with status 9 if ZIPPERS_DATA is in its environment.
The cases:
  1. pass: "all 21 checks pass", exit status 0;
  2. fail: each of the 21 checks reported FAIL, with its command and the program's output, "21 of 21 checks FAILED",
     exit status 1;
  3. pass, with ZIPPERS_DATA set in zippers' environment (to a folder of files under the defaults' names): still all
     pass, since the checks run without it;
  4. wait, then SIGINT, SIGTERM and SIGHUP in turn sent to zippers: it ends by that signal, says so, the temporary folder
     is gone, and so is the waiting script;
  5. hold, with SIGHUP and then SIGINT ignored when zippers starts (as under nohup, and in a background job of a shell
     without job control), zippers in a process group of its own: the signal sent to that group (zippers, the waiting
     script and its sleep, as a terminal sends Ctrl-C) changes nothing, and after "go" every check passes, exit 0;
  6. hold, and what reads zippers' output stops after its first line (as `zippers check | head -1`): after "go",
     zippers ends by SIGPIPE at its next line, says so, the temporary folder is gone, and no other check is run; with
     SIGPIPE ignored when zippers starts, it runs every check, ends with exit status 0, and the folder is gone.
TMPDIR is set, for zippers, to a folder of the test's own temporary folder.

usage: test_zippers_check.py REPO BUILD        (Python 3.6 or later, standard library only)
"""
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

REPO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
BUILD = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, "build"))
ZIPPERS = os.path.join(BUILD, "apps", "cli", "zippers")
REF = os.path.join(REPO, "tests", "reference")
PAIRS = ["4_10_1", "4_12_1", "4_17_1", "4_14_6", "2_10_13", "4_4_13", "10_3_13", "10_4_13", "13_10_13", "17_4_13",
         "2_12_16", "12_4_16", "13_12_16"]

SCRIPT = r"""#!/bin/sh
# a stand-in for one of the programs that zippers check runs (test_zippers_check.py)
here=$(cd "$(dirname "$0")" && pwd)
prog=$(basename "$0")
mode=$(cat "$here/mode")
if [ -n "${ZIPPERS_DATA+x}" ]; then echo "ZIPPERS_DATA reached the check: $ZIPPERS_DATA"; exit 9; fi
if [ "$mode" = wait ]; then echo "$$" >> "$here/started"; exec sleep 60; fi
if [ "$mode" = hold ]; then echo "$$" >> "$here/started"; while [ ! -e "$here/go" ]; do sleep 0.1; done; mode=pass; fi
case "$prog" in
  zipper) if [ "$mode" = fail ]; then exit 2; fi; exit 0 ;;
  zipper3|zipper5) key=stats ;;
  chz|chz5) key=$1 ;;
  link3) if [ "$1" = selftest ]; then key=selftest; else key="pair_$8_$9_${10}"; fi ;;
esac
cat "$here/out/$prog.$key.$mode"
"""

n_checks = 0
n_bad = 0


def check(ok, what, detail=""):
    """one check: `what` is printed in any case, `detail` only when it fails"""
    global n_checks, n_bad
    n_checks += 1
    print("  %s: %s" % ("ok" if ok else "FAIL", what))
    if not ok:
        n_bad += 1
        if detail:
            print("    " + detail.strip().replace("\n", "\n    "))
    return ok


def pid_in(path):
    """the process number that a waiting script wrote into `path`, once its line is complete; 0 before"""
    try:
        with open(path) as f:
            text = f.read()
    except OSError:
        return 0
    words = text.split()
    return int(words[0]) if text.endswith("\n") and words and words[0].isdigit() else 0


def recorded(run):
    with open(os.path.join(REF, run, "stdout.txt"), encoding="utf-8") as f:
        return f.read()


def outputs():
    """{(program, key): (the output that passes, one that fails)}"""
    o = {}
    sel = recorded("s3_chz_selftest")
    o[("chz", "selftest")] = (sel, sel.replace(": yes", ": NO", 1))
    k3 = recorded("s3_chz_kernel")
    o[("chz", "kernel")] = (k3, k3.replace("generates K: YES", "generates K: NO"))
    ls = recorded("s3_link3_selftest")
    o[("link3", "selftest")] = (ls, ls.replace("-1/-1 (", "-1/+1 (", 1))
    for p in PAIRS:
        a, b, h = p.split("_")
        out = recorded("s3_link3_pair_w%s_w%s_h%s" % (a, b, h))
        lines = out.split("\n")
        last = max(i for i, x in enumerate(lines) if "projection" in x)
        o[("link3", "pair_" + p)] = (out, "\n".join(lines[:last] + lines[last + 1:]))
    s5 = recorded("s4_chz5_selftest")
    o[("chz5", "selftest")] = (s5, s5.replace("; ok", "; FAILED"))
    k5 = recorded("s4_chz5_kernel")
    o[("chz5", "kernel")] = (k5, k5.replace("generates K: YES", "generates K: NO"))
    stats = "roots: 1 elements in B(1), of 1 searched in B(1); checked: closed under the steps inside that ball\n"
    for prog in ("zipper3", "zipper5"):
        o[(prog, "stats")] = (stats, stats.replace("checked:", "FAILED:"))
    return o


def make_tree(top):
    """T/bin/zippers (a copy) and T/libexec/zippers/ (the scripts and their outputs); returns T/libexec/zippers"""
    os.makedirs(os.path.join(top, "bin"))
    lib = os.path.join(top, "libexec", "zippers")
    os.makedirs(os.path.join(lib, "out"))
    shutil.copy2(ZIPPERS, os.path.join(top, "bin", "zippers"))
    for prog in ("zipper", "chz", "link3", "zipper3", "chz5", "zipper5"):
        path = os.path.join(lib, prog)
        with open(path, "w") as f:
            f.write(SCRIPT)
        os.chmod(path, 0o755)
    for (prog, key), (good, bad) in outputs().items():
        for mode, text in (("pass", good), ("fail", bad)):
            with open(os.path.join(lib, "out", "%s.%s.%s" % (prog, key, mode)), "w") as f:
                f.write(text)
    return lib


def set_mode(lib, mode):
    with open(os.path.join(lib, "mode"), "w") as f:
        f.write(mode + "\n")


def leftovers(tmp):
    return [x for x in os.listdir(tmp) if x.startswith("zippers_check_")]


def clear_leftovers(tmp):
    """the folders that an earlier case left (and reported), removed, so that each case reports only its own"""
    for x in leftovers(tmp):
        shutil.rmtree(os.path.join(tmp, x), ignore_errors=True)


def starting_with(disposition, sigs):
    """a preexec_fn: zippers starts with the signals `sigs` at `disposition`, whatever they are for the test itself
    (which may have been started with some of them ignored)"""
    def f():
        for s in sigs:
            signal.signal(s, disposition)
    return f


def main():
    if not os.access(ZIPPERS, os.X_OK):
        print("test_zippers_check: %s is not built" % ZIPPERS)
        return 1
    top = tempfile.mkdtemp(prefix="zippers_check_test_")
    try:
        lib = make_tree(top)
        z = os.path.join(top, "bin", "zippers")
        tmp = os.path.join(top, "tmp")
        os.makedirs(tmp)
        env = dict(os.environ)
        env.pop("ZIPPERS_DATA", None)
        env["TMPDIR"] = tmp
        work = os.path.join(top, "work")
        os.makedirs(work)

        def run(mode, extra_env=None):
            set_mode(lib, mode)
            e = dict(env)
            e.update(extra_env or {})
            p = subprocess.run([z, "check"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True, cwd=work, env=e, timeout=300)
            return p.returncode, p.stdout, p.stderr

        print("test_zippers_check: a copy of zippers, with stand-ins for its programs, in %s" % top)
        code, out, err = run("pass")
        check(code == 0 and "zippers check: all 21 checks pass" in out and out.count(" pass (") == 21,
              "every check passes with the recorded outputs, exit 0", "exit %d: %s" % (code, (out + err)[-400:]))
        check(not leftovers(tmp), "no temporary folder is left", "left: %s" % leftovers(tmp))

        code, out, err = run("fail")
        fails = re.findall(r"^  S[234]  .* FAIL \(", out, re.M)
        check(code == 1 and len(fails) == 21 and "zippers check: 21 of 21 checks FAILED" in out,
              "a failure in every check's output: each of the 21 reported FAIL, exit 1",
              "exit %d, %d FAIL lines: %s" % (code, len(fails), (out + err)[-400:]))
        check(out.count("    the command: ") == 21 and out.count("    its output:") == 21,
              "each failing check is shown with its command and the program's output",
              "%d commands, %d outputs shown" % (out.count("    the command: "), out.count("    its output:")))
        check("exit status 2" in out, "the S2 check fails on zipper's exit status 2", out[:400])
        check(not leftovers(tmp), "no temporary folder is left", "left: %s" % leftovers(tmp))

        data = os.path.join(top, "data")
        for g, names in (("s3", ["subst_4.00_9.50_t3_r1.txt", "subst_4.00_9.50_t3.txt"]),
                         ("s4", ["subst5_2.30_4.50_t1_r0.txt"])):
            os.makedirs(os.path.join(data, g))
            for n in names:
                shutil.copyfile(os.path.join(REPO, "data", g, "subst_4.00_9.50_t3_r0.txt" if g == "s3" else
                                             "subst5_2.30_4.50_t1_r1.txt"), os.path.join(data, g, n))
        code, out, err = run("pass", {"ZIPPERS_DATA": data})
        check(code == 0 and "all 21 checks pass" in out and "ZIPPERS_DATA reached" not in out,
              "with ZIPPERS_DATA set for zippers, the checks run without it: all pass",
              "exit %d: %s" % (code, (out + err)[-400:]))
        code, out, err = run("pass", {"ZIPPERS_DATA": ""})
        check(code == 0 and "all 21 checks pass" in out, "with ZIPPERS_DATA set to the empty string: the same",
              "exit %d: %s" % (code, (out + err)[-400:]))

        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            clear_leftovers(tmp)
            set_mode(lib, "wait")
            started = os.path.join(lib, "started")
            if os.path.exists(started):
                os.remove(started)
            caught = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)
            p = subprocess.Popen([z, "check"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, universal_newlines=True, cwd=work, env=env,
                                 preexec_fn=starting_with(signal.SIG_DFL, caught))
            t0 = time.monotonic()
            pid = pid_in(started)
            while not pid and time.monotonic() - t0 < 30:
                time.sleep(0.05)
                pid = pid_in(started)
            folders = leftovers(tmp)
            p.send_signal(sig)
            try:
                out, err = p.communicate(timeout=60)
            except subprocess.TimeoutExpired:
                p.kill()
                out, err = p.communicate()
            name = signal.Signals(sig).name
            check(pid > 0 and len(folders) == 1, "%s: the first check had started, in a temporary folder" % name,
                  "process %d; folders %s" % (pid, folders))
            check(p.returncode == -sig, "%s: zippers ends by the signal" % name, "status %s" % p.returncode)
            check(("zippers check: interrupted (%s): the running check was stopped, and " % name) in err and
                  "was removed" in err, "%s: it says so, and that the folder was removed" % name, err[-400:])
            check(not leftovers(tmp), "%s: the temporary folder is gone" % name, "left: %s" % leftovers(tmp))
            gone = True
            if pid > 0:
                for _ in range(100):
                    try:
                        os.kill(pid, 0)
                    except ProcessLookupError:
                        break
                    except PermissionError:
                        break
                    time.sleep(0.05)
                else:
                    gone = False
                    os.kill(pid, signal.SIGKILL)
            check(gone, "%s: the running check was stopped" % name, "process %d was still running" % pid)

        for sig in (signal.SIGHUP, signal.SIGINT):
            name = signal.Signals(sig).name
            clear_leftovers(tmp)
            set_mode(lib, "hold")
            started = os.path.join(lib, "started")
            go = os.path.join(lib, "go")
            for f in (started, go):
                if os.path.exists(f):
                    os.remove(f)
            p = subprocess.Popen([z, "check"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,   # as under nohup
                                 stderr=subprocess.PIPE, universal_newlines=True, cwd=work, env=env,
                                 start_new_session=True, preexec_fn=starting_with(signal.SIG_IGN, (sig,)))
            t0 = time.monotonic()
            pid = pid_in(started)
            while not pid and time.monotonic() - t0 < 30:
                time.sleep(0.05)
                pid = pid_in(started)
            os.killpg(p.pid, sig)                           # zippers, the waiting script, and its sleep
            time.sleep(0.5)
            running = p.poll() is None
            open(go, "w").close()
            try:
                out, err = p.communicate(timeout=120)
            except subprocess.TimeoutExpired:
                p.kill()
                out, err = p.communicate()
            check(pid > 0 and running, "%s ignored at the start: zippers goes on after it" % name,
                  "process %d; zippers %s" % (pid, "running" if running else "ended, status %s" % p.returncode))
            check(p.returncode == 0 and "zippers check: all 21 checks pass" in out,
                  "%s ignored at the start: the waiting check goes on too, and every check passes, exit 0" % name,
                  "exit %s: %s" % (p.returncode, (out + err)[-400:]))
            check(not leftovers(tmp), "%s ignored at the start: no temporary folder is left" % name,
                  "left: %s" % leftovers(tmp))

        for ignored in (False, True):
            how = "SIGPIPE ignored at the start" if ignored else "SIGPIPE"
            clear_leftovers(tmp)
            set_mode(lib, "hold")
            started = os.path.join(lib, "started")
            go = os.path.join(lib, "go")
            for f in (started, go):
                if os.path.exists(f):
                    os.remove(f)
            p = subprocess.Popen([z, "check"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, universal_newlines=True, cwd=work, env=env,
                                 preexec_fn=starting_with(signal.SIG_IGN if ignored else signal.SIG_DFL,
                                                          (signal.SIGPIPE,)))
            first = p.stdout.readline()
            t0 = time.monotonic()
            pid = pid_in(started)
            while not pid and time.monotonic() - t0 < 30:
                time.sleep(0.05)
                pid = pid_in(started)
            p.stdout.close()                                # what reads zippers' output stops
            open(go, "w").close()                           # the first check answers; zippers writes its line
            try:
                p.wait(timeout=120)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
            err = p.stderr.read()
            p.stderr.close()
            with open(started) as f:
                runs = len(f.read().split())
            check(first.startswith("zippers check: 21 exact checks") and pid > 0,
                  "%s: the first line was read, and the first check had started" % how, "%r; process %d" % (first, pid))
            if ignored:
                check(p.returncode == 0 and runs == 21, "%s: every check is run, exit 0" % how,
                      "status %s, %d checks run: %s" % (p.returncode, runs, err[-400:]))
            else:
                check(p.returncode == -signal.SIGPIPE and runs == 1,
                      "%s: zippers ends by the signal at its next line, and runs no other check" % how,
                      "status %s, %d checks run: %s" % (p.returncode, runs, err[-400:]))
                check("zippers check: interrupted (SIGPIPE): " in err and "was removed" in err and
                      "the running check was stopped" not in err,
                      "%s: it says so, and that the folder was removed (no check was running)" % how, err[-400:])
            check(not leftovers(tmp), "%s: no temporary folder is left" % how, "left: %s" % leftovers(tmp))
    finally:
        shutil.rmtree(top, ignore_errors=True)
    print("test_zippers_check: %d checks, %d fail" % (n_checks, n_bad))
    return 1 if n_bad else 0


if __name__ == "__main__":
    sys.exit(main())
