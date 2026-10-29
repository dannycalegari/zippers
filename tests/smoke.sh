#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# smoke.sh: a quick test of the tree of the repository (its layout, the licence, the citation file, no recordings).
#
# It checks that
#   - the files and folders of the skeleton are all there;
#   - COPYING is the text of the GNU GPL, version 3;
#   - CITATION.cff names the licence and the Zippers paper;
#   - git tracks no recordings (the rule is: no music and no recordings in this repository).
#
# usage:        smoke.sh [SOURCE_DIR]        (default: the folder above this script)
# exit status:  0 if all is in place; 1 if not (every problem is printed)

root=${1:-$(cd "$(dirname "$0")/.." && pwd)}
checked=0
problems=0

problem() {
  echo "smoke: PROBLEM: $1"
  problems=$((problems + 1))
}

# the files of the skeleton (COPYING was there before it)
for f in \
    COPYING CMakeLists.txt Makefile README.md CITATION.cff .gitignore .github/workflows/ci.yml \
    legacy/README.md film/README.md src/README.md apps/README.md data/README.md docs/README.md \
    tests/README.md tests/CMakeLists.txt tests/smoke.sh
do
  checked=$((checked + 1))
  [ -f "$root/$f" ] || problem "missing file: $f"
done

# the folders
for d in cmake
do
  checked=$((checked + 1))
  [ -d "$root/$d" ] || problem "missing folder: $d"
done

# the licence
checked=$((checked + 1))
if [ -f "$root/COPYING" ]; then
  if ! grep -q 'GNU GENERAL PUBLIC LICENSE' "$root/COPYING" || ! grep -q 'Version 3, 29 June 2007' "$root/COPYING"; then
    problem "COPYING is not the text of the GNU GPL, version 3"
  fi
fi

# the citation file
checked=$((checked + 1))
if [ -f "$root/CITATION.cff" ]; then
  grep -q 'GPL-3.0-or-later' "$root/CITATION.cff" || problem "CITATION.cff does not name the licence GPL-3.0-or-later"
  grep -q '2411.15610' "$root/CITATION.cff" || problem "CITATION.cff does not cite arXiv:2411.15610"
fi

# no recordings among the files that git tracks (a plain copy of the tree without .git is not checked)
checked=$((checked + 1))
if git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  recordings=$(git -C "$root" ls-files | grep -i -E '\.(mp4|wav|m4a|aac|mov|m4v|mp3|aif|aiff|flac|ogg)$')
  [ -z "$recordings" ] || problem "git tracks recordings: $(echo "$recordings" | tr '\n' ' ')"
else
  echo "smoke: note: $root is not a git checkout, so the check for recordings was skipped"
fi

if [ "$problems" -eq 0 ]; then
  echo "smoke: OK ($checked checks, in $root)"
  exit 0
fi
echo "smoke: $problems problem(s) in $root"
exit 1
