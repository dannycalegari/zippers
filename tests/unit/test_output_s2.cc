// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_output_s2: the PNG encoder, the PNG reader and the rasteriser of src/output/ (png.h, raster.h) agree BYTE FOR BYTE
// with the S2 originals of legacy/s2/zipper.cc and of the film engine legacy/s2/movie/src/zmovie.cc:
//   png::chunk          png_chunk            (zipper.cc; the film engine uses it)
//   png::write_rows     write_png_raw        (zipper.cc: rows filtered by the caller; frees them)
//   png::write_rgb_sub  png_write            (zmovie.cc: the filter "Sub", written to fn.tmp and renamed)
//   png::read           png_read             (zmovie.cc: RGB, RGBA over white, grey, all five filters)
//   raster::line_index  ras_line             (zipper.cc: the palette line)
//   raster::downsample_palette_rows          (the box filter inside write_png of zipper.cc), through write_png itself
// zmovie.cc includes a snapshot of zipper.cc, so including zmovie.cc (with its main left out, as ztest.cc does by
// ZMOVIE_NO_MAIN) gives all of them in one program; the snapshot is made by legacy/s2/CMakeLists.txt in the build folder.
// (The S3 and S4 copies of png_chunk and of the lines are tested by test_output_s3 and test_output_s4.)
//
// What is compared, and on what: the files and the return values on random images (sizes from 1x1 to 1000x1000, six kinds,
// the zlib levels, failures: a level zlib refuses, an unwritable path, a rename onto a folder, /dev/null); the reader on
// valid files of the four kinds it accepts (all five filters, several IDAT chunks, other chunks, with and without IEND), on
// every prefix of a file, on files with one byte changed, and on files it refuses (and what it prints for them); the
// palette lines on random images and lines; and write_png of zipper.cc itself, on random scenes (polylines on S^2, views,
// zooms, sizes, up to 300 layers: the palette runs out at 254), against the same picture assembled from raster.h and png.h.
// Build it with the flags of the programs (-O2), since those are the flags under which the results must agree.

#define ZMOVIE_NO_MAIN
#include "zmovie.cc"

#include "output_test_support.h"

#include <deque>

using outtest::Bytes;
using outtest::Category;
using outtest::Random;
using outtest::Report;
using outtest::WorkDir;
namespace zp = outtest::zp;
namespace zr = outtest::zr;

// ---- the legacy functions, as callables ------------------------------------------------------------------------------
static void legacy_chunk(FILE* f, const char* type, const uint8_t* data, size_t len) { png_chunk(f, type, data, len); }
static bool legacy_write_raw(const char* fn, Bytes& raw, int W, int H) { return write_png_raw(fn, raw, W, H); }
static bool legacy_film_write(const std::string& fn, const Bytes& rgb, int W, int H, int level) { return png_write(fn, rgb, W, H, level); }
static bool legacy_film_read(const std::string& fn, Bytes& rgb, int& W, int& H) { return png_read(fn, rgb, W, H); }
static void legacy_index_line(Bytes& img, int W, int H, int x0, int y0, int x1, int y1, uint8_t col) { ras_line(img, W, H, x0, y0, x1, y1, col); }

struct Size { int W, H; };
static const std::vector<Size> kSizes = {{1,1},{1,2},{2,1},{1,7},{7,1},{2,2},{3,3},{5,7},{7,5},{8,8},{9,9},{16,16},{17,13},{31,33},
                                         {64,1},{1,64},{255,2},{256,2},{257,2},{2,255},{2,256},{2,257},{100,100},{333,77},{77,333},{640,480}};

// the rows of a PNG (filter byte, then the pixels of the row) for the RGB image rgb; with random_filters the filter bytes are random
static Bytes rows_of(Random& r, const Bytes& rgb, int W, int H, bool random_filters)
{
    const size_t rowlen = 1 + (size_t)W*3;
    Bytes raw((size_t)H*rowlen);
    for (int y = 0; y < H; ++y) {
        raw[(size_t)y*rowlen] = random_filters ? r.byte() : 0;
        memcpy(&raw[(size_t)y*rowlen + 1], &rgb[(size_t)y*W*3], (size_t)W*3);
    }
    return raw;
}

