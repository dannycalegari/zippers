#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_zippers_cli: the command line zippers (apps/cli/zippers): what it runs, what it refuses, and its help.

Almost everything here uses zippers --dry-run, which prints the program command line that zippers would run, and runs
nothing; so the test compares command lines, never pictures, and holds on every platform.
  - every option of every command and sphere, from zippers' own table (zippers --table): it is passed on to the right
    program, under the program's name for it, with its values;
  - the translations, the defaults and the aliases, case by case (the S2 example as -m WORD, --format as -f, --rho as
    chz5's third number, --phi 1,0 dropped, search without numbers, link's settings, check's list);
  - the refusals, each with exit status 1 and its message: the options of the table that zippers leaves out, the
    options of another sphere, the wrong values that zippers checks itself, and a value that looks like an option ('-'
    and a letter, or '--'), while negative numbers and '-' itself stay values;
  - the quoting of --dry-run: arguments with spaces, quotes, '$', a backquote, a backslash, a newline, and the empty
    argument, given back exactly when the printed line is read by sh -c;
  - the usage texts (exit status 0), each listing the options of its command, and no hidden one;
  - an S2 picture without -o is refused when the standard output is a terminal (a pseudo-terminal here);
  - four messages of the programs: zipper5 -h (its usage text), zipper -v (no window in this build), ztest with an
    unknown word (refused), and link3 when a picture cannot be written.

usage: test_zippers_cli.py BUILD             (Python 3.6 or later, standard library only)
"""
import os
import pty
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading

BUILD = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build")
ZIPPERS = os.path.join(BUILD, "apps", "cli", "zippers")
EXAMPLE = {"S2": "fig8", "S3": "cartwright-steger", "S4": "imm-m5"}
SPHERE_OF = {"zipper": "S2", "zmovie": "S2", "zipper3": "S3", "chz": "S3", "link3": "S3", "zipper5": "S4",
             "chz5": "S4"}

n_checks = 0
n_bad = 0


def check(ok, what):
    global n_checks, n_bad
    n_checks += 1
    if not ok:
        n_bad += 1
        print("  FAIL: " + what)
    return ok


def run(args, cwd=None, env=None):
    p = subprocess.run([ZIPPERS] + args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True, cwd=cwd, env=env)
    return p.returncode, p.stdout, p.stderr


def dry(args):
    """zippers --dry-run ARGS: (exit status, the program's name, its arguments, stderr)"""
    code, out, err = run(["--dry-run"] + args)
    if code != 0 or not out.strip():
        return code, None, None, err
    words = shlex.split(out.strip().split("\n")[0])
    return code, os.path.basename(words[0]), words[1:], err


def expect(args, program, program_args, note=None):
    code, prog, pargs, err = dry(args)
    ok = code == 0 and prog == program and pargs == program_args and (note is None or note in err)
    return check(ok, "zippers %s: wanted %s %s%s (exit 0), got exit %d: %s %s %s" % (
        " ".join(args), program, " ".join(program_args), (" and the note '%s'" % note) if note else "", code, prog,
        " ".join(pargs or []), err.strip()[:300]))


def refuse(args, message):
    code, out, err = run(["--dry-run"] + args)
    return check(code == 1 and not out and message in err,
                 "zippers %s: wanted exit 1 and '%s', got exit %d: %s%s" % (" ".join(args), message, code,
                                                                          out.strip()[:200], err.strip()[:300]))


# a plausible value for an option (zippers does not look at most values: the program does)
def values_for(option, n):
    special = {"--only": ["+"], "--format": ["png"], "--phi": ["1,0"], "--centre": ["1,0,0,0"], "-o": ["x.png"],
               "--frames": ["0:3"], "--rho": ["3.2"], "-f": ["file.txt"], "--pairs": ["linked_pairs.txt"],
               "--assemble": ["timeline.txt", "out"], "--pole": ["10", "20"], "--pair": ["4", "10", "1"]}
    return special.get(option, ["7"] * n)[:n]


def read_table():
    code, out, err = run(["--table"])
    rows = [line.split("\t") for line in out.split("\n") if line and not line.startswith("#")]
    check(code == 0 and rows, "zippers --table: exit %d, %d rows" % (code, len(rows)))
    return rows


def section(title):
    print(title)


def drain(fd):
    """read the pseudo-terminal until it closes"""
    n = 0
    while True:
        try:
            b = os.read(fd, 65536)
        except OSError:
            return n
        if not b:
            return n
        n += len(b)


def terminal_check(master, slave):
    """zippers draw fig8 without -o, its standard output a pseudo-terminal: refused, exit 1"""
    drained = []
    reader = threading.Thread(target=lambda: drained.append(drain(master)), daemon=True)
    reader.start()                         # (were the picture written to the terminal, it would not block)
    p = subprocess.Popen([ZIPPERS, "draw", "fig8", "-R", "0", "-a", "0.3"], stdin=subprocess.DEVNULL, stdout=slave,
                         stderr=subprocess.PIPE, universal_newlines=True)
    try:
        err = p.communicate(timeout=120)[1]
    except subprocess.TimeoutExpired:
        p.kill()
        err = p.communicate()[1]
    os.close(slave)
    reader.join(timeout=10)
    os.close(master)
    check(p.returncode == 1 and "which is a terminal here" in err,
          "zippers draw fig8 with a terminal for its standard output: refused (exit %d: %s)" % (p.returncode, err[:200]))


def main():
    if not os.access(ZIPPERS, os.X_OK):
        print("test_zippers_cli: %s is not built" % ZIPPERS)
        return 1
    print("test_zippers_cli: %s" % ZIPPERS)
    rows = read_table()
    options = [r for r in rows if r[0] == "option"]
    left_out = [r for r in rows if r[0] == "left-out"]

    section("every option of the table is passed on to its program")
    for _, program, popt, nvalues, command, sphere, zopt, shown in options:
        n = int(nvalues)
        vals = values_for(zopt, n)
        ex = EXAMPLE[sphere]
        if command == "draw":
            args = ["draw", ex, zopt] + vals
            want = (["-m", ex] if sphere == "S2" else []) + ([] if popt == "-" else [popt] + vals)
        elif command == "search":
            nums = ["4", "9.5", "3"] if sphere == "S3" else ["2.3", "4.5"]
            args = ["search", ex] + nums + [zopt] + vals
            if sphere == "S3":
                want = ["kernel"] + nums + ([] if popt == "-" else [popt] + vals)
            else:
                rho = vals[0] if zopt == "--rho" else "3"
                want = ["kernel"] + nums + [rho] + ([] if popt == "-" else [popt] + vals)
        elif command == "link":
            args = ["link", ex, zopt] + vals
            defaults = [x for opt, val in (("-a", "0.01"), ("--nrel", "20"), ("--hr", "2.6")) if opt != zopt
                        for x in (opt, val)]
            want = defaults + ([] if popt == "-" else [popt] + vals)
        elif command == "film":
            if zopt == "--assemble":
                args, want = ["film", zopt] + vals, [zopt] + vals
            else:
                args, want = ["film", "shot.txt", zopt] + vals, ["shot.txt", zopt] + vals
        else:
            check(False, "an option of an unknown command in the table: %s" % command)
            continue
        expect(args, program, want)

    section("the translations, the defaults and the aliases")
    expect(["draw", "fig8", "-n", "2", "-o", "x.eps"], "zipper", ["-m", "fig8", "-n", "2", "-o", "x.eps"])
    expect(["draw", "RRL", "-n", "3", "--format", "png", "-o", "x.png"], "zipper",
           ["-m", "RRL", "-n", "3", "-f", "png", "-o", "x.png"])
    expect(["draw", "rrl", "-n", "inf"], "zipper", ["-m", "rrl", "-n", "inf"])
    expect(["draw", "RLLRL", "--format", "eps"], "zipper", ["-m", "RLLRL", "-f", "eps"])
    expect(["draw", "cs", "--phi", "1,0", "-R", "2"], "zipper3", ["-R", "2"])
    expect(["draw", "cartwright-steger", "--pairs", "linked_pairs.txt", "--pairindex", "5", "--context", "-o", "p.png"],
           "zipper3", ["--pairs", "linked_pairs.txt", "--pairindex", "5", "--context", "-o", "p.png"])
    expect(["draw", "imm", "--turn", "10", "--turn2", "-5", "-o", "x.png"], "zipper5",
           ["--turn", "10", "--turn2", "-5", "-o", "x.png"])
    expect(["draw", "imm-m5", "--centre", "0,0,0,0,1,1", "--zoom", "100", "-o", "z.png"], "zipper5",
           ["--centre", "0,0,0,0,1,1", "--zoom", "100", "-o", "z.png"])
    expect(["search", "cs"], "chz", ["kernel", "4", "9.5", "3", "--roofs", "--jexcess", "1"],
           note="without numbers, the search of the default file subst_4.00_9.50_t3_r1.txt")
    expect(["search", "imm"], "chz5", ["kernel", "2.3", "4.5", "3", "--roofs"],
           note="without numbers, the search of the default file subst5_2.30_4.50_t1_r0.txt")
    expect(["search", "cs", "--tlist"], "chz", ["kernel", "4", "9.5", "3", "--tlist"],
           note="the numbers of the search of the default file")
    expect(["search", "imm", "--rho", "3.2"], "chz5", ["kernel", "2.3", "4.5", "3.2", "--roofs"])
    expect(["search", "cs", "4", "9"], "chz", ["kernel", "4", "9"])
    expect(["search", "cs", "4", "9.5", "3", "--roofs", "--jexcess", "1"], "chz",
           ["kernel", "4", "9.5", "3", "--roofs", "--jexcess", "1"])
    expect(["search", "imm-m5", "2.3", "4.5"], "chz5", ["kernel", "2.3", "4.5", "3"])
    expect(["search", "imm-m5", "2.3", "4.2", "2", "--rho", "3.2"], "chz5", ["kernel", "2.3", "4.2", "3.2", "2"])
    expect(["search", "cs", "4", "9.5", "3", "--phi", "1,0", "--noinv"], "chz", ["kernel", "4", "9.5", "3", "--noinv"])
    expect(["link", "cs"], "link3", ["-a", "0.01", "--nrel", "20", "--hr", "2.6"])
    expect(["link", "cs", "-a", "0.02", "--hr", "3"], "link3", ["--nrel", "20", "-a", "0.02", "--hr", "3"])
    expect(["link", "cartwright-steger", "--pair", "4", "10", "1", "--phi", "1,0"], "link3",
           ["-a", "0.01", "--nrel", "20", "--hr", "2.6", "--pair", "4", "10", "1"])
    expect(["link", "cs", "-a", "0.01", "--nrel", "20", "--hr", "2.6", "--export", "link_run2.txt"], "link3",
           ["-a", "0.01", "--nrel", "20", "--hr", "2.6", "--export", "link_run2.txt"])
    expect(["film", "rot_n2.txt", "--frames", "0:3", "-o", "rot_n2"], "zmovie", ["rot_n2.txt", "--frames", "0:3", "-o", "rot_n2"])
    expect(["film", "shot.txt", "frames=10", "-j", "2", "--quiet"], "zmovie", ["shot.txt", "frames=10", "-j", "2", "--quiet"])
    expect(["film", "--assemble", "timeline.txt", "out"], "zmovie", ["--assemble", "timeline.txt", "out"])

    section("values that begin with '-' but are no option")
    expect(["draw", "cs", "--yaw", "-30", "--pitch", "-0.5", "--only", "-"], "zipper3",
           ["--yaw", "-30", "--pitch", "-0.5", "--only", "-"])
    expect(["draw", "cs", "--centre", "-.5,0.1,0.2,0.3", "--zoom", "2"], "zipper3",
           ["--centre", "-.5,0.1,0.2,0.3", "--zoom", "2"])
    expect(["draw", "imm", "--turn", "-.5", "--turn2", "-12"], "zipper5", ["--turn", "-.5", "--turn2", "-12"])
    expect(["film", "shot.txt", "-o", "-"], "zmovie", ["shot.txt", "-o", "-"])

    section("check: the list of the checks")
    code, out, err = run(["--dry-run", "check"])
    lines = [shlex.split(x) for x in out.strip().split("\n") if x]
    names = [os.path.basename(x[0]) + " " + " ".join(x[1:]) for x in lines]
    check(code == 0 and len(lines) == 21, "zippers --dry-run check: 21 checks (1 + 17 + 3), got %d (exit %d)" %
          (len(lines), code))
    for want in ["zipper -m fig8 -n 2 -R 5 --stats --memlimit 2 -o stats.eps", "chz selftest", "link3 selftest",
                 "chz kernel 4 9.5 3", "zipper3 -R 4.5 -a 0.3 --stats", "chz5 selftest", "chz5 kernel 2.3 4.5 3",
                 "zipper5 -R 2.1 -a 0.3 --stats", "link3 -a 0.01 --nrel 20 --hr 2.6 --pair 4 10 1",
                 "link3 -a 0.01 --nrel 20 --hr 2.6 --pair 13 12 16"]:
        check(want in names, "zippers --dry-run check runs %s" % want)
    check(sum(1 for x in names if "--pair" in x) == 13, "zippers --dry-run check: the 13 stored pairs")
    code, out, err = run(["--dry-run", "check", "RRL", "imm"])
    lines = [x for x in out.strip().split("\n") if x]
    check(code == 0 and len(lines) == 4 and "-m RRL" in lines[0], "zippers --dry-run check RRL imm: 4 checks, the first "
          "with -m RRL (got exit %d: %s)" % (code, out.strip()[:200]))

    section("the refusals")
    for _, program, popt, command, reason in left_out:
        if command not in ("draw", "film") or not popt.startswith("-") or popt in ("-h", "--help"):
            continue                       # (-h and --help are zippers' own help: below)
        sphere = SPHERE_OF[program]
        args = [command, EXAMPLE[sphere], popt, "1"] if command == "draw" else [command, "shot.txt", popt, "1"]
        refuse(args, reason)
    refuse([], "usage: zippers")
    refuse(["frob"], "unknown command 'frob'")
    refuse(["--frob"], "unknown option --frob")
    refuse(["draw"], "usage: zippers draw")
    refuse(["draw", "bogus"], "unknown example 'bogus'")
    refuse(["draw", "RRRR"], "unknown example 'RRRR'")
    refuse(["draw", "cs", "--bogus"], "unknown option --bogus")
    refuse(["draw", "fig8", "--pole", "1", "2"], "--pole is not an option for fig8 (S2); it is one for cartwright-steger (S3)")
    refuse(["draw", "cs", "--format", "png"], "--format is not an option for cartwright-steger (S3); it is one for fig8 (S2)")
    refuse(["draw", "imm", "--phi", "1,0"], "--phi is not an option for imm-m5 (S4)")
    refuse(["draw", "cs", "-R"], "-R needs 1 value")
    refuse(["draw", "cs", "--pole", "1"], "--pole needs 2 values")
    refuse(["draw", "cs", "-o", "x.eps"], "draws PNG pictures only")
    refuse(["draw", "imm", "-o", "x.EPS"], "draws PNG pictures only")
    refuse(["draw", "fig8", "-o", "x.svg"], "SVG is not available in this version")
    refuse(["draw", "fig8", "--format", "gif"], "the format is eps or png")
    refuse(["draw", "cs", "--phi", "2,1"], "only phi = 1,0")
    refuse(["draw", "cs", "--phi", "1,0,0"], "only phi = 1,0")
    refuse(["draw", "fig8", "stray"], "unexpected argument 'stray'")
    refuse(["search", "fig8"], "an S2 example has no search")
    refuse(["search", "cs", "4"], "give C R [TPHI], or no numbers")
    refuse(["search", "cs", "4", "9.5", "3", "1"], "give C R [TPHI], or no numbers")
    refuse(["search", "cs", "4", "x"], "'x' is not a number")
    refuse(["search", "cs", "4", "9.5", "3.5"], "'3.5' is not an integer")
    refuse(["search", "imm", "2.3", "4.5", "--rho", "x"], "--rho x: not a number")
    refuse(["search", "cs", "--rho", "3"], "--rho is not an option for cartwright-steger (S3)")
    refuse(["link", "imm"], "link: only cartwright-steger")
    refuse(["link", "fig8"], "link: only cartwright-steger")
    refuse(["link", "cs", "--pair", "1", "2"], "--pair needs 3 values")
    refuse(["film"], "usage: zippers film")
    refuse(["film", "--assemble", "a"], "--assemble TIMELINE.txt OUTDIR (two arguments)")
    refuse(["film", "-o", "d"], "the shot file comes first")
    refuse(["film", "shot.txt", "--bogus"], "unknown option --bogus")
    refuse(["film", "shot.txt", "--assemble", "a", "b"], "--assemble comes first")
    refuse(["film", "shot.txt", "-j"], "-j needs a value")
    for args, value in ((["draw", "cs", "-R", "-a"], "-a"), (["draw", "cs", "-o", "-R"], "-R"),
                        (["draw", "cs", "-R", "-a", "0.3"], "-a"), (["draw", "fig8", "-R", "-o", "x.eps"], "-o"),
                        (["draw", "imm", "--zoom", "--cull"], "--cull"), (["draw", "cs", "--pairs", "--context"], "--context"),
                        (["draw", "cs", "-o", "--"], "--"), (["draw", "cs", "--pole", "10", "-x"], "-x"),
                        (["link", "cs", "--pair", "4", "-a", "1"], "-a"), (["search", "cs", "--tcand", "-I"], "-I"),
                        (["film", "rot_n2.txt", "-o", "--quiet"], "--quiet"), (["film", "--assemble", "--x", "out"], "--x"),
                        (["film", "--assemble", "t.txt", "-o"], "-o")):
        refuse(args, "'%s' after it looks like an option" % value)
    refuse(["check", "bogus"], "unknown example 'bogus'")
    refuse(["list", "bogus"], "unknown example 'bogus'")
    refuse(["list", "cs", "imm"], "one example at most")
    for ex in ("fig8", "cs", "imm"):
        refuse(["view", ex], "the window is not in this version")

    section("the quoting of --dry-run, read back by sh -c")
    for value in ["a b.png", "it's.png", 'say "hi".png', "$HOME.png", "`date`.png", "back\\slash.png", "new\nline.png",
                  "", "tab\there.png", "$(echo hi).png", "!*?[x].png"]:
        code, out, err = run(["--dry-run", "draw", "cs", "-o", value])
        line = out[:-1] if out.endswith("\n") else out
        back = subprocess.run(["sh", "-c", "printf '%s\\0' " + line], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE).stdout.split(b"\0")[:-1]
        words = [w.decode("utf-8") for w in back]
        check(code == 0 and len(words) == 3 and os.path.basename(words[0]) == "zipper3" and words[1:] == ["-o", value],
              "zippers --dry-run draw cs -o %r: read back by sh -c as %r (exit %d, printed %r)" % (value, words[1:], code,
                                                                                               line))

    section("the usage texts")
    for args, wants, wants_not in [
            (["--help"], ["usage: zippers", "  draw EXAMPLE", "  check [EXAMPLE ...]", "--dry-run"], []),
            (["-h"], ["usage: zippers"], []),
            (["draw", "--help"], ["usage: zippers draw", "  -n N", "  --pole D1 D2", "  --turn D", " [S3]"], ["--memlimit"]),
            (["draw", "fig8", "--help"], ["  -n N", "  --format FMT", "  -C "], ["--pole", "--turn", "-f FILE", "--memlimit"]),
            (["draw", "cs", "-h"], ["  -f FILE", "  --pole D1 D2", "  --pairs FILE", "  --phi 1,0"], ["  -n N", "--turn"]),
            (["draw", "imm", "--help"], ["  --turn D", "  --hidfog H", "  --cullk K"], ["  --pole", "  --check", "  -n N"]),
            (["search", "--help"], ["usage: zippers search", "--rho RHO", "--roofs"], []),
            (["search", "cs", "--help"], ["--roofs"], ["--rho"]),
            (["link", "cs", "--help"], ["usage: zippers link", "--pair A B H", "--export FILE"], []),
            (["film", "--help"], ["usage: zippers film", "--frames A:B"], []),
            (["check", "--help"], ["usage: zippers check"], []),
            (["list", "--help"], ["usage: zippers list", "ZIPPERS_DATA"], []),
            (["view", "--help"], ["usage: zippers view"], [])]:
        code, out, err = run(args)
        ok = code == 0 and all(w in out for w in wants) and not any(w in out for w in wants_not)
        check(ok, "zippers %s: exit 0, with %s and without %s (got exit %d: %s)" % (
            " ".join(args), wants, wants_not, code, (out + err)[:300]))
    code, out, err = run(["--version"])
    check(code == 0 and re.match(r"^zippers \d+\.\d+", out), "zippers --version: 'zippers N.N' (got %r)" % out)
    # the options each usage text lists (its lines that begin with an option) are those of the table that are shown
    for command, sphere, args in [("draw", "S2", ["draw", "fig8", "--help"]), ("draw", "S3", ["draw", "cs", "--help"]),
                                  ("draw", "S4", ["draw", "imm", "--help"]), ("search", "S3", ["search", "cs", "--help"]),
                                  ("search", "S4", ["search", "imm", "--help"]), ("link", "S3", ["link", "cs", "--help"]),
                                  ("film", "S2", ["film", "--help"])]:
        code, out, err = run(args)
        listed = set(re.findall(r"^  (--?[A-Za-z][\w-]*)", out, re.M))
        shown = {r[6] for r in options if r[4] == command and r[5] == sphere and r[7] == "shown"}
        check(code == 0 and listed == shown, "zippers %s: lists exactly the shown options of the table (only listed: "
              "%s; only in the table: %s)" % (" ".join(args), sorted(listed - shown), sorted(shown - listed)))

    section("an S2 picture without -o on a terminal")
    try:
        master, slave = pty.openpty()
    except OSError as e:                   # (a machine without pseudo-terminals: the check cannot be made there)
        print("  note: no pseudo-terminal here (%s): this check is left out" % e)
        master = slave = None
    if master is not None:
        terminal_check(master, slave)

    section("the programs' own messages")
    tmp = tempfile.mkdtemp(prefix="zippers_cli_test_")
    try:
        def prog(name, args, cwd=tmp):
            sub = {"zipper": "s2", "ztest": "s2", "zipper5": "s4", "link3": "s3"}[name]
            q = subprocess.run([os.path.join(BUILD, "apps", sub, name)] + args, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, cwd=cwd,
                               timeout=300)
            return q.returncode, q.stdout, q.stderr
        code, out, err = prog("zipper5", ["-h"])
        check(code == 0 and err.startswith("usage: zipper5") and "--hidfog H" in err and "--cullk K" in err,
              "zipper5 -h: its usage text, exit 0 (got exit %d: %s)" % (code, err[:200]))
        code, out, err = prog("zipper5", ["--bogus"])
        check(code == 1 and "unknown option --bogus" in err and "usage: zipper5" in err and "zipper5.cc" not in err,
              "zipper5 --bogus: the option named and the usage text, exit 1 (got exit %d: %s)" % (code, err[:200]))
        code, out, err = prog("zipper", ["-v", "-R", "0"])
        check(code == 1 and "this build of zipper has no interactive viewer" in err and "rebuild" not in err,
              "zipper -v: this build has no window, exit 1 (got exit %d: %s)" % (code, err[:200]))
        code, out, err = prog("ztest", ["bogus"])
        check(code == 1 and "usage: ztest [same|depth|frame|all]" in err,
              "ztest bogus: refused, exit 1 (got exit %d: %s)" % (code, err[:200]))
        code, out, err = prog("link3", ["-a", "0.01", "--nrel", "20", "--hr", "2.6", "--pair", "4", "10", "1",
                                        "--png", "nofolder/pair"])
        cannot = err.count("cannot write nofolder/pair_yaw")
        check(code == 0 and cannot == 3 and "wrote nofolder" not in err,
              "link3 --png into a folder that does not exist: three 'cannot write' and no 'wrote' (got exit %d, %d: %s)"
              % (code, cannot, err[-300:]))
        os.mkdir(os.path.join(tmp, "folder"))
        code, out, err = prog("link3", ["-a", "0.01", "--nrel", "20", "--hr", "2.6", "--pair", "4", "10", "1",
                                        "--png", "folder/pair"])
        check(code == 0 and err.count("wrote folder/pair_yaw") == 3 and
              all(os.path.getsize(os.path.join(tmp, "folder", "pair_yaw%03d.png" % y)) > 0 for y in (0, 60, 120)),
              "link3 --png into a folder that exists: three 'wrote', three pictures (got exit %d: %s)" % (code, err[-300:]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("test_zippers_cli: %d checks, %d fail" % (n_checks, n_bad))
    return 1 if n_bad else 0


if __name__ == "__main__":
    sys.exit(main())
