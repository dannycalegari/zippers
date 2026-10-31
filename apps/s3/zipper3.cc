/*  zipper3.cc  --  draw approximations to the zippers Z+ and Z- on the 3-sphere at infinity
    of the Cartwright-Steger surface, for phi = the first coordinate of H_1 (see examples/cartwright_steger.h).

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

    The program of legacy/s3/zipper3.cc on the shared library src/, as its batch build (the legacy `nogui`
    build, legacy/s3/nogui/zipper3), with the same options, usage text, output and files. The viewer (-v, --snap,
    --movie) is not built: as in the legacy nogui build, those options are read, the zippers are drawn, and the program
    then says "built without the viewer" and exits with status 1. The example's data
    come from examples/cartwright_steger.h (csgroup.h there), its render policy from examples/cartwright_steger_render.h,
    the renderer from render/ and the view and painter from view/s3.h (zipcore.h there), the linked pairs from
    analysis/pairs.h (this file there) and the separation of --check from analysis/separation.h (zdist, zipcore.h).
    The changes are the includes and the names: cs::setup (setup), World<PU21> with prepare<cs::RenderPolicy> and
    build<cs::RenderPolicy> (World, prepare, build), LinkedPair<PU21> and build_pair<cs::RenderPolicy>, the view's names
    in zippers::s3, and init_view with the six numbers of the options that it reads; the options (Options, from
    zipcore.h) are here.

    One change of result: the render policy takes the renderer's look-ahead through
    one-letter roofs (render/renderer.h, set_lookthrough). None of the reference runs changes, but a few leaves
    differ from the legacy program's at some settings: -R 2.554 -a 0.04 draws 72,167 leaves of Z+, where the legacy
    program draws 72,169, and the build line says "2 split through one-letter roofs".

    --stats: after the build, the checks of the roots, the elements of Pi in B(R)
    (examples/cartwright_steger_roots.h: certify_roots), on stderr: one line, and a second when R is within 1e-6 of a
    shell distance, where the single-precision test of roots_info decides. It makes the search of the roots again and
    compares its roots with those drawn, bit for bit. Without --stats nothing that the program prints or writes changes;
    the usage text has one more line (no reference run prints it).

    --overlap L (default 0): the renderer's marking (render/renderer.h): when a leaf is accepted, the keys
    of its sub-branches down to L levels are marked as drawn, as the S^2 program's --overlap L does. It is off by
    default, and with L = 0 nothing that the program prints or writes changes; with L >= 1 the build lines are followed
    by one line of marking counts. In S^3 marking is not proved safe (there are no enclosures): its costs are in
    docs/developer_guide.md, "Marking and overdraw". The usage text has one more line.

    THE DEEP ZOOM (the view is set up before the build, which prints nothing):
      --centre P   the view centred at the point P of S^3 (four numbers, normalised): the default frame of P (view/s3.h:
                   default_frame, the contact plane at P horizontal and its Reeb direction vertical) in place of that of
                   t^+, then --pole as before;
      --cull, --cullk K   culling (render/renderer.h, render/cull.h): the window of the view (view/s3.h: window3) is
                   given to the build (World::cull), whose lines then add ", N culled"; like marking, a measured trade, not a
                   proof; on by default for a still of a zoomed view (THE DEFAULTS, below). Not with --pairs (whose
                   pictures have views of their own);
      --zoomacc    the accuracy on the sphere ACC/zoom (it needs culling, which may be the default's); the build lines print
                   the accuracy used;
      --focus M    the focus ball (render/world.h: World::focus), centred at an orbit point h o of Pi near the point at
                   distance M log(zoom) towards the centre of the view (examples/cartwright_steger.h: focus_centre3); one
                   line (stderr) says what h is and how far h o is from that point. Only with --zoom > 1; M = 1 is the
                   default of a still of a zoomed view.
    Three notes on stderr, each only when culling or the focus ball is on, asked for or by default: --stats with the focus
    ball checks B(R), the first roots (W.nmain), and says that the others are h B(R); --check with culling says that its
    minimum distance is over the culled picture; and the usage text of --cullk says what K = 16 was measured on.

    THE DEFAULTS.
      the file      -f defaults to subst_4.00_9.50_t3_r1.txt (data/s3/: chz kernel 4 9.5 3 --roofs --jexcess 1, the roofs
                    chosen at one letter of excess), and to subst_4.00_9.50_t3.txt (data/s3/, a copy of the file of
                    legacy/s3/) with --pairs: the linked pairs are loops of that file's base arcs, and with other roofs the
                    arcs are other curves. The file of -f (or the default) and that of --pairs are found by the search of
                    core/data_files.h: a name with a '/' as given; else the working folder, the folders of ZIPPERS_DATA,
                    then data/ of the source tree (installed: share/zippers/); the first place that holds an entry of the
                    name is used, and an entry there that is not a readable file stops the program, which says what is
                    there; when it is found nowhere the message says where it looked. A file found in the working folder
                    keeps its name, so the program prints what it did when it read the working folder only;
      zoomed stills with -o and --zoom F, F > 1, and no viewer (-v, --snap, --movie) and no --pairs, the program behaves as
                    if --cull and --focus 1 had been given (K = CULL_K, render/cull.h), unless --cull, --cullk K or --focus M
                    says otherwise or --nocull / --nofocus turns the default off; a note on stderr says which default is on.
                    Everywhere else (zoom 1, no -o, the viewer, --pairs) culling and the focus are off unless asked for.
                    --nocull is refused with --cull, --cullk and --zoomacc, --nofocus with --focus. --zoomacc needs culling,
                    which may be the default's.

    THREE OPTIONS FOR OTHER PROGRAMS (a window, among them), which change nothing when they are not given:
      --polylines FILE   the curves as data, a polyline file (output/polylines.h; write_polylines below says what its header
                    holds): the layers Z+ and Z-, the renderers' points with the root and the letter of each polyline, and
                    with --pairs pair.Z+ and pair.Z-, whose polyline k is the loop of pair k; for each layer, whether its
                    renderer reached the cap of leaves (so its curves are incomplete). Written after the build (and the
                    pairs, and --check), before any picture. An argument that holds a line break is refused at the start,
                    since the header's command line cannot hold it, and so is a FILE that cannot be written (a folder of
                    the name, a folder that may not be written in).
      --viewmatrix N1,...,N25   the view as its matrices: the frame Q of R^4 (16 numbers, row by row) and the rotation of
                    R^3 (9 numbers, row by row), comma-separated, in place of the view of --yaw, --pitch, --pole and
                    --centre (which it does not go with); --zoom and --clip as usual. Both must be orthonormal to within
                    1e-9 (view/options.h), and of determinant +1: a frame or a rotation of determinant -1 would draw the
                    mirror image, on which every linking number has the other sign (no option makes one). The view is
                    then exactly the one given, bit for bit, so a program that knows a view (a window, or the header of a
                    polyline file, whose viewmatrix is the view of the zippers in these numbers) can have it drawn. With
                    --pairs and -o the pair is drawn in this view, with --zoom and --clip, in place of its own
                    (pair_view: the frame of the pair's pole, its clip radius and zoom 1, which the header gives as
                    pair.K.pole and pair.K.clip; the header's viewmatrix is still the zippers' view).
      --colour L=R,G,B   the colour of what L names, three numbers in [0, 1]: Z+ and Z- (the zippers), pair.Z+ and pair.Z-
                    (a pair's loops), context.Z+ and context.Z- (the zippers drawn faintly behind a pair, --context). A
                    name that the picture does not draw is accepted and changes nothing; when one is given twice, the
                    last one counts. The defaults are the colours the program always had, so the same numbers given
                    again draw the same picture, byte for byte.
*/

