// SPDX-License-Identifier: GPL-3.0-or-later
//
// draw.cc: the window's drawing (draw.h). It includes no header of OpenGL: the few types, constants and functions it
// uses are declared here, with the values of the OpenGL 3.3 specification, and the functions are loaded at run time
// through the function given to Renderer::init.

#include "draw.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zippers::gui::draw {

// ---- the CPU half -------------------------------------------------------------------------------------------------

namespace {

// the normalised coordinates of a unit of the picture plane, when the shorter side shows a half-width `half`
void screen_scale(double half, int w, int h, float scale[2])
{
    const double m = std::min(w, h) > 0 ? std::min(w, h) : 1;
    scale[0] = (float)(m/(w > 0 ? w : 1)/half);
    scale[1] = (float)(m/(h > 0 ? h : 1)/half);
}

}  // namespace

Uniforms uniforms2(const s2::View& v, double zoom, int w, int h)
{
    Uniforms u;
    u.sphere = 2;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) u.rot[3*c + r] = (float)v.m[3*r + c];
    screen_scale(1.1/zoom, w, h, u.scale);
    return u;
}

Uniforms uniforms3(const s3::ViewState& v, int w, int h)
{
    Uniforms u;
    u.sphere = 3;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) u.rot[3*c + r] = (float)v.rot[3*r + c];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) u.Q[4*c + r] = (float)v.Q[r][c];
    u.clip = (float)v.clip;
    screen_scale(v.clip/v.zoom, w, h, u.scale);
    return u;
}

Uniforms uniforms4(const s4::View5& v, int w, int h)
{
    Uniforms u;
    u.sphere = 4;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++) u.Q[5*r + c] = (float)v.Q[r][c];
    double M[4][4];                          // diag(rot, 1) U
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 3; r++) M[r][c] = v.rot[3*r]*v.U[0][c] + v.rot[3*r + 1]*v.U[1][c] + v.rot[3*r + 2]*v.U[2][c];
        M[3][c] = v.U[3][c];
    }
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) u.M[4*c + r] = (float)M[r][c];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) u.rot[3*c + r] = (float)v.rot[3*r + c];
    u.clip = (float)v.clip;
    u.hidfog = (float)v.hidfog;
    screen_scale(v.clip/v.zoom, w, h, u.scale);
    return u;
}

Range range(const polylines::Layer& L, int dim, size_t first, size_t end)
{
    Range r;
    const size_t npts = dim > 0 ? L.pts.size()/(size_t)dim : 0;
    end = std::min(end, L.npoly());
    if (first >= end) return r;
    const size_t p0 = L.first[first], p1 = end < L.npoly() ? L.first[end] : npts;
    if (p1 < p0 + 2) return r;
    r.point = p0;
    r.segments = p1 - p0 - 1;
    return r;
}

std::vector<float> ends(const polylines::Layer& L, int dim)
{
    const size_t npts = dim > 0 ? L.pts.size()/(size_t)dim : 0;
    std::vector<float> e(npts, 0);
    for (size_t k = 0; k + 1 < L.npoly(); k++)
        if (L.first[k + 1] > 0 && L.first[k + 1] <= npts) e[L.first[k + 1] - 1] = 1;
    if (npts) e[npts - 1] = 1;
    return e;
}

std::vector<Seg2> circle2(double zoom, int w, int h)
{
    std::vector<Seg2> v;
    const double rad = zoom*0.5*std::min(w, h)/1.1;   // pixels
    if (!(rad > 0) || rad > 0.5*std::hypot((double)w, (double)h) + 4) return v;
    const int nc = (int)std::min((double)(1 << 22), std::max(512.0, rad*0.5));
    const double hw = 0.5*w + 2, hh = 0.5*h + 2;
    for (int i = 0; i < nc; i++) {
        const double a0 = 2*s2::PI*i/nc, a1 = 2*s2::PI*(i + 1)/nc;
        const double x0 = rad*std::cos(a0), y0 = rad*std::sin(a0), x1 = rad*std::cos(a1), y1 = rad*std::sin(a1);
        if (std::max(x0, x1) < -hw || std::min(x0, x1) > hw || std::max(y0, y1) < -hh || std::min(y0, y1) > hh)
            continue;
        v.push_back({(float)x0, (float)y0, (float)x1, (float)y1});
    }
    return v;
}

