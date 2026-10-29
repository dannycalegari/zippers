// SPDX-License-Identifier: GPL-3.0-or-later
//
// separation.h: the smallest distance between the drawn Z+ and the drawn Z-.
//
// The zippers are disjoint, so for a sound picture the distance between the two drawings must stay positive, and
// converge as the accuracy is refined (zipper3 --check). zdist measures it on S^3 (the chord in R^4), from the
// vertices of one renderer to the segments of the other, searching only distances below a cell size h: the segments
// of b are put in a grid of cells of side h by their first vertex (the segments are shorter than h), and each vertex
// of a is compared with the segments of the 3^4 = 81 cells around its own. It returns h if no segment is nearer than
// h, and otherwise the distance, with the vertex wa of a and the first vertex wb of the segment of b that attain it
// (the first such pair, in the order of the polylines).
//
// Moved from legacy/s3/zipcore.h (zdist), as it is: written for four dimensions (81 neighbouring cells of R^4), so for
// a backend B whose sphere at infinity is S^3 (B::N = 4) only. Every
// expression is the legacy one, in the same order (tests/unit/test_view_s3.cc). Changes of form: a template over the
// backend (the legacy Renderer is Renderer<PU21>), the points SpherePoint<4> (the legacy P4), segment_distance
// (geometry/sphere.h, the legacy segd4); the legacy local struct Cell, which nothing used, is left out.

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "geometry/sphere.h"
#include "render/renderer.h"

namespace zippers {

// The smallest distance on S^3 (chord in R^4) between the drawn Z+ and the drawn Z-, from
// vertex to segment: the zippers are disjoint, so for a sound picture this must stay
// positive and converge as the accuracy is refined.  Only distances below `h` are searched.
template <class B>
inline double zdist(const Renderer<B>& a, const Renderer<B>& b, double h, SpherePoint<4>& wa, SpherePoint<4>& wb)
{
    static_assert(B::N == 4, "zdist is written for S^3: 81 neighbouring cells of R^4");
    auto key = [&](const float* x) {
        int64_t k = 0;
        for (int c = 0; c < 4; c++) k = k*1000003 + (int64_t)std::floor((x[c] + 2)/h);
        return k;
    };
    // cells of b's segments, by their first vertex (segments are shorter than h)
    std::unordered_map<int64_t, std::vector<uint32_t>> grid;
    for (size_t k = 0; k < b.npoly(); k++)
        for (size_t i = b.pbegin(k); i + 1 < b.pend(k); i++) grid[key(&b.pts[4*i])].push_back(i);
    double best = h;
    for (size_t k = 0; k < a.npoly(); k++)
        for (size_t i = a.pbegin(k); i < a.pend(k); i++) {
            const float* x = &a.pts[4*i];
            int base[4];
            for (int c = 0; c < 4; c++) base[c] = (int)std::floor((x[c] + 2)/h);
            for (int m = 0; m < 81; m++) {
                int d[4] = {m % 3 - 1, (m/3) % 3 - 1, (m/9) % 3 - 1, (m/27) % 3 - 1};
                int64_t kk = 0;
                for (int c = 0; c < 4; c++) kk = kk*1000003 + base[c] + d[c];
                auto it = grid.find(kk);
                if (it == grid.end()) continue;
                for (uint32_t j : it->second) {
                    SpherePoint<4> p, q0, q1;
                    for (int c = 0; c < 4; c++) { p.x[c] = x[c]; q0.x[c] = b.pts[4*j + c]; q1.x[c] = b.pts[4*j + 4 + c]; }
                    double e = segment_distance(p, q0, q1);
                    if (e < best) { best = e; wa = p; wb = q0; }
                }
            }
        }
    return best;
}

}  // namespace zippers
