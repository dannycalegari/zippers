/*  zipper.cc  --  draw zippers on the Riemann sphere for hyperbolic
    3-orbifolds fibered over the circle, fiber a torus with one orbifold point.

    Zippers are defined in Calegari-Loukidou, "Zippers", arXiv:2411.15610.

    This is a rewrite of the original `zcircle` program with four goals:
      * faster            -- POD 2x2 matrices, incremental matrix products;
      * more accurate      -- exact eigenvectors, robust Newton solver;
      * small .eps output   -- each branch drawn ONCE (exact branch identity),
                              emitted as integer relative-coordinate polylines;
      * nicer interface     -- choose monodromy and orbifold order on the command line.

    The holonomy is computed rather than hardcoded: for a fixed monodromy we know the
    cusped (n = infinity) character; the orbifold of index n is reached by analytic
    continuation (Newton) along the level set tr[a,b] = -2 cos(pi/n) of the character
    variety of the fiber.

    The zipper is the union of the translates g.alpha_y (y in {a,b}) of two base arcs.
    These arcs are the branches of the LIFTED TRAIN TRACK: splitting a branch once along
    the monodromy (substitution y -> phi(y)) replaces it by consecutive sub-branches, each
    again a translate h.alpha_x.  A branch is therefore identified EXACTLY by the pair
    (group element h, label x) -- never by its projected geometry.  The finite
    approximation (which branches, how deeply split) is necessarily not group invariant:
    roots are the branches of the elements in a hyperbolic ball about the basepoint, and
    each is split until it is below the visual resolution.  Each branch is drawn once, and
    polylines follow the splitting order, so they are built by construction.

    Copyright original zcircle: Danny Calegari 2024, GNU GPL.  Rewrite: same terms.

    apps/s2/zipper.cc (the zippers suite): the program of legacy/s2/zipper.cc in its
    batch build (legacy/s2/nogui/zipper, without ZIPPER_VIEWER), on the shared library src/,
    with the same options, usage text, messages and files.  Its PNG writer draws its lines
    with raster::line_index, downsamples with raster::downsample_palette_rows and writes the
    file with png::write_rows (src/output/), and the memory guard reads resident_bytes and
    calls release_memory of src/core/memory.h.  Everything else (the character-variety
    solver, the ball search, the renderer, the Catherine wheel, the EPS writer) is the legacy
    code, unchanged.  The window is not built: as in the legacy nogui build, -v says so and
    exits with status 1.  The film engine apps/s2/zmovie.cc
    includes this file, with main renamed.
*/

#include <complex>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <deque>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include "core/memory.h"          // current resident memory (the drawing's memory guard)
#include <cmath>
#include <cstdint>
#include <chrono>
#include "output/png.h"           // PNG output (and zlib)
#include "output/raster.h"

using std::string;
using std::vector;
typedef std::complex<double> cpx;
using zippers::resident_bytes;    // (src/core/memory.h; the film engine calls release_memory too)
using zippers::release_memory;
namespace png = zippers::png;
namespace raster = zippers::raster;

// hidden --timing: wall-clock time of each phase, on stderr
static bool g_timing = false;
// hidden --memlimit GB: the drawing memory limit (testing; 0 = the default 4 GB, 6 with --stats)
static double g_drawMem = 0;
static void tlog(const char* what) {
    static auto t0 = std::chrono::steady_clock::now();
    if (!g_timing) return;
    auto t = std::chrono::steady_clock::now();
    fprintf(stderr, "timing: %-28s %8.3f s\n", what, std::chrono::duration<double>(t - t0).count());
    t0 = t;
}

static const double PI = 3.14159265358979323846264338327950288;

// (resident_bytes and release_memory: src/core/memory.h)

struct Mat { cpx a, b, c, d; };           // Mobius transformation of CP^1
struct Vec { cpx u, v; };                 // a point [u:v] of CP^1

// complex products written out in reals (avoids the slow NaN/Inf-recovery path of
// std::complex multiplication; this is the hot loop)
static inline cpx cmuladd(const cpx& p, const cpx& q, const cpx& r, const cpx& s) {   // p q + r s
    return { p.real()*q.real() - p.imag()*q.imag() + r.real()*s.real() - r.imag()*s.imag(),
             p.real()*q.imag() + p.imag()*q.real() + r.real()*s.imag() + r.imag()*s.real() };
}
static inline Mat matmul(const Mat& M, const Mat& N) {
    return { cmuladd(M.a, N.a, M.b, N.c), cmuladd(M.a, N.b, M.b, N.d),
             cmuladd(M.c, N.a, M.d, N.c), cmuladd(M.c, N.b, M.d, N.d) };
}
static inline cpx tr(const Mat& M)  { return M.a + M.d; }
static inline cpx det(const Mat& M) { return M.a*M.d - M.b*M.c; }
static inline Mat matinv(const Mat& M) {
    cpx e = M.a*M.d - M.b*M.c;
    return { M.d/e, -M.b/e, -M.c/e, M.a/e };
}
static inline Mat identity() { return { {1,0},{0,0},{0,0},{1,0} }; }
static inline Vec act(const Mat& M, const Vec& p) {
    return { cmuladd(M.a, p.u, M.b, p.v), cmuladd(M.c, p.u, M.d, p.v) };
}

static inline double frob2(const Mat& M) {
    return std::norm(M.a) + std::norm(M.b) + std::norm(M.c) + std::norm(M.d);
}

