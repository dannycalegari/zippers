// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_gui_model: the window's model (apps/gui/model.h), with no window, its jobs run by the build's zippers.
//
//   - THE KEYS, the state after each against the arithmetic of the S^2 and S^3 programs' windows, bit for bit, written
//     here again from those windows: S^2 <L><U> gives rotx(-6) roty(-6) make_view(0, 0), the other arrows and the
//     drag (roty(0.4 dx), then rotx(0.4 dy), on the left); S^3 <L> gives move_pole(Q, rot^T (-1, 0, 0), 4 degrees),
//     and the other arrows, w, s and the drag (the rotation of R^3 multiplied on the left); S^4 the arrows with
//     move_pole5_toward(Q, U^T (rot^T s, 0), 4 degrees), and h H j J (U = turn4(turn4(I, 2, 3, turn), 0, 3, turn2), as
//     zipper5 makes it); the zoom (* 1.5, * (1/1.5)) and its bounds [0.1, 1e7]; the clip cycle 3 6 12 1.5 3; the
//     accuracy's steps; c in each geometry; r; the pairs (n, N: the frame of the pair's pole, its clip, zoom 1; l);
//     the keys that do nothing in a geometry say so; and for each key the job it starts, or none;
//   - THE SHELLS (S^3, S^4): the model's are cs::shell_radii(5.5) and imm::shell_radii(2.5); + and - within the R of the
//     curves shown start no job, and show the first polylines, those of the roots up to R; beyond it one job; in detail
//     a job; S^2's + checked at once against the header's maxR, and - down to 1;
//   - p: its picture is that of the command it prints, run here, byte for byte, in S^2, S^3 (with a pair) and S^4, at
//     zoom 1 and zoomed; NAME.png and NAME.zsession never overwrite a file (NAME_1 next); the session, loaded by a new
//     model, gives the same state (every number bit for bit);
//   - f: the curves of the view in detail (culled, at the accuracy on the screen), and back to the whole picture on a
//     change of the view; cancel() ends the jobs.
// The jobs run small pictures (S^2 at R = 2, S^3 and S^4 at R = 0 or a shell, coarse accuracies), in a folder of the
// test's own and without ZIPPERS_DATA. About 30 s. ZIPPERS_ZIPPERS is the path of zippers.

#include "model.h"
#include "render_test_support.h"

#include "examples/cartwright_steger.h"
#include "examples/imm_m5.h"

#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef ZIPPERS_ZIPPERS
#define ZIPPERS_ZIPPERS ""
#endif

namespace z = zippers;
namespace gui = zippers::gui;
using rtest::check;
using rtest::section;

