// SPDX-License-Identifier: GPL-3.0-or-later
//
// cartwright_steger_render.h: what the Cartwright-Steger example supplies to the renderer (render/world.h): its
// render policy zippers::cs::RenderPolicy.
//
//   B          zippers::PU21
//   dialect    chz's substitution files: "j d=... f2=... word" for a letter, and a "member" section (skipped)
//   word       an a-word ("a1 A3 a2", or "1") -> its element, normalised: parse_word (cartwright_steger.h)
//   sides      the look-ahead through one-letter roofs on both zippers (set_lookthrough; the look-ahead (b) of
//              render/renderer.h; four letters of each side of the stored files have a one-letter roof); no tips, since Pi is cocompact and so has no parabolic element (render/tips.h). A
//              line on stderr only if a chain of one-letter roofs were cut (none can be: render/renderer.h)
//   prepared   the line of the legacy prepare: "FILE: |S| = n (n/2 pairs), roofs checked exactly; t^+ and t^- found"
//   roots      the elements of B(R), nearest first: roots_info(R), with the geometric generators geogens(4.5)
//              (cartwright_steger.h), as the legacy find_roots; and their distances
//
// From legacy/s3/zipcore.h: the dialect of load_subst, the message of prepare, find_roots. The roots' distances are
// RootInfo::d (the store's float d(o, g o)); the legacy find_roots dropped them.

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "examples/cartwright_steger.h"
#include "geometry/pu21/pu21.h"
#include "render/renderer.h"
#include "render/substitution.h"

namespace zippers {
namespace cs {

struct RenderPolicy {
    typedef PU21 B;

    static constexpr SubstitutionDialect dialect = {2, true};

    static M3 word(const std::string& w) { return parse_word(w); }

    // the look-ahead through one-letter roofs; no tips: Pi is cocompact, so no element is parabolic
    static void sides(const Letters<PU21>&, Side<PU21>& plus, Side<PU21>& minus)
    {
        for (Side<PU21>* sd : {&plus, &minus})
            if (set_lookthrough(*sd, true))
                fprintf(stderr, "%s: %zu chains of one-letter roofs cut at %d steps\n", sd->name, sd->throughcut,
                        THROUGH_LIMIT);
    }

    static void prepared(const char* subst, const Letters<PU21>& L)
    {
        size_t n = L.s.size();
        fprintf(stderr, "%s: |S| = %zu (%zu pairs), roofs checked exactly; t^+ and t^- found\n",
                subst, n, n/2);
    }

    static void roots(double R, std::vector<M3>& roots, std::vector<double>& rootd)
    {
        roots.clear(); rootd.clear();
        for (const RootInfo& x : roots_info(R)) { roots.push_back(x.g); rootd.push_back(x.d); }
    }
};

}  // namespace cs
}  // namespace zippers
