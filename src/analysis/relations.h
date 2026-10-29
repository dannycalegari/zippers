// SPDX-License-Identifier: GPL-3.0-or-later
//
// relations.h: relations of the kernel K = ker phi in the letters S of a substitution, for any backend B.
//
// A RELATION is a word w = s_{j_1} ... s_{j_m} in the letters (render/renderer.h: Letters) whose product is the
// identity. Each gives a closed loop in Z+ and one in Z- (render/arcs.h: loop_points), and translates of them; the
// linking numbers of such loops are what link3 computes (analysis/linking.h). A word is a std::vector<int> of letter
// numbers, and L.inv[x] is the letter inverse to x.
//   free_reduce     cancel x x^-1, from the left
//   cyclic_reduce   free_reduce, then cancel the letters at the two ends that are inverse to each other
//   canonical       the least (lexicographically) of the rotations of w and of its inverse word: one representative
//                   per loop up to translation and reversal
//   find_relations  relations of length <= maxlen, found exactly by a breadth-first search of K with steps S inside
//                   the ball B(Rr) (d(o, g o) <= Rr): every edge outside the search tree closes a cycle; cyclically
//                   reduced, one per class of canonical; the first maxcount found, shortest first (a stable sort, so
//                   in the order found within a length). The search stops after the depth maxlen/2, or once maxcount
//                   relations are found, and stores at most 1,500,000 elements.
//
// Moved from legacy/s3/link3.cc (free_reduce, cyclic_reduce, canonical, find_relations), written over the backend: the
// elements are exact (B::Elem), the search runs on the element store Store<B, Label> (store/store.h), and Label is
// the label type of the example's store (the elements are labelled Label{}), so that the store holds what the
// example's stores hold: for Cartwright-Steger cs::FVal, as the legacy Store held f0 = f1 = 0. Every step, and the
// order of the search, is the legacy one (tests/unit/test_analysis_s3.cc). The changes beyond the names:
//   - the key of g s and its distance are B::product_key(g, s) and B::key_displacement(key), the element stored
//     B::normal(B::mul(g, s)): for PU21 what prodkey(g, s, key, col), hdist(col[2]) and mrotk(mmul(g, s), k) computed
//     (the same integers, the same double; examples/cartwright_steger.h's enumerate makes the same change), the root
//     B::normal(B::identity()) (legacy mnormal(mid()));
//   - the distance of an element not yet in the store is computed once, where the legacy code computed
//     hdist(col[2]) twice (for the test against Rr and for the store), with the same result.

#pragma once

#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>

#include "render/renderer.h"
#include "store/store.h"

namespace zippers {

// ---------------------------------------------------------------------------------------
// Relations of K in the letters S

template <class B>
inline std::vector<int> free_reduce(const std::vector<int>& w, const Letters<B>& L)
{
    std::vector<int> r;
    for (int x : w) { if (!r.empty() && r.back() == L.inv[x]) r.pop_back(); else r.push_back(x); }
    return r;
}

template <class B>
inline std::vector<int> cyclic_reduce(std::vector<int> w, const Letters<B>& L)
{
    w = free_reduce(w, L);
    size_t a = 0, b = w.size();
    while (b - a >= 2 && w[a] == L.inv[w[b-1]]) { a++; b--; }
    return std::vector<int>(w.begin() + a, w.begin() + b);
}

// the least of the rotations of w and of its inverse: one representative per loop up to translation
template <class B>
inline std::vector<int> canonical(const std::vector<int>& w, const Letters<B>& L)
{
    std::vector<int> wi;
    for (int i = (int)w.size() - 1; i >= 0; i--) wi.push_back(L.inv[w[i]]);
    std::vector<int> best = w;
    for (const std::vector<int>* v : {&w, (const std::vector<int>*)&wi})
        for (size_t r = 0; r < v->size(); r++) {
            std::vector<int> c(v->begin() + r, v->end());
            c.insert(c.end(), v->begin(), v->begin() + r);
            if (c < best) best = c;
        }
    return best;
}

// relations of length <= maxlen from a breadth-first search of K with steps S inside B(Rr);
// the elements are stored with the label Label{} (call it as find_relations<Label>(L, ...))
template <class Label, class B>
inline std::vector<std::vector<int>> find_relations(const Letters<B>& L, double Rr, size_t maxlen, size_t maxcount)
{
    typedef typename B::Elem Elem;
    Store<B, Label> K;
    K.add(B::normal(B::identity()), -1, -1, 0, Label{}, 0.f);
    std::set<std::vector<int>> seen;
    std::vector<std::vector<int>> rels;
    const size_t CAP = 1500000;
    for (size_t q = 0; q < K.size(); q++) {
        if ((size_t)K.depth[q] > maxlen/2) break;
        Elem g = K.load(q);
        for (size_t j = 0; j < L.s.size(); j++) {
            typename B::Key key = B::product_key(g, L.s[j]);
            int f = K.find(key);
            if (f < 0) {
                double d = B::key_displacement(key);
                if (d > Rr || K.size() >= CAP) continue;
                K.add(B::normal(B::mul(g, L.s[j])), q, j, K.depth[q] + 1, Label{}, (float)d);
                continue;
            }
            if ((size_t)(K.depth[q] + 1 + K.depth[f]) > maxlen) continue;
            std::vector<int> w = K.path(q);
            w.push_back(j);
            std::vector<int> pf = K.path(f);
            for (int i = (int)pf.size() - 1; i >= 0; i--) w.push_back(L.inv[pf[i]]);
            w = cyclic_reduce(w, L);
            if (w.empty()) continue;
            std::vector<int> c = canonical(w, L);
            if (seen.insert(c).second) rels.push_back(c);
        }
        if (rels.size() >= maxcount) break;
    }
    std::stable_sort(rels.begin(), rels.end(), [](const std::vector<int>& a, const std::vector<int>& b) { return a.size() < b.size(); });
    if (rels.size() > maxcount) rels.resize(maxcount);
    return rels;
}

}  // namespace zippers
