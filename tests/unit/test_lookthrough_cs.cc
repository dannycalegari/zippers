// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_lookthrough_cs: the look-ahead through one-letter roofs, (b), in S^3 (render/renderer.h). With the
// Cartwright-Steger render policy, which turns it on:
//   - the Sides: every letter whose roof (Side::roof) is one letter has its chain of one-letter roofs (Side::through),
//     ending at a letter whose roof is longer, and no other letter has one; no chain is cut; no tips;
//   - at R = 0 nothing changes for the file of zipper3 and link3 (and of every S^3 reference run), _t3: the renderer
//     with (b) is the renderer without it, bit for bit (points, polylines, records, counters, branches), at accuracies
//     0.04, 0.02, 0.01 and 0.005, both zippers. (For the other two stored files it is not so: (b) splits a few leaves
//     there at R = 0 already; they are among the cases of the last section.);
//   - the rule, written again here from its definition (ref_through: the inner vertices of the first split of the only
//     child (h off[x][0], y), tested against the chord of (h, x), and so on while the roof is one letter): a copy of
//     the renderer with that rule draws what the renderer draws, bit for bit, and a recursive arc with it is
//     arc_points, bit for bit, at the roots of the first shell (R = 2.554, ACC 0.04 and 0.02) and of R = 3.327 (ACC
//     0.04), and on the base arcs of _t1 and _t2 (R = 0);
//   - what (b) changes in those cases: every leaf that the renderer without (b) draws and the renderer with (b) does
//     not is a branch whose letter has a one-letter roof and that the look-ahead through its roof splits; every leaf
//     that only the renderer with (b) draws is a branch that the renderer without (b) never visits (it lies inside the
//     refinement of such a split branch); and the splits of (b) are those leaves and splits inside their refinements.
//     The de-duplication can make the leaves fewer: a split branch whose pieces are drawn elsewhere already leaves
//     nothing. The counts are printed.
// Light: a few seconds. Uses src/ only. ZIPPERS_REPO is the repository.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "geometry/pu21/pu21.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/world.h"

#include "render_test_support.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::PU21 NB;
typedef NB::Elem E;
typedef NB::Point P4;
using rtest::check;
using rtest::note;
using rtest::section;

