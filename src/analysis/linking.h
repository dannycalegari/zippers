// SPDX-License-Identifier: GPL-3.0-or-later
//
// linking.h: linking numbers of closed polylines on S^3, and pictures of two of them.
//
// A loop gamma in Z+ and a loop delta in Z- (render/arcs.h: loop_points, from relations: analysis/relations.h) are
// disjoint closed polylines on S^3. Their linking number is computed in R^3: both are projected stereographically
// from a pole far from both, and the number is the sum of the signed crossings of one over the other in a random
// planar projection (the counts "A over B" and "B over A" must agree). It is trusted only when the loops are much
// farther apart than the accuracy, and the value is stable under refinement (link3 decides that).
//   Closed polylines in R^3 (s3::V3; the segment from the last point back to the first is included):
//     crossings        the signed crossings of A over B and of B over A in the projection to the xy-plane, and their
//                      number, with a grid of cells (about 4 times the mean segment length of A, at most 4096 a side)
//     gauss_lk         the Gauss integral, by the exact solid angles of the segment pairs (Klenin-Langowski): for
//                      testing
//     random_rotation  a rotation of R^3 from a random unit quaternion (four normal draws)
//     rotate           the points rotated
//   Point sets on S^3 (SpherePoint<4>):
//     minsep           the smallest distance between two point sets, searched below h (h if none), with a grid of
//                      cells of side h (81 neighbouring cells of R^4)
//     separation       the true smallest distance (minsep with h = 0.02, 0.04, ... until found; 2 if none below 2.1)
//     far_pole         a pole far from two curves: 64 random candidates ranked against subsamples, then checked in
//                      that order against all points until one is at least 0.15 away; returns that distance
//     linking          the linking number of two disjoint closed polylines on S^3: far_pole, frame_with_pole,
//                      random_rotation, then crossings; the separation (minsep below sephint if sephint > 0, else
//                      separation), the pole and its distance
//     draw_pair        a picture of two loops (A blue, B red, 900 pixels, brush 2), stereographically projected from
//                      the given pole, with the clip radius 1.02 times the largest radius of the two
//
// THE RANDOM NUMBERS. far_pole and random_rotation draw from the generator they are given, each through a new
// std::normal_distribution<double>(0, 1): 64 x 4 draws for far_pole, 4 for random_rotation; linking calls far_pole,
// then random_rotation. The projections, and so the crossing counts, depend on every draw and its order: keep them.
//
// Moved from legacy/s3/link3.cc (crossings, gauss_lk, random_rotation, rotate, minsep, separation, LinkResult,
// linking, draw_pair) and legacy/s3/zipcore.h (far_pole), with their names, in namespace zippers. Every expression,
// every draw of the random numbers and every sort is the legacy one, in the same order
// (tests/unit/test_analysis_s3.cc). Changes of form: the points of S^3 are SpherePoint<4> (the legacy P4) and chord
// (geometry/sphere.h) the legacy d4; the view's functions and PI are s3:: (view/s3.h); draw_pair is a template over
// the backend (for any backend whose sphere at infinity is S^3; the legacy Renderer, Side and Letters are those of
// PU21), with its template parameter named Backend, so that the two loops keep their legacy names A and B.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <unordered_map>
#include <vector>

#include "geometry/sphere.h"
#include "render/renderer.h"
#include "view/s3.h"

