// SPDX-License-Identifier: GPL-3.0-or-later
//
// world.h: what a drawing program renders, for any backend B: the letters, the two zippers, the roots and the two
// renderers; prepare (read a substitution file) and build (render from the roots).
//
// prepare and build are templates over an example's RENDER POLICY P (examples/*_render.h), with P::B its backend.
// The policy supplies what differs between the examples, and the generic code chooses none of it:
//   typedef ... B;                                           the backend (geometry/backend.h)
//   static constexpr SubstitutionDialect dialect;            its substitution files (render/substitution.h)
//   static B::Elem word(const std::string& w);               a word of the file -> its element, as B::normal leaves it
//   static void sides(const Letters<B>& L, Side<B>& plus, Side<B>& minus);
//                                                            the example's choices for the two zippers, made after
//                                                            make_side: the points its leaves are drawn
//                                                            through at their tips (Side::tipline, render/tips.h) and
//                                                            the look-ahead through one-letter roofs (Side::through,
//                                                            set_lookthrough); it may print lines (stderr). A policy
//                                                            that wants neither leaves the Sides as they are
//   static void prepared(const char* subst, const Letters<B>& L);
//                                                            the line prepare prints at the end (stderr)
//   static void roots(double R, std::vector<B::Elem>& roots, std::vector<double>& rootd);
//                                                            the roots of build: the elements of the lattice at
//                                                            distance <= R from o, nearest first, and their distances
// Call them as prepare<P>(file, W) and build<P>(W, R, acc, cap, which).
//
// Moved from legacy/s3/zipcore.h (World, prepare, build) and legacy/s4/zipcore5.h (World5, prepare5, build5). The S^3
// and S^4 code are the same up to the arithmetic and the lines above; every expression and loop is the legacy one,
// in the same order. Changes, apart from the names:
//   - prepare takes the name of the file, as prepare5 did (the legacy S^3 prepare read it from the program's options);
//     build takes R, acc, cap and which, as build5 did;
//   - the inverse of a letter, and t^-1 for Z-, are B::normal(B::inv(.)): mnormal(minv(.)) in S^3, m6inv(.) in S^4;
//   - plus[j] = j <= inv[j] (S^4; S^3 had j < inv[j], which is the same when no letter is its own inverse:
//     render/renderer.h);
//   - build draws the roots by number and gives draw the number, so every polyline records its root (S^4; the legacy
//     S^3 build did not); World has the roots' distances (rootd) in S^3 too;
//   - the World does not delete its renderers (as in the legacy code: they live until the program ends);
//   - prepare calls the policy's hook P::sides after make_side, and build's line adds the points drawn at tips
//     and the splits of the look-ahead through one-letter roofs when there are any (render/renderer.h).
// The lines build prints are the same text in both legacy programs, and are here (without tips and without the
// look-ahead through one-letter roofs, the legacy lines).
//
// Two options of build, both OFF by default, and then nothing that build does or prints changes:
//   - CULLING (World::cull): the window of the view, pulled back to the sphere (render/cull.h; view/s3.h window3,
//     view/s4.h window5); build gives it to both renderers, which then skip the branches whose curves miss it
//     (render/renderer.h), and the line of each zipper adds ", N culled";
//   - THE FOCUS BALL (World::focus, with its element fh): the roots are the ball B(R) of P::roots and its translate
//     fh B(R), the elements fh g for g in B(R) in the same order (nearest to fh o first), after B(R) and
//     de-duplicated by key with it (add_focus_roots): an element of both balls is a root once, in B(R)'s place. So the
//     roots of B(R) are drawn first, exactly as without the focus. When fh is an element of the lattice, fh B(R) is
//     the ball of radius R about the orbit point fh o. The programs choose fh (examples/*: the focus centre) and print
//     what it is; World::nmain is the number of roots of B(R), and the roots after it come from the focus ball.

#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/hash.h"
#include "core/keyset.h"
#include "core/timing.h"
#include "render/cull.h"
#include "render/renderer.h"
#include "render/substitution.h"

