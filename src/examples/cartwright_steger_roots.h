// SPDX-License-Identifier: GPL-3.0-or-later
//
// cartwright_steger_roots.h: exact checks on the root balls of the Cartwright-Steger surface group Pi, which zipper3
// --stats and link3 --stats print. They change nothing in the searches of cartwright_steger.h. namespace zippers::cs.
//
// With S = geogens(4.5) and T = S n S^-1:
//   certify_descent(S, tau), descent_certificate()   once per run (they do not depend on R):
//     - the form F': P (its upper block) Hermitian with det P = 1 and tr P = 2r, F'_33 = -1, F'^-1 F' = I (exact);
//     - every element of S is F'-unitary (exact); T = S n S^-1, by the key and then the whole matrix;
//     (a) THE COVERING of a grid of S^3 (the 8 facets of [-1,1]^4, cut into 4^3 cubes, octree refinement to level 8: the
//         grid, below): each cell must pass the cell test (margin_iv) for some t in T, that is lie in the region
//         R_t(tau) = {u in S^3 : |a_t - tau <b_t, u>| < 1} ((b_t, a_t) is the third column of t in ball coordinates),
//         tested IN INTERVAL ARITHMETIC: round to nearest, then one ulp outward after every operation,
//         zeta = sqrt3/2 + i/2, and no libm function but sqrt (correctly rounded) and nextafter (exact). The element of
//         T is chosen in double (zval); only the interval lower bound of its margin decides. tau^2 < sqrt3/2 is tested in
//         the same arithmetic. The cells that pass must tile the 8 facets: their volume in facet coordinates must be 64,
//         exactly (the cubes are dyadic);
//     (b) THE FIRST SHELL, exactly: every c in Z[zeta]^3 with c^* F' c = -1 and |c_3|^2 = 2 + sqrt3, enumerated through
//         the integral trace form T_2(x) = |sigma_1 x|^2 + |sigma_5 x|^2 (integer bounds, no floating point); the
//         reduction rho_3: Z[zeta] -> F_9 (zeta -> i), tested to be a ring homomorphism; G = rho_3(Pi) in PGL(3, F_9)
//         and the orbit O_0 of [e_3]; every such c whose class mod 3 lies in O_0 must be the key of an element of T;
//     (c) THE STABILISER of o, exactly: the x in Z[zeta] with x conj(x) = 1 must be powers of zeta; the blocks A with
//         A^* P A = P are enumerated; G must act freely on O_0 (|G| = |O_0|); det a_i = zeta^k with k = 0 mod 3; and the
//         only block with trivial reduction mod 3 and determinant zeta^k, k = 0 mod 3, must be the identity;
//     (d) THE DISTANCE ORDER: hdist (double) and its float must strictly increase with the exact x conj(x), over every
//         x in Z[zeta] with x conj(x) = 1 or alpha_n, n <= ORDER_SHELLS (alpha: below).
//   certify_store(B, S, rho, R, c), certify_roots(R)   on each search:
//     - the search radius rho <= MAX_SEARCH_RADIUS (above it the line says "FAILED");
//     - the search's S is the S of the descent checks, and those checks pass;
//     - the identity first; the CLOSURE under S inside B_c(rho) = {g : hdist(g_33) <= rho}, by the key and then the
//       whole matrix (as enumerate computes the distance); no key clashes; the distance order on the store (at each
//       boundary between two shells, the double and the stored float of one pair increase with the exact |g_33|^2; (d)
//       compares the extremes) and the stored distances those of the elements; enumerate's f-label count;
//     - every element != 1 has a strictly nearer g t, t in T (exact).
//   certify_roots(R) makes the search of roots_info(R) again (the same calls), checks it, and keeps the roots as
//   roots_info keeps them, so that a program can compare them with its own, bit for bit.
//   root_certificate_line, root_margin_line: the lines of zipper3 --stats (and link3 --stats); the second only when R
//   is within 1e-6 of a shell distance d_n (or of its float), where the single-precision test of roots_info, not the
//   exact ball, decides whether the shell's elements are roots.
//   stats_lines(R, roots, rootd, ...): all the lines of --stats, with the bit-for-bit comparison of the checked roots
//   and those the program uses (here rather than in zipper3 and link3, so that a test can give it roots that differ).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "examples/cartwright_steger.h"
#include "geometry/pu21/pu21.h"

