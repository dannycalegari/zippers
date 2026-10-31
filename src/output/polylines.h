// SPDX-License-Identifier: GPL-3.0-or-later
//
// polylines.h: the polyline file, format 1: the curves of a picture as data, written by zipper, zipper3 and zipper5
// (--polylines FILE), and read back exactly. namespace zippers::polylines.
//
// THE FILE. A text header, then binary blocks.
//   THE HEADER: lines "KEY VALUE", the key the first word of the line and the value the rest of it after one blank (it
//   may be empty). Its first line is "zippers polylines 1" (the format and its version), its last line is "data". The
//   keys that the format itself reads:
//     sphere N          the points lie on the sphere S^N, N = 2, 3 or 4: each point is N + 1 floats (in R^(N+1))
//     layer NAME P M    a layer: its name (one word), its number of polylines P and its number of points M. The layers
//                       come in the order of their blocks
//     roots R           the number of roots whose distances end the file (0 when the program records none)
//   Every other line is a key with its value, kept in order and written back as it was: the programs write there what
//   they drew (the example, the file, R, the accuracy, the view, the marks t+ and t-, ...; apps/README.md lists them).
//   A key may occur on several lines. Keys and values are text without a newline; a key has no blank.
//   THE BLOCKS, little-endian, from the byte after the newline of "data" to the end of the file, with nothing after
//   them:
//     for each layer, in the order of its "layer" line:
//       P uint32         the first point of each polyline: polyline k is the points first[k] .. first[k+1] - 1 (the
//                        last one ends at the last point of the layer); first[0] = 0, and each polyline has at least
//                        one point
//       P int32          the root of each polyline: an index into the roots, or -1 where the program records none
//       P int32          the letter of each polyline (its base arc), or -1
//       M (N + 1) float32  the points, coordinate by coordinate
//     R float64          the roots' distances d(o, g o)
// The floats are the renderer's own (the programs store their points as floats), so that what is read is exactly what
// the program drew, bit for bit; every float and every distance is a finite number. The reader checks the header,
// every count (each against the size of the file before anything is computed from it or allocated), the order of the
// first points, the roots named, that the numbers are finite, and the size of the file, and says what is wrong. The
// writer refuses what the reader would refuse, and writes the file under a temporary name in its folder, renamed at
// the end: a write that fails leaves no file behind, and no older file of the name is damaged. A name that is a symbolic
// link is written through: its target is written (by a temporary name in the target's folder), and the link stays. So
// the folder of the file must be one that may be written in, even when the file itself may be written. A program ended
// while it writes leaves its temporary file (FILE.tmpPID_K, PID the program's process) beside the file it was writing.
//
//   File             the file in memory: the sphere, the other header lines (keys) in order, the layers, the roots
//   Layer            a layer: its name, its polylines' first points, roots and letters, and its points
//   write            write a File (false and a message in *err if it cannot: what is wrong, or the system's reason)
//   read             read one (false and a message in *err if the file is not a polyline file of format 1)
//   exact            a double as the shortest decimal that reads back to the same double (strtod), bit for bit; the
//                    plain form (-70) rather than the exponent (-7e+01) when it is not longer
//   exact_list       several, separated by `sep`
//   parse_list       numbers separated by blanks or commas; false unless all of the text is numbers
//   command_line     a program's command line as a shell would read it: the program's name (without its folder),
//                    then its arguments, each quoted where a shell needs it
//   recordable       whether a header value can hold the text (it has no line break: a command line with an argument
//                    that holds one cannot be recorded)
//   target_of        the file that a name stands for: the name, or the target of the symbolic link it is
//   write_problem    why a file cannot be written, as far as can be told before writing it ("" if it can)
//   layer_of         a layer from a renderer of render/renderer.h (its points, polylines, roots and letters)

#pragma once

#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace zippers::polylines {

inline constexpr int VERSION = 1;
inline const char* const MAGIC = "zippers polylines";

struct Layer {
    std::string name;
    std::vector<uint32_t> first;              // the first point of each polyline
    std::vector<int32_t> root, letter;        // per polyline; -1 where not recorded
    std::vector<float> pts;                   // (sphere + 1) floats a point
    size_t npoly() const { return first.size(); }
};

struct File {
    int sphere = 0;                                             // S^sphere: the points are in R^(sphere + 1)
    std::vector<std::pair<std::string, std::string>> keys;      // the other header lines, in order
    std::vector<Layer> layers;
    std::vector<double> rootd;                                  // the roots' distances

