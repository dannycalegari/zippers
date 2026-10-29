// SPDX-License-Identifier: GPL-3.0-or-later
//
// on1.h: O(n,1) over (1/2)Z, exactly, and its action on S^{n-1} in double precision; the backend zippers::ON1<n>
// See geometry/backend.h for what a backend is. The suite uses n = 5 (examples/imm_m5.h).
//
// THE MATRICES are (n+1)x(n+1), preserving the form x_1^2 + ... + x_n^2 - x_{n+1}^2, with entries in (1/2)Z: an
// element is stored as TWICE its matrix, so all arithmetic is exact. The lattices act freely, with o = e_{n+1} of
// trivial stabiliser, so an element g is determined by g o, the last column of 2g: the KEY. d(o, g o) = acosh g_{n+1,n+1}.
// Elements are not projective here, so normal is the identity map.
//
// Moved from legacy/s4/rh5.h (M6, m6id, m6mul, m6inv, m6eq, m6dist, m6maxcoef; the layout and the guard of Store6) and
// legacy/s4/zipcore5.h (toD6, mv6, spoint5, attracting6), as one template on n: the legacy 6 is n + 1 and the legacy 5
// is n, and every expression is the legacy one, in the same order, so that ON1<5> is bit for bit the legacy code
// (tests/unit/test_on1.cc). Changes of form, apart from the names:
//   - the error message of mul on an odd entry says "ON1::mul" where it said "m6mul" (a path that cannot occur for
//     lattice elements);
//   - attracting starts its power iteration at the legacy vector (0.31, -0.17, 0.23, 0.11, -0.29, 1) when n = 5; for
//     other n the first n entries run through those five numbers and the last is 1;
//   - boundary returns a SpherePoint<n> (geometry/sphere.h) in place of the legacy P5 (the same five doubles when n = 5).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "geometry/backend.h"
#include "geometry/sphere.h"

namespace zippers {

template <int n>
struct ON1 {
    static_assert(n >= 2, "ON1<n>: the sphere at infinity S^{n-1} needs n >= 2");
    static constexpr int D = n + 1;                     // the matrices are D x D

    struct Elem { int64_t e[n + 1][n + 1]; };           // twice the matrix (legacy M6 when n = 5)
    static constexpr int N = n;                         // S^{n-1} in R^n
    typedef SpherePoint<n> Point;
    static constexpr int key_size = n + 1;              // the last column of 2g
    typedef std::array<int64_t, n + 1> Key;
    static constexpr int packed_size = (n + 1)*(n + 1); // every entry, as int32
    typedef std::array<std::array<double, n + 1>, n + 1> DMat;   // legacy D6
    typedef std::array<double, n + 1> DVec;                      // legacy V6

    static Elem identity()                                       // m6id
    {
        Elem m;
        std::memset(&m, 0, sizeof m);
        for (int i = 0; i < D; i++) m.e[i][i] = 2;
        return m;
    }

    static Elem mul(const Elem& a, const Elem& b)                // m6mul
    {
        Elem c;
        for (int i = 0; i < D; i++)
            for (int j = 0; j < D; j++) {
                __int128 s = 0;
                for (int k = 0; k < D; k++) s += (__int128)a.e[i][k]*b.e[k][j];
                if (s & 1) { fprintf(stderr, "ON1::mul: an odd entry (not in the lattice)\n"); exit(1); }
                c.e[i][j] = (int64_t)(s/2);
            }
        return c;
    }

    static Elem inv(const Elem& a)                               // m6inv: g^-1 = J g^T J
    {
        Elem c;
        for (int i = 0; i < D; i++)
            for (int j = 0; j < D; j++) c.e[i][j] = a.e[j][i]*((i == n) != (j == n) ? -1 : 1);
        return c;
    }

    static Elem normal(const Elem& a) { return a; }              // elements are not projective here
    static bool eq(const Elem& a, const Elem& b) { return !std::memcmp(&a, &b, sizeof a); }    // m6eq
    static bool same(const Elem& a, const Elem& b) { return eq(a, b); }                         // m6eq

