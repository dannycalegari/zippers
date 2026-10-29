// SPDX-License-Identifier: GPL-3.0-or-later
//
// backend.h: what a GEOMETRY BACKEND must provide.
//
// A backend is the exact arithmetic of one family of hyperbolic isometries, together with their action on the
// sphere at infinity in double precision. The renderer, the element store, the kernel search and the views are
// written once, as templates over a backend B, so the inner loops are as fast as in the legacy programs (no virtual
// calls). There are two backends:
//
//   zippers::PU21       (src/geometry/pu21/)  PU(2,1) over Z[zeta_12], zeta = exp(pi i/6): 3x3 matrices unitary for
//                       a Hermitian form, modulo the scalars zeta^k. The sphere at infinity is S^3 in R^4 = C^2.
//                       Legacy: legacy/s3/csgroup.h and legacy/s3/zipcore.h.
//   zippers::ON1<n>     (src/geometry/on1/)   O(n,1) over (1/2)Z: (n+1)x(n+1) matrices with entries in (1/2)Z, stored
//                       doubled. The sphere at infinity is S^{n-1} in R^n. The suite uses n = 5.
//                       Legacy: legacy/s4/rh5.h and legacy/s4/zipcore5.h.
//
// The PSL(2,C) code of S^2 is not a backend: the S^2 programs (apps/s2/) keep the renderer of legacy/s2/zipper.cc.
//
// THE RULE. Every function below replaces a legacy function, named in the table, and must give BIT FOR
// BIT the same result on every input: the same exact entries, and the same doubles (so the same expressions, in the
// same order). The legacy code is the specification; tests/unit/ compares the two on many random elements, and the
// reference test (tests/reference/) compares whole programs.
//
// ------------------------------------------------------------------------------------------------------------------
// TYPES AND CONSTANTS
//
//   B::Elem              an exact group element (a trivially copyable value).
//                          PU21: the legacy M3 (3x3 over Z[zeta_12]).  ON1<5>: the legacy M6 (twice the 6x6 matrix).
//   B::N                 static constexpr int: the sphere at infinity is S^{N-1} in R^N.   PU21: 4.  ON1<5>: 5.
//   B::Point             = SpherePoint<B::N> (sphere.h): a point of S^{N-1} in doubles.   Legacy P4, P5.
//   B::key_size          static constexpr int: the number of integers in an exact key.    PU21: 12.  ON1<5>: 6.
//   B::Key               = std::array<int64_t, B::key_size>: the exact key of an element, which determines it (the
//                          lattices act freely on the hyperbolic space with base point o, and the key is o's image).
//                          PU21: the third column, normalised (see normal), 3 entries x 4 coefficients.
//                          ON1<5>: the last column of the doubled matrix.
//   B::packed_size       static constexpr int: int32 words per stored element.             PU21: 36.  ON1<5>: 36.
//   B::DMat, B::DVec     the matrix of an element and a vector it acts on, in doubles.
//                          PU21: the legacy D3 and C3 (std::complex<double>).  ON1<5>: the legacy D6 and V6.
//
// EXACT OPERATIONS                                                   legacy PU21 (S^3)         legacy ON1<5> (S^4)
//
//   static Elem identity();                                          mid()                     m6id()
//   static Elem mul(const Elem& a, const Elem& b);   // a*b, NOT     mmul(a, b)                m6mul(a, b)
//                                                    // normalised
//   static Elem inv(const Elem& a);                  // NOT normal.  minv(a)                   m6inv(a)
//   static Elem normal(const Elem& a);               // the stored   mnormal(a): times the     a (elements are not
//                                                    // representative  zeta^k that makes the   projective here)
//                                                    //                 (3,3) entry least
//   static bool eq(const Elem& a, const Elem& b);    // the same     meq(a, b)                 m6eq(a, b)
//                                                    // representative (bitwise)
//   static bool same(const Elem& a, const Elem& b);  // the same     mscalar(mmul(a, minv(b)))   m6eq(a, b)
//                                                    // group element    >= 0
//   static int64_t max_coefficient(const Elem& a);   // growth guard maxcoef(a) (zipcore.h)    m6maxcoef(a)
//   static Key key(const Elem& a);                   // of a         the key prodkey(a, mid(),   the last column of a
//                                                    //                 ...) computes
//   static Key product_key(const Elem& h, const Elem& s);            the key prodkey(h, s, ...)  the last column of
//                                                    // of h*s, maybe     computes (9 products)    m6mul(h, s)
//                                                    // without the whole product
//   static double displacement(const Elem& a);       // d(o, a o)    hdist(a.e[2][2]), for a   m6dist(a)
//                                                    //                 normalised a
//   static double key_displacement(const Key& k);    // d(o, a o)    hdist of the normalised   acosh(k[5]/2), as
//                                                    // from the key    third entry, as         m6dist
//                                                    //                 enumerate() does
//   static void pack(const Elem& a, int32_t* out);   // packed_size  the Store's mat layout    Store6's mat layout
//                                                    // words; aborts   and its 2^28 guard      and its int32 guard
//                                                    // past the guard
//   static Elem unpack(const int32_t* in);                           Store::load               Store6::load
//   static Key packed_key(const int32_t* in);        // the key of   Store::keyof              Store6::col
//                                                    // a packed element
//
// DOUBLE-PRECISION OPERATIONS
//
//   static DMat to_double(const Elem& a);                            toD(a)                    toD6(a)
//   static DVec act(const DMat& m, const DVec& v);                   mv(m, v)                  mv6(m, v)
//   static Point boundary(const DVec& v);            // a null       spoint(v)                 spoint5(v)
//                                                    // vector's point of S^{N-1}
//   static DVec attracting(const Elem& t);           // the dominant attracting(t)            attracting6(t)
//                                                    // eigenvector
//
// GENERIC FUNCTIONS (not part of a backend): chord, segment_distance (sphere.h) replace d4/d5 and segd4/segd5, and
// the renderer's fingerprint of a key (src/core/hash.h) replaces fingerprint/fingerprint6.
//
// ------------------------------------------------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "geometry/sphere.h"

