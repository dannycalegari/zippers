// SPDX-License-Identifier: GPL-3.0-or-later
//
// raster.h: the software rasteriser of the three programs, written once: lines, the depth-sorted segment painter,
// the crosses of the marked points, and the 2x2 supersampling down to the picture.
//
// These are the legacy functions and statements below, moved with their bodies unchanged: `static` is `inline`,
// `vector` is written `std::vector`, and the names are new. Every expression and loop is the legacy one, so the
// buffers are byte for byte the same (tests/unit/test_output_*.cc checks this against each legacy original).
//
//   here                          legacy
//   raster::line_rgb              ras_line(img, W, H, x0, y0, x1, y1, c[3], th = 0)   legacy/s3/zipcore.h,
//                                 legacy/s4/zipper5.cc (one function: the S4 copy names the loop variables of the
//                                 brush ey, ex, the S3 copy dy, dx, which hide those of the Bresenham loop; the
//                                 hiding is harmless, and this is the S4 text)
//   raster::line_index            ras_line(img, W, H, x0, y0, x1, y1, col)            legacy/s2/zipper.cc
//   raster::Segment               the local struct Seg of write_png (S3) and write_png5 (S4)
//   raster::paint_segments        the sort and the drawing of the segments, in write_png and write_png5
//   raster::cross                 the black cross of a marked point, in write_png (brush 0) and write_png5 (brush 1)
//   raster::downsample_rgb        the SSxSS box filter of write_png and write_png5, RGB to RGB
//   raster::downsample_palette_rows  the SSxSS box filter of write_png of legacy/s2/zipper.cc: palette to PNG rows
//
// What is NOT here: the projections and the fog (how a segment gets its endpoints, its depth z, its colour and its
// priority) and the pixel of a marked point; they belong to the views. The caller fills a Segment, exactly as the
// legacy painters fill their Seg, and gives the integer pixel (cx, cy) of a mark.
//
// The legacy write_png (S3) and write_png5 (S4) then read, with SS = 2, w = h = size*SS:
//     std::vector<uint8_t> img((size_t)w*h*3, 255);                       // white
//     std::vector<raster::Segment> segs;  ...the view fills it...
//     raster::paint_segments(img, w, h, segs, thick);
//     for each mark in view:  raster::cross(img, w, h, cx, cy, SS, 0);     // brush 1 in S4
//     return png::write_rgb(fn, raster::downsample_rgb(img, w, size, SS), size, size);
// and the legacy write_png of S2 (a palette image, with pal[0] white and pal[1] black):
//     std::vector<uint8_t> img((size_t)w*h, 0);                           // index 0
//     raster::line_index(img, w, h, x0, y0, x1, y1, index);               // the silhouette and the polylines
//     std::vector<uint8_t> raw = raster::downsample_palette_rows(img, pal, w, W, H, SS);
//     std::vector<uint8_t>().swap(img);                                   // before the compression, as in the legacy code
//     return png::write_rows(fn, raw, W, H);
// tests/unit/test_output_s3.cc, test_output_s4.cc and test_output_s2.cc assemble exactly these pictures and compare them
// with the legacy functions.
//
// The images are `std::vector<uint8_t>`: W*H*3 bytes for RGB (row by row, 3 bytes a pixel), W*H bytes for a
// palette image. The lines clip to the image (a pixel outside it is skipped) and take any endpoints, but the
// loops run once for every pixel of the line, so keep the endpoints within a few image sizes of it.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace zippers::raster {

