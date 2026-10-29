// SPDX-License-Identifier: GPL-3.0-or-later
//
// imm_m5_search.h: the search policy of IMM's fibered 5-manifold (examples/imm_m5.h) for the block-filling search of
// search/kernel_search.h. namespace zippers::imm.
//
// Gamma = ker mu in the reflection group W of P5, phi = h/2, K = ker phi. What this example does at each step of the
// search (kernel_search.h lists the steps):
//   1. the small ball: the tiles of B(rho), rho = max(rho, c), with the count of (mu, h)-inconsistencies; the search
//      stops if it is not 0;
//   2. t: the shortest LOXODROMIC element of Gamma with h = 2 (an element g counts as loxodromic when
//      d(o, g^8 o) >= 4 d(o, g o));
//   3. S_c: mu = 0, h = 0, 0 < d <= c, sorted by d with std::stable_sort; each element with its word in the facet
//      reflections and its d; S_c must be closed under inverses;
//   4. the K-search: no labels (they are (0, 0)), the guard guard6(R, 6e7), cap 2,500,000;
//   5. the roofs: looked up in the K-store, or else found by the descent (at most 200,000 elements, none with d > 18),
//      and each checked exactly; the descents are counted after each pass over the roofs;
//   6. membership: g t^-phi(g) for the 192 Schreier generators g of Gamma (c_mu r_f c_{mu'}^-1);
//   7. the statistics of the roofs: their lengths, and the growth rate by 200 steps of another power iteration;
//   9. the file subst5_C_R_tTPHI.txt: rho in its header, the t-word, S_c with d, the roofs up and down;
//  10. the convergence: points of S^4 (the last column), stopping once d(o, chain) > 30.
//
// Moved from kernelmode (with its lambdas pick_t and pt), coset_rep, invword, Gen6, stats, dmul and convergence of
// apps/s4/chz5.cc, which were those of legacy/s4/chz5.cc under new names: the printed lines, the sort, the
// formats and every expression are the legacy ones, so that chz5 prints and writes exactly what the legacy program
// did.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/timing.h"
#include "examples/imm_m5.h"
#include "geometry/on1/on1.h"
#include "search/kernel_search.h"

namespace zippers {
namespace imm {

// the coset representatives of Gamma = ker mu: c_mu = product of r_{2m} over the moves m in mu
inline std::vector<int> coset_rep(int mu)
{
    std::vector<int> w;
    for (int m = 0; m < 4; m++) if (mu & (1 << m)) w.push_back(2*m);
    return w;
}

inline std::vector<int> invword(const std::vector<int>& w) { return std::vector<int>(w.rbegin(), w.rend()); }

// an element of S_c: its matrix, its word in the facet reflections, and d(o, s o)
struct Gen6 { M6 m; std::vector<int> word; double d; };

struct SearchPolicy {
    typedef O51 B;
    typedef MuH Label;                // in the K-store: (0, 0)
    typedef Gen6 Gen;

    static constexpr size_t kstore_cap = 2500000;
    static constexpr bool descent = true;                  // roofs outside the K-store are found by the descent
    static constexpr size_t descent_budget = 200000;
    static constexpr double descent_radius = 18;
    static constexpr bool check_roofs = true;              // and every roof is checked exactly
    static constexpr double chain_stop = 30;

    double rho;
    Store6 ball;

    explicit SearchPolicy(double rho_) : rho(rho_) {}

    // 1. the small ball
    bool small_ball(double c)
    {
        double t0 = now();
        rho = std::max(rho, c);
        guard6(rho);
        long bad = enumerate6(ball, rho);
        printf("ball: %zu tiles with d <= %.2f (%.2fs), (mu, h) inconsistencies %ld\n", ball.size(), rho, now() - t0, bad);
        if (bad) return false;
        return true;
    }

    // 2. t: the shortest element of Gamma with phi = ph (h = 2 ph) that is loxodromic
    int pick_t(int ph) const
    {
        int best = -1;
        for (size_t i = 0; i < ball.size(); i++) {
            if (ball.label[i].mu != 0 || ball.label[i].h != 2*ph) continue;
            M6 g = ball.load(i), p = O51::identity();
            for (int k = 0; k < 8; k++) p = O51::mul(p, g);          // loxodromic: d(o, g^8 o) grows linearly
            if (O51::displacement(p) < 8*0.5*ball.dist[i]) continue;
            if (best < 0 || ball.dist[i] < ball.dist[best]) best = (int)i;
        }
        return best;
    }
    void print_t(int ti, bool steep) const
    {
        if (!steep)
            printf("t = [%s]   d(o, t o) = %.4f   h(t) = %d\n", fword(ball.path(ti)).c_str(), ball.dist[ti], ball.label[ti].h);
        else
            printf("steep t = [%s]   d(o, t o) = %.4f   h(t) = %d\n", fword(ball.path(ti)).c_str(), ball.dist[ti], ball.label[ti].h);
    }
    void print_no_t(int ph) const { printf("no loxodromic element with phi = %d in the ball\n", ph); }

