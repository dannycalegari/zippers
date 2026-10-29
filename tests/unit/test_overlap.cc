// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_overlap: the renderer's marking (src/render/renderer.h) and the parts of the overdraw measure
// (apps/tools/overdraw.h), in S^3 (the Cartwright-Steger file of the programs, _t3) and S^4 (IMM's _t1).
//   1. MARKING, WRITTEN AGAIN. A recursive renderer written here from the definitions (RefRenderer: the splitting
//      rule, the keys of render/renderer.h's top comment, and marking: after an accepted leaf that is not forced, the
//      keys of its children, and below the first level those of the children of sub-branches whose chord is at least
//      acc/8, down to L levels; with reverse, the other orientation of every involution branch visited or marked)
//      draws what the renderer draws, bit for bit (the points, the polylines, the leaves, the counts of marked keys),
//      at L = 0 (nothing marked), 1 and 2, with and without reverse, at R = 0 and the first shell, ACC 0.04, both
//      zippers. So does the leaf copy of the tools (apps/tools/leafcopy.h). Also with a depth limit of 3, so that there
//      are forced leaves, which are not marked. And at a coarse accuracy (ACC 0.5, at the first shells), where many
//      roots are accepted whole as leaves at depth 0: they are marked like any other leaf (this catches a
//      version that leaves them unmarked, which passes every other test: at R = 0 and the first shells at ACC 0.04 no
//      root is a leaf). root_leaf_cases checks it directly: a root alone
//      in a renderer of its own, the keys of its children worked out again, all of them marked, and no others.
//   2. A SUBSET. With L >= 1 and without reverse, every branch visited is a branch visited without marking, and every
//      leaf drawn is a leaf drawn without marking, bit for bit: the same key and the same floats of its points (as
//      drawn: through its tips in S^4). That is what the code checks, on S^3 _t3 and S^4 _t1 at ACC 0.04; it does not
//      hold in every case: on S^3 _t1 some leaves (55 to 82 of 400 to 600 thousand) are drawn from their other end, with
//      the same keys and the same floats in reverse order, so there the subset holds by keys only (this check would
//      fail there), and in the coarse cases below (ACC 0.5) it fails on S^3 _t3
//      too (16 of 1641 leaves of Z+), where the code checks the subset by keys only (bitforbit = false). With reverse
//      the code checks less, and not bit for bit: up to 1 in 1000 of the branches visited, and of the leaves, may be
//      absent without marking (by their keys), and a leaf drawn from its other end is allowed (it is only counted, in
//      the notes).
//   3. THE OVERDRAW MEASURE, on cases where the answer is known:
//      - the generic points (overdraw.h: generic_points) are fixed points of their maps and lie on their arcs, away
//        from the ends;
//      - a branch and its own child, as two leaves: every generic point of the child's pieces is on the branch's curve
//        (OnCurve), and the measure by keys counts exactly the child's ink as drawn twice;
//      - a brute-force comparison, on the S^3 picture at R = 0, ACC 0.04: for sampled pieces and the leaves near them,
//        OnCurve's answer against the distance from the point to the leaf's complete refinement at 1e-3, 3e-4, 1e-4
//        (no pruning): a point found on a curve is within 3 times the accuracy of each refinement, and a point found
//        off it is not within the accuracy of all three;
//      - S^4: the arc of an involution letter and its other orientation are NOT the same set (docs/developer_guide.md):
//        some vertex of each refinement at 0.0025 lies farther than 0.01
//        from the other's.
// Light: about a minute. Uses src/ and apps/tools/ only. ZIPPERS_REPO is the repository.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "geometry/pu21/pu21.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/world.h"

#include "leafcopy.h"
#include "overdraw.h"

#include "render_test_support.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
namespace zt = zippers::tools;
using rtest::check;
using rtest::note;
using rtest::section;

// ---- marking, written again ------------------------------------------------------------------------------------------

template <class B>
struct RefRenderer {
    typedef typename B::Elem Elem;
    typedef typename B::Point Pt;
    const z::Side<B>& sd;
    const z::Letters<B>& L;
    double acc;
    int levels;
    bool rev;
    int maxdepth = 400;
    z::KeySet done;
    std::vector<float> pts;
    std::vector<uint32_t> starts;
    bool open = false;
    size_t nLeaves = 0, nMarked = 0, nReverse = 0, nVisited = 0;
    struct Leaf { uint64_t key; std::vector<float> p; };
    std::vector<Leaf> leaves;
    std::vector<uint64_t> visited;                      // the keys of the branches visited, in order

