// SPDX-License-Identifier: GPL-3.0-or-later
//
// output_test_support.h: what the three unit tests of src/output/ (test_output_s3.cc, test_output_s4.cc,
// test_output_s2.cc) share: random numbers, a work folder, a tally of comparisons, random images and PNG files.
//
// It knows nothing of the legacy code. Each test program includes its own legacy sources (the legacy headers cannot
// be included together: they define the same names), and passes its legacy functions to the templates here.

#pragma once

#include "output/png.h"
#include "output/raster.h"

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

namespace outtest {

typedef std::vector<uint8_t> Bytes;

// ---- random numbers (the sequence depends on the C++ library, but is the same in every run)
class Random {
public:
    explicit Random(uint64_t seed) : g(seed) {}
    int uniform(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(g); }        // lo..hi inclusive
    double real(double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(g); }
    double gauss() { return std::normal_distribution<double>(0, 1)(g); }
    bool coin(double p = 0.5) { return real(0, 1) < p; }
    uint8_t byte() { return (uint8_t)uniform(0, 255); }
    template <class T> T pick(std::initializer_list<T> v) { return *(v.begin() + uniform(0, (int)v.size() - 1)); }
    // random bytes, many at a time (about 8 bytes per call of the generator)
    void fill(uint8_t* p, size_t n)
    {
        for (; n >= 8; n -= 8, p += 8) { uint64_t w = g(); memcpy(p, &w, 8); }
        if (n) { uint64_t w = g(); memcpy(p, &w, n); }
    }
    void fill(std::vector<uint8_t>& v) { if (!v.empty()) fill(v.data(), v.size()); }
private:
    std::mt19937_64 g;
};

// ---- a tally of comparisons, one per category; a difference is reported (the first few) and counted
class Category {
public:
    explicit Category(std::string n) : name(std::move(n)) {}
    // good: did the two results agree? The message (printf style) is formatted only if they did not.
    __attribute__((format(printf, 3, 4)))                    // (3 and 4: the member function's `this` is the first argument)
    void check(bool good, const char* fmt, ...)
    {
        tested++;
        if (good) return;
        if (bad++ < 5) {
            char buf[512];
            va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
            printf("DIFFERENT in '%s': %s\n", name.c_str(), buf);
        }
    }
    // Is this a comparison with a legacy function? (The others are round trips, comparisons with a formula, checks that no
    // .tmp file is left or that nothing is written past the image, and checks of the test itself that its inputs do
    // exercise what they should.)
    bool with_legacy() const
    {
        return name.find("(the test itself)") != 0 && name.find("gives the image back") == std::string::npos &&
               name.find("well-known") == std::string::npos && name.find("is left after") == std::string::npos &&
               name.find("bytes after the image") == std::string::npos && name.find("the test lines do draw") == std::string::npos &&
               name.find("the mean of each block") == std::string::npos;
    }
    std::string name;
    long tested = 0, bad = 0;
};

class Report {
public:
    Category& cat(const std::string& name)
    {
        for (auto& c : cats) if (c.name == name) return c;
        cats.emplace_back(name);
        return cats.back();
    }
    // prints the table; the exit status: 0 if every category compared something and nothing differed
    int finish(const char* program)
    {
        long legacy = 0, other = 0, bad = 0;
        for (const auto& c : cats) {
            printf("  %-62s %9ld %s, %ld differ\n", c.name.c_str(), c.tested, c.with_legacy() ? "comparisons" : "checks", c.bad);
            (c.with_legacy() ? legacy : other) += c.tested;
            bad += c.bad;
            if (c.tested == 0) { printf("  (nothing was compared in '%s')\n", c.name.c_str()); bad++; }
        }
        printf("%s: %ld comparisons with the legacy functions and %ld other checks, %ld differ or failed\n", program, legacy, other, bad);
        return bad ? 1 : 0;
    }
private:
    std::deque<Category> cats;       // (a deque: the references returned by cat() stay valid)
};

// ---- a work folder, removed (with what is in it) at the end. It is made in the temporary folder ($TMPDIR, else /tmp), not in
// the build folder, which may be in a folder that a file-sync service keeps in sync: the tests make and remove tens of thousands of small files.
// If that cannot be done it is made in the current folder.
class WorkDir {
public:
    explicit WorkDir(const std::string& stem)
    {
        const char* tmp = getenv("TMPDIR");
        for (std::string base : {std::string(tmp && *tmp ? tmp : "/tmp"), std::string(".")}) {
            while (base.size() > 1 && base.back() == '/') base.pop_back();
            std::string t = base + "/zippers_" + stem + ".XXXXXX";
            std::vector<char> buf(t.begin(), t.end());
            buf.push_back(0);
            if (mkdtemp(buf.data())) { dir = buf.data(); return; }
        }
        perror("mkdtemp");
        exit(2);
    }
    ~WorkDir() { clean(); rmdir(dir.c_str()); }
    WorkDir(const WorkDir&) = delete;
    WorkDir& operator=(const WorkDir&) = delete;
    std::string path(const std::string& name) const { return dir + "/" + name; }
    void clean() const                                    // delete the files in it (a subfolder the test made is its own business)
    {
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n != "." && n != "..") unlink(path(n).c_str());
        }
        closedir(d);
    }