std::vector<std::vector<float>> mark_points(const polylines::File& f)
{
    std::vector<std::vector<float>> v;
    for (const std::string& m : f.values("mark")) {
        const size_t sp = m.find(' ');
        std::vector<double> x;
        if (sp == std::string::npos || !polylines::parse_list(m.substr(sp + 1), x) || (int)x.size() != f.dim()) continue;
        std::vector<float> p(x.size());
        for (size_t i = 0; i < x.size(); i++) p[i] = (float)x[i];
        v.push_back(p);
    }
    return v;
}

namespace {

void cross(std::vector<Seg2>& v, double ex, double ey, double sx, double sy, int w, int h, double half)
{
    // the painter's pixel: the nearest pixel of the screen point (here of the framebuffer, from its centre)
    const double cx = std::round(0.5*w + ex*sx*0.5*w) - 0.5*w, cy = std::round(0.5*h + ey*sy*0.5*h) - 0.5*h;
    v.push_back({(float)(cx - half), (float)cy, (float)(cx + half), (float)cy});
    v.push_back({(float)cx, (float)(cy - half), (float)cx, (float)(cy + half)});
}

}  // namespace

std::vector<Seg2> marks3(const s3::ViewState& v, const std::vector<std::vector<float>>& pts, int w, int h, double half)
{
    std::vector<Seg2> s;
    const Uniforms u = uniforms3(v, w, h);
    for (const std::vector<float>& p : pts) {
        double e[3];
        if (p.size() != 4 || !s3::eye(v, p.data(), e)) continue;
        cross(s, e[0], e[1], u.scale[0], u.scale[1], w, h, half);
    }
    return s;
}

std::vector<Seg2> marks4(const s4::View5& v, const std::vector<std::vector<float>>& pts, int w, int h, double half)
{
    std::vector<Seg2> s;
    const Uniforms u = uniforms4(v, w, h);
    for (const std::vector<float>& p : pts) {
        double e[3], hid;
        if (p.size() != 5 || !s4::eye5(v, p.data(), e, hid)) continue;
        cross(s, e[0], e[1], u.scale[0], u.scale[1], w, h, half);
    }
    return s;
}

// ---- OpenGL: the types, constants and functions used ---------------------------------------------------------------

namespace {

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef double GLdouble;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef std::ptrdiff_t GLsizeiptr;

constexpr GLenum COLOR_BUFFER_BIT = 0x4000, DEPTH_BUFFER_BIT = 0x0100, DEPTH_TEST = 0x0B71, LEQUAL = 0x0203,
                 ARRAY_BUFFER = 0x8892, STATIC_DRAW = 0x88E4, STREAM_DRAW = 0x88E0, FLOAT = 0x1406,
                 UNSIGNED_BYTE = 0x1401, TRIANGLE_STRIP = 0x0005, VERTEX_SHADER = 0x8B31, FRAGMENT_SHADER = 0x8B30,
                 COMPILE_STATUS = 0x8B81, LINK_STATUS = 0x8B82, VERSION = 0x1F02, RENDERER = 0x1F01, RGB = 0x1907,
                 PACK_ALIGNMENT = 0x0D05, BACK = 0x0405, MULTISAMPLE = 0x809D, BLEND = 0x0BE2;
constexpr GLboolean FALSE_ = 0;

// a byte offset into the buffer bound, as glVertexAttribPointer takes it
inline const void* offset(uintptr_t n) { return reinterpret_cast<const void*>(n); }

struct GL {
#define ZGL(ret, name, args) ret (*name) args = nullptr;
#include "draw_gl.inc"
#undef ZGL
};

// ---- the shaders ----------------------------------------------------------------------------------------------------

const char* const COMMON = R"(
uniform vec2 halfpx;    // half the framebuffer, in pixels
uniform float hw;       // half the line's width, in pixels
uniform vec3 col;       // the layer's colour
flat out vec3 vcol;
// a segment that is not drawn: its four corners at one point outside the screen
void drop()
{
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    vcol = vec3(1.0);
}
// the quad of the segment n0 n1 (normalised coordinates), hw pixels on each side and beyond each end
void quad(vec2 n0, vec2 n1, float depth)
{
    vec2 p0 = n0*halfpx, p1 = n1*halfpx, d = p1 - p0;
    float L = length(d);
    d = L > 1e-6 ? d/L : vec2(1.0, 0.0);
    vec2 nrm = vec2(-d.y, d.x);
    bool at_end = (gl_VertexID & 1) == 1;
    float side = (gl_VertexID & 2) == 2 ? 1.0 : -1.0;
    vec2 p = (at_end ? p1 + d*hw : p0 - d*hw) + nrm*(side*hw);
    gl_Position = vec4(p/halfpx, depth, 1.0);
}
)";

