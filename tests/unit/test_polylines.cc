// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_polylines: the polyline file (src/output/polylines.h), and the options --polylines, --viewmatrix and --colour of
// zipper3 and zipper5.
//
// THE FORMAT, in this process: files written and read back are the same, bit for bit (finite floats of every kind, -0
// and the subnormals among them, empty layers, keys repeated or with empty values, the roots); exact() reads back bit for
// bit, in the plain form when that is not longer; parse_list; and the reader refuses, with a message, what is not a
// polyline file of format 1 (another magic line or version, no "data" line, a missing or repeated "sphere" or "roots", a
// bad "layer" line, blocks shorter or longer than the header says, a first point that is not 0, a polyline with no points,
// a root out of range, a NaN or an infinity among the points or the distances, counts larger than the file, counts whose
// announced size overflows 64 bits back to the file's size (in a child process: refused, not a crash), and changed counts
// that keep the size where a float read from the wrong block is not finite), and the writer what a file cannot hold (NaN
// and infinities among them). THE WRITE BY RENAME: a write leaves only its file; a folder of the name, a folder that does
// not exist, a file that may not be written, and a write that fails in the middle (a child process with a limit on the
// size of its files) are refused with the system's reason, leave no file behind, and leave an older file as it was. A
// symbolic link of the name is written through (its target written, the link kept; also a link to a file that does not
// exist yet); a link into a folder that does not exist is refused, the message naming its target; a loop of links, and
// a chain of more links than open() follows, are refused as open() refuses them (ELOOP), and a chain of as many is
// written through.
// PICTURES FROM THE FILE: the programs are run with -o and --polylines on light cases, and the picture painted in this
// process from the file alone (its points, its view, its marks), with the painters of src/view/s3.h and s4.h, is the
// program's picture, byte for byte (the same machine, so this holds on any): S^3 and S^4 at their default views, in turned
// views (S^3: --yaw, --pitch, --pole, --zoom, --clip; S^4: --yaw, --pitch, --turn, --turn2, --hidfog, --zoom, --clip), in
// zoomed stills (culling and the focus ball on by default, so the file holds the curves near the view), and the pictures
// of linked pairs (with and without --context: the pair's loops from the layers pair.Z+ and pair.Z-, the view of the
// pair's pole and clip from the header). The header is checked too: its keys, the layers, the marks, the roots.
// --viewmatrix: the view of a run, as its header gives it, given to --viewmatrix draws the same picture, byte for byte: the
// default views, views turned by --yaw, --pitch and --pole (S^3) or --yaw, --pitch and --turn (S^4), and views of --centre;
// with --pairs, the pair drawn in the view given (its frame of the pole, its clip as --clip) is the pair's own picture.
// Refused, with status 1 and a message: too few or too many numbers, a word, a NaN, a frame or a rotation that is not
// orthonormal (an entry off by 1e-6, or by 1e-8: an error of 2e-8, above the tolerance 1e-9), in S^3 a frame or a rotation
// of determinant -1 (a mirror image), and --viewmatrix with --yaw, --pitch, --pole or --centre; accepted: the identity,
// and an entry off by 1e-10. In S^4 the signs do not show: the program's frame (determinant -1) with the hidden row
// negated draws its picture byte for byte, and so does the rotation with a column negated (determinant -1) together
// with the frame's rows 0 and 3 negated.
// --colour: the default colours given as options draw the same pictures, byte for byte (the zippers, the pair with its
// context, S^4), and so do names that the picture does not draw (the zippers' colours in a pair picture, the context's
// without --context, a pair's without --pairs) and an option given twice whose last value is the default; another colour
// changes only the pixels where its part is drawn (those that are not white in the picture of that part alone), and
// changes some: the zippers, a pair's loop, the context. Refused: an unknown name or one of another program, two or four
// numbers, a number outside [0, 1], no '='.
// THE HEADER says whether each layer's renderer reached its cap of leaves (capped NAME yes|no); --polylines with an
// argument that holds a line break is refused before anything is built.
// Every program runs in this test's own folder, without ZIPPERS_DATA in its environment: the test sets ZIPPERS_DATA to a
// folder where each data file that the programs look for is a folder, which would stop any program that looked there.
// THE S^4 POLE moved toward any direction of the frame (view/s4.h: move_pole5_toward): along a frame axis it is
// move_pole5, to the last bits; the frame stays orthonormal; the pole goes to cos(th) P + sin(th) A; and moved back by -th
// along the same direction (expressed in the moved frame) the frame is the first again.
// About 30 seconds. Uses src/ and the programs (ZIPPERS_ZIPPER3, ZIPPERS_ZIPPER5: the paths of the binaries).

#include "analysis/pairs.h"
#include "geometry/on1/on1.h"
#include "geometry/pu21/pu21.h"
#include "output/png.h"
#include "output/polylines.h"
#include "render/renderer.h"
#include "view/options.h"
#include "view/s3.h"
#include "view/s4.h"

#include "render_test_support.h"

#include <dirent.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace z = zippers;
namespace pl = zippers::polylines;
using rtest::check;
using rtest::note;
using rtest::section;

#ifndef ZIPPERS_ZIPPER3
#define ZIPPERS_ZIPPER3 ""
#endif
#ifndef ZIPPERS_ZIPPER5
#define ZIPPERS_ZIPPER5 ""
#endif