namespace zippers {
namespace cs {

inline constexpr double ROOT_GEOGENS = 4.5;    // S = geogens(4.5): roots_info, find_roots, shell_radii
inline constexpr double COVER_TAU = 0.9306;    // the tau of the covering (check (a) tests tau^2 < sqrt3/2)
inline constexpr int COVER_LEVELS = 8;         // the deepest octree level of the grid
inline constexpr double COVER_EPS = 1e-6;      // the double margin at which a cell is tested in intervals
inline constexpr long COVER_BUDGET = 2000000;  // cells visited at most
inline constexpr double MAX_SEARCH_RADIUS = 6.0;   // the largest search radius rho that certify_store accepts
inline constexpr int ORDER_SHELLS = 79;        // the shells of check (d): alpha_n, n <= 79 (d_79 = 6.997)

namespace cert {

// ---------------------------------------------------------------------------------------
// Z[sqrt3], exactly: m + n sqrt3

struct Q3 { int64_t m, n; };

// x conj(x) = m + n sqrt3; it is fixed by conjugation, so it is Z(m, 2n, 0, -n) (sqrt3 = 2 zeta - zeta^3). False if not.
inline bool norm3(const Zz& x, Q3& q)
{
    Zz y = zmul(x, zconj(x));
    q.m = y.c[0];
    q.n = -y.c[3];
    return y.c[2] == 0 && y.c[1] == 2*q.n;
}

// the sign of p + q sqrt3
inline int sgn3(__int128 p, __int128 q)
{
    if (p >= 0 && q >= 0) return (p != 0 || q != 0) ? 1 : 0;
    if (p <= 0 && q <= 0) return -1;
    __int128 a = p*p, b = 3*q*q;             // a != b: sqrt3 is irrational
    if (p > 0) return a > b ? 1 : -1;
    return b > a ? 1 : -1;
}
inline int cmp3(const Q3& x, const Q3& y) { return sgn3((__int128)x.m - y.m, (__int128)x.n - y.n); }

inline int64_t isqrt64(int64_t v)              // floor(sqrt(v)) for v >= 0, exactly
{
    int64_t r = (int64_t)std::sqrt((double)v);
    while (r > 0 && r*r > v) r--;
    while ((r + 1)*(r + 1) <= v) r++;
    return r;
}

// alpha_n = ceil(n sqrt3) + n sqrt3 (n >= 1), alpha_0 = 1: the shell values of |g_33|^2 that the checks use. 3n^2 is
// not a square.
inline Q3 alpha(int64_t n) { return n == 0 ? Q3{1, 0} : Q3{isqrt64(3*n*n) + 1, n}; }

// is x one of the alpha_n? Then n, else -1
inline int64_t shell_of(const Q3& x)
{
    if (x.m == 1 && x.n == 0) return 0;
    if (x.n < 1) return -1;
    return x.m == alpha(x.n).m ? x.n : -1;
}

// d_n = 2 acosh sqrt(alpha_n), in double (for the margin line only)
inline double shell_distance(int64_t n)
{
    Q3 a = alpha(n);
    return 2*std::acosh(std::sqrt((double)a.m + (double)a.n*std::sqrt(3.0)));
}

// ---------------------------------------------------------------------------------------
// The integral trace form T_2(x) = |sigma_1 x|^2 + |sigma_5 x|^2 = 2(a^2 + b^2 + c^2 + d^2 + ac + bd), x = Z(a, b, c, d)

inline int64_t trace2(const Zz& x)
{
    const int64_t* c = x.c;
    return 2*(c[0]*c[0] + c[1]*c[1] + c[2]*c[2] + c[3]*c[3] + c[0]*c[2] + c[1]*c[3]);
}

// every x with T_2(x) <= bound. Since a^2 + c^2 + ac >= (3/4) max(a^2, c^2), T_2 >= (3/2) max coefficient^2.
inline std::vector<Zz> trace_ball(int64_t bound)
{
    int64_t L = 0;
    while (3*(L + 1)*(L + 1) <= 2*bound) L++;
    std::vector<Zz> out;
    for (int64_t a = -L; a <= L; a++) for (int64_t b = -L; b <= L; b++)
    for (int64_t c = -L; c <= L; c++) for (int64_t d = -L; d <= L; d++) {
        Zz x = Z(a, b, c, d);
        if (trace2(x) <= bound) out.push_back(x);
    }
    return out;
}

// the bound on T_2 of each entry of c_12 that the enumerations use when sigma_1(beta) - sigma_5(beta) = 2n sqrt3
// (beta = c_12^* P c_12): 6n + floor(sqrt(24 n^2)), the floor of 2n(3 + sqrt6) (24 n^2 is not a square)
inline int64_t column_bound(int64_t n) { return 6*n + isqrt64(24*n*n); }

// h_P(x, y) = sum_{i,j < 2} conj(x_i) F'_ij y_j, exactly
inline Zz hP(const Zz x[2], const Zz y[2])
{
    Zz s = Z(0, 0, 0, 0);
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) s = zadd(s, zmul(zmul(zconj(x[i]), FP.e[i][j]), y[j]));
    return s;
}

// the key that PU21::key gives a matrix with this third column: the column times the zeta^k of normk(c_3)
inline PU21::Key colkey(const Zz c[3])
{
    int k = normk(c[2]);
    PU21::Key key;
    for (int i = 0; i < 3; i++) { Zz y = zrotk(c[i], k); for (int j = 0; j < 4; j++) key[4*i + j] = y.c[j]; }
    return key;
}

inline int unit_power(const Zz& x)             // the k with x = zeta^k, or -1
{
    Zz y = Z(1, 0, 0, 0);
    for (int k = 0; k < 12; k++) { if (zeq(x, y)) return k; y = zrot(y); }
    return -1;
}

inline Zz zdet(const M3& g)
{
    const Zz (*e)[3] = g.e;
    Zz t1 = zmul(e[0][0], zsub(zmul(e[1][1], e[2][2]), zmul(e[1][2], e[2][1])));
    Zz t2 = zmul(e[0][1], zsub(zmul(e[1][0], e[2][2]), zmul(e[1][2], e[2][0])));
    Zz t3 = zmul(e[0][2], zsub(zmul(e[1][0], e[2][1]), zmul(e[1][1], e[2][0])));
    return zadd(zsub(t1, t2), t3);
}

// (g t)_33 from the third row of g and the third column of t
inline Zz entry33(const M3& g, const M3& t)
{
    return zadd(zadd(zmul(g.e[2][0], t.e[0][2]), zmul(g.e[2][1], t.e[1][2])), zmul(g.e[2][2], t.e[2][2]));
}

// ---------------------------------------------------------------------------------------
// F_9 = F_3[i] and PGL(3, F_9). rho_3: Z[zeta] -> F_9, zeta -> i (CKY's r_3; Phi_12(i) = i^4 - i^2 + 1 = 3 = 0)

struct F9 { int a, b; };                       // a + b i, a and b in {0, 1, 2}

inline F9 f9(int64_t a, int64_t b) { return F9{(int)(((a % 3) + 3) % 3), (int)(((b % 3) + 3) % 3)}; }
inline F9 red3(const Zz& x) { return f9(x.c[0] - x.c[2], x.c[1] - x.c[3]); }     // 1, zeta, zeta^2, zeta^3 -> 1, i, -1, -i
inline F9 fadd(F9 x, F9 y) { return f9(x.a + y.a, x.b + y.b); }
inline F9 fmul(F9 x, F9 y) { return f9(x.a*y.a - x.b*y.b, x.a*y.b + x.b*y.a); }
inline bool feq(F9 x, F9 y) { return x.a == y.a && x.b == y.b; }
inline bool fzero(F9 x) { return x.a == 0 && x.b == 0; }
inline F9 finv(F9 x)
{
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) if (feq(fmul(x, F9{a, b}), F9{1, 0})) return F9{a, b};
    return F9{0, 0};
}

typedef std::array<int, 9> PM;                 // an element of PGL(3, F_9): entries a + 3b, the first nonzero one 1
typedef std::array<int, 3> PV;                 // a point of P^2(F_9), likewise

template <int L> inline std::array<int, L> pnormal(const F9* v)
{
    F9 s{0, 0};
    for (int i = 0; i < L && fzero(s); i++) s = v[i];
    F9 si = finv(s);
    std::array<int, L> p;
    for (int i = 0; i < L; i++) { F9 y = fmul(v[i], si); p[i] = y.a + 3*y.b; }
    return p;
}
inline F9 unpack9(int x) { return F9{x % 3, x / 3}; }
inline PM pm_mul(const PM& A, const PM& B)
{
    F9 c[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        F9 s{0, 0};
        for (int k = 0; k < 3; k++) s = fadd(s, fmul(unpack9(A[3*i + k]), unpack9(B[3*k + j])));
        c[3*i + j] = s;
    }
    return pnormal<9>(c);
}
inline PV pm_act(const PM& A, const PV& x)
{
    F9 w[3];
    for (int i = 0; i < 3; i++) {
        F9 s{0, 0};
        for (int k = 0; k < 3; k++) s = fadd(s, fmul(unpack9(A[3*i + k]), unpack9(x[k])));
        w[i] = s;
    }
    return pnormal<3>(w);
}
inline PM rho3(const M3& g)
{
    F9 m[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[3*i + j] = red3(g.e[i][j]);
    return pnormal<9>(m);
}
inline PV class3(const Zz c[3])                // the class mod 3 of a column (never 0 when c^* F' c = -1)
{
    F9 v[3] = {red3(c[0]), red3(c[1]), red3(c[2])};
    return pnormal<3>(v);
}

// rho_3 is a ring homomorphism: 1 -> 1, rho_3(xy) = rho_3(x) rho_3(y) on the basis 1, zeta, zeta^2, zeta^3 (it is
// additive as written, so this is all of it), and Phi_12(rho_3(zeta)) = 0
inline bool reduction_is_homomorphism()
{
    Zz e[4] = {Z(1, 0, 0, 0), Z(0, 1, 0, 0), Z(0, 0, 1, 0), Z(0, 0, 0, 1)};
    bool ok = feq(red3(e[0]), F9{1, 0});
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) ok = ok && feq(red3(zmul(e[i], e[j])), fmul(red3(e[i]), red3(e[j])));
    F9 z = red3(e[1]), z2 = fmul(z, z);
    return ok && fzero(fadd(fadd(fmul(z2, z2), fmul(F9{2, 0}, z2)), F9{1, 0}));
}

// ---------------------------------------------------------------------------------------
// Interval arithmetic: round to nearest, then one ulp outward. Every arithmetic result goes through dn or up before it
// is used again, so no product meets an addition (no contraction into a fused multiply-add can occur). No libm function
// but sqrt (IEEE 754: correctly rounded) and nextafter (exact).

struct Iv { double lo, hi; };

inline double dn(double x) { return std::nextafter(x, -std::numeric_limits<double>::infinity()); }
inline double up(double x) { return std::nextafter(x, std::numeric_limits<double>::infinity()); }
inline Iv ipt(double x) { return Iv{x, x}; }
inline Iv iall() { return Iv{-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()}; }
inline Iv operator+(Iv a, Iv b) { return Iv{dn(a.lo + b.lo), up(a.hi + b.hi)}; }
inline Iv operator-(Iv a, Iv b) { return Iv{dn(a.lo - b.hi), up(a.hi - b.lo)}; }
inline Iv operator-(Iv a) { return Iv{-a.hi, -a.lo}; }
inline Iv operator*(Iv a, Iv b)
{
    double p0 = a.lo*b.lo, p1 = a.lo*b.hi, p2 = a.hi*b.lo, p3 = a.hi*b.hi;
    return Iv{dn(std::min(std::min(p0, p1), std::min(p2, p3))), up(std::max(std::max(p0, p1), std::max(p2, p3)))};
}
inline Iv operator/(Iv a, Iv b)                // only positive denominators occur here; otherwise the whole line
{
    if (!(b.lo > 0)) return iall();
    double p0 = a.lo/b.lo, p1 = a.lo/b.hi, p2 = a.hi/b.lo, p3 = a.hi/b.hi;
    return Iv{dn(std::min(std::min(p0, p1), std::min(p2, p3))), up(std::max(std::max(p0, p1), std::max(p2, p3)))};
}
inline Iv isq(Iv a)
{
    if (a.lo >= 0) return Iv{dn(a.lo*a.lo), up(a.hi*a.hi)};
    if (a.hi <= 0) return Iv{dn(a.hi*a.hi), up(a.lo*a.lo)};
    return Iv{0.0, up(std::max(a.lo*a.lo, a.hi*a.hi))};
}
inline Iv isqrt(Iv a)
{
    if (!(a.hi >= 0)) return iall();
    return Iv{a.lo <= 0 ? 0.0 : dn(std::sqrt(a.lo)), up(std::sqrt(a.hi))};
}
inline Iv imax0(Iv a) { return Iv{std::max(0.0, a.lo), std::max(0.0, a.hi)}; }

struct CIv { Iv re, im; };
inline CIv operator+(CIv x, CIv y) { return CIv{x.re + y.re, x.im + y.im}; }
inline CIv operator-(CIv x, CIv y) { return CIv{x.re - y.re, x.im - y.im}; }
inline CIv operator*(CIv x, CIv y) { return CIv{x.re*y.re - x.im*y.im, x.re*y.im + x.im*y.re}; }
inline CIv operator*(Iv r, CIv x) { return CIv{r*x.re, r*x.im}; }
inline CIv operator/(CIv x, Iv r) { return CIv{x.re/r, x.im/r}; }
inline CIv iconj(CIv x) { return CIv{x.re, -x.im}; }
inline Iv iabs2(CIv x) { return isq(x.re) + isq(x.im); }

inline Iv isqrt3() { double s = std::sqrt(3.0); return Iv{dn(s), up(s)}; }

// sigma_1(Z(a, b, c, d)) with zeta = sqrt3/2 + i/2, zeta^2 = 1/2 + i sqrt3/2, zeta^3 = i: (a + c/2 + b sqrt3/2) +
// i (b/2 + d + c sqrt3/2), enclosed. The coefficients must be exact as doubles (|c| < 2^53; the checks ask <= 100).
inline CIv sigma1_iv(const Zz& x)
{
    const Iv h = isqrt3()*ipt(0.5), half = ipt(0.5);
    Iv a = ipt((double)x.c[0]), b = ipt((double)x.c[1]), c = ipt((double)x.c[2]), d = ipt((double)x.c[3]);
    return CIv{a + half*c + h*b, half*b + d + h*c};
}

// the third column (b1, b2, a) of t in ball coordinates (T c = (c_1, c_1 + (1 - r) c_2, sqrt(r - 1) c_3)),
// and |b|: enclosed, and in double (the choice of the element of a cell)
struct BallIv { CIv a, b1, b2; Iv nb; };
inline BallIv ballcol_iv(const M3& t)
{
    Iv r3 = isqrt3(), q = isqrt(r3 - ipt(1.0));
    CIv c0 = sigma1_iv(t.e[0][2]), c1 = sigma1_iv(t.e[1][2]), c2 = sigma1_iv(t.e[2][2]);
    BallIv B;
    B.a = c2;
    B.b1 = c0/q;
    B.b2 = (c0 + (ipt(1.0) - r3)*c1)/q;
    B.nb = isqrt(iabs2(B.b1) + iabs2(B.b2));
    return B;
}
struct BallD { cpx a, b1, b2; double nb; };
inline BallD ballcol_d(const M3& t)
{
    const double r3 = std::sqrt(3.0), q = std::sqrt(r3 - 1);
    cpx c0 = zval(t.e[0][2]), c1 = zval(t.e[1][2]), c2 = zval(t.e[2][2]);
    BallD B;
    B.a = c2;
    B.b1 = c0/q;
    B.b2 = (c0 + (1 - r3)*c1)/q;
    B.nb = std::sqrt(std::norm(B.b1) + std::norm(B.b2));
    return B;
}

// ---------------------------------------------------------------------------------------
// The grid. S^3 is the radial image of the boundary of [-1,1]^4: on facet f (0..7) coordinate f/2 is
// +1 (f even) or -1 (f odd), and the other three, c[0..2], are free in [-1,1]. A cell is a cube of a facet, with centre
// c and half-side h (dyadic, so exact); its points of S^3 are p/|p|, p in the cube. The cell test gives the cell the centre
// u_c = p_c/|p_c| and the radius rho_C = sqrt3 h / m_C, with m_C^2 = 1 + sum_j max(0, |c_j| - h)^2.

inline void facet_point(int f, const double c[3], double p[4])
{
    int k = 0;
    for (int i = 0; i < 4; i++) p[i] = (i == f/2) ? (f % 2 ? -1.0 : 1.0) : c[k++];
}

struct CellIv { Iv u[4]; Iv rho; };            // u_c and rho_C, enclosed
inline CellIv cell_iv(int f, const double c[3], double h)
{
    double p[4];
    facet_point(f, c, p);
    CellIv C;
    Iv n2 = ipt(1.0), m2 = ipt(1.0);
    for (int j = 0; j < 3; j++) {
        n2 = n2 + isq(ipt(c[j]));
        m2 = m2 + isq(imax0(ipt(std::fabs(c[j])) - ipt(h)));
    }
    Iv n = isqrt(n2);
    for (int i = 0; i < 4; i++) C.u[i] = ipt(p[i])/n;
    C.rho = (isqrt3()*ipt(h))/isqrt(m2);
    return C;
}

// the margin 1 - (|a - tau <b, u_c>| + tau |b| rho_C) of t on the cell, enclosed: the cell passes the test when its
// lower bound is > 0. <b, u> = b1 conj(u1) + b2 conj(u2), u1 = u_0 + i u_1, u2 = u_2 + i u_3.
inline Iv margin_iv(const BallIv& t, double tau, const CellIv& C)
{
    CIv u1{C.u[0], C.u[1]}, u2{C.u[2], C.u[3]};
    CIv w = t.b1*iconj(u1) + t.b2*iconj(u2);
    Iv T = ipt(tau);
    return ipt(1.0) - (isqrt(iabs2(t.a - T*w)) + T*t.nb*C.rho);
}

// the same in double, for the choice of t
inline void cell_d(int f, const double c[3], double h, cpx& u1, cpx& u2, double& rho)
{
    double p[4];
    facet_point(f, c, p);
    double n = std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2] + p[3]*p[3]), m2 = 1;
    for (int j = 0; j < 3; j++) { double t = std::max(0.0, std::fabs(c[j]) - h); m2 += t*t; }
    u1 = cpx(p[0]/n, p[1]/n);
    u2 = cpx(p[2]/n, p[3]/n);
    rho = std::sqrt(3.0)*h/std::sqrt(m2);
}
inline double margin_d(const BallD& t, double tau, cpx u1, cpx u2, double rho)
{
    cpx w = t.b1*std::conj(u1) + t.b2*std::conj(u2);
    return 1 - (std::abs(t.a - tau*w) + tau*t.nb*rho);
}

