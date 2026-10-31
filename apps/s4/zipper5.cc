/*  zipper5.cc  --  draw approximations to the zippers Z+ and Z- on the 4-sphere at infinity of
    the fibered hyperbolic 5-manifold of Italiano-Martelli-Migliorini (rh5.h, zipcore5.h).

    Usage: zipper5 [options] -o FILE.png, a still; the options are in usage() below (zipper5 -h).

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

    The program of legacy/s4/zipper5.cc on the shared library src/, with the same options and files, and the
    same output except for the two changes below:
    the example's data from examples/imm_m5.h (rh5.h there), its render policy from examples/imm_m5_render.h, the
    renderer from render/ and the view and painter from view/s4.h (zipcore5.h and write_png5 there). The changes are
    the includes and the names: imm::setup (rh5_setup), World<imm::O51> with prepare<imm::RenderPolicy> and
    build<imm::RenderPolicy> (World5, prepare5, build5), and the view's names in zippers::s4.

    One difference of output: the word reader of examples/imm_m5.h reads
    facet 1, which the legacy reader dropped from every word, so this program draws the true generators (44 base
    arcs of subst5_2.30_4.50_t1.txt, where the legacy program draws 47). Its reference runs are recorded from it.

    A second: the render policy draws each leaf of a letter on a parabolic cycle of the
    roofs through the cusp points of its cycles (render/tips.h), so that the spikes into the cusps are drawn to their
    tips. The same leaves are drawn, with more points; the build lines say how many ("N points at tips"). The
    reference runs are recorded from it.

    --stats (the checks of the roots, examples/imm_m5.h: certify_roots5). Without it nothing that
    the program prints or writes changes.

    --overlap L (default 0): the renderer's marking (render/renderer.h): when a leaf is accepted, the keys
    of its sub-branches down to L levels are marked as drawn, as the S^2 program's --overlap L does. It is off by
    default, and with L = 0 nothing that the program prints or writes changes; with L >= 1 the build lines are followed
    by one line of marking counts. In S^4 marking is not proved safe (there are no enclosures): its costs are in
    docs/developer_guide.md, "Marking and overdraw".

    The usage text (usage(): -h, and after an unknown option) is new; the legacy program had none.

    THE DEEP ZOOM (the view is set up before the build, which prints nothing):
      --centre P   the view centred at P (the frame of init_view5 with the pole -P in place of -t^+);
      --cull, --cullk K   culling (render/renderer.h, render/cull.h): the window of the view (view/s4.h: window5) is
                   given to the build (World::cull), whose lines then add ", N culled"; like marking, a measured trade, not a
                   proof; on by default for a still of a zoomed view (THE DEFAULTS, below);
      --zoomacc    the accuracy on the sphere ACC/zoom (it needs culling, which may be the default's: the whole sphere at
                   ACC/zoom would cost about zoom^2 times as much); the build lines print the accuracy used;
      --focus M    the focus ball (render/world.h: World::focus), centred at an orbit point h o of Gamma near the point at
                   distance M log(zoom) towards the centre of the view (examples/imm_m5.h: focus_centre5); one line
                   (stderr) says what h is and how far h o is from that point. Only with --zoom > 1; M = 1 is the default
                   of a still of a zoomed view.
    One note on stderr, only when the focus ball is on, asked for or by default: --stats with the focus ball checks B(R),
    the first roots (W.nmain), and says that the others are h B(R). (zipper5 has no --check.)

    THE DEFAULTS.
      the file      -f defaults to subst5_2.30_4.50_t1_r0.txt (data/s4/: chz5 kernel 2.3 4.5 3 --roofs, the same t, so the same
                    view, with the roofs chosen at no extra length: the two orientations of every involution arc are one
                    arc), in place of subst5_2.30_4.50_t1.txt (legacy/s4/, copied into data/s4/, which -f still reads, and
                    which film5 keeps). The file is found by the search of core/data_files.h: a name with a '/' as given;
                    else the working folder, the folders of ZIPPERS_DATA, then data/ of the source tree (installed:
                    share/zippers/); the first place that holds an entry of the name is used, and an entry there that
                    is not a readable file stops the program, which says what is there; when it is found nowhere the
                    message says where it looked. A file found in the
                    working folder keeps its name, so the program prints what it did when it read the working folder only;
      zoomed stills with -o and --zoom F, F > 1, the program behaves as if --cull and --focus 1 had been given (K = CULL_K,
                    render/cull.h), unless --cull, --cullk K or --focus M says otherwise or --nocull / --nofocus turns the
                    default off; a note on stderr says which default is on. Without -o or at zoom 1 culling and the focus are
                    off unless asked for. --nocull is refused with --cull, --cullk and --zoomacc, --nofocus with --focus.
                    --zoomacc needs culling, which may be the default's.

    THREE OPTIONS FOR OTHER PROGRAMS (a window, among them), which change nothing when they are not given:
      --polylines FILE   the curves as data, a polyline file (output/polylines.h; write_polylines below says what its header
                    holds): the layers Z+ and Z-, the renderers' points with the root and the letter of each polyline; for
                    each layer, whether its renderer reached the cap of leaves (so its curves are incomplete). Written
                    after the build (and --stats), before the picture; without -o it is the only output. An argument that
                    holds a line break is refused at the start, since the header's command line cannot hold it, and so is
                    a FILE that cannot be written (a folder of the name, a folder that may not be written in).
      --viewmatrix N1,...,N34   the view as its matrices: the frame Q of R^5 (25 numbers, row by row) and the rotation of
                    R^3 (9 numbers, row by row), comma-separated, in place of the view of --yaw, --pitch and --centre (which
                    it does not go with); the hidden direction U is still that of --turn and --turn2, and --zoom, --clip
                    and --hidfog are as usual. Both must be orthonormal to within 1e-9 (view/options.h); either
                    determinant is accepted: the program's own frames have determinant -1, and since the picture does not
                    see the sign of the hidden coordinate (it is a fog, even in it), a reflection of the frame or of the
                    rotation draws what a rotation of the frame draws: a mirror image of a view is itself a view that the
                    program makes (--turn2 180 draws the mirror image of the default view). The view is then exactly the
                    one given, bit for bit (the header of a polyline file gives the view drawn in these numbers).
      --colour L=R,G,B   the colour of the zippers Z+ or Z- in the picture, three numbers in [0, 1]; when one is given
                    twice, the last one counts. The defaults are the colours the program always had, so the same numbers
                    given again draw the same picture, byte for byte.
*/