namespace zippers {

// ---------------------------------------------------------------------------------------
// Linking numbers of closed polylines in R^3

// sum of signed crossings of A over B, and of B over A, in the projection to the xy-plane;
// both closed (the segment from the last point back to the first is included)
inline void crossings(const std::vector<s3::V3>& A, const std::vector<s3::V3>& B, long& aOverB, long& bOverA, long& ncross)
{
    aOverB = bOverA = ncross = 0;
    size_t n = A.size(), m = B.size();
    if (n < 2 || m < 2) return;
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300}, len = 0;
    for (const s3::V3& p : A) for (int c = 0; c < 2; c++) { lo[c] = std::min(lo[c], p[c]); hi[c] = std::max(hi[c], p[c]); }
    for (const s3::V3& p : B) for (int c = 0; c < 2; c++) { lo[c] = std::min(lo[c], p[c]); hi[c] = std::max(hi[c], p[c]); }
    for (size_t i = 0; i < n; i++) { const s3::V3 &p = A[i], &q = A[(i+1)%n]; len += std::hypot(q[0]-p[0], q[1]-p[1]); }
    double cell = std::max(4*len/n, 1e-12);
    long nx = std::min(4096L, (long)((hi[0]-lo[0])/cell) + 1), ny = std::min(4096L, (long)((hi[1]-lo[1])/cell) + 1);
    double cx = (hi[0]-lo[0])/nx + 1e-300, cy = (hi[1]-lo[1])/ny + 1e-300;
    std::unordered_map<long, std::vector<uint32_t>> grid;
    auto cellrange = [&](const s3::V3& p, const s3::V3& q, long& x0, long& x1, long& y0, long& y1) {
        x0 = std::max(0L, (long)((std::min(p[0],q[0]) - lo[0])/cx)); x1 = std::min(nx-1, (long)((std::max(p[0],q[0]) - lo[0])/cx));
        y0 = std::max(0L, (long)((std::min(p[1],q[1]) - lo[1])/cy)); y1 = std::min(ny-1, (long)((std::max(p[1],q[1]) - lo[1])/cy));
    };
    for (size_t i = 0; i < n; i++) {
        long x0, x1, y0, y1;
        cellrange(A[i], A[(i+1)%n], x0, x1, y0, y1);
        for (long x = x0; x <= x1; x++) for (long y = y0; y <= y1; y++) grid[x*ny + y].push_back((uint32_t)i);
    }
    std::vector<uint32_t> stamp(n, (uint32_t)-1);
    for (size_t k = 0; k < m; k++) {
        const s3::V3 &p = B[k], &q = B[(k+1)%m];
        long x0, x1, y0, y1;
        cellrange(p, q, x0, x1, y0, y1);
        for (long x = x0; x <= x1; x++) for (long y = y0; y <= y1; y++) {
            auto it = grid.find(x*ny + y);
            if (it == grid.end()) continue;
            for (uint32_t i : it->second) {
                if (stamp[i] == (uint32_t)k) continue;            // already tested against this segment
                stamp[i] = (uint32_t)k;
                const s3::V3 &a = A[i], &b = A[(i+1)%n];
                double ux = b[0]-a[0], uy = b[1]-a[1], vx = q[0]-p[0], vy = q[1]-p[1];
                double den = ux*vy - uy*vx;
                if (den == 0) continue;
                double wx = p[0]-a[0], wy = p[1]-a[1];
                double s = (wx*vy - wy*vx)/den, u = (wx*uy - wy*ux)/den;
                if (s < 0 || s >= 1 || u < 0 || u >= 1) continue;
                double za = a[2] + s*(b[2]-a[2]), zb = p[2] + u*(q[2]-p[2]);
                // the sign of the crossing is that of (over direction) x (under direction)
                // seen from +z; den is (A direction) x (B direction)
                int sg = den > 0 ? 1 : -1;
                ++ncross;
                if (za > zb) aOverB += sg; else bOverA -= sg;
            }
        }
    }
}