    RefRenderer(const z::Side<B>& s, const z::Letters<B>& l, double a, int lv, bool r) : sd(s), L(l), acc(a), levels(lv), rev(r) {}

    static bool guard(const Elem& h) { return B::max_coefficient(h) < (1LL << 40); }
    // the key of a branch (h, j): its own for j in S+, that of its reversal (h s_j, j^-1) otherwise
    uint64_t key(const Elem& h, int j) const
    {
        return L.plus[j] ? z::fingerprint(B::key(h), L.canon[j]) : z::fingerprint(B::key(B::mul(h, L.s[j])), L.canon[j]);
    }
    void push(std::vector<float>& v, const Pt& p) { for (int i = 0; i < B::N; i++) v.push_back((float)p.x[i]); }
    // the other orientation (h s_c, c) of an involution branch
    void mark_rev(const Elem& h, int c)
    {
        if (!rev || L.inv[c] != c || !guard(h)) return;
        if (done.insert(key(B::mul(h, L.s[c]), c))) { nMarked++; nReverse++; }
    }
    // the sub-branches of (h, j) with ends a, b, from its children at depth `level` + 1
    void mark(const Elem& h, int j, const Pt& a, const Pt& b, int level)
    {
        const std::vector<int>& r = sd.roof[j];
        typename B::DMat hd = B::to_double(h);
        for (size_t i = 0; i < r.size(); i++) {
            Elem hc = B::mul(h, sd.off[j][i]);
            if (done.insert(key(hc, r[i]))) nMarked++;
            mark_rev(hc, r[i]);
            if (level + 1 < levels) {
                Pt s = i == 0 ? a : B::boundary(B::act(hd, sd.wv[j][i]));
                Pt e = i + 1 == r.size() ? b : B::boundary(B::act(hd, sd.wv[j][i + 1]));
                if (z::chord(s, e) >= acc/8 && guard(hc)) mark(hc, r[i], s, e, level + 1);
            }
        }
    }
    void visit(const Elem& h, int j, int depth)
    {
        if (!done.insert(key(h, j))) { open = false; return; }
        nVisited++;
        visited.push_back(key(h, j));
        mark_rev(h, j);
        typename B::DMat hd = B::to_double(h);
        Pt a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[j]));
        bool split = z::chord(a, b) > acc;
        for (size_t i = 1; !split && i < sd.wv[j].size(); i++)
            if (z::segment_distance(B::boundary(B::act(hd, sd.wv[j][i])), a, b) > acc) split = true;
        if (!split && !sd.through.empty() && !sd.through[j].empty() && guard(h) && z::through_split(sd, h, j, a, b, acc))
            split = true;
        if (split && depth < maxdepth && guard(h)) {
            const std::vector<int>& r = sd.roof[j];
            std::vector<char> known(r.size());        // children drawn already when the branch is split
            std::vector<Elem> ch(r.size());
            for (size_t i = 0; i < r.size(); i++) { ch[i] = B::mul(h, sd.off[j][i]); known[i] = done.contains(key(ch[i], r[i])); }
            for (size_t i = 0; i < r.size(); i++) {
                if (known[i]) { open = false; continue; }
                visit(ch[i], r[i], depth + 1);
            }
            return;
        }
        // a leaf: from a, through its tips, to b
        nLeaves++;
        Leaf lf{key(h, j), {}};
        push(lf.p, a);
        if (!open) { starts.push_back((uint32_t)(pts.size()/B::N)); push(pts, a); open = true; }
        if (!sd.tipline.empty())
            for (const typename B::DVec& v : sd.tipline[j]) { Pt q = B::boundary(B::act(hd, v)); push(pts, q); push(lf.p, q); }
        push(pts, b);
        push(lf.p, b);
        leaves.push_back(lf);
        if (levels > 0 && !split && guard(h)) mark(h, j, a, b, 0);
    }
    void draw(const Elem& g, int j) { open = false; visit(g, j, 0); }
};

