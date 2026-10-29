// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_analysis_s3: the relations (src/analysis/relations.h) and the linking numbers (src/analysis/linking.h) agree
// BIT FOR BIT with their legacy originals in legacy/s3/link3.cc and legacy/s3/zipcore.h (far_pole), which are included
// here, with main renamed, in the global namespace:
//   - free_reduce, cyclic_reduce, canonical: random words over random inverse maps (involutions with and without
//     fixed points, arbitrary maps, the letters of the stored files), and words made to cancel;
//   - find_relations: the letters of the three stored files, with radii, lengths and counts around those of link3
//     (7.5, 6, 30 by default; 20 relations in the references), and random sets of letters closed under inversion;
//   - crossings and gauss_lk: random closed polylines (point clouds, random walks, the selftest's links), degenerate
//     ones (fewer than two points, a point repeated, collinear points, points on a grid), the stereographic images of
//     loops of the zippers; random_rotation (the rotation, and the generator's state after it) and rotate;
//   - minsep, separation, far_pole (with two kinds of generator) and linking: loops gamma_w and h delta_w' of the stored
//     files (the 13 linked pairs among them), at several accuracies, and random point sets; every result and the
//     generator's state after each call;
//   - draw_pair: the PNG files, with link3's angles and others.
// Built with the flags of the programs (-O2), under which the results must agree. ZIPPERS_REPO is the repository.

#define main legacy_main
#include "link3.cc"                      // legacy/s3: csgroup.h, zipcore.h and link3.cc
#undef main

#include "analysis/linking.h"
#include "analysis/relations.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "geometry/pu21/pu21.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s3.h"

#include "render_test_support.h"

#include <map>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::PU21 NB;
// The test's own render policy: cs::RenderPolicy without the look-ahead through one-letter roofs, which its
// sides turns on (render/renderer.h) and the legacy renderer does not have; so that the comparisons below
// are with the legacy code (the look-ahead is tested in test_lookthrough_cs.cc). No tips either, as cs::RenderPolicy.
struct NoLookThroughPolicy : z::cs::RenderPolicy {
    static void sides(const z::Letters<NB>&, z::Side<NB>&, z::Side<NB>&) {}       // neither: the legacy drawing
};
typedef NoLookThroughPolicy NP;
using rtest::check;
using rtest::gauss;
using rtest::note;
using rtest::rnd;
using rtest::same_bytes;
using rtest::same_vector_bytes;
using rtest::section;
using rtest::uniform;

static_assert(sizeof(P4) == sizeof(z::SpherePoint<4>), "the two points have the same layout");

