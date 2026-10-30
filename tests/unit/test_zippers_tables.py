#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_zippers_tables: the option table of zippers (zippers --table) against the programs it runs, both ways.

  (a) every option that zippers passes on is one that its program accepts: the program is run with it (and a value
      it accepts), on light settings, and must not answer that the option is unknown. zipper, zmovie, chz, chz5 and
      zipper5 read all their options before doing anything and name an unknown one, so each is run with the option
      and then "--bogus", and must name --bogus; zipper3 and link3 answer an unknown option with their usage text,
      without naming it, so each is run with the option alone, cheaply, and must not print its usage text;
  (b) every option that a program's own parser accepts (read from its source: the comparisons of the arguments in its
      main, and for chz and chz5 those of parse_kernel_options in src/search/kernel_search.h) is in the table, passed
      on by zippers or left out with a reason; and every option that the table leaves out, or passes on, is one of them.
      ztest and film5, which zippers does not run, must be listed as such.

usage: test_zippers_tables.py REPO BUILD        (Python 3.6 or later, standard library only)
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
BUILD = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, "build"))
ZIPPERS = os.path.join(BUILD, "apps", "cli", "zippers")
BIN = {"zipper": "apps/s2/zipper", "zmovie": "apps/s2/zmovie", "chz": "apps/s3/chz", "zipper3": "apps/s3/zipper3",
       "link3": "apps/s3/link3", "chz5": "apps/s4/chz5", "zipper5": "apps/s4/zipper5"}
SOURCE = {"zipper": "apps/s2/zipper.cc", "zmovie": "apps/s2/zmovie.cc", "chz": "apps/s3/chz.cc",
          "zipper3": "apps/s3/zipper3.cc", "link3": "apps/s3/link3.cc", "chz5": "apps/s4/chz5.cc",
          "zipper5": "apps/s4/zipper5.cc"}

# values that each program accepts for its options (the others take "1")
VALUES = {
    "zipper": {"-o": ["x.png"], "-R": ["2"], "-a": ["0.01"], "-s": ["64"], "--zoom": ["2"], "--yaw": ["10"],
               "--pitch": ["10"], "--overlap": ["1"], "-n": ["3"], "-f": ["png"], "-w": ["1"], "--wheelres": ["100"],
               "--memlimit": ["1"], "--roll": ["5"]},
    "zmovie": {"-o": ["frames"], "-j": ["2"], "--frames": ["0:3"]},
    "chz": {"--tcand": ["0"], "--excess": ["2"], "--jexcess": ["1"]},
    "chz5": {"--tcand": ["0"], "--excess": ["2"], "--jexcess": ["1"]},
    "zipper3": {"-o": ["x.png"], "-R": ["0"], "-a": ["0.3"], "-s": ["64"], "--zoom": ["2"], "--yaw": ["10"],
                "--pitch": ["10"], "--overlap": ["1"], "-f": ["subst_4.00_9.50_t3_r1.txt"], "--only": ["+"],
                "--clip": ["3"], "--thick": ["1"], "--cap": ["1000"], "--centre": ["1,0,0,0"], "--cullk": ["16"],
                "--focus": ["1"], "--pole": ["10", "20"], "--pairs": ["linked_pairs.txt"], "--pairindex": ["1"]},
    "link3": {"-f": ["subst_4.00_9.50_t3.txt"], "-a": ["0.3"], "--rr": ["0.5"], "--maxlen": ["2"], "--nrel": ["0"],
              "--hr": ["0.5"], "--seed": ["2"], "--pair": ["0", "0", "0"], "--nproj": ["2"], "--png": ["stem"],
              "--export": ["nofile.txt"]},
    "zipper5": {"-o": ["x.png"], "-R": ["0"], "-a": ["0.3"], "-s": ["64"], "--zoom": ["2"], "--overlap": ["1"],
                "-f": ["subst5_2.30_4.50_t1_r0.txt"], "--only": ["+"], "--centre": ["1,0,0,0,0"], "--cullk": ["16"],
                "--focus": ["1"], "--turn": ["5"], "--turn2": ["5"], "--hidfog": ["1.5"], "--cap": ["1000"]},
}

n_checks = 0
n_bad = 0


def check(ok, what):
    global n_checks, n_bad
    n_checks += 1
    if not ok:
        n_bad += 1
        print("  FAIL: " + what)
    return ok


def run(cmd, cwd):
    p = subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True, cwd=cwd, timeout=300)
    return p.returncode, p.stdout + p.stderr


