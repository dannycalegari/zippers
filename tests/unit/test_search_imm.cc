// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_search_imm: the generic block-filling search of search/kernel_search.h, with the policy of IMM's M5
// (examples/imm_m5_search.h), agrees BIT FOR BIT with kernelmode of legacy/s4/chz5.cc. The legacy code is a
// program, not a header, so the parts compared are copied below (section "the legacy code"), with their legacy names, on
// the legacy types of legacy/s4/rh5.h (which this program includes, in the global namespace); where the legacy code
// had a constant that a test varies (the cap, the budget and the radius of the descent), the copy takes it as a
// parameter. Compared:
//   - S_c (its indices and their order, the elements, words and distances), check_S, and t (pick_t, phi = -1..4);
//   - the K-search (kernel_fill), element by element, at two radii and with a small cap;
//   - the inverses in S_c (kernel_inverses);
//   - THE DESCENT (kernel_express) against the legacy express, on elements inside the K-store and outside it (the roofs
//     of t, t^2 and t^3, long words in S_c, elements not in K, whose descents fail), with the budget and radius of
//     chz5 and with smaller ones: whether a word is found, the word, and the counts of descents and nodes after every
//     call; and that every word found is exactly the element;
//   - what the policy prints (print_t, print_no_t, print_kstore, print_descents, stats on random data, check_S's
//     message), against the legacy printf lines;
//   - chain_point and dmul of the convergence report, bit for bit, on random matrices.
// Exact values are compared with memcmp, doubles by their bits. Built with -O2, the flags of the programs.

#include "rh5.h"                          // legacy/s4

#include "examples/imm_m5.h"
#include "examples/imm_m5_search.h"
#include "geometry/on1/on1.h"
#include "search/kernel_search.h"
#include "store/store.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

// The policy's constants are those of legacy/s4/chz5.cc (const size_t CAP = 2500000; the descent with BUDGET = 200000
// and dy > 18; every roof checked exactly; the convergence report stops at d > 30). A
// changed budget would pass every other fast test, so they are pinned here.
static_assert(zippers::imm::SearchPolicy::kstore_cap == 2500000, "chz5: the K-store cap of legacy/s4/chz5.cc");
static_assert(zippers::imm::SearchPolicy::descent, "chz5: the descent, as in legacy/s4/chz5.cc");
static_assert(zippers::imm::SearchPolicy::descent_budget == 200000, "chz5: the descent's budget (BUDGET)");
static_assert(zippers::imm::SearchPolicy::descent_radius == 18, "chz5: the descent's radius (dy > 18)");
static_assert(zippers::imm::SearchPolicy::check_roofs, "chz5: every roof checked exactly, as in legacy/s4/chz5.cc");
static_assert(zippers::imm::SearchPolicy::chain_stop == 30, "chz5: the convergence report stops at d > 30");

namespace z = zippers;
typedef z::ON1<5> O;
typedef z::imm::SearchPolicy Policy;

// ---- bookkeeping (as in test_on1.cc) -------------------------------------------------------------------------------

static const char* sec_name = nullptr;
static long sec_n = 0, sec_bad = 0, all_n = 0, all_bad = 0;
static std::string sec_notes;

static void end_section()
{
    if (sec_name) printf("  %-62s %9ld compared, %ld differ\n%s", sec_name, sec_n, sec_bad, sec_notes.c_str());
    fflush(stdout);
}
static void section(const char* name) { end_section(); sec_name = name; sec_n = sec_bad = 0; sec_notes.clear(); }