// ---- png::write_rows against write_png_raw ----------------------------------------------------------------------------------
static void test_write_rows(Report& rep, Random& r, const WorkDir& wd)
{
    Category& c = rep.cat("png::write_rows vs write_png_raw (S2): files, return values, and the freeing of raw");
    Category& rt = rep.cat("png::write_rows: png::read gives the image back (rows with filter byte 0)");
    Category& fail = rep.cat("png::write_rows vs write_png_raw (S2): failures");
    const std::string fa = wd.path("rows_legacy.png"), fb = wd.path("rows_new.png");
    auto compare = [&](Bytes raw_a, int W, int H, const char* what, const Bytes* original) {
        Bytes raw_b = raw_a;
        remove(fa.c_str()); remove(fb.c_str());
        bool ra = legacy_write_raw(fa.c_str(), raw_a, W, H), rb = zp::write_rows(fb.c_str(), raw_b, W, H);
        c.check(ra == rb && outtest::same_file(fa, fb) && raw_a.empty() && raw_b.empty() && raw_a.capacity() == 0 && raw_b.capacity() == 0,
                "%s %dx%d: returns %d, %d; raw left with %zu, %zu bytes (capacity %zu, %zu)", what, W, H, (int)ra, (int)rb,
                raw_a.size(), raw_b.size(), raw_a.capacity(), raw_b.capacity());
        if (original && rb) {
            Bytes back; int w2 = 0, h2 = 0;
            rt.check(zp::read(fb, back, w2, h2) && w2 == W && h2 == H && back == *original, "%s %dx%d", what, W, H);
        }
    };
    for (const Size& sz : kSizes)
        for (int kind = 0; kind < 6; kind++) {
            Bytes rgb = random_rgb(r, sz.W, sz.H, kind);
            compare(rows_of(r, rgb, sz.W, sz.H, false), sz.W, sz.H, "filter 0", &rgb);
            compare(rows_of(r, rgb, sz.W, sz.H, true), sz.W, sz.H, "random filter bytes", nullptr);
        }
    for (int t = 0; t < 100; t++) {
        int W = r.uniform(1, 300), H = r.uniform(1, 300);
        Bytes rgb = random_rgb(r, W, H, r.uniform(0, 5));
        compare(rows_of(r, rgb, W, H, false), W, H, "random", &rgb);
    }
    for (const Size& sz : {Size{1000, 3}, Size{3, 1000}, Size{999, 1}, Size{1, 999}, Size{1000, 1000}}) {
        Bytes rgb = random_rgb(r, sz.W, sz.H, 3);
        compare(rows_of(r, rgb, sz.W, sz.H, false), sz.W, sz.H, "large", &rgb);
    }
    {   // it writes what it is given: the header need not match the data
        for (int t = 0; t < 20; t++) {
            Bytes junk((size_t)r.uniform(0, 400));
            r.fill(junk);
            compare(junk, r.uniform(1, 30), r.uniform(1, 30), "raw not matching the size in the header", nullptr);
        }
    }
    {   // failures: a path that cannot be opened (raw is freed before the file is opened), /dev/null
        Bytes rgb = random_rgb(r, 20, 10, 3), raw_a = rows_of(r, rgb, 20, 10, false), raw_b = raw_a;
        const std::string na = wd.path("no_such_folder/a.png"), nb = wd.path("no_such_folder/b.png");
        bool ra = legacy_write_raw(na.c_str(), raw_a, 20, 10), rb = zp::write_rows(nb.c_str(), raw_b, 20, 10);
        fail.check(!ra && !rb && raw_a.empty() && raw_b.empty() && !outtest::exists(na) && !outtest::exists(nb),
                   "an unwritable path returns %d, %d; raw left with %zu, %zu bytes", (int)ra, (int)rb, raw_a.size(), raw_b.size());
        raw_a = rows_of(r, rgb, 20, 10, false); raw_b = raw_a;
        bool rc = legacy_write_raw("/dev/null", raw_a, 20, 10), rd = zp::write_rows("/dev/null", raw_b, 20, 10);
        fail.check(rc == rd && raw_a.empty() && raw_b.empty(), "/dev/null returns %d, %d", (int)rc, (int)rd);
    }
}