static const char* const FILES3[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
static std::string repo_file(const char* f) { return std::string(ZIPPERS_REPO) + "/legacy/s3/" + f; }

// the test's own policy: the Cartwright-Steger policy without (b), the legacy renderer's behaviour
struct NoLookThroughPolicy : z::cs::RenderPolicy {
    static void sides(const z::Letters<NB>&, z::Side<NB>&, z::Side<NB>&) {}
};

// ---- the rule, written again ------------------------------------------------------------------------------------------

// the look-ahead through the one-letter roof of letter j of the branch (h, j) with chord [a, b]: the only child
// (h off[j][0], y) stands for the same arc; a vertex inside its first split farther than acc from [a, b] splits the
// branch; if the roof of y is again one letter, the same one level down
static bool ref_through(const z::Side<NB>& sd, const E& h, int j, const P4& a, const P4& b, double acc, int steps = 0)
{
    if (sd.roof[j].size() != 1 || steps > 64) return false;
    E hc = NB::mul(h, sd.off[j][0]);
    int y = sd.roof[j][0];
    NB::DMat cd = NB::to_double(hc);
    for (size_t i = 1; i < sd.wv[y].size(); i++)
        if (z::segment_distance(NB::boundary(NB::act(cd, sd.wv[y][i])), a, b) > acc) return true;
    return ref_through(sd, hc, y, a, b, acc, steps + 1);
}

// the splitting rule: the present one, then (b)
static bool ref_split(const z::Side<NB>& sd, const E& h, int j, const P4& a, const P4& b, double acc, bool& spike,
                      bool& through)
{
    spike = through = false;
    if (z::chord(a, b) > acc) return true;
    NB::DMat hd = NB::to_double(h);
    const std::vector<NB::DVec>& wv = sd.wv[j];
    for (size_t i = 1; i < wv.size(); i++)
        if (z::segment_distance(NB::boundary(NB::act(hd, wv[i])), a, b) > acc) { spike = true; return true; }
    if (NB::max_coefficient(h) < (1LL << 40) && ref_through(sd, h, j, a, b, acc)) { spike = through = true; return true; }
    return false;
}

// the complete arc of the branch (h, j) by that rule, recursively (arc_points's leaves, in order)
static void ref_arc(const z::Side<NB>& sd, const E& h, int j, int depth, double acc, std::vector<P4>& pts)
{
    NB::DMat hd = NB::to_double(h);
    P4 a = NB::boundary(NB::act(hd, sd.vfix)), b = NB::boundary(NB::act(hd, sd.ev[j]));
    bool spike, through;
    bool split = ref_split(sd, h, j, a, b, acc, spike, through);
    if (split && depth < 400 && NB::max_coefficient(h) < (1LL << 40)) {
        for (size_t i = 0; i < sd.roof[j].size(); i++) ref_arc(sd, NB::mul(h, sd.off[j][i]), sd.roof[j][i], depth + 1, acc, pts);
        return;
    }
    if (pts.empty()) pts.push_back(a);
    pts.push_back(b);
}

// a copy of the renderer (render/renderer.h: draw, visit, emit) with the rule above, which records each leaf's key
struct RefRenderer {
    struct Frame { E h; int j, depth, kind; uint64_t key; };
    const z::Side<NB>& sd;
    const z::Letters<NB>& L;
    double acc;
    bool withb;
    z::KeySet done;
    std::vector<float> pts;
    std::vector<uint32_t> starts;
    std::vector<int> polyroot, polyletter;
    int curroot = -1, curletter = -1;
    bool open = false;
    size_t nVisited = 0, nSkipped = 0, nLeaves = 0, nSpike = 0, nForced = 0, nThrough = 0;
    int deepest = 0;
    std::vector<Frame> stk;
    std::map<uint64_t, int> leafletter;                  // the leaves: key -> letter
    std::map<uint64_t, int> splitby;                     // the split branches: key -> 1 (chord or vertices), 2 ((b))
    RefRenderer(const z::Side<NB>& s, const z::Letters<NB>& l, double a, bool b) : sd(s), L(l), acc(a), withb(b) {}
    void push(const P4& p) { for (int i = 0; i < 4; i++) pts.push_back((float)p.x[i]); }
    void draw(const E& g, int j, int rootindex)
    {
        open = false;
        curroot = rootindex; curletter = j;
        stk.clear();
        stk.push_back({g, j, 0, 0, z::fingerprint(NB::key(g), L.canon[j])});
        while (!stk.empty()) {
            Frame f = stk.back();
            stk.pop_back();
            if (f.kind == 1) { open = false; continue; }
            if (!done.insert(f.key)) { ++nSkipped; open = false; continue; }
            ++nVisited;
            NB::DMat hd = NB::to_double(f.h);
            P4 a = NB::boundary(NB::act(hd, sd.vfix)), b = NB::boundary(NB::act(hd, sd.ev[f.j]));
            bool spike = false, through = false, split;
            if (withb) split = ref_split(sd, f.h, f.j, a, b, acc, spike, through);
            else {
                split = z::chord(a, b) > acc;
                if (!split)
                    for (size_t i = 1; i < sd.wv[f.j].size(); i++)
                        if (z::segment_distance(NB::boundary(NB::act(hd, sd.wv[f.j][i])), a, b) > acc) { split = spike = true; break; }
            }
            nSpike += spike; nThrough += through;
            if (split && f.depth < 400 && NB::max_coefficient(f.h) < (1LL << 40)) {
                splitby[f.key] = through ? 2 : 1;
                deepest = std::max(deepest, f.depth + 1);
                const std::vector<int>& roof = sd.roof[f.j];
                for (int i = (int)roof.size() - 1; i >= 0; i--) {
                    int x = roof[i];
                    uint64_t kc = z::fingerprint(NB::product_key(f.h, sd.keyoff[f.j][i]), L.canon[x]);
                    if (done.contains(kc)) { stk.push_back({f.h, 0, 0, 1, 0}); continue; }
                    stk.push_back({NB::mul(f.h, sd.off[f.j][i]), x, f.depth + 1, 0, kc});
                }
            } else {
                if (split) ++nForced;
                ++nLeaves;
                leafletter[f.key] = f.j;
                if (!open) {
                    starts.push_back((uint32_t)(pts.size()/4));
                    polyroot.push_back(curroot); polyletter.push_back(curletter);
                    push(a);
                    open = true;
                }
                push(b);
            }
        }
    }
};

static bool same_renderer(const z::Renderer<NB>& a, const z::Renderer<NB>& b)
{
    return a.pts == b.pts && a.starts == b.starts && a.polyroot == b.polyroot && a.polyletter == b.polyletter &&
           a.nVisited == b.nVisited && a.nSkipped == b.nSkipped && a.nLeaves == b.nLeaves && a.nSpike == b.nSpike &&
           a.nForced == b.nForced && a.deepest == b.deepest && a.capped == b.capped && a.nThrough == b.nThrough &&
           a.done.n == b.done.n && a.done.t == b.done.t;
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::cs::setup();
    printf("test_lookthrough_cs: the look-ahead through one-letter roofs in S^3 (render/renderer.h)\n");
    std::vector<z::World<NB>> W(3), W0(3);              // with (b): the CS policy; without: the test's own
    for (int f = 0; f < 3; f++) {
        std::string fn = repo_file(FILES3[f]);
        rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(fn.c_str(), W[f]); });
        rtest::capture_stderr([&] { z::prepare<NoLookThroughPolicy>(fn.c_str(), W0[f]); });
    }

    section("the Sides: the chains of one-letter roofs, and nothing else");
    for (int f = 0; f < 3; f++)
        for (int s = 0; s < 2; s++) {
            const z::Side<NB>& sd = s ? W[f].minus : W[f].plus;
            const z::Side<NB>& s0 = s ? W0[f].minus : W0[f].plus;
            check(sd.tipline.empty() && s0.tipline.empty(), "%s %s: no tips", FILES3[f], sd.name);
            check(s0.through.empty(), "%s %s: the test's policy has no look-ahead through roofs", FILES3[f], sd.name);
            check(sd.through.size() == sd.roof.size() && sd.throughcut == 0, "%s %s: a chain for each letter, none cut",
                  FILES3[f], sd.name);
            std::string chains;
            for (size_t x = 0; x < sd.roof.size() && x < sd.through.size(); x++) {
                bool one = sd.roof[x].size() == 1;
                check(one == !sd.through[x].empty(), "%s %s, letter %zu: a chain exactly if its roof is one letter",
                      FILES3[f], sd.name, x);
                if (!one) continue;
                int y = (int)x;
                bool ok = true;
                for (size_t k = 0; k < sd.through[x].size(); k++) {
                    ok = ok && sd.roof[y].size() == 1 && sd.through[x][k] == sd.roof[y][0];
                    y = sd.roof[y][0];
                }
                check(ok && sd.roof[y].size() >= 2, "%s %s, letter %zu: the chain follows the one-letter roofs to a"
                      " longer roof", FILES3[f], sd.name, x);
                chains += " " + std::to_string(x);
                for (int c : sd.through[x]) chains += "->" + std::to_string(c);
            }
            note("(%s %s: the letters with a one-letter roof, and their chains:%s)", FILES3[f], sd.name, chains.c_str());
        }

    section("at R = 0 nothing changes for _t3: the renderers with and without (b), bit for bit");
    {
        long ncases = 0, nleaves = 0;
        for (int f = 2; f < 3; f++)
            for (double acc : {0.04, 0.02, 0.01, 0.005}) {
                std::vector<E> roots;
                std::vector<double> rootd;
                z::cs::RenderPolicy::roots(0, roots, rootd);
                for (int s = 0; s < 2; s++) {
                    const z::World<NB>& w = W[f];
                    const z::World<NB>& w0 = W0[f];
                    z::Renderer<NB> a(s ? w.minus : w.plus, w.L, acc, 100000000), b(s ? w0.minus : w0.plus, w0.L, acc,
                                                                                     100000000);
                    for (size_t r = 0; r < roots.size(); r++)
                        for (size_t j = 0; j < w.L.s.size(); j++)
                            if (w.L.plus[j]) { a.draw(roots[r], (int)j, (int)r); b.draw(roots[r], (int)j, (int)r); }
                    check(same_renderer(a, b) && a.nThrough == 0, "%s %s acc %g: the same (%zu leaves, %zu, %zu split"
                          " through roofs)", FILES3[f], s ? "Z-" : "Z+", acc, a.nLeaves, b.nLeaves, a.nThrough);
                    ncases++;
                    nleaves += (long)a.nLeaves;
                }
            }
        note("(%ld cases, %ld leaves)", ncases, nleaves);
    }

    // the cases: (file, R, acc)
    struct Case { int f; double R, acc; };
    const std::vector<Case> cases = {{2, 2.554, 0.04}, {2, 2.554, 0.02}, {2, 3.327, 0.04}, {0, 0, 0.04}, {0, 0, 0.02},
                                     {1, 0, 0.04}, {1, 0, 0.02}};
    section("the rule written again: the renderer and arc_points with (b), bit for bit");
    {
        long ncases = 0, narcs = 0, nthrough = 0;
        for (const Case& c : cases) {
            std::vector<E> roots;
            std::vector<double> rootd;
            z::cs::RenderPolicy::roots(c.R, roots, rootd);
            for (int s = 0; s < 2; s++) {
                const z::World<NB>& w = W[c.f];
                const z::Side<NB>& sd = s ? w.minus : w.plus;
                z::Renderer<NB> a(sd, w.L, c.acc, 100000000);
                RefRenderer b(sd, w.L, c.acc, true);
                for (size_t r = 0; r < roots.size(); r++)
                    for (size_t j = 0; j < w.L.s.size(); j++)
                        if (w.L.plus[j]) { a.draw(roots[r], (int)j, (int)r); b.draw(roots[r], (int)j, (int)r); }
                bool same = a.pts == b.pts && a.starts == b.starts && a.polyroot == b.polyroot &&
                            a.polyletter == b.polyletter && a.nVisited == b.nVisited && a.nSkipped == b.nSkipped &&
                            a.nLeaves == b.nLeaves && a.nSpike == b.nSpike && a.nForced == b.nForced &&
                            a.deepest == b.deepest && a.nThrough == b.nThrough && a.done.n == b.done.n &&
                            a.done.t == b.done.t;
                check(same, "%s %s R=%g acc=%g: the renderer is the copy with the rule written again (%zu leaves, %zu;"
                      " %zu split through roofs, %zu)", FILES3[c.f], sd.name, c.R, c.acc, a.nLeaves, b.nLeaves,
                      a.nThrough, b.nThrough);
                ncases++;
                nthrough += (long)a.nThrough;
                if (c.acc < 0.04) continue;                   // keep it light: the arcs at ACC 0.04, every other letter
                for (size_t r = 0; r < roots.size(); r++)
                    for (size_t j = r % 2; j < w.L.s.size(); j += 2) {
                        std::vector<P4> pa, pb;
                        size_t fa = 0;
                        z::arc_points(sd, roots[r], (int)j, c.acc, pa, fa);
                        ref_arc(sd, roots[r], (int)j, 0, c.acc, pb);
                        check(rtest::same_vector_bytes(pa, pb), "%s %s R=%g acc=%g: arc_points of letter %zu at root %zu"
                              " (%zu points, %zu)", FILES3[c.f], sd.name, c.R, c.acc, j, r, pa.size(), pb.size());
                        narcs++;
                    }
            }
        }
        note("(%ld renderers, %ld of their branches split through one-letter roofs; %ld complete arcs)", ncases, nthrough,
             narcs);
    }

    section("what (b) changes: only leaves whose letter has a one-letter roof, split by (b)");
    for (const Case& c : cases) {
        double R = c.R, acc = c.acc;
        {
            std::vector<E> roots;
            std::vector<double> rootd;
            z::cs::RenderPolicy::roots(R, roots, rootd);
            for (int s = 0; s < 2; s++) {
                const z::World<NB>& w = W[c.f];
                const z::Side<NB>& sd = s ? w.minus : w.plus;
                RefRenderer on(sd, w.L, acc, true), off(sd, w.L, acc, false);
                for (size_t r = 0; r < roots.size(); r++)
                    for (size_t j = 0; j < w.L.s.size(); j++)
                        if (w.L.plus[j]) { on.draw(roots[r], (int)j, (int)r); off.draw(roots[r], (int)j, (int)r); }
                long gone = 0, gone_b = 0, gone_other = 0, fresh = 0, fresh_new = 0, fresh_other = 0;
                for (const auto& kv : off.leafletter) {
                    if (on.leafletter.count(kv.first)) continue;
                    gone++;
                    auto sp = on.splitby.find(kv.first);
                    bool byb = sd.roof[kv.second].size() == 1 && sp != on.splitby.end() && sp->second == 2;
                    gone_b += byb;
                    gone_other += !byb;
                }
                for (const auto& kv : on.leafletter) {
                    if (off.leafletter.count(kv.first)) continue;
                    fresh++;
                    bool unseen = !off.splitby.count(kv.first);
                    fresh_new += unseen;
                    fresh_other += !unseen;
                }
                check(gone_other == 0, "%s %s R=%g acc=%g: every leaf drawn only without (b) is one that (b) splits (%ld of"
                      " %ld are not)", FILES3[c.f], sd.name, R, acc, gone_other, gone);
                check(fresh_other == 0, "%s %s R=%g acc=%g: every leaf drawn only with (b) is a branch never visited"
                      " without it (%ld of %ld are not)", FILES3[c.f], sd.name, R, acc, fresh_other, fresh);
                long nested = 0;                          // (b) splits of branches never visited without (b)
                for (const auto& kv : on.splitby)
                    if (kv.second == 2 && !off.leafletter.count(kv.first) && !off.splitby.count(kv.first)) nested++;
                check(gone_b + nested == (long)on.nThrough, "%s %s R=%g acc=%g: the splits of (b) are the leaves gone (%ld)"
                      " and splits inside their refinements (%ld): %zu", FILES3[c.f], sd.name, R, acc, gone_b, nested,
                      on.nThrough);
                note("(%s %s R=%g acc=%g: %zu leaves without (b), %zu with; (b) splits %ld leaves of letters with a"
                     " one-letter roof (and %ld branches inside their refinements); %ld leaves are new, %ld of them"
                     " branches never visited without (b); the points %s)", FILES3[c.f], sd.name, R, acc, off.nLeaves,
                     on.nLeaves, gone_b, nested, fresh, fresh_new, off.pts == on.pts ? "the same" : "differ");
            }
        }
    }

    rtest::end_section();
    printf("test_lookthrough_cs: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