static void note(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void note(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sec_notes += std::string("      ") + buf + "\n";
}

static void check(bool ok, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void check(bool ok, const char* fmt, ...)
{
    sec_n++; all_n++;
    if (ok) return;
    sec_bad++; all_bad++;
    if (all_bad > 40) return;
    va_list ap;
    va_start(ap, fmt);
    printf("    DIFFERS: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

template <class X, class Y>
static bool same_bytes(const X& a, const Y& b)
{
    static_assert(sizeof(X) == sizeof(Y), "the two types must have the same size");
    return !memcmp(&a, &b, sizeof(X));
}
static bool same_bits(double a, double b) { return !memcmp(&a, &b, sizeof a); }

// the legacy and the new elements have the same layout
static O::Elem nm(const M6& a) { O::Elem b; memcpy(&b, &a, sizeof b); return b; }

// what f prints on stdout
static std::string capture(const std::function<void()>& f)
{
    fflush(stdout);
    FILE* tmp = tmpfile();
    if (!tmp) { perror("tmpfile"); exit(2); }
    int saved = dup(1);
    dup2(fileno(tmp), 1);
    f();
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    std::string s;
    rewind(tmp);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, tmp)) > 0) s.append(buf, n);
    fclose(tmp);
    return s;
}

static std::mt19937_64 rng(20261025);
static int64_t rnd(int64_t lo, int64_t hi) { return std::uniform_int_distribution<int64_t>(lo, hi)(rng); }
static double gauss() { return std::normal_distribution<double>(0, 1)(rng); }

// ---- the legacy code (legacy/s4/chz5.cc, kernelmode and the functions it calls), copied with its names --------------

struct Gen6 { M6 m; vector<int> word; double d; };

// the lambda pick_t of kernelmode
static int legacy_pick_t(const Store6& B, int ph)
{
    int best = -1;
    for (size_t i = 0; i < B.size(); i++) {
        if (B.mu[i] != 0 || B.h[i] != 2*ph) continue;
        M6 g = B.load(i), p = m6id();
        for (int k = 0; k < 8; k++) p = m6mul(p, g);          // loxodromic: d(o, g^8 o) grows linearly
        if (m6dist(p) < 8*0.5*B.dist[i]) continue;
        if (best < 0 || B.dist[i] < B.dist[best]) best = (int)i;
    }
    return best;
}

// S_c, and its check (kernelmode)
static vector<Gen6> legacy_S(const Store6& B, double c, vector<int>& Sidx)
{
    Sidx.clear();
    for (size_t i = 1; i < B.size(); i++) if (B.mu[i] == 0 && B.h[i] == 0 && B.dist[i] <= c) Sidx.push_back(i);
    std::stable_sort(Sidx.begin(), Sidx.end(), [&](int a, int b) { return B.dist[a] < B.dist[b]; });
    vector<Gen6> S;
    for (int i : Sidx) S.push_back({B.load(i), B.path(i), B.dist[i]});
    return S;
}
static bool legacy_closed(const vector<Gen6>& S)
{
    for (auto& s : S) {
        M6 x = m6inv(s.m);
        bool f = false;
        for (auto& u : S) if (m6eq(u.m, x)) { f = true; break; }
        if (!f) { printf("S_c is not closed under inverses\n"); return false; }
    }
    return true;
}

// the K-search (kernelmode; CAP was the constant 2500000)
static bool legacy_kstore(Store6& Kst, const vector<Gen6>& S, double R, size_t CAP)
{
    Kst.add(m6id(), -1, -1, 0, 0, 0, 0.f);
    bool capped = false;
    for (size_t q = 0; q < Kst.size() && !capped; q++) {
        M6 g = Kst.load(q);
        for (size_t j = 0; j < S.size(); j++) {
            M6 x = m6mul(g, S[j].m);
            if (Kst.lookup(x) >= 0) continue;
            double d = m6dist(x);
            if (d > R) continue;
            Kst.add(x, q, j, Kst.depth[q] + 1, 0, 0, (float)d);
            if (Kst.size() >= CAP) { capped = true; break; }
        }
    }
    return capped;
}

static vector<int> legacy_INV(const vector<Gen6>& S)
{
    vector<int> INV(S.size(), -1);
    for (size_t j = 0; j < S.size(); j++) {
        M6 x = m6inv(S[j].m);
        for (size_t k = 0; k < S.size(); k++) if (m6eq(S[k].m, x)) INV[j] = (int)k;
    }
    return INV;
}

// the lambda express of kernelmode, its captures as members (BUDGET was the constant 200000, and the radius 18)
struct LegacyExpress {
    const Store6& Kst;
    const vector<Gen6>& S;
    const vector<int>& INV;
    long descents = 0, descent_nodes = 0;
    LegacyExpress(const Store6& K, const vector<Gen6>& S_, const vector<int>& I) : Kst(K), S(S_), INV(I) {}
    bool operator()(const M6& x, vector<int>& word, size_t BUDGET, double RADIUS)
    {
        int k = Kst.lookup(x);
        if (k >= 0) { word = Kst.path(k); return true; }
        descents++;
        Store6 Bk;
        Bk.add(x, -1, -1, 0, 0, 0, (float)m6dist(x));
        typedef std::pair<float, int> QE;
        std::priority_queue<QE, vector<QE>, std::greater<QE>> pq;
        pq.push({Bk.dist[0], 0});
        while (!pq.empty() && Bk.size() < BUDGET) {
            int i = pq.top().second;
            pq.pop();
            M6 g = Bk.load(i);
            for (size_t j = 0; j < S.size(); j++) {
                M6 y = m6mul(g, S[j].m);
                if (Bk.lookup(y) >= 0) continue;
                double dy = m6dist(y);
                if (dy > RADIUS) continue;
                int idx = Bk.add(y, i, (int)j, Bk.depth[i] + 1, 0, 0, (float)dy);
                int kk = Kst.lookup(y);
                if (kk >= 0) {
                    vector<int> back = Bk.path(idx);
                    word = Kst.path(kk);
                    for (int a = (int)back.size() - 1; a >= 0; a--) word.push_back(INV[back[a]]);
                    descent_nodes += Bk.size();
                    return true;
                }
                pq.push({(float)dy, idx});
            }
        }
        descent_nodes += Bk.size();
        return false;
    }
};

static void legacy_stats(const vector<vector<int>>& w, const vector<char>& ok, const char* nm)
{
    size_t mn = 1 << 30, mx = 0, n = 0;
    double sum = 0;
    for (size_t j = 0; j < w.size(); j++) if (ok[j]) { mn = std::min(mn, w[j].size()); mx = std::max(mx, w[j].size()); sum += w[j].size(); n++; }
    if (!n) return;
    printf("%s roof lengths (in S_c-steps): min %zu, mean %.2f, max %zu", nm, mn, sum/n, mx);
    // the growth rate of the substitution: Perron-Frobenius root by power iteration
    size_t S = w.size();
    vector<double> v(S, 1.0), v2(S);
    double lam = 0;
    for (int it = 0; it < 200; it++) {
        for (size_t j = 0; j < S; j++) { v2[j] = 0; for (int l : w[j]) v2[j] += v[l]; }
        double s = 0;
        for (double x : v2) s += x;
        lam = s/S*S/std::max(1e-300, [&]{ double t = 0; for (double x : v) t += x; return t; }());
        double nn = 0;
        for (double x : v2) nn = std::max(nn, x);
        for (size_t j = 0; j < S; j++) v[j] = v2[j]/nn;
    }
    printf(", growth rate %.3f\n", lam);
}

typedef std::array<std::array<double,6>,6> LD6;
static LD6 legacy_dmul(const LD6& a, const LD6& b)
{
    LD6 c;
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) { double s = 0; for (int k = 0; k < 6; k++) s += a[i][k]*b[k][j]; c[i][j] = s; }
    return c;
}
// the lambda pt of convergence
static void legacy_pt(const LD6& m, double s[5], double& d)
{
    double nn = 0;
    for (int i = 0; i < 5; i++) { s[i] = m[i][5]; nn += s[i]*s[i]; }
    nn = std::sqrt(nn);
    for (int i = 0; i < 5; i++) s[i] /= nn;
    d = m[5][5] <= 1 ? 0 : std::acosh(m[5][5]);
}

