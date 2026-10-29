// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_cull: the deep zoom in the library: culling (src/render/cull.h, src/render/renderer.h), the window
// pulled back (src/view/s3.h window3, src/view/s4.h window5), the focus ball (src/render/world.h) and the focus
// centres (src/examples/imm_m5.h focus_centre5, src/examples/cartwright_steger.h focus_centre3), on light cases of
// S^3 (the file of the programs, _t3) and S^4 (IMM's _t1).
//   1. OFF BY DEFAULT: a World and a Renderer start without a window and without a focus ball, and build with them
//      draws what it drew (the same points, polylines and counters as a World that never heard of them). These are the
//      library's defaults: zipper3 and zipper5 turn culling and the focus on for a zoomed still (test_cull_programs).
//   2. THE WINDOW: for points of the sphere (random ones, and points pushed to within 1e-5 of the boundary of the
//      window), window3 and window5 contain exactly the points that the painters show: eye() keeps them and they land
//      in the screen square (written again here from eye() and the half-width clip/zoom), away from a band of 1e-6 at
//      the boundary: the painters take the points in floats (as the renderer stores them), so a point nearer the
//      boundary than the rounding of floats, magnified by the projection, can fall on either side.
//   3. NOTHING LOST IN THE WINDOW: the same picture with and without culling; the segments DRAWN in the window (both
//      ends kept by eye(), and the segment's picture meeting the screen square: decided here by segment
//      intersections, not by the tool's clipping) are the same, exactly (the same two float points): none missing,
//      none added; and every segment of the culled picture, in the window or not, is a segment of the picture without
//      culling (culling only removes: a culled branch ends its polyline). At zooms 1, 10 and 100, about a place of S^3 where Z+ and Z- come close and about the cusp point
//      e_5 of S^4 (where 10 letters of Z+ and 16 of Z- have tips), and with the shortcuts of the renderer: the tips
//      (S^4), the look-ahead through one-letter roofs (S^3), marking (--overlap 1) and forced leaves (a depth limit
//      of 3). Culling must cull something in each zoomed case.
//   4. THE CULLING IS EFFECTIVE AT DEPTH: at zoom 100 the culled picture has under 40% of the leaves at ACC 0.04 (with
//      K = 16 the leaves, 0.04 across, have margins of 0.64 against a window about 0.06 wide: 26% and 35% are kept in
//      S^4, 12% and 9% in S^3), and under 15% at ACC 0.004 (S^4 Z- at the cusp: 7%; the others under 2%), so a window
//      that ignores the zoom, or a margin that does not shrink with the branches, is seen (they keep nearly everything).
//   5. THE POINTS OF THE TEST: cull_points gives, for a letter whose roof is one letter, the vertices of the first
//      branch of its chain whose roof is longer (worked out again here), and for a letter with tips its tip points;
//      and the size counts the tips.
//   6. THE FOCUS CENTRE: in S^4 an element of Gamma (mu = 0, and the product of its word), whose orbit point is within
//      1.5 of o_F (the tile it is walked to contains o_F), nearer than o for L >= 2; at the cusp e_5 the identity (the
//      geodesic towards e_5 never leaves P5). In S^3 an element of Pi (the product of its word), within 2 of o_F, and
//      no generator step brings it nearer.
//   7. THE FOCUS BALL: the roots are B(R), unchanged and first, then the elements h g of h B(R) not already roots, in
//      their order; no two roots have the same key, and every key of B(R) and of h B(R) is a root's; and the picture
//      of the roots of B(R) is drawn first, bit for bit as without the focus.
// Further checks (each catches a wrong version of the code that the checks above do not catch):
//   8. A TURNED VIEW: items 2 and 3 again with the view of the programs' options --yaw, --pitch
//      and --pole (S^3: the frame and the poles move) and --yaw, --pitch, --turn and --turn2 (S^4: the hidden-
//      direction rotation U is not the identity, and window5 must use U, not its transpose). The other cases have
//      U = I.
//   9. CULLING THROUGH build ITSELF: item 3's pictures are made by draw_picture here, which gives the window to
//      both renderers by hand. build gives it to Z+ and Z-, so each of them culls something, draws what it drew
//      without the window in the window, and prints its own count of culled branches.
//  10. THE VALUE OF K: CULL_K is 16, and so is the default of window3 and window5 and of a CullWindow; the
//      margin of the cull test (Renderer::cull_test) at its boundary: a branch whose points all lie beyond a face (or
//      inside it) by 0.98 K s is kept, by 1.02 K s it is culled (or found inside), with the window's default K and
//      with a K given explicitly.
//  11. THE FOCUS WALK IN S^3: at zoom 10 the walk reaches the nearest orbit point of B(5.5). That does not see the
//      pair stage of the walk: with the real generators, geogens(4.5), a walk without the products of two ended at the
//      same place in every one of 100 directions tried (zooms 10 to 10^4): a single step of up to 4.5 reaches the
//      nearest orbit point once h o is within about 2.4 of o_F, and at zoom 10 the nearest orbit point was never more
//      than 2.06 from o_F. What sees the pair stage is a smaller generating set, the 18 elements of the first shell,
//      where single steps do get stuck: the walk's end is then not improved by any product of two generators, and a
//      walk without the pairs ends farther in about half of the directions.
//  12. A CHAIN OF ONE-LETTER ROOFS OF LENGTH 3: in a hand-made Side, whose offsets differ from letter to letter
//      (in a Side made by make_side every first offset is t, so no stored file can show a wrong offset in a chain).
// Light: a few seconds. Uses src/ only; ZIPPERS_REPO is the repository.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "geometry/pu21/pu21.h"
#include "render/cull.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s3.h"
#include "view/s4.h"

#include "render_test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace z = zippers;
using rtest::check;
using rtest::note;
using rtest::section;

namespace {

// ---- the two views, written here as the programs set them up -------------------------------------------------------

// the options of a turned view: yaw and pitch (degrees) and two more, as the programs read them: S^3 --pole a b (the
// pole of the projection moves a degrees along E1, then b along E2), S^4 --turn a --turn2 b (the hidden-direction
// rotation U: a in the plane of the third visible axis and the hidden one, b in that of the first and the hidden one).
// No Turns is the programs' defaults: S^3 yaw 0, pitch 20, poles 0; S^4 yaw 0, pitch 15, turns 0.
struct Turns { double yaw, pitch, a, b; };

struct View3 {
    typedef z::PU21 B;
    typedef z::cs::RenderPolicy P;
    z::s3::ViewState v;
    static constexpr int N = 4;
    // as zipper3 sets the view up: init_view with the options, then, with --centre, the default frame of the point
    // and the two moves of the pole again
    void setup(const z::World<B>& W, const std::vector<double>& c, double zoom, const Turns* t = nullptr)
    {
        const Turns d = {0, 20, 0, 0};
        const Turns& u = t ? *t : d;
        z::s3::init_view(W, v, u.yaw, u.pitch, u.a, u.b, zoom, 3);
        if (!c.empty()) {
            double n = 0;
            for (double x : c) n += x*x;
            z::SpherePoint<4> p;
            for (int k = 0; k < 4; k++) p.x[k] = c[k]/std::sqrt(n);
            z::s3::default_frame(p, v.Q);
            double e1[3] = {1,0,0}, e2[3] = {0,1,0};
            z::s3::move_pole(v.Q, e1, u.a*z::s3::PI/180);
            z::s3::move_pole(v.Q, e2, u.b*z::s3::PI/180);
        }
    }
    z::CullWindow<4> window(double K = z::CULL_K) const { return z::s3::window3(v, K); }
    double half() const { return v.clip/v.zoom; }
    bool eye(const z::SpherePoint<4>& p, double& x, double& y) const
    {
        float f[4] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3]};
        double e[3];
        if (!z::s3::eye(v, f, e)) return false;
        x = e[0]; y = e[1];
        return true;
    }
    std::vector<double> centre() const { return {-v.Q[3][0], -v.Q[3][1], -v.Q[3][2], -v.Q[3][3]}; }
};

