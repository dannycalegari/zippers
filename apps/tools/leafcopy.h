// SPDX-License-Identifier: GPL-3.0-or-later
//
// leafcopy.h: what the measuring tools of apps/tools/ share (strays and overdraw). Moved unchanged from strays.cc,
// with the renderer's marking added to the copy of the renderer:
//   StampSet          a set of 64-bit keys that is emptied at no cost
//   branch_key        the renderer's key of a branch
//   tip_points, leaf_line   the points a leaf is drawn through between its ends (its tips), and the leaf as drawn
//   walk              the leaves of a branch, as arc_points finds them (render/arcs.h)
//   LeafRenderer      a copy of the renderer (render/renderer.h) that reports its leaves (with the renderer's
//                     culling)
//   View<4>, View<5>  the two painters (view/s3.h, view/s4.h) behind one interface (with the optional line
//                     thickness of the painters, 0 by default, and the view of the programs' zoom options, program,
//                     and its window)
//   ViewOptions       the view options of zipper3 and zipper5, for View<N>::program

#pragma once

#include "core/hash.h"
#include "core/keyset.h"
#include "examples/cartwright_steger.h"
#include "examples/imm_m5.h"
#include "geometry/backend.h"
#include "geometry/pu21/pu21.h"
#include "geometry/sphere.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s3.h"
#include "view/s4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace zippers {
namespace tools {

// a set of 64-bit keys that is emptied at no cost: each entry carries the number of the round it was added in
struct StampSet {
    std::vector<uint64_t> key;
    std::vector<uint32_t> stamp;
    uint32_t cur = 1;
    size_t n = 0;
    int shift = 54;
    StampSet() : key(1024, 0), stamp(1024, 0) {}
    void clear()
    {
        n = 0;
        if (++cur == 0) { std::fill(stamp.begin(), stamp.end(), 0); cur = 1; }
    }
    size_t slot(uint64_t k) const
    {
        size_t m = key.size() - 1, i = (size_t)((k*0x9E3779B97F4A7C15ULL) >> shift);
        while (stamp[i] == cur && key[i] != k) i = (i + 1) & m;
        return i;
    }
    bool insert(uint64_t k)                                      // true if k is new in this round
    {
        size_t i = slot(k);
        if (stamp[i] == cur) return false;
        if (2*(n + 1) > key.size()) {
            std::vector<uint64_t> ok;
            std::vector<uint32_t> os;
            ok.swap(key); os.swap(stamp);
            key.assign(2*ok.size(), 0); stamp.assign(2*ok.size(), 0);
            shift--;
            for (size_t s = 0; s < ok.size(); s++)
                if (os[s] == cur) { size_t t = slot(ok[s]); key[t] = ok[s]; stamp[t] = cur; }
            i = slot(k);
        }
        key[i] = k; stamp[i] = cur; n++;
        return true;
    }
};

// the renderer's key of the branch (g, y): (g, y) for y in S+, its reversal (g s_y, y^-1) otherwise
template <class B>
uint64_t branch_key(const Letters<B>& L, const typename B::Elem& g, int y)
{
    return L.plus[y] ? fingerprint(B::key(g), L.canon[y]) : fingerprint(B::product_key(g, L.s[y]), L.canon[y]);
}

// the points the leaf (h, j) is drawn through between its ends: h q for the points q of its tip line (Side::tipline,
// render/tips.h), with the expressions of the renderer and of arc_points; none without tips
template <class B, class F>
void tip_points(const Side<B>& sd, const typename B::Elem& h, int j, F&& out)
{
    if (sd.tipline.empty() || sd.tipline[j].empty()) return;
    typename B::DMat hd = B::to_double(h);
    for (const typename B::DVec& v : sd.tipline[j]) out(B::boundary(B::act(hd, v)));
}

// the leaf (h, j) as it is drawn: a, its tip points, b
template <class B>
std::vector<SpherePoint<B::N>> leaf_line(const Side<B>& sd, const typename B::Elem& h, int j, const SpherePoint<B::N>& a,
                                         const SpherePoint<B::N>& b)
{
    std::vector<SpherePoint<B::N>> l = {a};
    tip_points<B>(sd, h, j, [&](const SpherePoint<B::N>& q) { l.push_back(q); });
    l.push_back(b);
    return l;
}

// ---------------------------------------------------------------------------------------------------------------------
// The leaves of a branch, as arc_points finds them

struct NoSkip { void operator()() const {} };

// the leaves of the branch (h0, j0) at depth d0, in order, by the splitting rule of arc_points (render/arcs.h) at
// accuracy acc: with done == nullptr, the same leaves in the same order, with the same doubles (each expression is
// arc_points's). For each leaf, leaf(h, j, depth, child, parent, a, b, forced): child is its 1-based place in the roof
// of its parent, whose letter is parent (0 and -1 for the branch (h0, j0) itself); forced as arc_points counts it.
// With a set done, a branch whose key (branch_key) is in it already is skipped, and skip() is called.
template <class B, class F, class S>
void walk(const Side<B>& sd, const Letters<B>& L, const typename B::Elem& h0, int j0, int d0, double acc,
          StampSet* done, F&& leaf, S&& skip)
{
    struct Fr { typename B::Elem h; int j, depth, child, parent; };
    std::vector<Fr> st;
    st.push_back({h0, j0, d0, 0, -1});
    while (!st.empty()) {
        Fr f = st.back();
        st.pop_back();
        if (done && !done->insert(branch_key<B>(L, f.h, f.j))) { skip(); continue; }
        typename B::DMat hd = B::to_double(f.h);
        typename B::Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, a, b, acc)) split = true;          // (b), as arc_points
        if (split && f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40)) {
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1, i + 1, f.j});
        } else leaf(f.h, f.j, f.depth, f.child, f.parent, a, b, split);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// A copy of the renderer that reports its leaves

