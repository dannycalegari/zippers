// SPDX-License-Identifier: GPL-3.0-or-later
/*  islands5.cc  --  the islands of the finite zippers of S^4 

    THE QUESTION. zipper5 draws the finite zipper  U_{h, j} h alpha_j  for the roots h (the elements of Gamma with
    d(o, h o) <= R that find_roots5 finds; examples/imm_m5.h checks them) and the base arcs j (the representatives of the
    inverse pairs of letters, as build draws them). The arc h alpha_j runs from h V to h s_j V (V = t^+ for Z+, the
    attracting point of t^-1 for Z-). The finite zipper of the roots can be disconnected. This program measures how,
    in two senses, and changes nothing.

    AS A GRAPH. The vertices are the points h V and h s_j V, the edges the arcs. Two vertices k V and k' V are the
    same point when k^-1 k' fixes V, and that is decided EXACTLY: for this t (the files' t = 0 5 8 4) the attracting
    eigenvalue of t_Sigma is quadratic, lambda = 3 + 2 sqrt 2, and its eigenvector is v = p0 + q0 sqrt 2 with integer
    vectors p0, q0 (found and checked below: ExactPoint). The point k V of S^4 is (k v)'/(k v)_6, five numbers
    (a_i + b_i sqrt 2)/c of Q(sqrt 2); written over a common positive denominator in lowest terms, the 11 integers
    (a_1..a_5, b_1..b_5, c) are a canonical key of the point: two vertices are the same point exactly when their keys
    are equal. Every key is also compared with the point in doubles (as the renderer computes it: boundary(act(k,
    vfix))): one key, one point (spread below 1e-12), and distinct keys far apart. Then: the components (union-find),
    the identity's (the one of V, which every base arc leaves from), and the roots and arcs outside it, with d(o, h o).
    All the arcs of a root leave from its vertex h V, so a component is a union of roots' stars.

    AS DRAWN. The renderer draws a branch reached twice once, by its key (render/renderer.h: the fingerprint of the
    key of h and the letter of its canonical inverse pair), and the S^4 arcs retrace themselves, so arcs with
    no common end can share drawn branches. A copy of the renderer's traversal (TrackRenderer: Renderer::draw and
    visit, statement for statement) records which root arc (h, j) first visited each key, and at every skip (a key
    already done: in draw, counted in nSkipped, or a child found done in visit, which the renderer does not count) the
    arc that owns it. Each run checks the copy against the library's renderer: every counter, every point and every
    polyline record, bit for bit. Then: for each arc outside the identity's graph component, whether it shares a drawn
    branch with an arc of that component (so that the two touch in the picture); and the components "as drawn", where
    two arcs are joined when they share a vertex or a drawn branch. The drawing depends on the accuracy, so this does.

    THE PICTURE (--png PREFIX): for each side and radius, PREFIX_R<R>_<plus|minus>.png, the view of zipper5 (its
    defaults), at the first accuracy: the identity's component in the side's colour (blue Z+, red Z-; paler when there
    are islands), the island arcs that share a drawn branch with it in orange, and the other island arcs in green,
    drawn over the rest.

    Usage:  islands5 [-f SUBST] [-R R1,R2,...] [-a ACC1,ACC2,...] [--only +|-] [--png PREFIX] [-s SIZE] [--list]
                     [--graph]
            (defaults: subst5_2.30_4.50_t1_r0.txt, found as zipper5 finds it (data/s4/), -R 1.32,1.8,2.1,
            -a 0.02; --list prints every root outside the identity's component; --graph: the graph only, no drawing).
            Timings and memory on stderr.
*/

#include "core/hash.h"
#include "core/keyset.h"
#include "core/timing.h"
#include "default_file.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s4.h"

#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace z = zippers;
typedef z::imm::O51 B;
typedef B::Elem E;
typedef __int128 I128;
typedef std::array<int64_t, 6> V6;

static double peak_mb()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss/1e6;
#else
    return ru.ru_maxrss/1e3;
#endif
}

static void die(const char* why) { fprintf(stderr, "islands5: %s\n", why); exit(1); }

static int64_t narrow(I128 x)
{
    if (x > ((I128)1 << 62) || x < -((I128)1 << 62)) die("an integer beyond 2^62");
    return (int64_t)x;
}
static int64_t gcd64(int64_t a, int64_t b) { a = a < 0 ? -a : a; b = b < 0 ? -b : b; while (b) { int64_t r = a % b; a = b; b = r; } return a; }

// ---- the exact attracting eigenvector of t_Sigma --------------------------------------------------------------------