// The covering: a cell passes with the element of T of largest double margin if that margin is > eps and its
// interval lower bound is > 0; otherwise it is cut into 8, down to maxlevel. It stops at the first cell that
// fails at maxlevel (the covering then fails), or after budget cells.
struct Covering {
    long cells = 0, failed = 0, visited = 0;
    int deepest = 0;
    double least = std::numeric_limits<double>::infinity();   // the least interval lower bound of a passing cell
    double volume = 0;                                         // sum of (2h)^3 over the passing cells (64 if they tile)
    std::vector<long> per_level, used;                         // passing cells per level, and per element of T
};

struct CoverRun {
    const std::vector<BallD>& D;
    const std::vector<BallIv>& I;
    double tau, eps;
    int maxlevel;
    long budget;
    Covering& out;
    bool stop = false;

    CoverRun(const std::vector<BallD>& D_, const std::vector<BallIv>& I_, double tau_, double eps_, int maxlevel_,
             long budget_, Covering& out_)
        : D(D_), I(I_), tau(tau_), eps(eps_), maxlevel(maxlevel_), budget(budget_), out(out_) {}

    void cell(int f, const double c[3], double h, int level)
    {
        if (stop) return;
        if (++out.visited > budget) { out.failed++; stop = true; return; }
        cpx u1, u2;
        double rho;
        cell_d(f, c, h, u1, u2, rho);
        int best = -1;
        double bm = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < D.size(); i++) {
            double m = margin_d(D[i], tau, u1, u2, rho);
            if (m > bm) { bm = m; best = (int)i; }
        }
        if (best >= 0 && bm > eps) {
            Iv m = margin_iv(I[best], tau, cell_iv(f, c, h));
            if (m.lo > 0) {
                out.cells++;
                out.used[best]++;
                out.per_level[level]++;
                out.deepest = std::max(out.deepest, level);
                out.least = std::min(out.least, m.lo);
                out.volume += 8*h*h*h;
                return;
            }
        }
        if (level >= maxlevel) { out.failed++; stop = true; return; }
        double hh = h/2;
        for (int a = 0; a < 8; a++) {
            double cc[3] = {c[0] + (a & 1 ? hh : -hh), c[1] + (a & 2 ? hh : -hh), c[2] + (a & 4 ? hh : -hh)};
            cell(f, cc, hh, level + 1);
        }
    }
};

