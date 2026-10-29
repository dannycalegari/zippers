// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_overlap_programs: the --overlap option of zipper3 and zipper5 takes effect.
//
// It catches a program in which the option has no effect (for instance, default_marking.overlap set after build): no
// other test runs the programs with the option (test_overlap sets the marking on the renderers itself, and no reference
// run uses the option).
//
// For each program, on a light case (S^3 _t3 and S^4 _t1, R = 0, ACC 0.04: a fraction of a second), the program is run
// with no option and with --overlap 0, 1 and 2, and what it prints is compared word for word with what the library
// computes in this process with the same marking (default_marking.overlap = L, then prepare and build, as the program
// does): the lines of the build (branches, leaves, polylines, ...) and the lines of the keys marked. The counts are not
// pinned: the decisions of the renderer depend on floating point, so the library's own counts, here, are the reference,
// and the comparison holds on any machine. Marking does take effect in the library (checked on its own counts: no key
// marked at L = 0, keys marked and fewer leaves at L = 1 and 2), so a program that ignores the option prints the lines
// of L = 0, and fails. The range of the option: 17, x and the empty word are refused, with status 1, before anything is
// built.
// A few seconds. Uses src/ and the programs themselves (ZIPPERS_ZIPPER3, ZIPPERS_ZIPPER5: the paths of the binaries;
// ZIPPERS_REPO is the repository), as test_roots_cs does for zipper3 and link3.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "geometry/pu21/pu21.h"
#include "render/world.h"

#include "render_test_support.h"

#include <cstdio>
#include <string>
#include <vector>

namespace z = zippers;
using rtest::check;
using rtest::note;
using rtest::section;

// run a program; its exit status, and what it wrote to stderr (as test_roots_cs.cc does)
static int run_program(const std::string& cmd, std::string& err)
{
    err.clear();
    FILE* p = popen((cmd + " 2>&1 >/dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) err.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// the lines of the build ("Z+: R=0.00 acc=0.04: ...") and of the marking ("Z+: --overlap 1: N sub-branch keys marked as
// drawn") among the lines of stderr, in order
static std::vector<std::string> marking_lines(const std::string& err)
{
    std::vector<std::string> out;
    for (const std::string& line : rtest::split_lines(err))
        if (line.find(": R=") != std::string::npos || line.find(": --overlap ") != std::string::npos) out.push_back(line);
    return out;
}

// what the library prints for the same build with marking L, in the order of the programs (the build lines of both
// zippers, then the marking lines), and the leaves and the keys marked of each zipper
struct Expect {
    std::vector<std::string> lines;
    size_t leaves[2] = {0, 0}, marked[2] = {0, 0};
};

template <class P>
Expect library_lines(const std::string& file, double R, double acc, size_t cap, int L)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file.c_str(), W); });
    z::default_marking.overlap = L;                     // as the programs do, before build
    std::string err = rtest::capture_stderr([&] { z::build<P>(W, R, acc, cap, 3); });
    z::default_marking = z::Marking();
    Expect e;
    e.lines = marking_lines(err);
    int k = 0;
    for (z::Renderer<B>* r : {W.rp, W.rn}) {
        e.leaves[k] = r->nLeaves;
        e.marked[k] = r->nMarked;
        k++;
        if (L > 0 && r->nVisited) {
            char buf[256];
            snprintf(buf, sizeof buf, "%s: --overlap %d: %zu sub-branch keys marked as drawn", r->sd.name, L, r->nMarked);
            e.lines.push_back(buf);
        }
    }
    delete W.rp; delete W.rn;
    return e;
}

static std::string first_diff(const std::vector<std::string>& got, const std::vector<std::string>& want)
{
    for (size_t i = 0; i < got.size() || i < want.size(); i++) {
        const std::string g = i < got.size() ? got[i] : "(no line)", w = i < want.size() ? want[i] : "(no line)";
        if (g != w) return "line " + std::to_string(i + 1) + ": the program printed \"" + g + "\", the library gives \"" + w + "\"";
    }
    return "no difference";
}

