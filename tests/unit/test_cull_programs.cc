// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_cull_programs: the options of the deep zoom of zipper3 and zipper5 take effect, and nothing changes
// without them.
//
// For each program, on a light case (S^3 _t3 and S^4 _t1, R = 0, ACC 0.04: a fraction of a second), the program is run
// and the lines of its build (and its focus lines) are compared word for word with what the library computes in this
// process for the same view, set up as the program sets it up: init_view (S^3) or init_view5 (S^4) with the program's
// defaults, the frame of the --centre point, the zoom and clip; then window3 or window5 with the margin factor, and
// the focus centre for the centre of the view. The counts are not pinned (the library's own counts are the reference,
// so the comparison holds on any machine). The cases:
//   - --zoom 10 alone, with no -o: the lines of the build WITHOUT culling (nothing is drawn, so nothing is on by default;
//     no "culled" in them);
//   - --zoom 10 --centre P --cull: the lines with culling (", N culled", N > 0), K = 16;
//   - the same with --cullk 2: the lines of K = 2, which differ from those of K = 16;
//   - the same with --zoomacc: the lines at ACC/zoom on the sphere;
//   - --zoom 10 --focus 1 (at the default centre t^+): the build lines with the focus ball of the library's focus centre
//     (more roots), and the program's two focus lines;
//   - -o with --cull: the picture is written;
//   - refused, with status 1 and nothing built: --zoomacc without --cull; --focus without a zoom above 1; --centre
//     with the wrong count of numbers; zipper5 --centre with six numbers that are not a null vector; zipper3 --cull
//     with --pairs.
// Further checks (each catches a wrong version of the code that the checks above do not catch):
//   - THE DEFAULT K IS 16: the lines of the program without --cullk are those of the library with K = 16 written as a
//     number (not CULL_K, which the program and the library both read, so that a CULL_K changed to 1 is caught), and
//     both zippers' lines say that they culled something (this catches a build that gives the window to Z+ only: the
//     library's lines come from the same build, so they agree with the program's);
//   - A TURNED VIEW: --yaw, --pitch and --pole (zipper3) and --yaw, --turn and --turn2 (zipper5) all away from 0, so
//     that the view's frame, the rotation of R^3 and (S^4) the hidden-direction rotation U are not the defaults: the
//     program's lines are the library's for the same view (this catches a program that makes the window before it
//     turns the view), and the picture with --cull is the picture without it, byte for byte, in the view as given and
//     in the turned view (the same segments are drawn in the window: this catches a window5 with the transpose of U,
//     which cuts the window's own segments);
//   - --stats WITH --focus checks B(R), the first roots, and says that the others are h B(R): no "NOT the roots
//     drawn" (this catches a comparison of the checked B(R) with all the roots drawn);
//   - zipper3 --check with --cull says that its minimum distance is over the culled picture.
// THE DEFAULTS OF A ZOOMED STILL (-o with --zoom above 1): culling at K = 16 and the focus ball at log zoom are on, as if
// --cull --focus 1 had been given:
//   - with no option, the program's build and focus lines are the library's for K = 16 and the focus ball, a note on stderr
//     says that culling is on (with K = 16, written as a number) and one that the focus ball is, and the picture is the
//     picture of --cull --focus 1, byte for byte (the same computation), and not that of --nocull --nofocus;
//   - --nocull --nofocus gives the lines of no culling and no focus ball (what a zoomed still was before), and no note;
//     --nocull alone leaves the focus ball, --nofocus alone culling, --cullk 2 K = 2 and the focus ball, --cull the focus
//     ball, --focus 2 culling and the focus ball at 2 log zoom: each with the note of the default that is still on;
//   - --zoomacc is accepted (culling is on): the lines at ACC/zoom;
//   - at zoom 1, with no --zoom, at zoom 0.5, with no -o, with -v or --snap (zipper3: the viewer is not built, so the program
//     builds, says so and exits 1) and with --pairs (zipper3) nothing is on by default: no "culled", no focus line, no note;
//   - refused, with status 1 and nothing built: --nocull with --cull, --cullk and --zoomacc, --nofocus with --focus, and
//     --zoomacc where culling is not on (at zoom 1 with -o, or with --nocull).
// THE MARGIN TOOL: cull --mode margin (apps/tools/cull.cc), on a light case, prints for each side a "cullable: stray / size" line
// with a column "(K = 16: size < 1/16)" for the default margin factor CULL_K of the library (whatever it is), besides those of
// K = 2, 4, 8, 16, 32, in increasing order of K, and the largest stray in a column does not grow with K; --cullk 12 adds
// a column for 12; on the machine of the references (Apple arm64) the values of the columns are the stored ones.
// A few seconds. Uses src/ and the programs themselves (ZIPPERS_ZIPPER3, ZIPPERS_ZIPPER5, and the tool, ZIPPERS_CULL: the paths
// of the binaries; ZIPPERS_REPO is the repository).

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "geometry/pu21/pu21.h"
#include "render/cull.h"
#include "render/world.h"
#include "view/s3.h"
#include "view/s4.h"

#include "render_test_support.h"

#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace z = zippers;
using rtest::check;
using rtest::note;
using rtest::section;

