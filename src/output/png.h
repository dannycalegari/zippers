// SPDX-License-Identifier: GPL-3.0-or-later
//
// png.h: the PNG writers and the PNG reader of the three programs, written once.
//
// These are the legacy functions below, moved with their bodies unchanged: `static` is `inline`, `vector` and
// `string` are written `std::vector` and `std::string`, and the names are new. Every expression and loop is the
// legacy one, so the files are byte for byte the same (tests/unit/test_output_*.cc checks this against each legacy
// original, in the same process, with the same zlib).
//
//   here                  legacy                   where
//   png::chunk            png_chunk                legacy/s3/zipcore.h, legacy/s4/zipcore5.h, legacy/s2/zipper.cc
//                                                  (one function: the S2 copy differs in white space only)
//   png::write_rgb        write_png_rgb            legacy/s3/zipcore.h, legacy/s4/zipcore5.h (one function)
//   png::write_rows       write_png_raw            legacy/s2/zipper.cc
//   png::write_rgb_sub    png_write                legacy/s2/movie/src/zmovie.cc
//   png::read             png_read                 legacy/s2/movie/src/zmovie.cc
//
// The writers differ, and each keeps its own behaviour (do not "unify" them: each program's files must stay byte for byte the same):
//   write_rgb      RGB rows, filter 0, zlib at `level` (default 6); the file is opened only after the compression
//                  has worked; it returns true even if a write or the fclose failed.
//   write_rows     rows already filtered by the caller, zlib at level 6; it FREES `raw` once compressed (before the
//                  file is opened, so also when the file cannot be opened); it returns fclose(f) == 0.
//   write_rgb_sub  RGB rows with the filter "Sub" (smaller files), zlib at `level`; written to fn + ".tmp" and then
//                  renamed to fn; a failed rename leaves the .tmp file.
//   read           8-bit RGB, RGBA (composited over white) and grey (with or without alpha), not interlaced, all five
//                  filters, any number of IDAT chunks; it checks no CRC. It prints one line to stderr for a PNG of an
//                  unsupported kind.
//
// This header needs zlib: a program that includes it links ZLIB::ZLIB (the target zippers::core does not), as the
// legacy targets do.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace zippers::png {

// one chunk: length, type, data, CRC (legacy png_chunk)
inline void chunk(FILE* f, const char* type, const uint8_t* data, size_t len)
{
    uint8_t hdr[4] = { (uint8_t)(len>>24), (uint8_t)(len>>16), (uint8_t)(len>>8), (uint8_t)len };
    fwrite(hdr, 1, 4, f); fwrite(type, 1, 4, f); if (len) fwrite(data, 1, len, f);
    uLong crc = crc32(0, (const Bytef*)type, 4);
    if (len) crc = crc32(crc, (const Bytef*)data, (uInt)len);
    uint8_t cb[4] = { (uint8_t)(crc>>24), (uint8_t)(crc>>16), (uint8_t)(crc>>8), (uint8_t)crc };
    fwrite(cb, 1, 4, f);
}

// 8-bit RGB, W*H*3 bytes in `rgb`, every row with the filter byte 0 (legacy write_png_rgb)
inline bool write_rgb(const char* fn, const std::vector<uint8_t>& rgb, int W, int H, int level = 6)
{
    std::vector<uint8_t> raw;
    raw.reserve((size_t)H*(1 + W*3));
    for (int y = 0; y < H; ++y) { raw.push_back(0); const uint8_t* row = &rgb[(size_t)y*W*3]; raw.insert(raw.end(), row, row + W*3); }
    uLongf clen = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), (uLong)raw.size(), level) != Z_OK) return false;
    FILE* f = fopen(fn, "wb");
    if (!f) return false;
    const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = { (uint8_t)(W>>24), (uint8_t)(W>>16), (uint8_t)(W>>8), (uint8_t)W,
                         (uint8_t)(H>>24), (uint8_t)(H>>16), (uint8_t)(H>>8), (uint8_t)H, 8, 2, 0, 0, 0 };
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", comp.data(), clen);
    chunk(f, "IEND", nullptr, 0);
    fclose(f);
    return true;
}

// raw: H rows of (filter byte, W RGB triples); freed once compressed (legacy write_png_raw)
inline bool write_rows(const char* fn, std::vector<uint8_t>& raw, int W, int H)
{
    uLongf clen = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), (uLong)raw.size(), 6) != Z_OK) return false;
    std::vector<uint8_t>().swap(raw);
    FILE* f = fopen(fn, "wb"); if (!f) return false;
    const uint8_t sig[8] = {137,80,78,71,13,10,26,10}; fwrite(sig,1,8,f);
    uint8_t ihdr[13] = { (uint8_t)(W>>24),(uint8_t)(W>>16),(uint8_t)(W>>8),(uint8_t)W,
                         (uint8_t)(H>>24),(uint8_t)(H>>16),(uint8_t)(H>>8),(uint8_t)H,
                         8,2,0,0,0 };                    // 8-bit RGB
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", comp.data(), clen);
    chunk(f, "IEND", nullptr, 0);
    return fclose(f) == 0;
}

