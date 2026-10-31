// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_gui_draw: the CPU half of the window's drawing (apps/gui/draw.h), with no OpenGL and no window. The vertex
// shaders' formulas are written here again in C++, in single precision, reading the uniforms as the shaders read them
// (column-major matrices; S^4's Q row by row), and compared with the painters of view/s2.h, s3.h and s4.h, in double:
//   - S^3 and S^4, random views (frames, rotations, zoom 1 to 40, clip 1.5 to 12, hidfog) and random points of the
//     sphere: the same points kept as s3::eye and s4::eye5 keep (but within 1e-5 of the clip sphere), the same eye
//     coordinates and hidden coordinate (to 1e-4), the same pixel per unit of the picture as the PNG of the same size
//     (and, in a window that is not square, the same in x and y: the shorter side shows the picture's width); the
//     colours of the segments' middles (the fogs) within one level of 255 of the painters'; and the depth: for two
//     segments, the one the painter draws later (the larger z, S^4 the larger z + 10 fh) has the smaller depth, and
//     every depth lies in [-1, 1] without being clamped;
//   - S^2, random rotations and zooms: the front hemisphere as the painter's (Z > 0), and the window's position of a
//     point, as a fraction of its side, that of the PNG that p saves (s2::export_framing and the painter's pixel);
//   - the instances of a layer (range, ends): for every range of polylines, the segments drawn are exactly the
//     painter's, the consecutive points of each polyline;
//   - the silhouette circle (its radius zoom, in view or not as the painter's test) and the crosses of the marks (at
//     the points that eye() keeps, of the half-length asked for).
// It does not compile or run the shaders (that needs an OpenGL context). A few seconds.

#include "draw.h"
#include "render_test_support.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>
#include <vector>

namespace z = zippers;
namespace draw = zippers::gui::draw;
using rtest::check;
using rtest::section;

