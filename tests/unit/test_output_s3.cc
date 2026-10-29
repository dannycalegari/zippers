// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_output_s3: the PNG encoder and the rasteriser of src/output/ (png.h, raster.h) agree BYTE FOR BYTE with the
// S3 originals of legacy/s3/zipcore.h:
//   png::chunk       png_chunk                 raster::line_rgb   ras_line
//   png::write_rgb   write_png_rgb             raster::cross, paint_segments, downsample_rgb
//                                              the three pieces of write_png (which is called itself, below)
// (The S4 copies of these functions are tested by test_output_s4, the S2 ones by test_output_s2; the legacy headers
// cannot be included together.)
//
// What is compared, and on what:
//   * the chunks and the PNG files, on random chunks and on random images (sizes from 1x1 to 1000x1000, odd, wide, tall;
//     six kinds of image; every zlib level; failures), file against file;
//   * a file that cannot grow (a size limit makes fclose fail): write_png_rgb ignores that and returns true, and so must
//     the new function, with the same partial file;
//   * the lines: random lines inside, across and outside images from 1x1 to 48x48, of length zero, steep and shallow,
//     horizontal, vertical and diagonal, with brushes 0 to 3, at and over the edge; the whole image buffers;
//   * the crosses of the marks, and the segment painter on random lists with many equal depths and priorities;
//   * write_png of zipcore.h itself, on random scenes (polylines, views, marks, sizes, brushes, equal depths among
//     layers): the file it writes against the file of the same picture assembled from raster.h and png.h.
// Build it with the flags of the programs (-O2), since those are the flags under which the results must agree.

#include "zipcore.h"
#include "output_test_support.h"

#include <deque>

using outtest::Bytes;
using outtest::Category;
using outtest::Random;
using outtest::Report;
using outtest::WorkDir;
namespace zp = outtest::zp;
namespace zr = outtest::zr;

// ---- the legacy functions, as callables ------------------------------------------------------------------------------
static void legacy_chunk(FILE* f, const char* type, const uint8_t* data, size_t len) { png_chunk(f, type, data, len); }
static bool legacy_write(const char* fn, const Bytes& rgb, int W, int H, int level) { return write_png_rgb(fn, rgb, W, H, level); }
static bool legacy_write_default(const char* fn, const Bytes& rgb, int W, int H) { return write_png_rgb(fn, rgb, W, H); }
static void legacy_line9(Bytes& img, int W, int H, int x0, int y0, int x1, int y1, const uint8_t* c, int th) { ras_line(img, W, H, x0, y0, x1, y1, c, th); }
static void legacy_line8(Bytes& img, int W, int H, int x0, int y0, int x1, int y1, const uint8_t* c) { ras_line(img, W, H, x0, y0, x1, y1, c); }

// ---- raster::downsample_rgb against the mean of each block, on noise and any factor ----------------------------------------
static void test_downsample_formula(Report& rep, Random& r)
{
    Category& c = rep.cat("raster::downsample_rgb vs the mean of each block, on noise (factors 1 to 4)");
    for (int t = 0; t < 402; t++) {
        const int SS = t == 400 ? 2 : t == 401 ? 3 : r.uniform(1, 4), size = t == 400 ? 1000 : t == 401 ? 517 : r.uniform(1, 40), w = size*SS;   // (two large ones)
        Bytes img((size_t)w*w*3);
        if (t % 5 == 0) std::fill(img.begin(), img.end(), (uint8_t)255);                 // the largest sums
        else r.fill(img);
        Bytes ref((size_t)size*size*3);
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) for (int k = 0; k < 3; k++) {
            int sum = 0;
            for (int j = 0; j < SS; j++) for (int i = 0; i < SS; i++) sum += img[((size_t)(y*SS + j)*w + (x*SS + i))*3 + k];
            ref[((size_t)y*size + x)*3 + k] = (uint8_t)(sum/(SS*SS));
        }
        c.check(zr::downsample_rgb(img, w, size, SS) == ref, "size %d, factor %d", size, SS);
    }
}

