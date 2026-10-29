// SPDX-License-Identifier: GPL-3.0-or-later
//
// renderer.h: the adaptive renderer of the zipper arcs, for any backend B (geometry/backend.h).
//
// THE ARCS. A substitution file (render/substitution.h) gives a generating set S of K = ker phi (the LETTERS), an
// element t with phi(t) > 0, and for each letter s_j its ROOF: t^-1 s_j t = x_1 ... x_k with x_i in S. With V = t^+ (the
// attracting fixed point of t on the sphere at infinity), the arc alpha_j from V to s_j V is self-similar:
//     alpha_j = t ( alpha_{x_1} u x_1 alpha_{x_2} u ... u x_1...x_{k-1} alpha_{x_k} ).
// That is the zipper Z+; Z- is the same with t^-1 and the roofs t s_j t^-1 = ... A BRANCH (h, j) is the arc h alpha_j;
// it is split into the branches of its roof until its chord, and the distance of the vertices of its first split from
// the chord, are below the accuracy. The group elements are exact, and a branch is known by a fingerprint of its
// exact key (core/hash.h: B's key of h and the representative of the letter's inverse pair), so a branch reached
// twice (from two roots, or twice in one tree) is drawn once.
//
//   Letters<B>     the letters: exact elements, the index of each inverse, the representative of each inverse pair
//                  (plus) and the letter of the canonical key of a branch (canon)
//   Side<B>        one zipper: t, its attracting fixed point, the roofs, and for each letter the bases of the
//                  children (off), their key bases (keyoff), the vertices of the first split (wv) and the end of the
//                  arc (ev); and what the render policy adds (below): the points drawn through at its tips (tipline)
//                  and the chains of one-letter roofs that the look-ahead goes through (through)
//   make_side      a Side from t and the roofs, each roof checked exactly
//   set_lookthrough   turn the look-ahead through one-letter roofs on (or off) for a Side
//   through_split  that look-ahead for one branch
//   Renderer<B>    the renderer of one Side: polylines of points of S^{N-1} (as floats), with the root and the letter
//                  each polyline came from, and its counters
//
// TWO ADDITIONS, each a choice of the example's render policy, which its
// hook P::sides fills into the Side after make_side (render/world.h); the generic code chooses neither, and a Side
// that the hook has not filled has neither:
//   - TIPS (Side::tipline): a leaf whose letter has tips (render/tips.h: the cusp points of the parabolic cycles of
//     the roofs, through which the arc runs) is drawn from a through those points, mapped by the leaf's element, to
//     b. The same branches are split and the same leaves accepted, and every counter is the same (nLeaves counts
//     leaves, not points); nTipPoints counts the points added. IMM's policy computes the tips; CS's has none.
//   - THE LOOK-AHEAD THROUGH ONE-LETTER ROOFS, "(b)" (Side::through): at a letter whose roof is one letter y, the
//     first split has no inner vertex, so the look-ahead tests nothing. With (b), a branch (h, x) that the present
//     rule accepts is also split if a vertex inside the first split of its only child (h off[x][0], y), which stands
//     for the same arc with the same chord, is farther than acc from the chord; and so on while the roof is one
//     letter (through_split). Such a split counts as a spike, and in nThrough. CS's policy turns it on; IMM's leaves
//     it off.
// So the renderer below is bit for bit the legacy one only for a Side without tips and without (b).
//
// MARKING, an option of the Renderer, OFF by default (overlap = 0, reverse = false: then nothing below is
// computed, and the renderer is bit for bit what it was):
//   - overlap = L >= 1 (the S^2 program's --overlap L, apps/s2/zipper.cc: lookahead): when a leaf is accepted (not a
//     forced one), the keys of its sub-branches down to L levels go into the set of drawn branches `done`: all its
//     children, and below the first level only the children of sub-branches whose chord is at least acc/8. A marked
//     sub-branch reached again (from another root, or elsewhere in the tree) is then skipped: its curve is drawn
//     only as part of the leaf's. In S^2 the marked sub-branches are those that provably lie within acc of the
//     leaf's chord (its enclosures); here there are none, so marking is a measured trade, not a proved one
//     (docs/developer_guide.md, "Marking and overdraw"). The products below a leaf obey the renderer's growth guard: a
//     sub-branch whose element has a coefficient of 2^40 or more is not looked into.
//   - reverse = true: a branch (h, c) of an involution letter c (inv[c] = c) and its other orientation (h s_c, c) have
//     the same two ends and two keys, but they need not be the same arc: in S^4 they can be different arcs with the
//     same ends (a vertex of alpha_0 of IMM's _t1, Z+, lies 0.317 from s_0 alpha_0; docs/developer_guide.md, "The two
//     orientations of a branch"). With reverse, every
//     branch of an involution letter that the renderer visits or marks also puts the key of its other orientation
//     into `done`, so the other orientation is skipped when it is met, and real curve, drawn nowhere else, is dropped.
//     reverse is for MEASUREMENT only (apps/tools/strays, overdraw): not for
//     drawing. (Also off by default.)
//   Neither changes how a branch is split or drawn: a branch is only skipped more often. What holds without reverse is
//   a subset BY KEYS: every leaf drawn with marking has the key of a leaf drawn without it (tests/unit/test_overlap.cc,
//   and runs on all six stored files). It is not always the same leaf bit for bit: a branch (h, x) of a
//   letter that is not an involution has the key of its reversal (h s_x, x^-1), and when marking removes the route by
//   which (h, x) was first reached, it is reached later as its reversal, so its leaf is drawn from its other end (the
//   same key, the same floats in reverse order). That happens on the S^3 file _t1 (55 to 82 leaves of 400 to 600
//   thousand), and on _t3 at a coarse accuracy (16 of 1641 leaves at R = 2.554, ACC
//   0.5); not in the cases of the test at ACC 0.04 (S^3 _t3, S^4 _t1). With reverse not even the keys are exact (the
//   split decision of a branch reached from its other end can come out the other way, by rounding). nMarked counts
//   the keys put into `done` by marking (new ones only), nReverse those of the other orientations.
//   A Renderer starts with the marking in `default_marking` (below), which is off unless a program sets it: build
//   (render/world.h) makes the renderers, so `zipper3 --overlap L` and `zipper5 --overlap L` set it before build.
//
// CULLING, an option of the Renderer, OFF by default (cull = nullptr: then nothing below is computed, and the
// renderer is bit for bit what it was). With a window (render/cull.h: CullWindow, the screen square and the clip ball
// of a view pulled back to the sphere; view/s3.h window3, view/s4.h window5), each branch that draw visits is tested
// before it is split or drawn (cull_test): from its points (its ends, the vertices of its first split, its tips, and
// for a letter with a one-letter roof those of the first branch of its chain whose roof is longer) and a margin of K
// times its size. A branch whose curve misses the window is CULLED: its key stays in `done`, as for every branch
// visited, it is neither split nor drawn, and the polyline ends there (nCulled counts them); a branch whose curve lies
// inside the window passes the mark to its children, which are not tested again. So every shortcut of the renderer is
// tested with the rest: the tips are among the points (a leaf drawn through its tip reaches the tip), the look-ahead
// through one-letter roofs takes the points of the chain it looks at, a forced leaf is tested as it is visited, and
// marking only follows a leaf that was drawn. Culling, like marking, is a measured trade and not a proved one (there
// are no enclosures; docs/developer_guide.md, "Culling").
//
// Moved from legacy/s3/zipcore.h (Letters, Side, make_side, Renderer) and legacy/s4/zipcore5.h (Letters5, Side5,
// make_side5, Renderer5), which are the same code up to the arithmetic, as one template on the backend. Every
// expression is the legacy one, in the same order, so Renderer<PU21> is bit for bit the legacy Renderer and
// Renderer<ON1<5>> the legacy Renderer5 when the Side has neither (tests/unit/test_render_cs.cc, test_render_imm.cc,
// each with a render policy of its own that adds neither). Where the legacy code normalised (mnormal) the generic
// code calls B::normal, and nowhere else: the offsets and key offsets of make_side, and not the products of the
// renderer (geometry/backend.h). B::normal is the identity map in O(n,1), so that is the S^4 code too. The changes,
// apart from the names and the two additions:
//   - the root key of draw is fingerprint(B::key(g), canon[j]) and a child's fingerprint(B::product_key(h, keyoff),
//     canon[x]): what S^3's prodkey(g, mid(), ...) and prodkey(h, keyoff, ...) and S^4's fingerprint6(g, ...) and
//     fingerprint6(m6mul(h, keyoff), ...) compute;
//   - every polyline records its root and letter (polyroot, polyletter), as Renderer5 did: in S^3 too, where the
//     legacy Renderer did not record them (nothing printed or drawn reads them);
//   - Letters::plus is set by prepare (render/world.h) as j <= inv[j], as in S^4; the legacy S^3 code had j < inv[j].
//     They agree unless a letter is its own inverse, which no letter of the Cartwright-Steger S is (Pi is torsion
//     free; tests/unit/test_render_cs.cc checks it for every stored file).

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/hash.h"
#include "core/keyset.h"
#include "geometry/backend.h"
#include "geometry/sphere.h"
#include "render/cull.h"

