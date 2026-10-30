// SPDX-License-Identifier: GPL-3.0-or-later
/*  roofs.cc  --  the roofs of a substitution file, measured

    THE QUESTION. How good is a presentation: is the roof of x^-1 the inverse word of the roof of x, is the roof of
    every involution letter its own inverse word (so that an involution arc and its other orientation are the same
    arc at every level), how often do consecutive roofs cancel, and how fast do the roofs grow? This program reads a
    substitution file (from `chz kernel` or `chz5 kernel`, with or without the options of the kernel search) and answers, from the
    words alone and their exact check. It changes nothing: the reader, make_side and the measures are the library's
    (render/world.h: prepare; search/roof_choice.h). The definitions are at the top of search/roof_choice.h.

    THE OUTPUT (stdout), for each side:
      the exact check   every roof OF THE FILE (all letters, also those the renderer does not read: docs/developer_guide.md) is a word
                        for t^-1 s t (Z+) or t s t^-1 (Z-), exactly; prepare checks the renderer's roofs and exits if
                        one fails
      inverse words     the letters x (not involutions) whose file roof of x^-1 is the inverse word of their roof, as
                        the renderer makes it
      self-inverse      the involution letters whose roof is equal letter by letter to its own inverse word, and the
                        letters whose two orientations are reversals of each other at every level (every involution
                        letter they reach in the roof graph, themselves included, has a self-inverse roof)
      lengths           of the roofs as the renderer uses them: min, mean, max, total
      junctions         of theta(s) and of theta^2(s) over all letters s, and those that cancel (the last letter of
                        theta(y) is the inverse of the first of theta(z)); of the first, those at an involution letter
      all levels        the junctions of every theta^n(s), n >= 1 (a finite set), those that cancel, and the first level
                        at which one does; or "none": every theta^n(s) is freely reduced
      growth rate       the Perron-Frobenius root of the substitution matrix of the roofs as the renderer uses them
    Timings and memory go to stderr only.

    usage: roofs -e cs|imm [-f FILE]
      -e cs|imm        the example: the Cartwright-Steger surface (S^3) or IMM's M5 (S^4)
      -f FILE          its substitution file (default subst_4.00_9.50_t3_r1.txt or subst5_2.30_4.50_t1_r0.txt, as zipper3 and
                       zipper5: the files of data/s3/ and data/s4/), found as they find theirs (src/core/data_files.h)

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "core/timing.h"
#include "default_file.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "render/renderer.h"
#include "render/substitution.h"
#include "render/world.h"
#include "search/roof_choice.h"

#include <sys/resource.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace zippers;

namespace {

double peak_mb()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss/1e6;
#else
    return ru.ru_maxrss/1e3;
#endif
}

// the letters that x reaches in the roof graph of th (x itself, the letters of its roof, theirs, ...)
std::vector<char> reach(const std::vector<RoofWord>& th, int x)
{
    std::vector<char> r(th.size(), 0);
    std::vector<int> st = {x};
    r[x] = 1;
    while (!st.empty()) {
        int y = st.back();
        st.pop_back();
        for (int z : th[y]) if (!r[z]) { r[z] = 1; st.push_back(z); }
    }
    return r;
}

template <class P>
int run(const char* file)
{
    typedef typename P::B B;
    typedef typename B::Elem Elem;
    double t0 = now();
    World<B> W;
    prepare<P>(file, W);                       // checks the renderer's roofs exactly, and exits if one fails
    Substitution D;
    if (!load_substitution(file, D, P::dialect)) return 1;
    const Letters<B>& L = W.L;
    size_t n = L.s.size();
    int ninv = 0, nrep = 0;
    for (size_t j = 0; j < n; j++) { ninv += L.inv[j] == (int)j; nrep += L.plus[j]; }
    printf("roofs: file %s\n  |S| = %zu: %d involution letters, %d representatives (base arcs)\n", file, n, ninv, nrep);
    Elem t = P::word(D.tword), ti = B::inv(t);
    for (int side = 0; side < 2; side++) {
        const std::vector<RoofWord>& fileroof = side == 0 ? D.up : D.down;
        const Side<B>& sd = side == 0 ? W.plus : W.minus;
        const std::vector<RoofWord>& th = sd.roof;
        printf("\n== %s (%s) ==\n", sd.name, side == 0 ? "the up roofs, t^-1 s t" : "the down roofs, t s t^-1");
        // the exact check of the file's own roofs, all letters
        int ok = 0, empty = 0;
        for (size_t j = 0; j < n; j++) {
            if (fileroof[j].empty()) { empty++; continue; }
            Elem p = B::identity();
            for (int x : fileroof[j]) p = B::mul(p, L.s[x]);
            Elem want = side == 0 ? B::mul(B::mul(ti, L.s[j]), t) : B::mul(B::mul(t, L.s[j]), ti);
            if (B::same(p, want)) ok++;
        }
        printf("  the exact check: %d / %zu roofs of the file are words for their elements%s\n", ok, n,
               empty ? " (some are empty)" : "");
        // inverse words, as the renderer makes them
        int nonrep = 0, invw = 0;
        for (size_t j = 0; j < n; j++) {
            if (L.inv[j] == (int)j) continue;
            nonrep++;
            if (fileroof[j] == inverse_word(fileroof[L.inv[j]], L.inv)) invw++;
        }
        printf("  inverse words: for %d of the %d letters that are not involutions, the file's roof of x^-1 is the inverse"
               " word of the roof of x (the renderer makes it so for all)\n", invw, nonrep);
        // self-inverse involution roofs, and the orientations
        std::vector<int> si, notsi;
        for (size_t j = 0; j < n; j++)
            if (L.inv[j] == (int)j) (self_inverse(th[j], L.inv) ? si : notsi).push_back((int)j);
        printf("  self-inverse: %zu of the %d involution letters have a roof equal to its own inverse word", si.size(), ninv);
        if (!notsi.empty()) {
            printf("; not:");
            for (int z : notsi) printf(" %d", z);
        }
        printf("\n");
        int coincide = 0;
        for (size_t j = 0; j < n; j++) {
            std::vector<char> r = reach(th, (int)j);
            bool good = true;
            for (int z : notsi) if (r[z]) good = false;
            coincide += good;
        }
        printf("  the two orientations of a branch are reversals of each other at every level for %d of the %zu letters%s\n",
               coincide, n, coincide == (int)n ? " (all)" : "");
        // lengths, junctions, all levels, growth
        size_t mn = (size_t)-1, mx = 0;
        for (const RoofWord& w : th) { mn = std::min(mn, w.size()); mx = std::max(mx, w.size()); }
        JunctionCount jc = count_junctions(th, L.inv);
        printf("  lengths: min %zu, mean %.2f, max %zu, total %ld\n", mn, (double)jc.length/n, mx, jc.length);
        printf("  junctions of theta(s): %ld, cancelling %ld (%ld at an involution letter); of theta^2(s): %ld,"
               " cancelling %ld%s\n", jc.junctions1, jc.cancel1, jc.cancel1_inv, jc.junctions2, jc.cancel2,
               jc.inner ? "; NOT FREELY REDUCED roofs" : "");
        TurnClosure tc = turn_closure(th, L.inv);
        if (tc.cancelling)
            printf("  all levels: %zu junctions occur in some theta^n(s), %zu of them cancel; the first at level %d"
                   " (theta^%d(s) is not freely reduced for some s)\n", tc.turns, tc.cancelling, tc.first_level,
                   tc.first_level + 1);
        else
            printf("  all levels: %zu junctions occur in some theta^n(s), none cancels: every theta^n(s) is freely reduced\n",
                   tc.turns);
        printf("  growth rate (Perron-Frobenius root of the roofs as the renderer uses them): %.4f\n", growth_rate(th));
    }
    fprintf(stderr, "roofs: %.2fs, peak %.0f MB\n", now() - t0, peak_mb());
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string ex, file;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-e" && i + 1 < argc) ex = argv[++i];
        else if (a == "-f" && i + 1 < argc) file = argv[++i];
        else { fprintf(stderr, "usage: roofs -e cs|imm [-f FILE]\n"); return 1; }
    }
    if (ex == "cs" || ex == "imm") {
        const bool is_cs = ex == "cs";
        const bool default_file = file.empty();    // the programs' default
        if (default_file) file = tools::default_subst(is_cs);
        if (!tools::find_subst(file, is_cs, default_file, ZIPPERS_DATA_PLACES)) return 1;
        if (is_cs) {
            cs::setup();
            return run<cs::RenderPolicy>(file.c_str());
        }
        imm::setup();
        return run<imm::RenderPolicy>(file.c_str());
    }
    fprintf(stderr, "usage: roofs -e cs|imm [-f FILE]\n");
    return 1;
}