// bitforbit: also that every leaf drawn with marking is, floats and all, a leaf drawn without it. That holds in the cases at
// ACC 0.04 below; it does not hold in general (it is false on the S^3 file _t1, and at ACC 0.5 it fails
// on _t3 too: some leaves are drawn from their other end, with the same key). The subset by keys is always checked
template <class P>
void marking_cases(const char* file, double R, double acc, bool bitforbit = true)
{
    typedef typename P::B B;

    z::World<B> W0;
    rtest::capture_stderr([&] { z::prepare<P>(file, W0); });
    std::map<int, std::set<std::pair<uint64_t, std::vector<float>>>> base;   // the leaves at L = 0, by side
    std::map<int, std::set<uint64_t>> basekeys, basevisited;
    size_t ninv = 0;
    for (size_t j = 0; j < W0.L.s.size(); j++) ninv += W0.L.inv[j] == (int)j;
    for (int lv : {0, 1, 2})
        for (bool rev : {false, true}) {
            if (rev && lv == 2) continue;
            z::World<B> W = W0;
            z::default_marking.overlap = lv;
            z::default_marking.reverse = rev;
            rtest::capture_stderr([&] { z::build<P>(W, R, acc, (size_t)6e7, 3); });
            z::default_marking = z::Marking();
            for (int s = 1; s <= 2; s++) {
                const z::Side<B>& sd = s == 1 ? W.plus : W.minus;
                const z::Renderer<B>& r = *(s == 1 ? W.rp : W.rn);
                RefRenderer<B> ref(sd, W.L, acc, lv, rev);
                zt::LeafRenderer<B> lr(sd, W.L, acc, (size_t)6e7);
                lr.overlap = lv; lr.reverse = rev;
                for (size_t q = 0; q < W.roots.size(); q++)
                    for (size_t j = 0; j < W.L.s.size(); j++)
                        if (W.L.plus[j]) { ref.draw(W.roots[q], (int)j); lr.draw(W.roots[q], (int)j, (int)q); }
                bool same = ref.pts == r.pts && ref.starts == r.starts && ref.nLeaves == r.nLeaves &&
                            ref.nMarked == r.nMarked && ref.nReverse == r.nReverse && ref.nVisited == r.nVisited;
                check(same, "%s %s R=%g acc=%g L=%d%s: the renderer is marking written again, bit for bit (leaves %zu/%zu,"
                      " marked %zu/%zu, reverses %zu/%zu)", file, sd.name, R, acc, lv, rev ? " reverse" : "", r.nLeaves,
                      ref.nLeaves, r.nMarked, ref.nMarked, r.nReverse, ref.nReverse);
                check(lr.check(r), "%s %s R=%g acc=%g L=%d%s: the leaf copy of the tools draws the renderer's picture",
                      file, sd.name, R, acc, lv, rev ? " reverse" : "");
                if (lv == 0 && !rev) {
                    check(r.nMarked == 0 && r.nReverse == 0, "%s %s: nothing marked at L = 0", file, sd.name);
                    for (const auto& lf : ref.leaves) { base[s].insert({lf.key, lf.p}); basekeys[s].insert(lf.key); }
                    basevisited[s].insert(ref.visited.begin(), ref.visited.end());
                    continue;
                }
                // every leaf is a leaf drawn without marking: the same key and the same floats; or, for a branch of a
                // letter that is not an involution (one key for both orientations), the same key drawn from its other
                // end, when marking has removed the route by which it was first reached
                size_t missing = 0, flipped = 0, splitbefore = 0, unvisited = 0;
                for (const auto& lf : ref.leaves) {
                    if (base[s].count({lf.key, lf.p})) continue;
                    if (basekeys[s].count(lf.key)) flipped++;
                    else { missing++; if (basevisited[s].count(lf.key)) splitbefore++; }
                }
                for (uint64_t k : ref.visited) unvisited += !basevisited[s].count(k);
                // every branch visited is one visited without marking, and every leaf's key a leaf's key. With
                // reverse, a branch can be reached from its other end (marking removed the route by which it was
                // reached first), and its split decision, computed from another product, may then come out the other
                // way by rounding, opening branches that are not visited without marking: up to 1 in 4000 here
                if (!rev) {
                    check(unvisited == 0, "%s %s R=%g acc=%g L=%d: every branch visited is visited without marking (%zu"
                          " are not)", file, sd.name, R, acc, lv, unvisited);
                    check(missing == 0, "%s %s R=%g acc=%g L=%d: every leaf's key is that of a leaf drawn without marking"
                          " (%zu of %zu are not)", file, sd.name, R, acc, lv, missing, ref.leaves.size());
                } else
                    check(unvisited*1000 <= ref.visited.size() && missing*1000 <= ref.leaves.size(), "%s %s R=%g acc=%g"
                          " L=%d reverse: branches not visited without marking %zu of %zu, leaves not leaves without it %zu"
                          " of %zu (at most 1 in 1000)", file, sd.name, R, acc, lv, unvisited, ref.visited.size(), missing,
                          ref.leaves.size());
                check(rev || !bitforbit || flipped == 0, "%s %s R=%g acc=%g L=%d: every leaf is a leaf drawn without marking,"
                      " bit for bit (%zu of %zu are drawn from their other end)", file, sd.name, R, acc, lv, flipped,
                      ref.leaves.size());
                check(r.nMarked > 0 || (lv == 0 && ninv == 0), "%s %s L=%d%s: marking marks", file, sd.name, lv,
                      rev ? " reverse" : "");
                note("(%s %s R=%g acc=%g L=%d%s: %zu leaves, %zu without marking; drawn from the other end %zu, not a leaf"
                     " without marking %zu (%zu split there), branches not visited without marking %zu; %zu keys marked,"
                     " %zu of them reverses)", file, sd.name, R, acc, lv, rev ? " reverse" : "", r.nLeaves,
                     base[s].size(), flipped, missing, splitbefore, unvisited, r.nMarked, r.nReverse);
            }
            delete W.rp; delete W.rn;
        }
}

