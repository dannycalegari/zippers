// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_search_cs: the generic block-filling search of search/kernel_search.h, with the policy of the Cartwright-Steger
// example (examples/cartwright_steger_search.h), agrees BIT FOR BIT with kernelmode of legacy/s3/chz.cc. The
// legacy code is a program, not a header, so the parts compared are copied below (section "the legacy code"), with their
// legacy names, on the legacy types of legacy/s3/csgroup.h (which this program includes, in the global namespace);
// where the legacy code had a constant that a test varies (the cap of the K-search), the copy takes it as a parameter.
// Compared:
//   - S_c (its indices and their order, which is that of std::sort, the elements, f2 and the words) and t (pick_t,
//     phi = -2..3 and 40);
//   - the K-search (kernel_fill), element by element with its f2 labels and its count of f-inconsistencies, at two
//     radii and with a small cap;
//   - the membership test: a_i t^-phi(a_i), what it prints and the words it keeps for the file;
//   - what the policy prints (print_t, print_no_t, print_kstore, stats on random data), against the legacy printf lines;
//   - chain_point and dmul of the convergence report, bit for bit, on random matrices.
// And, with no legacy original (chz never used it): the generic descent (kernel_express) on PU(2,1), whose words must
// be exactly the elements (up to the scalars zeta^k, B::same) and agree with the K-store where it already has them.
// Exact values are compared with memcmp, doubles by their bits. Built with -O2, the flags of the programs.

#include "zipcore.h"                      // legacy/s3: csgroup.h and zipcore.h

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_search.h"
#include "geometry/pu21/pu21.h"
#include "search/kernel_search.h"
#include "store/store.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

// The policy's constants are those of legacy/s3/chz.cc (const size_t CAP = 2000000; no descent and no exact check of
// the roofs; the convergence report stops at d > 34). A changed cap would pass every other
// fast test, so they are pinned here.
static_assert(zippers::cs::SearchPolicy::kstore_cap == 2000000, "chz: the K-store cap of legacy/s3/chz.cc");
static_assert(!zippers::cs::SearchPolicy::descent, "chz: no descent, as in legacy/s3/chz.cc");
static_assert(!zippers::cs::SearchPolicy::check_roofs, "chz: no exact check of the roofs, as in legacy/s3/chz.cc");
static_assert(zippers::cs::SearchPolicy::chain_stop == 34, "chz: the convergence report stops at d > 34");

namespace z = zippers;
typedef z::PU21 P;
typedef z::cs::SearchPolicy Policy;

// ---- bookkeeping (as in test_pu21.cc) ------------------------------------------------------------------------------

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

// the legacy and the new elements have the same layout (Zz: four i64 = int64_t on this machine)
static z::M3 nm(const M3& a) { z::M3 b; static_assert(sizeof a == sizeof b, "M3"); memcpy(&b, &a, sizeof b); return b; }

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

// ---- the legacy code (legacy/s3/chz.cc, kernelmode and the functions it calls), copied with its names ---------------

// t: the shortest element with phi = ph (kernelmode, for phi = 1 and for the steep t)
static int legacy_pick_t(const Store& Ball, int ph)
{
    int ti = -1;
    for (size_t i = 0; i < Ball.size(); i++)
        if (Ball.f0[i] == ph && (ti < 0 || Ball.dist[i] < Ball.dist[ti])) ti = i;
    return ti;
}

// S_c (kernelmode)
static vector<Gen> legacy_S(const Store& Ball, const vector<Gen>& G, double c, vector<int>& Sidx)
{
    Sidx.clear();
    for (size_t i = 1; i < Ball.size(); i++) if (Ball.f0[i] == 0 && Ball.dist[i] <= c) Sidx.push_back(i);
    std::sort(Sidx.begin(), Sidx.end(), [&](int a, int b) { return Ball.dist[a] < Ball.dist[b]; });
    vector<Gen> S;
    for (int i : Sidx) S.push_back({Ball.load(i), 0, Ball.f1[i], word(Ball, i, G)});
    return S;
}