static const char* const FILES3[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
static std::string repo_file(const char* f) { return std::string(ZIPPERS_REPO) + "/legacy/s3/" + f; }

typedef std::vector<P4> LPts;
typedef std::vector<z::SpherePoint<4>> NPts;

static LPts to_legacy(const NPts& x) { LPts y(x.size()); if (!x.empty()) memcpy(y.data(), x.data(), x.size()*sizeof(P4)); return y; }

// a random point of S^3
static z::SpherePoint<4> random_point()
{
    z::SpherePoint<4> p;
    double s = 0;
    for (double& x : p.x) { x = gauss(); s += x*x; }
    s = std::sqrt(s);
    for (double& x : p.x) x /= s;
    return p;
}

// the same from the generator g
static z::SpherePoint<4> random_point_g(std::mt19937_64& g)
{
    std::normal_distribution<double> N(0, 1);
    z::SpherePoint<4> p;
    double s = 0;
    for (double& x : p.x) { x = N(g); s += x*x; }
    s = std::sqrt(s);
    for (double& x : p.x) x /= s;
    return p;
}

// a random closed curve on S^3: a random walk of n points with steps of about `step`, near the point c
static NPts random_curve(size_t n, double step, const z::SpherePoint<4>& c, double spread)
{
    NPts X;
    z::SpherePoint<4> p = c;
    for (double& x : p.x) x += spread*gauss();
    for (size_t i = 0; i < n; i++) {
        double s = 0;
        for (double x : p.x) s += x*x;
        s = std::sqrt(s);
        z::SpherePoint<4> q;
        for (int k = 0; k < 4; k++) q.x[k] = p.x[k]/s;
        X.push_back(q);
        for (double& x : p.x) x += step*gauss();
    }
    return X;
}

static std::string word_text(const std::vector<int>& w)
{
    std::string s;
    for (int x : w) s += (s.empty() ? "" : " ") + std::to_string(x);
    return s;
}

// ---- the worlds of the stored files ----------------------------------------------------------------------------------

struct Worlds {
    std::string fn;
    World L;                              // legacy
    z::World<NB> N;
};

static void prepare_both(Worlds& w, const std::string& fn)
{
    w.fn = fn;
    Options opt;
    opt.subst = fn;
    rtest::capture_stderr([&] { prepare(opt, w.L); });
    rtest::capture_stderr([&] { z::prepare<NP>(fn.c_str(), w.N); });
}

// ---- comparisons -----------------------------------------------------------------------------------------------------

static bool same_result(const LinkResult& a, const z::LinkResult& b)
{
    return a.lk == b.lk && a.agree == b.agree && a.ncross == b.ncross && same_bytes(a.sep, b.sep) &&
           same_bytes(a.poledist, b.poledist) && same_bytes(a.pole, b.pole);
}

// random closed polylines of R^3 of several kinds
static std::vector<V3> random_polyline(int kind, size_t n, double scale)
{
    std::vector<V3> X;
    V3 c = {uniform(-1, 1)*scale, uniform(-1, 1)*scale, uniform(-1, 1)*scale};
    V3 p = c;
    for (size_t i = 0; i < n; i++) {
        if (kind == 0) X.push_back({c[0] + scale*gauss(), c[1] + scale*gauss(), c[2] + scale*gauss()});   // a cloud
        else if (kind == 1) {                                                                            // a walk
            X.push_back(p);
            for (double& x : p) x += 0.1*scale*gauss();
        } else if (kind == 2) {                                                                          // a knot
            double t = 2*PI*i/n, a = rnd(1, 3), b = rnd(1, 4);
            X.push_back({c[0] + scale*(std::cos(a*t) + 0.3*std::cos(b*t)), c[1] + scale*(std::sin(a*t) + 0.3*std::sin(b*t)), c[2] + scale*0.5*std::sin(b*t)});
        } else {                                                                                         // a grid
            X.push_back({(double)rnd(-4, 4), (double)rnd(-4, 4), (double)rnd(-2, 2)});
        }
    }
    return X;
}

// ----------------------------------------------------------------------------------------------------------------------

int main()
{
    double t_start = z::now();
    setup();                               // legacy: F', u', v', b', j, a1, a2, a3
    z::cs::setup();
    printf("test_analysis_s3: src/analysis/relations.h and linking.h against legacy/s3, bit for bit\n");
    rtest::TempDir tmp;

    Worlds W[3];
    for (int f = 0; f < 3; f++) prepare_both(W[f], repo_file(FILES3[f]));

    // ---- relations ----
    section("free_reduce, cyclic_reduce, canonical: random words, random inverse maps");
    {
        long kinds[4] = {0, 0, 0, 0};
        for (int t = 0; t < 60000; t++) {
            Letters a;
            z::Letters<NB> b;
            int kind = t % 4;
            int n = kind == 3 ? 36 : (int)rnd(1, 40);
            std::vector<int> inv(n);
            if (kind == 0 || kind == 1) {                  // an involution: random pairs, and fixed points (kind 1)
                std::vector<int> perm(n);
                for (int i = 0; i < n; i++) perm[i] = i;
                std::shuffle(perm.begin(), perm.end(), rtest::rng);
                int i = 0;
                for (; i + 1 < n; i += 2) {
                    if (kind == 1 && rnd(0, 2) == 0) { inv[perm[i]] = perm[i]; i--; continue; }
                    inv[perm[i]] = perm[i + 1]; inv[perm[i + 1]] = perm[i];
                }
                for (; i < n; i++) inv[perm[i]] = perm[i];
            } else if (kind == 2) {                        // any map
                for (int& x : inv) x = (int)rnd(0, n - 1);
            } else inv = W[t % 3].N.L.inv;                 // the letters of a stored file
            a.inv = inv;
            b.inv = inv;
            kinds[kind]++;
            std::vector<int> w;
            int shape = (int)rnd(0, 5);
            int len = (int)rnd(0, 30);
            for (int i = 0; i < len; i++) w.push_back((int)rnd(0, n - 1));
            if (shape == 1) { std::vector<int> u = w; for (int i = (int)u.size() - 1; i >= 0; i--) w.push_back(inv[u[i]]); }   // w w^-1
            else if (shape == 2 && !w.empty()) {                                                                          // u v u^-1
                std::vector<int> u(w.begin(), w.begin() + rnd(0, (int64_t)w.size() - 1));
                for (int i = (int)u.size() - 1; i >= 0; i--) w.push_back(inv[u[i]]);
            } else if (shape == 3) { std::vector<int> u = w; for (int k = 0; k < 3; k++) w.insert(w.end(), u.begin(), u.end()); }   // a power
            else if (shape == 4 && n > 1) for (int& x : w) x = (int)rnd(0, 1);                                         // two letters
            check(free_reduce(w, a) == z::free_reduce(w, b), "free_reduce of %s", word_text(w).c_str());
            check(cyclic_reduce(w, a) == z::cyclic_reduce(w, b), "cyclic_reduce of %s", word_text(w).c_str());
            check(canonical(w, a) == z::canonical(w, b), "canonical of %s", word_text(w).c_str());
        }
        note("(20,000 words of each; over involutions %ld, involutions with fixed points %ld, any maps %ld, stored letters %ld)",
             kinds[0], kinds[1], kinds[2], kinds[3]);
    }

    section("find_relations: the letters of the stored files; radii, lengths and counts");
    {
        struct Case { double Rr; size_t maxlen, maxcount; };
        std::vector<Case> cases = {{7.5, 6, 30}, {7.5, 6, 20}, {7.5, 6, 1000}, {6, 6, 30}, {4.5, 4, 1000}, {3, 6, 1000},
                                   {7.5, 4, 5}, {5, 5, 50}, {7.5, 2, 30}, {7.5, 3, 1000}, {7.5, 1, 10}, {0, 6, 30},
                                   {7.5, 6, 0}, {7.5, 7, 40}, {2.6, 6, 1000}};
        long nrel = 0;
        for (int f = 0; f < 3; f++)
            for (const Case& c : cases) {
                std::vector<std::vector<int>> a = find_relations(W[f].L.L, c.Rr, c.maxlen, c.maxcount);
                std::vector<std::vector<int>> b = z::find_relations<z::cs::FVal>(W[f].N.L, c.Rr, c.maxlen, c.maxcount);
                check(a == b, "%s, Rr %g, maxlen %zu, maxcount %zu: %zu relations, legacy %zu", FILES3[f], c.Rr, c.maxlen,
                      c.maxcount, b.size(), a.size());
                nrel += (long)b.size();
                if (f == 2 && c.Rr == 7.5 && c.maxlen == 6 && c.maxcount == 20)
                    note("(t3 as link3 --nrel 20: %zu relations, the first %s, the last %s)", b.size(),
                         b.empty() ? "-" : word_text(b[0]).c_str(), b.empty() ? "-" : word_text(b.back()).c_str());
            }
        note("(%zu cases, %ld relations in all)", 3*cases.size(), nrel);
    }

    section("find_relations: random sets of letters closed under inversion");
    {
        long nrel = 0, ncase = 0;
        for (int t = 0; t < 40; t++) {
            Letters a;
            z::Letters<NB> b;
            int k = (int)rnd(1, 5);
            std::vector<std::string> words;
            for (int i = 0; i < k; i++) {
                int len = (int)rnd(0, 3);
                std::string w;
                for (int j = 0; j < len; j++) w += (w.empty() ? "" : " ") + std::string(rnd(0, 1) ? "a" : "A") + std::to_string(rnd(1, 3));
                words.push_back(w.empty() ? "1" : w);
            }
            for (const std::string& w : words) {                    // each word and its inverse
                M3 g = parse_word(w);
                a.s.push_back(g); a.s.push_back(mnormal(minv(g)));
                z::M3 h = z::cs::parse_word(w);
                b.s.push_back(h); b.s.push_back(NB::normal(NB::inv(h)));
            }
            size_t n = a.s.size();
            a.inv.assign(n, -1);
            for (size_t j = 0; j < n; j++) {
                M3 x = mnormal(minv(a.s[j]));
                for (size_t m = 0; m < n; m++) if (meq(x, a.s[m])) a.inv[j] = (int)m;
            }
            b.inv = a.inv;
            check(same_vector_bytes(a.s, b.s), "the random letters %d", t);
            double Rr = uniform(3, 6.5);
            size_t maxlen = (size_t)rnd(2, 6), maxcount = (size_t)rnd(1, 60);
            std::vector<std::vector<int>> ra = find_relations(a, Rr, maxlen, maxcount);
            std::vector<std::vector<int>> rb = z::find_relations<z::cs::FVal>(b, Rr, maxlen, maxcount);
            std::string ws;
            for (const std::string& w : words) ws += "[" + w + "]";
            check(ra == rb, "letters %s, Rr %g, maxlen %zu, maxcount %zu: %zu relations, legacy %zu", ws.c_str(), Rr, maxlen,
                  maxcount, rb.size(), ra.size());
            nrel += (long)rb.size();
            ncase++;
        }
        note("(%ld cases, %ld relations)", ncase, nrel);
    }

    // ---- the loops of the stored files, for what follows: gamma_w at 1 and h delta_w', h in B(2.6) ----
    std::vector<std::vector<int>> rels = z::find_relations<z::cs::FVal>(W[2].N.L, 7.5, 6, 20);
    std::vector<z::M3> H;
    z::cs::find_roots(2.6, H);
    auto gamma = [&](int f, size_t a, double acc) { size_t forced = 0; return z::loop_points(W[f].N.plus, W[f].N.L, NB::identity(), rels[a], acc, forced); };
    auto delta = [&](int f, size_t b, size_t hi, double acc) { size_t forced = 0; return z::loop_points(W[f].N.minus, W[f].N.L, H[hi], rels[b], acc, forced); };
    struct Hit { size_t a, b, hi; };
    const std::vector<Hit> hits = {{4, 10, 1}, {4, 12, 1}, {4, 17, 1}, {4, 14, 6}, {2, 10, 13}, {4, 4, 13}, {10, 3, 13},
                                   {10, 4, 13}, {13, 10, 13}, {17, 4, 13}, {2, 12, 16}, {12, 4, 16}, {13, 12, 16}};

    // ---- polylines in R^3 ----
    section("crossings: random closed polylines, degenerate ones, the stereographic images of loops");
    {
        long n = 0, nonzero = 0, crossed = 0;
        for (int t = 0; t < 3000; t++) {
            int ka = (int)rnd(0, 3), kb = (int)rnd(0, 3);
            size_t na = (size_t)rnd(0, 250), nb = (size_t)rnd(0, 250);
            // A without length makes cells of 1e-12, 4096 a side, and each segment of B scans its whole box of cells:
            // keep B small then (A of two points of the grid may coincide)
            bool nolength = t % 97 == 1 || t % 50 == 0;
            if (t % 50 == 0) na = (size_t)rnd(0, 2);
            if (nolength) nb = (size_t)rnd(0, 4);
            double scale = std::exp(uniform(-3, 3));
            std::vector<V3> A = random_polyline(ka, na, scale), B = random_polyline(kb, nb, scale);
            if (t % 97 == 1 && !A.empty()) for (V3& p : A) p = A[0];              // one point repeated: no length
            if (t % 97 == 2 && !B.empty()) for (V3& p : B) p = B[0];
            if (t % 97 == 3) for (V3& p : A) p[1] = 2*p[0] + 1;                    // collinear in the plane
            long ab, ba, nc, ab2, ba2, nc2;
            crossings(A, B, ab, ba, nc);
            z::crossings(A, B, ab2, ba2, nc2);
            check(ab == ab2 && ba == ba2 && nc == nc2, "crossings of %zu and %zu points (kinds %d %d): %+ld/%+ld (%ld), legacy %+ld/%+ld (%ld)",
                  na, nb, ka, kb, ab2, ba2, nc2, ab, ba, nc);
            n++;
            nonzero += ab2 != 0;
            crossed += nc2 > 0;
        }
        // the stereographic images of loops of the zippers, in random rotations (as linking computes them)
        for (int t = 0; t < 40; t++) {
            const Hit& x = hits[t % hits.size()];
            NPts A = gamma(2, x.a, 0.05), B = delta(2, x.b, x.hi, 0.05);
            double pole[4], F[4][4], R[9];
            std::mt19937_64 g((uint64_t)t);
            far_pole(to_legacy(A), to_legacy(B), g, pole);
            frame_with_pole(pole, F);
            random_rotation(g, R);
            std::vector<V3> a = rotate(stereo(to_legacy(A), F), R), b = rotate(stereo(to_legacy(B), F), R);
            long ab, ba, nc, ab2, ba2, nc2;
            crossings(a, b, ab, ba, nc);
            z::crossings(a, b, ab2, ba2, nc2);
            check(ab == ab2 && ba == ba2 && nc == nc2, "crossings of the loops w%zu, h%zu w%zu: %+ld/%+ld (%ld), legacy %+ld/%+ld (%ld)",
                  x.a, x.hi, x.b, ab2, ba2, nc2, ab, ba, nc);
            n++;
            nonzero += ab2 != 0;
            crossed += nc2 > 0;
        }
        note("(%ld pairs: %ld with crossings, %ld with a nonzero count of A over B)", n, crossed, nonzero);
    }

    section("gauss_lk: random closed polylines, the selftest's links, degenerate ones");
    {
        long n = 0, few = 0;
        auto circle = [](V3 c, int plane, double r, int k, double phase) {
            std::vector<V3> X;
            for (int i = 0; i < k; i++) {
                double t = 2*PI*i/k + phase;
                V3 p = c;
                if (plane == 0) { p[0] += r*std::cos(t); p[1] += r*std::sin(t); }
                else { p[0] += r*std::cos(t); p[2] += r*std::sin(t); }
                X.push_back(p);
            }
            return X;
        };
        std::vector<std::pair<std::vector<V3>, std::vector<V3>>> cases = {
            {circle({0,0,0}, 0, 1, 300, 0), circle({1,0,0}, 1, 1, 300, 0.1)},
            {circle({0,0,0}, 0, 1, 300, 0), circle({3,0,0}, 1, 1, 300, 0.1)},
            {circle({0,0,0}, 0, 1, 7, 0), circle({1,0,0}, 1, 1, 5, 0.1)}};
        for (int t = 0; t < 400; t++)
            cases.push_back({random_polyline((int)rnd(0, 3), (size_t)rnd(0, 60), 1), random_polyline((int)rnd(0, 3), (size_t)rnd(0, 60), 1)});
        for (const auto& c : cases) {
            double a = gauss_lk(c.first, c.second), b = z::gauss_lk(c.first, c.second);
            check(same_bytes(a, b), "gauss_lk of %zu and %zu points: %.17g, legacy %.17g", c.first.size(), c.second.size(), b, a);
            n++;
            few += c.first.size() < 2 || c.second.size() < 2;
        }
        note("(%ld pairs, %ld with a polyline of fewer than 2 points; the grid's polylines repeat points)", n, few);
    }

    section("random_rotation (the rotation and the generator after it), rotate");
    {
        long n = 0;
        for (int t = 0; t < 2000; t++) {
            uint64_t seed = t < 10 ? (uint64_t)t : (uint64_t)rnd(0, INT64_MAX);
            std::mt19937_64 ga(seed), gb(seed);
            int k = (int)rnd(1, 10);
            for (int i = 0; i < k; i++) {
                double Ra[9], Rb[9];
                random_rotation(ga, Ra);
                z::random_rotation(gb, Rb);
                check(same_bytes(Ra, Rb) && ga == gb, "random_rotation %d of the seed %llu", i, (unsigned long long)seed);
                n++;
                if (i == 0) {
                    std::vector<V3> X = random_polyline((int)rnd(0, 3), (size_t)rnd(0, 50), std::exp(uniform(-3, 3)));
                    check(same_vector_bytes(rotate(X, Ra), z::rotate(X, Rb)), "rotate of %zu points", X.size());
                    n++;
                }
            }
        }
        note("(%ld comparisons)", n);
    }

    // ---- point sets on S^3 ----
    section("minsep, separation: random point sets, loops of the zippers");
    {
        long n = 0;
        for (int t = 0; t < 200; t++) {
            z::SpherePoint<4> c = random_point();
            NPts A = random_curve((size_t)rnd(0, 800), uniform(0.001, 0.05), c, uniform(0, 0.5));
            NPts B = random_curve((size_t)rnd(0, 800), uniform(0.001, 0.05), c, uniform(0, 0.5));
            LPts la = to_legacy(A), lb = to_legacy(B);
            for (double h : {0.05, uniform(0.001, 0.3), uniform(0.3, t % 10 ? 1.2 : 2.5)}) {
                double a = minsep(la, lb, h), b = z::minsep(A, B, h);
                check(same_bytes(a, b), "minsep of %zu and %zu points below %g: %.17g, legacy %.17g", A.size(), B.size(), h, b, a);
                n++;
            }
            double a = separation(la, lb), b = z::separation(A, B);
            check(same_bytes(a, b), "separation of %zu and %zu points: %.17g, legacy %.17g", A.size(), B.size(), b, a);
            n++;
        }
        for (int f = 0; f < 3; f++)
            for (size_t i = 0; i < hits.size(); i += (f == 2 ? 2 : 5)) {
                const Hit& x = hits[i];
                NPts A = gamma(f, x.a, 0.05), B = delta(f, x.b, x.hi, 0.05);
                LPts la = to_legacy(A), lb = to_legacy(B);
                for (double h : {0.05, 0.02, 0.1}) {
                    double a = minsep(la, lb, h), b = z::minsep(A, B, h);
                    check(same_bytes(a, b), "minsep of the loops w%zu, h%zu w%zu of %s below %g: %.17g, legacy %.17g", x.a, x.hi, x.b, FILES3[f], h, b, a);
                    n++;
                }
                double a = separation(la, lb), b = z::separation(A, B);
                check(same_bytes(a, b), "separation of the loops w%zu, h%zu w%zu of %s: %.17g, legacy %.17g", x.a, x.hi, x.b, FILES3[f], b, a);
                n++;
            }
        // constructed (no random numbers): one point, and one point in each of the 81 cells around its cell, just across
        // the boundaries, nearer than h; only the loop over all 81 cells finds it
        long found81 = 0;
        for (int m = 0; m < 81; m++) {
            const double h = 0.25;
            z::SpherePoint<4> p, q;
            for (int c = 0; c < 4; c++) {
                int d = (m/(c == 0 ? 1 : c == 1 ? 3 : c == 2 ? 9 : 27)) % 3 - 1;
                p.x[c] = d > 0 ? 0.24 : d < 0 ? 0.01 : 0.125;
                q.x[c] = d > 0 ? 0.26 : d < 0 ? -0.01 : 0.125;
            }
            NPts A = {p}, B = {q};
            double a = minsep(to_legacy(A), to_legacy(B), h), b = z::minsep(A, B, h);
            check(same_bytes(a, b) && b < h, "minsep, a point in the neighbouring cell %d of 81: %.17g, legacy %.17g", m, b, a);
            n++;
            found81 += b < h;
        }
        note("(%ld comparisons; constructed: a point in each of the 81 neighbouring cells, %ld found)", n, found81);
    }

    section("far_pole: loops of the zippers and random point sets; two kinds of generator");
    {
        long n = 0, close = 0;
        for (int t = 0; t < 310; t++) {
            NPts A, B;
            if (t < 60) { const Hit& x = hits[t % hits.size()]; A = gamma(2, x.a, t < 30 ? 0.05 : 0.03); B = delta(2, x.b, x.hi, t < 30 ? 0.05 : 0.03); }
            else if (t >= 300) {                                              // dense on S^3: no pole 0.15 away
                for (int i = 0; i < 8000; i++) { A.push_back(random_point()); B.push_back(random_point()); }
            } else {
                z::SpherePoint<4> c = random_point();
                double spread = t % 5 == 0 ? 2.0 : uniform(0, 0.6);         // spread 2: curves all over S^3
                A = random_curve((size_t)rnd(1, 3000), uniform(0.001, 0.2), c, spread);
                B = random_curve((size_t)rnd(1, 3000), uniform(0.001, 0.2), c, spread);
            }
            LPts la = to_legacy(A), lb = to_legacy(B);
            uint64_t seed = (uint64_t)rnd(0, INT64_MAX);
            double pa[4], pb[4];
            memset(pa, 0x22, sizeof pa); memset(pb, 0x22, sizeof pb);
            double da, db;
            bool st;
            if (t % 4 == 3) {                                                 // a 32-bit generator
                std::mt19937 ga((uint32_t)seed), gb((uint32_t)seed);
                da = far_pole(la, lb, ga, pa); db = z::far_pole(A, B, gb, pb);
                st = ga == gb;
            } else {
                std::mt19937_64 ga(seed), gb(seed);
                da = far_pole(la, lb, ga, pa); db = z::far_pole(A, B, gb, pb);
                st = ga == gb;
            }
            check(same_bytes(da, db) && same_bytes(pa, pb) && st, "far_pole of %zu and %zu points: %.17g, legacy %.17g", A.size(), B.size(), db, da);
            n++;
            close += db < 0.15;
        }
        // uniform point sets of 2,400 to 5,000 points, whose best poles are about 0.15 to 0.2 away: the search stops at the
        // first candidate at least 0.15 away, and there are farther ones after it (their own generator)
        long near = 0;
        std::mt19937_64 g(424242);
        for (int t = 0; t < 30; t++) {
            NPts A, B;
            size_t na = (size_t)std::uniform_int_distribution<int>(1200, 2500)(g), nb = (size_t)std::uniform_int_distribution<int>(1200, 2500)(g);
            for (size_t i = 0; i < na; i++) A.push_back(random_point_g(g));
            for (size_t i = 0; i < nb; i++) B.push_back(random_point_g(g));
            LPts la = to_legacy(A), lb = to_legacy(B);
            std::mt19937_64 ga((uint64_t)t), gb((uint64_t)t);
            double pa[4], pb[4];
            memset(pa, 0x22, sizeof pa); memset(pb, 0x22, sizeof pb);
            double da = far_pole(la, lb, ga, pa), db = z::far_pole(A, B, gb, pb);
            check(same_bytes(da, db) && same_bytes(pa, pb) && ga == gb, "far_pole of %zu and %zu uniform points: %.17g, legacy %.17g", na, nb, db, da);
            n++;
            near += db >= 0.15 && db < 0.2;
        }
        note("(%ld cases, %ld whose best pole is nearer than 0.15; of 30 uniform sets, %ld stop at a pole 0.15 to 0.2 away)", n, close, near);
    }

    section("linking: the 13 linked pairs and others, several accuracies, separation hints; the generator after");
    {
        long n = 0, nonzero = 0, disagree = 0;
        // the hints: 0.05 as link3 mostly calls it, 0.1, and -1 and 0 for the full separation. The full separation of
        // loops far apart searches with cells up to 0.32, where minsep compares nearly every pair of points: it is made
        // on the loops at accuracy 0.1, and at 0.02 on the one pair that is close (w10, h13 w3: 0.0197 apart)
        struct Case { int f; Hit x; double acc; std::vector<double> hints; };
        std::vector<Case> cases;
        for (size_t i = 0; i < hits.size(); i++) {
            cases.push_back({2, hits[i], 0.05, {0.05, 0.05, 0.1}});
            cases.push_back({2, hits[i], 0.1, {-1.0, 0.0, 0.05}});
            if (i < 3) cases.push_back({2, hits[i], 0.02, {0.05, 0.1}});
        }
        cases.push_back({2, hits[6], 0.02, {-1.0, 0.05}});
        cases.push_back({2, hits[0], 0.01, {0.05}});
        for (int t = 0; t < 40; t++)
            cases.push_back({(int)(t % 3), {(size_t)rnd(0, 19), (size_t)rnd(0, 19), (size_t)rnd(0, 18)}, t < 30 ? 0.05 : 0.1,
                             t < 30 ? std::vector<double>{0.05, 0.1} : std::vector<double>{-1.0, 0.0}});
        for (const Case& c : cases) {
            NPts A = gamma(c.f, c.x.a, c.acc), B = delta(c.f, c.x.b, c.x.hi, c.acc);
            LPts la = to_legacy(A), lb = to_legacy(B);
            uint64_t seed = (uint64_t)rnd(0, INT64_MAX);
            std::mt19937_64 ga(seed), gb(seed);
            for (double hint : c.hints) {
                LinkResult a = linking(la, lb, ga, hint);
                z::LinkResult b = z::linking(A, B, gb, hint);
                check(same_result(a, b) && ga == gb, "linking of w%zu, h%zu w%zu of %s at acc %g (hint %g): lk %+ld %s, legacy %+ld %s",
                      c.x.a, c.x.hi, c.x.b, FILES3[c.f], c.acc, hint, b.lk, b.agree ? "agree" : "disagree", a.lk, a.agree ? "agree" : "disagree");
                n++;
                nonzero += b.lk != 0;
                disagree += !b.agree;
            }
        }
        for (int t = 0; t < 40; t++) {                                        // random curves, some of them crossing
            z::SpherePoint<4> c = random_point();
            NPts A = random_curve((size_t)rnd(3, 800), uniform(0.005, 0.1), c, 0.3);
            NPts B = random_curve((size_t)rnd(3, 800), uniform(0.005, 0.1), c, 0.3);
            LPts la = to_legacy(A), lb = to_legacy(B);
            std::mt19937_64 ga((uint64_t)t), gb((uint64_t)t);
            LinkResult a = linking(la, lb, ga, t % 2 ? 0.05 : -1);
            z::LinkResult b = z::linking(A, B, gb, t % 2 ? 0.05 : -1);
            check(same_result(a, b) && ga == gb, "linking of random curves %d: lk %+ld, legacy %+ld", t, b.lk, a.lk);
            n++;
            nonzero += b.lk != 0;
            disagree += !b.agree;
        }
        // two loops that meet (their own generator): great circles of S^3 through one common vertex, in a random
        // rotation of R^4. Where the projections cross at that point, both segments begin there (s = u = 0) and
        // za = zb, which counts as B over A: the two counts may disagree, as they cannot for disjoint loops
        long met = 0, metdis = 0;
        std::mt19937_64 g(777);
        std::normal_distribution<double> N(0, 1);
        for (int t = 0; t < 16; t++) {
            double M[4][4];                                                 // a random orthogonal matrix
            for (int i = 0; i < 4; i++) {
                for (int k = 0; k < 4; k++) M[i][k] = N(g);
                for (int j = 0; j < i; j++) { double d = 0; for (int k = 0; k < 4; k++) d += M[i][k]*M[j][k]; for (int k = 0; k < 4; k++) M[i][k] -= d*M[j][k]; }
                double l = 0;
                for (int k = 0; k < 4; k++) l += M[i][k]*M[i][k];
                l = std::sqrt(l);
                for (int k = 0; k < 4; k++) M[i][k] /= l;
            }
            auto circle = [&](int plane, int k) {                           // through M e_0 at i = 0
                NPts X;
                for (int i = 0; i < k; i++) {
                    double th = 2*PI*i/k, v[4] = {std::cos(th), 0, 0, 0};
                    v[plane] = std::sin(th);
                    z::SpherePoint<4> q;
                    for (int r = 0; r < 4; r++) q.x[r] = M[r][0]*v[0] + M[r][1]*v[1] + M[r][2]*v[2] + M[r][3]*v[3];
                    X.push_back(q);
                }
                return X;
            };
            NPts A = circle(1, 60 + 7*t), B = circle(2 + t % 2, 50 + 5*t);
            LPts la = to_legacy(A), lb = to_legacy(B);
            std::mt19937_64 ga((uint64_t)t + 1000), gb((uint64_t)t + 1000);
            for (int k = 0; k < 3; k++) {
                LinkResult a = linking(la, lb, ga, k == 1 ? -1 : 0.05);
                z::LinkResult b = z::linking(A, B, gb, k == 1 ? -1 : 0.05);
                check(same_result(a, b) && ga == gb, "linking of two great circles that meet (%d): lk %+ld %s, legacy %+ld %s", t,
                      b.lk, b.agree ? "agree" : "disagree", a.lk, a.agree ? "agree" : "disagree");
                n++;
                met++;
                metdis += !b.agree;
            }
        }
        note("(%ld linking numbers: %ld nonzero, %ld with the over/under counts disagreeing; of them %ld of two loops that "
             "meet, %ld disagreeing)", n, nonzero, disagree + metdis, met, metdis);
    }

    section("draw_pair: the PNG files, link3's angles and others");
    {
        long files = 0, bytes = 0;
        for (int t = 0; t < 8; t++) {
            const Hit& x = hits[(size_t)t*5 % hits.size()];
            int f = t < 6 ? 2 : t - 6;
            double acc = t == 0 ? 0.01 : 0.05;
            NPts A = gamma(f, x.a, acc), B = delta(f, x.b, x.hi, acc);
            LPts la = to_legacy(A), lb = to_legacy(B);
            std::mt19937_64 g((uint64_t)t + 100);
            double pole[4];
            far_pole(la, lb, g, pole);
            std::string fa = tmp.file("legacy.png"), fb = tmp.file("new.png");
            if (t % 3 == 0) {                                   // the defaults: yaw 30, pitch 20 (the scan's pictures)
                draw_pair(fa.c_str(), la, lb, pole, W[f].L.plus, W[f].L.L);
                z::draw_pair(fb.c_str(), A, B, pole, W[f].N.plus, W[f].N.L);
            } else {
                double yaw = t % 3 == 1 ? 60.0*(t % 3) : uniform(-180, 180), pitch = t % 3 == 1 ? 15 : uniform(-90, 90);
                draw_pair(fa.c_str(), la, lb, pole, W[f].L.plus, W[f].L.L, yaw, pitch);
                z::draw_pair(fb.c_str(), A, B, pole, W[f].N.plus, W[f].N.L, yaw, pitch);
            }
            std::string a = rtest::read_file(fa), b = rtest::read_file(fb);
            check(a == b && !b.empty(), "draw_pair of w%zu, h%zu w%zu of %s at acc %g: %zu bytes, legacy %zu", x.a, x.hi, x.b, FILES3[f], acc, b.size(), a.size());
            files++;
            bytes += (long)b.size();
            unlink(fa.c_str()); unlink(fb.c_str());
        }
        note("(%ld pictures, %ld bytes)", files, bytes);
    }

    rtest::end_section();
    printf("test_analysis_s3: %ld comparisons with the legacy code, %ld differ (%.1f s)\n", rtest::all_n, rtest::all_bad,
           z::now() - t_start);
    return rtest::all_bad ? 1 : 0;
}