#include "analysis/pairs.h"
#include "analysis/separation.h"
#include "core/data_files.h"
#include "core/timing.h"
#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/cartwright_steger_roots.h"
#include "geometry/pu21/pu21.h"
#include "render/world.h"
#include "output/polylines.h"
#include "view/options.h"
#include "view/s3.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace zippers;
using std::string;
using std::vector;

// the default substitution files (data/s3/): the roofs chosen at one letter of excess, and, for --pairs, the file whose
// base arcs the linked pairs are loops of (a copy of the file of legacy/s3/)
static const char* const DEFAULT_SUBST = "subst_4.00_9.50_t3_r1.txt";
static const char* const DEFAULT_PAIRS_SUBST = "subst_4.00_9.50_t3.txt";

struct Options {
    string subst = DEFAULT_SUBST;
    bool subst_given = false;      // -f was given (the default file is then not the program's business)
    double R = 0, acc = 0.01, clip = 3, zoom = 1, yaw = 0, pitch = 20, pole1 = 0, pole2 = 0;
    int size = 900;
    size_t cap = 8000000;
    int which = 3;                 // 1 = Z+, 2 = Z-, 3 = both
    string out;
    bool viewer = false, check = false;
    bool stats = false;            // the checks of the roots, after the build
    int overlap = 0;               // --overlap L: the renderer's marking (render/renderer.h), 0 = off
    string snap;                   // viewer: save the first frame to this PNG and exit (testing)
    string pairs;                  // linked pairs (from `link3 --export`) to show instead of the zippers
    int pairindex = 0;
    string movie;                  // viewer: play the built-in script and save its frames here
    double moviescale = 1;         // scale the length of every segment (testing)
    int moviescript = 1;           // 1: the 58 s film; 2 and 3: two films to a recording of the fugue
    string moviefile;              // or a script file, one segment per line (see zipper3 --help)
    int moviestart = 0;            // the number of the first frame (frame_NNNNN.png)
    int thick = 0;                 // PNG: line thickness (half-width of the brush, in supersampled pixels)
    bool context = false;          // PNG of a linked pair: draw the base arcs faintly behind it
    // the deep zoom: off unless asked for, or the default of a zoomed still (below)
    bool centre = false;           // --centre: the view centred at c
    double c[4] = {0, 0, 0, 0};
    bool cull = false;             // --cull, --cullk K
    double cullk = CULL_K;
    bool zoomacc = false;          // --zoomacc: ACC/zoom on the sphere
    double focus = 0;              // --focus M: the focus ball at M log(zoom), 0 = off
    // the defaults of a zoomed still
    bool nocull = false;           // --nocull: culling is not on by default for a zoomed still
    bool nofocus = false;          // --nofocus: neither is the focus ball
    bool cull_default = false;     // culling is on because of the default (a zoomed still), not because of an option
    bool focus_default = false;    // so is the focus ball
    // for other programs: the curves as data, the exact view, the colours
    string polylines;              // --polylines FILE (output/polylines.h)
    string command;                // the command line, for the header of --polylines
    bool viewmatrix = false;       // --viewmatrix: the frame Q and the rotation of R^3, as given
    double vm[25] = {0};           // Q row by row, then the rotation row by row
    bool view_options = false;     // --yaw, --pitch, --pole or --centre was given
    bool colour_given[6] = {false, false, false, false, false, false};   // --colour, in the order of COLOUR_NAMES
    double colour[6][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
};

// the colours of the pictures, unless --colour gives others: the zippers, a pair's loops (the zippers' colours), and the
// zippers drawn faintly behind a pair (--context)
static const double PLUS[3] = {0.05, 0.15, 0.85}, MINUS[3] = {0.85, 0.08, 0.08};
static const double CONTEXT_PLUS[3] = {0.70, 0.75, 0.95}, CONTEXT_MINUS[3] = {0.95, 0.72, 0.72};
static const char* const COLOUR_NAMES[6] = {"Z+", "Z-", "pair.Z+", "pair.Z-", "context.Z+", "context.Z-"};

// a comma-separated list of n numbers
static bool parse_numbers(const string& s, size_t n, vector<double>& v)
{
    v.clear();
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find(',', i);
        if (j == string::npos) j = s.size();
        string t = s.substr(i, j - i);
        char* end = nullptr;
        double x = strtod(t.c_str(), &end);
        if (t.empty() || *end) return false;
        v.push_back(x);
        i = j + 1;
    }
    return v.size() == n;
}

