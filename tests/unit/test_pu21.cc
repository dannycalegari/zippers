// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_pu21: the S^3 code moved into src/ agrees BIT FOR BIT with its legacy original in legacy/s3/
// (csgroup.h and zipcore.h, which this program includes, in the global namespace):
//   - Z[zeta_12] (geometry/pu21/zeta12.h) and the 3x3 matrices over it (geometry/pu21/pu21.h);
//   - every member of the backend zippers::PU21, against the legacy function it replaces (geometry/backend.h);
//   - normal(mul(g, s)) against the legacy K-search's mrotk(mmul(g, s), k), with the k of prodkey;
//   - splitmix, fingerprint and KeySet (core/hash.h, core/keyset.h);
//   - the store (store/store.h) and the Cartwright-Steger example (examples/cartwright_steger.h): the CKY data, the
//     generators, the balls of enumerate element by element, lookups, the words of the substitution files, the roots
//     and the shell radii;
//   - the storage guard of pack and the guard of the ball enumeration, in child processes (they exit).
// Exact values are compared with memcmp, doubles by their bits. Built with the flags of the programs (-O2), under which
// the results must agree. ZIPPERS_REPO is the repository (for the substitution files of legacy/s3/).

#include "zipcore.h"                      // legacy/s3: csgroup.h and zipcore.h

#include "core/hash.h"
#include "core/keyset.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "geometry/pu21/pu21.h"
#include "geometry/pu21/zeta12.h"
#include "store/store.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;

// ---- bookkeeping -----------------------------------------------------------------------------------------------------

static const char* sec_name = nullptr;
static long sec_n = 0, sec_bad = 0, all_n = 0, all_bad = 0;
static std::string sec_notes;              // printed under the section's line

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

// the legacy and the new types have the same layout
static z::Zz nz(const Zz& a) { z::Zz b; memcpy(&b, &a, sizeof b); return b; }
static z::M3 nm(const M3& a) { z::M3 b; memcpy(&b, &a, sizeof b); return b; }

// ---- random inputs ---------------------------------------------------------------------------------------------------

static std::mt19937_64 rng(20261025);
static int64_t rnd(int64_t lo, int64_t hi) { return std::uniform_int_distribution<int64_t>(lo, hi)(rng); }
static double gauss() { return std::normal_distribution<double>(0, 1)(rng); }

static Zz random_zz(int64_t m) { return Z(rnd(-m, m), rnd(-m, m), rnd(-m, m), rnd(-m, m)); }

static std::vector<int> random_word(int ngens, int len)
{
    std::vector<int> w(len);
    for (int& x : w) x = (int)rnd(0, ngens - 1);
    return w;
}

// a word in the generators, multiplied out (not normalised), with the legacy and with the new arithmetic
static M3 legacy_product(const std::vector<Gen>& G, const std::vector<int>& w)
{
    M3 g = mid();
    for (int x : w) g = mmul(g, G[x].m);
    return g;
}
static z::M3 new_product(const std::vector<z::cs::Gen>& G, const std::vector<int>& w)
{
    z::M3 g = z::mid();
    for (int x : w) g = z::mmul(g, G[x].m);
    return g;
}

static cpx random_cpx() { return cpx(gauss(), gauss()); }

// ---- child processes (for the paths that exit) -----------------------------------------------------------------------

// run f in a child process; its exit status (or -signal), and what it wrote to stderr
static int in_child(const std::function<void()>& f, std::string& err)
{
    int fd[2];
    if (pipe(fd) != 0) { perror("pipe"); return -1000; }
    fflush(stdout); fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1000; }
    if (pid == 0) {
        dup2(fd[1], 2);
        close(fd[0]); close(fd[1]);
        f();
        _exit(0);                         // f returned
    }
    close(fd[1]);
    err.clear();
    char buf[512];
    ssize_t n;
    while ((n = read(fd[0], buf, sizeof buf)) > 0) err.append(buf, (size_t)n);
    close(fd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
}

// ---- the stores ------------------------------------------------------------------------------------------------------