namespace zippers {

// the letters S (legacy Letters, Letters5)
template <class B>
struct Letters {
    std::vector<typename B::Elem> s;     // the generators, exact (as B::normal leaves them)
    std::vector<int> inv;                // index of the inverse
    std::vector<char> plus;              // representative of its inverse pair
    std::vector<int> canon;              // the letter of the canonical key of a branch (h, j)
};

// one zipper: Z+ (t, up-roofs) or Z- (t^-1, down-roofs) (legacy Side, Side5)
template <class B>
struct Side {
    const char* name;
    typename B::Elem t;
    typename B::DVec vfix;                                   // the attracting fixed point of t, a null vector
    typename B::Point V;                                     // the same point of the sphere
    std::vector<std::vector<int>> roof;                      // theta(j)
    std::vector<std::vector<typename B::Elem>> off;          // the bases t u_i of the children
    std::vector<std::vector<typename B::Elem>> keyoff;       // t u_i, or t u_i s_{x_i} if x_i is not in S+: the key bases
    std::vector<std::vector<typename B::DVec>> wv;           // t u_i . vfix: the vertices of the first split
    std::vector<typename B::DVec> ev;                        // s_j . vfix: the end of alpha_j
    // filled by the render policy's hook (P::sides, render/world.h), after make_side; empty unless it fills them:
    std::vector<std::vector<typename B::DVec>> tipline;      // for each letter, the points (null vectors) a leaf of it
                                                             // is drawn through, between its ends: its tips and the
                                                             // vertices between them (render/tips.h: tip_line)
    std::vector<std::vector<int>> through;                   // for each letter with a one-letter roof, the letters
                                                             // y_1, y_2, ... of its chain of one-letter roofs, to the
                                                             // first whose roof is longer: the look-ahead (b)
    size_t throughcut = 0;                                   // chains cut at THROUGH_LIMIT steps (none can be: below)
};

// the look-ahead through one-letter roofs, (b): for each letter x of sd whose roof is one letter, the chain x -> y_1
// -> y_2 -> ... while the roof is one letter, ending at the first letter whose roof is longer (or empty if off).
// A chain of one-letter roofs cannot close up: x = y_m would give t^-m s_x t^m = s_x, so s_x would commute with t^m and
// fix its attracting point V, and alpha_x, from V to s_x V, would not be an arc. The same holds for every letter of a
// chain, so a chain passes distinct letters, and has at most as many steps as there are letters with a one-letter
// roof. (No letter of the stored files fixes V: each of them was checked.) Still the chain is cut at
// THROUGH_LIMIT steps, and the cut chains are counted in sd.throughcut (and returned).
constexpr int THROUGH_LIMIT = 64;

template <class B>
inline size_t set_lookthrough(Side<B>& sd, bool on)
{
    sd.through.assign(on ? sd.roof.size() : 0, {});
    sd.throughcut = 0;
    if (!on) return 0;
    for (size_t x = 0; x < sd.roof.size(); x++) {
        int y = (int)x;
        while (sd.roof[y].size() == 1) {
            if ((int)sd.through[x].size() == THROUGH_LIMIT) { sd.throughcut++; break; }
            y = sd.roof[y][0];
            sd.through[x].push_back(y);
        }
    }
    return sd.throughcut;
}

// (b) for the branch (h, j), whose chord is [a, b]: is a vertex inside the first split of a branch of its chain of
// one-letter roofs, (h off[j][0], y_1), (h off[j][0] off[y_1][0], y_2), ..., farther than acc from [a, b]? Each such
// branch stands for the arc of (h, j), with the same chord. The elements are the renderer's: each the exact product
// of the one before and off[.][0], as the children are made.
template <class B>
inline bool through_split(const Side<B>& sd, const typename B::Elem& h, int j, const typename B::Point& a,
                          const typename B::Point& b, double acc)
{
    typename B::Elem e = h;
    int y = j;
    for (int z : sd.through[j]) {
        e = B::mul(e, sd.off[y][0]);
        y = z;
        const std::vector<typename B::DVec>& wv = sd.wv[y];
        if (wv.size() < 2) continue;                         // a one-letter roof again: nothing inside its first split
        typename B::DMat ed = B::to_double(e);
        for (size_t i = 1; i < wv.size(); i++)
            if (segment_distance(B::boundary(B::act(ed, wv[i])), a, b) > acc) return true;
    }
    return false;
}

// the Side of t with the roofs roofIn of the representatives (legacy make_side, make_side5); exits if a roof fails
// its exact check
template <class B>
inline void make_side(Side<B>& sd, const char* name, const typename B::Elem& t, const std::vector<std::vector<int>>& roofIn,
                      const Letters<B>& L)
{
    typedef typename B::Elem Elem;
    size_t n = L.s.size();
    sd.name = name;
    sd.t = t;
    sd.roof.assign(n, {});
    for (size_t j = 0; j < n; j++) {
        if (L.plus[j]) sd.roof[j] = roofIn[j];
        else {                                  // the inverse word of the roof of s_j^-1
            const std::vector<int>& r = roofIn[L.inv[j]];
            for (int i = (int)r.size() - 1; i >= 0; i--) sd.roof[j].push_back(L.inv[r[i]]);
        }
    }
    // check every roof exactly: t^-1 s_j t = x_1 ... x_k
    Elem ti = B::inv(t);
    int bad = 0;
    for (size_t j = 0; j < n; j++) {
        Elem p = B::identity();
        for (int x : sd.roof[j]) p = B::mul(p, L.s[x]);
        if (!B::same(p, B::mul(B::mul(ti, L.s[j]), t))) bad++;
    }
    if (bad) { fprintf(stderr, "%s: %d roofs fail the exact check\n", name, bad); exit(1); }

    sd.vfix = B::attracting(t);
    sd.V = B::boundary(sd.vfix);
    sd.off.assign(n, {}); sd.keyoff.assign(n, {}); sd.wv.assign(n, {}); sd.ev.assign(n, {});
    for (size_t j = 0; j < n; j++) {
        Elem u = B::identity();
        for (int x : sd.roof[j]) {
            Elem o = B::normal(B::mul(t, u));
            sd.off[j].push_back(o);
            sd.keyoff[j].push_back(L.plus[x] ? o : B::normal(B::mul(o, L.s[x])));
            sd.wv[j].push_back(B::act(B::to_double(o), sd.vfix));
            u = B::mul(u, L.s[x]);
        }
        sd.ev[j] = B::act(B::to_double(L.s[j]), sd.vfix);
    }
}

// THE POINTS OF THE CULL TEST (render/cull.h) of the branch (h, j), whose matrix is hd and whose ends are a and b:
// verts, the vertices inside its first split (in order), and tips, the points it is drawn through at its tips (in
// order; render/tips.h); and its SIZE: the chord |a - b|, or the farthest of verts and tips from it if that is larger.
// The tips count in the size here, though not for the renderer's acceptance of a leaf: deep in a parabolic chain a
// branch's spike to its tip is long against its chord (about k against 1/k^2 at the k-th level), and its curve strays
// from its points by up to 3.3 times its size without the tips in Z- of IMM's _t1 at ACC 0.01, a ratio that grows as
// ACC shrinks (1.6, 2.5, 3.3 at 0.04, 0.02, 0.01), but by at most 1.13 times its size with them. For a letter whose roof is one letter (the look-ahead (b) above) the first split
// has no inner vertex, so verts and tips are those of the first branch of its chain of one-letter roofs whose roof is
// longer: the same arc, with the same ends, whose element is the exact product along the chain, as the renderer would
// make its children. False (and nothing set) if a product of the chain would pass the growth guard, or the chain
// THROUGH_LIMIT steps: the branch is then left undecided. apps/tools/cull.cc measures how far the curve strays from
// these points.
template <class B>
inline bool cull_points(const Side<B>& sd, const typename B::Elem& h, const typename B::DMat& hd, int j,
                        const typename B::Point& a, const typename B::Point& b, std::vector<typename B::Point>& verts,
                        std::vector<typename B::Point>& tips, double& size)
{
    int y = j;
    typename B::DMat ed;
    const typename B::DMat* e = &hd;
    if (sd.roof[j].size() == 1) {
        typename B::Elem x = h;
        for (int c = 0; sd.roof[y].size() == 1; c++) {
            if (c == THROUGH_LIMIT || B::max_coefficient(x) >= (1LL << 40)) return false;
            x = B::mul(x, sd.off[y][0]);
            y = sd.roof[y][0];
        }
        ed = B::to_double(x);
        e = &ed;
    }
    verts.clear();
    tips.clear();
    double s = chord(a, b);
    const std::vector<typename B::DVec>& wv = sd.wv[y];
    for (size_t i = 1; i < wv.size(); i++) {
        typename B::Point v = B::boundary(B::act(*e, wv[i]));
        s = std::max(s, segment_distance(v, a, b));
        verts.push_back(v);
    }
    if (!sd.tipline.empty())
        for (const typename B::DVec& v : sd.tipline[y]) {
            typename B::Point q = B::boundary(B::act(*e, v));
            s = std::max(s, segment_distance(q, a, b));
            tips.push_back(q);
        }
    size = s;
    return true;
}

// the marking a new Renderer starts with (the top of the file): off unless a program sets it before build
struct Marking {
    int overlap = 0;                              // L: levels of sub-branches of an accepted leaf marked as drawn
    bool reverse = false;                         // also the other orientation of a branch of an involution letter
};
inline Marking default_marking;

// the renderer of one Side (legacy Renderer, Renderer5)
template <class B>
struct Renderer {
    typedef typename B::Elem Elem;
    typedef typename B::Point Point;
    static constexpr int N = B::N;                // the points are in R^N

