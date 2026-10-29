// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_on1: the S^4 code moved into src/ agrees BIT FOR BIT with its legacy original in legacy/s4/
// (rh5.h and zipcore5.h, which this program includes, in the global namespace):
//   - every member of the backend zippers::ON1<5> (geometry/on1/on1.h), against the legacy function it replaces
//     (geometry/backend.h);
//   - splitmix, fingerprint (6-integer keys, against fingerprint6) and KeySet (core/hash.h, core/keyset.h);
//   - the store (store/store.h) and the IMM example (examples/imm_m5.h): the facets, colouring, moves, initial state
//     and reflections, hstep, word_mu_h, word_matrix, the ball B(3) of enumerate6 element by element, lookups, fword,
//     parse_fword and word_matrix on the words of the substitution files, and the roots of find_roots5;
//   - parse_fword, corrected (it reads facet 1, which the legacy reader dropped), against the legacy one on
//     the words without facet 1, and on the words with it: the new list is the list of tokens, the legacy one is that
//     list without facet 1; round trips with fword, and a bad letter;
//   - the int32 guard of pack, the guard of the ball enumeration and the odd-entry check of mul, in child processes
//     (they exit).
// Exact values are compared with memcmp, doubles by their bits. Built with the flags of the programs (-O2), under which
// the results must agree. ZIPPERS_REPO is the repository (for the substitution files of legacy/s4/).

#include "zipcore5.h"                     // legacy/s4: rh5.h and zipcore5.h

#include "core/hash.h"
#include "core/keyset.h"
#include "core/timing.h"
#include "examples/imm_m5.h"
#include "geometry/on1/on1.h"
#include "store/store.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::ON1<5> O;

// ON1 is a template: other dimensions are backends too (the suite uses n = 5)
static_assert(z::is_backend<z::ON1<3>>() && z::is_backend<z::ON1<4>>(), "ON1<3> and ON1<4> are backends");

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
static O::Elem nm(const M6& a) { O::Elem b; memcpy(&b, &a, sizeof b); return b; }

// the last column of a legacy element: the key of Store6::lookup
static std::array<int64_t, 6> lastcol(const M6& g)
{
    std::array<int64_t, 6> c;
    for (int r = 0; r < 6; r++) c[r] = g.e[r][5];
    return c;
}

// ---- random inputs ---------------------------------------------------------------------------------------------------

static std::mt19937_64 rng(20261025);
static int64_t rnd(int64_t lo, int64_t hi) { return std::uniform_int_distribution<int64_t>(lo, hi)(rng); }
static double gauss() { return std::normal_distribution<double>(0, 1)(rng); }

static std::vector<int> random_fword(int len)
{
    std::vector<int> w(len);
    for (int& x : w) x = (int)rnd(0, 15);
    return w;
}

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

static std::string chomp(const std::string& s) { return s.size() && s.back() == '\n' ? s.substr(0, s.size() - 1) : s; }

// ---- the stores ------------------------------------------------------------------------------------------------------

static void compare_stores(const Store6& L, const z::imm::Store6& N, const char* what)
{
    check(L.size() == N.size(), "%s: %zu elements, legacy %zu", what, N.size(), L.size());
    check(L.mat.size() == N.packed.size() &&
          !memcmp(L.mat.data(), N.packed.data(), L.mat.size()*sizeof(int32_t)), "%s: the packed elements", what);
    size_t n = std::min(L.size(), N.size());
    for (size_t i = 0; i < n; i++) {
        check(same_bytes(L.load(i), N.load(i)), "%s: element %zu", what, i);
        check(L.parent[i] == N.parent[i], "%s: parent of %zu", what, i);
        check(L.letter[i] == N.letter[i], "%s: letter of %zu", what, i);
        check(L.depth[i] == N.depth[i], "%s: depth of %zu", what, i);
        check(L.mu[i] == N.label[i].mu && L.h[i] == N.label[i].h, "%s: (mu, h) of %zu", what, i);
        check(!memcmp(&L.dist[i], &N.dist[i], sizeof(float)), "%s: dist of %zu", what, i);
    }
}

// ----------------------------------------------------------------------------------------------------------------------