// ---- png::write_rgb_sub against png_write ----------------------------------------------------------------------------------
static void test_write_rgb_sub(Report& rep, Random& r, const WorkDir& wd)
{
    Category& c = rep.cat("png::write_rgb_sub vs png_write (film engine): files and return values");
    Category& tmp = rep.cat("png::write_rgb_sub: no fn.tmp is left after a success");
    Category& rt = rep.cat("png::write_rgb_sub: png::read gives the image back");
    Category& fail = rep.cat("png::write_rgb_sub vs png_write: failures (invalid level, unwritable path, rename onto a folder, overwriting)");
    const std::string fa = wd.path("sub_legacy.png"), fb = wd.path("sub_new.png");
    auto compare = [&](const Bytes& rgb, int W, int H, int level, const char* what) {
        remove(fa.c_str()); remove(fb.c_str());
        bool ra = legacy_film_write(fa, rgb, W, H, level), rb = zp::write_rgb_sub(fb, rgb, W, H, level);
        c.check(ra == rb && outtest::same_file(fa, fb), "%s %dx%d level %d (returns %d, %d)", what, W, H, level, (int)ra, (int)rb);
        tmp.check(!outtest::exists(fa + ".tmp") && !outtest::exists(fb + ".tmp"), "%s %dx%d level %d", what, W, H, level);
        if (rb) {
            Bytes back; int w2 = 0, h2 = 0;
            rt.check(zp::read(fb, back, w2, h2) && w2 == W && h2 == H && back == rgb, "%s %dx%d level %d", what, W, H, level);
        }
    };
    for (const Size& sz : kSizes)
        for (int kind = 0; kind < 6; kind++) {
            Bytes rgb = random_rgb(r, sz.W, sz.H, kind);
            for (int level : {2, 3, 6}) compare(rgb, sz.W, sz.H, level, "kind");                  // 3 is the film engine's default
            if (sz.W*sz.H <= 4096 || kind == 3)
                for (int level : {-1, 0, 1, 4, 5, 7, 8, 9}) compare(rgb, sz.W, sz.H, level, "kind");
        }
    for (int t = 0; t < 120; t++) {
        int W = r.uniform(1, 300), H = r.uniform(1, 300);
        compare(random_rgb(r, W, H, r.uniform(0, 5)), W, H, r.pick<int>({2, 3, 6}), "random");
    }
    for (const Size& sz : {Size{1000, 3}, Size{3, 1000}, Size{999, 1}, Size{1, 999}, Size{1000, 1000}})
        for (int kind : {2, 3, 4}) compare(random_rgb(r, sz.W, sz.H, kind), sz.W, sz.H, 3, "large");
    compare(random_rgb(r, 1000, 1000, 0), 1000, 1000, 3, "noise");                              // 3 MB that zlib cannot compress
    {
        Bytes rgb = random_rgb(r, 20, 10, 3);
        for (int level : {-2, 10, 99}) {                                                         // zlib refuses: false, and no file at all
            remove(fa.c_str()); remove(fb.c_str());
            bool ra = legacy_film_write(fa, rgb, 20, 10, level), rb = zp::write_rgb_sub(fb, rgb, 20, 10, level);
            fail.check(!ra && !rb && !outtest::exists(fa) && !outtest::exists(fb) && !outtest::exists(fa + ".tmp") && !outtest::exists(fb + ".tmp"),
                       "level %d returns %d, %d", level, (int)ra, (int)rb);
        }
        const std::string na = wd.path("no_such_folder/a.png"), nb = wd.path("no_such_folder/b.png");
        bool ra = legacy_film_write(na, rgb, 20, 10, 3), rb = zp::write_rgb_sub(nb, rgb, 20, 10, 3);
        fail.check(!ra && !rb, "an unwritable path returns %d, %d", (int)ra, (int)rb);
        // the rename fails when the name is a folder: false, and the .tmp file stays (the same one)
        const std::string da = wd.path("is_a_folder_legacy"), db = wd.path("is_a_folder_new");
        mkdir(da.c_str(), 0700); mkdir(db.c_str(), 0700);
        ra = legacy_film_write(da, rgb, 20, 10, 3); rb = zp::write_rgb_sub(db, rgb, 20, 10, 3);
        fail.check(!ra && !rb && outtest::exists(da + ".tmp") && outtest::exists(db + ".tmp") && outtest::same_file(da + ".tmp", db + ".tmp"),
                   "rename onto a folder returns %d, %d; .tmp files: %d, %d", (int)ra, (int)rb, (int)outtest::exists(da + ".tmp"), (int)outtest::exists(db + ".tmp"));
        unlink((da + ".tmp").c_str()); unlink((db + ".tmp").c_str()); rmdir(da.c_str()); rmdir(db.c_str());
        // overwriting a file that is there
        Bytes old(100); r.fill(old);
        outtest::spit(fa, old); outtest::spit(fb, old);
        ra = legacy_film_write(fa, rgb, 20, 10, 3); rb = zp::write_rgb_sub(fb, rgb, 20, 10, 3);
        fail.check(ra && rb && outtest::same_file(fa, fb) && !outtest::exists(fa + ".tmp") && !outtest::exists(fb + ".tmp"), "overwriting returns %d, %d", (int)ra, (int)rb);
    }
}

