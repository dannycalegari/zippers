// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_view_s2: the S^2 camera of src/view/s2.h against the S^2 program.
//
// make_view is moved from legacy/s2/zipper.cc, which this test includes (with its main renamed; without its window), and
// apps/s2/zipper.cc uses the moved one (tests/unit/check_s2_moved_text.py): the two give the same rotation, bit for bit,
// on a grid of yaw, pitch and roll and at random angles. The other functions come from the program's window, which needs
// GLUT and is not compiled here (tests/unit/check_moved_text.py checks their text against it, token for token); this test
// checks what they do:
//   - rotx and roty are make_view(0, d) and make_view(d, 0) (to the last bit but the sign of zeros), and mul3 is the
//     product of rotations;
//   - view_angles: make_view of the angles it returns is the rotation again, to 1e-13, for random rotations, for
//     rotations built by the window's turns about the screen axes, and near yaw = +-90 degrees; a pure yaw and pitch has
//     roll 0, also when the window's turns make it within 1e-5 degree of the gimbal lock (h, the length of the last row's
//     last two entries, from 2e-16 to 2e-7: their rounding then decides the pitch of the first candidate, and the
//     candidate of the lock, with the pitch of the second row, gives the roll 0);
//   - fmt_num: within the tolerance, the shortest form, and no exponent between 1e-4 and 1e7;
//   - command_angles: the angles of make_view(30, -20, 10) are "30", "-20", "10"; a rotation that has drifted by 1e-3 has
//     none; after a thousand turns about the screen axes the text rebuilds the rotation to 1/1000 of a pixel;
//   - export_framing: at least 1024 pixels, the zoom rescaled to frame the window's square, and the limits;
//   - export_name and export_stem: the first name that does not exist (export_stem: for any of its extensions).
// Under a second.

// (the warnings the film engine silences for the program; the GCC form, which clang reads too)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#define main legacy_zipper_main
#include "zipper.cc"                       // legacy/s2/zipper.cc: ::View, ::make_view
#undef main
#pragma GCC diagnostic pop

#include "view/s2.h"
#include "render_test_support.h"

#include <cmath>
#include <cstring>
#include <string>

namespace s2 = zippers::s2;
using rtest::check;
using rtest::note;
using rtest::section;

namespace {

double err(const s2::View& a, const s2::View& b)
{
    double e = 0;
    for (int i = 0; i < 9; i++) e = std::max(e, std::fabs(a.m[i] - b.m[i]));
    return e;
}

s2::View rotation(double y, double p, double r) { return s2::make_view(y, p, r); }

}  // namespace

