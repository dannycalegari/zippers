#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""test_sha256: the SHA-256 of src/core/sha256.h (through sha256_tool) against Python's hashlib.

  - the test vectors of FIPS 180-4 (and of its examples): "abc", the empty message, the 448-bit and the 896-bit
    messages, and a million 'a', each against its published digest and against hashlib;
  - every length from 0 to 300 bytes (the padding's boundaries at 55, 56, 63, 64 and 119, 120 bytes among them), of
    random bytes from a fixed seed;
  - every file of data/ and tests/sessions/.

usage: test_sha256.py SHA256_TOOL REPO        (Python 3.6 or later, standard library only)
"""
import hashlib
import os
import random
import subprocess
import sys
import tempfile

TOOL, REPO = sys.argv[1], sys.argv[2]
n_checks = 0
n_fail = 0


def check(ok, what):
    global n_checks, n_fail
    n_checks += 1
    if not ok:
        n_fail += 1
        print("  FAIL: " + what)


def tool(paths):
    """the tool's hashes of the files, in order"""
    p = subprocess.run([TOOL] + paths, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True)
    check(p.returncode == 0, "sha256_tool exit %d: %s" % (p.returncode, p.stderr[:200]))
    return [line.split("  ", 1)[0] for line in p.stdout.splitlines()]


def main():
    print("test_sha256: src/core/sha256.h against hashlib")
    vectors = [
        (b"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        (b"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
        (b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
        (b"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrst"
         b"nopqrstu", "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"),
        (b"a" * 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"),
    ]
    rng = random.Random(7)
    with tempfile.TemporaryDirectory(prefix="zippers_sha256_") as d:
        paths = []
        for k, (msg, _) in enumerate(vectors):
            paths.append(os.path.join(d, "v%d" % k))
            with open(paths[-1], "wb") as f:
                f.write(msg)
        got = tool(paths)
        for k, (msg, want) in enumerate(vectors):
            check(len(got) > k and got[k] == want == hashlib.sha256(msg).hexdigest(),
                  "the vector %d (%d bytes): %s, published %s" % (k, len(msg), got[k] if len(got) > k else "-", want))
        print("  the test vectors of FIPS 180-4: %d" % len(vectors))
        msgs, paths = [], []
        for n in range(301):
            msgs.append(bytes(rng.randrange(256) for _ in range(n)))
            paths.append(os.path.join(d, "r%d" % n))
            with open(paths[-1], "wb") as f:
                f.write(msgs[-1])
        got = tool(paths)
        bad = [n for n in range(301) if len(got) <= n or got[n] != hashlib.sha256(msgs[n]).hexdigest()]
        check(not bad, "every length from 0 to 300 bytes (wrong at %s)" % bad[:10])
        print("  every length from 0 to 300 bytes: 301 messages")
    files = []
    for top in ("data", os.path.join("tests", "sessions")):
        for root, _, names in os.walk(os.path.join(REPO, top)):
            files += [os.path.join(root, n) for n in sorted(names)]
    got = tool(files)
    bad = []
    for k, path in enumerate(files):
        with open(path, "rb") as f:
            if len(got) <= k or got[k] != hashlib.sha256(f.read()).hexdigest():
                bad.append(os.path.relpath(path, REPO))
    check(not bad and files, "the files of data/ and tests/sessions/ (wrong: %s)" % bad[:10])
    print("  the files of data/ and tests/sessions/: %d" % len(files))
    print("test_sha256: %d checks, %d fail" % (n_checks, n_fail))
    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