// the K-search (kernelmode; CAP was the constant 2000000)
static bool legacy_kstore(Store& Kst, const vector<Gen>& S, double R, size_t CAP, long& badk)
{
    Kst.add(mnormal(mid()), -1, -1, 0, 0, 0, 0.f);
    badk = 0;
    bool capped = false;
    i64 key[12];
    Zz col[3];
    for (size_t q = 0; q < Kst.size() && !capped; q++) {
        M3 g = Kst.load(q);
        for (size_t j = 0; j < S.size(); j++) {
            int k = prodkey(g, S[j].m, key, col);
            int a1 = Kst.f1[q] + S[j].f1;
            int f = Kst.find(key);
            if (f >= 0) { if (Kst.f1[f] != a1) badk++; continue; }
            double d = hdist(col[2]);
            if (d > R) continue;
            M3 h = mrotk(mmul(g, S[j].m), k);
            Kst.add(h, q, j, Kst.depth[q] + 1, 0, a1, (float)d);
            if (Kst.size() >= CAP) { capped = true; break; }
        }
    }
    return capped;
}

// the membership test (kernelmode)
static int legacy_membership(const Store& Kst, const M3& t, vector<vector<int>>& mw)
{
    int memb = 0;
    mw.assign(3, {});
    for (int i = 0; i < 3; i++) {
        int ia = Kst.lookup(mmul(A[i], mpow(t, -FA[i][0])));
        if (ia >= 0) { memb++; mw[i] = Kst.path(ia); }
        printf("a%d t^%d in <S_c>: %s\n", i+1, -FA[i][0], ia >= 0 ? "yes" : "not reached");
    }
    return memb;
}

static void legacy_stats(const vector<vector<int>>& w, const vector<char>& ok, const char* nm)
{
    size_t mx = 0, mn = (size_t)-1;
    double sum = 0;
    long n = 0;
    for (size_t j = 0; j < w.size(); j++)
        if (ok[j]) { mx = std::max(mx, w[j].size()); mn = std::min(mn, w[j].size()); sum += w[j].size(); n++; }
    if (!n) return;
    printf("%s roof lengths (in S_c-steps): min %zu, mean %.2f, max %zu", nm, mn, sum/n, mx);
    if ((size_t)n < w.size()) { printf("\n"); return; }
    vector<double> x(w.size(), 1.0), y(w.size());
    double lam = 0;
    for (int it = 0; it < 300; it++) {
        std::fill(y.begin(), y.end(), 0.0);
        for (size_t j = 0; j < w.size(); j++) for (int l : w[j]) y[j] += x[l];
        double sx = 0, sy = 0;
        for (size_t j = 0; j < w.size(); j++) { sx += x[j]; sy += y[j]; }
        lam = sy/sx;
        for (size_t j = 0; j < w.size(); j++) x[j] = y[j]/sy*w.size();
    }
    printf(";  growth rate of the substitution %.4f\n", lam);
}

typedef std::array<std::array<cpx,3>,3> LD3;
static LD3 legacy_dmul(const LD3& a, const LD3& b)
{
    LD3 c;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) c[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] + a[i][2]*b[2][j];
    return c;
}
static void legacy_sphpoint(const LD3& m, double s[4], double& d)
{
    cpx c[3] = {m[0][2], m[1][2], m[2][2]};
    double p[4];
    ballpoint(c, p);
    double n = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2] + p[3]*p[3]);
    for (int i = 0; i < 4; i++) s[i] = p[i]/n;
    double a = std::abs(m[2][2]);
    d = a <= 1 ? 0 : 2*acosh(a);          // d(o, g o) = 2 acosh |g_33|, accurate near S^3
}

static LD3 todbl(const M3& g)
{
    LD3 m;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = zval(g.e[i][j]);
    return m;
}

