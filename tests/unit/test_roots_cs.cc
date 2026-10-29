// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_roots_cs: the descent checks of the S^3 root balls (examples/cartwright_steger_roots.h) and the lines of
// zipper3 --stats and link3 --stats. It checks:
//   - the code the checks stand on: the interval arithmetic encloses the exact results (fma and two-sum tests),
//     strictly; the ball columns of the elements of T against the exact |(g t)_33| / |g_33| on random elements; the
//     cell radius rho_C against points sampled in random cells (a third at corners);
//   - exact enumerations, made here again: the units of norm 1; the reduction mod 3 of the library against an
//     independent one (zeta -> -i, the other root of Phi_12 mod 3); the blocks A with A^* P A = P against the group
//     <u', v'>; |g_33|^2 on every element of B(6) against alpha;
//   - the descent checks: they pass, with their numbers pinned (the cells per level, the least interval margin, the
//     elements used, |S| and |T|, the counts of the first shell and of the blocks, the distance order on every possible
//     (3,3) entry up to d_79); and they FAIL for tau = 0.931, for tau = 0.8555, for S = geogens(3.4), and for S
//     without an element of the first shell;
//   - each condition alone makes each struct fail (both structs);
//   - certify_roots at R = 0, 2.554, 3.327, 3.4, 4, 4.5: the checks pass, its roots and distances are those of
//     roots_info, bit for bit, its line word for word (the counts pinned), no second line; near the shells
//     (R = 2.55337378, 3.3257718, 4.425532) the second line, and the roots that the single-precision test keeps against
//     the exact ball;
//   - the checks fail when they should: u' (it fixes o) put into a store second, in the middle or last; an element left
//     out; an element x replaced by x u' (the same key); a stored distance changed by one float ulp; a store searched to a
//     smaller radius;
//   - B(6): its counts and shells, the elements of B(4.5) \ {1} in geogens(4.5), the line at R = 5.5, and "FAILED"
//     above 6;
//   - the interval operations on random intervals that are NOT points, of either sign, at every corner (and 0 for the
//     square), against the exact results (fma, two-sum);
//   - the interval bound decides: the covering with eps = -1, which tests every cell in intervals, gives the same cells
//     at COVER_TAU and fails at tau = 0.8555;
//   - tau next to the bound: the least double tau with tau^2 > sqrt3/2 (decided in exact integer arithmetic) must
//     not count as below;
//   - the distance order on given ranges, where the float comparison alone decides;
//   - a store whose first element is not 1 (u' first; 1 second); an S of the same size with one element changed; b^-1
//     put into a store (no strictly nearer neighbour, but 16 ties); a float tie at a boundary between shells;
//   - the second line within 5e-7 of d_2, not at 1.1e-6, and at an empty shell;
//   - stats_lines with roots that differ from the checked ones, and zipper3 --stats and link3 --stats themselves;
//   - A COUNT MOD 3, independent of the checks' code: for every shell n = 1..29 of B(6), every c in Z[zeta]^3 with
//     c^* F' c = -1 and |c_3|^2 = alpha_n (by the integral trace form and an exact prefilter), up to units; those whose
//     class mod 3 lies in the orbit of [e_3] under the reduction of Pi must be exactly the points of the elements the
//     search found, shell by shell.
// A few seconds. Uses src/ only.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_roots.h"
#include "geometry/pu21/pu21.h"

#include "render_test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace z = zippers;
namespace cs = zippers::cs;
namespace ce = zippers::cs::cert;
using z::cpx;
using z::M3;
using z::Z;
using z::Zz;
using rtest::check;
using rtest::note;
using rtest::section;

// ---- deterministic random numbers (splitmix64) --------------------------------------------------------------------------

static uint64_t rng_state = 0x5eed2026u;
static uint64_t rnext()
{
    rng_state += 0x9e3779b97f4a7c15ULL;
    uint64_t x = rng_state;
    x = (x ^ (x >> 30))*0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27))*0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static double runif(double lo, double hi) { return lo + (hi - lo)*((double)(rnext() >> 11)*0x1.0p-53); }
static int rbelow(int n) { return (int)(rnext() % (uint64_t)n); }

// a random reduced word of length len in the a_i^{+-1}, as a normalised element
static M3 random_element(int len)
{
    std::vector<cs::Gen> A6 = cs::agens();
    M3 g = z::mnormal(z::mid());
    int last = -1;
    for (int i = 0; i < len; i++) {
        int j;
        do { j = rbelow(6); } while (last >= 0 && (j ^ 1) == last);          // agens: a_i at 2i, A_i at 2i + 1
        g = z::mnormal(z::mmul(g, A6[j].m));
        last = j;
    }
    return g;
}

// ---- Z[sqrt3], written here again ---------------------------------------------------------------------------------------

struct N3 { int64_t m, n; };
static N3 nrm(const Zz& x)                     // x conj(x) = m + n sqrt3 = Z(m, 2n, 0, -n)
{
    Zz y = z::zmul(x, z::zconj(x));
    if (y.c[2] != 0 || y.c[1] != -2*y.c[3]) { printf("FATAL: a norm not in Z[sqrt3]\n"); exit(1); }
    return {y.c[0], -y.c[3]};
}
static int sign3(__int128 p, __int128 q)       // the sign of p + q sqrt3
{
    if (p == 0 && q == 0) return 0;
    if (p >= 0 && q >= 0) return 1;
    if (p <= 0 && q <= 0) return -1;
    return p > 0 ? (p*p > 3*q*q ? 1 : -1) : (3*q*q > p*p ? 1 : -1);
}
static int cmpN(N3 a, N3 b) { return sign3((__int128)a.m - b.m, (__int128)a.n - b.n); }
static int64_t ceil_n_sqrt3(int64_t n) { int64_t m = 0; while (m*m <= 3*n*n) m++; return m; }    // n >= 1
static int64_t shell(N3 q)                     // n with q = alpha_n, or -1
{
    if (q.m == 1 && q.n == 0) return 0;
    return q.n >= 1 && q.m == ceil_n_sqrt3(q.n) ? q.n : -1;
}
static double dshell(int64_t n) { int64_t m = n ? ceil_n_sqrt3(n) : 1; return 2*std::acosh(std::sqrt((double)m + n*std::sqrt(3.0))); }
static Zz zq(int64_t m, int64_t n) { return Z(m, 2*n, 0, -n); }
static int64_t isqrt_(int64_t v) { int64_t s = 0; while ((s + 1)*(s + 1) <= v) s++; return s; }
// floor(2n(3 + sqrt6)) = 6n + floor(sqrt(24 n^2)): the bound on T_2 of an entry of c_12 (24 n^2 is not a square)
static int64_t bound_n(int64_t n) { return 6*n + isqrt_(24*n*n); }

// T_2(x) = |sigma_1 x|^2 + |sigma_5 x|^2, an integer
static int64_t T2(const Zz& x) { const int64_t* c = x.c; return 2*(c[0]*c[0] + c[1]*c[1] + c[2]*c[2] + c[3]*c[3] + c[0]*c[2] + c[1]*c[3]); }

// the key of a point: the least of the 12 unit multiples of the WHOLE column (not the library's normk of c_3)
typedef std::array<int64_t, 12> K12;
static K12 key12(const Zz c[3])
{
    Zz x[3] = {c[0], c[1], c[2]};
    K12 best{};
    for (int k = 0; k < 12; k++) {
        K12 cur;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) cur[4*i + j] = x[i].c[j];
        if (k == 0 || cur < best) best = cur;
        for (int i = 0; i < 3; i++) x[i] = z::zrot(x[i]);
    }
    return best;
}
static K12 key12(const M3& g) { Zz c[3] = {g.e[0][2], g.e[1][2], g.e[2][2]}; return key12(c); }

// ---- an independent reduction mod 3: zeta -> -i (the other root of Phi_12 = x^4 - x^2 + 1 in F_9), and projective
// normal forms with the LAST nonzero entry 1 --------------------------------------------------------------------------------

struct G9 { int a, b; };                       // a + b i
static G9 g9(int64_t a, int64_t b) { return {(int)(((a % 3) + 3) % 3), (int)(((b % 3) + 3) % 3)}; }
static G9 r9(const Zz& x) { return g9(x.c[0] - x.c[2], -x.c[1] + x.c[3]); }   // 1, zeta, zeta^2, zeta^3 -> 1, -i, -1, i
static G9 m9(G9 x, G9 y) { return g9(x.a*y.a - x.b*y.b, x.a*y.b + x.b*y.a); }
static G9 a9(G9 x, G9 y) { return g9(x.a + y.a, x.b + y.b); }
static bool z9(G9 x) { return !x.a && !x.b; }
static G9 inv9(G9 x) { for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) { G9 p = m9(x, {a, b}); if (p.a == 1 && !p.b) return {a, b}; } return {0, 0}; }
template <int L> static std::array<int, L> last1(const G9* v)
{
    int i0 = L - 1;
    while (i0 > 0 && z9(v[i0])) i0--;
    G9 s = inv9(v[i0]);
    std::array<int, L> o;
    for (int i = 0; i < L; i++) { G9 y = m9(v[i], s); o[i] = 3*y.a + y.b; }
    return o;
}
static G9 un9(int x) { return {x/3, x % 3}; }
typedef std::array<int, 9> PM9;
typedef std::array<int, 3> PV9;
static PM9 red_mat(const M3& g) { G9 m[9]; for (int i = 0; i < 9; i++) m[i] = r9(g.e[i/3][i%3]); return last1<9>(m); }
static PM9 mul9(const PM9& A, const PM9& B)
{
    G9 c[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        G9 s{0, 0};
        for (int k = 0; k < 3; k++) s = a9(s, m9(un9(A[3*i + k]), un9(B[3*k + j])));
        c[3*i + j] = s;
    }
    return last1<9>(c);
}
static PV9 act9(const PM9& A, const PV9& x)
{
    G9 w[3];
    for (int i = 0; i < 3; i++) { G9 s{0, 0}; for (int k = 0; k < 3; k++) s = a9(s, m9(un9(A[3*i + k]), un9(x[k]))); w[i] = s; }
    return last1<3>(w);
}
static PV9 cls9(const Zz c[3]) { G9 v[3] = {r9(c[0]), r9(c[1]), r9(c[2])}; return last1<3>(v); }