private:
    std::string dir;
};

inline bool exists(const std::string& path) { struct stat st; return stat(path.c_str(), &st) == 0; }

inline bool slurp(const std::string& path, Bytes& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

inline bool spit(const std::string& path, const Bytes& data)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
    return fclose(f) == 0 && ok;
}

// the same file contents (or both files missing)
inline bool same_file(const std::string& a, const std::string& b)
{
    Bytes x, y;
    bool ha = slurp(a, x), hb = slurp(b, y);
    return ha == hb && x == y;
}

// ---- what the standard error stream receives while it is captured
class StderrCapture {
public:
    explicit StderrCapture(const std::string& tmpfile) : file(tmpfile)
    {
        fflush(stderr);
        saved = dup(2);
        int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        dup2(fd, 2);
        close(fd);
    }
    std::string stop()
    {
        fflush(stderr);
        dup2(saved, 2);
        close(saved);
        Bytes b;
        slurp(file, b);
        unlink(file.c_str());
        return std::string(b.begin(), b.end());
    }
private:
    std::string file;
    int saved;
};

// ---- while it exists, no file can grow beyond `bytes`: a write past that fails with EFBIG (as on a full disk) instead of
// killing the process. This is how the tests make fclose() fail, to compare what the writers return then.
class FileSizeLimit {
public:
    explicit FileSizeLimit(rlim_t bytes)
    {
        getrlimit(RLIMIT_FSIZE, &saved);
        old_handler = signal(SIGXFSZ, SIG_IGN);
        rlimit lim = saved;
        lim.rlim_cur = bytes;
        setrlimit(RLIMIT_FSIZE, &lim);
    }
    ~FileSizeLimit() { setrlimit(RLIMIT_FSIZE, &saved); signal(SIGXFSZ, old_handler); }
    FileSizeLimit(const FileSizeLimit&) = delete;
    FileSizeLimit& operator=(const FileSizeLimit&) = delete;
private:
    rlimit saved;
    void (*old_handler)(int);
};

inline long file_size(const std::string& path) { struct stat st; return stat(path.c_str(), &st) == 0 ? (long)st.st_size : -1; }

// ---- random RGB images; kind: 0 noise (does not compress), 1 one colour, 2 a smooth gradient, 3 white with random
// lines (like the pictures), 4 a few colours in runs, 5 stripes
inline Bytes random_rgb(Random& r, int W, int H, int kind)
{
    Bytes v((size_t)W*H*3);
    switch (kind) {
    case 0:
        r.fill(v);
        break;
    case 1: {
        uint8_t c[3] = {r.byte(), r.byte(), r.byte()};
        for (size_t i = 0; i < v.size(); i++) v[i] = c[i % 3];
        break; }
    case 2: {
        int a = r.uniform(1, 5), b = r.uniform(1, 5), c0 = r.byte();
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) for (int k = 0; k < 3; k++)
            v[((size_t)y*W + x)*3 + k] = (uint8_t)(a*x + b*y + 40*k + c0);
        break; }
    case 3: {
        std::fill(v.begin(), v.end(), 255);
        int n = r.uniform(1, 30);
        for (int i = 0; i < n; i++) {
            uint8_t c[3] = {r.byte(), r.byte(), r.byte()};
            zippers::raster::line_rgb(v, W, H, r.uniform(-W, 2*W), r.uniform(-H, 2*H), r.uniform(-W, 2*W), r.uniform(-H, 2*H), c, r.uniform(0, 2));
        }
        break; }
    case 4: {
        uint8_t pal[4][3];
        for (auto& p : pal) for (auto& c : p) c = r.byte();
        size_t i = 0;
        while (i < (size_t)W*H) {
            int k = r.uniform(0, 3), run = r.uniform(1, 40);
            for (int j = 0; j < run && i < (size_t)W*H; j++, i++) for (int c = 0; c < 3; c++) v[i*3 + c] = pal[k][c];
        }
        break; }
    default: {
        int period = r.uniform(1, 9);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) for (int k = 0; k < 3; k++)
            v[((size_t)y*W + x)*3 + k] = ((x + y/2) / period) % 2 ? (uint8_t)(255 - 60*k) : (uint8_t)(10 + 70*k);
        break; }
    }
    return v;
}