template <class P>
void program_cases(const char* name, const std::string& exe, const std::string& file, double R, double acc)
{
    const size_t cap = 10000000;
    char args[128];
    snprintf(args, sizeof args, " -R %g -a %g --cap 10000000", R, acc);
    const std::string base = std::string("'") + exe + "' -f '" + file + "'" + args;
    Expect lib[3];
    for (int L = 0; L <= 2; L++) lib[L] = library_lines<P>(file, R, acc, cap, L);
    // marking does take effect in the library: no key marked at L = 0; at L = 1 and 2 keys marked, more at L = 2, and
    // fewer leaves (else a program that ignores the option could not be told from one that does not)
    for (int k = 0; k < 2; k++) {
        check(lib[0].marked[k] == 0 && lib[1].marked[k] > 0 && lib[2].marked[k] > lib[1].marked[k],
              "%s zipper %d: keys marked at L = 0, 1, 2: %zu, %zu, %zu", name, k + 1, lib[0].marked[k], lib[1].marked[k],
              lib[2].marked[k]);
        check(lib[1].leaves[k] < lib[0].leaves[k] && lib[2].leaves[k] <= lib[1].leaves[k],
              "%s zipper %d: leaves at L = 0, 1, 2: %zu, %zu, %zu", name, k + 1, lib[0].leaves[k], lib[1].leaves[k],
              lib[2].leaves[k]);
    }
    check(lib[0].lines.size() == 2 && lib[1].lines.size() == 4 && lib[2].lines.size() == 4,
          "%s: the library's lines: %zu, %zu, %zu (2, 4, 4 expected: the two build lines, and the two marking lines at L >= 1)",
          name, lib[0].lines.size(), lib[1].lines.size(), lib[2].lines.size());
    // the program prints what the library computes: with no option and with --overlap 0 the lines of L = 0, and with
    // --overlap 1 and 2 those of L = 1 and 2 (the leaves of the build lines, and the keys marked)
    const char* opt[4] = {"", " --overlap 0", " --overlap 1", " --overlap 2"};
    const int level[4] = {0, 0, 1, 2};
    for (int i = 0; i < 4; i++) {
        std::string err;
        int st = run_program(base + opt[i], err);
        std::vector<std::string> got = marking_lines(err);
        check(st == 0 && got == lib[level[i]].lines, "%s%s: exit %d; %s", name, opt[i], st,
              first_diff(got, lib[level[i]].lines).c_str());
    }
    // the range of the option: refused before anything is built
    for (const char* bad : {"17", "x", ""}) {
        std::string err;
        int st = run_program(base + " --overlap '" + bad + "'", err);
        check(st == 1 && marking_lines(err).empty(), "%s --overlap '%s' is refused (exit %d, %zu lines of build or marking)",
              name, bad, st, marking_lines(err).size());
    }
    note("(%s: leaves %zu / %zu at L = 0, %zu / %zu at L = 1, %zu / %zu at L = 2; keys marked %zu / %zu at L = 1, %zu / %zu at L = 2)",
         name, lib[0].leaves[0], lib[0].leaves[1], lib[1].leaves[0], lib[1].leaves[1], lib[2].leaves[0], lib[2].leaves[1],
         lib[1].marked[0], lib[1].marked[1], lib[2].marked[0], lib[2].marked[1]);
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::cs::setup();
    z::imm::setup();
    printf("test_overlap_programs: the --overlap option of zipper3 and zipper5\n");
#if defined(ZIPPERS_ZIPPER3) && defined(ZIPPERS_ZIPPER5) && defined(ZIPPERS_REPO)
    const std::string f3 = std::string(ZIPPERS_REPO) + "/legacy/s3/subst_4.00_9.50_t3.txt";
    const std::string f4 = std::string(ZIPPERS_REPO) + "/legacy/s4/subst5_2.30_4.50_t1.txt";
    section("zipper3 --overlap L prints the build and marking lines of the library at L (S^3 _t3, R = 0, ACC 0.04)");
    program_cases<z::cs::RenderPolicy>("zipper3", ZIPPERS_ZIPPER3, f3, 0, 0.04);
    section("zipper5 --overlap L prints the build and marking lines of the library at L (S^4 _t1, R = 0, ACC 0.04)");
    program_cases<z::imm::RenderPolicy>("zipper5", ZIPPERS_ZIPPER5, f4, 0, 0.04);
#else
    note("zipper3 and zipper5 not built in this configuration: their runs are left out");
#endif
    rtest::end_section();
    printf("test_overlap_programs: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
