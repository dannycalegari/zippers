// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_shells: in S^3 and S^4, the curves of a smaller R are exactly the first polylines of the curves of a larger R.
//
// build (render/world.h) draws the roots in order, nearest first, and every polyline records its root; so the curves of
// the ball B(R') are the first polylines of those of B(R), R' < R, when the roots of B(R') are the first roots of B(R)
// and the renderers do the same with them. This test builds the zippers at every shell (the radii at which the roots
// change, plus 1e-6, as a window steps through them), one after the other in the same World, and checks, for each shell
// and the next:
//   - the roots of the smaller ball are the first roots of the larger one, element for element (B::eq), with the same
//     distances, bit for bit;
//   - in each zipper, the roots of the polylines of the larger ball never decrease, so that the polylines of the first
//     roots come first; and those polylines are the polylines of the smaller ball: the same first points, roots and
//     letters, and the same points, bit for bit; and nothing else;
//   - no renderer reached its cap of leaves.
// On the stored default files: S^3 subst_4.00_9.50_t3_r1.txt (zipper3) and subst_4.00_9.50_t3.txt (zipper3 --pairs, link3),
// at every shell up to 5.5 (thirteen, from cs::shell_radii, as far as a window's + and - go) at ACC 0.1, and up to 3.33
// at ACC 0.01, the default; S^4 subst5_2.30_4.50_t1_r0.txt (zipper5) and subst5_2.30_4.50_t1.txt (film5), at every shell up
// to 2.5 (six, from imm::shell_radii, the distances of find_roots5's roots) at ACC 0.05 and at ACC 0.02, the default.
// THE CUT AT R: -R exactly at a shell's radius includes that shell: at each shell the roots are those of the shell plus
// 1e-6, the same elements and distances, and more than at the shell before.
// SEPARATE RUNS OF THE PROGRAMS, as a window uses them: zipper3 and zipper5 run with --polylines and -R exactly at a few
// shells; each file holds exactly the roots of the file of the run at the largest shell up to its R (their number is
// that of the distances <= R there), and each layer exactly the first polylines of that file's layer, those of its roots;
// no cap reached. S^3 at ACC 0.1 up to 5.3975 and at 0.01 up to 3.3258; S^4 at ACC 0.05 up to 2.4779 and at 0.02 up to
// 2.0634.
// And the programs' files are these builds: zipper3 and zipper5 run with --polylines write, layer for layer, the curves and
// the roots that build gives in this process for the same file, R and accuracy.
// Without the focus ball: with it the roots of fh B(R) come after all of B(R) (render/world.h), so the curves of a
// smaller R are not the first ones there. The programs run in this test's own folder, without ZIPPERS_DATA.
// About 70 seconds, and at most about 1.5 GB (S^4 at ACC 0.02 up to 2.4779). Uses src/, and the programs (ZIPPERS_ZIPPER3,
// ZIPPERS_ZIPPER5);
// ZIPPERS_REPO is the repository, for the data files.

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "geometry/pu21/pu21.h"
#include "output/polylines.h"
#include "render/world.h"

#include "render_test_support.h"

#include <sys/wait.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace z = zippers;
namespace pl = zippers::polylines;
using rtest::check;
using rtest::note;
using rtest::section;

#ifndef ZIPPERS_REPO
#define ZIPPERS_REPO "."
#endif
#ifndef ZIPPERS_ZIPPER3
#define ZIPPERS_ZIPPER3 ""
#endif
#ifndef ZIPPERS_ZIPPER5
#define ZIPPERS_ZIPPER5 ""
#endif

