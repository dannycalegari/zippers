/*  chz5.cc  --  the block-filling search for the fibered hyperbolic 5-manifold of
    Italiano-Martelli-Migliorini (see rh5.h): Gamma = ker mu in the reflection group W of the
    right-angled polytope P5, phi = h/2, K = ker phi.

    THE SEARCH (Calegari-Loukidou, Zippers, Thm 4.9), as in chz.cc.  For a step radius c let
    S_c = { s in K, s != 1 : d(o, s o) <= c }, and fix t in Gamma with phi(t) = 1.  A
    breadth-first search of K with steps S_c, inside the ball of radius R, looks for chains from
    1 to t^-1 s t and to t s t^-1 (s in S_c): the roofs one level up and down.  If all of them
    are found, and every Schreier generator g of Gamma has g t^-phi(g) reached too, then <S_c>
    is normalised by t and <S_c, t> = Gamma, which PROVES that S_c generates K.

    Usage:  chz5 selftest
            chz5 ball RHO                   the tiles of B(RHO): counts, and the cocycle check
            chz5 kernel C R RHO [TPHI]      the search; writes subst5_C_R_tTPHI.txt

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "rh5.h"
#include <array>
#include <map>

static int selftest()
{
    int bad = 0;
    for (int f = 0; f < 16; f++) if (!m6eq(m6inv(REF[f]), REF[f])) bad++;
    // the relations of W and the cocycle on them: (r_f r_g)^2 = 1 for orthogonal facets, and
    // h, mu agree on r_f r_g and r_g r_f from every starting state
    int nsq = 0;
    for (int f = 0; f < 16; f++)
        for (int g = 0; g < 16; g++) {
            int d = 0;
            for (int k = 0; k < 5; k++) d += FS[f][k] != FS[g][k];
            if (d != 2) continue;
            nsq++;
            for (int mu = 0; mu < 16; mu++) {
                int h1 = hstep(mu, f) + hstep(mu ^ (1 << MOVE[f]), g);
                int h2 = hstep(mu, g) + hstep(mu ^ (1 << MOVE[g]), f);
                if (h1 != h2) bad++;
            }
        }
    // random words: g g^-1 = 1, and J-orthogonality
    uint64_t r = 12345;
    for (int trial = 0; trial < 200; trial++) {
        M6 g = m6id();
        for (int k = 0; k < 12; k++) { r = splitmix(r); g = m6mul(g, REF[r % 16]); }
        if (!m6eq(m6mul(g, m6inv(g)), m6id())) bad++;
    }
    printf("selftest: %d orthogonal pairs, cocycle on every square from all 16 states; %s\n",
           nsq, bad ? "FAILED" : "ok");
    return bad ? 1 : 0;
}

static int ballmode(double rho)
{
    guard6(rho);
    double t0 = now();
    Store6 B;
    long bad = enumerate6(B, rho);
    printf("B(%.2f): %zu tiles (%.2fs); (mu, h) inconsistencies %ld\n", rho, B.size(), now() - t0, bad);
    // counts by radius, in Gamma = ker mu, and in K (mu = 0, h = 0)
    for (double r = 1; r <= rho + 1e-9; r += 0.5) {
        size_t n = 0, ng = 0, nk = 0;
        std::map<int, int> hcount;
        for (size_t i = 0; i < B.size(); i++) if (B.dist[i] <= r) {
            n++;
            if (B.mu[i] == 0) { ng++; hcount[B.h[i]]++; if (B.h[i] == 0) nk++; }
        }
        printf("  d <= %.1f: %8zu tiles, %7zu in Gamma, %6zu in K   (0.39 e^4r = %.0f)   h on Gamma:",
               r, n, ng, nk, 0.39*std::exp(4*r));
        for (auto& kv : hcount) printf(" %d:%d", kv.first, kv.second);
        printf("\n");
    }
    return bad ? 1 : 0;
}

// the coset representatives of Gamma = ker mu: c_mu = product of r_{2m} over the moves m in mu
static vector<int> coset_rep(int mu)
{
    vector<int> w;
    for (int m = 0; m < 4; m++) if (mu & (1 << m)) w.push_back(2*m);
    return w;
}

static vector<int> invword(const vector<int>& w) { return vector<int>(w.rbegin(), w.rend()); }

struct Gen6 { M6 m; vector<int> word; double d; };

static void stats(const vector<vector<int>>& w, const vector<char>& ok, const char* nm)
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

// points on S^4 in double precision: g o -> (x1..x5)/|x|
static void s4point(const M6& g, double p[5])
{
    double n = 0;
    for (int i = 0; i < 5; i++) { p[i] = 0.5*(double)g.e[i][5]; n += p[i]*p[i]; }
    n = std::sqrt(n);
    for (int i = 0; i < 5; i++) p[i] = n > 0 ? p[i]/n : 0;
}

typedef std::array<std::array<double,6>,6> D6;
static D6 todbl(const M6& g) { D6 m; for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) m[i][j] = 0.5*(double)g.e[i][j]; return m; }
static D6 dmul(const D6& a, const D6& b)
{
    D6 c;
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) { double s = 0; for (int k = 0; k < 6; k++) s += a[i][k]*b[k][j]; c[i][j] = s; }
    return c;
}

// the largest chord between consecutive points of the level-n chain t^n u o, u running over
// the prefixes of theta^n(s_y): it must shrink geometrically for the arcs to exist
static void convergence(const vector<Gen6>& S, const vector<vector<int>>& th, const M6& t, int y, size_t Lmax, const char* nm)
{
    printf("convergence of the chains for %s over the segment (1, s_%d):\n", nm, y);
    printf("   n    #points   max chord on S^4   d(o, chain) min..max\n");
    vector<D6> Sd;
    for (auto& s : S) Sd.push_back(todbl(s.m));
    D6 td = todbl(t), T = todbl(m6id());
    vector<int> w = {y};
    for (int n = 0; n < 40; n++) {
        D6 P = T;
        auto pt = [&](const D6& m, double s[5], double& d) {
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
            P = dmul(P, Sd[w[m]]);
            pt(P, cur, d);
            dmin = std::min(dmin, d); dmax = std::max(dmax, d);
            double e = 0;
            for (int i = 0; i < 5; i++) e += (cur[i]-prev[i])*(cur[i]-prev[i]);
            mc = std::max(mc, std::sqrt(e));
            memcpy(prev, cur, sizeof prev);
        }
        printf("  %2d %10zu   %14.3e   %7.2f .. %.2f\n", n, w.size() + 1, mc, dmin, dmax);
        size_t next = 0;
        for (int l : w) next += th[l].size();
        if (next > Lmax || dmax > 30) break;
        vector<int> w2;
        w2.reserve(next);
        for (int l : w) for (int x : th[l]) w2.push_back(x);
        w.swap(w2);
        T = dmul(T, td);
    }
}

static int kernelmode(double c, double R, double rho, int tphi)
{
    double t0 = now();
    rho = std::max(rho, c);
    guard6(rho);
    Store6 B;
    long bad = enumerate6(B, rho);
    printf("ball: %zu tiles with d <= %.2f (%.2fs), (mu, h) inconsistencies %ld\n", B.size(), rho, now() - t0, bad);
    if (bad) return 1;

    // t: the shortest element of Gamma with phi = 1 (h = 2) that is loxodromic
    auto pick_t = [&](int ph) {
        int best = -1;
        for (size_t i = 0; i < B.size(); i++) {
            if (B.mu[i] != 0 || B.h[i] != 2*ph) continue;
            M6 g = B.load(i), p = m6id();
            for (int k = 0; k < 8; k++) p = m6mul(p, g);          // loxodromic: d(o, g^8 o) grows linearly
            if (m6dist(p) < 8*0.5*B.dist[i]) continue;
            if (best < 0 || B.dist[i] < B.dist[best]) best = (int)i;
        }
        return best;
    };
    int ti = pick_t(1);
    if (ti < 0) { printf("no loxodromic element with phi = 1 in the ball\n"); return 1; }
    M6 t = B.load(ti), tinv = m6inv(t);
    printf("t = [%s]   d(o, t o) = %.4f   h(t) = %d\n", fword(B.path(ti)).c_str(), B.dist[ti], B.h[ti]);

    // S_c
    vector<int> Sidx;
    for (size_t i = 1; i < B.size(); i++) if (B.mu[i] == 0 && B.h[i] == 0 && B.dist[i] <= c) Sidx.push_back(i);
    std::stable_sort(Sidx.begin(), Sidx.end(), [&](int a, int b) { return B.dist[a] < B.dist[b]; });
    vector<Gen6> S;
    for (int i : Sidx) S.push_back({B.load(i), B.path(i), B.dist[i]});
    printf("|S_c| = %zu kernel elements with d <= %.2f", S.size(), c);
    if (!S.empty()) printf(" (shortest %.4f)", S[0].d);
    printf("\n");
    if (S.empty()) return 1;
    // closed under inverses?
    for (auto& s : S) {
        M6 x = m6inv(s.m);
        bool f = false;
        for (auto& u : S) if (m6eq(u.m, x)) { f = true; break; }
        if (!f) { printf("S_c is not closed under inverses\n"); return 1; }
    }

    // breadth-first search of K with steps S_c inside B(R)
    guard6(R, 6e7);
    Store6 Kst;
    t0 = now();
    Kst.add(m6id(), -1, -1, 0, 0, 0, 0.f);
    const size_t CAP = 2500000;
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
    int maxdep = 0;
    for (size_t i = 0; i < Kst.size(); i++) maxdep = std::max(maxdep, Kst.depth[i]);
    printf("K-search: %zu elements of K reached inside B(%.2f) in %.2fs, max S_c-length %d%s\n",
           Kst.size(), R, now() - t0, maxdep, capped ? "  [CAPPED]" : "");

    // the inverses in S
    vector<int> INV(S.size(), -1);
    for (size_t j = 0; j < S.size(); j++) {
        M6 x = m6inv(S[j].m);
        for (size_t k = 0; k < S.size(); k++) if (m6eq(S[k].m, x)) INV[j] = (int)k;
    }
    // A word in S for an element x of K: from the K-store, or else by a best-first descent from
    // x (steps S, priority d(o, g o)) until it meets the K-store.  If x s_j1 ... s_jm = y, then
    // x = y s_jm^-1 ... s_j1^-1.
    const size_t BUDGET = 200000;
    long descents = 0, descent_nodes = 0;
    auto express = [&](const M6& x, vector<int>& word) -> bool {
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
                if (dy > 18) continue;
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
    };
    // the certificate
    auto roofs = [&](const M6& tt, const M6& tti, vector<vector<int>>& up, vector<vector<int>>& down,
                     vector<char>& uok, vector<char>& dok, long& nu, long& nd) {
        up.assign(S.size(), {}); down.assign(S.size(), {});
        uok.assign(S.size(), 0); dok.assign(S.size(), 0);
        nu = nd = 0;
        for (size_t j = 0; j < S.size(); j++) {
            if (express(m6mul(m6mul(tti, S[j].m), tt), up[j])) { uok[j] = 1; nu++; }
            if (express(m6mul(m6mul(tt, S[j].m), tti), down[j])) { dok[j] = 1; nd++; }
        }
        // check every roof exactly
        for (size_t j = 0; j < S.size(); j++) {
            M6 p = m6id(), q = m6id();
            for (int l : up[j]) p = m6mul(p, S[l].m);
            for (int l : down[j]) q = m6mul(q, S[l].m);
            if (uok[j] && !m6eq(p, m6mul(m6mul(tti, S[j].m), tt))) { printf("an up roof fails the exact check\n"); exit(1); }
            if (dok[j] && !m6eq(q, m6mul(m6mul(tt, S[j].m), tti))) { printf("a down roof fails the exact check\n"); exit(1); }
        }
        printf("  (%ld descents from outside the K-store, %ld nodes)\n", descents, descent_nodes);
    };
    vector<vector<int>> up, down;
    vector<char> uok, dok;
    long nu, nd;
    roofs(t, tinv, up, down, uok, dok, nu, nd);
    printf("roofs up   (t^-1 s t): %ld / %zu found\n", nu, S.size());
    printf("roofs down (t s t^-1): %ld / %zu found\n", nd, S.size());
    // the Schreier generators of Gamma
    int nmem = 0, ngen = 0;
    for (int mu = 0; mu < 16; mu++)
        for (int f = 0; f < 16; f++) {
            vector<int> w = coset_rep(mu);
            w.push_back(f);
            for (int x : invword(coset_rep(mu ^ (1 << MOVE[f])))) w.push_back(x);
            int m, hh;
            word_mu_h(w, m, hh);
            if (m != 0 || hh % 2) { printf("a Schreier generator is not in Gamma, or has odd h\n"); return 1; }
            M6 g = word_matrix(w);
            if (m6eq(g, m6id())) continue;
            ngen++;
            int ph = hh/2;
            M6 k = g;
            for (int e = 0; e < std::abs(ph); e++) k = m6mul(k, ph > 0 ? tinv : t);
            vector<int> w2;
            if (express(k, w2)) nmem++;
        }
    printf("Schreier generators g of Gamma with g t^-phi(g) in <S_c>: %d / %d\n", nmem, ngen);
    bool cert = nu == (long)S.size() && nd == (long)S.size() && nmem == ngen;
    printf("CERTIFICATE that S_c generates K: %s\n", cert ? "YES" : "no");
    stats(up, uok, "up  ");
    stats(down, dok, "down");
    if (!cert) return 2;

    // a steep t (phi(t) = tphi) climbs faster; its roofs are again words in S_c
    int tsi = ti;
    if (tphi != 1) {
        tsi = pick_t(tphi);
        if (tsi < 0) { printf("no loxodromic element with phi = %d in the ball\n", tphi); return 1; }
        t = B.load(tsi); tinv = m6inv(t);
        printf("steep t = [%s]   d(o, t o) = %.4f   h(t) = %d\n", fword(B.path(tsi)).c_str(), B.dist[tsi], B.h[tsi]);
        roofs(t, tinv, up, down, uok, dok, nu, nd);
        printf("steep roofs up %ld / %zu, down %ld / %zu\n", nu, S.size(), nd, S.size());
        stats(up, uok, "up  ");
        stats(down, dok, "down");
        if (nu < (long)S.size() || nd < (long)S.size()) return 2;
    }

    char fn[80];
    snprintf(fn, sizeof fn, "subst5_%.2f_%.2f_t%d.txt", c, R, tphi);
    FILE* f = fopen(fn, "w");
    fprintf(f, "# chz5 block-filling data: the IMM fibered 5-manifold; Gamma = ker mu in W(P5), phi = h/2\n");
    fprintf(f, "# words are in the 16 facet reflections (rh5.h); c %.4f  R %.4f  rho %.4f\n", c, R, rho);
    fprintf(f, "t %s\n", fword(B.path(tsi)).c_str());
    fprintf(f, "S %zu\n", S.size());
    for (size_t j = 0; j < S.size(); j++) fprintf(f, "%zu  d=%.6f  %s\n", j, S[j].d, fword(S[j].word).c_str());
    fprintf(f, "up      # t^-1 s_j t = product of the listed s's\n");
    for (size_t j = 0; j < S.size(); j++) { fprintf(f, "%zu:", j); for (int l : up[j]) fprintf(f, " %d", l); fprintf(f, "\n"); }
    fprintf(f, "down    # t s_j t^-1 = product of the listed s's\n");
    for (size_t j = 0; j < S.size(); j++) { fprintf(f, "%zu:", j); for (int l : down[j]) fprintf(f, " %d", l); fprintf(f, "\n"); }
    fclose(f);
    printf("wrote %s\n", fn);

    convergence(S, up, t, 0, 400000, "Z+ (t, theta_up)");
    convergence(S, down, tinv, 0, 400000, "Z- (t^-1, theta_down)");
    return 0;
}

int main(int argc, char** argv)
{
    rh5_setup();
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc >= 3 && !strcmp(argv[1], "ball")) return ballmode(atof(argv[2]));
    if (argc >= 5 && !strcmp(argv[1], "kernel"))
        return kernelmode(atof(argv[2]), atof(argv[3]), atof(argv[4]), argc >= 6 ? atoi(argv[5]) : 1);
    fprintf(stderr, "usage: chz5 selftest | ball RHO | kernel C R RHO [TPHI]\n");
    return 1;
}