const char* const VS2 = R"(
layout(location = 0) in vec3 a0;
layout(location = 1) in vec3 a1;
layout(location = 4) in float last;
uniform mat3 rot;
uniform vec2 scale;
void main()
{
    vec3 e0 = rot*a0, e1 = rot*a1;
    if (last > 0.5 || !(e0.z > 0.0 && e1.z > 0.0)) { drop(); return; }
    vcol = col;
    quad(e0.xy*scale, e1.xy*scale, 0.0);
}
)";

const char* const VS3 = R"(
layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 a1;
layout(location = 4) in float last;
uniform mat4 Q;
uniform mat3 rot;
uniform float clip;
uniform vec2 scale;
bool eye(vec4 x, out vec3 e)
{
    e = vec3(0.0);
    vec4 y = Q*x;
    float den = 1.0 - y.w;
    if (den <= 1e-9) return false;
    vec3 s = y.xyz/den;
    if (dot(s, s) > clip*clip) return false;
    e = rot*s;
    return true;
}
void main()
{
    vec3 e0, e1;
    bool k0 = eye(a0, e0), k1 = eye(a1, e1);
    if (last > 0.5 || !k0 || !k1) { drop(); return; }
    float z = 0.5*(e0.z + e1.z);
    float f = 0.12 + 0.88*clamp((z + clip)/(2.0*clip), 0.0, 1.0);
    vcol = vec3(1.0 - f) + f*col;
    quad(e0.xy*scale, e1.xy*scale, -z/clip);
}
)";

const char* const VS4 = R"(
layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 a1;
layout(location = 2) in float a0w;
layout(location = 3) in float a1w;
layout(location = 4) in float last;
uniform float Q[25];
uniform mat4 M;
uniform float clip, hidfog;
uniform vec2 scale;
bool eye5(vec4 x4, float x5, out vec3 e, out float hid)
{
    e = vec3(0.0);
    hid = 0.0;
    float x[5] = float[5](x4.x, x4.y, x4.z, x4.w, x5);
    float y[5];
    for (int r = 0; r < 5; r++) {
        float s = 0.0;
        for (int k = 0; k < 5; k++) s += Q[5*r + k]*x[k];
        y[r] = s;
    }
    float den = 1.0 - y[4];
    if (den <= 1e-9) return false;
    vec4 s4 = vec4(y[0], y[1], y[2], y[3])/den;
    if (dot(s4, s4) > clip*clip) return false;
    vec4 u = M*s4;
    e = u.xyz;
    hid = u.w;
    return true;
}
void main()
{
    vec3 e0, e1;
    float h0, h1;
    bool k0 = eye5(a0, a0w, e0, h0), k1 = eye5(a1, a1w, e1, h1);
    if (last > 0.5 || !k0 || !k1) { drop(); return; }
    float z = 0.5*(e0.z + e1.z), hm = 0.5*(h0 + h1);
    float fz = 0.15 + 0.85*clamp((z + clip)/(2.0*clip), 0.0, 1.0);
    float fh = 0.12 + 0.88*exp(-(hm/hidfog)*(hm/hidfog));
    float f = fz*fh;
    vcol = vec3(1.0 - f) + f*col;
    float key = z + 10.0*fh;
    quad(e0.xy*scale, e1.xy*scale, clamp(1.0 - 2.0*(key + clip + 1.0)/(2.0*clip + 12.0), -1.0, 1.0));
}
)";

// the overlays: segments given in pixels from the centre
const char* const VSO = R"(
layout(location = 0) in vec4 seg;
void main()
{
    vcol = col;
    quad(seg.xy/halfpx, seg.zw/halfpx, 0.0);
}
)";

const char* const FS = R"(#version 330 core
flat in vec3 vcol;
out vec4 frag;
void main()
{
    frag = vec4(vcol, 1.0);
}
)";

struct Program {
    GLuint id = 0;
    GLint halfpx = -1, hw = -1, col = -1, rot = -1, scale = -1, Q = -1, M = -1, clip = -1, hidfog = -1;
};

struct GpuLayer {
    std::string name;
    GLuint pts = 0, ends = 0;
};

}  // namespace

