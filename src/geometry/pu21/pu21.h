// SPDX-License-Identifier: GPL-3.0-or-later
//
// pu21.h: PU(2,1) over Z[zeta_12], exactly, and its action on S^3 in double precision; the backend zippers::PU21
// See geometry/backend.h for what a backend is.
//
// THE COORDINATES are the "primed" coordinates of Cartwright-Koziarz-Yeung (arXiv:1412.4137): every element of the
// lattices used here is a 3x3 matrix over Z[zeta] (zeta12.h), unitary for the Hermitian form F' below, and defined
// modulo the scalars zeta^k. An element is STORED normalised: multiplied by the zeta^k that makes its (3,3) entry
// lexicographically least (mnormal). The lattices act freely on the ball, so an element is determined by its third
// column, the image of the origin o = [0:0:1]: the normalised third column is the KEY. d(o, g o) = 2 acosh |g_33|
// (holomorphic curvature -1).
//
// Moved unchanged from legacy/s3/csgroup.h (M3 and its functions, F', hdist, ballpoint, prodkey) and
// legacy/s3/zipcore.h (toD, mv, spoint, maxcoef, attracting): static -> inline, namespace zippers, i64 -> int64_t.
// Two changes of form:
//   - F' and F'^-1 are constants here (FP, FPI), written out entry by entry; the legacy setup() computed them from
//     r = sqrt 3 before anything else. tests/unit/ checks that they are the legacy matrices.
//   - spoint returns a SpherePoint<4> (geometry/sphere.h) in place of the legacy P4 (the same four doubles).
// Every expression is the legacy one, in the same order, so every result is bit for bit the legacy one
// (tests/unit/test_pu21.cc). Do not "simplify" them.
//
// Note (backend.h): mul and inv do NOT normalise, and normal does; displacement reads the (3,3) entry, so it is meant
// for normalised elements. The rounding of zval, and so of every double here, depends on the representative.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "geometry/backend.h"
#include "geometry/pu21/zeta12.h"
#include "geometry/sphere.h"

namespace zippers {

// ---------------------------------------------------------------------------------------
// 3x3 matrices over Z[zeta]

struct M3 { Zz e[3][3]; };

inline M3 mzero() { M3 A; std::memset(&A, 0, sizeof A); return A; }
inline M3 mid() { M3 A = mzero(); for (int i = 0; i < 3; i++) A.e[i][i] = Z(1,0,0,0); return A; }

inline M3 mmul(const M3& A, const M3& B)
{
    M3 C;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            C.e[i][j] = zadd(zadd(zmul(A.e[i][0], B.e[0][j]), zmul(A.e[i][1], B.e[1][j])), zmul(A.e[i][2], B.e[2][j]));
    return C;
}

inline M3 madj(const M3& A)                 // the conjugate transpose
{
    M3 C;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) C.e[i][j] = zconj(A.e[j][i]);
    return C;
}

inline M3 mrotk(M3 A, int k) { for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) A.e[i][j] = zrotk(A.e[i][j], k); return A; }
inline bool meq(const M3& A, const M3& B) { return !std::memcmp(&A, &B, sizeof A); }