struct View4 {
    typedef z::imm::O51 B;
    typedef z::imm::RenderPolicy P;
    z::s4::View5 v;
    static constexpr int N = 5;
    // as zipper5 sets the view up (the turns last, after the zoom and the clip)
    void setup(const z::World<B>& W, const std::vector<double>& c, double zoom, const Turns* t = nullptr)
    {
        const Turns d = {0, 15, 0, 0};
        const Turns& u = t ? *t : d;
        z::s4::init_view5(W, v, u.yaw, u.pitch);
        if (!c.empty()) {
            double n = 0, p[5];
            for (int k = 0; k < 5; k++) n += c[k]*c[k];
            for (int k = 0; k < 5; k++) p[k] = -c[k]/std::sqrt(n);
            z::s4::orthonormal_frame(p, v.Q);
        }
        v.clip = 3; v.zoom = zoom; v.hidfog = 1.5;
        z::s4::turn4(v.U, 2, 3, u.a*z::s4::PI/180);
        z::s4::turn4(v.U, 0, 3, u.b*z::s4::PI/180);
    }
    // how far U is from the identity (the largest entry of U - I)
    double u_from_identity() const
    {
        double m = 0;
        for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) m = std::max(m, std::fabs(v.U[i][k] - (i == k)));
        return m;
    }
    // how far U is from its transpose (what window5 would get wrong with the transpose)
    double u_asymmetry() const
    {
        double m = 0;
        for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) m = std::max(m, std::fabs(v.U[i][k] - v.U[k][i]));
        return m;
    }
    z::CullWindow<5> window(double K = z::CULL_K) const { return z::s4::window5(v, K); }
    double half() const { return v.clip/v.zoom; }
    bool eye(const z::SpherePoint<5>& p, double& x, double& y) const
    {
        float f[5] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3], (float)p.x[4]};
        double e[3], h;
        if (!z::s4::eye5(v, f, e, h)) return false;
        x = e[0]; y = e[1];
        return true;
    }
    std::vector<double> centre() const { return {-v.Q[4][0], -v.Q[4][1], -v.Q[4][2], -v.Q[4][3], -v.Q[4][4]}; }
};

// ---- drawn in the window, written again: both ends kept, and the picture of the segment meets the square ------------

double cross2(double ax, double ay, double bx, double by) { return ax*by - ay*bx; }

// do the segments [p, q] and [r, s] of the plane meet (touching counts)?
bool meet(double px, double py, double qx, double qy, double rx, double ry, double sx, double sy)
{
    double d1 = cross2(sx - rx, sy - ry, px - rx, py - ry), d2 = cross2(sx - rx, sy - ry, qx - rx, qy - ry);
    double d3 = cross2(qx - px, qy - py, rx - px, ry - py), d4 = cross2(qx - px, qy - py, sx - px, sy - py);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) return true;
    auto on = [](double ax, double ay, double bx, double by, double cx, double cy) {
        return std::min(ax, bx) <= cx && cx <= std::max(ax, bx) && std::min(ay, by) <= cy && cy <= std::max(ay, by);
    };
    return (d1 == 0 && on(rx, ry, sx, sy, px, py)) || (d2 == 0 && on(rx, ry, sx, sy, qx, qy)) ||
           (d3 == 0 && on(px, py, qx, qy, rx, ry)) || (d4 == 0 && on(px, py, qx, qy, sx, sy));
}

template <class V>
bool drawn(const V& view, const z::SpherePoint<V::N>& p, const z::SpherePoint<V::N>& q)
{
    double x0, y0, x1, y1;
    if (!view.eye(p, x0, y0) || !view.eye(q, x1, y1)) return false;
    const double H = view.half();
    auto in = [&](double x, double y) { return std::fabs(x) <= H && std::fabs(y) <= H; };
    if (in(x0, y0) || in(x1, y1)) return true;
    const double c[4][2] = {{-H, -H}, {H, -H}, {H, H}, {-H, H}};
    for (int k = 0; k < 4; k++)
        if (meet(x0, y0, x1, y1, c[k][0], c[k][1], c[(k + 1)%4][0], c[(k + 1)%4][1])) return true;
    return false;
}

template <int N>
using Seg = std::array<float, 2*N>;