// forced leaves (the depth limit): a leaf split no further because of the limit is not marked
template <class P>
void forced_cases(const char* file, double acc, int maxdepth)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    for (int lv : {1, 2})
        for (int s = 1; s <= 2; s++) {
            const z::Side<B>& sd = s == 1 ? W.plus : W.minus;
            z::Renderer<B> r(sd, W.L, acc, (size_t)6e7);
            r.maxdepth = maxdepth; r.overlap = lv;
            RefRenderer<B> ref(sd, W.L, acc, lv, false);
            ref.maxdepth = maxdepth;
            for (size_t j = 0; j < W.L.s.size(); j++)
                if (W.L.plus[j]) { r.draw(B::identity(), (int)j, 0); ref.draw(B::identity(), (int)j); }
            check(r.nForced > 0, "%s %s depth limit %d: forced leaves (%zu)", file, sd.name, maxdepth, r.nForced);
            check(ref.pts == r.pts && ref.starts == r.starts && ref.nMarked == r.nMarked && ref.nLeaves == r.nLeaves,
                  "%s %s depth limit %d L=%d: with forced leaves, the renderer is marking written again (marked %zu/%zu)",
                  file, sd.name, maxdepth, lv, r.nMarked, ref.nMarked);
        }
}

// roots accepted whole as leaves (depth 0): marked like any other leaf. At a coarse accuracy many roots of the first
// shells are single leaves (at ACC 0.04 none is, so every other test passes with a root leaf left unmarked). Each root
// alone, in a renderer of its own with L = 1: when it is a leaf (one branch visited, one leaf drawn), the keys of its
// children, worked out again here from the exact products, are exactly what marking put into done
template <class P>
void root_leaf_cases(const char* file, double R, double acc)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    rtest::capture_stderr([&] { z::build<P>(W, R, acc, (size_t)6e7, 3); });      // for the roots of radius R
    for (int s = 1; s <= 2; s++) {
        const z::Side<B>& sd = s == 1 ? W.plus : W.minus;
        RefRenderer<B> ref(sd, W.L, acc, 1, false);                              // only for its key (the keys of the file's top comment)
        size_t nroots = 0, nleaves = 0, nbad = 0, nkids = 0;
        for (size_t q = 0; q < W.roots.size(); q++)
            for (size_t j = 0; j < W.L.s.size(); j++) {
                if (!W.L.plus[j]) continue;
                nroots++;
                z::Renderer<B> r(sd, W.L, acc, (size_t)6e7);
                r.overlap = 1;
                r.draw(W.roots[q], (int)j, (int)q);
                if (r.nVisited != 1 || r.nLeaves != 1 || r.nForced != 0) continue;   // not accepted whole
                nleaves++;
                std::set<uint64_t> kids;                                            // the keys of the children
                for (size_t i = 0; i < sd.roof[j].size(); i++)
                    kids.insert(ref.key(B::mul(W.roots[q], sd.off[j][i]), sd.roof[j][i]));
                kids.erase(ref.key(W.roots[q], (int)j));                            // (the root's own key was in done already)
                bool ok = r.nMarked == kids.size() && r.done.n == kids.size() + 1;
                for (uint64_t k : kids) ok = ok && r.done.contains(k);
                nkids += kids.size();
                nbad += !ok;
                if (!ok && nbad <= 3)
                    check(false, "%s %s R=%g acc=%g: the root %zu, letter %zu, accepted whole as a leaf: marked %zu keys of"
                          " its %zu children's, %zu keys in done", file, sd.name, R, acc, q, j, r.nMarked, kids.size(), r.done.n);
            }
        check(nleaves >= 20, "%s %s R=%g acc=%g: roots accepted whole as leaves: %zu of %zu", file, sd.name, R, acc,
              nleaves, nroots);
        check(nbad == 0, "%s %s R=%g acc=%g: the children of every root accepted whole as a leaf are marked, and only they"
              " (%zu of %zu roots not)", file, sd.name, R, acc, nbad, nleaves);
        note("(%s %s R=%g acc=%g: %zu of the %zu roots (the letters of S+, from %zu roots) are single leaves, with %zu children"
             " marked in all)", file, sd.name, R, acc, nleaves, nroots, W.roots.size(), nkids);
    }
    delete W.rp; delete W.rn;
}