static LD6 todbl(const M6& g) { LD6 m; for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) m[i][j] = 0.5*(double)g.e[i][j]; return m; }

// the loop of convergence, with its printf replaced by recording the row it printed
struct LRow { int n; size_t points; double mc, dmin, dmax; };
static vector<LRow> legacy_convergence_rows(const vector<Gen6>& S, const vector<vector<int>>& th, const M6& t, int y, size_t Lmax)
{
    vector<LRow> rows;
    vector<LD6> Sd;
    for (auto& s : S) Sd.push_back(todbl(s.m));
    LD6 td = todbl(t), T = todbl(m6id());
    vector<int> w = {y};
    for (int n = 0; n < 40; n++) {
        LD6 P = T;
        auto pt = [&](const LD6& m, double s[5], double& d) {
            double nn = 0;
            for (int i = 0; i < 5; i++) { s[i] = m[i][5]; nn += s[i]*s[i]; }
            nn = std::sqrt(nn);
            for (int i = 0; i < 5; i++) s[i] /= nn;
            d = m[5][5] <= 1 ? 0 : std::acosh(m[5][5]);
        };
        double prev[5], cur[5], d, dmin, dmax, mc = 0;
        pt(P, prev, d);
        dmin = dmax = d;
        for (size_t m = 0; m < w.size(); m++) {
            P = legacy_dmul(P, Sd[w[m]]);
            pt(P, cur, d);
            dmin = std::min(dmin, d); dmax = std::max(dmax, d);
            double e = 0;
            for (int i = 0; i < 5; i++) e += (cur[i]-prev[i])*(cur[i]-prev[i]);
            mc = std::max(mc, std::sqrt(e));
            memcpy(prev, cur, sizeof prev);
        }
        rows.push_back({n, w.size() + 1, mc, dmin, dmax});    // printf("  %2d %10zu   %14.3e   %7.2f .. %.2f\n", ...)
        size_t next = 0;
        for (int l : w) next += th[l].size();
        if (next > Lmax || dmax > 30) break;
        vector<int> w2;
        w2.reserve(next);
        for (int l : w) for (int x : th[l]) w2.push_back(x);
        w.swap(w2);
        T = legacy_dmul(T, td);
    }
    return rows;
}