typedef std::array<std::array<I128, 6>, 6> M128;
static M128 to128(const E& g) { M128 m; for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) m[i][j] = g.e[i][j]; return m; }
static M128 mmul(const M128& a, const M128& b)
{
    M128 c;
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) { I128 s = 0; for (int k = 0; k < 6; k++) s += a[i][k]*b[k][j]; c[i][j] = s; }
    return c;
}
static std::array<I128, 6> mvec(const M128& a, const std::array<I128, 6>& v)
{
    std::array<I128, 6> r;
    for (int i = 0; i < 6; i++) { I128 s = 0; for (int k = 0; k < 6; k++) s += a[i][k]*v[k]; r[i] = s; }
    return r;
}

// the characteristic polynomial det(x - T) of an integer 6x6 matrix (Faddeev-LeVerrier, exact): c[0..6], c[6] = 1
static std::array<I128, 7> charpoly(const M128& T)
{
    std::array<I128, 7> c{};
    c[6] = 1;
    M128 M{};                                         // M_0 = 0
    for (int k = 1; k <= 6; k++) {
        M128 TM = mmul(T, M);
        for (int i = 0; i < 6; i++) TM[i][i] += c[6 - k + 1];
        M = TM;                                       // M_k = T M_{k-1} + c_{n-k+1} I
        M128 TMk = mmul(T, M);
        I128 tr = 0;
        for (int i = 0; i < 6; i++) tr += TMk[i][i];
        if (tr % k) die("charpoly: a coefficient is not an integer");
        c[6 - k] = -tr/k;
    }
    return c;
}

struct ExactPoint {
    int64_t s = 0, m = 0, D = 0;                      // 2 lambda = (s + m sqrt D)/2, D squarefree: lambda the attracting eigenvalue
    V6 p0{}, q0{};                                    // the eigenvector p0 + q0 sqrt D (future)
};

// For T = 2 t_Sigma: s = 2 (lambda + 1/lambda) from the numerical attracting point, x^2 - s x + 4 must divide the
// characteristic polynomial exactly (lambda quadratic); then b = a nonzero column of Q(T), Q = charpoly/(x^2 - s x + 4),
// lies in E = ker(T^2 - s T + 4), and p0 + q0 sqrt D = 2 T b - (s - m sqrt D) b is the eigenvector of 2 lambda
// = (s + m sqrt D)/2. Every step is checked exactly; exits if t_Sigma's eigenvalue is not quadratic.
static ExactPoint exact_point(const z::Side<B>& sd)
{
    ExactPoint X;
    M128 T = to128(sd.t);
    // lambda from the power iteration's vector: (t v)_6 / v_6
    B::DMat td = B::to_double(sd.t);
    B::DVec tv = B::act(td, sd.vfix);
    double lam = tv[5]/sd.vfix[5];
    X.s = (int64_t)std::llround(2*(lam + 1/lam));
    std::array<I128, 7> c = charpoly(T);
    // divide c by x^2 - s x + 4
    std::array<I128, 5> q{};
    std::array<I128, 7> r = c;
    for (int d = 6; d >= 2; d--) {
        I128 k = r[d];
        q[d - 2] = k;
        r[d] -= k; r[d - 1] += k*X.s; r[d - 2] -= 4*k;
    }
    if (r[0] != 0 || r[1] != 0) die("t's attracting eigenvalue is not quadratic: x^2 - s x + 4 does not divide its polynomial");
    // s^2 - 16 = m^2 D
    int64_t disc = X.s*X.s - 16;
    if (disc <= 0) die("t is not loxodromic");
    X.m = 1; X.D = disc;
    for (int64_t p = 2; p*p <= X.D; p++) while (X.D % (p*p) == 0) { X.D /= p*p; X.m *= p; }
    if (X.D == 1) die("t's eigenvalue is rational: not handled");
    // Q(T) by Horner, and a nonzero column, reduced by its gcd
    M128 Q{};
    for (int d = 4; d >= 0; d--) {
        M128 TQ = mmul(T, Q);
        for (int i = 0; i < 6; i++) TQ[i][i] += q[d];
        Q = TQ;
    }
    std::array<I128, 6> b{};
    bool found = false;
    for (int j = 0; j < 6 && !found; j++) {
        for (int i = 0; i < 6; i++) b[i] = Q[i][j];
        for (int i = 0; i < 6; i++) found = found || b[i] != 0;
    }
    if (!found) die("Q(T) = 0");
    int64_t g = 0;
    for (int i = 0; i < 6; i++) g = gcd64(g, narrow(b[i]));
    for (int i = 0; i < 6; i++) b[i] /= g;
    // b in ker(T^2 - s T + 4)
    std::array<I128, 6> Tb = mvec(T, b), TTb = mvec(T, Tb);
    for (int i = 0; i < 6; i++) if (TTb[i] - X.s*Tb[i] + 4*b[i] != 0) die("b is not in the axis plane");
    for (int i = 0; i < 6; i++) { X.p0[i] = narrow(2*Tb[i] - X.s*b[i]); X.q0[i] = narrow(X.m*b[i]); }
    // checks: 2 T p0 = s p0 + m D q0, 2 T q0 = m p0 + s q0; null; future (p0_6 + q0_6 sqrt D > 0, else negate)
    std::array<I128, 6> P0, Q0;
    for (int i = 0; i < 6; i++) { P0[i] = X.p0[i]; Q0[i] = X.q0[i]; }
    std::array<I128, 6> TP = mvec(T, P0), TQ0 = mvec(T, Q0);
    for (int i = 0; i < 6; i++)
        if (2*TP[i] != X.s*P0[i] + X.m*X.D*Q0[i] || 2*TQ0[i] != X.m*P0[i] + X.s*Q0[i]) die("p0 + q0 sqrt D is not an eigenvector");
    I128 pp = 0, qq = 0, pq = 0;
    for (int i = 0; i < 6; i++) { I128 sg = i < 5 ? 1 : -1; pp += sg*P0[i]*P0[i]; qq += sg*Q0[i]*Q0[i]; pq += sg*P0[i]*Q0[i]; }
    if (pp + X.D*qq != 0 || pq != 0) die("the eigenvector is not null");
    // the sign of p6 + q6 sqrt D
    I128 a = X.p0[5], bb = X.q0[5];
    bool positive = (a >= 0 && bb >= 0 && (a > 0 || bb > 0)) || (a >= 0 && bb < 0 && a*a > X.D*bb*bb) ||
                    (a < 0 && bb > 0 && X.D*bb*bb > a*a);
    if (!positive) for (int i = 0; i < 6; i++) { X.p0[i] = -X.p0[i]; X.q0[i] = -X.q0[i]; }
    // the attracting one: (s + m sqrt D)/2 > 2, and the point is the power iteration's V
    double v6 = X.p0[5] + X.q0[5]*std::sqrt((double)X.D), err = 0;
    for (int i = 0; i < 5; i++) err = std::max(err, std::fabs((X.p0[i] + X.q0[i]*std::sqrt((double)X.D))/v6 - sd.V.x[i]));
    if (err > 1e-12) die("the exact eigenvector is not the power iteration's attracting point");
    return X;
}