// ---- the overdraw measure --------------------------------------------------------------------------------------------

template <class P>
void generic_cases(const char* file)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    for (const z::Side<B>* sd : {&W.plus, &W.minus}) {
        zt::GenericPoints<B> G = zt::generic_points(*sd, 8, 6);
        zt::OnCurve<B> oc(*sd, W.L);
        oc.tol = 1e-10;
        size_t npts = 0, inherited = 0;
        for (size_t x = 0; x < G.p.size(); x++) {
            check(!G.p[x].empty(), "%s %s letter %zu: generic points", file, sd->name, x);
            for (size_t q = 0; q < G.p[x].size(); q++) {
                npts++;
                inherited += G.prefix[x][q] > 0;
                // the map of the path, again: the prefix, then the closed part, and the point its fixed point mapped
                // back by the prefix
                typename B::Elem pre = B::identity(), cyc = B::identity();
                int u = (int)x, start = -1;
                for (size_t c = 0; c < G.path[x][q].size(); c++) {
                    const auto& st = G.path[x][q][c];
                    check(sd->roof[u][st.first] == st.second, "the path follows the roofs");
                    if ((int)c < G.prefix[x][q]) pre = B::mul(pre, sd->off[u][st.first]);
                    else { if (start < 0) start = u; cyc = B::mul(cyc, sd->off[u][st.first]); }
                    u = st.second;
                }
                check(u == start, "%s %s letter %zu: the path ends with a closed path", file, sd->name, x);
                typename B::Elem conj = B::mul(B::mul(pre, cyc), B::inv(pre));
                std::pair<double, double> ev = zt::eigen_check(B::to_double(conj), G.p[x][q]);
                check(ev.first > 1.01 && ev.second < 1e-8, "%s %s letter %zu: an attracting fixed point (%g, %g)", file,
                      sd->name, x, ev.first, ev.second);
                typename B::Point P0 = B::boundary(G.p[x][q]);
                check(oc.decide(B::identity(), (int)x, P0) == 1, "%s %s letter %zu: on alpha_x", file,
                      sd->name, x);
                double len = z::chord(sd->V, B::boundary(sd->ev[x]));
                check(z::chord(P0, sd->V) >= 0.01*len && z::chord(P0, B::boundary(sd->ev[x])) >= 0.01*len,
                      "%s %s letter %zu: away from the ends", file, sd->name, x);
            }
        }
        // the prune factor acts: with a larger one the same decisions keep more branches, and none that was on the
        // curve is lost
        {
            zt::OnCurve<B> o4(*sd, W.L), o16(*sd, W.L);
            o4.tol = o16.tol = 1e-10; o4.prune = 4; o16.prune = 16; o16.budget = 4*o4.budget;
            size_t on4 = 0, on16 = 0, lost = 0;
            for (size_t x = 0; x < G.p.size(); x++)
                for (size_t q = 0; q < G.p[x].size() && q < 2; q++) {
                    typename B::Point P0 = B::boundary(G.p[x][q]);
                    for (size_t y = 0; y < G.p.size(); y += 7) {       // P0 against some other base arcs too
                        int r4 = o4.decide(B::identity(), (int)y, P0), r16 = o16.decide(B::identity(), (int)y, P0);
                        on4 += r4 == 1; on16 += r16 == 1;
                        lost += r4 == 1 && r16 == 0;                    // (undecided, past the budget, is not lost)
                    }
                }
            check(o16.nodes > o4.nodes && lost == 0, "%s %s: the prune factor 16 keeps more branches than 4 (%zu, %zu),"
                  " and no point that 4 finds is found off the curve with 16 (%zu found with 4, %zu with 16, %zu lost)",
                  file, sd->name, o16.nodes, o4.nodes, on4, on16, lost);
        }
        note("(%s %s: %zu generic points, %zu of them from a child's)", file, sd->name, npts, inherited);
    }
}