// ---- PNG files built by the test itself (with zlib, independently of src/output/), for the tests of the reader
struct PngSpec {
    int W = 1, H = 1;                 // what the IHDR says
    int depth = 8, ctype = 2, inter = 0;
    Bytes raw;                        // what is compressed into the IDAT chunks: normally H rows of (filter byte, W*bpp bytes)
    int level = 6;
    int idat_pieces = 1;              // the compressed data is split into this many IDAT chunks
    bool extra_chunks = false;        // ancillary chunks before, between and after the IDAT chunks
    bool iend = true;                 // an IEND chunk at the end
    bool good_signature = true;
    Bytes trailing;                   // bytes after the last chunk
};

inline void put32(Bytes& o, uint32_t v) { o.push_back((uint8_t)(v >> 24)); o.push_back((uint8_t)(v >> 16)); o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)v); }

inline void put_chunk(Bytes& o, const char* type, const uint8_t* data, size_t len)
{
    put32(o, (uint32_t)len);
    o.insert(o.end(), type, type + 4);
    if (len) o.insert(o.end(), data, data + len);
    uLong crc = crc32(0, (const Bytef*)type, 4);
    if (len) crc = crc32(crc, (const Bytef*)data, (uInt)len);
    put32(o, (uint32_t)crc);
}

inline int bytes_per_pixel(int ctype) { return ctype == 2 ? 3 : ctype == 6 ? 4 : ctype == 0 ? 1 : ctype == 4 ? 2 : 3; }

inline Bytes make_png(const PngSpec& s)
{
    Bytes o = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!s.good_signature) o[1] = 'X';
    Bytes ihdr;
    put32(ihdr, (uint32_t)s.W); put32(ihdr, (uint32_t)s.H);
    ihdr.push_back((uint8_t)s.depth); ihdr.push_back((uint8_t)s.ctype); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back((uint8_t)s.inter);
    put_chunk(o, "IHDR", ihdr.data(), ihdr.size());
    const uint8_t text[] = "Comment\0made by the test";
    if (s.extra_chunks) put_chunk(o, "tEXt", text, sizeof text - 1);
    uLongf clen = compressBound((uLong)s.raw.size());
    Bytes comp(clen);
    compress2(comp.data(), &clen, s.raw.data(), (uLong)s.raw.size(), s.level);
    comp.resize(clen);
    size_t pieces = (size_t)std::max(1, s.idat_pieces), per = (comp.size() + pieces - 1) / pieces;
    for (size_t i = 0; i < pieces; i++) {
        size_t a = std::min(comp.size(), i*per), b = std::min(comp.size(), (i + 1)*per);
        put_chunk(o, "IDAT", comp.data() + a, b - a);
        if (s.extra_chunks && i + 1 < pieces) put_chunk(o, "pHYs", (const uint8_t*)"\0\0\x0b\x13\0\0\x0b\x13\x01", 9);
    }
    if (s.extra_chunks) put_chunk(o, "tIME", (const uint8_t*)"\x07\xea\x0a\x19\x14\x15\x16", 7);
    if (s.iend) put_chunk(o, "IEND", nullptr, 0);
    o.insert(o.end(), s.trailing.begin(), s.trailing.end());
    return o;
}

// H rows of (filter byte, stride random bytes), the filter byte from `filters` (cycled) or random 0..4 if empty
inline Bytes random_filtered_rows(Random& r, int H, size_t stride, const std::vector<int>& filters)
{
    Bytes raw((size_t)H*(stride + 1));
    for (int y = 0; y < H; y++) {
        uint8_t* row = &raw[(size_t)y*(stride + 1)];
        r.fill(row + 1, stride);
        row[0] = (uint8_t)(filters.empty() ? r.uniform(0, 4) : filters[(size_t)y % filters.size()]);
    }
    return raw;
}

