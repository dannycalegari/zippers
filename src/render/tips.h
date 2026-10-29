// SPDX-License-Identifier: GPL-3.0-or-later
//
// tips.h: the TIPS of the letters of a zipper in O(n,1): the cusp points its arcs run into.
//
// THE SPIKES. A letter x lies on a cycle of the roofs when a closed path x -> x_1 -> ... -> x_m = x of the roof graph
// exists (x_{k+1} is the child at place i_{k+1} of x_k). Its MAP is g = off[x][i_1] off[x_1][i_2] ...
// off[x_{m-1}][i_m] (render/renderer.h: the bases of the children), and g alpha_x is the sub-arc of alpha_x at the
// ADDRESS i_1 ... i_m: the nested arcs g^k alpha_x shrink to the fixed point of g, the point of alpha_x at the address
// (i_1 ... i_m) repeated forever. When g is parabolic, the k-th arc has a chord of about 1/k^2 but still runs out to
// the fixed point p, about 1/k away, so a leaf deep in the chain is accepted on its chord while its curve reaches p: a
// spike into a cusp, whose length in units of the accuracy grows like ACC^(-1/2) (docs/developer_guide.md, "Tips").
// The renderer draws such a leaf (h, x) through h p, an exact point of the
// true curve (Renderer<B>::visit, arc_points), which changes no split and no leaf.
//
// THE TIPS of x are the fixed points of the parabolic maps of its closed paths of length <= maxlen (4 by default),
// each point once, at the smallest address found for it; listed in the order the arc passes them (the addresses, the
// paths repeated forever, compared lexicographically). A point is kept once because one cusp is met at many addresses:
// when g and g' are parabolic with the same fixed point p, so is every product of them, and the arc passes through p
// at every address made of their two paths (in the S^4 files, e.g. letter 28 of Z+ of _t1 meets e_2 through its
// paths (5) and (4 5)). A power of a path gives the same address.
//
// EXACT. The maps are exact products in ON1<n> (twice the matrix, entries in Z), with the renderer's growth guard: a
// path is not extended past a product with an entry of 2^40 or more (counted in `skipped`). A parabolic g has every
// eigenvalue of modulus 1, so |tr g| <= n + 1, and the closed paths whose map has a larger trace (nearly all of them:
// the long ones are strongly loxodromic) are passed over at the cost of the trace. For the others, with G = 2g and
// N = g - 1, the candidate is P = (G - 2)^2 e_{n+1} = 4 N^2 o, reduced to a primitive integer vector with its last
// entry positive; it is the fixed point when the exact checks hold:
//     P != 0,  G P = 2 P,  <P, P> = 0   (in 128-bit integers, every product checked for overflow).
// Then g is parabolic, with fixed point P, by this argument (an element of O+(n,1) is elliptic, parabolic or
// loxodromic):
//   - not loxodromic: a loxodromic element fixes only the two ends of its axis on the sphere, with eigenvalues e^l and
//     e^-l, l > 0, never 1;
//   - not the identity: N^2 != 0;
//   - not elliptic: an elliptic element is semisimple, so R^{n,1} = ker N + im N with the sum direct, and a nonzero
//     fixed vector is not in im N; but P = N (4 N o) is.
// The torsion-freeness of the group is not used, and it would not do: the letters of the S^4 files include 8
// involutions (tests/unit/test_reader_imm.cc), elliptic elements that fix null vectors with eigenvalue 1.
// A parabolic element whose rotational part is not 1 (a screw) has N^2 o off its fixed line; then the same is tried
// for g^2 (P from g^2, then G P = 2 P checked for g itself: g is not loxodromic as above, and not elliptic since g^2
// is not). In W every parabolic element has a rotational part of order <= 2 (the cusp groups are reflection groups
// of boxes), so g or g^2 is unipotent; a parabolic map that both tests missed would only be missing from the tips.
// The test does not depend on floating point: no path is called parabolic that is not.
//
// NEVER AN ENDPOINT. The ends V and s_x V of alpha_x are fixed points of loxodromic elements (conjugates of t), so no
// tip is one; this is checked in doubles anyway (a tip within 1e-9 of an end, or of a vertex of the first split, is
// dropped and counted in `atvertex`).
//
// WHAT IS DRAWN (tip_line). A leaf (h, x) is drawn from a = h V through h q for each point q of tip_line(x), to b =
// h s_x V. The points are the tips in arc order and, between two consecutive tips, the vertices of the split at the
// level where their addresses part that lie between them: for the addresses c ... and c' ... with c < c' (the tips are
// on the children c and c' of the first split) the vertices h wv[c+1], ..., h wv[c'] of the first split; for a
// common prefix of r places the same one level down, in the branch that the prefix reaches. So the polyline goes from
// one spike back to where the arc is, and out to the next. Before the first tip and after the last no vertex is added:
// the look-ahead keeps the vertices of the first split within ACC of the chord, the chord is at most ACC, and a
// spike rises from there, so a -> h p -> b passes within about a chord of the arc. With one tip the leaf is
// a -> h p -> b.
//
//   Tip<n>        one tip: its path and letters, its map, P (exact, and in doubles as vfix is stored)
//   Tips<n>       the tips of every letter of a side, and the counts per path length
//   find_tips     the tips of a Side<ON1<n>> (its roofs and offsets: Side::roof and Side::off, which make_side derives
//                 from the representatives' roofs; never the file's roofs of the other letters, which differ)
//   tip_line      the points drawn between the ends of a leaf of letter x (any backend)
//   set_tips      fill Side::tipline from the tips (the render policy's hook does this: render/world.h)

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "geometry/on1/on1.h"
#include "geometry/sphere.h"
#include "render/renderer.h"