namespace zippers {

// legacy World, World5
template <class B>
struct World {
    Letters<B> L;
    Side<B> plus, minus;
    std::vector<typename B::Elem> roots;                 // the roots of build, nearest first
    std::vector<double> rootd;                           // their distances d(o, g o)
    Renderer<B> *rp = nullptr, *rn = nullptr;            // Z+ and Z-, made by build
    // the two options of build (the top of the file), off by default:
    const CullWindow<B::N>* cull = nullptr;              // culling: the window of the view
    bool focus = false;                                  // the focus ball fh B(R)
    typename B::Elem fh = B::identity();
    size_t nmain = 0;                                    // set by build: the roots of B(R); the rest are fh B(R)'s
};

// the focus ball (the top of the file): append fh g to roots for each g of the first n roots, in their order, unless
// an element with the same key is a root already (the key of an element determines it: geometry/backend.h); returns
// the number added
template <class B>
inline size_t add_focus_roots(const typename B::Elem& fh, std::vector<typename B::Elem>& roots, std::vector<double>& rootd)
{
    size_t n = roots.size();
    KeySet have;
    for (size_t i = 0; i < n; i++) have.insert(fingerprint(B::key(roots[i]), 0));
    for (size_t i = 0; i < n; i++) {
        typename B::Elem g = B::normal(B::mul(fh, roots[i]));
        if (!have.insert(fingerprint(B::key(g), 0))) continue;
        roots.push_back(g);
        rootd.push_back(B::displacement(g));
    }
    return roots.size() - n;
}

// read the substitution file `subst` (P's dialect), set up the letters and the two zippers, checking every roof
// exactly; exits if the file cannot be read, if S is not closed under inversion, or if a roof fails
// (legacy prepare, prepare5)
template <class P>
inline void prepare(const char* subst, World<typename P::B>& W)
{
    typedef typename P::B B;
    typedef typename B::Elem Elem;
    Substitution D;
    if (!load_substitution(subst, D, P::dialect)) exit(1);
    size_t n = D.sword.size();
    for (auto& w : D.sword) W.L.s.push_back(P::word(w));
    W.L.inv.assign(n, -1);
    for (size_t j = 0; j < n; j++) {
        Elem x = B::normal(B::inv(W.L.s[j]));
        for (size_t k = 0; k < n; k++) if (B::eq(x, W.L.s[k])) W.L.inv[j] = (int)k;
        if (W.L.inv[j] < 0) { fprintf(stderr, "S is not closed under inversion\n"); exit(1); }
    }
    W.L.plus.assign(n, 0);
    W.L.canon.assign(n, 0);
    for (size_t j = 0; j < n; j++) {
        // an involution is its own inverse: it is its own representative
        W.L.plus[j] = (int)j <= W.L.inv[j];
        W.L.canon[j] = W.L.plus[j] ? (int)j : W.L.inv[j];
    }
    Elem t = P::word(D.tword);
    make_side(W.plus, "Z+", t, D.up, W.L);
    make_side(W.minus, "Z-", B::normal(B::inv(t)), D.down, W.L);
    P::sides(W.L, W.plus, W.minus);                      // the example's tips and look-ahead
    P::prepared(subst, W.L);
}

// render Z+ (which & 1) and Z- (which & 2) from the roots of radius R, at accuracy acc, with at most cap leaves each;
// prints a line for each zipper drawn, and the time (legacy build, build5)
template <class P>
inline void build(World<typename P::B>& W, double R, double acc, size_t cap, int which = 3)
{
    typedef typename P::B B;
    double t0 = now();
    P::roots(R, W.roots, W.rootd);
    W.nmain = W.roots.size();
    if (W.focus) add_focus_roots<B>(W.fh, W.roots, W.rootd);   // the focus ball (off by default)
    delete W.rp; delete W.rn;
    W.rp = new Renderer<B>(W.plus, W.L, acc, cap);
    W.rn = new Renderer<B>(W.minus, W.L, acc, cap);
    W.rp->cull = W.rn->cull = W.cull;                    // culling (off by default)
    for (size_t r = 0; r < W.roots.size(); r++)
        for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) {
            if (which & 1) W.rp->draw(W.roots[r], (int)j, (int)r);
            if (which & 2) W.rn->draw(W.roots[r], (int)j, (int)r);
        }
    for (Renderer<B>* r : {W.rp, W.rn}) {
        if (r->nVisited == 0) continue;
        char extra[200] = "";                            // only with tips, the look-ahead through one-letter roofs or
        int k = 0;                                       // culling
        if (r->nTipPoints) k += snprintf(extra + k, sizeof extra - (size_t)k, ", %zu points at tips", r->nTipPoints);
        if (r->nThrough)
            k += snprintf(extra + k, sizeof extra - (size_t)k, ", %zu split through one-letter roofs", r->nThrough);
        if (r->cull) {
            k += snprintf(extra + k, sizeof extra - (size_t)k, ", %zu culled", r->nCulled);
            if (r->nCullOpen)
                snprintf(extra + k, sizeof extra - (size_t)k, " (%zu left undecided by the growth guard)", r->nCullOpen);
        }
        fprintf(stderr, "%s: R=%.2f acc=%.4g: %zu roots, %zu branches, %zu skipped, %zu leaves, %zu polylines,"
                " %zu spikes, %zu forced, depth %d%s%s\n",
                r->sd.name, R, acc, W.roots.size(), r->nVisited, r->nSkipped, r->nLeaves, r->npoly(),
                r->nSpike, r->nForced, r->deepest, extra, r->capped ? "  [LEAF CAP REACHED: picture truncated]" : "");
    }
    fprintf(stderr, "built in %.2fs\n", now() - t0);
}

}  // namespace zippers