// ---- a random line of a w x h image (any of its pixels, its border, and far outside), for the tests of the lines
struct LineCase { int W, H, x0, y0, x1, y1, th; uint8_t c[3]; };

inline int line_coord(Random& r, int n)
{
    switch (r.uniform(0, 19)) {
        case 0: case 1: case 2: return r.pick<int>({0, n - 1, -1, n});          // on or next to the border
        case 3: case 4: return r.uniform(-3, n + 2);                            // near it
        case 5: case 6: return r.uniform(-3*n - 5, 4*n + 5);                    // outside
        case 7: return r.uniform(-700, 700);                                    // far outside (a long line)
        default: return r.uniform(0, n - 1);                                    // inside
    }
}

inline LineCase random_line_case(Random& r)
{
    LineCase L;
    switch (r.uniform(0, 9)) {
        case 0: L.W = L.H = 1; break;
        case 1: L.W = 1; L.H = r.uniform(1, 40); break;
        case 2: L.W = r.uniform(1, 40); L.H = 1; break;
        default: L.W = r.uniform(1, 48); L.H = r.uniform(1, 48); break;
    }
    L.x0 = line_coord(r, L.W); L.y0 = line_coord(r, L.H);
    L.x1 = line_coord(r, L.W); L.y1 = line_coord(r, L.H);
    switch (r.uniform(0, 8)) {                                                    // the shape
        case 0: L.x1 = L.x0; L.y1 = L.y0; break;                                  // length zero
        case 1: L.y1 = L.y0; break;                                               // horizontal
        case 2: L.x1 = L.x0; break;                                               // vertical
        case 3: { int d = L.x1 - L.x0; L.y1 = L.y0 + (r.coin() ? d : -d); break; }   // diagonal
        case 4: { int dy = L.y1 - L.y0; L.x1 = L.x0 + dy/3; break; }              // steep
        case 5: { int dx = L.x1 - L.x0; L.y1 = L.y0 + dx/3; break; }              // shallow
        default: break;                                                           // as chosen
    }
    L.th = r.pick<int>({0, 0, 1, 2, 3});
    for (auto& c : L.c) c = r.byte();
    return L;
}

// =====================================================================================================================
// Tests shared by the programs. Each takes the legacy functions of its program as callables.
// =====================================================================================================================

namespace zp = zippers::png;
namespace zr = zippers::raster;

// ---- png::chunk against png_chunk: the same bytes in the file ---------------------------------------------------------
template <class LegacyChunk>
void test_chunk(Report& rep, Random& r, const WorkDir& wd, const char* label, LegacyChunk legacy)
{
    Category& c = rep.cat(std::string("png::chunk vs png_chunk (") + label + ")");
    Category& sane = rep.cat(std::string("png::chunk: the IEND chunk is the well-known 12 bytes (") + label + ")");
    const std::string fa = wd.path("chunk_legacy.bin"), fb = wd.path("chunk_new.bin");
    auto both = [&](const char* type, const Bytes& data, size_t len) {
        FILE* f = fopen(fa.c_str(), "wb");
        legacy(f, type, len ? data.data() : nullptr, len);
        fclose(f);
        f = fopen(fb.c_str(), "wb");
        zp::chunk(f, type, len ? data.data() : nullptr, len);
        fclose(f);
        return same_file(fa, fb);
    };
    {   // IEND: length 0, type, CRC AE 42 60 82 (the same 12 bytes in every PNG file)
        both("IEND", Bytes(), 0);
        const Bytes iend = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};
        Bytes x, y;
        slurp(fa, x); slurp(fb, y);
        sane.check(x == iend && y == iend, "IEND chunk differs from the known bytes");
    }
    const std::vector<size_t> lens = {0, 1, 2, 3, 4, 13, 255, 256, 257, 4095, 65535, 65536, 65537, 100000};
    for (int t = 0; t < 600; t++) {
        char type[4];
        for (char& ch : type) ch = (char)r.byte();                       // any four bytes
        size_t len = t < (int)lens.size() ? lens[(size_t)t] : (size_t)r.uniform(0, 3000);
        Bytes data(len);
        r.fill(data);
        c.check(both(type, data, len), "len=%zu type=%02x%02x%02x%02x", len, (uint8_t)type[0], (uint8_t)type[1], (uint8_t)type[2], (uint8_t)type[3]);
    }
    {   // a length whose fourth byte is not 0 (2^24 + 1): 16 MB of data
        size_t len = ((size_t)1 << 24) + 1;
        Bytes data(len);
        r.fill(data);
        c.check(both("IDAT", data, len), "len=%zu", len);
    }
}

