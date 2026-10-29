// SPDX-License-Identifier: GPL-3.0-or-later
//
// roof_choice.h: the choice of the roofs of a substitution, an option of the kernel search
// (search/kernel_search.h: --roofs), and the measures of a system of roofs that it improves, which apps/tools/roofs.cc
// and tests/unit/test_roof_choice.cc use too. Nothing here is used unless the option is given.
//
// THE ROOFS. A substitution (render/renderer.h) has a roof theta(j) for every letter s_j of S: a word x_1 ... x_k in S
// for t^-1 s_j t (Z+) or t s_j t^-1 (Z-). The renderer takes the roof of a letter j with j <= inv(j) from the file (the
// representative of its inverse pair; an involution letter, s_j = s_j^-1, is its own) and gives every other letter the
// INVERSE WORD of its representative's, x_k^-1 ... x_1^-1 (renderer_roofs below). So theta(x^-1) = theta(x)^-1, as
// words, for every letter that is not an involution.
//
// THE TWO ORIENTATIONS. The branch (h, x) is the arc h alpha_x, and (h s_x, x^-1) is the
// same pair of ends traced the other way; the renderer keys them alike. Splitting (h, x) once gives the pieces
// P_i = (h t x_1 ... x_{i-1}, x_i), i = 1..k. Split (h s_x, x^-1) by a roof y_1 ... y_k of x^-1: since
// h s_x t = h t x_1 ... x_k, its i-th piece is (h t x_1 ... x_k y_1 ... y_{i-1}, y_i), and the reversal of P_{k+1-i} is
// (h t x_1 ... x_{k+1-i}, x_{k+1-i}^-1). These agree for every i exactly when y_i = x_{k+1-i}^-1 for every i: when the
// roof of x^-1 is the inverse word of the roof of x. For an involution letter x = x^-1 that says: the roof of x is
// EQUAL LETTER BY LETTER TO ITS OWN INVERSE WORD (self_inverse below). Then the pieces of the reversal are the
// reversals of the pieces, in the opposite order, and the same holds one level down for every piece whose letter
// satisfies it again. So the refinements of (h, x) and (h s_x, x^-1) are reversals of each other AT EVERY LEVEL (and
// their limits, the arcs, are the same set) when every involution letter that the substitution reaches from x has a
// self-inverse roof; the non-involution letters satisfy it by the renderer's construction. If an involution z reached
// from x has a roof that is not self-inverse, the two trees part at the first level where a piece of letter z occurs:
// there the pieces of (g, z) and of its other orientation (g s_z, z) are different branches (in S^4's file _t1 the two
// arcs are 0.317 apart). The condition is on each side separately (Z+ uses the up
// roofs, Z- the down roofs), and for a letter it is on the involutions that the letter reaches; they are not all of
// them (in S^4's _t1, with the roofs the renderer uses, 76 of the 80 letters reach only the involutions 0, 2, 4, 6
// in Z+, and 77 only 0, 1, 3, 5, 7 in Z-), but every letter reaches
// some, so all 8 + 8 involution roofs self-inverse is what makes it hold for every letter. Equivalently: theta(w^-1) =
// theta(w)^-1 for every word w, so theta maps self-inverse words to self-inverse words,
// theta(u z u^-1) = theta(u) theta(z) theta(u)^-1.
//
// THE FACT. A word w = x_1 ... x_n that is equal letter by letter to its inverse word (x_i = x_{n+1-i}^-1) has odd
// length, unless it is freely trivial: for n = 2m, x_m = x_{m+1}^-1, so w = u u^-1 (u = x_1 ... x_m), whose product is
// 1, and a roof is a word for t^-1 s t != 1. For n = 2m + 1 the middle letter is its own inverse: an INVOLUTION LETTER
// z, and w = u z u^-1. So a self-inverse roof of an involution s_c is a conjugate u s_z u^-1 of an involution letter by
// an element u of K, written with any word for u: t^-1 s_c t (or t s_c t^-1) must be conjugate in K to an involution
// letter. (In S^4 it is, to s_c itself, for every involution letter of the files measured.)
//
// THE JUNCTIONS. A junction of a word is a pair (y, z) of consecutive letters. It
// CANCELS when the last letter of theta(y) is the inverse of the first letter of theta(z): theta of the word is not
// freely reduced there, and the arc goes out along a piece and back along the same piece (two consecutive leaves of
// the picture can then cover it: overdraw, which the keys cannot remove). count_junctions counts, over all letters s,
// the junctions of theta(s) and of theta^2(s), and those that cancel. turn_closure follows them to every level: the
// junctions of all theta^n(s), n >= 1, form a finite set T, the closure of those of the roofs under
// (y, z) -> (last letter of theta(y), first letter of theta(z)), and every theta^n(s) is freely reduced exactly when no
// junction of T cancels (the train-track property).
//
// THE CHOICE (choose_roofs). For every representative letter j of a side, the CANDIDATE WORDS for its target x_j:
//   - the word the search found (the default roof);
//   - MEET IN THE MIDDLE: every pair (u, v) of the K-store with u v = x_j (v found by its key), and the words w(u) w(v),
//     w(.) running over the shortest words of the K-store's breadth-first search (the paths of its DAG: every prefix
//     in the K-store, one step shorter at a time; dag_words); this finds words for targets outside the K-store too,
//     often shorter than the descent's;
//   - only freely reduced words, of length at most L_min + jexcess, L_min the shortest length found;
//   - for an INVOLUTION letter: the self-inverse words w(u) z w(u)^-1, for the elements u of the K-store with
//     u^-1 x_j u = s_z, z an involution letter, of length at most L_min + excess. If there is one, the candidates are
//     the self-inverse words only, of length at most L_si + jexcess (L_si the shortest of them); if not, the ordinary
//     candidates (and the report says so).
//   The candidates are sorted by length, then lexicographically, and the first `wordcap` are kept (the default word
//   too, if it qualifies).
// The roofs start as the default roofs (where they are candidates; else the first candidate), and a GREEDY PASS goes
// through the representatives in order: for each, every candidate is tried (with the inverse word for its inverse
// letter), and the one that makes the side's (cancelling junctions of theta, of theta^2, total length) least, compared
// in that order, is kept; a candidate replaces the current word only if it is strictly better. Passes are repeated
// until one changes nothing (at most maxpass). Everything is in a fixed order (the K-store's indices, the letters,
// std::set's order of words), so the same search always makes the same choice. The roofs of the non-representatives
// are the inverse words of their representatives', as the renderer would make them, so the file says what is drawn.
// Every chosen word is a word for its target by construction; the caller checks them all exactly again.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "store/store.h"