// ---- write_png of zipcore.h, rebuilt from src/output/ ----------------------------------------------------------------------
// The part of write_png that belongs to the views (the projection with `eye`, the fog, the pixel of a mark) is copied
// from it, as src/view/ has it; everything else is zippers::raster and zippers::png. With
// stable_sort the same picture is made with std::stable_sort (a check of the test: it must sometimes differ).
static bool write_png_new(const char* fn, int size, const vector<Layer>& layers, const ViewState& v, const vector<P4>& marks, int thick,
                          bool stable_sort = false, int* crosses = nullptr, size_t* nsegs = nullptr)
{
    const int SS = 2, w = size*SS, h = size*SS;
    vector<uint8_t> img((size_t)w*h*3, 255);
    double half = v.clip/v.zoom, scale = (w/2.0)/half;
    vector<zr::Segment> segs;
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
                    zr::Segment s = {(float)(w/2.0 + e0[0]*scale), (float)(h/2.0 - e0[1]*scale),
                                     (float)(w/2.0 + e1[0]*scale), (float)(h/2.0 - e1[1]*scale), (float)z, {0,0,0}, L.prio};
                    for (int c = 0; c < 3; c++) s.c[c] = (uint8_t)std::lround(255*(1 - f + f*L.col[c]));
                    segs.push_back(s);
                }
                memcpy(e0, e1, sizeof e0);
                ok0 = ok1;
            }
        }
    }
    if (nsegs) *nsegs = segs.size();
    if (!stable_sort) zr::paint_segments(img, w, h, segs, thick);
    else {
        std::stable_sort(segs.begin(), segs.end(), [](const zr::Segment& a, const zr::Segment& b) { return a.prio != b.prio ? a.prio < b.prio : a.z < b.z; });
        for (const zr::Segment& s : segs)
            zr::line_rgb(img, w, h, (int)std::lround(s.x0), (int)std::lround(s.y0), (int)std::lround(s.x1), (int)std::lround(s.y1), s.c, thick);
    }
    for (const P4& p : marks) {
        float x[4] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3]};
        double e[3];
        if (!eye(v, x, e)) continue;
        int cx = (int)std::lround(w/2.0 + e[0]*scale), cy = (int)std::lround(h/2.0 - e[1]*scale);
        zr::cross(img, w, h, cx, cy, SS, 0);
        if (crosses) ++*crosses;
    }
    vector<uint8_t> out = zr::downsample_rgb(img, w, size, SS);
    return zp::write_rgb(fn, out, size, size);
}

// ---- random scenes: polylines on S^3 (a Renderer that only holds points), a view, marks ------------------------------------
static Side g_side;            // empty: a Renderer holds references to them, and drawing never reads them
static Letters g_letters;

struct Scene {
    std::deque<Renderer> rends;
    vector<Layer> layers;
    ViewState v;
    vector<P4> marks;
    int size = 1, thick = 0;
    bool ties = false;
};

static P4 random_point(Random& r)                                      // on S^3
{
    P4 p;
    double s = 0;
    for (double& x : p.x) { x = r.gauss(); s += x*x; }
    for (double& x : p.x) x /= std::sqrt(s);
    return p;
}

static void random_orthogonal4(Random& r, double Q[4][4])
{
    for (int i = 0; i < 4; i++) {
        double v[4];
        for (double& x : v) x = r.gauss();
        for (int j = 0; j < i; j++) {
            double d = 0;
            for (int k = 0; k < 4; k++) d += v[k]*Q[j][k];
            for (int k = 0; k < 4; k++) v[k] -= d*Q[j][k];
        }
        double l = 0;
        for (double x : v) l += x*x;
        l = std::sqrt(l);
        for (int k = 0; k < 4; k++) Q[i][k] = v[k]/l;
    }
}