namespace zippers {

// is_backend<B>(): true if B has every member above with the right signature. A backend's header ends with
//     static_assert(zippers::is_backend<MyBackend>(), "MyBackend is not a complete backend");
// and each requirement that fails gives its own message here.
template <class B>
constexpr bool is_backend()
{
    using Elem = typename B::Elem;
    using Key = typename B::Key;
    using DMat = typename B::DMat;
    using DVec = typename B::DVec;
    using Point = typename B::Point;
    constexpr int N = B::N;
    constexpr int key_size = B::key_size;
    constexpr int packed_size = B::packed_size;

    static_assert(N >= 2, "backend: N, the dimension of R^N containing the sphere at infinity, is at least 2");
    static_assert(key_size >= 1 && packed_size >= 1, "backend: key_size and packed_size are positive");
    static_assert(std::is_same<Key, std::array<int64_t, key_size>>::value,
                  "backend: Key must be std::array<int64_t, key_size>");
    static_assert(std::is_same<Point, SpherePoint<N>>::value, "backend: Point must be SpherePoint<N>");
    static_assert(std::is_trivially_copyable<Elem>::value, "backend: Elem must be trivially copyable");

#define ZIPPERS_E std::declval<const Elem&>()
#define ZIPPERS_K std::declval<const Key&>()
    static_assert(std::is_same<decltype(B::identity()), Elem>::value, "backend: Elem identity()");
    static_assert(std::is_same<decltype(B::mul(ZIPPERS_E, ZIPPERS_E)), Elem>::value,
                  "backend: Elem mul(const Elem&, const Elem&)");
    static_assert(std::is_same<decltype(B::inv(ZIPPERS_E)), Elem>::value, "backend: Elem inv(const Elem&)");
    static_assert(std::is_same<decltype(B::normal(ZIPPERS_E)), Elem>::value, "backend: Elem normal(const Elem&)");
    static_assert(std::is_same<decltype(B::eq(ZIPPERS_E, ZIPPERS_E)), bool>::value,
                  "backend: bool eq(const Elem&, const Elem&)");
    static_assert(std::is_same<decltype(B::same(ZIPPERS_E, ZIPPERS_E)), bool>::value,
                  "backend: bool same(const Elem&, const Elem&)");
    static_assert(std::is_same<decltype(B::max_coefficient(ZIPPERS_E)), int64_t>::value,
                  "backend: int64_t max_coefficient(const Elem&)");
    static_assert(std::is_same<decltype(B::key(ZIPPERS_E)), Key>::value, "backend: Key key(const Elem&)");
    static_assert(std::is_same<decltype(B::product_key(ZIPPERS_E, ZIPPERS_E)), Key>::value,
                  "backend: Key product_key(const Elem&, const Elem&)");
    static_assert(std::is_same<decltype(B::displacement(ZIPPERS_E)), double>::value,
                  "backend: double displacement(const Elem&)");
    static_assert(std::is_same<decltype(B::key_displacement(ZIPPERS_K)), double>::value,
                  "backend: double key_displacement(const Key&)");
    static_assert(std::is_same<decltype(B::pack(ZIPPERS_E, std::declval<int32_t*>())), void>::value,
                  "backend: void pack(const Elem&, int32_t*)");
    static_assert(std::is_same<decltype(B::unpack(std::declval<const int32_t*>())), Elem>::value,
                  "backend: Elem unpack(const int32_t*)");
    static_assert(std::is_same<decltype(B::packed_key(std::declval<const int32_t*>())), Key>::value,
                  "backend: Key packed_key(const int32_t*)");
    static_assert(std::is_same<decltype(B::to_double(ZIPPERS_E)), DMat>::value,
                  "backend: DMat to_double(const Elem&)");
    static_assert(std::is_same<decltype(B::act(std::declval<const DMat&>(), std::declval<const DVec&>())), DVec>::value,
                  "backend: DVec act(const DMat&, const DVec&)");
    static_assert(std::is_same<decltype(B::boundary(std::declval<const DVec&>())), Point>::value,
                  "backend: Point boundary(const DVec&)");
    static_assert(std::is_same<decltype(B::attracting(ZIPPERS_E)), DVec>::value,
                  "backend: DVec attracting(const Elem&)");
#undef ZIPPERS_E
#undef ZIPPERS_K
    return true;
}

}  // namespace zippers