// ---- png::write_rgb against write_png_rgb: the same files ------------------------------------------------------------
// legacy(fn, rgb, W, H, level) and legacy_default(fn, rgb, W, H) (the call that leaves out the level)
template <class LegacyWrite, class LegacyWriteDefault>
void test_write_rgb(Report& rep, Random& r, const WorkDir& wd, const char* label, LegacyWrite legacy, LegacyWriteDefault legacy_default)
{
    Category& c = rep.cat(std::string("png::write_rgb vs write_png_rgb (") + label + "): files");
    Category& rt = rep.cat(std::string("png::write_rgb: png::read gives the image back (") + label + ")");
    Category& fail = rep.cat(std::string("png::write_rgb vs write_png_rgb (") + label + "): failures (return value, no file)");
    const std::string fa = wd.path("rgb_legacy.png"), fb = wd.path("rgb_new.png");
    auto compare = [&](const Bytes& rgb, int W, int H, int level, const char* what) {
        remove(fa.c_str()); remove(fb.c_str());
        bool ra = legacy(fa.c_str(), rgb, W, H, level), rb = zp::write_rgb(fb.c_str(), rgb, W, H, level);
        c.check(ra == rb && same_file(fa, fb), "%s %dx%d level %d (returns %d, %d)", what, W, H, level, (int)ra, (int)rb);
        if (rb && level >= -1 && level <= 9) {                       // the file is a valid PNG with the pixels of rgb
            Bytes back; int w2 = 0, h2 = 0;
            rt.check(zp::read(fb, back, w2, h2) && w2 == W && h2 == H && back == rgb, "%s %dx%d level %d", what, W, H, level);
        }
    };
    struct Size { int W, H; };
    const std::vector<Size> sizes = {{1,1},{1,2},{2,1},{1,7},{7,1},{2,2},{3,3},{5,7},{7,5},{8,8},{9,9},{16,16},{17,13},{31,33},
                                     {64,1},{1,64},{255,2},{256,2},{257,2},{2,255},{2,256},{2,257},{100,100},{333,77},{77,333},{640,480}};
    for (const Size& sz : sizes)
        for (int kind = 0; kind < 6; kind++) {
            Bytes rgb = random_rgb(r, sz.W, sz.H, kind);
            for (int level : {2, 6}) compare(rgb, sz.W, sz.H, level, "kind");
            if (sz.W*sz.H <= 4096 || kind == 3) {                     // every level, and the call without a level
                for (int level : {-1, 0, 1, 3, 4, 5, 7, 8, 9}) compare(rgb, sz.W, sz.H, level, "kind");
                remove(fa.c_str()); remove(fb.c_str());
                bool ra = legacy_default(fa.c_str(), rgb, sz.W, sz.H), rb = zp::write_rgb(fb.c_str(), rgb, sz.W, sz.H);
                c.check(ra == rb && same_file(fa, fb), "default level %dx%d (returns %d, %d)", sz.W, sz.H, (int)ra, (int)rb);
            }
        }
    for (int t = 0; t < 120; t++) {                                   // random sizes
        int W = r.uniform(1, 300), H = r.uniform(1, 300);
        Bytes rgb = random_rgb(r, W, H, r.uniform(0, 5));
        compare(rgb, W, H, r.pick<int>({2, 6}), "random");
    }
    for (const Size& sz : {Size{1000, 3}, Size{3, 1000}, Size{999, 1}, Size{1, 999}, Size{1000, 1000}}) {   // up to 1000 x 1000
        for (int kind : {0, 3}) {
            if (kind == 0 && sz.W*sz.H > 600000) continue;           // (1000 x 1000 noise only once, below)
            Bytes rgb = random_rgb(r, sz.W, sz.H, kind);
            for (int level : {2, 6}) compare(rgb, sz.W, sz.H, level, "large");
        }
    }
    {
        Bytes rgb = random_rgb(r, 1000, 1000, 0);                      // 1000 x 1000 noise: 3 MB that zlib cannot compress
        compare(rgb, 1000, 1000, 6, "noise");
    }
    {   // failures: a level zlib refuses (the legacy code returns false before it opens the file), a path that cannot be opened
        Bytes rgb = random_rgb(r, 20, 10, 3);
        for (int level : {-2, 10, 99}) {
            remove(fa.c_str()); remove(fb.c_str());
            bool ra = legacy(fa.c_str(), rgb, 20, 10, level), rb = zp::write_rgb(fb.c_str(), rgb, 20, 10, level);
            fail.check(!ra && !rb && !exists(fa) && !exists(fb), "level %d returns %d, %d; files exist: %d, %d", level, (int)ra, (int)rb, (int)exists(fa), (int)exists(fb));
        }
        const std::string na = wd.path("no_such_folder/legacy.png"), nb = wd.path("no_such_folder/new.png");
        bool ra = legacy(na.c_str(), rgb, 20, 10, 6), rb = zp::write_rgb(nb.c_str(), rgb, 20, 10, 6);
        fail.check(!ra && !rb && !exists(na) && !exists(nb), "an unwritable path returns %d, %d", (int)ra, (int)rb);
        bool rc = legacy("/dev/null", rgb, 20, 10, 6), rd = zp::write_rgb("/dev/null", rgb, 20, 10, 6);
        fail.check(rc == rd, "/dev/null returns %d, %d", (int)rc, (int)rd);
    }
}