namespace {

// what a build drew: the roots, and for each zipper its polylines
template <class B>
struct Snap {
    std::vector<typename B::Elem> roots;
    std::vector<double> rootd;
    struct Z { std::vector<float> pts; std::vector<uint32_t> starts; std::vector<int> root, letter; bool capped; } z[2];
};

template <class B>
Snap<B> take(z::World<B>& W)
{
    Snap<B> s;
    s.roots = W.roots;
    s.rootd = W.rootd;
    z::Renderer<B>* r[2] = {W.rp, W.rn};
    for (int k = 0; k < 2; k++) {
        s.z[k].pts = std::move(r[k]->pts);           // (build makes new renderers next time)
        s.z[k].starts = std::move(r[k]->starts);
        s.z[k].root = std::move(r[k]->polyroot);
        s.z[k].letter = std::move(r[k]->polyletter);
        s.z[k].capped = r[k]->capped;
    }
    delete W.rp; W.rp = nullptr;                     // (their sets of keys, before the next build)
    delete W.rn; W.rn = nullptr;
    return s;
}

// the curves of `small` are the first polylines of `big`
template <class B>
void compare(const Snap<B>& small, const Snap<B>& big, const std::string& what)
{
    const size_t n = small.roots.size();
    bool roots = n <= big.roots.size();
    for (size_t i = 0; roots && i < n; i++)
        roots = B::eq(small.roots[i], big.roots[i]) && !memcmp(&small.rootd[i], &big.rootd[i], sizeof(double));
    check(roots, "%s: the %zu roots are the first of the %zu", what.c_str(), n, big.roots.size());
    for (int k = 0; k < 2; k++) {
        const auto &s = small.z[k], &b = big.z[k];
        const char* zn = k ? "Z-" : "Z+";
        check(!s.capped && !b.capped, "%s, %s: no cap reached", what.c_str(), zn);
        bool order = true;
        for (size_t i = 1; i < b.root.size(); i++) order = order && b.root[i - 1] <= b.root[i];
        check(order, "%s, %s: the polylines come in the order of their roots", what.c_str(), zn);
        size_t m = 0;                                  // the polylines of the first n roots
        while (m < b.root.size() && b.root[m] >= 0 && (size_t)b.root[m] < n) m++;
        const size_t N = (size_t)B::N, pts = m < b.starts.size() ? b.starts[m] : b.pts.size()/N;
        bool same = s.starts.size() == m && s.pts.size() == N*pts &&
                    std::equal(s.starts.begin(), s.starts.end(), b.starts.begin()) &&
                    std::equal(s.root.begin(), s.root.end(), b.root.begin()) &&
                    std::equal(s.letter.begin(), s.letter.end(), b.letter.begin()) &&
                    (s.pts.empty() || !memcmp(s.pts.data(), b.pts.data(), s.pts.size()*sizeof(float)));
        check(same, "%s, %s: the %zu polylines (%zu points) are the first %zu polylines of the larger ball (%zu points there)",
              what.c_str(), zn, s.starts.size(), s.pts.size()/N, m, pts);
    }
}

// build at every shell, in order, and compare each with the next
template <class P>
void shells(const std::string& file, const std::vector<double>& radii, double acc, size_t cap, const std::string& name)
{
    typedef typename P::B B;
    z::World<B> W;
    z::prepare<P>(file.c_str(), W);
    Snap<B> prev;
    for (size_t i = 0; i < radii.size(); i++) {
        z::build<P>(W, radii[i] + 1e-6, acc, cap);
        Snap<B> cur = take(W);
        if (i) {
            char what[200];
            snprintf(what, sizeof what, "%s ACC %g: R = %.4f inside R = %.4f", name.c_str(), acc, radii[i - 1], radii[i]);
            compare(prev, cur, what);
        }
        prev = std::move(cur);
    }
    note("%s at ACC %g: %zu shells, up to R = %.4f (%zu roots; %zu + %zu points)", name.c_str(), acc, radii.size(),
         radii.back(), prev.roots.size(), prev.z[0].pts.size()/(size_t)B::N, prev.z[1].pts.size()/(size_t)B::N);
}

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

// the program's polyline file is the build of this process
template <class P>
void program_file(const std::string& prog, const std::string& file, double R, double acc, size_t cap,
                  const rtest::TempDir& t, const std::string& name)
{
    typedef typename P::B B;
    std::string err;
    char cmd[2000];
    snprintf(cmd, sizeof cmd, "'%s' -f '%s' -R %.17g -a %.17g --polylines f.zpl", prog.c_str(), file.c_str(), R, acc);
    int st = run(t.path, cmd, err);
    pl::File f;
    if (!check(st == 0 && pl::read(t.file("f.zpl"), f, &err), "%s: the program's file (%s)", name.c_str(), err.c_str())) return;
    z::World<B> W;
    z::prepare<P>(file.c_str(), W);
    z::build<P>(W, R, acc, cap);
    const pl::Layer *a = f.layer("Z+"), *b = f.layer("Z-");
    check(a && b && f.sphere == B::N - 1, "%s: the layers Z+ and Z-", name.c_str());
    if (!a || !b) return;
    const z::Renderer<B>* r[2] = {W.rp, W.rn};
    const pl::Layer* L[2] = {a, b};
    for (int k = 0; k < 2; k++) {
        bool same = rtest::same_vector_bytes(L[k]->pts, r[k]->pts) && L[k]->first == r[k]->starts &&
                    std::equal(L[k]->root.begin(), L[k]->root.end(), r[k]->polyroot.begin(), r[k]->polyroot.end()) &&
                    std::equal(L[k]->letter.begin(), L[k]->letter.end(), r[k]->polyletter.begin(), r[k]->polyletter.end());
        check(same, "%s, %s: the file's polylines are the build's (%zu points)", name.c_str(), k ? "Z-" : "Z+",
              r[k]->pts.size()/(size_t)B::N);
    }
    check(rtest::same_vector_bytes(f.rootd, W.rootd), "%s: the roots' distances", name.c_str());
}

// -R exactly at each shell: the roots of the shell plus 1e-6, and more than at the shell before
template <class P>
void exact_cut(const std::vector<double>& radii, const std::string& name)
{
    typedef typename P::B B;
    size_t prev = 0, bad = 0, same = 0;
    std::string where;
    for (size_t i = 0; i < radii.size(); i++) {
        std::vector<typename B::Elem> r0, r1;
        std::vector<double> d0, d1;
        P::roots(radii[i], r0, d0);
        P::roots(radii[i] + 1e-6, r1, d1);
        bool eq = r0.size() == r1.size() && rtest::same_vector_bytes(d0, d1);
        for (size_t k = 0; eq && k < r0.size(); k++) eq = B::eq(r0[k], r1[k]);
        same += eq;
        if (!eq || (i && r0.size() <= prev)) {
            bad++;
            if (where.empty()) where = pl::exact(radii[i]) + ": " + std::to_string(r0.size()) + " roots, " +
                                       std::to_string(r1.size()) + " at the shell plus 1e-6, " + std::to_string(prev) +
                                       " at the shell before";
        }
        prev = r0.size();
    }
    check(bad == 0, "%s: at R exactly at each of the %zu shells, its roots are included (%zu as at the shell plus 1e-6)%s%s",
          name.c_str(), radii.size(), same, bad ? "; first wrong at R = " : "", where.c_str());
}

// the file of a run at a smaller shell holds exactly the first roots and polylines of the file of the largest run
bool prefix_of(const pl::File& small, const pl::File& big, double R, std::string& why)
{
    size_t n = 0;
    while (n < big.rootd.size() && big.rootd[n] <= R) n++;
    if (small.rootd.size() != n) {
        why = std::to_string(small.rootd.size()) + " roots, where the largest run has " + std::to_string(n) + " up to R";
        return false;
    }
    if (n && memcmp(small.rootd.data(), big.rootd.data(), n*sizeof(double))) { why = "other distances"; return false; }
    if (small.values("capped") != std::vector<std::string>{"Z+ no", "Z- no"} ||
        big.values("capped") != std::vector<std::string>{"Z+ no", "Z- no"}) {
        why = "a cap reached";
        return false;
    }
    for (const char* name : {"Z+", "Z-"}) {
        const pl::Layer *a = small.layer(name), *b = big.layer(name);
        if (!a || !b) { why = std::string("no layer ") + name; return false; }
        for (size_t i = 1; i < b->npoly(); i++)
            if (b->root[i - 1] > b->root[i]) { why = std::string(name) + ": the polylines not in the order of their roots"; return false; }
        size_t m = 0;
        while (m < b->npoly() && b->root[m] >= 0 && (size_t)b->root[m] < n) m++;
        const size_t d = (size_t)big.dim(), pts = m < b->npoly() ? b->first[m] : b->pts.size()/d;
        bool same = a->npoly() == m && a->pts.size() == d*pts && std::equal(a->first.begin(), a->first.end(), b->first.begin()) &&
                    std::equal(a->root.begin(), a->root.end(), b->root.begin()) &&
                    std::equal(a->letter.begin(), a->letter.end(), b->letter.begin()) &&
                    (a->pts.empty() || !memcmp(a->pts.data(), b->pts.data(), a->pts.size()*sizeof(float)));
        if (!same) {
            why = std::string(name) + ": " + std::to_string(a->npoly()) + " polylines, not the first " + std::to_string(m) +
                  " of the largest run";
            return false;
        }
    }
    return true;
}

// the program run at the largest radius and at the others (each exactly at a shell): each file is the first part of the
// largest one
void separate_runs(const std::string& prog, const std::string& file, const std::vector<double>& radii, double acc,
                   const rtest::TempDir& t, const std::string& name)
{
    auto file_at = [&](double R, const std::string& out, pl::File& f) {
        std::string err;
        const std::string cmd = "'" + prog + "' -f '" + file + "' -R " + pl::exact(R) + " -a " + pl::exact(acc) +
                                " --polylines " + out;
        int st = run(t.path, cmd, err);
        return check(st == 0 && pl::read(t.file(out), f, &err), "%s: the run at R = %s (exit %d: %s)", name.c_str(),
                     pl::exact(R).c_str(), st, err.substr(0, 300).c_str());
    };
    pl::File big;
    if (!file_at(radii.back(), "big.zpl", big)) return;
    std::string list;
    for (size_t i = 0; i + 1 < radii.size(); i++) {
        pl::File small;
        if (!file_at(radii[i], "small.zpl", small)) continue;
        std::string why;
        check(prefix_of(small, big, radii[i], why), "%s: the run at R = %s is the first part of the run at R = %s (%s)",
              name.c_str(), pl::exact(radii[i]).c_str(), pl::exact(radii.back()).c_str(), why.c_str());
        char r[32];
        snprintf(r, sizeof r, " %.4f", radii[i]);
        list += r;
    }
    note("%s at ACC %g: the runs at%s, each the first part of the run at %.4f (%zu roots; %zu + %zu points)", name.c_str(), acc,
         list.c_str(), radii.back(), big.rootd.size(),
         big.layer("Z+") ? big.layer("Z+")->pts.size()/(size_t)big.dim() : (size_t)0,
         big.layer("Z-") ? big.layer("Z-")->pts.size()/(size_t)big.dim() : (size_t)0);
}

}  // namespace