inline Covering cover(const std::vector<M3>& T, double tau, int maxlevel = COVER_LEVELS, double eps = COVER_EPS,
                      long budget = COVER_BUDGET)
{
    std::vector<BallD> D;
    std::vector<BallIv> I;
    for (const M3& t : T) { D.push_back(ballcol_d(t)); I.push_back(ballcol_iv(t)); }
    Covering out;
    out.per_level.assign(maxlevel + 1, 0);
    out.used.assign(T.size(), 0);
    CoverRun run(D, I, tau, eps, maxlevel, budget, out);
    const int N0 = 4;
    const double h = 1.0/N0;
    for (int f = 0; f < 8; f++)
        for (int i = 0; i < N0; i++) for (int j = 0; j < N0; j++) for (int k = 0; k < N0; k++) {
            double c[3] = {-1 + (2*i + 1)*h, -1 + (2*j + 1)*h, -1 + (2*k + 1)*h};
            run.cell(f, c, h, 0);
        }
    return out;
}

}  // namespace cert

// ---------------------------------------------------------------------------------------
// THE DESCENT CHECKS (a)-(d), computed once per run

struct DescentCertificate {
    double tau = 0;
    // the form and the generators
    bool form_ok = false;                 // P Hermitian, det P = 1, tr P = 2r, F'_33 = -1, F'_i3 = F'_3i = 0, F'^-1 F' = I
    std::vector<M3> S;                    // the elements of S, in its order
    std::vector<int> T;                   // the indices in S of T = S n S^-1
    size_t not_unitary = 0;               // elements s of S with s^* F' s != F'
    size_t inverse_clashes = 0;           // s whose inverse's key is in S with another matrix
    int64_t coef_max = 0;                 // the largest coefficient of a third column of T (<= 100)
    // (a) the covering
    bool tau_below = false;               // tau^2 < sqrt3/2, in interval arithmetic
    double tau_gap = 0;                   // a lower bound of sqrt3/2 - tau^2
    long cells = 0, cells_failed = 0, visited = 0;
    int deepest = 0;
    double least_margin = 0;              // the least interval lower bound of the margin of a passing cell
    double volume = 0;                    // 64 when the passing cells tile the 8 facets
    size_t used = 0;                      // elements of T with which some cell passes
    std::vector<long> per_level;
    // (b) the first shell
    size_t units = 0, units_not_powers = 0;          // x conj(x) = 1: their number, and those that are not powers of zeta
    bool reduction_ok = false;            // rho_3 is a ring homomorphism Z[zeta] -> F_9
    size_t G = 0, O0 = 0;                 // |rho_3(Pi)| and |O_0|
    size_t shell_vectors = 0, shell_points = 0;      // the vectors c, and the points (up to units)
    size_t shell_in_O0 = 0, shell_in_T = 0;          // those with class in O_0, and of them the keys of elements of T
    // (c) the stabiliser of o
    size_t stab = 0;                      // blocks A over Z[zeta] with A^* P A = P
    size_t stab_trivial = 0;              // of them with trivial reduction mod 3
    size_t stab_trivial_det0 = 0;         // and with det A = zeta^k, k = 0 mod 3
    bool stab_identity = false;           // that one is the identity
    bool det_ok = false;                  // det a_i = zeta^k, k = 0 mod 3 (i = 1, 2, 3)
    // (d) the distance order
    size_t dist_values = 0;               // the x in Z[zeta] with x conj(x) = 1 or alpha_n, n <= ORDER_SHELLS
    size_t dist_shells = 0;               // the values among them
    size_t dist_order_breaks = 0;         // consecutive values where hdist (double) or its float does not increase
    double dist_gap = 0;                  // the least gap between consecutive values, in double
    double seconds = 0;

