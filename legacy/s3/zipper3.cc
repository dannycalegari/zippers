/*  zipper3.cc  --  draw approximations to the zippers Z+ and Z- on the 3-sphere at infinity
    of the Cartwright-Steger surface, for phi = the first coordinate of H_1 (see README.md).

    THE ARCS.  The block-filling search (chz) gives a finite generating set S of K = ker phi
    and an element t with phi(t) > 0, together with the roofs t^-1 s t = x_1 ... x_k (x_i in S)
    for every s in S.  Let V = t^+ be the attracting fixed point of t on S^3.  The arc alpha_s
    of Z+ from V to sV is the limit of the staircase chains t^n u o, u running over the
    prefixes of theta^n(s), and it is self-similar:
        alpha_s = t ( alpha_{x_1} u x_1 alpha_{x_2} u ... u x_1...x_{k-1} alpha_{x_k} ).
    So a BRANCH is a pair (h, s), the arc h.alpha_s from hV to hsV, and splitting it once gives
    the consecutive sub-branches (h t u_i, x_i), u_i = x_1 ... x_{i-1}.  Z- is the same with
    t^-1 and the down-roofs.  For s^-1 the roof is taken to be the inverse word of the roof of
    s, so that (hs, s^-1) is exactly the reversal of (h, s); a branch then has the canonical
    key (h, s) for s in S+ (one of each inverse pair), and (hs, s^-1) otherwise.

    THE APPROXIMATION is as in the S^2 program.  The roots are the branches (g, s), s in S+, of
    the elements g of the hyperbolic ball d(o, g o) <= R, nearest first.  Each branch is split
    until its chord on S^3 is below the accuracy, with a one-level look-ahead for spikes.
    Group elements are exact (Z[zeta_12]); a branch is identified by a 64-bit fingerprint of
    its exact key, and is drawn once.

    THE PICTURE.  S^3 is the unit sphere of C^2 = R^4, the visual sphere of the basepoint o.
    It is shown by stereographic projection from a pole P, clipped to a ball.  The default
    frame puts t^+ (=: V) at the centre: the pole is -V, and the axes are W, iW, iV with W the
    unit vector complex-orthogonal to V, so the contact plane at V is horizontal and its Reeb
    direction iV vertical.  Output: an interactive OpenGL viewer (-v) or a PNG (-o).

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "zipcore.h"
#include <deque>
#ifdef ZIPPER3_VIEWER
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif
#endif

// ---------------------------------------------------------------------------------------
// Interactive viewer

// ---------------------------------------------------------------------------------------
// Linked pairs (written by `link3 --export`): the loop gamma_w of Z+ and the translate
// h.delta_w' of a loop of Z-, with their linking number

struct LinkedPair {
    long lk = 0;
    vector<int> w, w2;
    int phi = 0;
    string hword;
    Renderer *a = nullptr, *b = nullptr;       // the two loops, closed polylines
    double pole[4] = {0,0,0,1};                // a projection pole far from both
    double clip = 3;
};

static bool load_pairs(const string& fn, vector<LinkedPair>& P)
{
    std::ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot open %s\n", fn.c_str()); return false; }
    string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        vector<string> f;                      // lk +1 | w 4 17 23 | w' 1 28 24 20 | phi(h) 1 | h a1
        std::stringstream ss(line);
        string part;
        while (std::getline(ss, part, '|')) f.push_back(part);
        if (f.size() != 5) continue;
        LinkedPair p;
        sscanf(f[0].c_str(), " lk %ld", &p.lk);
        auto ints = [](const string& x) { std::istringstream is(x); string tag; is >> tag; vector<int> v; int k; while (is >> k) v.push_back(k); return v; };
        p.w = ints(f[1]);
        p.w2 = ints(f[2]);
        sscanf(f[3].c_str(), " phi(h) %d", &p.phi);
        p.hword = f[4].substr(f[4].find('h') + 1);
        P.push_back(p);
    }
    fprintf(stderr, "%s: %zu linked pairs\n", fn.c_str(), P.size());
    return !P.empty();
}

static void build_pair(LinkedPair& p, const World& W, double acc)
{
    delete p.a; delete p.b;
    size_t forced = 0;
    vector<P4> A = loop_points(W.plus, W.L, mid(), p.w, acc, forced);
    vector<P4> B = loop_points(W.minus, W.L, parse_word(p.hword), p.w2, acc, forced);
    p.a = new Renderer(W.plus, W.L, acc, 1);
    p.b = new Renderer(W.minus, W.L, acc, 1);
    for (auto* pr : {p.a, p.b}) {
        const vector<P4>& X = pr == p.a ? A : B;
        pr->starts.push_back(0);
        for (const P4& q : X) pr->push(q);
        pr->push(X[0]);
    }
    std::mt19937_64 rng(7);
    far_pole(A, B, rng, p.pole);
    double F[4][4], rmax = 0;
    frame_with_pole(p.pole, F);
    for (const vector<P4>* X : {&A, &B})
        for (const V3& y : stereo(*X, F)) rmax = std::max(rmax, std::sqrt(y[0]*y[0] + y[1]*y[1] + y[2]*y[2]));
    p.clip = 1.05*rmax;
}

static void pair_view(const LinkedPair& p, ViewState& v)
{
    frame_with_pole(p.pole, v.Q);
    v.clip = p.clip;
    v.zoom = 1;
}

static string pair_desc(const LinkedPair& p, int k)
{
    char b[400];
    string w, w2;
    for (int x : p.w) w += " " + std::to_string(x);
    for (int x : p.w2) w2 += " " + std::to_string(x);
    snprintf(b, sizeof b, "pair %d: lk %+ld   gamma_w, w =%s (Z+)   h.delta_w', w' =%s (Z-), phi(h) = %d", k, p.lk, w.c_str(), w2.c_str(), p.phi);
    return b;
}

#ifdef ZIPPER3_VIEWER
namespace vw {
    Options* opt;
    World* W;
    ViewState view;
    int mode = 3;                      // 1 Z+, 2 Z-, 3 both
    vector<float> lp, ln;              // GL_LINES vertex arrays
    int mx = -1, my = -1;
    vector<double> shells;             // +/- step through these radii
    vector<LinkedPair>* pairs = nullptr;   // pair mode
    int cur = 0;
    bool arcs = false;                 // pair mode: show the base arcs faintly as well
    vector<float> la, lb;

    // The movie: a fixed script of segments, played frame by frame and captured to PNGs.
    // R < 0 keeps the current roots; pair < 0 shows the zippers, else that linked pair.
    struct Seg { int frames; double R; int pair; bool arcs; double yaw, pole; const char* caption; };
    const Seg script1[] = {                    // the 58 s film
        {210, 0,     -1, false, 0.9, 0,   "The 18 base arcs of the zippers Z+ (blue) and Z- (red), in a stereographic view of S^3 centred at t+"},
        {180, -1,    -1, false, 0.2, 0.5, "Rotating S^3 itself: the pole of the stereographic projection moves"},
        {180, 2.554, -1, false, 0.9, 0,   "Adding the translates by the 18 shortest group elements"},
        {180, 3.327, -1, false, 0.9, 0,   "Adding the next shell of translates: the zippers fill in"},
        {150, 0,     -1, false, 0.9, 0,   "Back to the base arcs"},
        {240, -1,     5, false, 1.0, 0,   "A loop in Z+ and a loop in Z-, from the same relation of the kernel: linking number -1"},
        {180, -1,     5, true,  1.0, 0,   "The same pair against the faint base arcs"},
        {240, -1,     0, false, 1.0, 0,   "Another linked pair: linking number +1"},
        {180, -1,     0, true,  1.0, 0,   "The same pair against the faint base arcs"},
    };
    const Seg script2[] = {                    // the second film (to the first half of a recording of the B-flat minor fugue)
        {210, 0,     -1, false, 0.9, 0,   "The 18 base arcs of the zippers Z+ (blue) and Z- (red), in a stereographic view of S^3 centred at t+"},
        {240, -1,    -1, false, 0.2, 0.5, "Rotating S^3 itself: the pole of the stereographic projection moves"},
        {210, 2.554, -1, false, 0.9, 0,   "Adding the translates by the 18 shortest group elements"},
        {210, 3.327, -1, false, 0.9, 0,   "Adding the next shell of translates: the zippers fill in"},
        {210, -1,    -1, false, 0.3, 0.4, "Flying through S^3 among the translates"},
        {150, 0,     -1, false, 0.9, 0,   "Back to the base arcs"},
        {240, -1,     5, false, 1.0, 0,   "A loop in Z+ and a loop in Z-, from the same relation of the kernel: linking number -1"},
        {180, -1,     5, true,  1.0, 0,   "The same pair against the faint base arcs"},
        {240, -1,     0, false, 1.0, 0,   "Another linked pair: linking number +1"},
        {180, -1,     0, true,  1.0, 0,   "The same pair against the faint base arcs"},
        {210, -1,     6, false, 1.0, 0,   "Linking number -1"},
        {210, -1,     9, false, 1.0, 0,   "Linking number +1"},
        {210, -1,    12, true,  1.0, 0,   "Linking number -1, against the base arcs"},
        {240, 0,     -1, false, 0.7, 0,   "The zippers of the Cartwright-Steger surface"},
    };
    const Seg script3[] = {                    // the third film (to the first part of a recording of the B-flat minor fugue)
        {240, 0,     -1, false, 0.9, 0,   "The 18 base arcs of the zippers Z+ (blue) and Z- (red), in a stereographic view of S^3 centred at t+"},
        {210, -1,    -1, false, 0.2, 0.5, "Rotating S^3 itself: the pole of the stereographic projection moves"},
        {180, 2.554, -1, false, 0.9, 0,   "Adding the translates by the 18 shortest group elements"},
        {180, 3.327, -1, false, 0.9, 0,   "Adding the next shell of translates: the zippers fill in"},
        {180, -1,    -1, false, 0.3, 0.4, "Flying through S^3 among the translates"},
        {120, 0,     -1, false, 0.9, 0,   "Back to the base arcs"},
        {210, -1,     5, false, 1.0, 0,   "A loop in Z+ and a loop in Z-, from the same relation of the kernel: linking number -1"},
        {150, -1,     5, true,  1.0, 0,   "The same pair against the faint base arcs"},
        {210, -1,     0, false, 1.0, 0,   "Another linked pair: linking number +1"},
        {150, -1,     0, true,  1.0, 0,   "The same pair against the faint base arcs"},
        {180, -1,     6, false, 1.0, 0,   "Linking number -1"},
        {150, -1,     9, false, 1.0, 0,   "Linking number +1"},
        {180, -1,    12, true,  1.0, 0,   "Linking number -1, against the base arcs"},
        {203, 0,     -1, false, 0.7, 0,   "Music: Bach, Fugue in B-flat minor from WTC II (BWV 891)"},
    };
    const Seg* script = script1;
    int nseg = sizeof script1/sizeof script1[0], seg = -1, segframe = 0, frame = 0;
    vector<LinkedPair>* allpairs = nullptr;
    const char* caption = nullptr;
    bool pending = false;              // a frame has been requested and not yet captured
    bool blank = false;                // a blank segment (pair -2): only the caption is drawn
    vector<Seg> filescript;            // --moviescript FILE
    std::deque<string> filecaps;       // its captions (a deque keeps the pointers valid)

    // One segment per line:  FRAMES R PAIR ARCS YAW POLE CAPTION...   ('#' starts a comment;
    // the caption is the rest of the line, '-' for none; PAIR -2 is a blank frame with the caption)
    static bool load_script(const string& fn)
    {
        FILE* f = fopen(fn.c_str(), "r");
        if (!f) { fprintf(stderr, "cannot read %s\n", fn.c_str()); return false; }
        char line[2048];
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            Seg S;
            int arcs = 0, n = 0;
            if (sscanf(line, "%d %lf %d %d %lf %lf %n", &S.frames, &S.R, &S.pair, &arcs, &S.yaw, &S.pole, &n) < 6) {
                fprintf(stderr, "%s: bad line: %s", fn.c_str(), line);
                fclose(f);
                return false;
            }
            S.arcs = arcs != 0;
            string cap = line + n;
            while (!cap.empty() && (cap.back() == '\n' || cap.back() == '\r' || cap.back() == ' ')) cap.pop_back();
            filecaps.push_back(cap == "-" ? string() : cap);
            S.caption = filecaps.back().empty() ? nullptr : filecaps.back().c_str();
            filescript.push_back(S);
        }
        fclose(f);
        return !filescript.empty();
    }

    static void lines(const Renderer* r, vector<float>& out)
    {
        out.clear();
        if (!r) return;
        // stereographic coordinates only: GL applies the R^3 rotation (modelview)
        ViewState v = view;
        double I3[9] = {1,0,0, 0,1,0, 0,0,1};
        memcpy(v.rot, I3, sizeof I3);
        for (size_t k = 0; k < r->npoly(); k++) {
            double e0[3], e1[3];
            bool ok0 = eye(v, &r->pts[4*r->pbegin(k)], e0);
            for (size_t i = r->pbegin(k) + 1; i < r->pend(k); i++) {
                bool ok1 = eye(v, &r->pts[4*i], e1);
                if (ok0 && ok1) {
                    for (int c = 0; c < 3; c++) out.push_back((float)e0[c]);
                    for (int c = 0; c < 3; c++) out.push_back((float)e1[c]);
                }
                memcpy(e0, e1, sizeof e0);
                ok0 = ok1;
            }
        }
    }
    static void reproject()
    {
        lines(W->rp, lp);
        lines(W->rn, ln);
        if (pairs) { lines((*pairs)[cur].a, la); lines((*pairs)[cur].b, lb); }
    }

    static void reshape(int w, int h)
    {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double a = h ? (double)w/h : 1.0, e = view.clip/view.zoom, c = view.clip;
        if (a >= 1) glOrtho(-e*a, e*a, -e, e, c, 3*c);
        else glOrtho(-e, e, -e/a, e/a, c, 3*c);
    }

    static void save_png();

    static void draw_lines(const vector<float>& v, float r, float g, float b)
    {
        if (v.empty()) return;
        glColor3f(r, g, b);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, v.data());
        glDrawArrays(GL_LINES, 0, (GLsizei)(v.size()/3));
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    static void display()
    {
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslated(0, 0, -2*view.clip);
        GLdouble g[16] = {0};
        g[15] = 1;
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) g[c*4 + r] = view.rot[3*r + c];
        glMultMatrixd(g);
        GLfloat fogc[4] = {1, 1, 1, 1};
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fogc);
        glFogf(GL_FOG_START, (GLfloat)(view.clip));
        glFogf(GL_FOG_END, (GLfloat)(3.4*view.clip));
        glEnable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
        if (blank) {
        } else if (!pairs) {
            if (mode & 1) draw_lines(lp, 0.05f, 0.15f, 0.85f);
            if (mode & 2) draw_lines(ln, 0.85f, 0.08f, 0.08f);
        } else {
            if (arcs && (mode & 1)) draw_lines(lp, 0.70f, 0.75f, 0.95f);
            if (arcs && (mode & 2)) draw_lines(ln, 0.95f, 0.72f, 0.72f);
            glLineWidth(2.5f);
            if (mode & 1) draw_lines(la, 0.05f, 0.15f, 0.85f);
            if (mode & 2) draw_lines(lb, 0.85f, 0.08f, 0.08f);
            glLineWidth(1.0f);
        }
        glDisable(GL_DEPTH_TEST);                  // the fixed points t^+ (blue) and t^- (red)
        glPointSize(7.0f);
        glBegin(GL_POINTS);
        for (int sgn = 0; sgn < 2 && !blank; sgn++) {
            const P4& V = sgn ? W->minus.V : W->plus.V;
            float x[4] = {(float)V.x[0], (float)V.x[1], (float)V.x[2], (float)V.x[3]};
            ViewState v = view;
            double I3[9] = {1,0,0, 0,1,0, 0,0,1};
            memcpy(v.rot, I3, sizeof I3);
            double e[3];
            if (!eye(v, x, e)) continue;
            glColor3f(0, 0, 0);
            glVertex3d(e[0], e[1], e[2]);
        }
        glEnd();
        if (caption) {                            // the movie's caption, bottom left
            glDisable(GL_FOG);
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
            int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
            glOrtho(0, w, 0, h, -1, 1);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            glColor3f(0.1f, 0.1f, 0.1f);
            glRasterPos2i(14, 16);
            for (const char* c = caption; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
        }
        if (!opt->movie.empty() && seg >= 0 && pending) {    // the movie: capture this frame
            pending = false;
            glFinish();
            int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
            vector<uint8_t> px((size_t)w*h*3), flip((size_t)w*h*3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            for (int y = 0; y < h; y++) memcpy(&flip[(size_t)y*w*3], &px[(size_t)(h - 1 - y)*w*3], (size_t)w*3);
            char fn[600];
            snprintf(fn, sizeof fn, "%s/frame_%05d.png", opt->movie.c_str(), frame);
            write_png_rgb(fn, flip, w, h, 2);
            frame++;
        }
        if (!opt->snap.empty()) {                 // testing: save this frame and quit
            glFinish();
            string o = opt->out;
            opt->out = opt->snap;
            save_png();
            opt->out = o;
            exit(0);
        }
        glutSwapBuffers();
    }

    static void rot_screen(bool aboutX, double deg)       // rotate R^3 about a screen axis
    {
        double t = deg*PI/180, c = std::cos(t), s = std::sin(t), M[9];
        if (aboutX) { double X[9] = {1,0,0, 0,c,-s, 0,s,c}; memcpy(M, X, sizeof M); }
        else { double Y[9] = {c,0,s, 0,1,0, -s,0,c}; memcpy(M, Y, sizeof M); }
        double T[9];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { T[3*i+j] = 0; for (int k = 0; k < 3; k++) T[3*i+j] += M[3*i+k]*view.rot[3*k+j]; }
        memcpy(view.rot, T, sizeof T);
    }

    // move the pole toward the frame direction that is currently the screen axis (sx, sy, sz)
    static void pole_screen(double sx, double sy, double sz, double deg)
    {
        double a[3];
        for (int i = 0; i < 3; i++) a[i] = view.rot[i]*sx + view.rot[3 + i]*sy + view.rot[6 + i]*sz;    // rot^T s
        move_pole(view.Q, a, deg*PI/180);
        reproject();
    }

    static void special(int key, int, int)
    {
        const double d = 4.0;
        if (key == GLUT_KEY_LEFT) pole_screen(-1, 0, 0, d);
        else if (key == GLUT_KEY_RIGHT) pole_screen(1, 0, 0, d);
        else if (key == GLUT_KEY_UP) pole_screen(0, 1, 0, d);
        else if (key == GLUT_KEY_DOWN) pole_screen(0, -1, 0, d);
        glutPostRedisplay();
    }

    static void save_png()
    {
        int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
        vector<uint8_t> px((size_t)w*h*3), flip((size_t)w*h*3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
        for (int y = 0; y < h; y++) memcpy(&flip[(size_t)y*w*3], &px[(size_t)(h - 1 - y)*w*3], (size_t)w*3);
        string fn = opt->out.empty() ? string("zipper3.png") : opt->out;
        if (write_png_rgb(fn.c_str(), flip, w, h)) fprintf(stderr, "saved %s\n", fn.c_str());
    }

    static void set_pair(int k)
    {
        int n = (int)pairs->size();
        cur = ((k % n) + n) % n;
        pair_view((*pairs)[cur], view);
        reshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
        reproject();
        fprintf(stderr, "%s\n", pair_desc((*pairs)[cur], cur).c_str());
    }

    static void rebuild_pairs()
    {
        if (!pairs) return;
        for (auto& p : *pairs) build_pair(p, *W, opt->acc);
        set_pair(cur);
    }

    static void begin_segment(const Seg& S)
    {
        blank = S.pair == -2;
        if (S.R >= 0 && std::abs(S.R - opt->R) > 1e-9) { opt->R = S.R; build(*opt, *W); }
        if (S.pair >= 0) { pairs = allpairs; arcs = S.arcs; set_pair(S.pair); }
        else {
            bool reset = pairs != nullptr || seg == 0;
            pairs = nullptr;
            if (reset) { init_view(*opt, *W, view); reshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT)); }
        }
        reproject();
        caption = S.caption;
        fprintf(stderr, "movie: segment %d of %d (%d frames)\n", seg + 1, nseg, S.frames);
    }

    static void movie_idle()
    {
        if (pending) { glutPostRedisplay(); return; }      // wait for the last frame to be captured
        if (seg < 0 || segframe >= std::max(1, (int)(script[seg].frames*opt->moviescale))) {
            seg++;
            segframe = 0;
            if (seg >= nseg) { fprintf(stderr, "movie: frames %d to %d written to %s\n", opt->moviestart, frame - 1, opt->movie.c_str()); exit(0); }
            begin_segment(script[seg]);
        }
        const Seg& S = script[seg];
        rot_screen(false, S.yaw);
        rot_screen(true, 0.12*std::sin(2*PI*frame/300.0));
        if (S.pole != 0) {                         // the pole moves along a slowly turning direction
            double th = 2*PI*segframe/S.frames;
            pole_screen(std::cos(th), std::sin(th), 0, S.pole);
        }
        segframe++;
        pending = true;
        glutPostRedisplay();
    }

    static void keyb(unsigned char k, int, int)
    {
        if (k == '+' || k == '=' || k == '-' || k == '_') {      // next / previous shell of roots
            bool up = (k == '+' || k == '=');
            double R = opt->R;
            if (up) { for (double r : shells) if (r > opt->R + 1e-4) { R = r + 1e-6; break; } }
            else { R = 0; for (double r : shells) if (r < opt->R - 1e-4) R = r + 1e-6; }
            if (R != opt->R) { opt->R = R; build(*opt, *W); reproject(); }
        }
        else if (k == ']') { opt->acc /= 1.5; build(*opt, *W); rebuild_pairs(); reproject(); }
        else if (k == '[') { opt->acc *= 1.5; build(*opt, *W); rebuild_pairs(); reproject(); }
        else if (pairs && k == 'n') set_pair(cur + 1);
        else if (pairs && k == 'N') set_pair(cur - 1);
        else if (pairs && k == 'l') arcs = !arcs;
        else if (k == 'z' || k == 'x') {
            view.zoom *= (k == 'z') ? 1.5 : 1/1.5;
            reshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
            fprintf(stderr, "zoom %.4g\n", view.zoom);
        }
        else if (k == 'w') pole_screen(0, 0, 1, 4);
        else if (k == 's') pole_screen(0, 0, -1, 4);
        else if (k == 'c') { mode = mode == 3 ? 1 : mode + 1; }
        else if (k == 'b') {
            view.clip = view.clip >= 8 ? 1.5 : view.clip*2;
            reshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
            reproject();
            fprintf(stderr, "clip radius %.3g\n", view.clip);
        }
        else if (k == 'r') {
            init_view(*opt, *W, view);
            if (pairs) pair_view((*pairs)[cur], view);
            reshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
            reproject();
        }
        else if (k == 'p') save_png();
        else if (k == 'q' || k == 27) exit(0);
        glutPostRedisplay();
    }

    static void mouse(int button, int state, int x, int y)
    {
        if (button == GLUT_LEFT_BUTTON) { if (state == GLUT_DOWN) { mx = x; my = y; } else mx = -1; }
    }
    static void motion(int x, int y)
    {
        if (mx < 0) return;
        rot_screen(false, 0.4*(x - mx));
        rot_screen(true, 0.4*(y - my));
        mx = x; my = y;
        glutPostRedisplay();
    }
}

static void run_viewer(Options& opt, World& W, vector<LinkedPair>* pairs, int argc, char** argv)
{
    vw::opt = &opt;
    vw::W = &W;
    vw::pairs = pairs;
    init_view(opt, W, vw::view);
    if (pairs) { vw::cur = opt.pairindex % (int)pairs->size(); pair_view((*pairs)[vw::cur], vw::view); }
    vw::mode = opt.which;
    vw::shells = shell_radii(5.5);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(opt.size, opt.size);
    glutCreateWindow("zipper3: Cartwright-Steger, S^3");
    glEnable(GL_MULTISAMPLE);
    vw::reproject();
    glutDisplayFunc(vw::display);
    glutReshapeFunc(vw::reshape);
    glutSpecialFunc(vw::special);
    glutKeyboardFunc(vw::keyb);
    glutMouseFunc(vw::mouse);
    glutMotionFunc(vw::motion);
    if (!opt.movie.empty()) {
        vw::allpairs = pairs;
        vw::pairs = nullptr;
        if (opt.moviescript == 2) { vw::script = vw::script2; vw::nseg = sizeof vw::script2/sizeof vw::script2[0]; }
        if (opt.moviescript == 3) { vw::script = vw::script3; vw::nseg = sizeof vw::script3/sizeof vw::script3[0]; }
        if (!opt.moviefile.empty()) {
            if (!vw::load_script(opt.moviefile)) exit(1);
            vw::script = vw::filescript.data();
            vw::nseg = (int)vw::filescript.size();
        }
        vw::frame = opt.moviestart;
        if (!pairs) { fprintf(stderr, "the movie needs --pairs\n"); exit(1); }
        glutIdleFunc(vw::movie_idle);
    }
    fprintf(stderr, "viewer: drag = rotate R^3;  arrows, w/s = move the pole (rotate S^3);  z/x = zoom;\n"
                    "        +/- = next/previous shell of translates;  ] [ = finer/coarser accuracy;  c = Z+/Z-/both;\n"
                    "        b = clip radius;  r = reset view;  p = save PNG;  q = quit\n");
    if (pairs) {
        fprintf(stderr, "pair mode: n / N = next / previous linked pair;  l = show the base arcs faintly\n");
        fprintf(stderr, "%s\n", pair_desc((*pairs)[vw::cur], vw::cur).c_str());
    }
    glutMainLoop();
}
#endif

static void usage()
{
    fprintf(stderr,
"usage: zipper3 [options]\n"
"  -f FILE      substitution file from `chz kernel` (default subst_4.00_9.50_t3.txt)\n"
"  -R R         roots: the elements g with d(o, g o) <= R (default 0: the base arcs only;\n"
"               the first shells of translates are at 2.553, 3.326, 3.983, ...)\n"
"  -a ACC       accuracy: maximal chord on the unit S^3 (default 0.01)\n"
"  -o FILE.png  write a PNG (software rendering, depth-cued)\n"
"  -v           interactive viewer\n"
"  -s SIZE      image / window size in pixels (default 900)\n"
"  --only +|-   draw only Z+ or only Z-\n"
"  --clip RHO   clip the stereographic picture to the ball of radius RHO (default 3)\n"
"  --zoom F     magnify the picture about its centre (default 1)\n"
"  --yaw D --pitch D     rotate R^3 (degrees; default 0, 20)\n"
"  --pole D1 D2          move the pole of the projection (degrees, along E1 then E2)\n"
"  --cap N      stop after N leaves per zipper (default 8e6)\n"
"  --check      report the smallest distance between the drawn Z+ and Z- (must stay > 0)\n"
"  --pairs FILE show linked pairs of loops (from `link3 --export`); n / N step through them,\n"
"               l shows the base arcs faintly; with -o, draw pair number --pairindex K\n"
"               (--context: with the base arcs faintly behind it)\n"
"  --thick N    PNG: thicker lines\n"
"  --movie DIR  play the built-in demonstration script (needs --pairs) and save its frames to DIR\n"
"  --moviescript 1|2|3|FILE   which script: built-in, or a file with one segment per line,\n"
"               FRAMES R PAIR ARCS YAW POLE CAPTION  (R -1: keep the roots; PAIR -1: the zippers,\n"
"               -2: a blank frame with the caption; YAW, POLE in degrees per frame; caption '-': none)\n"
"  --moviestart N   number the frames from N (default 0)\n");
}

int main(int argc, char** argv)
{
    setup();
    Options opt;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { usage(); exit(1); } return argv[++i]; };
        if (a == "-f") opt.subst = next();
        else if (a == "-R") opt.R = atof(next().c_str());
        else if (a == "-a") opt.acc = atof(next().c_str());
        else if (a == "-o") opt.out = next();
        else if (a == "-v") opt.viewer = true;
        else if (a == "-s") opt.size = atoi(next().c_str());
        else if (a == "--only") { string w = next(); opt.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--clip") opt.clip = atof(next().c_str());
        else if (a == "--zoom") opt.zoom = atof(next().c_str());
        else if (a == "--yaw") opt.yaw = atof(next().c_str());
        else if (a == "--pitch") opt.pitch = atof(next().c_str());
        else if (a == "--pole") { opt.pole1 = atof(next().c_str()); opt.pole2 = atof(next().c_str()); }
        else if (a == "--cap") opt.cap = (size_t)atof(next().c_str());
        else if (a == "--check") opt.check = true;
        else if (a == "--snap") { opt.snap = next(); opt.viewer = true; }
        else if (a == "--pairs") opt.pairs = next();
        else if (a == "--pairindex") opt.pairindex = atoi(next().c_str());
        else if (a == "--movie") { opt.movie = next(); opt.viewer = true; }
        else if (a == "--moviescale") opt.moviescale = atof(next().c_str());
        else if (a == "--moviescript") {
            string s = next();
            if (s.find_first_not_of("0123456789") == string::npos) opt.moviescript = atoi(s.c_str());
            else opt.moviefile = s;
        }
        else if (a == "--moviestart") opt.moviestart = atoi(next().c_str());
        else if (a == "--thick") opt.thick = atoi(next().c_str());
        else if (a == "--context") opt.context = true;
        else { usage(); return 1; }
    }
    World W;
    prepare(opt, W);
    build(opt, W);
    vector<LinkedPair> pairs;
    if (!opt.pairs.empty()) {
        if (!load_pairs(opt.pairs, pairs)) return 1;
        double t0 = now();
        for (auto& p : pairs) build_pair(p, W, opt.acc);
        fprintf(stderr, "built the %zu pairs of loops in %.2fs\n", pairs.size(), now() - t0);
    }
    if (opt.check && W.rp->nLeaves && W.rn->nLeaves) {
        // fine cells first: a pair at distance d is found once the cell size h exceeds d + acc
        P4 wa, wb;
        double t0 = now(), d = 1, h;
        for (h = 3*opt.acc; h < 0.8; h *= 2) {
            d = zdist(*W.rp, *W.rn, h, wa, wb);
            if (d < h - opt.acc) break;
        }
        if (h < 0.8) fprintf(stderr, "check: min distance Z+ to Z- (vertex to segment, on S^3) = %.5f  (%.2fs)  at Z+ point (%.4f %.4f %.4f %.4f)\n",
                             d, now() - t0, wa.x[0], wa.x[1], wa.x[2], wa.x[3]);
        else fprintf(stderr, "check: min distance Z+ to Z- is at least %.2f\n", h/2 - opt.acc);
    }
    if (opt.viewer) {
#ifdef ZIPPER3_VIEWER
        run_viewer(opt, W, pairs.empty() ? nullptr : &pairs, argc, argv);
#else
        fprintf(stderr, "built without the viewer\n");
        return 1;
#endif
    }
    if (!opt.out.empty() && !pairs.empty()) {             // a picture of one linked pair
        const LinkedPair& p = pairs[opt.pairindex % pairs.size()];
        ViewState v;
        init_view(opt, W, v);
        pair_view(p, v);
        vector<Layer> layers;
        if (opt.context) {
            layers.push_back({W.rp, {0.70, 0.75, 0.95}, 0});
            layers.push_back({W.rn, {0.95, 0.72, 0.72}, 0});
        }
        if (opt.which & 1) layers.push_back({p.a, {0.05, 0.15, 0.85}});
        if (opt.which & 2) layers.push_back({p.b, {0.85, 0.08, 0.08}});
        if (!write_png(opt.out.c_str(), opt.size, layers, v, {}, opt.thick ? opt.thick : 2)) { fprintf(stderr, "cannot write %s\n", opt.out.c_str()); return 1; }
        fprintf(stderr, "%s\nwrote %s\n", pair_desc(p, opt.pairindex % pairs.size()).c_str(), opt.out.c_str());
        return 0;
    }
    if (!opt.out.empty()) {
        ViewState v;
        init_view(opt, W, v);
        vector<Layer> layers;
        if (opt.which & 1) layers.push_back({W.rp, {0.05, 0.15, 0.85}});
        if (opt.which & 2) layers.push_back({W.rn, {0.85, 0.08, 0.08}});
        vector<P4> marks = {W.plus.V, W.minus.V};
        if (!write_png(opt.out.c_str(), opt.size, layers, v, marks, opt.thick)) { fprintf(stderr, "cannot write %s\n", opt.out.c_str()); return 1; }
        fprintf(stderr, "wrote %s\n", opt.out.c_str());
    }
    return 0;
}
