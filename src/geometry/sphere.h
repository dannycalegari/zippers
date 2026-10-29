// SPDX-License-Identifier: GPL-3.0-or-later
//
// sphere.h: points of the unit sphere S^{N-1} in R^N, and the two distances the renderers use.
//
// These are the legacy functions d4, segd4 (legacy/s3/zipcore.h, N = 4) and d5, segd5 (legacy/s4/zipcore5.h, N = 5),
// written once for every N. Every expression is the legacy one, in the same order, so the results are bit for bit
// the same (tests/unit/test_sphere_*.cc checks this). Do not "simplify" them: a changed expression changes the
// rounding, and so the pictures.

#pragma once

#include <algorithm>
#include <cmath>

namespace zippers {

// a point of R^N, normally on the unit sphere S^{N-1} (legacy P4, P5)
template <int N>
struct SpherePoint {
    double x[N];
};

// the Euclidean distance in R^N (the chord) between two points (legacy d4, d5)
template <int N>
inline double chord(const SpherePoint<N>& a, const SpherePoint<N>& b)
{
    double s = 0;
    for (int i = 0; i < N; i++) s += (a.x[i]-b.x[i])*(a.x[i]-b.x[i]);
    return std::sqrt(s);
}

// the Euclidean distance in R^N from p to the segment [a, b] (legacy segd4, segd5)
template <int N>
inline double segment_distance(const SpherePoint<N>& p, const SpherePoint<N>& a, const SpherePoint<N>& b)
{
    double u[N], w[N], uu = 0, uw = 0;
    for (int i = 0; i < N; i++) { u[i] = b.x[i]-a.x[i]; w[i] = p.x[i]-a.x[i]; uu += u[i]*u[i]; uw += u[i]*w[i]; }
    double t = uu > 0 ? std::max(0.0, std::min(1.0, uw/uu)) : 0;
    double s = 0;
    for (int i = 0; i < N; i++) { double e = w[i] - t*u[i]; s += e*e; }
    return std::sqrt(s);
}

}  // namespace zippers
