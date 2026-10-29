// SPDX-License-Identifier: GPL-3.0-or-later
//
// overdraw.h: the parts of the overdraw measure (apps/tools/overdraw.cc) that tests/unit/test_overlap.cc
// also uses.
//
//   walk_path        the fine pieces of a branch, as arc_points's rule finds them at an accuracy (render/arcs.h), each
//                    distinct branch once, with the path to each: the exact elements, letters, places and keys of its
//                    ancestors (the walk of leafcopy.h, which strays uses, with the path)
//   GenericPoints    for each letter x, points of alpha_x that are not vertices of its tree: the attracting fixed points
//                    of the maps of closed paths of the roof graph through x (as in the
//                    measurement made for the S^2 program)
//   OnCurve          does a point P lie on the curve of a branch? Decided by adaptive refinement, in doubles, down to a
//                    tolerance (below)
//   CandidateGrid    the leaves of a drawing, by cells of R^N: the leaves whose curves may pass near a point
//
// ON THE CURVE. The curve of a branch (M, y) (M its matrix, in doubles) is the union of the curves of its children
// (M off[y][i], roof[y][i]). A branch is described by its POINTS: its ends a = M V and b = M s_y V, the vertices of its
// first split M wv[y][i], and its tips (Side::tipline, render/tips.h), all of them points of its curve; its SIZE s is
// the largest distance of a vertex v_i from the chord [a, b], or the chord if larger (what the renderer accepts a leaf
// on; the tips are not counted, as the renderer does not count them), and its full size the same with the tips; its
// DISTANCE d from P is the distance of P from the polyline a, v_1, ..., v_{k-1}, b, or from the polyline through its
// tips if nearer (the leaf as drawn). A branch is kept while d <= prune s + tol, and split; P is on the curve when a
// kept branch has a full size <= tol (then P is within (prune + 1) tol of one of its points). A branch whose roof is one letter is the same arc as its only child, and is replaced by it. The
// prune factor is a bet, not a bound: the renderer accepts a branch whose points lie within its size of its chord, and
// the curves of such branches were measured to stray from the drawing by at most 2.7 times the size
// (apps/tools/strays measures it); there are no enclosures. overdraw.cc shows the stability of
// the answers under the prune factor, the tolerance and the choice of the generic point. A decision that meets the
// depth limit or the budget of branches is UNDECIDED, and counted.

#pragma once

#include "core/hash.h"
#include "geometry/backend.h"
#include "geometry/sphere.h"
#include "render/renderer.h"