    // the options --tlist and --tcand (kernel_search.h): the candidates for t with phi = ph, the elements that pick_t allows
    // (Gamma, h = 2 ph, loxodromic by its test), sorted by d(o, t o) (the float that pick_t compares), ties in the
    // ball's order: candidate 0 is pick_t(ph)'s
    std::vector<int> t_candidates(int ph) const
    {
        std::vector<int> c;
        for (size_t i = 0; i < ball.size(); i++) {
            if (ball.label[i].mu != 0 || ball.label[i].h != 2*ph) continue;
            M6 g = ball.load(i), p = O51::identity();
            for (int k = 0; k < 8; k++) p = O51::mul(p, g);
            if (O51::displacement(p) < 8*0.5*ball.dist[i]) continue;
            c.push_back((int)i);
        }
        std::stable_sort(c.begin(), c.end(), [&](int a, int b) { return ball.dist[a] < ball.dist[b]; });
        return c;
    }
    // whether the elements a and b of the ball have the same d(o, g o) = acosh g_66, exactly: the same entry 2 g_66
    bool same_length(int a, int b) const { return ball.load(a).e[5][5] == ball.load(b).e[5][5]; }
    void print_candidate(int k, int i) const
    {
        printf("  %4d  d(o, t o) = %.6f  h(t) = %d  [%s]\n", k, ball.dist[i], ball.label[i].h, fword(ball.path(i)).c_str());
    }

    // 3. S_c, closed under inverses
    std::vector<int> kernel_indices(double c) const
    {
        std::vector<int> Sidx;
        for (size_t i = 1; i < ball.size(); i++) if (ball.label[i].mu == 0 && ball.label[i].h == 0 && ball.dist[i] <= c) Sidx.push_back(i);
        std::stable_sort(Sidx.begin(), Sidx.end(), [&](int a, int b) { return ball.dist[a] < ball.dist[b]; });
        return Sidx;
    }
    Gen generator(int i) const { return {ball.load(i), ball.path(i), ball.dist[i]}; }
    bool check_S(const std::vector<Gen>& S) const
    {
        for (auto& s : S) {
            M6 x = O51::inv(s.m);
            bool f = false;
            for (auto& u : S) if (O51::eq(u.m, x)) { f = true; break; }
            if (!f) { printf("S_c is not closed under inverses\n"); return false; }
        }
        return true;
    }

    // 4. the K-search
    void kernel_guard(double R) const { guard6(R, 6e7); }
    Label kstore_root_label() const { return MuH{0, 0}; }
    Label kstore_label(const Label&, const Gen&) const { return MuH{0, 0}; }
    void kstore_revisit(const Label&, const Label&) {}
    void print_kstore(size_t n, double R, double seconds, int maxdep, bool capped) const
    {
        printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d%s\n",
               n, R, seconds, maxdep, capped ? "  [CAPPED]" : "");
    }

    // 5. the descents (counted from the start of the search)
    void print_descents(long descents, long nodes) const
    {
        printf("  (%ld descents from outside the K-store, %ld nodes)\n", descents, nodes);
    }

    // 6. membership: g t^-phi(g) for the Schreier generators g of Gamma
    template <class FindWord>
    KernelMembership membership(const M6& t, const M6& tinv, FindWord& find_word) const
    {
        int nmem = 0, ngen = 0;
        for (int mu = 0; mu < 16; mu++)
            for (int f = 0; f < 16; f++) {
                std::vector<int> w = coset_rep(mu);
                w.push_back(f);
                for (int x : invword(coset_rep(mu ^ (1 << MOVE[f])))) w.push_back(x);
                int m, hh;
                word_mu_h(w, m, hh);
                if (m != 0 || hh % 2) { printf("a Schreier generator is not in Gamma, or has odd h\n"); return KernelMembership::stop; }
                M6 g = word_matrix(w);
                if (O51::eq(g, O51::identity())) continue;
                ngen++;
                int ph = hh/2;
                M6 k = g;
                for (int e = 0; e < std::abs(ph); e++) k = O51::mul(k, ph > 0 ? tinv : t);
                std::vector<int> w2;
                if (find_word(k, w2)) nmem++;
            }
        printf("Schreier generators g of Gamma with g t^-phi(g) in <S_c>: %d / %d\n", nmem, ngen);
        return nmem == ngen ? KernelMembership::all : KernelMembership::not_all;
    }

