// SPDX-License-Identifier: GPL-3.0-or-later
//
// cull.h: the window of a zoomed picture, as the renderer's culling sees it.
//
// THE WINDOW. A painter (view/s3.h, view/s4.h) draws a segment when both its ends pass the clip ball of its
// stereographic projection, and then the part of it that lands in the screen square. Pulled back to the sphere, each
// of those conditions is a half-space of R^N, exactly: the screen square |e_0|, |e_1| <= clip/zoom gives four (a plane
// of the stereographic picture is a round sphere of S^{N-1} through the pole, the intersection of S^{N-1} with an
// affine hyperplane of R^N), and the clip ball gives one (a cap about the view centre). So a point x of S^{N-1} is
// in the WINDOW when
//     u_i . x <= c_i   for i = 1, ..., n,
// with u_i unit vectors of R^N. window3 (view/s3.h) and window5 (view/s4.h) compute them from a view; nothing else
// about the projection is needed, so the test is exact up to rounding everywhere, and in particular near the pole of
// the projection, where the stereographic picture of a small piece of the sphere is huge: the four sides of the square
// pull back to caps whose boundaries all pass through the pole, and the clip cap keeps it out.
// The ends of a drawn segment are points of S^{N-1}, and its picture on the screen lies between their pictures, so a
// segment whose ends are both beyond one of the half-spaces is not drawn at all.
//
// THE TEST of a branch (Renderer<B>::cull_test, render/renderer.h) uses the points of the branch that the renderer
// knows: its ends a and b, the vertices of its first split, and its tips (render/tips.h); for a letter whose roof is
// one letter, those of the first branch of its chain of one-letter roofs whose roof is longer, which is the same arc
// (the look-ahead (b) of render/renderer.h). Its SIZE s is the chord |a - b|, or the farthest of those vertices and tips
// from the chord if that is larger (the tips count here, though not for the renderer's acceptance of a leaf: see
// render/renderer.h, cull_points). The curve of the branch is taken to lie within K s of the convex hull of those
// points. That is a MEASURED BET, not a bound (there are no enclosures in S^3 and S^4; docs/developer_guide.md,
// "Culling", gives the strays measured, the margin they leave at K = 16, and where they were not measured). Then
//     test(...) = -1   if for some i every point p has u_i . p > c_i + K s: the curve misses the window, and the branch
//                      is CULLED (not split, not drawn);
//                  1   if every point has u_i . p < c_i - K s for every i: the curve lies inside the window, and the
//                      descendants of the branch are drawn without a test;
//                  0   otherwise.
// The margin K s is relative to the branch, so it scales with the zoom by itself: near the edge of a window 1/zoom
// wide the branches that decide are those of size about 1/zoom, and their margins are K/zoom (a fixed margin, such as
// K ACC, would keep a band K ACC wide around the window at every zoom).

#pragma once

#include <cmath>
#include <cstddef>

#include "geometry/sphere.h"

namespace zippers {

// the default margin factor K. With K = 16 only branches of size below 1/16 can be culled at all (a point of the sphere is at
// most 1 beyond a window's half-space, and those of the windows here at most about 1), and among them the largest stray, in
// units of the branch's size, is the "K = 16" column of the "cullable" line of `cull --mode margin -a 0.04` (apps/tools/cull.cc;
// one branch in 16 below depth 2, and one in 64, with --sample 64, at R = 4.5), the safety factor 16 / stray being in brackets:
//     S^3 subst_4.00_9.50_t3_r1.txt    Z+: 1.34 up to R = 3.983, 2.84 at R = 3.99, 3.32 at R = 4.5 (4.8);
//                                      Z-: 2.37 up to R = 3.983, 3.12 at R = 3.99 and 4.5 (5.1)
//     S^3 subst_4.00_9.50_t3.txt       2.68 at R = 3.983 (6.0), 2.92 at R = 3.99, 2.95 at R = 4.5 (5.4)
//     S^4 subst5_2.30_4.50_t1_r0.txt   1.42 at R = 2.1 (11.2)
//     S^4 subst5_2.30_4.50_t1.txt      1.51 at R = 2.1 (10.6)
// Larger R is unmeasured. For comparison, the K = 8 column gives 3.3 sizes (2.4) on subst_4.00_9.50_t3_r1.txt at R = 3.983 but
// 6.3 (1.26) at R = 3.99 and 4.5 (the root branches, those of depth 0, of far roots, whose first split does not say where the
// curve goes), and 3.4 (2.3) on subst_4.00_9.50_t3.txt there; the K = 4 column gives 3.3 (1.2) on the latter at R = 3.983. K = 16 keeps
// more leaves than K = 8: for a still at the plain accuracy, at zooms 10 to 1000, 1.06 to 1.8 times as many in S^3 and 1.2 to
// 3.8 times in S^4. NOT ENOUGH for the S^3 file subst_4.00_9.00_t1.txt, whose branches stray up to 8.9 sizes already at R = 0
// (its leaves stray up to 4.9 ACC from their chords at ACC 0.08: the roofs of that file, not the margin); use --cullk 32 for it.
constexpr double CULL_K = 16;

template <int N>
struct CullWindow {
    static constexpr int MAXH = 8;
    int n = 0;                       // the number of half-spaces
    double u[MAXH][N];               // unit normals
    double c[MAXH];                  // the window: u_i . x <= c_i for every i
    double K = CULL_K;               // the margin factor

    // add the half-space n . x <= d (n any nonzero vector: both sides are divided by |n|)
    void add(const double nv[N], double d)
    {
        double l = 0;
        for (int k = 0; k < N; k++) l += nv[k]*nv[k];
        l = std::sqrt(l);
        for (int k = 0; k < N; k++) u[n][k] = nv[k]/l;
        c[n] = d/l;
        n++;
    }
    // is the point x in the window?
    bool contains(const SpherePoint<N>& x) const
    {
        for (int i = 0; i < n; i++) if (excess(i, x) > 0) return false;
        return true;
    }
    // how far x is beyond the boundary of the half-space i (negative inside)
    double excess(int i, const SpherePoint<N>& x) const
    {
        double v = 0;
        for (int k = 0; k < N; k++) v += u[i][k]*x.x[k];
        return v - c[i];
    }
    // the test of points p[0..np) with the margin m (the top of the file): -1 beyond one half-space by more than m,
    // 1 inside every half-space by more than m, 0 otherwise
    int test(const SpherePoint<N>* p, size_t np, double m) const
    {
        bool in = true;
        for (int i = 0; i < n; i++) {
            double lo = INFINITY, hi = -INFINITY;
            for (size_t k = 0; k < np; k++) {
                double v = excess(i, p[k]);
                lo = v < lo ? v : lo;
                hi = v > hi ? v : hi;
            }
            if (lo > m) return -1;
            if (hi >= -m) in = false;
        }
        return in ? 1 : 0;
    }
};

}  // namespace zippers