#include "leafcopy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zippers {
namespace tools {

// ---------------------------------------------------------------------------------------------------------------------
// Doubles: the products and norms of the backends' DMat and DVec (std::array of double, or of complex<double>)

inline double sq(double v) { return v*v; }
inline double sq(const std::complex<double>& v) { return std::norm(v); }
inline double conjmul(double a, double b) { return a*b; }
inline std::complex<double> conjmul(const std::complex<double>& a, const std::complex<double>& b) { return std::conj(a)*b; }

template <class T, size_t K>
std::array<std::array<T, K>, K> dmul(const std::array<std::array<T, K>, K>& a, const std::array<std::array<T, K>, K>& b)
{
    std::array<std::array<T, K>, K> c;
    for (size_t i = 0; i < K; i++)
        for (size_t j = 0; j < K; j++) {
            T s = T(0);
            for (size_t k = 0; k < K; k++) s += a[i][k]*b[k][j];
            c[i][j] = s;
        }
    return c;
}

template <class T, size_t K>
double vnorm(const std::array<T, K>& v)
{
    double s = 0;
    for (const T& x : v) s += sq(x);
    return std::sqrt(s);
}

// v is an eigenvector of m? The eigenvalue's modulus, and the relative residual |m v - lambda v| / |m v|
template <class T, size_t K>
std::pair<double, double> eigen_check(const std::array<std::array<T, K>, K>& m, const std::array<T, K>& v)
{
    std::array<T, K> w;
    for (size_t i = 0; i < K; i++) { T s = T(0); for (size_t k = 0; k < K; k++) s += m[i][k]*v[k]; w[i] = s; }
    T num = T(0), den = T(0);
    for (size_t i = 0; i < K; i++) { num += conjmul(v[i], w[i]); den += conjmul(v[i], v[i]); }
    T lam = num/den;
    double r = 0;
    for (size_t i = 0; i < K; i++) r += sq(w[i] - lam*v[i]);
    return {std::abs(lam)/1.0, std::sqrt(r)/std::max(1e-300, vnorm(w))};
}

// ---------------------------------------------------------------------------------------------------------------------
// The fine pieces of a branch, with their paths

template <class B>
struct PathNode {
    typename B::Elem h;
    int j, place;                       // its letter; its place in its parent's roof (0-based; -1 for the top)
    uint64_t key;                       // the renderer's key (branch_key)
};

// the key of an ORIENTED branch: its element and its own letter. Two branches with the same oriented key are the same
// branch, so the same arc. The renderer's key (branch_key) also identifies (h, x) with (h s_x, x^-1), its reversal by
// the construction of the roofs (render/renderer.h); that is exact when no letter is an involution (S^3), and not in
// S^4: an involution letter c has one roof for both orientations, so below an occurrence of c the two trees differ,
// and (h, x) and (h s_x, x^-1) are different arcs with the same ends (docs/developer_guide.md)
template <class B>
uint64_t oriented_key(const typename B::Elem& h, int j) { return fingerprint(B::key(h), j + 100000); }

// is the renderer's key exact for these letters (no letter an involution)?
template <class B>
bool keys_exact(const Letters<B>& L)
{
    for (size_t j = 0; j < L.s.size(); j++) if (L.inv[j] == (int)j) return false;
    return true;
}

// the fine pieces of the branch (h0, j0) at depth d0, by arc_points's splitting rule at accuracy acc (the same tests,
// in the same order, as walk in leafcopy.h), each distinct branch once (by its key, in done, which the caller clears:
// the renderer's key, or with `oriented` the oriented key): piece(path, a, b, forced) with path[0] the branch (h0, j0)
// and path.back() the piece. The path's keys are the renderer's keys in either case.
template <class B, class F>
void walk_path(const Side<B>& sd, const Letters<B>& L, const typename B::Elem& h0, int j0, int d0, double acc,
               StampSet& done, F&& piece, bool oriented = false)
{
    struct Fr { typename B::Elem h; int j, depth, place; };
    std::vector<Fr> st;
    std::vector<PathNode<B>> path;
    st.push_back({h0, j0, d0, -1});
    while (!st.empty()) {
        Fr f = st.back();
        st.pop_back();
        uint64_t k = branch_key<B>(L, f.h, f.j);
        if (!done.insert(oriented ? oriented_key<B>(f.h, f.j) : k)) continue;
        path.resize((size_t)(f.depth - d0));
        path.push_back({f.h, f.j, f.place, k});
        typename B::DMat hd = B::to_double(f.h);
        typename B::Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, a, b, acc)) split = true;          // (b), as arc_points
        if (split && f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40)) {
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1, i});
        } else piece(path, a, b, split);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The leaves of a picture, and their refinements

// a leaf of a picture: its element and letter, its depth, its root and its key (the frame's, as the renderer keys it)
template <class B>
struct LeafRec {
    typename B::Elem h;
    int j, depth, root;
    uint64_t key;
};

// the refinement of a leaf as a tree (for the classes): every branch visited, with its parent

template <class B>
struct Tree {
    std::vector<PathNode<B>> node;
    std::vector<int> parent;
    std::unordered_map<uint64_t, int> index;
};

