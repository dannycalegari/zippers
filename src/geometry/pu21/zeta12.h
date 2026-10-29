// SPDX-License-Identifier: GPL-3.0-or-later
//
// zeta12.h: exact arithmetic in Z[zeta], zeta = exp(pi i/6) a primitive 12th root of unity.
//
// An element is given by its coefficients of 1, zeta, zeta^2, zeta^3 (a basis of Z[zeta] over Z), with
// zeta^4 = zeta^2 - 1. Moved unchanged from legacy/s3/csgroup.h (static -> inline; namespace zippers; the
// coefficients are int64_t, which the legacy i64 was).
//
//   Z(a, b, c, d)            a + b zeta + c zeta^2 + d zeta^3
//   zadd, zsub, zmul         ring operations          zzero, zeq    tests
//   zrot(a), zrotk(a, k)     zeta a, zeta^k a          zconj         complex conjugation (zeta -> zeta^-1)
//   zless                    the lexicographic order of the coefficients
//   normk(a)                 the k in 0..11 for which zeta^k a is least (a != 0): the normalisation of PU(2,1)
//   zval(a, w)               the value in C under zeta -> w: ZETA (the embedding in which CKY's form F' is
//                            indefinite, used everywhere) or ZETA5 = its Galois conjugate (F' definite)

#pragma once

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>

namespace zippers {

typedef std::complex<double> cpx;

struct Zz { int64_t c[4]; };

inline Zz Z(int64_t a, int64_t b, int64_t c, int64_t d) { Zz z; z.c[0]=a; z.c[1]=b; z.c[2]=c; z.c[3]=d; return z; }
inline Zz zadd(const Zz& a, const Zz& b) { return Z(a.c[0]+b.c[0], a.c[1]+b.c[1], a.c[2]+b.c[2], a.c[3]+b.c[3]); }
inline Zz zsub(const Zz& a, const Zz& b) { return Z(a.c[0]-b.c[0], a.c[1]-b.c[1], a.c[2]-b.c[2], a.c[3]-b.c[3]); }
inline bool zzero(const Zz& a) { return !a.c[0] && !a.c[1] && !a.c[2] && !a.c[3]; }
inline bool zeq(const Zz& a, const Zz& b) { return !std::memcmp(a.c, b.c, sizeof a.c); }

inline Zz zmul(const Zz& a, const Zz& b)
{
    int64_t p[7] = {0,0,0,0,0,0,0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) p[i+j] += a.c[i]*b.c[j];
    // zeta^4 = zeta^2 - 1, zeta^5 = zeta^3 - zeta, zeta^6 = -1
    return Z(p[0]-p[4]-p[6], p[1]-p[5], p[2]+p[4], p[3]+p[5]);
}

inline Zz zrot(const Zz& a) { return Z(-a.c[3], a.c[0], a.c[1]+a.c[3], a.c[2]); }      // times zeta
inline Zz zconj(const Zz& a) { return Z(a.c[0]+a.c[2], a.c[1], -a.c[2], -(a.c[1]+a.c[3])); }
inline Zz zrotk(Zz a, int k) { k = ((k % 12) + 12) % 12; while (k--) a = zrot(a); return a; }

inline const cpx ZETA = std::polar(1.0, M_PI/6);           // the embedding where F' is indefinite
inline const cpx ZETA5 = std::polar(1.0, 5*M_PI/6);        // the Galois conjugate: F' definite

inline cpx zval(const Zz& a, cpx w = ZETA)
{
    return (double)a.c[0] + w*((double)a.c[1] + w*((double)a.c[2] + w*(double)a.c[3]));
}

inline bool zless(const Zz& a, const Zz& b)
{
    for (int i = 0; i < 4; i++) if (a.c[i] != b.c[i]) return a.c[i] < b.c[i];
    return false;
}

// the k in 0..11 for which zeta^k a is lexicographically least (a != 0, so it is unique)
inline int normk(const Zz& a)
{
    Zz y = a, best = a;
    int bk = 0;
    for (int k = 1; k < 12; k++) {
        y = zrot(y);
        if (zless(y, best)) { best = y; bk = k; }
    }
    return bk;
}

}  // namespace zippers