#include "core/data_files.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "render/world.h"
#include "output/polylines.h"
#include "view/options.h"
#include "view/s4.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace zippers;
using std::string;
using std::vector;

// the default substitution file (data/s4/): the roofs chosen at no extra length
static const char* const DEFAULT_SUBST = "subst5_2.30_4.50_t1_r0.txt";

struct Opt5 {
    string subst = DEFAULT_SUBST;
    bool subst_given = false;      // -f was given (the default file is then not the program's business)
    double R = 0, acc = 0.02, clip = 3, zoom = 1, yaw = 0, pitch = 15, turn = 0, turn2 = 0, hidfog = 1.5;
    int size = 840, which = 3, thick = 0;
    bool stats = false;
    int overlap = 0;               // --overlap L: the renderer's marking (render/renderer.h), 0 = off
    size_t cap = 20000000;
    string out;
    // the deep zoom: off unless asked for, or the default of a zoomed still (below)
    bool centre = false;           // --centre: the view centred at c
    double c[5] = {0, 0, 0, 0, 0};
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
    double vm[34] = {0};           // Q row by row, then the rotation row by row
    bool view_options = false;     // --yaw, --pitch or --centre was given
    bool colour_given[2] = {false, false};                   // --colour: Z+, Z-
    double colour[2][3] = {{0, 0, 0}, {0, 0, 0}};
};