// a 64-bit hash of the exact bits of every segment of a renderer (its two points in a fixed order), sorted: for the
// comparison of whole pictures, at an eighth of the memory of the segments themselves
template <int N>
std::vector<uint64_t> segment_hashes(const std::vector<float>& pts, const std::vector<uint32_t>& starts)
{
    std::vector<uint64_t> out;
    const size_t nv = pts.size()/N;
    for (size_t k = 0; k < starts.size(); k++) {
        size_t e = k + 1 < starts.size() ? starts[k + 1] : nv;
        for (size_t g = starts[k]; g + 1 < e; g++) {
            const float* a = &pts[(size_t)N*g];
            const float* b = &pts[(size_t)N*(g + 1)];
            bool sw = std::lexicographical_compare(b, b + N, a, a + N);
            uint64_t h = 0x51ed2701f3a4c9b1ULL;
            for (int i = 0; i < 2*N; i++) {
                float f = i < N ? (sw ? b[i] : a[i]) : (sw ? a[i - N] : b[i - N]);
                uint32_t u;
                std::memcpy(&u, &f, 4);
                h = z::splitmix(h ^ u);
            }
            out.push_back(h);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// the segments of a renderer drawn in the window (all of them if all), each as its two float points in a fixed order,
// sorted
template <class V>
std::vector<Seg<V::N>> drawn_segments(const z::Renderer<typename V::B>& r, const V& view, bool all = false)
{
    constexpr int N = V::N;
    std::vector<Seg<N>> out;
    auto pt = [&](size_t g) {
        z::SpherePoint<N> p;
        for (int i = 0; i < N; i++) p.x[i] = r.pts[(size_t)N*g + i];
        return p;
    };
    for (size_t k = 0; k < r.npoly(); k++)
        for (size_t g = r.pbegin(k); g + 1 < r.pend(k); g++) {
            if (!all && !drawn(view, pt(g), pt(g + 1))) continue;
            const float* a = &r.pts[(size_t)N*g];
            const float* b = &r.pts[(size_t)N*(g + 1)];
            bool sw = std::lexicographical_compare(b, b + N, a, a + N);
            Seg<N> s;
            for (int i = 0; i < N; i++) { s[i] = sw ? b[i] : a[i]; s[N + i] = sw ? a[i] : b[i]; }
            out.push_back(s);
        }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// the picture of the roots of radius R at accuracy acc, as build draws it, with the cull window (or none), marking L
// and a depth limit: the two renderers (Z+, Z-), made here so that the depth limit can be set
template <class P>
struct Picture {
    typedef typename P::B B;
    z::Renderer<B>* rp = nullptr;
    z::Renderer<B>* rn = nullptr;
    ~Picture() { delete rp; delete rn; }
};

template <class P>
void draw_picture(const z::World<typename P::B>& W, double R, double acc, const z::CullWindow<P::B::N>* win,
                  int overlap, int maxdepth, Picture<P>& pic)
{
    typedef typename P::B B;
    std::vector<typename B::Elem> roots;
    std::vector<double> rootd;
    P::roots(R, roots, rootd);
    z::default_marking.overlap = overlap;
    pic.rp = new z::Renderer<B>(W.plus, W.L, acc, 20000000);
    pic.rn = new z::Renderer<B>(W.minus, W.L, acc, 20000000);
    z::default_marking = z::Marking();
    for (z::Renderer<B>* r : {pic.rp, pic.rn}) { r->cull = win; if (maxdepth) r->maxdepth = maxdepth; }
    for (size_t i = 0; i < roots.size(); i++)
        for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) {
            pic.rp->draw(roots[i], (int)j, (int)i);
            pic.rn->draw(roots[i], (int)j, (int)i);
        }
}

template <class B>
bool same_drawing(const z::Renderer<B>& a, const z::Renderer<B>& b)
{
    return a.pts == b.pts && a.starts == b.starts && a.polyroot == b.polyroot && a.polyletter == b.polyletter &&
           a.nVisited == b.nVisited && a.nSkipped == b.nSkipped && a.nLeaves == b.nLeaves && a.nSpike == b.nSpike &&
           a.nForced == b.nForced && a.deepest == b.deepest && a.nTipPoints == b.nTipPoints && a.nThrough == b.nThrough;
}

// ---- 1. off by default ------------------------------------------------------------------------------------------

template <class P>
void default_case(const char* file, double R, double acc)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    check(W.cull == nullptr && !W.focus, "%s: a World starts without a window and without a focus ball", file);
    z::Renderer<B> r(W.plus, W.L, acc, 100);
    check(r.cull == nullptr, "%s: a Renderer starts without a window", file);
    // build against a drawing made here without the options
    rtest::capture_stderr([&] { z::build<P>(W, R, acc, 20000000, 3); });
    Picture<P> pic;
    draw_picture<P>(W, R, acc, nullptr, 0, 0, pic);
    check(same_drawing(*W.rp, *pic.rp) && same_drawing(*W.rn, *pic.rn) && W.rp->nCulled == 0 && W.rn->nCulled == 0 &&
          W.nmain == W.roots.size(), "%s R=%g ACC %g: build without the options draws as before", file, R, acc);
}

// ---- 2. the window ------------------------------------------------------------------------------------------------

template <class V>
void window_case(const char* file, const std::vector<double>& c, double zoom, const Turns* turns = nullptr)
{
    typedef typename V::B B;
    typedef typename V::P P;
    constexpr int N = V::N;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    V view;
    view.setup(W, c, zoom, turns);
    z::CullWindow<N> win = view.window();
    long n = 0, bad = 0, in = 0, near = 0;
    auto test = [&](const z::SpherePoint<N>& p) {
        double x, y, dmin = INFINITY;
        for (int i = 0; i < win.n; i++) dmin = std::min(dmin, std::fabs(win.excess(i, p)));
        if (dmin < 1e-6) return;                                  // too near the boundary to decide in floats
        bool shown = view.eye(p, x, y) && std::fabs(x) <= view.half() && std::fabs(y) <= view.half();
        n++;
        in += shown;
        if (shown != win.contains(p)) bad++;
    };
    for (int t = 0; t < 200000; t++) {
        z::SpherePoint<N> p;
        double s = 0;
        for (int i = 0; i < N; i++) { p.x[i] = rtest::gauss(); s += p.x[i]*p.x[i]; }
        for (int i = 0; i < N; i++) p.x[i] /= std::sqrt(s);
        // half of them pulled towards the centre of the view, so that the window (small at a deep zoom) is sampled
        if (t & 1) {
            std::vector<double> ctr = view.centre();
            double w = std::pow(10.0, rtest::uniform(-4, 0)), q = 0;
            for (int i = 0; i < N; i++) { p.x[i] = ctr[i] + w*p.x[i]; q += p.x[i]*p.x[i]; }
            for (int i = 0; i < N; i++) p.x[i] /= std::sqrt(q);
        }
        test(p);
        // and the point pushed to the boundary of one half-space, then 1e-6 to either side
        int i = (int)rtest::rnd(0, win.n - 1);
        double e = win.excess(i, p);
        for (double side : {-1e-5, 1e-5}) {
            z::SpherePoint<N> q = p;
            double r = 0;
            for (int k = 0; k < N; k++) { q.x[k] -= (e - side)*win.u[i][k]; r += q.x[k]*q.x[k]; }
            for (int k = 0; k < N; k++) q.x[k] /= std::sqrt(r);
            near++;
            test(q);
        }
    }
    check(bad == 0 && in > 1000 && in < n, "%s zoom %g%s: the window contains exactly the points shown (%ld points, %ld "
          "shown, %ld near the boundary; %ld disagree)", N == 4 ? "S^3" : "S^4", zoom, turns ? " (turned view)" : "", n, in,
          near, bad);
}

// ---- 3. nothing lost in the window, and 4. effective at depth -------------------------------------------------------

template <class V>
void lost_case(const char* file, const std::vector<double>& c, double zoom, double R, double acc, int overlap = 0,
               int maxdepth = 0, double maxfrac = -1, const char* what = "", const Turns* turns = nullptr)
{
    typedef typename V::B B;
    typedef typename V::P P;
    constexpr int N = V::N;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    V view;
    view.setup(W, c, zoom, turns);
    z::CullWindow<N> win = view.window();
    Picture<P> a, b;
    draw_picture<P>(W, R, acc, nullptr, overlap, maxdepth, a);
    draw_picture<P>(W, R, acc, &win, overlap, maxdepth, b);
    size_t total = 0;
    for (int side = 0; side < 2; side++) {
        const z::Renderer<B>& r0 = side ? *a.rn : *a.rp;
        const z::Renderer<B>& r1 = side ? *b.rn : *b.rp;
        std::vector<Seg<N>> s0 = drawn_segments(r0, view), s1 = drawn_segments(r1, view);
        size_t missing = 0, extra = 0;
        for (const Seg<N>& s : s0) if (!std::binary_search(s1.begin(), s1.end(), s)) missing++;
        for (const Seg<N>& s : s1) if (!std::binary_search(s0.begin(), s0.end(), s)) extra++;
        total += s0.size();
        check(missing == 0 && extra == 0 && (zoom <= 1 || r1.nCulled > 0),
              "%s %s zoom %g R=%g ACC %g%s: the same %zu segments drawn in the window with culling (%zu missing, %zu "
              "added; %zu of %zu leaves kept, %zu culled, %zu forced)", N == 4 ? "S^3" : "S^4", r0.sd.name, zoom, R,
              acc, what, s0.size(), missing, extra, r1.nLeaves, r0.nLeaves, r1.nCulled, r1.nForced);
        // culling only removes: every segment of the culled picture, in the window or not, is one of the picture
        // without culling (a culled branch ends its polyline, so no segment joins across a gap)
        std::vector<uint64_t> a0 = segment_hashes<N>(r0.pts, r0.starts), a1 = segment_hashes<N>(r1.pts, r1.starts);
        size_t foreign = 0;
        for (uint64_t s : a1) if (!std::binary_search(a0.begin(), a0.end(), s)) foreign++;
        check(foreign == 0, "%s %s zoom %g R=%g ACC %g%s: every one of the %zu segments with culling is a segment without "
              "it (%zu are not)", N == 4 ? "S^3" : "S^4", r0.sd.name, zoom, R, acc, what, a1.size(), foreign);
        if (maxdepth) check(r0.nForced > 0, "%s %s: the depth limit %d makes forced leaves (%zu)", N == 4 ? "S^3" : "S^4",
                            r0.sd.name, maxdepth, r0.nForced);
        if (maxfrac > 0)
            check((double)r1.nLeaves < maxfrac*(double)r0.nLeaves, "%s %s zoom %g: culling keeps under %g%% of the leaves "
                  "(%zu of %zu)", N == 4 ? "S^3" : "S^4", r0.sd.name, zoom, 100*maxfrac, r1.nLeaves, r0.nLeaves);
    }
    check(total > 0, "%s zoom %g R=%g ACC %g%s: segments are drawn in the window (%zu), so the comparison means "
          "something", N == 4 ? "S^3" : "S^4", zoom, R, acc, what, total);
}

// ---- 9. culling through build itself (the pictures above are made by draw_picture, which gives the window to both
// renderers by hand) -------------------------------------------------------------------------------------------------

// The number N of ", N culled" in the build's line of the zipper `name` ("Z+" or "Z-"), or -1 if the line has none.
long culled_in_line(const std::string& err, const char* name)
{
    for (const std::string& line : rtest::split_lines(err)) {
        if (line.rfind(std::string(name) + ": R=", 0) != 0) continue;
        size_t p = line.find(" culled");
        if (p == std::string::npos) return -1;
        size_t q = p;
        while (q > 0 && isdigit((unsigned char)line[q - 1])) q--;
        return q == p ? -1 : atol(line.substr(q, p - q).c_str());
    }
    return -1;
}

// A World built twice by build, once with the window (World::cull) and once without. Both zippers must have been given
// the window (each renderer has it and culled something, and prints its own count), and each must draw in the window
// what it draws without it.
template <class V>
void build_case(const char* file, const std::vector<double>& c, double zoom, double R, double acc, const Turns* turns,
                const char* what)
{
    typedef typename V::B B;
    typedef typename V::P P;
    constexpr int N = V::N;
    z::World<B> W0, W1;
    rtest::capture_stderr([&] { z::prepare<P>(file, W0); });
    rtest::capture_stderr([&] { z::prepare<P>(file, W1); });
    V view;
    view.setup(W1, c, zoom, turns);
    z::CullWindow<N> win = view.window();
    W1.cull = &win;
    rtest::capture_stderr([&] { z::build<P>(W0, R, acc, 20000000, 3); });
    std::string err = rtest::capture_stderr([&] { z::build<P>(W1, R, acc, 20000000, 3); });
    for (int side = 0; side < 2; side++) {
        const z::Renderer<B>& r0 = side ? *W0.rn : *W0.rp;
        const z::Renderer<B>& r1 = side ? *W1.rn : *W1.rp;
        std::vector<Seg<N>> s0 = drawn_segments(r0, view), s1 = drawn_segments(r1, view);
        size_t missing = 0, extra = 0;
        for (const Seg<N>& s : s0) if (!std::binary_search(s1.begin(), s1.end(), s)) missing++;
        for (const Seg<N>& s : s1) if (!std::binary_search(s0.begin(), s0.end(), s)) extra++;
        check(r0.cull == nullptr && r1.cull == &win && r1.nCulled > 0 && r0.nCulled == 0,
              "%s %s zoom %g%s: build gave the window to this zipper (it has it: %d, culled %zu branches; without: %zu)",
              N == 4 ? "S^3" : "S^4", r0.sd.name, zoom, what, r1.cull == &win, r1.nCulled, r0.nCulled);
        check(culled_in_line(err, r0.sd.name) == (long)r1.nCulled,
              "%s %s: the build line says the count of culled branches (%ld, the renderer's %zu)", N == 4 ? "S^3" : "S^4",
              r0.sd.name, culled_in_line(err, r0.sd.name), r1.nCulled);
        check(missing == 0 && extra == 0 && s0.size() > 0,
              "%s %s zoom %g%s: the same %zu segments drawn in the window through build (%zu missing, %zu added)",
              N == 4 ? "S^3" : "S^4", r0.sd.name, zoom, what, s0.size(), missing, extra);
    }
    delete W0.rp; delete W0.rn; delete W1.rp; delete W1.rn;
}

// ---- 5. the points of the test ------------------------------------------------------------------------------------

// Is cull_points of the branch (h, j) what the first split and the tips of the letter y give when mapped by e, with the
// size that counts the tips? The caller works out e and y: the product of h and the offsets along the chain of one-letter
// roofs of j, and the first letter y of the chain whose roof is longer.
template <class B>
bool points_agree(const z::Side<B>& sd, int j, const typename B::Elem& h, const typename B::Elem& e, int y)
{
    typedef typename B::Point Point;
    typename B::DMat hd = B::to_double(h), ed = B::to_double(e);
    Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[j]));
    std::vector<Point> verts, tips;
    double size;
    if (!z::cull_points(sd, h, hd, j, a, b, verts, tips, size)) return false;
    std::vector<Point> v2, t2;
    double s2 = z::chord(a, b);
    for (size_t i = 1; i < sd.wv[y].size(); i++) v2.push_back(B::boundary(B::act(ed, sd.wv[y][i])));
    if (!sd.tipline.empty())
        for (const typename B::DVec& t : sd.tipline[y]) t2.push_back(B::boundary(B::act(ed, t)));
    for (const Point& p : v2) s2 = std::max(s2, z::segment_distance(p, a, b));
    for (const Point& p : t2) s2 = std::max(s2, z::segment_distance(p, a, b));
    bool same = verts.size() == v2.size() && tips.size() == t2.size() && size == s2;
    for (size_t i = 0; same && i < v2.size(); i++) same = std::equal(verts[i].x, verts[i].x + B::N, v2[i].x);
    for (size_t i = 0; same && i < t2.size(); i++) same = std::equal(tips[i].x, tips[i].x + B::N, t2[i].x);
    return same;
}