// ------------------------------------------------------------------ exact group-element key
// The action of the (discrete, faithful) holonomy group on H^3 is free on a GENERIC point,
// so a group element is identified by the image of a generic basepoint o' (not j, which
// may lie near a torsion axis).  Orbit points of o' are a definite hyperbolic distance
// apart: min d(o', g o') over g != 1 is 0.79..0.99 for fig8 (n = 2..20) and RRL, but only
// 0.15..0.39 for thin words (R^10 L at n = 2, 3, 5, inf; R^14 L at n = 5), while the
// accumulated rounding error of a matrix product reached along different routes is
// ~1e-8..1e-7 (hyperbolic displacement), so quantising the image in hyperbolically-uniform
// boxes of size delta = 1e-2 never merges two distinct elements (the orbit separation is at
// least 15x the box in every test; --stats checks for false merges) and only very
// rarely splits one element into two keys (the error is ~1e5x smaller than the box; a split
// merely draws a branch twice).  Boxes are built in the BALL model (upper half-space has a
// special point at infinity): a radial bin of hyperbolic width delta, and an angular bin
// of hyperbolic width delta at that radius.  `salt` distinguishes e.g. branch labels.
// The key is a full 64-bit hash of the bins plus a coarse cell of the direction of the image
// (octahedral map of the sphere); KeySet uses the cell to store keys of nearby elements
// together (cache locality), so identity is decided by 64 hash bits + the cell.
struct HKey { uint64_t h; uint32_t cell; };
static inline uint64_t mix64(uint64_t x) {             // splitmix64 finaliser
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
static inline uint64_t dbits(double d) {               // bit pattern (canonical +0.0)
    d += 0.0; uint64_t u; std::memcpy(&u, &d, sizeof u); return u;
}
static const int KEY_CELL_BITS = 16;
static HKey hkey(const Mat& M, uint64_t salt = 0) {
    static const cpx    z0(0.3183098861837907, 0.2718281828459045);
    static const double t0 = 1.1414213562373095, t02 = t0*t0, delta = 1e-2;
    // image (z,t) of o' = (z0,t0) in upper half-space:
    //   w = c z0 + d,  D = |w|^2 + |c|^2 t0^2,  z = ((a z0 + b) conj(w) + a conj(c) t0^2) / D
    const double ar = M.a.real(), ai = M.a.imag(), br = M.b.real(), bi = M.b.imag();
    const double cr = M.c.real(), ci = M.c.imag(), dr = M.d.real(), di = M.d.imag();
    const double zr = z0.real(), zi = z0.imag();
    double wr = cr*zr - ci*zi + dr, wi = cr*zi + ci*zr + di;
    double vr = ar*zr - ai*zi + br, vi = ar*zi + ai*zr + bi;          // a z0 + b
    double D = wr*wr + wi*wi + (cr*cr + ci*ci)*t02, iD = 1.0/D;
    double x = (vr*wr + vi*wi + (ar*cr + ai*ci)*t02)*iD;             // v conj(w) + a conj(c) t0^2
    double y = (vi*wr - vr*wi + (ai*cr - ar*ci)*t02)*iD;
    double t = t0*iD;
    // to the unit ball: Bv = (2x, 2y, x^2+y^2+t^2-1) / (x^2+y^2+(t+1)^2)
    double rho2 = x*x + y*y, D2 = rho2 + (t+1.0)*(t+1.0);
    double bx = 2.0*x/D2, by = 2.0*y/D2, bz = (rho2 + t*t - 1.0)/D2;
    double r = std::sqrt(bx*bx + by*by + bz*bz);
    double omr2 = 4.0*t/D2;                            // 1 - r^2, without cancellation
    double dist = std::log((1.0 + r)*(1.0 + r)/omr2);  // hyperbolic distance from centre
    double K = std::floor(dist/delta), X = 0, Y = 0, Zc = 0;
    if (r > 1e-12) {
        // angular step at the bin's reference radius rK = tanh(K delta/2):
        // alpha = delta (1 - rK^2) / (2 rK); tabulated (as 1/alpha) for speed
        static vector<double> inv_alpha;
        auto ia = [](double k) {
            double rK = std::tanh(k*delta/2.0), ch = std::cosh(k*delta/2.0);
            return 1.0 / (delta/(ch*ch)/(2.0*std::max(rK, 1e-12)));
        };
        if (inv_alpha.empty()) { inv_alpha.resize(60000); for (int k = 0; k < 60000; ++k) inv_alpha[k] = ia(k); }
        double s = ((K >= 0 && K < 60000) ? inv_alpha[(size_t)K] : ia(K)) / r;
        X = std::floor(bx*s); Y = std::floor(by*s); Zc = std::floor(bz*s);
    }
    uint32_t cell = 0;
    if (r > 1e-12) {                                   // octahedral map of the direction
        double s = std::abs(bx) + std::abs(by) + std::abs(bz), p = bx/s, q = by/s;
        if (bz < 0) { double p0 = p;
                      p = (1.0 - std::abs(q)) * (p0 >= 0 ? 1 : -1);
                      q = (1.0 - std::abs(p0)) * (q >= 0 ? 1 : -1); }
        const int G = 1 << (KEY_CELL_BITS/2);
        int i = std::min(G-1, std::max(0, (int)std::floor((p + 1.0)*0.5*G)));
        int j = std::min(G-1, std::max(0, (int)std::floor((q + 1.0)*0.5*G)));
        cell = (uint32_t)(i*G + j);
    }
    uint64_t h = mix64(dbits(K));
    h = mix64(h ^ dbits(X)); h = mix64(h ^ dbits(Y)); h = mix64(h ^ dbits(Zc));
    h = mix64(h ^ salt);
    return { h ? h : 1, cell };                        // 0 marks an empty KeySet slot
}
static inline uint64_t flatkey(const HKey& k) { return k.h ^ mix64(k.cell); }   // --stats maps

// attracting (dominant) fixed point of a loxodromic Mobius transformation
static Vec attracting_fixed_point(const Mat& M) {
    cpx aa = M.a, bb = M.b, cc = M.c, dd = M.d;
    if (std::abs(cc) < 1e-14) {
        return (std::abs(aa) >= std::abs(dd)) ? Vec{{1,0},{0,0}} : Vec{{0,0},{1,0}};
    }
    cpx disc = std::sqrt((aa - dd)*(aa - dd) + 4.0*bb*cc);
    cpx z1 = (aa - dd + disc) / (2.0*cc);
    cpx z2 = (aa - dd - disc) / (2.0*cc);
    cpx l1 = cc*z1 + dd, l2 = cc*z2 + dd;
    cpx z = (std::abs(l1) >= std::abs(l2)) ? z1 : z2;
    return { z, {1,0} };
}

// Hash set of the keys made by hkey: one small open-addressing table of 64-bit hashes per
// coarse direction cell, so that the lookups of a depth-first traversal, which stay in one
// region of the sphere, hit the cache.  Faster and smaller than std::unordered_set for the
// tens of millions of branch keys.  The cell array is allocated on the first insert.  The
// tables (8 * 2^c slots) come from mapped slabs with a free list per size, and the slabs are
// unmapped when the set is destroyed: the system allocator kept the freed tables resident,
// so the memory guard saw the first zipper's keys while the second one was drawn.
// With a memory limit (set_limit), a growth that needs freshly mapped memory first checks
// the process's memory against the limit; past it the set is `full`: insert then refuses
// keys whose table would have to grow (returns false, and the caller stops drawing).  So no
// burst of table doublings can pass the limit (the 256 sub-cells of a directory double at
// nearly the same moment, about a GB at once in a deep cusp crop).
class KeySet {
    // t: 0 = empty slot.  A cell whose table would pass 2^20 slots becomes a directory of 256
    // sub-cells, chosen by the top bits of the hash (keys deep in a cusp crop share one
    // direction cell, and doubling its table allocated hundreds of MB at once).
    struct Cell { uint64_t* t = nullptr; uint32_t cap = 0, n = 0; Cell* sub = nullptr; };
    static constexpr uint32_t DIR_CAP = 1u << 20;
    vector<Cell> cells;
    size_t n = 0;
    double limit = 0;                                       // bytes (0: no limit)
    bool isFull = false;
    struct Slab { void* p; size_t bytes; };
    vector<Slab> slabs;
    vector<uint64_t*> freeList[32];
    uint64_t* bump[32] = {}; size_t left[32] = {};          // the current slab of each size
    uint64_t* alloc_table(int c) {                          // 8 << c slots, zeroed
        const size_t len = (size_t)8 << c;
        uint64_t* t;
        if (!freeList[c].empty()) { t = freeList[c].back(); freeList[c].pop_back(); }
        else {
            if (left[c] < len) {
                size_t bytes = std::max((size_t)4 << 20, len*sizeof(uint64_t));
                void* m = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
                if (m == MAP_FAILED) throw std::bad_alloc();
                slabs.push_back({m, bytes});
                bump[c] = static_cast<uint64_t*>(m); left[c] = bytes/sizeof(uint64_t);
            }
            t = bump[c]; bump[c] += len; left[c] -= len;
        }
        std::memset(t, 0, len*sizeof(uint64_t));
        return t;
    }
    static int size_class(uint32_t cap) { int c = 0; while (((uint32_t)8 << c) < cap) ++c; return c; }
    // may the cell grow?  Only freshly mapped memory is checked (a table from the free list or
    // the current slab costs nothing new; a fresh slab is at least 4 MB, so the check is rare).
    // A directory's 256 sub-tables (with the smaller tables they outgrow while being filled)
    // take at most about 3x the old table, plus a slab of each size.
    bool may_grow(const Cell& c, bool top) {
        if (limit <= 0) return true;
        if (isFull) return false;
        size_t need;
        if (top && 2*c.cap > DIR_CAP) need = 4*(size_t)c.cap*sizeof(uint64_t) + ((size_t)48 << 20);
        else {
            int nc = c.cap ? size_class(c.cap) + 1 : 0;
            size_t len = (size_t)8 << nc;
            need = (!freeList[nc].empty() || left[nc] >= len) ? 0
                                                               : std::max((size_t)4 << 20, len*sizeof(uint64_t));
        }
        if (need == 0 || resident_bytes() + (double)need <= limit) return true;
        isFull = true;
        return false;
    }
    static void put(Cell& c, uint64_t k) {               // (room is known to exist)
        size_t mask = c.cap - 1, i = k & mask;
        while (c.t[i]) i = (i+1) & mask;
        c.t[i] = k; ++c.n;
    }
    void grow(Cell& c, bool top) {
        uint32_t ocap = c.cap; uint64_t* old = c.t;
        int oc = ocap ? size_class(ocap) : -1;
        if (top && 2*ocap > DIR_CAP) {                   // -> a directory of 256 sub-cells
            c.sub = new Cell[256];
            for (uint32_t j = 0; j < ocap; ++j) if (uint64_t k = old[j]) {
                Cell& d = c.sub[k >> 56];
                if (4*(d.n+1) > 3*d.cap) grow(d, false);
                put(d, k);
            }
            c.t = nullptr; c.cap = 0;
            freeList[oc].push_back(old);
            return;
        }
        int nc = oc + 1;
        c.t = alloc_table(nc); c.cap = (uint32_t)8 << nc; c.n = 0;
        for (uint32_t j = 0; j < ocap; ++j) if (uint64_t k = old[j]) put(c, k);
        if (old) freeList[oc].push_back(old);
    }
    void release() {
        for (Cell& c : cells) delete[] c.sub;
        for (const Slab& sl : slabs) munmap(sl.p, sl.bytes);
        slabs.clear(); vector<Cell>().swap(cells); n = 0;
        for (int c = 0; c < 32; ++c) { vector<uint64_t*>().swap(freeList[c]); bump[c] = nullptr; left[c] = 0; }
    }
public:
    KeySet() = default;
    KeySet(const KeySet&) = delete;
    KeySet& operator=(const KeySet&) = delete;
    KeySet(KeySet&& o) noexcept { *this = std::move(o); }
    KeySet& operator=(KeySet&& o) noexcept {
        if (this != &o) {
            release();
            cells.swap(o.cells); n = o.n; limit = o.limit; isFull = o.isFull; slabs.swap(o.slabs);
            for (int c = 0; c < 32; ++c) { freeList[c].swap(o.freeList[c]); bump[c] = o.bump[c]; left[c] = o.left[c];
                                           o.bump[c] = nullptr; o.left[c] = 0; }
            o.n = 0;
        }
        return *this;
    }
    ~KeySet() { release(); }
    // memory limit in bytes for the whole process (0: none; see the class comment)
    void set_limit(double bytes) { limit = bytes; }
    bool full() const { return isFull; }
    // true if newly inserted; false if present, or (full()) refused at the memory limit
    bool insert(const HKey& k) {
        if (cells.empty()) cells.resize((size_t)1 << KEY_CELL_BITS);
        Cell* c = &cells[k.cell];
        bool top = true;
        if (c->sub) { c = &c->sub[k.h >> 56]; top = false; }
        if (c->cap) {
            size_t mask = c->cap-1, i = k.h & mask;
            while (c->t[i]) { if (c->t[i] == k.h) return false; i = (i+1) & mask; }
        }
        if (4*(c->n+1) > 3*c->cap) {                      // load <= 3/4
            if (!may_grow(*c, top)) return false;
            grow(*c, top);
            if (c->sub) c = &c->sub[k.h >> 56];
            if (4*(c->n+1) > 3*c->cap) { if (!may_grow(*c, false)) return false; grow(*c, false); }
        }
        put(*c, k.h); ++n; return true;
    }
    bool count(const HKey& k) const {
        if (cells.empty()) return false;
        const Cell* c = &cells[k.cell];
        if (c->sub) c = &c->sub[k.h >> 56];
        if (!c->cap) return false;
        size_t mask = c->cap-1, i = k.h & mask;
        while (c->t[i]) { if (c->t[i] == k.h) return true; i = (i+1) & mask; }
        return false;
    }
    void prefetch(const HKey& k) const {
        if (cells.empty()) return;
        const Cell* c = &cells[k.cell];
        if (c->sub) c = &c->sub[k.h >> 56];
        if (c->cap) __builtin_prefetch(&c->t[k.h & (c->cap-1)]);
    }
    size_t size() const { return n; }
};

// ------------------------------------------------------------------ words
static char inv_letter(char ch) {
    switch (ch) { case 'a': return 'A'; case 'A': return 'a';
                  case 'b': return 'B'; case 'B': return 'b'; }
    return '?';
}
static string invert_word(const string& w) {
    string r; r.reserve(w.size());
    for (size_t i = w.size(); i-- > 0; ) r.push_back(inv_letter(w[i]));
    return r;
}
static string free_reduce(const string& w) {
    string s;
    for (char c : w) {
        if (!s.empty() && s.back() == inv_letter(c)) s.pop_back();
        else s.push_back(c);
    }
    return s;
}

struct BallGens;                  // the root-ball search's generators (see make_ball_gens)
struct Rep {
    Mat A, Ai, B, Bi, C, Ci;      // rho(a), rho(a)^-1, ..., rho(c)=C, rho(c)^-1=Ci
    std::shared_ptr<const BallGens> ball;   // set by prepare() for the final (centred) rep
    const Mat& gen(char ch) const {
        switch (ch) { case 'a': return A; case 'A': return Ai;
                      case 'b': return B; case 'B': return Bi;
                      case 'c': return C; case 'C': return Ci; }
        return A;
    }
};

struct Monodromy {
    string name, rl;
    string phi_a, phi_b;          // phi(a), phi(b): automorphism induced by c
    string psi_a, psi_b;          // phi^{-1}(a), phi^{-1}(b)
    cpx cuspx, cuspy, cuspz;      // geometric cusped character (tr a, tr b, tr ab)
    cpx tau = 0;                  // its cusp shape (see orient_cusp_character)
    bool conjugated = false;      // the orientation convention complex-conjugated it
};

// substitution of an automorphism (a -> ia, b -> ib) into a word over {a,A,b,B}
static string substitute(const string& w, const string& ia, const string& ib) {
    string iA = invert_word(ia), iB = invert_word(ib), out;
    for (char c : w) switch (c) {
        case 'a': out += ia; break; case 'A': out += iA; break;
        case 'b': out += ib; break; case 'B': out += iB; break; }
    return out;
}
// The mapping class group of the once-punctured torus is generated by the two Dehn twists
// R: a->a, b->ba  and  L: a->ab, b->b.  A monodromy given as a positive word in R,L induces
// the automorphism phi (composed left to right); psi = phi^{-1} uses R^{-1}: b->bA and
// L^{-1}: a->aB, applied in reverse.
static void compose_phi(const string& rl, string& pa, string& pb) {
    string a = "a", b = "b";
    for (char g : rl) {
        bool R = (g=='R'||g=='r');
        string sa = R ? "a"  : "ab", sb = R ? "ba" : "b";
        a = substitute(a, sa, sb); b = substitute(b, sa, sb);
    }
    pa = free_reduce(a); pb = free_reduce(b);
}
static void compose_psi(const string& rl, string& pa, string& pb) {
    string a = "a", b = "b";
    for (size_t i = rl.size(); i-- > 0; ) {
        bool R = (rl[i]=='R'||rl[i]=='r');
        string sa = R ? "a"  : "aB", sb = R ? "bA" : "b";
        a = substitute(a, sa, sb); b = substitute(b, sa, sb);
    }
    pa = free_reduce(a); pb = free_reduce(b);
}
static bool is_rl_word(const string& s) {
    if (s.empty()) return false;
    bool hasR = false, hasL = false;
    for (char c : s) {
        if (c=='R'||c=='r') hasR = true;
        else if (c=='L'||c=='l') hasL = true;
        else return false;
    }
    return hasR && hasL;                 // need both to be pseudo-Anosov (hyperbolic)
}

// ------------------------------------------------------------------ character variety
static void reconstruct_AB(cpx x, cpx y, cpx z, Mat& A, Mat& B) {
    cpx al = (x + std::sqrt(x*x - 4.0)) / 2.0, ai = 1.0 / al;
    A = { al, {0,0}, {0,0}, ai };
    cpx p = (z - ai*y) / (al - ai), s = y - p;
    B = { p, {1,0}, p*s - 1.0, s };
}
// The character-variety solver keeps the ORIGINAL program's complex arithmetic (plain
// std::complex products, not cmuladd), so that it produces the same candidate characters
// as the earlier versions of this program (see solve_cusp_character).
static inline Mat matmul_char(const Mat& M, const Mat& N) {
    return { M.a*N.a + M.b*N.c, M.a*N.b + M.b*N.d,
             M.c*N.a + M.d*N.c, M.c*N.b + M.d*N.d };
}
static cpx trace_of(const Mat& A, const Mat& B, const string& w) {
    Mat Ai = matinv(A), Bi = matinv(B), M = identity();
    for (char ch : w) switch (ch) {
        case 'a': M = matmul_char(M,A); break; case 'A': M = matmul_char(M,Ai); break;
        case 'b': M = matmul_char(M,B); break; case 'B': M = matmul_char(M,Bi); break; }
    return tr(M);
}
static cpx kappa(cpx x, cpx y, cpx z) { return x*x + y*y + z*z - x*y*z - 2.0; }

// Newton solve for the character fixed by the monodromy trace map on kappa=k, seeded from
// the passed-in (x,y,z).  Used both for analytic continuation in the orbifold index (seed
// = cusped character) and for solving the cusp itself (random seeds).
static bool newton_char(const string& pa, const string& pb, double k, cpx& x, cpx& y, cpx& z) {
    auto G = [&](cpx X, cpx Y, cpx Z, cpx& g0, cpx& g1, cpx& g2) {
        Mat A, B; reconstruct_AB(X, Y, Z, A, B);
        g0 = trace_of(A, B, pa) - X;
        g1 = trace_of(A, B, pb) - Y;
        g2 = kappa(X, Y, Z) - k;
    };
    const double h = 1e-8;
    for (int it = 0; it < 300; ++it) {
        cpx g0, g1, g2; G(x, y, z, g0, g1, g2);
        double err = std::max({std::abs(g0), std::abs(g1), std::abs(g2)});
        if (err < 1e-14) return true;
        cpx J[3][3], a0, a1, a2;
        G(x+h, y, z, a0,a1,a2); J[0][0]=(a0-g0)/h; J[1][0]=(a1-g1)/h; J[2][0]=(a2-g2)/h;
        G(x, y+h, z, a0,a1,a2); J[0][1]=(a0-g0)/h; J[1][1]=(a1-g1)/h; J[2][1]=(a2-g2)/h;
        G(x, y, z+h, a0,a1,a2); J[0][2]=(a0-g0)/h; J[1][2]=(a1-g1)/h; J[2][2]=(a2-g2)/h;
        cpx M[3][4] = {{J[0][0],J[0][1],J[0][2],-g0},
                       {J[1][0],J[1][1],J[1][2],-g1},
                       {J[2][0],J[2][1],J[2][2],-g2}};
        for (int col = 0; col < 3; ++col) {
            int piv = col;
            for (int r = col+1; r < 3; ++r)
                if (std::abs(M[r][col]) > std::abs(M[piv][col])) piv = r;
            for (int c = 0; c < 4; ++c) std::swap(M[col][c], M[piv][c]);
            cpx dg = M[col][col];
            if (std::abs(dg) < 1e-300) return false;
            for (int c = col; c < 4; ++c) M[col][c] /= dg;
            for (int r = 0; r < 3; ++r) if (r != col) {
                cpx f = M[r][col];
                for (int c = col; c < 4; ++c) M[r][c] -= f*M[col][c];
            }
        }
        x += M[0][3]; y += M[1][3]; z += M[2][3];
    }
    cpx g0,g1,g2; G(x,y,z,g0,g1,g2);
    return std::max({std::abs(g0),std::abs(g1),std::abs(g2)}) < 1e-10;
}

// Analytic continuation from the cusped character to level k = -2 cos(pi/n), in steps of
// at most 0.02 in k.  The path must stay away from the real characters (a real character
// is never the holonomy of a hyperbolic orbifold, and continuing through one could swap
// the character with its complex conjugate); it fails if max |Im| of the traces drops
// below 1e-6.
static bool solve_character(const Monodromy& mon, double k, cpx& x, cpx& y, cpx& z) {
    x = mon.cuspx; y = mon.cuspy; z = mon.cuspz;
    int steps = std::max(1, (int)std::ceil(std::abs(k + 2.0)/0.02));
    for (int i = 1; i <= steps; ++i) {
        double ki = (i == steps) ? k : -2.0 + (k + 2.0)*i/steps;
        if (!newton_char(mon.phi_a, mon.phi_b, ki, x, y, z)) return false;
        if (std::max({std::abs(x.imag()), std::abs(y.imag()), std::abs(z.imag())}) < 1e-6) {
            fprintf(stderr, "the continuation from the cusp reached a real (non-geometric) "
                    "character at tr[a,b] = %.6f\n", ki);
            return false;
        }
    }
    return true;
}

static bool solve_conjugator(const Mat& A, const Mat& B, const Mat& Ap, const Mat& Bp, Mat& out);
static char inv_letter6(char ch);
struct Rep;
static size_t count_ball(const Rep& rep, double R, size_t cap);

// Solve the CUSPED (k=-2) character of an arbitrary monodromy by random-restart Newton.
// Candidate fixed points are filtered to those giving an irreducible rep with a
// non-degenerate cusp (Im tau != 0, see cusp_shape: a real tau means the peripheral Z^2 is
// not a lattice, so the rep is not discrete and faithful; e.g. RLRL has such a candidate)
// that also passes a discreteness screen (a small hyperbolic ball must stay bounded);
// among these the geometric branch is chosen as the one of largest imaginary part.
// Returns false if none qualifies.
// TIES.  That criterion is exactly tied between a character, its complex conjugate (the
// mirror-image orbifold) and its sign-twisted SL(2,C) lifts; the tied sums differ only in
// the last bits, so which of them comes first is decided by rounding.  The tie is resolved
// afterwards, independently of rounding, by orient_cusp_character (a fixed lift and the
// cusp-shape chirality convention).  The Newton seeds and the sort are those of the
// earlier versions of this program, so the same candidate (up to the tie) is chosen.
// NOTE: for an exotic monodromy this is still a heuristic and the picture should be
// sanity checked.
static bool build_rep_from_char(const string& pa, const string& pb, cpx x, cpx y, cpx z, Rep& rep);
static bool peripheral_conjugator(const string& pa, const string& pb, string& w);
static bool cusp_shape(const Rep& r, const string& w, cpx& tau);
static bool solve_cusp_character(const string& pa, const string& pb, cpx& bx, cpx& by, cpx& bz) {
    uint64_t rng = 0x2545F4914F6CDD1DULL;
    auto rnd = [&]() {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        return ((double)(rng >> 11) / 9007199254740992.0) * 6.0 - 3.0;
    };
    vector<std::array<cpx,3>> cands;
    for (int t = 0; t < 6000; ++t) {
        cpx x(rnd(), rnd()), y(rnd(), rnd()), z(rnd(), rnd());
        if (!newton_char(pa, pb, -2.0, x, y, z)) continue;
        double im = std::abs(x.imag()) + std::abs(y.imag()) + std::abs(z.imag());
        if (im < 1e-4) continue;                                   // real branch: not geometric
        if (std::abs(x) > 50 || std::abs(y) > 50 || std::abs(z) > 50) continue;
        if (std::abs(x*x-4.0) < 1e-6 || std::abs(y*y-4.0) < 1e-6) continue;   // parabolic generator
        bool dup = false;
        for (auto& c : cands)
            if (std::abs(c[0]-x)<1e-5 && std::abs(c[1]-y)<1e-5 && std::abs(c[2]-z)<1e-5) { dup=true; break; }
        if (!dup) cands.push_back({x,y,z});
    }
    // prefer larger imaginary part; accept the first that is irreducible and discrete
    std::sort(cands.begin(), cands.end(), [](const std::array<cpx,3>& p, const std::array<cpx,3>& q){
        auto im=[](const std::array<cpx,3>& v){return std::abs(v[0].imag())+std::abs(v[1].imag())+std::abs(v[2].imag());};
        return im(p) > im(q);
    });
    string w;
    if (!peripheral_conjugator(pa, pb, w)) return false;
    for (auto& c : cands) {
        Rep rep; cpx tau;
        if (!build_rep_from_char(pa, pb, c[0], c[1], c[2], rep)) continue;   // reducible
        if (!cusp_shape(rep, w, tau) || std::abs(tau.imag()) < 1e-6) continue;   // degenerate cusp
        if (count_ball(rep, 3.5, 40000) >= 40000) continue;                 // non-discrete
        bx=c[0]; by=c[1]; bz=c[2]; return true;
    }
    return false;
}

// solve for C with C A C^-1 = Ap, C B C^-1 = Bp.  Returns false if the intertwiner is not
// unique up to scale (rank != 3) or is singular -- i.e. the representation is reducible.
static bool solve_conjugator(const Mat& A, const Mat& B, const Mat& Ap, const Mat& Bp, Mat& out) {
    auto addrows = [](const Mat& X, const Mat& Xp, vector<std::array<cpx,4>>& rows) {
        cpx X00=X.a,X01=X.b,X10=X.c,X11=X.d, P00=Xp.a,P01=Xp.b,P10=Xp.c,P11=Xp.d;
        rows.push_back({X00-P00, X10,     -P01,     cpx(0,0)});
        rows.push_back({X01,     X11-P00, cpx(0,0), -P01});
        rows.push_back({-P10,    cpx(0,0), X00-P11, X10});
        rows.push_back({cpx(0,0), -P10,    X01,     X11-P11});
    };
    vector<std::array<cpx,4>> rows;
    addrows(A, Ap, rows); addrows(B, Bp, rows);
    int n = 4; vector<int> pivcol; int prow = 0;
    for (int col = 0; col < n && prow < (int)rows.size(); ++col) {
        int sel = -1; double best = 1e-9;
        for (int r = prow; r < (int)rows.size(); ++r)
            if (std::abs(rows[r][col]) > best) { best = std::abs(rows[r][col]); sel = r; }
        if (sel < 0) continue;
        std::swap(rows[prow], rows[sel]);
        cpx dg = rows[prow][col];
        for (int c = 0; c < n; ++c) rows[prow][c] /= dg;
        for (int r = 0; r < (int)rows.size(); ++r) if (r != prow) {
            cpx f = rows[r][col];
            if (std::abs(f) > 1e-15)
                for (int c = 0; c < n; ++c) rows[r][c] -= f*rows[prow][c];
        }
        pivcol.push_back(col); ++prow;
    }
    if ((int)pivcol.size() != 3) return false;      // reducible: intertwiner not 1-dimensional
    int freecol = -1;
    for (int c = 0; c < n; ++c)
        if (std::find(pivcol.begin(), pivcol.end(), c) == pivcol.end()) { freecol = c; break; }
    cpx xv[4] = {0,0,0,0}; xv[freecol] = 1.0;
    for (int i = 0; i < (int)pivcol.size(); ++i) xv[pivcol[i]] = -rows[i][freecol];
    Mat C = { xv[0], xv[1], xv[2], xv[3] };
    cpx dt = det(C);
    if (std::abs(dt) < 1e-12) return false;          // singular conjugator
    cpx s = std::sqrt(dt);
    out = { C.a/s, C.b/s, C.c/s, C.d/s };
    return true;
}

// build the full representation (A,B,C and inverses) from a fiber character (x,y,z)
static bool build_rep_from_char(const string& pa, const string& pb, cpx x, cpx y, cpx z, Rep& rep) {
    if (std::abs(x*x - 4.0) < 1e-9 || std::abs(y*y - 4.0) < 1e-9) return false;  // parabolic gen
    Mat A, B; reconstruct_AB(x, y, z, A, B);
    Mat Ai = matinv(A), Bi = matinv(B);
    auto wmat = [&](const string& w) {
        Mat M = identity();
        for (char ch : w) switch (ch) {
            case 'a': M = matmul(M,A); break; case 'A': M = matmul(M,Ai); break;
            case 'b': M = matmul(M,B); break; case 'B': M = matmul(M,Bi); break; }
        return M;
    };
    Mat C;
    if (!solve_conjugator(A, B, wmat(pa), wmat(pb), C)) return false;            // reducible
    rep.A = A; rep.Ai = Ai; rep.B = B; rep.Bi = Bi; rep.C = C; rep.Ci = matinv(C);
    return true;
}

// ------------------------------------------------------------------ orientation convention
// A character and its complex conjugate are the holonomies of mirror-image orbifolds, and
// the trace equations cannot tell them apart.  The program fixes the chirality by the CUSP
// SHAPE.  At the cusp P = rho([a,b]) (here [a,b] = abAB) is parabolic and commutes with
// C' = rho(w)^-1 rho(c), where phi([a,b]) = w [a,b] w^-1 (w = 1 for fig8 and every
// positive R/L word, since R and L both fix abAB).  Conjugating their common fixed point
// to infinity makes them translations by t_P and t_C; tau = t_C / t_P.  tau does not
// depend on the conjugating map or on the signs of the matrices; c -> c [a,b]^k adds k
// and complex conjugation of rho conjugates tau.  CONVENTION: Im tau < 0, the sign of the
// built-in fig8 character (3 + i sqrt3)/2, which continues to n = 2 with the chirality of
// the original program's matrices (fig8ref) and the paper's pictures; there tau = -i/(2 sqrt3).
// Equivalently, for finite n >= 3, conjugating C' to diag(l, 1/l) with |l| > 1, P acts
// as z -> mu^2 z with arg mu^2 = +2 pi/n (sigma_n = +1).
static const int CUSP_TAU_SIGN = -1;

// w with phi(abAB) = w abAB w^-1 as reduced words (false if there is none)
static bool peripheral_conjugator(const string& pa, const string& pb, string& w) {
    string s = free_reduce(substitute("abAB", pa, pb)), u;
    while (s.size() >= 2 && s.back() == inv_letter(s.front())) {   // s = u v u^-1
        u.push_back(s.front()); s = s.substr(1, s.size() - 2);
    }
    const string q = "abAB";
    for (int k = 0; k < 4; ++k)                    // v = p^-1 abAB p with p = q[0..k)
        if (s == q.substr(k) + q.substr(0, k)) {
            w = free_reduce(u + invert_word(q.substr(0, k))); return true;
        }
    return false;
}
static Mat word_mat(const Rep& r, const string& w) {
    Mat M = identity();
    for (char ch : w) M = matmul(M, r.gen(ch));
    return M;
}
static Mat peripheral_C(const Rep& r, const string& w) { return matmul(matinv(word_mat(r, w)), r.C); }
// cusp shape t_C'/t_P of a cusped rep (false if P and C' are not commuting parabolics)
static bool cusp_shape(const Rep& r, const string& w, cpx& tau) {
    Mat P = matmul(matmul(r.A, r.B), matmul(r.Ai, r.Bi)), C = peripheral_C(r, w);
    cpx h2 = tr(P)/2.0;
    cpx v0 = P.a - h2, v1 = P.c;                   // a nonzero column of the nilpotent P - h2
    if (std::norm(P.b) + std::norm(P.d - h2) > std::norm(v0) + std::norm(v1)) { v0 = P.b; v1 = P.d - h2; }
    double s = std::sqrt(std::norm(v0) + std::norm(v1));
    if (!(s > 0)) return false;
    v0 /= s; v1 /= s;                              // the fixed point; h(inf) = v0/v1
    Mat h = { v0, -std::conj(v1), v1, std::conj(v0) }, hi = { std::conj(v0), std::conj(v1), -v1, v0 };
    Mat Pc = matmul(hi, matmul(P, h)), Cc = matmul(hi, matmul(C, h));
    double sc = 1.0 + std::abs(Pc.a) + std::abs(Cc.a);
    if (std::abs(Pc.c) > 1e-8*sc || std::abs(Cc.c) > 1e-8*sc || std::abs(Pc.b) < 1e-8*sc) return false;
    tau = (Cc.b/Cc.a) / (Pc.b/Pc.a);
    return true;
}
// sigma_n = sign(arg mu^2) at finite n >= 3 (0 if undefined), see above
static int finite_chirality(const Rep& r, const string& w) {
    Mat P = matmul(matmul(r.A, r.B), matmul(r.Ai, r.Bi)), C = peripheral_C(r, w);
    cpx t = tr(C), sq = std::sqrt(t*t - 4.0), l1 = (t + sq)/2.0, l2 = (t - sq)/2.0;
    if (std::abs(l2) > std::abs(l1)) std::swap(l1, l2);
    if (std::abs(l1) < 1.0 + 1e-9) return 0;       // C not loxodromic
    auto ev = [&](cpx l) {                         // eigenvector of C for l
        cpx x0 = C.b, x1 = l - C.a, y0 = l - C.d, y1 = C.c;
        return (std::norm(x0) + std::norm(x1) > std::norm(y0) + std::norm(y1))
               ? std::array<cpx,2>{x0, x1} : std::array<cpx,2>{y0, y1};
    };
    auto v = ev(l1), u = ev(l2);
    Mat h = { v[0], u[0], v[1], u[1] }, hi = matinv(h);   // h^-1 C h = diag(l1, l2)
    Mat Q = matmul(hi, matmul(P, h));
    if (std::abs(Q.b) + std::abs(Q.c) > 1e-6*(std::abs(Q.a) + std::abs(Q.d))) return 0;
    double ang = std::arg(Q.a/Q.d);
    return std::abs(ang) < 1e-9 || std::abs(std::abs(ang) - PI) < 1e-9 ? 0 : (ang > 0 ? 1 : -1);
}
// residual of the fixed-point equations tr phi(a) = x, tr phi(b) = y on kappa = -2
static double cusp_residual(const string& pa, const string& pb, cpx x, cpx y, cpx z) {
    Mat A, B; reconstruct_AB(x, y, z, A, B);
    return std::max({ std::abs(trace_of(A, B, pa) - x), std::abs(trace_of(A, B, pb) - y),
                      std::abs(kappa(x, y, z) + 2.0) });
}
// Normalise the cusped character of mon: (1) among its sign-twisted lifts (ea x, eb y,
// ea eb z) that are again fixed by the trace map (the same PSL(2,C) character, but a
// different normal form, hence a different frame and picture), take the one with
// Re tr a > 0, then Re tr b > 0, as far as the admissible twists allow; (2) complex-
// conjugate it if Im tau has the wrong sign.  Both choices are exact ties of
// the trace equations, so without this rounding would decide them.  Fills mon.tau.
static bool orient_cusp_character(Monodromy& mon) {
    const string &pa = mon.phi_a, &pb = mon.phi_b;
    cpx x = mon.cuspx, y = mon.cuspy, z = mon.cuspz;
    double r0 = cusp_residual(pa, pb, x, y, z), sc = 1.0 + std::abs(x) + std::abs(y) + std::abs(z);
    auto pos = [](cpx v) { return v.real() > 1e-9 || (std::abs(v.real()) <= 1e-9 && v.imag() > 0); };
    int best = -1;
    for (int t = 0; t < 4; ++t) {
        double ea = (t & 1) ? -1.0 : 1.0, eb = (t & 2) ? -1.0 : 1.0;
        cpx xt = ea*x, yt = eb*y, zt = ea*eb*z;
        if (t && cusp_residual(pa, pb, xt, yt, zt) > std::max(1e-9*sc, 100.0*r0)) continue;
        int score = (pos(xt) ? 2 : 0) + (pos(yt) ? 1 : 0);
        if (score > best) { best = score; mon.cuspx = xt; mon.cuspy = yt; mon.cuspz = zt; }
    }
    string w;
    Rep rep;
    if (!peripheral_conjugator(pa, pb, w) ||
        !build_rep_from_char(pa, pb, mon.cuspx, mon.cuspy, mon.cuspz, rep) ||
        !cusp_shape(rep, w, mon.tau) || std::abs(mon.tau.imag()) < 1e-6) {
        fprintf(stderr, "cannot determine the cusp shape of monodromy %s (degenerate cusp)\n",
                mon.name.c_str());
        return false;
    }
    if ((mon.tau.imag() > 0 ? 1 : -1) != CUSP_TAU_SIGN) {
        mon.cuspx = std::conj(mon.cuspx); mon.cuspy = std::conj(mon.cuspy);
        mon.cuspz = std::conj(mon.cuspz); mon.tau = std::conj(mon.tau);
        mon.conjugated = true;
    }
    return true;
}

static bool build_rep(const Monodromy& mon, int n, Rep& rep, cpx& xout) {
    double k = (n <= 0) ? -2.0 : -2.0*std::cos(PI/(double)n);
    cpx x, y, z;
    if (!solve_character(mon, k, x, y, z)) return false;
    xout = x;
    if (!build_rep_from_char(mon.phi_a, mon.phi_b, x, y, z, rep)) {
        fprintf(stderr, "degenerate or reducible representation\n");
        return false;
    }
    return true;
}

static double pz(double v) { return std::abs(v) < 5e-10 ? 0.0 : v; }   // print 0, not -0

// ------------------------------------------------------------------ projection
struct Pt { double x, y, z; };
static inline Pt project(const Vec& p) {
    double uu = std::norm(p.u), vv = std::norm(p.v), s = uu + vv;
    cpx cr = p.u * std::conj(p.v);
    return { 2.0*cr.real()/s, 2.0*cr.imag()/s, (vv - uu)/s };
}

// ------------------------------------------------------------------ conformal centering
// Conjugate the representation so the basepoint j becomes the conformal barycenter of the
// limit set.  The enumeration ball is centered at j, and the picture is the visual sphere
// from j; centering makes the Patterson-Sullivan (visual) measure round, so the drawing has
// even density and the ball-edge is pushed uniformly to the rim.  We sample the limit set by
// the shadows of a small hyperbolic ball of group elements, then iterate the ball-model
// isometry that drives the sample's centroid to the origin.
static Mat conformal_center(const Rep& rep) {
    Vec base = { {1.0,0.0}, {0.0,0.0} };            // an arbitrary seed point (infinity)
    // collect a limit-set sample = shadows of a small hyperbolic ball
    vector<Pt> S;
    KeySet seen;
    double thr = 2.0*std::cosh(4.5);
    // iterative DFS to avoid deep recursion
    struct Frame { Mat M; char last; int depth; };
    vector<Frame> st{{identity(), 0, 0}};
    static const char gens[6] = {'a','A','b','B','c','C'};
    while (!st.empty()) {
        Frame fr = st.back(); st.pop_back();
        if (!seen.insert(hkey(fr.M))) continue;
        S.push_back(project(act(fr.M, base)));
        if (fr.depth >= 64 || S.size() > 200000) continue;
        for (char g : gens) {
            if (fr.last != 0 && g == inv_letter6(fr.last)) continue;
            Mat Mc = matmul(fr.M, rep.gen(g));
            if (!(frob2(Mc) <= thr)) continue;
            st.push_back({Mc, g, fr.depth+1});
        }
    }
    if (S.size() < 8) return identity();
    // iterate: rotate centroid direction to the north pole, then translate it toward 0
    Mat total = identity();
    vector<Pt> cur = S;
    auto centroid = [](const vector<Pt>& P, double& t) {
        double cx=0, cy=0, cz=0;
        for (auto& p : P) { cx+=p.x; cy+=p.y; cz+=p.z; }
        double n = (double)P.size(); cx/=n; cy/=n; cz/=n;
        t = std::sqrt(cx*cx+cy*cy+cz*cz);
        return Pt{cx,cy,cz};
    };
    // sphere point -> a CP^1 representative, the inverse of project(): z = (X+iY)/(1+Z)
    auto to_cp1 = [](const Pt& p) -> Vec {
        if (p.z < -0.999999) return { {1,0}, {0,0} };          // south pole (Z=-1) -> infinity
        return { cpx(p.x, p.y) / (1.0 + p.z), {1,0} };
    };
    auto make_step = [&](const Pt& c, double t, double frac) {
        double nx=c.x/t, ny=c.y/t, nz=c.z/t;
        cpx zn = (nz > -0.999) ? cpx(nx,ny)/(1.0+nz) : cpx(1e6,0);
        double den = std::sqrt(1.0 + std::norm(zn));
        Mat Rm = { cpx(1,0)/den, -zn/den, std::conj(zn)/den, cpx(1,0)/den };   // rotate n -> north
        double s = frac * std::min(t, 0.25);                                    // damped step
        double mu = (1.0 + s) / (1.0 - s);
        Mat D = { cpx(std::sqrt(mu),0), {0,0}, {0,0}, cpx(1.0/std::sqrt(mu),0) };
        return matmul(matinv(Rm), matmul(D, Rm));
    };
    auto apply_all = [&](const Mat& st, vector<Pt>& outv, double& tt) {
        outv.clear(); outv.reserve(cur.size());
        for (auto& p : cur) outv.push_back(project(act(st, to_cp1(p))));
        Pt cc = centroid(outv, tt); (void)cc;
    };
    double t; Pt c = centroid(cur, t);
    for (int it = 0; it < 200 && t > 1e-3; ++it) {
        // damped step; try both axis directions, take the one that reduces the centroid norm
        vector<Pt> A1, A2; double t1, t2;
        Mat s1 = make_step(c, t, +1.0), s2 = make_step(c, t, -1.0);
        apply_all(s1, A1, t1); apply_all(s2, A2, t2);
        if (std::min(t1,t2) >= t - 1e-9) break;          // no further improvement
        if (t1 <= t2) { cur.swap(A1); total = matmul(s1, total); t = t1; }
        else          { cur.swap(A2); total = matmul(s2, total); t = t2; }
        c = centroid(cur, t);
    }
    return total;
}

// ------------------------------------------------------------------ renderer
// Branches of the lifted train track.  The branch (h, x), x in {a,A,b,B}, is the arc
// rho(h).alpha_x from rho(h).VV to rho(h x).VV, where VV is the attracting fixed point of
// `prepend`.  Splitting it once along the monodromy replaces it by the CONSECUTIVE
// sub-branches (h.prepend.u_i, x_i), where image(x) = x_1...x_k and u_i = x_1...x_{i-1}:
// child i ends exactly where child i+1 starts, the first starts at h.VV (prepend fixes VV)
// and the last ends at h x.VV (prepend.image(x).prepend^-1 = x).
// Identity: (h, X) with X uppercase is the reversal of (h X, x), so a branch is keyed by
// hkey of the normalised pair (h', lowercase label) -- exact, never geometric.
//
// Drawing (depth first, children in order, so consecutive leaves are contiguous and
// polylines are built by construction):
//  * a branch whose key is already in `done` is skipped (it is drawn, or covered, elsewhere)
//    and the current polyline ends;
//  * a branch whose limit arc provably misses the visible part of the sphere is skipped
//    (view culling, EPS/PNG only: the arc lies in the Mobius image h.D_x of a round disc D_x
//    containing alpha_x, computed exactly as a Hermitian form);
//  * a branch is split while its chord exceeds the accuracy, or while one of its first-level
//    vertices is more than the accuracy away from the chord, or while its curve is not
//    PROVABLY within the accuracy of the chord (the bound from round discs containing it, see
//    REGIONS below: this catches the thin spikes, whose tips the vertices reach only slowly);
//  * OVERLAPS.  The branches are train PATHS: the DAG of branches has up to four parents per
//    branch (one per occurrence of x or X in the images), so two branches of the same scale
//    reached from different roots can share sub-branches without being equal.  A leaf drawn
//    as one chord therefore records the keys of all its sub-branches down to `overlapDepth`
//    splitting steps in `done` ("covered by this chord"), and a would-be leaf that has a
//    sub-branch in `done` within that depth is split instead, so the shared sub-branch is
//    skipped and only the rest is drawn.  Every piece of the curve that is shared at the
//    level of branches is thus drawn once -- by exact identity, never by geometry.
// Hermitian form [[p,q],[conj q,s]] on C^2; the disc {w : w* H w <= 0} of CP^1 is the
// spherical cap {P : C.P >= cos r} with C = -(Re q, Im q, (s-p)/2)/|.|, cos r = (p+s)/(2|.|)
// and sin r = sqrt(-det H)/|.|.
struct Herm { double p, s; cpx q; };
static Herm congruence(const Herm& H, const Mat& G) {          // G^dagger H G
    cpx qc = H.q*G.c, qd = H.q*G.d;
    double p = H.p*std::norm(G.a) + H.s*std::norm(G.c) + 2.0*(std::conj(G.a)*qc).real();
    double s = H.p*std::norm(G.b) + H.s*std::norm(G.d) + 2.0*(std::conj(G.b)*qd).real();
    cpx q = H.p*std::conj(G.a)*G.b + H.s*std::conj(G.c)*G.d + std::conj(G.a)*qd
          + std::conj(G.c)*std::conj(H.q)*G.b;
    return { p, s, q };
}
static inline double dist3(const Pt& a, const Pt& b) {
    double x = a.x-b.x, y = a.y-b.y, z = a.z-b.z; return std::sqrt(x*x + y*y + z*z);
}
static double seg_dist(const Pt& p, const Pt& a, const Pt& b) {     // point to segment, R^3
    double ux = b.x-a.x, uy = b.y-a.y, uz = b.z-a.z, L2 = ux*ux + uy*uy + uz*uz;
    double t = L2 > 0 ? ((p.x-a.x)*ux + (p.y-a.y)*uy + (p.z-a.z)*uz)/L2 : 0;
    t = std::max(0.0, std::min(1.0, t));
    return dist3(p, Pt{a.x + t*ux, a.y + t*uy, a.z + t*uz});
}
static inline double angle3(const Pt& a, const Pt& b) {
    double cx = a.y*b.z - a.z*b.y, cy = a.z*b.x - a.x*b.z, cz = a.x*b.y - a.y*b.x;
    return std::atan2(std::sqrt(cx*cx + cy*cy + cz*cz), a.x*b.x + a.y*b.y + a.z*b.z);
}

// An append-only array in fixed chunks (no reallocation: a growing std::vector briefly
// holds up to 3x its size); used for the --stats tables, which can be large.
template <class T> struct Chunked {
    static const size_t CH = 1 << 16;
    vector<vector<T>> c;
    size_t n = 0;
    void push_back(const T& x) {
        if (n % CH == 0) { c.emplace_back(); c.back().reserve(CH); }
        c.back().push_back(x); ++n;
    }
    size_t size() const { return n; }
    const T& operator[](size_t i) const { return c[i / CH][i % CH]; }
    vector<T> flatten() {                              // move into one vector, freeing the chunks
        vector<T> v; v.reserve(n);
        for (auto& b : c) { v.insert(v.end(), b.begin(), b.end()); vector<T>().swap(b); }
        vector<vector<T>>().swap(c); n = 0;
        return v;
    }
    void clear() { vector<vector<T>>().swap(c); n = 0; }
};

struct Renderer {
    const Rep& rep;
    Vec        VV{{1,0},{0,0}};
    string     img[4];              // substitution images of a, A, b, B
    Mat        prepend = identity();
    double     accuracy = 0.004;
    int        maxdepth = 1000000;  // runaway guard only (parabolic splitting is slow)
    bool       stats = false;       // --stats: run the exactness checks
    int        overlapLevels = 1;   // overlap resolution: levels of sub-branches below a leaf
                                    // checked and marked (0 = off; see lookahead)
    // view culling (EPS/PNG): draw only branches whose arc can meet the cap {P : angle(P,
    // vaxis) <= vrad}, which contains the visible square (plus a margin)
    bool       cull = false;
    Pt         vaxis{0,0,1};
    double     vrad = PI;
    Herm       capH[2];             // discs D_a, D_b containing alpha_a, alpha_b
    double     capSin[2] = {0, 0};
    bool       capAll[2] = {true, true};   // no useful disc: never cull at this level

    KeySet     done;                // branch keys met (drawn, culled, or covered by a chord)
    // regions containing the base arcs, per letter a, A, b, B (see setup_regions)
    struct Reg {                    // a disc, or a union of up to 12 discs
        int n = 0; Herm H[12];
        Reg() {}
        explicit Reg(const Herm& h) : n(1) { H[0] = h; }
    };
    vector<Reg> regs[4];
    struct Tip { Mat Q; Vec p; int pos; };   // child pos of x is x again, via Q; p: fixed point
    vector<Tip> tips[4];
    double     devFactor = 1.0;     // a leaf's curve must provably stay this close (x accuracy)
    size_t     nDev = 0, nDevCapped = 0, nDevLeaves = 0;
    bool       regFail = false;     // the base arcs could not be sampled: no regions
    // runaway guard: the splits for the region bound may number at most 4x the leaves of
    // the ordinary splitting (those not below a branch whose chord is at most the accuracy)
    bool dev_budget() const { return nDev < 4*(nLeaves - nDevLeaves) + 4000000; }

    // Output: polylines of points on the unit sphere; polyline k is the point range
    // [pstart[k], pstart[k+1]) (the last one ends at px.size()).
    vector<float>    px, py, pz;
    vector<uint32_t> pstart;
    bool             open = false;  // the next leaf continues the current polyline

    size_t nVisited = 0, nSkipped = 0, nCulled = 0, nSpike = 0, nOverlap = 0, nLeaves = 0,
           polysBeforeJoin = 0, nCapped = 0;
    double maxCappedChord = 0;
    // --stats only
    // (compact: flat vectors, checked by sorting once the zipper is drawn -- finish_stats)
    struct EndRec { uint64_t k; double s[3]; };      // a branch key and its endpoints' sum
    struct __attribute__((packed)) CellRec { uint64_t k; uint32_t h; uint8_t e; };  // key, hash of the sum's cell, grid exponent
    Chunked<CellRec> cellRecs;                        // every key recorded
    Chunked<EndRec> exactRecs, hitRecs;               // 1/16 of them exactly; keys met again
    Chunked<uint64_t> segHashes;                      // hashes of the leaf segments (rounded)
    vector<std::pair<Mat,char>> leafBatch;           // leaves awaiting refine_batch
    struct __attribute__((packed)) Samp { uint64_t k; float c; };   // a sampled fine piece
    Chunked<Samp> odSamples[64];                      // bucketed by the key's top bits
    double fineS = 0;                                 // the sampled fine length
    size_t falseMerge = 0, geomDup = 0, spikeLeaves = 0;
    double maxMismatch = 0, leafLen = 0, fineLen = 0, extraLen = 0, maxSpike = 0;
    size_t overdrawSamples = 0;

    Renderer(const Rep& r) : rep(r) {}

    static int lidx(char ch) { return ch=='a' ? 0 : ch=='A' ? 1 : ch=='b' ? 2 : 3; }
    void setup(const string& pa, const string& pb, const Mat& P) {
        prepend = P;
        VV = attracting_fixed_point(P);
        img[0] = pa; img[1] = invert_word(pa); img[2] = pb; img[3] = invert_word(pb);
        setup_regions();
    }
    // ---- REGIONS: provable bounds for how far a branch's curve strays from its chord.
    // Every base arc alpha_x (the branch (1, x)) is enclosed in a few round discs D (or unions
    // of discs); the arc of the branch (g, x) then lies in g(D), a round cap computed exactly
    // (congruence of the Hermitian form), so the smallest over the regions of "the farthest
    // point of g(D) from the chord" (region_dev) bounds the distance of the true curve from the
    // drawn chord.  A leaf is accepted only when that bound is at most the accuracy.
    // One small disc D0 per letter is not enough: along a chain of sub-branches Q^k(alpha)
    // converging to a fixed point p of a chain element Q, the image Q^k(D) of a disc that
    // contains the repelling fixed point of Q (or, for a parabolic Q, that contains p in its
    // interior) does not shrink -- this is why a cap-based rule ran away at the cusp.  So each
    // chain also gets a disc adapted to Q, in coordinates z = S(w) with S(p) = infinity
    // (S in SU(2)), where Q(z) = mu z + beta:
    //  * mu real (in particular Q parabolic, or nearly so, |tr^2 Q - 4| < 1e-4, taken as
    //    mu = 1): the half-plane H = {Re(z conj e) >= s}, e = beta/|beta|.  Q(H) =
    //    {Re(z conj e) >= mu s + |beta|} lies inside H (for mu > 1 when H excludes the
    //    repelling fixed point), so H contains alpha as soon as it contains the part of alpha
    //    outside Q(alpha); Q^k(H) is a disc through p of size ~1/k (parabolic) or shrinking
    //    geometrically.
    //  * mu not real (a spiral, finite order): H as above contains the pieces Q^k(G) of alpha
    //    only for k < Kx (Kx: a rotation by pi/4), so the region is a union of a few such
    //    half-planes, mapped by Q^(j Kx), and a small disc about p (see spiral below).
    //  * Q loxodromic: also a disc about alpha that excludes the repelling fixed point.
    // The chains are: the first-child chains (Q = prepend, p = VV, the start of every base
    // arc; the letter may change along the chain), the last-child chains (by reversal,
    // alpha_x = x(alpha_X) as sets), and the middle self-loops (a child of x, not first or
    // last, with the letter x again: Q = prepend.x_1...x_{i-1}; its fixed point is a TIP of
    // alpha_x, the end of a thin spike -- at the cusp the curve has an outward cusp there).
    // The discs are fitted to samples of the base arcs (sample_arc), each with a margin of
    // twice the size of its piece; the containment is checked at 30x finer sampling in the
    // tests (build with -DZIPPER_REGCHECK).
    static Herm cap_herm(const Pt& C, double r) {            // {P : C.P >= cos r}
        double cr = std::cos(r);
        return { cr + C.z, cr - C.z, cpx(-C.x, -C.y) };
    }
    static double herm_sd(const Herm& H) { return std::sqrt(std::max(0.0, std::norm(H.q) - H.p*H.s)); }
    static Mat rot_to_inf(const Vec& p) {                   // S in SU(2) with S(p) = infinity
        double n = std::sqrt(std::norm(p.u) + std::norm(p.v));
        cpx p1 = p.u/n, p2 = p.v/n;
        return { std::conj(p1), std::conj(p2), -p2, p1 };
    }
    // a sample of a base arc: a point, and an angle the arc stays within near it
    struct Smp { Vec w; double m; };
    // Sample the arc of (b0, l0): split until each piece is at most thr in size (its chord,
    // and the distance of its tips from its ends: the vertices approach a tip only slowly),
    // and pieces at a fixed point of a loxodromic chain element (sp) until they are small
    // compared with its distance from the repelling fixed point (the discs fitted to the
    // samples must avoid that point); each piece contributes its end points and the images of
    // its tips, with the margin 2 x its size.  False if the budget ran out.
    struct Special { Pt s; double lim; };       // pieces at s are refined to size <= lim
    bool sample_arc(const Mat& b0, char l0, double thr, vector<Smp>& out, size_t budget = 1000000,
                    const vector<Special>* sp = nullptr) const {
        struct F { Mat b; char l; int d; };
        vector<F> st{{b0, l0, 0}};
        size_t visits = 0;
        Vec T[8]; int nt;
        while (!st.empty() && ++visits < budget) {
            F f = st.back(); st.pop_back();
            Mat bx = matmul(f.b, rep.gen(f.l));
            Vec A = act(f.b, VV), B = act(bx, VV);
            Pt PA = project(A), PB = project(B);
            double sz = dist3(PA, PB);
            nt = 0;
            for (const Tip& t : tips[lidx(f.l)]) {
                if (nt == 8) break;
                T[nt] = act(f.b, t.p); Pt PT = project(T[nt++]);
                sz = std::max(sz, std::min(dist3(PT, PA), dist3(PT, PB)));
            }
            double lim = thr;
            if (sp) for (const Special& q : *sp) {
                bool at = dist3(PA, q.s) < 1e-9 || dist3(PB, q.s) < 1e-9;
                for (int i = 0; i < nt && !at; ++i) at = dist3(project(T[i]), q.s) < 1e-9;
                if (at) lim = std::min(lim, q.lim);
            }
            if (sz > lim && f.d < 100000) {
                Mat N = matmul(f.b, prepend);
                for (char ch : img[lidx(f.l)]) { st.push_back({N, ch, f.d+1}); N = matmul(N, rep.gen(ch)); }
                continue;
            }
            const double m = 2.0*sz + 1e-4;
            for (int i = 0; i < nt; ++i) out.push_back({T[i], m});
            out.push_back({A, m}); out.push_back({B, m});
        }
        return st.empty();
    }
    void setup_regions() {
        static const char L[4] = {'a','A','b','B'};
        // (nearly) parabolic: |tr^2 - 4| < 1e-4, i.e. the two fixed points are ~1e-2 apart
        // relative to the base arc: the curve near p then differs from the parabolic picture
        // only at that relative scale (the orders above ~200, and the cusp)
        auto parabolic = [](const Mat& Q) { cpx t = tr(Q); return std::abs(t*t - 4.0) < 1e-4; };
        for (int y = 0; y < 4; ++y) { regs[y].clear(); tips[y].clear(); }
        capAll[0] = capAll[1] = true;
        for (int y = 0; y < 4; ++y) {                       // the middle self-loops
            const string& u = img[y];
            Mat N = prepend;
            for (size_t i = 0; i + 1 < u.size(); ++i) {
                if (i > 0 && lidx(u[i]) == y) tips[y].push_back({N, attracting_fixed_point(N), (int)i});
                N = matmul(N, rep.gen(u[i]));
            }
        }
        // the fixed points of loxodromic chain elements, with their distance from the other one
        vector<Special> spc[4];
        auto add_sp = [&](int y, const Vec& p, const Vec& q) {
            double d = dist3(project(p), project(q));
            if (0.1*d < 3e-3) spc[y].push_back({project(p), std::max(0.1*d, 1e-7)});
        };
        if (!parabolic(prepend)) {
            Vec qP = attracting_fixed_point(matinv(prepend));
            for (int y = 0; y < 4; ++y) {
                add_sp(y, VV, qP);
                add_sp(y, act(rep.gen(L[y]), VV), act(rep.gen(L[y]), qP));
            }
        }
        for (int y = 0; y < 4; ++y)
            for (const Tip& t : tips[y]) if (!parabolic(t.Q)) add_sp(y, t.p, attracting_fixed_point(matinv(t.Q)));
        // samples of each child of each base arc (in parallel)
        vector<vector<Smp>> cs[4];
        vector<std::pair<int,int>> jobs;
        for (int y = 0; y < 4; ++y) { cs[y].resize(img[y].size()); for (size_t i = 0; i < img[y].size(); ++i) jobs.push_back({y, (int)i}); }
        std::atomic<bool> okS{true};
        {
            vector<std::thread> th;
            for (auto jb : jobs) th.emplace_back([&, jb]() {
                int y = jb.first, i = jb.second;
                Mat N = prepend;
                for (int j = 0; j < i; ++j) N = matmul(N, rep.gen(img[y][j]));
                if (!sample_arc(N, img[y][i], 3e-3, cs[y][i], 1000000, &spc[y])) okS = false;
            });
            for (auto& t : th) t.join();
        }
        if (!okS) { regFail = true; for (auto& t : tips) t.clear(); return; }
        // D0: a small cap containing all samples (centre by Badoiu-Clarkson iterations); for
        // a and b it is also the culling disc (cap_test)
        for (int y = 0; y < 4; ++y) {
            vector<Pt> P; vector<double> M;
            for (auto& v : cs[y]) for (auto& w : v) { P.push_back(project(w.w)); M.push_back(w.m); }
            double cx = 0, cy = 0, cz = 0;
            for (auto& p : P) { cx += p.x; cy += p.y; cz += p.z; }
            double cn = std::sqrt(cx*cx + cy*cy + cz*cz);
            if (!(cn > 1e-9*(double)P.size())) continue;
            Pt C{cx/cn, cy/cn, cz/cn};
            size_t stride = std::max<size_t>(1, P.size()/4000);
            for (int it = 1; it <= 300; ++it) {
                size_t far = 0; double fa = 2;
                for (size_t i = 0; i < P.size(); i += stride) { double a = C.x*P[i].x + C.y*P[i].y + C.z*P[i].z; if (a < fa) { fa = a; far = i; } }
                double w = 1.0/(it + 1);
                Pt D{C.x + w*(P[far].x - C.x), C.y + w*(P[far].y - C.y), C.z + w*(P[far].z - C.z)};
                double dn = std::sqrt(D.x*D.x + D.y*D.y + D.z*D.z);
                C = Pt{D.x/dn, D.y/dn, D.z/dn};
            }
            double r = 0;
            for (size_t i = 0; i < P.size(); ++i) r = std::max(r, angle3(C, P[i]) + M[i]);
            if (!(r < PI - 0.02)) continue;
            Herm H = cap_herm(C, r);
            regs[y].push_back(Reg(H));
            if (y == 0 || y == 2) { int k = y/2; capH[k] = H; capSin[k] = std::sin(r); capAll[k] = false; }
        }
        // the image under S of a sample's cap: a disc (c, rho), (kind 1) the exterior
        // {|z - c| >= rho}, or (kind 2) a half-plane
        struct ZD { int kind; cpx c; double rho; };
        auto zdisc = [](const Mat& Si, const Smp& w) {           // Si = S^-1
            Herm H = congruence(cap_herm(project(w.w), std::min(w.m, PI - 1e-6)), Si);
            double sc = std::abs(H.p) + std::abs(H.s) + std::abs(H.q);
            if (!(std::abs(H.p) > 1e-12*sc)) return ZD{2, 0, 0};
            double r2 = (std::norm(H.q) - H.p*H.s)/(H.p*H.p);
            return ZD{H.p > 0 ? 0 : 1, -H.q/H.p, std::sqrt(std::max(0.0, r2))};
        };
        auto smin = [&](const Mat& S, cpx e, const vector<Smp>& v) {    // min Re(z conj e)
            double m = 1e300; const Mat Si = matinv(S);
            for (auto& w : v) { ZD d = zdisc(Si, w); if (d.kind) return -1e300;
                m = std::min(m, (d.c*std::conj(e)).real() - d.rho); }
            return m;
        };
        auto halfplane = [&](const Mat& S, cpx e, double s0) {         // {Re(S(w) conj e) >= s0}
            return Reg{congruence(Herm{0.0, s0, -0.5*e}, S)};
        };
        // a disc containing the samples in coordinates with q at infinity (so q is not in it)
        auto qdisc = [&](const Vec& q, const vector<const vector<Smp>*>& sets, Reg& out) {
            Mat S = rot_to_inf(q), Si = matinv(S);
            vector<cpx> Z; vector<double> Rr;
            for (auto* v : sets) for (auto& w : *v) { ZD d = zdisc(Si, w); if (d.kind) return false;
                Z.push_back(d.c); Rr.push_back(d.rho); }
            if (Z.empty()) return false;
            cpx c = 0; for (auto& z : Z) c += z; c /= (double)Z.size();
            size_t stride = std::max<size_t>(1, Z.size()/4000);
            for (int it = 1; it <= 300; ++it) {
                size_t far = 0; double fa = -1;
                for (size_t i = 0; i < Z.size(); i += stride) { double a = std::abs(Z[i] - c); if (a > fa) { fa = a; far = i; } }
                c += (Z[far] - c)/(double)(it + 1);
            }
            double R = 0;
            for (size_t i = 0; i < Z.size(); ++i) R = std::max(R, std::abs(Z[i] - c) + Rr[i]);
            if (!std::isfinite(R)) return false;
            out = Reg{congruence(Herm{1.0, std::norm(c) - R*R, -c}, S)};
            return true;
        };
        // Q with a real multiplier at p: S, mu, c = |beta|, e and s* (the value of Re(z conj e)
        // at the repelling fixed point; H must have s >= s*)
        struct Aff { Mat S; double mu, c, sstar; cpx e; bool ok; };
        auto affine = [&](const Mat& Q, const Vec& p) {
            Aff A; A.ok = false; A.S = rot_to_inf(p);
            Mat M = matmul(A.S, matmul(Q, matinv(A.S)));
            if (!(std::abs(M.d) > 0)) return A;
            cpx mu = M.a/M.d, beta = M.b/M.d;
            if (parabolic(Q)) mu = 1.0;
            if (!(std::abs(mu.imag()) <= 1e-9*std::abs(mu)) || !(mu.real() >= 1.0) || !(std::abs(beta) > 1e-12)) return A;
            A.mu = mu.real(); A.e = beta/std::abs(beta); A.c = std::abs(beta);
            A.sstar = (A.mu > 1.0) ? A.c/(1.0 - A.mu) : -1e300;
            A.ok = true;
            return A;
        };
        auto repelling = [](const Mat& Q) { return attracting_fixed_point(matinv(Q)); };
        auto all = [&](int y) { vector<const vector<Smp>*> v; for (auto& c : cs[y]) v.push_back(&c); return v; };
        // A spiral: Q(z) = q' + mu (z - q') with mu not real, |mu| < 1.5 (near-parabolic; a
        // faster one shrinks well enough with the q-disc).  For a start letter, alpha = U_{k<Kx}
        // Q^k(G_k) u Q^Kx(alpha'), with G_k the pieces of alpha outside the next chain element
        // (G for the letter seq[off+k]; Wl: all the samples of a letter), Kx a rotation by pi/4.
        // The half-plane {Re(z conj e) >= s}, e from q' towards G_0, s the least support value of
        // the Q^k(G_k) (convex hulls of the sample centres inflated by the largest margin), which
        // must exclude q', contains the first Kx pieces; repeating, alpha lies in the union of
        // H_0, Q^Kx(H_1), Q^2Kx(H_2), ... (J parts, until |mu|^(J Kx) >= 1e4) and the disc
        // B = Q^(J Kx){|z - q'| >= r} about p, r the largest with {|z - q'| <= r} missing alpha'.
        auto spiral = [&](const Mat& Q, const Vec& p, const vector<int>& seq,
                          const vector<const vector<Smp>*>* G, const vector<const vector<Smp>*>* Wl, Reg& out) {
            Mat S = rot_to_inf(p), Si = matinv(S);
            Mat M = matmul(S, matmul(Q, Si));
            if (!(std::abs(M.d) > 0)) return false;
            cpx mu = M.a/M.d, beta = M.b/M.d;
            double rho = std::abs(mu), th = std::arg(mu);
            if (!(rho > 1.0 + 1e-12) || !(std::abs(th) > 1e-9) || rho > 1.5) return false;
            cpx qp = beta/(1.0 - mu);
            int Kx = (int)std::min(4000.0, std::max(1.0, std::floor(0.25*PI/std::abs(th))));
            int J = 1;
            while (J < 11 && std::pow(rho, (double)J*Kx) < 1e4) ++J;
            if ((size_t)(J*Kx) >= seq.size()) return false;
            vector<cpx> hull[4]; double mmax[4], rmin[4]; cpx cen[4]; bool have[4] = {false, false, false, false};
            auto prep = [&](int l) {
                if (have[l]) return;
                have[l] = true;
                typedef std::pair<double,double> P2d;
                vector<P2d> P; mmax[l] = 0; cen[l] = 0;
                bool bad = false;
                for (auto* v : G[l]) for (auto& w : *v) { ZD zd = zdisc(Si, w); if (zd.kind) { bad = true; break; }
                    cpx d = zd.c - qp; P.push_back({d.real(), d.imag()}); cen[l] += d;
                    mmax[l] = std::max(mmax[l], zd.rho); }
                if (bad) P.clear();
                if (!P.empty()) cen[l] /= (double)P.size();
                std::sort(P.begin(), P.end());
                P.erase(std::unique(P.begin(), P.end()), P.end());
                if (P.size() == 1) hull[l].push_back(cpx(P[0].first, P[0].second));
                else if (P.size() >= 2) {                     // Andrew's monotone chain
                    vector<P2d> Hh(2*P.size());
                    size_t k = 0;
                    auto cross = [](const P2d& o, const P2d& a, const P2d& b) {
                        return (a.first - o.first)*(b.second - o.second) - (a.second - o.second)*(b.first - o.first); };
                    for (size_t i = 0; i < P.size(); ++i) { while (k >= 2 && cross(Hh[k-2], Hh[k-1], P[i]) <= 0) --k; Hh[k++] = P[i]; }
                    for (size_t i = P.size() - 1, t = k + 1; i-- > 0; ) { while (k >= t && cross(Hh[k-2], Hh[k-1], P[i]) <= 0) --k; Hh[k++] = P[i]; }
                    for (size_t i = 0; i + 1 < k; ++i) hull[l].push_back(cpx(Hh[i].first, Hh[i].second));
                }
                rmin[l] = 1e300;
                for (auto* v : Wl[l]) for (auto& w : *v) {
                    ZD zd = zdisc(Si, w);
                    double r = zd.kind == 0 ? std::abs(zd.c - qp) - zd.rho
                             : zd.kind == 1 ? zd.rho - std::abs(zd.c - qp) : -1.0;
                    rmin[l] = std::min(rmin[l], r);
                }
            };
            out.n = 0;
            for (int j = 0; j <= J; ++j) {
                const size_t off = (size_t)j*Kx;
                const double scale = std::pow(rho, (double)off);
                Herm Hz;
                if (j < J) {                                  // H_j, s.t. it holds Kx pieces
                    int l0 = seq[off]; prep(l0);
                    if (hull[l0].empty() || !(std::abs(cen[l0]) > 0)) return false;
                    cpx e = cen[l0]/std::abs(cen[l0]);
                    double sm = 1e300, rk = 1.0;
                    for (int k = 0; k < Kx; ++k, rk *= rho) {
                        int l = seq[off + k]; prep(l);
                        if (hull[l].empty()) return false;
                        cpx rot = std::polar(1.0, k*th)*std::conj(e);
                        double h = 1e300;
                        for (const cpx& v : hull[l]) h = std::min(h, (v*rot).real());
                        sm = std::min(sm, rk*(h - mmax[l]));
                    }
                    if (!(sm > 0)) return false;              // H would contain q'
                    Hz = Herm{0.0, (qp*std::conj(e)).real() + sm, -0.5*e};
                } else {                                      // B
                    int l = seq[off]; prep(l);
                    if (!(rmin[l] > 0) || !std::isfinite(rmin[l])) return false;
                    Hz = Herm{-1.0, rmin[l]*rmin[l] - std::norm(qp), qp};
                }
                if (off) {                                    // mapped by Q^off: congruence by Q^-off
                    cpx mi = std::pow(mu, -(double)off);
                    Hz = congruence(Hz, Mat{mi, qp*(1.0 - mi), cpx(0, 0), cpx(1, 0)});
                }
                (void)scale;
                out.H[out.n++] = congruence(Hz, S);
            }
            return true;
        };
        // first-child chains (Q = prepend at VV) and, by reversal, the last-child chains
        Reg first[4]; bool haveFirst[4] = {false, false, false, false};
        Aff AV = affine(prepend, VV);
        if (AV.ok) {
            // H_y must contain the non-first children G_y and prepend(H_y1) (y1 the first
            // child's letter): s_y = min(g_y, mu s_y1 + c), iterated to the fixed point
            double g[4], sF[4];
            for (int y = 0; y < 4; ++y) {
                g[y] = 1e300;
                for (size_t i = 1; i < cs[y].size(); ++i) g[y] = std::min(g[y], smin(AV.S, AV.e, cs[y][i]));
                sF[y] = g[y];
            }
            for (int it = 0; it < 64; ++it)
                for (int y = 0; y < 4; ++y) sF[y] = std::min(g[y], AV.mu*sF[lidx(img[y][0])] + AV.c);
            for (int y = 0; y < 4; ++y)
                if (sF[y] >= AV.sstar && sF[y] <= AV.mu*sF[lidx(img[y][0])] + AV.c + 1e-12*std::abs(sF[y]))
                    { first[y] = halfplane(AV.S, AV.e, sF[y]); haveFirst[y] = true; }
        } else if (!parabolic(prepend)) {                  // a spiralling first chain
            vector<const vector<Smp>*> G[4], Wl[4];
            for (int y = 0; y < 4; ++y)
                for (size_t i = 0; i < cs[y].size(); ++i) { Wl[y].push_back(&cs[y][i]); if (i) G[y].push_back(&cs[y][i]); }
            for (int y = 0; y < 4; ++y) {
                vector<int> seq(44001); seq[0] = y;
                for (size_t j = 1; j < seq.size(); ++j) seq[j] = lidx(img[seq[j-1]][0]);
                Reg R;
                if (spiral(prepend, VV, seq, G, Wl, R)) { first[y] = R; haveFirst[y] = true; }
            }
        }
        if (!parabolic(prepend)) {                         // loxodromic: a disc avoiding the
            Vec q = repelling(prepend);                      // repelling fixed point
            for (int y = 0; y < 4; ++y) {
                Reg R;
                if (!qdisc(q, all(y), R)) continue;
                if (haveFirst[y]) regs[y].push_back(R); else { first[y] = R; haveFirst[y] = true; }
            }
        }
        for (int y = 0; y < 4; ++y) {
            if (haveFirst[y]) regs[y].push_back(first[y]);
            if (haveFirst[y ^ 1]) {                        // alpha_y = y(alpha_Y)
                Mat yi = matinv(rep.gen(L[y]));
                Reg R = first[y ^ 1];
                for (int i = 0; i < R.n; ++i) R.H[i] = congruence(R.H[i], yi);
                regs[y].push_back(R);
            }
        }
        // middle self-loops (alpha_y = G u Q(alpha_y), G the other children)
        for (int y = 0; y < 4; ++y)
            for (const Tip& tp : tips[y]) {
                Aff A = affine(tp.Q, tp.p);
                if (A.ok) {
                    double s0 = 1e300;
                    for (size_t i = 0; i < cs[y].size(); ++i) if ((int)i != tp.pos) s0 = std::min(s0, smin(A.S, A.e, cs[y][i]));
                    if (s0 >= A.sstar) regs[y].push_back(halfplane(A.S, A.e, s0));
                } else if (!parabolic(tp.Q)) {
                    vector<const vector<Smp>*> G[4], Wl[4];
                    for (size_t i = 0; i < cs[y].size(); ++i) { Wl[y].push_back(&cs[y][i]); if ((int)i != tp.pos) G[y].push_back(&cs[y][i]); }
                    Reg R;
                    if (spiral(tp.Q, tp.p, vector<int>(44001, y), G, Wl, R)) regs[y].push_back(R);
                }
                if (!parabolic(tp.Q)) {
                    Reg R;
                    if (qdisc(repelling(tp.Q), all(y), R)) regs[y].push_back(R);
                }
            }
#ifdef ZIPPER_REGCHECK
        // test build: check the regions against a much finer sampling (env REGCHK = chord)
        if (getenv("REGCHK")) {
            double thr = atof(getenv("REGCHK"));
            for (int y = 0; y < 4; ++y) {
                vector<Smp> fs;
                bool ok = sample_arc(identity(), L[y], thr, fs, 60000000);
                auto val = [](const Herm& H, const Vec& w) {
                    double nrm = std::norm(w.u) + std::norm(w.v);
                    return (H.p*std::norm(w.u) + H.s*std::norm(w.v) + 2.0*(std::conj(w.u)*H.q*w.v).real())/nrm; };
                size_t bad = 0; double worst = 0; int ntwo = 0;
                for (const Reg& R : regs[y]) {
                    ntwo += R.n > 1;
                    for (auto& w : fs) {
                        double v = 1e300;
                        for (int i = 0; i < R.n; ++i) v = std::min(v, val(R.H[i], w.w));
                        if (v > 1e-12) { ++bad; worst = std::max(worst, v); }
                    }
                }
                fprintf(stderr, "REGCHK letter %c: %zu fine samples (complete %d), %zu regions (%d unions), %zu tips, outside %zu (worst %.3g)\n",
                        L[y], fs.size(), (int)ok, regs[y].size(), ntwo, tips[y].size(), bad, worst);
            }
        }
#endif
    }
    // the farthest point of the cap G^-1(D_H0) from the segment AB (an upper bound: distance
    // of the centre plus the chordal radius); -1 if degenerate
    static double cap_dev(const Herm& H0, const Mat& G, const Pt& A, const Pt& B) {
        Herm H = congruence(H0, G);
        double nx = H.q.real(), ny = H.q.imag(), nz = 0.5*(H.s - H.p);
        double nn = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (!(nn > 0) || !std::isfinite(nn)) return -1;
        double in = 1.0/nn;
        Pt C{-nx*in, -ny*in, -nz*in};
        double rc = 2.0 - (H.p + H.s)*in;                   // (chordal radius)^2 = 2 - 2 cos r
        if (!(rc >= 0)) rc = 0;
        return seg_dist(C, A, B) + std::sqrt(rc);
    }
    // an upper bound for the distance of the limit arc of (g, x) from the segment AB: the least
    // over the regions; the search stops at the first value <= stop (0 if there is no region)
    double region_dev(const Mat& g, char x, const Pt& A, const Pt& B, double stop) const {
        const vector<Reg>& R = regs[lidx(x)];
        Mat G = { g.d, -g.b, -g.c, g.a };                   // g^-1 (det g = 1)
        double best = 1e300;
        for (const Reg& rg : R) {
            double d = 0;
            for (int i = 0; i < rg.n && d >= 0 && d < best; ++i) {
                double di = cap_dev(rg.H[i], G, A, B);
                d = di < 0 ? -1 : std::max(d, di);
            }
            if (d < 0 || d >= best) continue;
            best = d;
            if (best <= stop) return best;
        }
        return best < 1e299 ? best : 0;
    }
    // -1: the whole limit arc of the branch at element g with label index k misses the view
    // cap; +1: it lies inside it (no descendant needs testing); 0: undecided
    int cap_test(const Mat& g, int k) const {
        if (capAll[k]) return 0;
        Mat G = { g.d, -g.b, -g.c, g.a };                 // g^-1 (det g = 1)
        Herm H = congruence(capH[k], G);
        double nx = H.q.real(), ny = H.q.imag(), nz = 0.5*(H.s - H.p);
        double nn = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (!(nn > 0) || !std::isfinite(nn)) return 0;
        Pt C{-nx/nn, -ny/nn, -nz/nn};
        double r = std::atan2(capSin[k]/nn, 0.5*(H.p + H.s)/nn);
        double th = angle3(C, vaxis);
        if (th > r + vrad + 1e-9) return -1;
        if (th + r < vrad - 1e-9) return 1;
        return 0;
    }

    size_t npoly() const { return pstart.size(); }
    size_t poly_begin(size_t k) const { return pstart[k]; }
    size_t poly_end(size_t k) const { return k+1 < pstart.size() ? pstart[k+1] : px.size(); }
    void clear_output() { px.clear(); py.clear(); pz.clear(); pstart.clear(); open = false; }

    void push_point(double x, double y, double z) {
        px.push_back((float)x); py.push_back((float)y); pz.push_back((float)z);
    }
    typedef std::array<int64_t,3> Q3;                 // a point rounded to 1e-9
    static Q3 q3(const Pt& P) {
        return { (int64_t)std::llround(P.x*1e9), (int64_t)std::llround(P.y*1e9),
                 (int64_t)std::llround(P.z*1e9) };
    }
    void emit_leaf(const Pt& Pi, const Pt& Pf) {
        ++nLeaves;
        if (!open) { pstart.push_back((uint32_t)px.size()); push_point(Pi.x, Pi.y, Pi.z); open = true; }
        if ((float)Pf.x != px.back() || (float)Pf.y != py.back() || (float)Pf.z != pz.back())
            push_point(Pf.x, Pf.y, Pf.z);          // (a leaf below float resolution adds no point)
        if (stats && dist3(Pi, Pf) >= SUBFLOAT) {   // residual overdraw: identical leaf segments
            Q3 u = q3(Pi), w = q3(Pf);
            if (w < u) std::swap(u, w);
            uint64_t h = 0x243F6A8885A308D3ULL;
            for (int64_t c : {u[0], u[1], u[2], w[0], w[1], w[2]}) h = mix64(h ^ (uint64_t)c);
            segHashes.push_back(h);
        }
    }
    HKey branch_key(const Mat& base, const Mat& basex, char letter) const {
        bool up = (letter=='A' || letter=='B');
        return up ? hkey(basex, (uint64_t)(letter - 'A' + 'a')) : hkey(base, (uint64_t)letter);
    }
    // Look-ahead below a would-be leaf (base, letter) with endpoints A, B.  Returns true if
    // one of its first-level vertices is farther than the accuracy from the chord AB (a
    // spike: split).  Otherwise, if overlap resolution is on, dbuf receives the keys of its
    // sub-branches down to `overlapLevels` levels below it (below the first level, only the
    // children of sub-branches whose chord is at least accuracy/8).  The first level is
    // needed even below tiny leaves: at large R most roots are single tiny leaves whose
    // train paths overlap (at -R 8 -a 0.02 it makes the EPS 26% smaller).  The default is
    // one level, the children.  Deeper levels remove more of the (invisible) overdraw but
    // cost much more -- near a parabolic point sub-branches shrink slowly, so the look-ahead
    // is large -- and do not make the EPS smaller (leaves split for an overlap break
    // polylines).  Measured at the defaults, overdraw Z+/Z- for n=2 and n=inf, and the time
    // for n=2:  off 16/18%, 1.9/17%, 0.55 s;  1 level 5.7/6.4%, 0.03/6.3%, 0.7 s;  2 levels
    // 1.7/2.1%, 0.03/2.8%, 1.1 s;  3 levels 1.0/1.4%, 0.03/2.1%, 1.4 s.  For n=inf at
    // -a 1e-4 -R 2, where it removes nothing: 3.5 s / 6.3 s for 1 / 2 levels.
    struct Desc { HKey k; Mat n, n2; char x; };
    struct DF { Mat b; char l; int d; Pt A, B; };
    vector<Desc> dbuf;
    vector<DF> dstk;
    bool lookahead(const Mat& base, char letter, const Pt& A, const Pt& B) {
        dbuf.clear(); dstk.clear();
        dstk.push_back({base, letter, 0, A, B});
        const double thr = accuracy/8;
        while (!dstk.empty()) {
            DF f = dstk.back(); dstk.pop_back();
            const string& u = img[lidx(f.l)];
            const bool coll = overlapLevels > 0;
            Mat N = matmul(f.b, prepend);
            Pt s = f.A;
            for (size_t i = 0; i < u.size(); ++i) {
                Mat N2 = matmul(N, rep.gen(u[i]));
                Pt e = (i + 1 == u.size()) ? f.B : project(act(N2, VV));
                if (f.d == 0 && i + 1 < u.size() && seg_dist(e, A, B) > accuracy) return true;
                if (coll) {
                    dbuf.push_back({branch_key(N, N2, u[i]), N, N2, u[i]});
                    if (f.d + 1 < overlapLevels && dist3(s, e) >= thr) dstk.push_back({N, u[i], f.d+1, s, e});
                }
                s = e; N = N2;
            }
        }
        for (auto& e : dbuf) done.prefetch(e.k);
        return false;
    }
    // --stats: every key is recorded once, when it is first inserted, with the sum s of its
    // branch's endpoints (which does not depend on their order); every later hit on a key is
    // compared with that record in finish_stats: a hit whose endpoints differ is a FALSE-MERGE,
    // two different branches with the same key.  To save memory a record keeps only a 32-bit
    // hash of the cell of s in a grid of step q (and the exponent of q), and a hit matches if
    // its own s lies in that cell or the nearest neighbouring one in each coordinate: so sums
    // that agree to q/2 always match, and sums 2q apart never do.  q = 1e-9 * 2^e is the
    // smallest such step of at least 2% of the branch's chord: two different branches with
    // the same key differ by about their size, while two routes to the same branch agree to
    // ~1e-13 at ordinary views but only to ~0.2% of the chord deep in a cusp crop (6.6e-9 for
    // a chord of 4.7e-6 at zoom 1000; a fixed 1e-9 grid reported those as false merges).  The
    // deviation itself is measured exactly on the 1/16 of the keys (by hash) whose sums are
    // kept in full.
    static int cell_exp(double chord) {
        int e = 0;
        while (e < 60 && 1e-9*std::ldexp(1.0, e) < 0.02*chord) ++e;
        return e;
    }
    static uint32_t cell_hash(const double* v, const int* off, int e) {
        const double sc = 1e9*std::ldexp(1.0, -e);
        uint64_t h = 0x13198A2E03707344ULL ^ (uint64_t)e;
        for (int i = 0; i < 3; ++i) h = mix64(h ^ (uint64_t)((int64_t)std::floor(v[i]*sc) + off[i]));
        return (uint32_t)(h >> 32);
    }
    // (Branches with a chord below SUBFLOAT are not checked: deep in a cusp their endpoints,
    // computed from long products, differ by more than their size between two routes to the
    // same branch -- up to 1.6e-8 at zoom 300 -- so the check cannot tell them apart.)
    static constexpr double SUBFLOAT = 1e-7;
    size_t nUnchecked = 0;
    void record_ends(const HKey& k, const Pt& A, const Pt& B) {
        if (dist3(A, B) < SUBFLOAT) { ++nUnchecked; return; }
        EndRec r{flatkey(k), {A.x + B.x, A.y + B.y, A.z + B.z}};
        const int z[3] = {0, 0, 0}, e = cell_exp(dist3(A, B));
        cellRecs.push_back({r.k, cell_hash(r.s, z, e), (uint8_t)e});
        if ((mix64(r.k) & 15) == 0) exactRecs.push_back(r);
    }
    void check_hit(const HKey& k, const Pt& Pi, const Pt& Pf) {
        if (dist3(Pi, Pf) < SUBFLOAT) return;
        hitRecs.push_back({flatkey(k), {Pi.x + Pf.x, Pi.y + Pf.y, Pi.z + Pf.z}});
    }
    void finish_stats() {
        // (the hits are few: sort them, and look every record up among them; hits on culled
        // keys have no record)
        auto byKey = [](const EndRec& p, const EndRec& q) { return p.k < q.k; };
        vector<EndRec> hits = hitRecs.flatten();
        std::sort(hits.begin(), hits.end(), byKey);
        for (size_t i = 0; i < cellRecs.size() && !hits.empty(); ++i) {
            const CellRec r = cellRecs[i];
            EndRec probe{r.k, {0, 0, 0}};
            auto rg = std::equal_range(hits.begin(), hits.end(), probe, byKey);
            const double sc = 1e9*std::ldexp(1.0, -(int)r.e);
            for (auto it = rg.first; it != rg.second; ++it) {
                int alt[3];
                for (int c = 0; c < 3; ++c) {
                    double f = it->s[c]*sc - std::floor(it->s[c]*sc);
                    alt[c] = f < 0.5 ? -1 : 1;
                }
                bool match = false;
                for (int m = 0; m < 8 && !match; ++m) {
                    int off[3] = { (m & 1) ? alt[0] : 0, (m & 2) ? alt[1] : 0, (m & 4) ? alt[2] : 0 };
                    match = cell_hash(it->s, off, r.e) == r.h;
                }
                if (!match) ++falseMerge;
            }
        }
        for (size_t i = 0; i < exactRecs.size() && !hits.empty(); ++i) {
            const EndRec& r = exactRecs[i];
            auto rg = std::equal_range(hits.begin(), hits.end(), r, byKey);
            for (auto it = rg.first; it != rg.second; ++it)
                maxMismatch = std::max({maxMismatch, std::abs(it->s[0] - r.s[0]),
                                        std::abs(it->s[1] - r.s[1]), std::abs(it->s[2] - r.s[2])});
        }
        cellRecs.clear(); exactRecs.clear(); vector<EndRec>().swap(hits);
        vector<uint64_t> sh = segHashes.flatten();
        std::sort(sh.begin(), sh.end());
        for (size_t i = 1; i < sh.size(); ++i) if (sh[i] == sh[i-1]) ++geomDup;
    }

    // Depth-first splitting of the branch (base0, letter0) with an explicit stack; the
    // children are processed in order.  A frame is a branch, or (kind 1) a chord A -> B that
    // stands for a run of sub-branches of a parabolic chain (see split_children).
    struct Frame { Mat base; char letter; char kind; int depth; bool inside; Pt A, B; bool fine = false; };
    // The stack grows in chunks (tip chains can push 10^5 frames at once, and a vector's
    // doubling then allocated hundreds of MB at a time, past the memory guard); chunks are
    // freed again as it shrinks.
    struct FrameStack {
        static constexpr size_t CH = (size_t)1 << 15;
        vector<std::unique_ptr<Frame[]>> ch;
        size_t n = 0;
        void push_back(const Frame& f) {
            if (n == ch.size()*CH) ch.emplace_back(new Frame[CH]);
            ch[n/CH][n%CH] = f; ++n;
        }
        const Frame& back() const { return ch[(n-1)/CH][(n-1)%CH]; }
        void pop_back() { --n; if (ch.size() > 2 && n + 2*CH < ch.size()*CH) ch.pop_back(); }
        bool empty() const { return n == 0; }
        size_t size() const { return n; }
        void clear() { n = 0; }
        void release() { ch.clear(); n = 0; }
    };
    FrameStack stk;
    // Memory guard: drawing stops (memFull, with a warning; the program then exits with status
    // 3) before the process holds DRAW_MEM bytes -- e.g. a deep cusp crop at a very fine -a,
    // where the spikes need ~size/accuracy levels each.  The key set checks the limit before
    // each fresh allocation (KeySet::may_grow); everything else grows slowly and is checked
    // every 2^16 units of work.  Z+ frees its branch keys, and returns them to the system,
    // before Z- is drawn, so Z- has the same budget.
    static constexpr double DRAW_MEM = 4.0e9;   // (+2 GB with --stats; hidden --memlimit GB)
    double draw_mem() const { return g_drawMem > 0 ? g_drawMem : DRAW_MEM + (stats ? 2.0e9 : 0.0); }
    bool memFull = false;
    size_t guardTick = 0, nextCheck = 0;
    void draw_arc(const Mat& base0, char letter0) {
        stk.clear();
        if (memFull) return;
        stk.push_back({base0, letter0, 0, 0, !cull, Pt{0,0,0}, Pt{0,0,0}});
        while (!stk.empty()) {
            if (done.full()) { memFull = true; stk.release(); open = false; break; }
            // checked every 2^16 units of work (frames visited or pushed, chain levels walked,
            // each a branch key or a frame): a frame can walk chains of 10^5 levels, so a count
            // of frames alone let the memory overshoot the limit by more than a GB
            if (++guardTick >= nextCheck) {
                nextCheck = guardTick + 65536;
                // (with room for the next doubling of the point arrays, which would otherwise
                // overshoot the limit by up to their size; the key set checks its own growth)
                double grow = 12.0*px.capacity() + 4.0*pstart.capacity();
                if (resident_bytes() + grow > draw_mem()) {
                    memFull = true; stk.release(); open = false; break;
                }
            }
            Frame f = stk.back(); stk.pop_back();
            if (f.kind == 1) { emit_leaf(f.A, f.B); ++nGapLeaves; }
            else visit(f.base, f.letter, f.depth, f.inside, f.fine);
        }
    }
    // (fine: below a branch whose chord is at most the accuracy; counts for dev_budget)
    void visit(const Mat& base, char letter, int depth, bool inside, bool fine) {
        Mat basex = matmul(base, rep.gen(letter));
        HKey key = branch_key(base, basex, letter);
        if (!done.insert(key)) {                                     // drawn or covered elsewhere
            if (done.full()) { open = false; return; }               // (or the memory limit)
            ++nSkipped; open = false;
            if (stats) check_hit(key, project(act(base, VV)), project(act(basex, VV)));
            return;
        }
        ++nVisited;
        if (!inside) {                                               // view culling
            bool up = (letter=='A' || letter=='B');
            int t = cap_test(up ? basex : base, (letter=='a' || letter=='A') ? 0 : 1);
            if (t < 0) { ++nCulled; open = false; return; }
            inside = t > 0;
        }
        Pt Pi = project(act(base, VV)), Pf = project(act(basex, VV));
        if (stats) record_ends(key, Pi, Pf);
        double chord = dist3(Pi, Pf);
        bool split = chord > accuracy && depth < maxdepth, haveDesc = false;
        if (!split && depth < maxdepth) {
            if (lookahead(base, letter, Pi, Pf)) { split = true; ++nSpike; }
            else if (region_dev(base, letter, Pi, Pf, devFactor*accuracy) > devFactor*accuracy) {
                if (dev_budget()) { split = true; ++nDev; } else ++nDevCapped;   // (a thin spike)
            }
            else if (overlapLevels > 0) {
                haveDesc = true;
                for (auto& e : dbuf) if (done.count(e.k)) { split = true; ++nOverlap; break; }
            }
        }
        if (split) split_children(base, letter, depth, inside, Pi, Pf, fine || chord <= accuracy);
        else {
            emit_leaf(Pi, Pf);
            if (fine) ++nDevLeaves;
            if (haveDesc)                         // these sub-branches are covered by the chord:
                for (auto& e : dbuf) {                // those whose arc provably lies within the
                    if (regs[lidx(e.x)].empty() ||    // accuracy of it (else they are drawn
                        region_dev(e.n, e.x, Pi, Pf, accuracy) > accuracy) continue;   // again)
                    if (done.insert(e.k) && stats)
                        record_ends(e.k, project(act(e.n, VV)), project(act(e.n2, VV)));
                }
            if (chord > accuracy) { ++nCapped; maxCappedChord = std::max(maxCappedChord, chord); }
            if (stats) {
                leafBatch.push_back({base, letter}); leafLen += chord;
                if (leafBatch.size() >= ((size_t)1 << 18)) refine_batch();
            }
        }
    }

    // PARABOLIC CHAINS.  Splitting (g,x) once gives children c_0 .. c_{m-1}; c_0 starts at
    // g.VV and c_{m-1} ends at g x.VV.  Repeating, the first children F_k = (g P^k, x_k) of
    // the first children all start at g.VV (P = prepend fixes VV), and the last children L_k
    // all end at g x.VV.  If P is loxodromic these chains shrink geometrically, but for the
    // cusp (n = inf) P is PARABOLIC and F_k shrinks only like 1/k: a branch of size s needs
    // ~s/accuracy splitting steps before its first child is below the accuracy, and each
    // step peels off siblings of size ~s/k^2 -- mostly far below the accuracy.  So the chains
    // are walked directly: F_1, F_2, ... are computed (one product per step) until F_K is
    // below the accuracy (chord and region bound, see visit), already drawn, or off the view; F_1 .. F_{K-1} are marked as split
    // (their keys go into `done`, exactly as if each had been visited and split), F_K becomes
    // an ordinary frame, and the siblings of the steps 1 .. K-1 follow in drawing order.  A
    // run of consecutive steps is drawn as ONE chord when that chord is at most the accuracy
    // and every sibling in the run provably lies within accuracy/2 of it (its arc lies in the
    // Mobius image of the base disc D_x, a round cap computed exactly); otherwise the run is
    // halved, down to single steps whose siblings become ordinary frames.  Nothing is decided
    // by geometry except where to stop splitting; the chains are the same branches ordinary
    // splitting would visit (and with no run merged the output is the same), so the identity
    // of every drawn branch is still exact.  (The siblings inside a merged run are not keyed:
    // were one of them reached again from elsewhere it would be drawn twice -- never missed.)
    struct CEl { Mat g, gx; char x; Pt P; };     // chain element: branch (g,x); P its moving end
    struct Sib { Mat g; char x; };
    vector<CEl> chn;
    vector<Mat> cN;
    vector<Frame> items;                         // the children of one split, in drawing order
    struct Step { Pt from, to; uint32_t s0, s1; int d; bool ok; };   // siblings [s0,s1) of sibv
    vector<Step> steps;
    vector<Sib> sibv;
    vector<uint32_t> badPrefix;
    size_t nGapLeaves = 0, nGapSteps = 0, nChainSteps = 0, nTipSteps = 0;
    bool chainAccel = true;

    // --stats: the farthest vertex of the refinement of branch (g,x) to chord accuracy/16
    // from the segment AB
    double maxGapDev = 0;
    double refine_dev(const Mat& g0, char x0, const Pt& A, const Pt& B) const {
        struct F { Mat b; char l; int d; };
        vector<F> st{{g0, x0, 0}};
        double far = 0; size_t budget = 20000;
        while (!st.empty() && budget--) {
            F f = st.back(); st.pop_back();
            Mat fx = matmul(f.b, rep.gen(f.l));
            Pt P = project(act(f.b, VV)), Q = project(act(fx, VV));
            far = std::max({far, seg_dist(P, A, B), seg_dist(Q, A, B)});
            for (const Tip& t : tips[lidx(f.l)]) far = std::max(far, seg_dist(project(act(f.b, t.p)), A, B));
            if (dist3(P, Q) > accuracy/16 && f.d < 60) {
                Mat N = matmul(f.b, prepend);
                for (char ch : img[lidx(f.l)]) { st.push_back({N, ch, f.d+1}); N = matmul(N, rep.gen(ch)); }
            }
        }
        return far;
    }

    // Walk the first (first = true) or last chain of the branch being split, starting at
    // its child (g1, x1); `fixedEnd` is the common endpoint.  Fills chn; returns K = size.
    size_t walk_chain(bool first, const Mat& g1, char x1, const Pt& fixedEnd, int depth,
                      bool inside, int& insideFrom, vector<HKey>& keys) {
        chn.clear(); keys.clear();
        insideFrom = inside ? 0 : -1;
        Mat g = g1; char x = x1;
        for (size_t k = 0; ; ++k) {
            Mat gx = matmul(g, rep.gen(x));
            Pt P = project(act(first ? gx : g, VV));
            chn.push_back({g, gx, x, P});
            if (depth + (int)k >= maxdepth || k >= TIP_CAP) break;   // (a longer chain continues from its head)
            if (dist3(P, fixedEnd) <= accuracy) {          // small: resolved unless its region
                if (!dev_budget()) break;                  // bound is too large (then it would be
                double d = first ? region_dev(g, x, fixedEnd, P, devFactor*accuracy)  // split
                                 : region_dev(g, x, P, fixedEnd, devFactor*accuracy); // anyway)
                if (d <= devFactor*accuracy) break;
                ++nDev;
            }
            HKey key = branch_key(g, gx, x);
            if (done.count(key)) break;                   // drawn elsewhere: the head is skipped
            if (insideFrom < 0) {
                bool up = (x=='A' || x=='B');
                int t = cap_test(up ? gx : g, (x=='a' || x=='A') ? 0 : 1);
                if (t < 0) break;                         // off the view: the head is culled
                if (t > 0) insideFrom = (int)k;
            }
            keys.push_back(key);
            const string& v = img[lidx(x)];
            Mat N = matmul(g, prepend);
            if (first) { g = N; x = v[0]; }
            else { for (size_t i = 0; i + 1 < v.size(); ++i) N = matmul(N, rep.gen(v[i])); g = N; x = v.back(); }
        }
        return chn.size();
    }

    // append the frames for drawing-order steps [i0, i1) to `items`, merging runs into chords
    void emit_steps(size_t i0, size_t i1) {
        if (i1 - i0 >= 2 && badPrefix[i1] == badPrefix[i0]) {
            const Pt& A = steps[i0].from; const Pt& B = steps[i1-1].to;
            if (dist3(A, B) <= accuracy) {
                bool ok = true;
                for (size_t i = i0; i < i1 && ok; ++i)
                    for (uint32_t t = steps[i].s0; t < steps[i].s1; ++t)
                        if (regs[lidx(sibv[t].x)].empty() ||
                            region_dev(sibv[t].g, sibv[t].x, A, B, 0.5*accuracy) > 0.5*accuracy) { ok = false; break; }
                if (ok) {
                    items.push_back({identity(), 0, 1, 0, true, A, B});
                    nGapSteps += i1 - i0;
                    if (stats)                            // check the bound on the actual curve
                        for (size_t i = i0; i < i1; ++i)
                            for (uint32_t t = steps[i].s0; t < steps[i].s1; ++t)
                                maxGapDev = std::max(maxGapDev, refine_dev(sibv[t].g, sibv[t].x, A, B)/accuracy);
                    return;
                }
            }
        }
        if (i1 - i0 == 1) {
            for (uint32_t t = steps[i0].s0; t < steps[i0].s1; ++t)
                items.push_back({sibv[t].g, sibv[t].x, 0, steps[i0].d, steps[i0].ok, Pt{0,0,0}, Pt{0,0,0}});
            return;
        }
        size_t mid = (i0 + i1)/2;
        emit_steps(i0, mid); emit_steps(mid, i1);
    }

    // one side (first or last chain) of the split of (base,letter), appended to `items`
    vector<HKey> ckeys;
    void chain_side(bool first, const Mat& g1, char x1, const Pt& fixedEnd, int depth, bool inside) {
        int insideFrom;
        size_t K = walk_chain(first, g1, x1, fixedEnd, depth, inside, insideFrom, ckeys);
        const Frame head{chn[K-1].g, chn[K-1].x, 0, depth + (int)K - 1,
                         insideFrom >= 0 && (int)K - 1 >= insideFrom, Pt{0,0,0}, Pt{0,0,0}};
        if (K == 1) { items.push_back(head); return; }
        for (size_t k = 0; k + 1 < K; ++k) {             // F_1 .. F_{K-1}: split
            if (done.insert(ckeys[k]) && stats)
                record_ends(ckeys[k], project(act(chn[k].g, VV)), project(act(chn[k].gx, VV)));
        }
        nChainSteps += K - 1;
        nVisited += K - 1;
        guardTick += K;                                   // (for the memory guard)
        // steps in drawing order: first chain K-2 .. 0 (outwards from g.VV), last chain 0 .. K-2
        steps.clear(); sibv.clear();
        for (size_t i = 0; i + 1 < K; ++i) {
            size_t s = first ? K - 2 - i : i;
            const CEl& e = chn[s];
            const string& v = img[lidx(e.x)];
            Step st;
            st.from = first ? chn[s+1].P : e.P;
            st.to   = first ? e.P : chn[s+1].P;
            st.ok = insideFrom >= 0 && (int)s >= insideFrom;
            st.d = depth + (int)s + 1;
            st.s0 = (uint32_t)sibv.size();
            Mat N = matmul(e.g, prepend);
            for (size_t j = 0; j < v.size(); ++j) {
                bool sib = first ? j > 0 : j + 1 < v.size();
                if (sib) sibv.push_back({N, v[j]});
                N = matmul(N, rep.gen(v[j]));
            }
            st.s1 = (uint32_t)sibv.size();
            steps.push_back(st);
        }
        // a step can be merged only if its span is below accuracy/2 (cheap pre-filter)
        badPrefix.assign(steps.size() + 1, 0);
        for (size_t i = 0; i < steps.size(); ++i)
            badPrefix[i+1] = badPrefix[i] + (dist3(steps[i].from, steps[i].to) > 0.5*accuracy ? 1 : 0);
        if (first) { items.push_back(head); emit_steps(0, steps.size()); }
        else       { emit_steps(0, steps.size()); items.push_back(head); }
    }

    // TIP CHAINS.  A middle child at position i of (g, x) with the letter x again (a Tip: child
    // i of (g, x) is (g Q, x)) starts the chain M_k = (g Q^k, x), whose arcs all pass through
    // the tip g.p (p the fixed point of Q) and shrink to it.  At the cusp Q is parabolic and
    // the region bound of M_k (the true distance of the spike's tip from its chord) decreases
    // only like 1/k, while its chord shrinks like 1/k^2: a spike of size s needs ~s/accuracy
    // levels.  The chain is walked directly (one product per level) until M_K is resolved
    // (chord and region bound at most the accuracy), drawn elsewhere, or off the view;
    // M_1 .. M_{K-1} are marked as split, and their siblings become ordinary frames in drawing
    // order (the left ones, before position i, inwards; M_K; the right ones outwards) -- the
    // same branches ordinary splitting visits.  These levels are bounded by the chain itself
    // (at most TIP_CAP per walk; a longer chain continues from its head), so they do not count
    // against the split budget for thin spikes, which a deep cusp zoom used to exhaust.  (The
    // siblings are not merged into runs like those of the end chains: a run of them can be the
    // same arcs as an end-chain run of another branch, or their parents or children -- e.g.
    // for fig8 the tip chain of (h, a) and the last chain of (h, B) share their left
    // siblings -- and keying every run to avoid drawing it twice cost more than it saved.)
    static const size_t TIP_CAP = 100000;
    vector<CEl> tchn;
    vector<HKey> tkeys;
    void tip_side(const Tip& tp, const Mat& g1, char x, int depth, bool inside) {
        tchn.clear(); tkeys.clear();
        int insideFrom = inside ? 0 : -1;
        Mat g = g1;
        for (size_t k = 0; ; ++k) {
            Mat gx = matmul(g, rep.gen(x));
            Pt A = project(act(g, VV)), B = project(act(gx, VV));
            tchn.push_back({g, gx, x, A});
            if (depth + (int)k >= maxdepth || k >= TIP_CAP) break;
            if (dist3(A, B) <= accuracy &&                // small and its region bound resolved
                region_dev(g, x, A, B, devFactor*accuracy) <= devFactor*accuracy) break;
            HKey key = branch_key(g, gx, x);
            if (done.count(key)) break;                   // drawn elsewhere: the head is skipped
            if (insideFrom < 0) {
                bool up = (x=='A' || x=='B');
                int t = cap_test(up ? gx : g, (x=='a' || x=='A') ? 0 : 1);
                if (t < 0) break;                         // off the view: the head is culled
                if (t > 0) insideFrom = (int)k;
            }
            tkeys.push_back(key);
            g = matmul(g, tp.Q);
        }
        const size_t K = tchn.size();
        const Frame head{tchn[K-1].g, x, 0, depth + (int)K - 1,
                         insideFrom >= 0 && (int)K - 1 >= insideFrom, Pt{0,0,0}, Pt{0,0,0}};
        if (K == 1) { items.push_back(head); return; }
        for (size_t k = 0; k + 1 < K; ++k)
            if (done.insert(tkeys[k]) && stats)
                record_ends(tkeys[k], tchn[k].P, project(act(tchn[k].gx, VV)));
        nTipSteps += K - 1;
        nVisited += K - 1;
        guardTick += K;
        const string& v = img[lidx(x)];
        const size_t pos = (size_t)tp.pos;
        for (int side = 0; side < 2; ++side) {        // 0: left (inwards), 1: right (outwards)
            for (size_t i = 0; i + 1 < K; ++i) {
                size_t k = side == 0 ? i : K - 2 - i;
                const CEl& e = tchn[k];
                bool ok = insideFrom >= 0 && (int)k >= insideFrom;
                Mat N = matmul(e.g, prepend);
                for (size_t j = 0; j < v.size(); ++j) {
                    if (side == 0 ? j < pos : j > pos)
                        items.push_back({N, v[j], 0, depth + (int)k + 1, ok, Pt{0,0,0}, Pt{0,0,0}});
                    N = matmul(N, rep.gen(v[j]));
                }
            }
            if (side == 0) items.push_back(head);
        }
    }

    void split_children(const Mat& base, char letter, int depth, bool inside, const Pt& Pi, const Pt& Pf, bool fine) {
        const string& u = img[lidx(letter)];
        const size_t m = u.size();
        cN.clear();
        Mat N = matmul(base, prepend);
        for (char ch : u) { cN.push_back(N); N = matmul(N, rep.gen(ch)); }
        items.clear();
        if (!chainAccel || m < 2) {
            for (size_t i = 0; i < m; ++i) items.push_back({cN[i], u[i], 0, depth+1, inside, Pt{0,0,0}, Pt{0,0,0}});
        } else {
            // the tip chains: every middle self-loop (for most monodromies the parabolic one
            // is not the first)
            const vector<Tip>& tl = tips[lidx(letter)];
            chain_side(true, cN[0], u[0], Pi, depth + 1, inside);
            for (size_t i = 1; i + 1 < m; ++i) {
                const Tip* tp = nullptr;
                if (u[i] == letter) for (const Tip& t : tl) if (t.pos == (int)i) { tp = &t; break; }
                if (tp) tip_side(*tp, cN[i], u[i], depth + 1, inside);
                else items.push_back({cN[i], u[i], 0, depth+1, inside, Pt{0,0,0}, Pt{0,0,0}});
            }
            Mat lastg = cN[m-1];
            chain_side(false, lastg, u[m-1], Pf, depth + 1, inside);
        }
        for (auto& it : items) it.fine = fine;
        guardTick += items.size();
        for (size_t i = items.size(); i-- > 0; ) stk.push_back(items[i]);   // last first
    }

    // draw all roots in the given order (nearest to the basepoint first: large branches first,
    // so smaller branches they contain become sub-polylines of them)
    void draw_roots(const vector<Mat>& roots) {
        done.set_limit(draw_mem());
        for (const Mat& M : roots) {
            open = false; draw_arc(M, 'a');
            open = false; draw_arc(M, 'b');
        }
        open = false;
        tlog("  split + emit");
        if (g_timing)
            fprintf(stderr, "timing:   (branches visited %zu, keys %zu, leaves %zu, points %zu, "
                    "polylines %zu; chain steps %zu + tip chain steps %zu, merged runs %zu of %zu steps)\n", nVisited,
                    done.size(), nLeaves, px.size(), pstart.size(), nChainSteps, nTipSteps, nGapLeaves, nGapSteps);
        done = KeySet();                          // (the branch keys are no longer needed)
        stk.release();
        release_memory();
        join_polylines();
        tlog("  join polylines");
        if (stats) {                              // (free the --stats tables as soon as done)
            finish_stats();
            refine_batch();
            measure_overdraw();
        }
    }

    // --stats: residual overdraw and spikes.  Every leaf is refined (exactly, by splitting)
    // to chord <= accuracy/16.  A piece of that refinement that also occurs in the tree of
    // another leaf (the same branch key) is a piece of curve covered by both (whether the two
    // leaves overlap partially or one lies deep inside the other); its length, halved per
    // occurrence, is the ink drawn twice.  (Pieces below accuracy/16, and overlaps that are not
    // aligned with the splitting at any finite depth, are not seen.)  To keep this cheap the
    // shared pieces are counted on a 1/16 SAMPLE of the fine pieces, chosen by a hash of the
    // key (so a piece is sampled in every leaf it occurs in, or in none), and the overdraw is
    // the sampled shared length over the sampled length.  The leaves are refined in batches,
    // in parallel, as they are drawn (refine_batch), so that only the samples are kept.
    // Also (on every leaf, not sampled): leaves whose refinement (its vertices and tips) strays
    // more than 2x the accuracy from the drawn chord, and the largest such distance.
    void refine_batch() {
        const double thr = accuracy/16;
        const size_t N = leafBatch.size();
        if (!N) return;
        struct Acc { vector<Samp> samp; double fine = 0, fineS = 0, maxSp = 0; size_t spikes = 0; };
        unsigned nt = N < 4096 ? 1u : std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
        vector<Acc> acc(nt);
        auto work = [&](unsigned t) {
            Acc& a = acc[t];
            struct F { Mat b; char l; int d; };
            vector<F> st;
            for (size_t li = N*t/nt; li < N*(t+1)/nt; ++li) {
                const auto& L = leafBatch[li];
                Mat Lx = matmul(L.first, rep.gen(L.second));
                Pt A = project(act(L.first, VV)), B = project(act(Lx, VV));
                st.clear(); st.push_back({L.first, L.second, 0});
                size_t budget = 20000; double far = 0;
                while (!st.empty() && budget) {
                    --budget;
                    F f = st.back(); st.pop_back();
                    Mat fx = matmul(f.b, rep.gen(f.l));
                    Pt P = project(act(f.b, VV)), Q = project(act(fx, VV));
                    double c = dist3(P, Q);
                    far = std::max({far, seg_dist(P, A, B), seg_dist(Q, A, B)});
                    for (const Tip& tp : tips[lidx(f.l)])       // (a tip is a point of the curve
                        far = std::max(far, seg_dist(project(act(f.b, tp.p)), A, B));  // too)
                    if (c > thr && f.d < 60) {
                        Mat Nn = matmul(f.b, prepend);
                        for (char ch : img[lidx(f.l)]) { st.push_back({Nn, ch, f.d+1}); Nn = matmul(Nn, rep.gen(ch)); }
                    } else {
                        a.fine += c;
                        uint64_t k = flatkey(branch_key(f.b, fx, f.l));
                        if ((mix64(k) & 15) == 0) { a.samp.push_back({k, (float)c}); a.fineS += c; }
                    }
                }
                if (far > 2*accuracy) ++a.spikes;
                a.maxSp = std::max(a.maxSp, far/accuracy);
            }
        };
        if (nt == 1) work(0);
        else { vector<std::thread> th; for (unsigned t = 0; t < nt; ++t) th.emplace_back(work, t);
               for (auto& x : th) x.join(); }
        for (auto& a : acc) {                          // (in thread order: deterministic)
            fineLen += a.fine; fineS += a.fineS; spikeLeaves += a.spikes; maxSpike = std::max(maxSpike, a.maxSp);
            for (const Samp& x : a.samp) { uint64_t k = x.k; odSamples[k >> 58].push_back(x); }
        }
        leafBatch.clear();
    }
    void measure_overdraw() {
        double sharedS = 0;
        overdrawSamples = 0;
        for (auto& bucket : odSamples) {                 // (one bucket at a time: less memory)
            vector<Samp> all = bucket.flatten();
            std::sort(all.begin(), all.end(), [](const Samp& p, const Samp& q) {
                uint64_t pk = p.k, qk = q.k; float pc = p.c, qc = q.c;
                return pk < qk || (pk == qk && pc < qc); });
            for (size_t i = 0; i < all.size(); ) {
                size_t j = i; double len = 0; uint64_t k = all[i].k;
                while (j < all.size() && all[j].k == k) { float c = all[j++].c; len += c; }
                if (j - i >= 2) sharedS += 0.5*len;
                i = j;
            }
            overdrawSamples += all.size();
        }
        extraLen = fineS > 0 ? fineLen*sharedS/fineS : 0.0;
        vector<std::pair<Mat,char>>().swap(leafBatch);
    }

    // Concatenate polylines where one ends exactly where another begins (the common vertex
    // g.VV, computed along two routes, agrees to ~1e-12; we require equality of the stored
    // float coordinates).  A polyline breaks wherever a branch was already drawn elsewhere, and that
    // branch starts at the break point, so this rejoins them.  Pure bookkeeping for smaller
    // output: it never changes what is drawn.  The polyline ends are grouped by point once
    // (hash table + counting sort); each group keeps a cursor past its used ends, so the
    // whole join is linear.
    void join_polylines() {
        size_t np = npoly();
        polysBeforeJoin = np;
        if (np >= 2) {
            const uint32_t ne = (uint32_t)(2*np);
            typedef std::array<uint32_t,3> F3;           // an end point: its float bit patterns
            auto endq = [&](uint32_t e) -> F3 {
                size_t i = (e & 1) ? poly_end(e>>1) - 1 : poly_begin(e>>1);
                auto fb = [](float f) { f += 0.0f; uint32_t u; std::memcpy(&u, &f, 4); return u; };
                return { fb(px[i]), fb(py[i]), fb(pz[i]) };
            };
            // group the ends by point: open-addressing hash table of distinct points, then a
            // counting sort of the ends by point id (linear time)
            size_t cap = 16; while (cap < 2*(size_t)ne) cap <<= 1;
            vector<uint32_t> slot(cap, 0xFFFFFFFFu);     // -> an end at that point
            vector<uint32_t> pid(ne);                    // per end: its point id
            uint32_t npts = 0;
            for (uint32_t e = 0; e < ne; ++e) {
                const F3 q = endq(e);
                size_t i = (size_t)mix64(((uint64_t)q[0] << 32 | q[1]) ^ mix64(q[2])) & (cap-1);
                for (;;) {
                    uint32_t t = slot[i];
                    if (t == 0xFFFFFFFFu) { slot[i] = e; pid[e] = npts++; break; }
                    if (endq(t) == q) { pid[e] = pid[t]; break; }
                    i = (i + 1) & (cap-1);
                }
            }
            vector<uint32_t>().swap(slot);
            vector<uint32_t> gstart(npts + 1, 0), ord(ne);
            for (uint32_t e = 0; e < ne; ++e) ++gstart[pid[e] + 1];
            for (uint32_t g = 0; g < npts; ++g) gstart[g+1] += gstart[g];
            vector<uint32_t> cur(gstart.begin(), gstart.end() - 1);   // per point: next unused
            for (uint32_t e = 0; e < ne; ++e) ord[cur[pid[e]]++] = e;  // (ends in index order)
            for (uint32_t g = 0; g < npts; ++g) cur[g] = gstart[g];
            vector<char> used(np, 0);
            auto take = [&](uint32_t e) -> int64_t {      // an unused end at the point of end e
                uint32_t g = pid[e], i = cur[g];
                while (i < gstart[g+1] && used[ord[i] >> 1]) ++i;
                cur[g] = i;
                return i < gstart[g+1] ? (int64_t)ord[i] : -1;
            };
            vector<float> nx, ny, nz; vector<uint32_t> ns;
            nx.reserve(px.size()); ny.reserve(px.size()); nz.reserve(px.size());
            vector<std::pair<uint32_t,bool>> fwd, bwd;    // (polyline, reversed)
            for (uint32_t k = 0; k < np; ++k) {
                if (used[k]) continue;
                used[k] = 1;
                fwd.assign(1, {k, false}); bwd.clear();
                for (uint32_t te = 2*k+1; ; ) {           // extend forwards from the tail
                    int64_t e = take(te); if (e < 0) break;
                    uint32_t m = (uint32_t)(e >> 1); bool atTail = e & 1;
                    used[m] = 1; fwd.push_back({m, atTail});
                    te = atTail ? 2*m : 2*m+1;
                }
                for (uint32_t he = 2*k; ; ) {             // extend backwards from the head
                    int64_t e = take(he); if (e < 0) break;
                    uint32_t m = (uint32_t)(e >> 1); bool atHead = !(e & 1);
                    used[m] = 1; bwd.push_back({m, atHead});
                    he = atHead ? 2*m+1 : 2*m;
                }
                ns.push_back((uint32_t)nx.size());
                bool first = true;
                auto emit = [&](uint32_t m, bool rev) {
                    size_t b = poly_begin(m), e = poly_end(m);
                    for (size_t t = first ? 0 : 1; t < e - b; ++t) {   // skip the shared joint
                        size_t i = rev ? e - 1 - t : b + t;
                        nx.push_back(px[i]); ny.push_back(py[i]); nz.push_back(pz[i]);
                    }
                    first = false;
                };
                for (size_t i = bwd.size(); i-- > 0; ) emit(bwd[i].first, bwd[i].second);
                for (auto& pr : fwd) emit(pr.first, pr.second);
            }
            px.swap(nx); py.swap(ny); pz.swap(nz); pstart.swap(ns);
        }
    }
};

// Seed enumeration by HYPERBOLIC distance in the 3-manifold.
//
// The zipper must be sampled uniformly by the visual (Patterson-Sullivan) measure on the
// sphere.  Because M is a finite-volume hyperbolic orbifold, that measure is the round
// Lebesgue measure, and the orbit of a basepoint o in H^3 equidistributes round on the
// sphere inside a hyperbolic ball d(o, g.o) <= R.  So we enumerate the group ELEMENTS g
// with d(o, g.o) <= R and draw the a- and b-branches at each g.  The relevant distance is
// the 3-MANIFOLD distance
//     cosh d(o, M.o) = ||M||_F^2 / 2 ,   o = j = (0,0,1),
// NOT any word length in the fiber group -- e.g. a c-shortcut reaches a deep fiber vertex
// at small 3-manifold distance, and must be sampled.
//
// COMPLETENESS.  A search of the Cayley graph of a,b,c restricted to the ball misses the
// elements whose every generator path leaves the ball (2-19% of B(6) for fig8, more for
// thin words, all within ~0.8 of its edge: an element near the edge can have all six
// neighbours g a^+-1, g b^+-1, g c^+-1 farther out).  The search below uses instead an
// enriched symmetric generating set S (a,b,c, their inverses and a few short elements,
// make_ball_gens) with the DESCENT
// property: every element g outside the small seed ball B(o, Kc) has an S-neighbour g s
// strictly nearer to o.  Then every element of B(R) is reached from a seed by a path of
// S-steps along which the distance increases -- the descent path read backwards, which
// stays in B(R) -- so the search may follow only steps that do not decrease the distance,
// and it finds the ball COMPLETELY, for every R.  The descent
// property is local and R-independent: by the hyperbolic law of cosines in the triangle
// o, g.o, gs.o, the step to gs is nearer iff the angle at o between the directions to
// s.o and to g^-1.o is below alpha_s(D) = arccos(tanh(l_s/2) coth D), with l_s =
// d(o, s.o) and D = d(o, g.o).  These caps grow with D, so if the caps of S at D = D0
// cover the sphere of directions, every element with d >= D0 has a nearer neighbour.  S is
// chosen greedily from the seed ball B(o, K) to cover a cube grid of 21600 cells (each cell
// wholly inside a cap, with a margin of 1e-3 rad), which certifies the property for d >= D0
// = K.  K starts at 3 and grows by 0.5 while cells stay uncovered (thin monodromies such as
// R^14 L need longer elements: without them the outward search lost up to 90% of the ball).
// The candidates include powers h P^k h^-1 of the peripheral element (see cover_level).
// The seed ball B(o, K) itself is found by a plain search with the generators a,b,c through
// B(K + 2): the margin 2 is more than the largest one any test needed (0.87 for fig8 for R
// up to 7, 1.2 for R^14 L).  At a cusp (and for large orders) the directions next to the
// cusp points cannot be covered with any margin: there the nearer neighbour is a parabolic
// step whose gain tends to zero.  These cells are tolerated (see cusp_cells); the greedy set
// still has the descent property there in every test (no element without a nearer
// neighbour among ~10^6 per test, nor among 3*10^6 elements up to distance 16 deep in the
// cusp), but it is not certified; --stats reports which holds.
//
// The search is best-first by distance from the basepoint, deduplicated by the group
// element itself (hkey, marked when first reached), so each element is stored once,
// reached along a near-geodesic word (short, so the matrix carries little rounding error).
// There is no word-length cap, and the torsion relation needs no special treatment.
static char inv_letter6(char ch) {
    switch (ch) { case 'a': return 'A'; case 'A': return 'a';
                  case 'b': return 'B'; case 'B': return 'b';
                  case 'c': return 'C'; case 'C': return 'c'; }
    return '?';
}
struct BallGens {
    vector<Mat> S;                // the enriched symmetric generating set (first: a,A,b,B,c,C)
    vector<Mat> seeds;            // the ball B(o, Kc), complete, nearest first (+ extraSeeds)
    double Kc = 3.0, D0 = 3.0;    // seed radius; descent certified for d >= D0 (= Kc)
    size_t cells = 0, uncovered = 0;   // cover certificate: grid cells not inside any cap,
    size_t tolerated = 0;              //   of which next to a cusp (or near-cusp) point
    size_t extraSeeds = 0;        // elements of B(Kc + 2) beyond Kc with no nearer S-neighbour
    double rEst = 0; size_t nEst = 0;  // |B(rEst)|: for size estimates
};
// Best-first search of the Cayley graph of `gens` from `seeds` within the region f <= thr,
// f(g) = ||g||_F^2 = 2cosh d(o, g.o), or with a focus Tf (`focusInv` = Tf^-1) the smaller
// of that and ||Tf^-1 g||_F^2 = 2cosh d(Tf.o, g.o) (`focusOnly`: the latter alone).  `outward`: from g follow only the
// steps to q with f(q) >= f(g) (valid when every element of the region outside the seeds
// has a neighbour with strictly smaller f).  Returns the elements with f <= keep (keep <=
// thr), nearest first.  At most `cap` elements are STORED (capHit is set).  `noDescent`:
// the number of non-seed elements found with no neighbour of smaller f (a diagnostic).
static vector<Mat> ball_search(const vector<Mat>& gens, const vector<Mat>& seeds, double thr,
                               double keep, const Mat* focusInv, bool outward, size_t cap,
                               bool* capHit = nullptr, size_t* noDescent = nullptr,
                               size_t expect = 0, bool focusOnly = false) {
    auto fval = [&](const Mat& M) {
        if (focusInv && focusOnly) return frob2(matmul(*focusInv, M));
        double k = frob2(M);
        if (focusInv) k = std::min(k, frob2(matmul(*focusInv, M)));
        return k;
    };
    const size_t ng = gens.size();
    KeySet seen;
    vector<Mat> pool;                                  // every element reached (in the region)
    vector<double> pd;                                 // its f
    vector<uint32_t> order;                            // pool indices, nearest first
    if (expect) { pool.reserve(std::min(expect, cap)); pd.reserve(std::min(expect, cap)); }
    // The queue is a bucket queue on log f ~ d, buckets of width 1e-3: a heap costs log n
    // per element, which dominated for large R.  An element nearer than the bucket being
    // expanded goes into that bucket (the expansion order is nearly monotone anyway).
    const double w = 1e-3;
    auto bucket = [&](double k) { return (size_t)std::max(0.0, std::floor(std::log(k)/w)); };
    vector<vector<uint32_t>> B(bucket(thr) + 2);
    size_t cur = B.size() - 1, nseed = 0;
    if (capHit) *capHit = false;
    if (noDescent) *noDescent = 0;
    for (const Mat& X : seeds) {
        double k = fval(X);
        if (!(k <= thr) || !seen.insert(hkey(X))) continue;
        if (pool.size() >= cap) { if (capHit) *capHit = true; break; }
        pool.push_back(X); pd.push_back(k);
        size_t b = std::min(bucket(k), B.size() - 1);
        B[b].push_back((uint32_t)(pool.size() - 1)); cur = std::min(cur, b);
    }
    nseed = pool.size();
    vector<Mat> Mc(ng); vector<double> kk(ng); vector<HKey> hk(ng);
    bool full = capHit && *capHit;
    while (!full) {
        while (cur < B.size() && B[cur].empty()) { vector<uint32_t>().swap(B[cur]); ++cur; }
        if (cur >= B.size()) break;
        uint32_t i = B[cur].back(); B[cur].pop_back();
        const Mat M = pool[i];                         // (copy: pool may reallocate below)
        const double kM = pd[i], kout = kM*(1.0 - 1e-9);
        // the neighbours; keys first, prefetched, since the hash lookups (cache misses)
        // dominate the cost
        int nn = 0; bool down = false;
        for (size_t t = 0; t < ng; ++t) {
            Mat X = matmul(M, gens[t]);
            double k = fval(X);
            if (!(k <= thr)) continue;                 // outside (NaN-safe: garbage is outside)
            if (k < kM) down = true;
            if (outward && k < kout) continue;         // nearer: reached from its own descent
            Mc[nn] = X; kk[nn] = k; hk[nn] = hkey(X); seen.prefetch(hk[nn]); ++nn;
        }
        if (!down && i >= nseed && noDescent) ++*noDescent;
        for (int t = 0; t < nn; ++t) {
            if (!seen.insert(hk[t])) continue;         // already reached
            if (pool.size() >= cap || pool.size() >= 0xFFFFFFF0u) { full = true; if (capHit) *capHit = true; break; }
            pool.push_back(Mc[t]); pd.push_back(kk[t]);
            B[std::min(std::max(bucket(kk[t]), cur), B.size() - 1)].push_back((uint32_t)(pool.size() - 1));
        }
    }
    vector<vector<uint32_t>>().swap(B);
    seen = KeySet();
    // sort nearest first (ties by pool index: deterministic), keep f <= keep, then permute
    // pool in place (all kept) or copy (a subset)
    order.resize(pool.size());
    for (size_t j = 0; j < pool.size(); ++j) order[j] = (uint32_t)j;
    std::sort(order.begin(), order.end(), [&](uint32_t p, uint32_t q) {
        return pd[p] != pd[q] ? pd[p] < pd[q] : p < q; });
    size_t nk = 0;
    while (nk < order.size() && pd[order[nk]] <= keep) ++nk;
    vector<double>().swap(pd);
    if (nk != pool.size()) {
        vector<Mat> out; out.reserve(nk);
        for (size_t j = 0; j < nk; ++j) out.push_back(pool[order[j]]);
        return out;
    }
    vector<char> placed(pool.size(), 0);               // pool[j] <- pool[order[j]], by cycles
    for (size_t j = 0; j < pool.size(); ++j) {
        if (placed[j]) continue;
        Mat tmp = pool[j];
        size_t k = j;
        for (;;) {
            placed[k] = 1;
            size_t nx = order[k];
            if (nx == j) { pool[k] = tmp; break; }
            pool[k] = pool[nx]; k = nx;
        }
    }
    return pool;
}
static vector<Mat> std_gens(const Rep& rep) {
    return { rep.A, rep.Ai, rep.B, rep.Bi, rep.C, rep.Ci };
}
// count group elements in a hyperbolic ball of radius R (capped) -- a cheap discreteness
// screen: a discrete (geometric) rep gives ~e^{2R} elements, a non-discrete one blows past
// the cap almost immediately.  (Plain a,b,c search: elements connected inside the ball.)
static size_t count_ball(const Rep& rep, double R, size_t cap) {
    double thr = 2.0*std::cosh(R);
    return ball_search(std_gens(rep), {identity()}, thr, thr, nullptr, false, cap).size();
}

// The direction at o = j of the point M.o (unit vector in the ball model; o is its centre).
static Pt ball_dir(const Mat& M) {
    double D = std::norm(M.d) + std::norm(M.c);
    cpx z = (M.b*std::conj(M.d) + M.a*std::conj(M.c))/D;     // M.j = (z, 1/D)
    double t = 1.0/D, x = z.real(), y = z.imag();
    double bx = 2*x, by = 2*y, bz = x*x + y*y + t*t - 1.0;
    double r = std::sqrt(bx*bx + by*by + bz*bz);
    return r > 1e-300 ? Pt{bx/r, by/r, bz/r} : Pt{0, 0, 0};
}
// One level of the construction (see the comment above): the seed ball B(o, K) (from the
// plain search `big` = B(o, K + 2) with a,b,c), and S chosen greedily from it to cover the
// sphere of directions at D0 = K.  `uncov`: the centres of the cells left uncovered.
static void cover_level(const Rep& rep, int order, double K, BallGens& bg, vector<Mat>& big, vector<Pt>& uncov) {
    bg.Kc = bg.D0 = K;
    const double Kc = K, M0 = 2.0, D0 = K;
    vector<Mat> g6 = std_gens(rep);
    double thr0 = 2.0*std::cosh(Kc + M0), thrK = 2.0*std::cosh(Kc);
    big = ball_search(g6, {identity()}, thr0, thr0, nullptr, false, 20000000);
    bg.rEst = Kc + M0; bg.nEst = big.size();
    size_t nK = 0;
    while (nK < big.size() && frob2(big[nK]) <= thrK) ++nK;
    bg.seeds.assign(big.begin(), big.begin() + nK);
    // candidates: the seed elements other than 1 and the generators, their inverses, and the
    // powers h P^k h^-1 (|k| <= min(n-1, 100), outside B(K) but shorter than 2K, where a cap
    // still exists) of the peripheral element P = [a,b] conjugated by the h in B(K) for which
    // h P h^-1 moves o least (to within 0.5).  The directions along the axis of a nearly
    // parabolic h P h^-1 (orders n of about 12..50) are covered by these rotations; without
    // them K had to grow to 4.5..5.5 there (or the cells stayed uncovered).
    vector<Mat> cand = bg.seeds;
    std::unordered_map<uint64_t, int> idx;
    for (size_t i = 0; i < cand.size(); ++i) idx.emplace(flatkey(hkey(cand[i])), (int)i);
    {
        Mat P = matmul(matmul(rep.A, rep.B), matmul(rep.Ai, rep.Bi));
        vector<double> disp(bg.seeds.size());
        double mn = 1e300;
        for (size_t i = 0; i < bg.seeds.size(); ++i) {
            const Mat& h = bg.seeds[i];
            disp[i] = std::acosh(std::max(1.0, frob2(matmul(h, matmul(P, matinv(h))))/2.0));
            mn = std::min(mn, disp[i]);
        }
        const int kmax = order > 0 ? std::min(order - 1, 100) : 100;
        for (size_t i = 0; i < bg.seeds.size(); ++i) {
            if (disp[i] > mn + 0.5) continue;
            const Mat& h = bg.seeds[i]; const Mat hi = matinv(h);
            Mat Pk = identity();
            for (int k = 1; k <= kmax; ++k) {
                Pk = matmul(Pk, P);
                for (const Mat& Q : {Pk, matinv(Pk)}) {
                    Mat X = matmul(h, matmul(Q, hi));
                    double f = frob2(X);
                    if (f > thrK && std::acosh(std::max(1.0, f/2.0)) < 2.0*K - 0.05 &&
                        idx.emplace(flatkey(hkey(X)), (int)cand.size()).second) cand.push_back(X);
                }
            }
        }
    }
    vector<char> skip(cand.size(), 0);
    skip[0] = 1;                                       // the identity (nearest first)
    for (const Mat& G : g6) { auto it = idx.find(flatkey(hkey(G))); if (it != idx.end()) skip[it->second] = 1; }
    // cube grid on the sphere of directions (6 faces of m x m cells, stored by blocks of
    // mb x mb cells): cell centres, the largest cell radius, and per block a bounding cap
    const int m = 60, mb = 10, nb = m/mb;
    auto P = [&](int f, double u, double v) {
        double a = -1 + 2*u/m, b = -1 + 2*v/m, x, y, z;
        switch (f) { case 0: x = 1; y = a; z = b; break;  case 1: x = -1; y = a; z = b; break;
                     case 2: x = a; y = 1; z = b; break;  case 3: x = a; y = -1; z = b; break;
                     case 4: x = a; y = b; z = 1; break;  default: x = a; y = b; z = -1; }
        double r = std::sqrt(x*x + y*y + z*z); return Pt{x/r, y/r, z/r};
    };
    auto angle = [](const Pt& p, const Pt& q) {
        return std::acos(std::max(-1.0, std::min(1.0, p.x*q.x + p.y*q.y + p.z*q.z))); };
    vector<Pt> cc; cc.reserve(6*m*m);
    vector<Pt> bc; vector<double> br;                  // block centre and radius (over its cells)
    double rmax = 0;
    for (int f = 0; f < 6; ++f) for (int I = 0; I < nb; ++I) for (int J = 0; J < nb; ++J) {
        Pt c0 = P(f, (I + 0.5)*mb, (J + 0.5)*mb);
        double r0 = 0;
        for (int i = I*mb; i < (I+1)*mb; ++i) for (int j = J*mb; j < (J+1)*mb; ++j) {
            Pt c = P(f, i + 0.5, j + 0.5);
            for (Pt q : {P(f, i, j), P(f, i+1, j), P(f, i, j+1), P(f, i+1, j+1)})
                rmax = std::max(rmax, angle(c, q));
            r0 = std::max(r0, angle(c0, c));
            cc.push_back(c);
        }
        bc.push_back(c0); br.push_back(r0);
    }
    const size_t N = cc.size(), cpb = (size_t)(mb*mb);
    const double cothD0 = 1.0/std::tanh(D0);
    // the cells wholly inside the cap of s at level D0 (with a margin of 1e-3 rad)
    auto cap_cells = [&](const Mat& s, vector<uint32_t>& out) {
        out.clear();
        double l = std::acosh(std::max(1.0, frob2(s)/2.0)), q = std::tanh(l/2.0)*cothD0;
        if (!(q < 1.0)) return;
        double a = std::acos(q) - rmax - 1e-3;
        if (a <= 0) return;
        Pt u = ball_dir(s); double ca = std::cos(a);
        for (size_t k = 0; k < bc.size(); ++k) {
            if (angle(bc[k], u) > a + br[k] + 1e-9) continue;     // no cell of the block inside
            for (size_t i = k*cpb; i < (k+1)*cpb; ++i)
                if (cc[i].x*u.x + cc[i].y*u.y + cc[i].z*u.z > ca) out.push_back((uint32_t)i);
        }
    };
    vector<char> cov(N, 0);
    vector<uint32_t> tmp;
    for (const Mat& G : g6) { cap_cells(G, tmp); for (uint32_t i : tmp) cov[i] = 1; }
    bg.S = g6;
    const size_t nc = cand.size();
    vector<vector<uint32_t>> lists(nc);
    vector<int> inv(nc, -1);
    for (size_t s = 0; s < nc; ++s) {
        if (skip[s]) continue;
        cap_cells(cand[s], lists[s]);
        auto it = idx.find(flatkey(hkey(matinv(cand[s]))));
        if (it != idx.end()) inv[s] = it->second;
    }
    // greedy cover: add the candidate (with its inverse) covering most uncovered cells
    for (;;) {
        int bi = -1; size_t bgain = 0;
        for (size_t s = 0; s < nc; ++s) {
            if (skip[s]) continue;
            size_t g = 0;
            for (uint32_t i : lists[s]) g += !cov[i];
            if (inv[s] >= 0 && inv[s] != (int)s)
                for (uint32_t i : lists[inv[s]]) g += !cov[i];
            if (g > bgain) { bgain = g; bi = (int)s; }
        }
        if (bi < 0) break;
        for (int s : {bi, inv[bi]}) {
            if (s < 0 || skip[s]) continue;
            skip[s] = 1; bg.S.push_back(cand[s]);
            for (uint32_t i : lists[s]) cov[i] = 1;
        }
        if (inv[bi] < 0) bg.S.push_back(matinv(cand[bi]));   // (an inverse not among them)
    }
    bg.cells = N; bg.uncovered = 0; uncov.clear();
    for (size_t i = 0; i < N; ++i) if (!cov[i]) { ++bg.uncovered; uncov.push_back(cc[i]); }
}

// The ball-model direction at o of the boundary point z (infinity: the north pole).
static Pt boundary_dir(const cpx& z) {
    double n = std::norm(z);
    return Pt{2*z.real()/(n + 1), 2*z.imag()/(n + 1), (n - 1)/(n + 1)};
}
// The uncovered cells next to a cusp point (n = inf) or near-cusp point (large n): within
// CUSP_TOL of a fixed point of a conjugate h P h^-1 of the peripheral element P = [a,b]
// (h in the seed ball) that moves o least (to within 0.5).  There the nearer neighbour is a
// (nearly) parabolic step whose gain tends to zero, so no finite S covers these directions
// with a margin (they stay uncovered for every K tried); the descent property holds there
// in every test, but it is not certified.
static const double CUSP_TOL = 0.04;
static size_t cusp_cells(const Rep& rep, const BallGens& bg, const vector<Pt>& uncov) {
    if (uncov.empty()) return 0;
    Mat P = matmul(matmul(rep.A, rep.B), matmul(rep.Ai, rep.Bi));
    vector<double> disp(bg.seeds.size());
    double mn = 1e300;
    for (size_t i = 0; i < bg.seeds.size(); ++i) {
        const Mat& h = bg.seeds[i];
        disp[i] = std::acosh(std::max(1.0, frob2(matmul(h, matmul(P, matinv(h))))/2.0));
        mn = std::min(mn, disp[i]);
    }
    vector<Pt> pts;
    for (size_t i = 0; i < bg.seeds.size(); ++i) {
        if (disp[i] > mn + 0.5) continue;
        const Mat& h = bg.seeds[i];
        Mat Q = matmul(h, matmul(P, matinv(h)));
        if (std::abs(Q.c) < 1e-13) {
            pts.push_back(Pt{0, 0, 1});
            if (std::abs(Q.a - Q.d) > 1e-9) pts.push_back(boundary_dir(Q.b/(Q.d - Q.a)));
            continue;
        }
        cpx disc = std::sqrt((Q.a - Q.d)*(Q.a - Q.d) + 4.0*Q.b*Q.c);
        pts.push_back(boundary_dir((Q.a - Q.d + disc)/(2.0*Q.c)));
        pts.push_back(boundary_dir((Q.a - Q.d - disc)/(2.0*Q.c)));
    }
    const double ct = std::cos(CUSP_TOL);
    size_t n = 0;
    for (const Pt& c : uncov)
        for (const Pt& q : pts)
            if (c.x*q.x + c.y*q.y + c.z*q.z > ct) { ++n; break; }
    return n;
}

// Build the enriched generating set S and the seed ball (see the comment above).  The seed
// radius K starts at 3 and grows by 0.5 while cells other than cusp cells stay uncovered (thin
// parts need longer elements of S), up to K = 6 or a plain search of
// about 2 million elements.  The candidates for S include the powers of the least-moving
// conjugates of P = [a,b] (cover_level); without them orders n of about 12..50 needed K up to
// 5.5, or stayed uncovered.  The cusp cells are tolerated at the cusp and for orders n >= 12
// only.  Then, if any cell is uncovered, the elements of the plain search beyond B(K) that
// have no nearer S-neighbour (none in every test once only cusp cells are left) are added to
// the seeds.  K is 3..4.5 in most tests, 5..6 for the thinnest words (R^14 L -n 5: 5;
// R^10 L and R^12 L at n = 11: 5.5 and 6, not certified, 1 and 2 cells).
static void make_ball_gens(const Rep& rep, int order, BallGens& bg) {
    const bool tol = order <= 0 || order >= 12;
    vector<Mat> big;
    vector<Pt> uncov;
    for (double K = 3.0; ; K += 0.5) {
        BallGens t;
        cover_level(rep, order, K, t, big, uncov);
        t.tolerated = tol ? cusp_cells(rep, t, uncov) : 0;
        bg = std::move(t);
        if (bg.uncovered == bg.tolerated) break;
        if (K + 0.5 > 6.0 + 1e-9 || (double)bg.nEst*std::exp(1.0) > 2e6) break;
    }
    if (bg.uncovered) {
        const double thrK = 2.0*std::cosh(bg.Kc);
        for (const Mat& g : big) {
            double k = frob2(g);
            if (k <= thrK) continue;
            bool down = false;
            for (const Mat& s : bg.S) if (frob2(matmul(g, s)) < k*(1.0 - 1e-9)) { down = true; break; }
            if (!down) { bg.seeds.push_back(g); ++bg.extraSeeds; }
        }
    }
    vector<Mat>().swap(big);
    // the size estimate: the complete ball B(max(5, Kc + 1)) (the plain search misses up to
    // 30% of a ball for thin monodromies)
    double rE = std::max(5.0, bg.Kc + 1.0), thrE = 2.0*std::cosh(rE);
    bg.nEst = ball_search(bg.S, bg.seeds, thrE, thrE, nullptr, true, 20000000).size();
    bg.rEst = rE;
}

// H^s for a positive Hermitian H of determinant 1 (a pure translation along an axis
// through o): the translation by s times the distance.
static Mat herm_pow(const Mat& H, double s) {
    double t = tr(H).real();
    if (!(t > 2.0 + 1e-15)) return identity();
    double l = 0.5*(t + std::sqrt(t*t - 4.0)), m = 1.0/l;
    double fl = std::pow(l, s), fm = std::pow(m, s);
    double c1 = (fl - fm)/(l - m), c0 = (l*fm - m*fl)/(l - m);
    return { c1*H.a + c0, c1*H.b, c1*H.c, c1*H.d + c0 };
}
// The elements g with d(o, g.o) <= R, or -- with a focus Tf -- d(Tf.o, g.o) <= R (i.e.
// ||Tf^-1 g||_F^2 <= 2cosh R); `focusInv` = Tf^-1 (Tf a pure translation).  At most `cap`
// elements (capHit is set).  Returned sorted nearest first (by the smaller of the two
// distances).  Complete (see above).  With a focus, an element with d(Tf.o, g.o) >= D0 has
// an S-neighbour nearer to Tf.o (the cover argument works about any centre), so the search
// must also be seeded with B(Tf.o, D0).  That ball is found by a chain of small searches
// along the geodesic from o to Tf.o, at points y_0 = o, y_1, ..., y_m = Tf.o spaced s <= 0.5
// apart: B(y_k, D0 + s), found by the same outward search about y_k from B(y_k, D0), contains
// B(y_{k+1}, D0).  Each search is small (a ball of radius D0 + s).  Near a cusp a point y_k
// deep in the cusp may have no orbit point within D0; there the nearest orbit point found by
// a greedy descent (and the ball about y_k of its distance) is used as well (empirical, as
// the cusp cells above; checked against complete searches up to zoom 10^6).
static vector<Mat> enumerate_ball(const Rep& rep, double R, const Mat* focusInv, size_t cap,
                                  bool* capHit = nullptr, size_t* noDescent = nullptr,
                                  double* focusDelta = nullptr) {
    const BallGens& bg = *rep.ball;
    const double thr = 2.0*std::cosh(R);
    // the expected size (as ball_estimate), to allocate the pool once
    double ex = 1.2*(double)bg.nEst*std::exp(2.0*(R - bg.rEst))*(focusInv ? 2.0 : 1.0);
    size_t expect = (size_t)std::min((double)cap, std::max(1024.0, ex));
    if (!focusInv)
        return ball_search(bg.S, bg.seeds, thr, thr, nullptr, true, cap, capHit, noDescent, expect);
    const Mat Tf = matinv(*focusInv);
    const double L = std::acosh(std::max(1.0, frob2(Tf)/2.0));
    const int m = std::max(1, (int)std::ceil(L/0.5 - 1e-9));
    const double st = L/m, D0 = bg.D0;
    vector<Mat> cur = ball_search(bg.S, bg.seeds, 2.0*std::cosh(D0 + st + 1e-6),
                                  2.0*std::cosh(D0 + st + 1e-6), nullptr, true, cap);  // B(y_0, D0 + s)
    vector<Mat> seeds = bg.seeds;
    double delta = 0;
    for (int k = 1; k <= m; ++k) {
        Mat Tki = matinv(herm_pow(Tf, (double)k/m));   // y_k = T_k.o
        auto fk = [&](const Mat& X) { return frob2(matmul(Tki, X)); };
        // the nearest orbit point to y_k: greedy S-descent from the nearest element of cur
        Mat h = identity(); double kh = fk(h);
        for (const Mat& X : cur) { double q = fk(X); if (q < kh) { kh = q; h = X; } }
        for (int it = 0; it < 100000; ++it) {
            Mat best = h; double kb = kh;
            for (const Mat& sg : bg.S) { Mat X = matmul(h, sg); double q = fk(X); if (q < kb) { kb = q; best = X; } }
            if (!(kb < kh)) break;
            h = best; kh = kb;
        }
        delta = std::acosh(std::max(1.0, kh/2.0));
        double r = std::max(D0, delta) + 1e-6;         // B(y_k, D0) (or about the nearest point)
        vector<Mat> sd;
        for (const Mat& X : cur) if (fk(X) <= 2.0*std::cosh(r)) sd.push_back(X);
        sd.push_back(h);
        if (k == m) { seeds.insert(seeds.end(), sd.begin(), sd.end()); break; }
        double tn = 2.0*std::cosh(r + st);
        cur = ball_search(bg.S, sd, tn, tn, &Tki, true, cap, nullptr, nullptr, 0, true);
    }
    if (focusDelta) *focusDelta = delta;
    return ball_search(bg.S, seeds, thr, thr, focusInv, true, cap, capHit, noDescent, expect);
}

// ------------------------------------------------------------------ viewing / projection
typedef std::pair<double,double> P2;
struct View { double m[9]; };                    // rotation: (X,Y,Z) = m * (x,y,z)
// Rz(roll) * Ry(yaw) * Rx(pitch).  The eye looks along Z, so the hidden --roll turns the
// picture in its plane (counterclockwise); with it every orientation of the viewer has an
// exact command-line form (see vw::view_angles).
static View make_view(double yaw_deg, double pitch_deg, double roll_deg = 0.0) {
    double y = yaw_deg*PI/180.0, p = pitch_deg*PI/180.0;
    double cy=std::cos(y), sy=std::sin(y), cp=std::cos(p), sp=std::sin(p);
    View v = {{ cy,  sy*sp,  sy*cp,                 // Ry(yaw) * Rx(pitch)
                0,   cp,    -sp,
               -sy,  cy*sp,  cy*cp }};
    if (roll_deg == 0.0) return v;
    double r = roll_deg*PI/180.0, cr = std::cos(r), sr = std::sin(r);
    View w = v;
    for (int j = 0; j < 3; ++j) { w.m[j] = cr*v.m[j] - sr*v.m[3+j]; w.m[3+j] = sr*v.m[j] + cr*v.m[3+j]; }
    return w;
}
static inline void vrot(const View& v, float x, float y, float z, double& X, double& Y, double& Z) {
    X = v.m[0]*x + v.m[1]*y + v.m[2]*z;
    Y = v.m[3]*x + v.m[4]*y + v.m[5]*z;
    Z = v.m[6]*x + v.m[7]*y + v.m[8]*z;
}

// Douglas-Peucker simplification of a 2D polyline to tolerance `tol`.  Turns the
// length-limited subdivision into an accuracy-bounded minimal polyline: straight stretches
// collapse to one segment (small files even at high accuracy), curved parts keep detail.
static void dp_simplify(const vector<P2>& pts, double tol, double maxseg, vector<P2>& out) {
    int n = (int)pts.size();
    if (n <= 2) { out = pts; return; }
    vector<char> keep(n, 0); keep[0] = keep[n-1] = 1;
    vector<std::pair<int,int>> stack{{0, n-1}};
    double tol2 = tol*tol, maxseg2 = maxseg*maxseg;
    while (!stack.empty()) {
        auto [i0, i1] = stack.back(); stack.pop_back();
        double ax = pts[i0].first, ay = pts[i0].second, bx = pts[i1].first, by = pts[i1].second;
        double dx = bx-ax, dy = by-ay, len2 = dx*dx + dy*dy;
        int best = -1; double bestd = tol2;
        for (int k = i0+1; k < i1; ++k) {
            double px = pts[k].first, py = pts[k].second, d2;
            if (len2 < 1e-18) { double ex=px-ax, ey=py-ay; d2 = ex*ex+ey*ey; }
            else { double cr = dx*(py-ay) - dy*(px-ax); d2 = cr*cr/len2; }
            if (d2 > bestd) { bestd = d2; best = k; }
        }
        // cap straight runs: never emit a chord longer than maxseg (avoids long straight arcs)
        if (best < 0 && len2 > maxseg2 && i1 - i0 > 1) best = (i0 + i1) / 2;
        if (best >= 0) { keep[best]=1; stack.push_back({i0,best}); stack.push_back({best,i1}); }
    }
    out.clear();
    for (int i = 0; i < n; ++i) if (keep[i]) out.push_back(pts[i]);
}

// Liang-Barsky: clip the segment P + t (Q - P), 0 <= t <= 1, to the square |X|,|Y| <= L;
// returns false if it misses the square, else the parameter range [t0, t1] inside it.
static bool clip_square(double px, double py, double qx, double qy, double L,
                        double& t0, double& t1) {
    t0 = 0; t1 = 1;
    const double d[2] = { qx - px, qy - py }, p[2] = { px, py };
    for (int a = 0; a < 2; ++a)
        for (int s = -1; s <= 1; s += 2) {           // the edge s*X_a <= L
            double num = L - s*p[a], den = s*d[a];   // need num - t den >= 0
            if (den == 0) { if (num < 0) return false; continue; }
            double t = num/den;
            if (den > 0) { if (t < t1) t1 = t; } else { if (t > t0) t0 = t; }
            if (t0 > t1) return false;
        }
    return true;
}

// Project a renderer's polylines to 2D from viewpoint `v`, magnified by `zoom`, keeping only
// the front hemisphere (rotated Z > 0) and the view square |X|,|Y| <= lim: each segment is
// CLIPPED to the square (so the output coordinates stay bounded however large the zoom, and
// no off-screen pixels are walked), the polylines are split where they leave it, and each
// run is DP-simplified.  At zoom 1 the output lies in the unit disk.
static size_t g_visible = 0;      // polylines that met the view square (for the warning)
static void collect_polylines(const Renderer& rr, const View& v, double tol, double zoom,
                              double lim, vector<vector<P2>>& out) {
    vector<P2> run, simp;
    auto flush = [&]() {
        if (run.size() >= 2) { dp_simplify(run, tol, 0.3, simp); out.push_back(simp); ++g_visible; }
        run.clear();
    };
    for (size_t k = 0; k < rr.npoly(); ++k) {
        size_t b = rr.poly_begin(k), e = rr.poly_end(k);
        run.clear();
        double pX = 0, pY = 0, pZ = -1;
        for (size_t i = b; i < e; ++i) {
            double X, Y, Z; vrot(v, rr.px[i], rr.py[i], rr.pz[i], X, Y, Z);
            X *= zoom; Y *= zoom;
            if (i > b) {
                double t0, t1;
                if (pZ > 0 && Z > 0 && clip_square(pX, pY, X, Y, lim, t0, t1)) {
                    double dx = X - pX, dy = Y - pY;
                    if (t0 > 0) flush();                         // enters the square
                    if (run.empty()) run.push_back(t0 > 0 ? P2{pX + t0*dx, pY + t0*dy} : P2{pX, pY});
                    run.push_back(t1 < 1 ? P2{pX + t1*dx, pY + t1*dy} : P2{X, Y});
                    if (t1 < 1) flush();                         // leaves it
                }
                else flush();
            }
            pX = X; pY = Y; pZ = Z;
        }
        flush();
    }
}

// ------------------------------------------------------------------ Catherine wheel
// A single embedded circle on S^2 separating the finite approximations of Z+ and Z- into
// two disks, each a neighbourhood of one zipper.  It is (essentially) the zero set of
//     f = d(.,Z-) - d(.,Z+),
// d the chordal distance to the drawn polylines, each segment read as the great-circle arc
// between its ends (what lies on S^2).  For finite n the two finite zippers are disjoint, but
// each is a forest of many pieces (the ball truncation cuts the infinite trees), so {f>0}
// may have small ISLANDS: a piece of Z+ cut off from the rest of Z+ by Z- (this happens near
// the elliptic points).  Every component of {f>0} contains a piece of Z+, and vice versa.
// The zero set is extracted by marching over an ADAPTIVE triangulation of the sphere (a
// subdivided icosahedron, kept conforming: a triangle whose edge was split by a neighbour is
// fanned from its centre).  A triangle is split while
//   * the zipper that must stay out of it (the opposite one if its corners agree in sign,
//     both if the zero set crosses it) comes within half its longest edge e of a corner, or
//   * f may change sign inside although the corners agree (|f| <= 2.2 e at a corner; f is
//     2-Lipschitz) and e exceeds the local gap d(.,Z+) + d(.,Z-),
// unless e is below a quarter of that gap or below pi/wheelres (default 3e-7).  So the mesh
// follows the local gap between the zippers, however small.  (For n = inf the zippers touch
// at the parabolic points, so there the wheel must meet them; there the mesh also stops at
// about the drawing accuracy.)  Components of one side inside the other ("pockets") are then
// handled in a few rounds:
//   * a pocket that contains a piece of its own zipper (an island) is joined to the main
//     part of its side by a BRIDGE: a mesh path of largest clearance from the other zipper,
//     then the shortest path with at least half that clearance.  The bridge is added to its
//     zipper for the distance function only (it is not drawn), and the mesh is refined
//     along it, so that the wheel runs around it;
//   * the rims of the other pockets (under-resolved necks) are refined.
// Pockets that remain are merged into the surrounding side, so that exactly one circle is
// output; a warning reports any merged island at finite n (its zipper piece is then on the
// wrong side).  Finally every wheel segment is tested against both zippers.  Where the drawn
// Z+ and Z- themselves cross (they can, where they come closer than the drawing accuracy
// resolves: the chords of length ~accuracy stray from the true curves) no circle can
// separate them; crossings of the wheel there are reported in a note, any others in a
// warning.
namespace wheel_detail {

// Allocator for the wheel's big arrays: blocks of 1 MB or more are mapped directly, so that
// freeing them returns the memory at once (the mesh arrays are rebuilt in each pocket round,
// and the system allocator would keep the freed blocks resident).
template <class T> struct BigAlloc {
    typedef T value_type;
    BigAlloc() = default;
    template <class U> BigAlloc(const BigAlloc<U>&) {}
    T* allocate(size_t n) {
        size_t b = n*sizeof(T);
        if (b < ((size_t)1 << 20)) return static_cast<T*>(::operator new(b));
        void* p = mmap(nullptr, b, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) throw std::bad_alloc();
        return static_cast<T*>(p);
    }
    void deallocate(T* p, size_t n) {
        size_t b = n*sizeof(T);
        if (b < ((size_t)1 << 20)) ::operator delete(p); else munmap(p, b);
    }
    template <class U> bool operator==(const BigAlloc<U>&) const { return true; }
    template <class U> bool operator!=(const BigAlloc<U>&) const { return false; }
};
template <class T> using bvec = std::vector<T, BigAlloc<T>>;

// squared distance between the segments p1q1 and p2q2 (Ericson, Real-Time Collision
// Detection, 5.1.9)
static double segseg2(const double* p1, const double* q1, const double* p2, const double* q2) {
    double d1[3], d2[3], r[3];
    for (int k = 0; k < 3; ++k) { d1[k] = q1[k]-p1[k]; d2[k] = q2[k]-p2[k]; r[k] = p1[k]-p2[k]; }
    auto dot = [](const double* u, const double* v) { return u[0]*v[0] + u[1]*v[1] + u[2]*v[2]; };
    auto clamp01 = [](double x) { return x < 0 ? 0.0 : x > 1 ? 1.0 : x; };
    double a = dot(d1,d1), e = dot(d2,d2), f = dot(d2,r), s, t;
    const double TINY = 1e-300;
    if (a <= TINY && e <= TINY) s = t = 0;
    else if (a <= TINY) { s = 0; t = clamp01(f/e); }
    else {
        double c = dot(d1,r);
        if (e <= TINY) { t = 0; s = clamp01(-c/a); }
        else {
            double b = dot(d1,d2), den = a*e - b*b;
            s = den > 0 ? clamp01((b*f - c*e)/den) : 0;
            t = (b*s + f)/e;
            if (t < 0) { t = 0; s = clamp01(-c/a); }
            else if (t > 1) { t = 1; s = clamp01((b - c)/a); }
        }
    }
    double x = r[0] + d1[0]*s - d2[0]*t, y = r[1] + d1[1]*s - d2[1]*t, z = r[2] + d1[2]*s - d2[2]*t;
    return x*x + y*y + z*z;
}

// Nearest-arc distance to a zipper's polylines.  A drawn segment stands for the great-circle
// arc between its endpoints (its radial projection, which is what lies on S^2 and what the
// crossing tests see; the chord itself dips up to L^2/8 inside the sphere, which matters
// where the zippers come within ~1e-6 of each other).  The distance from a unit vector p to
// an arc is the chordal distance to its nearest point.  A kd-tree on segment midpoints: long
// segments are cut into pieces, every point of a piece's arc is within H of its midpoint,
// so a subtree whose splitting plane is at distance t can be skipped once t - H exceeds the
// best distance found.
struct SegTree {
    // ends, chord midpoint, unit normal of the arc's plane, tangents into the arc at the ends
    struct S { float a[3], b[3], m[3], n[3], ta[3], tb[3]; };
    vector<S> s;
    double H = 0, sag = 0, maxlen = 0.004;            // sag: largest sagitta of a piece
    static double sagitta(double hl) { return hl*hl/(1 + std::sqrt(std::max(0.0, 1 - hl*hl))); }
    void add(double ax, double ay, double az, double bx, double by, double bz) {
        double dx = bx - ax, dy = by - ay, dz = bz - az;
        int m = std::max(1, (int)std::ceil(std::sqrt(dx*dx + dy*dy + dz*dz)/maxlen));
        double P[3] = {ax, ay, az};
        for (int j = 0; j < m; ++j) {
            double t1 = (double)(j+1)/m, Q[3] = {ax + t1*dx, ay + t1*dy, az + t1*dz};
            if (j + 1 < m) {                          // interior split points onto the sphere
                double l = std::sqrt(Q[0]*Q[0] + Q[1]*Q[1] + Q[2]*Q[2]); Q[0] /= l; Q[1] /= l; Q[2] /= l;
            }
            S q;
            double n[3] = {P[1]*Q[2] - P[2]*Q[1], P[2]*Q[0] - P[0]*Q[2], P[0]*Q[1] - P[1]*Q[0]};
            double nl = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
            for (int k = 0; k < 3; ++k) n[k] = nl > 1e-300 ? n[k]/nl : 0;
            double ta[3] = {n[1]*P[2] - n[2]*P[1], n[2]*P[0] - n[0]*P[2], n[0]*P[1] - n[1]*P[0]};
            double tb[3] = {Q[1]*n[2] - Q[2]*n[1], Q[2]*n[0] - Q[0]*n[2], Q[0]*n[1] - Q[1]*n[0]};
            double hl2 = 0;
            for (int k = 0; k < 3; ++k) {
                q.a[k] = (float)P[k]; q.b[k] = (float)Q[k]; q.m[k] = (float)(0.5*(P[k] + Q[k]));
                q.n[k] = (float)n[k]; q.ta[k] = (float)ta[k]; q.tb[k] = (float)tb[k];
                hl2 += 0.25*(Q[k] - P[k])*(Q[k] - P[k]);
            }
            double hl = std::sqrt(hl2), sg = sagitta(hl);
            H = std::max(H, hl + sg); sag = std::max(sag, sg);
            s.push_back(q);
            for (int k = 0; k < 3; ++k) P[k] = Q[k];
        }
    }
    void finish() {
        H = H*1.0001 + 1e-9;
        if (!s.empty()) build_rec(0, s.size(), 0);
    }
    void build(const Renderer& r, double ml) {
        maxlen = ml;
        for (size_t k = 0; k < r.npoly(); ++k)
            for (size_t i = r.poly_begin(k); i + 1 < r.poly_end(k); ++i)
                add(r.px[i], r.py[i], r.pz[i], r.px[i+1], r.py[i+1], r.pz[i+1]);
        finish();
    }
    void build(const vector<std::array<Pt,2>>& v, double ml) {   // from a list of segments
        s.clear(); H = 0; sag = 0; maxlen = ml;
        for (auto& g : v) add(g[0].x, g[0].y, g[0].z, g[1].x, g[1].y, g[1].z);
        finish();
    }
    void build_rec(size_t lo, size_t hi, int ax) {
        if (hi - lo <= 8) return;
        size_t mid = (lo + hi)/2;
        std::nth_element(s.begin()+lo, s.begin()+mid, s.begin()+hi,
                         [ax](const S& a, const S& b){ return a.m[ax] < b.m[ax]; });
        build_rec(lo, mid, (ax+1)%3); build_rec(mid+1, hi, (ax+1)%3);
    }
    // squared chordal distance from the unit vector p to the arc of q
    static double sd2(const double* p, const S& q) {
        if (p[0]*q.ta[0] + p[1]*q.ta[1] + p[2]*q.ta[2] > 0 && p[0]*q.tb[0] + p[1]*q.tb[1] + p[2]*q.tb[2] > 0) {
            double t = (p[0]-q.a[0])*q.n[0] + (p[1]-q.a[1])*q.n[1] + (p[2]-q.a[2])*q.n[2];
            return 2*t*t/(1 + std::sqrt(std::max(0.0, 1 - t*t)));
        }
        double ax = p[0]-q.a[0], ay = p[1]-q.a[1], az = p[2]-q.a[2];
        double bx = p[0]-q.b[0], by = p[1]-q.b[1], bz = p[2]-q.b[2];
        return std::min(ax*ax + ay*ay + az*az, bx*bx + by*by + bz*bz);
    }
    void search(const double* p, size_t lo, size_t hi, int ax, double& b2, double& b) const {
        if (hi - lo <= 8) {
            for (size_t i = lo; i < hi; ++i) { double d2 = sd2(p, s[i]); if (d2 < b2) { b2 = d2; b = std::sqrt(d2); } }
            return;
        }
        size_t mid = (lo + hi)/2;
        double d2 = sd2(p, s[mid]); if (d2 < b2) { b2 = d2; b = std::sqrt(d2); }
        double t = p[ax] - s[mid].m[ax];
        int nx = (ax+1)%3;
        if (t < 0) { search(p, lo, mid, nx, b2, b); if (-t - H < b) search(p, mid+1, hi, nx, b2, b); }
        else       { search(p, mid+1, hi, nx, b2, b); if (t - H < b) search(p, lo, mid, nx, b2, b); }
    }
    // exact distance from the unit vector P, given an upper bound (used only for pruning)
    double dist(const Pt& P, double bound) const {
        if (s.empty()) return 4.0;
        double p[3] = {P.x, P.y, P.z};
        double b = std::min(4.0, bound*(1 + 1e-6) + 1e-9), b2 = b*b;
        search(p, 0, s.size(), 0, b2, b);
        return b;
    }
    static double ss2(const double* p, const double* q, const S& g) {
        double a[3] = {g.a[0], g.a[1], g.a[2]}, c[3] = {g.b[0], g.b[1], g.b[2]};
        return segseg2(p, q, a, c);
    }
    void ssearch(const double* p, const double* q, size_t lo, size_t hi, int ax, double& b2, double& b) const {
        if (hi - lo <= 8) {
            for (size_t i = lo; i < hi; ++i) { double d2 = ss2(p, q, s[i]); if (d2 < b2) { b2 = d2; b = std::sqrt(d2); } }
            return;
        }
        size_t mid = (lo + hi)/2;
        double d2 = ss2(p, q, s[mid]); if (d2 < b2) { b2 = d2; b = std::sqrt(d2); }
        double pl = s[mid].m[ax], mn = std::min(p[ax], q[ax]), mx = std::max(p[ax], q[ax]);
        int nx = (ax+1)%3;
        // pieces below the plane reach at most pl + H, those above at least pl - H
        if (mn + mx < 2*pl) { ssearch(p, q, lo, mid, nx, b2, b); if (pl - mx - H < b) ssearch(p, q, mid+1, hi, nx, b2, b); }
        else { ssearch(p, q, mid+1, hi, nx, b2, b); if (mn - pl - H < b) ssearch(p, q, lo, mid, nx, b2, b); }
    }
    // a lower bound for the distance between the arc PQ (unit P, Q) and the stored arcs: the
    // distance between the chords less both sagittas (only needed down to `bound`)
    double segdist(const Pt& P, const Pt& Q, double bound) const {
        if (s.empty()) return 4.0;
        double sq = sagitta(0.5*dist3(P, Q)), slack = sq + sag;
        double p[3] = {P.x, P.y, P.z}, q[3] = {Q.x, Q.y, Q.z};
        double b = std::min(4.0, (bound + slack)*(1 + 1e-6) + 1e-9), b2 = b*b;
        ssearch(p, q, 0, s.size(), 0, b2, b);
        return std::max(0.0, b - slack);
    }
    // do the arcs PQ and q properly cross?
    static bool crosses(const Pt& P, const Pt& Q, const S& q) {
        double n1x = P.y*Q.z - P.z*Q.y, n1y = P.z*Q.x - P.x*Q.z, n1z = P.x*Q.y - P.y*Q.x;
        double n2x = (double)q.a[1]*q.b[2] - (double)q.a[2]*q.b[1], n2y = (double)q.a[2]*q.b[0] - (double)q.a[0]*q.b[2],
               n2z = (double)q.a[0]*q.b[1] - (double)q.a[1]*q.b[0];
        double c1 = n1x*q.a[0] + n1y*q.a[1] + n1z*q.a[2], c2 = n1x*q.b[0] + n1y*q.b[1] + n1z*q.b[2];
        double c3 = n2x*P.x + n2y*P.y + n2z*P.z, c4 = n2x*Q.x + n2y*Q.y + n2z*Q.z;
        if (!(c1*c2 < 0 && c3*c4 < 0)) return false;
        if ((P.x+Q.x)*((double)q.a[0]+q.b[0]) + (P.y+Q.y)*((double)q.a[1]+q.b[1]) + (P.z+Q.z)*((double)q.a[2]+q.b[2]) <= 0) return false;
        // the crossing point must lie on both arcs (rules out near-collinear arcs, where the
        // plane test is decided by rounding)
        Pt X = crosspt(P, Q, q), A{q.a[0], q.a[1], q.a[2]}, B{q.b[0], q.b[1], q.b[2]};
        auto on = [&X](const Pt& U, const Pt& V) {    // X between U and V on their great circle
            double n[3] = {U.y*V.z - U.z*V.y, U.z*V.x - U.x*V.z, U.x*V.y - U.y*V.x};
            double nl = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
            double tu = X.x*(n[1]*U.z - n[2]*U.y) + X.y*(n[2]*U.x - n[0]*U.z) + X.z*(n[0]*U.y - n[1]*U.x);
            double tv = X.x*(V.y*n[2] - V.z*n[1]) + X.y*(V.z*n[0] - V.x*n[2]) + X.z*(V.x*n[1] - V.y*n[0]);
            return tu >= -1e-9*nl && tv >= -1e-9*nl;
        };
        return on(P, Q) && on(A, B);
    }
    bool hits_rec(const double* p, double r2, double r, const Pt& P, const Pt& Q,
                  size_t lo, size_t hi, int ax) const {
        if (hi - lo <= 8) {
            for (size_t i = lo; i < hi; ++i) if (sd2(p, s[i]) <= r2 && crosses(P, Q, s[i])) return true;
            return false;
        }
        size_t mid = (lo + hi)/2;
        if (sd2(p, s[mid]) <= r2 && crosses(P, Q, s[mid])) return true;
        double t = p[ax] - s[mid].m[ax];
        int nx = (ax+1)%3;
        // pieces below the plane reach at most m + H, those above at least m - H
        if (t - H <= r && hits_rec(p, r2, r, P, Q, lo, mid, nx)) return true;
        if (-t - H <= r && hits_rec(p, r2, r, P, Q, mid+1, hi, nx)) return true;
        return false;
    }
    // the crossing point of the arcs PQ and q (they must cross)
    static Pt crosspt(const Pt& P, const Pt& Q, const S& q) {
        double n1[3] = {P.y*Q.z - P.z*Q.y, P.z*Q.x - P.x*Q.z, P.x*Q.y - P.y*Q.x};
        double n2[3] = {(double)q.a[1]*q.b[2] - (double)q.a[2]*q.b[1], (double)q.a[2]*q.b[0] - (double)q.a[0]*q.b[2],
                        (double)q.a[0]*q.b[1] - (double)q.a[1]*q.b[0]};
        double x = n1[1]*n2[2] - n1[2]*n2[1], y = n1[2]*n2[0] - n1[0]*n2[2], z = n1[0]*n2[1] - n1[1]*n2[0];
        double l = std::sqrt(x*x + y*y + z*z);
        if (l < 1e-300) return P;
        if (x*(P.x+Q.x) + y*(P.y+Q.y) + z*(P.z+Q.z) < 0) l = -l;
        return Pt{x/l, y/l, z/l};
    }
    void cross_rec(const double* p, double r2, double r, const Pt& P, const Pt& Q,
                   size_t lo, size_t hi, int ax, vector<Pt>& out) const {
        if (hi - lo <= 8) {
            for (size_t i = lo; i < hi; ++i) if (sd2(p, s[i]) <= r2 && crosses(P, Q, s[i])) out.push_back(crosspt(P, Q, s[i]));
            return;
        }
        size_t mid = (lo + hi)/2;
        if (sd2(p, s[mid]) <= r2 && crosses(P, Q, s[mid])) out.push_back(crosspt(P, Q, s[mid]));
        double t = p[ax] - s[mid].m[ax];
        int nx = (ax+1)%3;
        if (t - H <= r) cross_rec(p, r2, r, P, Q, lo, mid, nx, out);
        if (-t - H <= r) cross_rec(p, r2, r, P, Q, mid+1, hi, nx, out);
    }
    // the points where the arc PQ (unit P, Q) crosses the stored arcs
    void crossings(const Pt& P, const Pt& Q, vector<Pt>& out) const {
        if (s.empty()) return;
        double mx = P.x + Q.x, my = P.y + Q.y, mz = P.z + Q.z, ml = std::sqrt(mx*mx + my*my + mz*mz);
        if (ml < 1e-12) return;
        Pt M{mx/ml, my/ml, mz/ml};
        double r = dist3(P, M) + 1e-9, p[3] = {M.x, M.y, M.z};
        cross_rec(p, r*r, r, P, Q, 0, s.size(), 0, out);
    }
    // piece i's ends, on the sphere
    Pt end(size_t i, int k) const {
        const float* e = k ? s[i].b : s[i].a;
        double l = std::sqrt((double)e[0]*e[0] + (double)e[1]*e[1] + (double)e[2]*e[2]);
        return Pt{e[0]/l, e[1]/l, e[2]/l};
    }
    // does the arc PQ (unit P, Q) properly cross one of the stored arcs?
    bool hits(const Pt& P, const Pt& Q) const {
        if (s.empty()) return false;
        double mx = P.x + Q.x, my = P.y + Q.y, mz = P.z + Q.z, ml = std::sqrt(mx*mx + my*my + mz*mz);
        if (ml < 1e-12) return false;
        Pt M{mx/ml, my/ml, mz/ml};                     // the arc's midpoint
        double r = dist3(P, M) + 1e-9;                // every point of the arc is this close to M
        if (dist(M, r) > r) return false;             // nothing near: the common, cheap case
        double p[3] = {M.x, M.y, M.z};
        return hits_rec(p, r*r, r, P, Q, 0, s.size(), 0);
    }
};

// open-addressing map: edge (vertex pair) -> vertex id
struct EdgeMap {
    bvec<uint64_t> k; bvec<int> v; size_t n = 0, mask = 0;
    static uint64_t key(int a, int b) { if (a > b) std::swap(a, b); return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b; }
    static size_t h(uint64_t x) { return (size_t)mix64(x); }
    void grow() {
        bvec<uint64_t> ok; bvec<int> ov; ok.swap(k); ov.swap(v);
        size_t cap = ok.empty() ? 1024 : 2*ok.size();
        k.assign(cap, ~0ull); v.assign(cap, -1); mask = cap - 1;
        for (size_t i = 0; i < ok.size(); ++i) if (ok[i] != ~0ull) {
            size_t j = h(ok[i]) & mask; while (k[j] != ~0ull) j = (j+1) & mask;
            k[j] = ok[i]; v[j] = ov[i];
        }
    }
    int find(uint64_t x) const {
        if (k.empty()) return -1;
        for (size_t j = h(x) & mask; ; j = (j+1) & mask) { if (k[j] == x) return v[j]; if (k[j] == ~0ull) return -1; }
    }
    void insert(uint64_t x, int val) {                   // x must be absent
        if (4*(n+1) > 3*k.size()) grow();             // load factor <= 3/4
        size_t j = h(x) & mask; while (k[j] != ~0ull) j = (j+1) & mask;
        k[j] = x; v[j] = val; ++n;
    }
    void clear() { bvec<uint64_t>().swap(k); bvec<int>().swap(v); n = 0; mask = 0; }
};

// run work(lo, hi) over [0, n) in blocks of 4096, on up to 8 threads if n is large
template <class F> static void parallel_blocks(size_t n, F work) {
    std::atomic<size_t> next(0);
    auto run = [&]() {
        for (;;) {
            size_t a = next.fetch_add(4096);
            if (a >= n) break;
            work(a, std::min(n, a + 4096));
        }
    };
    unsigned nt = n < 16384 ? 1 : std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    vector<std::thread> pool;
    for (unsigned k = 1; k < nt; ++k) pool.emplace_back(run);
    run();
    for (auto& th : pool) th.join();
}

} // namespace wheel_detail

static void compute_wheel(const Renderer& rp, const Renderer& rn, int res, Renderer& wheel) {
    using namespace wheel_detail;
    wheel.clear_output();
    if (rp.px.empty() || rn.px.empty()) return;
    const double emin = PI/std::max(res, 8);          // finest mesh edge
    // For n = inf the zippers touch at every parabolic fixed point (tr[a,b] = -2), and near
    // those points they interlock below the drawing accuracy, so the wheel cannot avoid them
    // there anyway: stop refining at about the accuracy where they (nearly) meet.
    const Mat& A = rp.rep.A; const Mat& B = rp.rep.B;
    bool cusped = std::abs(tr(matmul(matmul(A, B), matmul(rp.rep.Ai, rp.rep.Bi))) + 2.0) < 1e-6;
    const double ENTER = 0.5, KGAP = 1.0, KFLOOR = 0.25, LIP = 2.2, TOUCH = 1.0;
    const double etouch = std::max(emin, rp.accuracy);   // n = inf: finest edge where the zippers meet
    const int LEV0 = 4, MAXROUND = 4;                 // uniform start (edge ~0.07); pocket rounds
    const double TEND = 0.02;                         // n = inf: crossing points this far from a vertex
    const size_t MAXV = 24000000;                     // mesh budget (~140 bytes per vertex)
    const double MESH_MEM = 3.5e9;                    // ... or once the process holds 3.5 GB
    size_t maxv = MAXV;                               // (lowered then)
    bool budgetHit = false;
    SegTree tz[2];                                    // tz[1] = Z+, tz[0] = Z-
    tz[1].build(rp, 0.004); tz[0].build(rn, 0.004);
    SegTree tb[2];                                    // the bridges of each side (not drawn)
    vector<std::array<Pt,2>> bridge[2];
    tlog("  wheel: kd-trees");

    // Mesh vertices with f and the gap d+ + d-.  New vertices are queued with upper bounds on
    // their two distances and evaluated in batches (in parallel: the kd-tree queries dominate).
    bvec<double> vx, vy, vz;                          // (double: the mesh can get very fine)
    bvec<float> vf, vg;
    bvec<double> qbp, qbm;                            // bounds for the queued vertices
    size_t nEval = 0;                                 // vertices [0, nEval) are evaluated
    auto addV = [&](double x, double y, double z, double bp, double bm) -> int {
        double n = std::sqrt(x*x + y*y + z*z); x /= n; y /= n; z /= n;
        vx.push_back(x); vy.push_back(y); vz.push_back(z);
        vf.push_back(0); vg.push_back(0); qbp.push_back(bp); qbm.push_back(bm);
        return (int)vx.size() - 1;
    };
    // distance to side sd's zipper and bridges
    auto zdist = [&](int sd, const Pt& p, double bound) {
        double d = tz[sd].dist(p, bound);
        if (!tb[sd].s.empty()) d = std::min(d, tb[sd].dist(p, d));
        return d;
    };
    auto flush = [&]() {
        const size_t lo = nEval, hi = vx.size();
        if (lo >= hi) return;
        parallel_blocks(hi - lo, [&](size_t a, size_t b) {
            for (size_t i = lo + a; i < lo + b; ++i) {
                Pt p{vx[i], vy[i], vz[i]};
                double dp = zdist(1, p, qbp[i-lo]), dm = zdist(0, p, qbm[i-lo]);
                vf[i] = (float)(dm - dp); vg[i] = (float)(dm + dp);
            }
        });
        nEval = hi; qbp.clear(); qbm.clear();
    };
    auto dplus  = [&](int v) { return 0.5*((double)vg[v] - vf[v]); };
    auto dminus = [&](int v) { return 0.5*((double)vg[v] + vf[v]); };
    auto dside  = [&](int sd, int v) { return sd ? dplus(v) : dminus(v); };
    auto elen = [&](int u, int w) {
        double x = vx[u]-vx[w], y = vy[u]-vy[w], z = vz[u]-vz[w];
        return std::sqrt(x*x + y*y + z*z);
    };
    auto vpt = [&](int v) { return Pt{vx[v], vy[v], vz[v]}; };
    // a new vertex at the (normalised) mean of corners; the distance at a corner plus the
    // distance to it bounds the new vertex's distance (2e-6 slack covers the float storage
    // of the distances)
    auto meanBounds = [&](const int* c, int nc, double& x, double& y, double& z, double& bp, double& bm) {
        x = 0; y = 0; z = 0;
        for (int i = 0; i < nc; ++i) { x += vx[c[i]]; y += vy[c[i]]; z += vz[c[i]]; }
        double n = std::sqrt(x*x + y*y + z*z); x /= n; y /= n; z /= n;
        bp = 4; bm = 4;
        for (int i = 0; i < nc; ++i) {
            double dx = x-vx[c[i]], dy = y-vy[c[i]], dz = z-vz[c[i]], r = std::sqrt(dx*dx + dy*dy + dz*dz) + 2e-6;
            bp = std::min(bp, dplus(c[i]) + r); bm = std::min(bm, dminus(c[i]) + r);
        }
    };
    auto addMean = [&](const int* c, int nc) -> int {
        double x, y, z, bp, bm;
        meanBounds(c, nc, x, y, z, bp, bm);
        return addV(x, y, z, bp, bm);
    };
    EdgeMap midp;                                     // split edge -> its midpoint vertex
    auto mid = [&](int u, int w) -> int {
        uint64_t k = EdgeMap::key(u, w);
        int m = midp.find(k); if (m >= 0) return m;
        int c[2] = {u, w};
        m = addMean(c, 2); midp.insert(k, m); return m;
    };

    // icosahedron
    const double gr = (1.0 + std::sqrt(5.0))/2.0;
    const double IV[12][3] = {{-1,gr,0},{1,gr,0},{-1,-gr,0},{1,-gr,0},{0,-1,gr},{0,1,gr},
                              {0,-1,-gr},{0,1,-gr},{gr,0,-1},{gr,0,1},{-gr,0,-1},{-gr,0,1}};
    const int IF[20][3] = {{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},
                           {10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},
                           {6,2,10},{8,6,7},{9,8,1}};
    for (auto& v : IV) addV(v[0], v[1], v[2], 4, 4);
    flush();
    struct T { int a, b, c, lev, force; };
    bvec<T> st;
    bvec<std::array<int,4>> leaves;                   // corners + level
    for (auto& f : IF) st.push_back({f[0], f[1], f[2], 0, 0});
    bvec<T> nextLevel;
    // grow the vertex arrays by half at a time (not 2x: they are the bulk of the memory)
    auto reserveV = [&](size_t n) {
        if (n <= vx.capacity()) return;
        size_t c = std::max(n, vx.capacity() + vx.capacity()/2);
        vx.reserve(c); vy.reserve(c); vz.reserve(c); vf.reserve(c); vg.reserve(c);
    };
    auto refine = [&]() {                            // level by level, so each level's new
        while (!st.empty()) {                         // vertices are evaluated in one batch
          if (maxv > vx.size() && resident_bytes() > MESH_MEM) maxv = vx.size();
          reserveV(vx.size() + 3*st.size());
          if (leaves.size() + st.size() > leaves.capacity())
              leaves.reserve(std::max(leaves.size() + st.size(), leaves.capacity() + leaves.capacity()/2));
          for (const T& t : st) {
            double e = std::max(elen(t.a,t.b), std::max(elen(t.b,t.c), elen(t.c,t.a)));
            bool split = t.lev < LEV0 || (t.force > 0 && e > emin);
            if (vx.size() >= maxv) { budgetHit = true; split = false; }
            else if (!split && e > emin) {
                int npos = 0; double fmin = 1e9, g = 1e9, dpm = 1e9, dmm = 1e9;
                for (int v : {t.a, t.b, t.c}) {
                    npos += vf[v] > 0; fmin = std::min(fmin, (double)std::fabs(vf[v]));
                    g = std::min(g, (double)vg[v]); dpm = std::min(dpm, dplus(v)); dmm = std::min(dmm, dminus(v));
                }
                double dmin = npos == 3 ? dmm : npos == 0 ? dpm : std::min(dpm, dmm);
                bool enter  = dmin <= ENTER*e;
                bool hidden = (npos == 3 || npos == 0) && fmin <= LIP*e && e > KGAP*g;
                bool touch  = cusped && g <= TOUCH*e && e <= etouch;  // the zippers meet here (n = inf)
                split = (enter || hidden) && e > KFLOOR*g && !touch;
            }
            if (!split) { leaves.push_back({t.a, t.b, t.c, t.lev}); continue; }
            int ab = mid(t.a,t.b), bc = mid(t.b,t.c), ca = mid(t.c,t.a), f = std::max(0, t.force-1);
            nextLevel.push_back({t.a, ab, ca, t.lev+1, f}); nextLevel.push_back({ab, t.b, bc, t.lev+1, f});
            nextLevel.push_back({ca, bc, t.c, t.lev+1, f}); nextLevel.push_back({ab, bc, ca, t.lev+1, f});
          }
          flush();
          st.swap(nextLevel); nextLevel.clear();
        }
        bvec<T>().swap(st); bvec<T>().swap(nextLevel);
    };
    refine();
    tlog("  wheel: refine");

    // Conforming triangulation (in parallel; leaf li's fan centre, if any, is vertex centre[li]),
    // labels, and pocket handling.
    bvec<std::array<int,3>> tri;
    bvec<int> centre, nbnd;
    // the vertices along the edge u->w, u first (none of w)
    auto expand = [&](int u, int w, vector<int>& out, vector<std::pair<int,int>>& es) {
        es.assign(1, {u, w});
        while (!es.empty()) {
            auto ed = es.back(); es.pop_back();
            int m = midp.find(EdgeMap::key(ed.first, ed.second));
            if (m < 0) out.push_back(ed.first);
            else { es.push_back({m, ed.second}); es.push_back({ed.first, m}); }
        }
    };
    auto triangulate = [&]() {
        const size_t NL = leaves.size();
        nbnd.assign(NL, 0);
        parallel_blocks(NL, [&](size_t a, size_t b) {
            vector<int> poly; vector<std::pair<int,int>> es;
            for (size_t li = a; li < b; ++li) {
                const auto& L = leaves[li];
                poly.clear(); expand(L[0], L[1], poly, es); expand(L[1], L[2], poly, es); expand(L[2], L[0], poly, es);
                nbnd[li] = (int)poly.size();
            }
        });
        // fan centres get consecutive ids, triangles consecutive slots, in leaf order
        // (nbnd becomes the offsets: leaf li's triangles are [nbnd[li], nbnd[li+1]))
        centre.assign(NL, -1);
        size_t nv0 = vx.size(), nc = 0;
        int ntri = 0;
        nbnd.push_back(0);
        for (size_t li = 0; li < NL; ++li) {
            int nb = nbnd[li];
            nbnd[li] = ntri; ntri += nb == 3 ? 1 : nb;
            if (nb > 3) centre[li] = (int)(nv0 + nc++);
        }
        nbnd[NL] = ntri;
        size_t nv1 = nv0 + nc;
        reserveV(nv1);
        vx.resize(nv1); vy.resize(nv1); vz.resize(nv1); vf.resize(nv1); vg.resize(nv1);
        qbp.resize(nc); qbm.resize(nc);
        tri.clear(); tri.shrink_to_fit(); tri.resize(ntri);
        parallel_blocks(NL, [&](size_t a, size_t b) {
            vector<int> poly; vector<std::pair<int,int>> es;
            for (size_t li = a; li < b; ++li) {
                const auto& L = leaves[li];
                size_t o = nbnd[li];
                if (centre[li] < 0) { tri[o] = {L[0], L[1], L[2]}; continue; }
                poly.clear(); expand(L[0], L[1], poly, es); expand(L[1], L[2], poly, es); expand(L[2], L[0], poly, es);
                int c = centre[li];
                double x, y, z, bp, bm;
                meanBounds(&L[0], 3, x, y, z, bp, bm);
                vx[c] = x; vy[c] = y; vz[c] = z; qbp[c-nv0] = bp; qbm[c-nv0] = bm;
                for (size_t i = 0; i < poly.size(); ++i) tri[o+i] = {c, poly[i], poly[(i+1)%poly.size()]};
            }
        });
        flush();
    };

    bvec<char> side;
    bvec<int> uf, comp;                             // comp: component root (labelling pass)
    auto find = [&](int x) { while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; };
    long pockets = 0, flipped = 0, islandsLeft[2] = {0, 0}, nBridges[2] = {0, 0}, bridgeFail = 0;
    size_t workE = 0, nClear = 0;                     // edges searched, clearances (bridges)
    int round = 0;
    for (;; ++round) {
        size_t nv0 = vx.size();
        triangulate();
        tlog("  wheel: triangulate");
        // each side should be connected; its other components are pockets (the zero set of
        // the labelling on a triangulated sphere is one circle iff both sides are connected)
        const int NV = (int)vx.size();
        side.assign(NV, 0);
        for (int v = 0; v < NV; ++v) side[v] = vf[v] > 0;
        bvec<char> stray(NV, 0);
        int best[2] = {-1, -1};
        comp.assign(NV, -1);
        pockets = 0; flipped = 0;
        uf.resize(NV);
        bvec<int> cnt(NV);
        // the components of both sides at once; the Z+ side's pockets are relabelled first, and
        // only if there are any must the Z- side's components be found again
        auto unite = [&](int sd) {
            for (int v = 0; v < NV; ++v) uf[v] = v;
            for (auto& t : tri) for (int k = 0; k < 3; ++k) {
                int u = t[k], w = t[(k+1)%3];
                if (side[u] == side[w] && (sd < 0 || side[u] == sd)) { int a = find(u), b = find(w); if (a != b) uf[a] = b; }
            }
        };
        unite(-1);
        for (int pass = 0; pass < 2; ++pass) {
            char sd = pass == 0;                      // the Z+ side first, then the Z- side
            if (pass == 1 && flipped > 0) unite(0);
            std::fill(cnt.begin(), cnt.end(), 0);
            int bst = -1;
            for (int v = 0; v < NV; ++v) if (side[v] == sd) {
                int r = find(v); if (++cnt[r] == 1) ++pockets;
                if (bst < 0 || cnt[r] > cnt[bst]) bst = r;
            }
            if (bst >= 0) --pockets;
            best[(int)sd] = bst;
            for (int v = 0; v < NV; ++v) if (side[v] == sd) {
                int r = find(v);
                if (!stray[v]) comp[v] = r;
                if (r != bst) { if (!stray[v]) stray[v] = sd ? 1 : 2; side[v] = !sd; ++flipped; }
            }
        }
        tlog("  wheel: label");
        bvec<int>().swap(cnt);
        // the pockets: their vertices, and whether they hold a piece of their own zipper
        // (some vertex much nearer to it than to the other zipper: an island)
        struct Pocket { int sd; vector<int> vs; bool island; };
        vector<Pocket> pk;
        {
            std::unordered_map<int64_t,int> idx;
            for (int v = 0; v < NV; ++v) if (stray[v]) {
                int64_t key = 2*(int64_t)comp[v] + (stray[v] - 1);
                auto it = idx.find(key);
                int i;
                if (it == idx.end()) { i = (int)pk.size(); idx[key] = i; pk.push_back({stray[v] == 1, {}, false}); }
                else i = it->second;
                pk[i].vs.push_back(v);
                if (dside(pk[i].sd, v) < 0.25*vg[v]) pk[i].island = true;
            }
        }
        islandsLeft[0] = islandsLeft[1] = 0;
        for (auto& p : pk) islandsLeft[p.sd] += p.island;
        // another round only for islands (the other pockets hold no piece of their zipper, so
        // merging them is harmless)
        if (pockets == 0 || round == MAXROUND || islandsLeft[0] + islandsLeft[1] == 0 || budgetHit) break;

        // bridges for the islands, Z+ side first (the Z- bridges then avoid the Z+ ones)
        long added = 0;
        for (int sd = 1; sd >= 0; --sd) {
            vector<int> isl;
            for (size_t i = 0; i < pk.size(); ++i) if (pk[i].island && pk[i].sd == sd) isl.push_back((int)i);
            if (isl.empty()) continue;
            // local search regions: a ball about each island
            struct Reg { Pt c; double r; };
            vector<Reg> reg;
            for (int i : isl) {
                double x = 0, y = 0, z = 0;
                for (int v : pk[i].vs) { x += vx[v]; y += vy[v]; z += vz[v]; }
                double n = std::sqrt(x*x + y*y + z*z); Pt c{x/n, y/n, z/n};
                double r = 0;
                for (int v : pk[i].vs) r = std::max(r, dist3(c, vpt(v)));
                reg.push_back({c, std::min(2.0, std::max(8*r, 0.02))});   // (2: the whole sphere)
            }
            // the local edges of each region (from the triangles inside it), found through a
            // grid of cubes; only the cubes that meet the sphere are listed (a large pocket
            // used to list millions of empty cubes, 6 GB at -n inf -R 6.5)
            const double CS = 0.05;
            std::unordered_map<uint64_t, vector<int>> grid;
            auto cell = [&](double x, double y, double z) {
                return ((uint64_t)(int64_t)std::floor(x/CS + 64) << 42) | ((uint64_t)(int64_t)std::floor(y/CS + 64) << 21)
                       | (uint64_t)(int64_t)std::floor(z/CS + 64);
            };
            for (size_t k = 0; k < reg.size(); ++k) {
                const Reg& R = reg[k];
                int i0 = (int)std::floor((R.c.x - R.r)/CS), i1 = (int)std::floor((R.c.x + R.r)/CS);
                int j0 = (int)std::floor((R.c.y - R.r)/CS), j1 = (int)std::floor((R.c.y + R.r)/CS);
                int k0 = (int)std::floor((R.c.z - R.r)/CS), k1 = (int)std::floor((R.c.z + R.r)/CS);
                for (int i = i0; i <= i1; ++i) for (int j = j0; j <= j1; ++j) for (int l = k0; l <= k1; ++l) {
                    double x = (i+0.5)*CS, y = (j+0.5)*CS, z = (l+0.5)*CS;
                    if (std::fabs(std::sqrt(x*x + y*y + z*z) - 1.0) > CS) continue;   // misses the sphere
                    grid[cell(x, y, z)].push_back((int)k);
                }
            }
            // The regions' edges are counted first, and the regions are then searched in batches
            // of at most EBUDGET edges in all (one pass over the triangles per batch): a few large
            // pockets (-n inf at -R 6.5 has dozens spanning much of the sphere) would otherwise
            // need many GB.  A region with more than EBUDGET edges is left unbridged (it is then
            // merged, as for a failed bridge), and so are all regions once WBUDGET edges have
            // been searched in all, or CBUDGET clearances computed (bounds on the time: -n inf
            // -R 6.5 and thin monodromies such as R^10 L have thousands of islands).
            const size_t EBUDGET = 20000000, WBUDGET = 200000000, CBUDGET = 10000000;
            vector<size_t> cntE(reg.size(), 0);
            auto inside = [&](const Reg& R, const std::array<int,3>& t) {
                return dist3(R.c, vpt(t[0])) <= R.r && dist3(R.c, vpt(t[1])) <= R.r && dist3(R.c, vpt(t[2])) <= R.r; };
            for (auto& t : tri) {
                auto it = grid.find(cell(vx[t[0]], vy[t[0]], vz[t[0]]));
                if (it == grid.end()) continue;
                for (int k : it->second) if (inside(reg[k], t)) cntE[k] += 3;
            }
            // the regions searched: the smallest first, within the budgets
            vector<char> admit(reg.size(), 0);
            {
                vector<int> ord(reg.size());
                for (size_t k = 0; k < reg.size(); ++k) ord[k] = (int)k;
                std::sort(ord.begin(), ord.end(), [&](int p, int q) {
                    return cntE[p] != cntE[q] ? cntE[p] < cntE[q] : p < q; });
                for (int k : ord)
                    if (cntE[k] <= EBUDGET && workE + cntE[k] <= WBUDGET) { admit[k] = 1; workE += cntE[k]; }
            }
            vector<vector<std::pair<int,int>>> redges(reg.size());
            vector<char> use(reg.size(), 0);
            size_t kb = 0;                                // the current batch: regions [kb, ke)
            auto next_batch = [&]() -> size_t {
                size_t ke = kb, tot = 0;
                while (ke < reg.size() && (ke == kb || tot + admit[ke]*cntE[ke] <= EBUDGET)) { tot += admit[ke]*cntE[ke]; ++ke; }
                std::fill(use.begin(), use.end(), 0);
                for (size_t k = kb; k < ke; ++k) if (admit[k]) { use[k] = 1; redges[k].reserve(cntE[k]); }
                for (auto& t : tri) {
                    auto it = grid.find(cell(vx[t[0]], vy[t[0]], vz[t[0]]));
                    if (it == grid.end()) continue;
                    for (int k : it->second) {
                        if (!use[k] || !inside(reg[k], t)) continue;
                        for (int q = 0; q < 3; ++q) {
                            int u = t[q], w = t[(q+1)%3];
                            if (u < w) redges[k].push_back({u, w}); else redges[k].push_back({w, u});
                        }
                    }
                }
                return ke;
            };
            size_t ke = 0;
            // clearance of the chord uw from the other side's zipper and bridges
            const int so = !sd;
            auto clear = [&](int u, int w, double bound) {
                ++nClear;
                Pt P = vpt(u), Q = vpt(w);
                double c = tz[so].segdist(P, Q, bound);
                if (!tb[so].s.empty()) c = std::min(c, tb[so].segdist(P, Q, c));
                return c;
            };
            const double CMIN = 1e-6;
            for (size_t k = 0; k < reg.size(); ++k) {
                if (k == ke) { kb = k; ke = next_batch(); }
                if (!use[k] || nClear > CBUDGET) { ++bridgeFail; continue; }
                auto& E = redges[k];
                std::sort(E.begin(), E.end()); E.erase(std::unique(E.begin(), E.end()), E.end());
                std::unordered_map<int,int> lid;      // mesh vertex -> local id
                vector<int> gv;
                auto L = [&](int v) { auto it = lid.find(v); if (it != lid.end()) return it->second;
                                      int i = (int)gv.size(); lid[v] = i; gv.push_back(v); return i; };
                vector<std::pair<int,int>> le;
                for (auto& e : E) le.push_back({L(e.first), L(e.second)});
                vector<std::pair<int,int>>().swap(E);
                const int n = (int)gv.size();
                vector<vector<int>> adj(n);
                for (size_t i = 0; i < le.size(); ++i) { adj[le[i].first].push_back((int)i); adj[le[i].second].push_back((int)i); }
                const Pocket& P = pk[isl[k]];
                vector<char> src(n, 0);
                for (int v : P.vs) { auto it = lid.find(v); if (it != lid.end()) src[it->second] = 1; }
                // vertex in the main part of side sd (its original label)
                auto target = [&](int i) { int v = gv[i]; return (vf[v] > 0) == (sd == 1) && comp[v] == best[sd]; };
                // 1. the largest clearance cs of a path (maximin Dijkstra)
                vector<double> bw(n, -1);
                std::priority_queue<std::pair<double,int>> pq;
                for (int i = 0; i < n; ++i) if (src[i]) { bw[i] = 1.0; pq.push({1.0, i}); }
                double cs = -1;
                while (!pq.empty()) {
                    auto [b, i] = pq.top(); pq.pop();
                    if (b < bw[i]) continue;
                    if (target(i)) { cs = b; break; }
                    for (int ei : adj[i]) {
                        int j = le[ei].first == i ? le[ei].second : le[ei].first;
                        if (bw[j] >= b) continue;
                        double nb = std::min(b, clear(gv[i], gv[j], b));
                        if (nb > CMIN && nb > bw[j]) { bw[j] = nb; pq.push({nb, j}); }
                    }
                }
                if (cs <= CMIN) { ++bridgeFail; continue; }
                // 2. the shortest path with clearance >= cs/2
                const double cth = 0.5*cs;
                vector<double> dd(n, 1e300); vector<int> from(n, -1);
                std::priority_queue<std::pair<double,int>, vector<std::pair<double,int>>, std::greater<std::pair<double,int>>> q2;
                for (int i = 0; i < n; ++i) if (src[i]) { dd[i] = 0; q2.push({0.0, i}); }
                int hit = -1;
                while (!q2.empty()) {
                    auto [d, i] = q2.top(); q2.pop();
                    if (d > dd[i]) continue;
                    if (target(i)) { hit = i; break; }
                    for (int ei : adj[i]) {
                        int j = le[ei].first == i ? le[ei].second : le[ei].first;
                        double nd = d + elen(gv[i], gv[j]);
                        if (nd >= dd[j] || clear(gv[i], gv[j], cth) < cth) continue;
                        dd[j] = nd; from[j] = i; q2.push({nd, j});
                    }
                }
                if (hit < 0) { ++bridgeFail; continue; }
                for (int j = hit; from[j] >= 0; j = from[j]) bridge[sd].push_back({vpt(gv[from[j]]), vpt(gv[j])});
                ++nBridges[sd]; ++added;
            }
            tb[sd].build(bridge[sd], 0.004);
        }
        // the bridges lower the distances near them: update the evaluated vertices
        bvec<char> chg;
        if (added) {
            chg.assign(nv0, 0);
            parallel_blocks(nv0, [&](size_t a, size_t b) {
                for (size_t v = a; v < b; ++v) {
                    Pt p = vpt((int)v);
                    double dp = dplus((int)v), dm = dminus((int)v), dp1 = dp, dm1 = dm;
                    if (!tb[1].s.empty()) dp1 = std::min(dp, tb[1].dist(p, dp));
                    if (!tb[0].s.empty()) dm1 = std::min(dm, tb[0].dist(p, dm));
                    if (dp1 < dp || dm1 < dm) { vf[v] = (float)(dm1 - dp1); vg[v] = (float)(dm1 + dp1); chg[v] = 1; }
                }
            });
        }
        if (!added) break;
        // refine the rims of the pockets (leaves with a stray corner and both signs) two more
        // levels, and re-examine the leaves that the bridges changed (the other leaves are
        // kept, compacted in place)
        size_t nk = 0;
        for (size_t li = 0; li < leaves.size(); ++li) {
            const std::array<int,4> L = leaves[li];
            int c = centre[li];
            bool hit = stray[L[0]] || stray[L[1]] || stray[L[2]] || (c >= 0 && stray[c]);
            bool pos = vf[L[0]] > 0;
            bool mixed = (vf[L[1]] > 0) != pos || (vf[L[2]] > 0) != pos || (c >= 0 && (vf[c] > 0) != pos);
            double e = std::max(elen(L[0],L[1]), std::max(elen(L[1],L[2]), elen(L[2],L[0])));
            if (hit && mixed && e > 2*emin) st.push_back({L[0], L[1], L[2], L[3], 2});
            else if (chg[L[0]] || chg[L[1]] || chg[L[2]]) st.push_back({L[0], L[1], L[2], L[3], 0});
            else leaves[nk++] = L;
        }
        leaves.resize(nk);
        vx.resize(nv0); vy.resize(nv0); vz.resize(nv0); vf.resize(nv0); vg.resize(nv0);   // drop the fan centres
        nEval = nv0;
        // the triangulation and labels are rebuilt after refining
        bvec<std::array<int,3>>().swap(tri); bvec<int>().swap(centre); bvec<int>().swap(nbnd);
        bvec<int>().swap(comp); bvec<int>().swap(uf); bvec<char>().swap(stray); bvec<char>().swap(side);
        refine();
        tlog("  wheel: refine pockets");
    }
    bvec<std::array<int,4>>().swap(leaves);
    bvec<int>().swap(centre); bvec<int>().swap(nbnd); bvec<int>().swap(comp);
    midp.clear();
    bvec<int>().swap(uf);
    const int NV = (int)vx.size();

    // march the triangles: one crossing point per sign-changing edge, one segment per triangle
    bvec<Pt> node;
    bvec<std::array<int,2>> adj;
    EdgeMap cross;
    auto crossPoint = [&](int u, int w) -> int {
        uint64_t key = EdgeMap::key(u, w);
        int id = cross.find(key); if (id >= 0) return id;
        double fu = vf[u], fw = vf[w];
        if (side[u] != (fu > 0) || side[w] != (fw > 0)) fu = fw = 0;   // merged pocket: midpoint
        double t = (fu == fw) ? 0.5 : std::max(0.0, std::min(1.0, fu/(fu - fw)));
        // n = inf: keep the point off the edge's ends.  Where the zippers touch, f is ~0 at some
        // vertices whose neighbours alternate in sign; the crossing points of all their edges
        // then fell on the vertex (in single precision), and the wheel passed through it more
        // than once (1245 self-crossings for fig8 at the defaults).  Each segment lies in its
        // own triangle, so with the points kept apart the curve is embedded.
        if (cusped) t = std::max(TEND, std::min(1.0 - TEND, t));
        double x = vx[u] + t*(vx[w]-vx[u]), y = vy[u] + t*(vy[w]-vy[u]), z = vz[u] + t*(vz[w]-vz[u]);
        double n = std::sqrt(x*x + y*y + z*z); if (n < 1e-12) n = 1;
        id = (int)node.size(); node.push_back({x/n, y/n, z/n}); adj.push_back({{-1,-1}});
        cross.insert(key, id); return id;
    };
    bool simple = true;
    auto link = [&](int a, int b) {
        if (adj[a][0] < 0) adj[a][0] = b; else if (adj[a][1] < 0) adj[a][1] = b; else simple = false;
    };
    for (auto& t : tri) {
        int e[3], m = 0;
        for (int k = 0; k < 3; ++k) { int u = t[k], w = t[(k+1)%3];
            if (side[u] != side[w]) e[m++] = crossPoint(u, w); }
        if (m == 2) { link(e[0], e[1]); link(e[1], e[0]); }
    }
    bvec<std::array<int,3>>().swap(tri);
    cross.clear();
    const int M = (int)node.size();
    tlog("  wheel: march");
    if (g_timing)
        fprintf(stderr, "timing:   (wheel mesh: %d vertices, %d crossings; %d pocket round(s), "
                "%ld+%ld bridges (%ld failed), %ld pockets merged (%ld+%ld islands), %ld vertices relabelled)\n",
                NV, M, round, nBridges[1], nBridges[0], bridgeFail, pockets, islandsLeft[1], islandsLeft[0], flipped);
    if (budgetHit)
        fprintf(stderr, "warning: Catherine wheel: mesh budget (%zu vertices, or 3.5 GB of memory) reached at "
                "%zu vertices, so the wheel may cross the zippers where they are close (use a smaller -R "
                "or --wheelres)\n", MAXV, maxv < MAXV ? maxv : MAXV);
    if (M == 0) return;
    // walk the cycle; it should visit every crossing point.  (Points that coincide once
    // stored in single precision, e.g. crossings near a vertex where f is ~0, are kept once.)
    int prev = -1, cur = 0; long steps = 0;
    wheel.pstart.push_back(0);
    auto emit = [&](const Pt& p) {
        size_t k = wheel.px.size();
        if (k > 0 && wheel.px[k-1] == (float)p.x && wheel.py[k-1] == (float)p.y && wheel.pz[k-1] == (float)p.z) return;
        wheel.push_point(p.x, p.y, p.z);
    };
    for (;;) {
        emit(node[cur]); ++steps;
        int nxt = adj[cur][0] != prev ? adj[cur][0] : adj[cur][1];
        if (nxt < 0 || steps > M) { simple = false; break; }
        prev = cur; cur = nxt;
        if (cur == 0) { emit(node[0]); break; }
    }
    {   // closed: the last point is the first
        size_t k = wheel.px.size();
        if (k >= 2 && !(wheel.px[k-1] == wheel.px[0] && wheel.py[k-1] == wheel.py[0] && wheel.pz[k-1] == wheel.pz[0]) && simple)
            wheel.push_point(wheel.px[0], wheel.py[0], wheel.pz[0]);
    }
    if (!simple || steps != M)
        fprintf(stderr, "warning: Catherine wheel is not a single simple cycle (%ld of %d crossings)\n",
                steps, M);
    // Self-check.  First the points where the drawn Z+ and Z- themselves cross (as arcs on
    // S^2; this happens where the zippers come closer than the drawing accuracy resolves):
    // there no curve can separate them, so the wheel must cross them nearby.
    vector<Pt> zx;
    {
        const size_t np = tz[1].s.size();
        bvec<char> fl(np, 0);
        parallel_blocks(np, [&](size_t a, size_t b) {
            for (size_t i = a; i < b; ++i) fl[i] = tz[0].hits(tz[1].end(i, 0), tz[1].end(i, 1));
        });
        for (size_t i = 0; i < np; ++i) if (fl[i]) tz[0].crossings(tz[1].end(i, 0), tz[1].end(i, 1), zx);
    }
    // then every wheel segment against both zippers; a crossing within rho of a point of zx
    // is accounted for by it
    const double rho = std::max(1e-5, 8*emin);
    std::unordered_map<uint64_t, vector<int>> zgrid;
    auto gkey = [&](const Pt& p, int di, int dj, int dk) {
        return ((uint64_t)(int64_t)(std::floor(p.x/rho) + di + (1 << 20)) << 42) ^
               ((uint64_t)(int64_t)(std::floor(p.y/rho) + dj + (1 << 20)) << 21) ^
                (uint64_t)(int64_t)(std::floor(p.z/rho) + dk + (1 << 20));
    };
    size_t nzx = 0;                                   // distinct crossing points (1e-7 apart)
    for (size_t i = 0; i < zx.size(); ++i) {
        bool dup = false;
        for (int d = 0; d < 27 && !dup; ++d) {
            auto it = zgrid.find(gkey(zx[i], d%3 - 1, (d/3)%3 - 1, d/9 - 1));
            if (it != zgrid.end()) for (int j : it->second) if (dist3(zx[i], zx[j]) < 1e-7) { dup = true; break; }
        }
        if (!dup) ++nzx;
        zgrid[gkey(zx[i], 0, 0, 0)].push_back((int)i);
    }
    auto nearZX = [&](const Pt& p) {
        for (int d = 0; d < 27; ++d) {
            auto it = zgrid.find(gkey(p, d%3 - 1, (d/3)%3 - 1, d/9 - 1));
            if (it != zgrid.end()) for (int j : it->second) if (dist3(p, zx[j]) <= rho) return true;
        }
        return false;
    };
    long xs[2] = {0, 0}, xfar[2] = {0, 0};             // crossings of Z- / Z+: all, and away from zx
    {
        std::mutex mu;
        parallel_blocks((size_t)M, [&](size_t a, size_t b) {
            long c[2] = {0, 0}, f[2] = {0, 0};
            for (size_t i = a; i < b; ++i) for (int j : adj[i]) if (j > (int)i)
                for (int sd = 0; sd < 2; ++sd) if (tz[sd].hits(node[i], node[j])) {
                    ++c[sd];
                    Pt m{node[i].x + node[j].x, node[i].y + node[j].y, node[i].z + node[j].z};
                    double l = std::sqrt(m.x*m.x + m.y*m.y + m.z*m.z); m = {m.x/l, m.y/l, m.z/l};
                    if (!nearZX(m)) ++f[sd];
                }
            std::lock_guard<std::mutex> lk(mu);
            for (int sd = 0; sd < 2; ++sd) { xs[sd] += c[sd]; xfar[sd] += f[sd]; }
        });
    }
    tlog("  wheel: self-check");
    if (g_timing)
        fprintf(stderr, "timing:   (wheel self-check: %ld / %ld segments cross Z+ / Z-, %ld / %ld of them "
                "away from the %zu crossing point(s) of Z+ and Z-)\n", xs[1], xs[0], xfar[1], xfar[0], nzx);
    long isl = islandsLeft[0] + islandsLeft[1];
    if (cusped) {
        if (xs[0] + xs[1] + pockets > 0)
            fprintf(stderr, "note: Catherine wheel: the finite zippers touch at the parabolic points "
                    "(n = inf; also n >~ 3100, treated as the cusp), so the wheel meets them there: %ld / %ld "
                    "wheel segment(s) cross Z+ / Z-, "
                    "%ld pocket(s) merged\n", xs[1], xs[0], pockets);
    } else {
        if (nzx > 0)
            fprintf(stderr, "note: Catherine wheel: the drawn Z+ and Z- cross each other at %zu point(s), "
                    "where they come closer than the drawing accuracy resolves (a smaller -a removes "
                    "this); the wheel must cross them there (%ld / %ld segment(s) cross Z+ / Z-)\n",
                    nzx, xs[1] - xfar[1], xs[0] - xfar[0]);
        if (xfar[0] + xfar[1] > 0)
            fprintf(stderr, "warning: Catherine wheel: %ld / %ld wheel segment(s) cross Z+ / Z-, where "
                    "the mesh does not resolve the gap between the zippers (%s)\n", xfar[1], xfar[0],
                    budgetHit ? "the mesh budget was reached" : "use a larger --wheelres");
        if (isl > 0)
            fprintf(stderr, "warning: Catherine wheel: %ld piece(s) of Z+ and %ld of Z- could not be "
                    "joined to the rest of their side, so the wheel leaves them on the wrong side\n",
                    islandsLeft[1], islandsLeft[0]);
    }
}

// ------------------------------------------------------------------ options
enum Format { FMT_EPS, FMT_PNG };
static const int    PNG_MAX_SIZE = 16384;  // -s limit for PNG (~7 bytes per pixel: 1.9 GB)
static const double MAX_ZOOM     = 1e7;    // --zoom limit (points are stored as float)
struct Options {
    string monodromy = "fig8";
    int    order      = 2;
    double accuracy   = 0.004;
    double radius     = 6.0;      // hyperbolic ball radius in the 3-manifold
    double merge      = -1;       // -e: obsolete (branch identity is exact); ignored
    double zoom       = 1.0;      // magnification for EPS/PNG output
    bool   stats      = false;    // --stats: diagnostics and exactness checks
    int    overlap    = 1;        // --overlap L: overlap resolution, levels below a leaf (0 = off)
    bool   focus      = true;     // --nofocus: zoomed output keeps the roots of the ball about o
    bool   cull       = true;     // --nocull: draw the whole sphere even for EPS/PNG
    bool   chains     = true;     // --nochains: split parabolic chains step by step (testing)
    bool   zippers    = true;     // draw Z+/Z- (the viewer's wheel-only export turns this off)
    int    size       = 512;      // output size (points for EPS, pixels for PNG)
    double linewidth  = 0.0;
    double yaw = 0.0, pitch = 0.0, roll = 0.0;   // viewpoint rotation (degrees; --roll hidden)
    Format format     = FMT_EPS;
    bool   view       = false;    // launch the interactive viewer
    bool   wheel      = false;    // draw the Catherine wheel
    int    wheelres   = 10000000; // wheel mesh: finest edge pi/wheelres
    string outfile;
};

// a drawable layer: the polylines of one renderer plus its RGB colour
struct Layer { Renderer* r; double col[3]; };

// ------------------------------------------------------------------ EPS output
static void write_eps(FILE* f, const Options& opt, const vector<Layer>& layers, const View& v) {
    int S = opt.size;
    double Rpt = S/2.0 - 2.0;
    const long long G = 1000000;                 // integer coordinate grid over [-1,1]
    string norder = (opt.order<=0) ? "inf" : std::to_string(opt.order);
    fprintf(f, "%%!PS-Adobe-3.0 EPSF-3.0\n%%%%BoundingBox: 0 0 %d %d\n", S, S);
    fprintf(f, "%%%%Title: zipper %s n=%s acc=%g\n", opt.monodromy.c_str(), norder.c_str(), opt.accuracy);
    fprintf(f, "%%%%Creator: zipper.cc\n%%%%EndComments\n");
    fprintf(f, "/m{moveto}bind def /l{rlineto}bind def\n");
    fprintf(f, "gsave newpath 0 0 moveto %d 0 lineto %d %d lineto 0 %d lineto closepath clip newpath\n",
            S, S, S, S);                          // clip to the bounding box (matters when zoomed)
    fprintf(f, "%.3f %.3f translate %.10f dup scale\n", S/2.0, S/2.0, Rpt/(double)G);
    double lw = (opt.linewidth <= 0) ? 0.0 : opt.linewidth * (double)G / Rpt;
    fprintf(f, "%.4f setlinewidth 1 setlinecap 1 setlinejoin\n", lw);
    double tol = opt.accuracy * 0.5;
    double lim = 1.05 * (S/2.0) / Rpt;           // view half-width (+5%) in unit-disk units
    if (opt.zoom <= std::sqrt(2.0)*lim)          // silhouette circle (radius zoom), if it is in view
        fprintf(f, "0 setgray 0 0 %.0f 0 360 arc stroke\n", (double)G*opt.zoom);
    for (const Layer& L : layers) {
        vector<vector<P2>> polys;
        collect_polylines(*L.r, v, tol, opt.zoom, lim, polys);
        fprintf(f, "%.3f %.3f %.3f setrgbcolor\n", L.col[0], L.col[1], L.col[2]);
        for (auto& p : polys) {
            long long cx = std::llround(p[0].first*G), cy = std::llround(p[0].second*G);
            fprintf(f, "%lld %lld m", cx, cy);
            for (size_t i = 1; i < p.size(); ++i) {
                long long X = std::llround(p[i].first*G), Y = std::llround(p[i].second*G);
                fprintf(f, " %lld %lld l", X - cx, Y - cy); cx = X; cy = Y;
            }
            fputc('\n', f);
        }
        fprintf(f, "stroke\n");
    }
    fprintf(f, "grestore\n%%%%EOF\n");
}

// ------------------------------------------------------------------ PNG output
// (png_chunk, write_png_raw and the Bresenham line ras_line are png::chunk, png::write_rows
// and raster::line_index of src/output/; anti-aliased-ish via 2x supersampling)
//
// The image is rasterised at 2x supersampling into one palette byte per sample (0 white,
// 1 black, 2.. the layers' colours), box-downsampled straight into the PNG's filtered rows,
// and compressed: about 7 bytes of memory per output pixel at the peak.
static bool write_png(const char* fn, const Options& opt, const vector<Layer>& layers, const View& v) {
    const int SS = 2, W = opt.size, H = opt.size, w = W*SS, h = H*SS;
    vector<uint8_t> img((size_t)w*h, 0);                 // palette indices; white background
    uint8_t pal[256][3]; pal[0][0] = pal[0][1] = pal[0][2] = 255; pal[1][0] = pal[1][1] = pal[1][2] = 0;
    double cx = w/2.0, cy = h/2.0, rad = w/2.0 - 2.0*SS;
    auto toPx = [&](const P2& q, int& px, int& py){ px = (int)std::lround(cx + q.first*rad);
                                                     py = (int)std::lround(cy - q.second*rad); };
    // silhouette circle (black), radius = zoom; skip pieces entirely outside the image
    double crad = rad*opt.zoom;
    if (crad <= std::sqrt(2.0)*(w/2.0) + 4.0) {          // (else it is wholly out of view)
        int nc = (int)std::min((double)(1 << 22), std::max(2048.0, crad*0.5));
        for (int i = 0; i < nc; ++i) {
            double a0 = 2*PI*i/nc, a1 = 2*PI*(i+1)/nc;
            double x0 = cx+crad*std::cos(a0), y0 = cy+crad*std::sin(a0);
            double x1 = cx+crad*std::cos(a1), y1 = cy+crad*std::sin(a1);
            if (std::max(x0,x1) < -2 || std::min(x0,x1) > w+2 || std::max(y0,y1) < -2 || std::min(y0,y1) > h+2)
                continue;
            raster::line_index(img, w, h, (int)std::lround(x0), (int)std::lround(y0),
                               (int)std::lround(x1), (int)std::lround(y1), 1);
        }
    }
    double tol = opt.accuracy * 0.5;
    double lim = 1.05 * (w/2.0) / rad;           // view half-width (+5%) in unit-disk units
    for (size_t li = 0; li < layers.size() && li < 254; ++li) {
        const Layer& L = layers[li];
        uint8_t ci = (uint8_t)(2 + li);
        for (int c = 0; c < 3; ++c) pal[ci][c] = (uint8_t)std::lround(L.col[c]*255);
        vector<vector<P2>> polys; collect_polylines(*L.r, v, tol, opt.zoom, lim, polys);
        for (auto& p : polys) {              // (clipped to |X|,|Y| <= lim: pixels stay in range)
            int px0, py0; toPx(p[0], px0, py0);
            for (size_t i = 1; i < p.size(); ++i) { int px1, py1; toPx(p[i], px1, py1);
                raster::line_index(img, w, h, px0, py0, px1, py1, ci); px0=px1; py0=py1; }
        }
    }
    // box-downsample SSxSS -> 1, into PNG rows (filter byte 0 + RGB)
    vector<uint8_t> raw = raster::downsample_palette_rows(img, pal, w, W, H, SS);
    vector<uint8_t>().swap(img);
    return png::write_rows(fn, raw, W, H);           // (frees raw once compressed)
}

// ------------------------------------------------------------------ monodromy table
static vector<Monodromy> monodromy_table() {
    vector<Monodromy> t;
    { // figure-eight orbifold family: c: a->ab, b->bab (homology LR).
      // cusped character (tr a, tr b, tr ab) = (3 +/- i sqrt3)/2, trace field Q(sqrt-3).
        Monodromy m;
        m.name="fig8"; m.rl="RL";
        m.phi_a="ab";  m.phi_b="bab";
        m.psi_a="aaB"; m.psi_b="bA";
        m.cuspx=cpx(1.5,  std::sqrt(3.0)/2.0);
        m.cuspy=cpx(1.5, -std::sqrt(3.0)/2.0);
        m.cuspz=cpx(1.5,  std::sqrt(3.0)/2.0);
        t.push_back(m);
    }
    return t;
}

// The primitive root of an R/L word: the shortest v with w = v^k.  (A cyclic rotation of a
// proper power is again one, w' = (v')^k with v' the rotated v, so this also covers powers
// up to rotation.)
static string primitive_root(const string& w) {
    size_t n = w.size();
    for (size_t p = 1; p < n; ++p) {
        if (n % p) continue;
        bool per = true;
        for (size_t i = p; i < n && per; ++i) per = (std::toupper(w[i]) == std::toupper(w[i - p]));
        if (per) return w.substr(0, p);
    }
    return w;
}

// Discreteness screen for the holonomy of the orbifold (cheap; run before the root ball).
// The fiber group <a,b> of a hyperbolic orbifold is a normal subgroup of a lattice, so a, b
// and ab have infinite order and are not peripheral: loxodromic, never elliptic or parabolic
// (in particular the traces are not all real).  For finite n, c commutes with the elliptic
// [a,b] and has infinite order: it is loxodromic (at the cusp it is parabolic).  Finally a
// small hyperbolic ball must stay small (a non-discrete group fills it up to the hkey
// resolution).  (The orbit growth ~e^{2R} is not used: the plain a,b,c search that would
// measure it cheaply is incomplete enough to make it unreliable.)
static bool loxodromic(cpx t) { return !(std::abs(t.imag()) < 1e-9 && std::abs(t.real()) <= 2.0 + 1e-9); }
static bool screen_rep(const Rep& rep, int order, const string& name) {
    const char* nm[3] = { "a", "b", "ab" };
    cpx t[3] = { tr(rep.A), tr(rep.B), tr(matmul(rep.A, rep.B)) };
    string why;
    char buf[160];
    for (int i = 0; i < 3 && why.empty(); ++i)
        if (!loxodromic(t[i])) {
            snprintf(buf, sizeof buf, "tr %s = %.6f%+.6fi, so %s is %s", nm[i], pz(t[i].real()),
                     pz(t[i].imag()), nm[i], std::abs(t[i].real()) < 2.0 - 1e-9 ? "elliptic" : "parabolic");
            why = buf;
        }
    cpx tc = tr(rep.C);
    if (why.empty() && order > 0 && std::abs(tc.imag()) < 1e-9 && std::abs(tc.real()) < 2.0 - 1e-9) {
        snprintf(buf, sizeof buf, "tr c = %.6f, so c is elliptic", tc.real());
        why = buf;
    }
    if (why.empty() && count_ball(rep, 3.5, 40000) >= 40000)
        why = "a ball of radius 3.5 holds more than 40000 orbit points";
    if (why.empty()) return true;
    fprintf(stderr, "the representation found for monodromy %s at order %s is not the holonomy of "
            "a hyperbolic orbifold (%s); not drawing it\n", name.c_str(),
            order > 0 ? std::to_string(order).c_str() : "inf", why.c_str());
    return false;
}
// resolve the monodromy, solve/continue the character, build & centre the representation
static bool prepare(Options& opt, Monodromy& monOut, Rep& rep) {
    if (is_rl_word(opt.monodromy)) {
        string v = primitive_root(opt.monodromy);
        if (v.size() < opt.monodromy.size()) {
            // phi^k has the same stable/unstable laminations as phi, and the union of the
            // translates of the base arcs over the index-k subgroup <a,b,c^k> is the same set
            // (c^{+-1}-translates of base arcs are unions of fiber-group translates of base
            // arcs), so the zippers are those of v; v's full group gives the denser ball.
            fprintf(stderr, "note: monodromy %s = (%s)^%zu is a proper power; it has the same "
                    "zippers as %s, which is drawn\n", opt.monodromy.c_str(), v.c_str(),
                    opt.monodromy.size()/v.size(), v.c_str());
            opt.monodromy = v;
        }
    }
    auto table = monodromy_table();
    const Monodromy* mon = nullptr;
    Monodromy dyn;
    string lookup = (opt.monodromy == "fig8ref") ? "fig8" : opt.monodromy;
    for (auto& m : table) if (m.name == lookup) { mon = &m; break; }
    if (!mon && is_rl_word(opt.monodromy)) {
        dyn.name = opt.monodromy; dyn.rl = opt.monodromy;
        compose_phi(opt.monodromy, dyn.phi_a, dyn.phi_b);
        compose_psi(opt.monodromy, dyn.psi_a, dyn.psi_b);
        cpx cx, cy, cz;
        if (!solve_cusp_character(dyn.phi_a, dyn.phi_b, cx, cy, cz)) {
            fprintf(stderr, "could not find a geometric cusped character for monodromy %s\n",
                    opt.monodromy.c_str()); return false;
        }
        dyn.cuspx = cx; dyn.cuspy = cy; dyn.cuspz = cz;
        fprintf(stderr, "monodromy %s: phi(a)=%s phi(b)=%s ; cusped char heuristically "
                "selected (verify the picture)\n", dyn.rl.c_str(), dyn.phi_a.c_str(), dyn.phi_b.c_str());
        mon = &dyn;
    }
    if (!mon) { fprintf(stderr, "unknown monodromy '%s' (known: fig8, or an R/L word e.g. RRL)\n",
                        opt.monodromy.c_str()); return false; }
    monOut = *mon;
    if (!orient_cusp_character(monOut)) return false;     // the chirality convention

    cpx x;
    if (opt.monodromy == "fig8ref") {         // original hardcoded matrices (validation ref)
        rep.A = { cpx(0,0), cpx(0.307271825633224,0.488561861474579),
                  cpx(-0.922437197775502,1.466674119925774), cpx(1.780776406404416,0.624810533843826) };
        rep.B = { cpx(-1.300242590220120,0.624810533843827), cpx(-0.213002508653881,0.338673752180953),
                  cpx(-0.135990441204185,1.193348373421253), cpx(-0.480533816184295,0) };
        rep.C = { cpx(-1.234443936192669,-0.433122076415053), cpx(0.213002508653881,0.338673752180953),
                  cpx(0.094269316979343,-0.827235613655533), cpx(-0.901335268877188,0.433122076415053) };
        rep.Ai = matinv(rep.A); rep.Bi = matinv(rep.B); rep.Ci = matinv(rep.C);
        x = tr(rep.A);
    } else if (!build_rep(monOut, opt.order, rep, x)) {
        fprintf(stderr, "failed to solve for the holonomy representation\n"); return false;
    }
    if (!screen_rep(rep, opt.order, monOut.name)) return false;
    string norder = (opt.order<=0) ? "inf" : std::to_string(opt.order);
    bool ref = (opt.monodromy == "fig8ref");
    cpx tc = tr(rep.C);
    fprintf(stderr, "monodromy %s, order %s: tr a = %.6f%+.6fi, tr c = %.6f%+.6fi",
            monOut.name.c_str(), norder.c_str(), pz(x.real()), pz(x.imag()), pz(tc.real()), pz(tc.imag()));
    if (!ref && opt.order <= 0)
        fprintf(stderr, ", cusp shape tau = %.6f%+.6fi", pz(monOut.tau.real()), pz(monOut.tau.imag()));
    fprintf(stderr, "\n");
    if (opt.stats && !ref) {
        string w; peripheral_conjugator(monOut.phi_a, monOut.phi_b, w);
        int sg = (opt.order >= 3) ? finite_chirality(rep, w) : 0;
        fprintf(stderr, "orientation: cusp character (%.6f%+.6fi, %.6f%+.6fi, %.6f%+.6fi), cusp shape "
                "tau = t_c/t_[a,b] = %.9f%+.9fi (convention Im tau %s 0)", monOut.cuspx.real(),
                monOut.cuspx.imag(), monOut.cuspy.real(), monOut.cuspy.imag(), monOut.cuspz.real(),
                monOut.cuspz.imag(), pz(monOut.tau.real()), pz(monOut.tau.imag()), CUSP_TAU_SIGN < 0 ? "<" : ">");
        if (opt.order >= 3) fprintf(stderr, "; order %d: sigma = %+d", opt.order, sg);
        fprintf(stderr, "\n");
    }

    tlog("solve holonomy");
    if (opt.monodromy != "fig8ref") {         // conformally-centre for even density
        Mat Mc = conformal_center(rep), Mi = matinv(Mc);
        auto cj = [&](const Mat& X){ return matmul(Mc, matmul(X, Mi)); };
        rep.A = cj(rep.A); rep.Ai = cj(rep.Ai);
        rep.B = cj(rep.B); rep.Bi = cj(rep.Bi);
        rep.C = cj(rep.C); rep.Ci = cj(rep.Ci);
    }
    tlog("conformal centre");
    auto bg = std::make_shared<BallGens>();               // the root-ball search's generators
    make_ball_gens(rep, opt.order, *bg);
    rep.ball = bg;
    tlog("ball generators");
    return true;
}

// The root ball's size: about |B(rEst)| e^{2(R - rEst)} (the orbit grows like e^{2R}; the
// factor 1.15 allows for the elements the plain search behind |B(rEst)| misses; it
// overestimates by ~10%), twice that for a focused (zoomed) picture.  At most ROOT_CAP
// roots are stored (the search needs ~110 bytes per root, i.e. <= 1.8 GB; drawing them
// needs several times more, depending on the monodromy and -a).  A larger -R is refused
// before anything is allocated, and the search itself stops at the cap (with a warning)
// should the estimate be low.
static const size_t ROOT_CAP = 16000000;
static double ball_estimate(const Rep& rep, double R, bool focused) {
    const BallGens& bg = *rep.ball;
    return 1.15*(double)bg.nEst*std::exp(2.0*(R - bg.rEst))*(focused ? 2.0 : 1.0);
}
static double max_radius(const Rep& rep, bool focused, double cap = (double)ROOT_CAP) {
    const BallGens& bg = *rep.ball;
    return bg.rEst + 0.5*std::log(cap/(1.15*(double)bg.nEst*(focused ? 2.0 : 1.0)));
}
// would build_zippers focus (render_file with a zoomed, culled view)?
static bool will_focus(const Options& opt) {
    return !opt.view && opt.cull && !opt.wheel && opt.focus && opt.zoom > 1.0 + 1e-9 && opt.radius > 1.0;
}
static bool check_radius(const Options& opt, const Rep& rep, bool focused) {
    double est = ball_estimate(rep, opt.radius, focused);
    if (est <= (double)ROOT_CAP) {
        if (est > 4e6)
            fprintf(stderr, "note: -R %g gives about %.2g roots; expect a long run and several GB of "
                    "memory\n", opt.radius, est);
        return true;
    }
    fprintf(stderr, "-R %g: the root ball would have about %.2g elements, more than the limit of %zu; "
            "the largest -R for this monodromy and order is about %.2f%s\n", opt.radius, est,
            ROOT_CAP, std::floor(100*max_radius(rep, focused))/100,
            focused ? " with --zoom (or use --nofocus)" : "");
    return false;
}

// The Catherine wheel's mesh grows quickly with R (the zippers come close at more and more
// points): for fig8 -n 2 at the default -a it has 16 million vertices (2.2 GB) at -R 6.5,
// and beyond about -R 6.6 it reaches its budget (the wheel may then cross the zippers).  So
// -C is refused where the root ball would have more than WHEEL_ROOTS elements.  (A finer -a
// makes the zippers, not the mesh, larger.)  The mesh and the bridges have budgets of their
// own (compute_wheel), which bound the memory and time where the zippers come close at many
// points (thin monodromies, large orders, -n inf beyond -R 6.25).
// Large finite orders are worse: there the zippers come within ~1/n^2 of each other near
// every (nearly parabolic) cone point, and the mesh follows that gap.  fig8 -C at -R 6 is
// fine up to n = 70 but reaches the budget at n = 85, 100 and 1000 (the wheel then crosses
// the zippers); at -R 5.5 it stays below 21 million vertices for n = 100..3000.  So for
// 60 <= n with tr[a,b] not yet within 1e-6 of -2 (n <~ 3100; beyond, compute_wheel treats
// the order as the cusp, as for n = inf) the cap is WHEEL_ROOTS_LARGE_N (about -R 5.5).
static const double WHEEL_ROOTS = 6.5e5, WHEEL_ROOTS_LARGE_N = 5.5e4;
static bool wheel_large_order(int order) {
    return order >= 60 && 2.0 - 2.0*std::cos(PI/order) >= 1e-6;
}
static bool check_wheel(const Options& opt, const Rep& rep) {
    const bool big = wheel_large_order(opt.order);
    const double cap = big ? WHEEL_ROOTS_LARGE_N : WHEEL_ROOTS;
    if (ball_estimate(rep, opt.radius, false) <= cap) return true;
    fprintf(stderr, "-C: the Catherine wheel at -R %g would need a mesh beyond its budget and several "
            "GB of memory%s; the largest -R for -C for this monodromy and order is about %.2f\n",
            opt.radius, big ? " (at orders 60 to about 3000 the zippers come very close at many points)" : "",
            std::floor(100*max_radius(rep, false, cap))/100);
    return false;
}

// The roots and the two zippers.  With a `view` (EPS/PNG output) branches that cannot meet
// the visible part of the sphere are culled, and a zoomed view is FOCUSED: the non-group-
// invariant finite approximation is taken relative to the point o_F of H^3 at distance
// L = log(zoom) from o towards the centre of the view (capped at R-1 so the two balls
// overlap).  Seen from o_F the zoomed window is magnified back to ordinary size, so the
// roots added from the ball d(o_F, g.o) <= R give the zoomed picture the same density of
// detail as an unzoomed one, at about the same cost.  Roots = ball(o, R) U ball(o_F, R).
struct ZView { View v; double zoom, lim; };           // lim: visible half-width (unit disk)
struct Zippers {
    vector<Mat> roots;                                // (freed once both zippers are drawn)
    size_t nroots = 0;
    bool capHit = false;
    double focusL = 0;
    size_t noDescent = 0;                             // roots with no nearer S-neighbour (seeds aside)
    double estimate = 0;                              // the size estimate (ball_estimate)
};
static void build_zippers(const Options& opt, const Monodromy& mon, const Rep& rep,
                          const ZView* zv, Renderer& rp, Renderer& rn, Zippers& zs) {
    Mat Ti; bool focused = false;
    Pt axis{0, 0, 1};
    if (zv) {
        axis = { zv->v.m[6], zv->v.m[7], zv->v.m[8] };        // world point at the view centre
        zs.focusL = std::min(std::log(std::max(1.0, zv->zoom)), std::max(0.0, opt.radius - 1.0));
        if (opt.focus && zs.focusL > 1e-9) {
            // T: translation by L towards `axis`; U: rotation taking infinity to `axis`
            Mat U = identity();
            if (1.0 + axis.z > 1e-9) {
                cpx zv0 = cpx(axis.x, axis.y)/(1.0 + axis.z);
                double nrm = std::sqrt(1.0 + std::norm(zv0));
                U = { zv0/nrm, cpx(-1.0/nrm, 0), cpx(1.0/nrm, 0), std::conj(zv0)/nrm };
            }
            double e = std::exp(zs.focusL/2.0);
            Mat D = { cpx(e,0), cpx(0,0), cpx(0,0), cpx(1.0/e,0) };
            Ti = matinv(matmul(U, matmul(D, matinv(U))));
            focused = true;
        } else zs.focusL = 0;
    }
    tlog("(start of build)");
    // Without the descent certificate the search is still complete for R <= Kc + 2 - m, where
    // m is the margin the plain search through B(Kc + 2) needs (the extra seeds then hold
    // every element of the ball without a nearer S-neighbour); m <= 1.7 in every test.
    const BallGens& bgs = *rep.ball;
    static double warnedR = -1;
    if (bgs.uncovered > bgs.tolerated && (focused || opt.radius > bgs.Kc + 0.3) && opt.radius != warnedR) {
        warnedR = opt.radius;
        fprintf(stderr, "warning: the root search is not certified complete for this monodromy and order "
                "beyond -R %.1f (%zu of %zu direction cells uncovered at seed radius %.1f); a few roots may "
                "be missing\n", bgs.Kc + 0.3, bgs.uncovered - bgs.tolerated, bgs.cells, bgs.Kc);
    }
    zs.roots = enumerate_ball(rep, opt.radius, focused ? &Ti : nullptr, ROOT_CAP, &zs.capHit,
                              &zs.noDescent);
    zs.nroots = zs.roots.size();
    zs.estimate = ball_estimate(rep, opt.radius, focused);
    if (zs.capHit)
        fprintf(stderr, "warning: enumeration cap reached (%zu roots); picture truncated (lower -R)\n",
                ROOT_CAP);
    else if (!focused && opt.radius >= rep.ball->Kc + 1.0 && (double)zs.nroots < 0.5*zs.estimate)
        fprintf(stderr, "warning: the root ball has %zu elements, far fewer than the %.3g expected; "
                "roots may be missing\n", zs.nroots, zs.estimate);
    tlog("enumerate roots");
    for (Renderer* R : {&rp, &rn}) {
        R->accuracy = opt.accuracy;
        R->stats    = opt.stats;
        R->overlapLevels = opt.overlap;
        R->chainAccel = opt.chains;
        if (R == &rp) R->setup(mon.phi_a, mon.phi_b, rep.Ci);
        else          R->setup(mon.psi_a, mon.psi_b, rep.C);
        tlog("  setup (base caps)");
        if (zv) {
            // the cap about the view centre containing the visible square |X|,|Y| <= lim/zoom
            // of the front hemisphere, plus the accuracy (segments may cross the edge) and a
            // safety margin that scales with the view (a fixed 1e-3 made deep zooms split
            // ~80x more leaves than are visible at zoom 1e4)
            double h = std::sqrt(2.0)*zv->lim/zv->zoom;
            R->cull = true; R->vaxis = axis;
            R->vrad = (h >= 1.0 ? PI/2 : std::asin(h)) + 2.0*opt.accuracy + 1e-3/std::max(1.0, zv->zoom);
        }
        R->draw_roots(zs.roots);
        if (R->memFull)
            fprintf(stderr, "warning: %s: drawing stopped at the memory limit (%.3g GB); the picture is "
                    "incomplete (use a larger -a, or a smaller --zoom or -R)\n",
                    R == &rp ? "Z+" : "Z-", R->draw_mem()/1e9);
        if (R->nDevCapped)
            fprintf(stderr, "warning: %s: the split budget for thin spikes was reached; %zu leaves may stray "
                    "more than the accuracy from their curve\n", R == &rp ? "Z+" : "Z-", R->nDevCapped);
    }
    vector<Mat>().swap(zs.roots);
}

// --stats: per-zipper diagnostics, including the exactness checks
static void print_stats(const char* name, const Renderer& r, const Zippers& zs) {
    fprintf(stderr, "%s: roots %zu (focus shift %.3g), branches visited %zu, skipped (drawn or "
            "covered elsewhere) %zu, culled (off view) %zu, split for a spike %zu, split for an "
            "overlap %zu, leaves %zu, polylines %zu (before joining %zu), points %zu\n",
            name, zs.nroots, zs.focusL, r.nVisited, r.nSkipped, r.nCulled, r.nSpike,
            r.nOverlap, r.nLeaves, r.npoly(), r.polysBeforeJoin, r.px.size());
    fprintf(stderr, "%s:   FALSE-MERGE mismatches %zu (max deviation of the endpoint sum on key hits, "
            "1/16 of the keys, %.3g; %zu branches below float resolution not checked)\n",
            name, r.falseMerge, r.maxMismatch, r.nUnchecked);
    fprintf(stderr, "%s:   GEOMETRIC-DUPLICATE leaf segments %zu (of those longer than %.0e)\n", name, r.geomDup,
            Renderer::SUBFLOAT);
    fprintf(stderr, "%s:   depth-capped leaves (chord > accuracy at depth %d) %zu, max chord %.3g\n",
            name, r.maxdepth, r.nCapped, r.maxCappedChord);
    fprintf(stderr, "%s:   overdraw: %.3g%% of the ink is drawn twice (leaves refined exactly to "
            "accuracy/16; sub-branches shared by two leaves; estimated from a 1/16 sample of %zu "
            "pieces)\n", name, r.fineLen > 0 ? 100.0*r.extraLen/r.fineLen : 0.0, r.overdrawSamples);
    fprintf(stderr, "%s:   spikes: %zu leaves stray > 2x accuracy from their chord (max %.3gx; leaves refined "
            "to accuracy/16 with their tips); split for the region bound %zu%s%s\n", name, r.spikeLeaves,
            r.maxSpike, r.nDev, r.nDevCapped ? ", NOT split (budget reached) " : "",
            r.nDevCapped ? std::to_string(r.nDevCapped).c_str() : "");
    if (r.regFail) fprintf(stderr, "%s:   NOTE: the base arcs could not be sampled: no region bounds\n", name);
    fprintf(stderr, "%s:   parabolic chains: %zu end-chain and %zu tip-chain steps walked, %zu runs of %zu "
            "steps drawn as single chords (their curve strays at most %.3gx accuracy from the chord)\n",
            name, r.nChainSteps, r.nTipSteps, r.nGapLeaves, r.nGapSteps, r.maxGapDev);
}

// Can the file be written (its directory exists and is writable, and so is the file if it
// exists)?  Checked before anything is computed.
static bool writable(const string& fn) {
    if (fn.empty()) return true;
    if (access(fn.c_str(), F_OK) == 0) return access(fn.c_str(), W_OK) == 0;
    size_t sl = fn.find_last_of('/');
    string dir = sl == string::npos ? string(".") : sl == 0 ? string("/") : fn.substr(0, sl);
    return access(dir.c_str(), W_OK) == 0;
}

// Compute and write one EPS/PNG file for the view v (also used by the viewer's 'p').
static int render_file(const Options& opt, const Monodromy& mon, const Rep& rep, const View& v) {
    Renderer rp(rep), rn(rep);
    Zippers zs;
    // the visible half-width in unit-disk units (as in write_eps / write_png)
    double lim = (opt.format == FMT_PNG) ? 1.05*opt.size/(opt.size - 4.0)
                                         : 1.05*(opt.size/2.0)/(opt.size/2.0 - 2.0);
    ZView zv{v, opt.zoom, lim};
    bool cull = opt.cull && !opt.wheel;               // the wheel needs the whole sphere
    build_zippers(opt, mon, rep, cull ? &zv : nullptr, rp, rn, zs);
    vector<Layer> layers;
    if (opt.zippers) layers = {{&rp,{0,0,1}}, {&rn,{1,0,0}}};

    std::unique_ptr<Renderer> rw;
    if (opt.wheel) {
        rw.reset(new Renderer(rep));
        compute_wheel(rp, rn, opt.wheelres, *rw);
        layers.push_back({rw.get(), {0.0, 0.6, 0.0}});   // Catherine wheel, green
        size_t np = rw->px.size();
        bool closed = np >= 4 && rw->npoly() == 1 && rw->px[0] == rw->px[np-1] &&
                      rw->py[0] == rw->py[np-1] && rw->pz[0] == rw->pz[np-1];
        fprintf(stderr, "Catherine wheel: %zu polyline(s), %zu points, %s\n", rw->npoly(), np,
                closed ? "closed" : "NOT closed");
    }

    bool ok = true;
    tlog("(wheel)");
    g_visible = 0;
    if (opt.format == FMT_PNG) {
        string fn = opt.outfile.empty() ? string("zipper.png") : opt.outfile;
        ok = write_png(fn.c_str(), opt, layers, v);
        if (ok) fprintf(stderr, "wrote %s\n", fn.c_str());
        else fprintf(stderr, "failed to write %s\n", fn.c_str());
    } else {
        FILE* f = opt.outfile.empty() ? stdout : fopen(opt.outfile.c_str(), "w");
        if (!f) { fprintf(stderr, "cannot open %s\n", opt.outfile.c_str()); return 1; }
        write_eps(f, opt, layers, v);
        if (f != stdout) ok = (fclose(f) == 0);
    }

    tlog("write output");
    fprintf(stderr, "Z+: %zu leaves, %zu polylines ; Z-: %zu leaves, %zu polylines\n",
            rp.nLeaves, rp.npoly(), rn.nLeaves, rn.npoly());
    if (opt.stats) {
        const BallGens& bg = *rep.ball;
        fprintf(stderr, "ball: %zu roots (estimate %.3g); search generators |S| = %zu (a,b,c, inverses "
                "and %zu short elements), seeds B(o,%.1f) = %zu elements (+ %zu beyond it without a nearer "
                "S-neighbour); descent certificate for d >= %.1f: %s; roots without a nearer S-neighbour "
                "(outside the seeds) %zu\n",
                zs.nroots, zs.estimate, bg.S.size(), bg.S.size() - 6, bg.Kc,
                bg.seeds.size() - bg.extraSeeds, bg.extraSeeds, bg.D0,
                bg.uncovered == 0 ? "certified" : bg.uncovered == bg.tolerated
                    ? "certified except next to the cusp points" : "NOT certified", zs.noDescent);
        if (bg.uncovered)
            fprintf(stderr, "ball:   %zu of %zu direction cells not covered with a margin, %zu of them next "
                    "to a %s point (there completeness is empirical, see the README)\n", bg.uncovered,
                    bg.cells, bg.tolerated, opt.order > 0 ? "near-cusp" : "cusp");
        print_stats("Z+", rp, zs); print_stats("Z-", rn, zs);
        if (rp.falseMerge + rn.falseMerge > 0) {
            // two different branches got the same key: arcs are missing from the picture (this
            // happens only for a non-discrete representation, or a key resolution far too coarse)
            fprintf(stderr, "error: FALSE-MERGE detected (%zu + %zu branch keys shared by different "
                    "branches); the picture is not reliable\n", rp.falseMerge, rn.falseMerge);
            return 2;
        }
    }
    if (opt.zippers && g_visible == 0)
        fprintf(stderr, "warning: nothing was drawn (no curve meets the view)\n");
    if (!ok) return 1;
    return (rp.memFull || rn.memFull) ? 3 : 0;            // 3: written, but incomplete
}

// ------------------------------------------------------------------ interactive viewer
// (not built here: the window, and its export_name, namespace vw and run_viewer, are in
// legacy/s2/zipper.cc; -v says so, below, as the legacy nogui build does)

static void usage(const char* prog) {
    fprintf(stderr,
"usage: %s [options]\n"
"  -m NAME    monodromy: 'fig8', or an R/L word e.g. RRL, RLLRL (default fig8); a proper\n"
"             power such as RLRL = (RL)^2 draws its root RL (the same zippers)\n"
"  -n N       orbifold order: a decimal integer 2..10^6, or 'inf'/0 for the cusp (default 2)\n"
"  -a ACC     accuracy: max chord on the sphere, and max distance of a chord from its\n"
"             curve, > 0 (default 0.004)\n"
"  -R RADIUS  hyperbolic ball radius in the 3-manifold (default 6; higher = denser); the\n"
"             ball may have at most 16M elements (about -R 8 for fig8; a larger -R is refused\n"
"             with the limit for the monodromy and order)\n"
"             (with --zoom the ball is also taken about a point pushed towards the view)\n"
"  -e TOL     obsolete, ignored (branch identity is exact)\n"
"  -s SIZE    output size: points (EPS, 16..100000) or pixels (PNG, 16..16384; ~7 bytes\n"
"             of memory per pixel) (default 512)\n"
"  -w WIDTH   line width in points, 0 = thinnest (default 0)\n"
"  --yaw D    view rotation about the vertical axis, degrees (default 0)\n"
"  --pitch D  view rotation about the horizontal axis, degrees (default 0)\n"
"  --zoom F   magnify the EPS/PNG view about its centre by F, adding detail, 0 < F <= 1e7\n"
"             (default 1; points are single precision, so above ~1e5 the curves show steps)\n"
"  -f FMT     output format: eps or png (default: inferred from -o, else eps)\n"
"  -C         also draw the Catherine wheel (green circle separating Z+ from Z-)\n"
"  --wheelres N   finest edge pi/N of the wheel's adaptive mesh, 8..10^7 (default 10^7)\n"
"  --overlap L    levels of sub-branches below a leaf marked as drawn, 0..16 (default 1)\n"
"  --stats    print per-zipper counts and the exactness checks (costs ~2-3x the memory and\n"
"             ~2-4x the time of the same run; exits with status 2 on a FALSE-MERGE)\n"
"  (exit status 3: the picture was written but is incomplete, at the memory limit)\n"
"  --timing   print the time of each phase\n"
"  --nofocus  with --zoom, use only the ball about the basepoint\n"
"  --nocull   with --zoom, draw the whole sphere (no view culling)\n"
"  -v         open an interactive window (arrows rotate, +/- depth, z/x zoom,\n"
"             c cycle zippers/wheel, p save PNG of the current view as STEM.png, STEM_1.png,\n"
"             ... (STEM from -o, default zipper; never overwrites), q quit)\n"
"  -o FILE    output file (default stdout for eps)\n"
"  -h         this help\n", prog);
}

static bool ends_with(const string& s, const string& suf) {
    return s.size() >= suf.size() && s.compare(s.size()-suf.size(), suf.size(), suf) == 0;
}

// strict numeric parsing (atof/atoi silently turn garbage into 0)
static bool parse_num(const string& v, double& out) {
    char* end = nullptr;
    out = std::strtod(v.c_str(), &end);
    return !v.empty() && end && *end == '\0' && std::isfinite(out);
}

int main(int argc, char** argv) {
    Options opt;
    bool fmt_set = false;
    for (int i = 1; i < argc; ++i) {
        string s = argv[i];
        auto need = [&](const char* w)->string {
            if (i+1 >= argc) { fprintf(stderr, "missing argument for %s\n", w); exit(1); }
            return argv[++i];
        };
        // a number satisfying `ok`, else exit with the message
        auto num = [&](const char* w, bool (*ok)(double), const char* msg)->double {
            string v = need(w); double x;
            if (!parse_num(v, x) || !ok(x)) { fprintf(stderr, "%s %s: %s\n", w, v.c_str(), msg); exit(1); }
            return x;
        };
        auto pos    = [](double x){ return x > 0; };
        auto nonneg = [](double x){ return x >= 0; };
        auto any    = [](double){ return true; };
        if      (s=="-m") opt.monodromy = need("-m");
        else if (s=="-n") { string v = need("-n"); double x;
                            bool dec = !v.empty() && v.size() <= 9 &&       // decimal digits only
                                       v.find_first_not_of("0123456789") == string::npos;
                            if (v=="inf" || v=="cusp") opt.order = 0;
                            else if (dec && parse_num(v, x) && (x == 0 || (x >= 2 && x <= 1e6)))
                                opt.order = (int)x;
                            else { fprintf(stderr, "-n %s: orbifold order must be a decimal integer "
                                           "2..1000000, or inf (or 0) for the cusp\n", v.c_str());
                                   return 1; } }
        else if (s=="-a") opt.accuracy = num("-a", pos, "accuracy must be a number > 0");
        else if (s=="-R"||s=="-d") opt.radius = num("-R", [](double x){ return x >= 0 && x <= 20; },
                                                    "radius must be a number between 0 and 20");
        else if (s=="-e") { need("-e"); }                              // obsolete, ignored
        else if (s=="-s") opt.size = (int)num("-s", [](double x){ return x >= 16 && x <= 100000 && x == std::floor(x); },
                                              "size must be an integer between 16 and 100000");
        else if (s=="-w") opt.linewidth = num("-w", nonneg, "line width must be >= 0");
        else if (s=="--yaw")   opt.yaw   = num("--yaw", any, "not a number");
        else if (s=="--pitch") opt.pitch = num("--pitch", any, "not a number");
        else if (s=="--roll")  opt.roll  = num("--roll", any, "not a number");   // (hidden: see vw::print_command)
        else if (s=="--zoom")  opt.zoom  = num("--zoom", [](double x){ return x > 0 && x <= MAX_ZOOM; },
                                               "zoom must be a number > 0 and at most 1e7 (the curves' "
                                               "points are stored in single precision, ~6e-8 on the sphere)");
        else if (s=="--stats") opt.stats = true;
        else if (s=="--timing") g_timing = true;
        else if (s=="--memlimit") g_drawMem = 1e9*num("--memlimit", [](double x){ return x >= 0.2 && x <= 64; },
                                                      "must be a number of GB, 0.2..64");
        else if (s=="--overlap"||s=="--nest")
                               opt.overlap = (int)num("--overlap", [](double x){ return x >= 0 && x <= 16 && x == std::floor(x); },
                                                      "must be an integer 0..16");
        else if (s=="--nofocus") opt.focus = false;
        else if (s=="--nocull")  opt.cull = false;
        else if (s=="--nochains") opt.chains = false;
        else if (s=="-f") { string v = need("-f"); fmt_set = true;
                            if (v=="png"||v=="PNG") opt.format = FMT_PNG;
                            else if (v=="eps"||v=="EPS") opt.format = FMT_EPS;
                            else { fprintf(stderr, "-f: format must be eps or png\n"); return 1; } }
        else if (s=="-v"||s=="--view") opt.view = true;
        else if (s=="-C"||s=="--wheel") opt.wheel = true;
        else if (s=="--wheelres") opt.wheelres = (int)num("--wheelres",
                                      [](double x){ return x >= 8 && x <= 10000000 && x == std::floor(x); },
                                      "must be an integer 8..10000000");
        else if (s=="-o") opt.outfile = need("-o");
        else if (s=="-h"||s=="--help") { usage(argv[0]); return 0; }
        else { fprintf(stderr, "unknown option %s\n", s.c_str()); usage(argv[0]); return 1; }
    }
    if (!fmt_set && ends_with(opt.outfile, ".png")) opt.format = FMT_PNG;
    if (opt.format == FMT_PNG && opt.size > PNG_MAX_SIZE) {
        fprintf(stderr, "-s %d: a PNG may be at most %d pixels wide (it needs about 7 bytes of "
                "memory per pixel); use EPS output for larger sizes\n", opt.size, PNG_MAX_SIZE);
        return 1;
    }
    if (opt.accuracy < 1e-6)
        fprintf(stderr, "warning: -a %g is very fine; expect long run times and large memory use\n",
                opt.accuracy);
    if (opt.zoom > 1e5)
        fprintf(stderr, "note: at --zoom %g the curves' single-precision points (~6e-8 apart on the "
                "sphere) show as steps\n", opt.zoom);

    Monodromy mon; Rep rep;
    tlog("(start)");
    if (!opt.view && !writable(opt.outfile)) { fprintf(stderr, "cannot write %s\n", opt.outfile.c_str()); return 1; }
    if (!prepare(opt, mon, rep)) return 1;
    if (!check_radius(opt, rep, will_focus(opt))) return 1;
    if (opt.wheel && !check_wheel(opt, rep)) return 1;

    if (opt.view) {
        fprintf(stderr, "interactive viewer not built; rebuild with 'make' (the default build has the viewer)\n");
        return 1;
    }
    return render_file(opt, mon, rep, make_view(opt.yaw, opt.pitch, opt.roll));
}