// Renderer<B> (render/renderer.h), copied statement for statement (draw, visit, emit, with the tips and the
// marking), with two additions that change nothing it computes: each frame carries its place in its parent's
// roof (child, parent), and emit calls hook(frame, a, b, forced, root) before it stores the leaf. check() compares the
// points, polylines and counters with the renderer's. (A third, onvisit, is empty unless a tool sets it, and then the
// copy is no longer the renderer: overdraw's containment experiment.)
template <class B>
struct LeafRenderer {
    typedef typename B::Elem Elem;
    typedef typename B::Point Point;
    static constexpr int N = B::N;
    struct Frame { Elem h; int j, depth, kind; uint64_t key; int child, parent; };

    const Side<B>& sd;
    const Letters<B>& L;
    double acc;
    size_t leafcap;
    int maxdepth = 400;
    KeySet done;
    std::vector<float> pts;
    std::vector<uint32_t> starts;
    std::vector<int> polyroot, polyletter;
    int curroot = -1, curletter = -1;
    bool open = false, capped = false;
    size_t nVisited = 0, nSkipped = 0, nLeaves = 0, nSpike = 0, nForced = 0;
    int deepest = 0;
    size_t nTipPoints = 0, nThrough = 0;
    std::vector<Frame> stk;
    std::function<void(const Frame&, const Point&, const Point&, bool, int)> hook;
    // the renderer's marking, copied: off unless set (as the renderer, from default_marking)
    int overlap = default_marking.overlap;
    bool reverse = default_marking.reverse;
    size_t nMarked = 0, nReverse = 0;
    struct MarkFrame { Elem h; int j, depth; Point a, b; };
    std::vector<MarkFrame> mstk;
    // not in the renderer (empty unless a tool sets it; overdraw's containment experiment): called after a branch
    // (h, j) is visited, before it is split or drawn, to put further keys into done
    std::function<void(LeafRenderer&, const Elem&, int)> onvisit;
    // the renderer's culling, copied: off unless set
    const CullWindow<N>* cull = nullptr;
    size_t nCulled = 0, nInside = 0, nCullOpen = 0;
    std::vector<Point> cpts, cverts, ctips;
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

    LeafRenderer(const Side<B>& s, const Letters<B>& l, double a, size_t cap) : sd(s), L(l), acc(a), leafcap(cap) {}

    // Renderer<B>::mark_reverse and Renderer<B>::mark, copied
    void mark_reverse(const Elem& h, int j)
    {
        if (L.inv[j] != j || B::max_coefficient(h) >= (1LL << 40)) return;
        if (done.insert(fingerprint(B::product_key(h, L.s[j]), L.canon[j]))) { ++nMarked; ++nReverse; }
    }
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
                bool down = false;
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