    const Side<B>& sd;
    const Letters<B>& L;
    double acc;
    size_t leafcap;
    int maxdepth = 400;
    KeySet done;

    // polylines of points of S^{N-1}; polyline k is [starts[k], starts[k+1]) (in points); each polyline also records
    // the root it came from (the rootindex of draw) and its base letter
    std::vector<float> pts;
    std::vector<uint32_t> starts;
    std::vector<int> polyroot, polyletter;
    int curroot = -1, curletter = -1;
    bool open = false, capped = false;

    size_t nVisited = 0, nSkipped = 0, nLeaves = 0, nSpike = 0, nForced = 0;
    int deepest = 0;
    size_t nTipPoints = 0, nThrough = 0;          // the points drawn at tips; the splits of (b) (also in nSpike)

    // marking (above): default_marking, which is off unless a program sets it
    int overlap = default_marking.overlap;        // L: the levels of sub-branches of an accepted leaf marked as drawn
    bool reverse = default_marking.reverse;       // also the other orientation of a branch of an involution letter
    size_t nMarked = 0, nReverse = 0;             // the keys marking put into done (new ones); of them, reverses

    // culling (the top of the file): off unless a window is set (build sets it from World::cull)
    const CullWindow<N>* cull = nullptr;
    size_t nCulled = 0, nInside = 0, nCullOpen = 0;   // branches culled; found inside the window; left undecided
                                                      // because a product would pass the growth guard