// F' and its inverse (det F' = -1, so both are integral). With r = sqrt 3 = 2 zeta - zeta^3 = Z(0,2,0,-1):
//        ( r+1  -1   0 )              ( r-1   1   0 )
//   F' = ( -1   r-1  0 ),     F'^-1 = (  1   r+1  0 ),     r+1 = Z(1,2,0,-1),  r-1 = Z(-1,2,0,-1).
//        (  0    0  -1 )              (  0    0  -1 )
// (Legacy: FP and FPI, computed so in setup() of csgroup.h.)
inline constexpr M3 FP = {{{{{1, 2, 0, -1}}, {{-1, 0, 0, 0}}, {{0, 0, 0, 0}}},
                           {{{-1, 0, 0, 0}}, {{-1, 2, 0, -1}}, {{0, 0, 0, 0}}},
                           {{{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{-1, 0, 0, 0}}}}};
inline constexpr M3 FPI = {{{{{-1, 2, 0, -1}}, {{1, 0, 0, 0}}, {{0, 0, 0, 0}}},
                            {{{1, 0, 0, 0}}, {{1, 2, 0, -1}}, {{0, 0, 0, 0}}},
                            {{{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{-1, 0, 0, 0}}}}};

inline M3 minv(const M3& A) { return mmul(mmul(FPI, madj(A)), FP); }

inline M3 mnormal(const M3& A) { return mrotk(A, normk(A.e[2][2])); }

// is A = zeta^k I for some k?  (returns k, or -1)
inline int mscalar(const M3& A)
{
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (i != j && !zzero(A.e[i][j])) return -1;
    if (!zeq(A.e[0][0], A.e[1][1]) || !zeq(A.e[0][0], A.e[2][2])) return -1;
    Zz one = Z(1,0,0,0), y = one;
    for (int k = 0; k < 12; k++) { if (zeq(A.e[0][0], y)) return k; y = zrot(y); }
    return -1;
}

inline M3 mpow(const M3& A, int n)
{
    M3 B = mid(), P = n >= 0 ? A : minv(A);
    for (int m = std::abs(n); m > 0; m--) B = mmul(B, P);
    return B;
}

// the largest coefficient of any entry, in absolute value: the renderer's growth guard (legacy zipcore.h)
inline int64_t maxcoef(const M3& g)
{
    int64_t m = 0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int c = 0; c < 4; c++) m = std::max<int64_t>(m, std::llabs(g.e[i][j].c[c]));
    return m;
}

// ---------------------------------------------------------------------------------------
// Distances and keys

inline double hdist(const Zz& g33)        // d(o, g o) from the (3,3) entry
{
    double a = std::abs(zval(g33));
    return a <= 1 ? 0 : 2*std::acosh(a);
}

// the key of g*s, computed from g and the third column of s only (9 products instead of 27): the third column of g*s,
// normalised (each entry times zeta^k, k = normk of its (3,3) entry), 3 entries x 4 coefficients. Returns k. col gets
// the normalised column.
inline int prodkey(const M3& g, const M3& s, int64_t* key, Zz* col)
{
    for (int i = 0; i < 3; i++)
        col[i] = zadd(zadd(zmul(g.e[i][0], s.e[0][2]), zmul(g.e[i][1], s.e[1][2])), zmul(g.e[i][2], s.e[2][2]));
    int k = normk(col[2]);
    for (int i = 0; i < 3; i++) {
        col[i] = zrotk(col[i], k);
        for (int c = 0; c < 4; c++) key[4*i+c] = col[i].c[c];
    }
    return k;
}

// ---------------------------------------------------------------------------------------
// Double precision: matrices and vectors over C, and the map to the unit ball and to S^3

typedef std::array<cpx,3> C3;
typedef std::array<std::array<cpx,3>,3> D3;

inline const double SQ3 = std::sqrt(3.0);

inline D3 toD(const M3& g)
{
    D3 m;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = zval(g.e[i][j]);
    return m;
}

inline C3 mv(const D3& m, const C3& v)
{
    C3 r;
    for (int i = 0; i < 3; i++) r[i] = m[i][0]*v[0] + m[i][1]*v[1] + m[i][2]*v[2];
    return r;
}

// the point g.o of the unit ball of C^2, from the third column of g (primed coordinates):
// gamma_0 maps primed to F-coordinates and D = diag(1,1,sqrt(r-1)) maps those to the unit ball
inline void ballpoint(const cpx c[3], double p[4])
{
    const double r = std::sqrt(3.0);
    cpx w1 = c[0], w2 = c[0] + (1-r)*c[1], w3 = std::sqrt(r-1)*c[2];
    cpx z1 = w1/w3, z2 = w2/w3;
    p[0] = z1.real(); p[1] = z1.imag(); p[2] = z2.real(); p[3] = z2.imag();
}

// a null vector in primed coordinates -> its point of S^3 (unit ball coordinates, see ballpoint)
inline SpherePoint<4> spoint(const C3& c)
{
    cpx w1 = c[0], w2 = c[0] + (1 - SQ3)*c[1], w3 = std::sqrt(SQ3 - 1)*c[2];
    cpx z1 = w1/w3, z2 = w2/w3;
    SpherePoint<4> p = {{z1.real(), z1.imag(), z2.real(), z2.imag()}};
    double n = std::sqrt(p.x[0]*p.x[0] + p.x[1]*p.x[1] + p.x[2]*p.x[2] + p.x[3]*p.x[3]);
    for (int i = 0; i < 4; i++) p.x[i] /= n;
    return p;
}

// the attracting fixed point of a loxodromic element: its dominant eigenvector (power iteration)
inline C3 attracting(const M3& t)
{
    D3 m = toD(t);
    C3 v = {cpx(0.3,0.1), cpx(-0.2,0.5), cpx(1.0,0.0)};
    for (int it = 0; it < 600; it++) {
        v = mv(m, v);
        double n = std::sqrt(std::norm(v[0]) + std::norm(v[1]) + std::norm(v[2]));
        for (auto& x : v) x /= n;
    }
    return v;
}

// ---------------------------------------------------------------------------------------
// The backend (geometry/backend.h). Each member is the legacy function named beside it.

struct PU21 {
    typedef M3 Elem;
    static constexpr int N = 4;                         // S^3 in R^4 = C^2
    typedef SpherePoint<4> Point;
    static constexpr int key_size = 12;                 // the normalised third column: 3 entries x 4 coefficients
    typedef std::array<int64_t, 12> Key;
    static constexpr int packed_size = 36;              // 9 entries x 4 coefficients, as int32
    typedef D3 DMat;
    typedef C3 DVec;

    static Elem identity() { return mid(); }                                     // mid
    static Elem mul(const Elem& a, const Elem& b) { return mmul(a, b); }         // mmul (not normalised)
    static Elem inv(const Elem& a) { return minv(a); }                           // minv (not normalised)
    static Elem normal(const Elem& a) { return mnormal(a); }                     // mnormal
    static bool eq(const Elem& a, const Elem& b) { return meq(a, b); }           // meq
    static bool same(const Elem& a, const Elem& b) { return mscalar(mmul(a, minv(b))) >= 0; }   // same (zipcore.h)
    static int64_t max_coefficient(const Elem& a) { return maxcoef(a); }         // maxcoef (zipcore.h)

    // the key of a: prodkey(a, mid(), ...), as the renderer's draw() computes it
    static Key key(const Elem& a)
    {
        Key k;
        Zz col[3];
        prodkey(a, mid(), k.data(), col);
        return k;
    }
    // the key of h*s, from h and the third column of s: prodkey(h, s, ...)
    static Key product_key(const Elem& h, const Elem& s)
    {
        Key k;
        Zz col[3];
        prodkey(h, s, k.data(), col);
        return k;
    }
    // d(o, a o) for a normalised a: hdist(a.e[2][2])
    static double displacement(const Elem& a) { return hdist(a.e[2][2]); }
    // d(o, a o) from the key: hdist of the normalised (3,3) entry, which is entries 8..11 of the key, as
    // enumerate() and the K-search compute it (hdist(col[2]) after prodkey)
    static double key_displacement(const Key& k) { return hdist(Z(k[8], k[9], k[10], k[11])); }

    // the layout of the legacy Store's mat: (row*3 + col)*4 + coefficient, with its 2^28 guard (Store::add)
    static void pack(const Elem& a, int32_t* out)
    {
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int c = 0; c < 4; c++) {
            int64_t x = a.e[i][j].c[c];
            if (x > (1LL<<28) || x < -(1LL<<28)) {
                fprintf(stderr, "coefficient %lld exceeds the 2^28 storage guard; aborting\n", (long long)x);
                exit(1);
            }
            out[(i*3+j)*4 + c] = (int32_t)x;
        }
    }
    static Elem unpack(const int32_t* m)                                         // Store::load
    {
        M3 g;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int c = 0; c < 4; c++)
            g.e[i][j].c[c] = m[(i*3+j)*4 + c];
        return g;
    }
    static Key packed_key(const int32_t* m)                                      // Store::keyof
    {
        Key k;
        for (int i = 0; i < 3; i++)
            for (int c = 0; c < 4; c++) k[4*i+c] = m[(i*3+2)*4 + c];
        return k;
    }

    static DMat to_double(const Elem& a) { return toD(a); }                      // toD (zipcore.h)
    static DVec act(const DMat& m, const DVec& v) { return mv(m, v); }           // mv (zipcore.h)
    static Point boundary(const DVec& v) { return spoint(v); }                   // spoint (zipcore.h)
    static DVec attracting(const Elem& t) { return zippers::attracting(t); }     // attracting (zipcore.h)
};

static_assert(is_backend<PU21>(), "PU21 is not a complete backend");

}  // namespace zippers