// ---- exact predicates for the interval tests ----------------------------------------------------------------------------

// is the exact x + y in [lo, hi]? Two-sum: x + y = s + e exactly, and |e| is at most half the spacing at s
static bool sum_in(double x, double y, double lo, double hi)
{
    double s = x + y, bb = s - x, e = (x - (s - bb)) + (y - bb);
    return (s > lo || (s == lo && e >= 0)) && (s < hi || (s == hi && e <= 0));
}
// is the exact x y in [lo, hi]? fma rounds x y - lo once, which keeps its sign
static bool prod_in(double x, double y, double lo, double hi) { return std::fma(x, y, -lo) >= 0 && std::fma(x, y, -hi) <= 0; }
// is the exact x / y in [lo, hi] (y > 0)? x / y >= lo iff x - lo y >= 0
static bool quot_in(double x, double y, double lo, double hi) { return std::fma(-lo, y, x) >= 0 && std::fma(-hi, y, x) <= 0; }
// is the exact sqrt(x) in [lo, hi] (x >= 0)?
static bool sqrt_in(double x, double lo, double hi)
{
    return (lo <= 0 || std::fma(lo, lo, -x) <= 0) && hi >= 0 && std::fma(hi, hi, -x) >= 0;
}
// a random interval: positive, negative, across 0, a few ulps wide, or with an endpoint at 0
static ce::Iv rand_iv(int kind)
{
    double a = runif(-10, 10), b = runif(-10, 10);
    if (kind == 0) { a = runif(0.001, 10); b = runif(0.001, 10); }
    if (kind == 1) { a = -runif(0.001, 10); b = -runif(0.001, 10); }
    if (kind == 2) { a = -runif(0.001, 10); b = runif(0.001, 10); }
    if (kind == 3) { a = b = runif(-10, 10); for (int k = 1 + rbelow(6); k > 0; k--) b = std::nextafter(b, 100.0); }
    if (kind == 4) { a = 0; }
    if (a > b) std::swap(a, b);
    return ce::Iv{a, b};
}

// the sign of 4 t^4 - 3 for a double t in [1/2, 1), exactly: t = m / 2^53, so it is the sign of m^4 - 3 2^210, with
// m^2 = A = A1 2^64 + A0 and A^2 = A1^2 2^128 + 2 A1 A0 2^64 + A0^2 in two 128-bit words
static int sign_4t4_minus_3(double t)
{
    int e;
    double f = std::frexp(t, &e);
    if (e != 0) return e > 0 ? 1 : -1;
    typedef unsigned __int128 U;
    const U M64 = ((U)1 << 64) - 1;
    U m = (U)std::ldexp(f, 53), A = m*m, A1 = A >> 64, A0 = A & M64;
    U hi = A1*A1, mid = 2*A1*A0, lo = A0*A0;
    U L = lo + ((mid & M64) << 64), H = hi + (mid >> 64) + (L < lo ? 1 : 0);
    U HT = (U)3 << 82;                         // 3 2^210 = (3 2^82) 2^128
    if (H != HT) return H > HT ? 1 : -1;
    return L > 0 ? 1 : 0;
}