    // (kind: 0 a branch, 1 the end of a polyline (a child drawn already), 2 a branch that lies inside the cull window,
    // so it is not tested (culling only: without it, kind is 0 or 1))
    struct Frame { Elem h; int j, depth, kind; uint64_t key; };
    std::vector<Frame> stk;
    struct MarkFrame { Elem h; int j, depth; Point a, b; };
    std::vector<MarkFrame> mstk;
    std::vector<Point> cpts, cverts, ctips;       // the points of the cull test

    Renderer(const Side<B>& s, const Letters<B>& l, double a, size_t cap) : sd(s), L(l), acc(a), leafcap(cap) {}

    // the key of the other orientation (h s_j, j) of a branch (h, j) of an involution letter j, into done
    void mark_reverse(const Elem& h, int j)
    {
        if (L.inv[j] != j || B::max_coefficient(h) >= (1LL << 40)) return;
        if (done.insert(fingerprint(B::product_key(h, L.s[j]), L.canon[j]))) { ++nMarked; ++nReverse; }
    }

    // marking below the accepted leaf (h0, j0), whose chord is [a0, b0] (the top of the file): its children's keys,
    // and while depth + 1 < overlap, the children of each sub-branch whose chord is at least acc/8 (its chord is
    // computed from the vertices of its parent's split, as the S^2 look-ahead does)
    void mark(const Elem& h0, int j0, const Point& a0, const Point& b0)
    {
        if (B::max_coefficient(h0) >= (1LL << 40)) return;
        const double thr = acc/8;
        mstk.clear();
        mstk.push_back({h0, j0, 0, a0, b0});
        while (!mstk.empty()) {
            MarkFrame m = mstk.back();
            mstk.pop_back();
            const std::vector<int>& roof = sd.roof[m.j];
            const bool deeper = m.depth + 1 < overlap;
            typename B::DMat hd;
            if (deeper) hd = B::to_double(m.h);
            Point s = m.a;
            for (size_t i = 0; i < roof.size(); i++) {
                int x = roof[i];
                if (done.insert(fingerprint(B::product_key(m.h, sd.keyoff[m.j][i]), L.canon[x]))) ++nMarked;
                bool down = false;                        // look into this child: its chord is at least acc/8
                Point e = s;
                if (deeper) {
                    e = i + 1 == roof.size() ? m.b : B::boundary(B::act(hd, sd.wv[m.j][i + 1]));
                    down = chord(s, e) >= thr;
                }
                if (down || (reverse && L.inv[x] == x)) {
                    Elem hc = B::mul(m.h, sd.off[m.j][i]);
                    if (reverse) mark_reverse(hc, x);
                    if (down && B::max_coefficient(hc) < (1LL << 40)) mstk.push_back({hc, x, m.depth + 1, s, e});
                }
                s = e;
            }
        }
    }