// the refinement of the branch (h0, j0) at depth d0 at accuracy acc (walk_path's rule and de-duplication), every
// branch visited, internal or fine
template <class B>
void tree_of(const Side<B>& sd, const Letters<B>& L, const typename B::Elem& h0, int j0, int d0, double acc, Tree<B>& T)
{
    T.node.clear(); T.parent.clear(); T.index.clear();
    struct Fr { typename B::Elem h; int j, depth, place, parent; };
    std::vector<Fr> st;
    st.push_back({h0, j0, d0, -1, -1});
    while (!st.empty()) {
        Fr f = st.back();
        st.pop_back();
        uint64_t k = branch_key<B>(L, f.h, f.j);
        if (T.index.count(k)) continue;
        int me = (int)T.node.size();
        T.index[k] = me;
        T.node.push_back({f.h, f.j, f.place, k});
        T.parent.push_back(f.parent);
        typename B::DMat hd = B::to_double(f.h);
        typename B::Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        const std::vector<typename B::DVec>& wv = sd.wv[f.j];
        bool split = chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            through_split(sd, f.h, f.j, a, b, acc)) split = true;
        if (split && f.depth < 400 && B::max_coefficient(f.h) < (1LL << 40))
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({B::mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1, i, me});
    }
}

// the ends of a branch
template <class B>
std::pair<typename B::Point, typename B::Point> ends(const Side<B>& sd, const typename B::Elem& h, int j)
{
    typename B::DMat hd = B::to_double(h);
    return {B::boundary(B::act(hd, sd.vfix)), B::boundary(B::act(hd, sd.ev[j]))};
}

// the standard error of a fraction red/tot from 8 blocks of the sample: the spread of the blocks' fractions, over
// sqrt(8 - 1) (the blocks are independent samples, as the sampling is by a hash)
inline double block_stderror(const double* tot, const double* red)
{
    double T = 0, Rd = 0;
    for (int b = 0; b < 8; b++) { T += tot[b]; Rd += red[b]; }
    if (T <= 0) return 0;
    double f = Rd/T, v = 0;
    for (int b = 0; b < 8; b++) {
        double fb = tot[b] > 0 ? red[b]/tot[b] : f;
        v += (fb - f)*(fb - f)*(tot[b]/T);
    }
    return std::sqrt(v/7.0);
}

// THE MEASURE BY KEYS: every leaf refined at facc (walk_path, each distinct branch of a leaf once); the fine pieces
// whose key's hash (salted) is 0 mod 2^keyshift are sampled in every leaf they occur in; a key in m leaves adds
// (m - 1)/m of its sampled ink to the ink drawn twice
struct KeyMeasure {
    double ink = 0, sampled = 0, redundant = 0;
    size_t nfine = 0, nforced = 0, samples = 0, keys = 0, shared = 0, maxm = 0;
    double bs[8] = {0}, br[8] = {0};                    // by 8 blocks of the keys (the hash's top bits)
    double fraction() const { return sampled > 0 ? redundant/sampled : 0.0; }
    double stderror() const { return block_stderror(bs, br); }
};
constexpr uint64_t SALT_KEY = 0x5bd1e9955bd1e995ULL;