// the canonical key of the point k V: (a_1..a_5, b_1..b_5, c) with (k v)_i/(k v)_6 = (a_i + b_i sqrt D)/c
typedef std::array<int64_t, 11> PKey;
static PKey point_key(const ExactPoint& X, const E& k)
{
    I128 P[6], Q[6];
    for (int i = 0; i < 6; i++) {
        I128 sp = 0, sq = 0;
        for (int j = 0; j < 6; j++) { sp += (I128)k.e[i][j]*X.p0[j]; sq += (I128)k.e[i][j]*X.q0[j]; }
        P[i] = sp; Q[i] = sq;
    }
    PKey key;
    for (int i = 0; i < 5; i++) {
        key[i] = narrow(P[i]*P[5] - X.D*Q[i]*Q[5]);
        key[5 + i] = narrow(Q[i]*P[5] - P[i]*Q[5]);
    }
    key[10] = narrow(P[5]*P[5] - X.D*Q[5]*Q[5]);
    if (key[10] == 0) die("a zero denominator");
    int64_t g = 0;
    for (int64_t x : key) g = gcd64(g, x);
    int sg = key[10] < 0 ? -1 : 1;
    for (int64_t& x : key) x = sg*(x/g);
    return key;
}

// ---- union-find ----------------------------------------------------------------------------------------------------

struct UF {
    std::vector<int> p;
    int add() { p.push_back((int)p.size()); return (int)p.size() - 1; }
    int find(int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
    void join(int a, int b) { a = find(a); b = find(b); if (a != b) p[std::max(a, b)] = std::min(a, b); }
};

// ---- the renderer's traversal, with the owners of the keys --------------------------------------------------------

// a map from fingerprints (nonzero) to an owner, by open addressing
struct OwnerMap {
    std::vector<uint64_t> k;
    std::vector<int32_t> v;
    size_t n = 0;
    OwnerMap() { k.assign(1 << 16, 0); v.assign(1 << 16, -1); }
    void set(uint64_t key, int32_t val)
    {
        if (2*(n + 1) > k.size()) {
            std::vector<uint64_t> ok; std::vector<int32_t> ov;
            ok.swap(k); ov.swap(v);
            k.assign(2*ok.size(), 0); v.assign(2*ov.size(), -1); n = 0;
            for (size_t i = 0; i < ok.size(); i++) if (ok[i]) set(ok[i], ov[i]);
        }
        uint64_t m = k.size() - 1, i = key & m;
        for (; k[i]; i = (i + 1) & m) if (k[i] == key) { v[i] = val; return; }
        k[i] = key; v[i] = val; n++;
    }
    int32_t get(uint64_t key) const
    {
        uint64_t m = k.size() - 1;
        for (uint64_t i = key & m; k[i]; i = (i + 1) & m) if (k[i] == key) return v[i];
        return -1;
    }
};

// Renderer::draw and Renderer::visit (render/renderer.h), statement for statement, with the owner of each key
// recorded when draw inserts it, and every skip reported with the key's owner. The renderer's own members (done, the
// stack, the counters, emit) are used, so the polylines are the renderer's.
struct TrackRenderer : z::Renderer<B> {
    typedef z::Renderer<B> R;
    using R::R;
    OwnerMap owner;
    int curarc = -1;
    std::vector<std::pair<int, int>> shares;          // (arc, owner) at a skip of a key owned by another arc
    size_t skipDraw = 0, skipChild = 0, skipSelf = 0;