template <class P>
void points_case(const char* file, double acc)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    size_t ones = 0, withtips = 0, bad = 0, n = 0;
    for (const z::Side<B>* sd : {&W.plus, &W.minus})
        for (size_t j = 0; j < sd->roof.size(); j++) {
            // the branch (s_j, j) of the first shell's element s_j, and (1, j)
            for (const typename B::Elem& h : {B::identity(), W.L.s[j]}) {
                // worked out again: the chain of one-letter roofs, then that branch's first split and tips
                typename B::Elem e = h;
                int y = (int)j;
                while (sd->roof[y].size() == 1) { e = B::mul(e, sd->off[y][0]); y = sd->roof[y][0]; }
                n++;
                if (y != (int)j) ones++;
                withtips += !sd->tipline.empty() && !sd->tipline[y].empty();
                if (!points_agree(*sd, (int)j, h, e, y)) bad++;
            }
        }
    (void)acc;
    check(bad == 0 && n > 0, "%s: cull_points = the chain's first split and tips, and the size with the tips, written "
          "again (%zu branches, %zu through a chain of one-letter roofs, %zu with tips; %zu differ)", file, n, ones,
          withtips, bad);
}

// 12. A chain of three one-letter roofs c0 -> c1 -> c2 -> y3 (the roof of y3 is longer), in a hand-made Side: a copy of
// Z+ whose letters c0, c1, c2 have the one-letter roofs {c1}, {c2}, {y3} and the first offsets t, t s_1 and t s_2. (The
// stored files have no chain of more than one step, and a Side made by make_side has the offset t for the first child of
// every letter, so that a chain which took the offset of the first letter at every step would give the same points on
// every real Side: a hand-made one is the only place where the offsets along a chain can be told apart.) The product
// along the chain is taken here from the three offsets in order, not by following the roofs of the Side as cull_points
// does: for c0 the offsets of c0, c1 and c2 in turn, for c1 those of c1 and c2, for c2 that of c2.
template <class P>
void chain_case(const char* file)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    {   // the reason for the hand-made Side: on the Sides make_side makes, the first offset of every letter is t
        size_t letters = 0, other = 0;
        for (const z::Side<B>* s : {&W.plus, &W.minus})
            for (size_t j = 0; j < s->roof.size(); j++) {
                if (s->off[j].empty()) continue;
                letters++;
                other += !B::eq(s->off[j][0], B::normal(s->t));
            }
        check(letters > 0 && other == 0, "%s: in the Sides that make_side makes the first offset of every letter is t (%zu "
              "letters, %zu with another)", file, letters, other);
    }
    z::Side<B> sd = W.plus;
    int y3 = -1, c[3], m = 0;
    for (size_t j = 0; j < sd.roof.size(); j++) if (sd.roof[j].size() >= 2) { y3 = (int)j; break; }
    for (size_t j = 0; j < sd.roof.size() && m < 3; j++) if ((int)j != y3) c[m++] = (int)j;
    if (y3 < 0 || m < 3) { check(false, "%s: a letter with a roof of two letters or more, and three others", file); return; }
    sd.roof[c[0]] = {c[1]}; sd.roof[c[1]] = {c[2]}; sd.roof[c[2]] = {y3};
    const typename B::Elem o[3] = {B::normal(sd.t), B::normal(B::mul(sd.t, W.L.s[1])), B::normal(B::mul(sd.t, W.L.s[2]))};
    for (int i = 0; i < 3; i++) sd.off[c[i]] = {o[i]};
    check(!B::eq(o[0], o[1]) && !B::eq(o[1], o[2]) && !B::eq(o[0], o[2]), "%s: the three offsets of the chain differ",
          file);
    size_t bad = 0, n = 0;
    for (int k = 0; k < 3; k++)                     // the chain from c_k: 3 - k steps
        for (const typename B::Elem& h : {B::identity(), W.L.s[c[k]]}) {
            typename B::Elem e = h;
            for (int i = k; i < 3; i++) e = B::mul(e, o[i]);       // the offsets of c_k, ..., c_2, in this order
            n++;
            if (!points_agree(sd, c[k], h, e, y3)) bad++;
        }
    check(bad == 0 && n == 6, "%s: cull_points follows a chain of three one-letter roofs with the offset of each letter "
          "(%zu branches, %zu differ)", file, n, bad);
}

