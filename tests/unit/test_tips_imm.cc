// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_tips_imm: the tips of the S^4 zippers (render/tips.h) and how the renderer draws through them. For each
// of the three substitution files of legacy/s4, read by imm::RenderPolicy (the true reader), and each zipper:
//   - every tip of every letter (find_tips, closed paths of length <= 4), checked again here in exact integers: its
//     path is a closed path of Side::roof from the letter back to it, its map is the product of the offsets along the
//     path, P is a primitive integer null vector with G P = 2 P, g is not 1, and P is a nonzero multiple of
//     (G_k - 2)^2 e_6 (k = 1, or 2 for the tips certified through g^2), so P lies in the image of g^k - 1 and g is not
//     elliptic (the argument at the top of render/tips.h); P in doubles is P/|P|, a null vector; the tip is not an end
//     of its arc (nor a vertex of the first split), in doubles; and the chain g^4096 o runs into it;
//   - the tips of a letter are in arc order (their addresses, the paths repeated forever, strictly increasing) and
//     are distinct points;
//   - the number of tips, of letters with tips, and of tips new at each length, pinned to the values found by two
//     independent enumerations, so that a lost tip fails;
//   - the points a leaf is drawn through (Side::tipline, as the policy's hook filled it) are those of the rule of
//     render/tips.h, computed again here: the tips, and between two consecutive tips the vertices of the split where
//     their addresses part, between them;
//   - a single leaf (accuracy 10, at the identity, at roots and at products of letters, for every letter) is drawn,
//     by the renderer and by arc_points, as a, then h q for each point q of its tip line, then b, bit for bit;
//   - the renderer with tips makes the same leaves as without them (the same Side with its tip lines removed): R = 0
//     and 1.2, accuracies 0.05 and 0.02, both zippers, with a leaf cap: the same counters, branches, polylines and
//     records, and each polyline with the points at tips taken out is the one without tips, bit for bit, the points
//     taken out being, leaf by leaf, as many as the leaf's letter has in its tip line, nTipPoints in all; and the
//     same for arc_points, on the base arcs and at roots;
// and the S^3 policy gives no tips (Pi is cocompact), on its three files. It prints the counts of the tips per path
// length. Light: about a second. Uses src/ only. ZIPPERS_REPO is the repository.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/tips.h"
#include "render/world.h"

#include "render_test_support.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::imm::O51 B;
typedef B::Elem E;
typedef __int128 I128;
using rtest::check;
using rtest::note;
using rtest::section;

static const char* const FILES4[3] = {"subst5_2.30_4.50_t1.txt", "subst5_2.30_4.50_t2.txt", "subst5_2.30_4.50_t3.txt"};
static const char* const FILES3[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
static std::string repo_file(const char* sub, const char* f) { return std::string(ZIPPERS_REPO) + "/legacy/" + sub + "/" + f; }

// ---- exact arithmetic, written here again ---------------------------------------------------------------------------

// M v in 128 bits; false on overflow
static bool apply(const E& M, int shift, const I128* v, I128* out)
{
    for (int i = 0; i < 6; i++) {
        I128 s = 0;
        for (int j = 0; j < 6; j++) {
            I128 t;
            if (__builtin_mul_overflow((I128)M.e[i][j] - (i == j ? shift : 0), v[j], &t) || __builtin_add_overflow(s, t, &s))
                return false;
        }
        out[i] = s;
    }
    return true;
}

// the exact checks of one tip: G P = 2 P, <P, P> = 0, P primitive with P_6 > 0, g != 1, and P a nonzero multiple of
// (G_k - 2)^2 e_6
static bool exact_tip(const z::Tip<5>& t, std::string& why)
{
    I128 P[6];
    for (int i = 0; i < 6; i++) P[i] = t.P[i];
    I128 GP[6];
    if (!apply(t.g, 0, P, GP)) { why = "overflow in G P"; return false; }
    for (int i = 0; i < 6; i++) if (GP[i] != 2*P[i]) { why = "G P != 2 P"; return false; }
    I128 q = 0;
    for (int i = 0; i < 6; i++) q += (i < 5 ? 1 : -1)*P[i]*P[i];
    if (q != 0) { why = "<P, P> != 0"; return false; }
    if (P[5] <= 0) { why = "P_6 <= 0"; return false; }
    I128 g = 0;
    for (int i = 0; i < 6; i++) { I128 a = P[i] < 0 ? -P[i] : P[i], b = g; while (b) { I128 r = a % b; a = b; b = r; } g = a; }
    if (g != 1) { why = "P not primitive"; return false; }
    if (B::eq(t.g, B::identity())) { why = "g = 1"; return false; }
    E Gk = t.via == 2 ? B::mul(t.g, t.g) : t.g;
    if (t.via != 1 && t.via != 2) { why = "via"; return false; }
    I128 e6[6] = {0, 0, 0, 0, 0, 1}, c[6], W[6];
    if (!apply(Gk, 2, e6, c) || !apply(Gk, 2, c, W)) { why = "overflow in (G_k - 2)^2 e_6"; return false; }
    bool nonzero = false;
    for (int i = 0; i < 6; i++) nonzero = nonzero || W[i] != 0;
    if (!nonzero) { why = "(G_k - 2)^2 e_6 = 0"; return false; }
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 6; j++)
            if (W[i]*P[j] != W[j]*P[i]) { why = "P not a multiple of (G_k - 2)^2 e_6"; return false; }
    return true;
}

