// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_polylines_s2: the options --polylines and --colour of zipper (S^2).
//
// PICTURES FROM THE FILE: zipper is run with -o and --polylines (fig8, light cases), and the picture painted in this
// process from the file alone (its points, its view and zoom, its accuracy), with the program's own painters (this test
// includes apps/s2/zipper.cc, with main renamed, as the film engine does, and is compiled with the program's -O3), is the
// program's picture, byte for byte: PNG at the default view, EPS, a turned and zoomed view (--yaw, --pitch, --roll,
// --zoom: culled to the view and focused, as the file's header says), the whole sphere (--nocull), and the Catherine wheel
// (-C: a third layer, wheel). The header: the layers, cull and focus as the program drew, the view, and the largest R of
// check_radius (without and with the focus ball) and of check_wheel, which are those the library computes in this process
// for the same example (max_radius), bit for bit.
// The header says that no cap was reached (capped NAME no, for each layer).
// WITHOUT -o: --polylines writes the file and no picture (nothing on the standard output), and no warning that nothing
// was drawn. Refused before anything is computed, with the system's reason: a file in a folder that does not exist, a
// folder of the name, a folder that may not be written in; and an argument that holds a line break.
// --colour: the default colours given as options draw the same pictures, byte for byte (PNG and EPS, with the wheel), and
// so do the wheel's colour without -C and an option given twice whose last value is the default; another colour for a
// layer changes the EPS in exactly one line, that layer's setrgbcolor, and the PNG only at that layer's pixels (two other
// colours change the same pixels, and no pixel changes elsewhere). Refused, with status 1 and before anything is
// computed: an unknown name, a name of another sphere, two numbers, a number outside [0, 1], no '='.
// zipper runs in this test's own folder, without ZIPPERS_DATA in its environment. A few seconds. ZIPPERS_ZIPPER is the
// path of the binary.

// (the warnings the film engine silences for the program; the GCC form, which clang reads too)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#define main zipper_main
#include "zipper.cc"                       // apps/s2/zipper.cc
#undef main
#pragma GCC diagnostic pop

#include "output/png.h"
#include "output/polylines.h"
#include "render_test_support.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <memory>

namespace pl = zippers::polylines;
using rtest::check;
using rtest::note;
using rtest::section;

#ifndef ZIPPERS_ZIPPER
#define ZIPPERS_ZIPPER ""
#endif