    // 7. roof lengths, and the growth rate of the substitution: Perron-Frobenius root by power iteration
    static void stats(const std::vector<std::vector<int>>& w, const std::vector<char>& ok, const char* nm)
    {
        size_t mn = 1 << 30, mx = 0, n = 0;
        double sum = 0;
        for (size_t j = 0; j < w.size(); j++) if (ok[j]) { mn = std::min(mn, w[j].size()); mx = std::max(mx, w[j].size()); sum += w[j].size(); n++; }
        if (!n) return;
        printf("%s roof lengths (in S_c-steps): min %zu, mean %.2f, max %zu", nm, mn, sum/n, mx);
        // the growth rate of the substitution: Perron-Frobenius root by power iteration
        size_t S = w.size();
        std::vector<double> v(S, 1.0), v2(S);
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

    // 9. the substitution file subst5_C_R_tTPHI.txt, for the t of ball index tsi
    std::string write_file(double c, double R, int tphi, int tsi, const std::vector<Gen>& S,
                           const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down) const
    {
        char fn[80];
        snprintf(fn, sizeof fn, "subst5_%.2f_%.2f_t%d.txt", c, R, tphi);
        return write_named(fn, c, R, tsi, S, up, down, "");
    }
    // the file of a search with options (kernel_search.h): the name with the options' suffix (subst5_C_R_tTPHI_SUFFIX.txt), and
    // their comment line after the header
    std::string write_file(double c, double R, int tphi, int tsi, const std::vector<Gen>& S,
                           const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down,
                           const std::string& suffix, const std::string& comment) const
    {
        char fn[80];
        snprintf(fn, sizeof fn, "subst5_%.2f_%.2f_t%d", c, R, tphi);
        return write_named(std::string(fn) + suffix + ".txt", c, R, tsi, S, up, down, comment);
    }
    std::string write_named(const std::string& fn, double c, double R, int tsi, const std::vector<Gen>& S,
                            const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down,
                            const std::string& comment) const
    {
        FILE* f = fopen(fn.c_str(), "w");
        fprintf(f, "# chz5 block-filling data: the IMM fibered 5-manifold; Gamma = ker mu in W(P5), phi = h/2\n");
        fprintf(f, "# words are in the 16 facet reflections (rh5.h); c %.4f  R %.4f  rho %.4f\n", c, R, rho);
        if (!comment.empty()) fprintf(f, "%s\n", comment.c_str());
        fprintf(f, "t %s\n", fword(ball.path(tsi)).c_str());
        fprintf(f, "S %zu\n", S.size());
        for (size_t j = 0; j < S.size(); j++) fprintf(f, "%zu  d=%.6f  %s\n", j, S[j].d, fword(S[j].word).c_str());
        fprintf(f, "up      # t^-1 s_j t = product of the listed s's\n");
        for (size_t j = 0; j < S.size(); j++) { fprintf(f, "%zu:", j); for (int l : up[j]) fprintf(f, " %d", l); fprintf(f, "\n"); }
        fprintf(f, "down    # t s_j t^-1 = product of the listed s's\n");
        for (size_t j = 0; j < S.size(); j++) { fprintf(f, "%zu:", j); for (int l : down[j]) fprintf(f, " %d", l); fprintf(f, "\n"); }
        fclose(f);
        return fn;
    }

    // 10. the convergence of the chains, on S^4. (D6 and to_double are those of the backend: ON1<5>::DMat, the legacy
    // D6, and ON1<5>::to_double, the legacy toD6 of zipcore5.h; the legacy chz5.cc had its own copies, D6 and todbl,
    // which are the same.)
    typedef O51::DMat D6;
    static void chain_header() { printf("   n    #points   max chord on S^4   d(o, chain) min..max\n"); }
    static D6 dmul(const D6& a, const D6& b)
    {
        D6 c;
        for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) { double s = 0; for (int k = 0; k < 6; k++) s += a[i][k]*b[k][j]; c[i][j] = s; }
        return c;
    }
    // the point m o of S^4 (the last column, radially projected), and d(o, m o) = acosh m_66 (legacy: the lambda pt
    // of convergence)
    static void chain_point(const D6& m, double s[5], double& d)
    {
        double nn = 0;
        for (int i = 0; i < 5; i++) { s[i] = m[i][5]; nn += s[i]*s[i]; }
        nn = std::sqrt(nn);
        for (int i = 0; i < 5; i++) s[i] /= nn;
        d = m[5][5] <= 1 ? 0 : std::acosh(m[5][5]);
    }
};

}  // namespace imm
}  // namespace zippers
