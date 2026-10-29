/*  csgroup.h  --  the Cartwright-Steger group Pi, exactly: arithmetic in Z[zeta_12], the
    generators a1, a2, a3 of Cartwright-Koziarz-Yeung, an exact element store, and ball
    enumeration.  Shared by chz.cc (the block-filling search) and zipper3.cc (the renderer).
    See the comment at the top of chz.cc for the conventions.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <complex>
#include <vector>
#include <array>
#include <string>
#include <queue>
#include <algorithm>
#include <chrono>

typedef int64_t i64;
typedef std::complex<double> cpx;
using std::vector;
using std::string;

static double now()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------------------------------
// Z[zeta], zeta = exp(pi i/6): coefficients of 1, zeta, zeta^2, zeta^3; zeta^4 = zeta^2 - 1.

struct Zz { i64 c[4]; };

static inline Zz Z(i64 a, i64 b, i64 c, i64 d) { Zz z; z.c[0]=a; z.c[1]=b; z.c[2]=c; z.c[3]=d; return z; }
static inline Zz zadd(const Zz& a, const Zz& b) { return Z(a.c[0]+b.c[0], a.c[1]+b.c[1], a.c[2]+b.c[2], a.c[3]+b.c[3]); }
static inline Zz zsub(const Zz& a, const Zz& b) { return Z(a.c[0]-b.c[0], a.c[1]-b.c[1], a.c[2]-b.c[2], a.c[3]-b.c[3]); }
static inline bool zzero(const Zz& a) { return !a.c[0] && !a.c[1] && !a.c[2] && !a.c[3]; }
static inline bool zeq(const Zz& a, const Zz& b) { return !memcmp(a.c, b.c, sizeof a.c); }

static inline Zz zmul(const Zz& a, const Zz& b)
{
    i64 p[7] = {0,0,0,0,0,0,0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) p[i+j] += a.c[i]*b.c[j];
    // zeta^4 = zeta^2 - 1, zeta^5 = zeta^3 - zeta, zeta^6 = -1
    return Z(p[0]-p[4]-p[6], p[1]-p[5], p[2]+p[4], p[3]+p[5]);
}

static inline Zz zrot(const Zz& a) { return Z(-a.c[3], a.c[0], a.c[1]+a.c[3], a.c[2]); }      // times zeta
static inline Zz zconj(const Zz& a) { return Z(a.c[0]+a.c[2], a.c[1], -a.c[2], -(a.c[1]+a.c[3])); }
static inline Zz zrotk(Zz a, int k) { k = ((k % 12) + 12) % 12; while (k--) a = zrot(a); return a; }

static const cpx ZETA = std::polar(1.0, M_PI/6);           // the embedding where F' is indefinite
static const cpx ZETA5 = std::polar(1.0, 5*M_PI/6);        // the Galois conjugate: F' definite

static inline cpx zval(const Zz& a, cpx w = ZETA)
{
    return (double)a.c[0] + w*((double)a.c[1] + w*((double)a.c[2] + w*(double)a.c[3]));
}

static inline bool zless(const Zz& a, const Zz& b)
{
    for (int i = 0; i < 4; i++) if (a.c[i] != b.c[i]) return a.c[i] < b.c[i];
    return false;
}

// the k in 0..11 for which zeta^k a is lexicographically least (a != 0, so it is unique)
static inline int normk(const Zz& a)
{
    Zz y = a, best = a;
    int bk = 0;
    for (int k = 1; k < 12; k++) {
        y = zrot(y);
        if (zless(y, best)) { best = y; bk = k; }
    }
    return bk;
}

// ---------------------------------------------------------------------------------------
// 3x3 matrices over Z[zeta]

struct M3 { Zz e[3][3]; };

static M3 mzero() { M3 A; memset(&A, 0, sizeof A); return A; }
static M3 mid() { M3 A = mzero(); for (int i = 0; i < 3; i++) A.e[i][i] = Z(1,0,0,0); return A; }

static M3 mmul(const M3& A, const M3& B)
{
    M3 C;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            C.e[i][j] = zadd(zadd(zmul(A.e[i][0], B.e[0][j]), zmul(A.e[i][1], B.e[1][j])), zmul(A.e[i][2], B.e[2][j]));
    return C;
}

static M3 madj(const M3& A)
{
    M3 C;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) C.e[i][j] = zconj(A.e[j][i]);
    return C;
}

static M3 mrotk(M3 A, int k) { for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) A.e[i][j] = zrotk(A.e[i][j], k); return A; }
static bool meq(const M3& A, const M3& B) { return !memcmp(&A, &B, sizeof A); }

static M3 FP, FPI;            // F' and its inverse (det F' = -1, so both are integral)

static M3 minv(const M3& A) { return mmul(mmul(FPI, madj(A)), FP); }

static M3 mnormal(const M3& A) { return mrotk(A, normk(A.e[2][2])); }

// is A = zeta^k I for some k?  (returns k, or -1)
static int mscalar(const M3& A)
{
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (i != j && !zzero(A.e[i][j])) return -1;
    if (!zeq(A.e[0][0], A.e[1][1]) || !zeq(A.e[0][0], A.e[2][2])) return -1;
    Zz one = Z(1,0,0,0), y = one;
    for (int k = 0; k < 12; k++) { if (zeq(A.e[0][0], y)) return k; y = zrot(y); }
    return -1;
}

static M3 mpow(const M3& A, int n)
{
    M3 B = mid(), P = n >= 0 ? A : minv(A);
    for (int m = abs(n); m > 0; m--) B = mmul(B, P);
    return B;
}

// ---------------------------------------------------------------------------------------
// The data of Cartwright-Steger / CKY, in primed coordinates.

static M3 U, V, B, J;         // u', v', b', j
static M3 A[3];               // a1, a2, a3
static const int FA[3][2] = { {1,3}, {-2,1}, {-1,-1} };     // f(a_i)

static void setup()
{
    Zz r = Z(0,2,0,-1);                      // sqrt 3 = zeta + zeta^-1 = 2 zeta - zeta^3
    Zz one = Z(1,0,0,0), mone = Z(-1,0,0,0), zero = Z(0,0,0,0);
    FP = mzero();
    FP.e[0][0] = zadd(r, one); FP.e[0][1] = mone;
    FP.e[1][0] = mone;         FP.e[1][1] = zsub(r, one);
    FP.e[2][2] = mone;
    FPI = mzero();
    FPI.e[0][0] = zsub(r, one); FPI.e[0][1] = one;
    FPI.e[1][0] = one;          FPI.e[1][1] = zadd(r, one);
    FPI.e[2][2] = mone;

    U = mzero();
    U.e[0][0] = Z(0,-1,1,1);  U.e[0][1] = Z(1,-1,0,0);
    U.e[1][0] = Z(-1,0,1,1);  U.e[1][1] = Z(0,1,0,-1);
    U.e[2][2] = one;
    V = mzero();
    V.e[0][0] = Z(0,0,0,1);
    V.e[1][0] = Z(-1,-1,1,1); V.e[1][1] = one;
    V.e[2][2] = one;
    B = mzero();
    B.e[0][0] = one;
    B.e[1][0] = Z(2,2,-1,-2); B.e[1][1] = Z(-1,-1,1,1); B.e[1][2] = Z(0,0,-1,-1);
    B.e[2][0] = Z(0,1,1,0);   B.e[2][1] = Z(-1,0,0,-1); B.e[2][2] = Z(1,1,0,-1);
    J = mzero();
    J.e[0][0] = Z(0,1,0,0); J.e[1][1] = Z(0,1,0,0); J.e[2][2] = one;
    (void)zero;

    M3 Ui = minv(U), Vi = minv(V);
    // a1 = v u v^-1 j^4 b u v j^2
    A[0] = mmul(mmul(mmul(mmul(mmul(mmul(mmul(V, U), Vi), mpow(J,4)), B), U), V), mpow(J,2));
    // a2 = v^2 u b u v^-1 u v^2 j
    A[1] = mmul(mmul(mmul(mmul(mmul(mmul(mmul(mpow(V,2), U), B), U), Vi), U), mpow(V,2)), J);
    // a3 = u^-1 v^2 u j^9 b v^-1 u v^-1 j^8
    A[2] = mmul(mmul(mmul(mmul(mmul(mmul(mmul(mmul(Ui, mpow(V,2)), U), mpow(J,9)), B), Vi), U), Vi), mpow(J,8));
    for (int i = 0; i < 3; i++) A[i] = mnormal(A[i]);
}

static double hdist(const Zz& g33)        // d(o, g o) from the (3,3) entry
{
    double a = std::abs(zval(g33));
    return a <= 1 ? 0 : 2*acosh(a);
}

// the point g.o of the unit ball of C^2, from the third column of g (primed coordinates):
// gamma_0 maps primed to F-coordinates and D = diag(1,1,sqrt(r-1)) maps those to the unit ball
static inline void ballpoint(const cpx c[3], double p[4])
{
    const double r = sqrt(3.0);
    cpx w1 = c[0], w2 = c[0] + (1-r)*c[1], w3 = sqrt(r-1)*c[2];
    cpx z1 = w1/w3, z2 = w2/w3;
    p[0] = z1.real(); p[1] = z1.imag(); p[2] = z2.real(); p[3] = z2.imag();
}

// ---------------------------------------------------------------------------------------
// A store of exact group elements with an open-addressing hash on the normalised third column.

static inline uint64_t splitmix(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct Store {
    vector<int32_t> mat;                          // 36 per element: (row*3+col)*4 + coefficient
    vector<int32_t> parent, gen, depth, f0, f1;
    vector<float> dist;
    vector<int32_t> slot;
    uint64_t mask = 0;
    long overflow_guard_hits = 0;

    size_t size() const { return parent.size(); }

    static uint64_t hkey(const i64* k)
    {
        uint64_t h = 0x2545f4914f6cdd1dULL;
        for (int i = 0; i < 12; i++) h = splitmix(h ^ (uint64_t)k[i]);
        return h;
    }
    void keyof(size_t idx, i64* k) const
    {
        for (int i = 0; i < 3; i++)
            for (int c = 0; c < 4; c++) k[4*i+c] = mat[36*idx + (i*3+2)*4 + c];
    }
    int find(const i64* k) const
    {
        uint64_t h = hkey(k) & mask;
        i64 kk[12];
        while (slot[h] >= 0) {
            keyof(slot[h], kk);
            if (!memcmp(kk, k, sizeof kk)) return slot[h];
            h = (h + 1) & mask;
        }
        return -1;
    }
    void place(int idx)
    {
        i64 k[12];
        keyof(idx, k);
        uint64_t h = hkey(k) & mask;
        while (slot[h] >= 0) h = (h + 1) & mask;
        slot[h] = idx;
    }
    void grow()
    {
        size_t n = slot.empty() ? 1024 : 2*slot.size();
        slot.assign(n, -1);
        mask = n - 1;
        for (size_t i = 0; i < size(); i++) place(i);
    }
    M3 load(size_t idx) const
    {
        M3 g;
        const int32_t* m = &mat[36*idx];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int c = 0; c < 4; c++)
            g.e[i][j].c[c] = m[(i*3+j)*4 + c];
        return g;
    }
    int add(const M3& g, int par, int gn, int dep, int a0, int a1, float d)
    {
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int c = 0; c < 4; c++) {
            i64 x = g.e[i][j].c[c];
            if (x > (1LL<<28) || x < -(1LL<<28)) {
                fprintf(stderr, "coefficient %lld exceeds the 2^28 storage guard; aborting\n", (long long)x);
                exit(1);
            }
            mat.push_back((int32_t)x);
        }
        parent.push_back(par); gen.push_back(gn); depth.push_back(dep);
        f0.push_back(a0); f1.push_back(a1); dist.push_back(d);
        if (2*size() > slot.size()) grow(); else place(size() - 1);
        return size() - 1;
    }
    // look up an arbitrary matrix (normalising it first)
    int lookup(const M3& g) const
    {
        M3 h = mnormal(g);
        i64 k[12];
        for (int i = 0; i < 3; i++) for (int c = 0; c < 4; c++) k[4*i+c] = h.e[i][2].c[c];
        int f = find(k);
        if (f >= 0 && !meq(load(f), h)) { fprintf(stderr, "third columns agree but matrices differ: the action is not free?!\n"); exit(1); }
        return f;
    }
    vector<int> path(int idx) const        // the generator labels from the root to idx
    {
        vector<int> w;
        for (int i = idx; parent[i] >= 0; i = parent[i]) w.push_back(gen[i]);
        std::reverse(w.begin(), w.end());
        return w;
    }
};

// the key of g*s, computed from g and the third column of s only (9 products instead of 27)
static inline int prodkey(const M3& g, const M3& s, i64* key, Zz* col)
{
    for (int i = 0; i < 3; i++)
        col[i] = zadd(zadd(zmul(g.e[i][0], s.e[0][2]), zmul(g.e[i][1], s.e[1][2])), zmul(g.e[i][2], s.e[2][2]));
    int k = normk(col[2]);
    for (int i = 0; i < 3; i++) {
        col[i] = zrotk(col[i], k);
        for (int c = 0; c < 4; c++) key[4*i+c] = col[i].c[c];
    }
    return k;
}

// ---------------------------------------------------------------------------------------
// Generators a_i^{+-1}, with labels for words: "a1".."a3", "A1".."A3" (A = inverse)

struct Gen { M3 m; int f0, f1; string name; };

static vector<Gen> agens()
{
    vector<Gen> G;
    for (int i = 0; i < 3; i++) {
        G.push_back({A[i], FA[i][0], FA[i][1], "a" + std::to_string(i+1)});
        G.push_back({mnormal(minv(A[i])), -FA[i][0], -FA[i][1], "A" + std::to_string(i+1)});
    }
    return G;
}

// best-first enumeration of { g : d(o, g o) <= D } by right multiplication by the generators,
// discarding anything farther than D.  Returns the number of f-inconsistencies found.
static long enumerate(Store& S, const vector<Gen>& G, double D)
{
    S.add(mnormal(mid()), -1, -1, 0, 0, 0, 0.f);
    typedef std::pair<float,int> P;
    std::priority_queue<P, vector<P>, std::greater<P>> pq;
    pq.push({0.f, 0});
    vector<char> done(1, 0);
    long bad = 0;
    i64 key[12];
    Zz col[3];
    while (!pq.empty()) {
        int i = pq.top().second; pq.pop();
        if (done[i]) continue;
        done[i] = 1;
        M3 g = S.load(i);
        for (size_t j = 0; j < G.size(); j++) {
            int k = prodkey(g, G[j].m, key, col);
            int a0 = S.f0[i] + G[j].f0, a1 = S.f1[i] + G[j].f1;
            int f = S.find(key);
            if (f >= 0) { if (S.f0[f] != a0 || S.f1[f] != a1) bad++; continue; }
            double d = hdist(col[2]);
            if (d > D) continue;
            M3 h = mrotk(mmul(g, G[j].m), k);
            int n = S.add(h, i, j, S.depth[i] + 1, a0, a1, (float)d);
            pq.push({(float)d, n});
            done.push_back(0);
        }
    }
    return bad;
}

static string word(const Store& S, int idx, const vector<Gen>& G)
{
    string w;
    for (int g : S.path(idx)) { if (!w.empty()) w += " "; w += G[g].name; }
    return w.empty() ? "1" : w;
}

// The ball B(D) has about sinh^4(D/2) elements (vol X = 8 pi^2), at ~170 bytes each.
// Refuse anything that would not fit comfortably in memory.
static void guard(double D)
{
    double n = pow(sinh(D/2), 4);
    if (n > 4e6) {
        fprintf(stderr, "refusing to enumerate B(%.2f): about %.2g elements, more than the 4e6 memory guard\n", D, n);
        exit(1);
    }
}

// A geometric generating set: all of B(rhog) \ {1}, found with the generators a_i^{+-1} and
// margin 3 (margins 3 and 4 give identical balls up to radius 5.5).  The names are a-words.
static vector<Gen> geogens(double rhog)
{
    vector<Gen> A6 = agens();
    Store S;
    guard(rhog + 3);
    enumerate(S, A6, rhog + 3);
    vector<Gen> G;
    for (size_t i = 1; i < S.size(); i++)
        if (S.dist[i] <= rhog) G.push_back({S.load(i), S.f0[i], S.f1[i], word(S, i, A6)});
    return G;
}

