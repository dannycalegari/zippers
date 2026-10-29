/*  zipcore.h  --  the zipper arcs of the Cartwright-Steger surface: substitution data, the
    adaptive renderer, views, and PNG output.  Shared by zipper3.cc (renderer and viewer)
    and link3.cc (linking numbers).  See the comment at the top of zipper3.cc.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#pragma once

#include "csgroup.h"
#include <sstream>
#include <fstream>
#include <random>
#include <unordered_map>
#include <zlib.h>

typedef std::array<cpx,3> C3;
typedef std::array<std::array<cpx,3>,3> D3;
struct P4 { double x[4]; };

static const double SQ3 = std::sqrt(3.0);
static const double PI = 3.14159265358979323846;

static D3 toD(const M3& g)
{
    D3 m;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = zval(g.e[i][j]);
    return m;
}

static inline C3 mv(const D3& m, const C3& v)
{
    C3 r;
    for (int i = 0; i < 3; i++) r[i] = m[i][0]*v[0] + m[i][1]*v[1] + m[i][2]*v[2];
    return r;
}

// a null vector in primed coordinates -> its point of S^3 (unit ball coordinates, see ballpoint)
static inline P4 spoint(const C3& c)
{
    cpx w1 = c[0], w2 = c[0] + (1 - SQ3)*c[1], w3 = std::sqrt(SQ3 - 1)*c[2];
    cpx z1 = w1/w3, z2 = w2/w3;
    P4 p = {{z1.real(), z1.imag(), z2.real(), z2.imag()}};
    double n = std::sqrt(p.x[0]*p.x[0] + p.x[1]*p.x[1] + p.x[2]*p.x[2] + p.x[3]*p.x[3]);
    for (int i = 0; i < 4; i++) p.x[i] /= n;
    return p;
}

static inline double d4(const P4& a, const P4& b)
{
    double s = 0;
    for (int i = 0; i < 4; i++) s += (a.x[i]-b.x[i])*(a.x[i]-b.x[i]);
    return std::sqrt(s);
}

static inline double segd4(const P4& p, const P4& a, const P4& b)        // point to segment, R^4
{
    double u[4], w[4], uu = 0, uw = 0;
    for (int i = 0; i < 4; i++) { u[i] = b.x[i]-a.x[i]; w[i] = p.x[i]-a.x[i]; uu += u[i]*u[i]; uw += u[i]*w[i]; }
    double t = uu > 0 ? std::max(0.0, std::min(1.0, uw/uu)) : 0;
    double s = 0;
    for (int i = 0; i < 4; i++) { double e = w[i] - t*u[i]; s += e*e; }
    return std::sqrt(s);
}

static i64 maxcoef(const M3& g)
{
    i64 m = 0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int c = 0; c < 4; c++) m = std::max<i64>(m, std::llabs(g.e[i][j].c[c]));
    return m;
}

// the attracting fixed point of a loxodromic element: its dominant eigenvector (power iteration)
static C3 attracting(const M3& t)
{
    D3 m = toD(t);
    C3 v = {cpx(0.3,0.1), cpx(-0.2,0.5), cpx(1.0,0.0)};
    for (int it = 0; it < 600; it++) {
        v = mv(m, v);
        double n = std::sqrt(std::norm(v[0]) + std::norm(v[1]) + std::norm(v[2]));
        for (auto& x : v) x /= n;
    }
    return v;
}

// ---------------------------------------------------------------------------------------
// The substitution data (a file written by `chz kernel`)

static M3 parse_word(const string& w)
{
    M3 g = mid();
    std::istringstream is(w);
    string tok;
    while (is >> tok) {
        if (tok == "1") continue;
        if (tok.size() != 2 || (tok[0] != 'a' && tok[0] != 'A') || tok[1] < '1' || tok[1] > '3') {
            fprintf(stderr, "bad letter '%s' in a word\n", tok.c_str()); exit(1);
        }
        int i = tok[1] - '1';
        g = mmul(g, tok[0] == 'a' ? A[i] : minv(A[i]));
    }
    return mnormal(g);
}

struct Subst {
    string tword;
    vector<string> sword;
    vector<vector<int>> up, down;
};

static bool load_subst(const char* fn, Subst& D)
{
    std::ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot open %s\n", fn); return false; }
    string line;
    int mode = 0;                      // 1: S lines, 2: up, 3: down, 4: member
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
        if (h == "member") { mode = 4; continue; }
        if (mode == 1) {               // "j  d=...  f2=...  word"
            string d, f2, w, tok;
            is >> d >> f2;
            while (is >> tok) { if (!w.empty()) w += " "; w += tok; }
            D.sword.push_back(w);
        } else if (mode == 2 || mode == 3) {
            int j = atoi(h.c_str());
            int x;
            vector<int> r;
            while (is >> x) r.push_back(x);
            (mode == 2 ? D.up : D.down)[j] = r;
        }
    }
    if (D.tword.empty() || D.sword.size() != n || D.up.size() != n || D.down.size() != n) {
        fprintf(stderr, "%s: incomplete substitution file\n", fn); return false;
    }
    return true;
}

struct Letters {
    vector<M3> s;            // the generators, exact and normalised
    vector<int> inv;         // index of the inverse
    vector<char> plus;       // representative of its inverse pair
    vector<int> canon;       // the letter of the canonical key of a branch (h, j)
};

struct Side {                // Z+ (t, up-roofs) or Z- (t^-1, down-roofs)
    const char* name;
    M3 t;
    C3 vfix;                         // the attracting fixed point of t, primed coordinates
    P4 V;                            // the same point of S^3
    vector<vector<int>> roof;        // theta(j)
    vector<vector<M3>> off;          // the bases t u_i of the children
    vector<vector<M3>> keyoff;       // t u_i, or t u_i s_{x_i} if x_i is not in S+: the key bases
    vector<vector<C3>> wv;           // t u_i . vfix: the vertices of the first split
    vector<C3> ev;                   // s_j . vfix: the end of alpha_j
};

static bool same(const M3& a, const M3& b) { return mscalar(mmul(a, minv(b))) >= 0; }

static void make_side(Side& sd, const char* name, const M3& t, const vector<vector<int>>& roofIn, const Letters& L)
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
    // check every roof exactly: t^-1 s_j t = x_1 ... x_k
    M3 ti = minv(t);
    int bad = 0;
    for (size_t j = 0; j < n; j++) {
        M3 p = mid();
        for (int x : sd.roof[j]) p = mmul(p, L.s[x]);
        if (!same(p, mmul(mmul(ti, L.s[j]), t))) bad++;
    }
    if (bad) { fprintf(stderr, "%s: %d roofs fail the exact check\n", name, bad); exit(1); }

    sd.vfix = attracting(t);
    sd.V = spoint(sd.vfix);
    sd.off.assign(n, {}); sd.keyoff.assign(n, {}); sd.wv.assign(n, {}); sd.ev.assign(n, {});
    for (size_t j = 0; j < n; j++) {
        M3 u = mid();
        for (int x : sd.roof[j]) {
            M3 o = mnormal(mmul(t, u));
            sd.off[j].push_back(o);
            sd.keyoff[j].push_back(L.plus[x] ? o : mnormal(mmul(o, L.s[x])));
            sd.wv[j].push_back(mv(toD(o), sd.vfix));
            u = mmul(u, L.s[x]);
        }
        sd.ev[j] = mv(toD(L.s[j]), sd.vfix);
    }
}

// ---------------------------------------------------------------------------------------
// The renderer

static inline uint64_t fingerprint(const i64* col, int letter)
{
    uint64_t h = 0x9ae16a3b2f90404fULL ^ ((uint64_t)(letter + 1) * 0xff51afd7ed558ccdULL);
    for (int i = 0; i < 12; i++) h = splitmix(h ^ (uint64_t)col[i]);
    return h ? h : 1;
}

struct KeySet {                      // open addressing, 0 = empty
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

struct Renderer {
    const Side& sd;
    const Letters& L;
    double acc;
    size_t leafcap;
    int maxdepth = 400;
    KeySet done;

    // output: polylines of points of S^3; polyline k is [starts[k], starts[k+1]) (in points)
    vector<float> pts;
    vector<uint32_t> starts;
    bool open = false, capped = false;

    size_t nVisited = 0, nSkipped = 0, nLeaves = 0, nSpike = 0, nForced = 0;
    int deepest = 0;

    struct Frame { M3 h; int j, depth, kind; uint64_t key; };
    vector<Frame> stk;

    Renderer(const Side& s, const Letters& l, double a, size_t cap) : sd(s), L(l), acc(a), leafcap(cap) {}

    size_t npoly() const { return starts.size(); }
    size_t pbegin(size_t k) const { return starts[k]; }
    size_t pend(size_t k) const { return k + 1 < starts.size() ? starts[k+1] : pts.size()/4; }

    void push(const P4& p) { for (int i = 0; i < 4; i++) pts.push_back((float)p.x[i]); }
    void emit(const P4& a, const P4& b)
    {
        ++nLeaves;
        if (!open) { starts.push_back((uint32_t)(pts.size()/4)); push(a); open = true; }
        push(b);
        if (nLeaves >= leafcap) capped = true;
    }

    void draw(const M3& g, int j)
    {
        if (capped) return;
        open = false;
        i64 key[12];
        Zz col[3];
        prodkey(g, mid(), key, col);
        stk.clear();
        stk.push_back({g, j, 0, 0, fingerprint(key, L.canon[j])});
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
        D3 hd = toD(f.h);
        P4 a = spoint(mv(hd, sd.vfix)), b = spoint(mv(hd, sd.ev[f.j]));
        const vector<C3>& wv = sd.wv[f.j];
        bool split = d4(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++)
                if (segd4(spoint(mv(hd, wv[i])), a, b) > acc) { split = true; ++nSpike; break; }
        if (split && f.depth < maxdepth && maxcoef(f.h) < (1LL << 40)) {
            deepest = std::max(deepest, f.depth + 1);
            const vector<int>& roof = sd.roof[f.j];
            i64 key[12];
            Zz col[3];
            for (int i = (int)roof.size() - 1; i >= 0; i--) {
                int x = roof[i];
                prodkey(f.h, sd.keyoff[f.j][i], key, col);
                uint64_t kc = fingerprint(key, L.canon[x]);
                if (done.contains(kc)) { stk.push_back({f.h, 0, 0, 1, 0}); continue; }
                stk.push_back({mmul(f.h, sd.off[f.j][i]), x, f.depth + 1, 0, kc});
            }
        } else {
            if (split) ++nForced;
            emit(a, b);
        }
    }
};

// ---------------------------------------------------------------------------------------
// Views: a 4D frame Q (rows E1, E2, E3, P; stereographic projection from P) followed by a
// rotation of R^3, an orthographic projection, a zoom, and a clip ball.

struct ViewState {
    double Q[4][4];
    double rot[9];
    double zoom = 1, clip = 3;
};

static void default_frame(const P4& V, double Q[4][4])
{
    cpx v1(V.x[0], V.x[1]), v2(V.x[2], V.x[3]);
    cpx w1 = -std::conj(v2), w2 = std::conj(v1);                // complex-orthogonal to V
    cpx I(0, 1);
    cpx rows[4][2] = { {w1, w2}, {I*w1, I*w2}, {I*v1, I*v2}, {-v1, -v2} };
    for (int r = 0; r < 4; r++) {
        Q[r][0] = rows[r][0].real(); Q[r][1] = rows[r][0].imag();
        Q[r][2] = rows[r][1].real(); Q[r][3] = rows[r][1].imag();
    }
    // keep the orientation: det Q = +1
    double m[4][4];
    memcpy(m, Q, sizeof m);
    double det = 1;
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (std::abs(m[r][c]) > std::abs(m[p][c])) p = r;
        if (p != c) { for (int k = 0; k < 4; k++) std::swap(m[p][k], m[c][k]); det = -det; }
        det *= m[c][c];
        for (int r = c + 1; r < 4; r++) { double f = m[r][c]/m[c][c]; for (int k = c; k < 4; k++) m[r][k] -= f*m[c][k]; }
    }
    if (det < 0) for (int k = 0; k < 4; k++) Q[2][k] = -Q[2][k];
}

static void rot3(double yaw, double pitch, double R[9])
{
    // the Reeb axis E3 up the screen, then yaw about the vertical and pitch about the horizontal
    double a = yaw*PI/180, b = pitch*PI/180;
    double base[9] = {1,0,0, 0,0,1, 0,-1,0};                        // E1 -> x, E3 -> y, E2 -> -z
    double Y[9] = {std::cos(a),0,std::sin(a), 0,1,0, -std::sin(a),0,std::cos(a)};
    double X[9] = {1,0,0, 0,std::cos(b),-std::sin(b), 0,std::sin(b),std::cos(b)};
    double T[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { T[3*i+j] = 0; for (int k = 0; k < 3; k++) T[3*i+j] += Y[3*i+k]*base[3*k+j]; }
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { R[3*i+j] = 0; for (int k = 0; k < 3; k++) R[3*i+j] += X[3*i+k]*T[3*k+j]; }
}

// rotate the pole P toward the frame direction a (a unit 3-vector in E-coordinates) by angle
// th: the "4D rotation" that moves the centre of the picture along a
static void move_pole(double Q[4][4], const double a[3], double th)
{
    double A4[4] = {0,0,0,0};
    for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) A4[k] += a[i]*Q[i][k];
    double P[4];
    for (int k = 0; k < 4; k++) P[k] = Q[3][k];
    double c = std::cos(th), s = std::sin(th);
    // in the frame, e4 -> c e4 + s a and a -> -s e4 + c a; the rows transform accordingly
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 4; k++) Q[i][k] += a[i]*((c - 1)*A4[k] - s*P[k]);
    for (int k = 0; k < 4; k++) Q[3][k] = c*P[k] + s*A4[k];
}

// project a point of S^3 to eye coordinates; false if it is outside the clip ball
static inline bool eye(const ViewState& v, const float* x, double e[3])
{
    double y[4];
    for (int r = 0; r < 4; r++) y[r] = v.Q[r][0]*x[0] + v.Q[r][1]*x[1] + v.Q[r][2]*x[2] + v.Q[r][3]*x[3];
    double den = 1 - y[3];
    if (den <= 1e-9) return false;
    double s[3] = {y[0]/den, y[1]/den, y[2]/den};
    if (s[0]*s[0] + s[1]*s[1] + s[2]*s[2] > v.clip*v.clip) return false;
    for (int i = 0; i < 3; i++) e[i] = v.rot[3*i]*s[0] + v.rot[3*i+1]*s[1] + v.rot[3*i+2]*s[2];
    return true;
}

// ---------------------------------------------------------------------------------------
// PNG output (software rendering, depth-cued: far lines fade toward the white background)

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

static void ras_line(vector<uint8_t>& img, int W, int H, int x0, int y0, int x1, int y1, const uint8_t c[3], int th = 0)
{
    int dx = std::abs(x1-x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1-y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        for (int dy = -th; dy <= th; dy++) for (int dx = -th; dx <= th; dx++) {     // a square brush
            int x = x0 + dx, y = y0 + dy;
            if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
                uint8_t* p = &img[((size_t)y*W + x)*3];
                p[0] = c[0]; p[1] = c[1]; p[2] = c[2];
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

struct Layer { const Renderer* r; double col[3]; int prio = 1; };     // lower prio is drawn first, underneath

static inline bool write_png(const char* fn, int size, const vector<Layer>& layers, const ViewState& v, const vector<P4>& marks, int thick = 0)
{
    const int SS = 2, w = size*SS, h = size*SS;
    vector<uint8_t> img((size_t)w*h*3, 255);
    double half = v.clip/v.zoom, scale = (w/2.0)/half;
    struct Seg { float x0, y0, x1, y1, z; uint8_t c[3]; int prio; };
    vector<Seg> segs;
    for (const Layer& L : layers) {
        const Renderer& r = *L.r;
        for (size_t k = 0; k < r.npoly(); k++) {
            double e0[3], e1[3];
            bool ok0 = eye(v, &r.pts[4*r.pbegin(k)], e0);
            for (size_t i = r.pbegin(k) + 1; i < r.pend(k); i++) {
                bool ok1 = eye(v, &r.pts[4*i], e1);
                if (ok0 && ok1) {
                    double z = 0.5*(e0[2] + e1[2]);
                    double f = 0.12 + 0.88*std::max(0.0, std::min(1.0, (z + v.clip)/(2*v.clip)));
                    Seg s = {(float)(w/2.0 + e0[0]*scale), (float)(h/2.0 - e0[1]*scale),
                             (float)(w/2.0 + e1[0]*scale), (float)(h/2.0 - e1[1]*scale), (float)z, {0,0,0}, L.prio};
                    for (int c = 0; c < 3; c++) s.c[c] = (uint8_t)std::lround(255*(1 - f + f*L.col[c]));
                    segs.push_back(s);
                }
                memcpy(e0, e1, sizeof e0);
                ok0 = ok1;
            }
        }
    }
    std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b) { return a.prio != b.prio ? a.prio < b.prio : a.z < b.z; });   // far first
    for (const Seg& s : segs)
        ras_line(img, w, h, (int)std::lround(s.x0), (int)std::lround(s.y0), (int)std::lround(s.x1), (int)std::lround(s.y1), s.c, thick);
    // marked points (the fixed points t^+-): small black crosses
    for (const P4& p : marks) {
        float x[4] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3]};
        double e[3];
        if (!eye(v, x, e)) continue;
        int cx = (int)std::lround(w/2.0 + e[0]*scale), cy = (int)std::lround(h/2.0 - e[1]*scale);
        const uint8_t blk[3] = {0,0,0};
        ras_line(img, w, h, cx - 4*SS, cy, cx + 4*SS, cy, blk);
        ras_line(img, w, h, cx, cy - 4*SS, cx, cy + 4*SS, blk);
    }
    vector<uint8_t> out((size_t)size*size*3);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) for (int c = 0; c < 3; ++c) {
        int sum = 0;
        for (int dy = 0; dy < SS; ++dy) for (int dx = 0; dx < SS; ++dx) sum += img[(((size_t)(y*SS+dy))*w + (x*SS+dx))*3 + c];
        out[((size_t)y*size + x)*3 + c] = (uint8_t)(sum/(SS*SS));
    }
    return write_png_rgb(fn, out, size, size);
}

// ---------------------------------------------------------------------------------------

struct Options {
    string subst = "subst_4.00_9.50_t3.txt";
    double R = 0, acc = 0.01, clip = 3, zoom = 1, yaw = 0, pitch = 20, pole1 = 0, pole2 = 0;
    int size = 900;
    size_t cap = 8000000;
    int which = 3;                 // 1 = Z+, 2 = Z-, 3 = both
    string out;
    bool viewer = false, check = false;
    string snap;                   // viewer: save the first frame to this PNG and exit (testing)
    string pairs;                  // linked pairs (from `link3 --export`) to show instead of the zippers
    int pairindex = 0;
    string movie;                  // viewer: play the built-in script and save its frames here
    double moviescale = 1;         // scale the length of every segment (testing)
    int moviescript = 1;           // 1: the 58 s film; 2 and 3: two films to a recording of the fugue
    string moviefile;              // or a script file, one segment per line (see zipper3 --help)
    int moviestart = 0;            // the number of the first frame (frame_NNNNN.png)
    int thick = 0;                 // PNG: line thickness (half-width of the brush, in supersampled pixels)
    bool context = false;          // PNG of a linked pair: draw the base arcs faintly behind it
};

struct World {
    Letters L;
    Side plus, minus;
    vector<M3> roots;
    Renderer *rp = nullptr, *rn = nullptr;
};

static void prepare(const Options& opt, World& W)
{
    Subst D;
    if (!load_subst(opt.subst.c_str(), D)) exit(1);
    size_t n = D.sword.size();
    for (auto& w : D.sword) W.L.s.push_back(parse_word(w));
    W.L.inv.assign(n, -1);
    for (size_t j = 0; j < n; j++) {
        M3 x = mnormal(minv(W.L.s[j]));
        for (size_t k = 0; k < n; k++) if (meq(x, W.L.s[k])) W.L.inv[j] = k;
        if (W.L.inv[j] < 0) { fprintf(stderr, "S is not closed under inversion\n"); exit(1); }
    }
    W.L.plus.assign(n, 0);
    W.L.canon.assign(n, 0);
    for (size_t j = 0; j < n; j++) {
        W.L.plus[j] = (int)j < W.L.inv[j];
        W.L.canon[j] = W.L.plus[j] ? j : W.L.inv[j];
    }
    M3 t = parse_word(D.tword);
    make_side(W.plus, "Z+", t, D.up, W.L);
    make_side(W.minus, "Z-", mnormal(minv(t)), D.down, W.L);
    fprintf(stderr, "%s: |S| = %zu (%zu pairs), roofs checked exactly; t^+ and t^- found\n",
            opt.subst.c_str(), n, n/2);
}

// the distinct values of d(o, g o) up to rmax: the radii at which the set of roots changes
static inline vector<double> shell_radii(double rmax)
{
    vector<Gen> G = geogens(4.5);
    Store B;
    guard(rmax + 0.5);
    enumerate(B, G, rmax + 0.5);
    vector<double> d;
    for (size_t i = 0; i < B.size(); i++) if (B.dist[i] <= rmax) d.push_back(B.dist[i]);
    std::sort(d.begin(), d.end());
    vector<double> r;
    for (double x : d) if (r.empty() || x > r.back() + 1e-4) r.push_back(x);
    return r;
}

// the elements of B(R), nearest first, with their f-values and a-words
struct RootInfo { M3 g; double d; int f0, f1; string word; };

static inline vector<RootInfo> roots_info(double R)
{
    vector<Gen> G = geogens(4.5);
    Store B;
    guard(R + 0.5);
    enumerate(B, G, R + 0.5);
    vector<int> idx;
    for (size_t i = 0; i < B.size(); i++) if (B.dist[i] <= R) idx.push_back(i);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return B.dist[a] < B.dist[b]; });
    vector<RootInfo> r;
    for (int i : idx) r.push_back({B.load(i), B.dist[i], B.f0[i], B.f1[i], word(B, i, G)});
    return r;
}

static void find_roots(double R, vector<M3>& roots)
{
    roots.clear();
    for (const RootInfo& x : roots_info(R)) roots.push_back(x.g);
}

static inline void build(const Options& opt, World& W)
{
    double t0 = now();
    find_roots(opt.R, W.roots);
    delete W.rp; delete W.rn;
    W.rp = new Renderer(W.plus, W.L, opt.acc, opt.cap);
    W.rn = new Renderer(W.minus, W.L, opt.acc, opt.cap);
    for (const M3& g : W.roots)
        for (size_t j = 0; j < W.L.s.size(); j++) if (W.L.plus[j]) {
            if (opt.which & 1) W.rp->draw(g, j);
            if (opt.which & 2) W.rn->draw(g, j);
        }
    for (Renderer* r : {W.rp, W.rn}) {
        if (r->nVisited == 0) continue;
        fprintf(stderr, "%s: R=%.2f acc=%.4g: %zu roots, %zu branches, %zu skipped, %zu leaves, %zu polylines,"
                " %zu spikes, %zu forced, depth %d%s\n",
                r->sd.name, opt.R, opt.acc, W.roots.size(), r->nVisited, r->nSkipped, r->nLeaves, r->npoly(),
                r->nSpike, r->nForced, r->deepest, r->capped ? "  [LEAF CAP REACHED: picture truncated]" : "");
    }
    fprintf(stderr, "built in %.2fs\n", now() - t0);
}

static inline void init_view(const Options& opt, const World& W, ViewState& v)
{
    default_frame(W.plus.V, v.Q);
    double e1[3] = {1,0,0}, e2[3] = {0,1,0};
    move_pole(v.Q, e1, opt.pole1*PI/180);
    move_pole(v.Q, e2, opt.pole2*PI/180);
    rot3(opt.yaw, opt.pitch, v.rot);
    v.zoom = opt.zoom;
    v.clip = opt.clip;
}

// The smallest distance on S^3 (chord in R^4) between the drawn Z+ and the drawn Z-, from
// vertex to segment: the zippers are disjoint, so for a sound picture this must stay
// positive and converge as the accuracy is refined.  Only distances below `h` are searched.
static inline double zdist(const Renderer& a, const Renderer& b, double h, P4& wa, P4& wb)
{
    struct Cell { int64_t k; uint32_t i; };
    auto key = [&](const float* x) {
        int64_t k = 0;
        for (int c = 0; c < 4; c++) k = k*1000003 + (int64_t)std::floor((x[c] + 2)/h);
        return k;
    };
    // cells of b's segments, by their first vertex (segments are shorter than h)
    std::unordered_map<int64_t, vector<uint32_t>> grid;
    for (size_t k = 0; k < b.npoly(); k++)
        for (size_t i = b.pbegin(k); i + 1 < b.pend(k); i++) grid[key(&b.pts[4*i])].push_back(i);
    double best = h;
    for (size_t k = 0; k < a.npoly(); k++)
        for (size_t i = a.pbegin(k); i < a.pend(k); i++) {
            const float* x = &a.pts[4*i];
            int base[4];
            for (int c = 0; c < 4; c++) base[c] = (int)std::floor((x[c] + 2)/h);
            for (int m = 0; m < 81; m++) {
                int d[4] = {m % 3 - 1, (m/3) % 3 - 1, (m/9) % 3 - 1, (m/27) % 3 - 1};
                int64_t kk = 0;
                for (int c = 0; c < 4; c++) kk = kk*1000003 + base[c] + d[c];
                auto it = grid.find(kk);
                if (it == grid.end()) continue;
                for (uint32_t j : it->second) {
                    P4 p, q0, q1;
                    for (int c = 0; c < 4; c++) { p.x[c] = x[c]; q0.x[c] = b.pts[4*j + c]; q1.x[c] = b.pts[4*j + 4 + c]; }
                    double e = segd4(p, q0, q1);
                    if (e < best) { best = e; wa = p; wb = q0; }
                }
            }
        }
    return best;
}

// ---------------------------------------------------------------------------------------
// Loops: complete arcs (no de-duplication), and stereographic frames (used by link3 and
// by the viewer's pair mode)

typedef std::array<double,3> V3;

// stereographic projection from the pole p (a frame E1, E2, E3, p of R^4)
static inline vector<V3> stereo(const vector<P4>& X, const double F[4][4])
{
    vector<V3> Y(X.size());
    for (size_t i = 0; i < X.size(); i++) {
        double y[4];
        for (int r = 0; r < 4; r++) y[r] = F[r][0]*X[i].x[0] + F[r][1]*X[i].x[1] + F[r][2]*X[i].x[2] + F[r][3]*X[i].x[3];
        double den = 1 - y[3];
        Y[i] = {y[0]/den, y[1]/den, y[2]/den};
    }
    return Y;
}

// an orthonormal frame of R^4 with last row p
static inline void frame_with_pole(const double p[4], double F[4][4])
{
    double B[4][4];
    for (int k = 0; k < 4; k++) B[0][k] = p[k];
    int r = 1;
    for (int e = 0; e < 4 && r < 4; e++) {
        double v[4] = {0,0,0,0};
        v[e] = 1;
        for (int i = 0; i < r; i++) { double d = 0; for (int k = 0; k < 4; k++) d += v[k]*B[i][k]; for (int k = 0; k < 4; k++) v[k] -= d*B[i][k]; }
        double l = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3]);
        if (l < 0.3) continue;
        for (int k = 0; k < 4; k++) B[r][k] = v[k]/l;
        r++;
    }
    for (int k = 0; k < 4; k++) { F[0][k] = B[1][k]; F[1][k] = B[2][k]; F[2][k] = B[3][k]; F[3][k] = B[0][k]; }
    // keep the orientation: det F = +1.  Otherwise the projection to R^3 is a mirror image
    // for some poles, and every linking number changes sign.
    double m[4][4], det = 1;
    memcpy(m, F, sizeof m);
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (std::abs(m[r][c]) > std::abs(m[p][c])) p = r;
        if (p != c) { for (int k = 0; k < 4; k++) std::swap(m[p][k], m[c][k]); det = -det; }
        det *= m[c][c];
        for (int r = c + 1; r < 4; r++) { double f = m[r][c]/m[c][c]; for (int k = c; k < 4; k++) m[r][k] -= f*m[c][k]; }
    }
    if (det < 0) for (int k = 0; k < 4; k++) F[2][k] = -F[2][k];
}

// all the leaves of the branch (h, j) in order, appended to pts (the first point only if empty);
// no de-duplication, so the arc is complete
static inline void arc_points(const Side& sd, const M3& h0, int j0, double acc, vector<P4>& pts, size_t& forced)
{
    struct F { M3 h; int j, depth; };
    vector<F> st;
    st.push_back({h0, j0, 0});
    while (!st.empty()) {
        F f = st.back();
        st.pop_back();
        D3 hd = toD(f.h);
        P4 a = spoint(mv(hd, sd.vfix)), b = spoint(mv(hd, sd.ev[f.j]));
        const vector<C3>& wv = sd.wv[f.j];
        bool split = d4(a, b) > acc;
        if (!split)
            for (size_t i = 1; i < wv.size(); i++) if (segd4(spoint(mv(hd, wv[i])), a, b) > acc) { split = true; break; }
        if (split && f.depth < 400 && maxcoef(f.h) < (1LL << 40)) {
            for (int i = (int)sd.roof[f.j].size() - 1; i >= 0; i--)
                st.push_back({mmul(f.h, sd.off[f.j][i]), sd.roof[f.j][i], f.depth + 1});
        } else {
            if (split) forced++;
            if (pts.empty()) pts.push_back(a);
            pts.push_back(b);
        }
    }
}

// the loop h0 . (alpha_{j_1} u s_{j_1} alpha_{j_2} u ...) of a relation w, as a closed polyline
static inline vector<P4> loop_points(const Side& sd, const Letters& L, const M3& h0, const vector<int>& w, double acc, size_t& forced)
{
    vector<P4> pts;
    M3 g = h0;
    for (int j : w) { arc_points(sd, g, j, acc, pts, forced); g = mmul(g, L.s[j]); }
    if (!same(g, h0)) { fprintf(stderr, "a relation does not close up\n"); exit(1); }
    if (pts.size() > 1) pts.pop_back();          // the last point is the first one again
    return pts;
}

// A pole far from two curves on S^3.  Near the pole the image of a short chord is a huge arc,
// which a straight segment in R^3 misrepresents, so the distance from the pole to EVERY point
// must be large: 64 random candidates are ranked against subsamples, then checked in that order
// against all points until one is at least 0.15 away.  Returns that distance.
template <class RNG>
static double far_pole(const vector<P4>& A, const vector<P4>& B, RNG& rng, double pole[4])
{
    std::normal_distribution<double> N(0, 1);
    struct Cand { double sub; P4 p; };
    vector<Cand> cand;
    size_t sa = std::max<size_t>(1, A.size()/400), sb = std::max<size_t>(1, B.size()/400);
    for (int trial = 0; trial < 64; trial++) {
        P4 pp;
        double s = 0;
        for (double& x : pp.x) { x = N(rng); s += x*x; }
        for (double& x : pp.x) x /= std::sqrt(s);
        double mn = 9;
        for (size_t i = 0; i < A.size(); i += sa) mn = std::min(mn, d4(pp, A[i]));
        for (size_t i = 0; i < B.size(); i += sb) mn = std::min(mn, d4(pp, B[i]));
        cand.push_back({mn, pp});
    }
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.sub > b.sub; });
    double best = -1;
    for (const Cand& c : cand) {
        double mn = 9;
        for (const P4& q : A) mn = std::min(mn, d4(c.p, q));
        for (const P4& q : B) mn = std::min(mn, d4(c.p, q));
        if (mn > best) { best = mn; memcpy(pole, c.p.x, 4*sizeof(double)); }
        if (mn >= 0.15) break;
    }
    return best;
}
