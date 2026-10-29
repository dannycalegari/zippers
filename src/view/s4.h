// SPDX-License-Identifier: GPL-3.0-or-later
//
// s4.h: the views of S^4 and their PNG painter. namespace zippers::s4.
//
// THE CAMERA. S^4 is the unit sphere of R^5 (the boundary of the Klein model). A frame Q of R^5 (rows E1..E4, P) gives
// the stereographic projection from P to R^4; then a rotation U of R^4: the first three coordinates of U s are shown
// (with a rotation of R^3, an orthographic projection, a zoom and a clip ball), and the fourth, the HIDDEN coordinate,
// is drawn as a second fog.
//   View5               the view: Q, U, the rotation of R^3, zoom, clip and the hidden fog's scale
//   orthonormal_frame   an orthonormal frame of R^5 with last row p (Gram-Schmidt on p, e_1, ..., e_5)
//   rot3                the rotation of R^3: yaw about the vertical, then pitch about the horizontal (degrees)
//   turn4               U <- G U, G the rotation of R^4 in the plane of coordinates (i, j)
//   move_pole5          move the pole P toward the frame direction E_a: a rotation of S^4
//   eye5                a point of S^4 -> eye coordinates and the hidden coordinate; false outside the clip ball
//   init_view5          the pole opposite t^+, so that t^+ is at the centre of the picture
// THE PAINTER. write_png5: the polylines of renderers (render/renderer.h), one colour and priority per layer, as
// depth-cued segments (far lines fade toward the white background, and so do lines far out along the hidden
// direction), and black crosses at marked points, supersampled 2x2, to a PNG file.
// THE WINDOW. window5: what write_png5 shows of S^4, pulled back to the sphere for the renderer's culling
// (render/cull.h): the points that eye5() keeps and that land in the screen square.
//
// Moved from legacy/s4/zipcore5.h (View5, orthonormal_frame, rot3, turn4, move_pole5, eye5, init_view5, PI) and
// legacy/s4/zipper5.cc (Layer5, write_png5), with their names, in namespace zippers::s4 (the S^3 view has its own
// rot3). Every expression is the legacy one, in the same order (tests/unit/test_render_imm.cc). Changes of form:
//   - init_view5 takes the World of the generic renderer (render/world.h), and Layer5 its Renderer, in place of the
//     legacy World5 and Renderer5;
//   - write_png5 is assembled from src/output/ as the end of the header of raster.h shows: the projection, the two
//     fogs and the depth z + 10 fh are the legacy statements; the sort and the lines are raster::paint_segments, the
//     crosses raster::cross with brush 1, the 2x2 filter raster::downsample_rgb, the file png::write_rgb (the legacy
//     ras_line, write_png_rgb and png_chunk).
// A program that includes this header links zlib (output/png.h).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "geometry/on1/on1.h"
#include "geometry/sphere.h"
#include "output/png.h"
#include "output/raster.h"
#include "render/cull.h"
#include "render/renderer.h"
#include "render/world.h"