struct Renderer::Impl {
    GL gl;
    bool ready = false;
    Program prog[5];                         // 2, 3, 4: the curves of S^2, S^3, S^4; 0: the overlays
    GLuint vao = 0, overlay = 0;
    const polylines::File* file = nullptr;   // the curves uploaded, and their serial
    uint64_t serial = 0;
    std::vector<GpuLayer> layers;

    bool compile(GLenum kind, const std::string& src, GLuint& sh, std::string& why);
    bool link(int k, const char* vs, std::string& why);
    void free_layers();
    void upload(const Model& m);
    void layer(int dim, const GpuLayer& g, const polylines::Layer& L, size_t first, size_t end);
    void overlays(const std::vector<Seg2>& s, float hw, const float col[3], int w, int h);
};

bool Renderer::Impl::compile(GLenum kind, const std::string& src, GLuint& sh, std::string& why)
{
    sh = gl.CreateShader(kind);
    const GLchar* s = src.c_str();
    gl.ShaderSource(sh, 1, &s, nullptr);
    gl.CompileShader(sh);
    GLint ok = 0;
    gl.GetShaderiv(sh, COMPILE_STATUS, &ok);
    if (ok) return true;
    char log[2000] = "";
    gl.GetShaderInfoLog(sh, sizeof log, nullptr, log);
    why = std::string("a shader does not compile: ") + log;
    gl.DeleteShader(sh);
    sh = 0;
    return false;
}

bool Renderer::Impl::link(int k, const char* vs, std::string& why)
{
    GLuint v = 0, f = 0;
    if (!compile(VERTEX_SHADER, std::string("#version 330 core\n") + COMMON + vs, v, why)) return false;
    if (!compile(FRAGMENT_SHADER, FS, f, why)) {
        gl.DeleteShader(v);
        return false;
    }
    Program& P = prog[k];
    P.id = gl.CreateProgram();
    gl.AttachShader(P.id, v);
    gl.AttachShader(P.id, f);
    gl.LinkProgram(P.id);
    gl.DeleteShader(v);
    gl.DeleteShader(f);
    GLint ok = 0;
    gl.GetProgramiv(P.id, LINK_STATUS, &ok);
    if (!ok) {
        char log[2000] = "";
        gl.GetProgramInfoLog(P.id, sizeof log, nullptr, log);
        why = std::string("the shaders do not link: ") + log;
        return false;
    }
    P.halfpx = gl.GetUniformLocation(P.id, "halfpx");
    P.hw = gl.GetUniformLocation(P.id, "hw");
    P.col = gl.GetUniformLocation(P.id, "col");
    P.rot = gl.GetUniformLocation(P.id, "rot");
    P.scale = gl.GetUniformLocation(P.id, "scale");
    P.Q = gl.GetUniformLocation(P.id, "Q");
    P.M = gl.GetUniformLocation(P.id, "M");
    P.clip = gl.GetUniformLocation(P.id, "clip");
    P.hidfog = gl.GetUniformLocation(P.id, "hidfog");
    return true;
}

void Renderer::Impl::free_layers()
{
    for (GpuLayer& g : layers) {
        gl.DeleteBuffers(1, &g.pts);
        gl.DeleteBuffers(1, &g.ends);
    }
    layers.clear();
    file = nullptr;
    serial = 0;
}

void Renderer::Impl::upload(const Model& m)
{
    const polylines::File* f = m.curves();
    if (f == file && m.curves_serial() == serial) return;
    free_layers();
    if (!f) return;
    for (const polylines::Layer& L : f->layers) {
        GpuLayer g;
        g.name = L.name;
        gl.GenBuffers(1, &g.pts);
        gl.BindBuffer(ARRAY_BUFFER, g.pts);
        gl.BufferData(ARRAY_BUFFER, (GLsizeiptr)(L.pts.size()*sizeof(float)), L.pts.data(), STATIC_DRAW);
        const std::vector<float> e = ends(L, f->dim());
        gl.GenBuffers(1, &g.ends);
        gl.BindBuffer(ARRAY_BUFFER, g.ends);
        gl.BufferData(ARRAY_BUFFER, (GLsizeiptr)(e.size()*sizeof(float)), e.data(), STATIC_DRAW);
        layers.push_back(g);
    }
    file = f;
    serial = m.curves_serial();
}

