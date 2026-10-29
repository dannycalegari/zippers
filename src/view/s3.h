// SPDX-License-Identifier: GPL-3.0-or-later
//
// s3.h: the views of S^3 and their PNG painter. namespace zippers::s3.
//
// THE CAMERA. S^3 is the unit sphere of C^2 = R^4, the visual sphere of the base point o. A frame Q of R^4 (rows
// E1, E2, E3, P) gives the stereographic projection from the pole P to R^3; then a rotation of R^3, an orthographic
// projection, a zoom and a clip ball.
//   ViewState         the view: Q, the rotation of R^3, zoom and clip
//   default_frame     the frame that puts a point V at the centre: the pole is -V, and the axes are W, iW, iV with W
//                     the unit vector complex-orthogonal to V (so the contact plane at V is horizontal and its Reeb
//                     direction iV vertical); E3 is reversed if need be, so that det Q = +1
//   rot3              the rotation of R^3: the Reeb axis E3 up the screen, then yaw about the vertical and pitch about
//                     the horizontal (degrees)
//   move_pole         move the pole toward a frame direction a (a unit 3-vector in E-coordinates) by an angle: the
//                     rotation of S^3 that moves the centre of the picture along a
//   eye               a point of S^3 -> eye coordinates; false outside the clip ball (or at the pole)
//   init_view         the default frame of t^+ (so that t^+ is at the centre), the pole moved by pole1 degrees along E1
//                     and then pole2 along E2, the rotation of R^3, the zoom and the clip
//   V3, stereo        points of R^3; the stereographic projection of points of S^3 from the pole of a frame
//   frame_with_pole   an orthonormal frame of R^4 with last row p (Gram-Schmidt on p, e_1, ..., e_4), with det +1:
//                     otherwise the projection to R^3 is a mirror image for some poles, and every linking number
//                     changes sign
// THE PAINTER. write_png: the polylines of renderers (render/renderer.h), one colour and priority per layer (Layer),
// as depth-cued segments (far lines fade toward the white background), and black crosses at marked points,
// supersampled 2x2, to a PNG file.
// THE WINDOW. window3: what write_png shows of S^3, pulled back to the sphere for the renderer's culling
// (render/cull.h): the points that eye() keeps and that land in the screen square.
//
// Moved from legacy/s3/zipcore.h (PI, ViewState, default_frame, rot3, move_pole, eye, init_view, V3, stereo,
// frame_with_pole, Layer, write_png), with their names, in namespace zippers::s3 (the S^4 view, view/s4.h, has another
// rot3 with the same signature). Every expression is the legacy one, in the same order (tests/unit/test_view_s3.cc).
// Changes of form:
//   - the points of S^3 are SpherePoint<4> (geometry/sphere.h), the legacy P4 (the same four doubles);
//   - init_view takes the World of the generic renderer (render/world.h) and the six numbers it read from the legacy
//     Options (yaw, pitch, pole1, pole2, zoom, clip), in place of the Options of the program; Layer holds a Renderer of
//     the generic renderer. Both are templates over the backend B, for any B whose sphere at infinity is S^3 (B::N = 4:
//     PU21 here; a real hyperbolic 4-manifold, O(4,1), would be another), as is write_png;
//   - write_png is assembled from src/output/ as the end of the header of raster.h shows: the projection and the fog
//     are the legacy statements; the sort and the lines are raster::paint_segments, the crosses raster::cross with
//     brush 0, the 2x2 filter raster::downsample_rgb, the file png::write_rgb (the legacy Seg, ras_line,
//     write_png_rgb and png_chunk). The same buffers are alive at the same times.
// A program that includes this header links zlib (output/png.h).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "geometry/sphere.h"
#include "output/png.h"
#include "output/raster.h"
#include "render/cull.h"
#include "render/renderer.h"
#include "render/world.h"