// the colours of the picture, unless --colour gives others
static const double PLUS[3] = {0.05, 0.15, 0.85}, MINUS[3] = {0.85, 0.08, 0.08};

// --polylines FILE: the curves as data (output/polylines.h). The layers Z+ and Z-, the renderers' points with the root and
// the letter of each polyline (roots: W.rootd, the distances of the roots; with the focus ball, the first roots.main are
// B(R)'s). The header: the program and its command line, the example, the file (as found), R, the accuracy drawn, the
// cap, marking, the zippers drawn (only), culling (its K, or off) and the focus ball (its M, or off), the view
// (viewmatrix: the numbers of --viewmatrix; turn and turn2, in degrees, and U, the rotation of R^4 that they give, row by
// row; zoom; clip; hidfog), the marks t+ and t- (mark NAME x1 ... x5), and for each layer whether its renderer reached
// the cap of leaves (capped NAME yes|no).
static bool write_polylines(const Opt5& o, const World<imm::O51>& W, const s4::View5& v, double acc)
{
    namespace pl = zippers::polylines;
    pl::File f;
    f.sphere = 4;
    f.add("program", "zipper5");
    f.add("command", o.command);
    f.add("example", "imm-m5");
    f.add("file", o.subst);
    f.add("R", pl::exact(o.R));
    f.add("acc", pl::exact(acc));
    f.add("cap", std::to_string(o.cap));
    f.add("overlap", std::to_string(o.overlap));
    f.add("only", o.which == 1 ? "+" : o.which == 2 ? "-" : "both");
    f.add("cull", W.cull ? pl::exact(o.cullk) : string("off"));
    f.add("focus", W.focus ? pl::exact(o.focus) : string("off"));
    if (W.focus) f.add("roots.main", std::to_string(W.nmain));
    double vm[34];
    for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) vm[5*r + k] = v.Q[r][k];
    for (int i = 0; i < 9; i++) vm[25 + i] = v.rot[i];
    f.add("viewmatrix", pl::exact_list(vm, 34, ','));
    f.add("turn", pl::exact(o.turn));
    f.add("turn2", pl::exact(o.turn2));
    f.add("U", pl::exact_list(&v.U[0][0], 16));
    f.add("zoom", pl::exact(v.zoom));
    f.add("clip", pl::exact(v.clip));
    f.add("hidfog", pl::exact(v.hidfog));
    f.add("mark", "t+ " + pl::exact_list(W.plus.V.x, 5));
    f.add("mark", "t- " + pl::exact_list(W.minus.V.x, 5));
    f.add("capped", string("Z+ ") + (W.rp->capped ? "yes" : "no"));
    f.add("capped", string("Z- ") + (W.rn->capped ? "yes" : "no"));
    f.layers.push_back(pl::layer_of("Z+", *W.rp));
    f.layers.push_back(pl::layer_of("Z-", *W.rn));
    f.rootd = W.rootd;
    string err;
    if (!pl::write(o.polylines, f, &err)) { fprintf(stderr, "%s\n", err.c_str()); return false; }
    fprintf(stderr, "wrote %s\n", o.polylines.c_str());
    return true;
}

// a comma-separated list of numbers; false unless it has n0 or n1 of them
static bool parse_numbers(const string& s, size_t n0, size_t n1, vector<double>& v)
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
    return v.size() == n0 || v.size() == n1;
}