namespace {

const double PLUS[3] = {0.05, 0.15, 0.85}, MINUS[3] = {0.85, 0.08, 0.08};
const double CONTEXT_PLUS[3] = {0.70, 0.75, 0.95}, CONTEXT_MINUS[3] = {0.95, 0.72, 0.72};

// run a program in the folder `dir`, without ZIPPERS_DATA; its exit status, and what it wrote on stderr
int run(const std::string& dir, const std::string& cmd, std::string& err)
{
    err.clear();
    FILE* p = popen(("cd '" + dir + "' && unset ZIPPERS_DATA && " + cmd + " 2>&1 >/dev/null < /dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) err.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

bool same_file(const std::string& a, const std::string& b)
{
    std::string x = rtest::read_file(a), y = rtest::read_file(b);
    return !x.empty() && x == y;
}

// the numbers of a header line, or nothing
std::vector<double> numbers(const pl::File& f, const std::string& key)
{
    std::vector<double> v;
    const std::string* s = f.value(key);
    if (!s || !pl::parse_list(*s, v)) v.clear();
    return v;
}

// a mark line "NAME x1 ... xn": its numbers
template <int N>
bool mark(const pl::File& f, const std::string& name, z::SpherePoint<N>& p)
{
    for (const std::string& m : f.values("mark")) {
        if (m.rfind(name + " ", 0) != 0) continue;
        std::vector<double> v;
        if (!pl::parse_list(m.substr(name.size() + 1), v) || v.size() != (size_t)N) return false;
        for (int k = 0; k < N; k++) p.x[k] = v[k];
        return true;
    }
    return false;
}

// renderers that hold a file's points, for the painters (which read only the points and the polylines)
template <class B>
struct Holder {
    z::Side<B> sd{};
    z::Letters<B> lt{};
    std::vector<z::Renderer<B>*> rs;
    ~Holder() { for (auto* r : rs) delete r; }
    // the whole layer
    const z::Renderer<B>* of(const pl::Layer& L)
    {
        auto* r = new z::Renderer<B>(sd, lt, 0.01, 1);
        r->pts = L.pts;
        r->starts = L.first;
        rs.push_back(r);
        return r;
    }
    // polyline k of a layer alone
    const z::Renderer<B>* polyline(const pl::Layer& L, size_t k)
    {
        const size_t d = (size_t)B::N, a = L.first[k], b = k + 1 < L.first.size() ? L.first[k + 1] : L.pts.size()/d;
        auto* r = new z::Renderer<B>(sd, lt, 0.01, 1);
        r->pts.assign(L.pts.begin() + (long)(d*a), L.pts.begin() + (long)(d*b));
        r->starts = {0};
        rs.push_back(r);
        return r;
    }
};

// the S^3 view of a header (viewmatrix, zoom, clip); false if it is not there
bool view3(const pl::File& f, z::s3::ViewState& v)
{
    std::vector<double> m = numbers(f, "viewmatrix"), zoom = numbers(f, "zoom"), clip = numbers(f, "clip");
    if (m.size() != 25 || zoom.size() != 1 || clip.size() != 1) return false;
    for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) v.Q[r][k] = m[4*r + k];
    for (int i = 0; i < 9; i++) v.rot[i] = m[16 + i];
    v.zoom = zoom[0];
    v.clip = clip[0];
    return true;
}

bool view5(const pl::File& f, z::s4::View5& v)
{
    std::vector<double> m = numbers(f, "viewmatrix"), U = numbers(f, "U"), zoom = numbers(f, "zoom"),
                        clip = numbers(f, "clip"), hid = numbers(f, "hidfog");
    if (m.size() != 34 || U.size() != 16 || zoom.size() != 1 || clip.size() != 1 || hid.size() != 1) return false;
    for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) v.Q[r][k] = m[5*r + k];
    for (int i = 0; i < 9; i++) v.rot[i] = m[25 + i];
    for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) v.U[r][k] = U[4*r + k];
    v.zoom = zoom[0];
    v.clip = clip[0];
    v.hidfog = hid[0];
    return true;
}

// the zippers' picture of an S^3 file, painted as zipper3 paints it (colours: Z+, Z-)
bool paint3(const pl::File& f, const std::string& out, int size, int thick, const double* cp = PLUS,
            const double* cm = MINUS)
{
    const pl::Layer *a = f.layer("Z+"), *b = f.layer("Z-");
    z::s3::ViewState v;
    z::SpherePoint<4> tp, tm;
    if (f.sphere != 3 || !a || !b || !view3(f, v) || !mark<4>(f, "t+", tp) || !mark<4>(f, "t-", tm)) return false;
    Holder<z::PU21> h;
    std::vector<z::s3::Layer<z::PU21>> layers;
    layers.push_back({h.of(*a), {cp[0], cp[1], cp[2]}});
    layers.push_back({h.of(*b), {cm[0], cm[1], cm[2]}});
    return z::s3::write_png(out.c_str(), size, layers, v, {tp, tm}, thick);
}

// the picture of pair k of an S^3 file of --pairs, as zipper3 paints it (pair_view: the frame of the pole, the clip of
// the pair, zoom 1, the rotation of R^3 of the view), with the context or not
bool paint3_pair(const pl::File& f, size_t k, bool context, const std::string& out, int size)
{
    const pl::Layer *a = f.layer("Z+"), *b = f.layer("Z-"), *pa = f.layer("pair.Z+"), *pb = f.layer("pair.Z-");
    z::s3::ViewState v;
    if (!a || !b || !pa || !pb || k >= pa->npoly() || k >= pb->npoly() || !view3(f, v)) return false;
    std::vector<double> pole = numbers(f, "pair." + std::to_string(k) + ".pole"),
                        clip = numbers(f, "pair." + std::to_string(k) + ".clip");
    if (pole.size() != 4 || clip.size() != 1) return false;
    z::s3::frame_with_pole(pole.data(), v.Q);
    v.clip = clip[0];
    v.zoom = 1;
    Holder<z::PU21> h;
    std::vector<z::s3::Layer<z::PU21>> layers;
    if (context) {
        layers.push_back({h.of(*a), {CONTEXT_PLUS[0], CONTEXT_PLUS[1], CONTEXT_PLUS[2]}, 0});
        layers.push_back({h.of(*b), {CONTEXT_MINUS[0], CONTEXT_MINUS[1], CONTEXT_MINUS[2]}, 0});
    }
    layers.push_back({h.polyline(*pa, k), {PLUS[0], PLUS[1], PLUS[2]}});
    layers.push_back({h.polyline(*pb, k), {MINUS[0], MINUS[1], MINUS[2]}});
    return z::s3::write_png(out.c_str(), size, layers, v, {}, 2);
}

bool paint5(const pl::File& f, const std::string& out, int size, int thick)
{
    const pl::Layer *a = f.layer("Z+"), *b = f.layer("Z-");
    z::s4::View5 v;
    z::SpherePoint<5> tp, tm;
    if (f.sphere != 4 || !a || !b || !view5(f, v) || !mark<5>(f, "t+", tp) || !mark<5>(f, "t-", tm)) return false;
    Holder<z::ON1<5>> h;
    std::vector<z::s4::Layer5> layers;
    layers.push_back({h.of(*a), {PLUS[0], PLUS[1], PLUS[2]}, 1});
    layers.push_back({h.of(*b), {MINUS[0], MINUS[1], MINUS[2]}, 1});
    return z::s4::write_png5(out.c_str(), size, layers, v, {tp, tm}, thick);
}

// the pixels of a PNG
bool pixels(const std::string& fn, std::vector<uint8_t>& rgb, int& w, int& h) { return z::png::read(fn, rgb, w, h); }

// another colour for a layer: the pixels that differ from the default picture are among the pixels that are not white in
// the picture of the layer alone; some differ
void only_its_layer(const std::string& what, const std::string& dflt, const std::string& other, const std::string& alone)
{
    std::vector<uint8_t> A, B, L;
    int w1, h1, w2, h2, w3, h3;
    if (!check(pixels(dflt, A, w1, h1) && pixels(other, B, w2, h2) && pixels(alone, L, w3, h3) && w1 == w2 && h1 == h2 &&
               w1 == w3 && h1 == h3, "%s: the three pictures can be read, at one size", what.c_str()))
        return;
    size_t diff = 0, outside = 0;
    for (size_t i = 0; i < A.size(); i += 3) {
        bool d = A[i] != B[i] || A[i+1] != B[i+1] || A[i+2] != B[i+2];
        bool white = L[i] == 255 && L[i+1] == 255 && L[i+2] == 255;
        diff += d;
        outside += d && white;
    }
    check(diff > 0, "%s: the other colour changes the picture", what.c_str());
    check(outside == 0, "%s: %zu pixels changed where the layer is not drawn", what.c_str(), outside);
    note("%s: %zu pixels changed, none outside the layer", what.c_str(), diff);
}

// a File of every kind of finite float, for the round trip
pl::File sample_file()
{
    pl::File f;
    f.sphere = 3;
    f.add("program", "a test");
    f.add("empty", "");
    f.add("repeated", "one");
    f.add("repeated", "two  with  blanks ");
    pl::Layer a;
    a.name = "Z+";
    const float specials[] = {0.f, -0.f, 1.f, -1.f, 1e-45f, -1e-45f, 1.17549435e-38f, 3.4028235e38f, -3.4028235e38f,
                              0.1f, 1.f/3, -2.5f, 7.f};
    for (float x : specials) a.pts.push_back(x);
    while (a.pts.size() % 4) a.pts.push_back(0.5f);
    for (int k = 0; k < 4000; k++) a.pts.push_back((float)rtest::uniform(-1, 1));
    const size_t M = a.pts.size()/4;
    a.first = {0, 3, 7, (uint32_t)(M/2), (uint32_t)(M - 1)};
    a.root = {0, -1, 2, 1, 0};
    a.letter = {5, 7, -1, 0, 35};
    pl::Layer e;
    e.name = "empty";
    pl::Layer b;
    b.name = "pair.Z-";
    b.pts = {0.25f, 0.5f, 0.75f, 1.f};
    b.first = {0};
    b.root = {-1};
    b.letter = {-1};
    f.layers = {a, e, b};
    f.rootd = {0, 2.5534, 1.0/3};
    return f;
}

// the shortest %g form that reads back (exact() without its plain form), for comparison
std::string shortest_g(double x)
{
    char b[40];
    for (int d = 1; d <= 17; d++) {
        snprintf(b, sizeof b, "%.*g", d, x);
        double y = strtod(b, nullptr);
        if (!memcmp(&x, &y, sizeof x)) break;
    }
    return b;
}

bool is_link(const std::string& p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

// the names in a folder (but . and ..), sorted
std::vector<std::string> names_in(const std::string& dir)
{
    std::vector<std::string> v;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n != "." && n != "..") v.push_back(n);
        }
        closedir(d);
    }
    std::sort(v.begin(), v.end());
    return v;
}

// run f() in a child process; its exit status (0..255), or 1000 + the signal that ended it; what it wrote into `msg`
template <class F>
int in_child(F f, std::string& msg)
{
    int fds[2];
    if (pipe(fds) != 0) return -1;
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        std::string m;
        int code = f(m);
        if (!m.empty() && write(fds[1], m.data(), m.size()) < 0) code = 99;
        close(fds[1]);
        _exit(code);
    }
    close(fds[1]);
    msg.clear();
    char buf[4096];
    ssize_t k;
    while ((k = read(fds[0], buf, sizeof buf)) > 0) msg.append(buf, (size_t)k);
    close(fds[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) != pid) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : WIFSIGNALED(st) ? 1000 + WTERMSIG(st) : -1;
}

bool same_layer(const pl::Layer& a, const pl::Layer& b)
{
    return a.name == b.name && rtest::same_vector_bytes(a.first, b.first) && rtest::same_vector_bytes(a.root, b.root) &&
           rtest::same_vector_bytes(a.letter, b.letter) && rtest::same_vector_bytes(a.pts, b.pts);
}

bool same_file_data(const pl::File& a, const pl::File& b)
{
    if (a.sphere != b.sphere || a.keys != b.keys || a.layers.size() != b.layers.size() ||
        !rtest::same_vector_bytes(a.rootd, b.rootd))
        return false;
    for (size_t i = 0; i < a.layers.size(); i++)
        if (!same_layer(a.layers[i], b.layers[i])) return false;
    return true;
}

// the reader must refuse the text, with a message containing `why`
void refused(const rtest::TempDir& t, const std::string& name, const std::string& text, const std::string& why)
{
    const std::string fn = t.file("bad.zpl");
    rtest::write_file(fn, text);
    pl::File f;
    std::string err;
    bool ok = pl::read(fn, f, &err);
    check(!ok && err.find(why) != std::string::npos, "the reader refuses %s (it said: %s)", name.c_str(),
          ok ? "nothing: it read the file" : err.c_str());
}

