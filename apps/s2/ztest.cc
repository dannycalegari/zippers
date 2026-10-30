/*  ztest.cc  --  checks of the frame engine against the zipper program itself.

      ztest same     the tagged build equals the program's build_zippers (same segments)
      ztest depth    leaves with tag <= R' of a build at R, against the program at R'
      ztest frame    framing across a T-morph: motion of the base arcs per step of T

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

    apps/s2/ztest.cc (the zippers suite): legacy/s2/movie/src/ztest.cc, unchanged
    but for this paragraph and one check: a word other than same, depth, frame and all is
    refused, with the usage line and exit status 1 (the legacy program ran nothing and exited
    with 0).  It includes the film engine of this folder, apps/s2/zmovie.cc,
    which includes the program apps/s2/zipper.cc, so it checks the engine against the
    program as they are built on the shared library src/.
*/
#pragma clang diagnostic ignored "-Wunused-function"
#define ZMOVIE_NO_MAIN
#include "zmovie.cc"

typedef std::array<float,6> Seg6;
// the segments of a renderer's polylines (optionally only polylines with tag <= rmax), each
// with its endpoints in a fixed order
static vector<Seg6> segments(const Renderer& r, const vector<float>* tag = nullptr, double rmax = INFINITY) {
    vector<Seg6> v;
    for (size_t k = 0; k < r.npoly(); ++k) {
        if (tag && (*tag)[k] > rmax) continue;
        for (size_t i = r.poly_begin(k) + 1; i < r.poly_end(k); ++i) {
            std::array<float,3> a{r.px[i-1], r.py[i-1], r.pz[i-1]}, b{r.px[i], r.py[i], r.pz[i]};
            if (b < a) std::swap(a, b);
            v.push_back({a[0], a[1], a[2], b[0], b[1], b[2]});
        }
    }
    std::sort(v.begin(), v.end());
    return v;
}
static double seglen(const Seg6& s) { return std::sqrt((s[0]-s[3])*(s[0]-s[3]) + (s[1]-s[4])*(s[1]-s[4]) + (s[2]-s[5])*(s[2]-s[5])); }
struct Diff { size_t na, nb, onlyA, onlyB; double lenA, lenOnlyA, lenOnlyB; };
static Diff compare(const vector<Seg6>& A, const vector<Seg6>& B) {
    Diff d{A.size(), B.size(), 0, 0, 0, 0, 0};
    size_t i = 0, j = 0;
    for (auto& s : A) d.lenA += seglen(s);
    while (i < A.size() || j < B.size()) {
        if (j >= B.size() || (i < A.size() && A[i] < B[j])) { d.onlyA++; d.lenOnlyA += seglen(A[i]); ++i; }
        else if (i >= A.size() || B[j] < A[i]) { d.onlyB++; d.lenOnlyB += seglen(B[j]); ++j; }
        else { ++i; ++j; }
    }
    return d;
}
// the program's own picture: prepare + build_zippers (whole sphere)
static void program_build(const string& m, int order, double R, double acc, Renderer*& rp, Renderer*& rn, Rep*& rep) {
    Options opt; opt.monodromy = m; opt.order = order; opt.radius = R; opt.accuracy = acc;
    Monodromy mon; rep = new Rep;
    if (!prepare(opt, mon, *rep)) exit(1);
    rp = new Renderer(*rep); rn = new Renderer(*rep);
    Zippers zs;
    build_zippers(opt, mon, *rep, nullptr, *rp, *rn, zs);
}
static Scene* my_build(const string& m, double T, double R, double acc) {
    static Framing fr; fr.parse("native");
    BuildSpec b; b.monName = m; b.T = T; b.R = R; b.acc = acc; b.framing = &fr;
    g_rc = RepCache();
    Scene* sc = new Scene;
    if (!build_scene(b, *sc)) exit(1);
    return sc;
}
// pixels that differ between two renderings of the same view (ztest's own rasteriser)
static void pixel_diff(const Renderer& a1, const Renderer& a2, const vector<float>* tag1, double rmax,
                       const Renderer& b1, const Renderer& b2, size_t& ndiff, double& maxd) {
    auto draw = [&](const Renderer& p, const Renderer& n, const vector<float>* t1, const vector<float>* t2, vector<uint8_t>& out) {
        Raster ras(840, 840, 3); double bg[3] = {1,1,1}; ras.clear(bg);
        FrameCtx F; F.v = make_view(0,0,0); F.zoom = 1; F.cx = ras.w/2.0; F.cy = ras.h/2.0; F.rad = 418*3; F.fog = 0;
        LayerDef L; L.width = 1; Track::parse("r", "1", 1, SP_LIN, L.reveal); L.depth = true;
        const Renderer* rs[2] = {&p, &n}; const vector<float>* ts[2] = {t1, t2};
        double col[2][3] = {{0.05,0.15,0.85},{0.85,0.08,0.08}};
        for (int s = 0; s < 2; ++s) {
            Curves C; C.r = rs[s];
            if (ts[s]) C.tag = *ts[s];
            C.arclen.assign(1, 1.0f); C.cum.assign(rs[s]->px.size(), 0.0f);
            draw_curves(ras, F, C, L, 0, false, ts[s] ? rmax : INFINITY, 0);
            ras.composite(col[s], 1.0);
        }
        ras.to_rgb(out);
    };
    vector<uint8_t> A, B;
    draw(a1, a2, tag1, tag1 ? tag1 + 1 : nullptr, A);
    draw(b1, b2, nullptr, nullptr, B);
    ndiff = 0; maxd = 0;
    for (size_t i = 0; i < A.size(); i += 3) {
        int d = 0; for (int k = 0; k < 3; ++k) d = std::max(d, std::abs((int)A[i+k] - (int)B[i+k]));
        if (d > 8) ++ndiff;
        maxd = std::max(maxd, (double)d);
    }
}