template <class B, class Tick>
KeyMeasure key_measure(const Side<B>& sd, const Letters<B>& L, const std::vector<LeafRec<B>>& leaves, double facc,
                       int keyshift, Tick&& tick, uint64_t salt = 0)
{
    struct Samp { uint64_t key; uint32_t leaf; float c; };
    const uint64_t keymask = (1ULL << keyshift) - 1;
    KeyMeasure km;
    std::vector<Samp> samp;
    StampSet df;
    for (size_t li = 0; li < leaves.size(); li++) {
        df.clear();
        const LeafRec<B>& A = leaves[li];
        walk_path<B>(sd, L, A.h, A.j, A.depth, facc, df, [&](const std::vector<PathNode<B>>& path,
                                                             const typename B::Point& a, const typename B::Point& b,
                                                             bool forced) {
            double c = chord(a, b);
            km.ink += c; km.nfine++; km.nforced += forced;
            uint64_t k = path.back().key;
            if ((splitmix(k ^ SALT_KEY ^ salt) & keymask) == 0) samp.push_back({k, (uint32_t)li, (float)c});
        });
        tick(li);
    }
    std::sort(samp.begin(), samp.end(), [](const Samp& u, const Samp& v) {
        return u.key < v.key || (u.key == v.key && u.leaf < v.leaf); });
    km.samples = samp.size();
    for (size_t i = 0; i < samp.size(); ) {
        size_t j = i;
        double sum = 0;
        while (j < samp.size() && samp[j].key == samp[i].key) sum += samp[j++].c;
        size_t m = j - i;
        int blk = (int)(splitmix(samp[i].key ^ 0x51ed270b27aceULL) >> 61);
        km.sampled += sum; km.keys++;
        km.bs[blk] += sum;
        if (m >= 2) {
            km.shared++; km.redundant += sum*(double)(m - 1)/(double)m; km.maxm = std::max(km.maxm, m);
            km.br[blk] += sum*(double)(m - 1)/(double)m;
        }
        i = j;
    }
    return km;
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic points

template <class B>
struct GenericPoints {
    // for each letter, its generic points (null vectors, in the frame of alpha_x) and, for each, its closed path:
    // the places (0-based) and letters x = x_0 -> x_1 -> ... -> x_m = x
    std::vector<std::vector<typename B::DVec>> p;
    std::vector<std::vector<std::vector<std::pair<int, int>>>> path;
    std::vector<std::vector<int>> prefix;               // the steps of the path before its closed part (0: a closed
                                                        // path through x; k: the point of the closed path of the letter
                                                        // reached after k steps, mapped back by their offsets)
    std::vector<std::vector<double>> lambda;            // the modulus of the dominant eigenvalue of the closed part
    size_t missing = 0;                                 // letters with fewer than `want` points
};

// the points: closed paths of length <= maxlen, shortest first (breadth first over the places), that take an inner
// child somewhere, or both a first and a last child (so that the address is not eventually first children or
// eventually last children: not a vertex of the tree); whose map passes the growth guard; and whose map is loxodromic,
// checked in doubles (the dominant eigenvalue's modulus above 1.01, the residual of the eigenvector below 1e-9).
// Each point also at least 0.01 of the chord of alpha_x from its ends and 1e-4 of it from the vertices of its first
// split, so that the points spread over the piece. The first `want` found for each letter. A letter that no closed
// path returns to (some letters with a one-letter roof, those no roof contains) takes the points of its children: the
// point p of the child (off[x][i], y) gives off[x][i] p, a point of alpha_x (inner children first), again at least
// 0.01 of the chord from the ends; repeated until no letter gains one.
template <class B>
GenericPoints<B> generic_points(const Side<B>& sd, int want = 8, int maxlen = 6)
{
    typedef typename B::Elem Elem;
    size_t n = sd.roof.size();
    GenericPoints<B> G;
    G.p.assign(n, {}); G.path.assign(n, {}); G.lambda.assign(n, {}); G.prefix.assign(n, {});
    struct Path { int y; std::vector<std::pair<int, int>> pl; Elem g; };
    for (size_t x = 0; x < n; x++) {
        typename B::Point A = sd.V, Bx = B::boundary(sd.ev[x]);
        double len = chord(A, Bx);
        std::vector<Path> layer = {{(int)x, {}, B::identity()}};
        for (int depth = 1; depth <= maxlen && (int)G.p[x].size() < want && !layer.empty(); depth++) {
            std::vector<Path> next;
            for (const Path& q : layer) {
                if ((int)G.p[x].size() >= want) break;
                if (B::max_coefficient(q.g) >= (1LL << 40)) continue;
                const std::vector<int>& r = sd.roof[q.y];
                for (size_t i = 0; i < r.size() && (int)G.p[x].size() < want; i++) {
                    Path e{r[i], q.pl, B::mul(q.g, sd.off[q.y][i])};
                    e.pl.push_back({(int)i, r[i]});
                    if (r[i] == (int)x) {                   // a closed path: test it
                        bool inner = false, first = false, last = false;
                        int u = (int)x;
                        for (const auto& c : e.pl) {
                            int k = (int)sd.roof[u].size();
                            if (c.first == 0) first = true;
                            else if (c.first == k - 1) last = true;
                            else inner = true;
                            u = c.second;
                        }
                        if (inner || (first && last)) {
                            typename B::DVec v = B::attracting(e.g);
                            std::pair<double, double> ev = eigen_check(B::to_double(e.g), v);
                            bool ok = ev.first > 1.01 && ev.second < 1e-9;
                            if (ok) {
                                typename B::Point P = B::boundary(v);
                                if (chord(P, A) < 0.01*len || chord(P, Bx) < 0.01*len) ok = false;
                                for (const typename B::DVec& w : sd.wv[x]) if (chord(P, B::boundary(w)) < 1e-4*len) ok = false;
                            }
                            if (ok) {
                                G.p[x].push_back(v);
                                G.path[x].push_back(e.pl);
                                G.lambda[x].push_back(ev.first);
                                G.prefix[x].push_back(0);
                            }
                        }
                    }
                    if (depth < maxlen) next.push_back(std::move(e));
                }
            }
            layer.swap(next);
            if (layer.size() > 200000) layer.resize(200000);   // (a bound on the work; the stored files stay far below)
        }
    }
    // the letters without a closed path: the points of their children
    for (bool gained = true; gained; ) {
        gained = false;
        for (size_t x = 0; x < n; x++) {
            if (!G.p[x].empty()) continue;
            const std::vector<int>& r = sd.roof[x];
            typename B::Point A = sd.V, Bx = B::boundary(sd.ev[x]);
            double len = chord(A, Bx);
            std::vector<size_t> order;                  // inner children first
            for (size_t i = 1; i + 1 < r.size(); i++) order.push_back(i);
            order.push_back(0);
            if (r.size() > 1) order.push_back(r.size() - 1);
            for (size_t i : order) {
                int y = r[i];
                if (y == (int)x) continue;
                typename B::DMat od = B::to_double(sd.off[x][i]);
                for (size_t q = 0; q < G.p[y].size() && (int)G.p[x].size() < want; q++) {
                    typename B::DVec v = B::act(od, G.p[y][q]);
                    typename B::Point P = B::boundary(v);
                    if (chord(P, A) < 0.01*len || chord(P, Bx) < 0.01*len) continue;
                    std::vector<std::pair<int, int>> pl = {{(int)i, y}};
                    pl.insert(pl.end(), G.path[y][q].begin(), G.path[y][q].end());
                    G.p[x].push_back(v);
                    G.path[x].push_back(pl);
                    G.lambda[x].push_back(G.lambda[y][q]);
                    G.prefix[x].push_back(G.prefix[y][q] + 1);
                }
            }
            gained = gained || !G.p[x].empty();
        }
    }
    for (size_t x = 0; x < n; x++) if ((int)G.p[x].size() < want) G.missing++;
    return G;
}

// ---------------------------------------------------------------------------------------------------------------------
// On the curve (the top of the file)

template <class B>
struct OnCurve {
    typedef typename B::Elem Elem;
    typedef typename B::DMat DMat;
    typedef typename B::Point Point;
    const Side<B>& sd;
    const Letters<B>& L;
    std::vector<std::vector<DMat>> offd;                // the offsets in doubles
    double tol = 1e-8, prune = 4;
    int maxdepth = 400;
    size_t budget = 20000;                              // branches per decision
    size_t nodes = 0, undecided = 0, decisions = 0;     // counters
    size_t cutdepth = 0, cutbudget = 0;                 // the undecided decisions: by the depth limit, by the budget
    size_t skipped = 0, inexact = 0;                    // branches met again in a decision; branches past the guard
    StampSet seen;                                      // the keys met in the present decision
    bool canonical;                                     // de-duplicate by the renderer's key (exact without
                                                        // involutions: S^3), else by the oriented key (S^4)

    OnCurve(const Side<B>& s, const Letters<B>& l) : sd(s), L(l), canonical(keys_exact(l))
    {
        offd.assign(sd.off.size(), {});
        for (size_t y = 0; y < sd.off.size(); y++)
            for (const Elem& o : sd.off[y]) offd[y].push_back(B::to_double(o));
    }

    // a branch's size and its distance from P (the top of the file), from its matrix M and letter y
    struct Eval { double s, sfull, d; };
    Eval eval(const DMat& M, int y, const Point& P) const
    {
        Point a = B::boundary(B::act(M, sd.vfix)), b = B::boundary(B::act(M, sd.ev[y]));
        double s = chord(a, b), d;
        const std::vector<typename B::DVec>& wv = sd.wv[y];
        Point prev = a;
        d = INFINITY;
        for (size_t i = 1; i < wv.size(); i++) {
            Point v = B::boundary(B::act(M, wv[i]));
            s = std::max(s, segment_distance(v, a, b));
            d = std::min(d, segment_distance(P, prev, v));
            prev = v;
        }
        d = std::min(d, segment_distance(P, prev, b));
        double sfull = s;
        if (!sd.tipline.empty() && !sd.tipline[y].empty()) {
            Point q0 = a;
            for (const typename B::DVec& t : sd.tipline[y]) {
                Point q = B::boundary(B::act(M, t));
                sfull = std::max(sfull, segment_distance(q, a, b));
                d = std::min(d, segment_distance(P, q0, q));
                q0 = q;
            }
            d = std::min(d, segment_distance(P, q0, b));
        }
        return {s, sfull, d};
    }

    // 1: P is on the curve of the branch (h0, y0) (within the tolerance); 0: not; -1: undecided. top: the least size
    // the branch (h0, y0) itself is pruned with (a leaf of a drawing: its accuracy, since its curve was measured
    // against that). The branches are exact products while their coefficients are below the renderer's growth guard,
    // and each distinct branch is looked at once in a decision (a complete S^4 arc retraces itself at every scale:
    // docs/developer_guide.md): by the renderer's key where it is exact (S^3), by the oriented key otherwise (S^4,
    // where the renderer's key merges different arcs: oriented_key above); past the guard, products in doubles,
    // without de-duplication.
    int decide(const Elem& h0, int y0, const Point& P, double top = 0)
    {
        struct Fr { Elem h; DMat M; bool exact; int y, depth; };
        std::vector<Fr> st;
        st.push_back({h0, DMat(), true, y0, 0});
        size_t used = 0;
        bool cut = false, bydepth = false;
        decisions++;
        seen.clear();
        while (!st.empty()) {
            Fr f = st.back();
            st.pop_back();
            // a chain of one-letter roofs is the same arc as its last branch
            for (int c = 0; c < 64 && sd.roof[f.y].size() == 1; c++) {
                if (f.exact && B::max_coefficient(f.h) < (1LL << 40)) f.h = B::mul(f.h, sd.off[f.y][0]);
                else {
                    if (f.exact) { f.M = B::to_double(f.h); f.exact = false; }
                    f.M = dmul(f.M, offd[f.y][0]);
                }
                f.y = sd.roof[f.y][0];
            }
            if (f.exact) {
                if (!seen.insert(canonical ? branch_key<B>(L, f.h, f.y) : oriented_key<B>(f.h, f.y))) { skipped++; continue; }
                f.M = B::to_double(f.h);
            }
            if (++used > budget) { cut = true; cutbudget++; break; }
            nodes++;
            Eval e = eval(f.M, f.y, P);
            double s = f.depth == 0 ? std::max(e.s, top) : e.s;
            if (e.d > prune*s + tol) continue;
            if (e.sfull <= tol) return 1;
            if (f.depth >= maxdepth) { cut = true; bydepth = true; continue; }
            // the children, nearest to P last on the stack (popped first)
            const std::vector<int>& r = sd.roof[f.y];
            std::vector<std::pair<double, int>> order;
            Point s0 = B::boundary(B::act(f.M, sd.vfix));
            const std::vector<typename B::DVec>& wv = sd.wv[f.y];
            for (size_t i = 0; i < r.size(); i++) {
                Point s1 = i + 1 < r.size() ? B::boundary(B::act(f.M, wv[i + 1])) : B::boundary(B::act(f.M, sd.ev[f.y]));
                order.push_back({segment_distance(P, s0, s1), (int)i});
                s0 = s1;
            }
            std::sort(order.begin(), order.end(), [](const std::pair<double, int>& u, const std::pair<double, int>& v) {
                return u.first > v.first || (u.first == v.first && u.second < v.second); });
            const bool exact = f.exact && B::max_coefficient(f.h) < (1LL << 40);
            for (const auto& o : order) {
                if (exact) st.push_back({B::mul(f.h, sd.off[f.y][o.second]), DMat(), true, r[o.second], f.depth + 1});
                else { inexact++; st.push_back({f.h, dmul(f.M, offd[f.y][o.second]), false, r[o.second], f.depth + 1}); }
            }
        }
        if (cut) { undecided++; if (bydepth && used <= budget) cutdepth++; return -1; }
        return 0;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Candidates

// each leaf of a drawing is listed in the cells of side h (h = prune ACC, at least) that the box of its points meets,
// grown by its reach minus h when that is positive (its reach: prune times its size, at least ACC, plus tol); a query
// at P reads the 3^N cells about P's cell, so it finds every leaf whose points' polyline passes within its reach of P
template <int N>
struct CandidateGrid {
    double h = 1;
    std::vector<std::pair<uint64_t, uint32_t>> items;    // (cell, leaf), sorted
    std::vector<uint32_t> big;                           // leaves in more than BIGCELLS cells: checked at every query
    static constexpr size_t BIGCELLS = 4096;
    // the cells: open addressing from a cell to its range in items
    std::vector<uint64_t> ckey;
    std::vector<uint32_t> cbeg, cend;
    uint64_t mask = 0;

    static uint64_t cellkey(const int* c)
    {
        uint64_t k = 0;
        for (int i = 0; i < N; i++) k |= (uint64_t)(c[i] + 2048) << (12*i);
        return k + 1;                                    // never 0 (0 = empty)
    }
    void cell_of(const double* x, int* c) const { for (int i = 0; i < N; i++) c[i] = (int)std::floor(x[i]/h); }

    // leaf `id` with the box [lo, hi] and the reach r
    void add(uint32_t id, const double* lo, const double* hi, double r)
    {
        double e = std::max(0.0, r - h);
        int c0[N], c1[N];
        size_t count = 1;
        for (int i = 0; i < N; i++) {
            c0[i] = (int)std::floor((lo[i] - e)/h);
            c1[i] = (int)std::floor((hi[i] + e)/h);
            count *= (size_t)(c1[i] - c0[i] + 1);
        }
        if (count > BIGCELLS) { big.push_back(id); return; }
        int c[N];
        for (int i = 0; i < N; i++) c[i] = c0[i];
        while (true) {
            items.push_back({cellkey(c), id});
            int i = 0;
            while (i < N && ++c[i] > c1[i]) { c[i] = c0[i]; i++; }
            if (i == N) break;
        }
    }
    void finish()
    {
        std::sort(items.begin(), items.end());
        size_t cap = 16;
        while (cap < 2*items.size() + 16) cap <<= 1;
        ckey.assign(cap, 0); cbeg.assign(cap, 0); cend.assign(cap, 0);
        mask = cap - 1;
        for (size_t i = 0; i < items.size(); ) {
            size_t j = i;
            while (j < items.size() && items[j].first == items[i].first) j++;
            uint64_t k = items[i].first;
            size_t s = (size_t)(splitmix(k) & mask);
            while (ckey[s]) s = (s + 1) & mask;
            ckey[s] = k; cbeg[s] = (uint32_t)i; cend[s] = (uint32_t)j;
            i = j;
        }
    }
    // the leaves listed about P (each once, in increasing order)
    void query(const double* x, std::vector<uint32_t>& out) const
    {
        out.clear();
        int c0[N], c[N];
        cell_of(x, c0);
        for (int i = 0; i < N; i++) c[i] = c0[i] - 1;
        while (true) {
            uint64_t k = cellkey(c);
            size_t s = (size_t)(splitmix(k) & mask);
            while (ckey[s] && ckey[s] != k) s = (s + 1) & mask;
            if (ckey[s] == k) for (uint32_t t = cbeg[s]; t < cend[s]; t++) out.push_back(items[t].second);
            int i = 0;
            while (i < N && ++c[i] > c0[i] + 1) { c[i] = c0[i] - 1; i++; }
            if (i == N) break;
        }
        out.insert(out.end(), big.begin(), big.end());
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }
};

}  // namespace tools
}  // namespace zippers