namespace {

int run_program(const std::string& cmd, std::string& err)
{
    err.clear();
    FILE* p = popen((cmd + " 2>&1 >/dev/null < /dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) err.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// the build lines ("Z+: R=0.00 acc=...") and the focus lines ("focus: ...") of a run, in order
std::vector<std::string> build_lines(const std::string& err, bool focus = false)
{
    std::vector<std::string> out;
    for (const std::string& line : rtest::split_lines(err))
        if (line.find(": R=") != std::string::npos || (focus && line.rfind("focus: ", 0) == 0)) out.push_back(line);
    return out;
}

std::string first_diff(const std::vector<std::string>& got, const std::vector<std::string>& want)
{
    for (size_t i = 0; i < got.size() || i < want.size(); i++) {
        const std::string g = i < got.size() ? got[i] : "(no line)", w = i < want.size() ? want[i] : "(no line)";
        if (g != w) return "line " + std::to_string(i + 1) + ": the program printed \"" + g + "\", the library gives \"" + w + "\"";
    }
    return "no difference";
}

// the options of a turned view: yaw and pitch (degrees) and two more, as the programs read them: zipper3 --pole a b (the
// pole of the projection moves a degrees along E1, then b along E2), zipper5 --turn a --turn2 b (the hidden-direction
// rotation U). No Turns is the programs' defaults: S^3 yaw 0, pitch 20, poles 0; S^4 yaw 0, pitch 15, turns 0.
struct Turns { double yaw, pitch, a, b; };

// the views as the programs set them up
void program_view(const z::World<z::PU21>& W, const std::vector<double>& c, double zoom, z::s3::ViewState& v,
                  const Turns* t = nullptr)
{
    const Turns d = {0, 20, 0, 0};
    const Turns& u = t ? *t : d;
    z::s3::init_view(W, v, u.yaw, u.pitch, u.a, u.b, zoom, 3);
    if (!c.empty()) {
        double n = 0;
        for (double x : c) n += x*x;
        z::SpherePoint<4> p;
        for (int k = 0; k < 4; k++) p.x[k] = c[k]/std::sqrt(n);
        z::s3::default_frame(p, v.Q);
        double e1[3] = {1,0,0}, e2[3] = {0,1,0};
        z::s3::move_pole(v.Q, e1, u.a*z::s3::PI/180);
        z::s3::move_pole(v.Q, e2, u.b*z::s3::PI/180);
    }
}
void program_view(const z::World<z::imm::O51>& W, const std::vector<double>& c, double zoom, z::s4::View5& v,
                  const Turns* t = nullptr)
{
    const Turns d = {0, 15, 0, 0};
    const Turns& u = t ? *t : d;
    z::s4::init_view5(W, v, u.yaw, u.pitch);
    if (!c.empty()) {
        double n = 0, p[5];
        for (int k = 0; k < 5; k++) n += c[k]*c[k];
        for (int k = 0; k < 5; k++) p[k] = -c[k]/std::sqrt(n);
        z::s4::orthonormal_frame(p, v.Q);
    }
    v.clip = 3; v.zoom = zoom; v.hidfog = 1.5;
    z::s4::turn4(v.U, 2, 3, u.a*z::s4::PI/180);
    z::s4::turn4(v.U, 0, 3, u.b*z::s4::PI/180);
}
z::CullWindow<4> program_window(const z::s3::ViewState& v, double K) { return z::s3::window3(v, K); }
z::CullWindow<5> program_window(const z::s4::View5& v, double K) { return z::s4::window5(v, K); }

// the focus centre for the view, and the program's first focus line
z::M3 program_focus(const z::World<z::PU21>&, const z::s3::ViewState& v, double L, double M, std::string& line)
{
    double xi[4];
    for (int k = 0; k < 4; k++) xi[k] = -v.Q[3][k];
    z::cs::FocusCentre3 fc = z::cs::focus_centre3(xi, L, z::cs::geogens(4.5));
    char buf[400];
    snprintf(buf, sizeof buf, "focus: L = %.4f (%g log zoom); h o at d(o, h o) = %.4f, d(h o, o_F) = %.4f (h: %d steps of "
             "the walk)", fc.L, M, fc.disp, fc.dist, fc.steps);
    line = buf;
    return fc.h;
}
z::imm::M6 program_focus(const z::World<z::imm::O51>&, const z::s4::View5& v, double L, double M, std::string& line)
{
    double xi[5];
    for (int k = 0; k < 5; k++) xi[k] = -v.Q[4][k];
    z::imm::FocusCentre fc = z::imm::focus_centre5(xi, L);
    char buf[400];
    snprintf(buf, sizeof buf, "focus: L = %.4f (%g log zoom); h o at d(o, h o) = %.4f, d(h o, o_F) = %.4f (h: %d facets "
             "crossed to the tile of o_F, then %zu to Gamma)", fc.L, M, fc.disp, fc.dist, fc.walk,
             fc.word.size() - (size_t)fc.walk);
    line = buf;
    return fc.h;
}

// what the library prints for the build of the program's view (culling with factor K if K > 0; the focus ball at
// M log zoom if M > 0), and the culled counts of the two zippers
template <class P, class V>
std::vector<std::string> library_lines(const std::string& file, double R, double acc, const std::vector<double>& c,
                                       double zoom, double K, double M, size_t culled[2], const Turns* turns = nullptr)
{
    typedef typename P::B B;
    z::World<B> W;
    rtest::capture_stderr([&] { z::prepare<P>(file.c_str(), W); });
    V v;
    program_view(W, c, zoom, v, turns);
    z::CullWindow<B::N> win = program_window(v, K);
    if (K > 0) W.cull = &win;
    std::vector<std::string> lines;
    std::string fline;
    if (M > 0) {
        W.focus = true;
        W.fh = program_focus(W, v, M*std::log(zoom), M, fline);
        lines.push_back(fline);
    }
    std::string err = rtest::capture_stderr([&] { z::build<P>(W, R, acc, 10000000, 3); });
    for (const std::string& l : build_lines(err)) lines.push_back(l);
    if (M > 0) {
        char buf[160];
        snprintf(buf, sizeof buf, "focus: %zu roots in B(R), %zu more from h B(R)", W.nmain, W.roots.size() - W.nmain);
        lines.push_back(buf);
    }
    culled[0] = W.rp->nCulled;
    culled[1] = W.rn->nCulled;
    delete W.rp; delete W.rn;
    return lines;
}

// the number N of ", N culled" in a build line, or -1 if the line has none
long culled_in(const std::string& line)
{
    size_t p = line.find(" culled");
    if (p == std::string::npos) return -1;
    size_t q = p;
    while (q > 0 && isdigit((unsigned char)line[q - 1])) q--;
    return q == p ? -1 : atol(line.substr(q, p - q).c_str());
}

// both zippers' build lines (Z+ and Z-) are there and each says that it culled something
bool both_culled(const std::vector<std::string>& got)
{
    int lines = 0;
    bool all = true;
    for (const std::string& l : got)
        if (l.find(": R=") != std::string::npos) {
            lines++;
            if (culled_in(l) <= 0) all = false;
        }
    return lines == 2 && all;
}

// the notes that a run prints on stderr ("note: ..."): which default is on, and how to turn it off
std::vector<std::string> note_lines(const std::string& err)
{
    std::vector<std::string> out;
    for (const std::string& line : rtest::split_lines(err))
        if (line.rfind("note: ", 0) == 0) out.push_back(line);
    return out;
}

// the notes of the two defaults, with the default margin factor written as a number
const char* const NOTE_CULL = "note: culling is on by default for a zoomed still (K = 16; --nocull turns it off)";
const char* const NOTE_FOCUS = "note: the focus ball is on by default for a zoomed still (--focus 1; --nofocus turns it off)";

// nothing of culling or of the focus ball in a run: no ", N culled" in a build line, no focus line, no note
bool plain_run(const std::string& err)
{
    return err.find(" culled") == std::string::npos && err.find("focus:") == std::string::npos && note_lines(err).empty();
}

// a line of stderr without its timings: "built in 0.25s" and "(0.37s)"
std::string mask_timings(const std::string& line)
{
    std::string s = rtest::mask_times(line), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '(') {
            size_t j = i + 1;
            while (j < s.size() && (isdigit((unsigned char)s[j]) || s[j] == '.')) j++;
            if (j > i + 1 && s.compare(j, 2, "s)") == 0) { out += "(<time>s)"; i = j + 1; continue; }
        }
        out += s[i];
    }
    return out;
}

// the stderr of a run, timings masked, without the lines "note: ..." that say which defaults are on
std::string stderr_without_notes(const std::string& err)
{
    std::string out;
    for (const std::string& line : rtest::split_lines(err))
        if (line.rfind("note: ", 0) != 0) out += mask_timings(line) + "\n";
    return out;
}

// --check and --stats print notes of their own when culling or the focus ball is on (the minimum distance is over the culled
// picture; the checks are of B(R), the first roots, and the others are h B(R)). A still of a zoomed view with no option has
// both on: its stderr is that of --cull --focus 1, but for the two notes of the defaults. `extra` is the option (and R), `key`
// a line that the stderr must have, whichever of the two ways the options were given
void default_stderr_case(const char* name, const std::string& base, const std::string& extra, const char* key,
                         const std::string& png)
{
    std::string e1, e2;
    const std::string o = " --zoom 10" + extra;
    const std::string tail = " -s 200 -o '" + png + "'";
    int s1 = run_program(base + o + tail, e1);
    int s2 = run_program(base + o + " --cull --focus 1" + tail, e2);
    check(s1 == 0 && s2 == 0 && e1.find(key) != std::string::npos && note_lines(e1).size() == 2 && note_lines(e2).empty() &&
          stderr_without_notes(e1) == stderr_without_notes(e2),
          "%s%s at a zoomed still with no option: the stderr, but for the two notes, is that of --cull --focus 1, and has \"%s\" "
          "(exit %d, %d)", name, extra.c_str(), key, s1, s2);
    remove(png.c_str());
}

template <class P, class V>
void program_cases(const char* name, const std::string& exe, const std::string& file, const std::vector<double>& c,
                   const std::string& cs, const Turns& turned, const std::string& turnopts, double Rstats)
{
    const double R = 0, acc = 0.04, zoom = 10;
    const double K16 = 16;                     // the default margin factor, as a number: the program and the library both
                                               // read CULL_K, so a changed CULL_K would agree with itself
    const std::string base = "'" + exe + "' -f '" + file + "' -R 0 -a 0.04 --cap 10000000";
    const std::string png = rtest::tmp_root() + "/test_cull_programs_" + name + ".png";
    const std::string out = " -s 200 -o '" + png + "'";
    // opt: the options after the common ones; zoom: the view's; acc: the accuracy on the sphere that the build lines show;
    // K, M: the margin factor (0: no culling) and the multiple of log zoom of the focus ball (0: none) that the lines must
    // show; notes: the notes of the defaults that must be printed (1: culling, 2: the focus ball); small: a zoom so near 1 that
    // the window is wide, so that culling may cull nothing and the focus ball may add no root (the nearest orbit point is o)
    struct Case { std::string opt; double zoom, acc, K, M; bool centre; int notes; const char* what; const Turns* turns;
                  bool small = false; };
    const Case cases[] = {
        // no picture: nothing is on by default
        {" --zoom 10", zoom, acc, 0, 0, false, 0, "no -o: neither culling nor the focus ball", nullptr},
        {" --zoom 10 --centre " + cs + " --cull", zoom, acc, K16, 0, true, 0, "culling, K = 16", nullptr},
        {" --zoom 10 --centre " + cs + " --cull --cullk 2", zoom, acc, 2, 0, true, 0, "culling, K = 2", nullptr},
        {" --zoom 10 --centre " + cs + " --cull --zoomacc", zoom, acc/zoom, K16, 0, true, 0, "culling at ACC/zoom", nullptr},
        {" --zoom 10 --focus 1", zoom, acc, 0, 1, false, 0, "the focus ball at log zoom (at t^+)", nullptr},
        // a turned view
        {" --zoom 10 --centre " + cs + turnopts + " --cull", zoom, acc, K16, 0, true, 0, "culling, K = 16, turned view", &turned},
        {" --zoom 10 --centre " + cs + turnopts + " --cullk 3", zoom, acc, 3, 0, true, 0, "culling, K = 3, turned view", &turned},
        {" --zoom 10 --focus 1" + turnopts, zoom, acc, 0, 1, false, 0, "the focus ball at log zoom, turned view", &turned},
        // a still (-o) of a zoomed view (zoom above 1): culling and the focus ball are on by default, and the lines are
        // those of the library for the same view with K = 16 and the focus ball at log zoom
        {" --zoom 10" + out, zoom, acc, K16, 1, false, 3, "a zoomed still: culling and the focus ball by default", nullptr},
        {" --zoom 10 --cull --focus 1" + out, zoom, acc, K16, 1, false, 0, "a zoomed still, --cull --focus 1 given: no note", nullptr},
        {" --zoom 10 --nocull --nofocus" + out, zoom, acc, 0, 0, false, 0, "a zoomed still, --nocull --nofocus: neither", nullptr},
        {" --zoom 10 --nocull" + out, zoom, acc, 0, 1, false, 2, "a zoomed still, --nocull: the focus ball only", nullptr},
        {" --zoom 10 --nofocus" + out, zoom, acc, K16, 0, false, 1, "a zoomed still, --nofocus: culling only", nullptr},
        {" --zoom 10 --cullk 2" + out, zoom, acc, 2, 1, false, 2, "a zoomed still, --cullk 2: K = 2, and the focus ball", nullptr},
        {" --zoom 10 --cull" + out, zoom, acc, K16, 1, false, 2, "a zoomed still, --cull: the focus ball is still the default", nullptr},
        {" --zoom 10 --focus 2" + out, zoom, acc, K16, 2, false, 1, "a zoomed still, --focus 2: culling, and the focus ball at 2 log zoom", nullptr},
        {" --zoom 10 --zoomacc" + out, zoom, acc/zoom, K16, 1, false, 3, "a zoomed still, --zoomacc: accepted, at ACC/zoom", nullptr},
        {" --zoom 10" + turnopts + out, zoom, acc, K16, 1, false, 3, "a zoomed still, turned view: the defaults", &turned},
        // and at any zoom above 1: the lower bound of the default
        {" --zoom 1.5" + out, 1.5, acc, K16, 1, false, 3, "a zoomed still at zoom 1.5: the defaults", nullptr, true},
        {" --zoom 1.01" + out, 1.01, acc, K16, 1, false, 3, "a zoomed still at zoom 1.01: the defaults", nullptr, true},
        // nothing is on by default at zoom 1 and below, with -o
        {" --zoom 1" + out, 1, acc, 0, 0, false, 0, "zoom 1: neither", nullptr},
        {out, 1, acc, 0, 0, false, 0, "no --zoom: neither", nullptr},
        {" --zoom 0.5" + out, 0.5, acc, 0, 0, false, 0, "zoom 0.5: neither", nullptr},
    };
    size_t culledK[2] = {0, 0};
    for (const Case& k : cases) {
        size_t culled[2] = {0, 0};
        std::vector<std::string> want = library_lines<P, V>(file, R, k.acc, k.centre ? c : std::vector<double>(), k.zoom,
                                                            k.K, k.M, culled, k.turns);
        std::string err;
        remove(png.c_str());
        int st = run_program(base + k.opt, err);
        std::vector<std::string> got = build_lines(err, k.M > 0);
        std::vector<std::string> notes = note_lines(err), want_notes;
        if (k.notes & 1) want_notes.push_back(NOTE_CULL);
        if (k.notes & 2) want_notes.push_back(NOTE_FOCUS);
        bool hascull = false;
        for (const std::string& l : got) if (l.find(" culled") != std::string::npos) hascull = true;
        check(st == 0 && got == want && hascull == (k.K > 0) &&
              (k.K == 0 || k.small || (culled[0] > 0 && culled[1] > 0 && both_culled(got))) && notes == want_notes, "%s%s (%s): exit %d, %zu lines, %zu notes; %s", name, k.opt.c_str(), k.what, st, got.size(),
              notes.size(), got != want ? first_diff(got, want).c_str() : notes != want_notes ? "the notes differ" : "ok");
        if (k.K == K16 && k.acc == acc && !k.turns && k.centre) { culledK[0] = culled[0]; culledK[1] = culled[1]; }
        if (k.K == 2 && k.centre)
            check(culled[0] != culledK[0] || culled[1] != culledK[1], "%s: K = 2 and K = 16 cull differently (%zu, %zu "
                  "against %zu, %zu), so that --cullk is seen", name, culled[0], culled[1], culledK[0], culledK[1]);
        if (k.M > 0 && !k.small) check(want.size() == 4 && want[3].find(" 0 more") == std::string::npos,
                           "%s: the focus ball adds roots (%s)", name, want.size() == 4 ? want[3].c_str() : "?");
    }
    // a picture with culling
    std::string err;
    remove(png.c_str());
    int st = run_program(base + " --zoom 10 --centre " + cs + " --cull -s 200 -o '" + png + "'", err);
    struct stat sb;
    check(st == 0 && stat(png.c_str(), &sb) == 0 && sb.st_size > 0, "%s --cull -o: the picture is written (exit %d)", name,
          st);
    remove(png.c_str());
    // the picture with culling is the picture without it, byte for byte: the segments drawn in the window are the same
    // (culling drops only branches that lie beyond the window by more than K times their size, whose segments draw no
    // pixel), in the view as given and in the turned view. Both pictures have ink (a picture of an empty window would
    // be the same with any window). The painter's sort (raster.h) is not stable, so that equal pictures are not
    // guaranteed by theory; they are equal in every case tried (and these). Neither has the focus ball, which is
    // a default of a zoomed still now: --nocull --nofocus is the picture without culling and --cull --nofocus the one with.
    for (int tv = 0; tv < 2; tv++) {
        const std::string o = " --zoom 10 --centre " + cs + (tv ? turnopts : "") + " -s 200 -o '";
        const std::string pa = rtest::tmp_root() + "/test_cull_programs_" + name + "_whole.png",
                          pb = rtest::tmp_root() + "/test_cull_programs_" + name + "_culled.png";
        remove(pa.c_str());
        remove(pb.c_str());
        int s1 = run_program(base + " --nocull --nofocus" + o + pa + "'", err);
        int s2 = run_program(base + " --cull --nofocus" + o + pb + "'", err);
        const std::string A = rtest::read_file(pa), B = rtest::read_file(pb);
        size_t diff = 0;
        while (diff < A.size() && diff < B.size() && A[diff] == B[diff]) diff++;
        check(s1 == 0 && s2 == 0 && A.size() > 2000 && A == B, "%s%s --cull: the picture with culling is the picture "
              "without it, byte for byte (exit %d, %d; %zu and %zu bytes; the first difference at byte %zu)", name,
              tv ? " in a turned view" : "", s1, s2, A.size(), B.size(), diff);
        remove(pa.c_str());
        remove(pb.c_str());
    }
    // the picture of a zoomed still with no option is the picture of --cull --focus 1 (byte for byte: the same computation
    // twice), at the default centre, where the focus ball adds roots; and it is not the picture of --nocull --nofocus, which
    // has neither (the focus ball adds segments in the window)
    {
        const std::string o = " --zoom 10 -s 200 -o '";
        const std::string pa = rtest::tmp_root() + "/test_cull_programs_" + name + "_default.png",
                          pb = rtest::tmp_root() + "/test_cull_programs_" + name + "_explicit.png",
                          pc = rtest::tmp_root() + "/test_cull_programs_" + name + "_neither.png";
        int s1 = run_program(base + o + pa + "'", err);
        int s2 = run_program(base + " --cull --focus 1" + o + pb + "'", err);
        int s3 = run_program(base + " --nocull --nofocus" + o + pc + "'", err);
        const std::string A = rtest::read_file(pa), B = rtest::read_file(pb), C = rtest::read_file(pc);
        check(s1 == 0 && s2 == 0 && A.size() > 2000 && A == B, "%s: the picture of a zoomed still with no option is the "
              "picture of --cull --focus 1, byte for byte (exit %d, %d; %zu and %zu bytes)", name, s1, s2, A.size(), B.size());
        check(s3 == 0 && C.size() > 2000 && A != C, "%s: the focus ball changes the picture of a zoomed still (exit %d; "
              "%zu and %zu bytes)", name, s3, A.size(), C.size());
        remove(pa.c_str());
        remove(pb.c_str());
        remove(pc.c_str());
    }
    // --stats with --focus checks B(R), the first roots, and says that the others are h B(R): there is no "NOT the
    // roots drawn" (the focus ball's roots are not the checked ball's)
    {
        std::string rs = " -R " + std::to_string(Rstats);
        int s1 = run_program(base + rs + " --zoom 10 --focus 1 --stats", err);
        bool ok1 = err.find("roots:") != std::string::npos && err.find("NOT the roots drawn") == std::string::npos &&
                   err.find("are h B(R), the focus ball") != std::string::npos;
        std::string e1 = err;
        int s2 = run_program(base + rs + " --stats", err);
        bool ok2 = err.find("roots:") != std::string::npos && err.find("NOT the roots drawn") == std::string::npos &&
                   err.find("the focus ball") == std::string::npos;
        check(s1 == 0 && s2 == 0 && ok1 && ok2, "%s --stats with --focus: the checks and the line saying that the "
              "other roots are h B(R), no \"NOT the roots drawn\" (exit %d; %s), and without --focus no focus line (exit "
              "%d)", name, s1, ok1 ? "as it should be" : e1.substr(0, 300).c_str(), s2);
    }
    default_stderr_case(name, base, " -R " + std::to_string(Rstats) + " --stats", "roots: the checks are of B(R), the first", png);
    // zipper3 --check with --cull: its minimum distance is over the culled picture, and it says so (zipper5 has no --check)
    if (std::string(name) == "zipper3") {
        default_stderr_case(name, base, " --check", "check: with --cull the minimum distance below is over the culled picture", png);
        int s1 = run_program(base + " --zoom 10 --centre " + cs + " --cull --check", err);
        bool n1 = err.find("check: with --cull") != std::string::npos && err.find("check: min distance") != std::string::npos;
        std::string e1 = err;
        int s2 = run_program(base + " --zoom 10 --check", err);
        bool n2 = err.find("check: with --cull") == std::string::npos && err.find("check: min distance") != std::string::npos;
        check(s1 == 0 && s2 == 0 && n1 && n2, "%s --check: with --cull a note says that the minimum is over the window "
              "(exit %d; %s), and without --cull there is none (exit %d)", name, s1, n1 ? "it does" : e1.substr(0, 300).c_str(),
              s2);
        // the viewer is not built: -v and --snap build, say so and exit 1; a zoomed picture asked for with them has neither
        // culling nor the focus ball by default
        for (const std::string& v : {std::string(" -v"), " --snap '" + png + "'"}) {
            int sv = run_program(base + " --zoom 10 " + v + " -o '" + png + "'", err);
            check(sv == 1 && err.find("built without the viewer") != std::string::npos && plain_run(err),
                  "%s%s with --zoom 10 and -o: no culling, no focus ball, no note by default (exit %d)", name, v.c_str(), sv);
        }
        // --pairs draws a picture of a linked pair, which has views of its own: nothing is on by default there either
        const std::string pairs = std::string(ZIPPERS_REPO) + "/legacy/s3/linked_pairs.txt";
        remove(png.c_str());
        int sp = run_program(base + " --zoom 10 --pairs '" + pairs + "' -o '" + png + "'", err);
        check(sp == 0 && stat(png.c_str(), &sb) == 0 && plain_run(err),
              "%s --pairs with --zoom 10 and -o: the picture of a pair, no culling, no focus ball, no note (exit %d)", name, sp);
        remove(png.c_str());
    }
    // refused before anything is built
    std::vector<std::string> bad = {" --zoomacc", " --zoom 10 --zoomacc", " --focus 1", " --zoom 1 --focus 1",
                                    " --centre 1,2,3",
                                    // --zoomacc needs culling, which a zoomed still has only if --nocull does not take it off,
                                    // and which a still at zoom 1 does not have
                                    " --zoom 1 --zoomacc" + out, " --zoom 10 --nocull --zoomacc" + out,
                                    // --nocull and --nofocus contradict the options that turn culling and the focus ball on
                                    " --nocull --cull", " --nocull --cullk 4", " --nocull --zoomacc", " --nofocus --focus 1",
                                    " --zoom 10 --nocull --cull" + out, " --zoom 10 --nofocus --focus 1" + out};
    if (std::string(name) == "zipper5") bad.push_back(" --centre 0,0,0,0,1,2");
    if (std::string(name) == "zipper3") bad.push_back(" --cull --pairs /dev/null");
    for (const std::string& b : bad) {
        int s2 = run_program(base + b, err);
        check(s2 == 1 && build_lines(err).empty(), "%s%s is refused (exit %d, %zu build lines)", name, b.c_str(), s2,
              build_lines(err).size());
    }
    remove(png.c_str());
}

// ---- the margin tool: cull --mode margin has a column for the default margin factor ---------------------------------------

// a run of a tool: its standard output and standard error together (stdin closed); the exit status
int run_output(const std::string& cmd, std::string& out)
{
    out.clear();
    FILE* p = popen((cmd + " 2>&1 <&-").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// the columns of a "cullable: stray / size" line of cull --mode margin, "max 0.911 (K = 2: size < 1/2), 0.911 (K = 4: size
// < 1/4), ...": the pairs (K, the largest stray / size among the branches of size below 1/K); wellformed is false if the text
// is not exactly that
std::vector<std::pair<double, double>> margin_columns(const std::string& line, bool& wellformed)
{
    std::vector<std::pair<double, double>> cols;
    wellformed = true;
    size_t p = line.find(" max");
    if (p == std::string::npos) { wellformed = false; return cols; }
    const char* s = line.c_str() + p + 4;
    for (;;) {
        double v = 0, k = 0, kk = 0;
        int n = 0;
        if (sscanf(s, " %lf (K = %lf: size < 1/%lf)%n", &v, &k, &kk, &n) != 3 || n == 0) break;
        if (k != kk) wellformed = false;                      // the same K in both places
        cols.push_back({k, v});
        s += n;
        if (*s == ',') s++;
    }
    while (*s == ' ' || *s == '\n') s++;
    if (*s != 0 || cols.empty()) wellformed = false;
    return cols;
}

// what a run of cull --mode margin on a light case (R = 0, ACC 0.3) shows in its "cullable" lines (one for each side): how many
// there are and whether they are well formed; whether each has a column for the default margin factor CULL_K of the library
// (whatever it is), for each of K = 2, 4, 8, 16, 32 and for extraK (0: none asked for); whether the columns are in increasing
// order of K; and whether the largest stray does not grow with K, since the branches that a larger K can cull (those of size
// below 1/K) are fewer
struct MarginRun {
    int status = -1, lines = 0;
    bool wellformed = true, has_default = true, has_round = true, has_extra = true, increasing = true, nested = true;
    std::vector<std::vector<std::pair<double, double>>> columns;      // the columns (K, stray) of each line
};

MarginRun margin_tool_run(const std::string& exe, const char* geometry, const std::string& file, const std::string& options,
                          double extraK)
{
    MarginRun r;
    std::string out;
    r.status = run_output("'" + exe + "' -e " + geometry + " -f '" + file + "' --mode margin -R 0 -a 0.3 --worst 0" + options, out);
    for (const std::string& line : rtest::split_lines(out)) {
        if (line.rfind("  cullable: stray / size", 0) != 0) continue;
        r.lines++;
        bool wf;
        std::vector<std::pair<double, double>> cols = margin_columns(line, wf);
        r.columns.push_back(cols);
        r.wellformed = r.wellformed && wf;
        auto has = [&](double K) {
            for (const auto& c : cols) if (c.first == K) return true;
            return false;
        };
        r.has_default = r.has_default && has(z::CULL_K);
        for (double K : {2.0, 4.0, 8.0, 16.0, 32.0}) r.has_round = r.has_round && has(K);
        if (extraK > 0) r.has_extra = r.has_extra && has(extraK);
        for (size_t i = 1; i < cols.size(); i++) {
            if (!(cols[i].first > cols[i - 1].first)) r.increasing = false;
            if (!(cols[i].second <= cols[i - 1].second)) r.nested = false;
        }
    }
    return r;
}

// want: the largest stray of the columns K = 2, 4, 8, 16, 32 of Z+ and of Z- as the tool printed them on the machine of the
// references (compared there only: the floating point of other machines can change the last digits, and the branches the
// tool measures)
void margin_tool_case(const char* what, const std::string& exe, const char* geometry, const std::string& file,
                      const double want[2][5])
{
    MarginRun a = margin_tool_run(exe, geometry, file, "", 0);
    check(a.status == 0 && a.lines == 2 && a.wellformed, "%s: cull --mode margin runs and prints a well formed \"cullable\" "
          "line for each of Z+ and Z- (exit %d, %d lines)", what, a.status, a.lines);
    check(a.lines == 2 && a.has_default, "%s: each \"cullable\" line has a column \"(K = %g: size < 1/%g)\" for the default "
          "margin factor CULL_K", what, z::CULL_K, z::CULL_K);
    check(a.lines == 2 && a.has_round && a.increasing && a.nested, "%s: its columns are those of K = 2, 4, 8, 16, 32 and CULL_K, "
          "in increasing order of K, with a largest stray that does not grow with K", what);
#if defined(__APPLE__) && defined(__aarch64__)
    {
        size_t wrong = 0, n = 0;
        for (size_t i = 0; i < 2 && i < a.columns.size(); i++)
            for (int j = 0; j < 5; j++) {
                const double K = 2.0*(1 << j);
                double v = -1;
                for (const auto& c : a.columns[i]) if (c.first == K) v = c.second;
                n++;
                wrong += !(std::fabs(v - want[i][j]) < 5e-4);
            }
        check(n == 10 && wrong == 0, "%s: the largest stray of the columns K = 2, 4, 8, 16, 32 of Z+ and Z- are the stored ones "
              "(%zu of %zu differ)", what, wrong, n);
    }
#else
    (void)want;
#endif
    // --cullk 12 adds a column for 12, and keeps the others and the default
    MarginRun b = margin_tool_run(exe, geometry, file, " --cullk 12", 12);
    check(b.status == 0 && b.lines == 2 && b.wellformed && b.has_extra && b.has_default && b.has_round && b.increasing && b.nested,
          "%s: with --cullk 12 the \"cullable\" lines have a column for 12 as well, and still those of CULL_K and of 2, 4, 8, 16, "
          "32, in increasing order (exit %d, %d lines)", what, b.status, b.lines);
}

// ---- the tools cull and strays on the default files: the K they use, and nothing lost in the window ----------------------

// what cull --mode window prints on a light case (zoom 10): the K of its window line; for each side the leaves with and without
// culling, the segments drawn in the window with and without it (distinct ones), those missing with culling and those only with
// it, and the vertices of the picture without culling in the window that are not vertices of the culled one, and the farthest
// of them from it
struct WindowRun {
    int status = -1, sides = 0;
    double K = -1;
    bool lost = false, empty = false, ineffective = false;
};

WindowRun window_tool_run(const std::string& exe, const char* geometry, const std::string& file, const std::string& centre,
                          double R, double acc, const std::string& options)
{
    WindowRun r;
    std::string out;
    r.status = run_output("'" + exe + "' -e " + geometry + " -f '" + file + "' --mode window -R " + std::to_string(R) + " -a " +
                          std::to_string(acc) + " --centre " + centre + " --zoom 10" + options, out);
    const char key[] = "half-spaces; K = ";
    for (const std::string& line : rtest::split_lines(out)) {
        const char* s = line.c_str();
        size_t p = line.find(key);
        if (line.rfind("window: zoom", 0) == 0 && p != std::string::npos) {
            sscanf(s + p + sizeof key - 1, "%lf", &r.K);
            continue;
        }
        if (line.size() < 3 || line[0] != 'Z' || (line[1] != '+' && line[1] != '-')) continue;
        size_t a = 0, b = 0, c = 0, d = 0, e = 0;
        double f = -1;
        if (sscanf(s, "Z%*c: without culling %zu leaves, %zu points, %zu segments, %zu drawn in the window; with culling %zu "
                      "leaves", &a, &b, &c, &d, &e) == 5) {
            if (!(e < a)) r.ineffective = true;                  // culling culls something
        } else if (sscanf(s, "Z%*c: drawn in the window: %zu distinct without culling, %zu with; MISSING with culling: %zu; "
                             "only with culling: %zu", &a, &b, &c, &d) == 4) {
            r.sides++;
            if (a != b || c != 0 || d != 0) r.lost = true;       // the same segments, none missing, none added
            if (a < 100) r.empty = true;                         // and there are some (the window is not empty)
        } else if (sscanf(s, "Z%*c: the vertices without culling in the window: %zu, of which %zu are not vertices with "
                             "culling; the farthest of them from the picture with culling: %lf ACC", &a, &b, &f) == 3) {
            if (b != 0 || f != 0) r.lost = true;
        }
    }
    return r;
}

// on a default file: nothing is lost in the window at the tool's default K, which is CULL_K when --cullk is not given, and at
// the K that --cullk gives
void window_tool_case(const char* what, const std::string& exe, const char* geometry, const std::string& file,
                      const std::string& centre, double R, double acc)
{
    WindowRun a = window_tool_run(exe, geometry, file, centre, R, acc, "");
    check(a.status == 0 && a.sides == 2 && !a.lost && !a.empty && !a.ineffective,
          "%s: cull --mode window (zoom 10): both sides have segments in the window, culling culls, and nothing is missing or "
          "added (exit %d, %d sides)", what, a.status, a.sides);
    check(a.K == z::CULL_K, "%s: cull --mode window uses the default margin factor CULL_K = %g when --cullk is not given (its "
          "window line says K = %g)", what, z::CULL_K, a.K);
    WindowRun b = window_tool_run(exe, geometry, file, centre, R, acc, " --cullk 4");
    check(b.status == 0 && b.sides == 2 && !b.lost && b.K == 4, "%s: with --cullk 4 it uses K = 4 (its window line says K = %g), "
          "and nothing is lost either (exit %d)", what, b.K, b.status);
}

// strays --cull says "both pictures culled to it (K = 16)" on each side: the K it uses, which is CULL_K without --cullk
void strays_tool_case(const char* what, const std::string& exe, const char* geometry, const std::string& file,
                      const std::string& centre)
{
    auto run = [&](const std::string& options, double& K, int& lines) {
        std::string out;
        int st = run_output("'" + exe + "' -e " + geometry + " -f '" + file + "' --mode picture -R 0 -a 0.3 -k 2 --zoom 10 "
                            "--centre " + centre + " --cull" + options, out);
        const char key[] = "both pictures culled to it (K = ";
        K = -1;
        lines = 0;
        for (const std::string& line : rtest::split_lines(out)) {
            size_t p = line.find(key);
            if (p == std::string::npos) continue;
            double v = -1;
            sscanf(line.c_str() + p + sizeof key - 1, "%lf", &v);
            K = lines == 0 || v == K ? v : -2;                    // both sides print the same K
            lines++;
        }
        return st;
    };
    double K1, K2;
    int n1, n2;
    int s1 = run("", K1, n1), s2 = run(" --cullk 4", K2, n2);
    check(s1 == 0 && n1 == 2 && K1 == z::CULL_K, "%s: strays --cull uses the default margin factor CULL_K = %g when --cullk is not "
          "given (it says K = %g on %d lines; exit %d)", what, z::CULL_K, K1, n1, s1);
    check(s2 == 0 && n2 == 2 && K2 == 4, "%s: strays --cull --cullk 4 uses K = 4 (it says K = %g on %d lines; exit %d)", what, K2,
          n2, s2);
}

}  // namespace

int main()
{
    double t_start = rtest::clock_seconds();
    z::cs::setup();
    z::imm::setup();
    printf("test_cull_programs: the deep zoom options of zipper3 and zipper5\n");
#if defined(ZIPPERS_ZIPPER3) && defined(ZIPPERS_ZIPPER5) && defined(ZIPPERS_REPO)
    const std::string f3 = std::string(ZIPPERS_REPO) + "/legacy/s3/subst_4.00_9.50_t3.txt";
    const std::string f4 = std::string(ZIPPERS_REPO) + "/legacy/s4/subst5_2.30_4.50_t1.txt";
    section("zipper3: --cull, --cullk, --zoomacc, --focus, --centre, a turned view (S^3 _t3, R = 0, ACC 0.04, zoom 10)");
    program_cases<z::cs::RenderPolicy, z::s3::ViewState>("zipper3", ZIPPERS_ZIPPER3, f3, {0.1152, -0.1133, 0.8356, 0.5250},
                                                         "0.1152,-0.1133,0.8356,0.5250", {40, -10, 15, -20},
                                                         " --yaw 40 --pitch -10 --pole 15 -20", 2.554);
    section("zipper5: --cull, --cullk, --zoomacc, --focus, --centre, a turned view (S^4 _t1, R = 0, ACC 0.04, zoom 10)");
    program_cases<z::imm::RenderPolicy, z::s4::View5>("zipper5", ZIPPERS_ZIPPER5, f4, {0, 0, 0, 0, 1}, "0,0,0,0,1,1",
                                                      {15, 15, 35, -20}, " --turn 35 --turn2 -20 --yaw 15", 1.317);
#else
    note("zipper3 and zipper5 not built in this configuration: their runs are left out");
#endif
#if defined(ZIPPERS_CULL) && defined(ZIPPERS_REPO)
    section("cull --mode margin: a column for the default margin factor (S^3 _t3 and S^4 _t1, R = 0, ACC 0.3)");
    {
        const double want3[2][5] = {{0.936, 0.874, 0.812, 0.812, 0.454}, {0.819, 0.819, 0.819, 0.819, 0.516}};
        const double want4[2][5] = {{1.350, 1.350, 1.111, 1.031, 0.762}, {1.057, 1.057, 1.057, 1.057, 1.057}};
        margin_tool_case("S^3", ZIPPERS_CULL, "cs", std::string(ZIPPERS_REPO) + "/legacy/s3/subst_4.00_9.50_t3.txt", want3);
        margin_tool_case("S^4", ZIPPERS_CULL, "imm", std::string(ZIPPERS_REPO) + "/legacy/s4/subst5_2.30_4.50_t1.txt", want4);
    }
#else
    note("the tool cull is not built in this configuration: its margin mode is left out");
#endif
#if defined(ZIPPERS_CULL) && defined(ZIPPERS_STRAYS) && defined(ZIPPERS_REPO)
    section("the tools cull and strays on the default files: the K they use, and nothing lost in the window (S^3 _t3_r1, S^4 _t1_r0)");
    {
        const std::string d3 = std::string(ZIPPERS_REPO) + "/data/s3/subst_4.00_9.50_t3_r1.txt",
                          d4 = std::string(ZIPPERS_REPO) + "/data/s4/subst5_2.30_4.50_t1_r0.txt";
        const std::string place = "0.1152,-0.1133,0.8356,0.5250", cusp = "0,0,0,0,1,1";
        window_tool_case("S^3 _t3_r1", ZIPPERS_CULL, "cs", d3, place, 2.554, 0.04);
        window_tool_case("S^4 _t1_r0", ZIPPERS_CULL, "imm", d4, cusp, 0, 0.04);
        strays_tool_case("S^3 _t3_r1", ZIPPERS_STRAYS, "cs", d3, place);
        strays_tool_case("S^4 _t1_r0", ZIPPERS_STRAYS, "imm", d4, cusp);
    }
#else
    note("the tools cull and strays are not built in this configuration: their window runs are left out");
#endif
    rtest::end_section();
    printf("test_cull_programs: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