// the colour of what COLOUR_NAMES[k] names, as --colour gives it, or the default
static const double* colour_of(const Options& opt, int k, const double* dflt)
{
    return opt.colour_given[k] ? opt.colour[k] : dflt;
}

// --polylines FILE: the curves as data (output/polylines.h). The layers Z+ and Z-, the renderers' points with the root and
// the letter of each polyline (roots: W.rootd, the distances of the roots; with the focus ball, the first roots.main are
// B(R)'s), and with --pairs pair.Z+ and pair.Z- (polyline k is the loop of pair k, with no root or letter). The header: the
// program and its command line, the example, the file (as found), R, the accuracy drawn, the cap, marking, the zippers
// drawn (only), culling (its K, or off) and the focus ball (its M, or off), the view of the zippers (viewmatrix: the
// numbers of --viewmatrix; zoom; clip), the marks t+ and t- (mark NAME x1 x2 x3 x4), for each layer whether its renderer
// reached the cap of leaves (capped NAME yes|no; a pair's loops have no cap), and with --pairs their number and for each
// pair K its pole (pair.K.pole), its clip radius (pair.K.clip), its linking number (pair.K.lk) and its line (pair.K.line).
// A pair's own view is the frame of its pole (s3::frame_with_pole), its clip radius and zoom 1 (pair_view).
static bool write_polylines(const Options& opt, const World<PU21>& W, const s3::ViewState& view, double acc,
                            const vector<LinkedPair<PU21>>& pairs)
{
    namespace pl = zippers::polylines;
    pl::File f;
    f.sphere = 3;
    f.add("program", "zipper3");
    f.add("command", opt.command);
    f.add("example", "cartwright-steger");
    f.add("file", opt.subst);
    f.add("R", pl::exact(opt.R));
    f.add("acc", pl::exact(acc));
    f.add("cap", std::to_string(opt.cap));
    f.add("overlap", std::to_string(opt.overlap));
    f.add("only", opt.which == 1 ? "+" : opt.which == 2 ? "-" : "both");
    f.add("cull", W.cull ? pl::exact(opt.cullk) : string("off"));
    f.add("focus", W.focus ? pl::exact(opt.focus) : string("off"));
    if (W.focus) f.add("roots.main", std::to_string(W.nmain));
    double vm[25];
    for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) vm[4*r + k] = view.Q[r][k];
    for (int i = 0; i < 9; i++) vm[16 + i] = view.rot[i];
    f.add("viewmatrix", pl::exact_list(vm, 25, ','));
    f.add("zoom", pl::exact(view.zoom));
    f.add("clip", pl::exact(view.clip));
    f.add("mark", "t+ " + pl::exact_list(W.plus.V.x, 4));
    f.add("mark", "t- " + pl::exact_list(W.minus.V.x, 4));
    f.add("capped", string("Z+ ") + (W.rp->capped ? "yes" : "no"));
    f.add("capped", string("Z- ") + (W.rn->capped ? "yes" : "no"));
    f.layers.push_back(pl::layer_of("Z+", *W.rp));
    f.layers.push_back(pl::layer_of("Z-", *W.rn));
    if (!pairs.empty()) {
        f.add("capped", "pair.Z+ no");
        f.add("capped", "pair.Z- no");
        f.add("pairs", std::to_string(pairs.size()));
        pl::Layer a, b;
        a.name = "pair.Z+";
        b.name = "pair.Z-";
        for (size_t k = 0; k < pairs.size(); k++) {
            const LinkedPair<PU21>& p = pairs[k];
            const string K = "pair." + std::to_string(k) + ".";
            f.add(K + "pole", pl::exact_list(p.pole, 4));
            f.add(K + "clip", pl::exact(p.clip));
            f.add(K + "lk", std::to_string(p.lk));
            f.add(K + "line", pair_desc(p, (int)k));
            for (pl::Layer* L : {&a, &b}) {
                const Renderer<PU21>& r = *(L == &a ? p.a : p.b);
                L->first.push_back((uint32_t)(L->pts.size()/4));
                L->root.push_back(-1);
                L->letter.push_back(-1);
                L->pts.insert(L->pts.end(), r.pts.begin(), r.pts.end());
            }
        }
        f.layers.push_back(std::move(a));
        f.layers.push_back(std::move(b));
    }
    f.rootd = W.rootd;
    string err;
    if (!pl::write(opt.polylines, f, &err)) { fprintf(stderr, "%s\n", err.c_str()); return false; }
    fprintf(stderr, "wrote %s\n", opt.polylines.c_str());
    return true;
}

