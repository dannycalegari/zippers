// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_roots_imm: the descent checks of the S^4 root balls (examples/imm_m5.h) and the line of zipper5 --stats. It
// checks, in exact integer arithmetic:
//   - the polytope P5 and its reflections: the 16 facets are the 16 even sign vectors; o = e_6 is strictly inside P5
//     (<o, a_f> = -1 < 0 for a_f = (s_f, 1)); each of the 16 inequalities is a facet (a point of H^5 where it alone is
//     tight); every two facets are orthogonal or tangent at infinity (the counts pinned); REF[f] is the reflection in
//     a_f^perp, twice its matrix, an element of O+(5,1); and (G REF[f])_66 - G_66 against (G a_f)_6 / 2, with (G a_f)_6
//     never 0, on elements of W;
//   - the checks on W n B(2.6) (2.1, the largest radius of the stored runs and the film, plus the margin 0.5): every
//     g != 1 that enumerate6 finds has a facet f with (G REF[f])_66 < G_66, by imm::descents and by (G a_f)_6 < 0
//     counted here again, and 1 has none; the parent of every element in the search tree is strictly nearer, and two
//     descending chains (by the first and by the last descending facet) reach 1 in as many steps as its depth; the
//     store is closed under the facet steps inside the ball; the computed distances increase with G_66; the (mu, h)
//     labels are consistent; and the number of elements with each number of descents, pinned;
//   - certify_roots5 (the checks of zipper5 --stats) at R = 0, 0.5, 1, 1.32, 1.8, 2.1: the checks pass, its roots and
//     distances are those of find_roots5, bit for bit, and its line, word for word (the counts in it pinned); and at
//     four radii within 1e-7 of acosh(2) and acosh(4), the second line, and the roots that the single-precision test of
//     find_roots5 keeps against the exact ball G_66 <= 2 cosh R;
//   - that the checks can fail: a symmetry of P5 (it fixes o) put into a store, second, in the middle or last, is
//     reported as an element without descent (and its neighbours as key clashes); a store with an element left out is
//     reported as not closed; the line then says FAILED, and why;
//   - enumerate6 against an INDEPENDENT ENUMERATION of W n B(2.1): every lattice point X of the hyperboloid (2X
//     integral, all entries of one parity, X_6 <= 2 cosh 2.1), walked down to P5 by the reflections of the facets it
//     breaks; the points that reach o give the elements of W (as products of reflections computed here); the two sets
//     of matrices must be the same. It uses none of the search's code;
//   - the counts |W n B(R)| and |Gamma n B(R)| for a few R (as chz5 ball 3 recorded them, tests/reference/s4_chz5_ball3),
//     and the number of elements of W at each G_66.
// Light: about a second. Uses src/ only.

#include "examples/imm_m5.h"
#include "geometry/on1/on1.h"

#include "render_test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace z = zippers;
typedef z::imm::O51 B;
typedef B::Elem E;
typedef std::array<int64_t, 6> V6;
using rtest::check;
using rtest::note;
using rtest::section;

// ---- exact arithmetic, written here again -----------------------------------------------------------------------------

// the form x1 y1 + ... + x5 y5 - x6 y6
static int64_t form(const V6& x, const V6& y)
{
    int64_t s = 0;
    for (int i = 0; i < 5; i++) s += x[i]*y[i];
    return s - x[5]*y[5];
}
static V6 column(const E& g, int j) { V6 v; for (int i = 0; i < 6; i++) v[i] = g.e[i][j]; return v; }
static V6 act6(const E& g, const V6& x)
{
    V6 y;
    for (int i = 0; i < 6; i++) { int64_t s = 0; for (int k = 0; k < 6; k++) s += g.e[i][k]*x[k]; y[i] = s; }
    return y;
}
// the reflection in a^perp, <a, a> = 4, on a vector X: X - (<X, a>/2) a (exact when <X, a> is even)
static V6 reflect(const V6& a, const V6& X)
{
    int64_t c = form(X, a);
    V6 Y;
    for (int i = 0; i < 6; i++) Y[i] = X[i] - (c/2)*a[i];
    return Y;
}