// a branch and its child, as two leaves: the child's pieces are on the branch's curve, and by keys the ink drawn twice
// is exactly the child's
template <class P>
void nested_case(const char* file, double facc)
{
    typedef typename P::B B;
    typedef typename B::Point Pt;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    for (const z::Side<B>* sd : {&W.plus, &W.minus}) {
        int x = -1;
        for (size_t j = 0; j < sd->roof.size() && x < 0; j++) if (W.L.plus[j] && sd->roof[j].size() >= 3) x = (int)j;
        int i = (int)sd->roof[x].size()/2, y = sd->roof[x][i];
        typename B::Elem hc = sd->off[x][i];
        std::vector<zt::LeafRec<B>> leaves = {{B::identity(), x, 0, 0, zt::branch_key<B>(W.L, B::identity(), x)},
                                              {hc, y, 1, 0, zt::branch_key<B>(W.L, hc, y)}};
        zt::GenericPoints<B> G = zt::generic_points(*sd, 8, 6);
        zt::OnCurve<B> oc(*sd, W.L);
        oc.tol = 1e-10;
        size_t n = 0, on = 0, und = 0, k = 0;
        double inkc = 0;
        zt::StampSet d;
        zt::walk_path<B>(*sd, W.L, hc, y, 1, facc, d, [&](const std::vector<zt::PathNode<B>>& path, const Pt& a,
                                                          const Pt& b, bool) {
            inkc += z::chord(a, b);
            const zt::PathNode<B>& F = path.back();
            if (k++ % 7 != 0) return;
            check(!G.p[F.j].empty(), "%s %s: a generic point for the letter %d of a piece", file, sd->name, F.j);
            if (G.p[F.j].empty()) return;
            Pt Q = B::boundary(B::act(B::to_double(F.h), G.p[F.j][0]));
            n++;
            int r = oc.decide(B::identity(), x, Q);
            on += r == 1; und += r < 0;
        });
        check(n > 20 && on + und == n && und*20 <= n, "%s %s: the generic points of the pieces of child %d of letter %d lie"
              " on its curve (%zu of %zu; %zu undecided)", file, sd->name, i + 1, x, on, n, und);
        zt::KeyMeasure km = zt::key_measure<B>(*sd, W.L, leaves, facc, 0, [](size_t) {});
        // every key of the child's refinement is in the branch's refinement (the same splitting rule below the child)
        // (exact in S^3; in S^4 a branch of the child's refinement that the parent's walk met first elsewhere, in its
        // other orientation, may be split there, by rounding, into other pieces: within 1%)
        double slack = W.L.s.size() == 36 ? 1e-6 : 1e-2;
        check(std::fabs(km.redundant - inkc) <= slack*inkc && km.maxm == 2, "%s %s: by keys, the ink drawn twice is the"
              " child's ink (%.9g, %.9g)", file, sd->name, km.redundant, inkc);
    }
}