namespace {

constexpr double WAIT = 300;                 // seconds: wide, for slower machines

gui::ModelConfig config(const std::vector<std::string>& args, const std::string& session = std::string())
{
    gui::ModelConfig c;
    c.args = args;
    c.session = session;
    c.version = "test";
    c.places = ZIPPERS_DATA_PLACES;
    c.jobs.zippers = ZIPPERS_ZIPPERS;
    c.quiet = true;
    return c;
}

bool same(const double* a, const double* b, size_t n) { return memcmp(a, b, n*sizeof(double)) == 0; }

size_t nreq(const gui::Model& m) { return m.requests().size(); }

bool said(const gui::Model& m, const std::string& text, size_t from = 0)
{
    for (size_t i = from; i < m.messages().size(); i++)
        if (m.messages()[i].find(text) != std::string::npos) return true;
    return false;
}

bool has_arg(const std::vector<std::string>& a, const std::string& opt, const std::string& value = std::string())
{
    for (size_t i = 0; i < a.size(); i++)
        if (a[i] == opt && (value.empty() || (i + 1 < a.size() && a[i + 1] == value))) return true;
    return false;
}

// the legacy S^3 window's rot_screen (legacy/s3/zipper3.cc), on a rotation of R^3
void rot_screen(double* rot, bool aboutX, double deg)
{
    double t = deg*z::s3::PI/180, c = std::cos(t), s = std::sin(t), M[9];
    if (aboutX) { double X[9] = {1,0,0, 0,c,-s, 0,s,c}; memcpy(M, X, sizeof M); }
    else { double Y[9] = {c,0,s, 0,1,0, -s,0,c}; memcpy(M, Y, sizeof M); }
    double T[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { T[3*i+j] = 0; for (int k = 0; k < 3; k++) T[3*i+j] += M[3*i+k]*rot[3*k+j]; }
    memcpy(rot, T, sizeof T);
}

// the legacy S^3 window's pole_screen, on a view
void pole_screen3(z::s3::ViewState& v, double sx, double sy, double sz, double deg)
{
    double a[3];
    for (int i = 0; i < 3; i++) a[i] = v.rot[i]*sx + v.rot[3 + i]*sy + v.rot[6 + i]*sz;    // rot^T s
    z::s3::move_pole(v.Q, a, deg*z::s3::PI/180);
}

// the same in S^4: toward U^T (rot^T s, 0)
void pole_screen4(z::s4::View5& v, double sx, double sy, double sz, double deg)
{
    double u[3], a[4] = {0, 0, 0, 0};
    for (int i = 0; i < 3; i++) u[i] = v.rot[i]*sx + v.rot[3 + i]*sy + v.rot[6 + i]*sz;
    for (int k = 0; k < 4; k++) for (int i = 0; i < 3; i++) a[k] += v.U[i][k]*u[i];
    z::s4::move_pole5_toward(v.Q, a, deg*z::s4::PI/180);
}

bool same3(const z::s3::ViewState& a, const z::s3::ViewState& b)
{
    return same(&a.Q[0][0], &b.Q[0][0], 16) && same(a.rot, b.rot, 9) && a.zoom == b.zoom && a.clip == b.clip;
}

bool same4(const z::s4::View5& a, const z::s4::View5& b)
{
    return same(&a.Q[0][0], &b.Q[0][0], 25) && same(&a.U[0][0], &b.U[0][0], 16) && same(a.rot, b.rot, 9) &&
           a.zoom == b.zoom && a.clip == b.clip && a.hidfog == b.hidfog;
}

// the picture saved by the last p, against the command it printed, run here into another file: byte for byte
bool remade(const gui::Model& m, size_t from, const std::string& what)
{
    std::string png, cmd;
    for (size_t i = from; i < m.messages().size(); i++) {
        const std::string& l = m.messages()[i];
        if (l.compare(0, 9, "p: wrote ") == 0) png = l.substr(9, l.find(';') - 9);
        if (l.compare(0, 24, "  zippers draw --session") == 0 && cmd.empty()) cmd = l.substr(2);
    }
    if (!check(!png.empty() && !cmd.empty(), "%s: p printed what it wrote and its command", what.c_str())) return false;
    const std::string again = "again_" + png;
    const size_t o = cmd.rfind(" -o ");
    const std::string run = std::string("'") + ZIPPERS_ZIPPERS + "'" + cmd.substr(7, o - 7) + " -o '" + again +
                            "' < /dev/null 2> /dev/null";
    const int st = system(run.c_str());
    const std::string a = rtest::read_file(png), b = rtest::read_file(again);
    return check(st == 0 && !a.empty() && a == b, "%s: %s (%zu bytes) is the printed command's picture, byte for byte "
                 "(%s: exit %d, %zu bytes)", what.c_str(), png.c_str(), a.size(), cmd.c_str(), st, b.size());
}

bool exists(const std::string& f) { return access(f.c_str(), F_OK) == 0; }

// ---- S^2 -------------------------------------------------------------------------------------------------------------

void test_s2()
{
    section("S2: fig8 -R 2, the keys");
    gui::Model m(config({"fig8", "-R", "2", "-a", "0.01"}));
    if (!check(m.ok(), "the model starts (%s)", m.error().c_str())) return;
    check(m.idle(WAIT) && m.ready(), "the first curves come");
    check(nreq(m) == 1 && m.requests()[0].kind == gui::JobKind::curves && has_arg(m.requests()[0].args, "--nocull") &&
              has_arg(m.requests()[0].args, "-R", "2") && !has_arg(m.requests()[0].args, "-C"),
          "one curves job, of the whole sphere: -R 2 --nocull");
    const z::s2::View v0 = z::s2::make_view(0, 0);
    check(same(m.view2().m, v0.m, 9) && m.zoom() == 1, "the start view is the program's: make_view(0, 0), zoom 1");
    size_t q = nreq(m);
    m.key("<L>");
    m.key("<U>");
    z::s2::View want = z::s2::mul3(z::s2::rotx(-6), z::s2::mul3(z::s2::roty(-6), v0));
    check(same(m.view2().m, want.m, 9), "<L><U> gives rotx(-6) roty(-6) make_view(0, 0), bit for bit");
    m.key("<R>");
    m.key("<D>");
    want = z::s2::mul3(z::s2::rotx(6), z::s2::mul3(z::s2::roty(6), want));
    check(same(m.view2().m, want.m, 9), "<R><D>: roty(6), then rotx(6), on the left");
    m.drag(10, -5);
    want = z::s2::mul3(z::s2::rotx(0.4*-5), z::s2::mul3(z::s2::roty(0.4*10), want));
    check(same(m.view2().m, want.m, 9), "a drag (10, -5): roty(4), then rotx(-2), on the left");
    check(nreq(m) == q, "the arrows and the drag start no job");
    m.key("z");
    m.key("Z");
    check(m.zoom() == 1*1.5*1.5, "z Z: the zoom * 1.5 twice");
    m.key("x");
    check(m.zoom() == 1.5*1.5*(1.0/1.5), "x: the zoom * (1/1.5)");
    for (int i = 0; i < 60; i++) m.key("z");
    check(m.zoom() == 1e7 && said(m, "beyond zoom 1e5"), "the zoom stops at 1e7, with the note beyond 1e5");
    for (int i = 0; i < 80; i++) m.key("X");
    check(m.zoom() == 0.1, "and at 0.1");
    m.scroll(3);
    check(m.zoom() == 0.1*1.5*1.5*1.5, "scroll 3: three notches * 1.5");
    m.scroll(-3);
    m.key("r");
    check(same(m.view2().m, v0.m, 9) && m.zoom() == 1, "r: the start view");
    check(nreq(m) == q, "the zoom, the scroll and r start no job");
    size_t at = m.messages().size();
    for (const char* k : {"w", "s", "b", "n", "N", "l", "h", "H", "j", "J"}) m.key(k);
    check(nreq(m) == q && said(m, "w: does nothing in S2", at) && said(m, "J: does nothing in S2", at),
          "w s b n N l h H j J: nothing, and they say so");
    at = m.messages().size();
    m.key("<T>");
    m.key("?");
    m.key("k");
    check(said(m, "no panel", at) && said(m, "the keys (S2)", at) && said(m, "not a key", at) && !m.key("<bogus>") &&
              !m.key(""), "Tab: no panel; ?: the keys; k: not a key; <bogus>: refused");

    section("S2: R, the accuracy, c");
    m.key("+");
    check(m.R() == 2.5 && nreq(m) == q + 1 && has_arg(m.requests().back().args, "-R", "2.5"), "+: R 2.5, one job");
    m.key("-");
    m.key("_");
    check(m.R() == 1.5 && nreq(m) == q + 3, "- _: R 1.5, a job each");
    m.key("-");
    at = m.messages().size();
    m.key("-");
    check(m.R() == 1 && nreq(m) == q + 4 && said(m, "already at its minimum, 1", at), "- stops at 1, and says so");
    m.key("+");
    m.key("+");
    q = nreq(m);
    m.key("]");
    check(m.acc() == 0.01/1.5 && nreq(m) == q + 1, "]: the accuracy / 1.5, a job");
    m.key("[");
    check(m.acc() == 0.01/1.5*1.5 && nreq(m) == q + 2, "[: * 1.5, a job");
    check(m.idle(WAIT) && m.R_computed() == 2, "the curves of R = 2 come");
    q = nreq(m);
    m.key("c");
    check(m.mode() == 1 && nreq(m) == q + 1 && has_arg(m.requests().back().args, "-C"),
          "c: the zippers and the wheel; the wheel's first showing: a job with -C");
    check(m.idle(WAIT) && m.curves()->layer("wheel"), "the wheel comes");
    std::vector<gui::Shown> sh = m.shown();
    check(sh.size() == 3 && sh[2].layer == "wheel" && sh[2].rgb[1] == 0.6, "shown: Z+, Z-, the wheel (0, 0.6, 0)");
    m.key("C");
    check(m.mode() == 2 && m.shown().size() == 1, "C: the wheel alone");
    m.key("c");
    m.key("c");
    check(m.mode() == 1 && nreq(m) == q + 1, "c c: the zippers, then the wheel again, with no job");
    m.key("c");
    m.key("c");
    check(m.mode() == 0 && nreq(m) == q + 1, "back to the zippers");

    section("S2: p");
    q = nreq(m);
    size_t from = m.messages().size();
    m.key("<L>");
    m.key("p");
    check(nreq(m) == q + 1 && m.requests().back().kind == gui::JobKind::picture &&
              m.requests().back().args == std::vector<std::string>({"--session", "fig8.zsession", "-o", "fig8.png"}),
          "p: one picture job, zippers draw --session fig8.zsession -o fig8.png");
    check(m.idle(WAIT), "it ends");
    remade(m, from, "S2 at zoom 1");
    from = m.messages().size();
    m.key("z");
    m.key("z");
    m.key("<U>");
    m.drag(3, 7);
    m.key("P");
    check(m.requests().back().args[1] == "fig8_1.zsession", "P (zoomed): fig8_1, overwriting nothing");
    check(m.idle(WAIT), "it ends");
    remade(m, from, "S2 zoomed");
    const double a_detail = std::min(m.acc(), std::max(1e-6, m.acc()/2.25));
    const std::string sa = z::s2::fmt_num(a_detail, 5e-3*a_detail);       // (as the S2 window prints it)
    check(said(m, "-a " + sa + " -o fig8_1_detail.png", from), "zoomed: the full-detail command, -a %s: divided by the "
          "zoom", sa.c_str());
    {
        z::session::Session s;
        std::string why;
        z::session::read("fig8_1.zsession", s, &why);
        int size;
        double zoom;
        z::s2::export_framing(512, 2.25, 16384, 1e7, size, zoom);
        check(*s.value("s") == "1024" && *s.value("zoom") == z::polylines::exact(zoom) && *s.value("C") == "no",
              "its session: the window's square framed by export_framing (1024, %s)", s.value("zoom")->c_str());
    }
    const z::s2::View vp = m.view2();
    gui::Model m2(config({}, "fig8_1.zsession"));
    check(m2.ok() && m2.idle(WAIT) && m2.ready(), "the session loads (%s)", m2.error().c_str());
    check(same(m2.view2().m, vp.m, 9) && m2.zoom() == 2.25 && m2.R() == 2 && m2.acc() == m.acc() && m2.mode() == 0 &&
              m2.stem() == "fig8" && m2.size() == 512, "and gives the same state: the view bit for bit, zoom, R, acc");
    from = m2.messages().size();
    m2.key("p");
    check(m2.idle(WAIT) && m2.requests().back().args[1] == "fig8_2.zsession", "its p: fig8_2");
    check(rtest::read_file("fig8_2.png") == rtest::read_file("fig8_1.png"), "the same picture as fig8_1.png");

    section("S2: f, and back");
    q = nreq(m);
    m.key("f");
    check(nreq(m) == q + 1 && m.requests().back().detail && !has_arg(m.requests().back().args, "--nocull") &&
              has_arg(m.requests().back().args, "-a", z::polylines::exact(strtod(sa.c_str(), nullptr))),
          "f: a curves job of the view, culled, -a %s", sa.c_str());
    check(m.idle(WAIT) && m.detail(), "the detail comes");
    m.key("+");
    check(nreq(m) == q + 2 && m.requests().back().detail, "+ in detail: a job of the detail");
    m.key("<L>");
    check(!m.detail() && nreq(m) == q + 3 && !m.requests().back().detail, "a change of the view: the whole picture "
          "again, recomputed at the new R");
    m.cancel();
    check(!m.busy(), "cancel: no job");
}

void test_s2_limits()
{
    section("S2: + against maxR, at once");
    gui::Model m(config({"fig8", "-R", "2", "-a", "0.02"}));
    check(m.ok() && m.idle(WAIT) && m.ready(), "the first curves come");
    double maxR = 0;
    sscanf(m.curves()->value("maxR")->c_str(), "%lf", &maxR);
    int presses = 0;
    while (m.R() + 0.5 <= maxR && presses < 40) {
        m.key("+");
        presses++;
    }
    const size_t q = nreq(m);
    const size_t at = m.messages().size();
    const double R = m.R();
    m.key("+");
    m.cancel();
    check(m.R() == R && nreq(m) == q && said(m, "+: R stays at", at), "+ past maxR %.3f: refused at once, R stays at %g",
          maxR, R);
}

// ---- S^3 -------------------------------------------------------------------------------------------------------------

void test_s3()
{
    section("S3: cs -a 0.1, the keys");
    gui::Model m(config({"cs", "-a", "0.1"}));
    if (!check(m.ok() && m.idle(WAIT) && m.ready(), "the first curves come (%s)", m.error().c_str())) return;
    z::cs::setup();
    check(m.shells() == z::cs::shell_radii(5.5) && m.shells().size() == 13, "the shells: cs::shell_radii(5.5)");
    {
        std::vector<double> vm;
        z::polylines::parse_list(*m.curves()->value("viewmatrix"), vm);
        z::s3::ViewState v;
        for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) v.Q[r][k] = vm[4*r + k];
        for (int i = 0; i < 9; i++) v.rot[i] = vm[16 + i];
        check(vm.size() == 25 && same(&v.Q[0][0], &m.view3().Q[0][0], 16) && same(v.rot, m.view3().rot, 9) &&
              m.view3().zoom == 1 && m.view3().clip == 3, "the start view: the header's viewmatrix, zoom 1, clip 3");
    }
    const z::s3::ViewState v0 = m.view3();
    size_t q = nreq(m);
    z::s3::ViewState want = v0;
    m.key("<L>");
    pole_screen3(want, -1, 0, 0, 4);
    check(same3(m.view3(), want), "<L>: move_pole(Q, rot^T (-1, 0, 0), 4 degrees), bit for bit");
    m.key("<R>"); pole_screen3(want, 1, 0, 0, 4);
    m.key("<U>"); pole_screen3(want, 0, 1, 0, 4);
    m.key("<D>"); pole_screen3(want, 0, -1, 0, 4);
    m.key("w"); pole_screen3(want, 0, 0, 1, 4);
    m.key("s"); pole_screen3(want, 0, 0, -1, 4);
    check(same3(m.view3(), want), "<R> <U> <D> w s: the pole toward +x, +y, -y, +z, -z");
    m.drag(-7, 12);
    rot_screen(want.rot, false, 0.4*-7);
    rot_screen(want.rot, true, 0.4*12);
    check(same3(m.view3(), want), "a drag (-7, 12): the rotation of R^3, as the S3 window's");
    for (double c : {6.0, 12.0, 1.5, 3.0}) {
        m.key("b");
        check(m.view3().clip == c, "b: the clip radius %g", c);
    }
    m.key("z");
    check(m.view3().zoom == 1.5, "z: zoom 1.5");
    for (int i = 0; i < 60; i++) m.key("z");
    check(m.view3().zoom == 1e7, "the zoom stops at 1e7 (S2's bounds)");
    m.key("r");
    check(same3(m.view3(), v0), "r: the start view, bit for bit");
    m.key("c");
    check(m.mode() == 1 && m.shown().size() == 1 && m.shown()[0].layer == "Z+", "c: Z+");
    m.key("C");
    check(m.mode() == 2 && m.shown().size() == 1 && m.shown()[0].layer == "Z-", "C: Z-");
    m.key("c");
    check(m.mode() == 3 && m.shown().size() == 2 && m.shown()[0].rgb[2] == 0.85, "c: both, in the programs' colours");
    size_t at = m.messages().size();
    for (const char* k : {"h", "J", "n", "l"}) m.key(k);
    check(nreq(m) == q && said(m, "h: does nothing in S3", at) && said(m, "no linked pairs", at),
          "h J: nothing in S3; n l: no pairs, and they say so");
    check(nreq(m) == q, "none of these starts a job");

    section("S3: the shells");
    const std::vector<double>& sh = m.shells();
    m.key("+");
    check(m.R() == sh[1] + 1e-6 && nreq(m) == q + 1 && has_arg(m.requests().back().args, "-R", z::polylines::exact(sh[1] + 1e-6)),
          "+: the next shell plus 1e-6, beyond the R computed: one job");
    m.key("+");
    check(m.R() == sh[2] + 1e-6 && nreq(m) == q + 2, "+ again: one job (the newer)");
    check(m.idle(WAIT) && m.R_computed() == sh[2] + 1e-6, "the curves of the second shell come");
    const size_t all = m.shown()[0].end;
    m.key("-");
    const size_t first = m.shown()[0].end;
    size_t count = 0;
    const z::polylines::Layer* L = m.curves()->layer("Z+");
    for (size_t k = 0; k < L->npoly(); k++) count += m.curves()->rootd[L->root[k]] <= sh[1] + 1e-6;
    check(m.R() == sh[1] + 1e-6 && nreq(m) == q + 2 && first == count && first < all,
          "-: within the R computed, no job: the first %zu of %zu polylines of Z+, those of the roots up to R", first,
          all);
    m.key("-");
    m.key("-");
    at = m.messages().size();
    m.key("-");
    check(m.R() == 0 && nreq(m) == q + 2 && said(m, "already at its minimum, 0", at), "- - -: R 1e-6, then 0, no job; "
          "then it says so");
    m.key("+");
    m.key("+");
    check(m.R() == sh[2] + 1e-6 && nreq(m) == q + 2 && m.shown()[0].end == all, "+ +: back to the R computed, no job");
    m.key("+");
    check(nreq(m) == q + 3, "+ beyond it: one job");
    m.key("-");
    check(m.idle(WAIT), "it ends");
    q = nreq(m);
    m.key("]");
    check(m.acc() == 0.1/1.5 && nreq(m) == q + 1, "]: the accuracy / 1.5: a job");
    m.key("[");
    check(m.acc() == 0.1/1.5*1.5 && nreq(m) == q + 2 && m.idle(WAIT), "[: * 1.5: a job");

    section("S3: f, the detail");
    m.key("z");
    m.key("z");
    q = nreq(m);
    m.key("f");
    const std::vector<std::string>& d = m.requests().back().args;
    check(nreq(m) == q + 1 && m.requests().back().detail && has_arg(d, "--cull") && has_arg(d, "--zoomacc") &&
              has_arg(d, "--focus", "1") && has_arg(d, "--viewmatrix"),
          "f (zoomed): a curves job of this view, --cull --zoomacc --focus 1");
    check(m.idle(WAIT) && m.detail(), "the detail comes");
    m.key("-");
    check(nreq(m) == q + 2 && m.requests().back().detail, "- in detail (the focus ball): a job");
    m.key("+");
    check(nreq(m) == q + 3 && m.requests().back().detail, "+ in detail: a job");
    check(m.idle(WAIT), "it ends");
    m.key("f");
    check(!m.detail() && nreq(m) == q + 3, "f again: the whole picture, kept: no job");

    section("S3: p");
    size_t from = m.messages().size();
    m.key("r");
    m.key("<L>");
    m.key("p");
    check(m.idle(WAIT), "p at zoom 1 ends");
    remade(m, from, "S3 at zoom 1");
    check(!said(m, "full detail", from), "at zoom 1 no full-detail command");
    from = m.messages().size();
    m.key("z");
    m.key("<U>");
    m.drag(5, 5);
    m.key("p");
    check(m.idle(WAIT), "p zoomed ends");
    remade(m, from, "S3 zoomed");
    check(said(m, "--zoomacc -o cartwright-steger_1_detail.png", from), "and the full-detail command, with --zoomacc");
    gui::Model m2(config({}, "cartwright-steger_1.zsession"));
    check(m2.ok() && m2.idle(WAIT) && m2.ready(), "its session loads (%s)", m2.error().c_str());
    check(same3(m2.view3(), m.view3()) && m2.R() == m.R() && m2.acc() == m.acc() && m2.mode() == m.mode(),
          "and gives the same state, bit for bit");
}

void test_s3_pairs()
{
    section("S3: the linked pairs");
    gui::Model m(config({"cs", "-a", "0.1", "--pairs", "linked_pairs.txt", "--pairindex", "2", "-o", "pairs"}));
    if (!check(m.ok() && m.idle(WAIT) && m.ready() && m.pairs(), "the pairs come (%s)", m.error().c_str())) return;
    auto pair_view = [&](int k, z::s3::ViewState v) {
        std::vector<double> p;
        z::polylines::parse_list(*m.curves()->value("pair." + std::to_string(k) + ".pole"), p);
        z::s3::frame_with_pole(p.data(), v.Q);
        v.clip = strtod(m.curves()->value("pair." + std::to_string(k) + ".clip")->c_str(), nullptr);
        v.zoom = 1;
        return v;
    };
    check(m.pair() == 2 && same3(m.view3(), pair_view(2, m.view3())), "the start: pair 2, in its view");
    const size_t q = nreq(m);
    m.drag(20, 0);
    z::s3::ViewState before = m.view3();
    m.key("n");
    check(m.pair() == 3 && same3(m.view3(), pair_view(3, before)) && said(m, "pair 3: lk"),
          "n: pair 3, its view (its pole, its clip, zoom 1; the rotation kept), its line");
    for (int i = 0; i < 4; i++) m.key("N");
    check(m.pair() == 12, "N four times: pair 12 (of 13, round)");
    std::vector<gui::Shown> sh = m.shown();
    check(sh.size() == 2 && sh[0].layer == "pair.Z+" && sh[0].first == 12 && sh[0].end == 13, "shown: the loops of pair 12");
    m.key("l");
    sh = m.shown();
    check(m.context() && sh.size() == 4 && sh[0].colour == "context.Z+" && sh[0].rgb[0] == 0.7,
          "l: the zippers behind it, faint");
    check(nreq(m) == q, "n N l start no job");
    size_t at = m.messages().size();
    m.key("f");
    check(nreq(m) == q && said(m, "no detail of a linked pair", at), "f: no detail of a pair, and it says so");
    size_t from = m.messages().size();
    m.key("p");
    check(m.idle(WAIT), "p ends");
    remade(m, from, "S3 pair 12 with its context");
    from = m.messages().size();
    m.key("z");
    m.key("<L>");
    m.key("p");
    check(m.idle(WAIT), "p zoomed ends");
    remade(m, from, "S3 pair zoomed");
    check(exists("pairs.png") && exists("pairs_1.zsession"), "the stem of -o: pairs, pairs_1");
    gui::Model m2(config({}, "pairs_1.zsession"));
    check(m2.ok() && m2.idle(WAIT) && same3(m2.view3(), m.view3()) && m2.pair() == 12 && m2.context() &&
              m2.stem() == "pairs",
          "its session gives the same state: the view, pair 12, its context");
    m.key("r");
    check(same3(m.view3(), pair_view(12, m.view3())), "r: the pair's view");
}

// ---- S^4 -------------------------------------------------------------------------------------------------------------

void test_s4()
{
    section("S4: imm -a 0.05, the keys");
    gui::Model m(config({"imm", "-a", "0.05"}));
    if (!check(m.ok() && m.idle(WAIT) && m.ready(), "the first curves come (%s)", m.error().c_str())) return;
    z::imm::setup();
    check(m.shells() == z::imm::shell_radii(2.5) && m.shells().size() == 6, "the shells: imm::shell_radii(2.5)");
    const z::s4::View5 v0 = m.view4();
    size_t q = nreq(m);
    z::s4::View5 want = v0;
    auto U = [&](double turn, double turn2) {
        memset(want.U, 0, sizeof want.U);
        for (int i = 0; i < 4; i++) want.U[i][i] = 1;
        z::s4::turn4(want.U, 2, 3, turn*z::s4::PI/180);
        z::s4::turn4(want.U, 0, 3, turn2*z::s4::PI/180);
    };
    U(0, 0);
    check(same4(m.view4(), want), "the start: U the identity");
    m.key("h");
    U(0, 5);
    check(m.turn2() == 5 && same4(m.view4(), want), "h: turn2 5; U = turn4(turn4(I, 2, 3, 0), 0, 3, 5 degrees)");
    m.key("j");
    m.key("j");
    m.key("H");
    m.key("H");
    U(10, -5);
    check(m.turn() == 10 && m.turn2() == -5 && same4(m.view4(), want), "j j H H: turn 10, turn2 -5");
    m.key("J");
    U(5, -5);
    check(same4(m.view4(), want), "J: turn 5");
    m.key("<L>");
    pole_screen4(want, -1, 0, 0, 4);
    check(same4(m.view4(), want), "<L>: move_pole5_toward(Q, U^T (rot^T (-1, 0, 0), 0), 4 degrees), bit for bit");
    m.key("<U>"); pole_screen4(want, 0, 1, 0, 4);
    m.key("s"); pole_screen4(want, 0, 0, -1, 4);
    m.drag(4, 9);
    rot_screen(want.rot, false, 0.4*4);
    rot_screen(want.rot, true, 0.4*9);
    check(same4(m.view4(), want), "<U> s and a drag");
    m.key("b");
    check(m.view4().clip == 6, "b: clip 6");
    m.key("b");
    m.key("b");
    m.key("b");
    m.key("r");
    check(same4(m.view4(), v0) && m.turn() == 0 && m.turn2() == 0, "r: the start view, and the turns 0");
    size_t at = m.messages().size();
    m.key("n");
    m.key("l");
    check(nreq(m) == q && said(m, "n: does nothing in S4", at), "n l: nothing in S4, and they say so; no job");
    m.key("+");
    check(m.R() == m.shells()[1] + 1e-6 && nreq(m) == q + 1, "+: the next shell, one job");
    check(m.idle(WAIT), "it ends");
    m.key("-");
    check(m.R() == 1e-6 && nreq(m) == q + 1, "-: R 1e-6 (the shell 0, plus 1e-6) within the R computed: no job");

    section("S4: p");
    size_t from = m.messages().size();
    m.key("h");
    m.key("<R>");
    m.key("p");
    check(m.idle(WAIT), "p at zoom 1 ends");
    remade(m, from, "S4 at zoom 1, turned");
    from = m.messages().size();
    m.key("z");
    m.key("j");
    m.key("w");
    m.key("p");
    check(m.idle(WAIT), "p zoomed ends");
    remade(m, from, "S4 zoomed");
    gui::Model m2(config({}, "imm-m5_1.zsession"));
    check(m2.ok() && m2.idle(WAIT) && same4(m2.view4(), m.view4()) && m2.turn() == m.turn() && m2.turn2() == m.turn2() &&
              m2.R() == m.R(), "its session gives the same state, bit for bit");
}

}  // namespace

int main()
{
    printf("test_gui_model: the window's model (apps/gui/model.h), with no window\n");
    if (!*ZIPPERS_ZIPPERS) {
        printf("zippers is not built\n");
        return 1;
    }
    rtest::TempDir t;
    if (chdir(t.path.c_str()) != 0) return 1;
    unsetenv("ZIPPERS_DATA");
    test_s2();
    test_s2_limits();
    test_s3();
    test_s3_pairs();
    test_s4();
    rtest::end_section();
    if (chdir("/") != 0) return 1;
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