// ---- png::write_rgb against write_png_rgb when the file cannot grow (fclose fails): the legacy function ignores that and
// returns true, and leaves the partial file; so must the new one
template <class LegacyWrite>
void test_write_rgb_full_disk(Report& rep, Random& r, const WorkDir& wd, const char* label, LegacyWrite legacy)
{
    Category& c = rep.cat(std::string("png::write_rgb vs write_png_rgb (") + label + "): a file that cannot grow (return value, partial file)");
    Category& teeth = rep.cat(std::string("(the test itself) the size limit did truncate files (") + label + ")");
    const std::string fa = wd.path("full_legacy.png"), fb = wd.path("full_new.png");
    long truncated = 0;
    for (int t = 0; t < 10; t++) {
        const int W = r.uniform(60, 100), H = r.uniform(60, 100);
        Bytes rgb = random_rgb(r, W, H, 0);                              // noise: 10 to 30 kB, more than the limit
        const rlim_t limit = r.pick<rlim_t>({0, 1, 50, 100, 5000});
        remove(fa.c_str()); remove(fb.c_str());
        bool ra, rb;
        {
            FileSizeLimit lim(limit);
            ra = legacy(fa.c_str(), rgb, W, H, 6);
            rb = zp::write_rgb(fb.c_str(), rgb, W, H, 6);
        }
        c.check(ra && rb && same_file(fa, fb), "limit %llu: returns %d, %d; file sizes %ld, %ld", (unsigned long long)limit, (int)ra, (int)rb, file_size(fa), file_size(fb));
        if (file_size(fa) >= 0 && (size_t)file_size(fa) < (size_t)W*H*3) truncated++;
    }
    teeth.check(truncated > 0, "no file was truncated by the size limit");
}