static void usage()
{
    fprintf(stderr,
"usage: zipper5 [options] -o FILE.png\n"
"  -f FILE      substitution file from `chz5 kernel` (default subst5_2.30_4.50_t1_r0.txt: `chz5 kernel 2.3 4.5 3 --roofs`,\n"
"               the t of subst5_2.30_4.50_t1.txt with the roofs chosen at no extra length); a name without '/' is looked\n"
"               for in the working folder, then in the folders of ZIPPERS_DATA, then in the data of the suite (data/s4/)\n"
"  -R R         roots: the elements g of Gamma with d(o, g o) <= R (default 0: the base arcs)\n"
"  -a ACC       accuracy: maximal chord on the unit S^4 (default 0.02)\n"
"  -o FILE.png  write a PNG; without it the zippers are built and nothing is written\n"
"  -s SIZE      image size in pixels (default 840)\n"
"  --clip RHO   clip R^4 to the ball of radius RHO (default 3)\n"
"  --zoom F     magnify (default 1); a still (-o) with F > 1 is drawn with culling and the focus ball (--cull --focus 1)\n"
"               unless --nocull and --nofocus say otherwise\n"
"  --yaw D --pitch D     rotate R^3 (degrees; default 0, 15)\n"
"  --turn D     turn the hidden direction: rotate R^4 in the plane of the third visible axis and the hidden one (degrees)\n"
"  --turn2 D    the same in the plane of the first visible axis and the hidden one\n"
"  --hidfog H   the hidden coordinate's fog scale (default 1.5)\n"
"  --only +|-   draw only Z+ or only Z-\n"
"  --thick N    thicker lines\n"
"  --stats      after the build, the checks of the roots (one line, stderr): their number, the elements of W searched, and\n"
"               whether the checks pass (closed under the 16 facet reflections inside the searched ball, and every element\n"
"               descends to 1, both verified exactly)\n"
"  --overlap L  mark the sub-branches of each leaf down to L levels as drawn, 0..16 (default 0: off)\n"
"  --centre P   centre the view at the point P of S^4: five numbers x1,...,x5 (normalised), or six, a null vector\n"
"               x1,...,x6 of R^{5,1} (the point x1/x6, ..., x5/x6), such as a cusp point (\"0,0,0,0,1,1\")\n"
"  --cull       draw only the branches whose curves may meet the window (margin K x size); on by default for a still\n"
"               (-o) with --zoom above 1, off otherwise\n"
"  --cullk K    the margin factor K (default 16: the K = 16 column of `cull --mode margin`, the largest stray among the\n"
"               branches of size below 1/16, is 1.4 sizes on the default file at R up to 2.1); implies --cull\n"
"  --nocull     do not cull by default (refused with --cull, --cullk and --zoomacc)\n"
"  --zoomacc    with culling: draw at ACC/zoom on the sphere, so that the accuracy on the screen is that of zoom 1\n"
"  --focus M    with --zoom: add the focus ball h B(R), about the orbit point h o nearest to the point at distance\n"
"               M log(zoom) from o towards the centre of the view; M = 1 by default for a still (-o) with --zoom above 1\n"
"  --nofocus    do not add the focus ball by default (refused with --focus)\n"
"  --cap N      stop after N leaves per zipper (default 2e7)\n"
"  --polylines FILE  write the curves as data (a polyline file): Z+ and Z-\n"
"  --viewmatrix N1,...,N34   the view as its matrices: the frame of R^5 (25 numbers, row by row), then the rotation\n"
"               of R^3 (9), comma-separated and orthonormal; in place of --yaw, --pitch and --centre\n"
"  --colour L=R,G,B  the colour of the zippers Z+ or Z-, three numbers in [0, 1] (defaults Z+=0.05,0.15,0.85\n"
"               Z-=0.85,0.08,0.08); given twice, the last counts\n");
}