    int dim() const { return sphere + 1; }
    void add(const std::string& key, const std::string& value) { keys.push_back({key, value}); }
    // the value of the first line with this key, or nullptr
    const std::string* value(const std::string& key) const
    {
        for (const auto& kv : keys) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    // every value of the lines with this key, in order
    std::vector<std::string> values(const std::string& key) const
    {
        std::vector<std::string> v;
        for (const auto& kv : keys) if (kv.first == key) v.push_back(kv.second);
        return v;
    }
    const Layer* layer(const std::string& name) const
    {
        for (const Layer& L : layers) if (L.name == name) return &L;
        return nullptr;
    }
};

// ---- numbers in the header ----------------------------------------------------------------------------------------

// the shortest decimal (%.Ng, N = 1..17) that strtod reads back to the same double, bit for bit (so -0 stays -0); where
// that has an exponent, the plain form (%.Nf, the fewest decimals that read back) when it is not longer: -70, not -7e+01
inline std::string exact(double x)
{
    char b[40];
    for (int d = 1; d <= 17; d++) {
        snprintf(b, sizeof b, "%.*g", d, x);
        double y = strtod(b, nullptr);
        if (!memcmp(&x, &y, sizeof x)) break;
    }
    std::string s = b;
    if (s.find('e') != std::string::npos && std::isfinite(x)) {
        char c[400];                                         // (the plain form of 1e308 has 309 digits)
        for (int f = 0; f <= 20; f++) {
            snprintf(c, sizeof c, "%.*f", f, x);
            if (strlen(c) > s.size()) break;                 // the forms with more decimals are longer still
            double y = strtod(c, nullptr);
            if (!memcmp(&x, &y, sizeof x)) { s = c; break; }
        }
    }
    return s;
}

inline std::string exact_list(const double* x, size_t n, char sep = ' ')
{
    std::string s;
    for (size_t i = 0; i < n; i++) { if (i) s += sep; s += exact(x[i]); }
    return s;
}

// numbers separated by blanks or commas (one separator between two numbers); false unless the whole text is numbers
inline bool parse_list(const std::string& s, std::vector<double>& v)
{
    v.clear();
    size_t i = 0, n = s.size();
    while (i < n && s[i] == ' ') i++;
    while (i < n) {
        const char* p = s.c_str() + i;
        char* end = nullptr;
        double x = strtod(p, &end);
        if (end == p) return false;
        v.push_back(x);
        i = (size_t)(end - s.c_str());
        if (i < n && (s[i] == ',' || s[i] == ' ')) {
            i++;
            if (i == n) return false;             // a separator at the end
        } else if (i < n) return false;
    }
    return true;
}

// a program's command line: its name without the folder, then the arguments, each in single quotes where a shell needs it
inline std::string command_line(int argc, char** argv)
{
    std::string s;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (i == 0) {
            size_t sl = a.find_last_of('/');
            if (sl != std::string::npos) a = a.substr(sl + 1);
        } else s += ' ';
        if (!a.empty() && a.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_@%+=:,./-") ==
                              std::string::npos)
            s += a;
        else {
            s += '\'';
            for (char c : a) s += c == '\'' ? std::string("'\\''") : std::string(1, c);
            s += '\'';
        }
    }
    return s;
}

// whether a header value can hold the text: it has no line break (LF or CR)
inline bool recordable(const std::string& value)
{
    return value.find_first_of("\n\r") == std::string::npos;
}

namespace detail {
inline std::string folder_of_name(const std::string& fn)
{
    size_t sl = fn.find_last_of('/');
    return sl == std::string::npos ? std::string(".") : sl == 0 ? std::string("/") : fn.substr(0, sl);
}
}  // namespace detail

// the most symbolic links that a name may go through, as open() follows them: on Linux the kernel's 40 (the C
// library's MAXSYMLINKS, 20, is not what open() follows), elsewhere MAXSYMLINKS (32 on macOS)
#if defined(__linux__)
inline constexpr int LINKS_MAX = 40;
#elif defined(MAXSYMLINKS)
inline constexpr int LINKS_MAX = MAXSYMLINKS;
#else
inline constexpr int LINKS_MAX = 32;
#endif