// ---- helpers ---------------------------------------------------------------------------------------------------------

// the K-search of a policy with a smaller cap
struct CappedPolicy : Policy {
    static constexpr size_t kstore_cap = 4000;
    using Policy::Policy;
};

// two stores, element by element: the element, its parent, letter, depth, label and distance
static void compare_stores(const Store6& L, const z::imm::Store6& N, const char* what)
{
    check(L.size() == N.size(), "%s: %zu elements, %zu new", what, L.size(), N.size());
    size_t n = std::min(L.size(), N.size());
    long bad = 0;
    for (size_t i = 0; i < n; i++) {
        bool ok = same_bytes(L.load(i), N.load(i)) && L.parent[i] == N.parent[i] && L.letter[i] == N.letter[i] &&
                  L.depth[i] == N.depth[i] && !memcmp(&L.dist[i], &N.dist[i], sizeof(float)) &&
                  L.mu[i] == N.label[i].mu && L.h[i] == N.label[i].h;
        if (!ok) bad++;
        sec_n++; all_n++;
        if (!ok) { sec_bad++; all_bad++; }
    }
    if (bad) printf("    DIFFERS: %s: %ld of %zu elements\n", what, bad, n);
}

static M6 product(const vector<Gen6>& S, const vector<int>& w)
{
    M6 p = m6id();
    for (int l : w) p = m6mul(p, S[l].m);
    return p;
}