// the same, the reader in a child process, so that a crash is reported as one
void refused_in_child(const rtest::TempDir& t, const std::string& name, const std::string& text, const std::string& why)
{
    const std::string fn = t.file("bad.zpl");
    rtest::write_file(fn, text);
    std::string msg;
    int code = in_child([&](std::string& m) {
        pl::File f;
        bool ok = pl::read(fn, f, &m);
        return ok ? 1 : 0;
    }, msg);
    check(code == 0 && msg.find(why) != std::string::npos && msg.find(fn) != std::string::npos,
          "the reader refuses %s, naming the file (%s: %s)", name.c_str(),
          code == 0 ? "refused" : code == 1 ? "it read the file" : code >= 1000 ? "the reader CRASHED" : "the child failed",
          msg.c_str());
}

void test_format(const rtest::TempDir& t)
{
    section("the format: a file written and read back is the same, bit for bit");
    const pl::File f = sample_file();
    std::string err;
    const std::string fn = t.file("sample.zpl");
    check(pl::write(fn, f, &err), "write the sample (%s)", err.c_str());
    pl::File g;
    check(pl::read(fn, g, &err), "read it back (%s)", err.c_str());
    check(same_file_data(f, g), "every key, layer, point and root as written");
    check(g.value("empty") && g.value("empty")->empty(), "an empty value");
    check(g.values("repeated").size() == 2 && g.values("repeated")[1] == "two  with  blanks ", "a repeated key, blanks kept");
    pl::File h;
    const std::string fn2 = t.file("sample2.zpl");
    check(pl::write(fn2, g, &err) && same_file(fn, fn2), "written again, the same bytes");
    // the size: the header and the blocks the format describes, nothing else
    const std::string bytes = rtest::read_file(fn);
    const size_t data = bytes.find("\ndata\n");
    size_t want = 0;
    for (const pl::Layer& L : f.layers) want += 12*L.npoly() + 4*L.pts.size();
    want += 8*f.rootd.size();
    check(data != std::string::npos && bytes.size() - (data + 6) == want, "the blocks are %zu bytes, as the format says", want);
    check(bytes.rfind("zippers polylines 1\nsphere 3\n", 0) == 0, "the first two lines");
    // little-endian: the first uint32 after "data" is first[0] = 0, then 3
    const unsigned char* p = (const unsigned char*)bytes.data() + data + 6;
    check(p[4] == 3 && p[5] == 0 && p[6] == 0 && p[7] == 0, "the uint32 are little-endian");

    section("the format: exact numbers, lists");
    size_t bad = 0;
    for (int k = 0; k < 20000; k++) {
        double x = k < 10000 ? rtest::uniform(-1, 1)*std::pow(10.0, (double)rtest::rnd(-30, 30)) : rtest::gauss();
        double y = strtod(pl::exact(x).c_str(), nullptr);
        bad += memcmp(&x, &y, sizeof x) != 0;
    }
    check(bad == 0, "20000 doubles read back bit for bit (%zu do not)", bad);
    double nz = -0.0, back = strtod(pl::exact(nz).c_str(), nullptr);
    check(pl::exact(nz) == "-0" && std::signbit(back), "-0 stays -0 (%s)", pl::exact(nz).c_str());
    check(pl::exact(0.1) == "0.1" && pl::exact(2.5534) == "2.5534" && pl::exact(1.0/3) == "0.3333333333333333",
          "the shortest decimals: %s %s %s", pl::exact(0.1).c_str(), pl::exact(2.5534).c_str(), pl::exact(1.0/3).c_str());
    check(pl::exact(-70) == "-70" && pl::exact(100) == "100" && pl::exact(10000) == "10000" && pl::exact(1e5) == "1e+05" &&
          pl::exact(1e23) == "1e+23" && pl::exact(1e-5) == "1e-05" && pl::exact(-2.5e-7) == "-2.5e-07" &&
          pl::exact(1.5e-4) == "0.00015",
          "the plain form when it is not longer: %s %s %s %s %s %s %s %s", pl::exact(-70).c_str(), pl::exact(100).c_str(),
          pl::exact(10000).c_str(), pl::exact(1e5).c_str(), pl::exact(1e23).c_str(), pl::exact(1e-5).c_str(),
          pl::exact(-2.5e-7).c_str(), pl::exact(1.5e-4).c_str());
    size_t longer = 0, back_bad = 0;
    for (int k = 0; k < 20000; k++) {
        double x = (double)rtest::rnd(-1000, 1000)*std::pow(10.0, (double)rtest::rnd(-8, 25));
        if (k % 2) x = rtest::uniform(-1, 1)*std::pow(10.0, (double)rtest::rnd(-12, 22));
        const std::string e = pl::exact(x);
        double y = strtod(e.c_str(), nullptr);
        back_bad += memcmp(&x, &y, sizeof x) != 0;
        longer += e.size() > shortest_g(x).size();
    }
    check(back_bad == 0 && longer == 0, "20000 more read back (%zu do not), none longer than the shortest %%g (%zu are)",
          back_bad, longer);
    std::vector<double> v;
    check(pl::parse_list("1,2.5,-3e-2", v) && v.size() == 3 && v[2] == -3e-2, "a comma-separated list");
    check(pl::parse_list("1 2 3 4", v) && v.size() == 4, "a blank-separated list");
    check(pl::parse_list("", v) && v.empty(), "an empty list");
    check(!pl::parse_list("1,,2", v) && !pl::parse_list("1,2,", v) && !pl::parse_list("1,a", v) && !pl::parse_list("x", v),
          "lists that are not numbers are refused");

    section("the format: the reader refuses what is not a polyline file of format 1");
    const std::string good = rtest::read_file(fn);
    const size_t d = good.find("\ndata\n") + 6;
    const std::string head = good.substr(0, d), blocks = good.substr(d);
    auto replace = [&](std::string s, const std::string& a, const std::string& b) {
        size_t i = s.find(a);
        return i == std::string::npos ? s : s.replace(i, a.size(), b);
    };
    refused(t, "another magic line", replace(good, "zippers polylines 1", "zipper polylines 1"), "not a polyline file");
    refused(t, "another version", replace(good, "zippers polylines 1", "zippers polylines 2"), "another format");
    refused(t, "no data line", head.substr(0, d - 5), "does not end with a line \"data\"");
    refused(t, "a missing sphere", replace(good, "sphere 3\n", ""), "no line \"sphere N\"");
    refused(t, "a repeated sphere", replace(good, "sphere 3\n", "sphere 3\nsphere 3\n"), "the sphere must be given once");
    refused(t, "sphere 5", replace(good, "sphere 3\n", "sphere 5\n"), "the sphere must be given once, 2, 3 or 4");
    refused(t, "a missing roots line", replace(good, "roots 3\n", ""), "no line \"roots R\"");
    refused(t, "a bad layer line", replace(good, "layer empty 0 0\n", "layer empty 0\n"), "a layer is");
    refused(t, "a layer count that is not a number", replace(good, "layer empty 0 0\n", "layer empty 0 x\n"), "a layer is");
    refused(t, "blocks one byte short", good.substr(0, good.size() - 1), "the blocks hold");
    refused(t, "blocks one byte long", good + "x", "the blocks hold");
    refused(t, "a layer with one point more than the blocks", replace(good, "layer pair.Z- 1 1\n", "layer pair.Z- 1 2\n"),
            "the blocks hold");
    {   // first[0] = 1 in the layer Z+ (the first uint32 of the blocks): its polyline 0 is points 1..2, not empty
        std::string s = good;
        s[d] = 1;
        refused(t, "a first point that is not 0", s, "has no points");
        s = good;                                   // first[1] = first[2] (polyline 1 empty): the second uint32 = 7
        s[d + 4] = 7;
        refused(t, "a polyline with no points", s, "has no points");
        s = good;                                   // root[0] = 3, past the 3 roots: the sixth uint32
        s[d + 20] = 3;
        refused(t, "a root out of range", s, "names a root that is not there");
    }
    refused(t, "an empty file", "", "not a polyline file");
    pl::File w = sample_file();
    std::string err2;
    w.add("bad key", "x");
    check(!pl::write(t.file("w1.zpl"), w, &err2), "the writer refuses a key with a blank");
    w = sample_file();
    w.add("key", "two\nlines");
    check(!pl::write(t.file("w2.zpl"), w, &err2), "the writer refuses a value with a newline");
    w = sample_file();
    w.add("roots", "1");
    check(!pl::write(t.file("w3.zpl"), w, &err2), "the writer refuses a key of the format's own");
    w = sample_file();
    w.layers[0].root[0] = 7;
    check(!pl::write(t.file("w4.zpl"), w, &err2), "the writer refuses a root out of range");
    w = sample_file();
    w.layers[0].pts.push_back(1.f);
    check(!pl::write(t.file("w5.zpl"), w, &err2), "the writer refuses floats that are not whole points");
    w = sample_file();
    w.layers[0].first[1] = 0;
    check(!pl::write(t.file("w6.zpl"), w, &err2), "the writer refuses a polyline with no points");

    section("the format: numbers that are not finite, counts that cannot fit, changed counts that keep the size");
    {   // S^3: the layers A (2 polylines, 3 points), E (empty) and B (1 polyline, 2 points), 2 roots. The blocks, from
        // byte d0: A's first points 8 bytes, roots 8, letters 8, points 48; B's first point, root and letter 4 each, its
        // points 32; the distances 16
        pl::File g;
        g.sphere = 3;
        g.add("program", "x");
        pl::Layer a, e, b;
        a.name = "A"; e.name = "E"; b.name = "B";
        a.pts = {0.5f, 0.5f, 0.5f, 0.5f, 1, 0, 0, 0, 0, 1, 0, 0};
        a.first = {0, 1}; a.root = {0, 1}; a.letter = {3, 4};
        b.pts = {0, 0, 0, 1, 0, 0, 1, 0};
        b.first = {0}; b.root = {-1}; b.letter = {-1};
        g.layers = {a, e, b};
        g.rootd = {0, 2.5};
        const std::string gfn = t.file("small.zpl");
        check(pl::write(gfn, g, &err2), "write the small file (%s)", err2.c_str());
        const std::string sm = rtest::read_file(gfn);
        const size_t d0 = sm.find("\ndata\n") + 6;
        auto rep = [&](std::string x, const std::string& from, const std::string& to) {
            size_t i = x.find(from);
            return i == std::string::npos ? x : x.replace(i, from.size(), to);
        };
        const uint32_t nan = 0x7fc00000u, pinf = 0x7f800000u, ninf = 0xff800000u;
        std::string x = sm;
        memcpy(&x[d0 + 24 + 4], &nan, 4);
        refused(t, "a NaN among the points", x, "bad.zpl: layer A: point 0 is not a finite number (coordinate 1)");
        x = sm;
        memcpy(&x[d0 + 24 + 8], &pinf, 4);
        refused(t, "+inf among the points", x, "bad.zpl: layer A: point 0 is not a finite number (coordinate 2)");
        x = sm;
        memcpy(&x[d0 + 24 + 16 + 12], &ninf, 4);
        refused(t, "-inf among the points", x, "bad.zpl: layer A: point 1 is not a finite number (coordinate 3)");
        const double dn = std::nan("");
        x = sm;
        memcpy(&x[x.size() - 8], &dn, 8);
        refused(t, "a NaN among the distances", x, "bad.zpl: the distances: root 1 has a distance that is not a finite number");
        for (double inf : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
            x = sm;
            memcpy(&x[x.size() - 16], &inf, 8);
            refused(t, inf > 0 ? "+inf among the distances" : "-inf among the distances", x,
                    "bad.zpl: the distances: root 0 has a distance that is not a finite number");
        }
        // one point more in A and one fewer in B, the size kept: A's last point is read from B's first point, root
        // (-1) and letter (-1), which are NaN as floats
        x = rep(rep(sm, "layer A 2 3\n", "layer A 2 4\n"), "layer B 1 2\n", "layer B 1 1\n");
        refused(t, "a point moved from one layer to another, the size kept", x, "layer A: point 3 is not a finite number");
        // counts whose announced size overflows 64 bits back to the size of the blocks (in a child: a crash is reported)
        refused_in_child(t, "counts that overflow 64 bits to the file's size (S4, one layer)",
                         "zippers polylines 1\nsphere 4\nlayer A 0 922337203685477582\nroots 0\ndata\n" + std::string(24, '\0'),
                         "layer A: 0 polylines and 922337203685477582 points cannot fit in the 24 bytes of the blocks");
        refused_in_child(t, "counts that overflow 64 bits to the file's size (S3, two layers)",
                         "zippers polylines 1\nsphere 3\nlayer A 1 576460752303423489\nlayer B 0 576460752303423488\n"
                         "roots 0\ndata\n" + std::string(28, '\0'), "cannot fit in the 28 bytes of the blocks");
        refused_in_child(t, "a count of roots larger than the file", "zippers polylines 1\nsphere 2\nroots 999999999999999999\ndata\n" +
                         std::string(16, '\0'), "999999999999999999 roots cannot fit in the 16 bytes of the blocks");
        refused_in_child(t, "a count of polylines larger than the file",
                         "zippers polylines 1\nsphere 2\nlayer A 999999999999999999 1\nroots 0\ndata\n" + std::string(24, '\0'),
                         "cannot fit in the 24 bytes of the blocks");
        // the writer: what is not finite is refused, naming the file, the layer or the distances, and the index
        pl::File w2 = g;
        w2.layers[2].pts[5] = std::numeric_limits<float>::quiet_NaN();
        bool ok = pl::write(t.file("w7.zpl"), w2, &err2);
        check(!ok && err2 == "cannot write " + t.file("w7.zpl") + ": layer B: point 1 is not a finite number (coordinate 1)",
              "the writer refuses a NaN point (%s)", err2.c_str());
        w2 = g;
        w2.layers[0].pts[0] = -std::numeric_limits<float>::infinity();
        ok = pl::write(t.file("w8.zpl"), w2, &err2);
        check(!ok && err2.find("layer A: point 0 is not a finite number (coordinate 0)") != std::string::npos,
              "the writer refuses -inf (%s)", err2.c_str());
        w2 = g;
        w2.rootd[1] = std::numeric_limits<double>::infinity();
        ok = pl::write(t.file("w9.zpl"), w2, &err2);
        check(!ok && err2 == "cannot write " + t.file("w9.zpl") + ": the distances: root 1 has a distance that is not a finite number",
              "the writer refuses an infinite distance (%s)", err2.c_str());
        check(rtest::read_file(t.file("w7.zpl")).empty() && rtest::read_file(t.file("w9.zpl")).empty(),
              "and writes nothing then");
    }

    section("the format: the write by rename: a write that fails leaves nothing behind, and says why");
    {
        rtest::TempDir wd;
        const pl::File f1 = sample_file();
        const std::string a = wd.file("a.zpl");
        check(pl::write(a, f1, &err2), "a write (%s)", err2.c_str());
        check(names_in(wd.path) == std::vector<std::string>{"a.zpl"}, "leaves only its file (%zu names)", names_in(wd.path).size());
        const std::string before = rtest::read_file(a);
        mkdir(wd.file("adir").c_str(), 0777);
        bool ok = pl::write(wd.file("adir"), f1, &err2);
        check(!ok && err2 == "cannot write " + wd.file("adir") + ": " + strerror(EISDIR), "a folder of the name: %s", err2.c_str());
        ok = pl::write(wd.file("nowhere/x.zpl"), f1, &err2);
        check(!ok && err2 == "cannot write " + wd.file("nowhere/x.zpl") + ": " + strerror(ENOENT), "a folder that does not exist: %s",
              err2.c_str());
        if (geteuid() != 0) {                         // (the owner of every file could write a read-only one)
            const std::string ro = wd.file("ro.zpl");
            check(pl::write(ro, f1, &err2) && chmod(ro.c_str(), 0444) == 0, "a file made read-only");
            const std::string ro_before = rtest::read_file(ro);
            pl::File f2 = f1;
            f2.add("another", "header");
            ok = pl::write(ro, f2, &err2);
            check(!ok && err2 == "cannot write " + ro + ": " + strerror(EACCES) && rtest::read_file(ro) == ro_before,
                  "a read-only file: refused, and unchanged (%s)", err2.c_str());
            chmod(ro.c_str(), 0644);
            unlink(ro.c_str());
        }
        // a write that fails in the middle: a child whose files may not grow past 100000 bytes (SIGXFSZ ignored) writes
        // 1.6 MB over a.zpl
        pl::File big = f1;
        pl::Layer L;
        L.name = "big";
        L.first = {0}; L.root = {-1}; L.letter = {-1};
        L.pts.assign(4*100000, 0.25f);
        big.layers.push_back(L);
        std::string msg;
        int code = in_child([&](std::string& m) {
            signal(SIGXFSZ, SIG_IGN);
            struct rlimit rl;
            rl.rlim_cur = rl.rlim_max = 100000;
            if (setrlimit(RLIMIT_FSIZE, &rl) != 0) { m = "setrlimit failed"; return 2; }
            return pl::write(a, big, &m) ? 1 : 0;
        }, msg);
        check(code == 0 && msg == "cannot write " + a + ": " + strerror(EFBIG), "a write that fails in the middle: %s (%d)",
              msg.c_str(), code);
        check(rtest::read_file(a) == before, "the older file stays as it was");
        check(names_in(wd.path) == std::vector<std::string>{"a.zpl", "adir"}, "no temporary file is left (%zu names)",
              names_in(wd.path).size());
        // and a write that succeeds replaces the older file
        check(pl::write(a, big, &err2) && rtest::read_file(a).size() > before.size() &&
              names_in(wd.path) == std::vector<std::string>{"a.zpl", "adir"}, "a write that succeeds replaces it (%s)",
              err2.c_str());
        // a symbolic link of the name is written through: its target is written, and the link stays a link
        mkdir(wd.file("tdir").c_str(), 0777);
        const std::string real = wd.file("tdir/real.zpl"), link = wd.file("link.zpl");
        check(rtest::write_file(real, "older") && symlink("tdir/real.zpl", link.c_str()) == 0, "a link to a file");
        check(pl::write(link, f1, &err2), "write through the link (%s)", err2.c_str());
        struct stat ls;
        pl::File g1;
        check(lstat(link.c_str(), &ls) == 0 && S_ISLNK(ls.st_mode) && pl::read(real, g1) && same_file_data(g1, f1) &&
              names_in(wd.file("tdir")) == std::vector<std::string>{"real.zpl"},
              "the link stays a link, its target holds the file, and no temporary file is left in the target's folder");
        const std::string dangling = wd.file("dangling.zpl");
        check(symlink("tdir/new.zpl", dangling.c_str()) == 0 && pl::write(dangling, f1, &err2) &&
              lstat(dangling.c_str(), &ls) == 0 && S_ISLNK(ls.st_mode) && pl::read(wd.file("tdir/new.zpl"), g1),
              "a link to a file that does not exist yet: the file is made where the link points (%s)", err2.c_str());
        check(pl::target_of(link) == wd.file("tdir/real.zpl") && pl::target_of(a) == a, "target_of: %s",
              pl::target_of(link).c_str());
        // a link into a folder that does not exist: refused, the message naming the link's target
        const std::string into = wd.file("into.zpl");
        check(symlink("nofolder/x.zpl", into.c_str()) == 0 && !pl::write(into, f1, &err2) &&
              err2 == "cannot write " + into + ": its target " + wd.file("nofolder/x.zpl") + ": " + strerror(ENOENT) &&
              pl::write_problem(into) == err2.substr(("cannot write " + into + ": ").size()),
              "a link into a folder that does not exist: refused, naming its target (%s)", err2.c_str());
        // a loop of links, and a chain of more links than open() follows, are refused as open() refuses them (ELOOP);
        // a chain of as many as it follows is written through. (In the real folder: a link in the folder's own path
        // would count too.)
        char rb[PATH_MAX];
        const std::string rd = realpath(wd.path.c_str(), rb) ? std::string(rb) : wd.path;
        const std::string l1 = rd + "/loop1.zpl", l2 = rd + "/loop2.zpl";
        bool loop = false;
        const bool linked = symlink("loop2.zpl", l1.c_str()) == 0 && symlink("loop1.zpl", l2.c_str()) == 0;
        check(linked && !pl::write(l1, f1, &err2) && err2 == "cannot write " + l1 + ": " + strerror(ELOOP) &&
              pl::target_of(l1, &loop) == l1 && loop && is_link(l1) && is_link(l2),
              "a loop of links: refused, the links left as they were (%s)", err2.c_str());
        auto chain = [&](const std::string& stem, int links) {   // stem0 -> stem1 -> ... -> stem_end.zpl
            for (int k = 0; k < links; k++) {
                const std::string to = k + 1 < links ? stem + std::to_string(k + 1) : stem + "_end.zpl";
                if (symlink(to.c_str(), (rd + "/" + stem + std::to_string(k)).c_str()) != 0) return false;
            }
            return true;
        };
        const int n = pl::LINKS_MAX;
        bool fine = chain("ok", n) && pl::write(rd + "/ok0", f1, &err2) && pl::read(rd + "/ok_end.zpl", g1);
        int fd = open((rd + "/ok0").c_str(), O_RDONLY);
        check(fine && fd >= 0 && pl::target_of(rd + "/ok0", &loop) == rd + "/ok_end.zpl" && !loop,
              "a chain of %d links, as many as open() follows: written through (%s)", n, err2.c_str());
        if (fd >= 0) close(fd);
        fine = chain("long", n + 1) && !pl::write(rd + "/long0", f1, &err2) &&
               err2 == "cannot write " + rd + "/long0: " + strerror(ELOOP);
        errno = 0;
        fd = open((rd + "/long0").c_str(), O_WRONLY | O_CREAT, 0666);
        const int oe = errno;
        check(fine && fd < 0 && oe == ELOOP && !pl::read(rd + "/long_end.zpl", g1),
              "a chain of %d: refused, as open() refuses it (%s; open: %s)", n + 1, err2.c_str(), strerror(oe));
        if (fd >= 0) close(fd);
        if (geteuid() != 0) {                         // a writable file in a folder that may not be written in
            mkdir(wd.file("rodir").c_str(), 0777);
            const std::string inro = wd.file("rodir/f.zpl");
            check(pl::write(inro, f1, &err2) && chmod(wd.file("rodir").c_str(), 0555) == 0, "a file in a folder made read-only");
            const std::string ro_before = rtest::read_file(inro);
            bool ok3 = pl::write(inro, big, &err2);
            check(!ok3 && err2 == "cannot write " + inro + ": its folder " + wd.file("rodir") + " may not be written in (the "
                  "file is written there under a temporary name, then renamed): " + strerror(EACCES) &&
                  rtest::read_file(inro) == ro_before, "a writable file in a folder that may not be written in: refused, "
                  "and unchanged (%s)", err2.c_str());
            check(pl::write_problem(inro) == err2.substr(("cannot write " + inro + ": ").size()), "write_problem says the same");
            chmod(wd.file("rodir").c_str(), 0755);
        }
    }
}