namespace zippers {
namespace s4 {

inline constexpr double PI = 3.14159265358979323846;

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

inline void orthonormal_frame(const double p[5], double F[5][5])        // rows E1..E4, then p
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

inline void rot3(double yaw, double pitch, double R[9])
{
    double a = yaw*PI/180, b = pitch*PI/180;
    double Y[9] = {std::cos(a),0,std::sin(a), 0,1,0, -std::sin(a),0,std::cos(a)};
    double X[9] = {1,0,0, 0,std::cos(b),-std::sin(b), 0,std::sin(b),std::cos(b)};
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { R[3*i+j] = 0; for (int k = 0; k < 3; k++) R[3*i+j] += X[3*i+k]*Y[3*k+j]; }
}

// rotate R^4 in the plane of coordinates (i, j) by th: U <- G U
inline void turn4(double U[4][4], int i, int j, double th)
{
    double c = std::cos(th), s = std::sin(th);
    for (int k = 0; k < 4; k++) {
        double a = U[i][k], b = U[j][k];
        U[i][k] = c*a - s*b;
        U[j][k] = s*a + c*b;
    }
}

// move the pole P toward the frame direction E_a (a = 0..3) by th: a rotation of S^4
inline void move_pole5(double Q[5][5], int a, double th)
{
    double c = std::cos(th), s = std::sin(th);
    for (int k = 0; k < 5; k++) {
        double e = Q[a][k], p = Q[4][k];
        Q[a][k] = c*e - s*p;
        Q[4][k] = s*e + c*p;
    }
}

// a point of S^4 -> eye coordinates (3) and the hidden coordinate; false outside the clip ball
inline bool eye5(const View5& v, const float* x, double e[3], double& hid)
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

// the window of the view v on S^4 (render/cull.h), with the margin factor K. With y = Q x, the projection is
// s = (y_0, ..., y_3)/(1 - y_4), eye5() keeps x when |s| <= clip, and write_png5 draws the screen square |e_0|, |e_1|
// <= clip/zoom of e = rot (U s)_{0,1,2} (a segment with both ends kept, and only its part in the square; the depth e_2
// and the hidden coordinate (U s)_3 are not limited). So e_r = w_r . s with w_r = sum_i rot[r][i] U[i] (a unit vector
// of R^4), and as in S^3 (view/s3.h: window3) e_r <= H (H = clip/zoom) is w_r . (y_0..y_3) + H y_4 <= H, a half-space
// of R^5 whose boundary passes through the pole; |s| <= clip is y_4 <= (clip^2 - 1)/(clip^2 + 1).
inline CullWindow<5> window5(const View5& v, double K = CULL_K)
{
    CullWindow<5> w;
    w.K = K;
    const double H = v.clip/v.zoom;
    for (int r = 0; r < 2; r++) {
        double wr[4];
        for (int k = 0; k < 4; k++) wr[k] = v.rot[3*r]*v.U[0][k] + v.rot[3*r+1]*v.U[1][k] + v.rot[3*r+2]*v.U[2][k];
        for (double sg : {1.0, -1.0}) {                             // e_r <= H and -e_r <= H
            double n[5];
            for (int m = 0; m < 5; m++) {
                double s = 0;
                for (int k = 0; k < 4; k++) s += wr[k]*v.Q[k][m];
                n[m] = sg*s + H*v.Q[4][m];
            }
            w.add(n, H);
        }
    }
    double p[5];
    for (int m = 0; m < 5; m++) p[m] = v.Q[4][m];
    w.add(p, (v.clip*v.clip - 1)/(v.clip*v.clip + 1));
    return w;
}

inline void init_view5(const World<ON1<5>>& W, View5& v, double yaw = 0, double pitch = 15)
{
    // the pole opposite t^+: t^+ is at the centre of the picture
    double p[5];
    for (int k = 0; k < 5; k++) p[k] = -W.plus.V.x[k];
    orthonormal_frame(p, v.Q);
    std::memset(v.U, 0, sizeof v.U);
    for (int i = 0; i < 4; i++) v.U[i][i] = 1;
    rot3(yaw, pitch, v.rot);
}

// ---------------------------------------------------------------------------------------
// PNG output (software rendering): far lines fade toward white, and so do lines far out along
// the hidden direction

// a renderer's polylines in one colour; lower priority is drawn first, underneath (legacy Layer5)
struct Layer5 { const Renderer<ON1<5>>* r; double col[3]; int prio; };

inline bool write_png5(const char* fn, int size, const std::vector<Layer5>& layers, const View5& v, const std::vector<SpherePoint<5>>& marks, int thick)
{
    const int SS = 2, w = size*SS, h = size*SS;
    std::vector<uint8_t> img((size_t)w*h*3, 255);
    double half = v.clip/v.zoom, scale = (w/2.0)/half;
    std::vector<raster::Segment> segs;
    for (const Layer5& L : layers) {
        const Renderer<ON1<5>>& r = *L.r;
        for (size_t k = 0; k < r.npoly(); k++) {
            double e0[3], e1[3], h0, h1;
            bool ok0 = eye5(v, &r.pts[5*r.pbegin(k)], e0, h0);
            for (size_t i = r.pbegin(k) + 1; i < r.pend(k); i++) {
                bool ok1 = eye5(v, &r.pts[5*i], e1, h1);
                if (ok0 && ok1) {
                    double z = 0.5*(e0[2] + e1[2]), hm = 0.5*(h0 + h1);
                    double fz = 0.15 + 0.85*std::max(0.0, std::min(1.0, (z + v.clip)/(2*v.clip)));
                    double fh = 0.12 + 0.88*std::exp(-(hm/v.hidfog)*(hm/v.hidfog));
                    double f = fz*fh;
                    raster::Segment s = {(float)(w/2.0 + e0[0]*scale), (float)(h/2.0 - e0[1]*scale),
                                         (float)(w/2.0 + e1[0]*scale), (float)(h/2.0 - e1[1]*scale), (float)(z + 10*fh), {0,0,0}, L.prio};
                    for (int c = 0; c < 3; c++) s.c[c] = (uint8_t)std::lround(255*(1 - f + f*L.col[c]));
                    segs.push_back(s);
                }
                std::memcpy(e0, e1, sizeof e0);
                h0 = h1;
                ok0 = ok1;
            }
        }
    }
    raster::paint_segments(img, w, h, segs, thick);
    for (const SpherePoint<5>& p : marks) {
        float x[5] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3], (float)p.x[4]};
        double e[3], hh;
        if (!eye5(v, x, e, hh)) continue;
        int cx = (int)std::lround(w/2.0 + e[0]*scale), cy = (int)std::lround(h/2.0 - e[1]*scale);
        raster::cross(img, w, h, cx, cy, SS, 1);
    }
    return png::write_rgb(fn, raster::downsample_rgb(img, w, size, SS), size, size);
}

}  // namespace s4
}  // namespace zippers