// ---- a file that cannot grow (the size limit makes fclose fail): what write_png_raw and png_write return, and what they leave
static void test_full_disk(Report& rep, Random& r, const WorkDir& wd)
{
    Category& rows = rep.cat("png::write_rows vs write_png_raw (S2): a file that cannot grow (return value, partial file, raw freed)");
    Category& sub = rep.cat("png::write_rgb_sub vs png_write: a file that cannot grow (return value, the .tmp file left, no rename)");
    Category& teeth = rep.cat("(the test itself) the size limit did make the writers fail");
    const std::string fa = wd.path("full_legacy.png"), fb = wd.path("full_new.png");
    long failed_rows = 0, failed_sub = 0;
    for (int t = 0; t < 10; t++) {
        const int W = r.uniform(60, 100), H = r.uniform(60, 100);
        Bytes rgb = random_rgb(r, W, H, 0);                              // noise: 10 to 30 kB, more than the limit
        const rlim_t limit = r.pick<rlim_t>({0, 1, 50, 100, 5000});
        {
            Bytes raw_a = rows_of(r, rgb, W, H, false), raw_b = raw_a;
            remove(fa.c_str()); remove(fb.c_str());
            bool ra, rb;
            {
                outtest::FileSizeLimit lim(limit);
                ra = legacy_write_raw(fa.c_str(), raw_a, W, H);
                rb = zp::write_rows(fb.c_str(), raw_b, W, H);
            }
            rows.check(ra == rb && outtest::same_file(fa, fb) && raw_a.empty() && raw_b.empty(), "limit %llu: returns %d, %d", (unsigned long long)limit, (int)ra, (int)rb);
            if (!ra) failed_rows++;
        }
        {
            remove(fa.c_str()); remove(fb.c_str()); remove((fa + ".tmp").c_str()); remove((fb + ".tmp").c_str());
            bool ra, rb;
            {
                outtest::FileSizeLimit lim(limit);
                ra = legacy_film_write(fa, rgb, W, H, 3);
                rb = zp::write_rgb_sub(fb, rgb, W, H, 3);
            }
            sub.check(ra == rb && !outtest::exists(fa) && !outtest::exists(fb) && outtest::same_file(fa + ".tmp", fb + ".tmp"),
                      "limit %llu: returns %d, %d", (unsigned long long)limit, (int)ra, (int)rb);
            if (!ra) failed_sub++;
            remove((fa + ".tmp").c_str()); remove((fb + ".tmp").c_str());
        }
    }
    teeth.check(failed_rows > 0 && failed_sub > 0, "the size limit made write_png_raw fail %ld times, png_write %ld times (of 10)", failed_rows, failed_sub);
}

// ---- png::read against png_read ---------------------------------------------------------------------------------------------
struct ReadResult { bool ok; Bytes rgb; int W, H; std::string err; };

template <class F>
static ReadResult run_reader(F reader, const WorkDir& wd, const std::string& file)
{
    ReadResult R{false, {1, 2, 3}, -7, -9, ""};                      // sentinels, to see what a failing call leaves alone
    outtest::StderrCapture cap(wd.path("stderr.txt"));
    R.ok = reader(file, R.rgb, R.W, R.H);
    R.err = cap.stop();
    return R;
}

