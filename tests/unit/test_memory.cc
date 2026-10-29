// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_memory: resident_bytes and release_memory of src/core/memory.h against their legacy originals, the functions of
// the same names in legacy/s2/zipper.cc, which this test includes (with its main renamed). The two read the
// system, so they cannot be compared on inputs; instead:
//   1. the text: each moved function is the legacy one, token for token (comments, white space and the keyword in
//      front, static or inline, aside), read from the two files; a changed copy is told apart (a check of the check);
//   2. resident_bytes: positive; called alternately with the legacy one, the two agree (to 1 MB; they are expected to
//      be equal); both see 128 MiB of fresh pages come (mmap, every page touched; at least 0.9 of it, at most 1.5 times,
//      as a sanitizer build adds its shadow) and go (munmap, to 10%); both see 128 MiB of small heap blocks come (at
//      least 0.9 of it, at most twice); the legacy and the new reading agree at every step;
//   3. release_memory: runs; after the same allocation and free (64 MiB in blocks of 4 kB), the legacy and the new one
//      give back the same, to 8 MB plus 10% of the 64 MiB, in alternating rounds; neither raises the memory.
//      (On macOS 26 (arm64), malloc_zone_pressure_relief gives back nothing measurable after these frees: the
//      resident size, which resident_bytes takes when it is the larger, stays until the system reclaims the pages.
//      The test then sees two releases of 0 MB, and it prints them.)
// Build it with the flags of the unit tests (-O2); ZIPPERS_REPO names the repository, for part 1.

// (the warnings the film engine silences for the program; the GCC form, which clang reads too)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#define main legacy_zipper_main
#include "zipper.cc"                       // legacy/s2/zipper.cc: ::resident_bytes, ::release_memory
#undef main
#pragma GCC diagnostic pop

#include "core/memory.h"                   // zippers::resident_bytes, zippers::release_memory
#include "render_test_support.h"

#include <sys/mman.h>

#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "ZIPPERS_REPO must name the repository (tests/unit/CMakeLists.txt defines it)"
#endif

using rtest::check;
using rtest::note;
using rtest::section;

static const double MB = 1e6, MiB = 1048576.0;

// ---- 1. the text ----------------------------------------------------------------------------------------------------

static bool read_file(const std::string& path, std::string& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

// C++ tokens, without comments and white space: identifiers and numbers, string and character literals, and every
// other character alone (so "==" is two tokens on both sides alike)
static std::vector<std::string> tokens(const std::string& s)
{
    std::vector<std::string> t;
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (isspace((unsigned char)c)) { ++i; continue; }
        if (c == '/' && i + 1 < n && s[i+1] == '/') { while (i < n && s[i] != '\n') ++i; continue; }
        if (c == '/' && i + 1 < n && s[i+1] == '*') { size_t e = s.find("*/", i + 2); i = e == std::string::npos ? n : e + 2; continue; }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < n && s[j] != c) j += s[j] == '\\' ? 2 : 1;
            t.push_back(s.substr(i, j + 1 - i)); i = j + 1; continue;
        }
        if (isalnum((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '_')) ++j;
            t.push_back(s.substr(i, j - i)); i = j; continue;
        }
        t.push_back(std::string(1, c)); ++i;
    }
    return t;
}

// the tokens of the function `type name()` of the file: from its type to the brace that closes its body (empty if
// it is not there exactly once)
static std::vector<std::string> function_tokens(const std::vector<std::string>& t, const char* type, const char* name)
{
    std::vector<std::string> out;
    int found = 0;
    for (size_t i = 0; i + 4 < t.size(); ++i) {
        if (t[i] != type || t[i+1] != name || t[i+2] != "(" || t[i+3] != ")" || t[i+4] != "{") continue;
        ++found;
        std::vector<std::string> f;
        int depth = 0;
        for (size_t j = i; j < t.size(); ++j) {
            f.push_back(t[j]);
            if (t[j] == "{") ++depth;
            else if (t[j] == "}" && --depth == 0) break;
        }
        out = f;
    }
    return found == 1 ? out : std::vector<std::string>();
}