// ---- 10. the value of K: the margin of the cull test at its boundary -----------------------------------------------

// For each branch of the first shell (and of the roots): its points p (cull_points: ends, vertices, tips) and its size
// s. A window of one half-space u . x <= c whose face lies beyond every point by f K s (c = min u . p - f K s) must
// leave the branch alone for f < 1 (the test says 0: not far enough) and cull it for f > 1 (-1); one whose face lies
// inside every point by f K s (c = max u . p + f K s) must say 0 for f < 1 and "inside" (1) for f > 1. Here K is the
// factor of the window: the default of a CullWindow (dflt, which the test says is KK = 16), or K set explicitly. The
// test goes through Renderer::cull_test, which makes the margin K s.
template <class P>
void margin_case(const char* file, double KK, bool dflt)
{
    typedef typename P::B B;
    typedef typename B::Point Point;
    constexpr int N = B::N;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file, W); });
    size_t n = 0, bad = 0, branches = 0;
    for (const z::Side<B>* sd : {&W.plus, &W.minus}) {
        z::Renderer<B> r(*sd, W.L, 0.04, 100);
        for (size_t j = 0; j < sd->roof.size(); j++)
            for (const typename B::Elem& h : {B::identity(), W.L.s[j]}) {
                typename B::DMat hd = B::to_double(h);
                Point a = B::boundary(B::act(hd, sd->vfix)), b = B::boundary(B::act(hd, sd->ev[j]));
                std::vector<Point> verts, tips;
                double s;
                if (!z::cull_points(*sd, h, hd, (int)j, a, b, verts, tips, s) || s < 1e-6) continue;
                std::vector<Point> pts = {a, b};
                pts.insert(pts.end(), verts.begin(), verts.end());
                pts.insert(pts.end(), tips.begin(), tips.end());
                branches++;
                // three faces: across the middle of the chord, along the chord, and a coordinate axis
                double face[3][N];
                for (int k = 0; k < N; k++) {
                    face[0][k] = a.x[k] + b.x[k];
                    face[1][k] = a.x[k] - b.x[k];
                    face[2][k] = k == (int)(j % N) ? 1 : 0;
                }
                for (int f = 0; f < 3; f++) {
                    double l = 0, u[N], lo = INFINITY, hi = -INFINITY;
                    for (int k = 0; k < N; k++) l += face[f][k]*face[f][k];
                    if (l < 1e-6) continue;
                    for (int k = 0; k < N; k++) u[k] = face[f][k]/std::sqrt(l);
                    for (const Point& p : pts) {
                        double v = 0;
                        for (int k = 0; k < N; k++) v += u[k]*p.x[k];
                        lo = std::min(lo, v);
                        hi = std::max(hi, v);
                    }
                    for (double fac : {0.98, 1.02}) {
                        z::CullWindow<N> beyond, inside;
                        if (!dflt) beyond.K = inside.K = KK;
                        beyond.add(u, lo - fac*KK*s);              // every point beyond the face by fac K s
                        inside.add(u, hi + fac*KK*s);              // every point inside it by fac K s
                        r.cull = &beyond;
                        int g1 = r.cull_test(h, hd, (int)j, a, b);
                        r.cull = &inside;
                        int g2 = r.cull_test(h, hd, (int)j, a, b);
                        n += 2;
                        bad += (g1 != (fac < 1 ? 0 : -1)) + (g2 != (fac < 1 ? 0 : 1));
                    }
                }
            }
        r.cull = nullptr;
    }
    check(bad == 0 && n > 1000, "%s, K = %g (%s): a branch beyond a face (or inside it) by 0.98 K s is kept, by 1.02 K s "
          "it is culled (found inside) (%zu branches, %zu tests; %zu wrong)", file, KK, dflt ? "the window's default" :
          "set explicitly", branches, n, bad);
}