static void usage()
{
    fprintf(stderr,
"usage: zipper3 [options]\n"
"  -f FILE      substitution file from `chz kernel` (default subst_4.00_9.50_t3_r1.txt; with --pairs subst_4.00_9.50_t3.txt:\n"
"               the linked pairs are loops of its base arcs); a name without '/' is looked for in the working folder,\n"
"               then in the folders of ZIPPERS_DATA, then in the data of the suite (data/s3/), as is that of --pairs\n"
"  -R R         roots: the elements g with d(o, g o) <= R (default 0: the base arcs only;\n"
"               the first shells of translates are at 2.553, 3.326, 3.983, ...)\n"
"  -a ACC       accuracy: maximal chord on the unit S^3 (default 0.01)\n"
"  -o FILE.png  write a PNG (software rendering, depth-cued)\n"
"  -v           interactive viewer\n"
"  -s SIZE      image / window size in pixels (default 900)\n"
"  --only +|-   draw only Z+ or only Z-\n"
"  --clip RHO   clip the stereographic picture to the ball of radius RHO (default 3)\n"
"  --zoom F     magnify the picture about its centre (default 1); a still (-o) with F > 1 is drawn with culling and the\n"
"               focus ball (--cull --focus 1) unless --nocull and --nofocus say otherwise; not with -v or --pairs\n"
"  --yaw D --pitch D     rotate R^3 (degrees; default 0, 20)\n"
"  --pole D1 D2          move the pole of the projection (degrees, along E1 then E2)\n"
"  --cap N      stop after N leaves per zipper (default 8e6)\n"
"  --check      report the smallest distance between the drawn Z+ and Z- (must stay > 0)\n"
"  --stats      check the roots (closure under S, descent; exact): one line on stderr, and a second near a shell\n"
"  --overlap L  mark the sub-branches of each leaf down to L levels as drawn, 0..16 (default 0: off)\n"
"  --centre P   centre the view at the point P = x1,x2,x3,x4 of S^3 (normalised)\n"
"  --cull       draw only the branches that may meet the window (margin K x size); on by default for a still (-o) with\n"
"               --zoom above 1, off otherwise\n"
"  --cullk K    the margin factor K (default 16: the K = 16 column of `cull --mode margin`, the largest stray among the\n"
"               branches of size below 1/16, is 3.3 sizes on the default file at R up to 4.5; the branches of the S^3\n"
"               file _t1 stray up to 8.9 sizes, so use --cullk 32 there); implies --cull\n"
"  --nocull     do not cull by default (refused with --cull, --cullk and --zoomacc)\n"
"  --zoomacc    with culling: draw at ACC/zoom on the sphere (the accuracy of zoom 1 on the screen)\n"
"  --focus M    with --zoom: add the ball about an orbit point near M log(zoom) towards the view centre; M = 1 by\n"
"               default for a still (-o) with --zoom above 1\n"
"  --nofocus    do not add the focus ball by default (refused with --focus)\n"
"  --pairs FILE show linked pairs of loops (from `link3 --export`); n / N step through them,\n"
"               l shows the base arcs faintly; with -o, draw pair number --pairindex K\n"
"               (--context: with the base arcs faintly behind it)\n"
"  --thick N    PNG: thicker lines\n"
"  --movie DIR  play the built-in demonstration script (needs --pairs) and save its frames to DIR\n"
"  --moviescript 1|2|3|FILE   which script: built-in, or a file with one segment per line,\n"
"               FRAMES R PAIR ARCS YAW POLE CAPTION  (R -1: keep the roots; PAIR -1: the zippers,\n"
"               -2: a blank frame with the caption; YAW, POLE in degrees per frame; caption '-': none)\n"
"  --moviestart N   number the frames from N (default 0)\n"
"  --polylines FILE  write the curves as data (a polyline file): Z+, Z-, and with --pairs the loops of every pair\n"
"  --viewmatrix N1,...,N25   the view as its matrices: the frame of R^4 (16 numbers, row by row), then the rotation\n"
"               of R^3 (9), comma-separated, orthonormal and of determinant +1; in place of --yaw, --pitch,\n"
"               --pole and --centre\n"
"  --colour L=R,G,B  the colour of L, three numbers in [0, 1]: Z+ and Z- the zippers (0.05,0.15,0.85 and\n"
"               0.85,0.08,0.08), pair.Z+ and pair.Z- a pair's loops (the same), context.Z+ and context.Z- the\n"
"               zippers behind a pair (--context: 0.70,0.75,0.95 and 0.95,0.72,0.72); a name the picture does not\n"
"               draw changes nothing; given twice, the last counts\n");
}

