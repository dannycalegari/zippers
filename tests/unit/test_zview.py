#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_zview: zview --headless (the window's model, its jobs and the key feed, with no window) and zippers view.

  - zview without --headless: it checks its options, then, in a build without the window, says that this build has no
    window (exit 1); in a build with it the test never lets it open one: every run here has ZIPPERS_NO_WINDOW set, with
    which zview says that it opens no window (exit 1). Its usage text has no --headless, and says that the build has no
    window only in a build without it;
  - EVERY KEY in every geometry and mode, fed by ZIPPERS_KEYS to zview --headless: S2 (the zippers; the wheel; the
    detail), S3 (the zippers; the linked pairs, with their context; the detail), S4 (the zippers; the detail), each
    printable character but q and <, the arrows, Tab, a pause, a drag, the scroll wheel, a resize and <S>, with jobs
    running between them; it ends with Esc, exit 0, nothing refused, and no file of a picture left half written;
  - RANDOM SEQUENCES from fixed seeds in each geometry (with at most two presses each of + and ], so that the jobs stay
    small): exit 0;
  - the feed's refusals (an unknown <...>, a < with no >, <G> with no window, a bad <drag> or <scroll>): exit 1, and the
    tokens after them are still fed; q ends the feed; <S> writes STATE_01.zsession, which zippers draw takes; a picture
    whose job is still running when the feed ends is removed, with its session;
  - zippers view: its table (every draw option but the outputs and those that only print, passed to zview unchanged),
    its refusals, --dry-run, the default example (fig8 -n 2 -R 6), --session, its usage (with view's own -o STEM and
    --session FILE), and the message without a window.
Every run is in a folder of the test's own, without ZIPPERS_DATA, with a time limit; the jobs are small (S2 at R = 2,
S3 and S4 at R = 0, coarse accuracies). About a minute.

usage: test_zview.py BUILD [WINDOW]    (WINDOW: ON when the build has the window, else OFF, the default; Python 3.6
                                       or later, standard library only)