// ---- 6. the focus centre --------------------------------------------------------------------------------------------

void focus_centre5_case(const char* file)
{
    typedef z::imm::O51 B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<z::imm::RenderPolicy>(file, W); });
    const double e5[5] = {0, 0, 0, 0, 1};
    for (int where = 0; where < 2; where++)
        for (double zoom : {10.0, 100.0, 1000.0})
            for (double M : {1.0, 2.0}) {
                double xi[5];
                for (int k = 0; k < 5; k++) xi[k] = where ? e5[k] : W.plus.V.x[k];
                double L = M*std::log(zoom);
                z::imm::FocusCentre fc = z::imm::focus_centre5(xi, L);
                int mu, hh;
                z::imm::word_mu_h(fc.word, mu, hh);
                bool inG = mu == 0 && B::eq(z::imm::word_matrix(fc.word), fc.h);
                // o_F and the distance, again
                double p[6], s = 0;
                for (int k = 0; k < 5; k++) p[k] = std::sinh(L)*xi[k];
                p[5] = std::cosh(L);
                for (int k = 0; k < 5; k++) s -= 0.5*(double)fc.h.e[k][5]*p[k];
                s += 0.5*(double)fc.h.e[5][5]*p[5];
                double d = std::acosh(std::max(1.0, s));
                if (where == 0)
                    // (both distances come from <h o, o_F>, whose terms are about cosh(L) cosh(d(o, h o)): in doubles
                    // they agree to about 1e-16 of that)
                    check(inG && std::fabs(d - fc.dist) < 1e-14*std::cosh(L)*std::cosh(fc.disp) && d < 1.5 && (L < 2 || d < L),
                          "S^4 focus at t^+, zoom %g, %g log zoom: h in Gamma (word of %zu facets), d(h o, o_F) = %.4f, "
                          "d(o, h o) = %.4f, L = %.4f", zoom, M, fc.word.size(), d, fc.disp, L);
                else
                    check(inG && B::eq(fc.h, B::identity()) && std::fabs(d - L) < 1e-6,
                          "S^4 focus at the cusp e_5, zoom %g, %g log zoom: h = 1 (d(h o, o_F) = L = %.4f: the geodesic "
                          "never leaves P5)", zoom, M, L);
            }
}

void focus_centre3_case(const char* file)
{
    typedef z::PU21 B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(file, W); });
    std::vector<z::cs::Gen> G = z::cs::geogens(4.5);
    const double place[4] = {0.1152, -0.1133, 0.8356, 0.5250};
    for (int where = 0; where < 2; where++)
        for (double zoom : {10.0, 100.0}) {
            double xi[4];
            for (int k = 0; k < 4; k++) xi[k] = where ? place[k] : W.plus.V.x[k];
            double L = std::log(zoom);
            z::cs::FocusCentre3 fc = z::cs::focus_centre3(xi, L, G);
            bool inPi = B::same(fc.h, z::cs::parse_word(fc.word));
            double nx = std::sqrt(xi[0]*xi[0] + xi[1]*xi[1] + xi[2]*xi[2] + xi[3]*xi[3]), p[4];
            for (int k = 0; k < 4; k++) p[k] = std::tanh(L/2)*xi[k]/nx;
            double d = z::cs::orbit_distance3(fc.h, p);
            bool local = true;                        // no generator step brings h o nearer
            for (const z::cs::Gen& g : G) if (z::cs::orbit_distance3(z::mmul(fc.h, g.m), p) < d - 1e-9) local = false;
            check(inPi && local && d < 2 && d < L, "S^3 focus at %s, zoom %g: h in Pi (the product of its word), "
                  "d(h o, o_F) = %.4f < L = %.4f, no generator step nearer", where ? "the place" : "t^+", zoom, d, L);
        }
}

// ---- 11. the focus walk in S^3, against the orbit -------------------------------------------------------------------

// the k-th test direction in R^4: four numbers of [-1, 1) from the SplitMix64 stream (the same on every platform)
void test_direction(uint64_t k, double xi[4])
{
    for (int i = 0; i < 4; i++)
        xi[i] = 2*(double)(z::splitmix(1000003*k + (uint64_t)i) >> 11)/9007199254740992.0 - 1;
}

// the point of the unit ball at distance L from o towards xi (o_F)
void focus_point3(const double xi[4], double L, double p[4])
{
    double nx = std::sqrt(xi[0]*xi[0] + xi[1]*xi[1] + xi[2]*xi[2] + xi[3]*xi[3]);
    for (int k = 0; k < 4; k++) p[k] = std::tanh(L/2)*xi[k]/nx;
}

// the walk of focus_centre3 without the products of two generators (a copy of its single steps): the distance at which it
// stops
double walk_without_pairs(const double xi[4], double L, const std::vector<z::cs::Gen>& G)
{
    double p[4];
    focus_point3(xi, L, p);
    z::M3 h = z::PU21::normal(z::PU21::identity());
    double dh = z::cs::orbit_distance3(h, p);
    for (int step = 0; step < 10000; step++) {
        int b = -1;
        double bd = dh;
        for (size_t i = 0; i < G.size(); i++) {
            double d = z::cs::orbit_distance3(z::mmul(h, G[i].m), p);
            if (d < bd - 1e-12) { bd = d; b = (int)i; }
        }
        if (b < 0) break;
        h = z::mnormal(z::mmul(h, G[b].m));
        dh = bd;
    }
    return dh;
}

