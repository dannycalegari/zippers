// SPDX-License-Identifier: GPL-3.0-or-later
//
// cartwright_steger_search.h: the search policy of the Cartwright-Steger example (examples/cartwright_steger.h) for
// the block-filling search of search/kernel_search.h. namespace zippers::cs.
//
// Pi = pi_1 of the Cartwright-Steger surface, phi = the first coordinate of f, K = ker phi. What this example does at
// each step of the search (kernel_search.h lists the steps):
//   1. the small ball: the geometric generators geogens(4.5) (all of B(4.5) \ {1}), and the ball B(rho + margin) that
//      they enumerate, with rho = max(rho, c) (rho = 5, margin = 0.5), and its count of f-inconsistencies;
//   2. t: the shortest element of the ball with phi = 1;
//   3. S_c: phi = 0, 0 < d <= c, sorted by d with std::sort (S_c has ties in d, so the order is that of libc++'s
//      std::sort: keep it, do not make it stable); each element with its f2 and its a-word;
//   4. the K-search: each element carries f2, checked whenever an element is reached again (the f-inconsistencies);
//      no guard; cap 2,000,000;
//   5. the roofs: looked up in the K-store only (no descent), not checked again;
//   6. membership: a_i t^-phi(a_i) for the three generators a_i, whose words go into the file;
//   7. the statistics of the roofs: their lengths, and the growth rate by 300 steps of a power iteration;
//   9. the file subst_C_R_tTPHI.txt: the t-word, S_c with d and f2, the roofs up and down, and the members;
//  10. the convergence: points of S^3 by ballpoint, stopping once d(o, chain) > 34.
//
// Moved from kernelmode, stats, writesubst, dmul, sphpoint and convergence of apps/s3/chz.cc, which were those
// of legacy/s3/chz.cc under new names: the printed lines, the sort, the formats and every expression are the legacy
// ones, so that chz prints and writes exactly what the legacy program did.

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "geometry/pu21/pu21.h"
#include "search/kernel_search.h"

namespace zippers {
namespace cs {

struct SearchPolicy {
    typedef PU21 B;
    typedef FVal Label;               // in the K-store: (0, f2)
    typedef cs::Gen Gen;              // an element of S_c: its matrix, (0, f2), its a-word

    static constexpr size_t kstore_cap = 2000000;          // about 340 MB
    static constexpr bool descent = false;                 // the roofs are looked up in the K-store only
    static constexpr bool check_roofs = false;
    static constexpr double chain_stop = 34;               // 34: double precision is still ample

    double rho, margin;
    std::vector<Gen> G;                                    // the geometric generators (for the words)
    Store ball;
    long badk = 0;                                         // the f-inconsistencies of the K-search
    std::vector<std::vector<int>> mw;                      // the words of the members a_i t^-phi(a_i)

    explicit SearchPolicy(double rho_ = 5.0, double margin_ = 0.5) : rho(rho_), margin(margin_), mw(3) {}

    // 1. the small ball
    bool small_ball(double c)
    {
        double t0 = now();
        G = geogens(4.5);
        rho = std::max(rho, c);
        guard(rho + margin);
        long bad = enumerate(ball, G, rho + margin);
        printf("small ball: %zu elements with d <= %.2f (%.2fs), f-inconsistencies %ld\n", ball.size(), rho + margin, now() - t0, bad);
        return true;
    }

    // 2. t: the shortest element with phi = ph
    int pick_t(int ph) const
    {
        int ti = -1;
        for (size_t i = 0; i < ball.size(); i++)
            if (ball.label[i].f0 == ph && (ti < 0 || ball.dist[i] < ball.dist[ti])) ti = i;
        return ti;
    }
    void print_t(int ti, bool steep) const
    {
        if (!steep)
            printf("t = %s   d(o,t o) = %.4f   f(t) = (%d,%d)\n", word(ball, ti, G).c_str(), ball.dist[ti], ball.label[ti].f0, ball.label[ti].f1);
        else
            printf("steep t = %s   d(o,t o) = %.4f   f(t) = (%d,%d)\n", word(ball, ti, G).c_str(), ball.dist[ti], ball.label[ti].f0, ball.label[ti].f1);
    }
    void print_no_t(int ph) const { printf("no element with phi = %d in the small ball\n", ph); }