// the Gauss integral by the exact segment-pair solid angles (Klenin-Langowski): for testing
inline double gauss_lk(const std::vector<s3::V3>& A, const std::vector<s3::V3>& B)
{
    using s3::V3;
    auto sub = [](const V3& a, const V3& b) { return V3{a[0]-b[0], a[1]-b[1], a[2]-b[2]}; };
    auto cr = [](const V3& a, const V3& b) { return V3{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]}; };
    auto dt = [](const V3& a, const V3& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
    auto unit = [&](V3 a) { double l = std::sqrt(dt(a, a)); for (double& x : a) x /= l; return a; };
    double tot = 0;
    size_t n = A.size(), m = B.size();
    for (size_t i = 0; i < n; i++) for (size_t k = 0; k < m; k++) {
        const V3 &p1 = A[i], &p2 = A[(i+1)%n], &p3 = B[k], &p4 = B[(k+1)%m];
        V3 r13 = sub(p3,p1), r14 = sub(p4,p1), r23 = sub(p3,p2), r24 = sub(p4,p2);
        V3 n1 = unit(cr(r13, r14)), n2 = unit(cr(r14, r24)), n3 = unit(cr(r24, r23)), n4 = unit(cr(r23, r13));
        auto as = [](double x) { return std::asin(std::max(-1.0, std::min(1.0, x))); };
        double om = as(dt(n1,n2)) + as(dt(n2,n3)) + as(dt(n3,n4)) + as(dt(n4,n1));
        double sgn = dt(cr(sub(p4,p3), sub(p2,p1)), r13);
        tot += sgn > 0 ? om : -om;
    }
    return tot/(4*s3::PI);
}

inline void random_rotation(std::mt19937_64& rng, double R[9])
{
    std::normal_distribution<double> N(0, 1);
    double q[4], s = 0;
    for (double& x : q) { x = N(rng); s += x*x; }
    for (double& x : q) x /= std::sqrt(s);
    double a = q[0], b = q[1], c = q[2], d = q[3];
    double M[9] = { a*a+b*b-c*c-d*d, 2*(b*c-a*d), 2*(b*d+a*c),
                    2*(b*c+a*d), a*a-b*b+c*c-d*d, 2*(c*d-a*b),
                    2*(b*d-a*c), 2*(c*d+a*b), a*a-b*b-c*c+d*d };
    std::memcpy(R, M, sizeof M);
}

inline std::vector<s3::V3> rotate(const std::vector<s3::V3>& X, const double R[9])
{
    std::vector<s3::V3> Y(X.size());
    for (size_t i = 0; i < X.size(); i++)
        for (int r = 0; r < 3; r++) Y[i][r] = R[3*r]*X[i][0] + R[3*r+1]*X[i][1] + R[3*r+2]*X[i][2];
    return Y;
}

// ---------------------------------------------------------------------------------------
// Loops in S^3, and their linking number

// the smallest distance between two point sets on S^3, searched below h (returns h if none)
inline double minsep(const std::vector<SpherePoint<4>>& A, const std::vector<SpherePoint<4>>& B, double h)
{
    std::unordered_map<int64_t, std::vector<uint32_t>> grid;
    auto ci = [&](double x) { return (int64_t)std::floor((x + 2)/h); };
    auto key = [&](const int64_t c[4]) { int64_t k = 0; for (int i = 0; i < 4; i++) k = k*1000003 + c[i]; return k; };
    for (size_t i = 0; i < B.size(); i++) {
        int64_t c[4];
        for (int k = 0; k < 4; k++) c[k] = ci(B[i].x[k]);
        grid[key(c)].push_back((uint32_t)i);
    }
    double best = h;
    for (const SpherePoint<4>& a : A) {
        int64_t c0[4];
        for (int k = 0; k < 4; k++) c0[k] = ci(a.x[k]);
        for (int m = 0; m < 81; m++) {
            int64_t c[4] = {c0[0] + m%3 - 1, c0[1] + (m/3)%3 - 1, c0[2] + (m/9)%3 - 1, c0[3] + (m/27)%3 - 1};
            auto it = grid.find(key(c));
            if (it == grid.end()) continue;
            for (uint32_t j : it->second) best = std::min(best, chord(a, B[j]));
        }
    }
    return best;
}

// the true smallest distance between two point sets on S^3 (doubling the search radius)
inline double separation(const std::vector<SpherePoint<4>>& A, const std::vector<SpherePoint<4>>& B)
{
    for (double h = 0.02; h < 2.1; h *= 2) {
        double d = minsep(A, B, h);
        if (d < h) return d;
    }
    return 2;
}