// the file that the name fn stands for: fn, or, when fn is a symbolic link, its target (followed through links to
// links, a relative target taken from the link's folder). The target need not exist. A loop of links, or a chain of
// more than LINKS_MAX, has no target: then *loop is set (open() refuses such a name, ELOOP), and fn is returned.
inline std::string target_of(const std::string& fn, bool* loop = nullptr)
{
    if (loop) *loop = false;
    std::string t = fn;
    for (int k = 0; k <= LINKS_MAX; k++) {
        struct stat st;
        if (lstat(t.c_str(), &st) != 0 || !S_ISLNK(st.st_mode)) return t;
        if (k == LINKS_MAX) break;
        char buf[4096];
        ssize_t n = readlink(t.c_str(), buf, sizeof buf - 1);
        if (n <= 0) return t;
        buf[n] = 0;
        t = buf[0] == '/' ? std::string(buf) : detail::folder_of_name(t) + "/" + buf;
    }
    if (loop) *loop = true;
    return fn;
}

// why the file fn cannot be written, as far as can be told before writing it: "" if it can, else the reason. The
// file written is fn's target (target_of); it is written under a temporary name in its folder and renamed, so it is
// refused when it is a folder, when it may not be written, and when its folder does not exist or may not be written
// in (also when the file itself may be written); and a name that is a loop of links, or a longer chain than LINKS_MAX,
// as open() refuses it (ELOOP). When fn is a link, the reason names its target.
inline std::string write_problem(const std::string& fn)
{
    bool loop = false;
    const std::string t = target_of(fn, &loop), dir = detail::folder_of_name(t);
    if (loop) return strerror(ELOOP);
    const std::string of = t == fn ? std::string() : "its target " + t + ": ";
    struct stat st;
    const bool exists = stat(t.c_str(), &st) == 0;
    if (exists) {
        if (S_ISDIR(st.st_mode)) return of + strerror(EISDIR);
        if (access(t.c_str(), W_OK) != 0) return of + strerror(errno);
    }
    if (access(dir.c_str(), W_OK | X_OK) != 0) {
        const int e = errno;
        if (!exists) return of + strerror(e);
        return of + "its folder " + dir + " may not be written in (the file is written there under a temporary name, "
               "then renamed): " + strerror(e);
    }
    return std::string();
}

// a layer from a renderer of render/renderer.h: its points (pts), the first point of each polyline (starts), and the
// root and letter of each (polyroot, polyletter), as they are
template <class R>
inline Layer layer_of(const std::string& name, const R& r)
{
    Layer L;
    L.name = name;
    L.first.assign(r.starts.begin(), r.starts.end());
    L.root.assign(r.polyroot.begin(), r.polyroot.end());
    L.letter.assign(r.polyletter.begin(), r.polyletter.end());
    L.pts = r.pts;
    return L;
}

// ---- the binary blocks ----------------------------------------------------------------------------------------------