namespace zippers {

template <int n>
struct Tip {
    typedef ON1<n> B;
    std::vector<int> path;                  // the closed path x -> x_1 -> ... -> x_m = x: its places (0-based)
    std::vector<int> letters;               // x_1, ..., x_m (the last is x)
    typename B::Elem g;                     // its map off[x][path[0]] off[x_1][path[1]] ..., twice the matrix
    std::array<int64_t, n + 1> P;           // the fixed point: primitive, last entry > 0, G P = 2 P, <P, P> = 0
    int via = 1;                            // P came from g (1) or from g^2 (2)
    typename B::DVec v;                     // P in doubles, of Euclidean norm 1 (as Side::vfix)
    int paths = 0;                          // the closed paths (of length <= maxlen) whose map fixes P
};

template <int n>
struct Tips {
    int maxlen = 0;
    std::vector<std::vector<Tip<n>>> of;    // the tips of each letter, in the order the arc passes them
    std::vector<long> closed, parabolic, fresh;   // per path length (index 1..maxlen): closed paths, with a parabolic
                                                  // map, and the tips first found at that length
    std::vector<long> via2;                 // per length: parabolic maps certified through g^2
    long skipped = 0;                       // paths not extended: a partial product had an entry of 2^40 or more
    long overflow = 0;                      // closed paths whose test overflowed 128 bits (not tips)
    long atvertex = 0;                      // tips dropped as within 1e-9 of an end or a vertex of the first split
    size_t count() const { size_t c = 0; for (const auto& t : of) c += t.size(); return c; }
    size_t letters() const { size_t c = 0; for (const auto& t : of) c += !t.empty(); return c; }
};

namespace tips_detail {

typedef __int128 I128;

inline I128 gcd(I128 a, I128 b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { I128 t = a % b; a = b; b = t; }
    return a;
}

// s += a*b, false on overflow
inline bool madd(I128& s, I128 a, I128 b)
{
    I128 t;
    return !__builtin_mul_overflow(a, b, &t) && !__builtin_add_overflow(s, t, &s);
}

// G P = 2 P, exactly (false on overflow)
template <int n>
bool fixes(const typename ON1<n>::Elem& G, const I128* P)
{
    for (int i = 0; i <= n; i++) {
        I128 s = 0;
        for (int j = 0; j <= n; j++) if (!madd(s, G.e[i][j], P[j])) return false;
        if (s != 2*P[i]) return false;
    }
    return true;
}

// the certificate of the map G (twice the matrix): 1 or 2 (P from g or from g^2) if g is parabolic, with P its fixed
// point (primitive, last entry > 0); 0 if not (or not decided: `ovf` set when 128 bits did not suffice)
template <int n>
int parabolic(const typename ON1<n>::Elem& G, std::array<int64_t, n + 1>& P, bool& ovf)
{
    typedef ON1<n> B;
    ovf = false;
    for (int k = 1; k <= 2; k++) {
        typename B::Elem Gk = G;
        if (k == 2) {
            if (B::max_coefficient(G) >= (1LL << 30)) { ovf = true; return 0; }   // B::mul would leave int64
            Gk = B::mul(G, G);
        }
        I128 c[n + 1], p[n + 1];
        for (int i = 0; i <= n; i++) c[i] = (I128)Gk.e[i][n] - (i == n ? 2 : 0);          // (G_k - 2) e_{n+1}
        bool bad = false;
        for (int i = 0; i <= n; i++) {                                                  // (G_k - 2)^2 e_{n+1}
            I128 s = 0;
            for (int j = 0; j <= n && !bad; j++) bad = !madd(s, (I128)Gk.e[i][j] - (i == j ? 2 : 0), c[j]);
            p[i] = s;
        }
        if (bad) { ovf = true; continue; }
        I128 d = 0;
        for (int i = 0; i <= n; i++) d = gcd(d, p[i]);
        if (d == 0) continue;                                                           // N^2 o = 0: not parabolic
        for (int i = 0; i <= n; i++) p[i] /= d;
        if (p[n] < 0) for (int i = 0; i <= n; i++) p[i] = -p[i];
        I128 q = 0;                                                                     // <P, P> = 0
        for (int i = 0; i <= n && !bad; i++) bad = !madd(q, p[i], i < n ? p[i] : -p[i]);
        if (bad) { ovf = true; continue; }
        if (q != 0) continue;
        bool big = false;
        for (int i = 0; i <= n; i++) big = big || p[i] > INT64_MAX || p[i] < -INT64_MAX;
        if (big) { ovf = true; continue; }
        if (!fixes<n>(Gk, p) || !fixes<n>(G, p)) continue;
        for (int i = 0; i <= n; i++) P[i] = (int64_t)p[i];
        return k;
    }
    return 0;
}

// the address u u u ... before the address w w w ... (both periodic, not equal)?
inline bool address_less(const std::vector<int>& u, const std::vector<int>& w)
{
    size_t L = u.size() + w.size();
    for (size_t k = 0; k < L; k++) {
        int a = u[k % u.size()], b = w[k % w.size()];
        if (a != b) return a < b;
    }
    return false;                           // equal (Fine and Wilf: agreeing on |u| + |w| places)
}

}  // namespace tips_detail

// the tips of every letter of the side sd: the fixed points of the parabolic maps of its closed paths of length <=
// maxlen, each verified exactly (see the top of the file)
template <int n>
Tips<n> find_tips(const Side<ON1<n>>& sd, int maxlen = 4)
{
    typedef ON1<n> B;
    typedef typename B::Elem Elem;
    using namespace tips_detail;
    const int nl = (int)sd.roof.size();
    Tips<n> T;
    T.maxlen = maxlen;
    T.of.assign(nl, {});
    T.closed.assign(maxlen + 1, 0); T.parabolic.assign(maxlen + 1, 0); T.fresh.assign(maxlen + 1, 0);
    T.via2.assign(maxlen + 1, 0);
    // back[x][y]: the fewest steps from y to x (to prune the paths that cannot close in time)
    const int unreached = 1 << 20;
    std::vector<std::vector<int>> parents(nl);
    for (int y = 0; y < nl; y++) for (int z : sd.roof[y]) parents[z].push_back(y);
    std::vector<std::vector<int>> back(nl, std::vector<int>(nl, unreached));
    for (int x = 0; x < nl; x++) {
        std::vector<int>& d = back[x];
        std::vector<int> q = {x};
        d[x] = 0;
        for (size_t k = 0; k < q.size(); k++)
            for (int y : parents[q[k]]) if (d[y] == unreached) { d[y] = d[q[k]] + 1; q.push_back(y); }
    }
    struct Found { std::vector<int> path, letters; Elem g; std::array<int64_t, n + 1> P; int via; };
    struct Walk {                                           // the paths from x that can still close in time
        const Side<B>& sd;
        const std::vector<int>& back;                       // back[y]: the fewest steps from y to x
        Tips<n>& T;
        int x, maxlen;
        std::vector<int> place, letter;                     // at depth k: the place taken and the letter reached
        std::vector<Elem> prod;                             // ... and the product of the offsets so far
        std::vector<Found> found;
        void step(int k)
        {
            if (k > 0 && B::max_coefficient(prod[k]) >= (1LL << 40)) { T.skipped++; return; }     // the growth guard
            int y = letter[k];
            const std::vector<int>& r = sd.roof[y];
            for (int i = 0; i < (int)r.size(); i++) {
                if (k + 1 + back[r[i]] > maxlen) continue;
                place[k + 1] = i;
                letter[k + 1] = r[i];
                const Elem& o = sd.off[y][i];
                if (k + 1 == maxlen) {                      // the last step closes the path: first only the trace
                    I128 tr = 0;
                    if (k == 0) for (int a = 0; a <= n; a++) tr += o.e[a][a];
                    else
                        for (int a = 0; a <= n; a++)
                            for (int c = 0; c <= n; c++) tr += (I128)prod[k].e[a][c]*o.e[c][a];
                    if (k > 0) tr /= 2;                     // the trace of twice the product
                    T.closed[k + 1]++;
                    if (tr > 2*(n + 1) || tr < -2*(n + 1)) continue;
                    prod[k + 1] = k == 0 ? o : B::mul(prod[k], o);
                    closed(k + 1);
                    continue;
                }
                prod[k + 1] = k == 0 ? o : B::mul(prod[k], o);
                if (r[i] == x) {
                    T.closed[k + 1]++;
                    I128 tr = 0;
                    for (int a = 0; a <= n; a++) tr += prod[k + 1].e[a][a];
                    if (tr <= 2*(n + 1) && tr >= -2*(n + 1)) closed(k + 1);
                }
                step(k + 1);
            }
        }
        void closed(int k)                                  // the closed path of length k, with |tr g| <= n + 1
        {
            if (B::max_coefficient(prod[k]) >= (1LL << 40)) { T.skipped++; return; }
            bool ovf = false;
            std::array<int64_t, n + 1> P;
            int via = parabolic<n>(prod[k], P, ovf);
            if (!via) { T.overflow += ovf; return; }
            T.parabolic[k]++;
            T.via2[k] += via == 2;
            Found f;
            f.path.assign(place.begin() + 1, place.begin() + 1 + k);
            f.letters.assign(letter.begin() + 1, letter.begin() + 1 + k);
            f.g = prod[k]; f.P = P; f.via = via;
            found.push_back(f);
        }
    };
    for (int x = 0; x < nl; x++) {
        Walk wk{sd, back[x], T, x, maxlen, std::vector<int>(maxlen + 1, -1), std::vector<int>(maxlen + 1, x),
                std::vector<Elem>(maxlen + 1, B::identity()), {}};
        wk.step(0);
        std::vector<Found>& found = wk.found;
        // each point once, at its smallest address; counted as new at the length of the shortest path that fixes it
        std::stable_sort(found.begin(), found.end(),
                         [](const Found& a, const Found& b) { return a.path.size() < b.path.size(); });
        std::vector<Tip<n>>& tips = T.of[x];
        std::vector<int> firstlen;
        for (const Found& f : found) {
            size_t t = 0;
            while (t < tips.size() && tips[t].P != f.P) t++;
            if (t == tips.size()) {
                Tip<n> tp;
                tp.path = f.path; tp.letters = f.letters; tp.g = f.g; tp.P = f.P; tp.via = f.via;
                tips.push_back(tp);
                firstlen.push_back((int)f.path.size());
            } else if (address_less(f.path, tips[t].path)) {
                tips[t].path = f.path; tips[t].letters = f.letters; tips[t].g = f.g; tips[t].via = f.via;
            }
            tips[t].paths++;
        }
        // in doubles, as vfix (Euclidean norm 1); never at an end or a vertex of the first split
        std::vector<Tip<n>> kept;
        typename B::Point V = B::boundary(sd.vfix), E = B::boundary(sd.ev[x]);
        for (size_t t = 0; t < tips.size(); t++) {
            Tip<n>& tp = tips[t];
            double nn = 0;
            for (int i = 0; i <= n; i++) nn += (double)tp.P[i]*(double)tp.P[i];
            nn = std::sqrt(nn);
            for (int i = 0; i <= n; i++) tp.v[i] = (double)tp.P[i]/nn;
            typename B::Point q = B::boundary(tp.v);
            bool near = chord(q, V) < 1e-9 || chord(q, E) < 1e-9;
            for (const typename B::DVec& w : sd.wv[x]) near = near || chord(q, B::boundary(w)) < 1e-9;
            if (near) { T.atvertex++; continue; }
            T.fresh[firstlen[t]]++;
            kept.push_back(tp);
        }
        std::sort(kept.begin(), kept.end(),
                  [](const Tip<n>& a, const Tip<n>& b) { return address_less(a.path, b.path); });
        tips = kept;
    }
    return T;
}

// the points drawn between the ends of a leaf of letter x, as null vectors (to be mapped by the leaf's element): the
// tips (addresses `paths`, in arc order, and their points `v`), and between two consecutive tips the vertices of the
// split at the level where their addresses part that lie between them (see the top of the file)
template <class B>
std::vector<typename B::DVec> tip_line(const Side<B>& sd, int x, const std::vector<std::vector<int>>& paths,
                                       const std::vector<typename B::DVec>& v)
{
    std::vector<typename B::DVec> line;
    for (size_t t = 0; t < paths.size(); t++) {
        if (t > 0) {
            const std::vector<int>& u = paths[t - 1];
            const std::vector<int>& w = paths[t];
            size_t r = 0;                                       // the common prefix of the two addresses
            while (r < u.size() + w.size() && u[r % u.size()] == w[r % w.size()]) r++;
            int z = x;
            typename B::Elem G = B::identity();
            for (size_t k = 0; k < r; k++) {
                int c = u[k % u.size()];
                G = k == 0 ? sd.off[z][c] : B::mul(G, sd.off[z][c]);
                z = sd.roof[z][c];
            }
            int c0 = u[r % u.size()], c1 = w[r % w.size()];      // c0 < c1: the children of (G, z) that hold the two
            for (int c = c0 + 1; c <= c1; c++)
                line.push_back(r == 0 ? sd.wv[x][c] : B::act(B::to_double(B::mul(G, sd.off[z][c])), sd.vfix));
        }
        line.push_back(v[t]);
    }
    return line;
}

// fill sd.tipline from the tips T (one entry per letter; empty for a letter without tips)
template <int n>
void set_tips(Side<ON1<n>>& sd, const Tips<n>& T)
{
    sd.tipline.assign(sd.roof.size(), {});
    for (size_t x = 0; x < sd.roof.size(); x++) {
        if (T.of[x].empty()) continue;
        std::vector<std::vector<int>> paths;
        std::vector<typename ON1<n>::DVec> v;
        for (const Tip<n>& tp : T.of[x]) { paths.push_back(tp.path); v.push_back(tp.v); }
        sd.tipline[x] = tip_line(sd, (int)x, paths, v);
    }
}

}  // namespace zippers