int main()
{
    double t_start = z::now();
    printf("test_search_imm: the kernel search of src/search/ with IMM's policy against legacy/s4/chz5.cc, bit for bit\n");
    rh5_setup();
    z::imm::setup();

    // the ball B(3) of the reference run, and S_c for c = 2.3 (80 elements)
    const double rho = 3.0, c = 2.3;
    Store6 LB;
    enumerate6(LB, rho);
    Policy p(rho);
    z::imm::enumerate6(p.ball, rho);

    section("the ball B(3) (enumerate6): the input");
    compare_stores(LB, p.ball, "B(3)");

    section("S_c (c = 1.6, 2, 2.3): indices, order, elements, words, d");
    vector<Gen6> LS;
    vector<Policy::Gen> S;
    for (double cc : {1.6, 2.0, c}) {
        vector<int> Lidx;
        vector<Gen6> L = legacy_S(LB, cc, Lidx);
        vector<int> Nidx = p.kernel_indices(cc);
        check(Lidx == Nidx, "c = %.1f: the indices of S_c", cc);
        vector<Policy::Gen> N;
        for (int i : Nidx) N.push_back(p.generator(i));
        check(L.size() == N.size(), "c = %.1f: |S_c| %zu, new %zu", cc, L.size(), N.size());
        for (size_t j = 0; j < std::min(L.size(), N.size()); j++) {
            check(same_bytes(L[j].m, N[j].m), "c = %.1f: s_%zu", cc, j);
            check(L[j].word == N[j].word, "c = %.1f: the word of s_%zu", cc, j);
            check(same_bits(L[j].d, N[j].d), "c = %.1f: d of s_%zu", cc, j);
        }
        note("c = %.1f: |S_c| = %zu", cc, N.size());
        if (cc == c) { LS = L; S = N; }
    }

    section("check_S (closed under inverses), with its message");
    {
        bool lok = true, nok = true;
        std::string lo = capture([&] { lok = legacy_closed(LS); });
        std::string no = capture([&] { nok = p.check_S(S); });
        check(lok && nok && lo == no && lo.empty(), "S_c itself: legacy %d, new %d", lok, nok);
        // without the inverse of the first s_j that is not an involution (Gamma has torsion: s_0 is one), the check
        // must fail
        vector<int> inv = legacy_INV(LS);
        size_t j0 = 0;
        while (j0 < inv.size() && inv[j0] == (int)j0) j0++;
        check(j0 < inv.size(), "every element of S_c is an involution?");
        vector<Gen6> L2;
        vector<Policy::Gen> N2;
        for (size_t j = 0; j < LS.size(); j++) if ((int)j != inv[j0]) { L2.push_back(LS[j]); N2.push_back(S[j]); }
        lo = capture([&] { lok = legacy_closed(L2); });
        no = capture([&] { nok = p.check_S(N2); });
        check(!lok && !nok && lo == no && lo == "S_c is not closed under inverses\n",
              "S_c without the inverse of s_%zu: legacy %d '%s', new %d '%s'", j0, lok, lo.c_str(), nok, no.c_str());
        long invol = 0;
        for (size_t j = 0; j < inv.size(); j++) invol += inv[j] == (int)j;
        note("%ld of the %zu elements of S_c are involutions; s_%zu is not, and its inverse is s_%d", invol, inv.size(),
             j0, inv[j0]);
    }

    section("t: pick_t for phi = -1 .. 4, and print_t, print_no_t");
    for (int ph = -1; ph <= 4; ph++) {
        int lt = legacy_pick_t(LB, ph), nt = p.pick_t(ph);
        check(lt == nt, "pick_t(%d): legacy %d, new %d", ph, lt, nt);
        if (nt >= 0) {
            std::string lo = capture([&] {
                printf("t = [%s]   d(o, t o) = %.4f   h(t) = %d\n", fword(LB.path(lt)).c_str(), LB.dist[lt], LB.h[lt]);
                printf("steep t = [%s]   d(o, t o) = %.4f   h(t) = %d\n", fword(LB.path(lt)).c_str(), LB.dist[lt], LB.h[lt]);
            });
            std::string no = capture([&] { p.print_t(nt, false); p.print_t(nt, true); });
            check(lo == no, "print_t(%d)", nt);
        } else {
            std::string lo = capture([&] { printf("no loxodromic element with phi = %d in the ball\n", ph); });
            std::string no = capture([&] { p.print_no_t(ph); });
            check(lo == no, "print_no_t(%d)", ph);
        }
        note("phi = %d: t = element %d", ph, nt);
    }
    {
        std::string lo = capture([&] { printf("no loxodromic element with phi = 1 in the ball\n"); });
        std::string no = capture([&] { p.print_no_t(1); });
        check(lo == no, "print_no_t(1), against the literal line of the legacy program");
    }

    section("the K-search (kernel_fill) at R = 3.6 and 3.8, and capped at 4000");
    Store6 LK;
    z::imm::Store6 K;
    for (double R : {3.6, 3.8}) {
        Store6 L;
        z::imm::Store6 N;
        bool lc = legacy_kstore(L, LS, R, 2500000);
        bool nc = z::kernel_fill(p, N, S, R);
        check(lc == nc, "R = %.1f: capped %d, new %d", R, lc, nc);
        compare_stores(L, N, R == 3.6 ? "K-store, R = 3.6" : "K-store, R = 3.8");
        note("R = %.1f: %zu elements", R, N.size());
        if (R == 3.8) { LK = L; K = N; }
    }
    {
        Store6 L;
        z::imm::Store6 N;
        CappedPolicy cp(rho);
        bool lc = legacy_kstore(L, LS, 3.8, CappedPolicy::kstore_cap);
        bool nc = z::kernel_fill(cp, N, S, 3.8);
        check(lc && nc, "capped: legacy %d, new %d", lc, nc);
        compare_stores(L, N, "K-store, R = 3.8, cap 4000");
        note("capped at %zu elements", N.size());
    }
    {
        std::string lo = capture([&] {
            printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d%s\n",
                   K.size(), 3.8, 0.25, 10, "");
            printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d%s\n",
                   K.size(), 3.8, 0.25, 10, "  [CAPPED]");
            printf("  (%ld descents from outside the K-store, %ld nodes)\n", 305L, 613924L);
        });
        std::string no = capture([&] {
            p.print_kstore(K.size(), 3.8, 0.25, 10, false);
            p.print_kstore(K.size(), 3.8, 0.25, 10, true);
            p.print_descents(305, 613924);
        });
        check(lo == no, "print_kstore, print_descents");
    }

    section("the inverses in S_c (kernel_inverses)");
    vector<int> LINV = legacy_INV(LS);
    std::vector<int> INV = z::kernel_inverses<O>(S);
    check(LINV == INV, "INV");
    for (size_t j = 0; j < INV.size(); j++) check(INV[j] >= 0, "s_%zu has no inverse", j);

    // ---- the descent ----------------------------------------------------------------------------------------------
    LegacyExpress lexp(LK, LS, LINV);
    z::KernelDescentCount count;
    long found = 0, notfound = 0, descended = 0, wrong = 0;
    auto both = [&](const M6& x, size_t budget, double radius, const char* what, vector<int>* keep = nullptr) {
        vector<int> lw = {-7}, nw = {-7};           // a word that is not found must be left as it was
        long d0 = lexp.descents;
        bool lf = lexp(x, lw, budget, radius);
        bool nf = z::kernel_express(K, S, INV, nm(x), nw, budget, radius, count);
        check(lf == nf, "%s: found %d, new %d", what, lf, nf);
        check(lw == nw, "%s: the words", what);
        check(lexp.descents == count.descents && lexp.descent_nodes == count.nodes,
              "%s: descents %ld, %ld nodes; new %ld, %ld", what, lexp.descents, lexp.descent_nodes, count.descents, count.nodes);
        if (nf) {
            found++;
            if (!m6eq(product(LS, nw), x)) wrong++;
            check(m6eq(product(LS, nw), x), "%s: the word found is not the element", what);
        } else {
            notfound++;
            check(nw.size() == 1 && nw[0] == -7, "%s: a word not found was changed", what);
        }
        if (lexp.descents > d0) descended++;
        if (keep) *keep = nf ? nw : vector<int>();
    };

    section("the descent (kernel_express): elements of the K-store");
    for (int trial = 0; trial < 3000; trial++) {
        size_t i = (size_t)rnd(0, (int64_t)LK.size() - 1);
        both(LK.load(i), 200000, 18, "K-store element");
    }
    both(m6id(), 200000, 18, "the identity");
    note("3001 elements: %ld found, %ld descents", found, lexp.descents);

    section("the descent: the roofs of t, t^2, t^3 (chz5's budget and radius)");
    found = notfound = descended = 0;
    vector<vector<int>> up1(LS.size()), down1(LS.size());         // the roofs of t, for the convergence report
    for (int ph = 1; ph <= 3; ph++) {
        int ti = legacy_pick_t(LB, ph);
        M6 t = LB.load(ti), ti_ = m6inv(t);
        for (size_t j = 0; j < LS.size(); j++) {
            both(m6mul(m6mul(ti_, LS[j].m), t), 200000, 18, "an up roof", ph == 1 ? &up1[j] : nullptr);
            both(m6mul(m6mul(t, LS[j].m), ti_), 200000, 18, "a down roof", ph == 1 ? &down1[j] : nullptr);
        }
    }
    note("480 roofs: %ld found (%ld after a descent), %ld not; %ld descents, %ld nodes in all", found, descended,
         notfound, count.descents, count.nodes);

    // (an element far out would not fit the store: entries of 2g beyond the int32 range make Store6::add, and the
    // generic store, abort with "Store6: entry too large"; so the words are kept within the descent's radius)
    section("the descent: words of 3 to 7 letters, d <= 17 (budget 20000)");
    found = notfound = descended = 0;
    {
        int nw = 0;
        double dmax = 0;
        while (nw < 150) {
            int len = (int)rnd(3, 7);
            M6 x = m6id();
            for (int k = 0; k < len; k++) x = m6mul(x, LS[(size_t)rnd(0, (int64_t)LS.size() - 1)].m);
            if (m6dist(x) > 17) continue;
            dmax = std::max(dmax, m6dist(x));
            both(x, 20000, 18, "a word in S_c");
            nw++;
        }
        note("150 words, d(o, x o) up to %.2f: %ld found (%ld after a descent), %ld not", dmax, found, descended, notfound);
    }

    section("the descent: outside K (it fails); small budgets and radii");
    found = notfound = descended = 0;
    {
        int ti = legacy_pick_t(LB, 1);
        M6 t = LB.load(ti);
        for (size_t budget : {(size_t)1, (size_t)2, (size_t)500, (size_t)3000}) {
            both(t, budget, 18, "t");
            both(m6mul(t, LS[3].m), budget, 18, "t s_3");
            both(m6mul(LS[5].m, m6inv(t)), budget, 18, "s_5 t^-1");
        }
        both(t, 200000, 4.0, "t, radius 4");                 // the queue runs dry before the budget
        both(m6mul(t, t), 200000, 3.6, "t^2, radius 3.6");
    }
    // roofs with a small radius or budget: some are found, some not
    {
        int ti = legacy_pick_t(LB, 2);
        M6 t = LB.load(ti), ti_ = m6inv(t);
        for (size_t j = 0; j < LS.size(); j += 3) {
            both(m6mul(m6mul(ti_, LS[j].m), t), 300, 18, "a roof of t^2, budget 300");
            both(m6mul(m6mul(t, LS[j].m), ti_), 200000, 4.2, "a roof of t^2, radius 4.2");
        }
    }
    note("%ld found, %ld not found (%ld descents); in all %ld descents, %ld nodes; %ld wrong words",
         found, notfound, descended, count.descents, count.nodes, wrong);

    section("stats (the roof statistics) on random substitutions, as printed");
    for (int trial = 0; trial < 300; trial++) {
        size_t n = (size_t)rnd(1, 90);
        vector<vector<int>> w(n);
        vector<char> ok(n, 1);
        for (size_t j = 0; j < n; j++) {
            int len = (int)rnd(0, 20);
            for (int k = 0; k < len; k++) w[j].push_back((int)rnd(0, (int64_t)n - 1));
            if (trial % 3 == 1 && rnd(0, 9) == 0) { ok[j] = 0; w[j].clear(); }     // roofs not found
        }
        if (trial % 50 == 7) std::fill(ok.begin(), ok.end(), 0);                     // none found: no line
        std::string lo = capture([&] { legacy_stats(w, ok, "up  "); });
        std::string no = capture([&] { Policy::stats(w, ok, "up  "); });
        check(lo == no, "stats, trial %d: '%s' / '%s'", trial, lo.c_str(), no.c_str());
    }

    section("the convergence report: chain_point and dmul, bit for bit");
    for (int trial = 0; trial < 20000; trial++) {
        LD6 a, b;
        for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) { a[i][j] = gauss()*std::exp(gauss()); b[i][j] = gauss(); }
        if (trial % 4 == 0) a[5][5] = 1 + std::fabs(a[5][5]);                // d > 0
        if (trial % 7 == 0) a[5][5] = 0.5;                                     // d = 0
        LD6 lc = legacy_dmul(a, b);
        Policy::D6 na, nb;
        memcpy(&na, &a, sizeof a); memcpy(&nb, &b, sizeof b);
        Policy::D6 nc = Policy::dmul(na, nb);
        check(!memcmp(&lc, &nc, sizeof lc), "dmul, trial %d", trial);
        double ls[5], ns[5], ld, nd;
        legacy_pt(a, ls, ld);
        Policy::chain_point(na, ns, nd);
        check(!memcmp(ls, ns, sizeof ls) && same_bits(ld, nd), "chain_point, trial %d", trial);
    }
    {
        std::string lo = capture([&] { printf("   n    #points   max chord on S^4   d(o, chain) min..max\n"); });
        std::string no = capture([&] { Policy::chain_header(); });
        check(lo == no, "chain_header");
    }

    section("the convergence report: its rows (kernel_chain_rows), bit for bit");
    {
        int ti = legacy_pick_t(LB, 1);
        M6 t = LB.load(ti), ti_ = m6inv(t);
        long rows = 0;
        for (int dir = 0; dir < 2; dir++)
            for (int y : {0, 1, 17, 79}) {
                const vector<vector<int>>& th = dir == 0 ? up1 : down1;
                const M6& tt = dir == 0 ? t : ti_;
                for (size_t Lmax : {(size_t)400000, (size_t)3000}) {
                    vector<LRow> L = legacy_convergence_rows(LS, th, tt, y, Lmax);
                    std::vector<z::KernelChainRow> N;
                    z::kernel_chain_rows<Policy>(S, th, nm(tt), y, Lmax, [&](const z::KernelChainRow& r) { N.push_back(r); });
                    check(L.size() == N.size(), "%s, s_%d, Lmax %zu: %zu levels, new %zu", dir ? "down" : "up", y, Lmax,
                          L.size(), N.size());
                    for (size_t k = 0; k < std::min(L.size(), N.size()); k++) {
                        check(L[k].n == N[k].n && L[k].points == N[k].points && same_bits(L[k].mc, N[k].chord) &&
                              same_bits(L[k].dmin, N[k].dmin) && same_bits(L[k].dmax, N[k].dmax),
                              "%s, s_%d, Lmax %zu: level %zu", dir ? "down" : "up", y, Lmax, k);
                        rows++;
                    }
                }
            }
        note("%ld rows (the roofs of t over s_0, s_1, s_17, s_79, up and down, two limits on the points)", rows);
        // and what kernel_convergence prints, against the legacy lines
        std::string lo = capture([&] {
            printf("convergence of the chains for %s over the segment (1, s_%d):\n", "Z+ (t, theta_up)", 0);
            printf("   n    #points   max chord on S^4   d(o, chain) min..max\n");
            for (const LRow& r : legacy_convergence_rows(LS, up1, t, 0, 400000))
                printf("  %2d %10zu   %14.3e   %7.2f .. %.2f\n", r.n, r.points, r.mc, r.dmin, r.dmax);
        });
        std::string no = capture([&] { z::kernel_convergence<Policy>(S, up1, nm(t), 0, 400000, "Z+ (t, theta_up)"); });
        check(lo == no, "kernel_convergence, as printed");
    }

    end_section();
    printf("test_search_imm: %ld compared, %ld differ (%.1f s)\n", all_n, all_bad, z::now() - t_start);
    return all_bad ? 1 : 0;
}