namespace detail {

inline void put32(std::vector<unsigned char>& out, uint32_t u)
{
    unsigned char b[4] = {(unsigned char)u, (unsigned char)(u >> 8), (unsigned char)(u >> 16), (unsigned char)(u >> 24)};
    out.insert(out.end(), b, b + 4);
}
inline void put64(std::vector<unsigned char>& out, uint64_t u)
{
    for (int k = 0; k < 8; k++) out.push_back((unsigned char)(u >> (8*k)));
}
inline uint32_t get32(const unsigned char* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
inline uint64_t get64(const unsigned char* p)
{
    uint64_t u = 0;
    for (int k = 0; k < 8; k++) u |= (uint64_t)p[k] << (8*k);
    return u;
}

inline bool bad(std::string* err, const std::string& what)
{
    if (err) *err = what;
    return false;
}

// "cannot write FN: " and the system's reason for the error e (errno)
inline bool cannot_write(std::string* err, const std::string& fn, int e)
{
    return bad(err, "cannot write " + fn + ": " + strerror(e));
}

// the first float of pts (d to a point) that is not a finite number, as "point K is not a finite number (coordinate C)";
// empty if there is none
inline std::string not_finite(const std::vector<float>& pts, size_t d)
{
    for (size_t j = 0; j < pts.size(); j++)
        if (!std::isfinite(pts[j]))
            return "point " + std::to_string(j/d) + " is not a finite number (coordinate " + std::to_string(j%d) + ")";
    return std::string();
}

// a key or a value that the header can hold: no newline (and a key no blank, and not empty)
inline bool header_text(const std::string& s, bool key)
{
    if (key && s.empty()) return false;
    for (char c : s) if (c == '\n' || c == '\r' || (key && (c == ' ' || c == '\t'))) return false;
    return true;
}

}  // namespace detail

// write the file (the header, then the blocks); false, and a message in *err, if it cannot. The file is written under a
// temporary name in its folder (FILE.tmpPID_K) and renamed to FILE at the end; a write that fails removes it, so that
// it leaves nothing behind and an older FILE stays as it was. A symbolic link of the name is written through: FILE is
// then its target (target_of)
inline bool write(const std::string& fn, const File& f, std::string* err = nullptr)
{
    using namespace detail;
    const std::string cw = "cannot write " + fn + ": ";
    if (f.sphere < 2 || f.sphere > 4) return bad(err, cw + "the sphere must be 2, 3 or 4");
    const size_t d = (size_t)f.dim();
    std::string head = std::string(MAGIC) + " " + std::to_string(VERSION) + "\nsphere " + std::to_string(f.sphere) + "\n";
    for (const auto& kv : f.keys) {
        if (!header_text(kv.first, true) || !header_text(kv.second, false))
            return bad(err, cw + "a header line that the file cannot hold: key \"" + kv.first + "\"");
        if (kv.first == "sphere" || kv.first == "layer" || kv.first == "roots" || kv.first == "data" ||
            kv.first == "zippers")
            return bad(err, cw + "the key \"" + kv.first + "\" is the format's own");
        head += kv.first + " " + kv.second + "\n";
    }
    for (const Layer& L : f.layers) {
        if (!header_text(L.name, true)) return bad(err, cw + "a layer name that the file cannot hold: \"" + L.name + "\"");
        if (L.pts.size() % d) return bad(err, cw + "layer " + L.name + ": its floats are not a whole number of points");
        const size_t P = L.first.size(), M = L.pts.size()/d;
        if (L.root.size() != P || L.letter.size() != P) return bad(err, cw + "layer " + L.name + ": the tags do not match");
        for (size_t k = 0; k < P; k++) {
            uint32_t end = k + 1 < P ? L.first[k + 1] : (uint32_t)M;
            if ((k == 0 && L.first[0] != 0) || L.first[k] >= end)
                return bad(err, cw + "layer " + L.name + ": polyline " + std::to_string(k) + " has no points");
            if (L.root[k] < -1 || (L.root[k] >= 0 && (size_t)L.root[k] >= f.rootd.size()))
                return bad(err, cw + "layer " + L.name + ": polyline " + std::to_string(k) + " names a root that is not there");
        }
        if (P == 0 && M != 0) return bad(err, cw + "layer " + L.name + ": points and no polyline");
        const std::string nf = not_finite(L.pts, d);
        if (!nf.empty()) return bad(err, cw + "layer " + L.name + ": " + nf);
        head += "layer " + L.name + " " + std::to_string(P) + " " + std::to_string(M) + "\n";
    }
    for (size_t j = 0; j < f.rootd.size(); j++)
        if (!std::isfinite(f.rootd[j]))
            return bad(err, cw + "the distances: root " + std::to_string(j) + " has a distance that is not a finite number");
    head += "roots " + std::to_string(f.rootd.size()) + "\ndata\n";
    // a folder of the name, or a file that may not be written, is refused as it is (the rename would replace it)
    const std::string wp = write_problem(fn);
    if (!wp.empty()) return bad(err, cw + wp);
    const std::string target = target_of(fn);
    std::string tmp;
    int fd = -1, oe = EEXIST;
    for (int k = 0; k < 100 && fd < 0 && oe == EEXIST; k++) {
        tmp = target + ".tmp" + std::to_string((long)getpid()) + "_" + std::to_string(k);
        fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (fd < 0) oe = errno;
    }
    if (fd < 0) return cannot_write(err, fn, oe);
    FILE* out = fdopen(fd, "wb");
    if (!out) { int e = errno; close(fd); unlink(tmp.c_str()); return cannot_write(err, fn, e); }
    int e = 0;                                                  // the first error
    bool ok = fwrite(head.data(), 1, head.size(), out) == head.size();
    if (!ok) e = errno;
    std::vector<unsigned char> buf;
    for (const Layer& L : f.layers) {
        if (!ok) break;
        buf.clear();
        for (uint32_t x : L.first) put32(buf, x);
        for (int32_t x : L.root) put32(buf, (uint32_t)x);
        for (int32_t x : L.letter) put32(buf, (uint32_t)x);
        ok = fwrite(buf.data(), 1, buf.size(), out) == buf.size();
        if (!ok) e = errno;
        // the points, in pieces (a layer can hold millions)
        const size_t piece = 1 << 20;
        for (size_t i = 0; ok && i < L.pts.size(); i += piece) {
            buf.clear();
            for (size_t j = i; j < L.pts.size() && j < i + piece; j++) {
                uint32_t u;
                memcpy(&u, &L.pts[j], 4);
                put32(buf, u);
            }
            ok = fwrite(buf.data(), 1, buf.size(), out) == buf.size();
            if (!ok) e = errno;
        }
    }
    if (ok) {
        buf.clear();
        for (double x : f.rootd) {
            uint64_t u;
            memcpy(&u, &x, 8);
            put64(buf, u);
        }
        ok = buf.empty() || fwrite(buf.data(), 1, buf.size(), out) == buf.size();
        if (!ok) e = errno;
    }
    if (fclose(out) != 0 && ok) { ok = false; e = errno; }
    if (ok && rename(tmp.c_str(), target.c_str()) != 0) { ok = false; e = errno; }
    if (!ok) {
        unlink(tmp.c_str());
        return cannot_write(err, fn, e ? e : EIO);
    }
    return true;
}

// read the file; false, and a message in *err, unless it is a polyline file of format 1 whose counts all agree
inline bool read(const std::string& fn, File& f, std::string* err = nullptr)
{
    using namespace detail;
    f = File();
    FILE* in = fopen(fn.c_str(), "rb");
    if (!in) return bad(err, "cannot read " + fn);
    std::vector<unsigned char> all;
    unsigned char chunk[1 << 16];
    size_t k;
    while ((k = fread(chunk, 1, sizeof chunk, in)) > 0) all.insert(all.end(), chunk, chunk + k);
    bool rd = !ferror(in);
    fclose(in);
    if (!rd) return bad(err, "cannot read " + fn);
    // the header, line by line, up to "data"
    size_t pos = 0, line_no = 0;
    bool have_sphere = false, have_roots = false, have_data = false;
    size_t nroots = 0;
    std::vector<std::pair<size_t, size_t>> counts;            // per layer: polylines, points
    const std::string magic = std::string(MAGIC) + " " + std::to_string(VERSION);
    while (pos < all.size()) {
        size_t nl = pos;
        while (nl < all.size() && all[nl] != '\n') nl++;
        if (nl == all.size())
            return bad(err, line_no == 0 ? fn + ": not a polyline file (no line \"" + magic + "\" first)"
                                         : fn + ": the header does not end with a line \"data\"");
        std::string line(all.begin() + (long)pos, all.begin() + (long)nl);
        pos = nl + 1;
        line_no++;
        if (line_no == 1) {
            if (line.rfind(std::string(MAGIC) + " ", 0) == 0 && line != magic)
                return bad(err, fn + ": a polyline file of another format (" + line + "); this reads format " +
                                std::to_string(VERSION));
            if (line != magic) return bad(err, fn + ": not a polyline file (no line \"" + magic + "\" first)");
            continue;
        }
        if (line == "data") { have_data = true; break; }
        size_t sp = line.find(' ');
        std::string key = line.substr(0, sp), val = sp == std::string::npos ? std::string() : line.substr(sp + 1);
        if (key.empty()) return bad(err, fn + ": line " + std::to_string(line_no) + " has no key");
        auto count = [&](const std::string& s, size_t& out) {
            if (s.empty() || s.size() > 18 || s.find_first_not_of("0123456789") != std::string::npos) return false;
            out = (size_t)strtoull(s.c_str(), nullptr, 10);
            return true;
        };
        if (key == "sphere") {
            if (have_sphere || (val != "2" && val != "3" && val != "4"))
                return bad(err, fn + ": line " + std::to_string(line_no) + ": the sphere must be given once, 2, 3 or 4");
            f.sphere = val[0] - '0';
            have_sphere = true;
        } else if (key == "layer") {
            size_t a = val.find(' '), b = a == std::string::npos ? a : val.find(' ', a + 1);
            size_t P = 0, M = 0;
            if (a == std::string::npos || b == std::string::npos || !count(val.substr(a + 1, b - a - 1), P) ||
                !count(val.substr(b + 1), M) || a == 0)
                return bad(err, fn + ": line " + std::to_string(line_no) + ": a layer is \"layer NAME POLYLINES POINTS\"");
            Layer L;
            L.name = val.substr(0, a);
            f.layers.push_back(L);
            counts.push_back({P, M});
        } else if (key == "roots") {
            if (have_roots || !count(val, nroots))
                return bad(err, fn + ": line " + std::to_string(line_no) + ": the roots must be given once, as a count");
            have_roots = true;
        } else f.keys.push_back({key, val});
    }
    if (line_no == 0) return bad(err, fn + ": not a polyline file (no line \"" + magic + "\" first)");
    if (!have_data) return bad(err, fn + ": the header does not end with a line \"data\"");
    if (!have_sphere) return bad(err, fn + ": no line \"sphere N\"");
    if (!have_roots) return bad(err, fn + ": no line \"roots R\"");
    // the size of the blocks the header announces, against the size of the file. Each count is bounded by the size of
    // the blocks before any product (a polyline takes 12 bytes, a point 4 (N + 1), a root 8), so that no product or sum
    // overflows and nothing larger than the file is allocated
    const size_t d = (size_t)f.dim();
    const unsigned long long have = (unsigned long long)(all.size() - pos);
    for (size_t i = 0; i < counts.size(); i++)
        if (counts[i].first > have/12 || counts[i].second > have/(4*d))
            return bad(err, fn + ": layer " + f.layers[i].name + ": " + std::to_string(counts[i].first) + " polylines and " +
                            std::to_string(counts[i].second) + " points cannot fit in the " + std::to_string(have) +
                            " bytes of the blocks");
    if (nroots > have/8)
        return bad(err, fn + ": " + std::to_string(nroots) + " roots cannot fit in the " + std::to_string(have) +
                        " bytes of the blocks");
    unsigned long long want = 8ull*nroots;                    // at most `have`, and so is each term below
    for (const auto& c : counts) {
        if (want > have) break;
        want += 12ull*c.first + 4ull*d*c.second;
    }
    if (want != have)
        return bad(err, fn + ": the blocks hold " + std::to_string(have) + " bytes, the header announces " +
                        (want > have ? "more" : std::to_string(want)));
    const unsigned char* p = all.data() + pos;
    for (size_t i = 0; i < f.layers.size(); i++) {
        Layer& L = f.layers[i];
        const size_t P = counts[i].first, M = counts[i].second;
        L.first.resize(P); L.root.resize(P); L.letter.resize(P); L.pts.resize(d*M);
        for (size_t j = 0; j < P; j++, p += 4) L.first[j] = get32(p);
        for (size_t j = 0; j < P; j++, p += 4) L.root[j] = (int32_t)get32(p);
        for (size_t j = 0; j < P; j++, p += 4) L.letter[j] = (int32_t)get32(p);
        for (size_t j = 0; j < d*M; j++, p += 4) { uint32_t u = get32(p); memcpy(&L.pts[j], &u, 4); }
        if (P == 0 && M != 0) return bad(err, fn + ": layer " + L.name + " has points and no polyline");
        const std::string nf = not_finite(L.pts, d);
        if (!nf.empty()) return bad(err, fn + ": layer " + L.name + ": " + nf);
        for (size_t j = 0; j < P; j++) {
            uint64_t end = j + 1 < P ? L.first[j + 1] : M;
            if ((j == 0 && L.first[0] != 0) || L.first[j] >= end)
                return bad(err, fn + ": layer " + L.name + ": polyline " + std::to_string(j) + " has no points");
            if (L.root[j] < -1 || (L.root[j] >= 0 && (size_t)L.root[j] >= nroots))
                return bad(err, fn + ": layer " + L.name + ": polyline " + std::to_string(j) + " names a root that is not there");
        }
    }
    f.rootd.resize(nroots);
    for (size_t j = 0; j < nroots; j++, p += 8) {
        uint64_t u = get64(p);
        memcpy(&f.rootd[j], &u, 8);
        if (!std::isfinite(f.rootd[j]))
            return bad(err, fn + ": the distances: root " + std::to_string(j) + " has a distance that is not a finite number");
    }
    return true;
}

}  // namespace zippers::polylines
