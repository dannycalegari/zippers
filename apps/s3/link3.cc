/*  link3.cc  --  linking numbers of loops in the zippers Z+ and Z- of the Cartwright-Steger
    surface (see zipper3.cc for the arcs, examples/cartwright_steger.h for the data).

    LOOPS.  A relation w = s_{j_1} ... s_{j_m} = 1 among the generators S of K = ker phi gives
    a closed loop in Z+,
        gamma_w = alpha_{j_1} u s_{j_1} alpha_{j_2} u ... u s_{j_1}...s_{j_{m-1}} alpha_{j_m},
    through the vertices t^+, s_{j_1} t^+, ..., and with the arcs of Z- in place of the alpha's a
    loop delta_w in Z-.  Translates h gamma_w, h delta_w are loops too.  Relations are found
    exactly by a breadth-first search of K: every edge outside the search tree closes a cycle;
    they are freely and cyclically reduced, and identified up to rotation and inversion (that
    is, the loops up to translation).

    WHY IT DECIDES THE QUESTION.  If lk(gamma, delta) != 0 for gamma in Z+ and delta in Z-, then
    gamma is not null-homotopic in Z+ (Z+ misses Z- and hence delta, so a null-homotopy in Z+
    would be one in S^3 - delta), and likewise delta in Z-.  A degenerate loop (one through a
    tree-like set) links nothing.

    THE COMPUTATION.  Each loop is a closed polyline: every arc is split, in order and without
    de-duplication, until its chords are below the accuracy.  Two loops are projected
    stereographically from a pole far from both, and the linking number is the sum of the
    signed crossings of one over the other in a random planar projection (the counts "A over
    B" and "B over A" must agree).  It is trusted only when the loops are much farther apart
    than the accuracy and the value is stable under refinement.

    Usage:  link3 selftest                    test the linking-number code on known links
            link3 [options]                   the search (see usage())

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

    The program of legacy/s3/link3.cc on the shared library src/, with the same modes, options, output and
    files. The example's data come from examples/cartwright_steger.h (csgroup.h and zipcore.h there), its render
    policy from examples/cartwright_steger_render.h, the loops from render/arcs.h (zipcore.h), the relations from
    analysis/relations.h and the linking numbers and pictures from analysis/linking.h (this file and zipcore.h there).
    The changes are the includes and the names: cs::setup (setup), World<PU21> with prepare<cs::RenderPolicy> (World,
    prepare), find_relations<cs::FVal> (the label of the example's store), cs::roots_info, cs::find_roots and
    cs::RootInfo, PU21::identity, PU21::mul and PU21::displacement (mid, mmul and hdist of the (3,3) entry), the points
    SpherePoint<4> (P4), s3::V3 and s3::PI; the two options of zipcore.h's Options that link3 reads are here.

    One change of result: the loops of render/arcs.h take the renderer's look-ahead
    through one-letter roofs (render/renderer.h, set_lookthrough). None of the reference runs changes, and no linking
    number in them.

    --stats: the checks of the translates h, the elements of Pi in B(R) for R = --hr
    (examples/cartwright_steger_roots.h: certify_roots), on stderr before anything else is computed: one
    line, and a second when R is within 1e-6 of a shell distance. It compares the checked roots with those of
    roots_info(R), bit for bit. Without --stats nothing that the program prints or writes changes; the usage text has one
    more line (no reference run prints it).
*/

#include "analysis/linking.h"
#include "analysis/relations.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/cartwright_steger_roots.h"
#include "geometry/pu21/pu21.h"
#include "render/arcs.h"
#include "render/world.h"
#include "view/s3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace zippers;
using std::string;
using std::vector;

// the options of zipcore.h's Options that link3 reads
struct Options {
    string subst = "subst_4.00_9.50_t3.txt";
    double acc = 0.01;
};

// ---------------------------------------------------------------------------------------

