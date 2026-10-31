// SPDX-License-Identifier: GPL-3.0-or-later
//
// options.h: two options that the drawing programs zipper, zipper3 and zipper5 share. namespace zippers::viewopt.
//
//   parse_colour       the value of --colour LAYER=R,G,B: the layer's name and its colour, three numbers in [0, 1]
//                      (the colours of the painters: 0 0 0 black, 1 1 1 white)
//   orthonormal_error  the largest entry of |M M^T - I| for an n x n matrix M given row by row
//   determinant        the determinant of an n x n matrix given row by row (n <= 5): the sign of an orthonormal frame
//   VIEWMATRIX_TOL     the largest orthonormal_error that the frame and the rotation of --viewmatrix may have. The views
//                      of a window are products of rotations computed in double precision, whose error grows by about
//                      1e-16 a step, so 1e-9 leaves room for ten million steps; a number mistyped anywhere but in its
//                      last few digits gives an error far above it

#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace zippers::viewopt {

inline constexpr double VIEWMATRIX_TOL = 1e-9;

// NAME=R,G,B, with NAME not empty and R, G, B numbers in [0, 1] (as strtod reads them); false otherwise
inline bool parse_colour(const std::string& v, std::string& name, double rgb[3])
{
    size_t eq = v.find('=');
    if (eq == std::string::npos || eq == 0) return false;
    name = v.substr(0, eq);
    const char* p = v.c_str() + eq + 1;
    for (int c = 0; c < 3; c++) {
        char* end = nullptr;
        double x = strtod(p, &end);
        if (end == p || !(x >= 0 && x <= 1)) return false;
        rgb[c] = x;
        p = end;
        if (c < 2) {
            if (*p != ',') return false;
            p++;
        }
    }
    return *p == 0;
}

// the largest entry of |M M^T - I|, M an n x n matrix given row by row; NaN if an entry is not a number
inline double orthonormal_error(const double* m, int n)
{
    double e = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0;
            for (int k = 0; k < n; k++) s += m[i*n + k]*m[j*n + k];
            double d = std::fabs(s - (i == j ? 1.0 : 0.0));
            if (std::isnan(d)) return d;        // (no tolerance passes a NaN)
            if (d > e) e = d;
        }
    return e;
}

// the determinant of the n x n matrix m (row by row, n <= 5), by elimination with partial pivoting
inline double determinant(const double* m, int n)
{
    double a[25];
    for (int i = 0; i < n*n; i++) a[i] = m[i];
    double det = 1;
    for (int c = 0; c < n; c++) {
        int p = c;
        for (int r = c + 1; r < n; r++) if (std::fabs(a[r*n + c]) > std::fabs(a[p*n + c])) p = r;
        if (a[p*n + c] == 0) return 0;
        if (p != c) { for (int k = 0; k < n; k++) std::swap(a[c*n + k], a[p*n + k]); det = -det; }
        det *= a[c*n + c];
        for (int r = c + 1; r < n; r++) {
            double f = a[r*n + c]/a[c*n + c];
            for (int k = c; k < n; k++) a[r*n + k] -= f*a[c*n + k];
        }
    }
    return det;
}

}  // namespace zippers::viewopt
