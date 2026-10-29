// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_view_s3: the S^3 view and painter (src/view/s3.h), the separation of the zippers (src/analysis/separation.h)
// and the linked pairs (src/analysis/pairs.h) agree BIT FOR BIT with their legacy originals in legacy/s3/zipcore.h and
// legacy/s3/zipper3.cc (its batch build, without ZIPPER3_VIEWER), which are included here, with main renamed, in the
// global namespace:
//   - PI, default_frame, rot3, move_pole, eye, init_view, frame_with_pole, stereo, on random inputs and special ones
//     (axes, points near the axes and near the pole, vectors that are not unit, angles of 0, 90, 180 degrees...),
//     and on the fixed points t^+- of the stored files;
//   - write_png: the files and return values, on the renderers of the stored files and on random polylines, in random
//     views and those of zipper3, with random sizes, brushes, layers, priorities and marks; and a file that cannot be
//     written;
//   - zdist: the distance and the two points, on the zippers of the stored files and on random polylines, for the cell
//     sizes of zipper3 --check (3 acc, doubling) and others; and that loop of --check itself;
//   - load_pairs (the stored linked_pairs.txt, variants of it, a missing and an empty file: the return value, the
//     message, every field), build_pair (the 13 stored pairs at three accuracies, rebuilt in place, and relations
//     s s^-1 and s s' s'^-1 s^-1 of every stored file with random words h), pair_view and pair_desc (random pairs,
//     long words that the description truncates).
// Built with the flags of the programs (-O2), under which the results must agree. ZIPPERS_REPO is the repository.

#define main legacy_main
#include "zipper3.cc"                    // legacy/s3: csgroup.h, zipcore.h and zipper3.cc (its batch build)
#undef main

#include "analysis/pairs.h"
#include "analysis/separation.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "geometry/pu21/pu21.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s3.h"

#include "render_test_support.h"

#include <climits>
#include <map>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::PU21 NB;
// The test's own render policy: cs::RenderPolicy without the look-ahead through one-letter roofs, which its
// sides turns on (render/renderer.h) and the legacy renderer does not have; so that the comparisons below
// are with the legacy code (the look-ahead is tested in test_lookthrough_cs.cc). No tips either, as cs::RenderPolicy.
struct NoLookThroughPolicy : z::cs::RenderPolicy {
    static void sides(const z::Letters<NB>&, z::Side<NB>&, z::Side<NB>&) {}       // neither: the legacy drawing
};
typedef NoLookThroughPolicy NP;
using rtest::check;
using rtest::gauss;
using rtest::note;
using rtest::rnd;
using rtest::same_bytes;
using rtest::same_vector_bytes;
using rtest::section;
using rtest::uniform;

static_assert(sizeof(ViewState) == sizeof(z::s3::ViewState), "the two views have the same layout");
static_assert(sizeof(P4) == sizeof(z::SpherePoint<4>), "the two points have the same layout");