std::string zipper3() { return std::string("'") + ZIPPERS_ZIPPER3 + "'"; }
std::string zipper5() { return std::string("'") + ZIPPERS_ZIPPER5 + "'"; }

// run a program; check that it succeeded, and read its polyline file (when it has one)
bool run_ok(const rtest::TempDir& t, const std::string& cmd, const std::string& what)
{
    std::string err;
    int st = run(t.path, cmd, err);
    return check(st == 0, "%s: exit %d: %s", what.c_str(), st, err.substr(0, 400).c_str());
}

bool read_ok(const rtest::TempDir& t, const std::string& name, pl::File& f)
{
    std::string err;
    return check(pl::read(t.file(name), f, &err), "read %s (%s)", name.c_str(), err.c_str());
}

void test_pictures_s3(const rtest::TempDir& t)
{
    section("S3: the picture painted from the file is the program's, byte for byte");
    struct Case { const char* what; const char* args; int size, thick; };
    const Case cases[] = {
        {"the default view, R = 0", "-R 0 -a 0.04 -s 300", 300, 0},
        {"a turned view (--yaw, --pitch, --pole, --zoom, --clip), the first shell", "-R 2.554 -a 0.04 -s 300 --yaw 30 --pitch -10 --pole 12 7 --zoom 1.5 --clip 2.5", 300, 0},
        {"a zoomed still at a point (culling and the focus ball by default)", "-R 2.554 -a 0.04 -s 240 --centre 0.1152,-0.1133,0.8356,0.5250 --zoom 30", 240, 0},
        {"--only + and --thick 1", "-R 0 -a 0.04 -s 200 --only + --thick 1", 200, 1},
    };
    for (const Case& c : cases) {
        if (!run_ok(t, zipper3() + " " + c.args + " -o a.png --polylines a.zpl", c.what)) continue;
        pl::File f;
        if (!read_ok(t, "a.zpl", f)) continue;
        check(paint3(f, t.file("b.png"), c.size, c.thick), "%s: paint the file", c.what);
        check(same_file(t.file("a.png"), t.file("b.png")), "%s: the same picture", c.what);
        // the header
        check(f.sphere == 3 && f.value("program") && *f.value("program") == "zipper3" && f.value("example") &&
              *f.value("example") == "cartwright-steger", "%s: sphere 3, program, example", c.what);
        const pl::Layer *a = f.layer("Z+"), *b = f.layer("Z-");
        check(a && b && f.layers.size() == 2 && !f.rootd.empty() && f.rootd[0] == 0, "%s: two layers, the roots from 0",
              c.what);
        if (a) {
            bool ok = true;
            for (size_t k = 0; k < a->npoly(); k++) ok = ok && a->root[k] >= 0 && a->root[k] < (int)f.rootd.size() &&
                                                         a->letter[k] >= 0 && a->letter[k] < 36;
            check(ok, "%s: every polyline of Z+ records its root and its letter", c.what);
        }
        check(f.values("capped") == std::vector<std::string>{"Z+ no", "Z- no"}, "%s: capped Z+ no, capped Z- no", c.what);
    }
    {   // the header's numbers are the program's: --polylines with no -o, and the same with -o, give the same file
        std::string e1, e2;
        int s1 = run(t.path, zipper3() + " -R 0 -a 0.04 --yaw 10 --polylines p1.zpl", e1);
        int s2 = run(t.path, zipper3() + " -R 0 -a 0.04 --yaw 10 -o x.png --polylines p2.zpl", e2);
        pl::File f1, f2;
        std::string r1, r2;
        bool ok1 = pl::read(t.file("p1.zpl"), f1, &r1), ok2 = pl::read(t.file("p2.zpl"), f2, &r2);
        if (check(s1 == 0 && s2 == 0 && ok1 && ok2, "the two runs (exit %d, %d; %s %s)", s1, s2, r1.c_str(), r2.c_str())) {
            for (pl::File* g : {&f1, &f2})               // the command lines differ
                g->keys.erase(std::remove_if(g->keys.begin(), g->keys.end(),
                                             [](const std::pair<std::string, std::string>& kv) { return kv.first == "command"; }),
                              g->keys.end());
            check(same_file_data(f1, f2), "without -o and with it, the same curves and header (but the command line)");
        }
        check(!rtest::read_file(t.file("x.png")).empty(), "the picture is written as well");
    }
    {   // a cap of leaves that is reached: the header says so, for each zipper
        pl::File f;
        if (run_ok(t, zipper3() + " -R 0 -a 0.04 --cap 1000 --polylines cap.zpl", "zipper3 --cap 1000") && read_ok(t, "cap.zpl", f))
            check(f.values("capped") == std::vector<std::string>{"Z+ yes", "Z- yes"}, "zipper3 --cap 1000: capped Z+ yes, Z- yes");
        pl::File g;
        if (run_ok(t, zipper3() + " -R 0 -a 0.04 --only - --cap 1000 --polylines cap1.zpl", "zipper3 --only - --cap 1000") &&
            read_ok(t, "cap1.zpl", g))
            check(g.values("capped") == std::vector<std::string>{"Z+ no", "Z- yes"},
                  "zipper3 --only - --cap 1000: capped Z+ no, capped Z- yes");
    }

    section("S3: the pictures of linked pairs, from the file of --pairs");
    for (int k : {0, 5}) {
        for (bool ctx : {false, true}) {
            std::string what = "pair " + std::to_string(k) + (ctx ? " with --context" : "");
            if (!run_ok(t, zipper3() + " --pairs linked_pairs.txt --pairindex " + std::to_string(k) + (ctx ? " --context" : "") +
                           " -s 300 -o p.png --polylines p.zpl", what))
                continue;
            pl::File f;
            if (!read_ok(t, "p.zpl", f)) continue;
            check(f.value("pairs") && *f.value("pairs") == "13" && f.layer("pair.Z+") && f.layer("pair.Z+")->npoly() == 13 &&
                  f.layer("pair.Z-") && f.layer("pair.Z-")->npoly() == 13, "%s: 13 pairs, a polyline each in pair.Z+ and pair.Z-",
                  what.c_str());
            check(paint3_pair(f, (size_t)k, ctx, t.file("q.png"), 300), "%s: paint it", what.c_str());
            check(same_file(t.file("p.png"), t.file("q.png")), "%s: the same picture", what.c_str());
            const std::string* line = f.value("pair." + std::to_string(k) + ".line");
            check(line && line->rfind("pair " + std::to_string(k) + ": lk ", 0) == 0, "%s: its line (%s)", what.c_str(),
                  line ? line->c_str() : "none");
            check(f.values("capped") == std::vector<std::string>{"Z+ no", "Z- no", "pair.Z+ no", "pair.Z- no"},
                  "%s: no cap reached, in each layer", what.c_str());
        }
    }
}

