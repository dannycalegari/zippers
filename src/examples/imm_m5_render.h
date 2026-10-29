// SPDX-License-Identifier: GPL-3.0-or-later
//
// imm_m5_render.h: what IMM's M5 supplies to the renderer (render/world.h): its render policy
// zippers::imm::RenderPolicy.
//
//   B          zippers::ON1<5> (imm::O51)
//   dialect    chz5's substitution files: "j d=... word" for a letter, no "member" section
//   word       a word in the facets ("0 5 8 4", or "1") -> its element: word_matrix(parse_fword(w)) (imm_m5.h)
//   sides      the TIPS of each zipper (render/tips.h): the fixed points of the parabolic maps of the closed
//              paths of length <= 4 of its roofs, each verified exactly, through which a leaf of each letter that
//              has them is drawn (without them, leaves deep in a parabolic chain spike into the cusps:
//              render/tips.h); and a line for each zipper (stderr): how many tips, on how many letters. The
//              look-ahead through one-letter roofs is off (Side::through empty): no letter of the stored files has a
//              one-letter roof here
//   prepared   the line of the legacy prepare5: "FILE: |S| = n (k base arcs), roofs checked exactly; t^+ and t^- found",
//              with k the number of representatives of the inverse pairs (an involution is its own)
//   roots      the elements of Gamma in B(R), nearest first, and their distances: find_roots5 (imm_m5.h)
//
// From legacy/s4/zipcore5.h: the dialect of load_subst5, the message of prepare5, find_roots5. The tips are new:
// with them the picture is no longer the legacy Renderer5's (test_render_imm.cc uses a policy without them).

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "examples/imm_m5.h"
#include "geometry/on1/on1.h"
#include "render/renderer.h"
#include "render/substitution.h"
#include "render/tips.h"

namespace zippers {
namespace imm {

struct RenderPolicy {
    typedef O51 B;

    static constexpr SubstitutionDialect dialect = {1, false};

    static M6 word(const std::string& w) { return word_matrix(parse_fword(w)); }

    // the tips (render/tips.h), and one line for each zipper; no look-ahead through one-letter roofs
    static void sides(const Letters<O51>&, Side<O51>& plus, Side<O51>& minus)
    {
        for (Side<O51>* sd : {&plus, &minus}) {
            Tips<5> T = find_tips(*sd, 4);
            set_tips(*sd, T);
            fprintf(stderr, "%s: %zu tips on %zu letters (the cusps of the closed paths of its roofs, of length <= %d,"
                    " each verified exactly)", sd->name, T.count(), T.letters(), T.maxlen);
            if (T.skipped || T.overflow || T.atvertex)
                fprintf(stderr, "; not tested: %ld (the growth guard), %ld (128 bits); dropped at a vertex: %ld",
                        T.skipped, T.overflow, T.atvertex);
            fprintf(stderr, "\n");
        }
    }

    static void prepared(const char* subst, const Letters<O51>& L)
    {
        size_t n = L.s.size();
        size_t np = 0;
        for (size_t j = 0; j < n; j++) np += L.plus[j];
        fprintf(stderr, "%s: |S| = %zu (%zu base arcs), roofs checked exactly; t^+ and t^- found\n", subst, n, np);
    }

    static void roots(double R, std::vector<M6>& roots, std::vector<double>& rootd) { find_roots5(R, roots, rootd); }
};

}  // namespace imm
}  // namespace zippers