// ---- raster::line_rgb against ras_line (with a brush, and without) ---------------------------------------------------
// legacy9(img, W, H, x0, y0, x1, y1, c, th) and legacy8(img, W, H, x0, y0, x1, y1, c) (the call that leaves out th)
template <class LegacyLine9, class LegacyLine8>
void test_lines_rgb(Report& rep, Random& r, const char* label, long trials, LegacyLine9 legacy9, LegacyLine8 legacy8)
{
    Category& c = rep.cat(std::string("raster::line_rgb vs ras_line (") + label + ")");
    Category& guard = rep.cat(std::string("raster::line_rgb: the bytes after the image are not touched (") + label + ")");
    Category& touched = rep.cat(std::string("raster::line_rgb: the test lines do draw (an image changed) (") + label + ")");
    long drew = 0;
    Bytes a, b, orig;
    for (long t = 0; t < trials; t++) {
        LineCase L = random_line_case(r);
        const size_t n = (size_t)L.W*L.H*3, canary = 24;
        orig.resize(n + canary);
        r.fill(orig);                                                  // random contents: a pixel the line does not touch must stay
        a = orig; b = orig;
        if (L.th == 0 && r.coin()) {
            legacy8(a, L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.c);
            zr::line_rgb(b, L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.c);
        } else {
            legacy9(a, L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.c, L.th);
            zr::line_rgb(b, L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.c, L.th);
        }
        c.check(a == b, "image %dx%d, (%d,%d)-(%d,%d), brush %d", L.W, L.H, L.x0, L.y0, L.x1, L.y1, L.th);
        guard.check(std::equal(orig.begin() + (long)n, orig.end(), b.begin() + (long)n), "image %dx%d", L.W, L.H);
        if (b != orig) drew++;
    }
    touched.check(drew > trials/4, "only %ld of %ld lines drew anything", drew, trials);
    // larger images (the row length and the index arithmetic above 255 and 65535 pixels), a few lines each
    for (int t = 0; t < 40; t++) {
        const int W = r.uniform(200, 1200), H = r.uniform(200, 1000);
        orig.resize((size_t)W*H*3);
        r.fill(orig);
        a = orig; b = orig;
        for (int k = 0; k < 6; k++) {
            LineCase L = random_line_case(r);
            L.x0 = line_coord(r, W); L.y0 = line_coord(r, H); L.x1 = line_coord(r, W); L.y1 = line_coord(r, H);
            legacy9(a, W, H, L.x0, L.y0, L.x1, L.y1, L.c, L.th);
            zr::line_rgb(b, W, H, L.x0, L.y0, L.x1, L.y1, L.c, L.th);
        }
        c.check(a == b, "large image %dx%d", W, H);
    }
}

// ---- raster::cross against the two ras_line calls of write_png / write_png5 ---------------------------------------------
template <class LegacyLine9>
void test_cross(Report& rep, Random& r, const char* label, long trials, LegacyLine9 legacy9)
{
    Category& c = rep.cat(std::string("raster::cross vs the two ras_line calls of the marks (") + label + ")");
    Bytes a, b;
    for (long t = 0; t < trials; t++) {
        int SS = r.pick<int>({1, 2, 2, 3}), th = r.pick<int>({0, 1, 1, 2, 3});
        int W = r.uniform(1, 60), H = r.uniform(1, 60);
        int cx = line_coord(r, W), cy = line_coord(r, H);
        a.resize((size_t)W*H*3); r.fill(a); b = a;
        const uint8_t blk[3] = {0, 0, 0};
        legacy9(a, W, H, cx - 4*SS, cy, cx + 4*SS, cy, blk, th);       // the legacy statements of the marks
        legacy9(a, W, H, cx, cy - 4*SS, cx, cy + 4*SS, blk, th);
        zr::cross(b, W, H, cx, cy, SS, th);
        c.check(a == b, "image %dx%d, centre (%d,%d), SS %d, brush %d", W, H, cx, cy, SS, th);
    }
}

// ---- raster::paint_segments against the legacy painter, on lists with many equal depths and priorities ----------------
// The legacy painter is the three statements of write_png (zipcore.h:480-482) and write_png5 (zipper5.cc:82-84), copied
// here, on a struct of the same layout as Seg; the composite tests of each program compare the real functions.
struct LegacySeg { float x0, y0, x1, y1, z; uint8_t c[3]; int prio; };
// the same layout as raster::Segment (tests/unit/check_moved_text.py shows that its members are those of the legacy Seg)
static_assert(sizeof(zr::Segment) == sizeof(LegacySeg) && alignof(zr::Segment) == alignof(LegacySeg) &&
              offsetof(zr::Segment, x0) == offsetof(LegacySeg, x0) && offsetof(zr::Segment, y0) == offsetof(LegacySeg, y0) &&
              offsetof(zr::Segment, x1) == offsetof(LegacySeg, x1) && offsetof(zr::Segment, y1) == offsetof(LegacySeg, y1) &&
              offsetof(zr::Segment, z) == offsetof(LegacySeg, z) && offsetof(zr::Segment, c) == offsetof(LegacySeg, c) &&
              offsetof(zr::Segment, prio) == offsetof(LegacySeg, prio), "raster::Segment must have the layout of the legacy Seg");