namespace {

// ---- the shaders' formulas, again, from the uniforms ------------------------------------------------------------

// mat3 * v, mat4 * v with column-major arrays, as GLSL computes them
void mat3v(const float* m, const float* v, float* o)
{
    for (int r = 0; r < 3; r++) o[r] = m[r]*v[0] + m[3 + r]*v[1] + m[6 + r]*v[2];
}
void mat4v(const float* m, const float* v, float* o)
{
    for (int r = 0; r < 4; r++) o[r] = m[r]*v[0] + m[4 + r]*v[1] + m[8 + r]*v[2] + m[12 + r]*v[3];
}

bool sh_eye3(const draw::Uniforms& u, const float* x, float e[3])
{
    float y[4];
    mat4v(u.Q, x, y);
    const float den = 1.0f - y[3];
    if (den <= 1e-9f) return false;
    const float s[3] = {y[0]/den, y[1]/den, y[2]/den};
    if (s[0]*s[0] + s[1]*s[1] + s[2]*s[2] > u.clip*u.clip) return false;
    mat3v(u.rot, s, e);
    return true;
}

bool sh_eye5(const draw::Uniforms& u, const float* x, float e[3], float& hid)
{
    float y[5];
    for (int r = 0; r < 5; r++) {
        float s = 0;
        for (int k = 0; k < 5; k++) s += u.Q[5*r + k]*x[k];
        y[r] = s;
    }
    const float den = 1.0f - y[4];
    if (den <= 1e-9f) return false;
    const float s4[4] = {y[0]/den, y[1]/den, y[2]/den, y[3]/den};
    if (s4[0]*s4[0] + s4[1]*s4[1] + s4[2]*s4[2] + s4[3]*s4[3] > u.clip*u.clip) return false;
    float o[4];
    mat4v(u.M, s4, o);
    e[0] = o[0];
    e[1] = o[1];
    e[2] = o[2];
    hid = o[3];
    return true;
}

float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

// ---- random views and points ------------------------------------------------------------------------------------

template <int N>
std::vector<float> random_point()
{
    double x[N], n = 0;
    for (int i = 0; i < N; i++) {
        x[i] = rtest::gauss();
        n += x[i]*x[i];
    }
    std::vector<float> p(N);
    for (int i = 0; i < N; i++) p[i] = (float)(x[i]/std::sqrt(n));
    return p;
}

const double ZOOMS[] = {1, 3.7, 40};
const double CLIPS[] = {1.5, 3, 6, 12};

z::s3::ViewState random_view3(double zoom, double clip)
{
    z::s3::ViewState v;
    std::vector<float> c = random_point<4>();
    z::SpherePoint<4> V;
    for (int i = 0; i < 4; i++) V.x[i] = c[i];
    z::s3::default_frame(V, v.Q);
    const double a[3] = {0.6, 0.8, 0};
    z::s3::move_pole(v.Q, a, rtest::uniform(0, 1));
    z::s3::rot3(rtest::uniform(-180, 180), rtest::uniform(-90, 90), v.rot);
    v.zoom = zoom;
    v.clip = clip;
    return v;
}

z::s4::View5 random_view4(double zoom, double clip)
{
    z::s4::View5 v;
    std::vector<float> c = random_point<5>();
    double p[5];
    for (int i = 0; i < 5; i++) p[i] = c[i];
    z::s4::orthonormal_frame(p, v.Q);
    std::memset(v.U, 0, sizeof v.U);
    for (int i = 0; i < 4; i++) v.U[i][i] = 1;
    z::s4::turn4(v.U, 2, 3, rtest::uniform(-3, 3));
    z::s4::turn4(v.U, 0, 3, rtest::uniform(-3, 3));
    z::s4::rot3(rtest::uniform(-180, 180), rtest::uniform(-90, 90), v.rot);
    v.zoom = zoom;
    v.clip = clip;
    v.hidfog = rtest::uniform(0.5, 2);
    return v;
}

// ---- S^3 ------------------------------------------------------------------------------------------------------------

void test_s3()
{
    section("S^3: the shader's eye(), fog and depth against s3::eye and write_png");
    const double col[3] = {0.05, 0.15, 0.85};
    long kept = 0, same_keep = 0, near = 0, n = 0, colours = 0, colour_bad = 0;
    double worst_e = 0, worst_scale = 0;
    for (double zoom : ZOOMS)
        for (double clip : CLIPS)
            for (int view = 0; view < 6; view++) {
                const z::s3::ViewState v = random_view3(zoom, clip);
                const int W = 840;
                const draw::Uniforms u = draw::uniforms3(v, W, W);
                // the pixels of a unit, the PNG's: (w/2)/(clip/zoom) at the same size
                const double png = (W/2.0)/(v.clip/v.zoom);
                worst_scale = std::max(worst_scale, std::fabs(u.scale[0]*W/2.0 - png)/png);
                worst_scale = std::max(worst_scale, std::fabs(u.scale[1]*W/2.0 - png)/png);
                const draw::Uniforms wide = draw::uniforms3(v, 1200, 800);
                worst_scale = std::max(worst_scale, std::fabs(wide.scale[0]*600.0 - (400.0/(v.clip/v.zoom)))/png);
                worst_scale = std::max(worst_scale, std::fabs(wide.scale[1]*400.0 - (400.0/(v.clip/v.zoom)))/png);
                std::vector<std::pair<double, float>> zs;      // the painter's z and the shader's depth
                for (int i = 0; i < 400; i++) {
                    std::vector<float> a = random_point<4>(), b = a;
                    for (int k = 0; k < 4; k++) b[k] += (float)(0.02*rtest::gauss());
                    double e0[3], e1[3];
                    float f0[3], f1[3];
                    const bool p0 = z::s3::eye(v, a.data(), e0), p1 = z::s3::eye(v, b.data(), e1);
                    const bool s0 = sh_eye3(u, a.data(), f0), s1 = sh_eye3(u, b.data(), f1);
                    n++;
                    // a point within 1e-5 of the clip sphere may go either way in single precision
                    auto marginal = [&](const float* x) {
                        double y[4];
                        for (int r = 0; r < 4; r++) y[r] = v.Q[r][0]*x[0] + v.Q[r][1]*x[1] + v.Q[r][2]*x[2] + v.Q[r][3]*x[3];
                        const double den = 1 - y[3];
                        const double s2 = (y[0]*y[0] + y[1]*y[1] + y[2]*y[2])/(den*den);
                        return std::fabs(std::sqrt(s2) - v.clip) < 1e-5*v.clip + 1e-6 || den < 1e-6;
                    };
                    if (p0 == s0) same_keep++;
                    else if (marginal(a.data())) near++;
                    if (!(p0 && p1 && s0 && s1)) continue;
                    kept++;
                    for (int k = 0; k < 3; k++)
                        worst_e = std::max({worst_e, std::fabs(e0[k] - f0[k]), std::fabs(e1[k] - f1[k])});
                    const double zp = 0.5*(e0[2] + e1[2]);
                    const double fp = 0.12 + 0.88*std::max(0.0, std::min(1.0, (zp + v.clip)/(2*v.clip)));
                    const float zf = 0.5f*(f0[2] + f1[2]);
                    const float ff = 0.12f + 0.88f*clamp01((zf + u.clip)/(2.0f*u.clip));
                    for (int c = 0; c < 3; c++) {
                        colours++;
                        const long pc = std::lround(255*(1 - fp + fp*col[c]));
                        const long sc = std::lround(255*((1.0f - ff) + ff*(float)col[c]));
                        if (std::labs(pc - sc) > 1) colour_bad++;
                    }
                    zs.push_back({zp, draw::depth3(zf, u.clip)});
                }
                // the depth: the painter draws the larger z later, on top; the depth test keeps the smaller depth
                long order_bad = 0, range_bad = 0;
                for (size_t i = 0; i < zs.size(); i++) {
                    if (zs[i].second < -1 || zs[i].second > 1) range_bad++;
                    for (size_t j = i + 1; j < zs.size(); j++) {
                        if (std::fabs(zs[i].first - zs[j].first) < 1e-4) continue;
                        if ((zs[i].first < zs[j].first) != (zs[j].second < zs[i].second)) order_bad++;
                    }
                }
                check(order_bad == 0 && range_bad == 0, "zoom %g, clip %g, view %d: the depth orders the %zu segments "
                      "as the painter's sort, in [-1, 1] (%ld out of order, %ld out of range)", zoom, clip, view,
                      zs.size(), order_bad, range_bad);
            }
    check(same_keep + near == n && near <= n/1000 + 2, "the points kept: %ld of %ld as eye(), %ld within 1e-5 of the "
          "clip sphere", same_keep, n, near);
    check(kept > 1000, "segments with both ends kept: %ld", kept);
    check(worst_e < 1e-4, "the eye coordinates of the shader within 1e-4 of eye()'s (worst %.3g)", worst_e);
    check(worst_scale < 1e-6, "the pixels of a unit: the PNG's at the same size, and the shorter side's in a window "
          "1200 x 800 (worst relative error %.3g)", worst_scale);
    check(colour_bad == 0, "the fog: %ld colour levels within 1 of the painter's (%ld more)", colours, colour_bad);
}

// ---- S^4 ------------------------------------------------------------------------------------------------------------

void test_s4()
{
    section("S^4: the shader's eye5(), the two fogs and the depth z + 10 fh against s4::eye5 and write_png5");
    const double col[3] = {0.85, 0.08, 0.08};
    long kept = 0, same_keep = 0, near = 0, n = 0, colours = 0, colour_bad = 0;
    double worst_e = 0, worst_h = 0, worst_scale = 0;
    for (double zoom : ZOOMS)
        for (double clip : CLIPS)
            for (int view = 0; view < 6; view++) {
                const z::s4::View5 v = random_view4(zoom, clip);
                const int W = 840;
                const draw::Uniforms u = draw::uniforms4(v, W, W);
                const double png = (W/2.0)/(v.clip/v.zoom);
                worst_scale = std::max({worst_scale, std::fabs(u.scale[0]*W/2.0 - png)/png,
                                        std::fabs(u.scale[1]*W/2.0 - png)/png});
                const draw::Uniforms tall = draw::uniforms4(v, 700, 1000);
                worst_scale = std::max({worst_scale, std::fabs(tall.scale[0]*350.0 - 350.0/(v.clip/v.zoom))/png,
                                        std::fabs(tall.scale[1]*500.0 - 350.0/(v.clip/v.zoom))/png});
                std::vector<std::pair<double, float>> keys;
                for (int i = 0; i < 400; i++) {
                    std::vector<float> a = random_point<5>(), b = a;
                    for (int k = 0; k < 5; k++) b[k] += (float)(0.02*rtest::gauss());
                    double e0[3], e1[3], h0, h1;
                    float f0[3], f1[3], g0, g1;
                    const bool p0 = z::s4::eye5(v, a.data(), e0, h0), p1 = z::s4::eye5(v, b.data(), e1, h1);
                    const bool s0 = sh_eye5(u, a.data(), f0, g0), s1 = sh_eye5(u, b.data(), f1, g1);
                    n++;
                    auto marginal = [&](const float* x) {
                        double y[5];
                        for (int r = 0; r < 5; r++) {
                            y[r] = 0;
                            for (int k = 0; k < 5; k++) y[r] += v.Q[r][k]*x[k];
                        }
                        const double den = 1 - y[4];
                        const double s2 = (y[0]*y[0] + y[1]*y[1] + y[2]*y[2] + y[3]*y[3])/(den*den);
                        return std::fabs(std::sqrt(s2) - v.clip) < 1e-5*v.clip + 1e-6 || den < 1e-6;
                    };
                    if (p0 == s0) same_keep++;
                    else if (marginal(a.data())) near++;
                    if (!(p0 && p1 && s0 && s1)) continue;
                    kept++;
                    for (int k = 0; k < 3; k++)
                        worst_e = std::max({worst_e, std::fabs(e0[k] - f0[k]), std::fabs(e1[k] - f1[k])});
                    worst_h = std::max({worst_h, std::fabs(h0 - g0), std::fabs(h1 - g1)});
                    const double zp = 0.5*(e0[2] + e1[2]), hp = 0.5*(h0 + h1);
                    const double fzp = 0.15 + 0.85*std::max(0.0, std::min(1.0, (zp + v.clip)/(2*v.clip)));
                    const double fhp = 0.12 + 0.88*std::exp(-(hp/v.hidfog)*(hp/v.hidfog));
                    const double fp = fzp*fhp;
                    const float zf = 0.5f*(f0[2] + f1[2]), hf = 0.5f*(g0 + g1);
                    const float fzf = 0.15f + 0.85f*clamp01((zf + u.clip)/(2.0f*u.clip));
                    const float fhf = 0.12f + 0.88f*std::exp(-(hf/u.hidfog)*(hf/u.hidfog));
                    const float ff = fzf*fhf;
                    for (int c = 0; c < 3; c++) {
                        colours++;
                        const long pc = std::lround(255*(1 - fp + fp*col[c]));
                        const long sc = std::lround(255*((1.0f - ff) + ff*(float)col[c]));
                        if (std::labs(pc - sc) > 1) colour_bad++;
                    }
                    keys.push_back({zp + 10*fhp, draw::depth4(zf + 10.0f*fhf, u.clip)});
                }
                long order_bad = 0, range_bad = 0;
                for (size_t i = 0; i < keys.size(); i++) {
                    if (keys[i].second <= -1 || keys[i].second >= 1) range_bad++;    // (clamped would tie)
                    for (size_t j = i + 1; j < keys.size(); j++) {
                        if (std::fabs(keys[i].first - keys[j].first) < 1e-4) continue;
                        if ((keys[i].first < keys[j].first) != (keys[j].second < keys[i].second)) order_bad++;
                    }
                }
                check(order_bad == 0 && range_bad == 0, "zoom %g, clip %g, view %d: the depth orders the %zu segments "
                      "as the painter's sort key z + 10 fh, inside (-1, 1) (%ld out of order, %ld at the bounds)", zoom,
                      clip, view, keys.size(), order_bad, range_bad);
            }
    // the extremes of the sort key, z in [-clip, clip] and fh in [0.12, 1], are inside (-1, 1)
    bool inside = true;
    for (double clip : CLIPS)
        for (double key : {-clip + 1.2, clip + 10.0})
            inside = inside && std::fabs(draw::depth4((float)key, (float)clip)) < 0.99f;
    check(inside, "the depth of the sort key's extremes, -clip + 1.2 and clip + 10, inside (-0.99, 0.99)");
    check(same_keep + near == n && near <= n/1000 + 2, "the points kept: %ld of %ld as eye5(), %ld within 1e-5 of the "
          "clip sphere", same_keep, n, near);
    check(kept > 1000, "segments with both ends kept: %ld", kept);
    check(worst_e < 1e-4 && worst_h < 1e-4, "the eye and hidden coordinates within 1e-4 of eye5()'s (worst %.3g, %.3g)",
          worst_e, worst_h);
    check(worst_scale < 1e-6, "the pixels of a unit: the PNG's at the same size, and the shorter side's in a window "
          "700 x 1000 (worst relative error %.3g)", worst_scale);
    check(colour_bad == 0, "the fogs: %ld colour levels within 1 of the painter's (%ld more)", colours, colour_bad);
}

// ---- S^2 ------------------------------------------------------------------------------------------------------------

void test_s2()
{
    section("S^2: the front hemisphere and the position of a point, against the painter and p's framing");
    long n = 0, same_front = 0, near = 0;
    double worst = 0;
    for (double zoom : {0.5, 1.0, 2.25, 30.0})
        for (int view = 0; view < 10; view++) {
            const z::s2::View v = z::s2::make_view(rtest::uniform(-180, 180), rtest::uniform(-90, 90),
                                                   rtest::uniform(-180, 180));
            const int W = 800;
            const draw::Uniforms u = draw::uniforms2(v, zoom, W, W);
            int size;
            double pz;
            z::s2::export_framing(W, zoom, 1 << 15, 1e9, size, pz);
            const int SS = 2, w = size*SS;
            const double cx = w/2.0, cy = w/2.0, rad = w/2.0 - 2.0*SS;
            for (int i = 0; i < 300; i++) {
                std::vector<float> x = random_point<3>();
                double X[3];
                for (int r = 0; r < 3; r++) X[r] = v.m[3*r]*x[0] + v.m[3*r + 1]*x[1] + v.m[3*r + 2]*x[2];
                float e[3];
                mat3v(u.rot, x.data(), e);
                n++;
                if ((X[2] > 0) == (e[2] > 0)) same_front++;
                else if (std::fabs(X[2]) < 1e-6) near++;
                // the PNG's pixel (before rounding), as a fraction of its side, and the window's
                const double px = (cx + X[0]*pz*rad)/w, py = (cy - X[1]*pz*rad)/w;
                const double qx = (e[0]*u.scale[0] + 1)/2, qy = 1 - (e[1]*u.scale[1] + 1)/2;
                worst = std::max({worst, std::fabs(px - qx)/zoom, std::fabs(py - qy)/zoom});
            }
        }
    check(same_front + near == n, "the front hemisphere (Z > 0): %ld of %ld as the painter's, %ld within 1e-6 of Z = 0",
          same_front, n, near);
    check(worst < 1e-6, "a point's place in the window, as a fraction of its side, is its place in p's PNG (worst "
          "error / zoom %.3g)", worst);
    const draw::Uniforms wide = draw::uniforms2(z::s2::make_view(0, 0), 1, 1000, 600);
    check(std::fabs(wide.scale[0]*500 - 300/1.1) < 1e-3 && std::fabs(wide.scale[1]*300 - 300/1.1) < 1e-3,
          "in a window 1000 x 600 the shorter side shows a half-width of 1.1/zoom, the same in x and y");
}

// ---- the instances of a layer -------------------------------------------------------------------------------------

void test_ranges()
{
    section("the instances of a layer: the segments drawn are the painter's");
    z::polylines::Layer L;
    const int sizes[] = {1, 3, 2, 4, 1, 1, 5, 2};
    uint32_t at = 0;
    for (int s : sizes) {
        L.first.push_back(at);
        at += (uint32_t)s;
    }
    const int dim = 4;
    L.pts.assign((size_t)at*dim, 0.5f);
    const std::vector<float> e = draw::ends(L, dim);
    long bad = 0, cases = 0;
    for (size_t first = 0; first <= L.npoly(); first++)
        for (size_t end = first; end <= L.npoly() + 2; end++) {
            cases++;
            std::set<std::pair<size_t, size_t>> want, got;
            for (size_t j = first; j < std::min(end, L.npoly()); j++) {
                const size_t b = L.first[j], t = j + 1 < L.npoly() ? L.first[j + 1] : at;
                for (size_t i = b; i + 1 < t; i++) want.insert({i, i + 1});
            }
            const draw::Range r = draw::range(L, dim, first, end);
            for (size_t k = 0; k < r.segments; k++) {
                const size_t i = r.point + k;
                if (i + 1 >= at) {
                    bad++;
                    continue;
                }
                if (e[i] == 0) got.insert({i, i + 1});
            }
            if (got != want) bad++;
        }
    check(e.size() == at && e.back() == 1, "one end flag a point, the last point's set");
    check(bad == 0, "%ld ranges of polylines (polylines of 1 to 5 points): the segments drawn are the consecutive "
          "points of each (%ld differ)", cases, bad);
}

// ---- the overlays -------------------------------------------------------------------------------------------------

void test_overlays()
{
    section("the silhouette circle and the crosses of the marks");
    for (double zoom : {0.1, 1.0, 1.5, 3.0, 1e4}) {
        const int w = 800, h = 600;
        const std::vector<draw::Seg2> c = draw::circle2(zoom, w, h);
        const double rad = zoom*0.5*h/1.1;
        double worst = 0;
        bool inside = true;
        for (const draw::Seg2& s : c) {
            worst = std::max({worst, std::fabs(std::hypot(s.x0, s.y0) - rad)/rad, std::fabs(std::hypot(s.x1, s.y1) - rad)/rad});
            inside = inside && !(std::max(s.x0, s.x1) < -w/2.0 - 2 || std::min(s.x0, s.x1) > w/2.0 + 2 ||
                                 std::max(s.y0, s.y1) < -h/2.0 - 2 || std::min(s.y0, s.y1) > h/2.0 + 2);
        }
        const bool in_view = rad <= 0.5*std::hypot(w, h) + 4;
        check(c.empty() != in_view && worst < 1e-6 && inside, "zoom %g: the circle of radius zoom (%.1f pixels) %s, %zu "
              "pieces in the framebuffer", zoom, rad, in_view ? "in view" : "out of view", c.size());
    }
    // the marks: a point at the centre of the picture, one at random, and one outside the clip ball (|s| = 7)
    z::s3::ViewState v = random_view3(2, 3);
    z::polylines::File f;
    f.sphere = 3;
    std::vector<float> outside(4), centre(4), off = random_point<4>();
    for (int k = 0; k < 4; k++) {
        outside[k] = (float)(0.28*v.Q[0][k] + 0.96*v.Q[3][k]);   // y = Q x = (0.28, 0, 0, 0.96)
        centre[k] = (float)-v.Q[3][k];
    }
    auto line = [](const char* name, const std::vector<float>& p) {
        std::string s = name;
        for (float x : p) s += " " + z::polylines::exact(x);
        return s;
    };
    f.add("mark", line("t+", centre));
    f.add("mark", line("t-", off));
    f.add("mark", line("out", outside));
    f.add("mark", "bad 1 2");
    const std::vector<std::vector<float>> pts = draw::mark_points(f);
    check(pts.size() == 3, "the header's marks with 4 numbers: %zu", pts.size());
    const int W = 900;
    const std::vector<draw::Seg2> m = draw::marks3(v, pts, W, W, 8);
    double e[3];
    const bool off_kept = z::s3::eye(v, off.data(), e);
    check(m.size() == (off_kept ? 4u : 2u), "a cross for each mark that eye() keeps: %zu segments", m.size());
    if (m.size() >= 2)
        check(std::fabs(m[0].x0 + 8) <= 0.5 && std::fabs(m[0].x1 - 8) <= 0.5 && std::fabs(m[0].y0) <= 0.5 &&
                  std::fabs(m[1].y0 + 8) <= 0.5 && std::fabs(m[1].y1 - 8) <= 0.5,
              "t+ at the centre: a cross of half-length 8 at its pixel (%g %g %g %g)", m[0].x0, m[0].y0, m[0].x1, m[0].y1);
    if (off_kept && m.size() == 4) {
        const double s = (W/2.0)/(v.clip/v.zoom);
        const double cx = 0.5*(m[2].x0 + m[2].x1), cy = m[2].y0;
        check(std::fabs(cx - e[0]*s) <= 0.5 && std::fabs(cy - e[1]*s) <= 0.5, "the other mark at eye()'s pixel, "
              "within half a pixel (%g, %g against %g, %g)", cx, cy, e[0]*s, e[1]*s);
    }
}

}  // namespace

int main()
{
    printf("test_gui_draw: the window's drawing (apps/gui/draw.h): the shaders' formulas against the painters\n");
    test_s3();
    test_s4();
    test_s2();
    test_ranges();
    test_overlays();
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