    void skip(uint64_t key, bool indraw)
    {
        (indraw ? skipDraw : skipChild)++;
        int32_t w = owner.get(key);
        if (w < 0) die("a key done with no owner");
        if (w == curarc) skipSelf++;
        else shares.push_back({curarc, w});
    }
    void draw_t(const E& g, int j, int rootindex, int arc)
    {
        if (capped) return;
        open = false;
        curroot = rootindex; curletter = j;
        curarc = arc;
        stk.clear();
        stk.push_back({g, j, 0, 0, z::fingerprint(B::key(g), L.canon[j])});
        while (!stk.empty() && !capped) {
            Frame f = stk.back();
            stk.pop_back();
            if (f.kind == 1) { open = false; continue; }
            if (!done.insert(f.key)) { ++nSkipped; open = false; skip(f.key, true); continue; }
            owner.set(f.key, arc);
            ++nVisited;
            visit_t(f);
        }
    }
    void visit_t(const Frame& f)
    {
        B::DMat hd = B::to_double(f.h);
        Point a = B::boundary(B::act(hd, sd.vfix)), b = B::boundary(B::act(hd, sd.ev[f.j]));
        const std::vector<B::DVec>& wv = sd.wv[f.j];
        bool split = z::chord(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (z::segment_distance(B::boundary(B::act(hd, wv[i])), a, b) > acc) { split = true; ++nSpike; break; }
        if (!split && !sd.through.empty() && !sd.through[f.j].empty() && B::max_coefficient(f.h) < (1LL << 40) &&
            z::through_split(sd, f.h, f.j, a, b, acc)) { split = true; ++nSpike; ++nThrough; }
        if (split && f.depth < maxdepth && B::max_coefficient(f.h) < (1LL << 40)) {
            deepest = std::max(deepest, f.depth + 1);
            const std::vector<int>& roof = sd.roof[f.j];
            for (int i = (int)roof.size() - 1; i >= 0; i--) {
                int x = roof[i];
                uint64_t kc = z::fingerprint(B::product_key(f.h, sd.keyoff[f.j][i]), L.canon[x]);
                if (done.contains(kc)) { skip(kc, false); stk.push_back({f.h, 0, 0, 1, 0}); continue; }
                stk.push_back({B::mul(f.h, sd.off[f.j][i]), x, f.depth + 1, 0, kc});
            }
        } else {
            if (split) ++nForced;
            if (sd.tipline.empty() || sd.tipline[f.j].empty()) emit(a, b);
            else emit(a, b, hd, sd.tipline[f.j]);
        }
    }
};

// a digest of a renderer: every counter, and a hash of every point and polyline record (the library's renderer is
// drawn first and freed, then the copy, so that the two need not be held at once)
struct Digest {
    size_t nVisited, nSkipped, nLeaves, nSpike, nForced, nTipPoints, nThrough, npts, npoly;
    int deepest;
    bool capped;
    uint64_t hash;
    bool operator==(const Digest& o) const
    {
        return nVisited == o.nVisited && nSkipped == o.nSkipped && nLeaves == o.nLeaves && nSpike == o.nSpike &&
               nForced == o.nForced && nTipPoints == o.nTipPoints && nThrough == o.nThrough && npts == o.npts &&
               npoly == o.npoly && deepest == o.deepest && capped == o.capped && hash == o.hash;
    }
};
template <class X>
static uint64_t hash_bytes(uint64_t h, const std::vector<X>& v)
{
    const unsigned char* p = (const unsigned char*)v.data();
    size_t n = v.size()*sizeof(X);
    for (size_t i = 0; i < n; i += 8) {
        uint64_t w = 0;
        memcpy(&w, p + i, std::min<size_t>(8, n - i));
        h = z::splitmix(h ^ w);
    }
    return z::splitmix(h ^ n);
}
static Digest digest(const z::Renderer<B>& r)
{
    uint64_t h = 0x7a1b2c3d4e5f6071ULL;
    h = hash_bytes(h, r.pts); h = hash_bytes(h, r.starts); h = hash_bytes(h, r.polyroot); h = hash_bytes(h, r.polyletter);
    return {r.nVisited, r.nSkipped, r.nLeaves, r.nSpike, r.nForced, r.nTipPoints, r.nThrough, r.pts.size(), r.npoly(),
            r.deepest, r.capped, h};
}

// ---- one side at one radius -----------------------------------------------------------------------------------------

struct Options {
    std::string subst = z::tools::default_subst(false), png;
    bool subst_given = false;
    std::vector<double> Rs = {1.32, 1.8, 2.1}, accs = {0.02};
    int which = 3, size = 840;
    bool list = false, graphonly = false;
    size_t cap = 20000000;
};

static std::vector<double> numbers(const std::string& s)
{
    std::vector<double> v;
    std::stringstream ss(s);
    std::string t;
    while (std::getline(ss, t, ',')) v.push_back(atof(t.c_str()));
    return v;
}

static void side_at(const Options& o, const z::World<B>& W, z::Side<B>& sd, const ExactPoint& X, double R,
                    const std::vector<E>& roots, const std::vector<double>& rootd)
{
    const z::Letters<B>& L = W.L;
    double t0 = z::now();
    std::vector<int> base;                             // the base arcs, as build draws them
    for (size_t j = 0; j < L.s.size(); j++) if (L.plus[j]) base.push_back((int)j);
    // ---- the graph ----
    std::map<PKey, int> vid;
    std::vector<z::SpherePoint<5>> vpt;                // the point of each vertex, in doubles (its first element)
    double spread = 0;                                 // the largest distance between two elements' points of one vertex
    UF uf;
    auto vertex = [&](const E& k) {
        PKey key = point_key(X, k);
        z::SpherePoint<5> p = B::boundary(B::act(B::to_double(k), sd.vfix));
        auto it = vid.find(key);
        if (it != vid.end()) {
            double d = 0;
            for (int i = 0; i < 5; i++) d += (p.x[i] - vpt[it->second].x[i])*(p.x[i] - vpt[it->second].x[i]);
            spread = std::max(spread, std::sqrt(d));
            return it->second;
        }
        int v = uf.add();
        vid[key] = v;
        vpt.push_back(p);
        return v;
    };
    struct Arc { int r, j, a, b; };
    std::vector<Arc> arcs;
    std::vector<int> rootv(roots.size());
    for (size_t r = 0; r < roots.size(); r++) {
        rootv[r] = vertex(roots[r]);
        for (int j : base) {
            int e = vertex(B::mul(roots[r], L.s[j]));
            arcs.push_back({(int)r, j, rootv[r], e});
            uf.join(rootv[r], e);
        }
    }
    if (!B::eq(roots[0], B::identity())) die("the first root is not 1");
    // distinct keys, distinct points: the smallest distance between two vertices (all pairs up to 20000 vertices;
    // beyond, the pairs in neighbouring cells of a grid of side 0.004, so a gap of 0.004 or more is printed as 0.004)
    double gap = 1e9;
    if (vpt.size() <= 20000) {
        for (size_t a = 0; a < vpt.size(); a++)
            for (size_t b = a + 1; b < vpt.size(); b++) {
                double d = 0;
                for (int i = 0; i < 5; i++) d += (vpt[a].x[i] - vpt[b].x[i])*(vpt[a].x[i] - vpt[b].x[i]);
                gap = std::min(gap, std::sqrt(d));
            }
    } else {
        const double cell = 0.004;
        std::map<std::array<int, 5>, std::vector<int>> grid;
        auto cellof = [&](const z::SpherePoint<5>& p) {
            std::array<int, 5> c;
            for (int i = 0; i < 5; i++) c[i] = (int)std::floor(p.x[i]/cell);
            return c;
        };
        for (size_t a = 0; a < vpt.size(); a++) grid[cellof(vpt[a])].push_back((int)a);
        gap = cell;
        for (size_t a = 0; a < vpt.size(); a++) {
            std::array<int, 5> c = cellof(vpt[a]), n;
            for (int m = 0; m < 243; m++) {
                int q = m;
                for (int i = 0; i < 5; i++) { n[i] = c[i] + q % 3 - 1; q /= 3; }
                auto it = grid.find(n);
                if (it == grid.end()) continue;
                for (int b : it->second) {
                    if (b <= (int)a) continue;
                    double d = 0;
                    for (int i = 0; i < 5; i++) d += (vpt[a].x[i] - vpt[b].x[i])*(vpt[a].x[i] - vpt[b].x[i]);
                    gap = std::min(gap, std::sqrt(d));
                }
            }
        }
    }
    int idc = uf.find(rootv[0]);
    std::map<int, std::vector<int>> comproots;         // component -> its roots
    std::set<int> comps;
    for (size_t v = 0; v < vpt.size(); v++) comps.insert(uf.find((int)v));
    for (size_t r = 0; r < roots.size(); r++) comproots[uf.find(rootv[r])].push_back((int)r);
    size_t outroots = 0, outarcs = 0;
    for (auto& kv : comproots) if (kv.first != idc) outroots += kv.second.size();
    for (const Arc& a : arcs) outarcs += uf.find(a.a) != idc;
    double tg = z::now() - t0;
    printf("%s R=%g: %zu roots, %zu arcs, %zu vertices (exact keys in Q(sqrt %lld); in doubles one key is one point to "
           "%.1e, distinct keys at least %.3g%s apart), %zu components\n", sd.name, R, roots.size(), arcs.size(),
           vpt.size(), (long long)X.D, spread, gap, vpt.size() > 20000 && gap >= 0.004 ? " (the grid's cell)" : "",
           comps.size());
    size_t idv = 0;
    for (size_t v = 0; v < vpt.size(); v++) idv += uf.find((int)v) == idc;
    printf("  as a graph: the identity's component has %zu roots, %zu arcs, %zu vertices; outside it %zu roots and %zu "
           "arcs, in %zu components", comproots[idc].size(), arcs.size() - outarcs, idv, outroots, outarcs, comps.size() - 1);
    // the components without roots cannot exist: every vertex is an end of an arc of some root
    std::map<int, int> sizes;                          // the outside components by their number of roots
    for (auto& kv : comproots) if (kv.first != idc) sizes[(int)kv.second.size()]++;
    if (!sizes.empty()) {
        printf(" (by roots:");
        for (auto& kv : sizes) printf(" %d x %d", kv.second, kv.first);
        printf(")");
    }
    printf("\n");
    if (outroots) {
        std::map<double, int> byd;                     // the outside roots by distance
        for (auto& kv : comproots) if (kv.first != idc) for (int r : kv.second) byd[std::round(rootd[r]*1e6)/1e6]++;
        printf("  the roots outside, by d(o, h o):");
        for (auto& kv : byd) printf(" %.6f: %d", kv.first, kv.second);
        printf("\n");
        if (o.list)
            for (auto& kv : comproots)
                if (kv.first != idc) {
                    printf("    component of %zu roots:", kv.second.size());
                    for (int r : kv.second) printf(" #%d (d %.6f)", r, rootd[r]);
                    printf("\n");
                }
    }
    // how the roots' stars are glued: two roots are adjacent when their stars share a vertex, through h V = h' V
    // (h^-1 h' fixes V), h V = h' s_k V, or h s_j V = h' s_k V; the depths of a breadth-first search from 1
    {
        std::map<int, std::set<int>> at;               // vertex -> the roots whose star has it
        std::map<int, int> centre;                     // vertex -> how many roots have it as their centre h V
        for (const Arc& a : arcs) { at[a.a].insert(a.r); at[a.b].insert(a.r); }
        for (size_t r = 0; r < roots.size(); r++) centre[rootv[r]]++;
        size_t samecentre = 0, centre_end = 0, end_end = 0;
        std::vector<std::set<int>> adj(roots.size());
        for (auto& kv : at) {
            std::vector<int> rs(kv.second.begin(), kv.second.end());
            for (size_t x = 0; x < rs.size(); x++) for (size_t y = x + 1; y < rs.size(); y++) {
                adj[rs[x]].insert(rs[y]); adj[rs[y]].insert(rs[x]);
            }
            int c = centre.count(kv.first) ? centre[kv.first] : 0;
            if (c >= 2) samecentre++;
            else if (c == 1 && rs.size() >= 2) centre_end++;
            else if (c == 0 && rs.size() >= 2) end_end++;
        }
        std::vector<int> depth(roots.size(), -1);
        std::vector<int> queue = {0};
        depth[0] = 0;
        for (size_t q = 0; q < queue.size(); q++)
            for (int y : adj[queue[q]]) if (depth[y] < 0) { depth[y] = depth[queue[q]] + 1; queue.push_back(y); }
        std::map<int, int> bydepth;
        size_t deg = 0, mindeg = (size_t)-1;
        for (size_t r = 0; r < roots.size(); r++) { bydepth[depth[r]]++; deg += adj[r].size(); mindeg = std::min(mindeg, adj[r].size()); }
        printf("  the roots' stars: vertices shared by two or more stars: %zu centres h V of two or more roots, %zu centres "
               "that are ends of other roots' arcs, %zu ends only; the roots adjacent to a root: %.1f on average, at "
               "least %zu; roots by their depth from 1:", samecentre, centre_end, end_end,
               roots.size() ? (double)deg/roots.size() : 0.0, mindeg);
        for (auto& kv : bydepth) printf(" %d: %d", kv.first, kv.second);
        printf("\n");
    }
    fprintf(stderr, "%s R=%g: the graph in %.2f s\n", sd.name, R, tg);
    if (o.graphonly) return;

    // ---- as drawn ----
    for (double acc : o.accs) {
        double t1 = z::now();
        std::vector<int> arcid(roots.size()*L.s.size(), -1);
        for (size_t i = 0; i < arcs.size(); i++) arcid[arcs[i].r*L.s.size() + arcs[i].j] = (int)i;
        Digest dP;
        {                                              // the library's renderer, as build draws (one side), then freed
            z::Renderer<B> P(sd, L, acc, o.cap);
            for (size_t r = 0; r < roots.size(); r++) for (int j : base) P.draw(roots[r], j, (int)r);
            dP = digest(P);
        }
        double peakP = peak_mb();
        TrackRenderer T(sd, L, acc, o.cap);
        for (size_t r = 0; r < roots.size(); r++) for (int j : base) T.draw_t(roots[r], j, (int)r, arcid[r*L.s.size() + j]);
        bool same = digest(T) == dP;
        double t2 = z::now();
        // the arcs' classes: 0 the identity's component, 1 an island arc that shares a drawn branch with an arc of it,
        // 2 an island arc that does not
        std::vector<char> cls(arcs.size(), 0);
        std::set<std::pair<int, int>> pairs;
        size_t crossI = 0;                             // shares between an island arc and an arc of the identity's
        for (auto& sh : T.shares) {
            int a = sh.first, b = sh.second;
            pairs.insert({std::min(a, b), std::max(a, b)});
        }
        std::vector<char> touches(arcs.size(), 0);
        for (auto& pr : pairs) {
            bool ia = uf.find(arcs[pr.first].a) == idc, ib = uf.find(arcs[pr.second].a) == idc;
            if (ia != ib) { crossI++; touches[ia ? pr.second : pr.first] = 1; }
        }
        size_t island = 0, touching = 0;
        for (size_t i = 0; i < arcs.size(); i++)
            if (uf.find(arcs[i].a) != idc) { island++; touching += touches[i]; cls[i] = touches[i] ? 1 : 2; }
        // the components as drawn: arcs joined by a common vertex (their graph component) or a shared drawn branch
        UF ud;
        for (size_t i = 0; i < arcs.size(); i++) ud.add();
        std::map<int, int> firstarc;
        for (size_t i = 0; i < arcs.size(); i++) {
            int c = uf.find(arcs[i].a);
            if (!firstarc.count(c)) firstarc[c] = (int)i; else ud.join(firstarc[c], (int)i);
        }
        for (auto& pr : pairs) ud.join(pr.first, pr.second);
        std::set<int> dcomps;
        for (size_t i = 0; i < arcs.size(); i++) dcomps.insert(ud.find((int)i));
        int idd = ud.find(firstarc[idc]);
        size_t dout = 0, droots = 0;
        std::set<int> outr;
        for (size_t i = 0; i < arcs.size(); i++) if (ud.find((int)i) != idd) { dout++; outr.insert(arcs[i].r); }
        droots = outr.size();
        printf("  as drawn, acc %g: %zu branches, %zu skipped in draw and %zu children found done (%zu by their own arc), "
               "%zu arc pairs share a drawn branch, %zu of them an island arc and an arc of the identity's component\n",
               acc, T.nVisited, T.skipDraw, T.skipChild, T.skipSelf, pairs.size(), crossI);
        printf("    of the %zu island arcs, %zu share a drawn branch with the identity's component; as drawn (joined by "
               "common ends and shared branches) %zu components, and outside the identity's %zu arcs of %zu roots\n",
               island, touching, dcomps.size(), dout, droots);
        if (island) {                                  // each island (graph component): its arcs, those touching
            std::map<int, std::pair<int, int>> per;    // component -> (arcs, arcs sharing a branch with the identity's)
            std::map<int, bool> joined;                // component -> in the identity's component as drawn
            for (size_t i = 0; i < arcs.size(); i++) {
                int c = uf.find(arcs[i].a);
                if (c == idc) continue;
                per[c].first++;
                per[c].second += touches[i];
                joined[c] = ud.find((int)i) == idd;
            }
            int direct = 0, via = 0, apart = 0;
            for (auto& kv : per) {
                if (kv.second.second) direct++; else if (joined[kv.first]) via++; else apart++;
            }
            printf("    the %zu islands: %d with an arc that shares a drawn branch with the identity's component, %d joined "
                   "to it only through other islands, %d not joined to it\n", per.size(), direct, via, apart);
            if (o.list)
                for (auto& kv : per)
                    printf("      island of %d arcs (roots of the first: #%d): %d share a drawn branch with the identity's "
                           "component\n", kv.second.first, arcs[firstarc[kv.first]].r, kv.second.second);
        }
        printf("    the traversal copy against the renderer: %s (%zu leaves, %zu polylines%s)\n",
               same ? "the same, bit for bit" : "DIFFERENT", T.nLeaves, T.npoly(), T.capped ? ", LEAF CAP REACHED" : "");
        fprintf(stderr, "%s R=%g acc=%g: drawn twice in %.2f s, the classes in %.2f s; peak %.0f MB (%.0f MB after the "
                "library's renderer)\n", sd.name, R, acc, t2 - t1, z::now() - t2, peak_mb(), peakP);
        if (!same) die("the traversal copy differs from the renderer");
        // ---- the picture ----
        if (!o.png.empty() && acc == o.accs.front()) {
            z::Renderer<B> layer[3] = {z::Renderer<B>(sd, L, acc, o.cap), z::Renderer<B>(sd, L, acc, o.cap),
                                       z::Renderer<B>(sd, L, acc, o.cap)};
            for (size_t k = 0; k < T.npoly(); k++) {
                int c = cls[arcid[(size_t)T.polyroot[k]*L.s.size() + T.polyletter[k]]];
                z::Renderer<B>& Lr = layer[c];
                Lr.starts.push_back((uint32_t)(Lr.pts.size()/5));
                Lr.polyroot.push_back(T.polyroot[k]); Lr.polyletter.push_back(T.polyletter[k]);
                Lr.pts.insert(Lr.pts.end(), T.pts.begin() + 5*T.pbegin(k), T.pts.begin() + 5*T.pend(k));
            }
            z::s4::View5 v;
            z::s4::init_view5(W, v, 0, 15);
            v.clip = 3; v.zoom = 1; v.hidfog = 1.5;
            bool plus = &sd == &W.plus;
            // the side's colour (zipper5's), paler when there are islands so that they stand out
            double w = island ? 0.6 : 0;
            double c0[3] = {plus ? 0.05 : 0.85, plus ? 0.15 : 0.08, plus ? 0.85 : 0.08};
            std::vector<z::s4::Layer5> layers = {
                {&layer[0], {c0[0] + w*(1 - c0[0]), c0[1] + w*(1 - c0[1]), c0[2] + w*(1 - c0[2])}, 1},
                {&layer[1], {0.95, 0.50, 0.0}, 2},
                {&layer[2], {0.0, 0.55, 0.10}, 3}};
            char fn[512];
            snprintf(fn, sizeof fn, "%s_R%g_%s.png", o.png.c_str(), R, plus ? "plus" : "minus");
            std::vector<B::Point> marks = {W.plus.V, W.minus.V};
            if (!z::s4::write_png5(fn, o.size, layers, v, marks, 0)) die("cannot write the picture");
            printf("    picture %s: %zu polylines of the identity's component, %zu of island arcs that touch it, %zu of "
                   "island arcs that do not\n", fn, layer[0].npoly(), layer[1].npoly(), layer[2].npoly());
        }
    }
}

int main(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) die("a value is missing"); return argv[++i]; };
        if (a == "-f") { o.subst = next(); o.subst_given = true; }
        else if (a == "-R") o.Rs = numbers(next());
        else if (a == "-a") o.accs = numbers(next());
        else if (a == "--only") { std::string w = next(); o.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--png") o.png = next();
        else if (a == "-s") o.size = atoi(next().c_str());
        else if (a == "--list") o.list = true;
        else if (a == "--graph") o.graphonly = true;
        else if (a == "--cap") o.cap = (size_t)atof(next().c_str());
        else { fprintf(stderr, "unknown option %s (see the top of islands5.cc)\n", a.c_str()); return 1; }
    }
    if (!z::tools::find_subst(o.subst, false, !o.subst_given, ZIPPERS_DATA_PLACES)) return 1;   // (the default: zipper5's)
    z::imm::setup();
    z::World<B> W;
    z::prepare<z::imm::RenderPolicy>(o.subst.c_str(), W);
    ExactPoint Xp = exact_point(W.plus), Xm = exact_point(W.minus);
    printf("%s: |S| = %zu; the attracting eigenvectors, exactly: Z+ 2 lambda = (%lld + %lld sqrt %lld)/2, Z- the same "
           "(%lld + %lld sqrt %lld)/2\n", o.subst.c_str(), W.L.s.size(), (long long)Xp.s, (long long)Xp.m, (long long)Xp.D,
           (long long)Xm.s, (long long)Xm.m, (long long)Xm.D);
    for (double R : o.Rs) {
        std::vector<E> roots;
        std::vector<double> rootd;
        z::imm::find_roots5(R, roots, rootd);
        z::imm::RootCertificate c = z::imm::certify_roots5(R);
        printf("%s\n", z::imm::root_certificate_line(c).c_str());
        if (o.which & 1) side_at(o, W, W.plus, Xp, R, roots, rootd);
        if (o.which & 2) side_at(o, W, W.minus, Xm, R, roots, rootd);
        fflush(stdout);
    }
    fprintf(stderr, "islands5: done, peak %.0f MB\n", peak_mb());
    return 0;
}