template <class LegacyLine9>
void test_paint_segments(Report& rep, Random& r, const char* label, long trials, LegacyLine9 ras_line)
{
    Category& c = rep.cat(std::string("raster::paint_segments vs the legacy painter, random lists (") + label + ")");
    Category& order = rep.cat(std::string("raster::paint_segments leaves the list in the legacy order (") + label + ")");
    Category& teeth = rep.cat(std::string("(the test itself) lists on which std::stable_sort would draw another picture (") + label + ")");
    long stable_differs = 0, with_ties = 0;
    for (long t = 0; t < trials; t++) {
        const int w = r.uniform(1, 70), h = r.uniform(1, 70), th = r.pick<int>({0, 0, 1, 2});
        const size_t n = (size_t)r.pick<int>({0, 1, 2, 5, 29, 30, 31, 32, 33, 40, 100, 200, 400});
        const int pattern = r.uniform(0, 4);
        std::vector<LegacySeg> legacy(n);
        for (LegacySeg& s : legacy) {
            auto coord = [&](int size) { return r.coin(0.3) ? (float)r.uniform(-size/2, 3*size/2) + 0.5f * r.uniform(0, 1) : (float)r.real(-0.4*size, 1.4*size); };
            s.x0 = coord(w); s.y0 = coord(h); s.x1 = coord(w); s.y1 = coord(h);
            switch (pattern) {
                case 0: s.z = 0.25f; s.prio = 1; break;                                    // everything equal
                case 1: s.z = (float)r.uniform(-1, 1) * 0.5f; s.prio = r.uniform(0, 1); break;   // a few depths and priorities
                case 2: s.z = r.coin() ? 0.0f : -0.0f; s.prio = r.uniform(0, 2); break;    // equal depths: 0 and -0
                case 3: s.z = (float)r.real(-3, 3); s.prio = r.uniform(0, 2); break;       // (almost) no ties
                default: s.z = (float)r.uniform(0, 3); s.prio = r.pick<int>({-5, 1, 1, 1000000}); break;
            }
            r.fill(s.c, 3);
        }
        std::vector<zr::Segment> fresh(n);
        for (size_t i = 0; i < n; i++) {
            fresh[i] = {legacy[i].x0, legacy[i].y0, legacy[i].x1, legacy[i].y1, legacy[i].z, {legacy[i].c[0], legacy[i].c[1], legacy[i].c[2]}, legacy[i].prio};
        }
        std::vector<zr::Segment> for_stable = fresh;

        Bytes img((size_t)w*h*3);
        r.fill(img);
        Bytes ia = img, ib = img, ic = img;
        // the legacy statements
        std::sort(legacy.begin(), legacy.end(), [](const LegacySeg& a, const LegacySeg& b) { return a.prio != b.prio ? a.prio < b.prio : a.z < b.z; });
        for (const LegacySeg& s : legacy)
            ras_line(ia, w, h, (int)std::lround(s.x0), (int)std::lround(s.y0), (int)std::lround(s.x1), (int)std::lround(s.y1), s.c, th);
        zr::paint_segments(ib, w, h, fresh, th);
        c.check(ia == ib, "list of %zu, pattern %d, image %dx%d, brush %d", n, pattern, w, h, th);
        // the list must also keep its length (a version that emptied it would pass the comparison of the images)
        bool same_order = fresh.size() == n;
        for (size_t i = 0; same_order && i < n; i++)
            same_order = same_order && legacy[i].x0 == fresh[i].x0 && legacy[i].y0 == fresh[i].y0 && legacy[i].x1 == fresh[i].x1 &&
                         legacy[i].y1 == fresh[i].y1 && legacy[i].z == fresh[i].z && legacy[i].prio == fresh[i].prio &&
                         memcmp(legacy[i].c, fresh[i].c, 3) == 0;
        order.check(same_order, "list of %zu, pattern %d", n, pattern);
        // is this a list whose picture depends on how ties are ordered? (a stable sort in place of std::sort)
        std::stable_sort(for_stable.begin(), for_stable.end(), [](const zr::Segment& a, const zr::Segment& b) { return a.prio != b.prio ? a.prio < b.prio : a.z < b.z; });
        for (const zr::Segment& s : for_stable)
            zr::line_rgb(ic, w, h, (int)std::lround(s.x0), (int)std::lround(s.y0), (int)std::lround(s.x1), (int)std::lround(s.y1), s.c, th);
        if (n > 30 && pattern <= 2) with_ties++;
        if (ic != ib) stable_differs++;
    }
    teeth.check(stable_differs > 0, "no list was sensitive to the order of equal keys (%ld lists with ties)", with_ties);
    printf("  [%s] %ld of %ld lists drew another picture with std::stable_sort: the lists do exercise the order of equal keys\n", label, stable_differs, trials);
}

}  // namespace outtest
