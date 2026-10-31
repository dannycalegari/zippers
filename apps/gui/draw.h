// SPDX-License-Identifier: GPL-3.0-or-later
//
// draw.h: the window's drawing, in OpenGL 3.3 (core profile), with no GLFW: the curves that the model shows (model.h),
// drawn as the PNG painters of view/s2.h, s3.h and s4.h draw them. namespace zippers::gui::draw.
//
// THE BUFFERS. One vertex buffer for each layer of a set of curves: its points as the polyline file holds them (S^2 3
// floats, S^3 4, S^4 5), uploaded once for each set (the model's curves(), with its serial), and beside it one float a
// point, 1 at the last point of each polyline (a float rather than a byte: every attribute is 4-byte aligned).
// EACH SEGMENT IS AN INSTANCED QUAD: instance k reads the points k and k + 1 of the range drawn (two attributes on the
// same buffer, one point apart) and the end flag of point k; the vertex shader projects both ends with the view's uniforms,
// drops the segment unless both ends pass the painter's test and point k ends no polyline, and widens it to the layer's
// width in pixels, with square ends (half the width beyond each end). The colour and the depth are those of its middle,
// as the painters compute them:
//   S^2   (X, Y, Z) = rot x; the segment is drawn when Z > 0 at both ends (the front hemisphere), in the layer's
//         colour; no depth: the layers are drawn in order, as the painter draws them. Before them, the silhouette
//         circle (the unit circle, of radius zoom), in black.
//   S^3   y = Q x, den = 1 - y_3, s = (y_0, y_1, y_2)/den, kept when den > 1e-9 and |s|^2 <= clip^2 (s3::eye); e = rot s;
//         f = 0.12 + 0.88 clamp((z + clip)/(2 clip), 0, 1) with z the middle's e_2, the colour 1 - f + f col, and the
//         depth -z/clip (the nearer, the larger z, as the painter's sort: far first).
//   S^4   y = Q x (5 x 5, as 25 floats), den = 1 - y_4, s = (y_0, .., y_3)/den, kept when den > 1e-9 and |s|^2 <= clip^2;
//         u = M s with M = diag(rot, 1) U, e = (u_0, u_1, u_2), the hidden coordinate u_3 (s4::eye5); fz = 0.15 + 0.85
//         clamp((z + clip)/(2 clip), 0, 1), fh = 0.12 + 0.88 exp(-(h/hidfog)^2) at the middle, f = fz fh; the depth is
//         the painter's sort key z + 10 fh (from -clip + 1.2 to clip + 10), mapped to [-1, 1] over [-clip - 1, clip + 11].
// THE SCREEN. The picture's centre is the window's; the shorter side of the window shows what the picture shows across
// its width: S^2 a half-width of 1.1/zoom (the window that p frames: s2::export_framing), S^3 and S^4 clip/zoom.
// THE PRIORITIES. S^3's linked pair (the layers pair.Z+ and pair.Z-) is drawn over its context: the depth buffer is
// cleared between the two, as the painter sorts by priority first.
// THE MARKS t+ and t- (the header's "mark" lines; S^3 without pairs, S^4): crosses of half-length 4 points, drawn last,
// as the painters draw them (S^3 1 point wide, S^4 1.5).
// THE WIDTHS, in the window's points (pixels of the framebuffer = points * its scale): the lines 1 (S^3's pair 2.5, as
// the painter's brush 2 at 2x2 supersampling).
// THE FUNCTIONS of OpenGL are loaded through the function given (glfwGetProcAddress), the same code on every system.
//
//   CPU half, with no OpenGL (also for the tests):
//   Uniforms   the view as the shaders take it (column-major matrices)
//   uniforms2/3/4   from the views of the model, for a framebuffer of w x h pixels
//   depth3, depth4  the depth of a segment's middle, in [-1, 1] (smaller is nearer)
//   Range, range    the instances that draw polylines first .. end - 1 of a layer
//   ends            the end flag of each point: 1 at the last point of each polyline, else 0
//   Seg2, circle2, marks   the silhouette circle and the crosses, as segments in pixels from the centre (y up)
//   LINE_PT, PAIR_PT, MARK_PT   the widths and the half-length of the crosses, in points
//   Renderer   the OpenGL half: init, draw, grab