// OnCurve against the complete refinement, on the S^3 picture
void brute_case(const char* file)
{
    typedef z::PU21 B;
    typedef B::Point Pt;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(file, W); });
    const double acc = 0.04, facc = acc/16;
    rtest::capture_stderr([&] { z::build<z::cs::RenderPolicy>(W, 0, acc, (size_t)6e7, 1); });
    const z::Side<B>& sd = W.plus;
    std::vector<zt::LeafRec<B>> leaves;
    zt::LeafRenderer<B> lr(sd, W.L, acc, (size_t)6e7);
    lr.hook = [&](const zt::LeafRenderer<B>::Frame& f, const Pt&, const Pt&, bool, int root) {
        leaves.push_back({f.h, f.j, f.depth, root, f.key}); };
    for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) lr.draw(W.roots[0], (int)j, 0);
    zt::GenericPoints<B> G = zt::generic_points(sd, 8, 6);
    zt::OnCurve<B> oc(sd, W.L);
    oc.tol = 1e-10;
    // the pairs (point, leaf): OnCurve on every leaf whose chord passes within 1.5 ACC of the point
    struct Pair { Pt Q; size_t bi; int r; };
    std::vector<Pair> onp, offp;
    zt::StampSet d;
    for (size_t li = 0; li < leaves.size() && (onp.size() < 40 || offp.size() < 60); li += 13) {
        d.clear();
        int taken = 0;
        zt::walk_path<B>(sd, W.L, leaves[li].h, leaves[li].j, leaves[li].depth, facc, d,
                         [&](const std::vector<zt::PathNode<B>>& path, const Pt&, const Pt&, bool) {
            if (taken++ % 11 != 3) return;
            const zt::PathNode<B>& F = path.back();
            Pt Q = B::boundary(B::act(B::to_double(F.h), G.p[F.j][0]));
            for (size_t bi = 0; bi < leaves.size(); bi++) {
                if (bi == li) continue;
                auto e = zt::ends<B>(sd, leaves[bi].h, leaves[bi].j);
                if (z::segment_distance(Q, e.first, e.second) > 1.5*acc) continue;
                int r = oc.decide(leaves[bi].h, leaves[bi].j, Q, acc);
                if (r == 1 && onp.size() < 40) onp.push_back({Q, bi, r});
                if (r == 0 && offp.size() < 60) offp.push_back({Q, bi, r});
            }
        });
    }
    long on = 0, off = 0, bad = 0, close = 0, tested = 0;
    for (const std::vector<Pair>* v : {&onp, &offp})
        for (const Pair& q : *v) {
            tested++;
            double dist[3];
            const double accs[3] = {1e-3, 3e-4, 1e-4};
            for (int k = 0; k < 3; k++) {              // the complete refinement at accs[k]
                std::vector<Pt> ptsk;
                size_t forced = 0;
                z::arc_points(sd, leaves[q.bi].h, leaves[q.bi].j, accs[k], ptsk, forced);
                double m = INFINITY;
                for (size_t t = 0; t + 1 < ptsk.size(); t++) m = std::min(m, z::segment_distance(q.Q, ptsk[t], ptsk[t + 1]));
                dist[k] = m;
            }
            // a point of the curve is within about the accuracy of each refinement (3 times, to be safe); a point
            // within a tenth of it at all three is suspicious if called off
            bool near = dist[0] <= 3e-3 && dist[1] <= 9e-4 && dist[2] <= 3e-4;
            bool within = dist[0] <= 1e-4 && dist[1] <= 3e-5 && dist[2] <= 1e-5;
            if (q.r == 1) { on++; if (!near) bad++; }
            else { off++; if (within) close++; }
        }
    check(on >= 40 && off >= 60 && bad == 0 && close*50 <= off, "%s Z+: OnCurve agrees with the complete"
          " refinements (%ld pairs: %ld on, of which %ld are not near; %ld off, of which %ld are within a tenth of the"
          " accuracy)", file, tested, on, bad, off, close);
}