void test_pictures_s4(const rtest::TempDir& t)
{
    section("S4: the picture painted from the file is the program's, byte for byte");
    struct Case { const char* what; const char* args; int size, thick; };
    const Case cases[] = {
        {"the default view, R = 0", "-R 0 -a 0.05 -s 300", 300, 0},
        {"a turned view (--yaw, --pitch, --turn, --turn2, --hidfog, --zoom, --clip), the first shell", "-R 1.318 -a 0.05 -s 300 --yaw 20 --pitch 5 --turn 30 --turn2 -15 --hidfog 1.2 --zoom 1.3 --clip 2.8", 300, 0},
        {"a zoomed still at a cusp point (culling and the focus ball by default)", "-R 1.318 -a 0.05 -s 240 --centre 0,0,0,0,1,1 --zoom 10", 240, 0},
        {"--only - and --thick 1", "-R 0 -a 0.05 -s 200 --only - --thick 1", 200, 1},
    };
    for (const Case& c : cases) {
        if (!run_ok(t, zipper5() + " " + c.args + " -o a.png --polylines a.zpl", c.what)) continue;
        pl::File f;
        if (!read_ok(t, "a.zpl", f)) continue;
        check(paint5(f, t.file("b.png"), c.size, c.thick), "%s: paint the file", c.what);
        check(same_file(t.file("a.png"), t.file("b.png")), "%s: the same picture", c.what);
        check(f.sphere == 4 && f.value("program") && *f.value("program") == "zipper5" && f.layer("Z+") && f.layer("Z-"),
              "%s: sphere 4, program, layers", c.what);
        check(f.values("capped") == std::vector<std::string>{"Z+ no", "Z- no"}, "%s: capped Z+ no, capped Z- no", c.what);
    }
    pl::File f;
    if (run_ok(t, zipper5() + " -R 0 -a 0.05 --cap 1000 --polylines cap.zpl", "zipper5 --cap 1000") && read_ok(t, "cap.zpl", f))
        check(f.values("capped") == std::vector<std::string>{"Z+ yes", "Z- yes"}, "zipper5 --cap 1000: capped Z+ yes, Z- yes");
}