    bool covering_ok() const
    {
        return tau_below && coef_max <= 100 && cells > 0 && cells_failed == 0 && least_margin > 0 && volume == 64;
    }
    bool passes() const
    {
        return form_ok && !S.empty() && not_unitary == 0 && !T.empty() && inverse_clashes == 0 && covering_ok() &&
               units == 12 && units_not_powers == 0 && reduction_ok && G > 0 && G == O0 && shell_points > 0 &&
               shell_in_T == shell_in_O0 && stab > 0 && stab_trivial_det0 == 1 && stab_identity && det_ok &&
               dist_values > 0 && dist_order_breaks == 0;
    }
    // which checks fail ("" if none)
    std::string failures() const
    {
        std::string s;
        char buf[200];
        if (!form_ok) s += " the form F' is not as the checks need;";
        if (S.empty()) s += " S is empty;";
        if (not_unitary) { snprintf(buf, sizeof buf, " %zu elements of S are not F'-unitary;", not_unitary); s += buf; }
        if (T.empty()) s += " T = S n S^-1 is empty;";
        if (inverse_clashes) { snprintf(buf, sizeof buf, " %zu inverses have the key of another element;", inverse_clashes); s += buf; }
        if (!tau_below) { snprintf(buf, sizeof buf, " tau = %.9g: tau^2 is not below sqrt3/2;", tau); s += buf; }
        if (coef_max > 100) { snprintf(buf, sizeof buf, " a third column of T has a coefficient %lld > 100;", (long long)coef_max); s += buf; }
        if (cells_failed || cells == 0) {
            snprintf(buf, sizeof buf, " the covering fails (a cell fails at level %d, or the budget is spent);", COVER_LEVELS);
            s += buf;
        }
        else if (!(least_margin > 0)) s += " the covering's least margin is not positive;";
        else if (volume != 64) { snprintf(buf, sizeof buf, " the passing cells have volume %.17g, not 64;", volume); s += buf; }
        if (units != 12 || units_not_powers) { snprintf(buf, sizeof buf, " %zu units of norm 1 (%zu not powers of zeta);", units, units_not_powers); s += buf; }
        if (!reduction_ok) s += " the reduction mod 3 is not a ring homomorphism;";
        if (G == 0 || G != O0) { snprintf(buf, sizeof buf, " rho_3(Pi) has %zu elements, the orbit O_0 %zu points;", G, O0); s += buf; }
        if (shell_points == 0 || shell_in_T != shell_in_O0) {
            snprintf(buf, sizeof buf, " of the %zu points of the first shell with class in O_0, %zu are keys of T;", shell_in_O0, shell_in_T);
            s += buf;
        }
        if (stab == 0 || stab_trivial_det0 != 1 || !stab_identity) {
            snprintf(buf, sizeof buf, " %zu of the %zu blocks fixing o reduce to 1 mod 3 with det in <zeta^3>%s;", stab_trivial_det0,
                     stab, stab_identity ? "" : ", not the identity alone");
            s += buf;
        }
        if (!det_ok) s += " det a_i is not in <zeta^3>;";
        if (dist_values == 0 || dist_order_breaks) {
            snprintf(buf, sizeof buf, " hdist is not increasing in |g_33|^2 at %zu places;", dist_order_breaks);
            s += buf;
        }
        if (!s.empty()) s.pop_back();
        return s;
    }
};

// (d) of the descent checks, on value ranges n -> {least hdist, largest hdist, least float, largest float}: for
// consecutive values, in increasing order, the largest hdist and the largest float of the smaller must lie below the
// least hdist and the least float of the larger; each pair that does not counts as a break. Also the least gap, in
// double. (A function of its own, so that a test can give it ranges on which the float comparison decides: on the
// true values it never does.)
inline void order_ranges(const std::map<int64_t, std::array<double, 4>>& rng, DescentCertificate& c)
{
    c.dist_shells = rng.size();
    c.dist_gap = std::numeric_limits<double>::infinity();
    const std::array<double, 4>* prev = nullptr;
    for (const auto& kv : rng) {
        if (prev) {
            if (!((*prev)[1] < kv.second[0]) || !((*prev)[3] < kv.second[2])) c.dist_order_breaks++;
            c.dist_gap = std::min(c.dist_gap, kv.second[0] - (*prev)[1]);
        }
        prev = &kv.second;
    }
}

