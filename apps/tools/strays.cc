// SPDX-License-Identifier: GPL-3.0-or-later
/*  strays.cc  --  how far do the true zipper curves stray from the drawn chords? 

    THE QUESTION. The S^3 and S^4 renderers (render/renderer.h) accept a leaf (h, x) when its chord is at most the
    accuracy ACC and the vertices of its first split lie within ACC of the chord. That bounds the chord and those
    vertices, not the rest of the curve the leaf stands for. This program measures how far the curve strays from the
    drawn chords, as was done for the S^2 program: it draws at ACC and at ACC/k (k = 8 by default) and compares the drawings.
    It changes nothing: the renderer, arc_points and the painters are the library's (src/).

    THE MEASURES. Every distance is a chord of R^N (N = 4 for S^3, 5 for S^4) from a point to a segment
    (geometry/sphere.h: segment_distance), printed as a multiple of ACC.
      fine -> coarse   every vertex of the fine drawing, to the nearest segment of the coarse drawing. The vertices are
                       points h V of the true curve, so this is the measure of the strays made for the S^2 program.
      coarse -> fine   every vertex of the coarse drawing, to the nearest segment of the fine drawing: the other way
                       round (large only where the fine drawing lacks a piece of the coarse one).
      leaf strays      every leaf (h, x) of the coarse drawing: the farthest vertex of its refinement at ACC/k (by
                       arc_points's rule, render/arcs.h) from its own chord. This is what an enclosure would have to
                       bound; for the same vertices it is at least the fine -> coarse distance. The refinement of a leaf
                       skips the branches it has met already (a repeated branch has the same vertices), except in arcs
                       mode without --dedup, where it is complete.
    The nearest segments are found exactly (a grid of cells, SegGrid below), in double precision from the stored
    points: doubles for the coarse arcs, floats (as the renderer stores them) for the fine arcs and for the pictures, so
    that distances below about 1e-7 are rounding.

    THE MODES.
      arcs (default)   each base arc alpha_x of each side (R = 0; x runs over the representatives of the inverse pairs,
                       as build draws them) as a complete arc: arc_points at ACC and at ACC/k, with no de-duplication.
                       The fine arc refines the coarse one (a branch split at ACC is split at ACC/k: the rule is
                       monotone in the accuracy, and its guards do not depend on it), so every fine vertex belongs to
                       one coarse leaf, and the strays are attributed to letters, depths and children. The leaves come
                       from walk (below), which follows arc_points step for step; --verify checks it, bit for bit.
                       With --dedup each arc skips the branches it has met already (by the renderer's key), at both
                       accuracies: the distances to a drawing do not depend on how often a piece is traversed, so the
                       largest values are the same, and only the weights of the quantiles and counts change. Complete
                       arcs can be far larger than the curve (an arc that retraces itself, at every scale, is traversed
                       many times over), and then only --dedup is affordable.
      picture          the renderers' polylines, as zipper3 and zipper5 draw them (render/world.h: build), from the
                       roots of radius R, at ACC and at ACC/k; and the leaf strays of every leaf of the coarse picture,
                       which a copy of the renderer that reports its leaves (LeafRenderer) finds. The copy is checked
                       against the renderer, point for point, in every run.
      chains           no drawing: the cycles of one and two steps in the roofs of each side (chains_side below),
                       each with its map, parabolic or loxodromic, and its fixed point: the chains an enclosure must
                       follow.

    THE OUTPUT (stdout) for each side: the counts; in each measure the largest value and the quantiles 50, 90, 99 and
    99.9 % (the value of rank floor(q (n - 1)) in increasing order); the numbers farther than 1, 2 and 4 ACC; a table
    per base letter; and the worst leaves (by their farthest fine vertex from the coarse drawing), each with its letter,
    its depth and its place in the roof of its parent, and, for the farthest vertex of its refinement from its chord:
    the level below the leaf at which that vertex is born, the path to it (child:letter, repeated blocks as (..)^r), the
    longest repeated block (a chain into the fixed point of its map g: parabolic, or g's translation length; and how
    far the chain's limit lies from the chord and from the coarse drawing), and the farthest vertex of each level.
    Timings and memory go to stderr only, so that two runs print the same.
    THE TIPS. In S^4 the renderer draws a leaf whose letter has tips (render/tips.h: the cusp points of the
    parabolic cycles of the roofs) through them: a, its tip points, b. Every drawing here is made the same way (the
    renderers' pictures, arc_points, and the copies below: walk, LeafRenderer, the refinements of the anatomy), and a
    leaf's stray is measured from the leaf as drawn, its polyline through its tips, not from its chord. --notips
    removes the tips (the Sides' tip lines) after prepare: the drawing without the tips, for the comparison.
    THE LOOK-AHEAD THROUGH ONE-LETTER ROOFS, (b). In S^3 the renderer also splits a branch whose letter has a
    one-letter roof when a vertex inside the first split of its only child (the same arc) is farther than ACC from
    its chord (render/renderer.h: through_split); so do the copies here (walk, LeafRenderer, the anatomy's
    refinements). --nothrough turns it off after prepare (the S^3 drawing without it); --through turns it on where
    the policy leaves it off (S^4, to see what it would change there).
    PICTURES: --png FILE, the coarse drawing of the sides measured, on the S^3 or S^4 painter (view/s3.h, view/s4.h) in
    the programs' default view, with the worst strays of each side ringed in green; --closeup PREFIX, for each side
    (PREFIX_Zplus.png, PREFIX_Zminus.png), a close-up of its worst leaf in the same projection, centred on it, with the
    leaf's chord along the screen's x axis and the stray along its y axis: the fine drawing in orange, the coarse one in
    blue, the leaf's own refinement in dark red, its chord in green (the vertices of its first split ringed in green),
    the stray vertex ringed in black, and a black bar one ACC long at the lower left.

    usage: strays -e cs|imm [options]
      -e cs|imm        the example: the Cartwright-Steger surface (S^3) or IMM's M5 (S^4)
      -f FILE          its substitution file (default subst_4.00_9.50_t3_r1.txt or subst5_2.30_4.50_t1_r0.txt, as zipper3 and
                       zipper5: the files of data/s3/ and data/s4/), found as they find theirs (src/core/data_files.h)
      --mode arcs|picture|chains
      -R R             picture mode: the radius of the roots (default 0)
      -a ACC           the coarse accuracy (default 0.01)
      -k K             the fine accuracy is ACC/K (default 8)
      --only +|-       one side only (default both)
      --dedup          arcs mode: each distinct branch of an arc once
      --verify         arcs mode (without --dedup): check walk against arc_points, bit for bit, at both accuracies
      --checkgrid      repeat a sample of the nearest-segment queries (at most 300 of each kind) by brute force
      --worst N        the worst leaves listed per side (default 10)
      --cap N          picture mode: the renderers' leaf cap (default 6e7; a capped picture is reported as such);
                       arcs mode: the most leaves of an arc at either accuracy (default 1e7; the run stops beyond)
      --memlimit GB    stop (exit status 3) when the process uses more (default 1.5)
      --png FILE       the overview picture;  -s SIZE its size (default 900 for S^3, 840 for S^4)
      --rings N        the worst strays ringed per side in the overview (default 8)
      --closeup PREFIX the close-ups;  --closesize SIZE their size (default 700)
      --closeown       the close-ups show the worst leaf stray (default: the worst leaf by fine -> coarse)
      --closeleaf A[,B] picture mode: the close-up of Z+ shows leaf number A of its coarse picture, that of Z- leaf B
                       (the numbers "#n" of the lists of worst leaves; the leaves, and so their numbers, are the same
                       with and without tips)
      --closesame      with --closeleaf: frame the close-up as the drawing without tips frames it (centred on the leaf
                       and its farthest fine vertex from the coarse picture without tips), and draw it with the tips:
                       the same close-up as without the tips, drawn again
      --notips         draw without the tips (the leaf strays are then from the chords)
      --nothrough      draw without the look-ahead through one-letter roofs (in S^3)
      --through        draw with it, also where the policy leaves it off (S^4)
      --overlap L      picture mode: the coarse picture with the renderer's marking, L levels (render/renderer.h);
                       the fine picture without it, so that the strays measure what marking costs
      --reverse        picture mode: the coarse picture with the reverse keys of involution branches marked too
    THE WINDOW, picture mode: the view of zipper3 or zipper5 with --zoom F, --clip RHO, --centre P, --yaw D,
    --pitch D, --pole D1 D2 (S^3), --turn D, --turn2 D (S^4) (apps/tools/leafcopy.h: View<N>::program), and
      --window         count in the measures only the vertices in its window (render/cull.h: CullWindow::contains):
                       fine -> coarse and leaf -> coarse for the fine vertices in it, coarse -> fine for the coarse ones,
                       the leaf strays for the vertices of a leaf's refinement in it (a leaf with none is left out)
      --cull           draw both pictures with culling to that window (the renderer's, render/renderer.h; the leaf copy
                       culls as the renderer does); implies --window
      --cullk K        its margin factor (default 16: render/cull.h, CULL_K)

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "core/hash.h"
#include "core/keyset.h"
#include "core/memory.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/backend.h"
#include "geometry/sphere.h"
#include "output/png.h"
#include "output/raster.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s3.h"
#include "view/s4.h"

#include "default_file.h"
#include "leafcopy.h"

#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace zippers;
using namespace zippers::tools;

namespace {

const double INF = std::numeric_limits<double>::infinity();

struct Options {
    std::string example, subst, mode = "arcs";
    double R = 0, acc = 0.01, memlimit = 1.5;
    int k = 8, which = 3, worst = 10, size = 0, closesize = 700, rings = 8;
    size_t cap = 0;
    bool verify = false, dedup = false, checkgrid = false, closeown = false, notips = false;
    long closeleaf[2] = {-1, -1};                      // --closeleaf: the leaf of each side's close-up
    bool closesame = false;                            // --closesame
    int through = -1;                                  // --nothrough (0), --through (1); -1: the policy's choice
    int overlap = 0;                                   // --overlap L: the coarse picture's marking
    bool reverse = false;                              // --reverse
    std::string png, closeup;
    // the window of a zoomed view (picture mode)
    ViewOptions view;                                  // --zoom, --clip, --centre, --yaw, --pitch, --pole, --turn, --turn2
    bool cull = false;                                 // --cull, --cullk: both pictures culled with its window
    double cullk = CULL_K;
    bool window = false;                               // --window (or --cull): the measures inside the window only
};

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

void memcheck(const Options& o, const char* where)
{
    double b = resident_bytes();
    if (b > o.memlimit*1e9) {
        fprintf(stderr, "strays: %.2f GB in use (%s), more than --memlimit %.2f GB: stopping\n", b/1e9, where,
                o.memlimit);
        exit(3);
    }
}

// the distance from p to the polyline l
template <int N>
double line_distance(const SpherePoint<N>& p, const std::vector<SpherePoint<N>>& l)
{
    double d = INF;
    for (size_t i = 0; i + 1 < l.size(); i++) d = std::min(d, segment_distance(p, l[i], l[i + 1]));
    return d;
}

// the nearest point of the polyline l to p (foot_point, below, on its nearest segment)
template <int N>
SpherePoint<N> foot_point(const SpherePoint<N>& p, const SpherePoint<N>& a, const SpherePoint<N>& b);
template <int N>
SpherePoint<N> line_foot(const SpherePoint<N>& p, const std::vector<SpherePoint<N>>& l)
{
    size_t k = 0;
    for (size_t i = 0; i + 1 < l.size(); i++)
        if (segment_distance(p, l[i], l[i + 1]) < segment_distance(p, l[k], l[k + 1])) k = i;
    return foot_point(p, l[k], l[k + 1]);
}

// ---------------------------------------------------------------------------------------------------------------------
// Polylines and the nearest-segment grid

// polylines of points of R^N with coordinates of type T: polyline k is the vertices [starts[k], starts[k+1])
template <int N, class T>
struct Lines {
    const T* x = nullptr;
    const uint32_t* starts = nullptr;
    size_t np = 0, nv = 0;
    size_t pend(size_t k) const { return k + 1 < np ? starts[k + 1] : nv; }
    SpherePoint<N> point(size_t g) const
    {
        SpherePoint<N> p;
        for (int i = 0; i < N; i++) p.x[i] = (double)x[(size_t)N*g + i];
        return p;
    }
};

template <int N, class T>
Lines<N, T> lines_of(const std::vector<T>& x, const std::vector<uint32_t>& starts)
{
    Lines<N, T> l;
    l.x = x.data(); l.starts = starts.data(); l.np = starts.size(); l.nv = x.size()/N;
    return l;
}

// The segments of a set of polylines (segment g joins the vertices g and g + 1 of one polyline), for exact
// nearest-segment queries. LEVELS of cells of side c, 8c, 64c, ... up to a side of at least 2.5 (the sphere lies in
// [-1, 1]^N): at each level a segment is listed in every cell that its bounding box meets; if the box spans more
// than 3 cells in some coordinate, the segment is cut into m pieces at most a cell long in each coordinate and listed
// in every cell that the box of a piece meets (they cover the segment); and if that would take more than 64 pieces,
// in the level's list of long segments, which every query of the level checks. (The spikes drawn to their tips
// are long segments, and checking them all in every query made the S^4 runs five times slower.) A level is built
// when a query first needs it.
// A QUERY from a point p with an upper bound D on the distance (and the segment that attains it) searches, at growing
// radii r (c/8 and c at the first level, then the side of each level), every segment listed in a cell within min(D, r)
// of p; once the nearest found is within r, no segment is nearer (a nearer one would meet a cell searched), so the
// answer is exact. Ties go to the lowest segment number, so the answer does not depend on the order of the cells.
template <int N, class T>
struct SegGrid {
    static constexpr int BITS = 60/N;                    // bits per coordinate in a cell's key
    Lines<N, T> ln;
    std::vector<uint32_t> segs;
    struct Level {
        double c = 0;
        std::vector<uint64_t> key;                       // the cells: open addressing, 0 = empty
        std::vector<uint32_t> beg, cnt, items, longs;
        int shift = 0;
        size_t n = 0;
        bool built = false;
    };
    std::vector<Level> lv;

    SegGrid(const Lines<N, T>& l, double c0) : ln(l)
    {
        for (size_t k = 0; k < ln.np; k++)
            for (size_t g = ln.starts[k]; g + 1 < ln.pend(k); g++) segs.push_back((uint32_t)g);
        c0 = std::max(c0, 3.0/((double)(1 << BITS) - 2));
        for (double c = c0;; c *= 8) {
            Level L;
            L.c = c;
            lv.push_back(L);
            if (c >= 2.5) break;
        }
    }

    static int64_t cell(double v, double c) { return (int64_t)std::floor((v + 2.0)/c); }
    static uint64_t pack(const int64_t* ci)
    {
        uint64_t k = 1ULL << 63;
        for (int i = 0; i < N; i++) k |= (uint64_t)ci[i] << (BITS*i);
        return k;
    }
    static size_t slot(const Level& L, uint64_t k)
    {
        size_t m = L.key.size() - 1, i = (size_t)((k*0x9E3779B97F4A7C15ULL) >> L.shift);
        while (L.key[i] && L.key[i] != k) i = (i + 1) & m;
        return i;
    }
    static void grow(Level& L)
    {
        std::vector<uint64_t> ok;
        std::vector<uint32_t> oc;
        ok.swap(L.key); oc.swap(L.cnt);
        size_t sz = ok.empty() ? (1u << 12) : 2*ok.size();
        L.key.assign(sz, 0); L.cnt.assign(sz, 0);
        L.shift = 64 - (int)std::lround(std::log2((double)sz));
        for (size_t i = 0; i < ok.size(); i++)
            if (ok[i]) { size_t s = slot(L, ok[i]); L.key[s] = ok[i]; L.cnt[s] = oc[i]; }
    }
    // the cells of the box [lo, hi] (cell indices), in odometer order
    template <class F>
    static void each_cell(const int64_t* lo, const int64_t* hi, F&& f)
    {
        int64_t ci[N];
        for (int i = 0; i < N; i++) ci[i] = lo[i];
        for (;;) {
            f(ci);
            int i = 0;
            for (; i < N; i++) { if (++ci[i] <= hi[i]) break; ci[i] = lo[i]; }
            if (i == N) return;
        }
    }
    // the cells of segment g at level L, each once (f(cell)); false if it is long there (more than 64 pieces)
    template <class F>
    bool cells(const Level& L, uint32_t g, F&& f) const
    {
        int64_t lo[N], hi[N];
        double a[N], b[N], span = 0;
        for (int i = 0; i < N; i++) {
            a[i] = (double)ln.x[(size_t)N*g + i]; b[i] = (double)ln.x[(size_t)N*(g + 1) + i];
            lo[i] = cell(std::min(a[i], b[i]), L.c); hi[i] = cell(std::max(a[i], b[i]), L.c);
            span = std::max(span, std::fabs(b[i] - a[i])/L.c);
        }
        bool small = true;
        for (int i = 0; i < N; i++) small = small && hi[i] - lo[i] <= 2;
        if (small) { each_cell(lo, hi, f); return true; }
        int m = (int)std::ceil(span);
        if (m > 64) return false;
        std::vector<std::array<int64_t, N>> all;         // the cells of the pieces' boxes, which overlap: each once
        for (int k = 0; k < m; k++) {
            double t0 = (double)k/m, t1 = (double)(k + 1)/m;
            for (int i = 0; i < N; i++) {
                double u = k == 0 ? a[i] : a[i] + t0*(b[i] - a[i]), v = k + 1 == m ? b[i] : a[i] + t1*(b[i] - a[i]);
                lo[i] = cell(std::min(u, v), L.c); hi[i] = cell(std::max(u, v), L.c);
            }
            each_cell(lo, hi, [&](const int64_t* ci) {
                std::array<int64_t, N> c;
                for (int i = 0; i < N; i++) c[i] = ci[i];
                all.push_back(c);
            });
        }
        std::sort(all.begin(), all.end());
        all.erase(std::unique(all.begin(), all.end()), all.end());
        for (const std::array<int64_t, N>& c : all) f(c.data());
        return true;
    }
    void build(Level& L)
    {
        grow(L);
        for (uint32_t g : segs) {
            bool listed = cells(L, g, [&](const int64_t* ci) {
                uint64_t k = pack(ci);
                size_t s = slot(L, k);
                if (!L.key[s]) {
                    if (2*(L.n + 1) > L.key.size()) { grow(L); s = slot(L, k); }
                    L.key[s] = k; L.n++;
                }
                L.cnt[s]++;
            });
            if (!listed) L.longs.push_back(g);
        }
        L.beg.assign(L.key.size(), 0);
        size_t tot = 0;
        for (size_t s = 0; s < L.key.size(); s++)
            if (L.key[s]) { L.beg[s] = (uint32_t)tot; tot += L.cnt[s]; L.cnt[s] = 0; }
        L.items.resize(tot);
        for (uint32_t g : segs)
            cells(L, g, [&](const int64_t* ci) {
                size_t s = slot(L, pack(ci));
                L.items[L.beg[s] + L.cnt[s]++] = g;
            });
        L.built = true;
    }
    void consider(const SpherePoint<N>& p, uint32_t g, double& best, uint32_t& arg) const
    {
        double d = segment_distance(p, ln.point(g), ln.point(g + 1));
        if (d < best || (d == best && g < arg)) { best = d; arg = g; }
    }
    void search(Level& L, const SpherePoint<N>& p, double r, double& best, uint32_t& arg)
    {
        if (!L.built) build(L);
        double rr = r*(1 + 1e-9) + 1e-12;
        int64_t lo[N], hi[N];
        for (int i = 0; i < N; i++) { lo[i] = cell(p.x[i] - rr, L.c); hi[i] = cell(p.x[i] + rr, L.c); }
        each_cell(lo, hi, [&](const int64_t* ci) {
            size_t s = slot(L, pack(ci));
            if (!L.key[s]) return;
            for (uint32_t i = L.beg[s], e = L.beg[s] + L.cnt[s]; i < e; i++) consider(p, L.items[i], best, arg);
        });
        for (uint32_t g : L.longs) consider(p, g, best, arg);
    }
    // the same by brute force, over every segment (for --checkgrid)
    double brute(const SpherePoint<N>& p) const
    {
        double best = INF;
        uint32_t arg = UINT32_MAX;
        for (uint32_t g : segs) consider(p, g, best, arg);
        return best;
    }
    // the distance from p to the nearest segment, and the segment (arg); best and arg may give an upper bound
    double nearest(const SpherePoint<N>& p, double best, uint32_t& arg)
    {
        if (best == INF) arg = UINT32_MAX;
        for (size_t step = 0; step <= lv.size(); step++) {
            Level& L = lv[step == 0 ? 0 : step - 1];
            double r = std::min(best, step == 0 ? L.c/8 : L.c);
            search(L, p, r, best, arg);
            if (best <= r) return best;
        }
        return best;                                     // INF if there are no segments
    }
};

// --checkgrid: every stride-th query of a kind is repeated by brute force (at most 300 of each kind)
struct GridCheck {
    size_t stride = 0, n = 0, checked = 0, bad = 0;
    double maxdiff = 0;
    template <class G, class Pt>
    void check(const G& g, const Pt& p, double d)
    {
        if (!stride || n++ % stride || checked >= 300) return;
        checked++;
        double e = g.brute(p);
        if (e != d) { bad++; maxdiff = std::max(maxdiff, std::fabs(e - d)); }
    }
    void print(const char* what) const
    {
        if (stride)
            printf("  grid check (%s): %zu queries repeated by brute force, %zu different (largest difference %.3g)\n",
                   what, checked, bad, maxdiff);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Statistics

// the value of rank floor(q (n - 1)) in increasing order (v is reordered)
double quantile(std::vector<float>& v, double q)
{
    if (v.empty()) return 0;
    size_t r = (size_t)std::floor(q*(double)(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + r, v.end());
    return v[r];
}

struct Dist {                                          // the distribution of one measure, in multiples of ACC
    std::vector<float> v;
    double max = 0;
    size_t over[3] = {0, 0, 0};                        // farther than 1, 2, 4 ACC
    void add(double d)
    {
        v.push_back((float)d);
        max = std::max(max, d);
        if (d > 1) over[0]++;
        if (d > 2) over[1]++;
        if (d > 4) over[2]++;
    }
    void print(const char* what, const char* unit)
    {
        size_t n = v.size();
        double q50 = quantile(v, 0.5), q90 = quantile(v, 0.9), q99 = quantile(v, 0.99), q999 = quantile(v, 0.999);
        printf("    %-15s %10zu %s: max %.4g  q50 %.3g  q90 %.3g  q99 %.3g  q99.9 %.3g   > 1: %zu  > 2: %zu"
               "  > 4: %zu\n", what, n, unit, max, q50, q90, q99, q999, over[0], over[1], over[2]);
        std::vector<float>().swap(v);
    }
};

// the leaves whose letter has a one-letter roof: their first split has no inner vertex, so the present look-ahead tests
// nothing there (with (b) it tests the first split of the only child)
struct OneLetter {
    size_t n = 0;
    double own = 0, poly = 0;                          // the largest leaf stray, and farthest vertex from the drawing
    void add(double o, double p) { n++; own = std::max(own, o); poly = std::max(poly, p); }
    void print() const
    {
        if (n)
            printf("  leaves with a one-letter roof: %zu; largest leaf stray %.4g, farthest vertex from"
                   " the coarse drawing %.4g\n", n, own, poly);
    }
};

template <int N>
double dot(const SpherePoint<N>& a, const SpherePoint<N>& b)
{
    double s = 0;
    for (int i = 0; i < N; i++) s += a.x[i]*b.x[i];
    return s;
}

// the nearest point of the segment [a, b] to p
template <int N>
SpherePoint<N> foot_point(const SpherePoint<N>& p, const SpherePoint<N>& a, const SpherePoint<N>& b)
{
    double uu = 0, uw = 0;
    for (int i = 0; i < N; i++) { double u = b.x[i] - a.x[i]; uu += u*u; uw += u*(p.x[i] - a.x[i]); }
    double t = uu > 0 ? std::max(0.0, std::min(1.0, uw/uu)) : 0;
    SpherePoint<N> f;
    for (int i = 0; i < N; i++) f.x[i] = a.x[i] + t*(b.x[i] - a.x[i]);
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Chains: translation lengths and limits

// what exact powers say about g (g of infinite order): g^n for n = 2^j, computed exactly by squaring while the
// coefficients of the power stay below a bound that keeps the next product exact (O(n,1): 2^29, its products being
// summed in 128 bits; PU(2,1): 2^26, in 64 bits), up to j = 20. Let D = d(o, g^n o) - d(o, g^(n/2) o) at the last
// step. A loxodromic g has D = (n/2) len + O(e^-(n len/2)), len its translation length (in the normalisation of
// B::displacement), and reaches the bound soon: by j = 20 only if len < 4e-5 or so. A parabolic g has polynomial
// growth: d(o, g^n o) = c log n + O(1) with c = 2 in O(n,1) and 2 or 4 in PU(2,1), so D tends to c log 2 < 3. g is
// called parabolic (len = 0) when all 20 squarings stay below the bound, or when n >= 1024 and D < 3.5 (which a
// loxodromic g would satisfy only if len < 0.007); otherwise len is spectral_length (exact powers give it to
// O(e^-(n len/2)) only, about 1e-3 for t). limit: the point h g^n V, the limit of the chain h g^m V up to
// O(e^-(n len)) (loxodromic) or O(1/n) (parabolic), relative to the size of h's arc.
template <class B>
struct ChainInfo {
    double len = -1;
    bool parabolic = false;
    typename B::Point limit;
};

inline double abs2(double v) { return v*v; }
inline double abs2(const std::complex<double>& v) { return std::norm(v); }

template <class M>
M mat_mul(const M& a, const M& b)
{
    M c = a;
    const size_t n = a.size();
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++) {
            auto s = a[i][0]*b[0][j];
            for (size_t k = 1; k < n; k++) s += a[i][k]*b[k][j];
            c[i][j] = s;
        }
    return c;
}
template <class M>
double mat_norm(const M& m)
{
    double s = 0;
    for (const auto& row : m) for (const auto& e : row) s += abs2(e);
    return std::sqrt(s);
}
template <class M>
void mat_scale(M& m, double f) { for (auto& row : m) for (auto& e : row) e *= f; }

// the translation length of a loxodromic g from its matrix in doubles: the log of its spectral radius, by repeated
// squaring (log rho = log s_0 + sum_j log(s_j)/2^j with M_0 = g/s_0, M_j = M_{j-1}^2/s_j), times 2 in PU(2,1) (d = 2
// acosh |g_33|) and 1 in O(n,1) (d = acosh g_{n+1,n+1}). Accurate for loxodromic elements only (for a parabolic one
// the rounding leaves about 1e-3).
template <class B>
double spectral_length(const typename B::Elem& g)
{
    typename B::DMat m = B::to_double(g);
    double s = mat_norm(m), lr = std::log(s);
    mat_scale(m, 1/s);
    double w = 1;
    for (int j = 1; j <= 60; j++) {
        m = mat_mul(m, m);
        s = mat_norm(m);
        mat_scale(m, 1/s);
        w *= 0.5;
        lr += w*std::log(s);
    }
    return (std::is_same<B, PU21>::value ? 2.0 : 1.0)*lr;
}

template <class B>
ChainInfo<B> chain_info(const Side<B>& sd, const typename B::Elem& h, const typename B::Elem& g)
{
    typedef typename B::Elem Elem;
    ChainInfo<B> c;
    const int64_t bound = std::is_same<B, PU21>::value ? (1LL << 26) : (1LL << 29);
    Elem p = g;
    double dprev = 0, d = B::displacement(B::normal(p));
    int j = 0;
    for (; j < 20 && B::max_coefficient(p) < bound; j++) {
        p = B::mul(p, p);
        dprev = d;
        d = B::displacement(B::normal(p));
    }
    c.parabolic = (j == 20 && B::max_coefficient(p) < bound) || (j >= 10 && d - dprev < 3.5);
    c.len = c.parabolic ? 0 : spectral_length<B>(g);
    c.limit = B::boundary(B::act(B::to_double(B::mul(h, p)), sd.vfix));
    return c;
}

// ---------------------------------------------------------------------------------------------------------------------
// The anatomy of a leaf's stray

struct Anatomy {
    double stray = 0;                     // the farthest vertex of the refinement from the leaf as drawn, / ACC
    double lookahead = 0;                 // the farthest vertex of the first split from the chord, / ACC
    int birth = 0;                        // the level below the leaf at which the farthest vertex is born
    size_t fineleaves = 0;                // the distinct fine leaves of the refinement
    int finedepth = 0;                    // the deepest fine leaf, below the leaf
    std::vector<std::pair<int, int>> path;   // (child, letter) from the leaf down to the branch the vertex ends
    std::vector<double> profile;          // the farthest vertex born at each level (0: the ends), / ACC
    std::string pattern;                  // the path, repeated blocks as (..)^r
    int bstart = -1, bP = 0, bR = 0;      // the repeated block that covers most of the path: start, length, repeats
    double blen = -1, bdisp = -1;         // its map g: translation length, d(o, g o)
    bool bpar = false;                    // g parabolic
    double blimit = -1;                   // the limit of the chain, its distance from the leaf as drawn / ACC
    double blimpoly = -1;                 // ... and from the coarse drawing / ACC (set by the caller)
    std::vector<double> limitpt;          // the limit of the chain, a point of S^{N-1}
    std::vector<double> farpt;            // the farthest vertex of the refinement from the leaf as drawn
    bool fartip = false;                  // that vertex is a tip point of a fine leaf (the last branch of the path)
    size_t ntip = 0;                      // the points the leaf is drawn through at its tips
    double reeb = -1;                     // S^3: the share of the stray (vertex minus its foot on the chord, made
                                          // tangent at the foot) along the Reeb direction i p
    double rootdist = -1;                 // picture mode: d(o, g o) of the root
};

// the leaf (h, x) at depth d, refined at facc (each distinct branch once): the farthest vertex from the leaf as drawn
// (its chord [a, b], through its tip points if it has tips) and how it is reached. The split of a branch has k + 1
// vertices; the vertex after its child c (c < k) is born one level below the branch, and the path to it is the chain
// of branches from the leaf down to that child. A fine leaf is drawn through its own tip points as well: they are
// vertices of the refinement, born one level below that fine leaf, at the end of its path.
template <class B>
Anatomy anatomy(const Side<B>& sd, const Letters<B>& Lt, const typename B::Elem& h, int x, int d, double acc,
                double facc)
{
    typedef typename B::Elem Elem;
    struct Node { int parent, child, letter, depth; };
    std::vector<Node> nodes;
    nodes.push_back({-1, 0, x, d});
    typename B::DMat hd = B::to_double(h);
    typename B::Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[x]));
    const std::vector<typename B::Point> line = leaf_line<B>(sd, h, x, a, b);
    Anatomy A;
    A.ntip = line.size() - 2;
    for (size_t i = 1; i < sd.wv[x].size(); i++)
        A.lookahead = std::max(A.lookahead, segment_distance(B::boundary(B::act(hd, sd.wv[x][i])), a, b)/acc);
    auto birth_node = [&](int m) {                     // the branch after whose sub-arc the end vertex of m sits
        while (m != 0 && nodes[m].child == (int)sd.roof[nodes[nodes[m].parent].letter].size()) m = nodes[m].parent;
        return m;
    };
    struct Fr { Elem h; int j, depth, node; };
    std::vector<Fr> st;
    st.push_back({h, x, d, 0});
    StampSet done;
    double best = -1;
    int bestnode = 0;
    typename B::Point bestpt = a;
    while (!st.empty()) {
        Fr f = st.back();
        st.pop_back();
        if (!done.insert(branch_key<B>(Lt, f.h, f.j))) continue;
        typename B::DMat fd = B::to_double(f.h);
        typename B::Point fa = B::boundary(B::act(fd, sd.vfix)), fb = B::boundary(B::act(fd, sd.ev[f.j]));
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(fa, fb) > facc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(fd, wv[i])), fa, fb) > facc) { split = true; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, fa, fb, facc)) split = true;       // (b), as arc_points
        if (split && f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40)) {
            int base = (int)nodes.size();
            for (size_t i = 0; i < sd.roof[f.j].size(); i++)
                nodes.push_back({f.node, (int)i + 1, sd.roof[f.j][i], f.depth + 1});
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1, base + i});
        } else {
            A.fineleaves++;
            A.finedepth = std::max(A.finedepth, f.depth - d);
            int m = birth_node(f.node);
            int lev = m == 0 ? 0 : nodes[m].depth - d;
            double e = line_distance(fb, line)/acc;
            if ((int)A.profile.size() <= lev) A.profile.resize(lev + 1, 0.0);
            A.profile[lev] = std::max(A.profile[lev], e);
            if (e > best) { best = e; bestnode = m; bestpt = fb; A.fartip = false; }
            tip_points<B>(sd, f.h, f.j, [&](const typename B::Point& q) {     // the fine leaf's own tip points
                double et = line_distance(q, line)/acc;
                int lt = nodes[f.node].depth - d + 1;
                if ((int)A.profile.size() <= lt) A.profile.resize(lt + 1, 0.0);
                A.profile[lt] = std::max(A.profile[lt], et);
                if (et > best) { best = et; bestnode = f.node; bestpt = q; A.fartip = true; }
            });
        }
    }
    A.stray = best;
    A.farpt.assign(bestpt.x, bestpt.x + B::N);
    if (B::N == 4 && best > 0) {                       // the Reeb share of the stray, in S^3 = the unit sphere of C^2
        size_t sg = 0;                                 // the nearest segment of the leaf as drawn
        for (size_t i = 0; i + 1 < line.size(); i++)
            if (segment_distance(bestpt, line[i], line[i + 1]) < segment_distance(bestpt, line[sg], line[sg + 1])) sg = i;
        typename B::Point f = foot_point(bestpt, line[sg], line[sg + 1]);
        double n = std::sqrt(dot(f, f)), u[4], uu = 0, up = 0, uj = 0;
        for (int i = 0; i < 4; i++) { f.x[i] /= n; u[i] = bestpt.x[i] - f.x[i]; up += u[i]*f.x[i]; }
        double jp[4] = {-f.x[1], f.x[0], -f.x[3], f.x[2]};
        for (int i = 0; i < 4; i++) { u[i] -= up*f.x[i]; uu += u[i]*u[i]; uj += u[i]*jp[i]; }
        A.reeb = uu > 0 ? std::fabs(uj)/std::sqrt(uu) : -1;
    }
    for (int m = bestnode; m != 0; m = nodes[m].parent) A.path.push_back({nodes[m].child, nodes[m].letter});
    std::reverse(A.path.begin(), A.path.end());
    A.birth = (int)A.path.size();
    // the pattern: greedily, at each place the block (up to 8 steps) whose repetitions cover the most steps
    const std::vector<std::pair<int, int>>& p = A.path;
    int L = (int)p.size(), best_cover = 0;
    for (int i = 0; i < L;) {
        int bp = 0, br = 0;
        for (int P = 1; P <= 8 && i + 2*P <= L; P++) {
            int r = 1;
            while (i + (r + 1)*P <= L && std::equal(p.begin() + i, p.begin() + i + P, p.begin() + i + r*P)) r++;
            if (r >= 2 && r*P > br*bp) { bp = P; br = r; }
        }
        if (!A.pattern.empty()) A.pattern += " ";
        if (br) {
            std::string blk;
            for (int s = 0; s < bp; s++)
                blk += (s ? " " : "") + std::to_string(p[i + s].first) + ":" + std::to_string(p[i + s].second);
            A.pattern += (bp > 1 ? "(" + blk + ")" : blk) + "^" + std::to_string(br);
            if (br*bp > best_cover) { best_cover = br*bp; A.bstart = i; A.bP = bp; A.bR = br; }
            i += br*bp;
        } else {
            A.pattern += std::to_string(p[i].first) + ":" + std::to_string(p[i].second);
            i++;
        }
    }
    if (A.bR >= 2) {                                   // the block's map, from its second repetition (a self-map)
        int s0 = A.bstart + A.bP, y = x;
        Elem H = h, g = B::identity();
        for (int s = 0; s < s0; s++) { H = B::mul(H, sd.off[y][p[s].first - 1]); y = p[s].second; }
        for (int s = s0; s < s0 + A.bP; s++) { g = B::mul(g, sd.off[y][p[s].first - 1]); y = p[s].second; }
        ChainInfo<B> ci = chain_info<B>(sd, H, g);
        A.blen = ci.len;
        A.bpar = ci.parabolic;
        A.bdisp = B::displacement(B::normal(g));
        A.blimit = line_distance(ci.limit, line)/acc;
        A.limitpt.assign(ci.limit.x, ci.limit.x + B::N);
    }
    if (A.path.empty()) A.pattern = "-";
    return A;
}

// ---------------------------------------------------------------------------------------------------------------------
// The worst leaves of a side

template <class B>
struct Worst {
    double key = -1;                       // its farthest fine vertex from the coarse drawing, / ACC
    double own = -1;                       // its leaf stray, / ACC
    size_t order = 0;                      // the leaf's number (in the order of the coarse drawing)
    typename B::Elem h;
    int x = -1, depth = 0, child = 0, parent = -1, kof = 0, arc = -1, root = -1;
    double chordlen = 0;                   // / ACC
    SpherePoint<B::N> v, foot, a, b;       // the farthest fine vertex, its nearest coarse point; the leaf's chord
    std::vector<SpherePoint<B::N>> line;   // the leaf as drawn: a, its tip points, b
    Anatomy an;
};

// keep the n largest (by key, then the earlier order)
template <class W>
void offer(std::vector<W>& top, size_t n, const W& w)
{
    auto better = [](const W& p, const W& q) { return p.key != q.key ? p.key > q.key : p.order < q.order; };
    if (top.size() == n && !better(w, top.back())) return;
    top.insert(std::upper_bound(top.begin(), top.end(), w, better), w);
    if (top.size() > n) top.pop_back();
}

// the list top, with the anatomy of each; byown: top is ordered by the leaf strays
template <class B>
void print_worst(const char* title, const std::vector<Worst<B>>& top, bool picture, bool byown = false)
{
    printf("  %s\n", title);
    int r = 0;
    for (const Worst<B>& w : top) {
        const Anatomy& A = w.an;
        if (byown) printf("    %2d. leaf stray %.4g ACC:", ++r, w.own);
        else printf("    %2d. %.4g ACC from the coarse drawing (leaf stray %.4g):", ++r, w.key, w.own);
        if (picture) printf(" root %d (d = %.3f), letter %d,", w.root, A.rootdist, w.arc);
        else printf(" arc %d,", w.arc);
        if (w.parent >= 0)
            printf(" leaf #%zu: letter %d at depth %d, child %d of %d of letter %d", w.order, w.x, w.depth, w.child, w.kof,
                   w.parent);
        else printf(" leaf #%zu: letter %d at depth %d (a root)", w.order, w.x, w.depth);
        printf("; chord %.3g ACC, first split within %.3g ACC", w.chordlen, A.lookahead);
        if (A.ntip) printf("; drawn through %zu point%s at tips", A.ntip, A.ntip > 1 ? "s" : "");
        printf("\n        refinement: %zu fine leaves, %d levels; its farthest vertex (%.4g) is born %d levels below, in"
               " child %d; path %s%s\n", A.fineleaves, A.finedepth, A.stray, A.birth,
               A.path.empty() ? 0 : A.path[0].first, A.pattern.c_str(),
               A.fartip ? " (a tip point of the last branch)" : "");
        if (A.bR >= 2) {
            printf("        longest repeated block: %d step%s x %d from level %d; its map g: ", A.bP,
                   A.bP > 1 ? "s" : "", A.bR, A.bstart + 1);
            if (A.bpar) printf("parabolic");
            else printf("translation length %.4g", A.blen);
            printf(", d(o, g o) = %.4g; the limit of the chain is %.4g ACC from the leaf as drawn, %.4g from the"
                   " coarse drawing\n", A.bdisp, A.blimit, A.blimpoly);
        }
        printf("        farthest by level:");
        for (size_t l = 1; l < A.profile.size(); l++) printf(" %.3g", A.profile[l]);
        if (A.reeb >= 0) printf("; Reeb share of the stray %.3f", A.reeb);
        printf("\n");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Pictures

template <int N>
bool unit_against(SpherePoint<N>& v, const std::vector<SpherePoint<N>>& basis)
{
    for (const SpherePoint<N>& e : basis) { double d = dot(v, e); for (int i = 0; i < N; i++) v.x[i] -= d*e.x[i]; }
    double n = std::sqrt(dot(v, v));
    if (n < 1e-9) return false;
    for (int i = 0; i < N; i++) v.x[i] /= n;
    return true;
}

// an orthonormal frame of R^N: c, then d1 and d2 made orthogonal (if they are independent), then the rest
template <int N>
std::vector<SpherePoint<N>> frame_at(const SpherePoint<N>& c, const SpherePoint<N>& d1, const SpherePoint<N>& d2)
{
    std::vector<SpherePoint<N>> F = {c};
    SpherePoint<N> u = d1, w = d2;
    if (unit_against(u, F)) F.push_back(u);
    if (unit_against(w, F)) F.push_back(w);
    for (int e = 0; e < N && (int)F.size() < N; e++) {
        SpherePoint<N> v;
        for (int i = 0; i < N; i++) v.x[i] = i == e ? 1 : 0;
        if (unit_against(v, F)) F.push_back(v);
    }
    return F;
}

// a ring of radius rpx pixels about p, in a picture of the given size: a small circle of S^{N-1} whose image is a
// circle on the screen (its tangent directions are those that the projection takes to the screen's x and y)
template <int N>
std::vector<SpherePoint<N>> ring(const typename View<N>::V& v, const SpherePoint<N>& p, double rpx, int size)
{
    std::vector<SpherePoint<N>> out;
    double e0[4];
    if (!View<N>::eye(v, p, e0)) return out;
    std::vector<SpherePoint<N>> T = frame_at(p, p, p);  // p, then a basis of its tangent space
    T.erase(T.begin());
    const int M = N - 1;                                 // outputs: x, y, depth (and the hidden coordinate)
    double J[4][4], eps = 1e-4;
    for (int m = 0; m < M; m++) {
        SpherePoint<N> q;
        for (int i = 0; i < N; i++) q.x[i] = p.x[i] + eps*T[m].x[i];
        double e1[4];
        if (!View<N>::eye(v, q, e1)) return out;
        for (int r = 0; r < M; r++) J[r][m] = (e1[r] - e0[r])/eps;
    }
    double X[2][4];                                      // solve J a = e_x and J b = e_y (Gauss-Jordan)
    for (int t = 0; t < 2; t++) {
        double A[4][5];
        for (int r = 0; r < M; r++) { for (int m = 0; m < M; m++) A[r][m] = J[r][m]; A[r][M] = r == t ? 1 : 0; }
        for (int c = 0; c < M; c++) {
            int pv = c;
            for (int r = c + 1; r < M; r++) if (std::fabs(A[r][c]) > std::fabs(A[pv][c])) pv = r;
            for (int m = 0; m <= M; m++) std::swap(A[c][m], A[pv][m]);
            if (std::fabs(A[c][c]) < 1e-12) return out;
            for (int r = 0; r < M; r++)
                if (r != c) { double f = A[r][c]/A[c][c]; for (int m = c; m <= M; m++) A[r][m] -= f*A[c][m]; }
        }
        for (int m = 0; m < M; m++) X[t][m] = A[m][M]/A[m][m];
    }
    double reye = rpx*2*View<N>::half(v)/size;          // pixels -> eye units
    for (int s = 0; s <= 48; s++) {
        double th = 2*s3::PI*s/48, cc = std::cos(th)*reye, ss = std::sin(th)*reye;
        SpherePoint<N> q = p;
        for (int m = 0; m < M; m++) for (int i = 0; i < N; i++) q.x[i] += (cc*X[0][m] + ss*X[1][m])*T[m].x[i];
        double n = std::sqrt(dot(q, q));
        for (int i = 0; i < N; i++) q.x[i] /= n;
        out.push_back(q);
    }
    return out;
}

// polylines in the form the painters read (the points and polylines of a Renderer, which holds nothing else here)
template <class B>
struct Drawing {
    Renderer<B> r;
    Drawing(const Side<B>& sd, const Letters<B>& L) : r(sd, L, 1, 1) {}
    void add(const std::vector<SpherePoint<B::N>>& pl)
    {
        if (pl.size() < 2) return;
        r.starts.push_back((uint32_t)(r.pts.size()/B::N));
        for (const SpherePoint<B::N>& p : pl) for (int i = 0; i < B::N; i++) r.pts.push_back((float)p.x[i]);
    }
    template <class T>
    void add(const Lines<B::N, T>& l)
    {
        for (size_t k = 0; k < l.np; k++) {
            if (l.pend(k) - l.starts[k] < 2) continue;
            r.starts.push_back((uint32_t)(r.pts.size()/B::N));
            for (size_t g = l.starts[k]; g < l.pend(k); g++)
                for (int i = 0; i < B::N; i++) r.pts.push_back((float)l.x[(size_t)B::N*g + i]);
        }
    }
};

// the close-up painter: the view's projection (View<N>::eye) and the shared lines and PNG writer (output/), in flat
// colours, each layer with its own brush, supersampled 2x2
template <int N>
struct Flat {
    struct Layer { const Renderer<typename View<N>::B>* r; uint8_t c[3]; int th; };
    static bool paint(const char* fn, int size, const typename View<N>::V& v, const std::vector<Layer>& layers)
    {
        const int SS = 2, w = size*SS;
        std::vector<uint8_t> img((size_t)w*w*3, 255);
        double scale = (w/2.0)/View<N>::half(v);
        for (const Layer& L : layers) {
            const auto& r = *L.r;
            for (size_t k = 0; k < r.npoly(); k++) {
                double e0[4], e1[4];
                SpherePoint<N> p;
                auto at = [&](size_t g) { for (int i = 0; i < N; i++) p.x[i] = r.pts[(size_t)N*g + i]; return p; };
                bool ok0 = View<N>::eye(v, at(r.pbegin(k)), e0);
                for (size_t g = r.pbegin(k) + 1; g < r.pend(k); g++) {
                    bool ok1 = View<N>::eye(v, at(g), e1);
                    if (ok0 && ok1) {
                        double x0 = w/2.0 + e0[0]*scale, y0 = w/2.0 - e0[1]*scale;
                        double x1 = w/2.0 + e1[0]*scale, y1 = w/2.0 - e1[1]*scale;
                        if (std::max(x0, x1) > -w && std::min(x0, x1) < 2*w && std::max(y0, y1) > -w &&
                            std::min(y0, y1) < 2*w)
                            raster::line_rgb(img, w, w, (int)std::lround(x0), (int)std::lround(y0),
                                             (int)std::lround(x1), (int)std::lround(y1), L.c, L.th);
                    }
                    std::memcpy(e0, e1, sizeof e0);
                    ok0 = ok1;
                }
            }
        }
        return png::write_rgb(fn, raster::downsample_rgb(img, w, size, SS), size, size);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// One side

template <class B>
struct SideResult {
    const char* name = "";
    std::vector<float> coarse;                 // the coarse drawing (for the overview)
    std::vector<uint32_t> cstarts;
    std::vector<Worst<B>> top;
};

const char* side_file(int s) { return s == 1 ? "Zplus" : "Zminus"; }

// the close-up of the worst leaf w: fine and coarse drawings, the leaf's refinement and the leaf as drawn (its chord, or
// its polyline through its tips), the stray vertex ringed
template <class B, class TC, class TF>
void closeup(const Options& o, const Side<B>& sd, const Letters<B>& L, int s, const Worst<B>& w,
             const Lines<B::N, TC>& coarse, const Lines<B::N, TF>& fine)
{
    constexpr int N = B::N;
    if (o.closeup.empty() || w.key < 0) return;
    std::string fn = o.closeup + "_" + side_file(s) + ".png";
    // centred on the leaf's chord and the stray; x along the chord, y towards the stray
    SpherePoint<N> c, d1, d2;
    for (int i = 0; i < N; i++) {
        c.x[i] = (w.a.x[i] + w.b.x[i] + w.v.x[i] + w.foot.x[i])/4;
        d1.x[i] = w.b.x[i] - w.a.x[i];
        d2.x[i] = w.v.x[i] - w.foot.x[i];
    }
    double cn = std::sqrt(dot(c, c));
    for (int i = 0; i < N; i++) c.x[i] /= cn;
    double ext = std::max({chord(w.a, w.b), chord(w.v, w.foot), chord(w.a, w.v), chord(w.b, w.v)});
    double H = std::max(0.8*ext, 1.5*o.acc);
    typename View<N>::V v;
    std::vector<SpherePoint<N>> Fr = frame_at(c, d1, d2);
    View<N>::frame(v, Fr, H);
    Drawing<B> fd(sd, L), cd(sd, L), own(sd, L), ch(sd, L), rg(sd, L), sp(sd, L), bar(sd, L);
    fd.add(fine);
    cd.add(coarse);
    std::vector<SpherePoint<N>> pl;                     // the leaf's own refinement (each distinct branch once)
    StampSet done;
    walk<B>(sd, L, w.h, w.x, w.depth, o.acc/o.k, &done,
            [&](const typename B::Elem& fh, int fj, int, int, int, const SpherePoint<N>& a, const SpherePoint<N>& b,
                bool) {
                if (pl.empty()) pl.push_back(a);
                tip_points<B>(sd, fh, fj, [&](const SpherePoint<N>& q) { pl.push_back(q); });
                pl.push_back(b);
            }, [&]() { own.add(pl); pl.clear(); });
    own.add(pl);
    ch.add(w.line);                                     // the leaf as drawn: its chord, or through its tips
    for (double rpx : {10.0, 11.0}) rg.add(ring<N>(v, w.v, rpx, o.closesize));
    typename B::DMat hd = B::to_double(w.h);           // the vertices of the leaf's first split: small green rings
    for (size_t i = 1; i < sd.wv[w.x].size(); i++)
        sp.add(ring<N>(v, B::boundary(B::act(hd, sd.wv[w.x][i])), 4, o.closesize));
    auto at = [&](double x, double y) {                // the point x F1 + y F2 near the centre, on the sphere
        SpherePoint<N> q;
        for (int i = 0; i < N; i++) q.x[i] = Fr[0].x[i] + x*Fr[1].x[i] + y*Fr[2].x[i];
        double n = std::sqrt(dot(q, q));
        for (int i = 0; i < N; i++) q.x[i] /= n;
        return q;
    };
    double bx = -0.85*H, by = -0.88*H, tick = 0.02*H;   // a scale bar one ACC long, at the lower left
    bar.add(std::vector<SpherePoint<N>>{at(bx, by), at(bx + o.acc, by)});
    bar.add(std::vector<SpherePoint<N>>{at(bx, by - tick), at(bx, by + tick)});
    bar.add(std::vector<SpherePoint<N>>{at(bx + o.acc, by - tick), at(bx + o.acc, by + tick)});
    std::vector<typename Flat<N>::Layer> layers = {
        {&fd.r, {245, 150, 40}, 2}, {&own.r, {150, 20, 20}, 1}, {&cd.r, {20, 50, 210}, 1}, {&ch.r, {0, 150, 40}, 2},
        {&sp.r, {0, 150, 40}, 0}, {&rg.r, {0, 0, 0}, 1}, {&bar.r, {0, 0, 0}, 1}};
    if (!Flat<N>::paint(fn.c_str(), o.closesize, v, layers)) {
        fprintf(stderr, "cannot write %s\n", fn.c_str());
        exit(1);
    }
    fprintf(stderr, "wrote %s (half-width %.3g on the sphere)\n", fn.c_str(), H);
}

// the drawing of the arc (1, j) at acc, as polylines (with done, a new one after each skipped branch), each leaf through
// its tip points, as arc_points draws it
template <class B, class T>
void arc_drawing(const Side<B>& sd, const Letters<B>& L, int j, double acc, StampSet* done, std::vector<T>& X,
                 std::vector<uint32_t>& st)
{
    constexpr int N = B::N;
    bool open = false;
    if (done) done->clear();
    walk<B>(sd, L, B::identity(), j, 0, acc, done,
            [&](const typename B::Elem& h, int x, int, int, int, const SpherePoint<N>& a, const SpherePoint<N>& b,
                bool) {
                if (!open) {
                    st.push_back((uint32_t)(X.size()/N));
                    for (int i = 0; i < N; i++) X.push_back((T)a.x[i]);
                    open = true;
                }
                tip_points<B>(sd, h, x, [&](const SpherePoint<N>& q) { for (int i = 0; i < N; i++) X.push_back((T)q.x[i]); });
                for (int i = 0; i < N; i++) X.push_back((T)b.x[i]);
            }, [&]() { open = false; });
}

// ARCS MODE: the base arcs of the side s (1: Z+, 2: Z-)
template <class P>
void arcs_side(const Options& o, World<typename P::B>& W, int s, SideResult<typename P::B>& res)
{
    typedef typename P::B B;
    typedef typename B::Elem Elem;
    constexpr int N = B::N;
    typedef SpherePoint<N> Pt;
    const Side<B>& sd = s == 1 ? W.plus : W.minus;
    const double acc = o.acc, facc = o.acc/o.k;
    const size_t cap = o.cap ? o.cap : (size_t)1e7;
    res.name = sd.name;
    double t0 = now();
    Dist fc, cf, ls;                                   // fine -> coarse, coarse -> fine, leaf strays
    size_t ncl = 0, nfl = 0, ncf = 0, nff = 0, narcs = 0, ncheck = 0, nbad = 0;
    int cdepth = 0, fdepth = 0;
    struct Row { int j; size_t cl, fl; double fc, cf, ls; int depth; };
    std::vector<Row> rows;
    std::vector<Worst<B>> top, topown;
    size_t order = 0;
    GridCheck kfc, kcf;                                // --checkgrid
    if (o.checkgrid) { kfc.stride = 4999; kcf.stride = 499; }
    OneLetter one;
    StampSet dc, df;                                   // --dedup: the branches of the arc, of one leaf's refinement
    StampSet* pdc = o.dedup ? &dc : nullptr;
    StampSet* pdf = o.dedup ? &df : nullptr;
    for (size_t j = 0; j < W.L.s.size(); j++) {
        if (!W.L.plus[j]) continue;
        narcs++;
        double ta = now();
        // the coarse drawing of the arc
        std::vector<double> C;
        std::vector<uint32_t> cst;
        arc_drawing<B>(sd, W.L, (int)j, acc, pdc, C, cst);
        Lines<N, double> cl_lines = lines_of<N>(C, cst);
        if (cl_lines.nv - cl_lines.np > cap) {
            fprintf(stderr, "strays: arc %zu has %zu coarse leaves, more than --cap %zu: stopping%s\n", j,
                    cl_lines.nv - cl_lines.np, cap, o.dedup ? "" : " (--dedup draws each distinct branch once)");
            exit(3);
        }
        SegGrid<N, double> gc(cl_lines, acc);
        // the same walk again: the refinement of each coarse leaf, and the distances of its vertices
        std::vector<float> F;
        std::vector<uint32_t> fst;
        Row row = {(int)j, 0, 0, 0, 0, 0, 0};
        bool copen = false, fopen = false;
        size_t cv = 0;                                 // the coarse vertices so far, as arc_drawing numbered them
        if (pdc) dc.clear();
        walk<B>(sd, W.L, B::identity(), (int)j, 0, acc, pdc,
                [&](const Elem& h, int x, int depth, int child, int parent, const Pt& a, const Pt& b, bool forced) {
            if (!copen) { cv++; copen = true; fopen = false; }
            const std::vector<Pt> line = leaf_line<B>(sd, h, x, a, b);      // the leaf as drawn
            const uint32_t seg0 = (uint32_t)(cv - 1);  // its segments: from vertex seg0 to seg0 + line.size() - 1
            const uint32_t nseg = (uint32_t)line.size() - 1;
            cv += nseg;
            row.cl++; ncf += forced; cdepth = std::max(cdepth, depth);
            double own = 0, worst = -1;
            Pt wv = a, wf = a;
            auto vertex = [&](const Pt& v) {
                for (int i = 0; i < N; i++) F.push_back((float)v.x[i]);
                double e = line_distance(v, line);
                own = std::max(own, e);
                uint32_t g = seg0;                     // (the stored segments: their ends are rounded differently)
                double ub = INF;
                for (uint32_t k = seg0; k < seg0 + nseg; k++) {
                    double dk = segment_distance(v, cl_lines.point(k), cl_lines.point(k + 1));
                    if (dk < ub) { ub = dk; g = k; }
                }
                double d = gc.nearest(v, ub, g);
                kfc.check(gc, v, d);
                fc.add(d/acc);
                if (d > worst) { worst = d; wv = v; wf = foot_point(v, cl_lines.point(g), cl_lines.point(g + 1)); }
            };
            if (pdf) df.clear();
            walk<B>(sd, W.L, h, x, depth, facc, pdf,
                    [&](const Elem& fh, int fx, int fdep, int, int, const Pt& fa, const Pt& fb, bool fforced) {
                        if (!fopen) { fst.push_back((uint32_t)(F.size()/N)); vertex(fa); fopen = true; }
                        tip_points<B>(sd, fh, fx, vertex);
                        vertex(fb);
                        row.fl++; nff += fforced; fdepth = std::max(fdepth, fdep);
                        if (row.fl % (1u << 20) == 0) memcheck(o, "a fine arc");
                    }, [&]() { fopen = false; });
            if (row.fl > cap) {
                fprintf(stderr, "strays: arc %zu has more than --cap %zu fine leaves: stopping%s\n", j, cap,
                        o.dedup ? "" : " (--dedup draws each distinct branch once)");
                exit(3);
            }
            ls.add(own/acc);
            if (sd.roof[x].size() == 1) one.add(own/acc, worst/acc);
            row.ls = std::max(row.ls, own/acc);
            if (worst/acc > row.fc) { row.fc = worst/acc; row.depth = depth; }
            Worst<B> w;
            w.key = worst/acc; w.own = own/acc; w.order = order++; w.h = h; w.x = x; w.depth = depth; w.child = child;
            w.parent = parent; w.kof = parent >= 0 ? (int)sd.roof[parent].size() : 0; w.arc = (int)j;
            w.chordlen = chord(a, b)/acc; w.v = wv; w.foot = wf; w.a = a; w.b = b; w.line = line;
            offer(top, (size_t)std::max(o.worst, o.rings), w);
            w.key = own/acc;
            offer(topown, (size_t)o.worst, w);
        }, [&]() { copen = false; });
        memcheck(o, "a fine arc");
        // coarse -> fine
        Lines<N, float> fl_lines = lines_of<N>(F, fst);
        {
            SegGrid<N, float> gf(fl_lines, acc);
            for (size_t k = 0; k < cl_lines.np; k++) {
                double best = INF;
                uint32_t arg = UINT32_MAX;
                for (size_t g = cl_lines.starts[k]; g < cl_lines.pend(k); g++) {
                    Pt p = cl_lines.point(g);
                    if (arg != UINT32_MAX) best = segment_distance(p, fl_lines.point(arg), fl_lines.point(arg + 1));
                    double d = gf.nearest(p, best, arg);
                    kcf.check(gf, p, d);
                    cf.add(d/acc);
                    row.cf = std::max(row.cf, d/acc);
                }
            }
        }
        // --verify: walk against arc_points, at both accuracies (complete arcs: one polyline each)
        if (o.verify && !o.dedup) {
            std::vector<Pt> pc, pf;
            size_t f1 = 0, f2 = 0;
            arc_points(sd, B::identity(), (int)j, acc, pc, f1);
            arc_points(sd, B::identity(), (int)j, facc, pf, f2);
            bool same = pc.size() == cl_lines.nv && pf.size() == fl_lines.nv && cl_lines.np == 1 && fl_lines.np == 1;
            for (size_t i = 0; same && i < pc.size(); i++)
                for (int c = 0; c < N; c++) if (pc[i].x[c] != C[(size_t)N*i + c]) same = false;
            for (size_t i = 0; same && i < pf.size(); i++)
                for (int c = 0; c < N; c++) if ((float)pf[i].x[c] != F[(size_t)N*i + c]) same = false;
            ncheck++;
            if (!same) nbad++;
        }
        ncl += row.cl; nfl += row.fl;
        rows.push_back(row);
        fprintf(stderr, "%s arc %zu: %zu coarse leaves, %zu fine leaves (%.2f s)\n", sd.name, j, row.cl, row.fl,
                now() - ta);
        for (size_t k = 0; k < cl_lines.np; k++) {     // the coarse drawing, for the overview
            res.cstarts.push_back((uint32_t)(res.coarse.size()/N));
            for (size_t g = cl_lines.starts[k]; g < cl_lines.pend(k); g++)
                for (int i = 0; i < N; i++) res.coarse.push_back((float)C[(size_t)N*g + i]);
        }
    }
    printf("\n== %s: the %s base arcs (R = 0), ACC %.4g, fine ACC %.4g (k = %d) ==\n", sd.name,
           o.dedup ? "de-duplicated (each distinct branch of an arc once)" : "complete", acc, facc, o.k);
    printf("  arcs %zu; coarse leaves %zu (forced %zu, depth <= %d); fine leaves %zu (forced %zu, depth <= %d)\n",
           narcs, ncl, ncf, cdepth, nfl, nff, fdepth);
    if (o.verify && !o.dedup)
        printf("  verify: walk = arc_points, bit for bit, at both accuracies: %zu of %zu arcs%s\n", ncheck - nbad,
               ncheck, nbad ? "  [DIFFERENT]" : "");
    kfc.print("fine -> coarse");
    kcf.print("coarse -> fine");
    printf("  distances in multiples of ACC:\n");
    fc.print("fine -> coarse", "fine vertices");
    cf.print("coarse -> fine", "coarse vertices");
    ls.print("leaf strays", "coarse leaves");
    one.print();
    printf("  per base arc:   letter  coarse leaves  fine leaves  fine->coarse  coarse->fine  leaf stray  (depth of"
           " the worst leaf)\n");
    for (const Row& r : rows)
        printf("                  %6d  %13zu  %11zu  %12.4g  %12.3g  %10.4g  (%d)\n", r.j, r.cl, r.fl, r.fc, r.cf, r.ls,
               r.depth);
    auto finish = [&](Worst<B>& w) {                   // the anatomy; the limit of its chain, to the coarse arc
        w.an = anatomy<B>(sd, W.L, w.h, w.x, w.depth, acc, facc);
        if (w.an.limitpt.empty()) return;
        std::vector<double> C;
        std::vector<uint32_t> cst;
        arc_drawing<B>(sd, W.L, w.arc, acc, pdc, C, cst);
        SegGrid<N, double> g(lines_of<N>(C, cst), acc);
        Pt q;
        for (int i = 0; i < N; i++) q.x[i] = w.an.limitpt[i];
        uint32_t arg = UINT32_MAX;
        w.an.blimpoly = g.nearest(q, INF, arg)/acc;
    };
    for (Worst<B>& w : top) finish(w);
    std::vector<Worst<B>> shown(top.begin(), top.begin() + std::min(top.size(), (size_t)o.worst));
    print_worst<B>("the worst leaves, by their farthest fine vertex from the coarse arc:", shown, false);
    std::vector<Worst<B>> own3(topown.begin(), topown.begin() + std::min<size_t>(3, topown.size()));
    for (Worst<B>& w : own3) finish(w);
    print_worst<B>("the worst leaf strays (the farthest vertex of a leaf's refinement from its own chord):", own3,
                   false, true);
    printf("    then:");
    for (size_t i = own3.size(); i < topown.size(); i++)
        printf("  %.4g (arc %d, letter %d, depth %d)", topown[i].own, topown[i].arc, topown[i].x, topown[i].depth);
    printf("\n");
    fflush(stdout);
    res.top = top;
    if (!o.closeup.empty() && !top.empty()) {          // the close-up of the worst: its arc again, coarse and fine
        Worst<B> w = o.closeown ? own3[0] : top[0];
        if (o.closeown) {                              // the leaf's farthest vertex from its own chord
            for (int i = 0; i < N; i++) w.v.x[i] = w.an.farpt[i];
            w.foot = line_foot(w.v, w.line);
        }
        std::vector<double> C;
        std::vector<float> F;
        std::vector<uint32_t> cst, fst;
        arc_drawing<B>(sd, W.L, w.arc, acc, pdc, C, cst);
        arc_drawing<B>(sd, W.L, w.arc, facc, pdc, F, fst);
        closeup<B>(o, sd, W.L, s, w, lines_of<N>(C, cst), lines_of<N>(F, fst));
    }
    fprintf(stderr, "%s arcs: %.2f s, peak %.0f MB\n", sd.name, now() - t0, peak_mb());
}

// PICTURE MODE: the renderers' pictures of the side s at R
template <class P>
void picture_side(const Options& o, const World<typename P::B>& W0, int s, SideResult<typename P::B>& res)
{
    typedef typename P::B B;
    typedef typename B::Elem Elem;
    constexpr int N = B::N;
    typedef SpherePoint<N> Pt;
    const double acc = o.acc, facc = o.acc/o.k;
    const size_t cap = o.cap ? o.cap : (size_t)6e7;
    double t0 = now();
    World<B> Wc = W0, Wf = W0;
    // the window: the view of the programs with the options; both pictures culled to it with --cull
    typename View<N>::V view;
    View<N>::program(W0, o.view, view);
    const CullWindow<N> win = View<N>::window(view, o.cullk);
    if (o.cull) Wc.cull = Wf.cull = &win;
    auto inwin = [&](const Pt& v) { return !o.window || win.contains(v); };
    size_t nfin = 0, ncin = 0;                         // the vertices measured (in the window, with --window)
    default_marking.overlap = o.overlap;               // the coarse picture with the marking of --overlap, --reverse
    default_marking.reverse = o.reverse;
    build<P>(Wc, o.R, acc, cap, s);
    default_marking = Marking();
    build<P>(Wf, o.R, facc, cap, s);
    const Side<B>& sd = s == 1 ? Wc.plus : Wc.minus;
    const Renderer<B>& rc = *(s == 1 ? Wc.rp : Wc.rn);
    Renderer<B>& rf = *(s == 1 ? Wf.rp : Wf.rn);
    std::vector<uint64_t>().swap(rf.done.t);           // the fine renderer's keys are not needed any more
    release_memory();
    memcheck(o, "the fine picture");
    res.name = sd.name;
    Lines<N, float> C = lines_of<N>(rc.pts, rc.starts), F = lines_of<N>(rf.pts, rf.starts);
    SegGrid<N, float> gc(C, acc);
    // the leaves of the coarse picture (the leaf copy): each refined at facc (each distinct branch once), the farthest
    // vertex of its refinement from the coarse picture and from its chord
    Dist ls, lc;
    size_t nlf = 0, nlff = 0, leaf = 0;
    std::vector<Worst<B>> top, topown;
    std::vector<double> letlc(Wc.L.s.size(), 0), letcf(Wc.L.s.size(), 0), letls(Wc.L.s.size(), 0);
    StampSet df;
    OneLetter one;
    bool same = false;
    Worst<B> chosen;                                   // --closeleaf
    bool havechosen = false;
    {
        LeafRenderer<B> lr(sd, Wc.L, acc, cap);
        lr.overlap = o.overlap; lr.reverse = o.reverse;
        if (o.cull) lr.cull = &win;                    // as the renderer
        lr.hook = [&](const typename LeafRenderer<B>::Frame& f, const Pt& a, const Pt& b, bool, int root) {
            // the leaf as drawn, and its segments in the coarse picture: from vertex seg0 (the leaf is stored after
            // this call) to seg0 + line.size() - 1
            const std::vector<Pt> line = leaf_line<B>(sd, f.h, f.j, a, b);
            const uint32_t seg0 = (uint32_t)(lr.open ? lr.pts.size()/N - 1 : lr.pts.size()/N);
            const uint32_t nseg = (uint32_t)line.size() - 1;
            double own = 0, worst = -1;
            Pt wv = a, wf = a;
            bool any = false;                          // (with --window: a vertex of the refinement in the window)
            auto vertex = [&](const Pt& v) {
                if (!inwin(v)) return;
                any = true;
                double e = line_distance(v, line);
                own = std::max(own, e);
                uint32_t g = seg0;                     // (the stored segments: their ends are rounded differently)
                double ub = INF;
                for (uint32_t k = seg0; k < seg0 + nseg; k++) {
                    double dk = segment_distance(v, C.point(k), C.point(k + 1));
                    if (dk < ub) { ub = dk; g = k; }
                }
                double d = gc.nearest(v, ub, g);
                if (d > worst) { worst = d; wv = v; wf = foot_point(v, C.point(g), C.point(g + 1)); }
            };
            df.clear();
            bool first = true;
            walk<B>(sd, Wc.L, f.h, f.j, f.depth, facc, &df,
                    [&](const Elem& fh, int fx, int, int, int, const Pt& fa, const Pt& fb, bool forced) {
                        if (first) { vertex(fa); first = false; }
                        tip_points<B>(sd, fh, fx, vertex);
                        vertex(fb);
                        nlf++; nlff += forced;
                    }, NoSkip());
            if (!any) return;                          // (only with --window: no vertex in the window)
            ls.add(own/acc);
            lc.add(worst/acc);
            if (sd.roof[f.j].size() == 1) one.add(own/acc, worst/acc);
            letlc[lr.curletter] = std::max(letlc[lr.curletter], worst/acc);
            letls[lr.curletter] = std::max(letls[lr.curletter], own/acc);
            Worst<B> w;
            w.key = worst/acc; w.own = own/acc; w.order = leaf++; w.h = f.h; w.x = f.j; w.depth = f.depth;
            w.child = f.child; w.parent = f.parent; w.kof = f.parent >= 0 ? (int)sd.roof[f.parent].size() : 0;
            w.root = root; w.arc = lr.curletter; w.chordlen = chord(a, b)/acc; w.v = wv; w.foot = wf; w.a = a; w.b = b;
            w.line = line;
            if ((long)w.order == o.closeleaf[s - 1]) { chosen = w; havechosen = true; }
            offer(top, (size_t)std::max(o.worst, o.rings), w);
            w.key = own/acc;
            offer(topown, (size_t)o.worst, w);
        };
        for (size_t r = 0; r < Wc.roots.size(); r++)
            for (size_t j = 0; j < Wc.L.s.size(); j++) if (Wc.L.plus[j]) lr.draw(Wc.roots[r], (int)j, (int)r);
        same = lr.check(rc);
    }
    memcheck(o, "the leaf strays");
    // fine -> coarse: the vertices of the fine picture, to the coarse picture
    Dist fc, cf;
    GridCheck kfc, kcf;                                // --checkgrid
    if (o.checkgrid) { kfc.stride = std::max<size_t>(1, F.nv/300); kcf.stride = std::max<size_t>(1, C.nv/300); }
    for (size_t k = 0; k < F.np; k++) {
        double best = INF;
        uint32_t arg = UINT32_MAX;
        for (size_t g = F.starts[k]; g < F.pend(k); g++) {
            Pt v = F.point(g);
            if (!inwin(v)) continue;
            nfin++;
            if (arg != UINT32_MAX) best = segment_distance(v, C.point(arg), C.point(arg + 1));
            double d = gc.nearest(v, best, arg);
            kfc.check(gc, v, d);
            fc.add(d/acc);
        }
    }
    memcheck(o, "fine -> coarse");
    // coarse -> fine
    {
        SegGrid<N, float> gf(F, acc);
        for (size_t k = 0; k < C.np; k++) {
            double best = INF;
            uint32_t arg = UINT32_MAX;
            for (size_t g = C.starts[k]; g < C.pend(k); g++) {
                Pt v = C.point(g);
                if (!inwin(v)) continue;
                ncin++;
                if (arg != UINT32_MAX) best = segment_distance(v, F.point(arg), F.point(arg + 1));
                double d = gf.nearest(v, best, arg);
                kcf.check(gf, v, d);
                cf.add(d/acc);
                letcf[rc.polyletter[k]] = std::max(letcf[rc.polyletter[k]], d/acc);
            }
        }
    }
    memcheck(o, "coarse -> fine");
    auto finish = [&](Worst<B>& w) {                   // the anatomy; the limit of its chain, to the coarse picture
        w.an = anatomy<B>(sd, Wc.L, w.h, w.x, w.depth, acc, facc);
        w.an.rootdist = Wc.rootd[w.root];
        if (w.an.limitpt.empty()) return;
        Pt q;
        for (int i = 0; i < N; i++) q.x[i] = w.an.limitpt[i];
        uint32_t arg = UINT32_MAX;
        w.an.blimpoly = gc.nearest(q, INF, arg)/acc;
    };
    for (Worst<B>& w : top) finish(w);
    std::vector<Worst<B>> own3(topown.begin(), topown.begin() + std::min<size_t>(3, topown.size()));
    for (Worst<B>& w : own3) finish(w);
    printf("\n== %s: the pictures at R = %.4g (%zu roots), ACC %.4g, fine ACC %.4g (k = %d) ==\n", sd.name, o.R,
           Wc.roots.size(), acc, facc, o.k);
    if (o.overlap || o.reverse)
        printf("  the coarse picture with marking: --overlap %d%s, %zu keys marked (%zu reverses); the fine picture"
               " without\n", o.overlap, o.reverse ? " --reverse" : "", rc.nMarked, rc.nReverse);
    if (o.window) {
        char cl[160] = "";
        if (o.cull) snprintf(cl, sizeof cl, ", both pictures culled to it (K = %g): %zu and %zu branches culled", o.cullk,
                             rc.nCulled, rf.nCulled);
        printf("  the window of the view at zoom %g, clip %g (half-width %.4g)%s; the measures count only what lies in "
               "it: %zu fine vertices, %zu coarse vertices\n", o.view.zoom, o.view.clip, View<N>::half(view), cl, nfin,
               ncin);
    }
    printf("  coarse: %zu leaves, %zu polylines (spikes %zu, forced %zu, depth %d; %zu points at tips; %zu split through"
           " one-letter roofs)%s\n", rc.nLeaves, rc.npoly(), rc.nSpike, rc.nForced, rc.deepest, rc.nTipPoints, rc.nThrough,
           rc.capped ? "  [CAPPED]" : "");
    printf("  fine:   %zu leaves, %zu polylines (spikes %zu, forced %zu, depth %d; %zu points at tips; %zu split through"
           " one-letter roofs)%s\n", rf.nLeaves, rf.npoly(), rf.nSpike, rf.nForced, rf.deepest, rf.nTipPoints, rf.nThrough,
           rf.capped ? "  [CAPPED: the measurement is not valid]" : "");
    printf("  the leaf copy draws the renderer's coarse picture: %s; the refinements of its leaves: %zu distinct fine"
           " leaves (forced %zu)\n", same ? "yes, point for point" : "NO", nlf, nlff);
    kfc.print("fine -> coarse");
    kcf.print("coarse -> fine");
    printf("  distances in multiples of ACC:\n");
    fc.print("fine -> coarse", "fine vertices");
    cf.print("coarse -> fine", "coarse vertices");
    lc.print("leaf -> coarse", "coarse leaves");
    ls.print("leaf strays", "coarse leaves");
    printf("  (leaf -> coarse: for each coarse leaf, the farthest vertex of its refinement from the coarse picture)\n");
    one.print();
    printf("  per letter (of the root branch): letter  leaf->coarse  coarse->fine  leaf stray\n");
    for (size_t j = 0; j < Wc.L.s.size(); j++)
        if (Wc.L.plus[j])
            printf("                                   %6zu  %12.4g  %12.3g  %10.4g\n", j, letlc[j], letcf[j],
                   letls[j]);
    std::vector<Worst<B>> shown(top.begin(), top.begin() + std::min(top.size(), (size_t)o.worst));
    print_worst<B>("the worst leaves, by the farthest vertex of their refinement from the coarse picture:", shown,
                   true);
    print_worst<B>("the worst leaf strays (the farthest vertex of a leaf's refinement from its own chord):", own3, true,
                   true);
    printf("    then:");
    for (size_t i = own3.size(); i < topown.size(); i++)
        printf("  %.4g (root %d, letter %d, depth %d)", topown[i].own, topown[i].root, topown[i].x, topown[i].depth);
    printf("\n");
    fflush(stdout);
    res.top = top;
    res.coarse = rc.pts;
    res.cstarts = rc.starts;
    if (!top.empty()) {
        Worst<B> w = o.closeown ? own3[0] : top[0];
        if (havechosen) {                              // --closeleaf: that leaf, with its anatomy
            w = chosen;
            finish(w);
            printf("  the close-up shows leaf #%zu: %.4g ACC from the coarse drawing, leaf stray %.4g\n", w.order, w.key,
                   w.own);
            if (o.closesame) {                         // --closesame: framed as the drawing without tips frames it
                World<B> W2 = W0;
                W2.plus.tipline.clear(); W2.minus.tipline.clear();
                build<P>(W2, o.R, acc, cap, s);
                const Renderer<B>& r2 = *(s == 1 ? W2.rp : W2.rn);
                const Side<B>& sd2 = s == 1 ? W2.plus : W2.minus;
                Lines<N, float> C2 = lines_of<N>(r2.pts, r2.starts);
                SegGrid<N, float> g2(C2, acc);
                StampSet d2;
                double far2 = -1;
                bool first = true;
                auto vertex = [&](const Pt& v) {       // the same vertices, in the same order, as the hook's
                    uint32_t g = UINT32_MAX;
                    double d = g2.nearest(v, INF, g);
                    if (d > far2) { far2 = d; w.v = v; w.foot = foot_point(v, C2.point(g), C2.point(g + 1)); }
                };
                walk<B>(sd2, W2.L, w.h, w.x, w.depth, facc, &d2,
                        [&](const Elem&, int, int, int, int, const Pt& fa, const Pt& fb, bool) {
                            if (first) { vertex(fa); first = false; }
                            vertex(fb);
                        }, NoSkip());
                printf("  the close-up is framed as without tips: leaf #%zu, its farthest fine vertex %.4g ACC from the"
                       " coarse picture without tips\n", w.order, far2/acc);
                delete W2.rp; delete W2.rn;
            }
        }
        if (o.closeown) {                              // the leaf's farthest vertex from its own chord
            for (int i = 0; i < N; i++) w.v.x[i] = w.an.farpt[i];
            w.foot = line_foot(w.v, w.line);
        }
        closeup<B>(o, sd, Wc.L, s, w, C, F);
    }
    fprintf(stderr, "%s picture: %.2f s, peak %.0f MB\n", sd.name, now() - t0, peak_mb());
    delete Wc.rp; delete Wc.rn; delete Wf.rp; delete Wf.rn;  // (build leaves them to the caller)
    release_memory();
}

// the overview: the coarse drawings of the sides measured, in the program's default view, the worst strays ringed
template <class P>
void overview(const Options& o, const World<typename P::B>& W, const std::vector<SideResult<typename P::B>>& res)
{
    typedef typename P::B B;
    constexpr int N = B::N;
    if (o.png.empty()) return;
    typename View<N>::V v;
    View<N>::defaults(W, v);
    int size = o.size ? o.size : View<N>::default_size;
    std::vector<Drawing<B>> d;
    d.reserve(res.size() + 1);
    for (const SideResult<B>& r : res) {
        d.emplace_back(W.plus, W.L);
        d.back().add(lines_of<N>(r.coarse, r.cstarts));
    }
    d.emplace_back(W.plus, W.L);
    for (const SideResult<B>& r : res)
        for (size_t i = 0; i < r.top.size() && (int)i < o.rings; i++)
            for (double rpx : {10.0, 11.0, 12.0, 13.0}) d.back().add(ring<N>(v, r.top[i].v, rpx, size));
    std::vector<const Renderer<B>*> layers;
    std::vector<std::array<double, 3>> cols;
    for (size_t i = 0; i < res.size(); i++) {
        layers.push_back(&d[i].r);
        bool plus = std::string(res[i].name) == "Z+";
        cols.push_back(plus ? std::array<double, 3>{0.05, 0.15, 0.85} : std::array<double, 3>{0.85, 0.08, 0.08});
    }
    layers.push_back(&d.back().r);
    cols.push_back({0.0, 0.55, 0.0});
    std::vector<SpherePoint<N>> marks = {W.plus.V, W.minus.V};
    if (!View<N>::paint(o.png.c_str(), size, layers, cols, v, marks)) {
        fprintf(stderr, "cannot write %s\n", o.png.c_str());
        exit(1);
    }
    fprintf(stderr, "wrote %s\n", o.png.c_str());
}

// CHAINS MODE: the cycles of one and two steps in the roofs of a side (a letter x that is its own child at place i,
// or x -> y at place i and y -> x at place j): each is a chain of nested arcs, x, its child, its child's child, ...,
// whose map g (off[x][i], or off[x][i] off[y][j]) is printed parabolic or with its translation length, and with its
// fixed point (the limit of the chain from the base arc: g^n V); a place strictly inside the roof (not its first or
// last child) is marked "inner": there the arc passes through the fixed point in its interior. Cycles through a letter
// and its inverse are listed for each.
template <class P>
void chains_side(const World<typename P::B>& W, int s)
{
    typedef typename P::B B;
    typedef typename B::Elem Elem;
    constexpr int N = B::N;
    const Side<B>& sd = s == 1 ? W.plus : W.minus;
    size_t n = W.L.s.size(), npar = 0, ninner = 0, total = 0;
    printf("\n== %s: the cycles of one and two steps in the roofs ==\n", sd.name);
    auto show = [&](const std::string& what, const Elem& g, bool inner) {
        ChainInfo<B> c = chain_info<B>(sd, B::identity(), g);
        total++; npar += c.parabolic; ninner += inner;
        printf("  %-30s %-6s %s", what.c_str(), inner ? "inner" : "end", c.parabolic ? "parabolic          " : "");
        if (!c.parabolic) printf("length %-11.4g", c.len);
        printf("  d(o, g o) %.4f  fixed point", B::displacement(B::normal(g)));
        for (int i = 0; i < N; i++) printf(" % .4f", c.limit.x[i]);
        printf("\n");
    };
    for (size_t x = 0; x < n; x++) {
        const std::vector<int>& r = sd.roof[x];
        for (size_t i = 0; i < r.size(); i++) {
            int y = r[i];
            bool inner_i = i > 0 && i + 1 < r.size();
            if (y == (int)x) {
                show("letter " + std::to_string(x) + ", child " + std::to_string(i + 1) + " of " +
                     std::to_string(r.size()), sd.off[x][i], inner_i);
                continue;
            }
            if (y < (int)x) continue;                  // each two-step cycle once, from its smaller letter
            for (size_t j = 0; j < sd.roof[y].size(); j++)
                if (sd.roof[y][j] == (int)x) {
                    bool inner_j = j > 0 && j + 1 < sd.roof[y].size();
                    show("letters " + std::to_string(x) + ", " + std::to_string(y) + ", children " +
                         std::to_string(i + 1) + ", " + std::to_string(j + 1), B::mul(sd.off[x][i], sd.off[y][j]),
                         inner_i || inner_j);
                }
        }
    }
    printf("  %zu cycles: %zu parabolic, %zu through an inner child\n", total, npar, ninner);
}

template <class P>
void run(const Options& o)
{
    typedef typename P::B B;
    World<B> W;
    prepare<P>(o.subst.c_str(), W);
    if (o.notips) { W.plus.tipline.clear(); W.minus.tipline.clear(); }   // the drawing without the tips
    if (o.through >= 0) { set_lookthrough(W.plus, o.through == 1); set_lookthrough(W.minus, o.through == 1); }
    size_t np = 0;
    for (size_t j = 0; j < W.L.s.size(); j++) np += W.L.plus[j];
    printf("strays: example %s (S^%d), file %s\n", o.example.c_str(), B::N - 1, o.subst.c_str());
    printf("  |S| = %zu, base arcs %zu; t: d(o, t o) = %.4f, translation length %.4f\n", W.L.s.size(), np,
           B::displacement(B::normal(W.plus.t)), chain_info<B>(W.plus, B::identity(), W.plus.t).len);
    for (const Side<B>* sd : {&W.plus, &W.minus}) {     // the tips (render/tips.h)
        size_t nl = 0, npt = 0;
        for (const auto& l : sd->tipline) { nl += !l.empty(); npt += l.size(); }
        printf("  %s: %s", sd->name, o.notips ? "tips removed (--notips)" : "");
        if (!o.notips) printf("%zu letters drawn through tips (%zu points in their tip lines)", nl, npt);
        size_t one = 0, chained = 0;                    // the one-letter roofs, and the look-ahead through them
        for (size_t x = 0; x < sd->roof.size(); x++) {
            one += sd->roof[x].size() == 1;
            chained += !sd->through.empty() && !sd->through[x].empty();
        }
        printf("; %zu letters with a one-letter roof, the look-ahead through them %s", one,
               sd->through.empty() ? "off" : "on");
        if (!sd->through.empty()) printf(" (%zu chains, %zu cut)", chained, sd->throughcut);
        printf("\n");
    }
    if (o.mode == "chains") {
        for (int s = 1; s <= 2; s++) if (o.which & s) chains_side<P>(W, s);
        return;
    }
    if (o.mode == "picture") printf("  mode picture, R %.4g, ACC %.4g, k %d\n", o.R, o.acc, o.k);
    else printf("  mode arcs%s, ACC %.4g, k %d\n", o.dedup ? " (--dedup)" : "", o.acc, o.k);
    std::vector<SideResult<B>> res;
    for (int s = 1; s <= 2; s++) {
        if (!(o.which & s)) continue;
        res.emplace_back();
        if (o.mode == "arcs") arcs_side<P>(o, W, s, res.back());
        else picture_side<P>(o, W, s, res.back());
    }
    overview<P>(o, W, res);
}

void usage()
{
    fprintf(stderr,
"usage: strays -e cs|imm [-f FILE] [--mode arcs|picture|chains] [-R R] [-a ACC] [-k K] [--only +|-] [--dedup]\n"
"              [--verify] [--checkgrid]\n"
"              [--worst N] [--cap N] [--memlimit GB] [--png FILE] [-s SIZE] [--rings N] [--closeup PREFIX]\n"
"              [--closesize SIZE] [--closeown] [--closeleaf A[,B]] [--closesame] [--notips] [--nothrough]\n"
"              [--through] [--overlap L] [--reverse] [--zoom F] [--clip RHO] [--centre P] [--yaw D] [--pitch D]\n"
"              [--pole D1 D2] [--turn D] [--turn2 D] [--window] [--cull] [--cullk K]\n"
"  (see the comment at the top of apps/tools/strays.cc)\n");
}

}  // namespace

int main(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { usage(); exit(1); } return argv[++i]; };
        if (a == "-e") o.example = next();
        else if (a == "-f") o.subst = next();
        else if (a == "--mode") o.mode = next();
        else if (a == "-R") o.R = atof(next().c_str());
        else if (a == "-a") o.acc = atof(next().c_str());
        else if (a == "-k") o.k = atoi(next().c_str());
        else if (a == "--only") { std::string w = next(); o.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--dedup") o.dedup = true;
        else if (a == "--verify") o.verify = true;
        else if (a == "--checkgrid") o.checkgrid = true;
        else if (a == "--worst") o.worst = atoi(next().c_str());
        else if (a == "--cap") o.cap = (size_t)atof(next().c_str());
        else if (a == "--memlimit") o.memlimit = atof(next().c_str());
        else if (a == "--png") o.png = next();
        else if (a == "-s") o.size = atoi(next().c_str());
        else if (a == "--rings") o.rings = atoi(next().c_str());
        else if (a == "--closeup") o.closeup = next();
        else if (a == "--closesize") o.closesize = atoi(next().c_str());
        else if (a == "--closeown") o.closeown = true;
        else if (a == "--notips") o.notips = true;
        else if (a == "--closesame") o.closesame = true;
        else if (a == "--nothrough") o.through = 0;
        else if (a == "--through") o.through = 1;
        else if (a == "--overlap") o.overlap = atoi(next().c_str());
        else if (a == "--reverse") o.reverse = true;
        else if (a == "--zoom") o.view.zoom = atof(next().c_str());
        else if (a == "--clip") o.view.clip = atof(next().c_str());
        else if (a == "--centre") {
            std::string v = next();
            o.view.centre.clear();
            size_t p0 = 0;
            while (p0 <= v.size()) {
                size_t c = v.find(',', p0);
                if (c == std::string::npos) c = v.size();
                o.view.centre.push_back(atof(v.substr(p0, c - p0).c_str()));
                p0 = c + 1;
            }
        }
        else if (a == "--yaw") o.view.yaw = atof(next().c_str());
        else if (a == "--pitch") o.view.pitch = atof(next().c_str());
        else if (a == "--pole") { o.view.pole1 = atof(next().c_str()); o.view.pole2 = atof(next().c_str()); }
        else if (a == "--turn") o.view.turn = atof(next().c_str());
        else if (a == "--turn2") o.view.turn2 = atof(next().c_str());
        else if (a == "--window") o.window = true;
        else if (a == "--cull") o.cull = o.window = true;
        else if (a == "--cullk") { o.cullk = atof(next().c_str()); o.cull = o.window = true; }
        else if (a == "--closeleaf") {
            std::string v = next();
            size_t c = v.find(',');
            o.closeleaf[0] = atol(v.substr(0, c).c_str());
            if (c != std::string::npos) o.closeleaf[1] = atol(v.substr(c + 1).c_str());
        }
        else { usage(); return 1; }
    }
    if ((o.example != "cs" && o.example != "imm") || (o.mode != "arcs" && o.mode != "picture" && o.mode != "chains") ||
        o.acc <= 0 ||
        o.k < 1 || o.worst < 1 || o.rings < 0 || o.overlap < 0 || o.overlap > 16 ||
        ((o.overlap || o.reverse) && o.mode != "picture") || (o.window && o.mode != "picture") ||
        !(o.view.zoom > 0) || !(o.cullk >= 0)) { usage(); return 1; }
    const bool default_file = o.subst.empty();     // the programs' default
    if (default_file) o.subst = tools::default_subst(o.example == "cs");
    if (!tools::find_subst(o.subst, o.example == "cs", default_file, ZIPPERS_DATA_PLACES)) return 1;
    double t0 = now();
    if (o.example == "cs") { cs::setup(); run<cs::RenderPolicy>(o); }
    else { imm::setup(); run<imm::RenderPolicy>(o); }
    fprintf(stderr, "strays: %.2f s, peak %.0f MB\n", now() - t0, peak_mb());
    return 0;
}