static void compare_stores(const Store& L, const z::cs::Store& N, const char* what)
{
    check(L.size() == N.size(), "%s: %zu elements, legacy %zu", what, N.size(), L.size());
    check(L.mat.size() == N.packed.size() &&
          !memcmp(L.mat.data(), N.packed.data(), L.mat.size()*sizeof(int32_t)), "%s: the packed elements", what);
    size_t n = std::min(L.size(), N.size());
    for (size_t i = 0; i < n; i++) {
        check(same_bytes(L.load(i), N.load(i)), "%s: element %zu", what, i);
        check(L.parent[i] == N.parent[i], "%s: parent of %zu", what, i);
        check(L.gen[i] == N.letter[i], "%s: letter of %zu", what, i);
        check(L.depth[i] == N.depth[i], "%s: depth of %zu", what, i);
        check(L.f0[i] == N.label[i].f0 && L.f1[i] == N.label[i].f1, "%s: f of %zu", what, i);
        check(!memcmp(&L.dist[i], &N.dist[i], sizeof(float)), "%s: dist of %zu", what, i);
    }
}

static void compare_gens(const std::vector<Gen>& L, const std::vector<z::cs::Gen>& N, const char* what)
{
    check(L.size() == N.size(), "%s: %zu generators, legacy %zu", what, N.size(), L.size());
    for (size_t i = 0; i < std::min(L.size(), N.size()); i++) {
        check(same_bytes(L[i].m, N[i].m), "%s: generator %zu", what, i);
        check(L[i].f0 == N[i].f0 && L[i].f1 == N[i].f1, "%s: f of generator %zu", what, i);
        check(L[i].name == N[i].name, "%s: name of generator %zu", what, i);
    }
}

// ----------------------------------------------------------------------------------------------------------------------