static void test_read(Report& rep, Random& r, const WorkDir& wd)
{
    Category& valid = rep.cat("png::read vs png_read: valid files (RGB, RGBA, grey, grey+alpha; five filters; IDAT pieces; other chunks)");
    Category& prefix = rep.cat("png::read vs png_read: every prefix of a small file");
    Category& flips = rep.cat("png::read vs png_read: files with one byte changed (the size in the header left alone)");
    Category& refused = rep.cat("png::read vs png_read: files that are not plain valid PNGs, mostly refused (return value, arguments, stderr)");
    Category& writers = rep.cat("png::read vs png_read: the files the writers of this program make");
    Category& decoded = rep.cat("(the test itself) valid files that were accepted");
    const std::string f = wd.path("read_me.png");
    long accepted = 0, ncases = 0, prefix_ok = 0, prefix_n = 0, flips_ok = 0, flips_n = 0, refused_ok = 0, refused_n = 0;
    auto compare = [&](Category& c, const Bytes& file, const char* what) {
        outtest::spit(f, file);
        ReadResult a = run_reader(legacy_film_read, wd, f);
        ReadResult b = run_reader([](const std::string& fn, Bytes& rgb, int& W, int& H) { return zp::read(fn, rgb, W, H); }, wd, f);
        c.check(a.ok == b.ok && a.rgb == b.rgb && a.W == b.W && a.H == b.H && a.err == b.err,
                "%s: returns %d, %d; size %dx%d, %dx%d; stderr '%s', '%s'", what, (int)a.ok, (int)b.ok, a.W, a.H, b.W, b.H, a.err.c_str(), b.err.c_str());
        if (&c == &valid) { ncases++; if (a.ok) accepted++; }
        if (&c == &prefix) { prefix_n++; if (a.ok) prefix_ok++; }
        if (&c == &flips) { flips_n++; if (a.ok) flips_ok++; }
        if (&c == &refused) { refused_n++; if (a.ok) refused_ok++; }
        return a;
    };
    // valid files
    for (int ctype : {2, 6, 0, 4}) {
        const size_t bpp = (size_t)outtest::bytes_per_pixel(ctype);
        for (int t = 0; t < 120; t++) {
            outtest::PngSpec s;
            s.W = t < 6 ? 1 + t % 3 : r.uniform(1, 40); s.H = t < 6 ? 1 + t / 3 : r.uniform(1, 40);
            if (t == 119) { s.W = 700; s.H = 500; }
            s.ctype = ctype;
            std::vector<int> filters;
            if (t % 7 < 5) filters = {t % 7};                          // all rows with one filter, 0 to 4
            s.raw = outtest::random_filtered_rows(r, s.H, (size_t)s.W*bpp, filters);
            s.level = r.pick<int>({1, 6, 9});
            s.idat_pieces = r.pick<int>({1, 1, 2, 3, 5});
            s.extra_chunks = r.coin(0.4);
            s.iend = r.coin(0.9);
            if (r.coin(0.1)) { s.trailing.resize((size_t)r.uniform(1, 20)); r.fill(s.trailing); }
            ReadResult a = compare(valid, outtest::make_png(s), "valid file");
            (void)a;
        }
    }
    // the files of the other writers of this program, and a round trip
    for (int t = 0; t < 40; t++) {
        const int W = r.uniform(1, 80), H = r.uniform(1, 80);
        Bytes rgb = random_rgb(r, W, H, r.uniform(0, 5));
        const std::string g = wd.path("written.png");
        Bytes raw = rows_of(r, rgb, W, H, false);
        write_png_raw(g.c_str(), raw, W, H);
        Bytes file; outtest::slurp(g, file);
        ReadResult a = compare(writers, file, "write_png_raw file");
        writers.check(a.ok && a.rgb == rgb && a.W == W && a.H == H, "write_png_raw file: the reader does not return the image");
        png_write(g, rgb, W, H, 3);
        outtest::slurp(g, file);
        a = compare(writers, file, "png_write file");
        writers.check(a.ok && a.rgb == rgb, "png_write file: the reader does not return the image");
        zippers::png::write_rgb(g.c_str(), rgb, W, H, 6);
        outtest::slurp(g, file);
        a = compare(writers, file, "write_rgb file");
        writers.check(a.ok && a.rgb == rgb, "write_rgb file: the reader does not return the image");
    }
    // every prefix of a small RGBA file, and of a small grey file
    for (int ctype : {6, 0}) {
        outtest::PngSpec s;
        s.W = 3; s.H = 2; s.ctype = ctype; s.extra_chunks = true; s.idat_pieces = 2;
        s.raw = outtest::random_filtered_rows(r, s.H, (size_t)s.W*(size_t)outtest::bytes_per_pixel(ctype), {});
        Bytes file = outtest::make_png(s);
        for (size_t n = 0; n <= file.size(); n++) compare(prefix, Bytes(file.begin(), file.begin() + (long)n), "prefix");
    }
    // one byte changed (not in the width and height of the IHDR: a huge image would only exhaust the memory of both)
    {
        const size_t whbegin = 16, whend = 24;
        for (int ctype : {2, 6, 0, 4}) {
            outtest::PngSpec s;
            s.W = 5; s.H = 4; s.ctype = ctype; s.extra_chunks = true; s.idat_pieces = 2;
            s.raw = outtest::random_filtered_rows(r, s.H, (size_t)s.W*(size_t)outtest::bytes_per_pixel(ctype), {});
            const Bytes file = outtest::make_png(s);
            for (int t = 0; t < 400; t++) {
                Bytes g = file;
                size_t pos = (size_t)r.uniform(0, (int)g.size() - 1);
                if (pos >= whbegin && pos < whend) continue;
                g[pos] = r.coin() ? (uint8_t)(g[pos] ^ (1u << r.uniform(0, 7))) : r.byte();
                compare(flips, g, "one byte changed");
            }
        }
    }
    // files it refuses
    {
        auto spec = [&](int W, int H, int ctype) { outtest::PngSpec s; s.W = W; s.H = H; s.ctype = ctype;
                                                  s.raw = outtest::random_filtered_rows(r, H, (size_t)W*(size_t)outtest::bytes_per_pixel(ctype), {0}); return s; };
        outtest::PngSpec s;
        s = spec(4, 3, 2); s.depth = 16;                                  compare(refused, outtest::make_png(s), "16 bit");
        s = spec(4, 3, 3);                                                compare(refused, outtest::make_png(s), "palette (colour type 3)");
        s = spec(4, 3, 5);                                                compare(refused, outtest::make_png(s), "colour type 5");
        s = spec(4, 3, 2); s.inter = 1;                                   compare(refused, outtest::make_png(s), "interlaced");
        s = spec(4, 3, 2); s.good_signature = false;                      compare(refused, outtest::make_png(s), "bad signature");
        s = spec(4, 3, 2); s.raw.pop_back();                              compare(refused, outtest::make_png(s), "one byte too few");
        s = spec(4, 3, 2); s.raw.push_back(0);                            compare(refused, outtest::make_png(s), "one byte too many");
        s = spec(4, 3, 2); s.raw.clear();                                 compare(refused, outtest::make_png(s), "no data");
        s = spec(4, 3, 2); s.raw[0] = 5;                                  compare(refused, outtest::make_png(s), "filter 5");
        s = spec(4, 3, 2); s.raw[1 + 4*3 + 0] = 255;                      compare(refused, outtest::make_png(s), "filter 255 in row 2");
        s = spec(4, 3, 2); s.iend = false;                                compare(refused, outtest::make_png(s), "without IEND");
        { Bytes file = outtest::make_png(spec(4, 3, 2)); file[8] = 0x7f;  compare(refused, file, "a chunk longer than the file"); }
        compare(refused, Bytes(), "empty file");
        compare(refused, Bytes(32, 0), "32 zero bytes");
        { Bytes file = outtest::make_png(spec(4, 3, 2)); compare(refused, Bytes(file.begin(), file.begin() + 32), "32 bytes of a PNG"); }
        {   // no such file
            ReadResult a = run_reader(legacy_film_read, wd, wd.path("no_such_file.png"));
            ReadResult b = run_reader([](const std::string& fn, Bytes& rgb, int& W, int& H) { return zp::read(fn, rgb, W, H); }, wd, wd.path("no_such_file.png"));
            refused.check(!a.ok && !b.ok && a.rgb == b.rgb && a.W == b.W && a.H == b.H && a.err == b.err, "a file that does not exist");
        }
    }
    decoded.check(accepted > ncases*9/10 && prefix_ok > 0 && prefix_ok < prefix_n && flips_ok > 0 && flips_ok < flips_n && refused_ok < refused_n,
                  "accepted: %ld of %ld valid files, %ld of %ld prefixes, %ld of %ld files with a byte changed", accepted, ncases, prefix_ok, prefix_n, flips_ok, flips_n);
    printf("  [S2 png::read] accepted: %ld of %ld valid files; %ld of %ld prefixes; %ld of %ld files with a byte changed; %ld of %ld files made to be refused\n",
           accepted, ncases, prefix_ok, prefix_n, flips_ok, flips_n, refused_ok, refused_n);
}