    void push(const Point& p) { for (int i = 0; i < N; i++) pts.push_back((float)p.x[i]); }
    void emit(const Frame& f, const Point& a, const Point& b, bool forced, const typename B::DMat& hd)
    {
        if (hook) hook(f, a, b, forced, curroot);
        ++nLeaves;
        if (!open) {
            starts.push_back((uint32_t)(pts.size()/N));
            polyroot.push_back(curroot); polyletter.push_back(curletter);
            push(a);
            open = true;
        }
        if (!sd.tipline.empty())                       // through the leaf's tips
            for (const typename B::DVec& v : sd.tipline[f.j]) { push(B::boundary(B::act(hd, v))); ++nTipPoints; }
        push(b);
        if (nLeaves >= leafcap) capped = true;
    }
    void draw(const Elem& g, int j, int rootindex = -1)
    {
        if (capped) return;
        open = false;
        curroot = rootindex; curletter = j;
        stk.clear();
        stk.push_back({g, j, 0, 0, fingerprint(B::key(g), L.canon[j]), 0, -1});
        while (!stk.empty() && !capped) {
            Frame f = stk.back();
            stk.pop_back();
            if (f.kind == 1) { open = false; continue; }
            if (!done.insert(f.key)) { ++nSkipped; open = false; continue; }
            ++nVisited;
            if (reverse) mark_reverse(f.h, f.j);
            if (onvisit) onvisit(*this, f.h, f.j);
            visit(f);
        }
    }
    void visit(const Frame& f)
    {
        typename B::DMat hd = B::to_double(f.h);
        Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        int kind = f.kind;                                // culling, as the renderer
        if (cull && kind != 2) {
            int c = cull_test(f.h, hd, f.j, a, b);
            if (c < 0) { ++nCulled; open = false; return; }
            if (c > 0) { kind = 2; ++nInside; }
        }
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; ++nSpike; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, a, b, acc)) { split = true; ++nSpike; ++nThrough; }     // (b), as the renderer
        if (split && f.depth < maxdepth && B::max_coefficient(f.h) < (1LL << 40)) {
            deepest = std::max(deepest, f.depth + 1);
            const std::vector<int>& roof = sd.roof[f.j];
            for (int i = (int)roof.size() - 1; i >= 0; i--) {
                int x = roof[i];
                uint64_t kc = fingerprint(B::product_key(f.h, sd.keyoff[f.j][i]), L.canon[x]);
                if (done.contains(kc)) { stk.push_back({f.h, 0, 0, 1, 0, 0, -1}); continue; }
                stk.push_back({B::mul(f.h, sd.off[f.j][i]), x, f.depth + 1, kind, kc, i + 1, f.j});
            }
        } else {
            if (split) ++nForced;
            emit(f, a, b, split, hd);
            if (overlap > 0 && !split) mark(f.h, f.j, a, b);   // marking, as the renderer
        }
    }
    // the same drawing as the renderer r?
    bool check(const Renderer<B>& r) const
    {
        return pts == r.pts && starts == r.starts && polyroot == r.polyroot && polyletter == r.polyletter
            && nVisited == r.nVisited && nSkipped == r.nSkipped && nLeaves == r.nLeaves && nSpike == r.nSpike
            && nForced == r.nForced && deepest == r.deepest && capped == r.capped && nTipPoints == r.nTipPoints
            && nThrough == r.nThrough && nMarked == r.nMarked && nReverse == r.nReverse && nCulled == r.nCulled
            && nInside == r.nInside && nCullOpen == r.nCullOpen;
    }
};

// the view options of zipper3 and zipper5 (their defaults: pitch 20 in S^3 and 15 in S^4, when pitch < 0)
struct ViewOptions {
    double zoom = 1, clip = 3, yaw = 0, pitch = -1, pole1 = 0, pole2 = 0, turn = 0, turn2 = 0;
    std::vector<double> centre;                        // empty: the default centre t^+
};

// the two painters behind one interface: View<4> (S^3, view/s3.h) and View<5> (S^4, view/s4.h)
template <int N> struct View;