int main()
{
    printf("test_view_s2: the S2 camera (src/view/s2.h) against the S2 program\n");

    section("make_view: the moved one is the program's, bit for bit");
    size_t n = 0, bad = 0;
    for (double y = -360; y <= 360; y += 7.5)
        for (double p = -180; p <= 180; p += 7.5)
            for (double r : {0.0, -0.0, 10.0, -90.0, 179.0, 1e-9}) {
                ::View a = ::make_view(y, p, r);
                s2::View b = s2::make_view(y, p, r);
                n++;
                bad += memcmp(a.m, b.m, sizeof a.m) != 0;
            }
    for (int k = 0; k < 20000; k++) {
        double y = rtest::uniform(-720, 720), p = rtest::uniform(-720, 720), r = k % 3 ? rtest::uniform(-720, 720) : 0;
        ::View a = k % 2 ? ::make_view(y, p, r) : ::make_view(y, p);
        s2::View b = k % 2 ? s2::make_view(y, p, r) : s2::make_view(y, p);
        n++;
        bad += memcmp(a.m, b.m, sizeof a.m) != 0;
    }
    check(bad == 0, "%zu of %zu rotations differ", bad, n);
    note("%zu rotations compared", n);

    section("rotx, roty and mul3");
    double e = 0;
    for (double d = -400; d <= 400; d += 0.37) {
        e = std::max(e, err(s2::rotx(d), s2::make_view(0, d)));
        e = std::max(e, err(s2::roty(d), s2::make_view(d, 0)));
    }
    check(e == 0, "rotx(d) = make_view(0, d) and roty(d) = make_view(d, 0), exactly as numbers (largest difference %g)", e);
    {
        s2::View a = rotation(10, 20, 30), b = rotation(-40, 5, 60), c = rotation(70, -80, 0);
        double ea = err(s2::mul3(s2::mul3(a, b), c), s2::mul3(a, s2::mul3(b, c)));
        s2::View I = s2::make_view(0, 0);
        check(ea < 1e-15 && err(s2::mul3(a, I), a) == 0 && err(s2::mul3(I, a), a) == 0, "mul3 is the product (%g)", ea);
        // a turn about the screen's vertical axis, multiplied on the left, is the yaw of a view with no pitch
        check(err(s2::mul3(s2::roty(25), s2::make_view(10, 0)), s2::make_view(35, 0)) < 1e-15, "two yaws compose");
    }

    section("view_angles: make_view of the angles is the rotation again");
    double worst = 0, worst_roll0 = 0;
    for (int k = 0; k < 20000; k++) {
        s2::View M;
        if (k < 5000) M = rotation(rtest::uniform(-180, 180), rtest::uniform(-180, 180), rtest::uniform(-180, 180));
        else if (k < 10000) {                 // the window's turns about the screen axes
            M = s2::make_view(0, 0);
            for (int j = 0; j < 30; j++) {
                double d = 6*rtest::rnd(-1, 1);
                M = s2::mul3(j % 2 ? s2::rotx(d) : s2::roty(d), M);
            }
        } else if (k < 15000) M = rotation((k % 2 ? 90 : -90) + rtest::uniform(-1e-7, 1e-7), rtest::uniform(-180, 180),
                                           rtest::uniform(-180, 180));
        else {                                 // pure yaw and pitch: roll 0
            M = rotation(rtest::uniform(-89, 89), rtest::uniform(-180, 180), 0);
            double y, p, r;
            s2::view_angles(M, y, p, r);
            worst_roll0 = std::max(worst_roll0, std::fabs(r));
        }
        double y, p, r;
        s2::view_angles(M, y, p, r);
        worst = std::max(worst, err(s2::make_view(y, p, r), M));
    }
    check(worst < 1e-13, "the largest error of the rebuilt rotation: %g", worst);
    check(worst_roll0 < 1e-9, "a pure yaw and pitch has roll 0 (largest |roll| %g)", worst_roll0);
    {   // a yaw of 90 - delta degrees and a pitch, made by the window's turns, near the gimbal lock
        double worst_lock = 0, worst_lock_err = 0, hmin = 1, hmax = 0;
        for (double delta : {1e-14, 1e-12, 1e-10, 1e-9, 1e-8, 1e-6, 1e-5})
            for (double pitch : {20.0, -65.0, 133.0, 0.5, -179.0}) {
                s2::View M = s2::mul3(s2::roty(45), s2::mul3(s2::roty(45 - delta), s2::rotx(pitch)));
                const double h = std::hypot(M.m[7], M.m[8]);
                hmin = std::min(hmin, h);
                hmax = std::max(hmax, h);
                double y, p, r;
                s2::view_angles(M, y, p, r);
                worst_lock = std::max(worst_lock, std::fabs(r));
                worst_lock_err = std::max(worst_lock_err, err(s2::make_view(y, p, r), M));
            }
        check(worst_lock < 1e-12 && worst_lock_err < 1e-13,
              "near the gimbal lock (h from %.1e to %.1e), made by turns: roll 0 (largest |roll| %g degree), rebuilt to %g",
              hmin, hmax, worst_lock, worst_lock_err);
    }

    section("fmt_num");
    check(s2::fmt_num(0.1, 0) == "0.1" && s2::fmt_num(2.5534, 0) == "2.5534" && s2::fmt_num(30.000000000000004, 1e-12) == "30" &&
          s2::fmt_num(1e-20, 1e-12) == "0" && s2::fmt_num(-12.5, 0) == "-12.5" && s2::fmt_num(1234567, 0) == "1234567",
          "examples: %s %s %s %s", s2::fmt_num(0.1, 0).c_str(), s2::fmt_num(30.000000000000004, 1e-12).c_str(),
          s2::fmt_num(1e-20, 1e-12).c_str(), s2::fmt_num(1234567, 0).c_str());
    size_t fbad = 0, expo = 0;
    for (int k = 0; k < 20000; k++) {
        double x = rtest::uniform(-1, 1)*std::pow(10.0, (double)rtest::rnd(-6, 8)), tol = k % 2 ? 0 : 1e-12;
        std::string s = s2::fmt_num(x, tol);
        fbad += !(std::fabs(strtod(s.c_str(), nullptr) - x) <= tol);
        expo += std::fabs(x) >= 1e-4 && std::fabs(x) < 1e7 && s.find('e') != std::string::npos;
    }
    check(fbad == 0 && expo == 0, "within the tolerance (%zu not), no exponent in [1e-4, 1e7) (%zu with one)", fbad, expo);

    section("command_angles");
    {
        std::string sy, sp, sr;
        double ec;
        const double px = 1.0/(1.0*(1024/2.0 - 2.0));
        bool ok = s2::command_angles(rotation(30, -20, 10), px, sy, sp, sr, ec);
        check(ok && sy == "30" && sp == "-20" && sr == "10", "make_view(30, -20, 10): %s %s %s", sy.c_str(), sp.c_str(),
              sr.c_str());
        s2::View D = rotation(30, -20, 10);
        D.m[4] += 1e-3;
        check(!s2::command_angles(D, px, sy, sp, sr, ec) && ec > 1e-4, "a rotation off by 1e-3: no command (error %g)", ec);
        s2::View M = s2::make_view(0, 0);
        for (int j = 0; j < 1000; j++) M = s2::mul3(j % 3 ? s2::roty(6*rtest::rnd(-1, 1)) : s2::rotx(6), M);
        ok = s2::command_angles(M, px, sy, sp, sr, ec);
        s2::View W = s2::make_view(strtod(sy.c_str(), nullptr), strtod(sp.c_str(), nullptr), strtod(sr.c_str(), nullptr));
        check(ok && err(W, M) <= 1e-3*px, "after 1000 turns: %s %s %s, within %g of a pixel", sy.c_str(), sp.c_str(),
              sr.c_str(), err(W, M)/px);
    }

    section("export_framing");
    {
        int size;
        double zoom;
        s2::export_framing(512, 1, 16384, 1e7, size, zoom);
        check(size == 1024 && zoom == 1.0*1024/((1024 - 4.0)*1.1), "512 at zoom 1: %d pixels, zoom %.17g", size, zoom);
        s2::export_framing(2000, 5, 16384, 1e7, size, zoom);
        check(size == 2000 && zoom == 5.0*2000/((2000 - 4.0)*1.1), "2000 at zoom 5: %d, %.17g", size, zoom);
        s2::export_framing(20000, 1, 16384, 1e7, size, zoom);
        check(size == 16384 && zoom == 1.0*20000/((20000 - 4.0)*1.1), "20000: %d pixels at most (the zoom of 20000: %.17g)",
              size, zoom);
        s2::export_framing(1024, 2e7, 16384, 1e7, size, zoom);
        check(size == 1024 && zoom == 1e7, "the zoom at most 1e7: %.17g", zoom);
        s2::export_framing(1024, 1e7, 16384, 1e7, size, zoom);
        check(size == 1024 && zoom == 1e7*1024/((1024 - 4.0)*1.1), "the window's zoom 1e7 gives %.17g, below the limit", zoom);
    }

    section("export_name and export_stem: never overwriting");
    {
        rtest::TempDir t;
        const std::string a = t.file("pic"), b = t.file("x.y/pic.old");
        auto touch = [](const std::string& f) { rtest::write_file(f, "x"); };
        check(s2::export_name(a + ".png") == a + ".png" && s2::export_stem(a + ".png", {".png", ".zsession"}) == a,
              "nothing there: STEM.png, and the stem; an extension dropped");
        touch(a + ".zsession");
        check(s2::export_name(a) == a + ".png" && s2::export_stem(a, {".png", ".zsession"}) == a + "_1",
              "STEM.zsession there: export_name STEM.png, export_stem STEM_1");
        touch(a + ".png");
        touch(a + "_1.png");
        touch(a + "_2.zsession");
        check(s2::export_name(a) == a + "_2.png" && s2::export_stem(a, {".png", ".zsession"}) == a + "_3" &&
              s2::export_stem(a, {".png"}) + ".png" == s2::export_name(a),
              "STEM.png, STEM_1.png and STEM_2.zsession there: export_name STEM_2.png, export_stem STEM_3; with "
              "{.png} it is export_name");
        check(s2::export_stem(b, {".png"}) == t.file("x.y/pic"), "a dot in a folder's name is not an extension's");
    }
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
