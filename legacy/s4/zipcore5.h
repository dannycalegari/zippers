/*  zipcore5.h  --  the zipper arcs of the fibered hyperbolic 5-manifold (rh5.h): substitution
    data, the adaptive renderer, and the views of S^4.  The same construction as zipcore.h (the
    Cartwright-Steger surface on S^3), one dimension up:

    THE ARCS.  From `chz5 kernel`: a generating set S of K = ker phi, an element t of Gamma with
    phi(t) > 0, and the roofs t^-1 s t = x_1 ... x_k (x_i in S).  With V = t^+ (the attracting
    fixed point of t on S^4), the arc alpha_s from V to sV is self-similar:
        alpha_s = t ( alpha_{x_1} u x_1 alpha_{x_2} u ... u x_1...x_{k-1} alpha_{x_k} ).
    A branch (h, s) is split until its chord is below the accuracy; group elements are exact
    and a branch is identified by a fingerprint of its exact key (the last column of 2h).

    THE PICTURE.  S^4 is the unit sphere of R^5 (the Klein model's boundary).  It is projected
    stereographically from a pole P to R^4, and R^4 is shown by its shadow in R^3 along a hidden
    direction that can turn; the hidden coordinate is drawn as a second fog.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#pragma once

#include "rh5.h"
#include <array>
#include <sstream>
#include <fstream>
#include <random>
#include <unordered_map>
#include <zlib.h>

typedef std::array<double,6> V6;
typedef std::array<std::array<double,6>,6> D6;
struct P5 { double x[5]; };

static const double PI = 3.14159265358979323846;

static D6 toD6(const M6& g)
{
    D6 m;
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) m[i][j] = 0.5*(double)g.e[i][j];
    return m;
}

static inline V6 mv6(const D6& m, const V6& v)
{
    V6 r;
    for (int i = 0; i < 6; i++) { double s = 0; for (int k = 0; k < 6; k++) s += m[i][k]*v[k]; r[i] = s; }
    return r;
}

// a null vector (x, x6) -> its point x/|x| of S^4
static inline P5 spoint5(const V6& c)
{
    P5 p;
    double n = 0;
    for (int i = 0; i < 5; i++) { p.x[i] = c[i]; n += c[i]*c[i]; }
    n = std::sqrt(n);
    for (int i = 0; i < 5; i++) p.x[i] /= n;
    return p;
}

static inline double d5(const P5& a, const P5& b)
{
    double s = 0;
    for (int i = 0; i < 5; i++) s += (a.x[i]-b.x[i])*(a.x[i]-b.x[i]);
    return std::sqrt(s);
}

static inline double segd5(const P5& p, const P5& a, const P5& b)
{
    double u[5], w[5], uu = 0, uw = 0;
    for (int i = 0; i < 5; i++) { u[i] = b.x[i]-a.x[i]; w[i] = p.x[i]-a.x[i]; uu += u[i]*u[i]; uw += u[i]*w[i]; }
    double t = uu > 0 ? std::max(0.0, std::min(1.0, uw/uu)) : 0;
    double s = 0;
    for (int i = 0; i < 5; i++) { double e = w[i] - t*u[i]; s += e*e; }
    return std::sqrt(s);
}

// the attracting fixed point of a loxodromic element: power iteration on R^{5,1}
static V6 attracting6(const M6& t)
{
    D6 m = toD6(t);
    V6 v = {0.31, -0.17, 0.23, 0.11, -0.29, 1.0};
    for (int it = 0; it < 3000; it++) {
        v = mv6(m, v);
        double n = 0;
        for (double x : v) n += x*x;
        n = std::sqrt(n);
        for (double& x : v) x /= n;
    }
    if (v[5] < 0) for (double& x : v) x = -x;
    return v;
}

// ---------------------------------------------------------------------------------------
// The substitution data (a file written by `chz5 kernel`)

static vector<int> parse_fword(const string& w)
{
    vector<int> r;
    std::istringstream is(w);
    string tok;
    while (is >> tok) {
        if (tok == "1") continue;
        int f = atoi(tok.c_str());
        if (f < 0 || f >= 16) { fprintf(stderr, "bad facet letter '%s'\n", tok.c_str()); exit(1); }
        r.push_back(f);
    }
    return r;
}

struct Subst5 {
    string tword;
    vector<string> sword;
    vector<vector<int>> up, down;
};

static bool load_subst5(const char* fn, Subst5& D)
{
    std::ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot open %s\n", fn); return false; }
    string line;
    int mode = 0;
    size_t n = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        string h;
        is >> h;
        if (h == "t") { std::getline(is, D.tword); mode = 0; continue; }
        if (h == "S") { is >> n; mode = 1; continue; }
        if (h == "up") { mode = 2; D.up.assign(n, {}); continue; }
        if (h == "down") { mode = 3; D.down.assign(n, {}); continue; }
        if (mode == 1) {                // "j  d=...  word"
            string d, w, tok;
            is >> d;
            while (is >> tok) { if (!w.empty()) w += " "; w += tok; }
            D.sword.push_back(w);
        } else if (mode == 2 || mode == 3) {
            int j = atoi(h.c_str()), x;
            vector<int> r;
            while (is >> x) r.push_back(x);
            (mode == 2 ? D.up : D.down)[j] = r;
        }
    }
    if (D.tword.empty() || D.sword.size() != n || D.up.size() != n || D.down.size() != n) {
        fprintf(stderr, "%s: incomplete substitution file\n", fn);
        return false;
    }
    return true;
}

struct Letters5 {
    vector<M6> s;
    vector<int> inv;
    vector<char> plus;
    vector<int> canon;
};

struct Side5 {
    const char* name;
    M6 t;
    V6 vfix;                          // the attracting fixed point of t (a null vector)
    P5 V;                             // the same point of S^4
    vector<vector<int>> roof;
    vector<vector<M6>> off, keyoff;
    vector<vector<V6>> wv;
    vector<V6> ev;
};

static void make_side5(Side5& sd, const char* name, const M6& t, const vector<vector<int>>& roofIn, const Letters5& L)
{
    size_t n = L.s.size();
    sd.name = name;
    sd.t = t;
    sd.roof.assign(n, {});
    for (size_t j = 0; j < n; j++) {
        if (L.plus[j]) sd.roof[j] = roofIn[j];
        else {                                  // the inverse word of the roof of s_j^-1
            const vector<int>& r = roofIn[L.inv[j]];
            for (int i = (int)r.size() - 1; i >= 0; i--) sd.roof[j].push_back(L.inv[r[i]]);
        }
    }
    M6 ti = m6inv(t);
    int bad = 0;
    for (size_t j = 0; j < n; j++) {
        M6 p = m6id();
        for (int x : sd.roof[j]) p = m6mul(p, L.s[x]);
        if (!m6eq(p, m6mul(m6mul(ti, L.s[j]), t))) bad++;
    }
    if (bad) { fprintf(stderr, "%s: %d roofs fail the exact check\n", name, bad); exit(1); }
    sd.vfix = attracting6(t);
    sd.V = spoint5(sd.vfix);
    sd.off.assign(n, {}); sd.keyoff.assign(n, {}); sd.wv.assign(n, {}); sd.ev.assign(n, {});
    for (size_t j = 0; j < n; j++) {
        M6 u = m6id();
        for (int x : sd.roof[j]) {
            M6 o = m6mul(t, u);
            sd.off[j].push_back(o);
            sd.keyoff[j].push_back(L.plus[x] ? o : m6mul(o, L.s[x]));
            sd.wv[j].push_back(mv6(toD6(o), sd.vfix));
            u = m6mul(u, L.s[x]);
        }
        sd.ev[j] = mv6(toD6(L.s[j]), sd.vfix);
    }
}

// ---------------------------------------------------------------------------------------
// The renderer

static inline uint64_t fingerprint6(const M6& g, int letter)
{
    uint64_t h = 0x9ae16a3b2f90404fULL ^ ((uint64_t)(letter + 1) * 0xff51afd7ed558ccdULL);
    for (int i = 0; i < 6; i++) h = splitmix(h ^ (uint64_t)g.e[i][5]);
    return h ? h : 1;
}

struct KeySet {
    vector<uint64_t> t;
    size_t n = 0;
    uint64_t mask = 0;
    KeySet() { t.assign(1 << 16, 0); mask = t.size() - 1; }
    bool contains(uint64_t k) const
    {
        for (uint64_t i = k & mask; t[i]; i = (i + 1) & mask) if (t[i] == k) return true;
        return false;
    }
    bool insert(uint64_t k)
    {
        if (2*(n + 1) > t.size()) {
            vector<uint64_t> old;
            old.swap(t);
            t.assign(2*old.size(), 0);
            mask = t.size() - 1;
            n = 0;
            for (uint64_t x : old) if (x) insert(x);
        }
        uint64_t i = k & mask;
        for (; t[i]; i = (i + 1) & mask) if (t[i] == k) return false;
        t[i] = k; n++;
        return true;
    }
};

struct Renderer5 {
    const Side5& sd;
    const Letters5& L;
    double acc;
    size_t leafcap;
    int maxdepth = 400;
    KeySet done;

    // polylines of points of S^4; polyline k is [starts[k], starts[k+1]) (in points); each
    // polyline also records the root it came from (rootof) and its base letter
    vector<float> pts;
    vector<uint32_t> starts;
    vector<int> polyroot, polyletter;
    int curroot = -1, curletter = -1;
    bool open = false, capped = false;

    size_t nVisited = 0, nSkipped = 0, nLeaves = 0, nSpike = 0, nForced = 0;
    int deepest = 0;

    struct Frame { M6 h; int j, depth, kind; uint64_t key; };
    vector<Frame> stk;

    Renderer5(const Side5& s, const Letters5& l, double a, size_t cap) : sd(s), L(l), acc(a), leafcap(cap) {}

    size_t npoly() const { return starts.size(); }
    size_t pbegin(size_t k) const { return starts[k]; }
    size_t pend(size_t k) const { return k + 1 < starts.size() ? starts[k+1] : pts.size()/5; }

    void push(const P5& p) { for (int i = 0; i < 5; i++) pts.push_back((float)p.x[i]); }
    void emit(const P5& a, const P5& b)
    {
        ++nLeaves;
        if (!open) {
            starts.push_back((uint32_t)(pts.size()/5));
            polyroot.push_back(curroot); polyletter.push_back(curletter);
            push(a);
            open = true;
        }
        push(b);
        if (nLeaves >= leafcap) capped = true;
    }

    void draw(const M6& g, int j, int rootindex = -1)
    {
        if (capped) return;
        open = false;
        curroot = rootindex; curletter = j;
        stk.clear();
        stk.push_back({g, j, 0, 0, fingerprint6(g, L.canon[j])});
        while (!stk.empty() && !capped) {
            Frame f = stk.back();
            stk.pop_back();
            if (f.kind == 1) { open = false; continue; }
            if (!done.insert(f.key)) { ++nSkipped; open = false; continue; }
            ++nVisited;
            visit(f);
        }
    }

    void visit(const Frame& f)
    {
        D6 hd = toD6(f.h);
        P5 a = spoint5(mv6(hd, sd.vfix)), b = spoint5(mv6(hd, sd.ev[f.j]));
        const vector<V6>& wv = sd.wv[f.j];
        bool split = d5(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segd5(spoint5(mv6(hd, wv[i])), a, b) > acc) { split = true; ++nSpike; break; }
        if (split && f.depth < maxdepth && m6maxcoef(f.h) < (1LL << 40)) {
            deepest = std::max(deepest, f.depth + 1);
            const vector<int>& roof = sd.roof[f.j];
            for (int i = (int)roof.size() - 1; i >= 0; i--) {
                int x = roof[i];
                uint64_t kc = fingerprint6(m6mul(f.h, sd.keyoff[f.j][i]), L.canon[x]);
                if (done.contains(kc)) { stk.push_back({f.h, 0, 0, 1, 0}); continue; }
                stk.push_back({m6mul(f.h, sd.off[f.j][i]), x, f.depth + 1, 0, kc});
            }
        } else {
            if (split) ++nForced;
            emit(a, b);
        }
    }
};

// ---------------------------------------------------------------------------------------
// The world: letters, the two sides, the roots (elements of Gamma in B(R), nearest first)

struct World5 {
    Letters5 L;
    Side5 plus, minus;
    vector<M6> roots;
    vector<double> rootd;
    Renderer5 *rp = nullptr, *rn = nullptr;
};

static void prepare5(const char* subst, World5& W)
{
    Subst5 D;
    if (!load_subst5(subst, D)) exit(1);
    size_t n = D.sword.size();
    for (auto& w : D.sword) W.L.s.push_back(word_matrix(parse_fword(w)));
    W.L.inv.assign(n, -1);
    for (size_t j = 0; j < n; j++) {
        M6 x = m6inv(W.L.s[j]);
        for (size_t k = 0; k < n; k++) if (m6eq(x, W.L.s[k])) W.L.inv[j] = (int)k;
        if (W.L.inv[j] < 0) { fprintf(stderr, "S is not closed under inversion\n"); exit(1); }
    }
    W.L.plus.assign(n, 0);
    W.L.canon.assign(n, 0);
    for (size_t j = 0; j < n; j++) {
        // an involution is its own inverse: it is its own representative
        W.L.plus[j] = (int)j <= W.L.inv[j];
        W.L.canon[j] = W.L.plus[j] ? (int)j : W.L.inv[j];
    }
    M6 t = word_matrix(parse_fword(D.tword));
    make_side5(W.plus, "Z+", t, D.up, W.L);
    make_side5(W.minus, "Z-", m6inv(t), D.down, W.L);
    size_t np = 0;
    for (size_t j = 0; j < n; j++) np += W.L.plus[j];
    fprintf(stderr, "%s: |S| = %zu (%zu base arcs), roofs checked exactly; t^+ and t^- found\n", subst, n, np);
}

static void find_roots5(double R, World5& W)
{
    W.roots.clear(); W.rootd.clear();
    if (R <= 0) { W.roots.push_back(m6id()); W.rootd.push_back(0); return; }
    guard6(R + 0.5);
    Store6 B;
    enumerate6(B, R + 0.5);
    vector<int> idx;
    for (size_t i = 0; i < B.size(); i++) if (B.mu[i] == 0 && B.dist[i] <= R) idx.push_back((int)i);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return B.dist[a] < B.dist[b]; });
    for (int i : idx) { W.roots.push_back(B.load(i)); W.rootd.push_back(B.dist[i]); }
}

static void build5(World5& W, double R, double acc, size_t cap, int which = 3)
{
    double t0 = now();
    find_roots5(R, W);
    delete W.rp; delete W.rn;
    W.rp = new Renderer5(W.plus, W.L, acc, cap);
    W.rn = new Renderer5(W.minus, W.L, acc, cap);
    for (size_t r = 0; r < W.roots.size(); r++)
        for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) {
            if (which & 1) W.rp->draw(W.roots[r], (int)j, (int)r);
            if (which & 2) W.rn->draw(W.roots[r], (int)j, (int)r);
        }
    for (Renderer5* r : {W.rp, W.rn}) {
        if (r->nVisited == 0) continue;
        fprintf(stderr, "%s: R=%.2f acc=%.4g: %zu roots, %zu branches, %zu skipped, %zu leaves, %zu polylines,"
                " %zu spikes, %zu forced, depth %d%s\n",
                r->sd.name, R, acc, W.roots.size(), r->nVisited, r->nSkipped, r->nLeaves, r->npoly(),
                r->nSpike, r->nForced, r->deepest, r->capped ? "  [LEAF CAP REACHED: picture truncated]" : "");
    }
    fprintf(stderr, "built in %.2fs\n", now() - t0);
}

// ---------------------------------------------------------------------------------------
// Views of S^4.  A frame Q of R^5 (rows E1..E4, P): stereographic projection from P gives
// s in R^4.  Then a rotation U of R^4: the first three coordinates of U s are shown (with a
// rotation of R^3 and the usual fog), and the fourth, the hidden coordinate, is a second fog.

struct View5 {
    double Q[5][5];
    double U[4][4];
    double rot[9];
    double zoom = 1, clip = 3, hidfog = 1.5;
};

static void orthonormal_frame(const double p[5], double F[5][5])        // rows E1..E4, then p
{
    double B[5][5];
    for (int k = 0; k < 5; k++) B[0][k] = p[k];
    int r = 1;
    for (int e = 0; e < 5 && r < 5; e++) {
        double v[5] = {0,0,0,0,0};
        v[e] = 1;
        for (int i = 0; i < r; i++) { double d = 0; for (int k = 0; k < 5; k++) d += v[k]*B[i][k]; for (int k = 0; k < 5; k++) v[k] -= d*B[i][k]; }
        double l = 0;
        for (int k = 0; k < 5; k++) l += v[k]*v[k];
        l = std::sqrt(l);
        if (l < 0.3) continue;
        for (int k = 0; k < 5; k++) B[r][k] = v[k]/l;
        r++;
    }
    for (int i = 0; i < 4; i++) for (int k = 0; k < 5; k++) F[i][k] = B[i + 1][k];
    for (int k = 0; k < 5; k++) F[4][k] = B[0][k];
}

static void rot3(double yaw, double pitch, double R[9])
{
    double a = yaw*PI/180, b = pitch*PI/180;
    double Y[9] = {std::cos(a),0,std::sin(a), 0,1,0, -std::sin(a),0,std::cos(a)};
    double X[9] = {1,0,0, 0,std::cos(b),-std::sin(b), 0,std::sin(b),std::cos(b)};
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { R[3*i+j] = 0; for (int k = 0; k < 3; k++) R[3*i+j] += X[3*i+k]*Y[3*k+j]; }
}

// rotate R^4 in the plane of coordinates (i, j) by th: U <- G U
static void turn4(double U[4][4], int i, int j, double th)
{
    double c = std::cos(th), s = std::sin(th);
    for (int k = 0; k < 4; k++) {
        double a = U[i][k], b = U[j][k];
        U[i][k] = c*a - s*b;
        U[j][k] = s*a + c*b;
    }
}

// move the pole P toward the frame direction E_a (a = 0..3) by th: a rotation of S^4
static void move_pole5(double Q[5][5], int a, double th)
{
    double c = std::cos(th), s = std::sin(th);
    for (int k = 0; k < 5; k++) {
        double e = Q[a][k], p = Q[4][k];
        Q[a][k] = c*e - s*p;
        Q[4][k] = s*e + c*p;
    }
}

// a point of S^4 -> eye coordinates (3) and the hidden coordinate; false outside the clip ball
static inline bool eye5(const View5& v, const float* x, double e[3], double& hid)
{
    double y[5];
    for (int r = 0; r < 5; r++) { double s = 0; for (int k = 0; k < 5; k++) s += v.Q[r][k]*x[k]; y[r] = s; }
    double den = 1 - y[4];
    if (den <= 1e-9) return false;
    double s4[4] = {y[0]/den, y[1]/den, y[2]/den, y[3]/den};
    if (s4[0]*s4[0] + s4[1]*s4[1] + s4[2]*s4[2] + s4[3]*s4[3] > v.clip*v.clip) return false;
    double u[4];
    for (int r = 0; r < 4; r++) u[r] = v.U[r][0]*s4[0] + v.U[r][1]*s4[1] + v.U[r][2]*s4[2] + v.U[r][3]*s4[3];
    for (int i = 0; i < 3; i++) e[i] = v.rot[3*i]*u[0] + v.rot[3*i+1]*u[1] + v.rot[3*i+2]*u[2];
    hid = u[3];
    return true;
}

static void init_view5(const World5& W, View5& v, double yaw = 0, double pitch = 15)
{
    // the pole opposite t^+: t^+ is at the centre of the picture
    double p[5];
    for (int k = 0; k < 5; k++) p[k] = -W.plus.V.x[k];
    orthonormal_frame(p, v.Q);
    memset(v.U, 0, sizeof v.U);
    for (int i = 0; i < 4; i++) v.U[i][i] = 1;
    rot3(yaw, pitch, v.rot);
}

// ---------------------------------------------------------------------------------------
// PNG output (software rendering): far lines fade toward white, and so do lines far out along
// the hidden direction

static void png_chunk(FILE* f, const char* type, const uint8_t* data, size_t len)
{
    uint8_t hdr[4] = { (uint8_t)(len>>24), (uint8_t)(len>>16), (uint8_t)(len>>8), (uint8_t)len };
    fwrite(hdr, 1, 4, f); fwrite(type, 1, 4, f); if (len) fwrite(data, 1, len, f);
    uLong crc = crc32(0, (const Bytef*)type, 4);
    if (len) crc = crc32(crc, (const Bytef*)data, (uInt)len);
    uint8_t cb[4] = { (uint8_t)(crc>>24), (uint8_t)(crc>>16), (uint8_t)(crc>>8), (uint8_t)crc };
    fwrite(cb, 1, 4, f);
}

static bool write_png_rgb(const char* fn, const vector<uint8_t>& rgb, int W, int H, int level = 6)
{
    vector<uint8_t> raw;
    raw.reserve((size_t)H*(1 + W*3));
    for (int y = 0; y < H; ++y) { raw.push_back(0); const uint8_t* row = &rgb[(size_t)y*W*3]; raw.insert(raw.end(), row, row + W*3); }
    uLongf clen = compressBound((uLong)raw.size());
    vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), (uLong)raw.size(), level) != Z_OK) return false;
    FILE* f = fopen(fn, "wb");
    if (!f) return false;
    const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = { (uint8_t)(W>>24), (uint8_t)(W>>16), (uint8_t)(W>>8), (uint8_t)W,
                         (uint8_t)(H>>24), (uint8_t)(H>>16), (uint8_t)(H>>8), (uint8_t)H, 8, 2, 0, 0, 0 };
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", comp.data(), clen);
    png_chunk(f, "IEND", nullptr, 0);
    fclose(f);
    return true;
}

// all the leaves of the branch (h, j) in order (no de-duplication, so the arc is complete)
static inline void arc_points5(const Side5& sd, const M6& h0, int j0, double acc, vector<P5>& pts)
{
    struct F { M6 h; int j, depth; };
    vector<F> st;
    st.push_back({h0, j0, 0});
    while (!st.empty()) {
        F f = st.back();
        st.pop_back();
        D6 hd = toD6(f.h);
        P5 a = spoint5(mv6(hd, sd.vfix)), b = spoint5(mv6(hd, sd.ev[f.j]));
        const vector<V6>& wv = sd.wv[f.j];
        bool split = d5(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++) if (segd5(spoint5(mv6(hd, wv[i])), a, b) > acc) { split = true; break; }
        if (split && f.depth < 400 && m6maxcoef(f.h) < (1LL << 40)) {
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({m6mul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1});
        } else {
            if (pts.empty()) pts.push_back(a);
            pts.push_back(b);
        }
    }
}