template <>
struct View<4> {
    typedef PU21 B;
    typedef s3::ViewState V;
    static constexpr int default_size = 900;
    static void defaults(const World<PU21>& W, V& v) { s3::init_view(W, v, 0, 20, 0, 0, 1, 3); }   // zipper3's
    // the view of zipper3 with these options (apps/s3/zipper3.cc), and its window (view/s3.h: window3)
    static void program(const World<PU21>& W, const ViewOptions& o, V& v)
    {
        s3::init_view(W, v, o.yaw, o.pitch < 0 ? 20 : o.pitch, o.pole1, o.pole2, o.zoom, o.clip);
        if (o.centre.size() == 4) {
            double n = 0;
            for (double x : o.centre) n += x*x;
            SpherePoint<4> c;
            for (int k = 0; k < 4; k++) c.x[k] = o.centre[k]/std::sqrt(n);
            s3::default_frame(c, v.Q);
            double e1[3] = {1,0,0}, e2[3] = {0,1,0};
            s3::move_pole(v.Q, e1, o.pole1*s3::PI/180);
            s3::move_pole(v.Q, e2, o.pole2*s3::PI/180);
        }
    }
    static CullWindow<4> window(const V& v, double K) { return s3::window3(v, K); }
    static std::vector<double> centre(const V& v) { return {-v.Q[3][0], -v.Q[3][1], -v.Q[3][2], -v.Q[3][3]}; }
    // a point -> eye coordinates (screen x, screen y, depth, and 0); false outside the clip ball
    static bool eye(const V& v, const SpherePoint<4>& p, double e[4])
    {
        float x[4] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3]};
        e[3] = 0;
        return s3::eye(v, x, e);
    }
    // the frame F (centre, screen x, screen y, depth): half-width H (in chords) at the centre
    static void frame(V& v, const std::vector<SpherePoint<4>>& F, double H)
    {
        for (int k = 0; k < 4; k++) {
            v.Q[0][k] = F[1].x[k]; v.Q[1][k] = -F[3].x[k]; v.Q[2][k] = F[2].x[k]; v.Q[3][k] = -F[0].x[k];
        }
        s3::rot3(0, 0, v.rot);                          // E1 -> x, E3 -> y, E2 -> -z
        v.clip = H; v.zoom = 2;                         // near the centre the projection halves chords
    }
    static double half(const V& v) { return v.clip/v.zoom; }
    static bool paint(const char* fn, int size, const std::vector<const Renderer<PU21>*>& r,
                      const std::vector<std::array<double, 3>>& col, const V& v,
                      const std::vector<SpherePoint<4>>& marks, int thick = 0)
    {
        std::vector<s3::Layer<PU21>> layers;
        for (size_t i = 0; i < r.size(); i++) layers.push_back({r[i], {col[i][0], col[i][1], col[i][2]}, (int)i});
        return s3::write_png(fn, size, layers, v, marks, thick);
    }
};

template <>
struct View<5> {
    typedef imm::O51 B;
    typedef s4::View5 V;
    static constexpr int default_size = 840;
    static void defaults(const World<imm::O51>& W, V& v)                       // zipper5's
    {
        s4::init_view5(W, v, 0, 15);
        v.clip = 3; v.zoom = 1; v.hidfog = 1.5;
    }
    // the view of zipper5 with these options (apps/s4/zipper5.cc; its --hidfog stays 1.5), and its window
    // (view/s4.h: window5)
    static void program(const World<imm::O51>& W, const ViewOptions& o, V& v)
    {
        s4::init_view5(W, v, o.yaw, o.pitch < 0 ? 15 : o.pitch);
        if (o.centre.size() >= 5) {
            double n = 0, p[5];
            for (int k = 0; k < 5; k++) n += o.centre[k]*o.centre[k];
            for (int k = 0; k < 5; k++) p[k] = -o.centre[k]/std::sqrt(n);
            s4::orthonormal_frame(p, v.Q);
        }
        v.clip = o.clip; v.zoom = o.zoom; v.hidfog = 1.5;
        s4::turn4(v.U, 2, 3, o.turn*s4::PI/180);
        s4::turn4(v.U, 0, 3, o.turn2*s4::PI/180);
    }
    static CullWindow<5> window(const V& v, double K) { return s4::window5(v, K); }
    static std::vector<double> centre(const V& v)
    {
        return {-v.Q[4][0], -v.Q[4][1], -v.Q[4][2], -v.Q[4][3], -v.Q[4][4]};
    }
    // a point -> eye coordinates (screen x, screen y, depth, hidden); false outside the clip ball
    static bool eye(const V& v, const SpherePoint<5>& p, double e[4])
    {
        float x[5] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3], (float)p.x[4]};
        return s4::eye5(v, x, e, e[3]);
    }
    // the frame F (centre, screen x, screen y, depth, hidden): half-width H (in chords) at the centre
    static void frame(V& v, const std::vector<SpherePoint<5>>& F, double H)
    {
        for (int k = 0; k < 5; k++) {
            v.Q[0][k] = F[1].x[k]; v.Q[1][k] = F[2].x[k]; v.Q[2][k] = F[3].x[k]; v.Q[3][k] = F[4].x[k];
            v.Q[4][k] = -F[0].x[k];
        }
        std::memset(v.U, 0, sizeof v.U);
        for (int i = 0; i < 4; i++) v.U[i][i] = 1;
        s4::rot3(0, 0, v.rot);
        v.clip = H; v.zoom = 2; v.hidfog = H/2;
    }
    static double half(const V& v) { return v.clip/v.zoom; }
    static bool paint(const char* fn, int size, const std::vector<const Renderer<imm::O51>*>& r,
                      const std::vector<std::array<double, 3>>& col, const V& v,
                      const std::vector<SpherePoint<5>>& marks, int thick = 0)
    {
        std::vector<s4::Layer5> layers;
        for (size_t i = 0; i < r.size(); i++) layers.push_back({r[i], {col[i][0], col[i][1], col[i][2]}, (int)i});
        return s4::write_png5(fn, size, layers, v, marks, thick);
    }
};

}  // namespace tools
}  // namespace zippers
