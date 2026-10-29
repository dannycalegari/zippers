# SPDX-License-Identifier: GPL-3.0-or-later
#
# A thin wrapper around CMake, so that `make` still works.
#
#   make            configure into build/ if needed, then build
#   make test       build, then run CTest (the fast tests: ctest -L fast)
#   make clean      remove build/
#   make sanitize   configure and build into build-sanitize/ with the address and undefined-behaviour sanitizers
#
# Options for CMake go in CMAKE_ARGS, for example   make CMAKE_ARGS=-DZIPPERS_VIEWERS=OFF
# They take effect when a build folder is first configured; to change them later, `make clean` first.
#
# Compiling runs in parallel: as many jobs as the machine has cores, but at most 4 (four compiles take about 1.7 GB).
# Choose another number with   make JOBS=2

CMAKE      ?= cmake
CMAKE_ARGS ?=

ifeq ($(origin JOBS),undefined)
  JOBS := $(shell n=$$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 2); [ "$$n" -gt 4 ] && n=4; echo $$n)
endif

.PHONY: all test clean sanitize

all: build/CMakeCache.txt
	$(CMAKE) --build build --parallel $(JOBS)

# configure, but only if build/ has not been configured yet
build/CMakeCache.txt:
	@command -v $(CMAKE) >/dev/null 2>&1 || { echo "$(CMAKE) not found: install CMake 3.20 or later"; exit 1; }
	$(CMAKE) -S . -B build $(CMAKE_ARGS)

test: all
	ctest --test-dir build -L fast --output-on-failure

clean:
	rm -rf build

sanitize:
	@command -v $(CMAKE) >/dev/null 2>&1 || { echo "$(CMAKE) not found: install CMake 3.20 or later"; exit 1; }
	$(CMAKE) -S . -B build-sanitize -DZIPPERS_SANITIZE=ON $(CMAKE_ARGS)
	$(CMAKE) --build build-sanitize --parallel $(JOBS)