void focus_walk3_case()
{
    // (a) the real generators, at zoom 10: the walk ends at the orbit point of roots_info(5.5) nearest to o_F (o_F is
    // at distance log 10 = 2.30 from o, with an orbit point within about 2 of it).
    {
        std::vector<z::cs::Gen> G = z::cs::geogens(4.5);
        std::vector<z::cs::RootInfo> ball = z::cs::roots_info(5.5);
        const double L = std::log(10.0);
        size_t miss = 0, nd = 12;
        double worst = 0;
        for (uint64_t t = 0; t < nd; t++) {
            double xi[4], p[4];
            test_direction(t, xi);
            focus_point3(xi, L, p);
            z::cs::FocusCentre3 fc = z::cs::focus_centre3(xi, L, G);
            double d = z::cs::orbit_distance3(fc.h, p), best = z::cs::orbit_distance3(z::PU21::normal(z::PU21::identity()), p);
            for (const z::cs::RootInfo& r : ball) best = std::min(best, z::cs::orbit_distance3(r.g, p));
            worst = std::max(worst, best);
            if (std::fabs(d - best) > 1e-9) miss++;
        }
        check(miss == 0 && ball.size() > 3000 && worst > 1, "S^3 focus walk at zoom 10, %zu directions: it ends at the "
              "nearest of the %zu orbit points of B(5.5) to o_F (%zu do not; the farthest nearest one is %.3f away)", nd,
              ball.size(), miss, worst);
    }
    // (b) the 18 elements of the first shell as generators: a single step often cannot bring h o nearer to o_F, and the
    // walk then tries the products of two of them. Its end is then not improved by any generator or product of two (a
    // walk without the pairs, copied above, ends farther in some of these directions: the products are used).
    {
        std::vector<z::cs::Gen> Gs = z::cs::geogens(2.6);
        size_t n = 0, notlocal = 0, usedpairs = 0;
        for (double zoom : {100.0, 1000.0})
            for (uint64_t t = 0; t < 24; t++) {
                double xi[4], p[4];
                test_direction(100 + t, xi);
                focus_point3(xi, std::log(zoom), p);
                z::cs::FocusCentre3 fc = z::cs::focus_centre3(xi, std::log(zoom), Gs);
                double d = z::cs::orbit_distance3(fc.h, p);
                bool local = true;
                for (const z::cs::Gen& g1 : Gs) {
                    z::M3 x = z::mmul(fc.h, g1.m);
                    if (z::cs::orbit_distance3(x, p) < d - 1e-9) local = false;
                    for (const z::cs::Gen& g2 : Gs) if (z::cs::orbit_distance3(z::mmul(x, g2.m), p) < d - 1e-9) local = false;
                }
                n++;
                notlocal += !local;
                usedpairs += walk_without_pairs(xi, std::log(zoom), Gs) > d + 1e-9;
            }
        check(Gs.size() == 18 && notlocal == 0 && usedpairs >= 8, "S^3 focus walk with the %zu generators of the first "
              "shell, zoom 100 and 1000, %zu directions: the end is not improved by any product of two generators (%zu "
              "are); the pairs are used (a walk without them ends farther in %zu directions)", Gs.size(), n, notlocal,
              usedpairs);
    }
}

// ---- 7. the focus ball --------------------------------------------------------------------------------------------

template <class P>
void focus_ball_case(const char* file, double R, double acc, const typename P::B::Elem& h, const char* what)
{
    typedef typename P::B B;
    z::World<B> W0, W1;
    rtest::capture_stderr([&] { z::prepare<P>(file, W0); });
    rtest::capture_stderr([&] { z::prepare<P>(file, W1); });
    W1.focus = true;
    W1.fh = h;
    rtest::capture_stderr([&] { z::build<P>(W0, R, acc, 20000000, 3); });
    rtest::capture_stderr([&] { z::build<P>(W1, R, acc, 20000000, 3); });
    // B(R) unchanged and first
    bool first = W1.nmain == W0.roots.size();
    for (size_t i = 0; first && i < W0.roots.size(); i++) first = B::eq(W0.roots[i], W1.roots[i]);
    // no two roots with one key; the keys are those of B(R) and h B(R)
    std::set<typename B::Key> keys, want;
    for (const typename B::Elem& g : W1.roots) keys.insert(B::key(g));
    for (const typename B::Elem& g : W0.roots) { want.insert(B::key(g)); want.insert(B::key(B::normal(B::mul(h, g)))); }
    size_t inboth = 0;
    for (const typename B::Elem& g : W0.roots) {
        typename B::Key k = B::key(B::normal(B::mul(h, g)));
        bool in = false;
        for (const typename B::Elem& x : W0.roots) if (B::key(x) == k) in = true;
        inboth += in;
    }
    // the added roots are h g in the order of B(R)
    bool order = true;
    size_t k = W1.nmain;
    for (const typename B::Elem& g : W0.roots) {
        typename B::Elem x = B::normal(B::mul(h, g));
        bool in = false;
        for (const typename B::Elem& y : W0.roots) if (B::key(y) == B::key(x)) in = true;
        if (in) continue;
        if (k >= W1.roots.size() || !B::eq(W1.roots[k], x)) { order = false; break; }
        k++;
    }
    check(first && keys.size() == W1.roots.size() && keys == want && order && k == W1.roots.size(),
          "%s R=%g: roots B(R) (%zu) then h B(R) without the %zu it shares with B(R): %zu roots, no key twice, the "
          "union of the keys, in order", what, R, W0.roots.size(), inboth, W1.roots.size());
    check(inboth > 0, "%s R=%g: the two balls overlap (%zu shared), so that the de-duplication is tested", what, R, inboth);
    // the picture of B(R) first, bit for bit
    for (int side = 0; side < 2; side++) {
        const z::Renderer<B>& r0 = side ? *W0.rn : *W0.rp;
        const z::Renderer<B>& r1 = side ? *W1.rn : *W1.rp;
        size_t np = 0;
        while (np < r1.npoly() && r1.polyroot[np] < (int)W1.nmain) np++;
        bool same = np == r0.npoly() && r1.pts.size() >= r0.pts.size() &&
                    std::equal(r0.pts.begin(), r0.pts.end(), r1.pts.begin()) &&
                    std::equal(r0.starts.begin(), r0.starts.end(), r1.starts.begin());
        check(same && r1.nLeaves > r0.nLeaves, "%s %s: the picture of B(R) is drawn first, as without the focus (%zu "
              "polylines), and the focus ball adds %zu leaves", what, r0.sd.name, r0.npoly(), r1.nLeaves - r0.nLeaves);
    }
}

}  // namespace

