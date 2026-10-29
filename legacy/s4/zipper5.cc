/*  zipper5.cc  --  draw approximations to the zippers Z+ and Z- on the 4-sphere at infinity of
    the fibered hyperbolic 5-manifold of Italiano-Martelli-Migliorini (rh5.h, zipcore5.h).

    Usage:  zipper5 [options] -o FILE.png       a still
      -f FILE      substitution file from `chz5 kernel` (default subst5_2.30_4.50_t1.txt)
      -R R         roots: the elements g of Gamma with d(o, g o) <= R (default 0: the base arcs)
      -a ACC       accuracy: maximal chord on the unit S^4 (default 0.02)
      -s SIZE      image size (default 840)
      --clip RHO   clip R^4 to the ball of radius RHO (default 3)
      --zoom F     magnify (default 1)
      --yaw D --pitch D   rotate R^3 (degrees)
      --turn D     turn the hidden direction: rotate R^4 in the plane of the third visible
                   axis and the hidden one (degrees)
      --turn2 D    the same in the plane of the first visible axis and the hidden one
      --hidfog H   the hidden coordinate's fog scale (default 1.5)
      --only +|-   draw only Z+ or only Z-
      --thick N    thicker lines

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "zipcore5.h"

struct Opt5 {
    string subst = "subst5_2.30_4.50_t1.txt";
    double R = 0, acc = 0.02, clip = 3, zoom = 1, yaw = 0, pitch = 15, turn = 0, turn2 = 0, hidfog = 1.5;
    int size = 840, which = 3, thick = 0;
    size_t cap = 20000000;
    string out;
};

static void ras_line(vector<uint8_t>& img, int W, int H, int x0, int y0, int x1, int y1, const uint8_t c[3], int th = 0)
{
    int dx = std::abs(x1-x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1-y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        for (int ey = -th; ey <= th; ey++) for (int ex = -th; ex <= th; ex++) {
            int x = x0 + ex, y = y0 + ey;
            if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
                uint8_t* p = &img[((size_t)y*W + x)*3];
                p[0] = c[0]; p[1] = c[1]; p[2] = c[2];
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

struct Layer5 { const Renderer5* r; double col[3]; int prio; };

static bool write_png5(const char* fn, int size, const vector<Layer5>& layers, const View5& v, const vector<P5>& marks, int thick)
{
    const int SS = 2, w = size*SS, h = size*SS;
    vector<uint8_t> img((size_t)w*h*3, 255);
    double half = v.clip/v.zoom, scale = (w/2.0)/half;
    struct Seg { float x0, y0, x1, y1, z; uint8_t c[3]; int prio; };
    vector<Seg> segs;
    for (const Layer5& L : layers) {
        const Renderer5& r = *L.r;
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
                    Seg s = {(float)(w/2.0 + e0[0]*scale), (float)(h/2.0 - e0[1]*scale),
                             (float)(w/2.0 + e1[0]*scale), (float)(h/2.0 - e1[1]*scale), (float)(z + 10*fh), {0,0,0}, L.prio};
                    for (int c = 0; c < 3; c++) s.c[c] = (uint8_t)std::lround(255*(1 - f + f*L.col[c]));
                    segs.push_back(s);
                }
                memcpy(e0, e1, sizeof e0);
                h0 = h1;
                ok0 = ok1;
            }
        }
    }
    std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b) { return a.prio != b.prio ? a.prio < b.prio : a.z < b.z; });
    for (const Seg& s : segs)
        ras_line(img, w, h, (int)std::lround(s.x0), (int)std::lround(s.y0), (int)std::lround(s.x1), (int)std::lround(s.y1), s.c, thick);
    for (const P5& p : marks) {
        float x[5] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3], (float)p.x[4]};
        double e[3], hh;
        if (!eye5(v, x, e, hh)) continue;
        int cx = (int)std::lround(w/2.0 + e[0]*scale), cy = (int)std::lround(h/2.0 - e[1]*scale);
        const uint8_t blk[3] = {0,0,0};
        ras_line(img, w, h, cx - 4*SS, cy, cx + 4*SS, cy, blk, 1);
        ras_line(img, w, h, cx, cy - 4*SS, cx, cy + 4*SS, blk, 1);
    }
    vector<uint8_t> out((size_t)size*size*3);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) for (int c = 0; c < 3; ++c) {
        int sum = 0;
        for (int dy = 0; dy < SS; ++dy) for (int dx = 0; dx < SS; ++dx) sum += img[(((size_t)(y*SS+dy))*w + (x*SS+dx))*3 + c];
        out[((size_t)y*size + x)*3 + c] = (uint8_t)(sum/(SS*SS));
    }
    return write_png_rgb(fn, out, size, size);
}

int main(int argc, char** argv)
{
    rh5_setup();
    Opt5 o;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return argv[++i]; };
        if (a == "-f") o.subst = next();
        else if (a == "-R") o.R = atof(next().c_str());
        else if (a == "-a") o.acc = atof(next().c_str());
        else if (a == "-o") o.out = next();
        else if (a == "-s") o.size = atoi(next().c_str());
        else if (a == "--clip") o.clip = atof(next().c_str());
        else if (a == "--zoom") o.zoom = atof(next().c_str());
        else if (a == "--yaw") o.yaw = atof(next().c_str());
        else if (a == "--pitch") o.pitch = atof(next().c_str());
        else if (a == "--turn") o.turn = atof(next().c_str());
        else if (a == "--turn2") o.turn2 = atof(next().c_str());
        else if (a == "--hidfog") o.hidfog = atof(next().c_str());
        else if (a == "--only") { string w = next(); o.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--thick") o.thick = atoi(next().c_str());
        else if (a == "--cap") o.cap = (size_t)atof(next().c_str());
        else { fprintf(stderr, "unknown option %s (see the comment at the top of zipper5.cc)\n", a.c_str()); return 1; }
    }
    World5 W;
    prepare5(o.subst.c_str(), W);
    build5(W, o.R, o.acc, o.cap, o.which);
    if (o.out.empty()) return 0;
    View5 v;
    init_view5(W, v, o.yaw, o.pitch);
    v.clip = o.clip; v.zoom = o.zoom; v.hidfog = o.hidfog;
    turn4(v.U, 2, 3, o.turn*PI/180);
    turn4(v.U, 0, 3, o.turn2*PI/180);
    vector<Layer5> layers;
    if (o.which & 1) layers.push_back({W.rp, {0.05, 0.15, 0.85}, 1});
    if (o.which & 2) layers.push_back({W.rn, {0.85, 0.08, 0.08}, 1});
    vector<P5> marks = {W.plus.V, W.minus.V};
    if (!write_png5(o.out.c_str(), o.size, layers, v, marks, o.thick)) { fprintf(stderr, "cannot write %s\n", o.out.c_str()); return 1; }
    fprintf(stderr, "wrote %s\n", o.out.c_str());
    return 0;
}
