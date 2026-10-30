// SPDX-License-Identifier: GPL-3.0-or-later
/*  overdraw.cc  --  the ink drawn twice in the S^3 and S^4 pictures, measured two ways

    THE QUESTION. The renderer (render/renderer.h) draws a branch reached twice only once, since branches are known by
    exact keys. But distinct branches still overlap: two leaves whose curves share a piece draw its ink twice. This
    program measures how much, as was done for the S^2 program, in the pictures that
    zipper3 and zipper5 draw (render/world.h: build, from the roots of radius R, at accuracy ACC), with or without the
    renderer's marking (--overlap L, --reverse: render/renderer.h). It changes nothing in src/.

    THE INK. Every leaf of the picture (a copy of the renderer that reports its leaves, leafcopy.h, checked against the
    renderer point for point) is refined exactly to the accuracy ACC/K (K = 16 by default), by arc_points's rule
    (render/arcs.h), each distinct branch of a leaf once: its FINE PIECES. The ink of a piece is its chord; the ink of
    the picture is the sum over the leaves. The INK DRAWN TWICE is the part that more than one leaf covers: a piece
    covered by m leaves in all adds (m - 1)/m of its ink once for each of them, so (m - 1) times its ink in all; it is
    printed as a fraction of all the ink. (The S^2 program counts m/2 times the ink for m copies: the same for m = 2.)

    TWO MEASURES.
      by keys          a fine piece whose key also occurs in another leaf's refinement is covered by both (the S^2
                       program's --stats). It sees only overlaps that line up with the splitting at ACC/K. Sampled by a
                       hash of the key (1/2^S, --keyshift S, default 4), so a piece is sampled in every leaf it occurs in.
      without keys     a GENERIC POINT P of a fine piece (overdraw.h: the attracting fixed point of a closed path of the
                       roof graph through its letter, so not a vertex of the tree) lies on another leaf's curve: decided
                       by adaptive refinement of that leaf toward P, in doubles, down to the tolerance --tol (overdraw.h:
                       OnCurve). The leaves tested are those whose points pass within --prune times their size (at least
                       ACC) of P (overdraw.h: CandidateGrid). Sampled by another hash of the piece's key (1/2^S,
                       --geoshift S, default 8). The STABILITY of the answers is shown on the first --stab pieces: the
                       tolerance from 1e-6 to 1e-10, the prune factor 3 to 16, and the other generic point.
    THE CLASSES (without keys): each pair (the piece F of the leaf A, a leaf B whose curve contains its point) is
      nested           B is a branch of A's refinement above F, or A a branch of B's refinement: one leaf lies inside
                       the other (what marking removes, down to L levels);
      by keys, then    F's key is in B's refinement. Its two paths, from A down to F and from B down to F, are followed up
                       from F while they agree; X is their last common branch, P1 and P2 its two parents (in A and B):
        cancellation   P1 and P2 share an end, and X is the child of each at that end: the curve leaves the common
                       vertex along X twice, the arc backtracks (a letter and its inverse meet at a junction of
                       consecutive roofs, inside the roof of the roof);
        two keys       P1 and P2 have the same two ends and two keys: an involution letter's branch in its two
                       orientations, which in S^4 can be different arcs with the same ends (docs/developer_guide.md);
        relation       anything else: two different branches that contain the same branch X;
      two keys         not by keys, but a branch above F (or F itself) of an involution letter has its other orientation in
                       B's refinement;
      unaligned        none of these: no branch of A's refinement at or above F is a branch of B's, in either orientation.
    The ink drawn twice is split among the classes (each covering leaf's share).

    usage: overdraw -e cs|imm [options]
      -e cs|imm        the example: the Cartwright-Steger surface (S^3) or IMM's M5 (S^4)
      -f FILE          its substitution file (default subst_4.00_9.50_t3_r1.txt or subst5_2.30_4.50_t1_r0.txt, as zipper3 and
                       zipper5: the files of data/s3/ and data/s4/), found as they find theirs (src/core/data_files.h)
      -R R             the radius of the roots (default 0)
      -a ACC           the accuracy of the picture (default 0.02)
      -k K             the pieces are refined to ACC/K (default 16)
      --only +|-       one side only (default both)
      --overlap L      the picture with marking, L levels (render/renderer.h; default 0)
      --reverse        the picture with the reverse keys of involution branches (render/renderer.h)
      --contain        the picture with the containments of the containment rule marked (below; not in the renderer)
      --keyshift S     the measure by keys samples 1/2^S of the keys (default 4)
      --geoshift S     the measure without keys samples 1/2^S of the pieces (default 8)
      --tol T          the tolerance (default 1e-8);  --prune F  the prune factor (default 4)
      --budget N       the most branches one decision looks at (default 20000); past it the decision is UNDECIDED
      --stab N         the stability study on the first N pieces of the measure without keys (default 2000; 0: none)
      --nogeo          the measure by keys only
      --salt N         another sample, for both measures (another hash); each measure also prints its standard error,
                       from 8 blocks of its sample
      --png FILE       a picture of each side measured: the drawing, pale, and the fine pieces without keys found covered
                       by another leaf, dark; -s SIZE its size (default 900 for S^3, 840 for S^4)
      --cap N          the renderers' leaf cap (default 6e7)
      --memlimit GB    stop (exit status 3) when the process uses more (default 1.5)
      --mode contain   no picture: the containment rule (below) for both sides, and what it finds
      --mode involutions   no picture: for each involution letter c, the generic points of the pieces of alpha_c on
                       the curve of s_c alpha_c, and the other way round (are they the same set? docs/developer_guide.md)
      --mode pointdist no picture: for each involution letter x (or the letter --letter x), the evidence that (1, x)
                       and (s_x, x^-1) are different sets, from exact refinements (pointdist, above)
      --mode missing   the picture against the root arcs refined at ACC/K with each oriented branch once (missing,
                       above): how much of the arcs it misses where the renderer's key merges different arcs (S^4)
      --mode picture   with --png FILE: the drawing in grey, and the fine pieces whose key is in two leaves'
                       refinements or more in red (the measure by keys, --keyshift S)
      --cwords N       the containment rule's candidates: the products of at most N letters (default 2)

    THE CONTAINMENT RULE (--mode contain). For a candidate g in K (the identity, and the products
    of at most --cwords letters) and letters x, y: the arc g alpha_x lies in alpha_y if every child of (g, x) lies in a
    child of (1, y): the same branch (by its exact key, in either orientation), or again a candidate triple (h, x', y')
    with h = off[y][i]^-1 g off[x][j]. The certified triples are the greatest fixed point of this condition, computed
    exactly (it starts from the triples whose ends pass a numerical screen: g V and g s_x V within 1e-9 of the curve
    alpha_y, refined at 1e-4, and removes a triple until every remaining one has every child matched). The greatest
    fixed point is sound GIVEN THE SCREEN: if every triple (g, x, y) of a set has its children matched within the set
    AND its ends g V and g s_x V on alpha_y, each point of g alpha_x is the limit of the ends of nested branches of
    alpha_y (alpha_y is closed). The matching of the children alone does not put those ends on alpha_y: they come from
    the numerical screen of the ends (OnCurve, in doubles, not exact), and the soundness depends on it. The trivial
    triples, a branch of alpha_y's own tree, are listed apart. With --contain, the copy of the renderer marks, when it
    visits a branch (h, y), the branches (h g, x) of the non-trivial certified triples (g, x, y) as drawn: what they
    would remove is measured as above.

    THE OUTPUT (stdout), for each side: the picture's counts; the ink; the measure by keys; the measure without keys,
    with its classes, its undecided pieces, and the stability study. Timings and memory go to stderr only.

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
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/world.h"

#include "default_file.h"
#include "leafcopy.h"
#include "overdraw.h"

#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace zippers;
using namespace zippers::tools;

namespace {

const uint64_t SALT_GEO = 0x27d4eb2f165667c5ULL, SALT_PT = 0x9e3779b97f4a7c15ULL;

struct Options {
    std::string example, subst, mode = "measure", png;
    double R = 0, acc = 0.02, tol = 1e-8, prune = 4, memlimit = 1.5;
    int k = 16, which = 3, overlap = 0, keyshift = 4, geoshift = 8, stab = 2000, size = 0, cwords = 2;
    size_t budget = 20000;
    uint64_t salt = 0;                                  // --salt N: another sample (both measures)
    int letter = -1;                                    // --letter x (--mode pointdist)
    double taumin = 1e-6;                               // --taumin T (--mode pointdist): the finest tolerance
    bool reverse = false, contain = false, nogeo = false;
    size_t cap = 0;
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
        fprintf(stderr, "overdraw: %.2f GB in use (%s), more than --memlimit %.2f GB: stopping\n", b/1e9, where,
                o.memlimit);
        exit(3);
    }
}

// a few trees, the most recent kept
template <class B>
struct TreeCache {
    std::list<std::pair<uint32_t, Tree<B>>> lru;
    size_t cap = 48;
    const Tree<B>& get(uint32_t id, const Side<B>& sd, const Letters<B>& L, const LeafRec<B>& lf, double acc)
    {
        for (auto it = lru.begin(); it != lru.end(); ++it)
            if (it->first == id) { lru.splice(lru.begin(), lru, it); return lru.front().second; }
        if (lru.size() >= cap) lru.pop_back();
        lru.emplace_front(id, Tree<B>());
        tree_of<B>(sd, L, lf.h, lf.j, lf.depth, acc, lru.front().second);
        return lru.front().second;
    }
};

enum Class { NESTED = 0, CANCEL, TWOKEYS_D, RELATION, TWOKEYS, UNALIGNED, NCLASS };
const char* CLASS_NAME[NCLASS] = {"nested", "by keys: cancellation", "by keys: two keys", "by keys: relation",
                                  "two keys (not by keys)", "unaligned"};

// the class of the pair (the piece path.back() of the leaf A, the leaf B whose curve contains its point); for a
// relation, its pattern (the letters of P1 and P2 and the places of X in them)
template <class B>
Class classify(const Side<B>& sd, const Letters<B>& L, const std::vector<PathNode<B>>& path, const LeafRec<B>& A,
               const LeafRec<B>& Bl, const Tree<B>& TB, std::string& pattern)
{
    for (const PathNode<B>& q : path) if (q.key == Bl.key) return NESTED;
    if (TB.index.count(A.key)) return NESTED;
    auto it = TB.index.find(path.back().key);
    if (it != TB.index.end()) {
        int ia = (int)path.size() - 1, ib = it->second;
        while (ia > 0 && TB.parent[ib] >= 0 && path[ia - 1].key == TB.node[TB.parent[ib]].key) { ia--; ib = TB.parent[ib]; }
        if (ia == 0 || TB.parent[ib] < 0) return NESTED;
        const PathNode<B>& P1 = path[ia - 1];
        const PathNode<B>& P2 = TB.node[TB.parent[ib]];
        int pa = path[ia].place, pb = TB.node[ib].place;
        int k1 = (int)sd.roof[P1.j].size(), k2 = (int)sd.roof[P2.j].size();
        auto e1 = ends<B>(sd, P1.h, P1.j), e2 = ends<B>(sd, P2.h, P2.j);
        const double eps = 1e-9;
        bool aa = chord(e1.first, e2.first) < eps, ab = chord(e1.first, e2.second) < eps,
             ba = chord(e1.second, e2.first) < eps, bb = chord(e1.second, e2.second) < eps;
        if ((aa && bb) || (ab && ba)) return TWOKEYS_D;
        // a backtrack at a common end E: X is the m-th child from E in both, and the children from E to X are the
        // same branches in both (by their keys)
        auto childkey = [&](const PathNode<B>& Q, int i) {
            return fingerprint(B::product_key(Q.h, sd.keyoff[Q.j][i]), L.canon[sd.roof[Q.j][i]]);
        };
        auto backtrack = [&](bool s1, bool s2) {     // E is the start (true) or the end of P1, of P2
            int m1 = s1 ? pa : k1 - 1 - pa, m2 = s2 ? pb : k2 - 1 - pb;
            if (m1 != m2) return false;
            for (int q = 0; q < m1; q++)
                if (childkey(P1, s1 ? q : k1 - 1 - q) != childkey(P2, s2 ? q : k2 - 1 - q)) return false;
            return true;
        };
        if ((aa && backtrack(true, true)) || (ab && backtrack(true, false)) || (ba && backtrack(false, true)) ||
            (bb && backtrack(false, false)))
            return CANCEL;
        char buf[160];
        snprintf(buf, sizeof buf, "P1 letter %d (child %d of %d), P2 letter %d (child %d of %d)%s", P1.j, pa + 1, k1, P2.j,
                 pb + 1, k2, (aa || ab || ba || bb) ? ", a common end" : "");
        pattern = buf;
        return RELATION;
    }
    for (size_t i = path.size(); i-- > 0;) {
        const PathNode<B>& q = path[i];
        if (L.inv[q.j] != q.j || B::max_coefficient(q.h) >= (1LL << 40)) continue;
        uint64_t rk = fingerprint(B::product_key(q.h, L.s[q.j]), L.canon[q.j]);
        if (TB.index.count(rk)) return TWOKEYS;
    }
    return UNALIGNED;
}

// ---------------------------------------------------------------------------------------------------------------------
// The containment rule (the top of the file)

template <class B>
struct Containments {
    typedef typename B::Elem Elem;
    struct Triple { Elem g; int x, y; bool trivial; };
    std::vector<Triple> certified;                      // the greatest fixed point
    std::vector<std::vector<int>> byletter;             // for each y, the non-trivial certified triples (g, x, y)
    size_t candidates = 0, screened = 0, elements = 0;
    const Letters<B>* L = nullptr;
    // marking: when (h, y) is visited, its non-trivial certified triples (h g, x) go into done
    size_t nMarked = 0;
    void mark(LeafRenderer<B>& r, const Elem& h, int y)
    {
        if (B::max_coefficient(h) >= (1LL << 40)) return;
        for (int t : byletter[y]) {
            const Triple& c = certified[t];
            if (r.done.insert(branch_key<B>(*L, B::mul(h, c.g), c.x))) nMarked++;
        }
    }
};


// the containment rule for one side (the top of the file): the candidates, the screen, the greatest fixed point
template <class B>
void containments(const Options& o, const Side<B>& sd, const Letters<B>& L, Containments<B>& C, bool print)
{
    typedef typename B::Elem Elem;
    typedef typename B::Point Pt;
    const int n = (int)L.s.size();
    C.L = &L;
    // the candidates: 1 and the products of at most cwords letters, each element once (by its exact key)
    std::vector<Elem> G;
    std::vector<std::vector<int>> gword;                // a word of each candidate in the letters
    std::unordered_map<uint64_t, int> gindex;
    auto gkey = [](const Elem& g) { return fingerprint(B::key(B::normal(g)), 0); };
    auto addg = [&](const Elem& g, const std::vector<int>& w) {
        uint64_t k = gkey(g);
        if (gindex.count(k)) return false;
        gindex[k] = (int)G.size(); G.push_back(B::normal(g)); gword.push_back(w);
        return true;
    };
    addg(B::identity(), {});
    std::vector<int> layer = {0};
    for (int w = 1; w <= o.cwords; w++) {
        std::vector<int> next;
        for (int gi : layer)
            for (int x = 0; x < n; x++) {
                std::vector<int> wx = gword[gi];
                wx.push_back(x);
                if (addg(B::mul(G[gi], L.s[x]), wx)) next.push_back((int)G.size() - 1);
            }
        layer.swap(next);
    }
    C.elements = G.size();
    // the screen: g V and g s_x V on alpha_y (OnCurve, tolerance 1e-10), for the arcs alpha_y passing within 0.04 of
    // g V (a grid of the leaves of the arcs refined at 0.01, each distinct branch once); g V first, for each (g, y)
    OnCurve<B> oc(sd, L);
    oc.tol = 1e-10; oc.prune = 4;
    const double cacc = 0.01;
    CandidateGrid<B::N> grid;
    grid.h = 4*cacc;
    for (int y = 0; y < n; y++) {
        StampSet d;
        walk<B>(sd, L, B::identity(), y, 0, cacc, &d, [&](const Elem&, int, int, int, int, const Pt& a, const Pt& b, bool) {
            double lo[B::N], hi[B::N];
            for (int i = 0; i < B::N; i++) { lo[i] = std::min(a.x[i], b.x[i]); hi[i] = std::max(a.x[i], b.x[i]); }
            grid.add((uint32_t)y, lo, hi, 0);
        }, NoSkip());
    }
    grid.finish();
    struct Cand { int g, x, y; };
    std::vector<Cand> cands;
    std::vector<uint32_t> near;
    for (int gi = 0; gi < (int)G.size(); gi++) {
        typename B::DMat gd = B::to_double(G[gi]);
        Pt p1 = B::boundary(B::act(gd, sd.vfix));
        grid.query(p1.x, near);
        for (uint32_t y : near) {
            C.candidates++;
            if (oc.decide(B::identity(), (int)y, p1) != 1) continue;
            for (int x = 0; x < n; x++) {
                if (gi == 0 && x == (int)y) continue;           // alpha_y in itself
                Pt p2 = B::boundary(B::act(gd, sd.ev[x]));
                if (oc.decide(B::identity(), (int)y, p2) == 1) cands.push_back({gi, x, (int)y});
            }
        }
    }
    C.screened = cands.size();
    // the greatest fixed point: for each candidate and each child of (g, x), the children of (1, y) it can lie in
    std::map<std::array<int, 3>, int> tindex;
    for (size_t t = 0; t < cands.size(); t++) tindex[{cands[t].g, cands[t].x, cands[t].y}] = (int)t;
    std::vector<std::vector<Elem>> offinv(n);
    for (int y = 0; y < n; y++) for (const Elem& e : sd.off[y]) offinv[y].push_back(B::inv(e));
    // need[t] = for each child j, the list of options: -1 (identical branch, always good) or a candidate index
    std::vector<std::vector<std::vector<int>>> need(cands.size());
    const bool exactkeys = keys_exact(L);
    std::vector<char> alive(cands.size(), 1);
    for (size_t t = 0; t < cands.size(); t++) {
        const Cand& c = cands[t];
        const std::vector<int>& rx = sd.roof[c.x];
        const std::vector<int>& ry = sd.roof[c.y];
        need[t].assign(rx.size(), {});
        for (size_t j = 0; j < rx.size(); j++) {
            Elem gj = B::mul(G[c.g], sd.off[c.x][j]);
            for (size_t i = 0; i < ry.size(); i++) {
                Elem rel = B::normal(B::mul(offinv[c.y][i], gj));
                // the identical branch: the child (1, ry[i]) itself, or (S^3 only, where the renderer's key is exact) its
                // reversal; in S^4 the reversal by the renderer's key is another arc (overdraw.h: oriented_key)
                bool same = exactkeys ? branch_key<B>(L, rel, rx[j]) == branch_key<B>(L, B::identity(), ry[i])
                                      : oriented_key<B>(rel, rx[j]) == oriented_key<B>(B::identity(), ry[i]);
                if (same) { need[t][j].push_back(-1); continue; }
                auto gi = gindex.find(gkey(rel));
                if (gi == gindex.end()) continue;
                auto ti = tindex.find({gi->second, rx[j], ry[i]});
                if (ti != tindex.end()) need[t][j].push_back(ti->second);
            }
        }
    }
    for (bool changed = true; changed; ) {
        changed = false;
        for (size_t t = 0; t < cands.size(); t++) {
            if (!alive[t]) continue;
            bool ok = true;
            for (const std::vector<int>& opts : need[t]) {
                bool any = false;
                for (int q : opts) if (q < 0 || alive[q]) { any = true; break; }
                if (!any) { ok = false; break; }
            }
            if (!ok) { alive[t] = 0; changed = true; }
        }
    }
    C.byletter.assign(n, {});
    std::vector<int> certg;                             // the candidate index of each certified triple's g
    for (size_t t = 0; t < cands.size(); t++) {
        if (!alive[t]) continue;
        // trivial: (g, x) is (1, y) itself, or its reversal by the renderer's key (exact in S^3): already one branch
        bool trivial = exactkeys && branch_key<B>(L, G[cands[t].g], cands[t].x) == branch_key<B>(L, B::identity(), cands[t].y);
        C.certified.push_back({G[cands[t].g], cands[t].x, cands[t].y, trivial});
        certg.push_back(cands[t].g);
        if (!trivial) C.byletter[cands[t].y].push_back((int)C.certified.size() - 1);
    }
    if (!print) return;
    printf("\n== %s: the containment rule: candidates 1 and the products of at most %d letters (%zu elements) ==\n",
           sd.name, o.cwords, C.elements);
    printf("  pairs (g, y) with g V within 0.04 of alpha_y: %zu; triples (g, x, y) with both ends on alpha_y (OnCurve,"
           " tolerance 1e-10): %zu;"
           " certified (the greatest fixed point): %zu\n", C.candidates, C.screened, C.certified.size());
    // the certified triples: equalities (both g alpha_x in alpha_y and g^-1 alpha_y in alpha_x), and by the length of g
    std::map<std::array<int, 3>, int> certidx;
    for (size_t t = 0; t < C.certified.size(); t++) certidx[{certg[t], C.certified[t].x, C.certified[t].y}] = (int)t;
    size_t equal = 0, bylen[4] = {0, 0, 0, 0}, ntriv = 0, nchk = 0, nchkon = 0, nchkund = 0, badtriples = 0;
    GenericPoints<B> GP = generic_points(sd, 8, 6);
    OnCurve<B> ochk(sd, L);
    ochk.tol = 1e-10;
    for (size_t t = 0; t < C.certified.size(); t++) {
        const typename Containments<B>::Triple& c = C.certified[t];
        if (c.trivial) { ntriv++; continue; }
        auto gi = gindex.find(gkey(B::inv(c.g)));
        bool eq = gi != gindex.end() && certidx.count({gi->second, c.y, c.x});
        equal += eq;
        bylen[std::min<size_t>(3, gword[certg[t]].size())]++;
        // the check: generic points of up to 16 pieces of (g, x) at 1e-3, on the curve of (1, y)
        size_t k = 0, on = 0, und = 0, n = 0;
        StampSet d;
        walk_path<B>(sd, L, c.g, c.x, 0, 1e-3, d, [&](const std::vector<PathNode<B>>& path, const Pt&, const Pt&, bool) {
            if (n >= 16 || k++ % 5 != 0) return;
            const PathNode<B>& F = path.back();
            if (GP.p[F.j].empty()) return;
            Pt Q = B::boundary(B::act(B::to_double(F.h), GP.p[F.j][0]));
            int r = ochk.decide(B::identity(), c.y, Q);
            n++; on += r == 1; und += r < 0;
        }, !exactkeys);
        nchk += n; nchkon += on; nchkund += und;
        if (on + und < n) badtriples++;
        printf("    g alpha_%d in alpha_%d, g =%s%s, d(o, g o) = %.4f%s; check: %zu of %zu generic points on alpha_%d%s\n",
               c.x, c.y, gword[certg[t]].empty() ? " 1" : "",
               [&] { std::string w; for (int q : gword[certg[t]]) w += " s_" + std::to_string(q); return w; }().c_str(),
               B::displacement(B::normal(c.g)), eq ? " (an equality: g^-1 alpha_y in alpha_x too)" : "", on, n, c.y,
               und ? " (some undecided)" : "");
    }
    printf("  certified: %zu trivial (a branch and its reversal by the renderer's key); the others: %zu with g = 1, %zu with"
           " g a letter, %zu with g of two letters; %zu of them equalities\n", ntriv, bylen[0], bylen[1], bylen[2], equal);
    printf("  the check of the non-trivial ones: %zu generic points, %zu on the curve, %zu undecided; %zu triples with a"
           " point off the curve\n", nchk, nchkon, nchkund, badtriples);
    // the triples that passed the screen but not the fixed point: why (the first unmatched child)
    size_t shown = 0;
    for (size_t t = 0; t < cands.size() && shown < 12; t++) {
        if (alive[t]) continue;
        const Cand& c = cands[t];
        size_t j = 0;
        for (; j < need[t].size(); j++) {
            bool any = false;
            for (int q : need[t][j]) if (q < 0 || alive[q]) any = true;
            if (!any) break;
        }
        printf("    not certified: g alpha_%d in alpha_%d (d(o, g o) = %.4f): child %zu of %zu unmatched\n", c.x, c.y,
               B::displacement(B::normal(G[c.g])), j + 1, need[t].size());
        shown++;
    }
}

// the two orientations: for each involution letter c, are alpha_c and its other orientation s_c alpha_c the
// same set? The generic points of the pieces of each (at ACC/K, sampled as the measure without keys), on the other's
// curve; and how many of the pieces of each have their key in the other's refinement
template <class B>
void involutions(const Options& o, const Side<B>& sd, const Letters<B>& L)
{
    typedef typename B::Elem Elem;
    typedef typename B::Point Pt;
    GenericPoints<B> G = generic_points(sd, 8, 6);
    OnCurve<B> oc(sd, L);
    oc.tol = o.tol; oc.prune = o.prune; oc.budget = o.budget;
    const double facc = o.acc/o.k;
    const uint64_t geomask = (1ULL << o.geoshift) - 1;
    printf("\n== %s: the involution letters c: alpha_c against s_c alpha_c (the two orientations), pieces at %.4g, 1/%llu of them"
           " ==\n", sd.name, facc, (unsigned long long)(geomask + 1));
    printf("  letter  from          pieces  on the other's curve (of the ink)  undecided  key in the other's refinement"
           "  vertices > 4 ACC/K from the other  farthest (of 64 of them)\n");
    for (int c = 0; c < (int)L.s.size(); c++) {
        if (L.inv[c] != c) continue;
        for (int dir = 0; dir < 2; dir++) {
            Elem ha = dir == 0 ? B::identity() : L.s[c], hb = dir == 0 ? L.s[c] : B::identity();
            Tree<B> TB;
            tree_of<B>(sd, L, hb, c, 0, facc, TB);
            StampSet d;
            double tot = 0, cov = 0;
            size_t np = 0, und = 0, shared = 0;
            walk_path<B>(sd, L, ha, c, 0, facc, d, [&](const std::vector<PathNode<B>>& path, const Pt& a, const Pt& b, bool) {
                const PathNode<B>& F = path.back();
                if ((splitmix(oriented_key<B>(F.h, F.j) ^ SALT_GEO) & geomask) != 0 || G.p[F.j].empty()) return;
                Pt P = B::boundary(B::act(B::to_double(F.h), G.p[F.j][(size_t)(splitmix(F.key ^ SALT_PT) % G.p[F.j].size())]));
                double ch = chord(a, b);
                tot += ch; np++;
                int r = oc.decide(hb, c, P, 0);
                if (r == 1) cov += ch; else if (r < 0) und++;
                if (TB.index.count(F.key)) shared++;
            }, true);
            // the vertices of the refinement of (ha, c), to the polyline of the refinement of (hb, c): a vertex with no
            // segment in the cells of side 4 ACC/K about it is farther than 4 ACC/K; the farthest, exactly (all the
            // segments), among the first 64 such vertices
            CandidateGrid<B::N> sg;
            sg.h = 4*facc;
            std::vector<std::pair<Pt, Pt>> seg;
            {                                           // (each oriented branch once: overdraw.h, oriented_key)
                StampSet d2;
                walk_path<B>(sd, L, hb, c, 0, facc, d2, [&](const std::vector<PathNode<B>>&, const Pt& a, const Pt& b, bool) {
                    double lo[B::N], hi[B::N];
                    for (int i = 0; i < B::N; i++) { lo[i] = std::min(a.x[i], b.x[i]); hi[i] = std::max(a.x[i], b.x[i]); }
                    sg.add((uint32_t)seg.size(), lo, hi, 0);
                    seg.push_back({a, b});
                }, true);
            }
            sg.finish();
            size_t nv = 0, nfar = 0;
            double farthest = 0;
            std::vector<uint32_t> cs;
            std::vector<Pt> farv;
            StampSet d3;
            walk_path<B>(sd, L, ha, c, 0, facc, d3, [&](const std::vector<PathNode<B>>&, const Pt&, const Pt& b, bool) {
                sg.query(b.x, cs);
                double dmin = INFINITY;
                for (uint32_t q : cs) dmin = std::min(dmin, segment_distance(b, seg[q].first, seg[q].second));
                nv++;
                if (dmin > 4*facc) { nfar++; if (farv.size() < 64) farv.push_back(b); }
            }, true);
            for (const Pt& v : farv) {
                double dmin = INFINITY;
                for (const auto& q : seg) dmin = std::min(dmin, segment_distance(v, q.first, q.second));
                farthest = std::max(farthest, dmin);
            }
            printf("  %6d  %-12s %7zu  %33.2f%%  %9zu  %29zu  %26.2f%%  %.3g\n", c, dir == 0 ? "alpha_c" : "s_c alpha_c",
                   np, tot > 0 ? 100*cov/tot : 0.0, und, shared, nv ? 100.0*nfar/nv : 0.0, farthest);
        }
    }
    fflush(stdout);
}

// --mode pointdist: is the arc (1, x) the same set as (s_x, x^-1), its reversal by the renderer's key? For x an
// involution, (s_x, x) is its other orientation. The evidence, from exact refinements:
//   - the point P: the first vertex of the refinement of (1, x) at 1e-3 (each oriented branch once) found farthest
//     from the refinement of the other at 1e-3, a distance seen only up to 0.03 (the cells of the search); P = g V for
//     an exact g, so a point of the arc itself; its path and coordinates;
//   - U(tau): the nearest vertex to P of the other arc's refinement at tolerance tau (a branch is split while its size
//     exceeds tau, and dropped when its points are farther from P than the nearest vertex found plus 4 times its
//     size): vertices are points of the arc, so U(tau) bounds the distance from above, and decreases to it;
//   - lower bounds: delta is certified with the factor K when every branch of the other arc whose points pass within
//     delta + K size of P is split until none is left; then no point of the arc is within delta of P, provided no
//     branch's curve strays from its points by more than K times its size (measured: at most 2.7;
//     apps/tools/strays measures it); delta = 0.5, 0.9 and 0.99 of U at the finest tolerance (1e-6, or --taumin), K = 4, 16 and 64;
//   - the control: a point of (1, x) that lies on the other arc (a generic point that OnCurve finds on it), and its
//     U(tau), which goes to 0 with tau.
template <class B>
struct PointDist {
    typedef typename B::Elem Elem;
    typedef typename B::Point Pt;
    const Side<B>& sd;
    const Letters<B>& L;
    size_t nodes = 0;
    PointDist(const Side<B>& s, const Letters<B>& l) : sd(s), L(l) {}
    // the points of a branch (ends, vertices of its first split, tips), its size (without the tips) and distance
    void points(const Elem& h, int y, const Pt& P, double& s, double& d, double& dv) const
    {
        typename B::DMat M = B::to_double(h);
        Pt a = B::boundary(B::act(M, sd.vfix)), b = B::boundary(B::act(M, sd.ev[y]));
        s = chord(a, b);
        dv = std::min(chord(P, a), chord(P, b));
        Pt prev = a;
        d = INFINITY;
        for (size_t i = 1; i < sd.wv[y].size(); i++) {
            Pt v = B::boundary(B::act(M, sd.wv[y][i]));
            s = std::max(s, segment_distance(v, a, b));
            d = std::min(d, segment_distance(P, prev, v));
            dv = std::min(dv, chord(P, v));
            prev = v;
        }
        d = std::min(d, segment_distance(P, prev, b));
        if (!sd.tipline.empty() && !sd.tipline[y].empty()) {
            Pt q0 = a;
            for (const auto& t : sd.tipline[y]) {
                Pt q = B::boundary(B::act(M, t));
                d = std::min(d, segment_distance(P, q0, q));
                dv = std::min(dv, chord(P, q));
                q0 = q;
            }
            d = std::min(d, segment_distance(P, q0, b));
        }
    }
    // U(tau): the nearest vertex of the refinement of (h0, y0) at tau (prune factor 4)
    double nearest(const Elem& h0, int y0, const Pt& P, double tau)
    {
        double best = INFINITY;
        StampSet seen;
        struct Fr { Elem h; int y, depth; };
        std::vector<Fr> st = {{h0, y0, 0}};
        while (!st.empty()) {
            Fr f = st.back();
            st.pop_back();
            if (!seen.insert(oriented_key<B>(f.h, f.y))) continue;
            nodes++;
            double s, d, dv;
            points(f.h, f.y, P, s, d, dv);
            best = std::min(best, dv);
            if (d > best + 4*s || s <= tau || f.depth >= 400 || B::max_coefficient(f.h) >= (1LL << 40)) continue;
            for (int i = (int)sd.roof[f.y].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.y][i]), sd.roof[f.y][i], f.depth + 1});
        }
        return best;
    }
    // is delta certified with the factor K (above)? -1 if a branch passing within delta + K size reaches size 1e-12
    // or the depth limit; else the number of branches looked at
    long certify(const Elem& h0, int y0, const Pt& P, double delta, double K)
    {
        StampSet seen;
        struct Fr { Elem h; int y, depth; };
        std::vector<Fr> st = {{h0, y0, 0}};
        long n = 0;
        while (!st.empty()) {
            Fr f = st.back();
            st.pop_back();
            if (!seen.insert(oriented_key<B>(f.h, f.y))) continue;
            n++;
            double s, d, dv;
            points(f.h, f.y, P, s, d, dv);
            if (d > delta + K*s) continue;
            if (s <= 1e-12 || f.depth >= 400 || B::max_coefficient(f.h) >= (1LL << 40) || n > 50000000) return -1;
            for (int i = (int)sd.roof[f.y].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.y][i]), sd.roof[f.y][i], f.depth + 1});
        }
        return n;
    }
};

template <class B>
void pointdist(const Options& o, const Side<B>& sd, const Letters<B>& L)
{
    typedef typename B::Elem Elem;
    typedef typename B::Point Pt;
    PointDist<B> pd(sd, L);
    std::vector<int> letters;
    for (int x = 0; x < (int)L.s.size(); x++)
        if (o.letter >= 0 ? x == o.letter : (L.inv[x] == x)) letters.push_back(x);
    printf("\n== %s: (1, x) against (s_x, x^-1), from exact refinements ==\n", sd.name);
    std::vector<double> taus;
    for (double tau : {1e-2, 3e-3, 1e-3, 3e-4, 1e-4, 3e-5, 1e-5, 3e-6, 1e-6}) if (tau >= o.taumin*(1 - 1e-9)) taus.push_back(tau);
    for (int x : letters) {
        int xb = L.inv[x];
        for (int dir = 0; dir < 2; dir++) {
            Elem ha = dir == 0 ? B::identity() : L.s[x], hb = dir == 0 ? L.s[x] : B::identity();
            int ya = dir == 0 ? x : xb, yb = dir == 0 ? xb : x;
            // the vertices of the refinement of (ha, ya) at 1e-3, with their paths; the farthest from the other's
            const double t0 = 1e-3;
            struct V { Pt p; std::vector<int> path; Elem g; };
            std::vector<V> verts;
            std::vector<std::pair<Pt, Pt>> other;
            {
                StampSet d;
                walk_path<B>(sd, L, hb, yb, 0, t0, d, [&](const std::vector<PathNode<B>>&, const Pt& a, const Pt& b, bool) {
                    other.push_back({a, b}); }, true);
            }
            CandidateGrid<B::N> g;
            g.h = 0.03;
            for (size_t i = 0; i < other.size(); i++) {
                double lo[B::N], hi[B::N];
                for (int c = 0; c < B::N; c++) { lo[c] = std::min(other[i].first.x[c], other[i].second.x[c]);
                                                  hi[c] = std::max(other[i].first.x[c], other[i].second.x[c]); }
                g.add((uint32_t)i, lo, hi, 0);
            }
            g.finish();
            double far = -1;
            V best;
            std::vector<uint32_t> cq;
            {
                StampSet d;
                walk_path<B>(sd, L, ha, ya, 0, t0, d, [&](const std::vector<PathNode<B>>& path, const Pt&, const Pt& b, bool) {
                    g.query(b.x, cq);
                    double m = g.h;                      // (no segment in the cells about b: farther than 0.03)
                    for (uint32_t q : cq) m = std::min(m, segment_distance(b, other[q].first, other[q].second));
                    if (m > far) {
                        far = m;
                        best.p = b;
                        best.path.clear();
                        for (size_t k = 1; k < path.size(); k++) best.path.push_back(path[k].place);
                        best.g = B::mul(path.back().h, L.s[path.back().j]);   // b = h s_j V
                    }
                }, true);
            }
            printf("  letter %d, from (%s, %d) to (%s, %d):\n", x, dir == 0 ? "1" : "s_x", ya, dir == 0 ? "s_x" : "1", yb);
            printf("    P = g V, the end of the piece at the places");
            for (int k : best.path) printf(" %d", k + 1);
            printf(" of the refinement at 1e-3 (d(o, g o) = %.4f), the first of its vertices found farthest from the"
                   " other's refinement, at least %.5g (the cells of the search are 0.03)\n    P =",
                   B::displacement(B::normal(best.g)), far);
            for (int c = 0; c < B::N; c++) printf(" %.12f", best.p.x[c]);
            printf("\n    U(tau), the nearest vertex of the other's refinement at tau:");
            double ulast = 0;
            for (double tau : taus) {
                pd.nodes = 0;
                ulast = pd.nearest(hb, yb, best.p, tau);
                printf("  %.0e: %.6f (%zu)", tau, ulast, pd.nodes);
            }
            printf("\n    certified lower bounds (delta, K: the branches looked at; FAIL if not certified):");
            for (double f : {0.5, 0.9, 0.99})
                for (double K : {4.0, 16.0, 64.0}) {
                    long n = pd.certify(hb, yb, best.p, f*ulast, K);
                    if (n < 0) printf("  %.5f K %g FAIL", f*ulast, K);
                    else printf("  %.5f K %g (%ld)", f*ulast, K, n);
                }
            printf("\n");
            // the control: a generic point of (ha, ya) on the other arc
            GenericPoints<B> G = generic_points(sd, 8, 6);
            OnCurve<B> oc(sd, L);
            oc.tol = 1e-10;
            bool done = false;
            StampSet d;
            walk_path<B>(sd, L, ha, ya, 0, t0, d, [&](const std::vector<PathNode<B>>& path, const Pt&, const Pt&, bool) {
                if (done) return;
                const PathNode<B>& F = path.back();
                if (G.p[F.j].empty()) return;
                Pt Q = B::boundary(B::act(B::to_double(F.h), G.p[F.j][0]));
                if (oc.decide(hb, yb, Q) != 1) return;
                done = true;
                printf("    control: a generic point Q of the piece at the places");
                for (size_t k = 1; k < path.size(); k++) printf(" %d", path[k].place + 1);
                printf(", on the other arc (OnCurve, 1e-10); U(tau):");
                for (double tau : taus) printf("  %.0e: %.3g", tau, pd.nearest(hb, yb, Q, tau));
                printf("\n");
            }, true);
            if (!done) printf("    control: no generic point of a piece at 1e-3 is on the other arc\n");
            fflush(stdout);
        }
    }
}

// --mode missing: how much of the union of the root arcs the renderer's picture misses, when its key merges different
// arcs (S^4: overdraw.h, oriented_key). The picture of zipper3/zipper5 at R, ACC; every root arc refined at ACC/K, each
// branch once over the whole picture, by the oriented key (the arcs as the substitution defines them) and, as the
// control, by the renderer's key (what the renderer draws); the vertices farther than 2 ACC from the picture (no
// segment of it in the cells of side 2 ACC about them), and the farthest of the first 64 such (all segments)
template <class P>
void missing(const Options& o, const World<typename P::B>& W0, int s)
{
    typedef typename P::B B;
    typedef typename B::Point Pt;
    constexpr int N = B::N;
    const double acc = o.acc, facc = o.acc/o.k;
    World<B> W = W0;
    build<P>(W, o.R, acc, o.cap ? o.cap : (size_t)6e7, s);
    const Side<B>& sd = s == 1 ? W.plus : W.minus;
    const Renderer<B>& r = *(s == 1 ? W.rp : W.rn);
    CandidateGrid<N> g;
    g.h = 2*acc;
    std::vector<std::pair<Pt, Pt>> seg;
    for (size_t k = 0; k < r.npoly(); k++)
        for (size_t q = r.pbegin(k); q + 1 < r.pend(k); q++) {
            Pt a, b;
            for (int c = 0; c < N; c++) { a.x[c] = r.pts[N*q + c]; b.x[c] = r.pts[N*(q + 1) + c]; }
            double lo[N], hi[N];
            for (int c = 0; c < N; c++) { lo[c] = std::min(a.x[c], b.x[c]); hi[c] = std::max(a.x[c], b.x[c]); }
            g.add((uint32_t)seg.size(), lo, hi, 0);
            seg.push_back({a, b});
        }
    g.finish();
    printf("\n== %s: R = %.4g (%zu roots), ACC %.4g: the root arcs refined at ACC/%d against the picture (%zu leaves) ==\n",
           sd.name, o.R, W.roots.size(), acc, o.k, r.nLeaves);
    for (int oriented = 1; oriented >= 0; oriented--) {
        StampSet d;
        size_t nv = 0, nfar = 0;
        double far = 0, inkall = 0, inkfar = 0;
        std::vector<Pt> farv;
        std::vector<uint32_t> cq;
        for (size_t q = 0; q < W.roots.size(); q++)
            for (size_t j = 0; j < W.L.s.size(); j++) {
                if (!W.L.plus[j]) continue;
                walk_path<B>(sd, W.L, W.roots[q], (int)j, 0, facc, d, [&](const std::vector<PathNode<B>>&, const Pt& a,
                                                                           const Pt& b, bool) {
                    g.query(b.x, cq);
                    bool near = false;
                    for (uint32_t t : cq) if (segment_distance(b, seg[t].first, seg[t].second) <= 2*acc) { near = true; break; }
                    nv++;
                    inkall += chord(a, b);
                    if (!near) { nfar++; inkfar += chord(a, b); if (farv.size() < 64) farv.push_back(b); }
                }, oriented == 1);
            }
        for (const Pt& v : farv) {
            double m = INFINITY;
            for (const auto& q : seg) m = std::min(m, segment_distance(v, q.first, q.second));
            far = std::max(far, m);
        }
        printf("  %s: %zu fine pieces; %zu of their ends farther than 2 ACC from the picture (%.3f%% of the ink); the"
               " farthest of the first %zu such: %.4g ACC\n", oriented ? "each oriented branch once (the arcs)"
               : "each branch once by the renderer's key (the control)", nv, nfar, inkall > 0 ? 100*inkfar/inkall : 0.0,
               farv.size(), far/acc);
        fflush(stdout);
    }
    delete W.rp; delete W.rn;
}

// --mode picture (with --png): the drawing in grey, and the fine pieces whose key is in two leaves' refinements or more
// (the measure by keys, sampled 1/2^S, --keyshift S) in red, thicker: where the ink is drawn twice
template <class P>
void picture(const Options& o, const World<typename P::B>& W0, int s)
{
    typedef typename P::B B;
    typedef typename B::Point Pt;
    constexpr int N = B::N;
    const double acc = o.acc, facc = o.acc/o.k;
    World<B> W = W0;
    build<P>(W, o.R, acc, o.cap ? o.cap : (size_t)6e7, s);
    const Side<B>& sd = s == 1 ? W.plus : W.minus;
    const Renderer<B>& rc = *(s == 1 ? W.rp : W.rn);
    std::vector<LeafRec<B>> leaves;
    LeafRenderer<B> lr(sd, W.L, acc, o.cap ? o.cap : (size_t)6e7);
    lr.hook = [&](const typename LeafRenderer<B>::Frame& f, const Pt&, const Pt&, bool, int root) {
        leaves.push_back({f.h, f.j, f.depth, root, f.key}); };
    for (size_t r = 0; r < W.roots.size(); r++)
        for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) lr.draw(W.roots[r], (int)j, (int)r);
    struct Rec { uint64_t key; uint32_t leaf; std::array<float, 2*N> ab; };
    std::vector<Rec> recs;
    const uint64_t mask = (1ULL << o.keyshift) - 1;
    StampSet df;
    for (size_t li = 0; li < leaves.size(); li++) {
        df.clear();
        walk_path<B>(sd, W.L, leaves[li].h, leaves[li].j, leaves[li].depth, facc, df,
                     [&](const std::vector<PathNode<B>>& path, const Pt& a, const Pt& b, bool) {
            uint64_t k = path.back().key;
            if ((splitmix(k ^ SALT_KEY ^ o.salt) & mask) != 0) return;
            Rec q{k, (uint32_t)li, {}};
            for (int c = 0; c < N; c++) { q.ab[c] = (float)a.x[c]; q.ab[N + c] = (float)b.x[c]; }
            recs.push_back(q);
        });
    }
    std::sort(recs.begin(), recs.end(), [](const Rec& u, const Rec& v) { return u.key < v.key || (u.key == v.key && u.leaf < v.leaf); });
    Renderer<B> cover(sd, W.L, 1, 1);
    size_t shared = 0;
    for (size_t i = 0; i < recs.size(); ) {
        size_t j = i;
        while (j < recs.size() && recs[j].key == recs[i].key) j++;
        if (j - i >= 2) {
            shared++;
            cover.starts.push_back((uint32_t)(cover.pts.size()/N));
            for (int c = 0; c < 2*N; c++) cover.pts.push_back(recs[i].ab[c]);
        }
        i = j;
    }
    std::string fn = o.png;
    size_t dot = fn.rfind(".png");
    fn = (dot == std::string::npos ? fn : fn.substr(0, dot)) + (s == 1 ? "_Zplus.png" : "_Zminus.png");
    typename View<N>::V v;
    View<N>::defaults(W, v);
    int size = o.size ? o.size : View<N>::default_size;
    std::vector<const Renderer<B>*> layers = {&rc, &cover};
    std::vector<std::array<double, 3>> cols = {{0.62, 0.66, 0.78}, {0.80, 0.0, 0.0}};
    std::vector<SpherePoint<N>> marks = {W.plus.V, W.minus.V};
    if (!View<N>::paint(fn.c_str(), size, layers, cols, v, marks, 2)) { fprintf(stderr, "cannot write %s\n", fn.c_str()); exit(1); }
    printf("%s: R = %.4g, ACC %.4g: %zu leaves; %zu sampled keys (1/%llu) in two leaves or more, drawn in red: %s\n", sd.name,
           o.R, acc, leaves.size(), shared, (unsigned long long)(mask + 1), fn.c_str());
    delete W.rp; delete W.rn;
}

// ---------------------------------------------------------------------------------------------------------------------
// The measures of one side

template <class P>
void measure_side(const Options& o, const World<typename P::B>& W0, int s)
{
    typedef typename P::B B;
    typedef typename B::Elem Elem;
    typedef typename B::Point Pt;
    typedef typename B::DMat DMat;
    constexpr int N = B::N;
    const double acc = o.acc, facc = o.acc/o.k;
    const size_t cap = o.cap ? o.cap : (size_t)6e7;
    double t0 = now();
    // the renderer's picture, with the marking of the options
    World<B> W = W0;
    default_marking.overlap = o.overlap;
    default_marking.reverse = o.reverse;
    build<P>(W, o.R, acc, cap, s);
    default_marking = Marking();
    const Side<B>& sd = s == 1 ? W.plus : W.minus;
    const Renderer<B>& rc = *(s == 1 ? W.rp : W.rn);
    Containments<B> C;
    if (o.contain) containments<B>(o, sd, W.L, C, false);
    // the leaf copy
    std::vector<LeafRec<B>> leaves;
    LeafRenderer<B> lr(sd, W.L, acc, cap);
    lr.overlap = o.overlap; lr.reverse = o.reverse;
    if (o.contain) lr.onvisit = [&](LeafRenderer<B>& r, const Elem& h, int j) { C.mark(r, h, j); };
    lr.hook = [&](const typename LeafRenderer<B>::Frame& f, const Pt&, const Pt&, bool, int root) {
        leaves.push_back({f.h, f.j, f.depth, root, f.key});
    };
    for (size_t r = 0; r < W.roots.size(); r++)
        for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) lr.draw(W.roots[r], (int)j, (int)r);
    bool same = lr.check(rc);
    printf("\n== %s: R = %.4g (%zu roots), ACC %.4g, pieces at ACC/%d; marking: --overlap %d%s%s ==\n", sd.name, o.R,
           W.roots.size(), acc, o.k, o.overlap, o.reverse ? " --reverse" : "", o.contain ? " --contain" : "");
    printf("  picture: %zu leaves, %zu polylines, %zu points, %zu branches visited, %zu keys marked (%zu reverses)%s%s\n",
           lr.nLeaves, lr.starts.size(), lr.pts.size()/N, lr.nVisited, lr.nMarked, lr.nReverse,
           o.contain ? "" : (same ? "; the leaf copy draws the renderer's picture, point for point" :
                             "; the leaf copy is NOT the renderer's picture"), lr.capped ? "  [CAPPED]" : "");
    if (o.contain)
        printf("  containments: %zu certified, %zu keys marked by them\n", C.certified.size(), C.nMarked);
    { KeySet().t.swap(lr.done.t); }
    memcheck(o, "the picture");
    // ---- the ink, and the measure by keys
    const uint64_t geomask = (1ULL << o.geoshift) - 1;
    KeyMeasure km = key_measure<B>(sd, W.L, leaves, facc, o.keyshift, [&](size_t li) { if ((li & 4095) == 0) memcheck(o, "the measure by keys"); }, o.salt);
    const double ink = km.ink;
    const size_t nfine = km.nfine, nforced = km.nforced;
    StampSet df;
    printf("  ink: %zu fine pieces (forced %zu), total chord %.6g\n", nfine, nforced, ink);
    printf("  by keys (1/%llu of the keys): %zu samples, %zu keys, %zu of them in two leaves or more (at most %zu);"
           " ink drawn twice %.2f%% (standard error %.2f%%, from 8 blocks of the sample)\n", 1ULL << o.keyshift, km.samples,
           km.keys, km.shared, km.maxm, 100*km.fraction(), 100*km.stderror());
    fflush(stdout);
    fprintf(stderr, "%s: the picture and the measure by keys: %.1f s, peak %.0f MB\n", sd.name, now() - t0, peak_mb());
    if (o.nogeo) return;
    // ---- the measure without keys
    double tg = now();
    GenericPoints<B> G = generic_points(sd, 8, 6);
    OnCurve<B> oc(sd, W.L);
    oc.tol = o.tol; oc.prune = o.prune; oc.budget = o.budget;
    // the candidates: a grid for the prune factor, and one for the stability study's larger factors
    const double maxprune = std::max(o.prune, 16.0);
    CandidateGrid<N> grid, wide;
    grid.h = o.prune*acc;
    wide.h = maxprune*acc;
    // each leaf's box and size: a leaf farther from P than its reach (prune times its size, at least ACC) is not
    // tested (the polyline lies in the box, so this changes no answer)
    std::vector<std::array<float, 2*N + 1>> box(leaves.size());
    auto far = [&](size_t li, const Pt& Q, double prune) {
        const std::array<float, 2*N + 1>& q = box[li];
        double d2 = 0;
        for (int i = 0; i < N; i++) {
            double e = std::max({0.0, (double)q[i] - Q.x[i], Q.x[i] - (double)q[N + i]});
            d2 += e*e;
        }
        double r = prune*std::max((double)q[2*N], acc) + 2e-6;
        return d2 > r*r;
    };
    for (size_t li = 0; li < leaves.size(); li++) {
        DMat M = B::to_double(leaves[li].h);
        int y = leaves[li].j;
        for (int c = 0; c < 64 && sd.roof[y].size() == 1; c++) { M = dmul(M, oc.offd[y][0]); y = sd.roof[y][0]; }
        std::vector<Pt> pts = {B::boundary(B::act(M, sd.vfix)), B::boundary(B::act(M, sd.ev[y]))};
        for (size_t i = 1; i < sd.wv[y].size(); i++) pts.push_back(B::boundary(B::act(M, sd.wv[y][i])));
        if (!sd.tipline.empty()) for (const auto& t : sd.tipline[y]) pts.push_back(B::boundary(B::act(M, t)));
        double lo[N], hi[N], sz = 0;
        for (int i = 0; i < N; i++) { lo[i] = hi[i] = pts[0].x[i]; }
        for (const Pt& q : pts) {
            for (int i = 0; i < N; i++) { lo[i] = std::min(lo[i], q.x[i]); hi[i] = std::max(hi[i], q.x[i]); }
            sz = std::max(sz, segment_distance(q, pts[0], pts[1]));
        }
        sz = std::max(sz, chord(pts[0], pts[1]));
        for (int i = 0; i < N; i++) { box[li][i] = (float)(lo[i] - 1e-6); box[li][N + i] = (float)(hi[i] + 1e-6); }
        box[li][2*N] = (float)(sz*(1 + 1e-6));
        grid.add((uint32_t)li, lo, hi, o.prune*std::max(sz, acc) + 1e-6);
        if (o.stab > 0) wide.add((uint32_t)li, lo, hi, maxprune*std::max(sz, acc) + 1e-6);
    }
    grid.finish();
    if (o.stab > 0) wide.finish();
    size_t gnone = 0, gone = 0, gmin = 1000, gpts = 0;
    for (size_t x = 0; x < G.p.size(); x++) {
        gpts += G.p[x].size();
        if (G.p[x].empty()) gnone++;
        else { if (G.p[x].size() == 1) gone++; gmin = std::min(gmin, G.p[x].size()); }
    }
    size_t ginh = 0;
    for (const auto& v : G.prefix) for (int k : v) ginh += k > 0;
    printf("  without keys: generic points (closed paths of length <= 6, or a child's): %zu in all, %zu of them from a"
           " child's; %zu letters have none, %zu have one, the others at least %zu; candidate cells %zu entries, %zu"
           " leaves checked at every query\n", gpts, ginh, gnone, gone, gmin, grid.items.size(), grid.big.size());
    struct Variant { const char* name; double tol, prune; int point; double red = 0, tot = 0; size_t covered = 0, und = 0; };
    std::vector<Variant> var = {{"tol 1e-6", 1e-6, o.prune, 0}, {"tol 1e-7", 1e-7, o.prune, 0},
                                {"tol 1e-8", 1e-8, o.prune, 0}, {"tol 1e-9", 1e-9, o.prune, 0},
                                {"tol 1e-10", 1e-10, o.prune, 0}, {"prune 3", o.tol, 3, 0}, {"prune 8", o.tol, 8, 0},
                                {"prune 16", o.tol, 16, 0}, {"the other point", o.tol, o.prune, 1}};
    double gtot = 0, gred = 0, gcov = 0, gup = 0, cls[NCLASS] = {0}, gbt[8] = {0}, gbr[8] = {0};
    size_t ngeo = 0, nund = 0, ncand = 0, nnogen = 0, ncov = 0;
    std::map<std::string, double> patterns;
    TreeCache<B> trees;
    std::vector<uint32_t> cand;
    Renderer<B> cover(sd, W.L, 1, 1);                   // the pieces found covered, for the picture
    const bool oriented = !keys_exact(W.L);             // S^4: each oriented branch of a leaf (overdraw.h: oriented_key)
    for (size_t li = 0; li < leaves.size(); li++) {
        df.clear();
        const LeafRec<B>& A = leaves[li];
        walk_path<B>(sd, W.L, A.h, A.j, A.depth, facc, df, [&](const std::vector<PathNode<B>>& path, const Pt& a,
                                                               const Pt& b, bool) {
            const PathNode<B>& F = path.back();
            const uint64_t hk = splitmix(oriented_key<B>(F.h, F.j) ^ SALT_GEO ^ o.salt);
            if ((hk & geomask) != 0) return;
            const int blk = (int)(hk >> 61);
            if (G.p[F.j].empty()) { nnogen++; return; }
            DMat hd = B::to_double(F.h);
            const size_t np = G.p[F.j].size(), ip = (size_t)(splitmix(F.key ^ SALT_PT) % np);
            Pt Pg = B::boundary(B::act(hd, G.p[F.j][ip]));
            double c = chord(a, b);
            ngeo++;
            gtot += c;
            grid.query(Pg.x, cand);
            int non = 0, nu = 0;
            std::vector<uint32_t> on;
            for (uint32_t bi : cand) {
                if (bi == li || far(bi, Pg, o.prune)) continue;
                ncand++;
                int r = oc.decide(leaves[bi].h, leaves[bi].j, Pg, acc);
                if (r == 1) { non++; on.push_back(bi); }
                else if (r < 0) nu++;
            }
            if (nu) nund++;
            gup += c*(non + nu)/(non + nu + 1);
            gbt[blk] += c; gbr[blk] += c*non/(non + 1);
            if (non) {
                ncov++; gcov += c;
                gred += c*non/(non + 1);
                for (uint32_t bi : on) {
                    const Tree<B>& TB = trees.get(bi, sd, W.L, leaves[bi], facc);
                    std::string pat;
                    Class k = classify<B>(sd, W.L, path, A, leaves[bi], TB, pat);
                    cls[k] += c/(non + 1);
                    if (k == RELATION) patterns[pat] += c/(non + 1);
                }
                if (!o.png.empty()) {
                    cover.starts.push_back((uint32_t)(cover.pts.size()/N));
                    cover.push(a); cover.push(b);
                }
            }
            if ((int)ngeo <= o.stab) {                  // the stability study
                for (Variant& v : var) {
                    OnCurve<B> ov(sd, W.L);
                    ov.tol = v.tol; ov.prune = v.prune;
                    ov.budget = (size_t)(o.budget*std::min(4.0, std::max(1.0, (v.prune/o.prune)*(v.prune/o.prune))));
                    Pt Q = Pg;
                    if (v.point == 1) Q = B::boundary(B::act(hd, G.p[F.j][(ip + 1) % np]));
                    int m = 0, u = 0;
                    std::vector<uint32_t> cq;
                    (v.prune > o.prune ? wide : grid).query(Q.x, cq);
                    for (uint32_t bi : cq) {
                        if (bi == li || far(bi, Q, v.prune)) continue;
                        int r = ov.decide(leaves[bi].h, leaves[bi].j, Q, acc);
                        if (r == 1) m++; else if (r < 0) u++;
                    }
                    v.tot += c; v.red += c*m/(m + 1); v.covered += m > 0; v.und += u > 0;
                }
            }
        }, oriented);
        if ((li & 1023) == 0) memcheck(o, "the measure without keys");
        if ((li & 8191) == 8191)
            fprintf(stderr, "%s: without keys: %zu of %zu leaves, %zu pieces, %.0f s\n", sd.name, li + 1, leaves.size(), ngeo,
                    now() - tg);
    }
    printf("  without keys (1/%llu of the pieces, tolerance %.3g, prune factor %.3g, budget %zu): %zu pieces, %zu of them on"
           " another leaf's curve (%.2f%% of their ink); ink drawn twice %.2f%% (standard error %.2f%%; %.2f%% if every"
           " undecided leaf covered its piece; %zu pieces with an undecided leaf); %.1f leaves tested per piece; %zu pieces"
           " without a generic point\n", (unsigned long long)(geomask + 1), o.tol, o.prune, o.budget, ngeo, ncov,
           gtot > 0 ? 100*gcov/gtot : 0.0, gtot > 0 ? 100*gred/gtot : 0.0, 100*block_stderror(gbt, gbr),
           gtot > 0 ? 100*gup/gtot : 0.0, nund, ngeo ? (double)ncand/ngeo : 0.0, nnogen);
    printf("    the ink drawn twice by class:");
    for (int k = 0; k < NCLASS; k++) printf("  %s %.1f%%", CLASS_NAME[k], gred > 0 ? 100*cls[k]/gred : 0.0);
    printf("\n");
    if (!patterns.empty()) {
        std::vector<std::pair<double, std::string>> pv;
        for (auto& q : patterns) pv.push_back({q.second, q.first});
        std::sort(pv.rbegin(), pv.rend());
        printf("    relations, the commonest (share of the ink drawn twice):\n");
        for (size_t i = 0; i < pv.size() && i < 8; i++) printf("      %.2f%%  %s\n", 100*pv[i].first/gred, pv[i].second.c_str());
    }
    if (o.stab > 0) {
        printf("    stability, on the first %d pieces (ink drawn twice; pieces covered; pieces with an undecided leaf; a larger"
               " prune factor F gets the budget times (F/%.3g)^2, at most 4):\n", std::min(o.stab, (int)ngeo), o.prune);
        for (const Variant& v : var)
            printf("      %-16s %.2f%%  %zu  %zu\n", v.name, v.tot > 0 ? 100*v.red/v.tot : 0.0, v.covered, v.und);
    }
    fflush(stdout);
    printf("    decisions: %zu, branches looked at %zu (and %zu met again, skipped; %zu products in doubles past the growth"
           " guard); undecided %zu: %zu by the budget, %zu by the depth limit (%d levels)\n", oc.decisions, oc.nodes,
           oc.skipped, oc.inexact, oc.undecided, oc.cutbudget, oc.cutdepth, oc.maxdepth);
    fprintf(stderr, "%s: the measure without keys: %.1f s (%zu branches in the decisions), peak %.0f MB\n", sd.name,
            now() - tg, oc.nodes, peak_mb());
    if (!o.png.empty()) {                               // the picture: the drawing pale, the covered pieces dark
        std::string fn = o.png;
        size_t dot = fn.rfind(".png");
        fn = (dot == std::string::npos ? fn : fn.substr(0, dot)) + (s == 1 ? "_Zplus.png" : "_Zminus.png");
        typename View<N>::V v;
        View<N>::defaults(W, v);
        int size = o.size ? o.size : View<N>::default_size;
        std::vector<const Renderer<B>*> layers = {&rc, &cover};
        std::vector<std::array<double, 3>> cols = {s == 1 ? std::array<double, 3>{0.70, 0.76, 0.95} :
                                                            std::array<double, 3>{0.95, 0.74, 0.74},
                                                   {0.0, 0.0, 0.0}};
        std::vector<SpherePoint<N>> marks = {W.plus.V, W.minus.V};
        if (!View<N>::paint(fn.c_str(), size, layers, cols, v, marks)) { fprintf(stderr, "cannot write %s\n", fn.c_str()); exit(1); }
        fprintf(stderr, "wrote %s\n", fn.c_str());
    }
    delete W.rp; delete W.rn;
}

template <class P>
void run(const Options& o)
{
    typedef typename P::B B;
    World<B> W;
    prepare<P>(o.subst.c_str(), W);
    printf("overdraw: example %s (S^%d), file %s\n", o.example.c_str(), B::N - 1, o.subst.c_str());
    if (o.mode == "picture") {
        if (o.png.empty()) { fprintf(stderr, "--mode picture needs --png FILE\n"); exit(1); }
        for (int s = 1; s <= 2; s++) if (o.which & s) picture<P>(o, W, s);
        return;
    }
    if (o.mode == "missing") {
        for (int s = 1; s <= 2; s++) if (o.which & s) missing<P>(o, W, s);
        return;
    }
    if (o.mode == "pointdist") {
        for (int s = 1; s <= 2; s++) if (o.which & s) pointdist<B>(o, s == 1 ? W.plus : W.minus, W.L);
        return;
    }
    if (o.mode == "involutions") {
        for (int s = 1; s <= 2; s++) if (o.which & s) involutions<B>(o, s == 1 ? W.plus : W.minus, W.L);
        return;
    }
    if (o.mode == "contain") {
        for (int s = 1; s <= 2; s++) {
            if (!(o.which & s)) continue;
            Containments<B> C;
            double t0 = now();
            containments<B>(o, s == 1 ? W.plus : W.minus, W.L, C, true);
            fprintf(stderr, "%s: the containment rule: %.1f s, peak %.0f MB\n", s == 1 ? "Z+" : "Z-", now() - t0, peak_mb());
        }
        return;
    }
    for (int s = 1; s <= 2; s++) if (o.which & s) measure_side<P>(o, W, s);
}

void usage()
{
    fprintf(stderr,
"usage: overdraw -e cs|imm [-f FILE] [-R R] [-a ACC] [-k K] [--only +|-] [--overlap L] [--reverse] [--contain]\n"
"                [--keyshift S] [--geoshift S] [--salt N] [--tol T] [--prune F] [--budget N] [--stab N] [--nogeo]\n"
"                [--png FILE]\n"
"                [-s SIZE]\n"
"                [--cap N] [--memlimit GB] [--mode measure|contain|involutions|pointdist|missing|picture] [--letter X] [--cwords N]\n"
"  (see the comment at the top of apps/tools/overdraw.cc)\n");
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
        else if (a == "-R") o.R = atof(next().c_str());
        else if (a == "-a") o.acc = atof(next().c_str());
        else if (a == "-k") o.k = atoi(next().c_str());
        else if (a == "--only") { std::string w = next(); o.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--overlap") o.overlap = atoi(next().c_str());
        else if (a == "--reverse") o.reverse = true;
        else if (a == "--contain") o.contain = true;
        else if (a == "--keyshift") o.keyshift = atoi(next().c_str());
        else if (a == "--geoshift") o.geoshift = atoi(next().c_str());
        else if (a == "--tol") o.tol = atof(next().c_str());
        else if (a == "--prune") o.prune = atof(next().c_str());
        else if (a == "--stab") o.stab = atoi(next().c_str());
        else if (a == "--budget") o.budget = (size_t)atof(next().c_str());
        else if (a == "--letter") o.letter = atoi(next().c_str());
        else if (a == "--salt") o.salt = splitmix((uint64_t)atoll(next().c_str()));
        else if (a == "--taumin") o.taumin = atof(next().c_str());
        else if (a == "--nogeo") o.nogeo = true;
        else if (a == "--png") o.png = next();
        else if (a == "-s") o.size = atoi(next().c_str());
        else if (a == "--cap") o.cap = (size_t)atof(next().c_str());
        else if (a == "--memlimit") o.memlimit = atof(next().c_str());
        else if (a == "--mode") o.mode = next();
        else if (a == "--cwords") o.cwords = atoi(next().c_str());
        else { usage(); return 1; }
    }
    if ((o.example != "cs" && o.example != "imm") || (o.mode != "measure" && o.mode != "contain" && o.mode != "involutions" && o.mode != "pointdist" &&
         o.mode != "missing" && o.mode != "picture") || o.acc <= 0 ||
        o.k < 1 || o.overlap < 0 || o.overlap > 16 || o.keyshift < 0 || o.keyshift > 30 || o.geoshift < 0 ||
        o.geoshift > 30 || o.tol <= 0 || o.prune <= 0 || o.cwords < 0 || o.cwords > 3) { usage(); return 1; }
    const bool default_file = o.subst.empty();     // the programs' default
    if (default_file) o.subst = tools::default_subst(o.example == "cs");
    if (!tools::find_subst(o.subst, o.example == "cs", default_file, ZIPPERS_DATA_PLACES)) return 1;
    double t0 = now();
    if (o.example == "cs") { cs::setup(); run<cs::RenderPolicy>(o); }
    else { imm::setup(); run<imm::RenderPolicy>(o); }
    fprintf(stderr, "overdraw: %.2f s, peak %.0f MB\n", now() - t0, peak_mb());
    return 0;
}