// kind: 0 random walks, 1 independent points (long segments), 2 equal depths (the identity view, depths from three
// values), 3 a large zoom on a small picture, 4 no polylines (marks only)
static void random_scene(Random& r, int kind, Scene& S)
{
    S.size = kind == 2 ? r.pick<int>({40, 64, 100, 150}) : kind == 3 ? r.uniform(1, 6) : r.pick<int>({1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, r.uniform(1, 120)});
    S.thick = r.uniform(0, 3);
    S.ties = kind == 2;
    if (kind == 2) {
        memset(S.v.Q, 0, sizeof S.v.Q);
        for (int i = 0; i < 4; i++) S.v.Q[i][i] = 1;
        double I[9] = {1,0,0, 0,1,0, 0,0,1};
        memcpy(S.v.rot, I, sizeof I);
        S.v.zoom = 1; S.v.clip = 3;
    } else {
        random_orthogonal4(r, S.v.Q);
        rot3(r.real(-180, 180), r.real(-90, 90), S.v.rot);
        S.v.zoom = kind == 3 ? r.pick<double>({5, 10, 40}) : r.pick<double>({0.5, 1, 1, 2, 4});
        S.v.clip = r.pick<double>({1.5, 3, 3, 6});
    }
    const int nlayers = kind == 4 ? 1 : r.uniform(1, 3);
    for (int l = 0; l < nlayers; l++) {
        S.rends.emplace_back(g_side, g_letters, 1.0, (size_t)1 << 30);
        Renderer& R = S.rends.back();
        const int npoly = kind == 4 ? 0 : kind == 2 ? r.uniform(2, 6) : r.uniform(1, 6);
        for (int k = 0; k < npoly; k++) {
            R.starts.push_back((uint32_t)(R.pts.size()/4));
            const int npts = kind == 2 ? r.uniform(10, 80) : r.pick<int>({1, 2, 2, 3, 10, 30, 60});   // (never empty: the legacy loop reads the first point)
            P4 p = random_point(r);
            for (int i = 0; i < npts; i++) {
                if (kind == 2) { p.x[0] = r.real(-1, 1); p.x[1] = r.real(-1, 1); p.x[2] = r.pick<double>({-0.5, 0, 0.5}); p.x[3] = 0; }
                else if (kind == 1 || kind == 3) p = random_point(r);
                else {                                                    // a random walk on the sphere
                    double s = 0;
                    for (double& x : p.x) { x += 0.25*r.gauss(); s += x*x; }
                    for (double& x : p.x) x /= std::sqrt(s);
                }
                R.push(p);
            }
        }
        S.layers.push_back(Layer{&R, {r.real(0, 1), r.real(0, 1), r.real(0, 1)}, kind == 2 ? r.pick<int>({1, 1, 1, 2}) : r.pick<int>({0, 1, 1, 2})});
    }
    const int nmarks = r.uniform(0, 4);
    for (int i = 0; i < nmarks; i++) {
        P4 p = random_point(r);
        if (!S.rends.empty() && S.rends[0].pts.size() >= 4 && r.coin(0.6)) {          // a point of a polyline: surely in view if that is
            size_t j = (size_t)r.uniform(0, (int)(S.rends[0].pts.size()/4) - 1);
            for (int k = 0; k < 4; k++) p.x[k] = S.rends[0].pts[4*j + (size_t)k];
        }
        S.marks.push_back(p);
    }
}