// S^4: alpha_c and s_c alpha_c are not the same set
void involution_case(const char* file)
{
    typedef z::ON1<5> B;
    typedef B::Point Pt;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<z::imm::RenderPolicy>(file, W); });
    const double facc = 0.0025;
    for (const z::Side<B>* sd : {&W.plus, &W.minus}) {
        size_t ninv = 0, differ = 0;
        for (int c = 0; c < (int)W.L.s.size(); c++) {
            if (W.L.inv[c] != c) continue;
            ninv++;
            for (int dir = 0; dir < 2 ; dir++) {
                B::Elem ha = dir == 0 ? B::identity() : W.L.s[c], hb = dir == 0 ? W.L.s[c] : B::identity();
                std::vector<std::pair<Pt, Pt>> seg;
                zt::StampSet d1, d2;
                zt::walk<B>(*sd, W.L, hb, c, 0, facc, &d1, [&](const B::Elem&, int, int, int, int, const Pt& a,
                                                              const Pt& b, bool) { seg.push_back({a, b}); }, zt::NoSkip());
                double farthest = 0;
                size_t k = 0;
                zt::walk<B>(*sd, W.L, ha, c, 0, facc, &d2, [&](const B::Elem&, int, int, int, int, const Pt&, const Pt& b,
                                                              bool) {
                    if (k++ % 97 != 0 || farthest > 0.01) return;
                    double m = INFINITY;
                    for (const auto& q : seg) m = std::min(m, z::segment_distance(b, q.first, q.second));
                    farthest = std::max(farthest, m);
                }, zt::NoSkip());
                differ += farthest > 0.01;
            }
        }
        check(ninv == 8 && differ == 2*ninv, "%s %s: for each of the 8 involution letters c, alpha_c and s_c alpha_c have"
              " vertices more than 0.01 from each other (%zu of %zu directions)", file, sd->name, differ, 2*ninv);
    }
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::cs::setup();
    z::imm::setup();
    printf("test_overlap: the renderer's marking (render/renderer.h) and the overdraw measure (apps/tools/overdraw.h)\n");
    const std::string f3 = std::string(ZIPPERS_REPO) + "/legacy/s3/subst_4.00_9.50_t3.txt";
    const std::string f4 = std::string(ZIPPERS_REPO) + "/legacy/s4/subst5_2.30_4.50_t1.txt";

    section("marking written again; the leaves drawn with it are leaves drawn without it");
    marking_cases<z::cs::RenderPolicy>(f3.c_str(), 0, 0.04);
    marking_cases<z::cs::RenderPolicy>(f3.c_str(), 2.554, 0.04);
    marking_cases<z::imm::RenderPolicy>(f4.c_str(), 0, 0.04);
    marking_cases<z::imm::RenderPolicy>(f4.c_str(), 1.32, 0.04);
    // a coarse accuracy at the first shells: many roots are accepted whole as leaves (below)
    // (by keys only: at ACC 0.5 some leaves of S^3 _t3 are drawn from their other end, as on _t1 at ACC 0.04)
    marking_cases<z::cs::RenderPolicy>(f3.c_str(), 2.554, 0.5, false);
    marking_cases<z::imm::RenderPolicy>(f4.c_str(), 1.32, 0.5, false);

    section("roots accepted whole as leaves (depth 0): their children are marked (ACC 0.5, the first shells)");
    root_leaf_cases<z::cs::RenderPolicy>(f3.c_str(), 2.554, 0.5);
    root_leaf_cases<z::imm::RenderPolicy>(f4.c_str(), 1.32, 0.5);

    section("forced leaves (a depth limit of 3): not marked");
    forced_cases<z::cs::RenderPolicy>(f3.c_str(), 0.01, 3);
    forced_cases<z::imm::RenderPolicy>(f4.c_str(), 0.01, 3);

    section("the generic points");
    generic_cases<z::cs::RenderPolicy>(f3.c_str());
    generic_cases<z::imm::RenderPolicy>(f4.c_str());

    section("a branch and its child: on the curve, and the ink drawn twice by keys");
    nested_case<z::cs::RenderPolicy>(f3.c_str(), 0.0025);
    nested_case<z::imm::RenderPolicy>(f4.c_str(), 0.005);

    section("OnCurve against the complete refinements (S^3, R = 0, ACC 0.04)");
    brute_case(f3.c_str());

    section("S^4: alpha_c and s_c alpha_c are different sets");
    involution_case(f4.c_str());

    rtest::end_section();
    printf("test_overlap: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
