// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_backend_contract: src/geometry/backend.h compiles, and is_backend<B>() accepts a backend that has every
// required member. The mock below does no geometry; it only has the right shape. (The real backends have their
// bit-for-bit tests against the legacy code: test_pu21, test_on1.)

#include "geometry/backend.h"

#include <cstdio>

namespace {

struct MockBackend {
    struct Elem { int64_t a[4]; };
    static constexpr int N = 3;
    static constexpr int key_size = 2;
    static constexpr int packed_size = 4;
    using Key = std::array<int64_t, key_size>;
    using Point = zippers::SpherePoint<N>;
    struct DMat { double m[4]; };
    struct DVec { double v[2]; };

    static Elem identity() { return Elem{{1, 0, 0, 1}}; }
    static Elem mul(const Elem& a, const Elem&) { return a; }
    static Elem inv(const Elem& a) { return a; }
    static Elem normal(const Elem& a) { return a; }
    static bool eq(const Elem&, const Elem&) { return true; }
    static bool same(const Elem&, const Elem&) { return true; }
    static int64_t max_coefficient(const Elem&) { return 1; }
    static Key key(const Elem& a) { return Key{{a.a[0], a.a[2]}}; }
    static Key product_key(const Elem& h, const Elem&) { return key(h); }
    static double displacement(const Elem&) { return 0; }
    static double key_displacement(const Key&) { return 0; }
    static void pack(const Elem& a, int32_t* out) { for (int i = 0; i < 4; i++) out[i] = (int32_t)a.a[i]; }
    static Elem unpack(const int32_t* in) { return Elem{{in[0], in[1], in[2], in[3]}}; }
    static Key packed_key(const int32_t* in) { return Key{{in[0], in[2]}}; }
    static DMat to_double(const Elem&) { return DMat{{1, 0, 0, 1}}; }
    static DVec act(const DMat&, const DVec& v) { return v; }
    static Point boundary(const DVec&) { return Point{{1, 0, 0}}; }
    static DVec attracting(const Elem&) { return DVec{{1, 0}}; }
};

static_assert(zippers::is_backend<MockBackend>(), "the mock backend is complete");

}  // namespace

int main()
{
    printf("test_backend_contract: is_backend<> accepts a complete backend\n");
    return 0;
}
