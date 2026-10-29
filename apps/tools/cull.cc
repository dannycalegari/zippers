// SPDX-License-Identifier: GPL-3.0-or-later
/*  cull.cc  --  the measurements of the deep zoom in S^3 and S^4: culling and the focus ball

    It changes nothing: the renderer, its culling, the views and the focus centres are the library's (src/).

    THE MODES
      margin   (to choose K) how far the curve of a branch strays from the points of its cull test, in units of its
               size: render/renderer.h, cull_points (its ends a and b, the vertices of its first split, its tips; for a
               letter with a one-letter roof, those of its chain), and the SIZE there (the chord, or the farthest of
               those vertices and tips from it); also in units of the size without the tips (s: the chord or the
               farthest vertex, the renderer's), and, from (a, vertices, b) alone, what a test that ignored the tips
               would need; and for one-letter roofs, from their own chord alone, what a test that ignored the chain
               would need. The branches are those of the picture's trees: from each root (g, j) that build
               draws, split by the renderer's rule at ACC, each distinct branch once by the renderer's key (as the
               renderer meets it); all those of depth <= --alldepth and one in --sample of the deeper ones, chosen by a
               hash of the key. Each is refined by the renderer's rule at s/k, each distinct ORIENTED branch (g, y) of the
               refinement once (one curve; the two orientations of a key can be different arcs in S^4:
               docs/developer_guide.md). Its STRAY is the largest distance of a point of the refinement (the ends and tips of its
               leaves) from the two polylines of the branch's points, (a, vertices, b) and (a, tips, b). That is at least
               the distance from their convex hull, which is what the cull test needs (render/cull.h): the margin
               K size is safe for the branch when K >= stray/size. And it is at most the true stray plus about the
               strays of the refinement's own leaves (about 3 s/k; the refinement is at s/k, s without the tips).
               Printed: the largest stray/size, its quantiles, by size and by depth, and the worst branches; and the line
               "cullable: stray / size", one column for each margin factor K of a list (2, 4, 8, 16, 32, the default CULL_K
               of render/cull.h, and --cullk if it is given): "(K = 16: size < 1/16)" is the largest stray/size among the
               branches that a window can cull at all at that K, those of size below 1/K, and the safety factor of K is K
               over it.
      window   the same picture with and without culling (World::cull), at the same accuracy on the sphere (and the same
               roots, with the focus ball if --focus), compared in the window of the view:
                 - the segments DRAWN in the window: both ends pass the clip ball and the segment meets the screen square,
                   as the painters draw them (view/s3.h, view/s4.h), from the renderers' float points; matched exactly
                   (the same two points, in either order): those without culling that are missing with it, and those with
                   culling that are not drawn without it;
                 - the accuracy: every vertex of the picture without culling that lies in the window (render/cull.h:
                   CullWindow::contains, the exact pull-back), and its distance from the nearest segment of the picture
                   with culling (0 when it is a vertex of it), in units of ACC (searched to 4 ACC);
                 - what culling keeps outside the window: its segments not drawn there.
      focus    the focus centre (examples/imm_m5.h: focus_centre5; examples/cartwright_steger.h: focus_centre3) for the
               view, at --focus M (default 1 and 2) and --zoom F.
      density  (to choose the focus distance) the picture with culling, and with the focus ball at --focus M if M > 0:
               the segments drawn in the window, those of them from the roots of B(R) and from the roots of the focus
               ball, the distinct roots that draw in the window, and the time of the build.
      tips     the cusp points of the sides' tips (render/tips.h), each with the letters that have it, and its point of
               the sphere.

    usage: cull -e cs|imm [options]
      -e cs|imm        the example: the Cartwright-Steger surface (S^3) or IMM's M5 (S^4)
      -f FILE          its substitution file (default subst_4.00_9.50_t3_r1.txt or subst5_2.30_4.50_t1_r0.txt in the working
                       folder, as zipper3 and zipper5: the files of data/s3/ and data/s4/)
      --mode margin|window|focus|density|tips
      -R R             the radius of the roots (default 0)
      -a ACC           the accuracy (default 0.02); with --zoomacc ACC/zoom on the sphere, as the programs
      --only +|-       one side only (default both)
      the view, as zipper3 and zipper5 take it: --zoom F, --clip RHO, --centre P (S^3: x1,..,x4; S^4: x1,..,x5 or a null
               vector x1,..,x6), --yaw D, --pitch D (defaults 0 and 20 in S^3, 0 and 15 in S^4), --pole D1 D2 (S^3),
               --turn D, --turn2 D (S^4)
      --cullk K        the margin factor (default 16: render/cull.h, CULL_K); margin mode: a column for it as well
      --zoomacc        draw at ACC/zoom on the sphere (both pictures)
      --focus M        window mode: both pictures with the focus ball at M log(zoom); focus mode: only M
      --overlap L      window mode: both pictures with the renderer's marking, L levels (render/renderer.h)
      -k K             margin mode: the refinement at s/K (default 16)
      --alldepth D     margin mode: every branch of depth <= D (default 2)
      --sample N       margin mode: one in N of the deeper branches (default 16)
      --refcap N       margin mode: the most leaves of one refinement (default 400000; a capped one is counted)
      --worst N        margin mode: the worst branches listed (default 8)
      --cap N          the renderers' leaf cap (default 2e7)
      --memlimit GB    stop (exit status 3) when the process uses more (default 1.5)
    Timings and memory go to stderr only.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "core/hash.h"
#include "core/memory.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/backend.h"
#include "geometry/sphere.h"
#include "render/cull.h"
#include "render/renderer.h"
#include "render/tips.h"
#include "render/world.h"
#include "view/s3.h"
#include "view/s4.h"

#include "default_file.h"
#include "leafcopy.h"

#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using namespace zippers;
using namespace zippers::tools;

namespace {

struct Options {
    std::string example, subst, mode = "margin";
    double R = 0, acc = 0.02, zoom = 1, clip = 3, yaw = 0, pitch = -1, pole1 = 0, pole2 = 0, turn = 0, turn2 = 0;
    double cullk = CULL_K, focus = 0, memlimit = 1.5;
    bool centre = false, zoomacc = false;
    std::vector<double> c;
    int which = 3, overlap = 0, k = 16, alldepth = 2, sample = 16, worst = 8;
    size_t refcap = 400000, cap = 20000000;
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
        fprintf(stderr, "cull: %.2f GB in use (%s), more than --memlimit %.2f GB: stopping\n", b/1e9, where, o.memlimit);
        exit(3);
    }
}

bool parse_numbers(const std::string& s, std::vector<double>& v)
{
    v.clear();
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find(',', i);
        if (j == std::string::npos) j = s.size();
        std::string t = s.substr(i, j - i);
        char* end = nullptr;
        double x = strtod(t.c_str(), &end);
        if (t.empty() || *end) return false;
        v.push_back(x);
        i = j + 1;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The views, set up as zipper3 and zipper5 set them up (leafcopy.h: View<N>::program)

ViewOptions view_options(const Options& o)
{
    ViewOptions w;
    w.zoom = o.zoom; w.clip = o.clip; w.yaw = o.yaw; w.pitch = o.pitch; w.pole1 = o.pole1; w.pole2 = o.pole2;
    w.turn = o.turn; w.turn2 = o.turn2;
    if (o.centre) w.centre = o.c;
    return w;
}

// the focus element for the view centre xi at the distance L, and a line saying what it is
M3 focus_element(const World<PU21>&, const std::vector<double>& xi, double L, std::string& line)
{
    cs::FocusCentre3 fc = cs::focus_centre3(xi.data(), L, cs::geogens(4.5));
    char buf[400];
    snprintf(buf, sizeof buf, "L = %.4f: d(o, h o) = %.4f, d(h o, o_F) = %.4f, %d steps; h = %s", L, fc.disp, fc.dist,
             fc.steps, fc.word.size() > 120 ? (fc.word.substr(0, 117) + "...").c_str() : fc.word.c_str());
    line = buf;
    return fc.h;
}
imm::M6 focus_element(const World<imm::O51>&, const std::vector<double>& xi, double L, std::string& line)
{
    imm::FocusCentre fc = imm::focus_centre5(xi.data(), L);
    char buf[400];
    snprintf(buf, sizeof buf, "L = %.4f: d(o, h o) = %.4f, d(h o, o_F) = %.4f; %d facets to the tile of o_F, %zu more "
             "to Gamma; h = %s", L, fc.disp, fc.dist, fc.walk, fc.word.size() - (size_t)fc.walk,
             fc.word.size() > 60 ? "(long)" : imm::fword(fc.word).c_str());
    line = buf;
    return fc.h;
}

// is the segment [p, q] drawn in the window? Both ends pass the clip ball, and its picture meets the screen square
// (Liang-Barsky), as the painters draw it
template <int N>
bool drawn(const typename View<N>::V& v, const SpherePoint<N>& p, const SpherePoint<N>& q)
{
    double e0[4], e1[4];
    if (!View<N>::eye(v, p, e0) || !View<N>::eye(v, q, e1)) return false;
    double H = View<N>::half(v), t0 = 0, t1 = 1, dx = e1[0] - e0[0], dy = e1[1] - e0[1];
    auto clip = [&](double pp, double qq) {
        if (pp == 0) return qq >= 0;
        double r = qq/pp;
        if (pp < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
        else { if (r < t0) return false; if (r < t1) t1 = r; }
        return true;
    };
    return clip(-dx, e0[0] + H) && clip(dx, H - e0[0]) && clip(-dy, e0[1] + H) && clip(dy, H - e0[1]);
}

template <int N>
SpherePoint<N> fpoint(const std::vector<float>& pts, size_t g)
{
    SpherePoint<N> p;
    for (int i = 0; i < N; i++) p.x[i] = (double)pts[(size_t)N*g + i];
    return p;
}

// the quantile of rank floor(q (n - 1)) of sorted values
double quantile(const std::vector<double>& s, double q)
{
    if (s.empty()) return 0;
    return s[(size_t)std::floor(q*(double)(s.size() - 1))];
}

// ---------------------------------------------------------------------------------------------------------------------
// Margin mode

template <class B>
struct Branch { typename B::Elem h; int j, depth; };

// the branches of the picture's trees (the top of the file), the sample of them, and their count
template <class P>
std::vector<Branch<typename P::B>> picture_branches(const World<typename P::B>& W, const Side<typename P::B>& sd,
                                                    const Options& o, double acc, size_t& total)
{
    typedef typename P::B B;
    std::vector<Branch<B>> out;
    KeySet done;
    total = 0;
    std::vector<Branch<B>> st;
    for (size_t r = 0; r < W.roots.size(); r++)
        for (size_t j0 = 0; j0 < W.L.s.size(); j0++) {
            if (!W.L.plus[j0]) continue;
            st.clear();
            st.push_back({W.roots[r], (int)j0, 0});
            while (!st.empty()) {
                Branch<B> f = st.back();
                st.pop_back();
                uint64_t key = branch_key<B>(W.L, f.h, f.j);
                if (!done.insert(key)) continue;
                total++;
                if (f.depth <= o.alldepth || splitmix(key) % (uint64_t)o.sample == 0) out.push_back(f);
                typename B::DMat hd = B::to_double(f.h);
                typename B::Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
                const std::vector<typename B::DVec>& wv = sd.wv[f.j];
                bool split = chord(a, b) > acc;
                if (!split)
                    for (size_t i = 1; i < wv.size(); i++)
                        if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; break; }
                if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40)
                    && through_split(sd, f.h, f.j, a, b, acc)) split = true;
                if (split && f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40))
                    for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                        st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1});
            }
        }
    return out;
}

template <class B>
struct Stray {
    int j = 0, depth = 0;
    double s = 0, sfull = 0, d = 0;     // the size, with the tips, the stray
    double dA = 0;                      // the stray from (a, vertices, b) alone: what a test without the tips would see
    double dO = 0, sO = 0;              // for a one-letter roof: the stray from its own chord [a, b], and the chord: what a
                                        // test without the chain would see
    size_t leaves = 0;
    bool capped = false, tips = false, one = false;
};

// the stray of the branch (h0, j0) (the top of the file)
template <class B>
Stray<B> branch_stray(const Side<B>& sd, const Branch<B>& br, const Options& o, StampSet& seen,
                      std::vector<Branch<B>>& st)
{
    typedef typename B::Point Point;
    Stray<B> r;
    r.j = br.j; r.depth = br.depth;
    r.one = sd.roof[br.j].size() == 1;
    typename B::DMat hd = B::to_double(br.h);
    Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[br.j]));
    std::vector<Point> verts, tips;
    double size;
    if (!cull_points(sd, br.h, hd, br.j, a, b, verts, tips, size)) { r.s = -1; return r; }
    // s: the size without the tips (the renderer's, from the same vertices); sfull: the cull's size, with them
    double s = chord(a, b);
    for (const Point& v : verts) s = std::max(s, segment_distance(v, a, b));
    r.s = s;
    r.tips = !tips.empty();
    r.sfull = size;
    std::vector<Point> A = {a}, T = {a};
    A.insert(A.end(), verts.begin(), verts.end());
    A.push_back(b);
    T.insert(T.end(), tips.begin(), tips.end());
    T.push_back(b);
    r.sO = chord(a, b);
    auto distA = [&](const Point& q) {
        double d = INFINITY;
        for (size_t i = 0; i + 1 < A.size(); i++) d = std::min(d, segment_distance(q, A[i], A[i + 1]));
        return d;
    };
    auto dist = [&](const Point& q) {
        double d = distA(q);
        if (tips.size())
            for (size_t i = 0; i + 1 < T.size(); i++) d = std::min(d, segment_distance(q, T[i], T[i + 1]));
        return d;
    };
    const double tau = s/o.k;
    seen.clear();
    st.clear();
    st.push_back({br.h, br.j, 0});
    while (!st.empty()) {
        Branch<B> f = st.back();
        st.pop_back();
        if (!seen.insert(fingerprint(B::key(f.h), f.j))) continue;      // each oriented branch once
        typename B::DMat fd = B::to_double(f.h);
        Point fa = B::boundary(B::act(fd, sd.vfix)), fb = B::boundary(B::act(fd, sd.ev[f.j]));
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(fa, fb) > tau;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(fd, wv[i])), fa, fb) > tau) { split = true; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, fa, fb, tau)) split = true;
        if (split && br.depth + f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40)) {
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1});
            continue;
        }
        r.d = std::max(r.d, std::max(dist(fa), dist(fb)));
        r.dA = std::max(r.dA, std::max(distA(fa), distA(fb)));
        if (r.one) r.dO = std::max(r.dO, std::max(segment_distance(fa, a, b), segment_distance(fb, a, b)));
        if (!sd.tipline.empty())
            for (const typename B::DVec& v : sd.tipline[f.j]) {
                Point q = B::boundary(B::act(fd, v));
                r.d = std::max(r.d, dist(q));
                r.dA = std::max(r.dA, distA(q));
            }
        if (++r.leaves > o.refcap) { r.capped = true; break; }
    }
    return r;
}

// the margin factors K whose "cullable" columns the margin mode prints: 2, 4, 8, 16 and 32, the default CULL_K of the
// programs (render/cull.h) whatever it is, and the K that --cullk gives, in increasing order, each once
std::vector<double> margin_factors(const Options& o)
{
    std::vector<double> Ks = {2, 4, 8, 16, 32, CULL_K, o.cullk};
    std::sort(Ks.begin(), Ks.end());
    Ks.erase(std::unique(Ks.begin(), Ks.end()), Ks.end());
    return Ks;
}

template <class P>
void margin_side(const World<typename P::B>& W, const Side<typename P::B>& sd, const Options& o, double acc)
{
    typedef typename P::B B;
    double t0 = now();
    size_t total = 0;
    std::vector<Branch<B>> br = picture_branches<P>(W, sd, o, acc, total);
    StampSet seen;
    std::vector<Branch<B>> st;
    std::vector<Stray<B>> res;
    size_t undecided = 0, capped = 0, refleaves = 0;
    for (size_t i = 0; i < br.size(); i++) {
        Stray<B> r = branch_stray<B>(sd, br[i], o, seen, st);
        if (r.s < 0) { undecided++; continue; }
        if (r.capped) capped++;
        refleaves += r.leaves;
        res.push_back(r);
        if ((i & 255) == 0) memcheck(o, "margin");
    }
    printf("%s: %zu branches in the picture's trees at ACC %.4g; %zu measured (all of depth <= %d, 1 in %d deeper), "
           "%zu undecided (growth guard), %zu refinements capped at %zu leaves; refined at s/%d: %zu leaves\n",
           sd.name, total, acc, res.size(), o.alldepth, o.sample, undecided, capped, o.refcap, o.k, refleaves);
    if (res.empty()) return;
    auto table = [&](const char* what, auto ratio) {
        std::vector<double> v;
        for (const Stray<B>& r : res) v.push_back(ratio(r));
        std::sort(v.begin(), v.end());
        size_t over[4] = {0, 0, 0, 0};
        const double lim[4] = {1, 2, 2.7, 4};
        for (double x : v) for (int q = 0; q < 4; q++) over[q] += x > lim[q];
        printf("  %-22s max %.3f   q50 %.3f  q90 %.3f  q99 %.3f  q99.9 %.3f   > 1: %zu  > 2: %zu  > 2.7: %zu  > 4: %zu\n",
               what, v.back(), quantile(v, 0.5), quantile(v, 0.9), quantile(v, 0.99), quantile(v, 0.999), over[0],
               over[1], over[2], over[3]);
    };
    table("stray / size", [](const Stray<B>& r) { return r.d/r.sfull; });
    {
        // the branches that a window can cull at all: one beyond a half-space u.x <= c by more than K size needs
        // K size < 1 - c < 1 (a point of the sphere has u.x <= 1, and every window here has c > 0 or so); so at the
        // margin factor K only the branches of size < 1/K can ever be culled, and the bet matters only for them: the
        // safety factor of K is K over the largest stray / size among them. A column for each K of the list, which
        // always has CULL_K among its values.
        const std::vector<double> Ks = margin_factors(o);
        std::vector<double> mK(Ks.size(), 0);
        for (const Stray<B>& r : res)
            for (size_t q = 0; q < Ks.size(); q++) if (Ks[q]*r.sfull < 1) mK[q] = std::max(mK[q], r.d/r.sfull);
        printf("  %-22s max", "cullable: stray / size");
        for (size_t q = 0; q < Ks.size(); q++) printf("%s %.3f (K = %g: size < 1/%g)", q ? "," : "", mK[q], Ks[q], Ks[q]);
        printf("\n");
    }
    table("stray / size w/o tips", [](const Stray<B>& r) { return r.d/r.s; });
    table("no tips / size w/o tips", [](const Stray<B>& r) { return r.dA/r.s; });
    // by size (decades of s) and by depth
    std::map<int, std::pair<size_t, double>> bys, byd;
    for (const Stray<B>& r : res) {
        int e = (int)std::floor(std::log10(r.sfull));
        auto& x = bys[e];
        x.first++; x.second = std::max(x.second, r.d/r.sfull);
        auto& y = byd[r.depth];
        y.first++; y.second = std::max(y.second, r.d/r.sfull);
    }
    printf("  stray / size, by size:");
    for (auto& x : bys) printf("  [1e%d, 1e%d): %zu, max %.3f;", x.first, x.first + 1, x.second.first, x.second.second);
    printf("\n  stray / size, by depth:");
    for (auto& x : byd) printf("  %d: %zu, max %.3f;", x.first, x.second.first, x.second.second);
    printf("\n");
    // the classes: with tips, with a one-letter roof, the others
    for (int cls = 0; cls < 3; cls++) {
        double m = 0;
        size_t n = 0;
        for (const Stray<B>& r : res)
            if ((cls == 0 && r.tips) || (cls == 1 && r.one) || (cls == 2 && !r.tips && !r.one)) { n++; m = std::max(m, r.d/r.sfull); }
        printf("  %s: %zu, max stray / size %.3f\n", cls == 0 ? "letters with tips" : cls == 1 ? "one-letter roofs" : "the others",
               n, m);
    }
    {
        double m = 0;
        for (const Stray<B>& r : res) if (r.one) m = std::max(m, r.dO/r.sO);
        printf("  one-letter roofs from their own chord alone (no chain): max stray/chord %.3f\n", m);
    }
    std::vector<size_t> idx(res.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return res[a].d/res[a].sfull > res[b].d/res[b].sfull; });
    printf("  the worst:\n");
    for (int w = 0; w < o.worst && w < (int)idx.size(); w++) {
        const Stray<B>& r = res[idx[w]];
        printf("    letter %d, depth %d%s%s: size %.4g (%.4g without the tips), stray %.4g = %.3f sizes, %zu leaves%s\n",
               r.j, r.depth, r.tips ? ", tips" : "", r.one ? ", one-letter roof" : "", r.sfull, r.s, r.d, r.d/r.sfull,
               r.leaves,
               r.capped ? " (capped)" : "");
    }
    fprintf(stderr, "%s: margin in %.2fs, peak %.0f MB\n", sd.name, now() - t0, peak_mb());
}

// ---------------------------------------------------------------------------------------------------------------------
// Window mode

// a segment as two float points in a canonical order, for exact matching
template <int N>
using Seg = std::array<float, 2*N>;

template <int N>
Seg<N> seg_of(const std::vector<float>& pts, size_t g)
{
    Seg<N> s;
    const float* p = &pts[(size_t)N*g];
    const float* q = &pts[(size_t)N*(g + 1)];
    bool swap = std::lexicographical_compare(q, q + N, p, p + N);
    for (int i = 0; i < N; i++) { s[i] = swap ? q[i] : p[i]; s[N + i] = swap ? p[i] : q[i]; }
    return s;
}

// the segments of a renderer: (all, drawn in the window)
template <class B>
void segments(const Renderer<B>& r, const typename View<B::N>::V& v, std::vector<Seg<B::N>>& all,
              std::vector<Seg<B::N>>& in)
{
    constexpr int N = B::N;
    all.clear(); in.clear();
    for (size_t k = 0; k < r.npoly(); k++)
        for (size_t g = r.pbegin(k); g + 1 < r.pend(k); g++) {
            Seg<N> s = seg_of<N>(r.pts, g);
            all.push_back(s);
            if (drawn<N>(v, fpoint<N>(r.pts, g), fpoint<N>(r.pts, g + 1))) in.push_back(s);
        }
}

// the nearest segment of a picture to a point, searched to a radius: a hashed grid of cells of side c over the boxes
// of the segments (a segment more than 64 cells across in some coordinate goes in a list that every query checks)
template <int N>
struct NearGrid {
    double c;
    const std::vector<Seg<N>>& segs;
    std::unordered_map<uint64_t, std::vector<uint32_t>> cells;
    std::vector<uint32_t> longs;
    static uint64_t key(const int64_t* ci)
    {
        uint64_t h = 0x12345;
        for (int i = 0; i < N; i++) h = splitmix(h ^ (uint64_t)ci[i]);
        return h;
    }
    NearGrid(const std::vector<Seg<N>>& s, double cell) : c(cell), segs(s)
    {
        for (uint32_t g = 0; g < segs.size(); g++) {
            int64_t lo[N], hi[N];
            bool big = false;
            for (int i = 0; i < N; i++) {
                double a = segs[g][i], b = segs[g][N + i];
                lo[i] = (int64_t)std::floor(std::min(a, b)/c);
                hi[i] = (int64_t)std::floor(std::max(a, b)/c);
                if (hi[i] - lo[i] > 64) big = true;
            }
            if (big) { longs.push_back(g); continue; }
            each(lo, hi, [&](const int64_t* ci) { cells[key(ci)].push_back(g); });
        }
    }
    template <class F>
    static void each(const int64_t* lo, const int64_t* hi, F&& f)
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
    static double dist(const SpherePoint<N>& p, const Seg<N>& s)
    {
        SpherePoint<N> a, b;
        for (int i = 0; i < N; i++) { a.x[i] = s[i]; b.x[i] = s[N + i]; }
        return segment_distance(p, a, b);
    }
    // the distance from p to the nearest segment, if it is at most r; INFINITY otherwise
    double nearest(const SpherePoint<N>& p, double r) const
    {
        int64_t lo[N], hi[N];
        for (int i = 0; i < N; i++) { lo[i] = (int64_t)std::floor((p.x[i] - r)/c); hi[i] = (int64_t)std::floor((p.x[i] + r)/c); }
        double best = INFINITY;
        each(lo, hi, [&](const int64_t* ci) {
            auto it = cells.find(key(ci));
            if (it == cells.end()) return;
            for (uint32_t g : it->second) best = std::min(best, dist(p, segs[g]));
        });
        for (uint32_t g : longs) best = std::min(best, dist(p, segs[g]));
        return best <= r ? best : INFINITY;
    }
};

template <class P>
int window_mode(const Options& o)
{
    typedef typename P::B B;
    constexpr int N = B::N;
    typedef typename View<N>::V V;
    double t0 = now();
    World<B> W0, W1;
    prepare<P>(o.subst.c_str(), W0);
    prepare<P>(o.subst.c_str(), W1);
    V v;
    View<N>::program(W0, view_options(o), v);
    CullWindow<N> win = View<N>::window(v, o.cullk);
    W1.cull = &win;
    if (o.focus > 0) {
        std::string line;
        typename B::Elem h = focus_element(W0, View<N>::centre(v), o.focus*std::log(o.zoom), line);
        W0.focus = W1.focus = true;
        W0.fh = W1.fh = h;
        printf("focus: %s\n", line.c_str());
    }
    double acc = o.zoomacc ? o.acc/o.zoom : o.acc;
    default_marking.overlap = o.overlap;
    double tb = now();
    build<P>(W0, o.R, acc, o.cap, o.which);
    double t_unculled = now() - tb;
    memcheck(o, "the picture without culling");
    tb = now();
    build<P>(W1, o.R, acc, o.cap, o.which);
    double t_culled = now() - tb;
    default_marking = Marking();
    memcheck(o, "the picture with culling");
    printf("window: zoom %g, clip %g, half-width %.4g; %d half-spaces; K = %g; ACC %.4g on the sphere; R = %g, %zu roots "
           "(%zu of B(R)); build %.2fs without culling, %.2fs with\n", o.zoom, o.clip, View<N>::half(v), win.n, win.K,
           acc, o.R, W0.roots.size(), W0.nmain, t_unculled, t_culled);
    bool fail = false;
    for (int side = 0; side < 2; side++) {
        if (!(o.which & (1 << side))) continue;
        const Renderer<B>& r0 = side ? *W0.rn : *W0.rp;
        const Renderer<B>& r1 = side ? *W1.rn : *W1.rp;
        std::vector<Seg<N>> all0, in0, all1, in1;
        segments<B>(r0, v, all0, in0);
        segments<B>(r1, v, all1, in1);
        std::vector<Seg<N>> s0 = in0, s1 = in1, a1 = all1;
        std::sort(s0.begin(), s0.end()); s0.erase(std::unique(s0.begin(), s0.end()), s0.end());
        std::sort(s1.begin(), s1.end()); s1.erase(std::unique(s1.begin(), s1.end()), s1.end());
        size_t missing = 0, extra = 0;
        for (const Seg<N>& s : s0) if (!std::binary_search(s1.begin(), s1.end(), s)) missing++;
        for (const Seg<N>& s : s1) if (!std::binary_search(s0.begin(), s0.end(), s)) extra++;
        // the accuracy: every vertex of the picture without culling in the window, from the picture with culling
        std::vector<std::array<float, N>> v1;
        for (size_t g = 0; g < r1.pts.size()/N; g++) {
            std::array<float, N> x;
            for (int i = 0; i < N; i++) x[i] = r1.pts[(size_t)N*g + i];
            v1.push_back(x);
        }
        std::sort(v1.begin(), v1.end());
        NearGrid<N> grid(a1, 2*acc);
        size_t nin = 0, notvertex = 0, far = 0;
        double worst = 0;
        for (size_t g = 0; g < r0.pts.size()/N; g++) {
            SpherePoint<N> p = fpoint<N>(r0.pts, g);
            if (!win.contains(p)) continue;
            nin++;
            std::array<float, N> x;
            for (int i = 0; i < N; i++) x[i] = r0.pts[(size_t)N*g + i];
            if (std::binary_search(v1.begin(), v1.end(), x)) continue;
            notvertex++;
            double d = grid.nearest(p, 4*acc);
            if (!(d < INFINITY)) { far++; worst = INFINITY; }
            else worst = std::max(worst, d);
        }
        size_t outside = all1.size() - in1.size();
        printf("%s: without culling %zu leaves, %zu points, %zu segments, %zu drawn in the window; with culling %zu leaves, "
               "%zu points, %zu culled, %zu segments, %zu drawn in the window\n", r0.sd.name, r0.nLeaves, r0.pts.size()/N,
               all0.size(), in0.size(), r1.nLeaves, r1.pts.size()/N, r1.nCulled, all1.size(), in1.size());
        printf("%s: drawn in the window: %zu distinct without culling, %zu with; MISSING with culling: %zu; only with "
               "culling: %zu\n", r0.sd.name, s0.size(), s1.size(), missing, extra);
        printf("%s: the vertices without culling in the window: %zu, of which %zu are not vertices with culling; the "
               "farthest of them from the picture with culling: %s ACC%s\n", r0.sd.name, nin, notvertex,
               worst < INFINITY ? (std::to_string(worst/acc)).c_str() : "> 4", far ? (" (" + std::to_string(far) +
               " farther than 4 ACC)").c_str() : "");
        printf("%s: culling keeps %zu segments outside the window (%.1f%% of its segments; %.2f per segment drawn in it)\n",
               r0.sd.name, outside, all1.empty() ? 0.0 : 100.0*(double)outside/(double)all1.size(),
               in1.empty() ? 0.0 : (double)outside/(double)in1.size());
        if (missing) fail = true;
    }
    fprintf(stderr, "window: %.2fs, peak %.0f MB\n", now() - t0, peak_mb());
    return fail ? 2 : 0;
}

// ---------------------------------------------------------------------------------------------------------------------

template <class P>
int density_mode(const Options& o)
{
    typedef typename P::B B;
    constexpr int N = B::N;
    typedef typename View<N>::V V;
    World<B> W;
    prepare<P>(o.subst.c_str(), W);
    V v;
    View<N>::program(W, view_options(o), v);
    CullWindow<N> win = View<N>::window(v, o.cullk);
    W.cull = &win;
    std::string line = "none";
    if (o.focus > 0) {
        W.focus = true;
        W.fh = focus_element(W, View<N>::centre(v), o.focus*std::log(o.zoom), line);
    }
    double acc = o.zoomacc ? o.acc/o.zoom : o.acc;
    double t0 = now();
    build<P>(W, o.R, acc, o.cap, o.which);
    double tb = now() - t0;
    memcheck(o, "the picture");
    printf("density: zoom %g, ACC %.4g on the sphere, R = %g: %zu roots (%zu of B(R)); focus %g: %s; build %.2fs\n",
           o.zoom, acc, o.R, W.roots.size(), W.nmain, o.focus, line.c_str(), tb);
    for (int side = 0; side < 2; side++) {
        if (!(o.which & (1 << side))) continue;
        const Renderer<B>& r = side ? *W.rn : *W.rp;
        size_t inw = 0, inmain = 0, infocus = 0;
        std::vector<char> rootin(W.roots.size(), 0);
        for (size_t k = 0; k < r.npoly(); k++)
            for (size_t g = r.pbegin(k); g + 1 < r.pend(k); g++)
                if (drawn<N>(v, fpoint<N>(r.pts, g), fpoint<N>(r.pts, g + 1))) {
                    inw++;
                    int root = r.polyroot[k];
                    if (root >= 0 && (size_t)root < W.nmain) inmain++; else infocus++;
                    if (root >= 0) rootin[(size_t)root] = 1;
                }
        size_t rmain = 0, rfocus = 0;
        for (size_t i = 0; i < rootin.size(); i++) if (rootin[i]) (i < W.nmain ? rmain : rfocus)++;
        printf("%s: %zu leaves, %zu culled; drawn in the window: %zu segments, %zu from B(R) and %zu from the focus ball; "
               "roots drawing in it: %zu of B(R), %zu of the focus ball\n", r.sd.name, r.nLeaves, r.nCulled, inw, inmain,
               infocus, rmain, rfocus);
    }
    return 0;
}

template <class P>
int run(const Options& o)
{
    typedef typename P::B B;
    constexpr int N = B::N;
    if (o.mode == "window") return window_mode<P>(o);
    if (o.mode == "density") return density_mode<P>(o);
    World<B> W;
    prepare<P>(o.subst.c_str(), W);
    typename View<N>::V v;
    View<N>::program(W, view_options(o), v);
    if (o.mode == "focus") {
        std::vector<double> xi = View<N>::centre(v);
        printf("the centre of the view:");
        for (double x : xi) printf(" %.9f", x);
        printf("\n");
        for (double M : o.focus > 0 ? std::vector<double>{o.focus} : std::vector<double>{1, 2}) {
            std::string line;
            double t0 = now();
            focus_element(W, xi, M*std::log(o.zoom), line);
            printf("zoom %g, M = %g: %s\n", o.zoom, M, line.c_str());
            fprintf(stderr, "  (%.2fs)\n", now() - t0);
        }
        return 0;
    }
    if (o.mode == "tips") {
        if constexpr (N == 5) {
            for (const Side<B>* sd : {&W.plus, &W.minus}) {
                Tips<5> T = find_tips(*sd, 4);
                std::map<std::array<int64_t, 6>, std::vector<int>> at;
                for (size_t j = 0; j < T.of.size(); j++)
                    for (const Tip<5>& t : T.of[j]) at[t.P].push_back((int)j);
                printf("%s: %zu tips on %zu letters, at %zu points\n", sd->name, T.count(), T.letters(), at.size());
                for (auto& x : at) {
                    printf("  P = (");
                    for (int i = 0; i < 6; i++) printf("%s%lld", i ? "," : "", (long long)x.first[i]);
                    printf("): %zu letters:", x.second.size());
                    for (int j : x.second) printf(" %d", j);
                    printf("\n");
                }
            }
        } else printf("no tips in S^3 (Pi is cocompact)\n");
        return 0;
    }
    if (o.mode != "margin") { fprintf(stderr, "cull: unknown mode %s\n", o.mode.c_str()); return 1; }
    double acc = o.zoomacc ? o.acc/o.zoom : o.acc;
    P::roots(o.R, W.roots, W.rootd);
    printf("margin: R = %g (%zu roots), ACC %.4g, refined at s/%d\n", o.R, W.roots.size(), acc, o.k);
    if (o.which & 1) margin_side<P>(W, W.plus, o, acc);
    if (o.which & 2) margin_side<P>(W, W.minus, o, acc);
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { fprintf(stderr, "cull: %s needs a value\n", a.c_str()); exit(1); }
            return argv[++i];
        };
        if (a == "-e") o.example = next();
        else if (a == "-f") o.subst = next();
        else if (a == "--mode") o.mode = next();
        else if (a == "-R") o.R = atof(next().c_str());
        else if (a == "-a") o.acc = atof(next().c_str());
        else if (a == "--only") { std::string w = next(); o.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--zoom") o.zoom = atof(next().c_str());
        else if (a == "--clip") o.clip = atof(next().c_str());
        else if (a == "--centre") {
            if (!parse_numbers(next(), o.c)) { fprintf(stderr, "cull: --centre: numbers x1,x2,...\n"); return 1; }
            o.centre = true;
        }
        else if (a == "--yaw") o.yaw = atof(next().c_str());
        else if (a == "--pitch") o.pitch = atof(next().c_str());
        else if (a == "--pole") { o.pole1 = atof(next().c_str()); o.pole2 = atof(next().c_str()); }
        else if (a == "--turn") o.turn = atof(next().c_str());
        else if (a == "--turn2") o.turn2 = atof(next().c_str());
        else if (a == "--cullk") o.cullk = atof(next().c_str());
        else if (a == "--zoomacc") o.zoomacc = true;
        else if (a == "--focus") o.focus = atof(next().c_str());
        else if (a == "--overlap") o.overlap = atoi(next().c_str());
        else if (a == "-k") o.k = atoi(next().c_str());
        else if (a == "--alldepth") o.alldepth = atoi(next().c_str());
        else if (a == "--sample") o.sample = std::max(1, atoi(next().c_str()));
        else if (a == "--refcap") o.refcap = (size_t)atof(next().c_str());
        else if (a == "--worst") o.worst = atoi(next().c_str());
        else if (a == "--cap") o.cap = (size_t)atof(next().c_str());
        else if (a == "--memlimit") o.memlimit = atof(next().c_str());
        else { fprintf(stderr, "cull: unknown option %s (the usage is at the top of apps/tools/cull.cc)\n", a.c_str()); return 1; }
    }
    if (o.example != "cs" && o.example != "imm") { fprintf(stderr, "cull: -e cs|imm\n"); return 1; }
    const int n = o.example == "cs" ? 4 : 5;
    if (o.centre && !((int)o.c.size() == n || (n == 5 && o.c.size() == 6))) {
        fprintf(stderr, "cull: --centre takes %d numbers%s\n", n, n == 5 ? " (or a null vector of 6)" : "");
        return 1;
    }
    if (o.centre && o.c.size() == 6) o.c.resize(5);           // a null vector: the point x/x6, normalised below
    if (o.subst.empty()) {                         // the programs' default; it is not in the working folder by itself
        o.subst = tools::default_subst(o.example == "cs");
        if (!tools::default_subst_readable(o.subst, o.example == "cs")) return 1;
    }
    if (o.example == "cs") {
        cs::setup();
        return run<cs::RenderPolicy>(o);
    }
    imm::setup();
    return run<imm::RenderPolicy>(o);
}
