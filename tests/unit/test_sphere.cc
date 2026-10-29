// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_sphere: zippers::chord and zippers::segment_distance (src/geometry/sphere.h) agree BIT FOR BIT with the legacy
// d4, segd4 (legacy/s3/zipcore.h) when ZIPPERS_TEST_N is 4, and with d5, segd5 (legacy/s4/zipcore5.h) when it is 5.
// (The two legacy headers cannot be included in one program: they define the same names.)
//
// The inputs are random points of the sphere, random points of R^N, nearby points (short chords, as in the
// renderer), and degenerate segments (a = b). Build it with the flags of the programs (-O2), since those are the
// flags under which the results must agree.

#if ZIPPERS_TEST_N == 4
#include "zipcore.h"
typedef P4 LegacyPoint;
static double legacy_chord(const P4& a, const P4& b) { return d4(a, b); }
static double legacy_seg(const P4& p, const P4& a, const P4& b) { return segd4(p, a, b); }
#elif ZIPPERS_TEST_N == 5
#include "zipcore5.h"
typedef P5 LegacyPoint;
static double legacy_chord(const P5& a, const P5& b) { return d5(a, b); }
static double legacy_seg(const P5& p, const P5& a, const P5& b) { return segd5(p, a, b); }
#else
#error "define ZIPPERS_TEST_N as 4 or 5"
#endif

#include "geometry/sphere.h"

#include <cstdio>
#include <cstring>
#include <random>

static const int N = ZIPPERS_TEST_N;
typedef zippers::SpherePoint<N> NewPoint;

static bool same_bits(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

int main()
{
    std::mt19937_64 rng(20261025);
    std::normal_distribution<double> gauss(0, 1);
    std::uniform_real_distribution<double> unif(-1, 1);
    long tested = 0, bad = 0;
    for (int trial = 0; trial < 300000; trial++) {
        LegacyPoint L[3];
        NewPoint P[3];
        int kind = trial % 4;
        for (int j = 0; j < 3; j++) {
            double s = 0;
            for (int i = 0; i < N; i++) {
                double v = kind == 1 ? unif(rng) : gauss(rng);
                if (kind >= 2 && j > 0) v = L[0].x[i] + 1e-3*gauss(rng);       // a short chord near the first point
                L[j].x[i] = v;
                s += v*v;
            }
            if (kind != 1)                                                        // on the sphere
                for (int i = 0; i < N; i++) L[j].x[i] /= std::sqrt(s);
            if (kind == 3 && j == 2) L[2] = L[1];                                 // a degenerate segment
            std::memcpy(P[j].x, L[j].x, sizeof P[j].x);
        }
        double c0 = legacy_chord(L[0], L[1]), c1 = zippers::chord(P[0], P[1]);
        double s0 = legacy_seg(L[0], L[1], L[2]), s1 = zippers::segment_distance(P[0], P[1], P[2]);
        tested += 2;
        if (!same_bits(c0, c1)) { if (bad++ < 5) printf("chord differs: %.17g vs %.17g\n", c0, c1); }
        if (!same_bits(s0, s1)) { if (bad++ < 5) printf("segment distance differs: %.17g vs %.17g\n", s0, s1); }
    }
    printf("test_sphere (N = %d): %ld comparisons with the legacy functions, %ld differ\n", N, tested, bad);
    return bad ? 1 : 0;
}