// ---- raster::line_index against ras_line (S2) --------------------------------------------------------------------------------
static void test_lines_index(Report& rep, Random& r, long trials)
{
    Category& c = rep.cat("raster::line_index vs ras_line (S2)");
    Category& guard = rep.cat("raster::line_index: the bytes after the image are not touched");
    Category& touched = rep.cat("raster::line_index: the test lines do draw (an image changed)");
    long drew = 0;
    Bytes a, b, orig;
    for (long t = 0; t < trials; t++) {
        outtest::LineCase L = outtest::random_line_case(r);
        const size_t n = (size_t)L.W*L.H, canary = 8;
        orig.resize(n + canary);
        r.fill(orig);
        a = orig; b = orig;
        legacy_index_line(a, L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.c[0]);
        zr::line_index(b, L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.c[0]);
        c.check(a == b, "image %dx%d, (%d,%d)-(%d,%d)", L.W, L.H, L.x0, L.y0, L.x1, L.y1);
        guard.check(std::equal(orig.begin() + (long)n, orig.end(), b.begin() + (long)n), "image %dx%d", L.W, L.H);
        if (b != orig) drew++;
    }
    touched.check(drew > trials/4, "only %ld of %ld lines drew anything", drew, trials);
}

// ---- raster::downsample_palette_rows against the mean of each block, on noise and any factor ----------------------------------------
static void test_downsample_palette_formula(Report& rep, Random& r)
{
    Category& c = rep.cat("raster::downsample_palette_rows vs the mean of each block, on noise (factors 1 to 4)");
    for (int t = 0; t < 302; t++) {
        const int SS = t >= 300 ? 2 : r.uniform(1, 4), W = t == 300 ? 700 : t == 301 ? 301 : r.uniform(1, 30), H = t == 300 ? 650 : t == 301 ? 1000 : r.uniform(1, 30), w = W*SS;   // (two large ones)
        uint8_t pal[256][3];
        r.fill(&pal[0][0], sizeof pal);
        Bytes img((size_t)w*H*SS);
        if (t % 3 == 0) for (auto& b : img) b = (uint8_t)r.uniform(0, 3);                       // a few indices, as in the pictures
        else r.fill(img);
        const size_t rowlen = 1 + (size_t)W*3;
        Bytes ref((size_t)H*rowlen);
        for (int y = 0; y < H; y++) {
            ref[(size_t)y*rowlen] = 0;
            for (int x = 0; x < W; x++) for (int k = 0; k < 3; k++) {
                int sum = 0;
                for (int j = 0; j < SS; j++) for (int i = 0; i < SS; i++) sum += pal[img[(size_t)(y*SS + j)*w + (size_t)(x*SS + i)]][k];
                ref[(size_t)y*rowlen + 1 + (size_t)x*3 + (size_t)k] = (uint8_t)(sum/(SS*SS));
            }
        }
        c.check(zr::downsample_palette_rows(img, pal, w, W, H, SS) == ref, "%dx%d, factor %d", W, H, SS);
    }
}