// a header's viewmatrix, as an option's value
std::string viewmatrix_of(const pl::File& f)
{
    const std::string* s = f.value("viewmatrix");
    return s ? *s : std::string();
}

void test_viewmatrix(const rtest::TempDir& t)
{
    section("--viewmatrix: the view of a run, given as matrices, draws the same picture");
    struct Case { const char* what; const char* prog; const char* base; const char* view; };
    const Case cases[] = {
        {"S3, the default view", "3", "-R 0 -a 0.04 -s 240", ""},
        {"S3, --yaw --pitch --pole", "3", "-R 0 -a 0.04 -s 240", "--yaw 40 --pitch -25 --pole 15 -8"},
        {"S3, --centre and --pole", "3", "-R 2.554 -a 0.08 -s 240 --zoom 2", "--centre 0.3,-0.2,0.5,0.6 --pole 3 4"},
        {"S4, the default view", "5", "-R 0 -a 0.05 -s 240 --turn 25", ""},
        {"S4, --yaw --pitch", "5", "-R 0 -a 0.05 -s 240 --turn2 10", "--yaw -30 --pitch 40"},
        {"S4, --centre", "5", "-R 1.318 -a 0.08 -s 240 --hidfog 1.1", "--centre 0.2,0.1,-0.3,0.5,0.4"},
    };
    for (const Case& c : cases) {
        const std::string prog = c.prog[0] == '3' ? zipper3() : zipper5();
        if (!run_ok(t, prog + " " + c.base + " " + c.view + " -o a.png --polylines a.zpl", c.what)) continue;
        pl::File f;
        if (!read_ok(t, "a.zpl", f)) continue;
        const std::string vm = viewmatrix_of(f);
        if (!run_ok(t, prog + " " + c.base + " --viewmatrix " + vm + " -o b.png --polylines b.zpl", std::string(c.what) + ", as a matrix"))
            continue;
        check(same_file(t.file("a.png"), t.file("b.png")), "%s: the same picture", c.what);
        pl::File g;
        check(read_ok(t, "b.zpl", g) && viewmatrix_of(g) == vm, "%s: the same view in the header", c.what);
    }
    {   // with --pairs and -o: the pair in the view given (the frame of its pole, its clip as --clip) is its own picture
        const std::string what = "S3, pair 5 in the view given";
        if (run_ok(t, zipper3() + " --pairs linked_pairs.txt --pairindex 5 -s 240 -o p.png --polylines p.zpl", what)) {
            pl::File f;
            if (read_ok(t, "p.zpl", f)) {
                std::vector<double> m = numbers(f, "viewmatrix"), pole = numbers(f, "pair.5.pole");
                const std::string* clip = f.value("pair.5.clip");
                if (check(m.size() == 25 && pole.size() == 4 && clip, "%s: the header", what.c_str())) {
                    double Q[4][4];
                    z::s3::frame_with_pole(pole.data(), Q);
                    for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) m[4*r + k] = Q[r][k];
                    run_ok(t, zipper3() + " --pairs linked_pairs.txt --pairindex 5 -s 240 --viewmatrix " +
                              pl::exact_list(m.data(), 25, ',') + " --clip " + *clip + " -o q.png", what);
                    check(same_file(t.file("p.png"), t.file("q.png")), "%s: the same picture", what.c_str());
                }
            }
        }
    }

    section("--viewmatrix: refusals");
    const std::string id3 = "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,0,0,1";
    const std::string id5 = "1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,1,0,0,0,1,0,0,0,1";
    struct Bad { const char* prog; std::string args; const char* why; };
    const Bad bad[] = {
        {"3", "--viewmatrix " + id3.substr(0, id3.size() - 2), "25 numbers"},
        {"3", "--viewmatrix " + id3 + ",0", "25 numbers"},
        {"3", "--viewmatrix " + id3.substr(0, id3.size() - 1) + "x", "25 numbers"},
        {"3", "--viewmatrix nan" + id3.substr(1), "25 numbers"},
        {"3", "--viewmatrix 1.000001" + id3.substr(1), "frame of R^4 (the first 16 numbers) is not orthonormal"},
        {"3", "--viewmatrix 1.00000001" + id3.substr(1), "frame of R^4 (the first 16 numbers) is not orthonormal"},
        {"3", "--viewmatrix " + id3.substr(0, 32) + "1.000001" + id3.substr(33), "rotation of R^3 (the last 9) is not orthonormal"},
        {"3", "--viewmatrix " + id3 + " --yaw 10", "does not go with"},
        {"3", "--pitch 10 --viewmatrix " + id3, "does not go with"},
        {"3", "--viewmatrix " + id3 + " --pole 1 2", "does not go with"},
        {"3", "--viewmatrix " + id3 + " --centre 1,0,0,0", "does not go with"},
        {"3", "--viewmatrix -" + id3, "the frame of R^4 (the first 16 numbers) has determinant -1: its picture would be the mirror image"},
        {"3", "--viewmatrix " + id3.substr(0, 32) + "-" + id3.substr(32), "the rotation of R^3 (the last 9) has determinant -1"},
        {"3", "--viewmatrix -" + id3.substr(0, 32) + "-" + id3.substr(32), "the frame of R^4 (the first 16 numbers) has determinant -1"},
        {"5", "--viewmatrix " + id5.substr(0, id5.size() - 2), "34 numbers"},
        {"5", "--viewmatrix " + id5 + ",1", "34 numbers"},
        {"5", "--viewmatrix 0.999999" + id5.substr(1), "frame of R^5 (the first 25 numbers) is not orthonormal"},
        {"5", "--viewmatrix " + id5.substr(0, 50) + "0.999999" + id5.substr(51), "rotation of R^3 (the last 9) is not orthonormal"},
        {"5", "--viewmatrix " + id5 + " --yaw 10", "does not go with"},
        {"5", "--centre 1,0,0,0,0 --viewmatrix " + id5, "does not go with"},
    };
    for (const Bad& b : bad) {
        std::string err;
        const std::string prog = b.prog[0] == '3' ? zipper3() : zipper5();
        int st = run(t.path, prog + " -R 0 -a 0.3 " + b.args + " -o r.png", err);
        check(st == 1 && err.find(b.why) != std::string::npos && err.find(": R=") == std::string::npos,
              "zipper%s %s: refused, exit 1, nothing built (exit %d: %s)", b.prog, b.args.substr(0, 60).c_str(), st,
              err.substr(0, 200).c_str());
    }
    {   // the identity, and a frame within the tolerance, are accepted
        std::string err;
        int st = run(t.path, zipper3() + " -R 0 -a 0.3 --viewmatrix " + id3 + " -o r.png", err);
        check(st == 0, "zipper3 with the identity frame: accepted (exit %d: %s)", st, err.substr(0, 200).c_str());
        st = run(t.path, zipper3() + " -R 0 -a 0.3 --viewmatrix 1.0000000001" + id3.substr(1) + " -o r.png", err);
        check(st == 0, "zipper3 with an entry 1e-10 off: accepted (exit %d: %s)", st, err.substr(0, 200).c_str());
    }

    section("--viewmatrix in S4: the signs of the frame and of the rotation do not show");
    {   // the default view (--turn 0 --turn2 0: U = I), whose frame has determinant -1, as the program makes it
        pl::File f;
        const std::string base = " -R 1.318 -a 0.08 -s 240";
        if (run_ok(t, zipper5() + base + " -o m0.png --polylines m0.zpl", "S4, the default view") && read_ok(t, "m0.zpl", f)) {
            std::vector<double> m = numbers(f, "viewmatrix"), U = numbers(f, "U");
            bool unit = U.size() == 16;
            for (int i = 0; unit && i < 16; i++) unit = U[i] == (i % 5 == 0 ? 1.0 : 0.0);
            if (check(m.size() == 34 && unit, "the default view: 34 numbers, U the identity") &&
                check(z::viewopt::determinant(m.data(), 5) < 0 && z::viewopt::determinant(m.data() + 25, 3) > 0,
                      "the program's own frame has determinant -1, its rotation +1")) {
                // the frame with its fourth row (the hidden direction) negated: determinant +1
                std::vector<double> m1 = m, m2 = m, m3 = m;
                for (int k = 0; k < 5; k++) m1[15 + k] = -m1[15 + k];
                // the frame's rows 0 and 3 negated (the determinant kept); the rotation's first column negated
                for (int k = 0; k < 5; k++) { m2[k] = -m2[k]; m2[15 + k] = -m2[15 + k]; }
                for (int r = 0; r < 3; r++) m3[25 + 3*r] = -m3[25 + 3*r];
                std::vector<double> m23 = m2;
                for (int r = 0; r < 3; r++) m23[25 + 3*r] = -m23[25 + 3*r];
                const std::string v1 = pl::exact_list(m1.data(), 34, ','), v2 = pl::exact_list(m2.data(), 34, ','),
                                  v3 = pl::exact_list(m3.data(), 34, ','), v23 = pl::exact_list(m23.data(), 34, ',');
                if (run_ok(t, zipper5() + base + " --viewmatrix " + v1 + " -o m1.png", "S4, the hidden row negated"))
                    check(same_file(t.file("m0.png"), t.file("m1.png")),
                          "the frame of determinant +1 (its hidden row negated) draws the default picture, byte for byte");
                if (run_ok(t, zipper5() + base + " --viewmatrix " + v23 + " -o m23.png", "S4, rows 0 and 3, and a column"))
                    check(same_file(t.file("m0.png"), t.file("m23.png")),
                          "a rotation of determinant -1 (its first column negated), with the frame's rows 0 and 3 negated, "
                          "draws the default picture, byte for byte");
                if (run_ok(t, zipper5() + base + " --viewmatrix " + v3 + " -o m3.png", "S4, a column negated") &&
                    run_ok(t, zipper5() + base + " --viewmatrix " + v2 + " -o m2.png", "S4, rows 0 and 3 negated"))
                    check(same_file(t.file("m3.png"), t.file("m2.png")) && !same_file(t.file("m3.png"), t.file("m0.png")),
                          "the rotation of determinant -1 draws what the frame with rows 0 and 3 negated (determinant -1) "
                          "draws with the program's rotation, byte for byte (another picture than the default)");
            }
        }
    }

    section("--polylines: refused at the start when the file cannot be written or the header could not hold the command line");
    mkdir(t.file("adir").c_str(), 0777);
    for (const std::string& prog : {zipper3(), zipper5()}) {
        std::string err;
        int st = run(t.path, prog + " -R 0 -a 0.3 --polylines adir", err);
        check(st == 1 && err.find(std::string("cannot write adir: ") + strerror(EISDIR)) != std::string::npos &&
              err.find(": R=") == std::string::npos,
              "%s --polylines with a folder of the name: refused before the build (exit %d: %s)",
              prog == zipper3() ? "zipper3" : "zipper5", st, err.substr(0, 200).c_str());
        st = run(t.path, prog + " -R 0 -a 0.3 --polylines nofolder/x.zpl", err);
        check(st == 1 && err.find(std::string("cannot write nofolder/x.zpl: ") + strerror(ENOENT)) != std::string::npos &&
              err.find(": R=") == std::string::npos,
              "%s --polylines in a folder that does not exist: refused before the build (exit %d: %s)",
              prog == zipper3() ? "zipper3" : "zipper5", st, err.substr(0, 200).c_str());
    }
    for (const std::string& prog : {zipper3(), zipper5()}) {
        std::string err;
        int st = run(t.path, prog + " -R 0 -a 0.3 -o 'a\nb.png' --polylines nl.zpl", err);
        check(st == 1 && err.find("--polylines: an argument holds a line break") != std::string::npos &&
              err.find(": R=") == std::string::npos && rtest::read_file(t.file("nl.zpl")).empty(),
              "%s with an argument that holds a newline: refused, exit 1, nothing built (exit %d: %s)",
              prog == zipper3() ? "zipper3" : "zipper5", st, err.substr(0, 200).c_str());
    }
}

