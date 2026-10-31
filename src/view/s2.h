// SPDX-License-Identifier: GPL-3.0-or-later
//
// s2.h: the views of S^2: the camera of the S^2 program and of a window on it. namespace zippers::s2.
//
// THE CAMERA. A view is a rotation of R^3 (View, row-major: (X, Y, Z) = m (x, y, z)); the eye looks along Z, the
// front hemisphere is Z > 0, and the picture is the orthographic projection (X, Y), magnified by the zoom.
//   View             the rotation
//   make_view        Rz(roll) Ry(yaw) Rx(pitch), from degrees: the view of the options --yaw, --pitch and --roll
//   mul3             the product of two rotations
//   rotx, roty       the rotation of R^3 about the screen's horizontal / vertical axis by an angle in degrees; a window
//                    turns its view about the screen-fixed axes by multiplying on the left (mul3(roty(d), v))
//   wrap180          an angle in degrees brought into (-180, 180]
//   view_angles      yaw, pitch and roll of a rotation: make_view of them is the rotation again (the smallest |roll| of
//                    the solutions; at yaw = +-90 degrees only pitch - roll or pitch + roll is determined)
//   fmt_num          the shortest %g form of a number (at most 17 digits) within a tolerance of it, with no exponent
//                    between 1e-4 and 1e7
//   command_angles   the yaw, pitch and roll of a rotation as the text of a command line: printed to 1e-12 degree when
//                    that rebuilds the rotation to within 1/1000 of a pixel of the picture, else exactly; false (with
//                    the error) when no text does
//   export_framing   the size and zoom of a PNG that frames the square of a window: the window shows a half-width of
//                    1.1/zoom on its shorter side, a PNG of S pixels S/(S - 4)/zoom; at least 1024 pixels
//   export_name      the file of a window's picture: the stem of -o (default "zipper"; an extension dropped) with .png,
//                    else stem_1.png, stem_2.png, ...: the first that does not exist, so that no file is overwritten
//   export_stem      the same rule for several files saved side by side (a picture and its session): the first of
//                    stem, stem_1, stem_2, ... for which no file stem + EXT exists, for any of the extensions EXT
//
// Moved from legacy/s2/zipper.cc, with their names: make_view and View from the program; mul3, rotx, roty, wrap180,
// view_angles and fmt_num from its window (namespace vw), and export_name from beside it; the statements of
// command_angles from the window's print_command, and those of export_framing from its key 'p'. export_stem is new. `static` is `inline`, `string` is `std::string`, and the
// window's variables of the two assembled functions are their parameters; every expression is the legacy one, in the
// same order (tests/unit/check_moved_text.py checks the text, tests/unit/test_view_s2.cc the values).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace zippers {
namespace s2 {

inline constexpr double PI = 3.14159265358979323846264338327950288;

struct View { double m[9]; };                    // rotation: (X,Y,Z) = m * (x,y,z)

// Rz(roll) * Ry(yaw) * Rx(pitch).  The eye looks along Z, so the hidden --roll turns the
// picture in its plane (counterclockwise); with it every orientation of the viewer has an
// exact command-line form (see view_angles).
inline View make_view(double yaw_deg, double pitch_deg, double roll_deg = 0.0) {
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

// 3x3 (row-major) helpers so arrow keys rotate about SCREEN-fixed axes
inline View mul3(const View& A, const View& B) {
    View C;
    for (int i=0;i<3;i++) for (int j=0;j<3;j++) {
        double s=0; for (int k=0;k<3;k++) s += A.m[i*3+k]*B.m[k*3+j];
        C.m[i*3+j]=s;
    }
    return C;
}
inline View rotx(double deg){ double t=deg*PI/180, c=std::cos(t), s=std::sin(t);
    return {{1,0,0, 0,c,-s, 0,s,c}}; }
inline View roty(double deg){ double t=deg*PI/180, c=std::cos(t), s=std::sin(t);
    return {{c,0,s, 0,1,0, -s,0,c}}; }

// The arrow keys compose rotations about screen-fixed axes, so a window's view is in general not
// Ry(yaw) Rx(pitch); it is Rz(roll) Ry(yaw) Rx(pitch) = make_view(yaw, pitch, roll).  Row
// 2 of that is (-sin y, cos y sin p, cos y cos p), which gives y and p; the roll is read
// off M A^T with A = make_view(y, p), accurate even where cos y ~ 0 (an error in p is
// then made up by r).  At yaw = +-90 only p - r (or p + r) is determined; near it the
// candidate with roll 0 (pitch from row 1, which is then (0, cos p, -sin p)) is also tried.
// Of the candidates that rebuild M to within 1e-15 of the best, and of the two solutions
// (y, p, r) and (180 - y, p + 180, r - 180), the one with the smallest |roll| is returned
// (roll 0 for a pure yaw/pitch view).
inline double wrap180(double a) { a = std::remainder(a, 360.0); return a == -180.0 ? 180.0 : a + 0.0; }
inline void view_angles(const View& M, double& y, double& p, double& r) {
    const double D = 180.0/PI, h = std::hypot(M.m[7], M.m[8]);
    const double y0 = std::atan2(-M.m[6], h)*D;
    double cand[4][4];                        // yaw, pitch, roll, error of the rebuilt M
    int nc = 0;
    for (int lock = 0; lock < (h < 1e-6 ? 2 : 1); ++lock)
        for (int k = 0; k < 2; ++k) {
            double p0 = (lock ? std::atan2(-M.m[5], M.m[4]) : std::atan2(M.m[7], M.m[8]))*D;
            double yk = wrap180(k ? 180.0 - y0 : y0), pk = wrap180(k ? p0 + 180.0 : p0);
            View A = make_view(yk, pk);
            double c = 0, s = 0;              // (M A^T)[0][0] = cos r, [1][0] = sin r
            for (int j = 0; j < 3; ++j) { c += M.m[j]*A.m[j]; s += M.m[3+j]*A.m[j]; }
            double rk = wrap180(std::atan2(s, c)*D), e = 0;
            View W = make_view(yk, pk, rk);
            for (int i = 0; i < 9; ++i) e = std::max(e, std::fabs(W.m[i] - M.m[i]));
            double* q = cand[nc++]; q[0] = yk; q[1] = pk; q[2] = rk; q[3] = e;
        }
    double emin = 1e9;
    for (int i = 0; i < nc; ++i) emin = std::min(emin, cand[i][3]);
    int b = -1;
    for (int i = 0; i < nc; ++i)
        if (cand[i][3] <= emin + 1e-15 && (b < 0 || std::fabs(cand[i][2]) < std::fabs(cand[b][2]))) b = i;
    y = cand[b][0]; p = cand[b][1]; r = cand[b][2];
}
// the shortest %g form of x (at most 17 digits) within tol of it (no exponent for
// 1e-4 <= |x| < 1e7)
inline std::string fmt_num(double x, double tol) {
    if (std::fabs(x) <= tol) return "0";
    const bool plain = std::fabs(x) >= 1e-4 && std::fabs(x) < 1e7;
    char b[40] = "";
    for (int d = 1; d <= 17; ++d) {
        snprintf(b, sizeof b, "%.*g", d, x);
        if (plain && strchr(b, 'e')) continue;
        if (std::fabs(strtod(b, nullptr) - x) <= tol) break;
    }
    return b;
}

// The yaw, pitch and roll of the rotation gview as the text of a command line (sy, sp, sr): to 1e-12 degree when
// make_view of that text is within 1/1000 of px (one pixel of the picture, in the units of the unit disk) of gview,
// else exactly. False when neither is (a rotation that has drifted from one); e is the error of the last text tried.
inline bool command_angles(const View& gview, double px, std::string& sy, std::string& sp, std::string& sr, double& e) {
    double y, p, r;
    e = 0;
    view_angles(gview, y, p, r);
    for (double t : {1e-12, 0.0}) {
        sy = fmt_num(y, t); sp = fmt_num(p, t); sr = fmt_num(r, t);
        View w = make_view(strtod(sy.c_str(), nullptr), strtod(sp.c_str(), nullptr), strtod(sr.c_str(), nullptr));
        e = 0;
        for (int i = 0; i < 9; ++i) e = std::max(e, std::fabs(w.m[i] - gview.m[i]));
        if (e <= 1e-3*px) break;
    }
    return e <= 1e-3*px;
}

// The size and zoom of a PNG that frames the square of a window of the given size and zoom, as the window's 'p' saves
// it: at least 1024 pixels, at most max_size, and the zoom rescaled (at most max_zoom). The window shows a half-width of
// 1.1/zoom on its shorter side, a PNG of S pixels S/(S-4)/zoom.
inline void export_framing(int window_size, double window_zoom, int max_size, double max_zoom, int& size, double& zoom) {
    size = window_size; if (size < 1024) size = 1024;
    zoom = window_zoom * (double)size / ((size - 4.0) * 1.1);
    size = std::min(size, max_size); zoom = std::min(zoom, max_zoom);
}

// The viewer's 'p' export file: the -o file's stem (default "zipper") with .png, never
// overwriting a file: stem.png, else stem_1.png, stem_2.png, ...
inline std::string export_name(const std::string& outfile) {
    std::string stem = outfile.empty() ? std::string("zipper") : outfile;
    size_t slash = stem.find_last_of('/'), dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > (slash == std::string::npos ? 0 : slash + 1)) stem.erase(dot);
    struct stat st;
    std::string fn = stem + ".png";
    for (int k = 1; stat(fn.c_str(), &st) == 0; ++k) fn = stem + "_" + std::to_string(k) + ".png";
    return fn;
}

// export_name's rule for files saved side by side: the stem of outfile (default "zipper"; an extension dropped as
// export_name drops it), else stem_1, stem_2, ...: the first for which no file stem + EXT exists, for every EXT of exts.
// With exts = {".png"}, export_stem(outfile, exts) + ".png" is export_name(outfile).
inline std::string export_stem(const std::string& outfile, const std::vector<std::string>& exts) {
    std::string stem = outfile.empty() ? std::string("zipper") : outfile;
    size_t slash = stem.find_last_of('/'), dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > (slash == std::string::npos ? 0 : slash + 1)) stem.erase(dot);
    auto taken = [&](const std::string& s) {
        struct stat st;
        for (const std::string& e : exts)
            if (stat((s + e).c_str(), &st) == 0) return true;
        return false;
    };
    std::string s = stem;
    for (int k = 1; taken(s); ++k) s = stem + "_" + std::to_string(k);
    return s;
}

}  // namespace s2
}  // namespace zippers