// ---- write_png of zipper.cc, rebuilt from src/output/ ------------------------------------------------------------------------
// The part of write_png that belongs to the view (the circle of the sphere, the pixels, collect_polylines) is copied from
// it, as apps/s2/zipper.cc has it; everything else is zippers::raster and zippers::png.
static bool write_png_new(const char* fn, const Options& opt, const vector<Layer>& layers, const View& v, long* nlines = nullptr)
{
    const int SS = 2, W = opt.size, H = opt.size, w = W*SS, h = H*SS;
    vector<uint8_t> img((size_t)w*h, 0);                 // palette indices; white background
    uint8_t pal[256][3]; pal[0][0] = pal[0][1] = pal[0][2] = 255; pal[1][0] = pal[1][1] = pal[1][2] = 0;
    double cx = w/2.0, cy = h/2.0, rad = w/2.0 - 2.0*SS;
    auto toPx = [&](const P2& q, int& px, int& py){ px = (int)std::lround(cx + q.first*rad);
                                                     py = (int)std::lround(cy - q.second*rad); };
    double crad = rad*opt.zoom;
    if (crad <= std::sqrt(2.0)*(w/2.0) + 4.0) {          // (else it is wholly out of view)
        int nc = (int)std::min((double)(1 << 22), std::max(2048.0, crad*0.5));
        for (int i = 0; i < nc; ++i) {
            double a0 = 2*PI*i/nc, a1 = 2*PI*(i+1)/nc;
            double x0 = cx+crad*std::cos(a0), y0 = cy+crad*std::sin(a0);
            double x1 = cx+crad*std::cos(a1), y1 = cy+crad*std::sin(a1);
            if (std::max(x0,x1) < -2 || std::min(x0,x1) > w+2 || std::max(y0,y1) < -2 || std::min(y0,y1) > h+2)
                continue;
            zr::line_index(img, w, h, (int)std::lround(x0), (int)std::lround(y0),
                           (int)std::lround(x1), (int)std::lround(y1), 1);
        }
    }
    double tol = opt.accuracy * 0.5;
    double lim = 1.05 * (w/2.0) / rad;           // view half-width (+5%) in unit-disk units
    for (size_t li = 0; li < layers.size() && li < 254; ++li) {
        const Layer& L = layers[li];
        uint8_t ci = (uint8_t)(2 + li);
        for (int c = 0; c < 3; ++c) pal[ci][c] = (uint8_t)std::lround(L.col[c]*255);
        vector<vector<P2>> polys; collect_polylines(*L.r, v, tol, opt.zoom, lim, polys);
        for (auto& p : polys) {              // (clipped to |X|,|Y| <= lim: pixels stay in range)
            int px0, py0; toPx(p[0], px0, py0);
            for (size_t i = 1; i < p.size(); ++i) { int px1, py1; toPx(p[i], px1, py1);
                zr::line_index(img, w, h, px0, py0, px1, py1, ci); px0=px1; py0=py1; if (nlines) ++*nlines; }
        }
    }
    vector<uint8_t> raw = zr::downsample_palette_rows(img, pal, w, W, H, SS);
    vector<uint8_t>().swap(img);
    return zp::write_rows(fn, raw, W, H);
}

// ---- random scenes: polylines on S^2 (a Renderer that only holds points), a view, a size and a zoom ---------------------------------
struct PngScene {
    Rep rep;                         // empty: a Renderer holds a reference to it, and drawing never reads it
    std::deque<Renderer> rends;
    vector<Layer> layers;
    Options opt;
    View view;
};