static void test_text()
{
    section("1. the moved functions are the legacy text, token for token");
    std::string legacy, moved;
    bool ok = read_file(ZIPPERS_REPO "/legacy/s2/zipper.cc", legacy) && read_file(ZIPPERS_REPO "/src/core/memory.h", moved);
    check(ok, "cannot read legacy/s2/zipper.cc or src/core/memory.h under %s", ZIPPERS_REPO);
    if (!ok) return;
    std::vector<std::string> tl = tokens(legacy), tm = tokens(moved);
    struct F { const char* type; const char* name; size_t min_tokens; } fs[] = {{"double", "resident_bytes", 100}, {"void", "release_memory", 15}};
    for (const F& f : fs) {
        std::vector<std::string> a = function_tokens(tl, f.type, f.name), b = function_tokens(tm, f.type, f.name);
        check(a.size() >= f.min_tokens && b.size() >= f.min_tokens, "%s: found %zu legacy and %zu new tokens (at least %zu expected)",
              f.name, a.size(), b.size(), f.min_tokens);
        check(a == b, "%s: the new text is not the legacy text (%zu and %zu tokens)", f.name, a.size(), b.size());
        // the keyword in front: static in the legacy program, inline in the header
        auto before = [&](const std::vector<std::string>& t, const char* kw) {
            for (size_t i = 1; i + 1 < t.size(); ++i) if (t[i] == f.type && t[i+1] == f.name && t[i-1] == kw) return true;
            return false;
        };
        check(before(tl, "static") && before(tm, "inline"), "%s: not static in the legacy file and inline in the header", f.name);
        note("%s: %zu tokens, the same in both", f.name, b.size());
    }
    // a check of the check: one token changed, one added, one removed are told apart
    std::vector<std::string> a = function_tokens(tm, "double", "resident_bytes");
    if (a.size() > 40) {
        std::vector<std::string> changed = a, added = a, removed = a;
        for (auto& x : changed) if (x == "phys_footprint") { x = "resident_size"; break; }
        added.insert(added.begin() + 20, ";");
        removed.erase(removed.begin() + 20);
        check(changed != a && added != a && removed != a, "the token comparison does not tell a changed text apart");
    }
}

// ---- 2. resident_bytes ----------------------------------------------------------------------------------------------

// make the compiler keep allocations whose contents are never read
static void* volatile g_sink;
static void keep(void* p) { g_sink = p; }

static void test_resident()
{
    section("2. resident_bytes: equal to the legacy one; both follow the memory");
    double a = ::resident_bytes(), b = zippers::resident_bytes();
    check(a > 0 && b > 0 && a < 1e13 && b < 1e13, "legacy %.0f, new %.0f bytes: not plausible", a, b);
    note("now: legacy %.1f MB, new %.1f MB", a/MB, b/MB);

    // alternately: legacy, new, legacy, ...
    int equal = 0, rounds = 2000; double worst = 0;
    for (int i = 0; i < rounds; ++i) {
        double l0 = ::resident_bytes(), m = zippers::resident_bytes(), l1 = ::resident_bytes();
        double d = std::max(std::abs(m - l0), std::abs(m - l1));
        worst = std::max(worst, d);
        equal += (m == l0 && m == l1);
        check(d <= 1*MB, "round %d: legacy %.0f, new %.0f, legacy %.0f bytes", i, l0, m, l1);
    }
    note("%d alternating rounds (legacy, new, legacy): %d with all three equal, largest difference %.0f bytes", rounds, equal, worst);

    // 128 MiB of fresh pages, each touched, then returned
    const size_t len = (size_t)128 << 20;
    double l0 = ::resident_bytes(), m0 = zippers::resident_bytes();
    void* p = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    check(p != MAP_FAILED, "mmap of 128 MiB failed");
    if (p == MAP_FAILED) return;
    long page = sysconf(_SC_PAGESIZE);
    for (size_t i = 0; i < len; i += (size_t)page) static_cast<volatile char*>(p)[i] = 1;
    double l1 = ::resident_bytes(), m1 = zippers::resident_bytes();
    munmap(p, len);
    double l2 = ::resident_bytes(), m2 = zippers::resident_bytes();
    // (at least 0.9 of it; at most 1.5 times, since a sanitizer build adds its shadow memory: +1/8 with AddressSanitizer)
    auto rose = [](double x, double target, double hi) { return x >= 0.9*target && x <= hi*target; };
    check(rose(l1 - l0, len, 1.5) && rose(m1 - m0, len, 1.5), "mmap of 128 MiB, touched: legacy +%.1f MB, new +%.1f MB",
          (l1 - l0)/MB, (m1 - m0)/MB);
    check(std::abs(l1 - m1) <= 1*MB, "with the mapping: legacy %.0f, new %.0f bytes", l1, m1);
    check(std::abs(l2 - l0) <= 0.10*len && std::abs(m2 - m0) <= 0.10*len, "after munmap: legacy %+.1f MB, new %+.1f MB from before",
          (l2 - l0)/MB, (m2 - m0)/MB);
    note("128 MiB mapped and touched: legacy +%.1f MB, new +%.1f MB; unmapped: legacy %+.1f MB, new %+.1f MB from before",
         (l1 - l0)/MB, (m1 - m0)/MB, (l2 - l0)/MB, (m2 - m0)/MB);

    // 128 MiB of heap blocks of 4 kB (each touched): both see them come
    const size_t blk = 4096, k = len/blk;
    std::vector<char*> v(k);
    l0 = ::resident_bytes(); m0 = zippers::resident_bytes();
    for (size_t i = 0; i < k; ++i) { v[i] = static_cast<char*>(malloc(blk)); keep(v[i]); memset(v[i], 1, blk); }
    l1 = ::resident_bytes(); m1 = zippers::resident_bytes();
    // (at most twice: the allocator's headers, and a sanitizer's red zones and shadow, come on top)
    check(rose(l1 - l0, len, 2.0) && rose(m1 - m0, len, 2.0), "128 MiB of 4 kB blocks: legacy +%.1f MB, new +%.1f MB",
          (l1 - l0)/MB, (m1 - m0)/MB);
    check(std::abs(l1 - m1) <= 1*MB, "with the blocks: legacy %.0f, new %.0f bytes", l1, m1);
    for (char* q : v) free(q);
    l2 = ::resident_bytes(); m2 = zippers::resident_bytes();
    check(std::abs(l2 - m2) <= 1*MB, "after the frees: legacy %.0f, new %.0f bytes", l2, m2);
    note("128 MiB of 4 kB blocks: legacy +%.1f MB, new +%.1f MB; freed: legacy %+.1f MB, new %+.1f MB from before the blocks",
         (l1 - l0)/MB, (m1 - m0)/MB, (l2 - l0)/MB, (m2 - m0)/MB);
}