void test_colour(const rtest::TempDir& t)
{
    section("--colour: the default colours given again draw the same pictures");
    struct Same { const char* what; std::string prog, args, colours; };
    const Same same[] = {
        {"S3, the zippers", zipper3(), "-R 0 -a 0.04 -s 240", "--colour Z+=0.05,0.15,0.85 --colour Z-=0.85,0.08,0.08"},
        {"S3, a pair with its context", zipper3(), "--pairs linked_pairs.txt --pairindex 5 --context -s 240",
         "--colour context.Z+=0.70,0.75,0.95 --colour context.Z-=0.95,0.72,0.72 --colour pair.Z+=0.05,0.15,0.85 "
         "--colour pair.Z-=0.85,0.08,0.08"},
        {"S3, a pair with its context: the zippers' own colours, which it does not draw", zipper3(),
         "--pairs linked_pairs.txt --pairindex 5 --context -s 240", "--colour Z+=0.05,0.15,0.85 --colour Z-=0,1,0"},
        {"S3, a pair without its context: the context's colours", zipper3(), "--pairs linked_pairs.txt --pairindex 2 -s 240",
         "--colour context.Z+=0,0,0 --colour context.Z-=0,1,0 --colour Z+=1,0,0"},
        {"S3, the zippers: the colours of a pair and of the context", zipper3(), "-R 0 -a 0.04 -s 240",
         "--colour pair.Z+=0,0,0 --colour context.Z-=0,0,0"},
        {"S3, given twice: the last counts", zipper3(), "-R 0 -a 0.04 -s 240",
         "--colour Z+=1,0,0 --colour Z-=0,1,0 --colour Z+=0.05,0.15,0.85 --colour Z-=0.85,0.08,0.08"},
        {"S4, the zippers", zipper5(), "-R 0 -a 0.05 -s 240", "--colour Z-=0.85,0.08,0.08 --colour Z+=0.05,0.15,0.85"},
    };
    for (const Same& s : same) {
        if (!run_ok(t, s.prog + " " + s.args + " -o a.png", s.what) ||
            !run_ok(t, s.prog + " " + s.args + " " + s.colours + " -o b.png", std::string(s.what) + " with --colour"))
            continue;
        check(same_file(t.file("a.png"), t.file("b.png")), "%s: the same picture", s.what);
    }

    section("--colour: another colour changes only its layer");
    struct Other { const char* what; std::string prog, args, colour, alone; };
    const Other other[] = {
        {"S3, Z-", zipper3(), "-R 2.554 -a 0.08 -s 240", "--colour Z-=0,1,0.5", "--only -"},
        {"S3, Z+", zipper3(), "-R 2.554 -a 0.08 -s 240", "--colour Z+=1,0.5,0", "--only +"},
        {"S3, the loop pair.Z+ of pair 3", zipper3(), "--pairs linked_pairs.txt --pairindex 3 -s 240", "--colour pair.Z+=0,0.7,0",
         "--only +"},
        {"S3, the context context.Z+ behind pair 4", zipper3(), "--pairs linked_pairs.txt --pairindex 4 --context -s 240",
         "--colour context.Z+=0.3,0.3,0.3", "--only + --colour pair.Z+=1,1,1"},
        {"S4, Z+", zipper5(), "-R 1.318 -a 0.08 -s 240", "--colour Z+=0.9,0.9,0", "--only +"},
        {"S4, Z-", zipper5(), "-R 1.318 -a 0.08 -s 240", "--colour Z-=0,0,0", "--only -"},
    };
    for (const Other& o : other) {
        if (!run_ok(t, o.prog + " " + o.args + " -o a.png", o.what) ||
            !run_ok(t, o.prog + " " + o.args + " " + o.colour + " -o b.png", o.what) ||
            !run_ok(t, o.prog + " " + o.args + " " + o.alone + " -o c.png", o.what))
            continue;
        only_its_layer(o.what, t.file("a.png"), t.file("b.png"), t.file("c.png"));
    }

    section("--colour: refusals");
    struct Bad { std::string prog; const char* value; };
    const Bad bad[] = {
        {zipper3(), "Q=0,0,0"}, {zipper3(), "wheel=0,0,0"}, {zipper3(), "Z+=0,0"}, {zipper3(), "Z+=0,0,0,0"},
        {zipper3(), "Z+=0,0,2"}, {zipper3(), "Z+=-0.1,0,0"}, {zipper3(), "Z+"}, {zipper3(), "=0,0,0"}, {zipper3(), "Z+=a,0,0"},
        {zipper5(), "pair.Z+=0,0,0"}, {zipper5(), "Z-=1,1"}, {zipper5(), "Z-=1,1,1.5"}, {zipper5(), "context.Z+=0,0,0"},
        {zipper3(), "context=0,0,0"}, {zipper3(), "context.Z=0,0,0"},
    };
    for (const Bad& b : bad) {
        std::string err;
        int st = run(t.path, b.prog + " -R 0 -a 0.3 --colour '" + b.value + "' -o r.png", err);
        check(st == 1 && err.find("--colour") != std::string::npos && err.find(": R=") == std::string::npos,
              "--colour %s: refused, exit 1, nothing built (exit %d: %s)", b.value, st, err.substr(0, 200).c_str());
    }
}