// the order of two periodic addresses (u u u ... against w w w ...), over the first 64 places (more than |u| + |w|)
static int compare_addresses(const std::vector<int>& u, const std::vector<int>& w)
{
    for (size_t k = 0; k < 64; k++) {
        int a = u[k % u.size()], b = w[k % w.size()];
        if (a != b) return a < b ? -1 : 1;
    }
    return 0;
}

// the rule of render/tips.h, written here again: the tips, and between two consecutive tips the vertices of the split
// at the level where their addresses part, between them
static std::vector<B::DVec> line_again(const z::Side<B>& sd, int x, const std::vector<z::Tip<5>>& tips)
{
    std::vector<B::DVec> out;
    for (size_t t = 0; t < tips.size(); t++) {
        if (t > 0) {
            const std::vector<int>& u = tips[t - 1].path;
            const std::vector<int>& w = tips[t].path;
            size_t r = 0;
            while (r < 64 && u[r % u.size()] == w[r % w.size()]) r++;
            E G = B::identity();
            int y = x;
            for (size_t k = 0; k < r; k++) {
                int c = u[k % u.size()];
                G = k == 0 ? sd.off[y][c] : B::mul(G, sd.off[y][c]);
                y = sd.roof[y][c];
            }
            for (int c = u[r % u.size()] + 1; c <= w[r % w.size()]; c++)
                out.push_back(r == 0 ? sd.wv[x][c] : B::act(B::to_double(B::mul(G, sd.off[y][c])), sd.vfix));
        }
        out.push_back(tips[t].v);
    }
    return out;
}

// the leaf (h, x) as a single polyline: a, h q for the points q of its tip line, b
static std::vector<z::SpherePoint<5>> one_leaf(const z::Side<B>& sd, const E& h, int x)
{
    B::DMat hd = B::to_double(h);
    std::vector<z::SpherePoint<5>> p = {B::boundary(B::act(hd, sd.vfix))};
    if (!sd.tipline.empty())
        for (const B::DVec& v : sd.tipline[x]) p.push_back(B::boundary(B::act(hd, v)));
    p.push_back(B::boundary(B::act(hd, sd.ev[x])));
    return p;
}

// the polyline N is O with points inserted after its first point: the lengths of the inserted runs, one per segment of
// O (in order); false if it is not
template <class P>
static bool insertions(const std::vector<P>& N, const std::vector<P>& O, std::vector<size_t>& runs)
{
    runs.clear();
    if (O.empty()) return N.empty();
    if (N.empty() || memcmp(&N[0], &O[0], sizeof(P))) return false;
    size_t i = 0;
    for (size_t s = 1; s < O.size(); s++) {
        size_t j = i + 1;
        while (j < N.size() && memcmp(&N[j], &O[s], sizeof(P))) j++;
        if (j == N.size()) return false;
        runs.push_back(j - i - 1);
        i = j;
    }
    return i + 1 == N.size();
}