static int selftest()
{
    std::mt19937_64 rng(1);
    auto circle = [](s3::V3 c, int plane, double r, int n, double phase) {
        vector<s3::V3> X;
        for (int i = 0; i < n; i++) {
            double t = 2*s3::PI*i/n + phase;
            s3::V3 p = c;
            if (plane == 0) { p[0] += r*std::cos(t); p[1] += r*std::sin(t); }
            else { p[0] += r*std::cos(t); p[2] += r*std::sin(t); }
            X.push_back(p);
        }
        return X;
    };
    auto torus = [](double shift, int n) {           // a (1,2) curve on the torus R = 2, r = 1
        vector<s3::V3> X;
        for (int i = 0; i < n; i++) {
            double t = 2*s3::PI*i/n, m = 2*t + shift;
            X.push_back({(2 + std::cos(m))*std::cos(t), (2 + std::cos(m))*std::sin(t), std::sin(m)});
        }
        return X;
    };
    struct T { const char* name; vector<s3::V3> a, b; };
    vector<T> tests = {
        {"Hopf link", circle({0,0,0}, 0, 1, 300, 0), circle({1,0,0}, 1, 1, 300, 0.1)},
        {"Hopf link, reversed", circle({0,0,0}, 0, 1, 300, 0), [&]{ auto x = circle({1,0,0}, 1, 1, 300, 0.1); std::reverse(x.begin(), x.end()); return x; }()},
        {"split circles", circle({0,0,0}, 0, 1, 300, 0), circle({3,0,0}, 1, 1, 300, 0.1)},
        {"(2,4) torus link", torus(0, 600), torus(s3::PI, 600)},
    };
    for (auto& t : tests) {
        double g = gauss_lk(t.a, t.b);
        printf("  %-22s Gauss %+.4f   crossings:", t.name, g);
        for (int k = 0; k < 4; k++) {
            double R[9];
            random_rotation(rng, R);
            long ab, ba, nc;
            crossings(rotate(t.a, R), rotate(t.b, R), ab, ba, nc);
            printf("  %+ld/%+ld (%ld)", ab, ba, nc);
        }
        printf("\n");
    }
    return 0;
}

static void usage()
{
    fprintf(stderr,
"usage: link3 selftest\n"
"       link3 [options]\n"
"  -f FILE      substitution file (default subst_4.00_9.50_t3.txt)\n"
"  -a ACC       accuracy of the loops (default 0.005)\n"
"  --rr R       find relations inside the hyperbolic ball B(R) (default 7.5)\n"
"  --maxlen N   longest relation (default 6)\n"
"  --nrel N     use the first N relations (default 30)\n"
"  --hr R       translate the Z- loops by the elements of B(R) (default 3.4)\n"
"  --stats      check the translates (closure under S, descent; exact): one line on stderr, and a second near a shell\n"
"  --seed N     random seed for the projections (default 1)\n"
"  --pair A B H the single pair (gamma_wA, h_H delta_wB), with --nproj projections (default 20)\n");
}