def source_options(program):
    """the options (and mode words) that the parser of a program compares its arguments with"""
    text = open(os.path.join(REPO, SOURCE[program]), encoding="utf-8").read()
    main = text[text.index("int main(int argc"):]
    found = set(re.findall(r'\b(?:a|s|a1)\s*==\s*"(--?[A-Za-z][\w-]*)"', main))
    found |= set(re.findall(r'strcmp\(argv\[\d\],\s*"(\w+)"\)', main))
    if program in ("chz", "chz5"):
        ks = open(os.path.join(REPO, "src/search/kernel_search.h"), encoding="utf-8").read()
        body = ks[ks.index("inline bool parse_kernel_options"):]
        body = body[:body.index("\n}\n")]
        found |= set(re.findall(r'\ba\s*==\s*"(--[\w-]+)"', body))
    return found


def main():
    if not os.access(ZIPPERS, os.X_OK):
        print("test_zippers_tables: %s is not built" % ZIPPERS)
        return 1
    p = subprocess.run([ZIPPERS, "--table"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                       universal_newlines=True)
    rows = [line.split("\t") for line in p.stdout.split("\n") if line and not line.startswith("#")]
    check(p.returncode == 0 and rows, "zippers --table")
    passed = {}                            # program -> {program option: number of values}
    for r in rows:
        if r[0] == "option" and r[2] != "-":
            passed.setdefault(r[1], {})[r[2]] = int(r[3])
    left = {}
    for r in rows:
        if r[0] == "left-out":
            left.setdefault(r[1], set()).add(r[2])
    not_run = {r[1] for r in rows if r[0] == "not-run"}
    print("test_zippers_tables: %d options passed on, %d left out, %d programs not run" % (
        sum(len(v) for v in passed.values()), sum(len(v) for v in left.values()), len(not_run)))

    tmp = tempfile.mkdtemp(prefix="zippers_tables_test_")
    try:
        with open(os.path.join(tmp, "shot.txt"), "w") as f:
            f.write("frames = 1\nmonodromy = fig8\norder = 2\nR = 0\n")
        print("(a) every option passed on is accepted by its program")
        for program, opts in sorted(passed.items()):
            exe = os.path.join(BUILD, BIN[program])
            for opt, n in sorted(opts.items()):
                vals = VALUES.get(program, {}).get(opt, ["1"] * n)
                if len(vals) != n:
                    check(False, "%s %s: the test has %d values, the table %d" % (program, opt, len(vals), n))
                    continue
                if program == "zipper":
                    code, out = run([exe, opt] + vals + ["--bogus"], tmp)
                    ok = "unknown option --bogus" in out
                elif program == "zmovie":
                    if opt == "--assemble":
                        code, out = run([exe, opt, "nofile.txt", "out"], tmp)
                        ok = "cannot read nofile.txt" in out and "unknown argument" not in out
                    else:
                        code, out = run([exe, "shot.txt", opt] + vals + ["--bogus"], tmp)
                        ok = "unknown argument --bogus" in out
                elif program in ("chz", "chz5"):
                    nums = ["4", "9.5", "3"] if program == "chz" else ["2.3", "4.5", "3"]
                    code, out = run([exe, "kernel"] + nums + [opt] + vals + ["--bogus"], tmp)
                    ok = "unknown option --bogus" in out and ("unknown option %s\n" % opt) not in out
                elif program == "zipper5":
                    code, out = run([exe, opt] + vals + ["--bogus"], tmp)
                    ok = "unknown option --bogus" in out
                elif program == "zipper3":
                    base = [] if opt in ("-R", "-a") else ["-R", "0", "-a", "0.3"]
                    code, out = run([exe] + base + [opt] + vals, tmp)
                    ok = "usage: zipper3" not in out
                elif program == "link3":
                    code, out = run([exe, "--rr", "0.5", "--nrel", "0", "--hr", "0.5", "--hinfo", opt] + vals, tmp)
                    ok = "usage: link3" not in out
                else:
                    ok, out = False, "no recipe for " + program
                check(ok, "%s accepts %s %s (got: %s)" % (program, opt, " ".join(vals), out.strip()[-300:]))
        print("(b) every option of the programs' parsers is in the table, and nothing else")
        for program in sorted(SOURCE):
            src = source_options(program)
            table = set(passed.get(program, {})) | left.get(program, set())
            check(src, "%s: options found in %s" % (program, SOURCE[program]))
            check(src <= table, "%s: the options of its parser that the table does not name: %s" % (
                program, sorted(src - table)))
            check(table <= src, "%s: the options of the table that its parser does not take: %s" % (
                program, sorted(table - src)))
        check(not_run == {"ztest", "film5"}, "the programs that zippers does not run: %s" % sorted(not_run))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("test_zippers_tables: %d checks, %d fail" % (n_checks, n_bad))
    return 1 if n_bad else 0


if __name__ == "__main__":
    sys.exit(main())