    size_t npoly() const { return starts.size(); }
    size_t pbegin(size_t k) const { return starts[k]; }
    size_t pend(size_t k) const { return k + 1 < starts.size() ? starts[k+1] : pts.size()/N; }

    void push(const Point& p) { for (int i = 0; i < N; i++) pts.push_back((float)p.x[i]); }
    void emit(const Point& a, const Point& b)
    {
        ++nLeaves;
        if (!open) {
            starts.push_back((uint32_t)(pts.size()/N));
            polyroot.push_back(curroot); polyletter.push_back(curletter);
            push(a);
            open = true;
        }
        push(b);
        if (nLeaves >= leafcap) capped = true;
    }
    // a leaf whose letter has tips: from a through the points `line` (null vectors) mapped by hd, its element, to b
    void emit(const Point& a, const Point& b, const typename B::DMat& hd, const std::vector<typename B::DVec>& line)
    {
        ++nLeaves;
        if (!open) {
            starts.push_back((uint32_t)(pts.size()/N));
            polyroot.push_back(curroot); polyletter.push_back(curletter);
            push(a);
            open = true;
        }
        for (const typename B::DVec& v : line) push(B::boundary(B::act(hd, v)));
        nTipPoints += line.size();
        push(b);
        if (nLeaves >= leafcap) capped = true;
    }