namespace zippers {
namespace s3 {

inline constexpr double PI = 3.14159265358979323846;

// ---------------------------------------------------------------------------------------
// Views: a 4D frame Q (rows E1, E2, E3, P; stereographic projection from P) followed by a
// rotation of R^3, an orthographic projection, a zoom, and a clip ball.

struct ViewState {
    double Q[4][4];
    double rot[9];
    double zoom = 1, clip = 3;
};

inline void default_frame(const SpherePoint<4>& V, double Q[4][4])
{
    typedef std::complex<double> cpx;
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
    std::memcpy(m, Q, sizeof m);
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

inline void rot3(double yaw, double pitch, double R[9])
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
inline void move_pole(double Q[4][4], const double a[3], double th)
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
inline bool eye(const ViewState& v, const float* x, double e[3])
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

// the view of a World: t^+ at the centre, the pole moved by pole1 and pole2 degrees (along E1, then E2), the rotation
// of R^3 by yaw and pitch, the zoom and the clip radius (legacy init_view, which read them from the Options)
template <class B>
inline void init_view(const World<B>& W, ViewState& v, double yaw, double pitch, double pole1, double pole2,
                      double zoom, double clip)
{
    static_assert(B::N == 4, "the views of S^3 are for backends whose sphere at infinity is S^3");
    default_frame(W.plus.V, v.Q);
    double e1[3] = {1,0,0}, e2[3] = {0,1,0};
    move_pole(v.Q, e1, pole1*PI/180);
    move_pole(v.Q, e2, pole2*PI/180);
    rot3(yaw, pitch, v.rot);
    v.zoom = zoom;
    v.clip = clip;
}

// the window of the view v on S^3 (render/cull.h), with the margin factor K. With y = Q x, the projection is
// s = (y_0, y_1, y_2)/(1 - y_3), eye() keeps x when |s| <= clip, and write_png draws the screen square |e_0|, |e_1| <=
// clip/zoom of e = rot s (a segment with both ends kept, and only its part in the square). Since 1 - y_3 > 0 off the
// pole, e_0 <= H (H = clip/zoom) is r_0 . (y_0, y_1, y_2) + H y_3 <= H, with r_0 the first row of rot: a half-space of
// R^4 whose boundary passes through the pole (y = (0, 0, 0, 1)); and |s| <= clip is y_3 <= (clip^2 - 1)/(clip^2 + 1),
// a cap about the centre of the picture (eye() also refuses 1 - y_3 <= 1e-9, which that cap excludes).
inline CullWindow<4> window3(const ViewState& v, double K = CULL_K)
{
    CullWindow<4> w;
    w.K = K;
    const double H = v.clip/v.zoom;
    for (int r = 0; r < 2; r++)                                     // e_r <= H and -e_r <= H
        for (double sg : {1.0, -1.0}) {
            double n[4];
            for (int k = 0; k < 4; k++)
                n[k] = sg*(v.rot[3*r]*v.Q[0][k] + v.rot[3*r+1]*v.Q[1][k] + v.rot[3*r+2]*v.Q[2][k]) + H*v.Q[3][k];
            w.add(n, H);
        }
    double p[4];
    for (int k = 0; k < 4; k++) p[k] = v.Q[3][k];
    w.add(p, (v.clip*v.clip - 1)/(v.clip*v.clip + 1));
    return w;
}

// ---------------------------------------------------------------------------------------
// Stereographic frames (used by the linking numbers and the linked pairs)

typedef std::array<double,3> V3;

// stereographic projection from the pole p (a frame E1, E2, E3, p of R^4)
inline std::vector<V3> stereo(const std::vector<SpherePoint<4>>& X, const double F[4][4])
{
    std::vector<V3> Y(X.size());
    for (size_t i = 0; i < X.size(); i++) {
        double y[4];
        for (int r = 0; r < 4; r++) y[r] = F[r][0]*X[i].x[0] + F[r][1]*X[i].x[1] + F[r][2]*X[i].x[2] + F[r][3]*X[i].x[3];
        double den = 1 - y[3];
        Y[i] = {y[0]/den, y[1]/den, y[2]/den};
    }
    return Y;
}

// an orthonormal frame of R^4 with last row p
inline void frame_with_pole(const double p[4], double F[4][4])
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
    std::memcpy(m, F, sizeof m);
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (std::abs(m[r][c]) > std::abs(m[p][c])) p = r;
        if (p != c) { for (int k = 0; k < 4; k++) std::swap(m[p][k], m[c][k]); det = -det; }
        det *= m[c][c];
        for (int r = c + 1; r < 4; r++) { double f = m[r][c]/m[c][c]; for (int k = c; k < 4; k++) m[r][k] -= f*m[c][k]; }
    }
    if (det < 0) for (int k = 0; k < 4; k++) F[2][k] = -F[2][k];
}

// ---------------------------------------------------------------------------------------
// PNG output (software rendering, depth-cued: far lines fade toward the white background)

// a renderer's polylines in one colour; lower prio is drawn first, underneath (legacy Layer)
template <class B>
struct Layer { const Renderer<B>* r; double col[3]; int prio = 1; };

template <class B>
inline bool write_png(const char* fn, int size, const std::vector<Layer<B>>& layers, const ViewState& v,
                      const std::vector<SpherePoint<4>>& marks, int thick = 0)
{
    static_assert(B::N == 4, "the views of S^3 are for backends whose sphere at infinity is S^3");
    const int SS = 2, w = size*SS, h = size*SS;
    std::vector<uint8_t> img((size_t)w*h*3, 255);
    double half = v.clip/v.zoom, scale = (w/2.0)/half;
    std::vector<raster::Segment> segs;
    for (const Layer<B>& L : layers) {
        const Renderer<B>& r = *L.r;
        for (size_t k = 0; k < r.npoly(); k++) {
            double e0[3], e1[3];
            bool ok0 = eye(v, &r.pts[4*r.pbegin(k)], e0);
            for (size_t i = r.pbegin(k) + 1; i < r.pend(k); i++) {
                bool ok1 = eye(v, &r.pts[4*i], e1);
                if (ok0 && ok1) {
                    double z = 0.5*(e0[2] + e1[2]);
                    double f = 0.12 + 0.88*std::max(0.0, std::min(1.0, (z + v.clip)/(2*v.clip)));
                    raster::Segment s = {(float)(w/2.0 + e0[0]*scale), (float)(h/2.0 - e0[1]*scale),
                                         (float)(w/2.0 + e1[0]*scale), (float)(h/2.0 - e1[1]*scale), (float)z, {0,0,0}, L.prio};
                    for (int c = 0; c < 3; c++) s.c[c] = (uint8_t)std::lround(255*(1 - f + f*L.col[c]));
                    segs.push_back(s);
                }
                std::memcpy(e0, e1, sizeof e0);
                ok0 = ok1;
            }
        }
    }
    raster::paint_segments(img, w, h, segs, thick);
    // marked points (the fixed points t^+-): small black crosses
    for (const SpherePoint<4>& p : marks) {
        float x[4] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3]};
        double e[3];
        if (!eye(v, x, e)) continue;
        int cx = (int)std::lround(w/2.0 + e[0]*scale), cy = (int)std::lround(h/2.0 - e[1]*scale);
        raster::cross(img, w, h, cx, cy, SS, 0);
    }
    return png::write_rgb(fn, raster::downsample_rgb(img, w, size, SS), size, size);
}

}  // namespace s3
}  // namespace zippers