// A pole far from two curves on S^3.  Near the pole the image of a short chord is a huge arc,
// which a straight segment in R^3 misrepresents, so the distance from the pole to EVERY point
// must be large: 64 random candidates are ranked against subsamples, then checked in that order
// against all points until one is at least 0.15 away.  Returns that distance.
template <class RNG>
inline double far_pole(const std::vector<SpherePoint<4>>& A, const std::vector<SpherePoint<4>>& B, RNG& rng, double pole[4])
{
    std::normal_distribution<double> N(0, 1);
    struct Cand { double sub; SpherePoint<4> p; };
    std::vector<Cand> cand;
    size_t sa = std::max<size_t>(1, A.size()/400), sb = std::max<size_t>(1, B.size()/400);
    for (int trial = 0; trial < 64; trial++) {
        SpherePoint<4> pp;
        double s = 0;
        for (double& x : pp.x) { x = N(rng); s += x*x; }
        for (double& x : pp.x) x /= std::sqrt(s);
        double mn = 9;
        for (size_t i = 0; i < A.size(); i += sa) mn = std::min(mn, chord(pp, A[i]));
        for (size_t i = 0; i < B.size(); i += sb) mn = std::min(mn, chord(pp, B[i]));
        cand.push_back({mn, pp});
    }
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.sub > b.sub; });
    double best = -1;
    for (const Cand& c : cand) {
        double mn = 9;
        for (const SpherePoint<4>& q : A) mn = std::min(mn, chord(c.p, q));
        for (const SpherePoint<4>& q : B) mn = std::min(mn, chord(c.p, q));
        if (mn > best) { best = mn; std::memcpy(pole, c.p.x, 4*sizeof(double)); }
        if (mn >= 0.15) break;
    }
    return best;
}

struct LinkResult { long lk = 0; bool agree = false; long ncross = 0; double sep = 0, poledist = 0; double pole[4]; };

// the linking number of two disjoint closed polylines on S^3
inline LinkResult linking(const std::vector<SpherePoint<4>>& A, const std::vector<SpherePoint<4>>& B, std::mt19937_64& rng, double sephint)
{
    LinkResult r;
    double pole[4];
    r.poledist = far_pole(A, B, rng, pole);
    double F[4][4], R[9];
    s3::frame_with_pole(pole, F);
    random_rotation(rng, R);
    std::vector<s3::V3> a = rotate(s3::stereo(A, F), R), b = rotate(s3::stereo(B, F), R);
    long ab, ba;
    crossings(a, b, ab, ba, r.ncross);
    r.lk = ab;
    r.agree = (ab == ba);
    r.sep = sephint > 0 ? minsep(A, B, sephint) : separation(A, B);
    std::memcpy(r.pole, pole, sizeof pole);
    return r;
}

// a picture of two loops: A blue, B red, stereographically projected from the given pole
template <class Backend>
inline void draw_pair(const char* fn, const std::vector<SpherePoint<4>>& A, const std::vector<SpherePoint<4>>& B, const double pole[4],
                      const Side<Backend>& sd, const Letters<Backend>& L, double yaw = 30, double pitch = 20)
{
    static_assert(Backend::N == 4, "the views of S^3 are for backends whose sphere at infinity is S^3");
    Renderer<Backend> ra(sd, L, 1, 1), rb(sd, L, 1, 1);
    for (auto* pr : {&ra, &rb}) {
        const std::vector<SpherePoint<4>>& X = pr == &ra ? A : B;
        pr->starts.push_back(0);
        for (const SpherePoint<4>& p : X) pr->push(p);
        pr->push(X[0]);                          // close the loop
    }
    s3::ViewState v;
    s3::frame_with_pole(pole, v.Q);
    double rmax = 0;
    for (const std::vector<SpherePoint<4>>* X : {&A, &B})
        for (const s3::V3& y : s3::stereo(*X, v.Q)) rmax = std::max(rmax, std::sqrt(y[0]*y[0] + y[1]*y[1] + y[2]*y[2]));
    v.clip = 1.02*rmax;
    s3::rot3(yaw, pitch, v.rot);
    std::vector<s3::Layer<Backend>> layers = {{&ra, {0.05, 0.15, 0.85}}, {&rb, {0.85, 0.08, 0.08}}};
    s3::write_png(fn, 900, layers, v, {}, 2);
}

}  // namespace zippers
