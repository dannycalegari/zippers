// SPDX-License-Identifier: GPL-3.0-or-later
//
// arcs.h: complete arcs and loops of a zipper, for any backend B.
//
// The renderer (render/renderer.h) draws each branch once, so its polylines are pieces of arcs. These functions give
// whole curves instead, with no de-duplication: every leaf of a branch, in order.
//   arc_points    the points of the arc h alpha_j (the branch (h, j)), with the same splitting rule as the renderer
//                 (depth limit 400, the growth guard 2^40), with the look-ahead through one-letter roofs when the
//                 Side has it, and each leaf drawn as the renderer draws it, through its tips when the Side has them
//                 (render/renderer.h). `forced` counts the leaves split no further although they were not fine
//                 enough (the depth limit or the growth guard)
//   loop_points   the closed polyline h0 . (alpha_{j_1} u s_{j_1} alpha_{j_2} u ...) of a relation w = j_1 j_2 ... of
//                 the letters (a word whose product is the identity); it exits if w is not one
//
// Moved from legacy/s3/zipcore.h (arc_points, loop_points) and legacy/s4/zipcore5.h (arc_points5), the same code up
// to the arithmetic, as templates on the backend; every expression is the legacy one, in the same order, so a Side
// without tips and without the look-ahead through one-letter roofs gives the legacy points bit for bit
// (tests/unit/test_render_cs.cc, test_render_imm.cc). The one
// difference between the two legacy copies: arc_points counted the forced leaves and arc_points5 did not. Here every
// caller gets the counter; one that does not need it (as the legacy S^4 code) ignores it.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "geometry/backend.h"
#include "geometry/sphere.h"
#include "render/renderer.h"

namespace zippers {

// all the leaves of the branch (h0, j0) in order, appended to pts (the first point only if pts is empty); no
// de-duplication, so the arc is complete (legacy arc_points, arc_points5)
template <class B>
inline void arc_points(const Side<B>& sd, const typename B::Elem& h0, int j0, double acc, std::vector<typename B::Point>& pts,
                       size_t& forced)
{
    struct F { typename B::Elem h; int j, depth; };
    std::vector<F> st;
    st.push_back({h0, j0, 0});
    while (!st.empty()) {
        F f = st.back();
        st.pop_back();
        typename B::DMat hd = B::to_double(f.h);
        typename B::Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++) if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, a, b, acc)) split = true;          // (b), as Renderer<B>::visit
        if (split && f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40)) {
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1});
        } else {
            if (split) forced++;
            if (pts.empty()) pts.push_back(a);
            if (!sd.tipline.empty())                                       // through the leaf's tips, as the renderer
                for (const typename B::DVec& v : sd.tipline[f.j]) pts.push_back(B::boundary(B::act(hd, v)));
            pts.push_back(b);
        }
    }
}

// the loop h0 . (alpha_{j_1} u s_{j_1} alpha_{j_2} u ...) of a relation w, as a closed polyline: its last point is not
// repeated (legacy loop_points)
template <class B>
inline std::vector<typename B::Point> loop_points(const Side<B>& sd, const Letters<B>& L, const typename B::Elem& h0,
                                                  const std::vector<int>& w, double acc, size_t& forced)
{
    std::vector<typename B::Point> pts;
    typename B::Elem g = h0;
    for (int j : w) { arc_points(sd, g, j, acc, pts, forced); g = B::mul(g, L.s[j]); }
    if (!B::same(g, h0)) { fprintf(stderr, "a relation does not close up\n"); exit(1); }
    if (pts.size() > 1) pts.pop_back();          // the last point is the first one again
    return pts;
}

}  // namespace zippers