    // the options --tlist and --tcand (kernel_search.h): the candidates for t with phi = ph, the elements of the ball with
    // f0 = ph (Pi is torsion free and cocompact, so every element but 1 is loxodromic), sorted by d(o, t o) (the float
    // that pick_t compares), ties in the ball's order: candidate 0 is pick_t(ph)'s
    std::vector<int> t_candidates(int ph) const
    {
        std::vector<int> c;
        for (size_t i = 0; i < ball.size(); i++) if (ball.label[i].f0 == ph) c.push_back((int)i);
        std::stable_sort(c.begin(), c.end(), [&](int a, int b) { return ball.dist[a] < ball.dist[b]; });
        return c;
    }
    // whether the elements a and b of the ball have the same d(o, g o), exactly: the same |g_33|^2 in Z[zeta]
    bool same_length(int a, int b) const
    {
        Zz x = ball.load(a).e[2][2], y = ball.load(b).e[2][2];
        return zeq(zmul(x, zconj(x)), zmul(y, zconj(y)));
    }
    void print_candidate(int k, int i) const
    {
        printf("  %4d  d(o,t o) = %.6f  f(t) = (%d,%d)  %s\n", k, ball.dist[i], ball.label[i].f0, ball.label[i].f1,
               word(ball, i, G).c_str());
    }

    // 3. S_c
    std::vector<int> kernel_indices(double c) const
    {
        std::vector<int> Sidx;
        for (size_t i = 1; i < ball.size(); i++) if (ball.label[i].f0 == 0 && ball.dist[i] <= c) Sidx.push_back(i);
        std::sort(Sidx.begin(), Sidx.end(), [&](int a, int b) { return ball.dist[a] < ball.dist[b]; });
        return Sidx;
    }
    Gen generator(int i) const { return {ball.load(i), 0, ball.label[i].f1, word(ball, i, G)}; }
    bool check_S(const std::vector<Gen>&) const { return true; }

    // 4. the K-search: f2 along the way, and its consistency
    void kernel_guard(double) const {}
    Label kstore_root_label() const { return FVal{0, 0}; }
    Label kstore_label(const Label& from, const Gen& s) const { return FVal{0, from.f1 + s.f1}; }
    void kstore_revisit(const Label& found, const Label& label) { if (found.f1 != label.f1) badk++; }
    void print_kstore(size_t n, double R, double seconds, int maxdep, bool capped) const
    {
        printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d, f-inconsistencies %ld%s\n",
               n, R, seconds, maxdep, badk, capped ? "  [CAPPED]" : "");
    }

    // 6. membership: a_i t^-phi(a_i) for the generators a_i of Pi
    template <class FindWord>
    KernelMembership membership(const M3& t, const M3&, FindWord& find_word)
    {
        int memb = 0;
        for (int i = 0; i < 3; i++) {
            std::vector<int> w;
            bool found = find_word(mmul(A[i], mpow(t, -FA[i][0])), w);
            if (found) { memb++; mw[i] = w; }
            printf("a%d t^%d in <S_c>: %s\n", i+1, -FA[i][0], found ? "yes" : "not reached");
        }
        return memb == 3 ? KernelMembership::all : KernelMembership::not_all;
    }