// the loop of convergence, with its printf replaced by recording the row it printed
struct LRow { int n; size_t points; double mc, dmin, dmax; };
static vector<LRow> legacy_convergence_rows(const vector<Gen>& S, const vector<vector<int>>& th, const M3& t, int y, size_t Lmax)
{
    vector<LRow> rows;
    vector<LD3> Sd;
    for (auto& s : S) Sd.push_back(todbl(s.m));
    LD3 td = todbl(t), T = todbl(mid());
    vector<int> w = {y};
    for (int n = 0; n < 40; n++) {
        LD3 P = T;
        double prev[4], cur[4], d, dmin, dmax, mc = 0;
        legacy_sphpoint(P, prev, d);
        dmin = dmax = d;
        for (size_t m = 0; m < w.size(); m++) {
            P = legacy_dmul(P, Sd[w[m]]);
            legacy_sphpoint(P, cur, d);
            dmin = std::min(dmin, d); dmax = std::max(dmax, d);
            double e = 0;
            for (int i = 0; i < 4; i++) e += (cur[i]-prev[i])*(cur[i]-prev[i]);
            mc = std::max(mc, sqrt(e));
            memcpy(prev, cur, sizeof prev);
        }
        rows.push_back({n, w.size() + 1, mc, dmin, dmax});    // printf("  %2d %10zu   %14.3e   %7.2f .. %.2f\n", ...)
        size_t next = 0;
        for (int l : w) next += th[l].size();
        if (next > Lmax || dmax > 34) break;      // 34: double precision is still ample
        vector<int> w2;
        w2.reserve(next);
        for (int l : w) for (int x : th[l]) w2.push_back(x);
        w.swap(w2);
        T = legacy_dmul(T, td);
    }
    return rows;
}

// ---- helpers ---------------------------------------------------------------------------------------------------------

struct CappedPolicy : Policy {
    static constexpr size_t kstore_cap = 3000;
    using Policy::Policy;
};

// two stores, element by element: the element, its parent, letter, depth, labels and distance
static void compare_stores(const Store& L, const z::cs::Store& N, const char* what)
{
    check(L.size() == N.size(), "%s: %zu elements, %zu new", what, L.size(), N.size());
    size_t n = std::min(L.size(), N.size());
    long bad = 0;
    for (size_t i = 0; i < n; i++) {
        bool ok = same_bytes(L.load(i), N.load(i)) && L.parent[i] == N.parent[i] && L.gen[i] == N.letter[i] &&
                  L.depth[i] == N.depth[i] && !memcmp(&L.dist[i], &N.dist[i], sizeof(float)) &&
                  L.f0[i] == N.label[i].f0 && L.f1[i] == N.label[i].f1;
        if (!ok) bad++;
        sec_n++; all_n++;
        if (!ok) { sec_bad++; all_bad++; }
    }
    if (bad) printf("    DIFFERS: %s: %ld of %zu elements\n", what, bad, n);
}

static z::M3 product(const std::vector<Policy::Gen>& S, const std::vector<int>& w)
{
    z::M3 p = P::identity();
    for (int l : w) p = P::mul(p, S[l].m);
    return p;
}