static void test_same() {
    struct C { const char* m; int n; double R; } cs[] = {{"fig8", 2, 5}, {"fig8", 3, 6}, {"fig8", 0, 5}, {"RRL", 3, 5}};
    for (auto& c : cs) {
        Renderer *pp, *pn; Rep* rep; program_build(c.m, c.n, c.R, 0.004, pp, pn, rep);
        Scene* sc = my_build(c.m, c.n == 0 ? INFINITY : c.n, c.R, 0.004);
        Diff dp = compare(segments(*sc->rp), segments(*pp)), dn = compare(segments(*sc->rn), segments(*pn));
        printf("same  %s n=%s R=%g: Z+ %zu vs %zu segments, %zu/%zu differ;  Z- %zu vs %zu, %zu/%zu differ  (polylines: program %zu+%zu joined, tagged %zu+%zu)\n",
               c.m, c.n ? std::to_string(c.n).c_str() : "inf", c.R, dp.na, dp.nb, dp.onlyA, dp.onlyB, dn.na, dn.nb, dn.onlyA, dn.onlyB,
               pp->npoly(), pn->npoly(), sc->rp->npoly(), sc->rn->npoly());
        fflush(stdout);
    }
}
static void test_depth() {
    struct C { const char* m; int n; double R; vector<double> sub; } cs[] = {
        {"fig8", 2, 6, {2, 3, 4, 5, 5.5}}, {"fig8", 3, 6, {3, 4.5, 5.25}}, {"fig8", 0, 6, {4, 5}} };
    for (auto& c : cs) {
        Scene* sc = my_build(c.m, c.n == 0 ? INFINITY : c.n, c.R, 0.004);
        for (double r : c.sub) {
            Renderer *pp, *pn; Rep* rep; program_build(c.m, c.n, r, 0.004, pp, pn, rep);
            Diff dp = compare(segments(*sc->rp, &sc->cp.tag, r), segments(*pp));
            Diff dn = compare(segments(*sc->rn, &sc->cn.tag, r), segments(*pn));
            size_t npx; double maxd;
            // (pixel_diff wants the two tag arrays adjacent: pass them through a pair)
            vector<float> tags[2] = {sc->cp.tag, sc->cn.tag};
            pixel_diff(*sc->rp, *sc->rn, tags, r, *pp, *pn, npx, maxd);
            printf("depth %s n=%s R=%g, tag <= %g vs the program at R=%g: Z+ %zu vs %zu segments, only-tagged %zu (%.2g%% of length), only-program %zu (%.2g%%); "
                   "Z- %zu vs %zu, %zu (%.2g%%), %zu (%.2g%%); pixels differing by > 8/255: %zu (max %g)\n",
                   c.m, c.n ? std::to_string(c.n).c_str() : "inf", c.R, r, r,
                   dp.na, dp.nb, dp.onlyA, 100*dp.lenOnlyA/dp.lenA, dp.onlyB, 100*dp.lenOnlyB/dp.lenA,
                   dn.na, dn.nb, dn.onlyA, 100*dn.lenOnlyA/dn.lenA, dn.onlyB, 100*dn.lenOnlyB/dn.lenA, npx, maxd);
            fflush(stdout);
            delete pp; delete pn;
        }
    }
}
// the vertices of the base arcs' splitting down to depth 4: points g.V of the arcs, continuous in T
static vector<Pt> arc_vertices(const Scene& sc) {
    vector<Pt> P;
    for (int side = 0; side < 2; ++side) {
        Renderer r(sc.rep);
        if (!side) r.setup(sc.mon.phi_a, sc.mon.phi_b, sc.rep.Ci); else r.setup(sc.mon.psi_a, sc.mon.psi_b, sc.rep.C);
        struct F { Mat b; char l; int d; };
        vector<F> st{{identity(), 'a', 0}, {identity(), 'b', 0}};
        while (!st.empty()) {
            F f = st.back(); st.pop_back();
            P.push_back(project(act(f.b, r.VV)));
            if (f.d >= 4) continue;
            Mat N = matmul(f.b, r.prepend);
            for (char ch : r.img[Renderer::lidx(f.l)]) { st.push_back({N, ch, f.d + 1}); N = matmul(N, sc.rep.gen(ch)); }
        }
    }
    return P;
}
static void test_frame() {
    // a morph T = 2 -> 3 and 3 -> inf (in 60 steps of 1/T): the largest motion of a vertex of the
    // base arcs between steps, per framing, in pixels of an 840 frame (radius 418)
    for (const char* mode : {"native", "ref 2", "interp 2 3", "none"}) {
        for (int leg = 0; leg < 2; ++leg) {
            Framing fr; fr.parse(mode);
            if (leg == 1 && fr.mode == Framing::INTERP) { fr.T0 = 3; fr.T1 = INFINITY; }
            if (leg == 1 && fr.mode == Framing::REF) fr.T0 = 3;
            double maxStep = 0, endJump = 0, total = 0; vector<Pt> prev, first;
            int N = 60;
            for (int i = 0; i <= N; ++i) {
                double u0 = leg ? 1.0/3 : 0.5, u1 = leg ? 0.0 : 1.0/3;
                double u = u0 + (u1 - u0)*i/(double)N, T = u <= 1e-15 ? INFINITY : 1.0/u;
                if (std::abs(T - std::round(T)) < 1e-9) T = std::round(T);
                BuildSpec b; b.monName = "fig8"; b.T = T; b.R = 0; b.acc = 0.004; b.framing = &fr;
                g_rc = RepCache();
                Scene sc; string mn; if (!scene_rep(b, sc, mn)) exit(1);
                vector<Pt> P = arc_vertices(sc);
                if (!prev.empty()) {
                    double m = 0;
                    for (size_t j = 0; j < P.size(); ++j) m = std::max(m, dist3(P[j], prev[j]));
                    maxStep = std::max(maxStep, m*418);
                    if (i == N) endJump = m*418;
                } else first = P;
                if (i == N) for (size_t j = 0; j < P.size(); ++j) total = std::max(total, dist3(P[j], first[j])*418);
                prev = P;
            }
            printf("frame %-11s leg %s: largest step of an arc vertex %6.1f px, last step %6.1f px, largest total motion %6.1f px\n",
                   mode, leg ? "3 -> inf" : "2 -> 3  ", maxStep, endJump, total);
            fflush(stdout);
        }
    }
}
int main(int argc, char** argv) {
    g_quiet = true;
    string w = argc > 1 ? argv[1] : "all";
    if (w != "same" && w != "depth" && w != "frame" && w != "all") {
        fprintf(stderr, "usage: ztest [same|depth|frame|all]\n");
        return 1;
    }
    if (w == "same" || w == "all") test_same();
    if (w == "depth" || w == "all") test_depth();
    if (w == "frame" || w == "all") test_frame();
    return 0;
}