// 8-bit RGB with the filter "Sub", written to fn + ".tmp" and renamed (the film engine's png_write)
inline bool write_rgb_sub(const std::string& fn, const std::vector<uint8_t>& rgb, int W, int H, int level)
{
    const size_t rowlen = 1 + (size_t)W*3;
    std::vector<uint8_t> raw((size_t)H*rowlen);
    for (int y = 0; y < H; ++y) {                    // filter "Sub" (smaller files than none)
        uint8_t* row = &raw[(size_t)y*rowlen]; row[0] = 1;
        const uint8_t* src = &rgb[(size_t)y*W*3];
        for (int i = 0; i < W*3; ++i) row[1 + i] = (uint8_t)(src[i] - (i >= 3 ? src[i-3] : 0));
    }
    uLongf clen = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), (uLong)raw.size(), level) != Z_OK) return false;
    std::string tmp = fn + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb"); if (!f) return false;
    const uint8_t sig[8] = {137,80,78,71,13,10,26,10}; fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = { (uint8_t)(W>>24),(uint8_t)(W>>16),(uint8_t)(W>>8),(uint8_t)W,
                         (uint8_t)(H>>24),(uint8_t)(H>>16),(uint8_t)(H>>8),(uint8_t)H, 8,2,0,0,0 };
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", comp.data(), clen);
    chunk(f, "IEND", nullptr, 0);
    if (fclose(f) != 0) return false;
    return rename(tmp.c_str(), fn.c_str()) == 0;
}

// 8-bit RGB / RGBA (composited over white) / grey, non-interlaced (the film engine's png_read)
inline bool read(const std::string& fn, std::vector<uint8_t>& rgb, int& W, int& H)
{
    FILE* f = fopen(fn.c_str(), "rb"); if (!f) return false;
    std::vector<uint8_t> d; uint8_t buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    fclose(f);
    if (d.size() < 33 || memcmp(d.data(), "\x89PNG\r\n\x1a\n", 8)) return false;
    auto be = [&](size_t p) { return (uint32_t)d[p] << 24 | (uint32_t)d[p+1] << 16 | (uint32_t)d[p+2] << 8 | d[p+3]; };
    size_t p = 8; int depth = 0, ctype = 0, inter = 0; std::vector<uint8_t> idat;
    while (p + 8 <= d.size()) {
        uint32_t len = be(p); std::string type((const char*)&d[p+4], 4);
        if (p + 12 + len > d.size()) return false;
        const uint8_t* c = &d[p+8];
        if (type == "IHDR") { W = (int)be(p+8); H = (int)be(p+12); depth = c[8]; ctype = c[9]; inter = c[12]; }
        else if (type == "IDAT") idat.insert(idat.end(), c, c + len);
        else if (type == "IEND") break;
        p += 12 + len;
    }
    int bpp = ctype == 2 ? 3 : ctype == 6 ? 4 : ctype == 0 ? 1 : ctype == 4 ? 2 : 0;
    if (depth != 8 || !bpp || inter) { fprintf(stderr, "%s: unsupported PNG (8-bit RGB/RGBA/grey only)\n", fn.c_str()); return false; }
    size_t stride = (size_t)W*bpp;
    std::vector<uint8_t> raw((size_t)H*(stride + 1));
    uLongf rl = raw.size();
    if (uncompress(raw.data(), &rl, idat.data(), idat.size()) != Z_OK || rl != raw.size()) return false;
    std::vector<uint8_t> px((size_t)H*stride);
    for (int y = 0; y < H; ++y) {
        int ft = raw[(size_t)y*(stride + 1)];
        const uint8_t* s = &raw[(size_t)y*(stride + 1) + 1];
        uint8_t* o = &px[(size_t)y*stride];
        const uint8_t* up = y ? &px[(size_t)(y-1)*stride] : nullptr;
        for (size_t i = 0; i < stride; ++i) {
            int a = i >= (size_t)bpp ? o[i - bpp] : 0, b = up ? up[i] : 0, c = (up && i >= (size_t)bpp) ? up[i - bpp] : 0, v = s[i];
            switch (ft) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b)/2; break;
                case 4: { int pp = a + b - c, pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
                          v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
                default: return false;
            }
            o[i] = (uint8_t)v;
        }
    }
    rgb.resize((size_t)W*H*3);
    for (size_t i = 0; i < (size_t)W*H; ++i) {
        const uint8_t* q = &px[i*bpp];
        if (bpp >= 3) {
            int al = bpp == 4 ? q[3] : 255;
            for (int k = 0; k < 3; ++k) rgb[3*i + k] = (uint8_t)((q[k]*al + 255*(255 - al) + 127)/255);
        } else {
            int al = bpp == 2 ? q[1] : 255, g = (q[0]*al + 255*(255 - al) + 127)/255;
            rgb[3*i] = rgb[3*i+1] = rgb[3*i+2] = (uint8_t)g;
        }
    }
    return true;
}

}  // namespace zippers::png