int main()
{
    printf("test_shells: the curves of a smaller R are the first polylines of those of a larger R (S3, S4)\n");
    const std::string repo = ZIPPERS_REPO;
    z::cs::setup();
    z::imm::setup();
    const std::string s3[2] = {repo + "/data/s3/subst_4.00_9.50_t3_r1.txt", repo + "/data/s3/subst_4.00_9.50_t3.txt"};
    const std::string s4[2] = {repo + "/data/s4/subst5_2.30_4.50_t1_r0.txt", repo + "/data/s4/subst5_2.30_4.50_t1.txt"};
    const char* n3[2] = {"S3 _t3_r1", "S3 _t3"};
    const char* n4[2] = {"S4 _t1_r0", "S4 _t1"};

    std::vector<double> sh3 = z::cs::shell_radii(5.5), sh3_fine;
    for (double r : sh3) if (r < 3.33) sh3_fine.push_back(r);
    std::vector<double> sh4 = z::imm::shell_radii(2.5), sh4_fine;
    for (double r : sh4) if (r < 2.1) sh4_fine.push_back(r);
    section("the shells");
    check(sh3.size() == 13 && sh3[0] == 0 && sh3[1] > 2.553 && sh3[1] < 2.554, "S3: 13 shells up to 5.5, from 0, then 2.553 (%zu)",
          sh3.size());
    const bool shells_ok = sh3.size() == 13 && sh3_fine.size() == 3 && sh4.size() == 6 && sh4_fine.size() == 4;
    check(sh4.size() == 6 && sh4[0] == 0 && sh4_fine.size() == 4 && sh4[5] > 2.477 && sh4[5] < 2.478,
          "S4: 6 shells up to 2.5, from 0, the last 2.4779; 4 up to 2.1 (%zu, %zu)", sh4.size(), sh4_fine.size());
    {   // imm::shell_radii: the distinct distances of find_roots5's roots, as they are
        std::vector<z::imm::M6> r5;
        std::vector<double> d5, direct;
        z::imm::find_roots5(2.5, r5, d5);
        for (double x : d5) if (direct.empty() || x > direct.back() + 1e-4) direct.push_back(x);
        check(rtest::same_vector_bytes(direct, sh4) && z::imm::shell_radii(0) == std::vector<double>{0.0} &&
              z::imm::shell_radii(2.1).size() == 4, "imm::shell_radii: the distinct distances of find_roots5's roots, bit "
              "for bit; {0} at R = 0; 4 up to 2.1");
    }
    std::string list;
    for (double r : sh3) list += " " + pl::exact((float)r);
    note("S3:%s", list.c_str());
    list.clear();
    for (double r : sh4) list += " " + pl::exact((float)r);
    note("S4:%s", list.c_str());

    section("the cut at R: -R exactly at a shell's radius includes that shell");
    exact_cut<z::cs::RenderPolicy>(sh3, "S3");
    exact_cut<z::imm::RenderPolicy>(sh4, "S4");

    std::vector<std::string> titles;                 // (section keeps the pointer)
    for (int f = 0; f < 2; f++) titles.push_back(std::string(n3[f]) + ": every shell up to 5.5 at ACC 0.1, up to 3.33 at ACC 0.01");
    for (int f = 0; f < 2; f++) titles.push_back(std::string(n4[f]) + ": every shell up to 2.5 at ACC 0.05 and 0.02");
    for (int f = 0; f < 2; f++) {
        section(titles[f].c_str());
        shells<z::cs::RenderPolicy>(s3[f], sh3, 0.1, 8000000, n3[f]);
        shells<z::cs::RenderPolicy>(s3[f], sh3_fine, 0.01, 8000000, n3[f]);
    }
    for (int f = 0; f < 2; f++) {
        section(titles[2 + f].c_str());
        shells<z::imm::RenderPolicy>(s4[f], sh4, 0.05, 20000000, n4[f]);
        shells<z::imm::RenderPolicy>(s4[f], sh4, 0.02, 20000000, n4[f]);
    }

    section("separate runs of the programs: each file is the first part of the file of the run at the largest shell");
    if (!*ZIPPERS_ZIPPER3 || !*ZIPPERS_ZIPPER5) {
        check(false, "the programs are not built");
    } else if (!shells_ok) {
        check(false, "the shells are not those of the first section, so the runs at chosen shells are not made");
    } else {
        rtest::TempDir t;
        separate_runs(ZIPPERS_ZIPPER3, s3[0], {sh3[0], sh3[1], sh3[4], sh3[7], sh3[11], sh3[12]}, 0.1, t, "zipper3");
        separate_runs(ZIPPERS_ZIPPER3, s3[0], {sh3_fine[0], sh3_fine[1], sh3_fine[2]}, 0.01, t, "zipper3");
        separate_runs(ZIPPERS_ZIPPER5, s4[0], {sh4[0], sh4[1], sh4[3], sh4[4], sh4[5]}, 0.05, t, "zipper5");
        separate_runs(ZIPPERS_ZIPPER5, s4[0], {sh4_fine[0], sh4_fine[1], sh4_fine[3]}, 0.02, t, "zipper5");
    }

    section("the programs' files are these builds");
    if (!*ZIPPERS_ZIPPER3 || !*ZIPPERS_ZIPPER5) {
        check(false, "the programs are not built");
    } else {
        rtest::TempDir t;
        program_file<z::cs::RenderPolicy>(ZIPPERS_ZIPPER3, s3[0], 2.554, 0.1, 8000000, t, "zipper3, R 2.554, ACC 0.1");
        program_file<z::imm::RenderPolicy>(ZIPPERS_ZIPPER5, s4[0], 1.318, 0.05, 20000000, t, "zipper5, R 1.318, ACC 0.05");
    }
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