    // the branch (g, j), from the root number rootindex
    void draw(const Elem& g, int j, int rootindex = -1)
    {
        if (capped) return;
        open = false;
        curroot = rootindex; curletter = j;
        stk.clear();
        stk.push_back({g, j, 0, 0, fingerprint(B::key(g), L.canon[j])});
        while (!stk.empty() && !capped) {
            Frame f = stk.back();
            stk.pop_back();
            if (f.kind == 1) { open = false; continue; }
            if (!done.insert(f.key)) { ++nSkipped; open = false; continue; }
            ++nVisited;
            if (reverse) mark_reverse(f.h, f.j);
            visit(f);
        }
    }

    // culling (render/cull.h): -1 if the curve of the branch (h, j), whose matrix is hd and whose ends are a and
    // b, misses the window; 1 if it lies inside it; 0 otherwise (also when cull_points leaves it undecided). The points
    // and the size are those of cull_points (above); the margin is K times the size.
    int cull_test(const Elem& h, const typename B::DMat& hd, int j, const Point& a, const Point& b)
    {
        double s;
        if (!cull_points(sd, h, hd, j, a, b, cverts, ctips, s)) { ++nCullOpen; return 0; }
        cpts.clear();
        cpts.push_back(a);
        cpts.push_back(b);
        cpts.insert(cpts.end(), cverts.begin(), cverts.end());
        cpts.insert(cpts.end(), ctips.begin(), ctips.end());
        return cull->test(cpts.data(), cpts.size(), cull->K*s);
    }