// the 16 even sign vectors, made here; a_f = (s_f, 1) from the code's FS
static std::vector<V6> facet_normals()
{
    std::vector<V6> a(16);
    for (int f = 0; f < 16; f++) { for (int k = 0; k < 5; k++) a[f][k] = z::imm::FS[f][k]; a[f][5] = 1; }
    return a;
}

// all X in Z^6 with entries of one parity, X_6 in [2, N], X_1^2 + ... + X_5^2 = X_6^2 - 4: the lattice points of the
// hyperboloid (as 2x) at distance <= acosh(N/2) from o
static std::vector<V6> lattice_points(int64_t N)
{
    std::vector<V6> out;
    for (int64_t x6 = 2; x6 <= N; x6++) {
        int64_t S = x6*x6 - 4, p = x6 & 1;
        V6 X{};
        X[5] = x6;
        std::function<void(int, int64_t)> rec = [&](int i, int64_t rest) {
            if (i == 5) { if (rest == 0) out.push_back(X); return; }
            for (int64_t v = -x6; v <= x6; v++) {
                if (((v % 2) + 2) % 2 != p || v*v > rest) continue;
                X[i] = v;
                rec(i + 1, rest - v*v);
            }
        };
        rec(0, S);
    }
    return out;
}

// walk X down to P5: while some facet inequality is broken, <X, a_f> > 0, reflect in the first such facet. Returns
// the end point and the facets used, in order
static V6 walk_down(const std::vector<V6>& a, V6 X, std::vector<int>& word)
{
    word.clear();
    for (;;) {
        int f = -1;
        for (int k = 0; k < 16 && f < 0; k++) if (form(X, a[k]) > 0) f = k;
        if (f < 0) return X;
        X = reflect(a[f], X);
        word.push_back(f);
    }
}

// twice the matrix of r_{w_0} r_{w_1} ... r_{w_k-1}, by reflecting the columns of 2I (independent of O51::mul and REF)
static E product(const std::vector<V6>& a, const std::vector<int>& w)
{
    E g;
    for (int j = 0; j < 6; j++) {
        V6 X{};
        X[j] = 2;
        for (int k = (int)w.size() - 1; k >= 0; k--) X = reflect(a[w[k]], X);
        for (int i = 0; i < 6; i++) g.e[i][j] = X[i];
    }
    return g;
}