static const char* const FILES3[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
static std::string repo_file(const char* f) { return std::string(ZIPPERS_REPO) + "/legacy/s3/" + f; }

static z::SpherePoint<4> np4(const P4& p) { z::SpherePoint<4> q; memcpy(&q, &p, sizeof q); return q; }
static z::s3::ViewState nview(const ViewState& v) { z::s3::ViewState w; memcpy(&w, &v, sizeof w); return w; }

// a random point of R^4: on S^3 (most), near an axis, or not of unit length
static P4 random_p4(int kind)
{
    P4 p;
    double s = 0;
    for (double& x : p.x) { x = gauss(); s += x*x; }
    s = std::sqrt(s);
    for (double& x : p.x) x /= s;
    if (kind == 1) {                                  // an axis, maybe negated
        for (double& x : p.x) x = 0;
        p.x[rnd(0, 3)] = rnd(0, 1) ? 1 : -1;
    } else if (kind == 2) {                           // near an axis
        for (double& x : p.x) x *= 1e-6*uniform(0, 3);
        p.x[rnd(0, 3)] += rnd(0, 1) ? 1 : -1;
    } else if (kind == 3) {                           // not unit
        double f = std::exp(uniform(-6, 6));
        for (double& x : p.x) x *= f;
    } else if (kind == 4) {                           // two equal coordinates, one zero
        p.x[rnd(0, 3)] = 0;
        p.x[1] = p.x[0];
    }
    return p;
}

static double random_angle()
{
    switch (rnd(0, 5)) {
        case 0: { const double special[] = {0, 90, -90, 180, -180, 45, 360, 270, -720, 30, 60, 120, 15}; return special[rnd(0, 12)]; }
        case 1: return uniform(-1e-6, 1e-6);
        case 2: return uniform(-2000, 2000);
        default: return uniform(-200, 200);
    }
}

// ---- the worlds of the stored files ----------------------------------------------------------------------------------

struct Worlds {
    std::string fn;
    World L;                              // legacy
    z::World<NB> N;
};

static void prepare_both(Worlds& w, const std::string& fn)
{
    w.fn = fn;
    Options opt;
    opt.subst = fn;
    rtest::capture_stderr([&] { prepare(opt, w.L); });
    rtest::capture_stderr([&] { z::prepare<NP>(fn.c_str(), w.N); });
}

// build both worlds' renderers at R, acc (legacy build, build<NP>), quietly
static void build_both(Worlds& w, double R, double acc, int which = 3)
{
    Options opt;
    opt.subst = w.fn;
    opt.R = R;
    opt.acc = acc;
    opt.which = which;
    rtest::capture_stderr([&] { build(opt, w.L); });
    rtest::capture_stderr([&] { z::build<NP>(w.N, R, acc, opt.cap, which); });
}

// a legacy and a new renderer with the same random polylines on S^3 (random walks, steps of about `step`), some of
// one point only
static void random_polylines(Renderer& a, z::Renderer<NB>& b, int npoly, int maxlen, double step)
{
    for (int k = 0; k < npoly; k++) {
        a.starts.push_back((uint32_t)(a.pts.size()/4));
        b.starts.push_back((uint32_t)(b.pts.size()/4));
        int len = (int)rnd(1, maxlen);
        P4 p = random_p4(0);
        for (int i = 0; i < len; i++) {
            a.push(p);
            b.push(np4(p));
            double s = 0;
            for (double& x : p.x) { x += step*gauss(); s += x*x; }
            s = std::sqrt(s);
            for (double& x : p.x) x /= s;
        }
    }
}

// ---- the linked pairs: comparisons -----------------------------------------------------------------------------------

static bool same_pair_fields(const LinkedPair& a, const z::LinkedPair<NB>& b)
{
    return a.lk == b.lk && a.w == b.w && a.w2 == b.w2 && a.phi == b.phi && a.hword == b.hword &&
           same_bytes(a.pole, b.pole) && same_bytes(a.clip, b.clip);
}

static bool same_loop_renderer(const Renderer* a, const z::Renderer<NB>* b)
{
    if (!a || !b) return !a && !b;
    return same_vector_bytes(a->pts, b->pts) && a->starts == b->starts && a->acc == b->acc && a->leafcap == b->leafcap &&
           a->nLeaves == b->nLeaves && a->nVisited == b->nVisited && a->open == b->open && a->capped == b->capped &&
           b->polyroot.empty() && b->polyletter.empty();
}

static std::string word_text(const std::vector<int>& w)
{
    std::string s;
    for (int x : w) s += (s.empty() ? "" : " ") + std::to_string(x);
    return s;
}

// a random word in a1..A3 ("1" for the empty word)
static std::string random_aword(int maxlen)
{
    int n = (int)rnd(0, maxlen);
    std::string w;
    for (int i = 0; i < n; i++) w += (w.empty() ? "" : " ") + std::string(rnd(0, 1) ? "a" : "A") + std::to_string(rnd(1, 3));
    return w.empty() ? "1" : w;
}

// ----------------------------------------------------------------------------------------------------------------------

int main()
{
    double t_start = z::now();
    setup();                               // legacy: F', u', v', b', j, a1, a2, a3
    z::cs::setup();
    printf("test_view_s3: src/view/s3.h, src/analysis/separation.h and pairs.h against legacy/s3, bit for bit\n");
    rtest::TempDir tmp;

    Worlds W[3];
    for (int f = 0; f < 3; f++) prepare_both(W[f], repo_file(FILES3[f]));

    // ---- the camera ----
    section("PI, default_frame: random points, axes, near-axes, not unit, t^+-");
    {
        check(same_bytes(PI, z::s3::PI), "PI");
        long n = 0;
        for (int t = 0; t < 20000; t++) {
            P4 V = random_p4((int)rnd(0, 4));
            double Qa[4][4], Qb[4][4];
            memset(Qa, 0x5a, sizeof Qa); memset(Qb, 0x5a, sizeof Qb);
            default_frame(V, Qa);
            z::s3::default_frame(np4(V), Qb);
            check(same_bytes(Qa, Qb), "default_frame of (%.17g %.17g %.17g %.17g)", V.x[0], V.x[1], V.x[2], V.x[3]);
            n++;
        }
        for (int f = 0; f < 3; f++)
            for (const P4& V : {W[f].L.plus.V, W[f].L.minus.V}) {
                double Qa[4][4], Qb[4][4];
                default_frame(V, Qa);
                z::s3::default_frame(np4(V), Qb);
                check(same_bytes(Qa, Qb), "default_frame of t^+- of %s", FILES3[f]);
                n++;
            }
        note("(%ld frames)", n);
    }

    section("rot3: random angles, special ones (0, 90, 180, ...), tiny ones");
    for (int t = 0; t < 20000; t++) {
        double yaw = random_angle(), pitch = random_angle(), Ra[9], Rb[9];
        rot3(yaw, pitch, Ra);
        z::s3::rot3(yaw, pitch, Rb);
        check(same_bytes(Ra, Rb), "rot3(%.17g, %.17g)", yaw, pitch);
    }

    section("move_pole: frames of random points and random matrices, random and axis directions, angles");
    for (int t = 0; t < 20000; t++) {
        double Qa[4][4];
        if (t % 3 == 0) for (auto& row : Qa) for (double& x : row) x = gauss();
        else default_frame(random_p4((int)rnd(0, 4)), Qa);
        double Qb[4][4];
        memcpy(Qb, Qa, sizeof Qb);
        double a[3];
        int kind = (int)rnd(0, 3);
        if (kind == 0) { a[0] = 1; a[1] = 0; a[2] = 0; }
        else if (kind == 1) { a[0] = 0; a[1] = 1; a[2] = 0; }
        else {
            double s = 0;
            for (double& x : a) { x = gauss(); s += x*x; }
            if (kind == 2) for (double& x : a) x /= std::sqrt(s);
        }
        double th = rnd(0, 1) ? random_angle()*PI/180 : uniform(-7, 7);
        int reps = (int)rnd(1, 3);
        for (int k = 0; k < reps; k++) { move_pole(Qa, a, th); z::s3::move_pole(Qb, a, th); }
        check(same_bytes(Qa, Qb), "move_pole by %.17g (%d times)", th, reps);
    }

    section("eye: random points (on S^3, near the pole, not unit) in random views");
    {
        long shown = 0, hidden = 0;
        for (int t = 0; t < 2000; t++) {
            ViewState v;
            Options opt;
            opt.yaw = random_angle(); opt.pitch = random_angle();
            opt.pole1 = rnd(0, 2) ? 0 : random_angle(); opt.pole2 = rnd(0, 2) ? 0 : random_angle();
            opt.zoom = std::exp(uniform(-2, 2)); opt.clip = rnd(0, 3) ? 3 : uniform(0.2, 12);
            init_view(opt, W[t % 3].L, v);
            if (t % 7 == 0) for (auto& row : v.Q) for (double& x : row) x = gauss();
            z::s3::ViewState nv = nview(v);
            for (int i = 0; i < 50; i++) {
                float x[4];
                int kind = (int)rnd(0, 5);
                if (kind == 0) {                       // at or near the pole P (the fourth row of Q)
                    double eps = rnd(0, 1) ? 0 : 1e-5*gauss();
                    for (int c = 0; c < 4; c++) x[c] = (float)(v.Q[3][c] + eps*gauss());
                } else if (kind == 1) {                // near -P: the centre of the picture
                    for (int c = 0; c < 4; c++) x[c] = (float)(-v.Q[3][c] + 0.3*gauss());
                } else {
                    P4 p = random_p4(kind == 2 ? 3 : 0);
                    for (int c = 0; c < 4; c++) x[c] = (float)p.x[c];
                }
                double ea[3], eb[3];
                memset(ea, 0x77, sizeof ea); memset(eb, 0x77, sizeof eb);
                bool ra = eye(v, x, ea), rb = z::s3::eye(nv, x, eb);
                check(ra == rb && same_bytes(ea, eb), "eye of (%.9g %.9g %.9g %.9g): %d, legacy %d", x[0], x[1], x[2], x[3], rb, ra);
                (rb ? shown : hidden)++;
            }
        }
        note("(100,000 points in 2,000 views: %ld shown, %ld outside the clip ball or at the pole)", shown, hidden);
    }

    section("init_view: random options, the three stored files");
    for (int t = 0; t < 3000; t++) {
        Options opt;
        if (t % 10) {
            opt.yaw = random_angle(); opt.pitch = random_angle();
            opt.pole1 = rnd(0, 1) ? 0 : random_angle(); opt.pole2 = rnd(0, 1) ? 0 : random_angle();
            opt.zoom = std::exp(uniform(-3, 3)); opt.clip = uniform(0.1, 20);
        }                                      // else: the defaults of zipper3
        ViewState a;
        z::s3::ViewState b;
        memset(&a, 0x33, sizeof a); memset(&b, 0x33, sizeof b);
        init_view(opt, W[t % 3].L, a);
        z::s3::init_view(W[t % 3].N, b, opt.yaw, opt.pitch, opt.pole1, opt.pole2, opt.zoom, opt.clip);
        check(same_bytes(a, b), "init_view (yaw %g pitch %g pole %g %g zoom %g clip %g) of %s", opt.yaw, opt.pitch,
              opt.pole1, opt.pole2, opt.zoom, opt.clip, FILES3[t % 3]);
    }

    section("frame_with_pole: random poles, axes, near-axes, not unit, zero");
    for (int t = 0; t < 20000; t++) {
        P4 p = random_p4((int)rnd(0, 4));
        if (t == 0) for (double& x : p.x) x = 0;
        double Fa[4][4], Fb[4][4];
        memset(Fa, 0x11, sizeof Fa); memset(Fb, 0x11, sizeof Fb);
        frame_with_pole(p.x, Fa);
        z::s3::frame_with_pole(p.x, Fb);
        check(same_bytes(Fa, Fb), "frame_with_pole of (%.17g %.17g %.17g %.17g)", p.x[0], p.x[1], p.x[2], p.x[3]);
    }

    section("stereo: random point sets (with the pole itself), frames of random poles and random matrices");
    {
        long npts = 0;
        for (int t = 0; t < 3000; t++) {
            double F[4][4];
            P4 pole = random_p4((int)rnd(0, 3));
            if (t % 5 == 0) for (auto& row : F) for (double& x : row) x = gauss();
            else frame_with_pole(pole.x, F);
            std::vector<P4> X;
            std::vector<z::SpherePoint<4>> Y;
            int n = (int)rnd(0, 120);
            for (int i = 0; i < n; i++) {
                P4 p = random_p4((int)rnd(0, 4));
                if (rnd(0, 30) == 0) for (int c = 0; c < 4; c++) p.x[c] = F[3][c];      // the pole: den = 0 or nearly
                X.push_back(p);
                Y.push_back(np4(p));
            }
            std::vector<V3> a = stereo(X, F);
            std::vector<z::s3::V3> b = z::s3::stereo(Y, F);
            check(same_vector_bytes(a, b), "stereo of %d points", n);
            npts += n;
        }
        note("(%ld points)", npts);
    }

    // ---- the painter ----
    section("write_png: the stored files' zippers and random polylines; views, sizes, brushes, layers, marks");
    {
        long files = 0, bytes = 0, blank = 0;
        // the renderers: base arcs of each stored file at two accuracies, and R = 2.554 for t3
        struct Scene { int f; double R, acc; };
        std::vector<Scene> scenes = {{0, 0, 0.05}, {1, 0, 0.03}, {2, 0, 0.02}, {2, 2.554, 0.05}};
        for (const Scene& sc : scenes) {
            Worlds& w = W[sc.f];
            build_both(w, sc.R, sc.acc);
            // and random polylines, as a third and fourth layer
            Renderer ra(w.L.plus, w.L.L, 1, 1), rb(w.L.minus, w.L.L, 1, 1);
            z::Renderer<NB> na(w.N.plus, w.N.L, 1, 1), nb(w.N.minus, w.N.L, 1, 1);
            random_polylines(ra, na, 40, 30, 0.02);
            random_polylines(rb, nb, 20, 60, 0.05);
            for (int t = 0; t < 15; t++) {
                Options opt;
                if (t % 5) {
                    opt.yaw = random_angle(); opt.pitch = random_angle();
                    opt.pole1 = rnd(0, 1) ? 0 : uniform(-90, 90); opt.pole2 = rnd(0, 1) ? 0 : uniform(-90, 90);
                    opt.zoom = std::exp(uniform(-1, 1.5)); opt.clip = rnd(0, 2) ? 3 : uniform(0.5, 8);
                }
                ViewState v;
                init_view(opt, w.L, v);
                z::s3::ViewState nv;
                z::s3::init_view(w.N, nv, opt.yaw, opt.pitch, opt.pole1, opt.pole2, opt.zoom, opt.clip);
                std::vector<Layer> la;
                std::vector<z::s3::Layer<NB>> lb;
                int which = (int)rnd(1, 3);
                bool context = rnd(0, 3) == 0;
                if (context) {                      // the zippers as the faint context of a pair (priority 0)
                    la.push_back({w.L.rp, {0.70, 0.75, 0.95}, 0}); lb.push_back({w.N.rp, {0.70, 0.75, 0.95}, 0});
                    la.push_back({w.L.rn, {0.95, 0.72, 0.72}, 0}); lb.push_back({w.N.rn, {0.95, 0.72, 0.72}, 0});
                    la.push_back({&ra, {0.05, 0.15, 0.85}}); lb.push_back({&na, {0.05, 0.15, 0.85}});
                    la.push_back({&rb, {0.85, 0.08, 0.08}}); lb.push_back({&nb, {0.85, 0.08, 0.08}});
                } else {
                    if (which & 1) { la.push_back({w.L.rp, {0.05, 0.15, 0.85}}); lb.push_back({w.N.rp, {0.05, 0.15, 0.85}}); }
                    if (which & 2) { la.push_back({w.L.rn, {0.85, 0.08, 0.08}}); lb.push_back({w.N.rn, {0.85, 0.08, 0.08}}); }
                    if (rnd(0, 2) == 0) {           // random colours and priorities, some equal
                        int p = (int)rnd(-1, 2);
                        double c[3] = {uniform(0, 1), uniform(0, 1), uniform(0, 1)};
                        la.push_back({&ra, {c[0], c[1], c[2]}, p}); lb.push_back({&na, {c[0], c[1], c[2]}, p});
                    }
                }
                std::vector<P4> marks;
                if (t % 3 != 2) marks = {w.L.plus.V, w.L.minus.V};
                if (t % 4 == 1) marks.push_back(random_p4(0));
                std::vector<z::SpherePoint<4>> nmarks;
                for (const P4& m : marks) nmarks.push_back(np4(m));
                int size = t == 0 ? 900 : (int)rnd(8, 400), thick = t % 6 == 5 ? 0 : (int)rnd(0, 3);
                std::string fa = tmp.file("legacy.png"), fb = tmp.file("new.png");
                bool ra_ok, rb_ok;
                if (t % 2) { ra_ok = write_png(fa.c_str(), size, la, v, marks, thick); rb_ok = z::s3::write_png(fb.c_str(), size, lb, nv, nmarks, thick); }
                else { ra_ok = write_png(fa.c_str(), size, la, v, marks); rb_ok = z::s3::write_png(fb.c_str(), size, lb, nv, nmarks); }
                std::string A = rtest::read_file(fa), B = rtest::read_file(fb);
                check(same_bytes(v, nv), "the views of picture %ld", files);
                check(ra_ok == rb_ok && A == B && !A.empty(), "picture %ld (%s, R %g, acc %g, size %d, brush %d, %zu layers, "
                      "%zu marks): %zu bytes, legacy %zu", files, FILES3[sc.f], sc.R, sc.acc, size, thick, lb.size(),
                      nmarks.size(), B.size(), A.size());
                files++;
                bytes += (long)B.size();
                if (B.size() < 200 + (size_t)size) blank++;
                unlink(fa.c_str()); unlink(fb.c_str());
            }
        }
        {   // a file that cannot be written
            Worlds& w = W[2];
            ViewState v;
            Options opt;
            init_view(opt, w.L, v);
            std::vector<Layer> la = {{w.L.rp, {0.05, 0.15, 0.85}}};
            std::vector<z::s3::Layer<NB>> lb = {{w.N.rp, {0.05, 0.15, 0.85}}};
            std::string fn = tmp.file("no such folder/x.png");
            bool ra_ok = write_png(fn.c_str(), 64, la, v, {}), rb_ok = z::s3::write_png(fn.c_str(), 64, lb, nview(v), {});
            check(!ra_ok && !rb_ok, "a file that cannot be written: %d, legacy %d", rb_ok, ra_ok);
        }
        note("(%ld pictures, %ld bytes; %ld nearly blank)", files, bytes, blank);
    }

    // ---- the separation of the zippers ----
    section("zdist: the zippers of the stored files, random polylines; the cell sizes of --check and others");
    {
        long cases = 0, found = 0;
        struct Scene { int f; double R, acc; };
        std::vector<Scene> scenes = {{0, 0, 0.05}, {1, 0, 0.02}, {2, 0, 0.02}, {2, 0, 0.01}, {2, 2.554, 0.05}};
        for (const Scene& sc : scenes) {
            Worlds& w = W[sc.f];
            build_both(w, sc.R, sc.acc);
            // the cell sizes of --check up to the one where it stops, and one more; two small random ones. (Large cells
            // compare nearly every vertex with every segment: at h = 0.5 one case of these zippers takes seconds.)
            std::vector<double> hs;
            for (double h = 3*sc.acc; h < 0.25; h *= 2) {
                hs.push_back(h);
                P4 wa, wb;
                if (zdist(*w.L.rp, *w.L.rn, h, wa, wb) < h - sc.acc) { if (2*h < 0.2) hs.push_back(2*h); break; }
            }
            for (int k = 0; k < 2; k++) hs.push_back(uniform(0.005, 0.12));
            for (double h : hs)
                for (int dir = 0; dir < 2; dir++) {
                    const Renderer &a = dir ? *w.L.rn : *w.L.rp, &b = dir ? *w.L.rp : *w.L.rn;
                    const z::Renderer<NB> &na = dir ? *w.N.rn : *w.N.rp, &nb = dir ? *w.N.rp : *w.N.rn;
                    P4 wa, wb;
                    z::SpherePoint<4> nwa, nwb;
                    memset(&wa, 0x44, sizeof wa); memset(&wb, 0x44, sizeof wb);
                    memset(&nwa, 0x44, sizeof nwa); memset(&nwb, 0x44, sizeof nwb);
                    double da = zdist(a, b, h, wa, wb), db = z::zdist(na, nb, h, nwa, nwb);
                    check(same_bytes(da, db) && same_bytes(wa, nwa) && same_bytes(wb, nwb),
                          "zdist of %s (R %g acc %g, %s), h %g: %.17g, legacy %.17g", FILES3[sc.f], sc.R, sc.acc,
                          dir ? "Z- to Z+" : "Z+ to Z-", h, db, da);
                    cases++;
                    found += db < h;
                }
            // the loop of zipper3 --check, both ways
            P4 wa, wb;
            z::SpherePoint<4> nwa, nwb;
            double d = 1, h, nd = 1, nh;
            for (h = 3*sc.acc; h < 0.8; h *= 2) { d = zdist(*w.L.rp, *w.L.rn, h, wa, wb); if (d < h - sc.acc) break; }
            for (nh = 3*sc.acc; nh < 0.8; nh *= 2) { nd = z::zdist(*w.N.rp, *w.N.rn, nh, nwa, nwb); if (nd < nh - sc.acc) break; }
            check(same_bytes(d, nd) && same_bytes(h, nh) && (h >= 0.8 || same_bytes(wa, nwa)), "the --check loop of %s (R %g acc %g): %.17g at h %g, legacy %.17g at h %g",
                  FILES3[sc.f], sc.R, sc.acc, nd, nh, d, h);
            note("(%s, R %g, acc %g: --check finds %.5f with cells of %g)", FILES3[sc.f], sc.R, sc.acc, nd, nh);
        }
        // random polylines, near each other (and a renderer with no polyline)
        Worlds& w = W[2];
        for (int t = 0; t < 60; t++) {
            Renderer ra(w.L.plus, w.L.L, 1, 1), rb(w.L.minus, w.L.L, 1, 1);
            z::Renderer<NB> na(w.N.plus, w.N.L, 1, 1), nb(w.N.minus, w.N.L, 1, 1);
            if (t != 0) random_polylines(ra, na, (int)rnd(1, 30), 200, uniform(0.001, 0.05));
            if (t != 1) random_polylines(rb, nb, (int)rnd(1, 30), 200, uniform(0.001, 0.05));
            double h = uniform(0.005, 0.5);
            P4 wa, wb;
            z::SpherePoint<4> nwa, nwb;
            memset(&wa, 0x44, sizeof wa); memset(&wb, 0x44, sizeof wb);
            memset(&nwa, 0x44, sizeof nwa); memset(&nwb, 0x44, sizeof nwb);
            double da = zdist(ra, rb, h, wa, wb), db = z::zdist(na, nb, h, nwa, nwb);
            check(same_bytes(da, db) && same_bytes(wa, nwa) && same_bytes(wb, nwb), "zdist of random polylines %d, h %g: %.17g, legacy %.17g", t, h, db, da);
            cases++;
            found += db < h;
        }
        note("(%ld cases, %ld with a pair nearer than h)", cases, found);
        // Constructed cases (they draw no random numbers, so the cases above keep their inputs). (1) One vertex, and one
        // short segment in each of the 81 cells around its cell, just across the boundaries: each is nearer than h, and
        // only the loop over all 81 cells finds it. (2) Vertices on the boundaries of the cells, where x + 2 rounds in
        // float as the legacy key computes it: -1e-8 + 2 is 2 in float (the segment's cell k) but not in double (k - 1),
        // and the vertex just below the next boundary rounds up into cell k + 1, from which only cell k is near.
        long nc = 0, nnear = 0;
        auto one = [&](const P4& p, const P4& q0, const P4& q1, double h, const char* what) {
            Renderer ra(w.L.plus, w.L.L, 1, 1), rb(w.L.minus, w.L.L, 1, 1);
            z::Renderer<NB> na(w.N.plus, w.N.L, 1, 1), nb(w.N.minus, w.N.L, 1, 1);
            ra.starts.push_back(0); ra.push(p);
            na.starts.push_back(0); na.push(np4(p));
            rb.starts.push_back(0); rb.push(q0); rb.push(q1);
            nb.starts.push_back(0); nb.push(np4(q0)); nb.push(np4(q1));
            P4 wa, wb;
            z::SpherePoint<4> nwa, nwb;
            memset(&wa, 0x44, sizeof wa); memset(&wb, 0x44, sizeof wb);
            memset(&nwa, 0x44, sizeof nwa); memset(&nwb, 0x44, sizeof nwb);
            double da = zdist(ra, rb, h, wa, wb), db = z::zdist(na, nb, h, nwa, nwb);
            check(same_bytes(da, db) && same_bytes(wa, nwa) && same_bytes(wb, nwb) && db < h, "zdist, %s: %.17g, legacy %.17g (h %g)", what, db, da, h);
            nc++;
            nnear += db < h;
        };
        for (int m = 0; m < 81; m++) {
            const double h = 0.25;                 // the vertex in cell 8 of every coordinate: (x + 2)/0.25 in [8, 9)
            P4 p, q0, q1;
            for (int c = 0; c < 4; c++) {
                int d = (m/(c == 0 ? 1 : c == 1 ? 3 : c == 2 ? 9 : 27)) % 3 - 1;
                p.x[c] = d > 0 ? 0.24 : d < 0 ? 0.01 : 0.125;
                q0.x[c] = d > 0 ? 0.26 : d < 0 ? -0.01 : 0.125;
                q1.x[c] = q0.x[c] + (c == 3 ? 0.002 : 0);
            }
            char what[80];
            snprintf(what, sizeof what, "a segment in the neighbouring cell %d of 81", m);
            one(p, q0, q1, h, what);
        }
        for (int c = 0; c < 4; c++)
            for (double h : {0.5, 0.25}) {
                P4 p = {{0, 0, 0, 0}}, q0 = {{0, 0, 0, 0}}, q1;
                q0.x[c] = (double)-1e-8f;
                p.x[c] = (double)std::nextafter((float)(h), 0.0f);      // the largest float below h
                q1 = q0;
                q1.x[(c + 1) % 4] = 0.001;
                char what[80];
                snprintf(what, sizeof what, "the float rounding of x + 2 in coordinate %d, h %g", c, h);
                one(p, q0, q1, h, what);
            }
        note("(constructed: %ld cases, a segment in each of the 81 cells and on the cells' float boundaries; %ld found)", nc, nnear);
    }

    // ---- the linked pairs ----
    std::vector<LinkedPair> LP;
    std::vector<z::LinkedPair<NB>> NLP;
    section("load_pairs: linked_pairs.txt, variants of it, a missing and an empty file");
    {
        std::string text = rtest::read_file(repo_file("linked_pairs.txt"));
        check(!text.empty(), "reading linked_pairs.txt");
        std::vector<std::string> lines = rtest::split_lines(text);
        std::vector<std::pair<std::string, std::string>> variants = {{"as stored", text}};
        {
            std::vector<std::string> v;
            for (const std::string& x : lines) { if (rnd(0, 2) == 0) v.push_back(rnd(0, 1) ? "" : "# lk +1 | w 1 | w' 2 | phi(h) 3 | h a1"); v.push_back(x); }
            variants.push_back({"with comments and blank lines", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v.push_back("lk +1 | w 1 2 | w' 3 4 | phi(h) 1");                         // 4 fields
            v.push_back("lk -1 | w 1 2 | w' 3 4 | phi(h) 1 | h a1 | extra");          // 6 fields
            v.push_back("lk x | w | w' | phi(h) y | h");                              // fields that do not parse
            v.push_back("lk +3|w 5 6 7|w' 8|phi(h) -2|h A1 a2 A3");                   // no blanks
            v.push_back("  lk -2 | w 9 x 10 | w' 11 | phi(h) 4 | hh a1");             // a word that stops, h twice
            v.push_back("lk +1 | w 1 | w' 2 | phi(h) 1 | no letter");                 // no 'h' in the last field
            v.push_back("lk +1 | w 1 | w' 2 | phi(h) 1 | h a1\r");                    // a carriage return
            variants.push_back({"with odd lines", rtest::join_lines(v)});
        }
        variants.push_back({"one pair", lines.size() > 1 ? lines[0] + "\n" + lines[1] + "\n" : text});
        variants.push_back({"comments only", "# nothing\n\n# here\n"});
        variants.push_back({"empty", ""});
        for (const auto& var : variants) {
            std::string fn = tmp.file("pairs.txt");
            rtest::write_file(fn, var.second);
            std::vector<LinkedPair> a;
            std::vector<z::LinkedPair<NB>> b;
            bool ra = false, rb = false;
            std::string ea = rtest::capture_stderr([&] { ra = load_pairs(fn, a); });
            std::string eb = rtest::capture_stderr([&] { rb = z::load_pairs(fn, b); });
            check(ra == rb && ea == eb, "%s: %d \"%s\", legacy %d \"%s\"", var.first.c_str(), rb, eb.c_str(), ra, ea.c_str());
            bool same = a.size() == b.size();
            for (size_t i = 0; same && i < a.size(); i++) same = same_pair_fields(a[i], b[i]) && !b[i].a && !b[i].b;
            check(same, "%s: the pairs read (%zu, legacy %zu)", var.first.c_str(), b.size(), a.size());
            if (var.first == "with odd lines") note("(with odd lines: %zu pairs read)", b.size());
        }
        std::string fn = tmp.file("no such file.txt");
        std::vector<LinkedPair> a;
        std::vector<z::LinkedPair<NB>> b;
        bool ra = true, rb = true;
        std::string ea = rtest::capture_stderr([&] { ra = load_pairs(fn, a); });
        std::string eb = rtest::capture_stderr([&] { rb = z::load_pairs(fn, b); });
        check(!ra && !rb && ea == eb && a.empty() && b.empty(), "a missing file: %d \"%s\", legacy %d \"%s\"", rb, eb.c_str(), ra, ea.c_str());
        // the stored pairs, for build_pair
        std::string fs = repo_file("linked_pairs.txt");
        rtest::capture_stderr([&] { load_pairs(fs, LP); });
        rtest::capture_stderr([&] { z::load_pairs(fs, NLP); });
        note("(the stored file: %zu pairs)", NLP.size());
    }

    section("build_pair: the 13 stored pairs at accuracies 0.05, 0.02, 0.01 (rebuilt in place), with t3");
    {
        long n = 0;
        size_t npts = 0;
        for (double acc : {0.05, 0.02, 0.01})
            for (size_t i = 0; i < LP.size() && i < NLP.size(); i++) {
                build_pair(LP[i], W[2].L, acc);
                z::build_pair<NP>(NLP[i], W[2].N, acc);
                check(same_pair_fields(LP[i], NLP[i]) && same_loop_renderer(LP[i].a, NLP[i].a) && same_loop_renderer(LP[i].b, NLP[i].b),
                      "pair %zu (w %s, w' %s, h %s) at acc %g", i, word_text(NLP[i].w).c_str(), word_text(NLP[i].w2).c_str(),
                      NLP[i].hword.c_str(), acc);
                n++;
                npts += NLP[i].a->pts.size()/4 + NLP[i].b->pts.size()/4;
            }
        note("(%ld pairs built, %zu points)", n, npts);
    }

    section("build_pair: relations s s^-1 and s s' s'^-1 s^-1 of every stored file, random words h");
    {
        long n = 0;
        for (int f = 0; f < 3; f++) {
            const Letters& L = W[f].L.L;
            for (int t = 0; t < 8; t++) {
                LinkedPair a;
                z::LinkedPair<NB> b;
                int s = (int)rnd(0, (int64_t)L.s.size() - 1), s2 = (int)rnd(0, (int64_t)L.s.size() - 1);
                a.w = t % 2 ? std::vector<int>{s, L.inv[s]} : std::vector<int>{s, s2, L.inv[s2], L.inv[s]};
                a.w2 = t % 3 ? std::vector<int>{s2, L.inv[s2]} : std::vector<int>{s2, s, L.inv[s], L.inv[s2]};
                a.hword = " " + random_aword(4);
                a.lk = rnd(-3, 3); a.phi = (int)rnd(-2, 2);
                b.w = a.w; b.w2 = a.w2; b.hword = a.hword; b.lk = a.lk; b.phi = a.phi;
                double acc = t < 4 ? 0.05 : 0.03;
                build_pair(a, W[f].L, acc);
                z::build_pair<NP>(b, W[f].N, acc);
                check(same_pair_fields(a, b) && same_loop_renderer(a.a, b.a) && same_loop_renderer(a.b, b.b),
                      "%s: w %s, w' %s, h %s at acc %g", FILES3[f], word_text(b.w).c_str(), word_text(b.w2).c_str(), b.hword.c_str(), acc);
                delete a.a; delete a.b; delete b.a; delete b.b;
                n++;
            }
        }
        note("(%ld pairs)", n);
    }

    section("pair_view, pair_desc: the stored pairs and random ones (long words, truncated)");
    {
        long n = 0, truncated = 0;
        for (size_t i = 0; i < LP.size() && i < NLP.size(); i++)
            for (int k : {(int)i, (int)i + 13, -1, 0, 1000000}) {
                check(pair_desc(LP[i], k) == z::pair_desc(NLP[i], k), "pair_desc of pair %zu as %d", i, k);
                ViewState v;
                Options opt;
                init_view(opt, W[2].L, v);
                z::s3::ViewState nv = nview(v);
                pair_view(LP[i], v);
                z::pair_view(NLP[i], nv);
                check(same_bytes(v, nv), "pair_view of pair %zu", i);
                n += 2;
            }
        for (int t = 0; t < 3000; t++) {
            LinkedPair a;
            z::LinkedPair<NB> b;
            a.lk = t % 50 == 0 ? (rnd(0, 1) ? LONG_MAX : LONG_MIN) : rnd(-1000, 1000);
            a.phi = t % 50 == 1 ? (rnd(0, 1) ? INT_MAX : INT_MIN) : (int)rnd(-100, 100);
            int la = t % 10 == 0 ? (int)rnd(40, 120) : (int)rnd(0, 12), lb = t % 10 == 1 ? (int)rnd(40, 120) : (int)rnd(0, 12);
            for (int i = 0; i < la; i++) a.w.push_back((int)rnd(-5, 40));
            for (int i = 0; i < lb; i++) a.w2.push_back((int)rnd(-5, 40));
            P4 p = random_p4((int)rnd(0, 4));
            memcpy(a.pole, p.x, sizeof a.pole);
            a.clip = uniform(0.1, 50);
            b.lk = a.lk; b.phi = a.phi; b.w = a.w; b.w2 = a.w2;
            memcpy(b.pole, a.pole, sizeof b.pole);
            b.clip = a.clip;
            int k = (int)rnd(-5, 100000);
            std::string da = pair_desc(a, k), db = z::pair_desc(b, k);
            check(da == db, "pair_desc of a random pair: \"%s\", legacy \"%s\"", db.c_str(), da.c_str());
            truncated += db.size() == 399;
            ViewState v;
            Options opt;
            opt.yaw = random_angle(); opt.pitch = random_angle(); opt.zoom = uniform(0.2, 5); opt.clip = uniform(0.5, 9);
            init_view(opt, W[t % 3].L, v);
            z::s3::ViewState nv = nview(v);
            pair_view(a, v);
            z::pair_view(b, nv);
            check(same_bytes(v, nv), "pair_view of a random pair");
            n += 2;
        }
        note("(%ld comparisons; %ld descriptions truncated at 399 characters)", n, truncated);
    }

    for (size_t i = 0; i < LP.size(); i++) { delete LP[i].a; delete LP[i].b; }
    for (size_t i = 0; i < NLP.size(); i++) { delete NLP[i].a; delete NLP[i].b; }
    for (Worlds& w : W) { delete w.L.rp; delete w.L.rn; delete w.N.rp; delete w.N.rn; }

    rtest::end_section();
    printf("test_view_s3: %ld comparisons with the legacy code, %ld differ (%.1f s)\n", rtest::all_n, rtest::all_bad,
           z::now() - t_start);
    return rtest::all_bad ? 1 : 0;
}