void Renderer::Impl::layer(int dim, const GpuLayer& g, const polylines::Layer& L, size_t first,
                           size_t end)
{
    const Range r = range(L, dim, first, end);
    if (!r.segments) return;
    const size_t p0 = r.point;
    const GLsizei n = (GLsizei)r.segments;
    const GLsizei stride = (GLsizei)(dim*sizeof(float));
    const uintptr_t base = p0*(size_t)stride;
    gl.BindVertexArray(vao);
    gl.BindBuffer(ARRAY_BUFFER, g.pts);
    const GLint head = dim < 4 ? dim : 4;
    gl.VertexAttribPointer(0, head, FLOAT, FALSE_, stride, offset(base));
    gl.VertexAttribPointer(1, head, FLOAT, FALSE_, stride, offset(base + stride));
    gl.EnableVertexAttribArray(0);
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribDivisor(0, 1);
    gl.VertexAttribDivisor(1, 1);
    if (dim == 5) {
        gl.VertexAttribPointer(2, 1, FLOAT, FALSE_, stride, offset(base + 4*sizeof(float)));
        gl.VertexAttribPointer(3, 1, FLOAT, FALSE_, stride, offset(base + stride + 4*sizeof(float)));
        gl.EnableVertexAttribArray(2);
        gl.EnableVertexAttribArray(3);
        gl.VertexAttribDivisor(2, 1);
        gl.VertexAttribDivisor(3, 1);
    }
    gl.BindBuffer(ARRAY_BUFFER, g.ends);
    gl.VertexAttribPointer(4, 1, FLOAT, FALSE_, sizeof(float), offset(p0*sizeof(float)));
    gl.EnableVertexAttribArray(4);
    gl.VertexAttribDivisor(4, 1);
    gl.DrawArraysInstanced(TRIANGLE_STRIP, 0, 4, n);
    for (GLuint a = 0; a < 5; a++) gl.DisableVertexAttribArray(a);
}

void Renderer::Impl::overlays(const std::vector<Seg2>& s, float hw, const float col[3], int w, int h)
{
    if (s.empty()) return;
    const Program& P = prog[0];
    gl.UseProgram(P.id);
    gl.Uniform2f(P.halfpx, 0.5f*w, 0.5f*h);
    gl.Uniform1f(P.hw, hw);
    gl.Uniform3f(P.col, col[0], col[1], col[2]);
    gl.BindVertexArray(vao);
    gl.BindBuffer(ARRAY_BUFFER, overlay);
    gl.BufferData(ARRAY_BUFFER, (GLsizeiptr)(s.size()*sizeof(Seg2)), s.data(), STREAM_DRAW);
    gl.VertexAttribPointer(0, 4, FLOAT, FALSE_, sizeof(Seg2), nullptr);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribDivisor(0, 1);
    gl.DrawArraysInstanced(TRIANGLE_STRIP, 0, 4, (GLsizei)s.size());
    gl.DisableVertexAttribArray(0);
}

Renderer::Renderer() : p_(new Impl) {}

Renderer::~Renderer()
{
    if (!p_->ready) return;
    p_->free_layers();
    for (Program& P : p_->prog)
        if (P.id) p_->gl.DeleteProgram(P.id);
    p_->gl.DeleteBuffers(1, &p_->overlay);
    p_->gl.DeleteVertexArrays(1, &p_->vao);
}