static std::array<int64_t, 36> flat(const E& g)
{
    std::array<int64_t, 36> f;
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) f[6*i + j] = g.e[i][j];
    return f;
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::imm::setup();
    printf("test_roots_imm: the descent checks of the S^4 root balls (the 16 facet reflections)\n");
    const std::vector<V6> a = facet_normals();
    const V6 o = {0, 0, 0, 0, 0, 1};

    // ---- the polytope and its reflections ----
    section("P5: o strictly inside, 16 facets, right angles, the reflections (exact)");
    {
        // the 16 even sign vectors, made here, are the code's 16 facets
        std::vector<std::array<int, 5>> even, code;
        for (int m = 0; m < 32; m++) {
            std::array<int, 5> s;
            int neg = 0;
            for (int k = 0; k < 5; k++) { s[k] = (m >> k) & 1 ? -1 : 1; neg += s[k] < 0; }
            if (neg % 2 == 0) even.push_back(s);
        }
        for (int f = 0; f < 16; f++) { std::array<int, 5> s; for (int k = 0; k < 5; k++) s[k] = z::imm::FS[f][k]; code.push_back(s); }
        std::sort(even.begin(), even.end());
        std::sort(code.begin(), code.end());
        check(even.size() == 16 && code == even, "the facets are the 16 even sign vectors");
        int ortho = 0, parallel = 0, other = 0;
        for (int f = 0; f < 16; f++) {
            check(form(a[f], a[f]) == 4, "facet %d: <a, a> = %lld, not 4", f, (long long)form(a[f], a[f]));
            check(form(o, a[f]) == -1, "facet %d: <o, a> = %lld: o is not strictly inside", f, (long long)form(o, a[f]));
            // the point (s_f, 5) of H^5 (as a timelike vector): on facet f, strictly inside every other
            V6 X;
            for (int k = 0; k < 5; k++) X[k] = a[f][k];
            X[5] = 5;
            bool alone = form(X, X) < 0 && X[5] > 0 && form(X, a[f]) == 0;
            for (int g = 0; g < 16; g++) if (g != f) alone = alone && form(X, a[g]) < 0;
            check(alone, "facet %d: (s_f, 5) is not a point of H^5 where inequality f alone is tight", f);
            for (int g = f + 1; g < 16; g++) {
                int64_t q = form(a[f], a[g]);
                if (q == 0) ortho++; else if (q == -4) parallel++; else other++;
            }
            // REF[f] = 2I - a a^T J, the reflection in a^perp, in O+(5,1)
            const E& R = z::imm::REF[f];
            bool formula = true;
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 6; j++)
                    formula = formula && R.e[i][j] == (i == j ? 2 : 0) - a[f][i]*a[f][j]*(j == 5 ? -1 : 1);
            check(formula, "REF[%d] is not 2I - a a^T J", f);
            V6 Ra = act6(R, a[f]);
            bool neg = true;
            for (int i = 0; i < 6; i++) neg = neg && Ra[i] == -2*a[f][i];
            check(neg, "REF[%d] a != -2 a", f);
            bool fixes = true;
            for (int k = 0; k < 6; k++) {                 // 4 e_k - <e_k, a> a spans a^perp as k runs over 0..5
                V6 e{};
                e[k] = 1;
                V6 x;
                int64_t c = form(e, a[f]);
                for (int i = 0; i < 6; i++) x[i] = 4*e[i] - c*a[f][i];
                V6 Rx = act6(R, x);
                for (int i = 0; i < 6; i++) fixes = fixes && Rx[i] == 2*x[i];
            }
            check(fixes, "REF[%d] does not fix a^perp", f);
            bool iso = true;
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 6; j++) iso = iso && form(column(R, i), column(R, j)) == (i == j ? (i == 5 ? -4 : 4) : 0);
            check(iso && R.e[5][5] == 3, "REF[%d]: not twice an element of O+(5,1)", f);
        }
        check(ortho == 80 && parallel == 40 && other == 0, "pairs of facets: %d orthogonal, %d parallel, %d other",
              ortho, parallel, other);
        note("16 facets, o inside each with <o, a_f> = -1; %d orthogonal pairs, %d tangent at infinity, %d other", ortho,
             parallel, other);
        // the formula of the descent on elements of W (random words): (G REF[f])_66 - G_66 = (G a_f)_6 / 2, never 0
        long nform = 0;
        for (int trial = 0; trial < 400; trial++) {
            std::vector<int> w;
            int len = (int)rtest::rnd(0, 12);
            for (int k = 0; k < len; k++) w.push_back((int)rtest::rnd(0, 15));
            E G = z::imm::word_matrix(w);
            for (int f = 0; f < 16; f++) {
                int64_t Ga6 = act6(G, a[f])[5];
                int64_t step = B::mul(G, z::imm::REF[f]).e[5][5] - G.e[5][5];
                check(Ga6 != 0 && Ga6 % 2 == 0 && step == Ga6/2, "word of length %d, facet %d: (G a)_6 = %lld, step %lld",
                      len, f, (long long)Ga6, (long long)step);
                nform++;
            }
        }
        note("the formula (G REF[f])_66 - G_66 = (G a_f)_6 / 2 != 0 on %ld (element, facet) pairs", nform);
    }

    // ---- the descent on W n B(2.6) ----
    section("the descent, exactly, on W n B(2.6) (2.1 plus the margin)");
    z::imm::Store6 B26;
    {
        double t0 = rtest::clock_seconds();
        long bad = z::imm::enumerate6(B26, 2.6);
        double t1 = rtest::clock_seconds();
        size_t without = 0, disagree = 0, parent = 0, chains = 0;
        std::map<int, size_t> hist;                      // the number of descents -> elements
        for (size_t i = 0; i < B26.size(); i++) {
            E g = B26.load(i);
            int lib = z::imm::descents(g), here = 0;
            for (int f = 0; f < 16; f++) here += act6(g, a[f])[5] < 0;
            if (lib != here) disagree++;
            hist[here]++;
            if (i == 0) {
                check(B::eq(g, B::identity()) && here == 0, "the first element is 1, with no descent");
                continue;
            }
            if (here == 0) without++;
            if (B26.load(B26.parent[i]).e[5][5] >= g.e[5][5]) parent++;
            // two descending chains to 1, by the first and by the last descending facet: each must have as many steps
            // as the depth of g in the search tree
            int len[2] = {0, 0};
            for (int way = 0; way < 2; way++) {
                E x = g;
                while (!B::eq(x, B::identity()) && len[way] < 64) {
                    int f = -1;
                    for (int k = 0; k < 16; k++) {
                        int kk = way ? 15 - k : k;
                        if (f < 0 && act6(x, a[kk])[5] < 0) f = kk;
                    }
                    if (f < 0) break;
                    x = B::mul(x, z::imm::REF[f]);
                    len[way]++;
                }
                if (!B::eq(x, B::identity())) len[way] = -1;
            }
            if (len[0] != B26.depth[i] || len[1] != B26.depth[i]) chains++;
        }
        double t2 = rtest::clock_seconds();
        check(bad == 0, "(mu, h) inconsistencies: %ld", bad);
        check(without == 0, "%zu elements without a descent", without);
        check(disagree == 0, "%zu elements where imm::descents and the formula disagree", disagree);
        check(parent == 0, "%zu elements whose parent is not strictly nearer", parent);
        check(chains == 0, "%zu elements whose descending chains are not as long as their depth", chains);
        z::imm::RootCertificate c;
        z::imm::certify_store5(B26, 2.6, 2.1, c);
        check(c.passes() && c.no_descent == 0 && c.not_closed == 0 && c.parent_not_nearer == 0 && c.order_breaks == 0 &&
              c.identity_first, "certify_store5 on B(2.6): the checks fail");
        std::string h;
        for (auto& kv : hist) h += " " + std::to_string(kv.first) + ":" + std::to_string(kv.second);
        // pinned: a lost or extra element, or a changed descent, shows here
        check(B26.size() == 12633 && c.max_g66 == 13, "B(2.6): %zu elements, largest G_66 %lld (pinned: 12633, 13)",
              B26.size(), (long long)c.max_g66);
        check(h == " 0:1 1:816 2:3040 3:6080 4:2520 5:176", "descents per element:%s", h.c_str());
        note("%zu elements of W in B(2.6), largest G_66 = %lld; enumerate6 %.3f s, the exact checks %.3f s", B26.size(),
             (long long)c.max_g66, t1 - t0, t2 - t1);
        note("descents per element (number: elements):%s", h.c_str());
    }

    // ---- certify_roots5 against find_roots5, and the line ----
    section("certify_roots5: the roots of find_roots5, the checks pass, and the line of zipper5 --stats");
    {
        const double Rs[6] = {0, 0.5, 1.0, 1.32, 1.8, 2.1};
        // pinned: the roots and the elements searched (W n B(R + 0.5)), as the line says them
        const char* const LINE[6] = {
            "roots: 1 (R <= 0: the identity alone, no search)",
            "roots: 1 elements of Gamma in B(0.5), of 17 elements of W searched in B(1); "
            "checked: closed under the 16 facet reflections inside that ball, and every element "
            "descends to 1, both verified exactly",
            "roots: 1 elements of Gamma in B(1), of 97 elements of W searched in B(1.5); "
            "checked: closed under the 16 facet reflections inside that ball, and every element "
            "descends to 1, both verified exactly",
            "roots: 9 elements of Gamma in B(1.32), of 457 elements of W searched in B(1.82); "
            "checked: closed under the 16 facet reflections inside that ball, and every element "
            "descends to 1, both verified exactly",
            "roots: 41 elements of Gamma in B(1.8), of 4073 elements of W searched in B(2.3); "
            "checked: closed under the 16 facet reflections inside that ball, and every element "
            "descends to 1, both verified exactly",
            "roots: 137 elements of Gamma in B(2.1), of 12633 elements of W searched in B(2.6); "
            "checked: closed under the 16 facet reflections inside that ball, and every element "
            "descends to 1, both verified exactly"};
        for (int r = 0; r < 6; r++) {
            double R = Rs[r];
            z::imm::RootCertificate c = z::imm::certify_roots5(R);
            std::vector<E> roots;
            std::vector<double> rootd;
            z::imm::find_roots5(R, roots, rootd);
            bool same = c.rootv.size() == roots.size() && c.rootd.size() == rootd.size();
            for (size_t i = 0; same && i < roots.size(); i++) same = B::eq(c.rootv[i], roots[i]);
            check(same && rtest::same_vector_bytes(c.rootd, rootd), "R = %g: the roots differ from find_roots5's", R);
            check(c.passes() && c.label_inconsistencies == 0 && c.parent_not_nearer == 0, "R = %g: the checks fail", R);
            std::string line = z::imm::root_certificate_line(c);
            check(line == LINE[r], "R = %g: the line is \"%s\"", R, line.c_str());
            check(z::imm::root_margin_line(c).empty(), "R = %g: a margin line", R);
            note("R = %g: %s", R, line.c_str());
            if (R > 0) note("        R is %.3g from the nearest distance of an element of W, %.6f", c.margin, c.nearest);
        }
    }

    // ---- R within single-precision rounding of a distance ----
    section("R near the distance of elements of W: the second line of --stats, and what the float test does");
    {
        // acosh(2) = 1.3169578969... rounds UP in single precision (1.3169579506), acosh(4) = 2.0634370689... DOWN
        // (2.0634369850): between the two values the float test of find_roots5 and the exact ball disagree
        struct Case { double R; size_t roots, exact; const char* why; };
        const Case cases[4] = {{1.31695785, 1, 1, "below acosh(2) and its float: the 8 elements at acosh(2) are out"},
                               {1.31695792, 1, 9, "above acosh(2), below its float: in the exact ball, not roots"},
                               {2.06343700, 137, 41, "below acosh(4), above its float: out of the exact ball, roots"},
                               {2.06343710, 137, 137, "above acosh(4) and its float: in"}};
        for (const Case& k : cases) {
            z::imm::RootCertificate c = z::imm::certify_roots5(k.R);
            // the exact ball of Gamma: G_66 <= 2 cosh R, an integer bound (2 cosh R is here at least 8e-8 from an
            // integer, far beyond the error of cosh in double)
            int64_t N = (int64_t)std::floor(2*std::cosh(k.R));
            size_t exact = 0;
            z::imm::Store6 S;
            z::imm::enumerate6(S, k.R + 0.5);
            for (size_t i = 0; i < S.size(); i++) exact += S.label[i].mu == 0 && S.load(i).e[5][5] <= N;
            check(c.passes() && c.roots == k.roots && exact == k.exact, "R = %.8f: %zu roots, %zu in the exact ball "
                  "(pinned %zu, %zu)", k.R, c.roots, exact, k.roots, k.exact);
            std::string m = z::imm::root_margin_line(c);
            check(!m.empty(), "R = %.8f: no margin line", k.R);
            note("R = %.8f (%s): %zu roots, %zu in the exact ball", k.R, k.why, c.roots, exact);
        }
        note("%s", z::imm::root_margin_line(z::imm::certify_roots5(1.31695792)).c_str());
    }

    // ---- each check alone decides (without this, a passes() that ignored key_clashes would pass every test,
    // because the only key clashes made below come with a missing descent) ----
    section("each failure alone makes the checks fail");
    {
        z::imm::RootCertificate ok;
        check(ok.passes(), "a struct with no failure does not pass");
        for (int k = 0; k < 6; k++) {
            z::imm::RootCertificate c;
            const char* what[6] = {"identity_first", "identity_descents", "no_descent", "not_closed", "key_clashes",
                                   "order_breaks"};
            if (k == 0) c.identity_first = false;
            if (k == 1) c.identity_descents = 1;
            if (k == 2) c.no_descent = 1;
            if (k == 3) c.not_closed = 1;
            if (k == 4) c.key_clashes = 1;
            if (k == 5) c.order_breaks = 1;
            check(!c.passes(), "%s alone leaves the checks passing", what[k]);
        }
    }

    // ---- the checks fail when they should ----
    section("the checks fail when they should: a symmetry of P5 (no descent), an element left out (not closed)");
    {
        z::imm::Store6 S;
        z::imm::enumerate6(S, 1.5);
        z::imm::RootCertificate c0;
        z::imm::certify_store5(S, 1.5, 1.0, c0);
        check(c0.passes(), "B(1.5): the checks fail");
        // sigma: swap x_1 and x_2. It preserves the form and the 16 even sign vectors, so it maps P5 to itself and fixes
        // o; it is not 1, and no facet brings sigma o nearer.
        // It is put into a copy of the store at the place k (the parents moved with it): second, in the middle, last
        E sigma = B::identity();
        sigma.e[0][0] = sigma.e[1][1] = 0;
        sigma.e[0][1] = sigma.e[1][0] = 2;
        check(z::imm::descents(sigma) == 0 && !B::eq(sigma, B::identity()), "sigma has a descent, or is 1");
        for (size_t k : {(size_t)1, S.size()/2, S.size()}) {
            z::imm::Store6 T;
            for (size_t i = 0; i <= S.size(); i++) {
                if (i == k) T.add(sigma, 0, -1, 1, z::imm::MuH{0, 0}, 0.f);
                if (i == S.size()) break;
                int p = S.parent[i];
                T.add(S.load(i), p < 0 ? -1 : p + (p >= (int)k ? 1 : 0), S.letter[i], S.depth[i], S.label[i], S.dist[i]);
            }
            z::imm::RootCertificate c;
            z::imm::certify_store5(T, 1.5, 1.0, c);
            // (its 16 neighbours sigma r_f have the keys of the r_{f'}: the key clashes are reported too)
            check(T.size() == S.size() + 1 && !c.passes() && c.no_descent == 1 && c.parent_not_nearer == 1 &&
                  c.not_closed == 0 && c.key_clashes == 16, "sigma at %zu of %zu is not reported: %zu without descent, "
                  "%zu parents not nearer, %zu key clashes", k, T.size(), c.no_descent, c.parent_not_nearer, c.key_clashes);
            std::string line = z::imm::root_certificate_line(c);
            check(line.find("FAILED: 1 elements without a nearer facet neighbour") != std::string::npos,
                  "sigma at %zu: the line does not say so: %s", k, line.c_str());
            if (k == S.size()) note("sigma last: %s", line.c_str());
        }
        // a store with one element left out (the parents after it moved down; its children then hang from 1) is not
        // closed: the element is a facet neighbour, within the ball, of its parent
        for (size_t k : {(size_t)1, S.size()/2, S.size() - 1}) {
            z::imm::Store6 T;
            for (size_t i = 0; i < S.size(); i++) {
                if (i == k) continue;
                int p = S.parent[i];
                p = p < 0 ? -1 : p == (int)k ? 0 : p - (p > (int)k ? 1 : 0);
                T.add(S.load(i), p, S.letter[i], S.depth[i], S.label[i], S.dist[i]);
            }
            z::imm::RootCertificate c;
            z::imm::certify_store5(T, 1.5, 1.0, c);
            check(!c.passes() && c.not_closed > 0, "element %zu left out: not reported (%zu not closed)", k, c.not_closed);
            std::string line = z::imm::root_certificate_line(c);
            check(line.find("facet neighbours in B(1.5) not found") != std::string::npos, "element %zu left out: the line "
                  "does not say so: %s", k, line.c_str());
            if (k == S.size() - 1) note("the last element left out: %s", line.c_str());
        }
    }

    // ---- an independent enumeration ----
    section("enumerate6 against an independent enumeration of W n B(2.6): lattice points walked down to P5");
    {
        double t0 = rtest::clock_seconds();
        const int64_t N = (int64_t)std::floor(2*std::cosh(2.6));      // 13: d <= 2.6 iff G_66 <= 13
        std::vector<V6> pts = lattice_points(N);
        std::map<V6, std::array<int64_t, 36>> mine;     // key (g o, doubled) -> twice g
        std::map<V6, size_t> ends;                      // the other end points in P5 -> how many
        std::vector<int> w;
        for (const V6& X : pts) {
            V6 Y = walk_down(a, X, w);
            if (Y == V6{0, 0, 0, 0, 0, 2}) {
                E g = product(a, w);
                check(column(g, 5) == X, "the product of the walk does not take o to the lattice point");
                mine[X] = flat(g);
            } else ends[Y]++;
        }
        double t1 = rtest::clock_seconds();
        const z::imm::Store6& S = B26;
        size_t matched = 0;
        for (size_t i = 0; i < S.size(); i++) {
            E g = S.load(i);
            auto it = mine.find(column(g, 5));
            if (it != mine.end() && it->second == flat(g)) matched++;
        }
        check(S.size() == mine.size() && matched == S.size(), "enumerate6 found %zu elements, the lattice points %zu, "
              "%zu the same", S.size(), mine.size(), matched);
        // pinned: the other lattice points end at the 16 finite vertices (u, 3)/2 of P5, u odd, equally often
        bool vertices = ends.size() == 16;
        for (auto& kv : ends) {
            int odd = 0;
            for (int k = 0; k < 5; k++) { vertices = vertices && (kv.first[k] == 1 || kv.first[k] == -1); odd += kv.first[k] < 0; }
            vertices = vertices && kv.first[5] == 3 && odd % 2 == 1 && kv.second == ends.begin()->second;
        }
        check(vertices && pts.size() == 12633 + 16*ends.begin()->second, "the other end points are not the 16 finite vertices");
        size_t other = 0;
        std::string e;
        for (auto& kv : ends) {
            other += kv.second;
            if (e.size() < 400) {
                char buf[96];
                snprintf(buf, sizeof buf, " (%lld,%lld,%lld,%lld,%lld,%lld):%zu", (long long)kv.first[0], (long long)kv.first[1],
                         (long long)kv.first[2], (long long)kv.first[3], (long long)kv.first[4], (long long)kv.first[5], kv.second);
                e += buf;
            }
        }
        note("%zu lattice points with X_6 <= %lld: %zu reach o (enumerate6: %zu, the same matrices: %zu), %zu end at %zu "
             "other points of P5 (%.3f s)", pts.size(), (long long)N, mine.size(), S.size(), matched, other, ends.size(),
             t1 - t0);
        note("the other end points (2x), all the finite vertices of P5, each %zu times:%s", ends.begin()->second, e.c_str());
    }

    // ---- the counts ----
    section("the counts |W n B(R)| and |Gamma n B(R)|, and the elements of W at each G_66");
    {
        std::map<int64_t, size_t> byg, byg_gamma;
        for (size_t i = 0; i < B26.size(); i++) {
            byg[B26.load(i).e[5][5]]++;
            if (B26.label[i].mu == 0) byg_gamma[B26.load(i).e[5][5]]++;
        }
        std::string h;
        for (auto& kv : byg) h += " " + std::to_string(kv.first) + ":" + std::to_string(kv.second) + "/" + std::to_string(byg_gamma[kv.first]);
        check(h == " 2:1/1 3:16/0 4:80/8 5:160/0 6:200/32 7:496/0 8:960/96 9:960/0 10:1200/144 11:2000/0 12:3040/336 13:3520/0",
              "the elements at each G_66:%s", h.c_str());
        note("G_66: elements of W / of Gamma:%s", h.c_str());
        // pinned; R = 1, 1.5, 2, 2.5 as chz5 ball 3 recorded them (17/1, 97/9, 953/41, 9113/617)
        struct Count { double R; size_t w, g; };
        const Count counts[9] = {{0.5, 1, 1}, {1.0, 17, 1}, {1.32, 97, 9}, {1.5, 97, 9}, {1.8, 457, 41}, {2.0, 953, 41},
                                 {2.1, 1913, 137}, {2.5, 9113, 617}, {2.6, 12633, 617}};
        for (const Count& k : counts) {
            z::imm::Store6 S;
            z::imm::enumerate6(S, k.R);
            size_t nw = S.size(), ng = 0;
            for (size_t i = 0; i < S.size(); i++) ng += S.label[i].mu == 0;
            check(nw == k.w && ng == k.g, "R = %g: |W n B(R)| = %zu, |Gamma n B(R)| = %zu (pinned %zu, %zu)", k.R, nw, ng,
                  k.w, k.g);
            note("R = %g: |W n B(R)| = %zu, |Gamma n B(R)| = %zu", k.R, nw, ng);
        }
    }

    rtest::end_section();
    printf("test_roots_imm: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