namespace zippers {

typedef std::vector<int> RoofWord;

// the inverse word x_k^-1 ... x_1^-1 of w (INV: the index of each letter's inverse)
inline RoofWord inverse_word(const RoofWord& w, const std::vector<int>& INV)
{
    RoofWord r;
    r.reserve(w.size());
    for (size_t i = w.size(); i-- > 0;) r.push_back(INV[w[i]]);
    return r;
}

// no two consecutive letters x, x^-1
inline bool freely_reduced(const RoofWord& w, const std::vector<int>& INV)
{
    for (size_t i = 0; i + 1 < w.size(); i++) if (w[i + 1] == INV[w[i]]) return false;
    return true;
}

// equal letter by letter to its inverse word, and of odd length (THE FACT above: an even one is freely trivial)
inline bool self_inverse(const RoofWord& w, const std::vector<int>& INV)
{
    if (w.size() % 2 == 0) return false;
    for (size_t i = 0; i < w.size(); i++) if (w[i] != INV[w[w.size() - 1 - i]]) return false;
    return true;
}

// the roofs of one side as the renderer uses them (render/renderer.h: make_side): a letter j with j <= INV[j] keeps
// roof[j], any other gets the inverse word of roof[INV[j]]
inline std::vector<RoofWord> renderer_roofs(const std::vector<RoofWord>& roof, const std::vector<int>& INV)
{
    std::vector<RoofWord> th(roof.size());
    for (size_t j = 0; j < roof.size(); j++)
        th[j] = (int)j <= INV[j] ? roof[j] : inverse_word(roof[INV[j]], INV);
    return th;
}

// THE JUNCTIONS (the top of the file), over all letters s of a side whose roofs (as the renderer uses them) are th
struct JunctionCount {
    long junctions1 = 0, cancel1 = 0;      // the junctions of theta(s), and those that cancel
    long cancel1_inv = 0;                  // of those, the ones where the cancelling letter is an involution
    long junctions2 = 0, cancel2 = 0;      // the junctions of theta^2(s), and those that cancel
    long inner = 0;                        // consecutive letters x, x^-1 inside a roof (none in a reduced roof)
    long length = 0;                       // the total length of the roofs
};

inline bool junction_cancels(const std::vector<RoofWord>& th, const std::vector<int>& INV, int y, int z)
{
    return th[y].back() == INV[th[z].front()];
}

inline JunctionCount count_junctions(const std::vector<RoofWord>& th, const std::vector<int>& INV)
{
    JunctionCount c;
    RoofWord w2;
    for (size_t s = 0; s < th.size(); s++) {
        const RoofWord& w = th[s];
        c.length += (long)w.size();
        for (size_t i = 0; i + 1 < w.size(); i++) {
            c.junctions1++;
            if (w[i + 1] == INV[w[i]]) c.inner++;
            if (junction_cancels(th, INV, w[i], w[i + 1])) {
                c.cancel1++;
                int a = th[w[i]].back();
                if (INV[a] == a) c.cancel1_inv++;
            }
        }
        w2.clear();
        for (int x : w) w2.insert(w2.end(), th[x].begin(), th[x].end());
        for (size_t i = 0; i + 1 < w2.size(); i++) {
            c.junctions2++;
            if (junction_cancels(th, INV, w2[i], w2[i + 1])) c.cancel2++;
        }
    }
    return c;
}

// THE TURNS OF ALL LEVELS (the top of the file): T, the junctions of every theta^n(s), n >= 1; how many of them
// cancel; and the first level n at which one does (so theta^(n+1)(s) is not freely reduced for some s), or 0 if none
// does (every theta^n(s) is freely reduced)
struct TurnClosure {
    size_t turns = 0, cancelling = 0;
    int first_level = 0;
};

inline TurnClosure turn_closure(const std::vector<RoofWord>& th, const std::vector<int>& INV)
{
    TurnClosure tc;
    std::set<std::pair<int, int>> seen;
    std::vector<std::pair<int, int>> level;
    for (const RoofWord& w : th)
        for (size_t i = 0; i + 1 < w.size(); i++)
            if (seen.insert({w[i], w[i + 1]}).second) level.push_back({w[i], w[i + 1]});
    for (int n = 1; !level.empty(); n++) {
        std::vector<std::pair<int, int>> next;
        for (const auto& yz : level) {
            if (junction_cancels(th, INV, yz.first, yz.second)) {
                tc.cancelling++;
                if (!tc.first_level) tc.first_level = n;
            }
            std::pair<int, int> im = {th[yz.first].back(), th[yz.second].front()};
            if (seen.insert(im).second) next.push_back(im);
        }
        level.swap(next);
    }
    tc.turns = seen.size();
    return tc;
}

// the Perron-Frobenius root of the substitution matrix of th (the growth rate of |theta^n(s)|), by power iteration
// from the vector of ones, normalised by its sum
inline double growth_rate(const std::vector<RoofWord>& th, int iterations = 1000)
{
    size_t n = th.size();
    std::vector<double> v(n, 1.0), v2(n);
    double lam = 0;
    for (int it = 0; it < iterations; it++) {
        double s0 = 0, s1 = 0;
        for (size_t j = 0; j < n; j++) {
            v2[j] = 0;
            for (int l : th[j]) v2[j] += v[l];
            s0 += v[j];
            s1 += v2[j];
        }
        lam = s1/s0;
        for (size_t j = 0; j < n; j++) v[j] = v2[j]/s1*(double)n;
    }
    return lam;
}

// the options of the choice (kernel_search's --excess, --jexcess)
struct RoofChoiceOptions {
    int excess = 2;          // a self-inverse roof may be this much longer than the shortest word found
    int jexcess = 0;         // the junction pass may take words this much longer than the shortest (self-inverse) one
    size_t wordcap = 256;    // the candidates kept per roof
    size_t dagcap = 64;      // the shortest words of one element of the K-store
    int maxpass = 20;        // the passes of the greedy pass
};

// what the choice did on one side
struct RoofChoiceReport {
    JunctionCount before, after;            // the default roofs and the chosen ones (as the renderer uses them)
    long candidates = 0;                    // candidate words, over the representatives
    int representatives = 0, shorter = 0;   // roofs chosen; of them, shorter than the search's
    int involutions = 0, selfinv = 0;       // involution letters, and those with a self-inverse roof
    std::map<int, int> excess_needed;       // the shortest self-inverse roof minus the shortest word, and how often
    std::vector<int> no_selfinv;            // the involution letters with no self-inverse candidate
    int passes = 0, changed = 0;            // the passes made, and the roofs that differ from the defaults at the end
};

// the shortest words of the K-store element idx (the paths of the breadth-first search's DAG: each step back goes to
// an element of the K-store one step nearer the root), at most cap of them, memoised; in a fixed order
template <class B, class Label, class Gen>
const std::vector<RoofWord>& dag_words(const Store<B, Label>& K, const std::vector<Gen>& S, const std::vector<int>& INV,
                                       int idx, size_t cap, std::unordered_map<int, std::vector<RoofWord>>& memo)
{
    auto it = memo.find(idx);
    if (it != memo.end()) return it->second;
    std::vector<RoofWord> out;
    if (K.parent[idx] < 0) out.push_back({});
    else {
        typename B::Elem x = K.load(idx);
        for (size_t j = 0; j < S.size() && out.size() < cap; j++) {
            int p = K.find(B::product_key(x, S[INV[j]].m));          // x s_j^-1
            if (p < 0 || K.depth[p] != K.depth[idx] - 1) continue;
            const std::vector<RoofWord>& pre = dag_words(K, S, INV, p, cap, memo);
            for (const RoofWord& w : pre) {
                if (out.size() >= cap) break;
                RoofWord v = w;
                v.push_back((int)j);
                out.push_back(v);
            }
        }
    }
    return memo.emplace(idx, out).first->second;
}

// THE CHOICE (the top of the file) for one side: the targets x_j and the default roofs (all found) in roof; on return
// roof holds the chosen roofs of all letters (the non-representatives' as inverse words)
template <class B, class Label, class Gen>
void choose_roofs(const Store<B, Label>& K, const std::vector<Gen>& S, const std::vector<int>& INV,
                  const std::vector<typename B::Elem>& target, std::vector<RoofWord>& roof,
                  const RoofChoiceOptions& opt, RoofChoiceReport& rep)
{
    typedef typename B::Elem Elem;
    size_t n = S.size();
    std::vector<RoofWord> def = renderer_roofs(roof, INV);
    rep = RoofChoiceReport();
    rep.before = count_junctions(def, INV);
    std::vector<int> invols;
    for (size_t j = 0; j < n; j++) if (INV[j] == (int)j) invols.push_back((int)j);
    rep.involutions = (int)invols.size();

    // the representatives, and the bounds of the sweep: the default word's length (an upper bound for L_min)
    std::vector<int> reps;
    for (size_t j = 0; j < n; j++) if ((int)j <= INV[j]) reps.push_back((int)j);
    int maxlen = 0;
    for (int j : reps) maxlen = std::max(maxlen, (int)def[j].size() + std::max(opt.excess, opt.jexcess));

    // one sweep of the K-store (in its order): the pairs (u, v) with u v = x_j, and the self-inverse conjugators
    std::vector<std::vector<std::pair<int, int>>> pairs(n);            // (index of u, index of v)
    std::vector<std::vector<std::pair<int, int>>> conj(n);             // (index of u, the involution letter z)
    std::vector<typename B::Key> zkey(invols.size());
    for (size_t q = 0; q < K.size(); q++) {
        int dq = K.depth[q];
        if (dq > maxlen) break;
        Elem u = K.load(q), ui = B::inv(u);
        for (size_t a = 0; a < invols.size(); a++) zkey[a] = B::product_key(u, S[invols[a]].m);   // u s_z
        for (int j : reps) {
            int L0 = (int)def[j].size();
            if (dq <= L0 + opt.jexcess) {
                int v = K.find(B::product_key(ui, target[j]));
                if (v >= 0 && dq + K.depth[v] <= L0 + opt.jexcess) pairs[j].push_back({(int)q, v});
            }
            if (INV[j] == j && 2*dq + 1 <= L0 + opt.excess) {
                typename B::Key xk = B::product_key(target[j], u);                                 // x u = u s_z ?
                for (size_t a = 0; a < invols.size(); a++)
                    if (xk == zkey[a]) conj[j].push_back({(int)q, invols[a]});
            }
        }
    }

    // the candidates of each representative
    std::unordered_map<int, std::vector<RoofWord>> memo;
    std::vector<std::vector<RoofWord>> cand(n);
    for (int j : reps) {
        int L0 = (int)def[j].size(), Lmin = L0;
        for (const auto& pr : pairs[j]) Lmin = std::min(Lmin, K.depth[pr.first] + K.depth[pr.second]);
        std::set<RoofWord> words;
        if (L0 <= Lmin + opt.jexcess) words.insert(def[j]);
        for (const auto& pr : pairs[j]) {
            if (K.depth[pr.first] + K.depth[pr.second] > Lmin + opt.jexcess) continue;
            const std::vector<RoofWord>& a = dag_words(K, S, INV, pr.first, opt.dagcap, memo);
            const std::vector<RoofWord>& b = dag_words(K, S, INV, pr.second, opt.dagcap, memo);
            for (const RoofWord& wa : a)
                for (const RoofWord& wb : b) {
                    RoofWord w = wa;
                    w.insert(w.end(), wb.begin(), wb.end());
                    if (!w.empty() && freely_reduced(w, INV)) words.insert(w);
                }
        }
        // (if every shortest word met is not freely reduced, and the default is longer than allowed, the default:
        // it is always a word for x_j; this did not happen in any search tried)
        if (words.empty()) words.insert(def[j]);
        if (INV[j] == j) {
            std::set<RoofWord> si;
            int Lsi = 1 << 30;
            for (const auto& cz : conj[j]) {
                if (2*K.depth[cz.first] + 1 > Lmin + opt.excess) continue;
                for (const RoofWord& wu : dag_words(K, S, INV, cz.first, opt.dagcap, memo)) {
                    RoofWord w = wu;
                    w.push_back(cz.second);
                    RoofWord wi = inverse_word(wu, INV);
                    w.insert(w.end(), wi.begin(), wi.end());
                    if (freely_reduced(w, INV) && self_inverse(w, INV)) { si.insert(w); Lsi = std::min(Lsi, (int)w.size()); }
                }
            }
            for (auto it = si.begin(); it != si.end();) { if ((int)it->size() > Lsi + opt.jexcess) it = si.erase(it); else ++it; }
            if (!si.empty()) {
                rep.selfinv++;
                rep.excess_needed[Lsi - Lmin]++;
                words = si;
            } else rep.no_selfinv.push_back(j);
        }
        std::vector<RoofWord> ws(words.begin(), words.end());
        std::stable_sort(ws.begin(), ws.end(), [](const RoofWord& x, const RoofWord& y) { return x.size() < y.size(); });
        bool has_def = std::find(ws.begin(), ws.end(), def[j]) != ws.end();
        if (ws.size() > opt.wordcap) {
            ws.resize(opt.wordcap);
            if (has_def && std::find(ws.begin(), ws.end(), def[j]) == ws.end()) ws.push_back(def[j]);
        }
        rep.candidates += (long)ws.size();
        cand[j] = ws;
    }

    // the start, and the greedy pass
    std::vector<RoofWord> th = def;
    auto set_roof = [&](int j, const RoofWord& w) {
        th[j] = w;
        if (INV[j] != j) th[INV[j]] = inverse_word(w, INV);
    };
    for (int j : reps)
        if (std::find(cand[j].begin(), cand[j].end(), def[j]) == cand[j].end()) set_roof(j, cand[j][0]);
    auto better = [](const JunctionCount& a, const JunctionCount& b) {
        if (a.cancel1 != b.cancel1) return a.cancel1 < b.cancel1;
        if (a.cancel2 != b.cancel2) return a.cancel2 < b.cancel2;
        return a.length < b.length;
    };
    rep.passes = 0;
    for (int pass = 0; pass < opt.maxpass; pass++) {
        rep.passes++;
        bool changed = false;
        for (int j : reps) {
            RoofWord cur = th[j], best = cur;
            JunctionCount bc = count_junctions(th, INV);
            for (const RoofWord& w : cand[j]) {
                if (w == cur) continue;
                set_roof(j, w);
                JunctionCount c = count_junctions(th, INV);
                if (better(c, bc)) { bc = c; best = w; }
            }
            set_roof(j, best);
            if (best != cur) changed = true;
        }
        if (!changed) break;
    }
    rep.after = count_junctions(th, INV);
    rep.representatives = (int)reps.size();
    for (int j : reps) {
        if (th[j].size() < def[j].size()) rep.shorter++;
        if (th[j] != def[j]) rep.changed++;
    }
    roof = th;
}

}  // namespace zippers