static void test_write_png(Report& rep, Random& r, const WorkDir& wd)
{
    Category& c = rep.cat("write_png (S3) vs the same picture from raster.h + png.h: files and return values");
    Category& teeth = rep.cat("(the test itself) scenes with equal depths on which std::stable_sort draws another picture");
    Category& ink = rep.cat("(the test itself) scenes that drew segments, crosses, or any non-white pixel");
    const std::string fa = wd.path("scene_legacy.png"), fb = wd.path("scene_new.png"), fc = wd.path("scene_stable.png");
    long scenes = 0, tie_scenes = 0, stable_differs = 0, with_segments = 0, with_crosses = 0, with_ink = 0;
    size_t total_segments = 0;
    const int kinds[] = {0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 4};
    for (int t = 0; t < 720; t++) {
        const int kind = kinds[t % 12];
        Scene S;
        random_scene(r, kind, S);
        remove(fa.c_str()); remove(fb.c_str());
        bool ra = write_png(fa.c_str(), S.size, S.layers, S.v, S.marks, S.thick);
        int crosses = 0; size_t nsegs = 0;
        bool rb = write_png_new(fb.c_str(), S.size, S.layers, S.v, S.marks, S.thick, false, &crosses, &nsegs);
        c.check(ra == rb && outtest::same_file(fa, fb), "scene %d (kind %d): size %d, brush %d, %zu layers, %zu marks", t, kind, S.size, S.thick, S.layers.size(), S.marks.size());
        scenes++;
        total_segments += nsegs;
        if (nsegs) with_segments++;
        if (crosses) with_crosses++;
        {
            Bytes px; int pw = 0, ph = 0;
            if (zp::read(fa, px, pw, ph) && std::any_of(px.begin(), px.end(), [](uint8_t b) { return b != 255; })) with_ink++;
        }
        if (S.ties) {
            tie_scenes++;
            remove(fc.c_str());
            write_png_new(fc.c_str(), S.size, S.layers, S.v, S.marks, S.thick, true);
            if (!outtest::same_file(fa, fc)) stable_differs++;
        }
    }
    {   // the default arguments of write_png (thick = 0), and a file that cannot be written
        Scene S;
        random_scene(r, 0, S);
        bool ra = write_png(fa.c_str(), S.size, S.layers, S.v, S.marks), rb = write_png_new(fb.c_str(), S.size, S.layers, S.v, S.marks, 0);
        c.check(ra == rb && outtest::same_file(fa, fb), "the call with the default brush");
        const std::string na = wd.path("no_such_folder/a.png"), nb = wd.path("no_such_folder/b.png");
        ra = write_png(na.c_str(), S.size, S.layers, S.v, S.marks, 1);
        rb = write_png_new(nb.c_str(), S.size, S.layers, S.v, S.marks, 1);
        c.check(!ra && !rb, "an unwritable path returns %d, %d", (int)ra, (int)rb);
    }
    teeth.check(stable_differs > 0, "no scene was sensitive to the order of equal depths (%ld scenes)", tie_scenes);
    ink.check(with_segments > scenes/2 && with_crosses > scenes/4 && with_ink > scenes/2, "%ld of %ld scenes drew segments, %ld drew crosses, %ld have ink", with_segments, scenes, with_crosses, with_ink);
    printf("  [S3 write_png] %ld scenes, %zu segments painted; %ld scenes drew segments, %ld drew crosses, %ld have a non-white pixel; "
           "of %ld scenes with equal depths, %ld drew another picture with std::stable_sort\n", scenes, total_segments, with_segments, with_crosses, with_ink, tie_scenes, stable_differs);
}

int main()
{
    Report rep;
    WorkDir wd("test_output_s3");
    Random r(20261025ULL * 3 + 1);
    outtest::test_chunk(rep, r, wd, "S3", legacy_chunk);
    outtest::test_write_rgb(rep, r, wd, "S3", legacy_write, legacy_write_default);
    outtest::test_write_rgb_full_disk(rep, r, wd, "S3", legacy_write);
    outtest::test_lines_rgb(rep, r, "S3", 150000, legacy_line9, legacy_line8);
    outtest::test_cross(rep, r, "S3", 20000, legacy_line9);
    outtest::test_paint_segments(rep, r, "S3", 3000, legacy_line9);
    test_downsample_formula(rep, r);
    test_write_png(rep, r, wd);
    return rep.finish("test_output_s3");
}