// the descent checks for the generators S (as geogens returns them), with the covering at tau
inline DescentCertificate certify_descent(const std::vector<Gen>& gens, double tau)
{
    using namespace cert;
    double t0 = std::clock()/(double)CLOCKS_PER_SEC;
    DescentCertificate c;
    c.tau = tau;
    const Zz one = Z(1, 0, 0, 0), zero = Z(0, 0, 0, 0), r = Z(0, 2, 0, -1);      // r = sqrt3
    // the form: P = [[r+1, -1], [-1, r-1]] Hermitian (diagonal fixed by conj, P_12 = conj P_21), det P = 1, tr P = 2r
    {
        Zz detP = zsub(zmul(FP.e[0][0], FP.e[1][1]), zmul(FP.e[0][1], FP.e[1][0]));
        Zz trP = zadd(FP.e[0][0], FP.e[1][1]);
        c.form_ok = zeq(detP, one) && zeq(trP, zadd(r, r)) && zeq(zconj(FP.e[0][0]), FP.e[0][0]) &&
                    zeq(zconj(FP.e[1][1]), FP.e[1][1]) && zeq(zconj(FP.e[0][1]), FP.e[1][0]) &&
                    zeq(FP.e[2][2], Z(-1, 0, 0, 0)) && zeq(FP.e[0][2], zero) && zeq(FP.e[1][2], zero) &&
                    zeq(FP.e[2][0], zero) && zeq(FP.e[2][1], zero) && meq(mmul(FPI, FP), mid());
    }
    // S, its unitarity, and T = S n S^-1
    std::map<PU21::Key, int> idx;
    for (size_t i = 0; i < gens.size(); i++) {
        c.S.push_back(gens[i].m);
        idx[PU21::key(gens[i].m)] = (int)i;
        if (!meq(mmul(mmul(madj(gens[i].m), FP), gens[i].m), FP)) c.not_unitary++;
    }
    for (size_t i = 0; i < c.S.size(); i++) {
        M3 v = mnormal(minv(c.S[i]));
        auto it = idx.find(PU21::key(v));
        if (it == idx.end()) continue;
        if (!meq(c.S[it->second], v)) { c.inverse_clashes++; continue; }
        c.T.push_back((int)i);
    }
    std::vector<M3> T;
    for (int i : c.T) {
        T.push_back(c.S[i]);
        for (int k = 0; k < 3; k++) for (int j = 0; j < 4; j++) c.coef_max = std::max<int64_t>(c.coef_max, std::llabs(c.S[i].e[k][2].c[j]));
    }
    // (a) the covering at tau, and tau^2 < sqrt3/2
    {
        Iv t2 = ipt(tau)*ipt(tau), h = isqrt3()*ipt(0.5);
        c.tau_below = tau > 0 && t2.hi < h.lo;
        c.tau_gap = dn(h.lo - t2.hi);
        if (c.coef_max <= 100 && !T.empty()) {
            Covering cv = cover(T, tau);
            c.cells = cv.cells; c.cells_failed = cv.failed; c.visited = cv.visited; c.deepest = cv.deepest;
            c.least_margin = cv.cells ? cv.least : 0; c.volume = cv.volume; c.per_level = cv.per_level;
            c.per_level.resize(c.deepest + 1);
            for (long u : cv.used) c.used += u > 0;
        }
    }
    // (b) the first shell. The units first: x conj(x) = 1 means |sigma_1 x| = |sigma_5 x| = 1, so T_2(x) = 2
    for (const Zz& x : trace_ball(2)) {
        if (!zeq(zmul(x, zconj(x)), one)) continue;
        c.units++;
        if (unit_power(x) < 0) c.units_not_powers++;
    }
    c.reduction_ok = reduction_is_homomorphism();
    std::set<PV> O0;
    {
        std::vector<PM> g3;
        for (int i = 0; i < 3; i++) { g3.push_back(rho3(A[i])); g3.push_back(rho3(minv(A[i]))); }
        std::set<PM> G3 = {rho3(mid())};
        std::vector<PM> q(G3.begin(), G3.end());
        for (size_t i = 0; i < q.size() && q.size() < 100000; i++)
            for (const PM& s : g3) { PM x = pm_mul(q[i], s); if (G3.insert(x).second) q.push_back(x); }
        for (const PM& g : G3) O0.insert(pm_act(g, PV{0, 0, 1}));
        c.G = G3.size();
        c.O0 = O0.size();
    }
    {
        // c_3: T_2 = 2m = 4 and c_3 conj(c_3) = 2 + sqrt3; c_1, c_2: T_2 <= column_bound(1) = 10, h_P(c_12, c_12) = 1 + sqrt3
        std::vector<Zz> c3s, X = trace_ball(column_bound(1));
        for (const Zz& x : trace_ball(4)) { Q3 q; if (norm3(x, q) && q.m == 2 && q.n == 1) c3s.push_back(x); }
        const Zz beta = Z(1, 2, 0, -1);
        std::set<PU21::Key> tkeys, seen;
        for (const M3& t : T) tkeys.insert(PU21::key(t));
        for (const Zz& x : X) for (const Zz& y : X) {
            Zz v[2] = {x, y};
            if (!zeq(hP(v, v), beta)) continue;
            for (const Zz& z : c3s) {
                Zz col[3] = {x, y, z};
                c.shell_vectors++;
                PU21::Key k = colkey(col);
                if (!seen.insert(k).second) continue;
                c.shell_points++;
                if (!O0.count(class3(col))) continue;
                c.shell_in_O0++;
                if (tkeys.count(k)) c.shell_in_T++;
            }
        }
    }
    // (c) the stabiliser: the blocks A with A^* P A = P; first columns h_P(x, x) = r + 1, second r - 1, both with
    // T_2 of each entry <= column_bound(1) (sigma_1 - sigma_5 of r +- 1 is 2 sqrt3); h_P(x, y) = P_12 = -1
    {
        std::vector<Zz> X = trace_ball(column_bound(1));
        const Zz rp1 = Z(1, 2, 0, -1), rm1 = Z(-1, 2, 0, -1), m1 = Z(-1, 0, 0, 0);
        std::vector<std::array<Zz, 2>> col1, col2;
        for (const Zz& x : X) for (const Zz& y : X) {
            Zz v[2] = {x, y};
            Zz h = hP(v, v);
            if (zeq(h, rp1)) col1.push_back({x, y});
            if (zeq(h, rm1)) col2.push_back({x, y});
        }
        PM triv = rho3(mid());
        for (const auto& p : col1) for (const auto& q : col2) {
            Zz a[2] = {p[0], p[1]}, b[2] = {q[0], q[1]};
            if (!zeq(hP(a, b), m1)) continue;
            c.stab++;
            M3 g = mzero();
            g.e[0][0] = p[0]; g.e[1][0] = p[1]; g.e[0][1] = q[0]; g.e[1][1] = q[1]; g.e[2][2] = one;
            if (rho3(g) != triv) continue;
            c.stab_trivial++;
            int k = unit_power(zdet(g));
            if (k >= 0 && k % 3 == 0) {
                c.stab_trivial_det0++;
                c.stab_identity = meq(g, mid());
            }
        }
        c.det_ok = true;
        for (int i = 0; i < 3; i++) { int k = unit_power(zdet(A[i])); c.det_ok = c.det_ok && k >= 0 && k % 3 == 0; }
    }
    // (d) the distance order: every x with x conj(x) = alpha_n, n <= ORDER_SHELLS, has T_2(x) = 2m_n <= 2m_TOP
    {
        std::map<int64_t, std::array<double, 4>> rng;          // n -> least and largest hdist, least and largest float
        for (const Zz& x : trace_ball(2*alpha(ORDER_SHELLS).m)) {
            Q3 q;
            if (!norm3(x, q)) { c.dist_order_breaks++; continue; }
            int64_t n = shell_of(q);
            if (n < 0 || n > ORDER_SHELLS) continue;
            double d = hdist(x), f = (float)d;
            auto it = rng.find(n);
            if (it == rng.end()) rng[n] = {d, d, f, f};
            else {
                auto& v = it->second;
                v[0] = std::min(v[0], d); v[1] = std::max(v[1], d); v[2] = std::min(v[2], f); v[3] = std::max(v[3], f);
            }
            c.dist_values++;
        }
        order_ranges(rng, c);
    }
    c.seconds = std::clock()/(double)CLOCKS_PER_SEC - t0;
    return c;
}