int main(int argc, char** argv)
{
    imm::setup();
    Opt5 o;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(1); } return argv[++i]; };
        if (a == "-f") { o.subst = next(); o.subst_given = true; }
        else if (a == "-R") o.R = atof(next().c_str());
        else if (a == "-a") o.acc = atof(next().c_str());
        else if (a == "-o") o.out = next();
        else if (a == "-s") o.size = atoi(next().c_str());
        else if (a == "--clip") o.clip = atof(next().c_str());
        else if (a == "--zoom") o.zoom = atof(next().c_str());
        else if (a == "--yaw") { o.yaw = atof(next().c_str()); o.view_options = true; }
        else if (a == "--pitch") { o.pitch = atof(next().c_str()); o.view_options = true; }
        else if (a == "--turn") o.turn = atof(next().c_str());
        else if (a == "--turn2") o.turn2 = atof(next().c_str());
        else if (a == "--hidfog") o.hidfog = atof(next().c_str());
        else if (a == "--only") { string w = next(); o.which = w == "+" ? 1 : w == "-" ? 2 : 3; }
        else if (a == "--thick") o.thick = atoi(next().c_str());
        else if (a == "--cap") o.cap = (size_t)atof(next().c_str());
        else if (a == "--stats") o.stats = true;
        else if (a == "--overlap") {
            string v = next();
            if (v.empty() || v.size() > 2 || v.find_first_not_of("0123456789") != string::npos || atoi(v.c_str()) > 16) {
                fprintf(stderr, "--overlap: a number of levels, 0..16\n");
                return 1;
            }
            o.overlap = atoi(v.c_str());
        }
        else if (a == "--centre") {
            vector<double> v;
            if (!parse_numbers(next(), 5, 6, v)) { fprintf(stderr, "--centre: five or six numbers, x1,...\n"); return 1; }
            double d = v.size() == 6 ? v[5] : 0, nn = 0;
            if (v.size() == 6) {
                double q = 0;
                for (int k = 0; k < 5; k++) q += v[k]*v[k];
                if (!(d > 0) || std::fabs(q - d*d) > 1e-9*d*d) { fprintf(stderr, "--centre: not a null vector with x6 > 0\n"); return 1; }
            }
            for (int k = 0; k < 5; k++) nn += v[k]*v[k];
            if (!(nn > 0)) { fprintf(stderr, "--centre: the zero vector\n"); return 1; }
            for (int k = 0; k < 5; k++) o.c[k] = v[k]/std::sqrt(nn);
            o.centre = true;
            o.view_options = true;
        }
        else if (a == "--cull") o.cull = true;
        else if (a == "--cullk") {
            o.cullk = atof(next().c_str());
            if (!(o.cullk >= 0)) { fprintf(stderr, "--cullk: a margin factor K >= 0\n"); return 1; }
            o.cull = true;
        }
        else if (a == "--zoomacc") o.zoomacc = true;
        else if (a == "--focus") {
            o.focus = atof(next().c_str());
            if (!(o.focus > 0)) { fprintf(stderr, "--focus: a multiple M > 0 of log(zoom)\n"); return 1; }
        }
        else if (a == "--nocull") o.nocull = true;
        else if (a == "--nofocus") o.nofocus = true;
        else if (a == "--polylines") o.polylines = next();
        else if (a == "--viewmatrix") {
            vector<double> v;
            bool ok = parse_numbers(next(), 34, 34, v);
            for (size_t k = 0; ok && k < v.size(); k++) ok = std::isfinite(v[k]);
            if (!ok) {
                fprintf(stderr, "--viewmatrix: 34 numbers, comma-separated: the frame of R^5 (25, row by row), then the "
                        "rotation of R^3 (9, row by row)\n");
                return 1;
            }
            for (int k = 0; k < 34; k++) o.vm[k] = v[k];
            o.viewmatrix = true;
        }
        else if (a == "--colour") {
            string v = next(), name;
            double c[3];
            int k = -1;
            if (viewopt::parse_colour(v, name, c)) k = name == "Z+" ? 0 : name == "Z-" ? 1 : -1;
            if (k < 0) {
                fprintf(stderr, "--colour %s: a name (Z+ or Z-) and three numbers in [0, 1], as Z+=0.05,0.15,0.85\n",
                        v.c_str());
                return 1;
            }
            for (int j = 0; j < 3; j++) o.colour[k][j] = c[j];
            o.colour_given[k] = true;
        }
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); usage(); return 1; }
    }
    o.command = polylines::command_line(argc, argv);
    if (!o.polylines.empty() && !polylines::recordable(o.command)) {
        fprintf(stderr, "--polylines: an argument holds a line break, which the command line in the file's header cannot "
                "hold\n");
        return 1;
    }
    if (!o.polylines.empty()) {                          // a file that cannot be written: refused before the build
        const string wp = polylines::write_problem(o.polylines);
        if (!wp.empty()) { fprintf(stderr, "cannot write %s: %s\n", o.polylines.c_str(), wp.c_str()); return 1; }
    }
    // --viewmatrix: the whole view (but U), orthonormal; of either determinant (the top of the file says why)
    if (o.viewmatrix && o.view_options) {
        fprintf(stderr, "--viewmatrix gives the whole view: it does not go with --yaw, --pitch or --centre\n");
        return 1;
    }
    if (o.viewmatrix) {
        double eq = viewopt::orthonormal_error(o.vm, 5), er = viewopt::orthonormal_error(o.vm + 25, 3);
        if (!(eq <= viewopt::VIEWMATRIX_TOL) || !(er <= viewopt::VIEWMATRIX_TOL)) {
            fprintf(stderr, "--viewmatrix: the %s is not orthonormal: the largest entry of |M M^T - I| is %g, above %g\n",
                    !(eq <= viewopt::VIEWMATRIX_TOL) ? "frame of R^5 (the first 25 numbers)" : "rotation of R^3 (the last 9)",
                    !(eq <= viewopt::VIEWMATRIX_TOL) ? eq : er, viewopt::VIEWMATRIX_TOL);
            return 1;
        }
    }
    // --nocull and --nofocus turn off what a zoomed still has by default; they contradict the options that turn it on
    if (o.nocull && (o.cull || o.zoomacc)) {
        fprintf(stderr, "--nocull does not go with --cull, --cullk or --zoomacc\n");
        return 1;
    }
    if (o.nofocus && o.focus > 0) { fprintf(stderr, "--nofocus does not go with --focus\n"); return 1; }
    // a still (-o) of a zoomed view (--zoom F, F > 1) behaves as if --cull and --focus 1 had been given, unless an
    // option says otherwise. Without -o (nothing is drawn) or at zoom 1, nothing is on by default.
    const bool zoomed_still = !o.out.empty() && o.zoom > 1;
    if (zoomed_still && !o.cull && !o.nocull) o.cull = o.cull_default = true;
    if (zoomed_still && !(o.focus > 0) && !o.nofocus) { o.focus = 1; o.focus_default = true; }
    if (o.zoomacc && !o.cull) { fprintf(stderr, "--zoomacc needs --cull\n"); return 1; }
    if (o.focus > 0 && !(o.zoom > 1)) { fprintf(stderr, "--focus needs --zoom F with F > 1\n"); return 1; }
    // the file: the working folder, then ZIPPERS_DATA, then the data of the suite (core/data_files.h)
    if (!find_data(o.subst, "s4", ZIPPERS_DATA_PLACES, o.subst_given ? nullptr : "the default substitution file", "-f"))
        return 1;
    World<imm::O51> W;
    prepare<imm::RenderPolicy>(o.subst.c_str(), W);
    // the view (before the build, for culling and the focus ball; it prints nothing)
    s4::View5 v;
    s4::init_view5(W, v, o.yaw, o.pitch);
    if (o.centre) {                            // the pole opposite the centre
        double p[5];
        for (int k = 0; k < 5; k++) p[k] = -o.c[k];
        s4::orthonormal_frame(p, v.Q);
    }
    if (o.viewmatrix) {                        // the view as given: the frame, then the rotation of R^3
        for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) v.Q[r][k] = o.vm[5*r + k];
        for (int i = 0; i < 9; i++) v.rot[i] = o.vm[25 + i];
    }
    v.clip = o.clip; v.zoom = o.zoom; v.hidfog = o.hidfog;
    s4::turn4(v.U, 2, 3, o.turn*s4::PI/180);
    s4::turn4(v.U, 0, 3, o.turn2*s4::PI/180);
    // say which defaults are on, and how to turn them off (a line each, before the build's own lines)
    if (o.cull_default)
        fprintf(stderr, "note: culling is on by default for a zoomed still (K = %g; --nocull turns it off)\n", o.cullk);
    if (o.focus_default)
        fprintf(stderr, "note: the focus ball is on by default for a zoomed still (--focus %g; --nofocus turns it off)\n",
                o.focus);
    CullWindow<5> window = s4::window5(v, o.cullk);
    if (o.cull) W.cull = &window;              // culling
    if (o.focus > 0) {                         // the focus ball
        double xi[5];                          // the centre of the view: the point opposite the pole
        for (int k = 0; k < 5; k++) xi[k] = -v.Q[4][k];
        imm::FocusCentre fc = imm::focus_centre5(xi, o.focus*std::log(o.zoom));
        W.focus = true;
        W.fh = fc.h;
        fprintf(stderr, "focus: L = %.4f (%g log zoom); h o at d(o, h o) = %.4f, d(h o, o_F) = %.4f (h: %d facets "
                "crossed to the tile of o_F, then %zu to Gamma)\n", fc.L, o.focus, fc.disp, fc.dist, fc.walk,
                fc.word.size() - (size_t)fc.walk);
    }
    double acc = o.zoomacc ? o.acc/o.zoom : o.acc;
    default_marking.overlap = o.overlap;       // the renderers that build makes start with it (render/renderer.h)
    build<imm::RenderPolicy>(W, o.R, acc, o.cap, o.which);
    default_marking = Marking();
    if (W.focus)
        fprintf(stderr, "focus: %zu roots in B(R), %zu more from h B(R)\n", W.nmain, W.roots.size() - W.nmain);
    if (o.overlap > 0)
        for (Renderer<imm::O51>* r : {W.rp, W.rn})
            if (r->nVisited)
                fprintf(stderr, "%s: --overlap %d: %zu sub-branch keys marked as drawn\n", r->sd.name, o.overlap,
                        r->nMarked);
    if (o.stats) {
        // the search of the roots made again and checked; its roots must be those that build drew
        // with --focus the roots drawn are B(R), the first W.nmain, and then h B(R): the checks are of B(R)
        const size_t nr = W.focus ? W.nmain : W.roots.size();
        imm::RootCertificate c = imm::certify_roots5(o.R);
        bool same = c.rootv.size() == nr;
        for (size_t i = 0; same && i < c.rootv.size(); i++) same = imm::O51::eq(c.rootv[i], W.roots[i]);
        fprintf(stderr, "%s\n", imm::root_certificate_line(c).c_str());
        if (!same)
            fprintf(stderr, "roots: NOT the roots drawn: the checked search kept %zu, build drew %zu (or others)\n",
                    c.rootv.size(), nr);
        std::string m = imm::root_margin_line(c);
        if (!m.empty()) fprintf(stderr, "%s\n", m.c_str());
        if (W.focus)
            fprintf(stderr, "roots: the checks are of B(R), the first %zu roots; the other %zu roots drawn are h B(R), the "
                    "focus ball\n", nr,
                    W.roots.size() - nr);
    }
    if (!o.polylines.empty() && !write_polylines(o, W, v, acc)) return 1;
    if (o.out.empty()) return 0;
    vector<s4::Layer5> layers;
    const double* c0 = o.colour_given[0] ? o.colour[0] : PLUS;
    const double* c1 = o.colour_given[1] ? o.colour[1] : MINUS;
    if (o.which & 1) layers.push_back({W.rp, {c0[0], c0[1], c0[2]}, 1});
    if (o.which & 2) layers.push_back({W.rn, {c1[0], c1[1], c1[2]}, 1});
    vector<imm::O51::Point> marks = {W.plus.V, W.minus.V};
    if (!s4::write_png5(o.out.c_str(), o.size, layers, v, marks, o.thick)) { fprintf(stderr, "cannot write %s\n", o.out.c_str()); return 1; }
    fprintf(stderr, "wrote %s\n", o.out.c_str());
    return 0;
}