int main(int argc, char** argv)
{
    cs::setup();
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    Options opt;
    opt.acc = 0.005;
    double rr = 7.5, hr = 3.4;
    size_t maxlen = 6, nrel = 30;
    uint64_t seed = 1;
    int pairA = -1, pairB = -1, pairH = -1, nproj = 20;
    string pngout, exportfrom;
    bool hinfo = false, stats = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { usage(); exit(1); } return argv[++i]; };
        if (a == "-f") opt.subst = next();
        else if (a == "-a") opt.acc = atof(next().c_str());
        else if (a == "--rr") rr = atof(next().c_str());
        else if (a == "--maxlen") maxlen = atoi(next().c_str());
        else if (a == "--nrel") nrel = atoi(next().c_str());
        else if (a == "--hr") hr = atof(next().c_str());
        else if (a == "--seed") seed = strtoull(next().c_str(), nullptr, 10);
        else if (a == "--pair") { pairA = atoi(next().c_str()); pairB = atoi(next().c_str()); pairH = atoi(next().c_str()); }
        else if (a == "--nproj") nproj = atoi(next().c_str());
        else if (a == "--png") pngout = next();
        else if (a == "--hinfo") hinfo = true;
        else if (a == "--export") exportfrom = next();
        else if (a == "--stats") stats = true;
        else { usage(); return 1; }
    }
    World<PU21> W;
    prepare<cs::RenderPolicy>(opt.subst.c_str(), W);
    if (stats) {                       // the translates checked, against those of roots_info, bit for bit
        vector<M3> g;
        vector<double> d;
        for (const cs::RootInfo& x : cs::roots_info(hr)) { g.push_back(x.g); d.push_back(x.d); }
        for (const string& line : cs::stats_lines(hr, g, d, "the translates of roots_info", "roots_info"))
            fprintf(stderr, "%s\n", line.c_str());
    }
    std::mt19937_64 rng(seed);

    double t0 = now();
    vector<vector<int>> rels = find_relations<cs::FVal>(W.L, rr, maxlen, nrel);
    if (hinfo) {                       // the translates: phi(h) = f0, and their a-words
        vector<cs::RootInfo> RI = cs::roots_info(hr);
        for (size_t i = 0; i < RI.size(); i++)
            printf("h%zu  d=%.4f  f=(%d,%d)  phi=%d  %s\n", i, RI[i].d, RI[i].f0, RI[i].f1, RI[i].f0, RI[i].word.c_str());
        return 0;
    }
    if (!exportfrom.empty()) {         // the verified pairs of a run, as explicit words
        vector<cs::RootInfo> RI = cs::roots_info(hr);
        std::ifstream in(exportfrom);
        string line;
        FILE* out = fopen("linked_pairs.txt", "w");
        fprintf(out, "# linked pairs (gamma_w in Z+, h delta_w' in Z-), verified by link3; letters are indices into S\n");
        int n = 0;
        while (std::getline(in, line)) {
            if (line.find("STABLE") == string::npos || line.find("ALL AGREE") == string::npos) continue;
            size_t a, b, hi;
            if (sscanf(line.c_str(), "w%zu w%zu h%zu:", &a, &b, &hi) != 3) continue;
            size_t p = line.find("[");
            long lk = 0;
            sscanf(line.c_str() + line.find(":", p) + 1, "%ld", &lk);
            fprintf(out, "lk %+ld | w", lk);
            for (int x : rels[a]) fprintf(out, " %d", x);
            fprintf(out, " | w'");
            for (int x : rels[b]) fprintf(out, " %d", x);
            fprintf(out, " | phi(h) %d | h %s\n", RI[hi].f0, RI[hi].word.c_str());
            n++;
        }
        fclose(out);
        fprintf(stderr, "wrote %d pairs to linked_pairs.txt\n", n);
        return 0;
    }
    if (pairA >= 0) {                  // one pair, many projections
        vector<M3> H;
        cs::find_roots(hr, H);
        // the indices are checked (out of range they were read past the end, as in the legacy
        // program, and the run crashed or not at random)
        if ((size_t)pairA >= rels.size() || pairB < 0 || (size_t)pairB >= rels.size() || pairH < 0 ||
            (size_t)pairH >= H.size()) {
            fprintf(stderr, "--pair %d %d %d: there are %zu relations (--nrel) and %zu elements of B(%g) (--hr)\n",
                    pairA, pairB, pairH, rels.size(), H.size(), hr);
            return 1;
        }
        size_t forced = 0;
        vector<SpherePoint<4>> A = loop_points(W.plus, W.L, PU21::identity(), rels[pairA], opt.acc, forced);
        vector<SpherePoint<4>> B = loop_points(W.minus, W.L, H[pairH], rels[pairB], opt.acc, forced);
        printf("pair w%d w%d h%d at acc %.4g: %zu + %zu points, separation %.4f\n", pairA, pairB, pairH, opt.acc,
               A.size(), B.size(), separation(A, B));
        double pole0[4] = {0,0,0,1};
        for (int k = 0; k < nproj; k++) {
            LinkResult r = linking(A, B, rng, 0.05);
            if (k == 0) memcpy(pole0, r.pole, sizeof pole0);
            printf("  projection %2d: lk %+ld  (A over B and B over A %s)  crossings %ld  pole distance %.4f\n",
                   k, r.lk, r.agree ? "agree" : "DISAGREE", r.ncross, r.poledist);
        }
        if (!pngout.empty())
            for (double yaw : {0.0, 60.0, 120.0}) {
                char fn[200];
                snprintf(fn, sizeof fn, "%s_yaw%03d.png", pngout.c_str(), (int)yaw);
                draw_pair(fn, A, B, pole0, W.plus, W.L, yaw, 15);
                fprintf(stderr, "wrote %s\n", fn);
            }
        return 0;
    }
    fprintf(stderr, "%zu relations of length <= %zu (%.2fs):\n", rels.size(), maxlen, now() - t0);
    for (size_t r = 0; r < rels.size(); r++) {
        fprintf(stderr, "  w%zu:", r);
        for (int x : rels[r]) fprintf(stderr, " %d", x);
        fprintf(stderr, "\n");
    }

    // the Z+ loops, based at the identity
    t0 = now();
    vector<vector<SpherePoint<4>>> gam;
    size_t forced = 0;
    for (auto& w : rels) gam.push_back(loop_points(W.plus, W.L, PU21::identity(), w, opt.acc, forced));
    size_t np = 0;
    for (auto& g : gam) np += g.size();
    fprintf(stderr, "Z+ loops: %zu points in all (%.2fs), %zu forced leaves\n", np, now() - t0, forced);

    // the Z- loops h.delta_w, h in B(hr), against every Z+ loop
    vector<M3> H;
    cs::find_roots(hr, H);
    fprintf(stderr, "translating the Z- loops by the %zu elements of B(%.2f)\n", H.size(), hr);
    long tested = 0, nonzero = 0, disagree = 0, close = 0, projdis = 0;
    struct Hit { size_t a, b, hi; };
    vector<Hit> hits;
    double minsepall = 1;
    t0 = now();
    printf("# nonzero linking numbers lk(gamma_w, h delta_w'): w  w'  h-index  d(o,ho)  lk  crossings  separation\n");
    for (size_t hi = 0; hi < H.size(); hi++) {
        vector<vector<SpherePoint<4>>> del;
        for (auto& w : rels) del.push_back(loop_points(W.minus, W.L, H[hi], w, opt.acc, forced));
        for (size_t a = 0; a < gam.size(); a++)
            for (size_t b = 0; b < del.size(); b++) {
                LinkResult r = linking(gam[a], del[b], rng, 0.05);
                LinkResult r2 = linking(gam[a], del[b], rng, 0.05);
                if (r2.lk != r.lk) { projdis++; r.agree = false; }
                tested++;
                minsepall = std::min(minsepall, r.sep);
                if (!r.agree) disagree++;
                if (r.sep < 5*opt.acc) close++;
                if (r.lk != 0 || r2.lk != 0) {
                    nonzero++;
                    hits.push_back({a, b, hi});
                    printf("w%zu w%zu h%zu %.4f  lk %+ld  %ld  %s%.4f%s\n", a, b, hi, PU21::displacement(H[hi]), r.lk, r.ncross,
                           r.sep >= 0.05 ? ">= " : "", r.sep, r.agree ? "" : "  [over/under counts DISAGREE]");
                    fflush(stdout);
                }
            }
    }
    fprintf(stderr, "tested %ld pairs in %.1fs: %ld nonzero, %ld over/under disagreements, %ld pairs where two projections disagree, %ld closer than 5 acc; min separation %.4f\n",
            tested, now() - t0, nonzero, disagree, projdis, close, minsepall);

    // verification of the nonzero pairs: refine twice, three projections each, true separation
    printf("# verification: w w' h | for acc, acc/2, acc/4: lk over three projections, separation, points\n");
    int pic = 0;
    for (const Hit& x : hits) {
        printf("w%zu w%zu h%zu:", x.a, x.b, x.hi);
        bool stable = true;
        long lk0 = 0;
        double minpole = 9;
        double pole[4] = {0,0,0,1};
        vector<SpherePoint<4>> Afin, Bfin;
        for (int lev = 0; lev < 3; lev++) {
            double acc = opt.acc/(1 << lev);
            vector<SpherePoint<4>> A = loop_points(W.plus, W.L, PU21::identity(), rels[x.a], acc, forced);
            vector<SpherePoint<4>> B = loop_points(W.minus, W.L, H[x.hi], rels[x.b], acc, forced);
            printf("  [%.4g:", acc);
            double sep = 0;
            for (int s = 0; s < 3; s++) {
                LinkResult r = linking(A, B, rng, s == 0 ? -1 : 0.05);
                if (s == 0) { sep = r.sep; memcpy(pole, r.pole, sizeof pole); }
                printf(" %+ld%s%s", r.lk, r.agree ? "" : "!", r.poledist < 0.15 ? "?" : "");
                minpole = std::min(minpole, r.poledist);
                if (lev == 0 && s == 0) lk0 = r.lk;
                if (r.lk != lk0 || !r.agree) stable = false;
            }
            printf(" sep %.4f, %zu+%zu pts]", sep, A.size(), B.size());
            if (lev == 2) { Afin.swap(A); Bfin.swap(B); }
        }
        printf("  pole >= %.3f  %s", minpole, stable ? "STABLE" : "not stable");
        // invariance: lk(g gamma, g h delta) = lk(gamma, h delta) for g in Pi (here: four g's
        // from B(2.6), at the middle accuracy); the arcs are then drawn in other positions
        printf("  | translated by g:");
        for (size_t gi : {(size_t)2, (size_t)7, (size_t)11, (size_t)17}) {
            if (gi >= H.size()) continue;
            double acc = opt.acc/2;
            vector<SpherePoint<4>> A = loop_points(W.plus, W.L, H[gi], rels[x.a], acc, forced);
            vector<SpherePoint<4>> B = loop_points(W.minus, W.L, PU21::mul(H[gi], H[x.hi]), rels[x.b], acc, forced);
            LinkResult r = linking(A, B, rng, 0.05);
            printf(" %+ld%s", r.lk, r.agree ? "" : "!");
            if (r.lk != lk0 || !r.agree) stable = false;
        }
        printf("  %s\n", stable ? "ALL AGREE" : "MISMATCH");
        fflush(stdout);
        if (stable && pic < 6) {
            char fn[80];
            snprintf(fn, sizeof fn, "pictures/link_w%zu_w%zu_h%zu.png", x.a, x.b, x.hi);
            draw_pair(fn, Afin, Bfin, pole, W.plus, W.L);
            fprintf(stderr, "wrote %s\n", fn);
            pic++;
        }
    }
    return 0;
}