// the descent checks of S = geogens(4.5) at tau = 0.9306, computed once per run
inline const DescentCertificate& descent_certificate()
{
    static const DescentCertificate c = certify_descent(geogens(ROOT_GEOGENS), COVER_TAU);
    return c;
}

// ---------------------------------------------------------------------------------------
// THE STORE CHECKS (the closure and the distance order), on each search

struct RootCertificate {
    double R = 0, rho = 0;                // the radius of the roots, and of the search (R + 0.5)
    size_t generators = 0;                // |S|
    size_t searched = 0;                  // the elements of Pi the search found in B(rho)
    size_t roots = 0;                     // those with (float) d <= R: the roots, as roots_info keeps them
    bool in_range = true;                 // rho <= MAX_SEARCH_RADIUS
    bool descent = true;                  // the descent checks pass
    bool same_S = true;                   // the search's S is the S of the descent checks (whole matrices)
    bool identity_first = true;           // the first element of the store is 1, the root of the search tree
    size_t not_closed = 0;                // (g, s), g stored, d(o, g s o) <= rho (computed as enumerate does), g s not stored
    size_t key_clashes = 0;               // (g, s) whose key is stored with another matrix
    size_t order_breaks = 0;              // consecutive exact values of |g_33|^2 in the store where hdist or the stored
                                          // float does not increase (the distance order)
    size_t dist_wrong = 0;                // stored distances that are not the float of hdist of the element
    long label_inconsistencies = 0;       // enumerate's count: elements reached twice with different f-values
    size_t no_descent = 0;                // consistency: elements g != 1 with no t in T with |(g t)_33| < |g_33|
    // copied from the descent checks, for the line
    long cells = 0;
    int deepest = 0;
    double least_margin = 0;
    std::string descent_failures;
    // R against the shells d_n (n >= 1)
    double margin = 1e300, nearest = 0;   // |R - d| for the d_n or float(d_n) nearest R, and that d_n
    int64_t nearest_n = 0;
    size_t nearest_count = 0;             // the elements of the store in that shell
    std::vector<M3> rootv;                // the roots and their distances, as roots_info returns them
    std::vector<double> rootd;

    bool passes() const
    {
        return in_range && descent && same_S && identity_first && not_closed == 0 && key_clashes == 0 &&
               order_breaks == 0 && dist_wrong == 0 && label_inconsistencies == 0 && no_descent == 0;
    }
};

// the checks on a store B made by enumerate(B, S, rho), and its roots of radius R (roots_info's filter and order)
inline void certify_store(const Store& B, const std::vector<Gen>& S, double rho, double R, RootCertificate& c)
{
    using namespace cert;
    const DescentCertificate& D = descent_certificate();
    c.R = R;
    c.rho = rho;
    c.generators = S.size();
    c.searched = B.size();
    c.in_range = rho <= MAX_SEARCH_RADIUS;
    c.descent = D.passes();
    c.descent_failures = D.failures();
    c.cells = D.cells;
    c.deepest = D.deepest;
    c.least_margin = D.least_margin;
    c.same_S = S.size() == D.S.size();
    for (size_t i = 0; c.same_S && i < S.size(); i++) c.same_S = meq(S[i].m, D.S[i]);
    c.identity_first = B.size() > 0 && meq(B.load(0), mnormal(mid())) && B.parent[0] < 0;
    // the closure, by the key and then the whole element
    for (size_t i = 0; i < B.size(); i++) {
        M3 g = B.load(i);
        for (const Gen& s : S) {
            PU21::Key k = PU21::product_key(g, s.m);
            int f = B.find(k);
            if (f >= 0) { if (!meq(B.load(f), mnormal(mmul(g, s.m)))) c.key_clashes++; }
            else if (!(PU21::key_displacement(k) > rho)) c.not_closed++;
        }
    }
    // the exact |g_33|^2; a strictly nearer g t, t in T; the stored distances
    std::vector<std::pair<Q3, size_t>> byq;
    for (size_t i = 0; i < B.size(); i++) {
        M3 g = B.load(i);
        Q3 q;
        if (!norm3(g.e[2][2], q)) q = Q3{0, -1};
        byq.push_back({q, i});
        if ((float)hdist(g.e[2][2]) != B.dist[i]) c.dist_wrong++;
        if (i == 0) continue;
        bool nearer = false;
        for (size_t j = 0; j < D.T.size() && !nearer; j++) {
            Q3 p;
            nearer = norm3(entry33(g, D.S[D.T[j]]), p) && cmp3(p, q) < 0;
        }
        if (!nearer) c.no_descent++;
    }
    // the distance order on the store, at each boundary between shells: in the order (|g_33|^2, index), the last element of a
    // shell against the first of the next, in double and in the stored float. (It compares one pair per boundary, not
    // the extremes; check (d) compares the extremes over every possible (3,3) entry.)
    std::sort(byq.begin(), byq.end(), [](const std::pair<Q3, size_t>& x, const std::pair<Q3, size_t>& y) {
        int k = cmp3(x.first, y.first);
        return k != 0 ? k < 0 : x.second < y.second;
    });
    for (size_t t = 1; t < byq.size(); t++) {
        if (cmp3(byq[t - 1].first, byq[t].first) >= 0) continue;
        double d0 = hdist(B.load(byq[t - 1].second).e[2][2]), d1 = hdist(B.load(byq[t].second).e[2][2]);
        float f0 = B.dist[byq[t - 1].second], f1 = B.dist[byq[t].second];
        if (!(d0 < d1) || !(f0 < f1)) c.order_breaks++;
    }
    // R against the shells d_n (n >= 1) and their floats
    for (int64_t n = 1; R > 0 && n <= 100000; n++) {
        double d = shell_distance(n);
        for (double e : {d, (double)(float)d})
            if (std::fabs(R - e) < c.margin) { c.margin = std::fabs(R - e); c.nearest = d; c.nearest_n = n; }
        if (d > R + 1) break;
    }
    if (c.nearest_n > 0) {
        Q3 a = alpha(c.nearest_n);
        for (const auto& x : byq) if (x.first.m == a.m && x.first.n == a.n) c.nearest_count++;
    }
    // the roots, kept as roots_info keeps them
    c.rootv.clear(); c.rootd.clear();
    std::vector<int> ix;
    for (size_t i = 0; i < B.size(); i++) if (B.dist[i] <= R) ix.push_back((int)i);
    std::stable_sort(ix.begin(), ix.end(), [&](int a, int b) { return B.dist[a] < B.dist[b]; });
    for (int i : ix) { c.rootv.push_back(B.load(i)); c.rootd.push_back(B.dist[i]); }
    c.roots = c.rootv.size();
}

