// SPDX-License-Identifier: GPL-3.0-or-later
//
// pairs.h: the linked pairs of loops that `link3 --export` writes and `zipper3 --pairs` draws.
//
// A LINKED PAIR is the loop gamma_w of Z+ (at the identity) and the translate h.delta_w' of a loop of Z-, with their
// linking number. The file (linked_pairs.txt) has one pair per line, five fields separated by '|':
//     lk +1 | w 4 17 23 | w' 1 28 24 20 | phi(h) 1 | h a1
// the linking number, the two relations (letter numbers), phi(h), and h as a word of the example; lines that are
// empty or begin with '#' are skipped, and so is a line without exactly five fields.
//   LinkedPair    one pair: the file's fields, the two loops as closed polylines in renderers (the first point
//                 repeated at the end), a projection pole far from both, and the clip radius of its picture
//   load_pairs    read a file of pairs; prints "FILE: n linked pairs"; false if it cannot be read or has none
//   build_pair    make the two loops at the accuracy acc (render/arcs.h: loop_points), the pole (analysis/linking.h:
//                 far_pole with the seed 7) and the clip (1.05 times the largest stereographic radius)
//   pair_view     the view of a pair: the frame of its pole, its clip, zoom 1
//   pair_desc     the line that describes pair k
//
// Moved from legacy/s3/zipper3.cc (LinkedPair, load_pairs, build_pair, pair_view, pair_desc), in namespace zippers.
// Every step is the legacy one, in the same order (tests/unit/test_view_s3.cc). Changes of form: LinkedPair holds the
// generic Renderer<B> (a template over the backend, for any B whose sphere at infinity is S^3; the legacy Renderer is
// Renderer<PU21>); build_pair is a template over the example's render policy P (examples/*_render.h), whose P::word
// reads the word of h (for Cartwright-Steger parse_word, as the legacy build_pair called), with the backend named
// Backend, so that the two loops keep their legacy names A and B; the view is s3::ViewState (view/s3.h).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "analysis/linking.h"
#include "geometry/sphere.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/world.h"
#include "view/s3.h"

namespace zippers {

// ---------------------------------------------------------------------------------------
// Linked pairs (written by `link3 --export`): the loop gamma_w of Z+ and the translate
// h.delta_w' of a loop of Z-, with their linking number

template <class B>
struct LinkedPair {
    long lk = 0;
    std::vector<int> w, w2;
    int phi = 0;
    std::string hword;
    Renderer<B> *a = nullptr, *b = nullptr;    // the two loops, closed polylines
    double pole[4] = {0,0,0,1};                // a projection pole far from both
    double clip = 3;
};

template <class B>
inline bool load_pairs(const std::string& fn, std::vector<LinkedPair<B>>& P)
{
    std::ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot open %s\n", fn.c_str()); return false; }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f;            // lk +1 | w 4 17 23 | w' 1 28 24 20 | phi(h) 1 | h a1
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) f.push_back(part);
        if (f.size() != 5) continue;
        LinkedPair<B> p;
        sscanf(f[0].c_str(), " lk %ld", &p.lk);
        auto ints = [](const std::string& x) { std::istringstream is(x); std::string tag; is >> tag; std::vector<int> v; int k; while (is >> k) v.push_back(k); return v; };
        p.w = ints(f[1]);
        p.w2 = ints(f[2]);
        sscanf(f[3].c_str(), " phi(h) %d", &p.phi);
        p.hword = f[4].substr(f[4].find('h') + 1);
        P.push_back(p);
    }
    fprintf(stderr, "%s: %zu linked pairs\n", fn.c_str(), P.size());
    return !P.empty();
}

template <class P>
inline void build_pair(LinkedPair<typename P::B>& p, const World<typename P::B>& W, double acc)
{
    typedef typename P::B Backend;
    static_assert(Backend::N == 4, "the linked pairs are loops on S^3");
    delete p.a; delete p.b;
    size_t forced = 0;
    std::vector<SpherePoint<4>> A = loop_points(W.plus, W.L, Backend::identity(), p.w, acc, forced);
    std::vector<SpherePoint<4>> B = loop_points(W.minus, W.L, P::word(p.hword), p.w2, acc, forced);
    p.a = new Renderer<Backend>(W.plus, W.L, acc, 1);
    p.b = new Renderer<Backend>(W.minus, W.L, acc, 1);
    for (auto* pr : {p.a, p.b}) {
        const std::vector<SpherePoint<4>>& X = pr == p.a ? A : B;
        pr->starts.push_back(0);
        for (const SpherePoint<4>& q : X) pr->push(q);
        pr->push(X[0]);
    }
    std::mt19937_64 rng(7);
    far_pole(A, B, rng, p.pole);
    double F[4][4], rmax = 0;
    s3::frame_with_pole(p.pole, F);
    for (const std::vector<SpherePoint<4>>* X : {&A, &B})
        for (const s3::V3& y : s3::stereo(*X, F)) rmax = std::max(rmax, std::sqrt(y[0]*y[0] + y[1]*y[1] + y[2]*y[2]));
    p.clip = 1.05*rmax;
}

template <class B>
inline void pair_view(const LinkedPair<B>& p, s3::ViewState& v)
{
    s3::frame_with_pole(p.pole, v.Q);
    v.clip = p.clip;
    v.zoom = 1;
}

template <class B>
inline std::string pair_desc(const LinkedPair<B>& p, int k)
{
    char b[400];
    std::string w, w2;
    for (int x : p.w) w += " " + std::to_string(x);
    for (int x : p.w2) w2 += " " + std::to_string(x);
    snprintf(b, sizeof b, "pair %d: lk %+ld   gamma_w, w =%s (Z+)   h.delta_w', w' =%s (Z-), phi(h) = %d", k, p.lk, w.c_str(), w2.c_str(), p.phi);
    return b;
}

}  // namespace zippers