// the S^4 pole moved toward any direction of the frame (view/s4.h: move_pole5_toward)
void test_pole5()
{
    section("S4: the pole moved toward a direction (move_pole5_toward)");
    auto orth = [](const double Q[5][5]) {
        double e = 0;
        for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) {
            double s = 0;
            for (int k = 0; k < 5; k++) s += Q[i][k]*Q[j][k];
            e = std::max(e, std::fabs(s - (i == j)));
        }
        return e;
    };
    double worst_axis = 0, worst_orth = 0, worst_back = 0, worst_pole = 0;
    for (int trial = 0; trial < 200; trial++) {
        double p[5], n = 0;
        for (int k = 0; k < 5; k++) { p[k] = rtest::gauss(); n += p[k]*p[k]; }
        for (int k = 0; k < 5; k++) p[k] /= std::sqrt(n);
        double Q[5][5];
        z::s4::orthonormal_frame(p, Q);
        const double th = rtest::uniform(-3, 3);
        for (int a = 0; a < 4; a++) {          // along a frame axis it is move_pole5
            double A[5][5], B[5][5], e[4] = {0, 0, 0, 0};
            e[a] = 1;
            memcpy(A, Q, sizeof A); memcpy(B, Q, sizeof B);
            z::s4::move_pole5(A, a, th);
            z::s4::move_pole5_toward(B, e, th);
            for (int i = 0; i < 5; i++) for (int k = 0; k < 5; k++) worst_axis = std::max(worst_axis, std::fabs(A[i][k] - B[i][k]));
        }
        double a4[4], m = 0;                    // along any unit direction
        for (int k = 0; k < 4; k++) { a4[k] = rtest::gauss(); m += a4[k]*a4[k]; }
        for (int k = 0; k < 4; k++) a4[k] /= std::sqrt(m);
        double R[5][5], P0[5], A5[5] = {0, 0, 0, 0, 0};
        memcpy(R, Q, sizeof R);
        for (int k = 0; k < 5; k++) { P0[k] = Q[4][k]; for (int i = 0; i < 4; i++) A5[k] += a4[i]*Q[i][k]; }
        z::s4::move_pole5_toward(R, a4, th);
        worst_orth = std::max(worst_orth, orth(R));
        for (int k = 0; k < 5; k++)             // the pole goes to cos th P + sin th A
            worst_pole = std::max(worst_pole, std::fabs(R[4][k] - (std::cos(th)*P0[k] + std::sin(th)*A5[k])));
        double b4[4];                           // and back: the same direction, now expressed in the moved frame
        for (int i = 0; i < 4; i++) {
            double s = 0;
            for (int k = 0; k < 5; k++) s += (std::cos(th)*A5[k] - std::sin(th)*P0[k])*R[i][k];
            b4[i] = s;
        }
        z::s4::move_pole5_toward(R, b4, -th);
        for (int i = 0; i < 5; i++) for (int k = 0; k < 5; k++) worst_back = std::max(worst_back, std::fabs(R[i][k] - Q[i][k]));
    }
    check(worst_axis < 1e-15, "along a frame axis, move_pole5 to %g", worst_axis);
    check(worst_orth < 1e-14, "the frame stays orthonormal (%g)", worst_orth);
    check(worst_pole < 1e-15, "the pole goes to cos(th) P + sin(th) A (%g)", worst_pole);
    check(worst_back < 1e-14, "moved back by -th along the moved direction, the frame is the first (%g)", worst_back);
}

}  // namespace

int main()
{
    printf("test_polylines: the polyline file, and --polylines, --viewmatrix and --colour of zipper3 and zipper5\n");
    rtest::TempDir t;
    {   // a ZIPPERS_DATA in which every data file the programs look for is a folder: a program that looked there would
        // stop (core/data_files.h); the runs take it out of their environment
        const std::string dd = t.file("decoy");
        for (const char* p : {"", "/s3", "/s4", "/s3/linked_pairs.txt", "/s3/subst_4.00_9.50_t3_r1.txt",
                              "/s3/subst_4.00_9.50_t3.txt", "/s4/subst5_2.30_4.50_t1_r0.txt", "/linked_pairs.txt"})
            mkdir((dd + p).c_str(), 0777);
        setenv("ZIPPERS_DATA", dd.c_str(), 1);
    }
    test_format(t);
    test_pole5();
    if (!*ZIPPERS_ZIPPER3 || !*ZIPPERS_ZIPPER5) {
        rtest::end_section();
        printf("the programs are not built: only the format was tested\n");
        return 1;
    }
    test_pictures_s3(t);
    test_pictures_s4(t);
    test_viewmatrix(t);
    test_colour(t);
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