"""
import os
import random
import shlex
import shutil
import subprocess
import sys
import tempfile

BUILD = sys.argv[1] if len(sys.argv) > 1 else "build"
WINDOW = (sys.argv[2] if len(sys.argv) > 2 else "OFF") == "ON"
ZVIEW = os.path.join(BUILD, "apps", "gui", "zview")
ZIPPERS = os.path.join(BUILD, "apps", "cli", "zippers")
TIMEOUT = 600                                  # seconds, for one run: wide, for slower machines

n_checks = 0
n_bad = 0


def check(ok, what):
    global n_checks, n_bad
    n_checks += 1
    if not ok:
        n_bad += 1
        print("  FAIL: " + what)
    return ok


def section(title):
    print(title)


ENV = dict(os.environ)
ENV.pop("ZIPPERS_DATA", None)
ENV.pop("ZIPPERS_KEYS", None)
ENV["ZIPPERS_NO_WINDOW"] = "1"                 # with the window, zview then never opens it
NO_WINDOW = "ZIPPERS_NO_WINDOW is set" if WINDOW else "this build has no window"


def run(program, args, cwd, keys=None):
    env = dict(ENV)
    if keys is not None:
        env["ZIPPERS_KEYS"] = keys
    p = subprocess.run([program] + args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True, cwd=cwd, env=env, timeout=TIMEOUT)
    return p.returncode, p.stdout, p.stderr


def headless(args, keys, cwd):
    return run(ZVIEW, ["--headless"] + args, cwd, keys)


def fresh(root, name):
    d = os.path.join(root, name)
    os.mkdir(d)
    return d


def half_written(folder):
    """the pictures without their session, the sessions without their picture, and temporary files"""
    names = os.listdir(folder)
    bad = [n for n in names if ".tmp" in n]
    for n in names:
        stem, ext = os.path.splitext(n)
        if ext == ".png" and not stem.startswith("again") and stem + ".zsession" not in names:
            bad.append(n)
        if ext == ".zsession" and not stem.startswith("STATE_") and stem + ".png" not in names:
            bad.append(n)
    return bad


# every key: the printable characters but q (it quits) and < (it begins a token), and the other tokens
TOKENS = [c for c in map(chr, range(33, 127)) if c not in "q<"] + \
         ["<L>", "<R>", "<U>", "<D>", "<T>", "<W>", "<drag 3,4>", "<drag -10,2.5>", "<scroll 2>", "<scroll -1>",
          "<s300x200>", "<S>", " "]
KEYS = "".join(TOKENS)
# the same, waiting for the jobs after each key that starts one
KEYS_WAITING = "".join(t + ("<wait>" if t in "+-=_[]cCfpP" else "") for t in TOKENS)

GEOMETRIES = [
    ("S2, the zippers", ["fig8", "-R", "2", "-a", "0.02"], ""),
    ("S2, the wheel", ["fig8", "-R", "2", "-a", "0.02", "-C"], ""),
    ("S2, the detail", ["fig8", "-R", "2", "-a", "0.02"], "zf<wait>"),
    ("S3, the zippers", ["cs", "-a", "0.1"], ""),
    ("S3, the linked pairs", ["cs", "-a", "0.1", "--pairs", "linked_pairs.txt"], "l"),
    ("S3, the detail", ["cs", "-a", "0.1"], "zf<wait>"),
    ("S4, the zippers", ["imm", "-a", "0.05"], ""),
    ("S4, the detail", ["imm", "-a", "0.05"], "zf<wait>"),
]


def every_key(root):
    section("every key in every geometry and mode")
    for k, (name, args, first) in enumerate(GEOMETRIES):
        d = fresh(root, "keys%d" % k)
        # each key twice: once with jobs running, once after waiting for them
        keys = first + KEYS + KEYS_WAITING + "<wait><E>"
        code, out, err = headless(args, keys, d)
        check(code == 0 and "refused" not in err, "%s: every key, exit 0, nothing refused (exit %d: %s)" % (
            name, code, err[-400:]))
        check(not half_written(d), "%s: no picture half written (%s)" % (name, half_written(d)))
        check(os.path.exists(os.path.join(d, "STATE_01.zsession")), "%s: <S> wrote STATE_01.zsession" % name)


def random_sequences(root):
    section("random sequences from fixed seeds")
    tokens = list("+-_=][zZxXcCbrnNlhHjJwsfp?") + ["<L>", "<R>", "<U>", "<D>", "<T>", "<drag 5,-3>", "<scroll 1>",
                                                    "<scroll -2>", "<wait>", "<wait>"]
    for name, args, first in GEOMETRIES:
        if "detail" in name:
            continue
        for seed in (1, 2, 3):
            rng = random.Random(seed)
            seq, plus, finer = [], 0, 0
            while len(seq) < 40:
                t = rng.choice(tokens)
                if t in ("+", "="):
                    if plus == 2:
                        continue
                    plus += 1
                if t == "]":
                    if finer == 2:
                        continue
                    finer += 1
                seq.append(t)
            d = fresh(root, "random_%s_%d" % (name.replace(" ", "").replace(",", ""), seed))
            code, out, err = headless(args, first + "".join(seq) + "<wait>", d)
            check(code == 0 and "refused" not in err and not half_written(d),
                  "%s, seed %d: %s: exit 0 (exit %d: %s)" % (name, seed, "".join(seq), code, err[-400:]))


def feed(root):
    section("the feed: refusals, q, <S>, a picture not finished")
    d = fresh(root, "feed")
    for keys, what in [("<bogus>z", "an unknown token"), ("z<", "a < with no >"), ("<G>z", "<G> with no window"),
                       ("<drag 1>z", "<drag> with one number"), ("<scroll x>z", "<scroll> with no integer"),
                       ("<s0x5>z", "a size 0")]:
        code, out, err = headless(["imm", "-a", "0.1"], keys, d)
        check(code == 1 and "refused" in err and ("zoom 1.5" in err or keys.endswith("<")),
              "%s (%s): exit 1, refused, and the rest fed (exit %d: %s)" % (keys, what, code, err[-300:]))
    code, out, err = headless(["imm", "-a", "0.1"], "zq+z", d)
    check(code == 0 and "zoom 1.5" in err and "computing R" not in err and "zoom 2.25" not in err,
          "z q + z: q ends the feed (exit %d: %s)" % (code, err[-300:]))
    code, out, err = headless(["imm", "-a", "0.1"], "", d)
    check(code == 0, "an empty sequence: exit 0 (exit %d: %s)" % (code, err[-300:]))
    d2 = fresh(root, "state")
    code, out, err = headless(["cs", "-a", "0.1"], "z<L><S>", d2)
    code2, out2, err2 = run(ZIPPERS, ["--dry-run", "draw", "--session", "STATE_01.zsession", "-o", "x.png"], d2)
    check(code == 0 and code2 == 0 and "--viewmatrix" in out2 and "--zoom 1.5" in out2,
          "<S>: STATE_01.zsession, which zippers draw takes (exit %d, %d: %s %s)" % (code, code2, out2, err2[-200:]))
    d3 = fresh(root, "unfinished")
    code, out, err = headless(["fig8", "-R", "3", "-a", "0.004"], "p", d3)
    check(code == 0 and not os.listdir(d3), "p, and the feed ends at once: the picture and its session removed "
          "(exit %d, %s)" % (code, os.listdir(d3)))
    code, out, err = headless(["fig8", "-R", "2", "-a", "0.02"], "p<wait>p<wait>", d3)
    check(code == 0 and sorted(os.listdir(d3)) == ["fig8.png", "fig8.zsession", "fig8_1.png", "fig8_1.zsession"],
          "p p: fig8 and fig8_1, overwriting nothing (%s)" % sorted(os.listdir(d3)))


def zview_without_window(root):
    section("zview without --headless")
    d = fresh(root, "nowindow")
    code, out, err = run(ZVIEW, ["fig8", "-R", "2"], d)
    check(code == 1 and NO_WINDOW in err and not os.listdir(d),
          "zview fig8: exit 1, '%s', nothing written (exit %d: %s)" % (NO_WINDOW, code, err))
    code, out, err = run(ZVIEW, [], d)
    check(code == 1 and NO_WINDOW in err, "zview with no example: the same (exit %d: %s)" % (code, err))
    code, out, err = run(ZVIEW, ["fig8", "--bogus"], d)
    check(code == 1 and "unknown option --bogus" in err, "zview fig8 --bogus: refused (exit %d: %s)" % (code, err))
    code, out, err = run(ZVIEW, ["cs", "--polylines", "x.zpl"], d)
    check(code == 1 and "not an option of the window" in err, "zview cs --polylines: refused (exit %d: %s)" % (code, err))
    code, out, err = run(ZVIEW, ["--help"], d)
    check(code == 0 and "usage: zview" in out and "headless" not in out and ("has no window" in out) != WINDOW,
          "zview --help: its usage, without --headless, saying that the build has no window only without one")


# a plausible value for an option of draw (zippers does not look at most values)
def values_for(option, n):
    special = {"--only": ["+"], "--phi": ["1,0"], "--centre": ["1,0,0,0"], "-o": ["pic"], "-f": ["file.txt"],
               "--pairs": ["linked_pairs.txt"], "--pole": ["10", "20"], "--colour": ["Z+=0,0,1"]}
    return special.get(option, ["7"] * n)[:n]


def zippers_view(root):
    section("zippers view")
    d = fresh(root, "view")

    def dry(args):
        code, out, err = run(ZIPPERS, ["--dry-run", "view"] + args, d)
        words = shlex.split(out.strip()) if code == 0 and out.strip() else []
        return code, words, err

    code, words, err = dry([])
    check(code == 0 and words and os.path.basename(words[0]) == "zview" and words[1:] == ["fig8", "-n", "2", "-R", "6"],
          "zippers --dry-run view: zview fig8 -n 2 -R 6 (exit %d: %s %s)" % (code, words, err))
    code, out, err = run(ZIPPERS, ["--table"], d)
    rows = [l.split("\t") for l in out.split("\n") if l.startswith("option\t")]
    left = {"--stats", "--check", "--timing", "--polylines", "--format", "--session"}
    examples = {"S2": "fig8", "S3": "cs", "S4": "imm"}
    n = 0
    for _, program, popt, nvalues, command, sphere, zopt, shown in rows:
        if command != "draw":
            continue
        args = [examples[sphere], zopt] + values_for(zopt, int(nvalues))
        code, words, err = dry(args)
        if zopt in left:
            if zopt == "--session":
                check(code == 1 and "takes the place of the example" in err, "view %s: refused" % " ".join(args))
            else:
                check(code == 1 and "is not an option of zippers view" in err,
                      "view %s: refused, an output or an option that only prints (exit %d: %s)" % (" ".join(args), code,
                                                                                                    err))
            continue
        n += 1
        check(code == 0 and words[1:] == args, "view %s: zview with the same arguments (exit %d: %s %s)" % (
            " ".join(args), code, words, err))
    check(n > 30, "the options of draw that view takes: %d" % n)
    for args, message in [(["bogus"], "unknown example 'bogus'"), (["cs", "-n", "3"], "it is one for fig8"),
                          (["fig8", "-R"], "-R needs 1 value"), (["imm", "--pole", "1", "2"], "not an option for"),
                          (["fig8", "--session", "x"], "takes the place of the example"),
                          (["--session", "nofile.zsession"], "cannot read it"),
                          (["cs", "--phi", "2,1"], "only phi = 1,0")]:
        code, words, err = dry(args)
        check(code == 1 and message in err, "view %s: refused with '%s' (exit %d: %s)" % (" ".join(args), message, code,
                                                                                         err))
    # a session: written by zview's <S>, and passed on with the options after it
    code, out, err = headless(["imm", "-a", "0.1"], "z<S>", d)
    code, words, err = dry(["--session", "STATE_01.zsession", "-R", "1", "-o", "pic"])
    check(code == 0 and words[1:] == ["--session", "STATE_01.zsession", "-R", "1", "-o", "pic"],
          "view --session STATE_01.zsession -R 1 -o pic: zview with the same arguments (exit %d: %s %s)" % (code, words,
                                                                                                            err))
    code, words, err = dry(["--session", "STATE_01.zsession", "--pole", "1", "2"])
    check(code == 1 and "not an option for" in err, "view --session (S4) --pole: refused (exit %d: %s)" % (code, err))
    code, out, err = run(ZIPPERS, ["view", "--help"], d)
    check(code == 0 and "usage: zippers view" in out and "  -R R" in out and "--pole D1 D2" in out and
          "  --polylines" not in out and "  --stats" not in out and "  --format" not in out,
          "zippers view --help: draw's options, less the outputs and those that only print")
    lines = {l.split()[0]: l for l in out.split("\n") if l.startswith("  -")}
    check(lines.get("-o", "").startswith("  -o STEM") and "the stem of the pictures that p saves" in lines.get("-o", "")
          and "zippers view --session FILE" in lines.get("--session", "") and "PNG" not in lines.get("-o", ""),
          "zippers view --help: -o STEM and --session FILE say what view takes (%s | %s)" % (lines.get("-o"),
                                                                                         lines.get("--session")))
    check(("has no window" in out) != WINDOW, "zippers view --help says that the build has no window only without one")
    code, out, err = run(ZIPPERS, ["--help"], d)
    check(code == 0 and ("has no window" in out + err) != WINDOW,
          "zippers --help says that the build has no window only without one")
    for args in (["fig8", "-R", "2"], [], ["--session", "STATE_01.zsession"]):
        code, out, err = run(ZIPPERS, ["view"] + args, d)
        check(code == 1 and NO_WINDOW in err,
              "zippers view %s: exit 1, '%s' (exit %d: %s)" % (" ".join(args), NO_WINDOW, code, err))


def main():
    for p in (ZVIEW, ZIPPERS):
        if not os.access(p, os.X_OK):
            print("test_zview: %s is not built" % p)
            return 1
    print("test_zview: %s" % ZVIEW)
    root = tempfile.mkdtemp(prefix="zippers_test_zview_")
    try:
        zview_without_window(root)
        feed(root)
        every_key(root)
        random_sequences(root)
        zippers_view(root)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("test_zview: %d checks, %d fail" % (n_checks, n_bad))
    return 1 if n_bad else 0


if __name__ == "__main__":
    sys.exit(main())