// the search of roots_info(R) made again (the same calls, so the same elements in the same order), and checked
inline RootCertificate certify_roots(double R)
{
    RootCertificate c;
    std::vector<Gen> G = geogens(ROOT_GEOGENS);
    Store B;
    guard(R + 0.5);
    c.label_inconsistencies = enumerate(B, G, R + 0.5);
    certify_store(B, G, R + 0.5, R, c);
    return c;
}

// the line of zipper3 --stats
inline std::string root_certificate_line(const RootCertificate& c)
{
    char buf[800];
    int k = snprintf(buf, sizeof buf, "roots: %zu elements of Pi in B(%.9g), of %zu searched in B(%.9g); ", c.roots, c.R,
                     c.searched, c.rho);
    std::string s(buf, (size_t)std::max(0, k));
    if (c.passes()) {
        snprintf(buf, sizeof buf, "checked: closed under the %zu elements of S inside that ball, and every "
                 "element descends to 1 (the covering: %ld cells to level %d, least margin %.2e in interval "
                 "arithmetic; the first shell and the stabiliser of o, exactly)", c.generators, c.cells, c.deepest,
                 c.least_margin);
        return s + buf;
    }
    s += "FAILED:";
    if (!c.in_range) { snprintf(buf, sizeof buf, " the search radius %.9g is above %g;", c.rho, MAX_SEARCH_RADIUS); s += buf; }
    if (!c.descent) s += c.descent_failures.empty() ? std::string(" the descent checks fail;")
                                                     : " the descent checks fail:" + c.descent_failures + ";";
    if (!c.same_S) s += " the search's S is not the S of the descent checks;";
    if (!c.identity_first) s += " the search does not start at the identity;";
    if (c.not_closed) { snprintf(buf, sizeof buf, " %zu S-neighbours in B(%.9g) not found;", c.not_closed, c.rho); s += buf; }
    if (c.key_clashes) { snprintf(buf, sizeof buf, " %zu S-neighbours with the key of another element;", c.key_clashes); s += buf; }
    if (c.order_breaks) { snprintf(buf, sizeof buf, " the computed distance is not increasing in |g_33|^2 at %zu places;", c.order_breaks); s += buf; }
    if (c.dist_wrong) { snprintf(buf, sizeof buf, " %zu stored distances are not those of their elements;", c.dist_wrong); s += buf; }
    if (c.label_inconsistencies) { snprintf(buf, sizeof buf, " f inconsistent on %ld elements;", c.label_inconsistencies); s += buf; }
    if (c.no_descent) { snprintf(buf, sizeof buf, " %zu elements without a strictly nearer neighbour g t, t in T;", c.no_descent); s += buf; }
    s.pop_back();
    return s;
}

// a second line, only when R is within 1e-6 of a shell distance d_n (n >= 1) or of its float (empty otherwise)
inline std::string root_margin_line(const RootCertificate& c)
{
    if (!(c.R > 0) || c.margin >= 1e-6) return "";
    char buf[400];
    snprintf(buf, sizeof buf, "roots: R is within %.1e of d_%lld = %.9f (in double or in single precision), the distance "
             "of a shell (%zu elements of Pi in it): whether they are roots is decided by the single-precision "
             "test of roots_info, not by the exact ball", c.margin, (long long)c.nearest_n, c.nearest, c.nearest_count);
    return buf;
}

// the lines of zipper3 --stats and link3 --stats: the search of roots_info(R) made again and checked; its roots and
// distances compared, bit for bit, with those the program uses (roots, rootd); and the second line near a shell. If
// they differ, a line "roots: NOT <theirs>: the checked search kept N, <by> M (or others)" follows the first.
inline std::vector<std::string> stats_lines(double R, const std::vector<M3>& roots, const std::vector<double>& rootd,
                                            const char* theirs, const char* by)
{
    RootCertificate c = certify_roots(R);
    bool same = c.rootv.size() == roots.size() && c.rootd.size() == rootd.size();
    for (size_t i = 0; same && i < c.rootv.size(); i++)
        same = meq(c.rootv[i], roots[i]) && !std::memcmp(&c.rootd[i], &rootd[i], sizeof(double));
    std::vector<std::string> out = {root_certificate_line(c)};
    if (!same) {
        char buf[300];
        snprintf(buf, sizeof buf, "roots: NOT %s: the checked search kept %zu, %s %zu (or others)", theirs,
                 c.rootv.size(), by, roots.size());
        out.push_back(buf);
    }
    std::string m = root_margin_line(c);
    if (!m.empty()) out.push_back(m);
    return out;
}

}  // namespace cs
}  // namespace zippers