int main(int argc, char** argv)
{
    cs::setup();
    Options opt;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { usage(); exit(1); } return argv[++i]; };
        if (a == "-f") { opt.subst = next(); opt.subst_given = true; }
        else if (a == "-R") opt.R = atof(next().c_str());
        else if (a == "-a") opt.acc = atof(next().c_str());
        else if (a == "-o") opt.out = next();
        else if (a == "-v") opt.viewer = true;
        else if (a == "-s") opt.size = atoi(next().c_str());
        else if (a == "--only") { string w = next(); opt.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--clip") opt.clip = atof(next().c_str());
        else if (a == "--zoom") opt.zoom = atof(next().c_str());
        else if (a == "--yaw") { opt.yaw = atof(next().c_str()); opt.view_options = true; }
        else if (a == "--pitch") { opt.pitch = atof(next().c_str()); opt.view_options = true; }
        else if (a == "--pole") { opt.pole1 = atof(next().c_str()); opt.pole2 = atof(next().c_str()); opt.view_options = true; }
        else if (a == "--cap") opt.cap = (size_t)atof(next().c_str());
        else if (a == "--check") opt.check = true;
        else if (a == "--stats") opt.stats = true;
        else if (a == "--overlap") {
            string v = next();
            if (v.empty() || v.size() > 2 || v.find_first_not_of("0123456789") != string::npos || atoi(v.c_str()) > 16) {
                usage(); return 1;
            }
            opt.overlap = atoi(v.c_str());
        }
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
        else if (a == "--centre") {
            vector<double> v;
            if (!parse_numbers(next(), 4, v)) { usage(); return 1; }
            double nn = v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3];
            if (!(nn > 0)) { usage(); return 1; }
            for (int k = 0; k < 4; k++) opt.c[k] = v[k]/std::sqrt(nn);
            opt.centre = true;
            opt.view_options = true;
        }
        else if (a == "--cull") opt.cull = true;
        else if (a == "--cullk") {
            opt.cullk = atof(next().c_str());
            if (!(opt.cullk >= 0)) { usage(); return 1; }
            opt.cull = true;
        }
        else if (a == "--zoomacc") opt.zoomacc = true;
        else if (a == "--focus") {
            opt.focus = atof(next().c_str());
            if (!(opt.focus > 0)) { usage(); return 1; }
        }
        else if (a == "--nocull") opt.nocull = true;
        else if (a == "--nofocus") opt.nofocus = true;
        else if (a == "--polylines") opt.polylines = next();
        else if (a == "--viewmatrix") {
            vector<double> v;
            bool ok = parse_numbers(next(), 25, v);
            for (size_t k = 0; ok && k < v.size(); k++) ok = std::isfinite(v[k]);
            if (!ok) {
                fprintf(stderr, "--viewmatrix: 25 numbers, comma-separated: the frame of R^4 (16, row by row), then the "
                        "rotation of R^3 (9, row by row)\n");
                return 1;
            }
            for (int k = 0; k < 25; k++) opt.vm[k] = v[k];
            opt.viewmatrix = true;
        }
        else if (a == "--colour") {
            string v = next(), name;
            double c[3];
            int k = -1;
            if (viewopt::parse_colour(v, name, c))
                for (int j = 0; j < 6; j++) if (name == COLOUR_NAMES[j]) k = j;
            if (k < 0) {
                fprintf(stderr, "--colour %s: a name (Z+, Z-, pair.Z+, pair.Z-, context.Z+ or context.Z-) and three numbers "
                        "in [0, 1], as Z+=0.05,0.15,0.85\n", v.c_str());
                return 1;
            }
            for (int j = 0; j < 3; j++) opt.colour[k][j] = c[j];
            opt.colour_given[k] = true;
        }
        else { usage(); return 1; }
    }
    opt.command = polylines::command_line(argc, argv);
    if (!opt.polylines.empty() && !polylines::recordable(opt.command)) {
        fprintf(stderr, "--polylines: an argument holds a line break, which the command line in the file's header cannot "
                "hold\n");
        return 1;
    }
    if (!opt.polylines.empty()) {                        // a file that cannot be written: refused before the build
        const string wp = polylines::write_problem(opt.polylines);
        if (!wp.empty()) { fprintf(stderr, "cannot write %s: %s\n", opt.polylines.c_str(), wp.c_str()); return 1; }
    }
    // --viewmatrix: the whole view, orthonormal, of determinant +1
    if (opt.viewmatrix && opt.view_options) {
        fprintf(stderr, "--viewmatrix gives the whole view: it does not go with --yaw, --pitch, --pole or --centre\n");
        return 1;
    }
    if (opt.viewmatrix) {
        double eq = viewopt::orthonormal_error(opt.vm, 4), er = viewopt::orthonormal_error(opt.vm + 16, 3);
        if (!(eq <= viewopt::VIEWMATRIX_TOL) || !(er <= viewopt::VIEWMATRIX_TOL)) {
            fprintf(stderr, "--viewmatrix: the %s is not orthonormal: the largest entry of |M M^T - I| is %g, above %g\n",
                    !(eq <= viewopt::VIEWMATRIX_TOL) ? "frame of R^4 (the first 16 numbers)" : "rotation of R^3 (the last 9)",
                    !(eq <= viewopt::VIEWMATRIX_TOL) ? eq : er, viewopt::VIEWMATRIX_TOL);
            return 1;
        }
        const double dq = viewopt::determinant(opt.vm, 4), dr = viewopt::determinant(opt.vm + 16, 3);
        if (dq < 0 || dr < 0) {
            fprintf(stderr, "--viewmatrix: the %s has determinant -1: its picture would be the mirror image, on which every "
                    "linking number has the other sign; a view is a rotation (determinant +1)\n",
                    dq < 0 ? "frame of R^4 (the first 16 numbers)" : "rotation of R^3 (the last 9)");
            return 1;
        }
    }
    // --nocull and --nofocus turn off what a zoomed still has by default; they contradict the options that turn it on
    if (opt.nocull && (opt.cull || opt.zoomacc)) {
        fprintf(stderr, "--nocull does not go with --cull, --cullk or --zoomacc\n");
        return 1;
    }
    if (opt.nofocus && opt.focus > 0) { fprintf(stderr, "--nofocus does not go with --focus\n"); return 1; }
    // the file of --pairs: the one whose base arcs the pairs are loops of
    if (!opt.subst_given && !opt.pairs.empty()) opt.subst = DEFAULT_PAIRS_SUBST;
    // a still (-o) of a zoomed view (--zoom F, F > 1) behaves as if --cull and --focus 1 had been given, unless an
    // option says otherwise. Not for the viewer (-v, --snap, --movie), not for --pairs (whose pictures have views of their
    // own, and which refuses --cull), and not without -o or at zoom 1: nothing is on by default there.
    const bool zoomed_still = !opt.out.empty() && opt.zoom > 1 && !opt.viewer && opt.pairs.empty();
    if (zoomed_still && !opt.cull && !opt.nocull) opt.cull = opt.cull_default = true;
    if (zoomed_still && !(opt.focus > 0) && !opt.nofocus) { opt.focus = 1; opt.focus_default = true; }
    if (opt.zoomacc && !opt.cull) { fprintf(stderr, "--zoomacc needs --cull\n"); return 1; }
    if (opt.focus > 0 && !(opt.zoom > 1)) { fprintf(stderr, "--focus needs --zoom F with F > 1\n"); return 1; }
    if (opt.cull && !opt.pairs.empty()) { fprintf(stderr, "--cull is not for --pairs\n"); return 1; }
    // the files: the working folder, then ZIPPERS_DATA, then the data of the suite (core/data_files.h)
    if (!find_data(opt.subst, "s3", ZIPPERS_DATA_PLACES, opt.subst_given ? nullptr : "the default substitution file",
                   "-f"))
        return 1;
    if (!opt.pairs.empty() && !find_data(opt.pairs, "s3", ZIPPERS_DATA_PLACES, "the file of linked pairs", "--pairs"))
        return 1;
    World<PU21> W;
    prepare<cs::RenderPolicy>(opt.subst.c_str(), W);
    // the view of the zippers' picture (before the build, for culling and the focus ball; it prints nothing)
    s3::ViewState view;
    s3::init_view(W, view, opt.yaw, opt.pitch, opt.pole1, opt.pole2, opt.zoom, opt.clip);
    if (opt.centre) {                          // the frame of the point c, then the poles
        SpherePoint<4> cp = {{opt.c[0], opt.c[1], opt.c[2], opt.c[3]}};
        s3::default_frame(cp, view.Q);
        double e1[3] = {1,0,0}, e2[3] = {0,1,0};
        s3::move_pole(view.Q, e1, opt.pole1*s3::PI/180);
        s3::move_pole(view.Q, e2, opt.pole2*s3::PI/180);
    }
    if (opt.viewmatrix) {                      // the view as given: the frame, then the rotation of R^3
        for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) view.Q[r][k] = opt.vm[4*r + k];
        for (int i = 0; i < 9; i++) view.rot[i] = opt.vm[16 + i];
    }
    // say which defaults are on, and how to turn them off (a line each, before the build's own lines)
    if (opt.cull_default)
        fprintf(stderr, "note: culling is on by default for a zoomed still (K = %g; --nocull turns it off)\n", opt.cullk);
    if (opt.focus_default)
        fprintf(stderr, "note: the focus ball is on by default for a zoomed still (--focus %g; --nofocus turns it off)\n",
                opt.focus);
    CullWindow<4> window = s3::window3(view, opt.cullk);
    if (opt.cull) W.cull = &window;            // culling
    if (opt.focus > 0) {                       // the focus ball
        double xi[4];                          // the centre of the view: the point opposite the pole
        for (int k = 0; k < 4; k++) xi[k] = -view.Q[3][k];
        cs::FocusCentre3 fc = cs::focus_centre3(xi, opt.focus*std::log(opt.zoom), cs::geogens(4.5));
        W.focus = true;
        W.fh = fc.h;
        fprintf(stderr, "focus: L = %.4f (%g log zoom); h o at d(o, h o) = %.4f, d(h o, o_F) = %.4f (h: %d steps of the "
                "walk)\n", fc.L, opt.focus, fc.disp, fc.dist, fc.steps);
    }
    double acc = opt.zoomacc ? opt.acc/opt.zoom : opt.acc;
    default_marking.overlap = opt.overlap;     // the renderers that build makes start with it (render/renderer.h)
    build<cs::RenderPolicy>(W, opt.R, acc, opt.cap, opt.which);
    default_marking = Marking();               // (only the zippers' renderers: not those of --pairs)
    if (W.focus)
        fprintf(stderr, "focus: %zu roots in B(R), %zu more from h B(R)\n", W.nmain, W.roots.size() - W.nmain);
    if (opt.overlap > 0)
        for (Renderer<PU21>* r : {W.rp, W.rn})
            if (r->nVisited)
                fprintf(stderr, "%s: --overlap %d: %zu sub-branch keys marked as drawn\n", r->sd.name, opt.overlap,
                        r->nMarked);
    if (opt.stats) {      // the search of the roots made again and checked; its roots must be those build drew, bit for bit
        // with --focus the roots drawn are B(R), the first W.nmain, and then h B(R): the checks are of B(R)
        vector<PU21::Elem> mainroots;
        vector<double> maind;
        if (W.focus) {
            mainroots.assign(W.roots.begin(), W.roots.begin() + (long)W.nmain);
            maind.assign(W.rootd.begin(), W.rootd.begin() + (long)W.nmain);
        }
        for (const string& line : cs::stats_lines(opt.R, W.focus ? mainroots : W.roots, W.focus ? maind : W.rootd,
                                                  "the roots drawn", "build drew"))
            fprintf(stderr, "%s\n", line.c_str());
        if (W.focus)
            fprintf(stderr, "roots: the checks are of B(R), the first %zu roots; the other %zu roots drawn are h B(R), the "
                    "focus ball\n", W.nmain,
                    W.roots.size() - W.nmain);
    }
    vector<LinkedPair<PU21>> pairs;
    if (!opt.pairs.empty()) {
        if (!load_pairs(opt.pairs, pairs)) return 1;
        double t0 = now();
        for (auto& p : pairs) build_pair<cs::RenderPolicy>(p, W, opt.acc);
        fprintf(stderr, "built the %zu pairs of loops in %.2fs\n", pairs.size(), now() - t0);
    }
    if (opt.check && W.rp->nLeaves && W.rn->nLeaves) {
        if (opt.cull)
            fprintf(stderr, "check: with --cull the minimum distance below is over the culled picture, the part of the zippers "
                    "near the window: the whole picture's may be smaller\n");
        // fine cells first: a pair at distance d is found once the cell size h exceeds d + acc
        SpherePoint<4> wa, wb;
        double t0 = now(), d = 1, h;
        for (h = 3*opt.acc; h < 0.8; h *= 2) {
            d = zdist(*W.rp, *W.rn, h, wa, wb);
            if (d < h - opt.acc) break;
        }
        if (h < 0.8) fprintf(stderr, "check: min distance Z+ to Z- (vertex to segment, on S^3) = %.5f  (%.2fs)  at Z+ point (%.4f %.4f %.4f %.4f)\n",
                             d, now() - t0, wa.x[0], wa.x[1], wa.x[2], wa.x[3]);
        else fprintf(stderr, "check: min distance Z+ to Z- is at least %.2f\n", h/2 - opt.acc);
    }
    if (!opt.polylines.empty() && !write_polylines(opt, W, view, acc, pairs)) return 1;
    if (opt.viewer) {
        fprintf(stderr, "built without the viewer\n");
        return 1;
    }
    if (!opt.out.empty() && !pairs.empty()) {             // a picture of one linked pair
        const LinkedPair<PU21>& p = pairs[opt.pairindex % pairs.size()];
        s3::ViewState v;
        s3::init_view(W, v, opt.yaw, opt.pitch, opt.pole1, opt.pole2, opt.zoom, opt.clip);
        if (opt.viewmatrix) v = view;          // the view as given, with --zoom and --clip
        else pair_view(p, v);
        vector<s3::Layer<PU21>> layers;
        if (opt.context) {
            const double* c0 = colour_of(opt, 4, CONTEXT_PLUS);
            const double* c1 = colour_of(opt, 5, CONTEXT_MINUS);
            layers.push_back({W.rp, {c0[0], c0[1], c0[2]}, 0});
            layers.push_back({W.rn, {c1[0], c1[1], c1[2]}, 0});
        }
        const double* c2 = colour_of(opt, 2, PLUS);
        const double* c3 = colour_of(opt, 3, MINUS);
        if (opt.which & 1) layers.push_back({p.a, {c2[0], c2[1], c2[2]}});
        if (opt.which & 2) layers.push_back({p.b, {c3[0], c3[1], c3[2]}});
        if (!s3::write_png(opt.out.c_str(), opt.size, layers, v, {}, opt.thick ? opt.thick : 2)) { fprintf(stderr, "cannot write %s\n", opt.out.c_str()); return 1; }
        fprintf(stderr, "%s\nwrote %s\n", pair_desc(p, opt.pairindex % pairs.size()).c_str(), opt.out.c_str());
        return 0;
    }
    if (!opt.out.empty()) {
        const s3::ViewState& v = view;
        vector<s3::Layer<PU21>> layers;
        const double* c0 = colour_of(opt, 0, PLUS);
        const double* c1 = colour_of(opt, 1, MINUS);
        if (opt.which & 1) layers.push_back({W.rp, {c0[0], c0[1], c0[2]}});
        if (opt.which & 2) layers.push_back({W.rn, {c1[0], c1[1], c1[2]}});
        vector<SpherePoint<4>> marks = {W.plus.V, W.minus.V};
        if (!s3::write_png(opt.out.c_str(), opt.size, layers, v, marks, opt.thick)) { fprintf(stderr, "cannot write %s\n", opt.out.c_str()); return 1; }
        fprintf(stderr, "wrote %s\n", opt.out.c_str());
    }
    return 0;
}
