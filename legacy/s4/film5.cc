/*  film5.cc  --  the four-dimensional ending of the zipper film: the zippers of the fibered
    hyperbolic 5-manifold of Italiano-Martelli-Migliorini on S^4 (rh5.h, zipcore5.h), set to the
    last four bars of the fugue and the beat before them.

    The picture is the stereographic projection of S^4 from the point opposite t^+, seen as its
    shadow in R^3 along a hidden fourth direction; lines far out along the hidden direction are
    paler (a second fog).  Turning the hidden direction (a rotation of R^4 in the plane of the
    screen's horizontal axis and the hidden one) makes the curves move through each other in a
    way that no rotation of R^3 can.

    The choreography is keyed to musical landmarks (a file of NAME SECONDS lines), over the last
    phrase of the fugue, its last six bars:
      START  bar 96          the cut from the 3D film
      B96    bar 96          blue pens trace two base arcs of Z+ with the subject (right hand, sixths)
      B96b   bar 96 beat 2   red pens trace the same two arcs of Z-, with the inversion (left hand,
                             thirds) a beat later; the pens move slowly through the half notes...
      B98, B98b              ...and faster with the eighth-note runs
      B99c   bar 99 beat 3   the blue pens arrive
      B100   bar 100         the red pens arrive; all the base arcs come in
      B100b  bar 100 beat 2  the translates come in
      FINAL  the last chord  everything is drawn; the hidden direction turns
      FADE0, FADE1           the picture fades to white
      CRED, END              the credits, then the end of the film
    Frame k is shown at k/30 s, as in the rest of the film.

    Usage:  film5 -f SUBST --landmarks FILE --movie DIR [--pens J1,J2] [--Rfull R] [--acc A]
            film5 ... --list                 list the base arcs (to choose the pens)
            film5 ... --still K OUT.png       one frame, then quit

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "zipcore5.h"
#include <map>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

namespace fm {

struct Opts {
    string subst = "subst5_2.30_4.50_t1.txt", landmarks, movie, still;
    int stillframe = -1, size = 840;
    double Rfull = 2.1, acc = 0.025, penacc = 0.008, clip = 3, hidfog = 1.6;
    vector<int> pens;
    bool list = false, nocaptions = false;      // --nocaptions: the captions are added afterwards (film/captions.py)
};
Opts O;
std::map<string, double> LM;
World5 W0, W1;                          // the base arcs (R = 0), and the translates (R = Rfull)
vector<Renderer5*> penB, penR;          // the pen arcs of Z+ and Z- (each de-duplicated on its own)
int frame = 0, last = 0;
bool pending = false;

double lm(const char* n)
{
    auto it = LM.find(n);
    if (it == LM.end()) { fprintf(stderr, "landmark %s is missing\n", n); exit(1); }
    return it->second;
}

static double clamp01(double x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static double smooth(double x) { x = clamp01(x); return x*x*(3 - 2*x); }
static double ramp(double t, double a, double b) { return b > a ? clamp01((t - a)/(b - a)) : (t >= b ? 1 : 0); }

// the view at time t
static void view_at(double t, View5& v)
{
    init_view5(W0, v, 0, 12);
    v.clip = O.clip; v.zoom = 1; v.hidfog = O.hidfog;
    double t0 = lm("START"), tf = lm("FINAL"), f1 = lm("FADE1");
    // the hidden direction: a slow turn while the pens draw, then a quarter turn under the chord
    double th = 14*ramp(t, t0, tf) + 86*smooth((t - tf)/(f1 - tf));
    turn4(v.U, 0, 3, th*PI/180);
    // a slow turn of R^3 as well, for the depth
    double yaw = 9*(t - t0);
    rot3(yaw, 12, v.rot);
}

struct Buf { vector<float> p, c; };

static void add_lines(const Renderer5& r, const View5& v, const double col[3], double op, Buf& B)
{
    if (op <= 0) return;
    for (size_t k = 0; k < r.npoly(); k++) {
        double e0[3], e1[3], h0, h1;
        bool ok0 = eye5(v, &r.pts[5*r.pbegin(k)], e0, h0);
        for (size_t i = r.pbegin(k) + 1; i < r.pend(k); i++) {
            bool ok1 = eye5(v, &r.pts[5*i], e1, h1);
            if (ok0 && ok1) {
                for (int s = 0; s < 2; s++) {
                    const double* e = s ? e1 : e0;
                    double hh = s ? h1 : h0;
                    double fh = 0.12 + 0.88*std::exp(-(hh/v.hidfog)*(hh/v.hidfog));
                    double a = op*fh;
                    for (int c = 0; c < 3; c++) { B.p.push_back((float)e[c]); B.c.push_back((float)(1 - a*(1 - col[c]))); }
                }
            }
            memcpy(e0, e1, sizeof e0);
            h0 = h1;
            ok0 = ok1;
        }
    }
}

// the first fraction p of a pen arc (by number of points, in the order they were drawn), and its tip
static bool add_pen(const Renderer5& r, double p, const View5& v, const double col[3], Buf& B, double tip[3])
{
    size_t total = r.pts.size()/5;
    if (p <= 0 || total < 2) return false;
    size_t n = std::min(total, (size_t)std::lround(p*total) + 1);
    bool have = false;
    for (size_t k = 0; k < r.npoly() && r.pbegin(k) < n; k++) {
        double e0[3], e1[3], h0, h1;
        bool ok0 = eye5(v, &r.pts[5*r.pbegin(k)], e0, h0);
        size_t end = std::min(r.pend(k), n);
        for (size_t i = r.pbegin(k) + 1; i < end; i++) {
            bool ok1 = eye5(v, &r.pts[5*i], e1, h1);
            if (ok0 && ok1) {
                for (int s = 0; s < 2; s++) {
                    const double* e = s ? e1 : e0;
                    double hh = s ? h1 : h0;
                    double a = 0.35 + 0.65*std::exp(-(hh/v.hidfog)*(hh/v.hidfog));
                    for (int c = 0; c < 3; c++) { B.p.push_back((float)e[c]); B.c.push_back((float)(1 - a*(1 - col[c]))); }
                }
                memcpy(tip, e1, 3*sizeof(double));
                have = true;
            }
            memcpy(e0, e1, sizeof e0);
            h0 = h1;
            ok0 = ok1;
        }
    }
    return have && n < total;
}

static void draw_buf(const Buf& B)
{
    if (B.p.empty()) return;
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, B.p.data());
    glColorPointer(3, GL_FLOAT, 0, B.c.data());
    glDrawArrays(GL_LINES, 0, (GLsizei)(B.p.size()/3));
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

static void caption_text(const char* s, float grey)
{
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glColor3f(grey, grey, grey);
    glRasterPos2i(14, 16);
    for (const char* c = s; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

static const double BLUE[3] = {0.05, 0.15, 0.85}, RED[3] = {0.85, 0.08, 0.08};

static void render(double t)
{
    View5 v;
    view_at(t, v);
    double fade = 1 - smooth((t - lm("FADE0"))/(lm("FADE1") - lm("FADE0")));
    bool credits = t >= lm("CRED");
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    double e = v.clip/v.zoom, c = v.clip;
    glOrtho(-e, e, -e, e, c, 3*c);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0, 0, -2*v.clip);
    GLfloat fogc[4] = {1, 1, 1, 1};
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogc);
    glFogf(GL_FOG_START, (GLfloat)(v.clip));
    glFogf(GL_FOG_END, (GLfloat)(3.4*v.clip));
    glEnable(GL_DEPTH_TEST);
    if (!credits && fade > 0) {
        // the pens: 30% of the way through the half notes of bars 96-97, the rest with the runs
        auto pen = [&](const char* a, const char* b, const char* c) {
            return t < lm(b) ? 0.3*ramp(t, lm(a), lm(b)) : 0.3 + 0.7*ramp(t, lm(b), lm(c));
        };
        double pb = pen("B96", "B98", "B99c"), pr = pen("B96b", "B98b", "B100");
        double abase = smooth((t - lm("B100"))/(lm("B100b") - lm("B100"))), atrans = smooth((t - lm("B100b"))/(lm("FINAL") - lm("B100b")));
        // the translates and the other base arcs, underneath
        Buf B;
        add_lines(*W1.rp, v, BLUE, 0.85*atrans*fade, B);
        add_lines(*W1.rn, v, RED, 0.85*atrans*fade, B);
        if (atrans < 1) {
            add_lines(*W0.rp, v, BLUE, abase*(1 - atrans)*fade, B);
            add_lines(*W0.rn, v, RED, abase*(1 - atrans)*fade, B);
        }
        glLineWidth(1.0f);
        draw_buf(B);
        // the pens, on top
        glDisable(GL_DEPTH_TEST);
        Buf P;
        vector<std::array<double,4>> tips;
        double pcol[3];
        for (int z = 0; z < 2; z++) {
            const auto& arcs = z ? penR : penB;
            double p = z ? pr : pb;
            for (int cc = 0; cc < 3; cc++) pcol[cc] = 1 - fade*(1 - (z ? RED : BLUE)[cc]);
            for (const Renderer5* a : arcs) {
                double tip[3];
                if (add_pen(*a, p, v, pcol, P, tip)) tips.push_back({tip[0], tip[1], tip[2], (double)z});
            }
        }
        glLineWidth(2.2f);
        draw_buf(P);
        glLineWidth(1.0f);
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        for (auto& q : tips) { const double* cl = q[3] ? RED : BLUE; glColor3d(cl[0], cl[1], cl[2]); glVertex3d(q[0], q[1], q[2]); }
        // the fixed points t^+ and t^-
        for (int s = 0; s < 2; s++) {
            const P5& V = s ? W0.minus.V : W0.plus.V;
            float x[5] = {(float)V.x[0], (float)V.x[1], (float)V.x[2], (float)V.x[3], (float)V.x[4]};
            double ee[3], hh;
            if (!eye5(v, x, ee, hh)) continue;
            double g = 1 - fade;
            glColor3d(g, g, g);
            glPointSize(7.0f);
            glVertex3d(ee[0], ee[1], ee[2]);
        }
        glEnd();
    }
    // the captions
    if (O.nocaptions) {}
    else if (credits) caption_text("Music: Bach, Fugue in B-flat minor from WTC II (BWV 891)", 0.1f);
    else if (t < lm("FINAL")) caption_text("A hyperbolic 5-manifold that fibers over the circle (Italiano-Martelli-Migliorini): its zippers lie in S^4", 0.1f);
    else if (fade > 0) caption_text("The shadow of S^4 in R^3, turning in the fourth dimension", (float)(1 - 0.9*fade));
}

static void save(const char* fn)
{
    glFinish();
    int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
    vector<uint8_t> px((size_t)w*h*3), flip((size_t)w*h*3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; y++) memcpy(&flip[(size_t)y*w*3], &px[(size_t)(h - 1 - y)*w*3], (size_t)w*3);
    write_png_rgb(fn, flip, w, h, 2);
}

static void display()
{
    render(frame/30.0);
    if (pending) {
        pending = false;
        char fn[600];
        if (!O.still.empty()) snprintf(fn, sizeof fn, "%s", O.still.c_str());
        else snprintf(fn, sizeof fn, "%s/frame_%05d.png", O.movie.c_str(), frame);
        save(fn);
        if (!O.still.empty()) { fprintf(stderr, "wrote %s (frame %d, t = %.3f s)\n", fn, frame, frame/30.0); exit(0); }
        frame++;
    }
    glutSwapBuffers();
}

static void idle()
{
    if (pending) { glutPostRedisplay(); return; }
    if (frame >= last) { fprintf(stderr, "film5: frames written to %s\n", O.movie.c_str()); exit(0); }
    if (frame % 30 == 0) fprintf(stderr, "frame %d (t = %.2f s)\n", frame, frame/30.0);
    pending = true;
    glutPostRedisplay();
}

}  // namespace fm

int main(int argc, char** argv)
{
    using namespace fm;
    rh5_setup();
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return argv[++i]; };
        if (a == "-f") O.subst = next();
        else if (a == "--landmarks") O.landmarks = next();
        else if (a == "--movie") O.movie = next();
        else if (a == "--still") { O.stillframe = atoi(next().c_str()); O.still = next(); }
        else if (a == "--Rfull") O.Rfull = atof(next().c_str());
        else if (a == "--acc") O.acc = atof(next().c_str());
        else if (a == "--penacc") O.penacc = atof(next().c_str());
        else if (a == "--hidfog") O.hidfog = atof(next().c_str());
        else if (a == "--list") O.list = true;
        else if (a == "--nocaptions") O.nocaptions = true;
        else if (a == "--pens") { string s = next(); for (size_t p = 0; p < s.size();) { size_t q = s.find(',', p); if (q == string::npos) q = s.size(); O.pens.push_back(atoi(s.substr(p, q - p).c_str())); p = q + 1; } }
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    prepare5(O.subst.c_str(), W0);
    if (O.list) {
        // each base arc of Z+: its number of points and its extent in the starting view
        View5 v;
        init_view5(W0, v, 0, 12);
        for (size_t j = 0; j < W0.L.s.size(); j++) if (W0.L.plus[j]) {
            vector<P5> a;
            arc_points5(W0.plus, m6id(), (int)j, 0.02, a);
            double ext = 0, e[3], h;
            for (const P5& p : a) {
                float x[5] = {(float)p.x[0], (float)p.x[1], (float)p.x[2], (float)p.x[3], (float)p.x[4]};
                if (eye5(v, x, e, h)) ext = std::max(ext, std::sqrt(e[0]*e[0] + e[1]*e[1] + e[2]*e[2]));
            }
            fprintf(stderr, "arc %2zu: %6zu points, extent %.3f, end point (%.3f %.3f %.3f %.3f %.3f)\n", j, a.size(), ext,
                    a.back().x[0], a.back().x[1], a.back().x[2], a.back().x[3], a.back().x[4]);
        }
        return 0;
    }
    if (O.landmarks.empty()) { fprintf(stderr, "--landmarks is needed\n"); return 1; }
    {
        std::ifstream in(O.landmarks);
        string n;
        double x;
        while (in >> n >> x) LM[n] = x;
    }
    build5(W0, 0, O.acc*0.8, 20000000);
    prepare5(O.subst.c_str(), W1);
    build5(W1, O.Rfull, O.acc, 20000000);
    if (O.pens.empty()) O.pens = {0, 1};
    for (int j : O.pens) {
        Renderer5* a = new Renderer5(W0.plus, W0.L, O.penacc, 4000000);
        Renderer5* b = new Renderer5(W0.minus, W0.L, O.penacc, 4000000);
        a->draw(m6id(), j);
        b->draw(m6id(), j);
        penB.push_back(a); penR.push_back(b);
        fprintf(stderr, "pen arc %d: %zu points in %zu polylines (Z+), %zu points in %zu polylines (Z-)\n", j,
                a->pts.size()/5, a->npoly(), b->pts.size()/5, b->npoly());
    }
    frame = (int)std::lround(30*lm("START"));
    last = (int)std::lround(30*lm("END"));
    if (O.stillframe >= 0) frame = O.stillframe;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(O.size, O.size);
    glutCreateWindow("film5: the zippers on S^4");
    glEnable(GL_MULTISAMPLE);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    if (!O.still.empty()) pending = true;
    fprintf(stderr, "film5: frames %d to %d\n", frame, last - 1);
    glutMainLoop();
}