    static int64_t max_coefficient(const Elem& g)                // m6maxcoef
    {
        int64_t m = 0;
        for (int i = 0; i < D; i++) for (int j = 0; j < D; j++) m = std::max<int64_t>(m, std::llabs(g.e[i][j]));
        return m;
    }

    static Key key(const Elem& a)                                // the last column (Store6::lookup)
    {
        Key k;
        for (int r = 0; r < D; r++) k[r] = a.e[r][n];
        return k;
    }
    // the last column of mul(h, s). The whole product is computed, as the legacy code does (Store6::lookup and
    // fingerprint6 of m6mul(h, s)), so that the odd-entry check is made on every entry.
    static Key product_key(const Elem& h, const Elem& s) { return key(mul(h, s)); }

    static double displacement(const Elem& g)                    // m6dist: d(o, g o) = acosh g_66
    {
        double c = 0.5*(double)g.e[n][n];
        return c <= 1 ? 0 : std::acosh(c);
    }
    static double key_displacement(const Key& k)                 // the same, from the key
    {
        double c = 0.5*(double)k[n];
        return c <= 1 ? 0 : std::acosh(c);
    }

    // the layout of the legacy Store6's mat: D*row + col, with its int32 guard (Store6::add)
    static void pack(const Elem& g, int32_t* out)
    {
        for (int r = 0; r < D; r++) for (int c = 0; c < D; c++) {
            int64_t v = g.e[r][c];
            if (v > INT32_MAX || v < INT32_MIN) { fprintf(stderr, "Store6: entry too large\n"); exit(1); }
            out[D*r + c] = (int32_t)v;
        }
    }
    static Elem unpack(const int32_t* m)                         // Store6::load
    {
        Elem g;
        for (int r = 0; r < D; r++) for (int c = 0; c < D; c++) g.e[r][c] = m[D*r + c];
        return g;
    }
    static Key packed_key(const int32_t* m)                      // Store6::col
    {
        Key k;
        for (int r = 0; r < D; r++) k[r] = m[D*r + n];
        return k;
    }

    static DMat to_double(const Elem& g)                         // toD6
    {
        DMat m;
        for (int i = 0; i < D; i++) for (int j = 0; j < D; j++) m[i][j] = 0.5*(double)g.e[i][j];
        return m;
    }

    static DVec act(const DMat& m, const DVec& v)                // mv6
    {
        DVec r;
        for (int i = 0; i < D; i++) { double s = 0; for (int k = 0; k < D; k++) s += m[i][k]*v[k]; r[i] = s; }
        return r;
    }

    // a null vector (x, x_{n+1}) -> its point x/|x| of S^{n-1}                    spoint5
    static Point boundary(const DVec& c)
    {
        Point p;
        double nn = 0;
        for (int i = 0; i < n; i++) { p.x[i] = c[i]; nn += c[i]*c[i]; }
        nn = std::sqrt(nn);
        for (int i = 0; i < n; i++) p.x[i] /= nn;
        return p;
    }

    // the start of the power iteration of attracting (see the top of the file)
    static DVec attracting_start()
    {
        static const double s[5] = {0.31, -0.17, 0.23, 0.11, -0.29};
        DVec v;
        for (int i = 0; i < n; i++) v[i] = s[i % 5];
        v[n] = 1.0;
        return v;
    }

    // the attracting fixed point of a loxodromic element: power iteration on R^{n,1}  attracting6
    static DVec attracting(const Elem& t)
    {
        DMat m = to_double(t);
        DVec v = attracting_start();
        for (int it = 0; it < 3000; it++) {
            v = act(m, v);
            double nn = 0;
            for (double x : v) nn += x*x;
            nn = std::sqrt(nn);
            for (double& x : v) x /= nn;
        }
        if (v[n] < 0) for (double& x : v) x = -x;
        return v;
    }
};

static_assert(is_backend<ON1<5>>(), "ON1<5> is not a complete backend");

}  // namespace zippers