bool Renderer::init(GetProc get, std::string& why)
{
    GL& gl = p_->gl;
    std::string missing;
#define ZGL(ret, name, args)                                                     \
    gl.name = reinterpret_cast<ret(*) args>(get("gl" #name));                    \
    if (!gl.name) missing += std::string(missing.empty() ? "" : ", ") + "gl" #name;
#include "draw_gl.inc"
#undef ZGL
    if (!missing.empty()) {
        why = "OpenGL lacks " + missing;
        return false;
    }
    if (!p_->link(2, VS2, why) || !p_->link(3, VS3, why) || !p_->link(4, VS4, why) || !p_->link(0, VSO, why))
        return false;
    gl.GenVertexArrays(1, &p_->vao);
    gl.GenBuffers(1, &p_->overlay);
    p_->ready = true;
    return true;
}

std::string Renderer::version() const
{
    if (!p_->ready) return "";
    const GLubyte* v = p_->gl.GetString(VERSION);
    const GLubyte* r = p_->gl.GetString(RENDERER);
    return std::string(v ? (const char*)v : "?") + ", " + (r ? (const char*)r : "?");
}

void Renderer::draw(const Model& m, int w, int h, double scale)
{
    Impl& I = *p_;
    GL& gl = I.gl;
    if (!I.ready || w <= 0 || h <= 0) return;
    gl.Viewport(0, 0, w, h);
    gl.ClearColor(1, 1, 1, 1);
    gl.ClearDepth(1.0);
    gl.Disable(BLEND);
    gl.Enable(MULTISAMPLE);
    gl.Clear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);
    I.upload(m);
    const polylines::File* f = m.curves();
    if (!f || f != I.file) return;
    const int S = m.sphere();
    Uniforms u = S == 2 ? uniforms2(m.view2(), m.zoom(), w, h) : S == 3 ? uniforms3(m.view3(), w, h)
                                                                        : uniforms4(m.view4(), w, h);
    const float black[3] = {0, 0, 0};
    if (S == 2) {
        gl.Disable(DEPTH_TEST);
        I.overlays(circle2(m.zoom(), w, h), (float)(0.5*LINE_PT*scale), black, w, h);
    } else {
        gl.Enable(DEPTH_TEST);
        gl.DepthFunc(LEQUAL);
    }
    const Program& P = I.prog[S];
    const std::vector<Shown> shown = m.shown();
    // the priorities: the layers of S^3's pair last, over the others, with the depth buffer cleared between them
    for (int prio = 0; prio < 2; prio++) {
        bool any = false;
        for (const Shown& s : shown) {
            const bool pair = s.layer.compare(0, 5, "pair.") == 0;
            if ((int)pair != prio) continue;
            const GpuLayer* g = nullptr;
            for (const GpuLayer& x : I.layers)
                if (x.name == s.layer) g = &x;
            const polylines::Layer* L = f->layer(s.layer);
            if (!g || !L) continue;
            if (!any && prio == 1 && S != 2) gl.Clear(DEPTH_BUFFER_BIT);
            any = true;
            gl.UseProgram(P.id);
            gl.Uniform2f(P.halfpx, 0.5f*w, 0.5f*h);
            gl.Uniform1f(P.hw, (float)(0.5*(pair ? PAIR_PT : LINE_PT)*scale));
            gl.Uniform3f(P.col, (float)s.rgb[0], (float)s.rgb[1], (float)s.rgb[2]);
            gl.Uniform2f(P.scale, u.scale[0], u.scale[1]);
            if (S == 2 || S == 3) gl.UniformMatrix3fv(P.rot, 1, FALSE_, u.rot);
            if (S == 3) gl.UniformMatrix4fv(P.Q, 1, FALSE_, u.Q);
            if (S == 4) {
                gl.Uniform1fv(P.Q, 25, u.Q);
                gl.UniformMatrix4fv(P.M, 1, FALSE_, u.M);
                gl.Uniform1f(P.hidfog, u.hidfog);
            }
            if (S != 2) gl.Uniform1f(P.clip, u.clip);
            I.layer(f->dim(), *g, *L, s.first, s.end);
        }
    }
    // the marks, last (S^3 without its pairs, S^4)
    gl.Disable(DEPTH_TEST);
    if (S == 3 && !m.pairs())
        I.overlays(marks3(m.view3(), mark_points(*f), w, h, MARK_PT*scale), (float)(0.5*1.0*scale), black, w, h);
    if (S == 4)
        I.overlays(marks4(m.view4(), mark_points(*f), w, h, MARK_PT*scale), (float)(0.5*1.5*scale), black, w, h);
}

bool Renderer::grab(int w, int h, std::vector<uint8_t>& rgb, int& side)
{
    Impl& I = *p_;
    if (!I.ready || w <= 0 || h <= 0) return false;
    side = std::min(w, h);
    const int x0 = (w - side)/2, y0 = (h - side)/2;
    std::vector<uint8_t> raw((size_t)side*side*3);
    I.gl.ReadBuffer(BACK);
    I.gl.PixelStorei(PACK_ALIGNMENT, 1);
    I.gl.ReadPixels(x0, y0, side, side, RGB, UNSIGNED_BYTE, raw.data());
    rgb.resize(raw.size());
    const size_t row = (size_t)side*3;
    for (int y = 0; y < side; y++) memcpy(&rgb[(size_t)y*row], &raw[(size_t)(side - 1 - y)*row], row);
    return true;
}

}  // namespace zippers::gui::draw