// run a program; its exit status, and what it wrote to stderr
static int run_program(const std::string& cmd, std::string& err)
{
    err.clear();
    FILE* p = popen((cmd + " 2>&1 >/dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) err.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static std::vector<std::string> roots_lines(const std::string& err)
{
    std::vector<std::string> out;
    size_t a = 0;
    while (a < err.size()) {
        size_t b = err.find('\n', a);
        if (b == std::string::npos) b = err.size();
        if (err.compare(a, 7, "roots: ") == 0) out.push_back(err.substr(a, b - a));
        a = b + 1;
    }
    return out;
}

// ---- copies of a store, with a change -------------------------------------------------------------------------------------

// S with the element g put in at index k, with the stored distance d (the parents after it move up)
static cs::Store with_inserted(const cs::Store& S, size_t k, const M3& g, float d = 0.f)
{
    cs::Store T;
    for (size_t i = 0; i <= S.size(); i++) {
        if (i == k) T.add(g, 0, -1, 1, cs::FVal{0, 0}, d);
        if (i == S.size()) break;
        int p = S.parent[i];
        T.add(S.load(i), p < 0 ? -1 : p + (p >= (int)k ? 1 : 0), S.letter[i], S.depth[i], S.label[i], S.dist[i]);
    }
    return T;
}
// S without the element k (its children then hang from 1)
static cs::Store with_removed(const cs::Store& S, size_t k)
{
    cs::Store T;
    for (size_t i = 0; i < S.size(); i++) {
        if (i == k) continue;
        int p = S.parent[i];
        p = p < 0 ? -1 : p == (int)k ? 0 : p - (p > (int)k ? 1 : 0);
        T.add(S.load(i), p, S.letter[i], S.depth[i], S.label[i], S.dist[i]);
    }
    return T;
}
// S with its elements 0 and 1 exchanged (and the parents with them)
static cs::Store with_swapped01(const cs::Store& S)
{
    cs::Store T;
    auto mp = [](int p) { return p == 0 ? 1 : p == 1 ? 0 : p; };
    for (size_t j = 0; j < S.size(); j++) {
        size_t i = j == 0 ? 1 : j == 1 ? 0 : j;
        T.add(S.load(i), S.parent[i] < 0 ? -1 : mp(S.parent[i]), S.letter[i], S.depth[i], S.label[i], S.dist[i]);
    }
    return T;
}
// S with the element k replaced by g, and its stored distance by d
static cs::Store with_replaced(const cs::Store& S, size_t k, const M3& g, float d)
{
    cs::Store T;
    for (size_t i = 0; i < S.size(); i++)
        T.add(i == k ? g : S.load(i), S.parent[i], S.letter[i], S.depth[i], S.label[i], i == k ? d : S.dist[i]);
    return T;
}

static const char* const TAIL =
    "checked: closed under the 508 elements of S inside that ball, and every element descends to 1 (the "
    "covering: 12531 cells to level 3, least margin 1.76e-04 in interval arithmetic; the first shell and the stabiliser "
    "of o, exactly)";

int main()
{
    double t_start = rtest::clock_seconds();
    cs::setup();
    printf("test_roots_cs: the descent checks of the S^3 root balls (the covering, the first shell, the stabiliser of o)\n");
    const std::vector<cs::Gen> S45 = cs::geogens(4.5);
    const cs::DescentCertificate& D = cs::descent_certificate();
    std::vector<M3> T;
    for (int i : D.T) T.push_back(D.S[i]);

    // ---- the interval arithmetic ----
    section("the interval arithmetic encloses the exact results, strictly (fma, two-sum)");
    {
        long bad = 0, n = 0;
        for (int t = 0; t < 20000; t++) {
            double x = runif(-10, 10), y = runif(-10, 10), yp = runif(0.01, 10), xp = runif(0, 50);
            ce::Iv a = ce::ipt(x), b = ce::ipt(y), bp = ce::ipt(yp), ap = ce::ipt(xp);
            ce::Iv s = a + b, d = a - b, p = a*b, q = a/bp, r = ce::isqrt(ap), sq = ce::isq(a);
            // a + b, a - b: the exact value lies within half an ulp of the rounded one, so strict brackets suffice
            bad += !(s.lo < x + y && x + y < s.hi) + !(d.lo < x - y && x - y < d.hi);
            // x y in [lo, hi] exactly, by the sign of the fused x y - lo (exact up to one rounding, which keeps the sign)
            bad += !(std::fma(x, y, -p.lo) >= 0 && std::fma(x, y, -p.hi) <= 0 && p.lo < x*y && x*y < p.hi);
            bad += !(std::fma(-q.lo, yp, x) >= 0 && std::fma(-q.hi, yp, x) <= 0 && q.lo < x/yp && x/yp < q.hi);
            bad += !(std::fma(r.lo, r.lo, -xp) <= 0 && std::fma(r.hi, r.hi, -xp) >= 0 && r.lo < std::sqrt(xp) && std::sqrt(xp) < r.hi);
            bad += !(std::fma(x, x, -sq.lo) >= 0 && std::fma(x, x, -sq.hi) <= 0 && sq.lo <= x*x && x*x < sq.hi);
            n += 6;
        }
        ce::Iv r3 = ce::isqrt3();
        bad += !(std::fma(r3.lo, r3.lo, -3.0) < 0 && std::fma(r3.hi, r3.hi, -3.0) > 0);
        check(bad == 0, "%ld of %ld interval operations do not enclose the exact result strictly", bad, n);
        note("%ld random operations (+, -, *, /, sqrt, square) and sqrt3: every enclosure contains the exact value, and "
             "strictly brackets the rounded one", n + 1);
    }

    // ---- intervals that are not points ----
    section("the interval arithmetic on intervals that are not points, of either sign: every corner, exactly");
    {
        long bad = 0, n = 0, inverted = 0;
        for (int t = 0; t < 20000; t++) {
            ce::Iv A = rand_iv(t % 5), B = rand_iv((t/5) % 5);
            ce::Iv Bp{runif(0.01, 10), 0}, Ap{runif(0, 50), 0};
            Bp.hi = Bp.lo + (t % 2 ? runif(0, 10) : 0x1p-50); Ap.hi = Ap.lo + (t % 3 ? runif(0, 50) : 0x1p-48);
            if (t % 7 == 0) Ap.lo = 0;
            ce::Iv s_ = A + B, d = A - B, p = A*B, q = A/Bp, sq = ce::isq(A), r = ce::isqrt(Ap), m0 = ce::imax0(A), ng = -A;
            for (const ce::Iv& C : {s_, d, p, q, sq, r, m0, ng}) inverted += !(C.lo <= C.hi);
            const double ax[2] = {A.lo, A.hi}, bx[2] = {B.lo, B.hi}, bp[2] = {Bp.lo, Bp.hi}, ap[2] = {Ap.lo, Ap.hi};
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    bad += !sum_in(ax[i], bx[j], s_.lo, s_.hi) + !sum_in(ax[i], -bx[j], d.lo, d.hi);
                    bad += !prod_in(ax[i], bx[j], p.lo, p.hi) + !quot_in(ax[i], bp[j], q.lo, q.hi);
                    n += 4;
                }
                bad += !prod_in(ax[i], ax[i], sq.lo, sq.hi) + !sqrt_in(ap[i], r.lo, r.hi);
                double mx = std::max(0.0, ax[i]);
                bad += !(m0.lo <= mx && mx <= m0.hi) + !(ng.lo <= -ax[i] && -ax[i] <= ng.hi);
                n += 4;
            }
            if (A.lo < 0 && A.hi > 0) { bad += !(sq.lo <= 0); n++; }                // the square of an interval across 0
        }
        check(bad == 0 && inverted == 0, "%ld of %ld corner values outside their enclosure, %ld inverted intervals", bad, n,
              inverted);
        note("%ld corner values of +, -, *, / (positive denominators), square, sqrt, max(0, .) and - on 20000 random pairs "
             "of intervals (positive, negative, across 0, a few ulps wide, from 0): all inside, exactly", n);
    }

    // ---- the geometry of the checks ----
    section("the ball columns of T against the exact |(g t)_33| / |g_33|; the cell radius rho_C");
    {
        // g^-1 o has ball point tau u, from the third ROW of hat g = T g T^-1 (not minv):
        // hat g_31 = q (g_31 + g_32/(r-1)), hat g_32 = -q g_32/(r-1), tau u = -(conj hat g_31, conj hat g_32)/conj g_33
        const double r = std::sqrt(3.0), q = std::sqrt(r - 1);
        std::vector<ce::BallD> BD;
        std::vector<ce::BallIv> BI;
        for (const M3& t : T) { BD.push_back(ce::ballcol_d(t)); BI.push_back(ce::ballcol_iv(t)); }
        long pairs = 0, agree = 0, disagree = 0, ties = 0, tieclose = 0, outside = 0, descend = 0, words = 0;
        for (int w = 0; w < 120; w++) {
            M3 g = random_element(1 + rbelow(10));
            N3 ng = nrm(g.e[2][2]);
            if (ng.m == 1 && ng.n == 0) continue;
            words++;
            cpx g31 = z::zval(g.e[2][0]), g32 = z::zval(g.e[2][1]), g33 = z::zval(g.e[2][2]);
            cpx h31 = q*(g31 + g32/(r - 1)), h32 = -q*g32/(r - 1);
            cpx z1 = -std::conj(h31)/std::conj(g33), z2 = -std::conj(h32)/std::conj(g33);
            bool any = false;
            for (size_t i = 0; i < T.size(); i++) {
                double f = std::abs(BD[i].a - (BD[i].b1*std::conj(z1) + BD[i].b2*std::conj(z2)));   // |a - <b, tau u>|
                int c = cmpN(nrm(ce::entry33(g, T[i])), ng);
                pairs++;
                if (c < 0) any = true;
                if (c == 0) { ties++; tieclose += std::fabs(f - 1) < 1e-9; continue; }
                if ((f < 1) == (c < 0)) agree++; else disagree++;
            }
            descend += any;
        }
        // the interval ball columns enclose the double ones (to their rounding)
        for (size_t i = 0; i < T.size(); i++) {
            auto in = [](ce::Iv I, double x) { return I.lo - 1e-13 <= x && x <= I.hi + 1e-13; };
            outside += !in(BI[i].a.re, BD[i].a.real()) + !in(BI[i].a.im, BD[i].a.imag()) + !in(BI[i].b1.re, BD[i].b1.real()) +
                       !in(BI[i].b1.im, BD[i].b1.imag()) + !in(BI[i].b2.re, BD[i].b2.real()) + !in(BI[i].b2.im, BD[i].b2.imag()) +
                       !in(BI[i].nb, BD[i].nb);
            outside += !(BI[i].nb.hi - BI[i].nb.lo < 1e-12) + !(std::fabs(std::norm(BD[i].a) - BD[i].nb*BD[i].nb - 1) < 1e-12);
        }
        check(disagree == 0 && tieclose == ties && descend == words, "|(g t)_33| / |g_33| with the ball columns of T: %ld pairs, "
              "%ld disagree, %ld ties (%ld at 1), %ld of %ld words descend", pairs, disagree, ties, tieclose, descend, words);
        check(outside == 0, "%ld ball-column values outside their enclosures (or |a|^2 - |b|^2 != 1)", outside);
        note("|(g t)_33| / |g_33|: %ld words x %zu elements of T; on all %ld pairs without a tie, the exact comparison of "
             "|(g t)_33|^2 with |g_33|^2 and that of |a - <b, tau u>| with 1 agree (%ld ties, all at 1 to 1e-9); every word has a "
             "nearer g t", words, T.size(), agree, ties);
        // the cell radius: points of random cells (a third of them at corners) against u_c and rho_C of cell_iv
        double worst = 0;
        long cells = 0, cbad = 0;
        for (int t = 0; t < 100000; t++) {
            int level = rbelow(8), f = rbelow(8), per = 4 << level;
            double h = 0.25/(1 << level), c[3];
            for (int j = 0; j < 3; j++) c[j] = -1 + (2*rbelow(per) + 1)*h;
            ce::CellIv C = ce::cell_iv(f, c, h);
            double p[4], x[4], nx = 0;
            ce::facet_point(f, c, p);
            int k = 0;
            for (int i = 0; i < 4; i++) {
                if (i == f/2) { x[i] = p[i]; continue; }
                double off = t % 3 == 0 ? (rnext() & 1 ? h : -h) : runif(-h, h);
                x[i] = c[k++] + off;
            }
            for (int i = 0; i < 4; i++) nx += x[i]*x[i];
            nx = std::sqrt(nx);
            double d2 = 0, np = 0;
            for (int i = 0; i < 4; i++) { double u = (C.u[i].lo + C.u[i].hi)/2; d2 += (x[i]/nx - u)*(x[i]/nx - u); np += p[i]*p[i]; }
            for (int i = 0; i < 4; i++) cbad += !(C.u[i].lo <= p[i]/std::sqrt(np) + 1e-15 && p[i]/std::sqrt(np) - 1e-15 <= C.u[i].hi);
            double ratio = std::sqrt(d2)/C.rho.hi;
            worst = std::max(worst, ratio);
            cells++;
        }
        check(worst <= 1 + 1e-9 && cbad == 0, "the cell radius: a point of a cell at %.6f rho_C from its centre (must be <= 1); "
              "%ld centres outside their enclosure", worst, cbad);
        note("the cell radius: %ld points of random cells (levels 0..7, a third at corners): max |u - u_c| / rho_C = %.4f", cells, worst);
    }

    // ---- the exact enumerations, made here again ----
    section("exact enumerations: the units, rho_3 against zeta -> -i, <u', v'> against the blocks, |g_33|^2 on B(6)");
    cs::Store B6;
    long bad6 = 0;
    {
        // the units: the x in Z[zeta] with coefficients in {-1, 0, 1} and x conj(x) = 1, and which of them are powers
        // of zeta
        long units = 0, powers = 0;
        for (int a = -1; a <= 1; a++) for (int b = -1; b <= 1; b++) for (int c = -1; c <= 1; c++) for (int d = -1; d <= 1; d++) {
            Zz x = Z(a, b, c, d);
            if (!z::zeq(z::zmul(x, z::zconj(x)), Z(1, 0, 0, 0))) continue;
            units++;
            Zz y = Z(1, 0, 0, 0);
            for (int k = 0; k < 12; k++) { if (z::zeq(x, y)) { powers++; break; } y = z::zrot(y); }
        }
        check(units == 12 && powers == 12 && D.units == 12 && D.units_not_powers == 0, "units of norm 1: %ld (%ld powers of "
              "zeta); the checks: %zu", units, powers, D.units);
        // the library's rho_3 (zeta -> i) and the reduction here (zeta -> -i) differ by the Frobenius of F_9 (i -> -i):
        // rho_3(x) = conj(r9(x)), on random elements, and both are multiplicative
        long red = 0;
        for (int t = 0; t < 5000; t++) {
            Zz x = Z(rbelow(41) - 20, rbelow(41) - 20, rbelow(41) - 20, rbelow(41) - 20);
            Zz y = Z(rbelow(41) - 20, rbelow(41) - 20, rbelow(41) - 20, rbelow(41) - 20);
            ce::F9 lx = ce::red3(x);
            G9 mx = r9(x);
            red += !(lx.a == mx.a && lx.b == (3 - mx.b) % 3);
            red += !ce::feq(ce::red3(z::zmul(x, y)), ce::fmul(ce::red3(x), ce::red3(y)));
            G9 p = r9(z::zmul(x, y)), pp = m9(r9(x), r9(y));
            red += !(p.a == pp.a && p.b == pp.b);
        }
        check(red == 0 && D.reduction_ok && ce::reduction_is_homomorphism(), "rho_3: %ld disagreements with zeta -> -i, or "
              "not multiplicative", red);
        // the reduction of Pi here: its order and the orbit of [e_3], as in the checks
        std::vector<PM9> gens;
        for (int i = 0; i < 3; i++) { gens.push_back(red_mat(cs::A[i])); gens.push_back(red_mat(z::minv(cs::A[i]))); }
        std::set<PM9> G = {red_mat(z::mid())};
        std::vector<PM9> q(G.begin(), G.end());
        for (size_t i = 0; i < q.size(); i++) for (const PM9& s : gens) { PM9 x = mul9(q[i], s); if (G.insert(x).second) q.push_back(x); }
        std::set<PV9> O;
        for (const PM9& g : G) O.insert(act9(g, PV9{0, 0, 1}));
        check(G.size() == 21 && O.size() == 21 && D.G == 21 && D.O0 == 21, "the reduction of Pi: %zu elements, the orbit of "
              "[e_3] %zu points (the checks: %zu, %zu)", G.size(), O.size(), D.G, D.O0);
        // K = <u', v'> (closure by right multiplication, normalised, by whole matrices), against the blocks of check (c)
        std::set<std::array<int64_t, 36>> seen;
        std::vector<M3> Kel = {z::mnormal(z::mid())};
        auto flat = [](const M3& g) { std::array<int64_t, 36> a; for (int i = 0; i < 36; i++) a[i] = g.e[i/12][(i/4) % 3].c[i % 4]; return a; };
        seen.insert(flat(Kel[0]));
        for (size_t i = 0; i < Kel.size(); i++)
            for (const M3* s : {&cs::U, &cs::V}) { M3 x = z::mnormal(z::mmul(Kel[i], *s)); if (seen.insert(flat(x)).second) Kel.push_back(x); }
        long fixo = 0, triv = 0;
        for (const M3& k : Kel) {
            fixo += z::zzero(k.e[0][2]) && z::zzero(k.e[1][2]) && z::zzero(k.e[2][0]) && z::zzero(k.e[2][1]);
            triv += red_mat(k) == red_mat(z::mid());
        }
        check(Kel.size() == 288 && fixo == 288 && D.stab == 288 && triv == 3 && D.stab_trivial == 3 && D.stab_trivial_det0 == 1,
              "<u', v'>: %zu elements, %ld fixing o, %ld with trivial reduction (the checks: %zu blocks, %zu trivial, %zu "
              "with det in <zeta^3>)", Kel.size(), fixo, triv, D.stab, D.stab_trivial, D.stab_trivial_det0);
        note("12 units, all powers of zeta; rho_3 = the conjugate of zeta -> -i on 5000 random elements, multiplicative; "
             "the reduction of Pi of order %zu, the orbit of [e_3] %zu points; <u', v'> = %zu elements fixing o, %ld of "
             "them trivial mod 3", G.size(), O.size(), Kel.size(), triv);
        // |g_33|^2 on B(6), the ball of the search with S = geogens(4.5) through 6
        cs::guard(6.0);
        bad6 = cs::enumerate(B6, S45, 6.0);
        long notadm = 0;
        for (size_t i = 0; i < B6.size(); i++) notadm += shell(nrm(B6.load(i).e[2][2])) < 0;
        check(B6.size() == 10327 && bad6 == 0 && notadm == 0, "B(6): %zu elements, %ld f-inconsistencies, %ld with |g_33|^2 "
              "not 1 or alpha_n", B6.size(), bad6, notadm);
        note("|g_33|^2 on B(6): for each of the %zu elements it is 1 or ceil(n sqrt3) + n sqrt3", B6.size());
    }

    // ---- the descent checks ----
    section("the descent checks (once per run): they pass, and their numbers");
    {
        std::string lv;
        for (long x : D.per_level) lv += " " + std::to_string(x);
        check(D.passes() && D.failures().empty(), "the descent checks fail:%s", D.failures().c_str());
        // pinned
        check(D.S.size() == 508 && D.T.size() == 506 && D.not_unitary == 0 && D.inverse_clashes == 0 && D.coef_max == 4 &&
              D.form_ok, "S: %zu, T: %zu, %zu not unitary, %zu clashes, largest coefficient %lld", D.S.size(), D.T.size(),
              D.not_unitary, D.inverse_clashes, (long long)D.coef_max);
        check(D.tau == 0.9306 && D.tau_below && D.tau_gap > 9.0e-6 && D.tau_gap < 9.1e-6, "tau = %.9g, below %d, gap %.3e",
              D.tau, D.tau_below, D.tau_gap);
        check(D.cells == 12531 && D.cells_failed == 0 && D.deepest == 3 && lv == " 16 2760 9651 104" && D.used == 469 &&
              D.visited == 14248 && D.volume == 64, "the covering: %ld cells (%ld failed, %ld visited), deepest %d, per "
              "level%s, %zu used, volume %.17g", D.cells, D.cells_failed, D.visited, D.deepest, lv.c_str(), D.used, D.volume);
        check(D.least_margin > 1.75e-4 && D.least_margin < 1.77e-4, "the least interval margin %.6e (1.76e-4)", D.least_margin);
        check(D.shell_vectors == 864 && D.shell_points == 72 && D.shell_in_O0 == 18 && D.shell_in_T == 18, "the first shell: "
              "%zu vectors, %zu points, %zu in O_0, %zu keys of T", D.shell_vectors, D.shell_points, D.shell_in_O0, D.shell_in_T);
        check(D.stab == 288 && D.stab_trivial == 3 && D.stab_trivial_det0 == 1 && D.stab_identity && D.det_ok, "the "
              "stabiliser of o: %zu blocks, %zu trivial, %zu with det in <zeta^3>", D.stab, D.stab_trivial, D.stab_trivial_det0);
        check(D.dist_values == 912 && D.dist_shells == 45 && D.dist_order_breaks == 0 && D.dist_gap > 0.0138,
              "the distance order: %zu values in %zu shells, %zu breaks, least gap %.6f", D.dist_values, D.dist_shells,
              D.dist_order_breaks, D.dist_gap);
        note("S = geogens(4.5): %zu elements, T = S n S^-1: %zu, all F'-unitary, largest coefficient of a third column %lld",
             D.S.size(), D.T.size(), (long long)D.coef_max);
        note("the covering at tau = %.4f (tau^2 < sqrt3/2 by %.3e): %ld cells (per level%s), deepest level %d, %zu elements "
             "used, least interval margin %.4e, volume %g", D.tau, D.tau_gap, D.cells, lv.c_str(), D.deepest, D.used,
             D.least_margin, D.volume);
        note("the first shell: %zu vectors, %zu points, %zu with class in O_0, all of them keys of T; Stab(o): %zu blocks, "
             "%zu trivial mod 3, %zu with det in <zeta^3> (the identity)", D.shell_vectors, D.shell_points, D.shell_in_O0,
             D.stab, D.stab_trivial, D.stab_trivial_det0);
        note("the distance order: %zu possible (3,3) entries x (x conj(x) = 1 or alpha_n, n <= %d) in %zu values; hdist "
             "and its float increase strictly, least gap %.4f; %.3f s of cpu", D.dist_values, cs::ORDER_SHELLS, D.dist_shells,
             D.dist_gap, D.seconds);
    }

    section("the descent checks fail when they should: tau too large or too small, S too small, a shell element out");
    {
        double t0 = rtest::clock_seconds();
        cs::DescentCertificate a = cs::certify_descent(S45, 0.931);
        check(!a.passes() && !a.tau_below && a.failures().find("tau^2 is not below sqrt3/2") != std::string::npos,
              "tau = 0.931 (tau^2 above sqrt3/2): %s", a.passes() ? "passes" : a.failures().c_str());
        cs::DescentCertificate b = cs::certify_descent(S45, 0.8555);
        check(!b.passes() && b.tau_below && b.cells_failed > 0, "tau = 0.8555: "
              "%ld cells failed", b.cells_failed);
        cs::DescentCertificate g34 = cs::certify_descent(cs::geogens(3.4), cs::COVER_TAU);
        check(!g34.passes() && g34.cells_failed > 0 && g34.S.size() == 54, "S = geogens(3.4) (%zu elements): %ld cells "
              "failed", g34.S.size(), g34.cells_failed);
        std::vector<cs::Gen> S1(S45.begin() + 1, S45.end());           // S[0] lies in the first shell
        cs::DescentCertificate e = cs::certify_descent(S1, cs::COVER_TAU);
        check(!e.passes() && e.shell_in_O0 == 18 && e.shell_in_T == 16 && e.covering_ok(), "S without its first element: "
              "%zu of %zu shell points are keys of T, covering %s", e.shell_in_T, e.shell_in_O0, e.covering_ok() ? "ok" : "fails");
        note("tau = 0.931: \"%s\"", a.failures().c_str());
        note("tau = 0.8555: %ld cells passed before the first failure; S = geogens(3.4): %ld; S without S[0]: \"%s\" "
             "(%.2f s)", b.cells, g34.cells, e.failures().c_str(), rtest::clock_seconds() - t0);
    }

    // ---- the interval bound decides; tau next to the bound; the distance order on given ranges
    section("the interval bound decides: the covering with eps = -1, every cell tested in intervals");
    {
        ce::Covering a = ce::cover(T, cs::COVER_TAU, cs::COVER_LEVELS, -1.0);
        std::string lv;
        for (int d = 0; d <= a.deepest; d++) lv += " " + std::to_string(a.per_level[d]);
        check(a.failed == 0 && a.cells == 12531 && a.deepest == 3 && lv == " 16 2760 9651 104" && a.volume == 64 &&
              a.least > 1.75e-4 && a.least < 1.77e-4, "eps = -1 at COVER_TAU: %ld cells (%ld failed), deepest %d, per "
              "level%s, volume %g, least bound %.4e", a.cells, a.failed, a.deepest, lv.c_str(), a.volume, a.least);
        ce::Covering b = ce::cover(T, 0.8555, cs::COVER_LEVELS, -1.0);
        check(b.failed > 0, "eps = -1 at tau = 0.8555: the covering does not fail (%ld cells, least "
              "bound %.4e)", b.cells, b.least);
        note("eps = -1: %ld cells at COVER_TAU, the same tree, least bound %.4e; at tau = 0.8555 it fails (%ld cells first)",
             a.cells, a.least, b.cells);
    }

    section("tau next to the bound: the least double tau with tau^2 > sqrt3/2 is not below");
    {
        // the least double tau with tau^2 > sqrt3/2, i.e. 4 tau^4 > 3, decided exactly in integers
        double tau = std::sqrt(std::sqrt(3.0)/2);
        while (sign_4t4_minus_3(tau) < 0) tau = std::nextafter(tau, 1.0);
        while (sign_4t4_minus_3(std::nextafter(tau, 0.0)) > 0) tau = std::nextafter(tau, 0.0);
        check(sign_4t4_minus_3(tau) > 0 && sign_4t4_minus_3(std::nextafter(tau, 0.0)) < 0 && sign_4t4_minus_3(0.9306) < 0,
              "the exact test of 4 tau^4 > 3");
        cs::DescentCertificate c = cs::certify_descent(S45, tau);
        check(!c.tau_below && !c.passes(), "tau = %a (tau^2 > sqrt3/2 exactly) is said to be below the bound", tau);
        note("tau = %.17g = %a, the least double with 4 tau^4 > 3: not below (the covering there: %ld cells)", tau, tau, c.cells);
    }

    section("the distance order, on given ranges: the double and the float each decide alone");
    {
        // n -> {least hdist, largest hdist, least float, largest float}
        const double x = 1.0000000001, y = 1.0000000002;         // above 1 in double, 1 in float
        struct Case { std::map<int64_t, std::array<double, 4>> r; size_t breaks; const char* what; };
        const Case cases[4] = {
            {{{1, {1.0, 1.1, (float)1.0, (float)1.1}}, {2, {2.0, 2.1, (float)2.0, (float)2.1}}}, 0, "increasing"},
            {{{1, {1.0, x, (float)1.0, (float)x}}, {2, {y, 1.1, (float)y, (float)1.1}}}, 1, "the doubles increase, the floats tie"},
            {{{1, {1.0, 2.0, (float)1.0, (float)2.0}}, {2, {1.5, 2.5, (float)1.5, (float)2.5}}}, 1, "the doubles overlap"},
            {{{0, {0.0, 0.0, 0.0, 0.0}}, {1, {1.0, x, (float)1.0, (float)x}}, {5, {y, 3.0, (float)y, 3.0}}}, 1, "three values"}};
        for (const Case& k : cases) {
            cs::DescentCertificate c;
            cs::order_ranges(k.r, c);
            check(c.dist_order_breaks == k.breaks && c.dist_shells == k.r.size(), "%s: %zu breaks (expected %zu)", k.what,
                  c.dist_order_breaks, k.breaks);
        }
        note("four sets of ranges, among them one where only the float comparison breaks the order");
    }

    section("each failure alone makes each struct fail");
    {
        int k = 0;
        auto alone = [&](const char* what, const std::function<void(cs::DescentCertificate&)>& f) {
            cs::DescentCertificate c = D;
            f(c);
            check(!c.passes() && !c.failures().empty(), "descent: %s alone leaves the checks passing", what);
            k++;
        };
        alone("form_ok", [](cs::DescentCertificate& c) { c.form_ok = false; });
        alone("S empty", [](cs::DescentCertificate& c) { c.S.clear(); });
        alone("not_unitary", [](cs::DescentCertificate& c) { c.not_unitary = 1; });
        alone("T empty", [](cs::DescentCertificate& c) { c.T.clear(); });
        alone("inverse_clashes", [](cs::DescentCertificate& c) { c.inverse_clashes = 1; });
        alone("tau_below", [](cs::DescentCertificate& c) { c.tau_below = false; });
        alone("coef_max", [](cs::DescentCertificate& c) { c.coef_max = 101; });
        alone("cells", [](cs::DescentCertificate& c) { c.cells = 0; });
        alone("cells_failed", [](cs::DescentCertificate& c) { c.cells_failed = 1; });
        alone("least_margin", [](cs::DescentCertificate& c) { c.least_margin = 0; });
        alone("volume", [](cs::DescentCertificate& c) { c.volume = 64 - 0.125; });
        alone("units", [](cs::DescentCertificate& c) { c.units = 11; });
        alone("units_not_powers", [](cs::DescentCertificate& c) { c.units_not_powers = 1; });
        alone("reduction_ok", [](cs::DescentCertificate& c) { c.reduction_ok = false; });
        alone("G", [](cs::DescentCertificate& c) { c.G = 0; c.O0 = 0; });
        alone("G != O0", [](cs::DescentCertificate& c) { c.O0 = 20; });
        alone("shell_points", [](cs::DescentCertificate& c) { c.shell_points = 0; });
        alone("shell_in_T", [](cs::DescentCertificate& c) { c.shell_in_T = 17; });
        alone("stab", [](cs::DescentCertificate& c) { c.stab = 0; });
        alone("stab_trivial_det0", [](cs::DescentCertificate& c) { c.stab_trivial_det0 = 2; });
        alone("stab_identity", [](cs::DescentCertificate& c) { c.stab_identity = false; });
        alone("det_ok", [](cs::DescentCertificate& c) { c.det_ok = false; });
        alone("dist_values", [](cs::DescentCertificate& c) { c.dist_values = 0; });
        alone("dist_order_breaks", [](cs::DescentCertificate& c) { c.dist_order_breaks = 1; });
        cs::RootCertificate ok;
        check(ok.passes(), "a root struct with no failure does not pass");
        const char* what[10] = {"in_range", "descent", "same_S", "identity_first", "not_closed", "key_clashes",
                                "order_breaks", "dist_wrong", "label_inconsistencies", "no_descent"};
        const char* says[10] = {"is above", "the descent checks fail", "not the S of the descent",
                                "does not start at the identity", "not found", "the key of another element",
                                "not increasing in |g_33|^2", "stored distances", "f inconsistent", "strictly nearer"};
        for (int j = 0; j < 10; j++) {
            cs::RootCertificate c;
            c.rho = 7;
            if (j == 0) c.in_range = false;
            if (j == 1) c.descent = false;
            if (j == 2) c.same_S = false;
            if (j == 3) c.identity_first = false;
            if (j == 4) c.not_closed = 1;
            if (j == 5) c.key_clashes = 1;
            if (j == 6) c.order_breaks = 1;
            if (j == 7) c.dist_wrong = 1;
            if (j == 8) c.label_inconsistencies = 1;
            if (j == 9) c.no_descent = 1;
            std::string line = cs::root_certificate_line(c);
            check(!c.passes() && line.find("FAILED") != std::string::npos && line.find(says[j]) != std::string::npos,
                  "roots: %s alone: passes %d, line \"%s\"", what[j], c.passes(), line.c_str());
        }
        note("%d conditions of the descent checks and 10 of the store checks, each alone, make them fail", k);
    }

    // ---- certify_roots against roots_info, and the line ----
    section("certify_roots: the roots of roots_info, the checks pass, and the line of zipper3 --stats");
    {
        struct Case { double R; const char* head; };
        const Case cases[6] = {
            {0, "roots: 1 elements of Pi in B(0), of 1 searched in B(0.5); "},
            {2.554, "roots: 19 elements of Pi in B(2.554), of 19 searched in B(3.054); "},
            {3.327, "roots: 55 elements of Pi in B(3.327), of 79 searched in B(3.827); "},
            {3.4, "roots: 55 elements of Pi in B(3.4), of 79 searched in B(3.9); "},
            {4.0, "roots: 331 elements of Pi in B(4), of 511 searched in B(4.5); "},
            {4.5, "roots: 511 elements of Pi in B(4.5), of 1447 searched in B(5); "}};
        for (const Case& k : cases) {
            cs::RootCertificate c = cs::certify_roots(k.R);
            std::vector<cs::RootInfo> ri = cs::roots_info(k.R);
            bool same = c.rootv.size() == ri.size() && c.rootd.size() == ri.size();
            for (size_t i = 0; same && i < ri.size(); i++) same = z::meq(c.rootv[i], ri[i].g) && rtest::same_bytes(c.rootd[i], ri[i].d);
            check(same, "R = %g: the roots differ from roots_info's", k.R);
            check(c.passes() && c.no_descent == 0 && c.label_inconsistencies == 0, "R = %g: the checks fail", k.R);
            std::string line = cs::root_certificate_line(c);
            check(line == std::string(k.head) + TAIL, "R = %g: the line is \"%s\"", k.R, line.c_str());
            check(cs::root_margin_line(c).empty(), "R = %g: a margin line", k.R);
            note("R = %g: %s", k.R, line.substr(0, line.find(';')).c_str());
            if (k.R > 0) note("        R is %.3g from the nearest shell distance, d_%lld = %.6f", c.margin, (long long)c.nearest_n, c.nearest);
        }
        note("each line ends: %s", TAIL);
    }

    section("R near a shell distance: the second line, and the single-precision test against the exact ball");
    {
        // float(d_1) = d_1 + 7.7e-8, float(d_2) = d_2 + 2.7e-8, float(d_6) = d_6 - 1.8e-7
        struct Case { double R; size_t roots, exact; int64_t n; const char* why; };
        const Case cases[3] = {{2.55337378, 1, 19, 1, "above d_1, below its float: in the exact ball, not roots"},
                               {3.3257718, 19, 55, 2, "above d_2, below its float: in the exact ball, not roots"},
                               {4.425532, 511, 331, 6, "below d_6, above its float: out of the exact ball, roots"}};
        for (const Case& k : cases) {
            cs::RootCertificate c = cs::certify_roots(k.R);
            size_t exact = 0;
            for (size_t i = 0; i < B6.size(); i++) { int64_t n = shell(nrm(B6.load(i).e[2][2])); exact += dshell(n) <= k.R; }
            std::string m = cs::root_margin_line(c);
            check(c.passes() && c.roots == k.roots && exact == k.exact && c.nearest_n == k.n && !m.empty(), "R = %.8g: %zu "
                  "roots, %zu in the exact ball (pinned %zu, %zu), shell %lld, margin line %s", k.R, c.roots, exact, k.roots,
                  k.exact, (long long)c.nearest_n, m.empty() ? "missing" : "present");
            note("R = %.8g (%s): %zu roots, %zu in the exact ball", k.R, k.why, c.roots, exact);
            if (k.n == 6) note("%s", m.c_str());
        }
    }

    section("the second line: within 5e-7 of d_2, not at 1.1e-6, and at an empty shell");
    {
        struct Case { double R; bool line; int64_t n; size_t count; };
        const Case cases[3] = {{dshell(2) + 5e-7, true, 2, 36}, {dshell(2) + 1.1e-6, false, 2, 36}, {dshell(5) + 5e-7, true, 5, 0}};
        for (const Case& k : cases) {
            cs::RootCertificate c = cs::certify_roots(k.R);
            std::string m = cs::root_margin_line(c);
            bool ok = k.line ? !m.empty() && c.nearest_n == k.n && c.nearest_count == k.count : m.empty();
            check(c.passes() && ok, "R = %.10g: margin %.3g, shell %lld, second line %s", k.R, c.margin,
                  (long long)c.nearest_n, m.empty() ? "absent" : m.c_str());
            if (k.line && k.n == 5) note("an empty shell: %s", m.c_str());
        }
    }

    // ---- the checks fail when they should ----
    section("the checks fail when they should: u' put in, an element left out, x -> x u', a distance changed, a pruned store");
    {
        cs::Store S;
        cs::enumerate(S, S45, 3.9);
        cs::RootCertificate c0;
        c0.label_inconsistencies = 0;
        cs::certify_store(S, S45, 3.9, 3.4, c0);
        check(c0.passes() && S.size() == 79, "B(3.9) (%zu elements): the checks fail", S.size());
        // u' fixes o (its third column is e_3) and is not 1; it has no nearer g t
        M3 u = z::mnormal(cs::U);
        check(z::zzero(u.e[0][2]) && z::zzero(u.e[1][2]) && !z::meq(u, z::mnormal(z::mid())), "u' does not fix o, or is 1");
        for (size_t k : {(size_t)1, S.size()/2, S.size()}) {
            cs::Store X = with_inserted(S, k, u);
            cs::RootCertificate c;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            std::string line = cs::root_certificate_line(c);
            // pinned: u' takes 30 points of S o to points of Pi o (key clashes) and 48 to points outside it
            check(!c.passes() && c.no_descent == 1 && c.key_clashes == 30 && c.not_closed == 48 &&
                  line.find("1 elements without a strictly nearer") != std::string::npos,
                  "u' at %zu of %zu: %zu without descent, %zu clashes, %zu not closed (pinned 1, 30, 48)", k, X.size(),
                  c.no_descent, c.key_clashes, c.not_closed);
            if (k == S.size()) note("u' last: %s", line.c_str());
        }
        const size_t gone[3] = {30, 14, 11};          // pinned: the S-steps that reach the element left out
        int gi = 0;
        for (size_t k : {(size_t)1, S.size()/2, S.size() - 1}) {
            cs::Store X = with_removed(S, k);
            cs::RootCertificate c;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            std::string line = cs::root_certificate_line(c);
            check(!c.passes() && c.not_closed == gone[gi] && c.key_clashes == 0 && c.no_descent == 0 &&
                  line.find("S-neighbours in B(3.9) not found") != std::string::npos,
                  "element %zu left out: %zu not closed (pinned %zu)", k, c.not_closed, gone[gi]);
            gi++;
            if (k == S.size() - 1) note("the last element left out: %s", line.c_str());
        }
        // x -> x u': the same key (x u' o = x o), another matrix: the parent's step finds it, a key clash
        const size_t clash[2] = {38, 13}, open_[2] = {23, 6};   // pinned
        int ci = 0;
        for (size_t k : {(size_t)5, S.size() - 1}) {
            M3 x = z::mnormal(z::mmul(S.load(k), cs::U));
            cs::Store X = with_replaced(S, k, x, S.dist[k]);
            cs::RootCertificate c;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            std::string line = cs::root_certificate_line(c);
            check(!c.passes() && c.key_clashes == clash[ci] && c.not_closed == open_[ci] &&
                  line.find("with the key of another element") != std::string::npos,
                  "element %zu replaced by x u': %zu key clashes, %zu not closed (pinned %zu, %zu)", k, c.key_clashes,
                  c.not_closed, clash[ci], open_[ci]);
            ci++;
            if (k == 5) note("element 5 replaced by x u': %s", line.c_str());
        }
        // one stored distance changed by a float ulp: that alone
        {
            size_t k = S.size()/3;
            cs::Store X = with_replaced(S, k, S.load(k), std::nextafter(S.dist[k], 10.f));
            cs::RootCertificate c;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            check(!c.passes() && c.dist_wrong == 1 && c.order_breaks == 0 && c.not_closed == 0 && c.key_clashes == 0 &&
                  c.no_descent == 0, "a changed distance: %zu wrong, %zu breaks", c.dist_wrong, c.order_breaks);
        }
        // a store searched through 3.7, checked as one through 3.9: the shell at d_3 = 3.7556 is missing
        {
            cs::Store X;
            cs::enumerate(X, S45, 3.7);
            cs::RootCertificate c;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            check(!c.passes() && c.not_closed == 191 && X.size() == 55, "a store of B(3.7) (%zu) as B(3.9): %zu not closed "
                  "(pinned 191)", X.size(), c.not_closed);
        }
        // the search's S must be the descent checks'
        {
            std::vector<cs::Gen> S1(S45.begin(), S45.end() - 1);
            cs::RootCertificate c;
            cs::certify_store(S, S1, 3.9, 3.4, c);
            check(!c.passes() && !c.same_S, "a store checked with another S");
        }
        // An S of the same size with one element changed (S[0], at d_1, replaced by an
        // element of Pi at d_9): that alone fails
        {
            cs::Store B5;
            cs::enumerate(B5, S45, 5.0);
            std::vector<cs::Gen> S2 = S45;
            S2[0].m = B5.load(600);
            cs::RootCertificate c;
            cs::certify_store(S, S2, 3.9, 3.4, c);
            check(!c.passes() && !c.same_S && c.descent && c.not_closed == 0 && c.key_clashes == 0 && c.no_descent == 0,
                  "an S of the same size with S[0] changed: same_S %d, %zu not closed, %zu clashes", c.same_S, c.not_closed,
                  c.key_clashes);
        }
        // a store whose first element is not 1: u' first, or 1 second (elements 0 and 1 exchanged)
        {
            cs::Store X = with_inserted(S, 0, u), Y = with_swapped01(S);
            cs::RootCertificate c, d;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            cs::certify_store(Y, S45, 3.9, 3.4, d);
            std::string lc = cs::root_certificate_line(c), ld = cs::root_certificate_line(d);
            check(!c.passes() && !c.identity_first && lc.find("does not start at the identity") != std::string::npos,
                  "u' first: identity_first %d: %s", c.identity_first, lc.c_str());
            // pinned: with 1 second the only other failure is 1 itself, an element without a nearer neighbour
            check(!d.passes() && !d.identity_first && d.no_descent == 1 && d.not_closed == 0 && d.key_clashes == 0 &&
                  ld.find("does not start at the identity") != std::string::npos, "1 second: identity_first %d, %zu without "
                  "descent, %zu not closed, %zu clashes", d.identity_first, d.no_descent, d.not_closed, d.key_clashes);
            note("1 second: %s", ld.c_str());
        }
        // b^-1 put in, with its own distance: no g t is strictly nearer to o than b^-1, but 16 are as near (ties): the
        // consistency check must be strict
        {
            M3 x = z::mnormal(z::minv(cs::B));
            N3 nx = nrm(x.e[2][2]);
            long less = 0, ties = 0;
            for (const M3& t : T) { int k = cmpN(nrm(ce::entry33(x, t)), nx); less += k < 0; ties += k == 0; }
            check(nx.m == 2 && nx.n == 1 && less == 0 && ties == 16, "b^-1: |x_33|^2 = %lld + %lld sqrt3, %ld nearer, %ld ties",
                  (long long)nx.m, (long long)nx.n, less, ties);
            for (size_t k : {(size_t)1, S.size()/2, S.size()}) {
                cs::Store X = with_inserted(S, k, x, (float)z::hdist(x.e[2][2]));
                cs::RootCertificate c;
                cs::certify_store(X, S45, 3.9, 3.4, c);
                check(!c.passes() && c.no_descent == 1 && c.dist_wrong == 0 && c.order_breaks == 0, "b^-1 at %zu: %zu "
                      "without descent, %zu wrong distances, %zu order breaks", k, c.no_descent, c.dist_wrong, c.order_breaks);
            }
            note("b^-1: no t in T with |(b^-1 t)_33|^2 < 2 + sqrt3, %ld ties; put into a store it is the one element "
                 "without a strictly nearer neighbour", ties);
        }
        // a float tie at the boundary between shells 1 and 2 (the first element of shell 2, in the order of the store,
        // given the stored distance of the last of shell 1): an order break, besides the wrong distance
        {
            long last1 = -1, first2 = -1;
            for (size_t i = 0; i < S.size(); i++) {
                int64_t n = shell(nrm(S.load(i).e[2][2]));
                if (n == 1) last1 = (long)i;
                if (n == 2 && first2 < 0) first2 = (long)i;
            }
            cs::Store X = with_replaced(S, (size_t)first2, S.load((size_t)first2), S.dist[last1]);
            cs::RootCertificate c;
            cs::certify_store(X, S45, 3.9, 3.4, c);
            check(!c.passes() && c.order_breaks == 1 && c.dist_wrong == 1 && c.not_closed == 0 && c.no_descent == 0,
                  "a float tie between shells 1 and 2: %zu order breaks, %zu wrong distances", c.order_breaks, c.dist_wrong);
        }
    }

    // ---- B(6) ----
    section("B(6): the counts, the shells, the elements of geogens(4.5), the line at R = 5.5 and above 6");
    {
        std::map<int64_t, long> sh;
        for (size_t i = 0; i < B6.size(); i++) sh[shell(nrm(B6.load(i).e[2][2]))]++;
        std::string h;
        for (auto& kv : sh) h += " " + std::to_string(kv.first) + ":" + std::to_string(kv.second);
        check(h == " 0:1 1:18 2:36 3:24 4:252 6:180 8:144 9:792 11:36 12:504 14:504 15:468 16:504 18:144 20:1008 23:2592 "
                   "24:384 25:504 28:2232", "the shells of B(6):%s", h.c_str());
        const double rs[7] = {3, 3.5, 4, 4.5, 5, 5.5, 6};
        const long counts[7] = {19, 55, 331, 511, 1447, 3463, 10327};
        std::string cnt;
        for (int i = 0; i < 7; i++) {
            long n = 0;
            for (size_t j = 0; j < B6.size(); j++) n += B6.dist[j] <= rs[i];
            check(n == counts[i], "|Pi n B(%g)| = %ld (pinned %ld)", rs[i], n, counts[i]);
            cnt += " " + std::to_string(n);
        }
        // the elements of B(4.5) \ {1} in geogens(4.5), and the others (pinned below)
        std::set<z::PU21::Key> sk;
        for (const auto& g : S45) sk.insert(z::PU21::key(g.m));
        long in = 0, ball = 0;
        std::string miss;
        for (size_t i = 1; i < B6.size(); i++) {
            if (!(B6.dist[i] <= 4.5)) continue;
            ball++;
            if (sk.count(B6.key_of(i))) { in++; continue; }
            char buf[120];
            snprintf(buf, sizeof buf, " d = %.6f f = (%d, %d) inverse %s;", B6.dist[i], B6.label[i].f0, B6.label[i].f1,
                     sk.count(z::PU21::key(z::mnormal(z::minv(B6.load(i))))) ? "in S" : "NOT in S");
            miss += buf;
        }
        check(ball == 510 && in == 508 && S45.size() == 508 &&
              miss == " d = 4.425532 f = (1, -3) inverse in S; d = 4.425532 f = (0, 0) inverse in S;",
              "B(4.5) \\ {1}: %ld elements, %ld in geogens(4.5) (%zu); missing:%s", ball, in, S45.size(), miss.c_str());
        // the checks of B(6): R = 5.5 (they pass), and as a search through 6.01 (R = 5.51: above MAX_SEARCH_RADIUS)
        cs::RootCertificate c, d;
        c.label_inconsistencies = d.label_inconsistencies = bad6;
        cs::certify_store(B6, S45, 6.0, 5.5, c);
        cs::certify_store(B6, S45, 6.01, 5.51, d);
        std::string lc = cs::root_certificate_line(c), ld = cs::root_certificate_line(d);
        check(c.passes() && lc == std::string("roots: 3463 elements of Pi in B(5.5), of 10327 searched in B(6); ") + TAIL,
              "R = 5.5: %s", lc.c_str());
        check(!d.passes() && ld == "roots: 3463 elements of Pi in B(5.51), of 10327 searched in B(6.01); FAILED: "
              "the search radius 6.01 is above 6", "R = 5.51: %s", ld.c_str());
        note("the shells n:count of B(6):%s", h.c_str());
        note("|Pi n B(r)| at r = 3, 3.5, 4, 4.5, 5, 5.5, 6:%s", cnt.c_str());
        note("geogens(4.5): %ld of the %ld elements of B(4.5) \\ {1}; missing:%s", in, ball, miss.c_str());
        note("%s", ld.c_str());
    }

    // ---- the lines of --stats, and the programs ----
    section("stats_lines: roots that differ from the checked ones are reported; zipper3 --stats and link3 --stats");
    {
        const std::string L2554 = std::string("roots: 19 elements of Pi in B(2.554), of 19 searched in B(3.054); ") + TAIL;
        std::vector<M3> g;
        std::vector<double> d;
        for (const cs::RootInfo& x : cs::roots_info(2.554)) { g.push_back(x.g); d.push_back(x.d); }
        std::vector<std::string> a = cs::stats_lines(2.554, g, d, "the roots drawn", "build drew");
        check(a.size() == 1 && a[0] == L2554, "the roots of roots_info: %zu lines", a.size());
        std::vector<M3> g1(g.begin(), g.end() - 1);
        std::vector<double> d1(d.begin(), d.end() - 1), d2 = d;
        d2[5] = std::nextafter(d2[5], 10.0);
        std::vector<std::string> b = cs::stats_lines(2.554, g1, d1, "the roots drawn", "build drew");
        std::vector<std::string> e = cs::stats_lines(2.554, g, d2, "the roots drawn", "build drew");
        std::string f = cs::stats_lines(2.554, g1, d1, "the translates of roots_info", "roots_info").back();
        check(b.size() == 2 && b[1] == "roots: NOT the roots drawn: the checked search kept 19, build drew 18 (or others)" &&
              e.size() == 2 && e[1] == "roots: NOT the roots drawn: the checked search kept 19, build drew 19 (or others)" &&
              f == "roots: NOT the translates of roots_info: the checked search kept 19, roots_info 18 (or others)",
              "a root left out, or a distance changed, is not reported: %zu, %zu lines", b.size(), e.size());
#if defined(ZIPPERS_ZIPPER3) && defined(ZIPPERS_LINK3) && defined(ZIPPERS_REPO)
        // the programs themselves, on the substitution file of the references, drawn lightly (the roots do not depend on it)
        const std::string file = std::string("'") + ZIPPERS_REPO + "/legacy/s3/subst_4.00_9.50_t3.txt'";
        const std::string z3 = std::string("'") + ZIPPERS_ZIPPER3 + "' -f " + file + " -a 0.2 --cap 20000 --stats";
        struct Run { std::string cmd; std::vector<std::string> lines; };
        // (the near-tie run catches a zipper3 that prints only its first line, which the other runs, printing one line
        // each, would let pass)
        const Run runs[4] = {
            {z3 + " -R 2.554", {L2554}},
            {z3 + " -R 2.55337378",
             {std::string("roots: 1 elements of Pi in B(2.55337378), of 19 searched in B(3.05337378); ") + TAIL,
              "roots: R is within 3.4e-08 of d_1 = 2.553373737 (in double or in single precision), the distance of a shell "
              "(18 elements of Pi in it): whether they are roots is decided by the single-precision test of "
              "roots_info, not by the exact ball"}},
            {z3 + " -R 5.6", {"roots: 3607 elements of Pi in B(5.6), of 12199 searched in B(6.1); FAILED: the search "
                              "radius 6.1 is above 6"}},
            {std::string("'") + ZIPPERS_LINK3 + "' -f " + file + " --hr 3.4 --stats --hinfo",
             {std::string("roots: 55 elements of Pi in B(3.4), of 79 searched in B(3.9); ") + TAIL}}};
        for (const Run& r : runs) {
            std::string err;
            int st = run_program(r.cmd, err);
            std::vector<std::string> got = roots_lines(err);
            check(st == 0 && got == r.lines, "%s: exit %d, %zu lines starting \"roots: \"%s%s", r.cmd.c_str(), st, got.size(),
                  got.empty() ? "" : ", the first: ", got.empty() ? "" : got[0].c_str());
        }
        note("zipper3 --stats at R = 2.554, 2.55337378 (a near-tie: two lines) and 5.6 (search radius 6.1), and link3 "
             "--stats --hinfo at --hr 3.4: their lines");
#else
        note("zipper3 and link3 not built in this configuration: their runs are left out");
#endif
    }

    // ---- the count mod 3 ----
    section("the count mod 3: the lattice points of each shell of B(6) against the search");
    {
        double t0 = rtest::clock_seconds();
        // 0.317 < sqrt3 - sqrt2, exactly: (317 + 1000 sqrt2)^2 < 3e6 <=> 634000 sqrt2 < 899511 <=> 2 634000^2 < 899511^2
        check((__int128)2*634000*634000 < (__int128)899511*899511, "0.317 is not below sqrt3 - sqrt2");
        // the orbit of [e_3] under the reduction here (zeta -> -i)
        std::vector<PM9> gens;
        for (int i = 0; i < 3; i++) { gens.push_back(red_mat(cs::A[i])); gens.push_back(red_mat(z::minv(cs::A[i]))); }
        std::set<PM9> G = {red_mat(z::mid())};
        std::vector<PM9> q(G.begin(), G.end());
        for (size_t i = 0; i < q.size(); i++) for (const PM9& s : gens) { PM9 x = mul9(q[i], s); if (G.insert(x).second) q.push_back(x); }
        std::set<PV9> O;
        for (const PM9& g : G) O.insert(act9(g, PV9{0, 0, 1}));
        // the points of the search, by shell (keys here: the least unit multiple of the whole column)
        std::map<int64_t, std::set<K12>> found;
        long outside = 0;
        for (size_t i = 1; i < B6.size(); i++) {
            M3 g = B6.load(i);
            found[shell(nrm(g.e[2][2]))].insert(key12(g));
            Zz c[3] = {g.e[0][2], g.e[1][2], g.e[2][2]};
            outside += !O.count(cls9(c));                                   // the reduction mod 3
        }
        // every x with T_2(x) <= the largest bound, 6n + floor(2n sqrt6) at n = 29, with its norm
        const int64_t NMAX = 29;
        int64_t Bmax = bound_n(NMAX);
        struct Cand { Zz x; int64_t t2; N3 nx; };
        std::vector<Cand> all;
        int64_t L = 0;
        while (3*(L + 1)*(L + 1) <= 2*Bmax) L++;
        for (int64_t a = -L; a <= L; a++) for (int64_t b = -L; b <= L; b++) for (int64_t c = -L; c <= L; c++) for (int64_t d = -L; d <= L; d++) {
            Zz x = Z(a, b, c, d);
            int64_t t = T2(x);
            if (t <= Bmax) all.push_back({x, t, nrm(x)});
        }
        const Zz rp1 = Z(1, 2, 0, -1), rm1 = Z(-1, 2, 0, -1);           // r + 1, r - 1
        long tot = 0, totO = 0, totPi = 0, differ = 0;
        std::string per;
        for (int64_t n = 1; n <= NMAX; n++) {
            int64_t m = ceil_n_sqrt3(n);
            // the candidates: c_3 with T_2 = 2m and c_3 conj(c_3) = alpha_n; c_1, c_2 (h_P(c_12, c_12) = beta =
            // (m - 1) + n sqrt3) with T_2(c_i) <= bound_n(n), 0.317 |sigma_1 c_i|^2 <= sigma_1(beta) and
            // 0.317 |sigma_5 c_i|^2 <= -sigma_5(beta), exact conditions on c_i conj(c_i) = p + q sqrt3
            int64_t bound = bound_n(n);
            std::vector<Zz> c3;
            std::vector<Zz> cx, cxc;
            std::vector<Zz> Ax, By;
            for (const Cand& k : all) {
                if (k.t2 == 2*m && k.nx.m == m && k.nx.n == n) c3.push_back(k.x);
                if (k.t2 > bound) continue;
                if (sign3(317*(__int128)k.nx.m - 1000*(__int128)(m - 1), 317*(__int128)k.nx.n - 1000*(__int128)n) > 0) continue;
                if (sign3(317*(__int128)k.nx.m + 1000*(__int128)(m - 1), -317*(__int128)k.nx.n - 1000*(__int128)n) > 0) continue;
                cx.push_back(k.x);
                cxc.push_back(z::zconj(k.x));
                Ax.push_back(z::zmul(rp1, zq(k.nx.m, k.nx.n)));
                By.push_back(z::zmul(rm1, zq(k.nx.m, k.nx.n)));
            }
            // h_P(c_12, c_12) = (r+1) N(c_1) - w - conj(w) + (r-1) N(c_2), w = conj(c_1) c_2
            const Zz beta = zq(m - 1, n);
            std::set<K12> pts, ptsO;
            long nvec = 0;
            for (size_t i = 0; i < cx.size(); i++) for (size_t j = 0; j < cx.size(); j++) {
                Zz w = z::zmul(cxc[i], cx[j]);
                if (!z::zeq(z::zadd(w, z::zconj(w)), z::zsub(z::zadd(Ax[i], By[j]), beta))) continue;
                for (const Zz& z3 : c3) {
                    Zz col[3] = {cx[i], cx[j], z3};
                    nvec++;
                    K12 k = key12(col);
                    if (pts.insert(k).second && O.count(cls9(col))) ptsO.insert(k);
                }
            }
            const std::set<K12>& F = found[n];
            size_t inF = 0;
            for (const K12& k : F) inF += ptsO.count(k);
            bool same = inF == F.size() && F.size() == ptsO.size() && nvec == 12*(long)pts.size();
            differ += !same;
            tot += pts.size(); totO += ptsO.size(); totPi += F.size();
            per += " " + std::to_string(n) + ":" + std::to_string(pts.size()) + "/" + std::to_string(ptsO.size());
            if (!same) note("shell %lld: %zu lattice points, %zu with class in O_0, %zu found by the search, %zu of them among "
                            "those (%ld vectors)", (long long)n, pts.size(), ptsO.size(), F.size(), inF, nvec);
        }
        check(differ == 0 && outside == 0, "%ld shells where the lattice points with class in O_0 are not the search's; %ld "
              "elements of the search with their class outside O_0", differ, outside);
        // pinned
        check(tot == 30936 && totO == 10326 && totPi == 10326, "totals: %ld lattice points, %ld in O_0, %ld found", tot, totO, totPi);
        check(per == " 1:72/18 2:96/36 3:72/24 4:672/252 5:0/0 6:720/180 7:0/0 8:576/144 9:1872/792 10:0/0 11:144/36 "
                     "12:1512/504 13:0/0 14:1344/504 15:1872/468 16:1344/504 17:0/0 18:576/144 19:0/0 20:4032/1008 21:0/0 "
                     "22:0/0 23:6912/2592 24:1152/384 25:2016/504 26:0/0 27:0/0 28:5952/2232 29:0/0",
              "per shell (lattice points / with class in O_0):%s", per.c_str());
        note("%ld lattice points in the 29 shells of B(6), %ld with class in O_0, the same points as the %ld elements of "
             "Pi n B(6) \\ {1} the search found, shell by shell (%.2f s)", tot, totO, totPi, rtest::clock_seconds() - t0);
        note("per shell n: points / with class in O_0:%s", per.c_str());
    }

    rtest::end_section();
    printf("test_roots_cs: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