static void random_scene(Random& r, int kind, PngScene& S)
{
    S.opt.size = r.pick<int>({8, 9, 16, 33, 64, 100, 150, 257, r.uniform(8, 120)});
    S.opt.zoom = r.pick<double>({0.5, 1, 1, 1, 1.3, 2, 5});
    S.opt.accuracy = r.pick<double>({0.004, 0.0005, 0.02});
    S.view = make_view(r.real(-180, 180), r.real(-90, 90), r.coin(0.2) ? r.real(-180, 180) : 0.0);
    const int nlayers = kind == 1 ? 300 : r.uniform(0, 4);                  // kind 1: more layers than the 254 colours of the palette
    for (int l = 0; l < nlayers; l++) {
        S.rends.emplace_back(S.rep);
        Renderer& R = S.rends.back();
        const int npoly = kind == 1 ? 1 : r.uniform(1, 5);
        for (int k = 0; k < npoly; k++) {
            R.pstart.push_back((uint32_t)R.px.size());
            const int npts = kind == 1 ? r.uniform(2, 8) : r.pick<int>({2, 2, 3, 10, 30, 60});
            double p[3];
            double s = 0;
            for (double& x : p) { x = r.gauss(); s += x*x; }
            for (double& x : p) x /= std::sqrt(s);
            for (int i = 0; i < npts; i++) {
                if (kind == 2) {                                            // independent points: long chords
                    s = 0;
                    for (double& x : p) { x = r.gauss(); s += x*x; }
                    for (double& x : p) x /= std::sqrt(s);
                } else {                                                    // a random walk on the sphere
                    s = 0;
                    for (double& x : p) { x += 0.2*r.gauss(); s += x*x; }
                    for (double& x : p) x /= std::sqrt(s);
                }
                R.px.push_back((float)p[0]); R.py.push_back((float)p[1]); R.pz.push_back((float)p[2]);
            }
        }
        S.layers.push_back(Layer{&R, {r.real(0, 1), r.real(0, 1), r.real(0, 1)}});
    }
}

static void test_write_png(Report& rep, Random& r, const WorkDir& wd)
{
    Category& c = rep.cat("write_png (S2) vs the same picture from raster.h + png.h: files and return values");
    Category& ink = rep.cat("(the test itself) scenes with lines, with a non-white pixel, with more than 254 layers");
    const std::string fa = wd.path("scene_legacy.png"), fb = wd.path("scene_new.png");
    long scenes = 0, with_lines = 0, with_ink = 0, many_layers = 0, total_lines = 0;
    const int kinds[] = {0, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 1};
    for (int t = 0; t < 300; t++) {
        const int kind = kinds[t % 12];
        PngScene S;
        random_scene(r, kind, S);
        remove(fa.c_str()); remove(fb.c_str());
        bool ra = write_png(fa.c_str(), S.opt, S.layers, S.view);
        long nlines = 0;
        bool rb = write_png_new(fb.c_str(), S.opt, S.layers, S.view, &nlines);
        c.check(ra == rb && outtest::same_file(fa, fb), "scene %d (kind %d): size %d, zoom %g, accuracy %g, %zu layers", t, kind, S.opt.size, S.opt.zoom, S.opt.accuracy, S.layers.size());
        scenes++;
        total_lines += nlines;
        if (nlines) with_lines++;
        if (S.layers.size() > 254) many_layers++;
        Bytes px; int pw = 0, ph = 0;
        if (zp::read(fa, px, pw, ph) && std::any_of(px.begin(), px.end(), [](uint8_t b) { return b != 255; })) with_ink++;
    }
    {   // a file that cannot be written
        PngScene S;
        random_scene(r, 0, S);
        const std::string na = wd.path("no_such_folder/a.png"), nb = wd.path("no_such_folder/b.png");
        bool ra = write_png(na.c_str(), S.opt, S.layers, S.view), rb = write_png_new(nb.c_str(), S.opt, S.layers, S.view);
        c.check(!ra && !rb, "an unwritable path returns %d, %d", (int)ra, (int)rb);
    }
    ink.check(with_lines > scenes/2 && with_ink > scenes*3/4 && many_layers > 0, "%ld of %ld scenes drew lines, %ld have ink, %ld have more than 254 layers", with_lines, scenes, with_ink, many_layers);
    printf("  [S2 write_png] %ld scenes, %ld polyline pieces drawn; %ld scenes drew lines, %ld have a non-white pixel, %ld have more than 254 layers\n",
           scenes, total_lines, with_lines, with_ink, many_layers);
}

int main()
{
    Report rep;
    WorkDir wd("test_output_s2");
    Random r(20261025ULL * 2 + 1);
    outtest::test_chunk(rep, r, wd, "S2", legacy_chunk);
    test_write_rows(rep, r, wd);
    test_write_rgb_sub(rep, r, wd);
    test_full_disk(rep, r, wd);
    test_read(rep, r, wd);
    test_lines_index(rep, r, 150000);
    test_downsample_palette_formula(rep, r);
    test_write_png(rep, r, wd);
    return rep.finish("test_output_s2");
}