#pragma once

#include "model.h"

#include "output/polylines.h"
#include "view/s2.h"
#include "view/s3.h"
#include "view/s4.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zippers::gui::draw {

inline constexpr double LINE_PT = 1.0;       // a line's width, in points
inline constexpr double PAIR_PT = 2.5;       // the linked pair's
inline constexpr double MARK_PT = 4.0;       // the half-length of a cross

// the view as the shaders take it: matrices column-major, as glUniformMatrix*fv reads them without transposing
struct Uniforms {
    int sphere = 0;
    float rot[9] = {0};                      // S^2 the view, S^3 the rotation of R^3: rot[3 c + r] = m[r][c]
    float Q[25] = {0};                       // S^3: Q[4 c + r] = Q[r][c] (16); S^4: Q[5 r + c] = Q[r][c] (25)
    float M[16] = {0};                       // S^4: M[4 c + r] = (diag(rot, 1) U)[r][c]
    float scale[2] = {0, 0};                 // the screen's normalised coordinates of a unit of the picture plane
    float clip = 0, hidfog = 0;
};

Uniforms uniforms2(const s2::View& v, double zoom, int w, int h);
Uniforms uniforms3(const s3::ViewState& v, int w, int h);
Uniforms uniforms4(const s4::View5& v, int w, int h);

// the depth of a segment's middle in [-1, 1], smaller nearer: S^3 from its z, S^4 from the sort key z + 10 fh
inline float depth3(float z, float clip) { return -z/clip; }
inline float depth4(float key, float clip)
{
    float d = 1.0f - 2.0f*(key + clip + 1.0f)/(2.0f*clip + 12.0f);
    return d < -1.0f ? -1.0f : d > 1.0f ? 1.0f : d;
}

// the instances that draw the polylines first .. end - 1 of a layer: segment k joins the points point + k and
// point + k + 1 (dropped where point + k ends its polyline)
struct Range {
    size_t point = 0, segments = 0;
};
Range range(const polylines::Layer& L, int dim, size_t first, size_t end);
std::vector<float> ends(const polylines::Layer& L, int dim);

// a segment of the overlays, in pixels of the framebuffer from its centre, y up
struct Seg2 {
    float x0, y0, x1, y1;
};
// S^2's silhouette circle (radius zoom, in units of the unit disk) for a framebuffer of w x h pixels: empty when it
// lies wholly outside the picture, as the painter's test; pieces outside the framebuffer are left out
std::vector<Seg2> circle2(double zoom, int w, int h);
// the header's marks ("mark NAME x0 x1 ..."): their points
std::vector<std::vector<float>> mark_points(const polylines::File& f);
// the crosses of the marks that eye() (eye5()) keeps, of half-length `half` pixels
std::vector<Seg2> marks3(const s3::ViewState& v, const std::vector<std::vector<float>>& pts, int w, int h, double half);
std::vector<Seg2> marks4(const s4::View5& v, const std::vector<std::vector<float>>& pts, int w, int h, double half);

// the OpenGL half
using GetProc = void* (*)(const char* name);

class Renderer {
public:
    Renderer();
    ~Renderer();                             // with the context current
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // loads the functions and compiles the shaders; false, with the reason
    bool init(GetProc get, std::string& why);
    // the OpenGL version and the renderer's name
    std::string version() const;
    // draws the model's curves into the framebuffer of w x h pixels, `scale` pixels a point
    void draw(const Model& m, int w, int h, double scale);
    // reads the picture area back (the centred square of the shorter side) from the buffer drawn last: RGB rows, top
    // first
    bool grab(int w, int h, std::vector<uint8_t>& rgb, int& side);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

}  // namespace zippers::gui::draw