int main()
{
    double t_start = now();
    setup();                               // legacy: F', F'^-1, u', v', b', j, a1, a2, a3
    z::cs::setup();
    printf("test_pu21: src/ against legacy/s3, bit for bit\n");

    // ---- Z[zeta_12] ----
    section("Z[zeta]: Z, zadd, zsub, zmul, zrot, zrotk, zconj, zeq, zzero, zless");
    for (int t = 0; t < 40000; t++) {
        int64_t m = t % 2 ? (1 << 20) : 3;
        Zz a = random_zz(m), b = random_zz(m);
        if (t % 50 == 0) b = a;
        z::Zz na = nz(a), nb = nz(b);
        check(same_bytes(Z(a.c[0], a.c[1], a.c[2], a.c[3]), z::Z(a.c[0], a.c[1], a.c[2], a.c[3])), "Z");
        check(same_bytes(zadd(a, b), z::zadd(na, nb)), "zadd");
        check(same_bytes(zsub(a, b), z::zsub(na, nb)), "zsub");
        check(same_bytes(zmul(a, b), z::zmul(na, nb)), "zmul");
        check(same_bytes(zrot(a), z::zrot(na)), "zrot");
        int k = (int)rnd(-30, 30);
        check(same_bytes(zrotk(a, k), z::zrotk(na, k)), "zrotk %d", k);
        check(same_bytes(zconj(a), z::zconj(na)), "zconj");
        check(zeq(a, b) == z::zeq(na, nb), "zeq");
        check(zzero(a) == z::zzero(na), "zzero");
        check(zzero(zsub(a, a)) == z::zzero(z::zsub(na, na)), "zzero of 0");
        check(zless(a, b) == z::zless(na, nb) && zless(b, a) == z::zless(nb, na), "zless");
    }
    section("Z[zeta]: normk, zval (zeta, zeta^5, other w), ZETA, ZETA5");
    check(same_bytes(ZETA, z::ZETA) && same_bytes(ZETA5, z::ZETA5), "ZETA, ZETA5");
    for (int t = 0; t < 40000; t++) {
        Zz a = random_zz(t % 2 ? (1 << 20) : 2);
        if (zzero(a)) continue;
        z::Zz na = nz(a);
        check(normk(a) == z::normk(na), "normk");
        check(same_bytes(zval(a), z::zval(na)), "zval");
        check(same_bytes(zval(a, ZETA5), z::zval(na, z::ZETA5)), "zval at zeta^5");
        cpx w = random_cpx();
        check(same_bytes(zval(a, w), z::zval(na, w)), "zval at w");
    }

    // ---- random elements of Pi: words in a_i^{+-1} ----
    std::vector<Gen> LA = agens();
    std::vector<z::cs::Gen> NA = z::cs::agens();
    section("the CKY data: F', F'^-1, u', v', b', j, a1, a2, a3, f(a_i), agens");
    check(same_bytes(FP, z::FP) && same_bytes(FPI, z::FPI), "F', F'^-1");
    check(same_bytes(U, z::cs::U) && same_bytes(V, z::cs::V) && same_bytes(B, z::cs::B) && same_bytes(J, z::cs::J),
          "u', v', b', j");
    for (int i = 0; i < 3; i++) check(same_bytes(A[i], z::cs::A[i]), "a%d", i + 1);
    check(!memcmp(FA, z::cs::FA, sizeof FA), "f(a_i)");
    compare_gens(LA, NA, "agens");

    std::vector<M3> E;                     // random elements, legacy representation
    section("words in a_i^{+-1} multiplied out by both (mmul), lengths 0..10");
    for (int t = 0; t < 3000; t++) {
        std::vector<int> w = random_word(6, t % 11);
        M3 g = legacy_product(LA, w);
        check(same_bytes(g, new_product(NA, w)), "the product of a word of length %zu", w.size());
        E.push_back(g);
        if (t % 3 == 0) E.push_back(mnormal(g));
        if (t % 5 == 0) E.push_back(mrotk(g, (int)rnd(0, 11)));
    }
    note("(%zu random elements: products of words, some normalised, some rotated)", E.size());

    section("3x3 matrices: mzero, mid, mmul, madj, mrotk, meq, minv, mnormal, maxcoef");
    check(same_bytes(mzero(), z::mzero()) && same_bytes(mid(), z::mid()), "mzero, mid");
    for (size_t t = 0; t < E.size(); t++) {
        const M3& a = E[t];
        const M3& b = E[(size_t)rnd(0, (int64_t)E.size() - 1)];
        z::M3 na = nm(a), nb = nm(b);
        check(same_bytes(mmul(a, b), z::mmul(na, nb)), "mmul");
        check(same_bytes(madj(a), z::madj(na)), "madj");
        int k = (int)rnd(-13, 13);
        check(same_bytes(mrotk(a, k), z::mrotk(na, k)), "mrotk");
        check(meq(a, b) == z::meq(na, nb) && meq(a, a) == z::meq(na, na), "meq");
        check(same_bytes(minv(a), z::minv(na)), "minv");
        check(same_bytes(mnormal(a), z::mnormal(na)), "mnormal");
        check(maxcoef(a) == z::maxcoef(na), "maxcoef");
    }
    section("3x3 matrices: mscalar (on scalars and others), mpow (n = -3..3)");
    for (size_t t = 0; t < E.size(); t++) {
        const M3& a = E[t];
        z::M3 na = nm(a);
        M3 s = mrotk(mmul(a, minv(a)), (int)rnd(0, 11));       // a scalar zeta^k I
        check(mscalar(s) == z::mscalar(nm(s)), "mscalar of a scalar");
        check(mscalar(a) == z::mscalar(na), "mscalar");
        if (z::maxcoef(na) < 4096) {
            int n = (int)rnd(-3, 3);
            check(same_bytes(mpow(a, n), z::mpow(na, n)), "mpow %d", n);
        }
    }

    // ---- the backend ----
    section("PU21: identity, mul, inv, normal, eq, same, max_coefficient");
    check(same_bytes(mid(), z::PU21::identity()), "identity");
    for (size_t t = 0; t < E.size(); t++) {
        const M3& a = E[t];
        M3 b = t % 4 == 0 ? mrotk(a, (int)rnd(1, 11)) : E[(size_t)rnd(0, (int64_t)E.size() - 1)];
        z::M3 na = nm(a), nb = nm(b);
        check(same_bytes(mmul(a, b), z::PU21::mul(na, nb)), "mul");
        check(same_bytes(minv(a), z::PU21::inv(na)), "inv");
        check(same_bytes(mnormal(a), z::PU21::normal(na)), "normal");
        check(meq(a, b) == z::PU21::eq(na, nb) && z::PU21::eq(na, na), "eq");
        check(same(a, b) == z::PU21::same(na, nb), "same");
        check(same(a, a) == z::PU21::same(na, na), "same (a, a)");
        check(maxcoef(a) == z::PU21::max_coefficient(na), "max_coefficient");
    }
    section("PU21: key, product_key, displacement, key_displacement (prodkey, hdist)");
    for (size_t t = 0; t < E.size(); t++) {
        const M3& a = E[t];
        const M3& s = E[(size_t)rnd(0, (int64_t)E.size() - 1)];
        z::M3 na = nm(a), ns = nm(s);
        i64 key[12];
        Zz col[3];
        prodkey(a, mid(), key, col);
        z::PU21::Key k1 = z::PU21::key(na);
        check(!memcmp(key, k1.data(), sizeof key), "key");
        int kk = prodkey(a, s, key, col);
        z::PU21::Key k2 = z::PU21::product_key(na, ns);
        check(!memcmp(key, k2.data(), sizeof key), "product_key");
        check(same_bits(hdist(col[2]), z::PU21::key_displacement(k2)), "key_displacement");
        z::Zz ncol[3];
        i64 key2[12];
        int nk = z::prodkey(na, ns, key2, ncol);
        check(kk == nk && !memcmp(key, key2, sizeof key) && !memcmp(col, ncol, sizeof col), "prodkey");
        M3 an = mnormal(a);
        check(same_bits(hdist(an.e[2][2]), z::PU21::displacement(nm(an))), "displacement (normalised)");
        check(same_bits(hdist(a.e[2][2]), z::PU21::displacement(na)), "displacement (any representative)");
        check(same_bits(hdist(a.e[2][2]), z::hdist(nz(a.e[2][2]))), "hdist");
    }
    section("PU21: pack, unpack, packed_key (the legacy Store's mat, load, keyof)");
    {
        Store st;
        for (size_t t = 0; t < E.size(); t++) {
            const M3& a = E[t];
            if (maxcoef(a) > (1LL << 28)) continue;
            int i = st.add(a, -1, -1, 0, 0, 0, 0.f);
            int32_t buf[36];
            z::PU21::pack(nm(a), buf);
            check(!memcmp(&st.mat[36*(size_t)i], buf, sizeof buf), "pack");
            check(same_bytes(st.load(i), z::PU21::unpack(buf)), "unpack");
            i64 k[12];
            st.keyof(i, k);
            z::PU21::Key nk = z::PU21::packed_key(buf);
            check(!memcmp(k, nk.data(), sizeof k), "packed_key");
        }
    }
    section("PU21: to_double, act (toD, mv)");
    for (size_t t = 0; t < E.size(); t++) {
        const M3& a = E[t];
        D3 m = toD(a);
        check(same_bytes(m, z::PU21::to_double(nm(a))) && same_bytes(m, z::toD(nm(a))), "to_double");
        if (t % 2) for (auto& row : m) for (auto& x : row) x = random_cpx();
        C3 v = {random_cpx(), random_cpx(), random_cpx()};
        check(same_bytes(mv(m, v), z::PU21::act(m, v)), "act");
    }
    section("PU21: attracting (loxodromic elements), boundary (null and other vectors)");
    {
        std::vector<C3> nulls;
        for (int t = 0; t < 1500; t++) {
            std::vector<int> w = random_word(6, (int)rnd(1, 6));
            M3 g = legacy_product(LA, w);
            if (mscalar(g) >= 0) continue;                       // the identity: not loxodromic
            if (t % 2) g = mnormal(g);
            C3 v = attracting(g);
            check(same_bytes(v, z::PU21::attracting(nm(g))), "attracting");
            nulls.push_back(v);
            const M3& h = E[(size_t)rnd(0, (int64_t)E.size() - 1)];
            nulls.push_back(mv(toD(h), v));                      // the fixed point moved: null again
        }
        for (const C3& v : nulls) check(same_bytes(spoint(v), z::PU21::boundary(v)), "boundary of a null vector");
        for (int t = 0; t < 20000; t++) {
            C3 v = {random_cpx(), random_cpx(), random_cpx()};
            check(same_bytes(spoint(v), z::PU21::boundary(v)), "boundary");
            check(same_bytes(spoint(v), z::spoint(v)), "spoint");
            double p0[4], p1[4];
            ballpoint(v.data(), p0);
            z::ballpoint(v.data(), p1);
            check(!memcmp(p0, p1, sizeof p0), "ballpoint");
        }
        check(same_bits(SQ3, z::SQ3), "SQ3");
    }

    // ---- hashes ----
    section("splitmix, fingerprint (12-integer keys)");
    for (int t = 0; t < 200000; t++) {
        uint64_t x = rng();
        check(splitmix(x) == z::splitmix(x), "splitmix");
        z::PU21::Key k;
        int64_t m = t % 3 == 0 ? 3 : t % 3 == 1 ? (1LL << 30) : INT64_MAX;
        for (auto& c : k) c = rnd(-m, m);
        int letter = (int)rnd(-1, 2000);
        check(fingerprint(k.data(), letter) == z::fingerprint(k, letter), "fingerprint");
    }
    section("KeySet: the same answers to the same inserts and queries");
    {
        KeySet L;
        z::KeySet N;
        std::vector<uint64_t> pool(100000);
        for (auto& x : pool) { do x = rng(); while (!x); }
        for (int t = 0; t < 400000; t++) {
            uint64_t k = t % 7 == 6 ? (rng() | 1) : pool[(size_t)rnd(0, (int64_t)pool.size() - 1)];
            if (t % 3) check(L.insert(k) == N.insert(k), "insert");
            else check(L.contains(k) == N.contains(k), "contains");
        }
        check(L.t == N.t && L.n == N.n && L.mask == N.mask, "the tables");
        note("(%zu keys in a table of %zu)", N.n, N.t.size());
    }

    // ---- the store and the example ----
    section("geogens(4.5): the geometric generators");
    std::vector<Gen> LG = geogens(4.5);
    std::vector<z::cs::Gen> NG = z::cs::geogens(4.5);
    compare_gens(LG, NG, "geogens(4.5)");
    note("(%zu generators)", NG.size());

    section("enumerate B(5) with geogens(4.5): elements, parents, letters, ...");
    Store LS;
    z::cs::Store NS;
    long lb = enumerate(LS, LG, 5.0), nb = z::cs::enumerate(NS, NG, 5.0);
    check(lb == nb, "the f-inconsistencies, %ld and %ld", nb, lb);
    compare_stores(LS, NS, "B(5)");
    note("(%zu elements, %ld f-inconsistencies)", NS.size(), nb);
    section("enumerate B(5): word and path of every element");
    for (size_t i = 0; i < LS.size() && i < NS.size(); i++) {
        check(word(LS, i, LG) == z::cs::word(NS, i, NG), "word of %zu", i);
        check(LS.path(i) == NS.path(i), "path of %zu", i);
    }
    section("enumerate B(6) with agens: elements, parents, letters, ...");
    {
        Store L6;
        z::cs::Store N6;
        long l6 = enumerate(L6, LA, 6.0), n6 = z::cs::enumerate(N6, NA, 6.0);
        check(l6 == n6, "the f-inconsistencies");
        compare_stores(L6, N6, "B(6) with agens");
        note("(%zu elements)", N6.size());
    }

    section("normal(mul(g, s)) = mrotk(mmul(g, s), k), k from prodkey; find");
    for (int t = 0; t < 30000; t++) {
        size_t i = (size_t)rnd(0, (int64_t)LS.size() - 1), j = (size_t)rnd(0, (int64_t)LG.size() - 1);
        M3 g = LS.load(i);
        const M3& s = LG[j].m;
        i64 key[12];
        Zz col[3];
        int k = prodkey(g, s, key, col);
        M3 h = mrotk(mmul(g, s), k);
        z::M3 nh = z::PU21::normal(z::PU21::mul(NS.load(i), NG[j].m));
        check(same_bytes(h, nh), "normal(mul(g, s)) for element %zu and generator %zu", i, j);
        check(same_bytes(h, mnormal(mmul(g, s))), "the legacy mrotk(.., k) is mnormal");
        check(LS.find(key) == NS.find(z::PU21::product_key(NS.load(i), NG[j].m)), "find");
    }
    section("Store::lookup of products, inverses and other elements");
    for (int t = 0; t < 30000; t++) {
        size_t i = (size_t)rnd(0, (int64_t)LS.size() - 1);
        M3 g = LS.load(i), x;
        int kind = t % 4;
        if (kind == 0) x = mmul(g, LG[(size_t)rnd(0, (int64_t)LG.size() - 1)].m);
        else if (kind == 1) x = minv(g);
        else if (kind == 2) x = mmul(g, LS.load((size_t)rnd(0, (int64_t)LS.size() - 1)));
        else x = mrotk(mmul(g, LA[(size_t)rnd(0, 5)].m), (int)rnd(0, 11));
        check(LS.lookup(x) == NS.lookup(nm(x)), "lookup");
    }

    section("parse_word on the words of the substitution files of legacy/s3");
    {
        const char* files[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
        for (const char* f : files) {
            std::string fn = std::string(ZIPPERS_REPO) + "/legacy/s3/" + f;
            Subst D;
            check(load_subst(fn.c_str(), D), "reading %s", fn.c_str());
            std::vector<std::string> words = D.sword;
            words.push_back(D.tword);
            for (const std::string& w : words) check(same_bytes(parse_word(w), z::cs::parse_word(w)), "%s: %s", f, w.c_str());
        }
    }

    section("roots_info, find_roots, shell_radii at R = 0, 2.554, 3.4");
    for (double R : {0.0, 2.554, 3.4}) {
        std::vector<RootInfo> L = roots_info(R);
        std::vector<z::cs::RootInfo> N = z::cs::roots_info(R);
        check(L.size() == N.size(), "roots_info(%g): %zu roots, legacy %zu", R, N.size(), L.size());
        for (size_t i = 0; i < std::min(L.size(), N.size()); i++) {
            check(same_bytes(L[i].g, N[i].g) && same_bits(L[i].d, N[i].d) && L[i].f0 == N[i].f0 && L[i].f1 == N[i].f1 &&
                  L[i].word == N[i].word, "roots_info(%g): root %zu", R, i);
        }
        std::vector<M3> lr;
        std::vector<z::M3> nr;
        find_roots(R, lr);
        z::cs::find_roots(R, nr);
        check(lr.size() == nr.size() && (lr.empty() || !memcmp(lr.data(), nr.data(), lr.size()*sizeof(M3))),
              "find_roots(%g)", R);
        std::vector<double> ls = shell_radii(R), ns = z::cs::shell_radii(R);
        check(ls.size() == ns.size() && (ls.empty() || !memcmp(ls.data(), ns.data(), ls.size()*sizeof(double))),
              "shell_radii(%g)", R);
        note("(R = %g: %zu roots, %zu shell radii)", R, N.size(), ns.size());
    }

    section("guards (in child processes): pack's 2^28, guard(D)");
    {
        std::string le, ne, se;
        for (int64_t c : {(int64_t)(1LL << 28), -(int64_t)(1LL << 28)}) {     // at the guard: accepted
            M3 g = mid();
            g.e[1][2].c[3] = c;
            Store st;
            st.add(g, -1, -1, 0, 0, 0, 0.f);
            int32_t buf[36];
            z::PU21::pack(nm(g), buf);
            check(!memcmp(st.mat.data(), buf, sizeof buf), "pack at the guard (%lld)", (long long)c);
        }
        for (int64_t c : {(int64_t)(1LL << 28) + 1, -(int64_t)(1LL << 28) - 1, (int64_t)1 << 40}) {   // past it
            M3 g = mid();
            g.e[2][0].c[1] = c;
            int ls = in_child([&] { Store st; st.add(g, -1, -1, 0, 0, 0, 0.f); }, le);
            int ns = in_child([&] { int32_t buf[36]; z::PU21::pack(nm(g), buf); }, ne);
            int ss = in_child([&] { z::cs::Store st; st.add(nm(g), -1, -1, 0, z::cs::FVal{0, 0}, 0.f); }, se);
            check(ls == 1 && ns == 1 && ss == 1, "exit status past the guard: %d, %d, legacy %d", ns, ss, ls);
            check(le == ne && le == se, "the message past the guard: \"%s\", legacy \"%s\"", ne.c_str(), le.c_str());
        }
        note("(past the guard: \"%s\")", std::string(le, 0, le.size() ? le.size() - 1 : 0).c_str());
        int ls = in_child([] { guard(40); }, le), ns = in_child([] { z::cs::guard(40); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "guard(40): %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        ls = in_child([] { guard(5); }, le);
        ns = in_child([] { z::cs::guard(5); }, ne);
        check(ls == 0 && ns == 0 && le.empty() && ne.empty(), "guard(5) passes");
    }

    section("now(): the same clock");
    for (int t = 0; t < 1000; t++) {
        double a = now(), b = z::now(), c = now();
        check(a <= b && b <= c, "now");
    }

    end_section();
    printf("test_pu21: %ld comparisons with the legacy code, %ld differ (%.1f s)\n", all_n, all_bad, now() - t_start);
    return all_bad ? 1 : 0;
}