int main()
{
    double t_start = z::now();
    printf("test_search_cs: the kernel search of src/search/ with Cartwright-Steger's policy against legacy/s3/chz.cc, "
           "bit for bit\n");
    setup();
    z::cs::setup();

    // the small ball of chz: geogens(4.5) and B(5.5)
    vector<Gen> LG = geogens(4.5);
    Store LB;
    enumerate(LB, LG, 5.5);
    Policy p;
    p.G = z::cs::geogens(4.5);
    z::cs::enumerate(p.ball, p.G, 5.5);

    section("the small ball B(5.5) (geogens, enumerate): the input");
    compare_stores(LB, p.ball, "B(5.5)");

    section("S_c (c = 3.8, 4, 4.2): indices, std::sort order, f2, words");
    vector<Gen> LS;
    std::vector<Policy::Gen> S;
    long ties = 0;
    for (double cc : {3.8, 4.0, 4.2}) {
        vector<int> Lidx;
        vector<Gen> L = legacy_S(LB, LG, cc, Lidx);
        std::vector<int> Nidx = p.kernel_indices(cc);
        check(Lidx == Nidx, "c = %.1f: the indices of S_c", cc);
        std::vector<Policy::Gen> N;
        for (int i : Nidx) N.push_back(p.generator(i));
        check(L.size() == N.size(), "c = %.1f: |S_c| %zu, new %zu", cc, L.size(), N.size());
        for (size_t j = 0; j < std::min(L.size(), N.size()); j++) {
            check(same_bytes(L[j].m, N[j].m), "c = %.1f: s_%zu", cc, j);
            check(L[j].f0 == N[j].f0 && L[j].f1 == N[j].f1, "c = %.1f: f of s_%zu", cc, j);
            check(L[j].name == N[j].name, "c = %.1f: the word of s_%zu", cc, j);
        }
        long tc = 0;
        for (size_t j = 1; j < Nidx.size(); j++) if (p.ball.dist[Nidx[j]] == p.ball.dist[Nidx[j-1]]) tc++;
        note("c = %.1f: |S_c| = %zu, %ld ties in d between neighbours", cc, N.size(), tc);
        if (cc == 4.0) { LS = L; S = N; ties = tc; }
    }
    check(p.check_S(S), "check_S");

    section("t: pick_t for phi = -2 .. 3 and 40, and print_t, print_no_t");
    for (int ph : {-2, -1, 0, 1, 2, 3, 40}) {
        int lt = legacy_pick_t(LB, ph), nt = p.pick_t(ph);
        check(lt == nt, "pick_t(%d): legacy %d, new %d", ph, lt, nt);
        if (nt >= 0) {
            std::string lo = capture([&] {
                printf("t = %s   d(o,t o) = %.4f   f(t) = (%d,%d)\n", word(LB, lt, LG).c_str(), LB.dist[lt], LB.f0[lt], LB.f1[lt]);
                printf("steep t = %s   d(o,t o) = %.4f   f(t) = (%d,%d)\n", word(LB, lt, LG).c_str(), LB.dist[lt], LB.f0[lt], LB.f1[lt]);
            });
            std::string no = capture([&] { p.print_t(nt, false); p.print_t(nt, true); });
            check(lo == no, "print_t(%d)", nt);
        } else {
            std::string lo = capture([&] { printf("no element with phi = %d in the small ball\n", ph); });
            std::string no = capture([&] { p.print_no_t(ph); });
            check(lo == no, "print_no_t(%d)", ph);
        }
        note("phi = %d: t = element %d", ph, nt);
    }
    {
        std::string lo = capture([&] { printf("no element with phi = 1 in the small ball\n"); });
        std::string no = capture([&] { p.print_no_t(1); });
        check(lo == no, "print_no_t(1), against the literal line of the legacy program");
    }

    section("the K-search (kernel_fill) at R = 7.5 and 8, and capped at 3000");
    Store LK;
    z::cs::Store K;
    for (double R : {7.5, 8.0}) {
        Store L;
        z::cs::Store N;
        long lbad = 0;
        Policy q;
        bool lc = legacy_kstore(L, LS, R, 2000000, lbad);
        bool nc = z::kernel_fill(q, N, S, R);
        check(lc == nc, "R = %.1f: capped %d, new %d", R, lc, nc);
        check(lbad == q.badk, "R = %.1f: f-inconsistencies %ld, new %ld", R, lbad, q.badk);
        compare_stores(L, N, R == 7.5 ? "K-store, R = 7.5" : "K-store, R = 8");
        note("R = %.1f: %zu elements, %ld f-inconsistencies", R, N.size(), q.badk);
        if (R == 8.0) { LK = L; K = N; }
    }
    {
        Store L;
        z::cs::Store N;
        long lbad = 0;
        CappedPolicy cp;
        bool lc = legacy_kstore(L, LS, 8.0, CappedPolicy::kstore_cap, lbad);
        bool nc = z::kernel_fill(cp, N, S, 8.0);
        check(lc && nc, "capped: legacy %d, new %d", lc, nc);
        check(lbad == cp.badk, "capped: f-inconsistencies %ld, new %ld", lbad, cp.badk);
        compare_stores(L, N, "K-store, R = 8, cap 3000");
        note("capped at %zu elements", N.size());
    }
    {
        std::string lo = capture([&] {
            printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d, f-inconsistencies %ld%s\n",
                   K.size(), 8.0, 0.5, 17, 0L, "");
            printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d, f-inconsistencies %ld%s\n",
                   K.size(), 8.0, 0.5, 17, 3L, "  [CAPPED]");
        });
        Policy q;
        std::string no = capture([&] {
            q.print_kstore(K.size(), 8.0, 0.5, 17, false);
            q.badk = 3;
            q.print_kstore(K.size(), 8.0, 0.5, 17, true);
        });
        check(lo == no, "print_kstore");
    }

    section("membership: a_i t^-phi(a_i), its lines and its words (R = 8)");
    for (int ph : {1, 2, 3}) {
        int ti = legacy_pick_t(LB, ph);
        M3 t = LB.load(ti);
        vector<vector<int>> lmw;
        int lm = 0;
        std::string lo = capture([&] { lm = legacy_membership(LK, t, lmw); });
        Policy q;
        auto find_word = [&](const z::M3& x, std::vector<int>& word) -> bool {      // kernel_search's, for chz
            int k = K.lookup(x);
            if (k >= 0) { word = K.path(k); return true; }
            return false;
        };
        z::KernelMembership nmb = z::KernelMembership::stop;
        std::string no = capture([&] { nmb = q.membership(nm(t), P::inv(nm(t)), find_word); });
        check(lo == no, "phi(t) = %d: the lines '%s' / '%s'", ph, lo.c_str(), no.c_str());
        check((lm == 3) == (nmb == z::KernelMembership::all) && nmb != z::KernelMembership::stop, "phi(t) = %d: the verdict", ph);
        check(lmw == q.mw, "phi(t) = %d: the member words", ph);
        note("phi(t) = %d: %d of 3 members", ph, lm);
    }

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
        std::string lo = capture([&] { legacy_stats(w, ok, "down"); });
        std::string no = capture([&] { Policy::stats(w, ok, "down"); });
        check(lo == no, "stats, trial %d: '%s' / '%s'", trial, lo.c_str(), no.c_str());
    }

    section("the convergence report: chain_point and dmul, bit for bit");
    for (int trial = 0; trial < 20000; trial++) {
        LD3 a, b;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
            a[i][j] = cpx(gauss(), gauss())*std::exp(gauss());
            b[i][j] = cpx(gauss(), gauss());
        }
        if (trial % 7 == 0) a[2][2] = cpx(0.3, 0.2);                           // |m_33| <= 1: d = 0
        LD3 lc = legacy_dmul(a, b);
        z::D3 na, nb;
        memcpy(&na, &a, sizeof a); memcpy(&nb, &b, sizeof b);
        z::D3 nc = Policy::dmul(na, nb);
        check(!memcmp(&lc, &nc, sizeof lc), "dmul, trial %d", trial);
        double ls[4], ns[4], ld, nd;
        legacy_sphpoint(a, ls, ld);
        Policy::chain_point(na, ns, nd);
        check(!memcmp(ls, ns, sizeof ls) && same_bits(ld, nd), "chain_point, trial %d", trial);
    }
    // and on the matrices of a real chain: t^n u o for the prefixes u of the up roofs of t
    {
        int ti = legacy_pick_t(LB, 1);
        z::D3 T = z::toD(nm(LB.load(ti)));
        LD3 LT;
        memcpy(&LT, &T, sizeof T);
        LD3 LP = LT;
        z::D3 NP = T;
        for (int k = 0; k < 400; k++) {
            const z::D3 s = z::toD(S[(size_t)rnd(0, (int64_t)S.size() - 1)].m);
            LD3 Ls;
            memcpy(&Ls, &s, sizeof s);
            LP = legacy_dmul(LP, Ls);
            NP = Policy::dmul(NP, s);
            check(!memcmp(&LP, &NP, sizeof LP), "a chain, step %d", k);
            double ls[4], ns[4], ld, nd;
            legacy_sphpoint(LP, ls, ld);
            Policy::chain_point(NP, ns, nd);
            check(!memcmp(ls, ns, sizeof ls) && same_bits(ld, nd), "a chain's point, step %d", k);
        }
    }
    {
        std::string lo = capture([&] { printf("   n    #points   max chord on S^3   d(o, chain) min..max\n"); });
        std::string no = capture([&] { Policy::chain_header(); });
        check(lo == no, "chain_header");
    }

    section("the convergence report: its rows (kernel_chain_rows), bit for bit");
    {
        // the roofs of t (phi = 1) in the K-store of R = 8 (a roof not found: an empty word)
        int ti = legacy_pick_t(LB, 1);
        M3 t = LB.load(ti), ti_ = minv(t);
        vector<vector<int>> up(LS.size()), down(LS.size());
        long nup = 0, ndown = 0;
        for (size_t j = 0; j < LS.size(); j++) {
            int iu = LK.lookup(mmul(mmul(ti_, LS[j].m), t)), id = LK.lookup(mmul(mmul(t, LS[j].m), ti_));
            if (iu >= 0) { up[j] = LK.path(iu); nup++; }
            if (id >= 0) { down[j] = LK.path(id); ndown++; }
        }
        long rows = 0;
        for (int dir = 0; dir < 2; dir++)
            for (int y : {0, 1, 20, 35}) {
                const vector<vector<int>>& th = dir == 0 ? up : down;
                const M3& tt = dir == 0 ? t : ti_;
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
        note("%ld rows (the roofs of t in B(8), %ld up and %ld down of 36, over s_0, s_1, s_20, s_35; two limits)",
             rows, nup, ndown);
        std::string lo = capture([&] {
            printf("convergence of the chains for %s over the segment (1, s_%d):\n", "Z- (t^-1, theta_down)", 0);
            printf("   n    #points   max chord on S^3   d(o, chain) min..max\n");
            for (const LRow& r : legacy_convergence_rows(LS, down, ti_, 0, 400000))
                printf("  %2d %10zu   %14.3e   %7.2f .. %.2f\n", r.n, r.points, r.mc, r.dmin, r.dmax);
        });
        std::string no = capture([&] { z::kernel_convergence<Policy>(S, down, nm(ti_), 0, 400000, "Z- (t^-1, theta_down)"); });
        check(lo == no, "kernel_convergence, as printed");
    }

    section("the generic descent on PU(2,1) (no legacy: chz never used it)");
    {
        std::vector<int> INV = z::kernel_inverses<P>(S);
        long noinv = 0;
        for (size_t j = 0; j < S.size(); j++) {
            if (INV[j] < 0) { noinv++; continue; }
            check(P::same(S[INV[j]].m, P::inv(S[j].m)), "INV of s_%zu", j);
        }
        check(noinv == 0, "%ld elements of S_c without an inverse in S_c", noinv);
        // a small K-store, so that most roofs are outside it
        z::cs::Store K7;
        Policy q;
        z::kernel_fill(q, K7, S, 7.0);
        z::KernelDescentCount count;
        long found = 0, descended = 0, notfound = 0, inside = 0;
        for (int ph : {1, 2}) {
            z::M3 t = p.ball.load(p.pick_t(ph)), tinv = P::inv(t);
            for (size_t j = 0; j < S.size(); j++)
                for (int up = 0; up < 2; up++) {
                    z::M3 x = up ? P::mul(P::mul(tinv, S[j].m), t) : P::mul(P::mul(t, S[j].m), tinv);
                    std::vector<int> w = {-7};
                    long d0 = count.descents;
                    bool f = z::kernel_express(K7, S, INV, x, w, 20000, 14.0, count);
                    if (count.descents == d0) {
                        inside++;
                        check(f && w == K7.path(K7.lookup(x)), "a roof in the K-store: the word is its path");
                    }
                    if (f) {
                        found++;
                        if (count.descents > d0) descended++;
                        check(P::same(product(S, w), x), "phi(t) = %d, roof %zu (%s): the word is not the element",
                              ph, j, up ? "up" : "down");
                    } else {
                        notfound++;
                        check(w.size() == 1 && w[0] == -7, "a word not found was changed");
                    }
                }
        }
        // an element not in K: the descent must fail
        std::vector<int> w = {-7};
        bool f = z::kernel_express(K7, S, INV, p.ball.load(p.pick_t(1)), w, 2000, 14.0, count);
        check(!f && w.size() == 1, "t is not in K: the descent must fail");
        note("K-store B(7): %zu elements; 144 roofs of t and t^2: %ld found (%ld in the K-store, %ld by a descent), %ld not",
             K7.size(), found, inside, descended, notfound);
        note("%ld descents, %ld nodes", count.descents, count.nodes);
        check(descended > 0, "no roof needed the descent: the test tests nothing");
    }

    end_section();
    printf("test_search_cs: %ld compared, %ld differ (%.1f s); S_c (c = 4) has %ld ties in d between neighbours, so "
           "its order is that of std::sort\n", all_n, all_bad, z::now() - t_start, ties);
    return all_bad ? 1 : 0;
}