int main()
{
    double t_start = now();
    rh5_setup();                           // legacy: facets, colouring, moves, initial state, reflections
    z::imm::setup();
    printf("test_on1: src/ against legacy/s4, bit for bit\n");

    // ---- the data of the example ----
    section("IMM: facets, sign vectors, colours, moves, initial state, reflections");
    for (int f = 0; f < 16; f++) check(!strcmp(FACET_NAME[f], z::imm::FACET_NAME[f]), "the name of facet %d", f);
    check(!memcmp(FS, z::imm::FS, sizeof FS), "the sign vectors");
    check(!memcmp(COLOUR, z::imm::COLOUR, sizeof COLOUR), "the colours");
    check(!memcmp(MOVE, z::imm::MOVE, sizeof MOVE), "the moves");
    check(!memcmp(INIT_I, z::imm::INIT_I, sizeof INIT_I), "the initial state");
    for (int f = 0; f < 16; f++) check(same_bytes(REF[f], z::imm::REF[f]), "the reflection in facet %d", f);
    section("IMM: hstep (every state and facet), word_mu_h, word_matrix");
    for (int mu = 0; mu < 16; mu++)
        for (int f = 0; f < 16; f++) check(hstep(mu, f) == z::imm::hstep(mu, f), "hstep(%d, %d)", mu, f);
    std::vector<M6> E;                     // random elements, legacy representation
    std::vector<std::vector<int>> EW;      // their words
    for (int t = 0; t < 4000; t++) {
        std::vector<int> w = random_fword(t % 31);
        int m0, h0, m1, h1;
        word_mu_h(w, m0, h0);
        z::imm::word_mu_h(w, m1, h1);
        check(m0 == m1 && h0 == h1, "word_mu_h");
        M6 g = word_matrix(w);
        check(same_bytes(g, z::imm::word_matrix(w)), "word_matrix of a word of length %zu", w.size());
        E.push_back(g);
        EW.push_back(w);
    }
    note("(%zu random elements: words of length 0..30)", E.size());

    // ---- the backend ----
    section("ON1<5>: identity, mul, inv, normal, eq, same, max_coefficient");
    check(same_bytes(m6id(), O::identity()), "identity");
    for (size_t t = 0; t < E.size(); t++) {
        const M6& a = E[t];
        const M6& b = t % 5 == 0 ? a : E[(size_t)rnd(0, (int64_t)E.size() - 1)];
        O::Elem na = nm(a), nb = nm(b);
        check(same_bytes(m6mul(a, b), O::mul(na, nb)), "mul");
        check(same_bytes(m6inv(a), O::inv(na)), "inv");
        check(same_bytes(a, O::normal(na)), "normal");
        check(m6eq(a, b) == O::eq(na, nb), "eq");
        check(m6eq(a, b) == O::same(na, nb), "same");
        check(m6maxcoef(a) == O::max_coefficient(na), "max_coefficient");
    }
    section("ON1<5>: key, product_key, displacement, key_displacement");
    for (size_t t = 0; t < E.size(); t++) {
        const M6& a = E[t];
        const M6& s = E[(size_t)rnd(0, (int64_t)E.size() - 1)];
        O::Elem na = nm(a), ns = nm(s);
        check(lastcol(a) == O::key(na), "key");
        M6 p = m6mul(a, s);
        O::Key pk = O::product_key(na, ns);
        check(lastcol(p) == pk, "product_key");
        check(same_bits(m6dist(a), O::displacement(na)), "displacement");
        check(same_bits(m6dist(p), O::key_displacement(pk)), "key_displacement");
    }
    section("ON1<5>: pack, unpack, packed_key (Store6's mat, load, col)");
    {
        Store6 st;
        for (size_t t = 0; t < E.size(); t++) {
            const M6& a = E[t];
            if (m6maxcoef(a) > INT32_MAX) continue;
            int i = st.add(a, -1, -1, 0, 0, 0, 0.f);
            int32_t buf[36];
            O::pack(nm(a), buf);
            check(!memcmp(&st.mat[36*(size_t)i], buf, sizeof buf), "pack");
            check(same_bytes(st.load(i), O::unpack(buf)), "unpack");
            i64 c[6];
            st.col(i, c);
            O::Key k = O::packed_key(buf);
            check(!memcmp(c, k.data(), sizeof c), "packed_key");
        }
    }
    section("ON1<5>: to_double, act (toD6, mv6)");
    for (size_t t = 0; t < E.size(); t++) {
        const M6& a = E[t];
        D6 m = toD6(a);
        check(same_bytes(m, O::to_double(nm(a))), "to_double");
        if (t % 2) for (auto& row : m) for (auto& x : row) x = gauss();
        V6 v;
        for (auto& x : v) x = gauss();
        check(same_bytes(mv6(m, v), O::act(m, v)), "act");
    }
    section("ON1<5>: attracting (loxodromic elements), boundary (null and other vectors)");
    {
        std::vector<V6> nulls;
        int nlox = 0;
        for (size_t t = 0; t < E.size() && nlox < 600; t++) {
            const M6& g = E[t];
            if (m6dist(g) <= 0) continue;
            M6 p = m6id();
            for (int k = 0; k < 8; k++) p = m6mul(p, g);       // loxodromic, as chz5 tests it
            if (m6dist(p) < 8*0.5*m6dist(g)) continue;
            nlox++;
            V6 v = attracting6(g);
            check(same_bytes(v, O::attracting(nm(g))), "attracting");
            nulls.push_back(v);
            nulls.push_back(mv6(toD6(E[(size_t)rnd(0, (int64_t)E.size() - 1)]), v));   // moved: null again
        }
        note("(%d loxodromic elements)", nlox);
        for (int t = 0; t < 20000; t++) {                          // random null vectors (x, |x|)
            V6 v;
            double n = 0;
            for (int i = 0; i < 5; i++) { v[i] = gauss(); n += v[i]*v[i]; }
            v[5] = std::sqrt(n);
            if (t % 2) for (auto& x : v) x *= std::exp(10*gauss());
            nulls.push_back(v);
        }
        for (const V6& v : nulls) check(same_bytes(spoint5(v), O::boundary(v)), "boundary of a null vector");
        for (int t = 0; t < 20000; t++) {
            V6 v;
            for (auto& x : v) x = gauss();
            check(same_bytes(spoint5(v), O::boundary(v)), "boundary");
        }
    }

    // ---- hashes ----
    section("splitmix; fingerprint (6-integer keys) against fingerprint6");
    for (int t = 0; t < 200000; t++) {
        uint64_t x = rng();
        check(splitmix(x) == z::splitmix(x), "splitmix");
        M6 g;
        int64_t m = t % 3 == 0 ? 3 : t % 3 == 1 ? (1LL << 30) : INT64_MAX;
        for (auto& row : g.e) for (auto& c : row) c = rnd(-m, m);
        int letter = (int)rnd(-1, 2000);
        check(fingerprint6(g, letter) == z::fingerprint(O::key(nm(g)), letter), "fingerprint");
    }
    for (size_t t = 0; t < E.size(); t++) {
        int letter = (int)rnd(0, 200);
        check(fingerprint6(E[t], letter) == z::fingerprint(O::key(nm(E[t])), letter), "fingerprint of an element");
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
    section("enumerate6 B(3): elements, parents, letters, depths, (mu, h), distances");
    Store6 LB;
    z::imm::Store6 NB;
    long lb = enumerate6(LB, 3.0), nb = z::imm::enumerate6(NB, 3.0);
    check(lb == nb, "the (mu, h) inconsistencies, %ld and %ld", nb, lb);
    compare_stores(LB, NB, "B(3)");
    note("(%zu elements, %ld inconsistencies)", NB.size(), nb);
    section("enumerate6 B(3): path and fword of every element");
    for (size_t i = 0; i < LB.size() && i < NB.size(); i++) {
        std::vector<int> lp = LB.path(i), np = NB.path(i);
        check(lp == np, "path of %zu", i);
        check(fword(lp) == z::imm::fword(np), "fword of %zu", i);
    }
    section("enumerate6 B(1.5) twice into one store (enumerate6 empties it first)");
    {
        Store6 L;
        z::imm::Store6 N;
        enumerate6(L, 2.5);
        z::imm::enumerate6(N, 2.5);
        long l = enumerate6(L, 1.5), n = z::imm::enumerate6(N, 1.5);
        check(l == n, "the inconsistencies");
        compare_stores(L, N, "B(1.5) after B(2.5)");
    }
    section("Store6::lookup and find of products, inverses and other elements");
    for (int t = 0; t < 30000; t++) {
        size_t i = (size_t)rnd(0, (int64_t)LB.size() - 1);
        M6 g = LB.load(i), x;
        int kind = t % 4;
        if (kind == 0) x = m6mul(g, REF[rnd(0, 15)]);
        else if (kind == 1) x = m6inv(g);
        else if (kind == 2) x = m6mul(g, LB.load((size_t)rnd(0, (int64_t)LB.size() - 1)));
        else x = E[(size_t)rnd(0, (int64_t)E.size() - 1)];
        check(LB.lookup(x) == NB.lookup(nm(x)), "lookup");
        i64 c[6];
        for (int r = 0; r < 6; r++) c[r] = x.e[r][5];
        check(LB.find(c) == NB.find(O::key(nm(x))), "find");
    }
    section("fword on random words");
    for (const std::vector<int>& w : EW) check(fword(w) == z::imm::fword(w), "fword");

    // The reader is corrected: the legacy parse_fword skips every token "1", so it drops facet 1 from every word; the
    // new one reads every token as a facet, and only the word that is exactly "1" as the empty word (as fword writes
    // it). So the two agree on the words without facet 1, and differ on the others. tests/unit/test_reader_imm.cc
    // checks that the new reader gives chz5's S_c and t.
    section("parse_fword and word_matrix on the words of the substitution files of legacy/s4");
    {
        const char* files[3] = {"subst5_2.30_4.50_t1.txt", "subst5_2.30_4.50_t2.txt", "subst5_2.30_4.50_t3.txt"};
        int without1 = 0, with1 = 0;
        for (const char* f : files) {
            std::string fn = std::string(ZIPPERS_REPO) + "/legacy/s4/" + f;
            Subst5 D;
            check(load_subst5(fn.c_str(), D), "reading %s", fn.c_str());
            std::vector<std::string> words = D.sword;
            words.push_back(D.tword);
            for (const std::string& w : words) {
                std::vector<int> lw = parse_fword(w), nw = z::imm::parse_fword(w);
                std::vector<int> toks, toks1;              // the tokens, and the tokens other than "1"
                std::string text;                          // the tokens, one blank between them
                {
                    std::istringstream is(w);
                    std::string tok;
                    while (is >> tok) {
                        toks.push_back(atoi(tok.c_str()));
                        if (tok != "1") toks1.push_back(atoi(tok.c_str()));
                        text += (text.empty() ? "" : " ") + tok;
                    }
                }
                if (toks1.size() == toks.size()) {         // no facet 1: the legacy reading, bit for bit
                    without1++;
                    check(lw == nw, "%s: parse_fword(%s)", f, w.c_str());
                    check(same_bytes(word_matrix(lw), z::imm::word_matrix(nw)), "%s: word_matrix(%s)", f, w.c_str());
                } else {                                   // facet 1: the new reader keeps every facet
                    with1++;
                    check(nw == toks, "%s: parse_fword(%s) is the list of its tokens", f, w.c_str());
                    check(lw == toks1, "%s: the legacy parse_fword(%s) drops facet 1, and only it", f, w.c_str());
                    check(same_bytes(word_matrix(toks), z::imm::word_matrix(nw)), "%s: word_matrix(%s)", f, w.c_str());
                }
                check(z::imm::fword(nw) == text, "%s: fword(parse_fword(%s))", f, w.c_str());
            }
        }
        note("(%d words without facet 1, read as the legacy reader did; %d with it, read in full)", without1, with1);
    }
    section("parse_fword: the empty word, round trips with fword, bad letters");
    {
        // "1", with or without blanks, is the empty word; every other "1" is facet 1
        check(z::imm::parse_fword("1").empty() && z::imm::parse_fword(" 1").empty() && z::imm::parse_fword(" 1 ").empty(),
              "\"1\" is the empty word");
        check(z::imm::parse_fword("").empty() && z::imm::parse_fword(" ").empty(), "no token: the empty word");
        check(z::imm::parse_fword("1 1") == std::vector<int>({1, 1}), "\"1 1\"");
        check(z::imm::parse_fword("1 0") == std::vector<int>({1, 0}) && z::imm::parse_fword("0 1") == std::vector<int>({0, 1}),
              "\"1 0\" and \"0 1\"");
        check(z::imm::parse_fword("11") == std::vector<int>({11}) && z::imm::parse_fword("15") == std::vector<int>({15}),
              "\"11\" and \"15\"");
        for (int f = 0; f < 16; f++)
            check(z::imm::parse_fword(z::imm::fword({f})) == (f == 1 ? std::vector<int>() : std::vector<int>({f})),
                  "the one-letter word %d", f);
        // fword and parse_fword are inverse on every word but the one-letter word of facet 1, which fword also writes "1"
        int n1 = 0;
        for (const std::vector<int>& w : EW) {
            bool r1 = w == std::vector<int>({1});
            n1 += r1;
            check(z::imm::parse_fword(z::imm::fword(w)) == (r1 ? std::vector<int>() : w), "parse_fword(fword(w))");
            std::vector<int> w1;                           // the legacy reading: w without facet 1 (but "1" is empty)
            for (int x : w) if (x != 1) w1.push_back(x);
            check(parse_fword(fword(w)) == w1, "the legacy parse_fword(fword(w)) drops facet 1");
        }
        note("(%zu random words, %d of them the one-letter word of facet 1)", EW.size(), n1);
        std::string le, ne;
        for (const char* w : {"0 16", "1 3 -1", "1 x 17"}) {
            int ls = in_child([&] { parse_fword(w); }, le), ns = in_child([&] { z::imm::parse_fword(w); }, ne);
            check(ls == 1 && ns == 1 && le == ne, "parse_fword(\"%s\"): %d \"%s\", legacy %d \"%s\"", w, ns, chomp(ne).c_str(),
                  ls, chomp(le).c_str());
        }
        note("(a bad letter: \"%s\")", chomp(ne).c_str());
    }

    section("find_roots5 at R = 0, 1.2, 1.8");
    for (double R : {0.0, 1.2, 1.8}) {
        World5 W;
        find_roots5(R, W);
        std::vector<z::imm::M6> roots;
        std::vector<double> rootd;
        z::imm::find_roots5(R, roots, rootd);
        check(W.roots.size() == roots.size() && W.rootd.size() == rootd.size(), "find_roots5(%g): %zu roots, legacy %zu",
              R, roots.size(), W.roots.size());
        check(W.roots.size() == roots.size() && !memcmp(W.roots.data(), roots.data(), roots.size()*sizeof(M6)),
              "find_roots5(%g): the roots", R);
        check(W.rootd.size() == rootd.size() && !memcmp(W.rootd.data(), rootd.data(), rootd.size()*sizeof(double)),
              "find_roots5(%g): the distances", R);
        note("(R = %g: %zu roots)", R, roots.size());
    }

    section("guards (in child processes): pack's int32, guard6, mul's odd entries");
    {
        std::string le, ne, se;
        for (int64_t c : {(int64_t)INT32_MAX, (int64_t)INT32_MIN}) {       // at the guard: accepted
            M6 g = m6id();
            g.e[3][1] = c;
            Store6 st;
            st.add(g, -1, -1, 0, 0, 0, 0.f);
            int32_t buf[36];
            O::pack(nm(g), buf);
            check(!memcmp(st.mat.data(), buf, sizeof buf), "pack at the guard (%lld)", (long long)c);
        }
        for (int64_t c : {(int64_t)INT32_MAX + 1, (int64_t)INT32_MIN - 1, (int64_t)1 << 40}) {    // past it
            M6 g = m6id();
            g.e[4][5] = c;
            int ls = in_child([&] { Store6 st; st.add(g, -1, -1, 0, 0, 0, 0.f); }, le);
            int ns = in_child([&] { int32_t buf[36]; O::pack(nm(g), buf); }, ne);
            int ss = in_child([&] { z::imm::Store6 st; st.add(nm(g), -1, -1, 0, z::imm::MuH{0, 0}, 0.f); }, se);
            check(ls == 1 && ns == 1 && ss == 1, "exit status past the guard: %d, %d, legacy %d", ns, ss, ls);
            check(le == ne && le == se, "the message past the guard: \"%s\", legacy \"%s\"", ne.c_str(), le.c_str());
        }
        note("(past the guard: \"%s\")", chomp(le).c_str());
        int ls = in_child([] { guard6(5); }, le), ns = in_child([] { z::imm::guard6(5); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "guard6(5): %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        ls = in_child([] { guard6(4.5, 6e7); }, le);
        ns = in_child([] { z::imm::guard6(4.5, 6e7); }, ne);
        check(ls == 0 && ns == 0 && le.empty() && ne.empty(), "guard6(4.5, 6e7) passes");
        // an odd entry: half the identity, squared. The message names the function, so it differs (on purpose).
        M6 half;
        memset(&half, 0, sizeof half);
        for (int i = 0; i < 6; i++) half.e[i][i] = 1;
        ls = in_child([&] { m6mul(half, half); }, le);
        ns = in_child([&] { O::mul(nm(half), nm(half)); }, ne);
        check(ls == 1 && ns == 1, "mul of odd entries: exit status %d, legacy %d", ns, ls);
        note("(an odd entry: \"%s\", legacy \"%s\")", chomp(ne).c_str(), chomp(le).c_str());
    }

    section("now(): the same clock");
    for (int t = 0; t < 1000; t++) {
        double a = now(), b = z::now(), c = now();
        check(a <= b && b <= c, "now");
    }

    end_section();
    printf("test_on1: %ld comparisons with the legacy code, %ld differ (%.1f s)\n", all_n, all_bad, now() - t_start);
    return all_bad ? 1 : 0;
}