namespace {

int run(const std::string& dir, const std::string& cmd, std::string& out, std::string& err)
{
    out.clear(); err.clear();
    const std::string eo = dir + "/.stderr";
    FILE* p = popen(("cd '" + dir + "' && unset ZIPPERS_DATA && " + cmd + " 2>'" + eo + "' < /dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    int st = pclose(p);
    err = rtest::read_file(eo);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

std::string zipper(const std::string& args) { return std::string("'") + ZIPPERS_ZIPPER + "' " + args; }

bool run_ok(const rtest::TempDir& t, const std::string& args, const std::string& what)
{
    std::string out, err;
    int st = run(t.path, zipper(args), out, err);
    return check(st == 0, "%s: exit %d: %s", what.c_str(), st, err.substr(0, 300).c_str());
}

bool same_file(const std::string& a, const std::string& b)
{
    std::string x = rtest::read_file(a), y = rtest::read_file(b);
    return !x.empty() && x == y;
}

std::vector<double> numbers(const pl::File& f, const std::string& key)
{
    std::vector<double> v;
    const std::string* s = f.value(key);
    if (!s || !pl::parse_list(*s, v)) v.clear();
    return v;
}

// the example of the file, prepared in this process (the painters need a Rep only to make renderers)
struct Example {
    Options opt;
    Monodromy mon;
    Rep rep;
    bool ok = false;
    Example() { ok = prepare(opt, mon, rep); }   // fig8, order 2: the defaults
};

// paint the file's picture with the program's painters: PNG or EPS by the name, with the size and colours given
bool paint(const Example& ex, const pl::File& f, const std::string& out, int size,
           const double colours[3][3] = nullptr)
{
    std::vector<double> view = numbers(f, "view"), zoom = numbers(f, "zoom"), acc = numbers(f, "acc");
    if (f.sphere != 2 || view.size() != 9 || zoom.size() != 1 || acc.size() != 1) return false;
    Options o = ex.opt;
    o.size = size;
    o.zoom = zoom[0];
    o.accuracy = acc[0];
    const bool png = out.size() > 4 && out.substr(out.size() - 4) == ".png";
    o.format = png ? FMT_PNG : FMT_EPS;
    View v;
    for (int i = 0; i < 9; i++) v.m[i] = view[i];
    const Options dflt;
    std::vector<std::unique_ptr<Renderer>> rs;
    vector<Layer> layers;
    const char* names[3] = {"Z+", "Z-", "wheel"};
    for (int k = 0; k < 3; k++) {
        const pl::Layer* L = f.layer(names[k]);
        if (!L) continue;
        rs.emplace_back(new Renderer(ex.rep));
        Renderer& r = *rs.back();
        r.pstart = L->first;
        for (size_t i = 0; i < L->pts.size(); i += 3) {
            r.px.push_back(L->pts[i]); r.py.push_back(L->pts[i+1]); r.pz.push_back(L->pts[i+2]);
        }
        const double* c = colours ? colours[k] : dflt.colour[k];
        layers.push_back({&r, {c[0], c[1], c[2]}});
    }
    if (png) return write_png(out.c_str(), o, layers, v);
    FILE* fp = fopen(out.c_str(), "w");
    if (!fp) return false;
    write_eps(fp, o, layers, v);
    return fclose(fp) == 0;
}

bool read_ok(const rtest::TempDir& t, const std::string& name, pl::File& f)
{
    std::string err;
    return check(pl::read(t.file(name), f, &err), "read %s (%s)", name.c_str(), err.c_str());
}

// the set of pixels at which two pictures differ
std::vector<char> diff(const std::string& a, const std::string& b, size_t& n, bool& ok)
{
    std::vector<uint8_t> A, B;
    int w1, h1, w2, h2;
    ok = zippers::png::read(a, A, w1, h1) && zippers::png::read(b, B, w2, h2) && w1 == w2 && h1 == h2;
    std::vector<char> d;
    n = 0;
    if (!ok) return d;
    d.resize(A.size()/3);
    for (size_t i = 0; i < d.size(); i++) {
        d[i] = A[3*i] != B[3*i] || A[3*i+1] != B[3*i+1] || A[3*i+2] != B[3*i+2];
        n += d[i];
    }
    return d;
}

void test_pictures(const rtest::TempDir& t, const Example& ex)
{
    section("S2: the picture painted from the file is the program's, byte for byte");
    struct Case { const char* what; const char* args; const char* out; int size; const char* cull; const char* focus; };
    const Case cases[] = {
        {"PNG, the default view", "-R 3 -s 300", "a.png", 300, "on", "off"},
        {"EPS, the default view", "-R 3 -s 300", "a.eps", 300, "on", "off"},
        {"PNG, turned and zoomed (culled and focused)", "-R 3 --yaw 30 --pitch -20 --roll 10 --zoom 3 -s 300", "a.png", 300,
         "on", "on"},
        {"EPS, turned and zoomed", "-R 3 --yaw 30 --pitch -20 --roll 10 --zoom 3 -s 300", "a.eps", 300, "on", "on"},
        {"PNG, the whole sphere (--nocull)", "-R 3 --nocull --yaw 100 -s 256", "a.png", 256, "off", "off"},
        {"PNG, the cusp", "-n inf -R 3 -s 256", "a.png", 256, "on", "off"},
        {"PNG, the Catherine wheel (-C)", "-R 3 -C -s 300", "a.png", 300, "off", "off"},
        {"EPS, the Catherine wheel", "-R 3 -C -s 300", "a.eps", 300, "off", "off"},
    };
    for (const Case& c : cases) {
        if (!run_ok(t, std::string(c.args) + " -o " + c.out + " --polylines a.zpl", c.what)) continue;
        pl::File f;
        if (!read_ok(t, "a.zpl", f)) continue;
        const std::string mine = std::string("b") + (c.out + 1);
        check(paint(ex, f, t.file(mine), c.size), "%s: paint the file", c.what);
        check(same_file(t.file(c.out), t.file(mine)), "%s: the same picture", c.what);
        const bool wheel = std::string(c.args).find("-C") != std::string::npos;
        check(f.layer("Z+") && f.layer("Z-") && (f.layer("wheel") != nullptr) == wheel && f.layers.size() == (wheel ? 3u : 2u),
              "%s: the layers", c.what);
        check(f.value("cull") && *f.value("cull") == c.cull && f.value("focus") && *f.value("focus") == c.focus,
              "%s: cull %s, focus %s (the file says %s, %s)", c.what, c.cull, c.focus,
              f.value("cull") ? f.value("cull")->c_str() : "-", f.value("focus") ? f.value("focus")->c_str() : "-");
        bool tags = f.rootd.empty();
        for (const pl::Layer& L : f.layers)
            for (size_t k = 0; k < L.npoly(); k++) tags = tags && L.root[k] == -1 && L.letter[k] == -1;
        check(tags, "%s: no roots, no letters", c.what);
        const std::vector<std::string> capped = wheel ? std::vector<std::string>{"Z+ no", "Z- no", "wheel no"}
                                                      : std::vector<std::string>{"Z+ no", "Z- no"};
        check(f.values("capped") == capped, "%s: no cap reached, in each layer", c.what);
    }
    {   // the largest R, as the library computes them for fig8, order 2
        pl::File f;
        if (run_ok(t, "-R 2 --polylines m.zpl", "the limits") && read_ok(t, "m.zpl", f)) {
            const double want[3] = {max_radius(ex.rep, false), max_radius(ex.rep, true),
                                    max_radius(ex.rep, false, wheel_large_order(2) ? WHEEL_ROOTS_LARGE_N : WHEEL_ROOTS)};
            const char* keys[3] = {"maxR", "maxR.focus", "maxR.wheel"};
            for (int k = 0; k < 3; k++) {
                std::vector<double> v = numbers(f, keys[k]);
                check(v.size() == 1 && !memcmp(&v[0], &want[k], sizeof(double)), "%s = %.17g, the library's %.17g", keys[k],
                      v.size() == 1 ? v[0] : 0.0, want[k]);
            }
            note("maxR %.4f, with the focus %.4f, with the wheel %.4f", want[0], want[1], want[2]);
        }
    }

    section("S2: --polylines without -o writes no picture");
    std::string out, err;
    int st = run(t.path, zipper("-R 2 --polylines only.zpl"), out, err);
    pl::File f;
    check(st == 0 && out.empty() && pl::read(t.file("only.zpl"), f), "exit 0, nothing on the standard output, the file (exit %d,"
          " %zu bytes out)", st, out.size());
    check(err.find("nothing was drawn") == std::string::npos && err.find("wrote only.zpl") != std::string::npos,
          "no warning that nothing was drawn, and the file announced");
    st = run(t.path, zipper("-R 2 --polylines nofolder/x.zpl"), out, err);
    check(st == 1 && err.find(std::string("cannot write nofolder/x.zpl: ") + strerror(ENOENT)) != std::string::npos &&
          err.find("monodromy") == std::string::npos,
          "a file in a folder that does not exist: refused before anything is computed (exit %d: %s)", st, err.substr(0, 200).c_str());
    mkdir(t.file("adir").c_str(), 0777);
    st = run(t.path, zipper("-R 2 --polylines adir"), out, err);
    check(st == 1 && err.find(std::string("cannot write adir: ") + strerror(EISDIR)) != std::string::npos &&
          err.find("monodromy") == std::string::npos,
          "a folder of the name: refused before anything is computed (exit %d: %s)", st, err.substr(0, 200).c_str());
    if (geteuid() != 0) {                             // (the owner of every file may write anywhere)
        mkdir(t.file("rodir").c_str(), 0555);
        st = run(t.path, zipper("-R 2 --polylines rodir/x.zpl"), out, err);
        check(st == 1 && err.find(std::string("cannot write rodir/x.zpl: ") + strerror(EACCES)) != std::string::npos &&
              err.find("monodromy") == std::string::npos,
              "a folder that may not be written in: refused before anything is computed (exit %d: %s)", st,
              err.substr(0, 200).c_str());
        chmod(t.file("rodir").c_str(), 0755);
    }
    {   // the drawing stopped at the memory limit: the header says that each zipper is incomplete (exit status 3)
        st = run(t.path, zipper("-R 6.5 -a 0.001 --nocull --memlimit 0.2 --polylines mem.zpl"), out, err);
        pl::File m;
        check(st == 3 && err.find("drawing stopped at the memory limit") != std::string::npos && pl::read(t.file("mem.zpl"), m) &&
              m.values("capped") == std::vector<std::string>{"Z+ yes", "Z- yes"},
              "the memory limit reached (--memlimit 0.2): exit 3, capped Z+ yes, capped Z- yes (exit %d)", st);
    }
    st = run(t.path, zipper("-R 2 -o 'a\nb.png' --polylines nl.zpl"), out, err);
    check(st == 1 && err.find("--polylines: an argument holds a line break") != std::string::npos &&
          err.find("monodromy") == std::string::npos && rtest::read_file(t.file("nl.zpl")).empty(),
          "an argument that holds a newline: refused before anything is computed (exit %d: %s)", st, err.substr(0, 200).c_str());
}

void test_colour(const rtest::TempDir& t)
{
    section("S2: --colour: the default colours given again draw the same pictures");
    const std::string dflt = " --colour Z+=0,0,1 --colour Z-=1,0,0 --colour wheel=0,0.6,0";
    for (const char* ext : {".png", ".eps"}) {
        const std::string base = "-R 3 -C -s 256 -o ";
        if (run_ok(t, base + "c0" + ext, ext) && run_ok(t, base + "c1" + ext + dflt, ext))
            check(same_file(t.file(std::string("c0") + ext), t.file(std::string("c1") + ext)),
                  "%s with the default colours given: the same picture", ext);
    }

    {   // what the picture does not draw, and an option given twice: the last counts
        const std::string base = "-R 3 -s 256 -o ";
        if (run_ok(t, base + "d0.png", "PNG") &&
            run_ok(t, base + "d1.png --colour wheel=1,0,1 --colour Z+=1,0,0 --colour Z-=0,1,0 --colour Z+=0,0,1 --colour Z-=1,0,0",
                   "PNG"))
            check(same_file(t.file("d0.png"), t.file("d1.png")),
                  "the wheel's colour without -C, and the defaults given last: the same picture");
    }

    section("S2: --colour: another colour changes only its layer");
    {   // EPS: exactly the layer's setrgbcolor line
        if (run_ok(t, "-R 3 -s 256 -o e1.eps", "EPS") && run_ok(t, "-R 3 -s 256 --colour Z-=0,1,0.5 -o e2.eps", "EPS")) {
            std::vector<std::string> a = rtest::split_lines(rtest::read_file(t.file("e1.eps"))),
                                     b = rtest::split_lines(rtest::read_file(t.file("e2.eps")));
            size_t nd = 0;
            std::string line;
            for (size_t i = 0; i < a.size() && i < b.size(); i++)
                if (a[i] != b[i]) { nd++; line = a[i] + " -> " + b[i]; }
            check(a.size() == b.size() && nd == 1 && line == "1.000 0.000 0.000 setrgbcolor -> 0.000 1.000 0.500 setrgbcolor",
                  "the EPS differs in one line, Z-'s colour (%zu lines differ: %s)", nd, line.c_str());
        }
    }
    struct Layer2 { const char* name; const char* c1; const char* c2; };
    const Layer2 layers[] = {{"Z+", "1,0.5,0", "0,0,0"}, {"Z-", "0,1,0.5", "0.2,0.2,0.2"}, {"wheel", "1,0,1", "0,0,0"}};
    for (const Layer2& L : layers) {
        const std::string base = "-R 3 -C -s 256";
        if (!run_ok(t, base + " -o p0.png", L.name) ||
            !run_ok(t, base + " --colour " + L.name + "=" + L.c1 + " -o p1.png", L.name) ||
            !run_ok(t, base + " --colour " + L.name + "=" + L.c2 + " -o p2.png", L.name))
            continue;
        size_t n1, n2, n12;
        bool ok1, ok2, ok3;
        std::vector<char> d1 = diff(t.file("p0.png"), t.file("p1.png"), n1, ok1), d2 = diff(t.file("p0.png"), t.file("p2.png"), n2, ok2),
                          d12 = diff(t.file("p1.png"), t.file("p2.png"), n12, ok3);
        check(ok1 && ok2 && ok3 && n1 > 0 && d1 == d2 && d1 == d12, "%s: two colours change the same %zu pixels (%zu, %zu), and"
              " no other", L.name, n1, n2, n12);
        note("%s: %zu pixels of %zu", L.name, n1, d1.size());
    }

    section("S2: --colour: refusals, before anything is computed");
    for (const char* v : {"Q=0,0,0", "pair.Z+=0,0,0", "context.Z+=0,0,0", "Z+=0,0", "Z+=0,0,1,0", "Z+=0,0,2", "Z-=-1,0,0",
                          "wheel", "=1,1,1"}) {
        std::string out, err;
        int st = run(t.path, zipper(std::string("-R 2 --colour '") + v + "' -o r.png"), out, err);
        check(st == 1 && err.find("--colour") != std::string::npos && err.find("monodromy") == std::string::npos,
              "--colour %s: refused, exit 1 (exit %d: %s)", v, st, err.substr(0, 200).c_str());
    }
}

}  // namespace

int main()
{
    printf("test_polylines_s2: --polylines and --colour of zipper (S2)\n");
    if (!*ZIPPERS_ZIPPER) { printf("zipper is not built\n"); return 1; }
    rtest::TempDir t;
    Example ex;
    if (!ex.ok) { printf("fig8 could not be prepared\n"); return 1; }
    test_pictures(t, ex);
    test_colour(t);
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