    void visit(const Frame& f)
    {
        typename B::DMat hd = B::to_double(f.h);
        Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        int kind = f.kind;                                // 2: inside the cull window (culling only)
        if (cull && kind != 2) {                          // culling (off by default)
            int c = cull_test(f.h, hd, f.j, a, b);
            if (c < 0) { ++nCulled; open = false; return; }
            if (c > 0) { kind = 2; ++nInside; }
        }
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; ++nSpike; break; }
        // (b), below the growth guard only (above it the branch could not be split, and the product would not be
        // made): the look-ahead through a one-letter roof
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, a, b, acc)) { split = true; ++nSpike; ++nThrough; }
        if (split && f.depth < maxdepth && B::max_coefficient(f.h) < (1LL << 40)) {
            deepest = std::max(deepest, f.depth + 1);
            const std::vector<int>& roof = sd.roof[f.j];
            for (int i = (int)roof.size() - 1; i >= 0; i--) {
                int x = roof[i];
                uint64_t kc = fingerprint(B::product_key(f.h, sd.keyoff[f.j][i]), L.canon[x]);
                if (done.contains(kc)) { stk.push_back({f.h, 0, 0, 1, 0}); continue; }
                stk.push_back({B::mul(f.h, sd.off[f.j][i]), x, f.depth + 1, kind, kc});
            }
        } else {
            if (split) ++nForced;
            if (sd.tipline.empty() || sd.tipline[f.j].empty()) emit(a, b);
            else emit(a, b, hd, sd.tipline[f.j]);         // through the leaf's tips
            if (overlap > 0 && !split) mark(f.h, f.j, a, b);  // marking (off by default)
        }
    }
};

}  // namespace zippers