// the points of polyline k of a renderer, as one record per point
struct F5 { float x[5]; };
static std::vector<F5> poly(const z::Renderer<B>& r, size_t k)
{
    std::vector<F5> v;
    for (size_t g = r.pbegin(k); g < r.pend(k); g++) { F5 f; memcpy(f.x, &r.pts[5*g], sizeof f.x); v.push_back(f); }
    return v;
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::imm::setup();
    printf("test_tips_imm: the tips of the S^4 zippers (render/tips.h) and the drawing through them\n");

    struct File { z::World<B> W; std::string err; };
    std::vector<File> F(3);
    for (int f = 0; f < 3; f++) {
        std::string fn = repo_file("s4", FILES4[f]);
        F[f].err = rtest::capture_stderr([&] { z::prepare<z::imm::RenderPolicy>(fn.c_str(), F[f].W); });
    }

    // ---- the tips, exactly ----
    section("the tips of every letter: exact checks, arc order, distinct points");
    for (int f = 0; f < 3; f++)
        for (int s = 0; s < 2; s++) {
            const z::Side<B>& sd = s ? F[f].W.minus : F[f].W.plus;
            z::Tips<5> T = z::find_tips(sd, 4);
            std::string what = std::string(FILES4[f]) + " " + sd.name;
            const char* w = what.c_str();
            check(T.skipped == 0 && T.overflow == 0 && T.atvertex == 0, "%s: no path skipped (%ld), undecided (%ld) or"
                  " dropped at a vertex (%ld)", w, T.skipped, T.overflow, T.atvertex);
            long nfresh = 0;
            for (int L = 1; L <= 4; L++) nfresh += T.fresh[L];
            check(nfresh == (long)T.count(), "%s: the tips new at each length add up (%ld, %zu)", w, nfresh, T.count());
            // the tips pinned: their number, their letters, and the tips new at each length 1-4 (found by two
            // independent enumerations). The checks below test each tip found, so without this a lost tip would pass
            // (for instance, with a trace filter too strict)
            static const long PIN[3][2][6] = {{{24, 22, 22, 2, 0, 0}, {35, 33, 26, 6, 3, 0}},
                                              {{20, 19, 18, 2, 0, 0}, {5, 5, 5, 0, 0, 0}},
                                              {{13, 13, 12, 1, 0, 0}, {2, 2, 2, 0, 0, 0}}};
            const long* pin = PIN[f][s];
            check((long)T.count() == pin[0] && (long)T.letters() == pin[1] && T.fresh[1] == pin[2] &&
                  T.fresh[2] == pin[3] && T.fresh[3] == pin[4] && T.fresh[4] == pin[5],
                  "%s: %zu tips on %zu letters, new at lengths 1-4: %ld %ld %ld %ld (pinned: %ld on %ld, %ld %ld %ld %ld)",
                  w, T.count(), T.letters(), T.fresh[1], T.fresh[2], T.fresh[3], T.fresh[4], pin[0], pin[1], pin[2],
                  pin[3], pin[4], pin[5]);
            size_t nchecked = 0;
            for (size_t x = 0; x < T.of.size(); x++) {
                const std::vector<z::Tip<5>>& tips = T.of[x];
                for (size_t t = 0; t < tips.size(); t++) {
                    const z::Tip<5>& tp = tips[t];
                    // the path: closed in Side::roof, and its map
                    bool closed = !tp.path.empty() && tp.path.size() == tp.letters.size() && tp.path.size() <= 4;
                    int y = (int)x;
                    E g = B::identity();
                    for (size_t k = 0; closed && k < tp.path.size(); k++) {
                        int c = tp.path[k];
                        closed = c >= 0 && c < (int)sd.roof[y].size() && sd.roof[y][c] == tp.letters[k];
                        if (!closed) break;
                        g = k == 0 ? sd.off[y][c] : B::mul(g, sd.off[y][c]);
                        y = sd.roof[y][c];
                    }
                    closed = closed && y == (int)x;
                    check(closed, "%s, letter %zu, tip %zu: a closed path of the roofs", w, x, t);
                    check(closed && B::eq(g, tp.g), "%s, letter %zu, tip %zu: its map is the product of the offsets", w, x, t);
                    std::string why;
                    check(exact_tip(tp, why), "%s, letter %zu, tip %zu: exact: %s", w, x, t, why.c_str());
                    // in doubles: P/|P|, null; not an end, nor a vertex of the first split
                    double nn = 0;
                    for (int i = 0; i < 6; i++) nn += (double)tp.P[i]*(double)tp.P[i];
                    nn = std::sqrt(nn);
                    bool same = true;
                    for (int i = 0; i < 6; i++) same = same && tp.v[i] == (double)tp.P[i]/nn;
                    double q = 0;
                    for (int i = 0; i < 6; i++) q += (i < 5 ? 1 : -1)*tp.v[i]*tp.v[i];
                    check(same && std::fabs(q) < 1e-14, "%s, letter %zu, tip %zu: v = P/|P|, null (%.2g)", w, x, t, q);
                    z::SpherePoint<5> p = B::boundary(tp.v);
                    double dmin = std::min(z::chord(p, sd.V), z::chord(p, B::boundary(sd.ev[x])));
                    for (const B::DVec& wv : sd.wv[x]) dmin = std::min(dmin, z::chord(p, B::boundary(wv)));
                    check(dmin > 1e-6, "%s, letter %zu, tip %zu: not an end or a vertex of the first split (%.3g)", w, x,
                          t, dmin);
                    // the chain g^n o runs into it (a parabolic chain approaches like 1/n): n = 4096 steps, in doubles
                    // (one step at a time: squaring the matrix in doubles is unstable here, since g - 1 is nilpotent)
                    B::DMat m = B::to_double(g);
                    B::DVec o = {0, 0, 0, 0, 0, 1};
                    for (int k = 0; k < 4096; k++) {
                        o = B::act(m, o);
                        double on = 0;
                        for (double c : o) on += c*c;
                        for (double& c : o) c /= std::sqrt(on);
                    }
                    double dc = z::chord(B::boundary(o), p);
                    check(dc < 2e-2, "%s, letter %zu, tip %zu: g^4096 o is %.3g from it", w, x, t, dc);
                    nchecked++;
                    if (t > 0) {
                        check(compare_addresses(tips[t - 1].path, tp.path) < 0, "%s, letter %zu: tips %zu and %zu in arc"
                              " order", w, x, t - 1, t);
                        check(tips[t - 1].P != tp.P, "%s, letter %zu: tips %zu and %zu are distinct", w, x, t - 1, t);
                    }
                    for (size_t u = 0; u + 1 < t; u++) check(tips[u].P != tp.P, "%s, letter %zu: tips %zu and %zu are"
                                                              " distinct", w, x, u, t);
                }
            }
            check(nchecked == T.count(), "%s: every tip checked", w);
            char buf[400];
            int k = snprintf(buf, sizeof buf, "%s: %zu tips on %zu letters; per path length (closed paths / parabolic maps"
                             " (through g^2) / new tips):", w, T.count(), T.letters());
            for (int L = 1; L <= 4; L++)
                k += snprintf(buf + k, sizeof buf - (size_t)k, "  %d: %ld / %ld (%ld) / %ld", L, T.closed[L],
                              T.parabolic[L], T.via2[L], T.fresh[L]);
            note("%s", buf);
        }
    {
        // an example: the letters with more than one tip in Z- of the first file
        const z::Side<B>& sd = F[0].W.minus;
        z::Tips<5> T = z::find_tips(sd, 4);
        for (size_t x = 0; x < T.of.size(); x++) {
            if (T.of[x].size() < 2) continue;
            std::string s;
            for (const z::Tip<5>& t : T.of[x]) {
                s += " [path";
                for (int c : t.path) s += " " + std::to_string(c + 1);
                s += ": P = (";
                for (int i = 0; i < 6; i++) s += std::to_string(t.P[i]) + (i < 5 ? "," : ")");
                s += ", " + std::to_string(t.paths) + " paths]";
            }
            note("(%s Z-, letter %zu, roof of %zu, tip line of %zu points:%s)", FILES4[0], x, sd.roof[x].size(),
                 sd.tipline[x].size(), s.c_str());
        }
    }

    // ---- the tip lines ----
    section("the tip lines: the policy's (Side::tipline) are the rule's, computed again");
    for (int f = 0; f < 3; f++)
        for (int s = 0; s < 2; s++) {
            const z::Side<B>& sd = s ? F[f].W.minus : F[f].W.plus;
            z::Tips<5> T = z::find_tips(sd, 4);
            check(sd.tipline.size() == sd.roof.size(), "%s %s: a tip line for each letter", FILES4[f], sd.name);
            size_t npts = 0, nlong = 0;
            for (size_t x = 0; x < sd.roof.size() && x < sd.tipline.size(); x++) {
                std::vector<B::DVec> want = line_again(sd, (int)x, T.of[x]);
                check(rtest::same_vector_bytes(want, sd.tipline[x]), "%s %s, letter %zu: the tip line (%zu points, the"
                      " rule's %zu)", FILES4[f], sd.name, x, sd.tipline[x].size(), want.size());
                npts += sd.tipline[x].size();
                nlong += sd.tipline[x].size() > T.of[x].size();
            }
            note("(%s %s: %zu points in the tip lines of %zu letters; %zu of them also pass vertices between tips)",
                 FILES4[f], sd.name, npts, T.letters(), nlong);
        }

    // ---- single leaves ----
    section("a single leaf: a, its tip line, b (the renderer and arc_points, accuracy 10)");
    {
        std::vector<E> roots;
        std::vector<double> rootd;
        z::imm::RenderPolicy::roots(1.2, roots, rootd);
        long nleaves = 0, ntips = 0;
        for (int f = 0; f < 3; f++)
            for (int s = 0; s < 2; s++) {
                const z::World<B>& W = F[f].W;
                const z::Side<B>& sd = s ? W.minus : W.plus;
                std::vector<E> hs = {B::identity()};
                for (size_t r = 1; r < roots.size(); r += 2) hs.push_back(roots[r]);
                for (int k = 0; k < 3; k++) {
                    E g = B::identity();
                    for (int m = 0; m < 3; m++) g = B::mul(g, W.L.s[(size_t)rtest::rnd(0, (int64_t)W.L.s.size() - 1)]);
                    hs.push_back(g);
                }
                for (const E& h : hs)
                    for (size_t x = 0; x < W.L.s.size(); x++) {
                        std::vector<z::SpherePoint<5>> want = one_leaf(sd, h, (int)x), got;
                        size_t forced = 0;
                        z::arc_points(sd, h, (int)x, 10.0, got, forced);
                        check(rtest::same_vector_bytes(want, got), "%s %s, letter %zu: arc_points of a single leaf (%zu"
                              " points, %zu wanted)", FILES4[f], sd.name, x, got.size(), want.size());
                        z::Renderer<B> r(sd, W.L, 10.0, 1000);
                        r.draw(h, (int)x, 0);
                        std::vector<float> wf;
                        for (const auto& p : want) for (int i = 0; i < 5; i++) wf.push_back((float)p.x[i]);
                        check(r.nLeaves == 1 && r.npoly() == 1 && r.pts == wf &&
                              r.nTipPoints == (sd.tipline.empty() ? 0 : sd.tipline[x].size()),
                              "%s %s, letter %zu: the renderer's single leaf", FILES4[f], sd.name, x);
                        nleaves++;
                        ntips += sd.tipline.empty() ? 0 : (long)sd.tipline[x].size();
                    }
            }
        note("(%ld single leaves, %ld points at tips among them)", nleaves, ntips);
    }

    // ---- the same leaves ----
    section("with tips, the same leaves as without: the renderer, and arc_points");
    {
        long ncases = 0, nleaves = 0, ntip = 0, npts_old = 0, npts_new = 0;
        for (int f = 0; f < 3; f++)
            for (double R : {0.0, 1.2})
                for (double acc : {0.05, 0.02}) {
                    std::vector<E> roots;
                    std::vector<double> rootd;
                    z::imm::RenderPolicy::roots(R, roots, rootd);
                    for (int s = 0; s < 2; s++) {
                        const z::World<B>& W = F[f].W;
                        const z::Side<B>& sd = s ? W.minus : W.plus;
                        z::Side<B> bare = sd;
                        bare.tipline.clear();
                        const size_t CAP = 25000;
                        z::Renderer<B> a(sd, W.L, acc, CAP), b(bare, W.L, acc, CAP);
                        for (size_t r = 0; r < roots.size(); r++)
                            for (size_t j = 0; j < W.L.s.size(); j++)
                                if (W.L.plus[j]) { a.draw(roots[r], (int)j, (int)r); b.draw(roots[r], (int)j, (int)r); }
                        char what[200];
                        snprintf(what, sizeof what, "%s %s R=%g acc=%g", FILES4[f], sd.name, R, acc);
                        check(a.nVisited == b.nVisited && a.nSkipped == b.nSkipped && a.nLeaves == b.nLeaves &&
                              a.nSpike == b.nSpike && a.nForced == b.nForced && a.deepest == b.deepest &&
                              a.capped == b.capped && a.open == b.open, "%s: the counters", what);
                        check(a.done.n == b.done.n && a.done.t == b.done.t, "%s: the branches drawn", what);
                        check(a.starts.size() == b.starts.size() && a.polyroot == b.polyroot &&
                              a.polyletter == b.polyletter, "%s: the polylines and their records", what);
                        check(b.nTipPoints == 0 && a.pts.size() == b.pts.size() + 5*a.nTipPoints,
                              "%s: the points: %zu, %zu without tips, %zu at tips", what, a.pts.size()/5, b.pts.size()/5,
                              a.nTipPoints);
                        // each polyline: the one without tips, with runs inserted of the leaves' tip-line lengths
                        std::vector<char> runlen(64, 0);
                        for (const auto& l : sd.tipline) if (l.size() < runlen.size()) runlen[l.size()] = 1;
                        runlen[0] = 1;
                        bool ok = a.starts.size() == b.starts.size();
                        size_t inserted = 0;
                        for (size_t k = 0; ok && k < a.npoly(); k++) {
                            std::vector<size_t> runs;
                            ok = insertions(poly(a, k), poly(b, k), runs);
                            for (size_t r : runs) { ok = ok && r < runlen.size() && runlen[r]; inserted += r; }
                        }
                        check(ok && inserted == a.nTipPoints, "%s: each polyline is the one without tips, with the points"
                              " at tips inserted (%zu)", what, inserted);
                        ncases++;
                        nleaves += (long)a.nLeaves;
                        ntip += (long)a.nTipPoints;
                        npts_old += (long)b.pts.size()/5;
                        npts_new += (long)a.pts.size()/5;
                        // arc_points: the base arcs at the first roots, complete
                        for (size_t r = 0; r < roots.size() && r < 3; r++)
                            for (size_t j = 0; j < W.L.s.size(); j += 3) {
                                if (acc < 0.05 && (r > 0 || j % 9)) continue;            // keep it light
                                std::vector<z::SpherePoint<5>> pa, pb;
                                size_t fa = 0, fb = 0;
                                z::arc_points(sd, roots[r], (int)j, acc, pa, fa);
                                z::arc_points(bare, roots[r], (int)j, acc, pb, fb);
                                std::vector<size_t> runs;
                                bool same = fa == fb && insertions(pa, pb, runs);
                                for (size_t q : runs) same = same && q < runlen.size() && runlen[q];
                                check(same, "%s: arc_points of letter %zu at root %zu: %zu points, %zu without tips", what,
                                      j, r, pa.size(), pb.size());
                            }
                    }
                }
        note("(%ld cases: %ld leaves; %ld points without tips, %ld with, %ld of them at tips)", ncases, nleaves, npts_old,
             npts_new, ntip);
    }

    // ---- no tips in S^3 ----
    section("the S^3 policy gives no tips (Pi is cocompact)");
    z::cs::setup();
    for (int f = 0; f < 3; f++) {
        z::World<z::PU21> W;
        std::string fn = repo_file("s3", FILES3[f]);
        rtest::capture_stderr([&] { z::prepare<z::cs::RenderPolicy>(fn.c_str(), W); });
        check(W.plus.tipline.empty() && W.minus.tipline.empty(), "%s: no tip lines", FILES3[f]);
    }

    rtest::end_section();
    printf("test_tips_imm: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
