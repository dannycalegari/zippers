#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_install: cmake --install into a folder of the build, and the installed zippers run from an empty folder.

  1. install the build into BUILD/install_test/ (the folder is made afresh; nothing is installed anywhere else), and
     compare the installed files with the list that they must be: bin/zippers, the seven programs in
     libexec/zippers/, the files of data/ in share/zippers/, and README.md, COPYING, CITATION.cff in share/doc/zippers/;
  2. from an empty folder outside the build: zippers --version; zippers list cartwright-steger and imm-m5 find every
     default file in the installed data; zippers draw of each geometry writes a picture that is byte for byte the one
     the build's own program draws (in a folder that holds the data file it reads: the same machine, so this holds on
     every platform); zippers check passes;
  3. move the installed tree, and do 2 again from the moved tree (list, and a picture);
  4. install with DESTDIR and an absolute prefix (as packagers do): the files land under DESTDIR/prefix.
The moved tree is removed at the end when everything passed.

usage: test_install.py CMAKE BUILD REPO BINDIR LIBEXECDIR DATADIR DOCDIR   (the install folders as CMake names them,
       CMAKE_INSTALL_BINDIR and so on; Python 3.6 or later, standard library only)
"""
import os
import shutil
import subprocess
import sys
import tempfile

CMAKE, BUILD, REPO, BINDIR, LIBEXECDIR, DATADIR, DOCDIR = sys.argv[1:8]
BUILD = os.path.abspath(BUILD)
REPO = os.path.abspath(REPO)
PREFIX = os.path.join(BUILD, "install_test")
MOVED = os.path.join(BUILD, "install_test_moved")
DESTDIR = os.path.join(BUILD, "install_test_destdir")
PROGRAMS = ["zipper", "zmovie", "chz", "zipper3", "link3", "chz5", "zipper5"]

n_checks = 0
n_bad = 0


def check(ok, what):
    global n_checks, n_bad
    n_checks += 1
    print("  %s: %s" % ("ok" if ok else "FAIL", what))
    if not ok:
        n_bad += 1
    return ok


def run(cmd, cwd, env=None, timeout=600):
    p = subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True, cwd=cwd, env=env, timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def files_under(root):
    out = set()
    for d, dirs, files in os.walk(root):
        for f in files:
            out.add(os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/"))
    return out


def expected():
    want = {BINDIR + "/zippers"} | {LIBEXECDIR + "/zippers/" + p for p in PROGRAMS}
    want |= {DATADIR + "/zippers/README.md"}
    for g in ("s3", "s4"):
        for f in os.listdir(os.path.join(REPO, "data", g)):
            want.add("%s/zippers/%s/%s" % (DATADIR, g, f))
    want |= {DOCDIR + "/" + f for f in ("README.md", "COPYING", "CITATION.cff")}
    return want


def same_file(a, b):
    with open(a, "rb") as x, open(b, "rb") as y:
        return x.read() == y.read()


def env_without_data():
    env = dict(os.environ)
    env.pop("ZIPPERS_DATA", None)
    return env


def use(prefix, work, pictures):
    """run the installed zippers of `prefix` in the empty folder `work`; `pictures`: which pictures to draw"""
    z = os.path.join(prefix, BINDIR, "zippers")
    env = env_without_data()
    data = os.path.realpath(os.path.join(prefix, DATADIR, "zippers"))
    code, out, err = run([z, "--version"], work, env)
    check(code == 0 and out.startswith("zippers "), "%s --version: %s" % (z, (out + err).strip()))
    for ex, files in (("cartwright-steger", ["subst_4.00_9.50_t3_r1.txt", "subst_4.00_9.50_t3.txt", "linked_pairs.txt"]),
                      ("imm-m5", ["subst5_2.30_4.50_t1_r0.txt"])):
        code, out, err = run([z, "list", ex], work, env)
        g = "s3" if ex == "cartwright-steger" else "s4"
        for f in files:
            line = [x for x in out.split("\n") if (" %s " % f) in x + " "]
            ok = code == 0 and line and all(("found in %s/%s" % (data, g)) in x for x in line)
            check(ok, "zippers list %s: %s found in the installed data %s/%s (%s)" % (ex, f, data, g,
                                                                                    line or (out + err)[:300]))
    for name, args, program, pargs, datafile in pictures:
        code, out, err = run([z, "draw"] + args + ["-o", name], work, env)
        got = os.path.join(work, name)
        ok = code == 0 and os.path.isfile(got)
        check(ok, "zippers draw %s -o %s: exit %d%s" % (" ".join(args), name, code, "" if ok else ": " + err[-300:]))
        if not ok:
            continue
        ref = tempfile.mkdtemp(prefix="zippers_install_ref_")
        try:
            if datafile:
                shutil.copyfile(os.path.join(REPO, "data", datafile), os.path.join(ref, os.path.basename(datafile)))
            code, out, err = run([os.path.join(BUILD, program)] + pargs + ["-o", name], ref, env)
            check(code == 0 and same_file(got, os.path.join(ref, name)),
                  "the installed zippers draws %s byte for byte as the build's %s (exit %d)" % (name, program, code))
        finally:
            shutil.rmtree(ref, ignore_errors=True)


def main():
    print("test_install: %s --install %s --prefix %s" % (CMAKE, BUILD, PREFIX))
    for d in (PREFIX, MOVED, DESTDIR):
        if os.path.lexists(d):
            shutil.rmtree(d)
    env = env_without_data()
    env.pop("DESTDIR", None)
    code, out, err = run([CMAKE, "--install", BUILD, "--prefix", PREFIX], BUILD, env)
    if not check(code == 0, "cmake --install (exit %d%s)" % (code, "" if code == 0 else ": " + (out + err)[-500:])):
        return 1
    got, want = files_under(PREFIX), expected()
    check(got == want, "the installed files are the expected %d (only installed: %s; missing: %s)" % (
        len(want), sorted(got - want), sorted(want - got)))
    for p in PROGRAMS + ["zippers"]:
        path = os.path.join(PREFIX, BINDIR if p == "zippers" else LIBEXECDIR + "/zippers", p)
        check(os.access(path, os.X_OK), "%s is installed, and can be run" % os.path.relpath(path, PREFIX))
    for f in ("README.md", "COPYING", "CITATION.cff"):
        check(same_file(os.path.join(PREFIX, DOCDIR, f), os.path.join(REPO, f)), "%s/%s is %s" % (DOCDIR, f, f))
    for g in ("s3", "s4"):
        for f in os.listdir(os.path.join(REPO, "data", g)):
            if not same_file(os.path.join(PREFIX, DATADIR, "zippers", g, f), os.path.join(REPO, "data", g, f)):
                check(False, "%s/zippers/%s/%s is data/%s/%s" % (DATADIR, g, f, g, f))

    pictures = [("s2.png", ["fig8", "-R", "2"], "apps/s2/zipper", ["-m", "fig8", "-R", "2"], None),
                ("s3.png", ["cartwright-steger", "-R", "0", "-a", "0.3"], "apps/s3/zipper3", ["-R", "0", "-a", "0.3"],
                 "s3/subst_4.00_9.50_t3_r1.txt"),
                ("s4.png", ["imm-m5", "-R", "0", "-a", "0.3"], "apps/s4/zipper5", ["-R", "0", "-a", "0.3"],
                 "s4/subst5_2.30_4.50_t1_r0.txt")]
    work = tempfile.mkdtemp(prefix="zippers_install_work_")
    try:
        print("the installed tree, from an empty folder (%s)" % work)
        use(PREFIX, work, pictures)
        code, out, err = run([os.path.join(PREFIX, BINDIR, "zippers"), "check"], work, env_without_data(), timeout=900)
        last = out.strip().split("\n")[-1] if out.strip() else ""
        check(code == 0 and "all 21 checks pass" in last, "the installed zippers check: %s%s" % (
            last, "" if code == 0 else "\n" + out[-1500:] + err[-500:]))
        print("the installed tree, moved to %s" % MOVED)
        os.rename(PREFIX, MOVED)
        work2 = tempfile.mkdtemp(prefix="zippers_install_work_")
        try:
            use(MOVED, work2, pictures[2:])
        finally:
            shutil.rmtree(work2, ignore_errors=True)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    print("an install with DESTDIR and an absolute prefix")
    env = env_without_data()
    env["DESTDIR"] = DESTDIR
    code, out, err = run([CMAKE, "--install", BUILD, "--prefix", "/opt/zippers"], BUILD, env)
    got = files_under(os.path.join(DESTDIR, "opt", "zippers")) if code == 0 else set()
    check(code == 0 and got == expected() and set(os.listdir(DESTDIR)) == {"opt"},
          "DESTDIR=%s cmake --install --prefix /opt/zippers puts the same files under DESTDIR/opt/zippers, and nothing "
          "else (exit %d)" % (DESTDIR, code))
    if not n_bad:
        for d in (MOVED, DESTDIR):
            shutil.rmtree(d, ignore_errors=True)
    print("test_install: %d checks, %d fail" % (n_checks, n_bad))
    return 1 if n_bad else 0


if __name__ == "__main__":
    sys.exit(main())