// ---- 3. release_memory ----------------------------------------------------------------------------------------------

// 64 MiB of 4 kB blocks, touched and freed; then `release`. What resident_bytes says before and after the release.
static void free_then_release(void (*release)(), double (*measure)(), double& before, double& after)
{
    const size_t len = (size_t)64 << 20, blk = 4096, k = len/blk;
    std::vector<char*> v(k);
    for (size_t i = 0; i < k; ++i) { v[i] = static_cast<char*>(malloc(blk)); keep(v[i]); memset(v[i], 2, blk); }
    for (char* q : v) free(q);
    before = measure();
    release();
    after = measure();
}

static void legacy_release() { ::release_memory(); }
static void new_release() { zippers::release_memory(); }
static double legacy_measure() { return ::resident_bytes(); }
static double new_measure() { return zippers::resident_bytes(); }

static void test_release()
{
    section("3. release_memory: gives back what the legacy one gives back");
    const double len = 64*MiB, tol = 8*MB + 0.10*len;
    std::string drops;
    for (int round = 0; round < 3; ++round) {
        double lb, la, mb, ma;
        free_then_release(legacy_release, legacy_measure, lb, la);
        free_then_release(new_release, new_measure, mb, ma);
        double dl = lb - la, dm = mb - ma;
        check(la <= lb + 1*MB && ma <= mb + 1*MB, "round %d: the release raised the memory: legacy %+.1f MB, new %+.1f MB",
              round, (la - lb)/MB, (ma - mb)/MB);
        check(std::abs(dl - dm) <= tol, "round %d: the legacy release gave back %.1f MB, the new one %.1f MB", round, dl/MB, dm/MB);
        char buf[160]; snprintf(buf, sizeof buf, "%s%.1f / %.1f", round ? ", " : "", dl/MB, dm/MB);
        drops += buf;
    }
    note("given back after 64 MiB of 4 kB blocks were freed (legacy / new, MB, three rounds): %s", drops.c_str());
}

int main()
{
    printf("test_memory: src/core/memory.h against legacy/s2/zipper.cc\n");
    double t0 = rtest::clock_seconds();
    test_text();
    test_resident();
    test_release();
    rtest::end_section();
    printf("test_memory: %ld checks, %ld failed (%.2f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t0, rtest::peak_mb());
    return rtest::all_bad == 0 && rtest::all_n > 0 ? 0 : 1;
}