int main()
{
    double t_start = rtest::clock_seconds();
    z::cs::setup();
    z::imm::setup();
    printf("test_cull: culling, the window and the focus ball of the deep zoom\n");
    const std::string f3s = std::string(ZIPPERS_REPO) + "/legacy/s3/subst_4.00_9.50_t3.txt";
    const std::string f4s = std::string(ZIPPERS_REPO) + "/legacy/s4/subst5_2.30_4.50_t1.txt";
    const char* f3 = f3s.c_str();
    const char* f4 = f4s.c_str();
    const std::vector<double> place = {0.1152, -0.1133, 0.8356, 0.5250};   // S^3: Z+ and Z- 0.006 apart (R = 2.554)
    const std::vector<double> e5 = {0, 0, 0, 0, 1};                          // S^4: the cusp point e_5

    section("off by default");
    default_case<z::cs::RenderPolicy>(f3, 0, 0.04);
    default_case<z::imm::RenderPolicy>(f4, 0, 0.04);

    section("the window pulled back: exactly the points the painters show");
    for (double zoom : {1.0, 10.0, 1000.0}) {
        window_case<View3>(f3, place, zoom);
        window_case<View4>(f4, e5, zoom);
    }
    window_case<View3>(f3, {}, 10);
    window_case<View4>(f4, {}, 10);

    section("nothing lost in the window (the tips in S^4, the look-ahead through one-letter roofs in S^3)");
    for (double zoom : {1.0, 10.0, 100.0}) {
        lost_case<View3>(f3, place, zoom, 0, 0.04);
        lost_case<View4>(f4, e5, zoom, 0, 0.04);
    }
    lost_case<View3>(f3, place, 10, 2.554, 0.04);
    lost_case<View4>(f4, e5, 10, 1.317, 0.04);
    lost_case<View3>(f3, place, 100, 0, 0.004, 0, 0, 0.15, " (ACC/zoom at zoom 10)");
    lost_case<View4>(f4, e5, 100, 0, 0.004, 0, 0, 0.15, " (ACC/zoom at zoom 10)");

    section("nothing lost with marking (--overlap 1) and with forced leaves (depth limit 3)");
    lost_case<View3>(f3, place, 10, 0, 0.04, 1, 0, -1, ", marking 1");
    lost_case<View4>(f4, e5, 10, 0, 0.04, 1, 0, -1, ", marking 1");
    lost_case<View3>(f3, place, 10, 0, 0.01, 0, 3, -1, ", depth limit 3");
    lost_case<View4>(f4, e5, 10, 0, 0.01, 0, 3, -1, ", depth limit 3");

    section("culling is effective at zoom 100");
    lost_case<View3>(f3, place, 100, 0, 0.04, 0, 0, 0.40);
    lost_case<View4>(f4, e5, 100, 0, 0.04, 0, 0, 0.40);

    // A turned view: yaw, pitch, --pole (S^3) and --turn, --turn2 (S^4) all away from 0, so that the frame, the rotation
    // of R^3 and (S^4) the hidden-direction rotation U are not the defaults. S^3: yaw 40, pitch -10, --pole 15 -20; S^4:
    // --yaw 15, --turn 35, --turn2 -20 (and the default pitch 15).
    const Turns turned3 = {40, -10, 15, -20}, turned4 = {15, 15, 35, -20};
    section("a turned view: the window, and nothing lost in it (S^4: U is not the identity)");
    {
        z::World<z::imm::O51> W4;
        rtest::capture_stderr([&] { z::prepare<z::imm::RenderPolicy>(f4, W4); });
        View4 tv, dv;
        tv.setup(W4, e5, 10, &turned4);
        dv.setup(W4, e5, 10);
        check(tv.u_from_identity() > 0.3 && tv.u_asymmetry() > 0.3 && dv.u_from_identity() == 0,
              "S^4 turned view: U is far from the identity (%.3f) and from its transpose (%.3f), so a window5 that used "
              "the transpose would be seen; the default view has U = I", tv.u_from_identity(), tv.u_asymmetry());
        z::World<z::PU21> W3;
        rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(f3, W3); });
        View3 t3, d3;
        t3.setup(W3, place, 10, &turned3);
        d3.setup(W3, place, 10);
        double dq = 0, dr = 0;
        for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) dq = std::max(dq, std::fabs(t3.v.Q[i][k] - d3.v.Q[i][k]));
        for (int i = 0; i < 9; i++) dr = std::max(dr, std::fabs(t3.v.rot[i] - d3.v.rot[i]));
        check(dq > 0.1 && dr > 0.1, "S^3 turned view: the frame (%.3f) and the rotation of R^3 (%.3f) differ from the "
              "default view's", dq, dr);
    }
    for (double zoom : {10.0, 100.0}) {
        window_case<View3>(f3, place, zoom, &turned3);
        window_case<View4>(f4, e5, zoom, &turned4);
    }
    window_case<View3>(f3, {}, 10, &turned3);
    window_case<View4>(f4, {}, 10, &turned4);
    for (double zoom : {10.0, 100.0}) {
        lost_case<View3>(f3, place, zoom, 0, 0.04, 0, 0, -1, ", turned view", &turned3);
        lost_case<View4>(f4, e5, zoom, 0, 0.04, 0, 0, -1, ", turned view", &turned4);
    }
    lost_case<View3>(f3, place, 10, 2.554, 0.04, 0, 0, -1, ", turned view", &turned3);
    lost_case<View4>(f4, e5, 10, 1.317, 0.04, 0, 0, -1, ", turned view", &turned4);
    lost_case<View3>(f3, place, 10, 0, 0.04, 1, 0, -1, ", marking 1, turned view", &turned3);
    lost_case<View4>(f4, e5, 10, 0, 0.04, 1, 0, -1, ", marking 1, turned view", &turned4);

    section("culling through build itself: both zippers get the window (and a turned view)");
    build_case<View3>(f3, place, 10, 0, 0.04, nullptr, "");
    build_case<View4>(f4, e5, 10, 0, 0.04, nullptr, "");
    build_case<View3>(f3, place, 10, 0, 0.04, &turned3, ", turned view");
    build_case<View4>(f4, e5, 10, 0, 0.04, &turned4, ", turned view");

    section("the value of K: 16 by default, and the margin K s at its boundary");
    {
        z::World<z::PU21> W3;
        rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(f3, W3); });
        z::World<z::imm::O51> W4;
        rtest::capture_stderr([&] { z::prepare<z::imm::RenderPolicy>(f4, W4); });
        View3 v3;
        v3.setup(W3, place, 10);
        View4 v4;
        v4.setup(W4, e5, 10);
        check(z::CULL_K == 16 && z::CullWindow<4>().K == 16 && z::CullWindow<5>().K == 16 && z::s3::window3(v3.v).K == 16 &&
              z::s4::window5(v4.v).K == 16, "the default margin factor K is 16 (CULL_K %g; the K of a CullWindow, of window3 "
              "and of window5 without an argument)", z::CULL_K);
        check(z::s3::window3(v3.v, 3).K == 3 && z::s4::window5(v4.v, 3).K == 3 && z::s3::window3(v3.v, 0).K == 0,
              "window3 and window5 take the K they are given");
    }
    margin_case<z::cs::RenderPolicy>(f3, 16, true);
    margin_case<z::imm::RenderPolicy>(f4, 16, true);
    margin_case<z::cs::RenderPolicy>(f3, 3, false);
    margin_case<z::imm::RenderPolicy>(f4, 3, false);

    section("the points of the cull test (and a chain of one-letter roofs of length 3)");
    points_case<z::cs::RenderPolicy>(f3, 0.04);
    points_case<z::imm::RenderPolicy>(f4, 0.04);
    chain_case<z::cs::RenderPolicy>(f3);
    chain_case<z::imm::RenderPolicy>(f4);

    section("the focus centre");
    focus_centre5_case(f4);
    focus_centre3_case(f3);
    focus_walk3_case();

    section("the focus ball: B(R) first, then h B(R), de-duplicated by key");
    {
        z::World<z::imm::O51> W;
        rtest::capture_stderr([&] { z::prepare<z::imm::RenderPolicy>(f4, W); });
        double xi[5];
        for (int k = 0; k < 5; k++) xi[k] = W.plus.V.x[k];
        z::imm::FocusCentre fc = z::imm::focus_centre5(xi, std::log(10.0));
        focus_ball_case<z::imm::RenderPolicy>(f4, 1.8, 0.04, fc.h, "S^4 (zoom 10 at t^+)");
    }
    {
        z::World<z::PU21> W;
        rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(f3, W); });
        double xi[4];
        for (int k = 0; k < 4; k++) xi[k] = W.plus.V.x[k];
        z::cs::FocusCentre3 fc = z::cs::focus_centre3(xi, std::log(10.0), z::cs::geogens(4.5));
        focus_ball_case<z::cs::RenderPolicy>(f3, 2.554, 0.04, fc.h, "S^3 (zoom 10 at t^+)");
    }

    rtest::end_section();
    printf("test_cull: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