    // 7. roof lengths, and the Perron-Frobenius root of the substitution (the growth rate of |theta^n(s)|), by power
    // iteration
    static void stats(const std::vector<std::vector<int>>& w, const std::vector<char>& ok, const char* nm)
    {
        size_t mx = 0, mn = (size_t)-1;
        double sum = 0;
        long n = 0;
        for (size_t j = 0; j < w.size(); j++)
            if (ok[j]) { mx = std::max(mx, w[j].size()); mn = std::min(mn, w[j].size()); sum += w[j].size(); n++; }
        if (!n) return;
        printf("%s roof lengths (in S_c-steps): min %zu, mean %.2f, max %zu", nm, mn, sum/n, mx);
        if ((size_t)n < w.size()) { printf("\n"); return; }
        std::vector<double> x(w.size(), 1.0), y(w.size());
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

    // 9. the substitution file subst_C_R_tTPHI.txt, for the t of ball index ti
    std::string write_file(double c, double R, int tphi, int ti, const std::vector<Gen>& S,
                           const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down) const
    {
        char fn[64];
        snprintf(fn, sizeof fn, "subst_%.2f_%.2f_t%d.txt", c, R, tphi);
        writesubst(fn, c, R, word(ball, ti, G), S, up, down, mw);
        return fn;
    }
    // the file of a search with options (kernel_search.h): the name with the options' suffix (subst_C_R_tTPHI_SUFFIX.txt), and
    // their comment line after the header
    std::string write_file(double c, double R, int tphi, int ti, const std::vector<Gen>& S,
                           const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down,
                           const std::string& suffix, const std::string& comment) const
    {
        char fn[64];
        snprintf(fn, sizeof fn, "subst_%.2f_%.2f_t%d", c, R, tphi);
        std::string name = std::string(fn) + suffix + ".txt";
        writesubst(name.c_str(), c, R, word(ball, ti, G), S, up, down, mw, comment);
        return name;
    }
    static void writesubst(const char* fn, double c, double R, const std::string& tw, const std::vector<Gen>& S,
                           const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down,
                           const std::vector<std::vector<int>>& mw, const std::string& comment = "")
    {
        FILE* f = fopen(fn, "w");
        if (!f) { perror(fn); return; }
        fprintf(f, "# chz block-filling data: Cartwright-Steger Pi, phi = first coordinate of f\n");
        fprintf(f, "# c %.4f  R %.4f\n", c, R);
        if (!comment.empty()) fprintf(f, "%s\n", comment.c_str());
        fprintf(f, "t %s\n", tw.c_str());
        fprintf(f, "S %zu\n", S.size());
        for (size_t j = 0; j < S.size(); j++)
            fprintf(f, "%zu  d=%.6f  f2=%d  %s\n", j, hdist(S[j].m.e[2][2]), S[j].f1, S[j].name.c_str());
        fprintf(f, "up      # t^-1 s_j t = product of the listed s's\n");
        for (size_t j = 0; j < S.size(); j++) { fprintf(f, "%zu:", j); for (int l : up[j]) fprintf(f, " %d", l); fprintf(f, "\n"); }
        fprintf(f, "down    # t s_j t^-1 = product of the listed s's\n");
        for (size_t j = 0; j < S.size(); j++) { fprintf(f, "%zu:", j); for (int l : down[j]) fprintf(f, " %d", l); fprintf(f, "\n"); }
        fprintf(f, "member  # a_i t^-phi(a_i) = product of the listed s's\n");
        for (int i = 0; i < 3; i++) { fprintf(f, "a%d:", i+1); for (int l : mw[i]) fprintf(f, " %d", l); fprintf(f, "\n"); }
        fclose(f);
    }

    // 10. the convergence of the chains, on S^3. (D3 and toD are those of pu21.h; the legacy chz.cc had its own
    // copies, D3 and todbl, which are the same.)
    static void chain_header() { printf("   n    #points   max chord on S^3   d(o, chain) min..max\n"); }
    static D3 dmul(const D3& a, const D3& b)
    {
        D3 c;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) c[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] + a[i][2]*b[2][j];
        return c;
    }
    // the point m o of S^3 (radially projected from the ball), and d(o, m o) = 2 acosh |m_33|, accurate near S^3
    // (legacy sphpoint)
    static void chain_point(const D3& m, double s[4], double& d)
    {
        cpx c[3] = {m[0][2], m[1][2], m[2][2]};
        double p[4];
        ballpoint(c, p);
        double n = std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2] + p[3]*p[3]);
        for (int i = 0; i < 4; i++) s[i] = p[i]/n;
        double a = std::abs(m[2][2]);
        d = a <= 1 ? 0 : 2*std::acosh(a);
    }
};

}  // namespace cs
}  // namespace zippers