// a Bresenham line in an RGB image, colour c, drawn with a square brush of half-width th (legacy ras_line of
// zipcore.h and zipper5.cc)
inline void line_rgb(std::vector<uint8_t>& img, int W, int H, int x0, int y0, int x1, int y1, const uint8_t c[3], int th = 0)
{
    int dx = std::abs(x1-x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1-y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        for (int ey = -th; ey <= th; ey++) for (int ex = -th; ex <= th; ex++) {
            int x = x0 + ex, y = y0 + ey;
            if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
                uint8_t* p = &img[((size_t)y*W + x)*3];
                p[0] = c[0]; p[1] = c[1]; p[2] = c[2];
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// the same line in a palette image (one byte a pixel: an index into a palette), no brush (legacy ras_line of
// zipper.cc)
inline void line_index(std::vector<uint8_t>& img, int W, int H, int x0, int y0, int x1, int y1, uint8_t col)
{
    int dx = std::abs(x1-x0), sx = x0<x1?1:-1, dy = -std::abs(y1-y0), sy = y0<y1?1:-1, err = dx+dy;
    for (;;) {
        if ((unsigned)x0 < (unsigned)W && (unsigned)y0 < (unsigned)H) {
            img[(size_t)y0*W + x0] = col;
        }
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// a segment of a drawing, in image coordinates, with its depth z, its RGB colour and its priority (the local struct
// Seg of write_png and write_png5). Lower priority is drawn first, underneath; within a priority, lower z first
// ("far first").
struct Segment { float x0, y0, x1, y1, z; uint8_t c[3]; int prio; };

// sort the segments by (prio, z) and draw them in that order, with the brush th, in an RGB image of w x h pixels.
// The sort is std::sort, which is NOT stable: segments with equal (prio, z) come out in an order that depends on the
// input order, so the picture is the legacy one only if `segs` is in the legacy order. `segs` is left sorted.
inline void paint_segments(std::vector<uint8_t>& img, int w, int h, std::vector<Segment>& segs, int th)
{
    std::sort(segs.begin(), segs.end(), [](const Segment& a, const Segment& b) { return a.prio != b.prio ? a.prio < b.prio : a.z < b.z; });   // far first
    for (const Segment& s : segs)
        line_rgb(img, w, h, (int)std::lround(s.x0), (int)std::lround(s.y0), (int)std::lround(s.x1), (int)std::lround(s.y1), s.c, th);
}

// the small black cross of a marked point at the pixel (cx, cy) of an RGB image of w x h pixels: two lines of
// half-length 4*SS (SS = the supersampling factor), brush th (0 in write_png of zipcore.h, 1 in write_png5)
inline void cross(std::vector<uint8_t>& img, int w, int h, int cx, int cy, int SS, int th)
{
    const uint8_t blk[3] = {0,0,0};
    line_rgb(img, w, h, cx - 4*SS, cy, cx + 4*SS, cy, blk, th);
    line_rgb(img, w, h, cx, cy - 4*SS, cx, cy + 4*SS, blk, th);
}

// the SSxSS box filter: the RGB image img (w pixels a row, SS*size rows) to the RGB image of size x size pixels, each
// the mean of its block, rounded down
inline std::vector<uint8_t> downsample_rgb(const std::vector<uint8_t>& img, int w, int size, int SS)
{
    std::vector<uint8_t> out((size_t)size*size*3);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) for (int c = 0; c < 3; ++c) {
        int sum = 0;
        for (int dy = 0; dy < SS; ++dy) for (int dx = 0; dx < SS; ++dx) sum += img[(((size_t)(y*SS+dy))*w + (x*SS+dx))*3 + c];
        out[((size_t)y*size + x)*3 + c] = (uint8_t)(sum/(SS*SS));
    }
    return out;
}

// the same box filter for a palette image img (w pixels a row, SS*H rows), straight into the rows of a PNG of W x H
// pixels: H rows of (filter byte 0, W RGB triples), as png::write_rows takes them. pal[i] is the colour of index i.
// The caller frees img afterwards, as the legacy write_png does, before the rows are compressed.
inline std::vector<uint8_t> downsample_palette_rows(const std::vector<uint8_t>& img, const uint8_t pal[256][3], int w, int W, int H, int SS)
{
    const size_t rowlen = 1 + (size_t)W*3;
    std::vector<uint8_t> raw((size_t)H*rowlen);
    for (int y = 0; y < H; ++y) {
        uint8_t* row = &raw[(size_t)y*rowlen]; row[0] = 0;
        for (int x = 0; x < W; ++x) for (int c = 0; c < 3; ++c) {
            int sum = 0;
            for (int dy = 0; dy < SS; ++dy) for (int dx = 0; dx < SS; ++dx)
                sum += pal[img[((size_t)(y*SS+dy))*w + (x*SS+dx)]][c];
            row[1 + (size_t)x*3 + c] = (uint8_t)(sum/(SS*SS));
        }
    }
    return raw;
}

}  // namespace zippers::raster
