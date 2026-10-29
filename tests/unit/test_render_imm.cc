// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_render_imm: the renderer of src/render/ on the backend ON1<5>, with the render policy of IMM's M5
// (examples/imm_m5_render.h), and the S^4 view and painter (src/view/s4.h) agree BIT FOR BIT with the legacy S^4 code
// of legacy/s4/zipcore5.h and legacy/s4/zipper5.cc (included here, with its main renamed, in the global namespace).
// The policy's word reader reads facet 1, which the legacy reader dropped, so the two would be given other
// letters: the test uses imm::RenderPolicy with the legacy word reader (LegacyReadingPolicy, below), so that both
// renderers get the same generators and the comparisons test the renderer, not the reader. The policy
// also gives the zippers tips, which the legacy renderer does not draw; LegacyReadingPolicy gives none.
//   - the substitution reader (render/substitution.h) in the IMM dialect against load_subst5, on every stored file of
//     legacy/s4 and legacy/s3 and on many variants of them, and on a missing file: the return value, the message and
//     the contents;
//   - prepare<LegacyReadingPolicy> against prepare5: the letters, both sides, the message; and its error paths, in
//     child processes;
//   - make_side against make_side5, both sides, with each file's letters, and a roof that fails;
//   - Renderer<ON1<5>> against Renderer5: every stored file, R = 0, 1.2, 1.8, accuracies 0.05, 0.02, 0.01, both
//     sides, drawn from the roots as build5 draws them: the points (as floats), the polyline starts, the root and
//     letter of every polyline (against Renderer5's records and against the draws that made them), the counters
//     (visited, skipped, leaves, spikes, forced, depth, cap, open), the fingerprints of the branches drawn, the frames
//     left by a capped draw. A leaf cap keeps each case light; some cases are complete, some capped, and two use a
//     small depth limit, so that leaves are forced;
//   - build<LegacyReadingPolicy> against build5: the lines it prints (time masked), the roots and their distances, the
//     renderers; the guard of a large R (in a child);
//   - arc_points against arc_points5, and loop_points against the same loop made of the legacy arc_points5 (the
//     legacy S^4 code has no loop_points): branches at the identity, at roots and at products of letters; relations;
//     a side with t = 1, whose arcs reach the depth limit (forced leaves); and branches at powers of t^-1, which
//     reach the growth guard (forced leaves);
//   - the view: orthonormal_frame, rot3, turn4, move_pole5, eye5, init_view5 and PI, on random inputs;
//   - write_png5 against write_png5: the PNG files of real renderers (the legacy ones drawn by Renderer5, the new ones
//     by Renderer<ON1<5>>) in random views, sizes, brushes and layers; and a file that cannot be written.
// Built with the flags of the programs (-O2), under which the results must agree. ZIPPERS_REPO is the repository.

#define main legacy_main
#include "zipper5.cc"                    // legacy/s4: rh5.h, zipcore5.h and zipper5.cc (write_png5)
#undef main

#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "geometry/on1/on1.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/substitution.h"
#include "render/world.h"
#include "view/s4.h"

#include "render_test_support.h"

#include <map>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::ON1<5> NB;

// The test's own render policy: imm::RenderPolicy, but with the words read as the legacy reader reads them
// (legacy/s4/zipcore5.h's parse_fword, which drops facet 1). imm::parse_fword reads facet 1, so imm::RenderPolicy
// makes other letters (and another t, in two of the three files) than prepare5; with this policy both renderers
// are given the same generators, and the comparisons below test the renderer, not the reader. The corrected reader
// is tested in test_on1.cc (against the legacy one) and test_reader_imm.cc (against chz5's S_c and t).
// And without tips: imm::RenderPolicy::sides gives each zipper its tips (render/tips.h), through which
// the leaves are drawn, as the legacy Renderer5 did not; this policy's sides adds none, so that the renderer is
// compared with the legacy one (the tips are tested in test_tips_imm.cc).
struct LegacyReadingPolicy : z::imm::RenderPolicy {
    static NB::Elem word(const std::string& w) { return z::imm::word_matrix(::parse_fword(w)); }
    static void sides(const z::Letters<NB>&, z::Side<NB>&, z::Side<NB>&) {}       // no tips: the legacy drawing
};
typedef LegacyReadingPolicy NP;

using rtest::check;
using rtest::note;
using rtest::section;
using rtest::same_bytes;
using rtest::same_vector_bytes;

// the legacy and the new elements have the same layout
static NB::Elem nm(const M6& a) { NB::Elem b; memcpy(&b, &a, sizeof b); return b; }

static const char* const FILES4[3] = {"subst5_2.30_4.50_t1.txt", "subst5_2.30_4.50_t2.txt", "subst5_2.30_4.50_t3.txt"};
static const char* const FILES3[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
static std::string repo_file(const char* sub, const char* f) { return std::string(ZIPPERS_REPO) + "/legacy/" + sub + "/" + f; }

// ---- comparisons -----------------------------------------------------------------------------------------------------

static void compare_letters(const Letters5& a, const z::Letters<NB>& b, const std::string& what)
{
    check(same_vector_bytes(a.s, b.s), "%s: the letters", what.c_str());
    check(a.inv == b.inv, "%s: inv", what.c_str());
    check(a.plus == b.plus, "%s: plus", what.c_str());
    check(a.canon == b.canon, "%s: canon", what.c_str());
}

static void compare_side(const Side5& a, const z::Side<NB>& b, const std::string& what)
{
    const char* w = what.c_str();
    check(!strcmp(a.name, b.name), "%s: the name", w);
    check(same_bytes(a.t, b.t), "%s: t", w);
    check(same_bytes(a.vfix, b.vfix), "%s: vfix", w);
    check(same_bytes(a.V, b.V), "%s: V", w);
    check(a.roof == b.roof, "%s: the roofs", w);
    bool off = a.off.size() == b.off.size(), keyoff = a.keyoff.size() == b.keyoff.size(), wv = a.wv.size() == b.wv.size();
    for (size_t j = 0; off && j < a.off.size(); j++) off = same_vector_bytes(a.off[j], b.off[j]);
    for (size_t j = 0; keyoff && j < a.keyoff.size(); j++) keyoff = same_vector_bytes(a.keyoff[j], b.keyoff[j]);
    for (size_t j = 0; wv && j < a.wv.size(); j++) wv = same_vector_bytes(a.wv[j], b.wv[j]);
    check(off, "%s: the offsets", w);
    check(keyoff, "%s: the key offsets", w);
    check(wv, "%s: wv", w);
    check(same_vector_bytes(a.ev, b.ev), "%s: ev", w);
}

static void compare_renderer(const Renderer5& a, const z::Renderer<NB>& b, const std::vector<int>& eroot,
                             const std::vector<int>& eletter, const std::string& what)
{
    const char* w = what.c_str();
    check(same_vector_bytes(a.pts, b.pts), "%s: the points (%zu floats, legacy %zu)", w, b.pts.size(), a.pts.size());
    check(a.starts == b.starts, "%s: the polyline starts (%zu, legacy %zu)", w, b.starts.size(), a.starts.size());
    check(a.polyroot == b.polyroot && a.polyletter == b.polyletter, "%s: the root and letter of every polyline (Renderer5's)", w);
    check(b.polyroot == eroot && b.polyletter == eletter, "%s: the root and letter of every polyline (the draws)", w);
    check(a.curroot == b.curroot && a.curletter == b.curletter, "%s: the current root and letter", w);
    check(a.nVisited == b.nVisited, "%s: visited %zu, legacy %zu", w, b.nVisited, a.nVisited);
    check(a.nSkipped == b.nSkipped, "%s: skipped %zu, legacy %zu", w, b.nSkipped, a.nSkipped);
    check(a.nLeaves == b.nLeaves, "%s: leaves %zu, legacy %zu", w, b.nLeaves, a.nLeaves);
    check(a.nSpike == b.nSpike, "%s: spikes %zu, legacy %zu", w, b.nSpike, a.nSpike);
    check(a.nForced == b.nForced, "%s: forced %zu, legacy %zu", w, b.nForced, a.nForced);
    check(a.deepest == b.deepest, "%s: depth %d, legacy %d", w, b.deepest, a.deepest);
    check(a.capped == b.capped, "%s: capped %d, legacy %d", w, b.capped, a.capped);
    check(a.open == b.open, "%s: open %d, legacy %d", w, b.open, a.open);
    check(a.done.n == b.done.n && a.done.mask == b.done.mask && a.done.t == b.done.t,
          "%s: the fingerprints of the branches drawn (%zu, legacy %zu)", w, b.done.n, a.done.n);
    bool st = a.stk.size() == b.stk.size();
    for (size_t i = 0; st && i < a.stk.size(); i++)
        st = same_bytes(a.stk[i].h, b.stk[i].h) && a.stk[i].j == b.stk[i].j && a.stk[i].depth == b.stk[i].depth &&
             a.stk[i].kind == b.stk[i].kind && a.stk[i].key == b.stk[i].key;
    check(st, "%s: the frames left on the stack (%zu, legacy %zu)", w, b.stk.size(), a.stk.size());
}

// ---- the worlds of the stored files ----------------------------------------------------------------------------------

struct Worlds {
    std::string fn;
    World5 L;                             // legacy (its renderers stay null unless build5 makes them)
    z::World<NB> N;
};

static void prepare_both(Worlds& w, const std::string& fn, std::string& lerr, std::string& nerr)
{
    w.fn = fn;
    lerr = rtest::capture_stderr([&] { prepare5(fn.c_str(), w.L); });
    nerr = rtest::capture_stderr([&] { z::prepare<NP>(fn.c_str(), w.N); });
}

// the roots of radius R: the legacy find_roots5 and the policy's roots
struct Roots { std::vector<M6> L, N; std::vector<double> Ld, Nd; };

static const Roots& roots_of(double R)
{
    static std::map<double, Roots> cache;
    auto it = cache.find(R);
    if (it != cache.end()) return it->second;
    Roots& r = cache[R];
    World5 w;
    find_roots5(R, w);
    r.L = w.roots;
    r.Ld = w.rootd;
    std::vector<NB::Elem> nr;
    NP::roots(R, nr, r.Nd);
    for (const NB::Elem& g : nr) { M6 x; memcpy(&x, &g, sizeof x); r.N.push_back(x); }
    return r;
}

// a legacy renderer of one side drawn from the roots as build5 draws them, recording for every polyline the root and
// the letter of the draw that made it (draw starts a new polyline)
static void legacy_records(Renderer5& a, const Letters5& L, const std::vector<M6>& roots, std::vector<int>& eroot,
                           std::vector<int>& eletter)
{
    for (size_t r = 0; r < roots.size(); r++)
        for (size_t j = 0; j < L.s.size(); j++) if (L.plus[j]) {
            size_t before = a.npoly();
            a.draw(roots[r], (int)j, (int)r);
            for (size_t k = before; k < a.npoly(); k++) { eroot.push_back((int)r); eletter.push_back((int)j); }
        }
}

struct Tally { size_t leaves = 0, forced = 0, skipped = 0, spikes = 0, left = 0; int complete = 0, capped = 0; };

// one renderer of each kind for one side, drawn from the roots as build5 draws them
static void render_case(const Worlds& w, bool plus, const Roots& rt, double acc, size_t cap, int maxdepth,
                        const std::string& what, Tally& T)
{
    const Side5& ls = plus ? w.L.plus : w.L.minus;
    const z::Side<NB>& ns = plus ? w.N.plus : w.N.minus;
    Renderer5 a(ls, w.L.L, acc, cap);
    z::Renderer<NB> b(ns, w.N.L, acc, cap);
    a.maxdepth = maxdepth;
    b.maxdepth = maxdepth;
    std::vector<int> eroot, eletter;
    legacy_records(a, w.L.L, rt.L, eroot, eletter);
    for (size_t r = 0; r < rt.N.size(); r++)
        for (size_t j = 0; j < w.N.L.s.size(); j++) if (w.N.L.plus[j]) b.draw(nm(rt.N[r]), (int)j, (int)r);
    compare_renderer(a, b, eroot, eletter, what);
    T.leaves += b.nLeaves; T.forced += b.nForced; T.skipped += b.nSkipped; T.spikes += b.nSpike; T.left += b.stk.size();
    (b.capped ? T.capped : T.complete)++;
}

// ---- relations among the letters, for loop_points ---------------------------------------------------------------------

// words w in the letters with s_{w_1} ... s_{w_k} = 1: s_j s_j^-1, and those of length 3 and 4 that the products of two
// letters show (s_a s_b = s_c, s_a s_b = s_c s_d); at most `count` of each length
static std::vector<std::vector<int>> relations(const z::Letters<NB>& L, size_t count)
{
    std::vector<std::vector<int>> rel2, rel3, rel4;
    size_t n = L.s.size();
    for (size_t j = 0; j < n && rel2.size() < count; j++) rel2.push_back({(int)j, L.inv[j]});
    std::map<NB::Key, std::vector<std::pair<int, int>>> prod;
    std::map<NB::Key, int> single;
    for (size_t c = 0; c < n; c++) single[NB::key(L.s[c])] = (int)c;
    for (size_t a = 0; a < n; a++)
        for (size_t b = 0; b < n; b++) {
            if ((int)b == L.inv[a]) continue;
            NB::Key k = NB::key(NB::mul(L.s[a], L.s[b]));
            auto s = single.find(k);
            if (s != single.end() && rel3.size() < count) rel3.push_back({(int)a, (int)b, L.inv[s->second]});
            std::vector<std::pair<int, int>>& v = prod[k];
            for (const auto& cd : v)
                if (rel4.size() < count && cd.first != (int)a) rel4.push_back({(int)a, (int)b, L.inv[cd.second], L.inv[cd.first]});
            v.push_back({(int)a, (int)b});
        }
    std::vector<std::vector<int>> all = rel2;
    all.insert(all.end(), rel3.begin(), rel3.end());
    all.insert(all.end(), rel4.begin(), rel4.end());
    return all;
}

// the loop of a relation made of the legacy pieces, as legacy/s3/zipcore.h's loop_points makes it
static std::vector<P5> legacy_loop(const Side5& sd, const Letters5& L, const M6& h0, const std::vector<int>& w, double acc)
{
    std::vector<P5> pts;
    M6 g = h0;
    for (int j : w) { arc_points5(sd, g, j, acc, pts); g = m6mul(g, L.s[j]); }
    if (!m6eq(g, h0)) { fprintf(stderr, "a relation does not close up\n"); exit(1); }
    if (pts.size() > 1) pts.pop_back();
    return pts;
}

static std::string word_text(const std::vector<int>& w)
{
    std::string s;
    for (int x : w) s += (s.empty() ? "" : " ") + std::to_string(x);
    return s;
}

// ---- views -------------------------------------------------------------------------------------------------------------

static void random_unit5(double p[5])
{
    double s = 0;
    for (int k = 0; k < 5; k++) { p[k] = rtest::gauss(); s += p[k]*p[k]; }
    for (int k = 0; k < 5; k++) p[k] /= std::sqrt(s);
}

// a random legacy view and the same new one: a frame with a random pole moved toward a direction, turns of R^4, a
// rotation of R^3, and zoom, clip and fog
static void random_view(View5& a, z::s4::View5& b)
{
    double p[5];
    random_unit5(p);
    orthonormal_frame(p, a.Q);
    if (rtest::rnd(0, 1)) move_pole5(a.Q, (int)rtest::rnd(0, 3), rtest::uniform(-1, 1));
    memset(a.U, 0, sizeof a.U);
    for (int i = 0; i < 4; i++) a.U[i][i] = 1;
    for (int t = (int)rtest::rnd(0, 3); t > 0; t--) turn4(a.U, (int)rtest::rnd(0, 3), (int)rtest::rnd(0, 3), rtest::uniform(-3.2, 3.2));
    rot3(rtest::uniform(-180, 180), rtest::uniform(-90, 90), a.rot);
    a.zoom = rtest::uniform(0.3, 4);
    a.clip = rtest::uniform(0.4, 6);
    a.hidfog = rtest::uniform(0.2, 4);
    static_assert(sizeof(View5) == sizeof(z::s4::View5), "the views have the same layout");
    memcpy(&b, &a, sizeof b);
}

// ----------------------------------------------------------------------------------------------------------------------

int main()
{
    double t_start = now();
    rh5_setup();                           // legacy: facets, colouring, moves, initial state, reflections
    z::imm::setup();
    printf("test_render_imm: src/render/ with the IMM policy, and src/view/s4.h, against legacy/s4, bit for bit\n");
    rtest::TempDir tmp;

    // ---- the substitution reader ----
    section("substitution reader (IMM dialect) vs load_subst5: stored files and variants");
    {
        long nfiles = 0, nok = 0;
        std::vector<std::string> sources;
        for (const char* f : FILES4) sources.push_back(repo_file("s4", f));
        for (const char* f : FILES3) sources.push_back(repo_file("s3", f));
        for (const std::string& src : sources) {
            std::string text = rtest::read_file(src);
            check(!text.empty(), "reading %s", src.c_str());
            for (const auto& v : rtest::subst_variants(text)) {
                std::string fn = tmp.file("variant.txt");
                rtest::write_file(fn, v.second);
                Subst5 a;
                z::Substitution b;
                bool ra = false, rb = false;
                std::string ea = rtest::capture_stderr([&] { ra = load_subst5(fn.c_str(), a); });
                std::string eb = rtest::capture_stderr([&] { rb = z::load_substitution(fn.c_str(), b, NP::dialect); });
                std::string what = src.substr(src.rfind('/') + 1) + ", " + v.first;
                check(ra == rb, "%s: returns %d, legacy %d", what.c_str(), rb, ra);
                check(ea == eb, "%s: the message \"%s\", legacy \"%s\"", what.c_str(), eb.c_str(), ea.c_str());
                check(a.tword == b.tword && a.sword == b.sword && a.up == b.up && a.down == b.down, "%s: the contents", what.c_str());
                nfiles++;
                nok += rb;
            }
        }
        {
            std::string fn = tmp.file("no such file.txt");
            Subst5 a;
            z::Substitution b;
            bool ra = true, rb = true;
            std::string ea = rtest::capture_stderr([&] { ra = load_subst5(fn.c_str(), a); });
            std::string eb = rtest::capture_stderr([&] { rb = z::load_substitution(fn.c_str(), b, NP::dialect); });
            check(!ra && !rb && ea == eb, "a missing file: %d \"%s\", legacy %d \"%s\"", rb, eb.c_str(), ra, ea.c_str());
        }
        note("(%ld files: 6 stored ones and their variants; %ld read as complete)", nfiles, nok);
    }

    // ---- prepare ----
    section("prepare<LegacyReadingPolicy> vs prepare5: letters, both sides, the message");
    std::vector<Worlds> W(3);
    for (int f = 0; f < 3; f++) {
        std::string le, ne;
        prepare_both(W[f], repo_file("s4", FILES4[f]), le, ne);
        compare_letters(W[f].L.L, W[f].N.L, FILES4[f]);
        compare_side(W[f].L.plus, W[f].N.plus, std::string(FILES4[f]) + " Z+");
        compare_side(W[f].L.minus, W[f].N.minus, std::string(FILES4[f]) + " Z-");
        check(le == ne, "%s: the message \"%s\", legacy \"%s\"", FILES4[f], ne.c_str(), le.c_str());
        // a note (nothing compared): the letters that both renderers are given here, read as the legacy reader reads
        // them, without facet 1 (LegacyReadingPolicy, above). They are not chz5's S_c, which has no two equal letters:
        // the corrected reader (imm::RenderPolicy) makes S_c itself, with 8 involutions and 44 base arcs
        // (test_reader_imm.cc)
        int self = 0, invol = 0, equal = 0;
        const z::Letters<NB>& NL = W[f].N.L;
        for (size_t j = 0; j < NL.s.size(); j++) {
            self += NL.inv[j] == (int)j;
            invol += NB::eq(NB::inv(NL.s[j]), NL.s[j]);
            for (size_t k = j + 1; k < NL.s.size(); k++) equal += NB::eq(NL.s[j], NL.s[k]);
        }
        note("%s", rtest::chomp(ne.substr(ne.rfind('/') + 1)).c_str());
        note("  (%zu letters, read as the legacy reader reads them, without facet 1: %d involutions, %d pairs of equal",
             NL.s.size(), invol, equal);
        note("   letters, inv[j] = j for %d; the true letters are tested in test_reader_imm)", self);
    }

    section("prepare's error paths (in child processes): exit status and message");
    {
        std::string text = rtest::read_file(repo_file("s4", FILES4[0]));
        std::vector<std::string> lines = rtest::split_lines(text);
        size_t iS = rtest::find_header(lines, "S"), iu = rtest::find_header(lines, "up"), id = rtest::find_header(lines, "down");
        std::vector<std::pair<std::string, std::string>> bad;
        bad.push_back({"a missing file", ""});
        {
            std::vector<std::string> v(lines.begin(), lines.begin() + (long)id);
            bad.push_back({"no down section", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[iS + 1] = "0  d=1.3  0 5 8 4 0 5";
            bad.push_back({"S not closed under inversion", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[iS + 1] = "0  d=1.3  0 16";
            bad.push_back({"a bad facet letter", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[iu + 1] += " 0";
            bad.push_back({"an up roof that fails", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            for (size_t i = id + 1; i < v.size(); i++) v[i] += " 5";
            bad.push_back({"down roofs that fail", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[rtest::find_header(lines, "t")] = "t 0 -1";
            bad.push_back({"a bad facet letter in t", rtest::join_lines(v)});
        }
        for (const auto& b : bad) {
            std::string fn = b.second.empty() ? tmp.file("missing.txt") : tmp.file("bad.txt");
            if (!b.second.empty()) rtest::write_file(fn, b.second);
            std::string le, ne;
            int ls = rtest::in_child([&] { World5 w; prepare5(fn.c_str(), w); }, le);
            int ns = rtest::in_child([&] { z::World<NB> w; z::prepare<NP>(fn.c_str(), w); }, ne);
            check(ls == ns && le == ne, "%s: %d \"%s\", legacy %d \"%s\"", b.first.c_str(), ns, ne.c_str(), ls, le.c_str());
            check(ls == 1, "%s: the legacy prepare5 exits with status 1 (%d)", b.first.c_str(), ls);
            std::string m = rtest::chomp(ne);
            if (m.size() > 70) m = m.substr(0, 30) + "..." + m.substr(m.size() - 37);
            note("(%s: exit %d, \"%s\")", b.first.c_str(), ns, m.c_str());
        }
    }

    // ---- make_side ----
    section("make_side vs make_side5: both sides of every file, from the same letters and roofs");
    for (int f = 0; f < 3; f++) {
        Subst5 D;
        z::Substitution ND;
        std::string fn = repo_file("s4", FILES4[f]);
        rtest::capture_stderr([&] { load_subst5(fn.c_str(), D); z::load_substitution(fn.c_str(), ND, NP::dialect); });
        M6 t = word_matrix(parse_fword(D.tword));
        NB::Elem nt = NP::word(ND.tword);
        check(same_bytes(t, nt), "%s: t", FILES4[f]);
        Side5 lp, lmi;
        z::Side<NB> np, nmi;
        make_side5(lp, "Z+", t, D.up, W[f].L.L);
        z::make_side(np, "Z+", nt, ND.up, W[f].N.L);
        make_side5(lmi, "Z-", m6inv(t), D.down, W[f].L.L);
        z::make_side(nmi, "Z-", NB::normal(NB::inv(nt)), ND.down, W[f].N.L);
        compare_side(lp, np, std::string(FILES4[f]) + " Z+");
        compare_side(lmi, nmi, std::string(FILES4[f]) + " Z-");
    }
    {
        Subst5 D;
        std::string fn = repo_file("s4", FILES4[1]);
        rtest::capture_stderr([&] { load_subst5(fn.c_str(), D); });
        std::vector<std::vector<int>> roofs = D.up;
        roofs[0].push_back(0);
        roofs[5].pop_back();
        M6 t = word_matrix(parse_fword(D.tword));
        std::string le, ne;
        int ls = rtest::in_child([&] { Side5 s; make_side5(s, "Z+", t, roofs, W[1].L.L); }, le);
        int ns = rtest::in_child([&] { z::Side<NB> s; z::make_side(s, "Z+", nm(t), roofs, W[1].N.L); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "failing roofs: %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        note("(failing roofs: \"%s\")", rtest::chomp(ne).c_str());
    }

    // ---- the renderer ----
    section("Renderer<ON1<5>> vs Renderer5: files x R x accuracies, both sides");
    {
        struct Case { int file; double R, acc; size_t cap; int maxdepth; };
        std::vector<Case> cases;
        const size_t CAP = 25000;             // leaves per side: keeps each case light
        for (int f = 0; f < 3; f++)
            for (double R : {0.0, 1.2, 1.8})
                for (double acc : {0.05, 0.02, 0.01}) cases.push_back({f, R, acc, CAP, 400});
        // complete runs (no cap) of moderate size, and forced leaves (a small depth limit)
        cases.push_back({0, 1.8, 0.05, 100000000, 400});
        cases.push_back({0, 0.0, 0.02, 100000000, 400});
        cases.push_back({2, 1.2, 0.03, 100000000, 400});
        cases.push_back({0, 0.0, 0.02, 100000000, 5});
        cases.push_back({1, 1.8, 0.05, 100000000, 4});
        Tally T;
        for (const Case& c : cases) {
            const Roots& rt = roots_of(c.R);
            for (int side = 0; side < 2; side++) {
                char what[200];
                snprintf(what, sizeof what, "%s R=%g acc=%g cap=%zu maxdepth=%d %s", FILES4[c.file], c.R, c.acc, c.cap,
                         c.maxdepth, side ? "Z-" : "Z+");
                render_case(W[c.file], side == 0, rt, c.acc, c.cap, c.maxdepth, what, T);
            }
        }
        note("(%zu cases x 2 sides: %d complete, %d capped; in all %zu leaves, %zu of them forced, %zu branches skipped,",
             cases.size(), T.complete, T.capped, T.leaves, T.forced, T.skipped);
        note(" %zu spikes, %zu frames left by capped draws)", T.spikes, T.left);
    }

    section("the roots (imm::RenderPolicy::roots vs find_roots5)");
    for (double R : {0.0, 1.2, 1.8}) {
        const Roots& rt = roots_of(R);
        check(same_vector_bytes(rt.L, rt.N), "R = %g: the roots (%zu, legacy %zu)", R, rt.N.size(), rt.L.size());
        check(same_vector_bytes(rt.Ld, rt.Nd), "R = %g: the distances", R);
        note("(R = %g: %zu roots)", R, rt.N.size());
    }

    // ---- build ----
    section("build<LegacyReadingPolicy> vs build5: the lines printed (time masked), the roots, the renderers");
    {
        struct BCase { int file; double R, acc; size_t cap; int which; };
        std::vector<BCase> bc = {{0, 1.8, 0.05, 20000000, 3}, {1, 0.0, 0.03, 20000000, 1}, {2, 1.8, 0.05, 30000, 2},
                                 {0, 0.0, 0.02, 5000, 3}, {0, 0.0, 0.05, 20000000, 0}};
        for (const BCase& c : bc) {
            Worlds w;
            std::string le, ne;
            prepare_both(w, repo_file("s4", FILES4[c.file]), le, ne);
            le = rtest::capture_stderr([&] { build5(w.L, c.R, c.acc, c.cap, c.which); });
            ne = rtest::capture_stderr([&] { z::build<NP>(w.N, c.R, c.acc, c.cap, c.which); });
            char what[200];
            snprintf(what, sizeof what, "%s R=%g acc=%g cap=%zu which=%d", FILES4[c.file], c.R, c.acc, c.cap, c.which);
            check(rtest::mask_times(le) == rtest::mask_times(ne), "%s: the lines\n%s    legacy\n%s", what, ne.c_str(), le.c_str());
            check(same_vector_bytes(w.L.roots, w.N.roots), "%s: the roots", what);
            check(same_vector_bytes(w.L.rootd, w.N.rootd), "%s: the distances of the roots", what);
            for (int side = 0; side < 2; side++) {
                const Renderer5& a = side ? *w.L.rn : *w.L.rp;
                const z::Renderer<NB>& b = side ? *w.N.rn : *w.N.rp;
                Renderer5 e(side ? w.L.minus : w.L.plus, w.L.L, c.acc, c.cap);
                std::vector<int> eroot, eletter;
                if (c.which & (side ? 2 : 1)) legacy_records(e, w.L.L, w.L.roots, eroot, eletter);
                compare_renderer(a, b, eroot, eletter, std::string(what) + (side ? " Z-" : " Z+"));
            }
            if (&c == &bc[3] || &c == &bc.back())
                for (const std::string& line : rtest::split_lines(rtest::mask_times(ne))) note("%s", line.c_str());
            delete w.L.rp; delete w.L.rn; delete w.N.rp; delete w.N.rn;
        }
        std::string le, ne;
        int ls = rtest::in_child([&] { build5(W[0].L, 5, 0.02, 20000000, 3); }, le);
        int ns = rtest::in_child([&] { z::World<NB> w = W[0].N; z::build<NP>(w, 5, 0.02, 20000000, 3); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "build at R = 5 (the guard): %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        note("(R = 5: \"%s\")", rtest::chomp(ne).c_str());
    }

    // ---- arc_points and loop_points ----
    section("arc_points vs arc_points5: branches at 1, at roots, at products of letters");
    {
        long narcs = 0;
        size_t npts = 0, nforced = 0;
        const Roots& rt = roots_of(1.8);
        for (int f = 0; f < 3; f++) {
            const Worlds& w = W[f];
            size_t n = w.N.L.s.size();
            for (int side = 0; side < 2; side++) {
                const Side5& ls = side ? w.L.minus : w.L.plus;
                const z::Side<NB>& ns = side ? w.N.minus : w.N.plus;
                std::vector<std::pair<M6, NB::Elem>> hs = {{m6id(), NB::identity()}};
                for (size_t r = 1; r < rt.L.size(); r += 10) hs.push_back({rt.L[r], nm(rt.N[r])});
                for (int k = 0; k < 3; k++) {              // a product of letters
                    M6 g = m6id();
                    NB::Elem ng = NB::identity();
                    for (int m = (int)rtest::rnd(1, 3); m > 0; m--) {
                        size_t x = (size_t)rtest::rnd(0, (int64_t)n - 1);
                        g = m6mul(g, w.L.L.s[x]);
                        ng = NB::mul(ng, w.N.L.s[x]);
                    }
                    hs.push_back({g, ng});
                }
                // Z- of the second file (t2), as the legacy reader reads it, does not converge as complete arcs: every
                // arc at the identity reaches the depth limit 400, with forced leaves and up to millions of points (in
                // arc_points5 too; the renderer copes, since it de-duplicates). Only its two smallest arcs are drawn:
                // they exercise the forced leaves.
                bool t2minus = f == 1 && side == 1;
                for (size_t hi = 0; hi < (t2minus ? 1 : hs.size()); hi++)
                    for (size_t j = 0; j < n; j += (hi == 0 ? 1 : 9))
                        for (double acc : {0.05, 0.02}) {
                            if (acc == 0.02 && (hi > 0 || j % 3)) continue;     // keep it light
                            if (t2minus && (acc != 0.05 || (j != 2 && j != 14))) continue;
                            std::vector<P5> a;
                            std::vector<z::SpherePoint<5>> b;
                            size_t fb = 0;
                            arc_points5(ls, hs[hi].first, (int)j, acc, a);
                            z::arc_points(ns, hs[hi].second, (int)j, acc, b, fb);
                            check(same_vector_bytes(a, b), "%s side %d h %zu letter %zu acc %g: %zu points, legacy %zu",
                                  FILES4[f], side, hi, j, acc, b.size(), a.size());
                            narcs++;
                            npts += b.size();
                            nforced += fb;
                            if (j % 7 == 0) {                // appended to points already there
                                arc_points5(ls, hs[hi].first, (int)((j + 1) % n), acc, a);
                                z::arc_points(ns, hs[hi].second, (int)((j + 1) % n), acc, b, fb);
                                check(same_vector_bytes(a, b), "%s: two arcs in one list", FILES4[f]);
                            }
                        }
            }
        }
        note("(%ld arcs, %zu points; the new counter of forced leaves says %zu: arc_points5 had none)", narcs, npts, nforced);
        note("(Z- of %s: only letters 2 and 14 at the identity, whose leaves reach the depth limit)", FILES4[1]);
    }

    section("a side with t = 1 (roofs s_j = s_j): arcs that reach the depth limit; the renderer meets its own branch");
    {
        // the roofs t^-1 s_j t = s_j pass the exact check, but the arcs never contract: every complete arc is split 400
        // times and ends in one forced leaf (arc_points5 does not count them); the renderer finds the child's key
        // already drawn (a frame of kind 1)
        const Worlds& w = W[0];
        size_t n = w.N.L.s.size();
        std::vector<std::vector<int>> roofs(n);
        for (size_t j = 0; j < n; j++) roofs[j] = {(int)j};
        Side5 ls;
        z::Side<NB> ns;
        make_side5(ls, "Z1", m6id(), roofs, w.L.L);
        z::make_side(ns, "Z1", NB::identity(), roofs, w.N.L);
        compare_side(ls, ns, "t = 1");
        size_t fb = 0, npts = 0;
        for (size_t j = 0; j < n; j++) {
            std::vector<P5> a;
            std::vector<z::SpherePoint<5>> b;
            arc_points5(ls, m6id(), (int)j, 0.05, a);
            z::arc_points(ns, NB::identity(), (int)j, 0.05, b, fb);
            check(same_vector_bytes(a, b), "t = 1, letter %zu: %zu points, legacy %zu", j, b.size(), a.size());
            npts += b.size();
        }
        check(fb > 0, "t = 1: %zu forced leaves", fb);
        Renderer5 ra(ls, w.L.L, 0.05, 1000);
        z::Renderer<NB> rb(ns, w.N.L, 0.05, 1000);
        std::vector<int> eroot, eletter;
        legacy_records(ra, w.L.L, std::vector<M6>{m6id()}, eroot, eletter);
        for (size_t j = 0; j < n; j++) if (w.N.L.plus[j]) rb.draw(NB::identity(), (int)j, 0);
        compare_renderer(ra, rb, eroot, eletter, "t = 1: the renderer");
        note("(%zu arcs, %zu points, %zu forced leaves; the renderer: %zu branches, %zu leaves)", n, npts, fb, rb.nVisited, rb.nLeaves);
    }

    section("the growth guard: branches at powers of t^-1, whose entries pass 2^40");
    {
        // h = (t^-1)^k: its repelling point is t^+ = V, where every arc begins, so the arcs h alpha_j stay coarse while
        // the entries of h grow with k. Those with the largest entry in [2^30, 2^42) are drawn: below 2^40 the first
        // branch splits; from 2^40 on every first branch is a forced leaf (the guard)
        const Worlds& w = W[0];
        size_t n = w.N.L.s.size();
        int kept = 0;
        size_t nforced = 0, nleaves = 0, npts = 0, nfa = 0;
        for (int side = 0; side < 2; side++) {
            const Side5& ls = side ? w.L.minus : w.L.plus;
            const z::Side<NB>& ns = side ? w.N.minus : w.N.plus;
            M6 lti = m6inv(ls.t), h = m6id();
            NB::Elem nti = NB::normal(NB::inv(ns.t)), nh = NB::identity();
            for (int k = 1; k <= 60; k++) {
                h = m6mul(h, lti);
                nh = NB::normal(NB::mul(nh, nti));
                int64_t mc = NB::max_coefficient(nh);
                if (mc >= (1LL << 42)) break;
                if (mc < (1LL << 30)) continue;
                check(same_bytes(h, nh), "side %d: (t^-1)^%d", side, k);
                kept++;
                Renderer5 a(ls, w.L.L, 0.05, 30000);
                z::Renderer<NB> b(ns, w.N.L, 0.05, 30000);
                std::vector<int> eroot, eletter;
                legacy_records(a, w.L.L, std::vector<M6>{h}, eroot, eletter);
                for (size_t jj = 0; jj < n; jj++) if (w.N.L.plus[jj]) b.draw(nh, (int)jj, 0);
                char what[100];
                snprintf(what, sizeof what, "side %d, (t^-1)^%d (largest entry 2^%.1f)", side, k, std::log2((double)mc));
                compare_renderer(a, b, eroot, eletter, what);
                nforced += b.nForced;
                nleaves += b.nLeaves;
                for (size_t jj = 0; jj < n; jj += 9) {
                    std::vector<P5> pa;
                    std::vector<z::SpherePoint<5>> pb;
                    size_t fb = 0;
                    arc_points5(ls, h, (int)jj, 0.05, pa);
                    z::arc_points(ns, nh, (int)jj, 0.05, pb, fb);
                    check(same_vector_bytes(pa, pb), "%s: the arc of letter %zu", what, jj);
                    npts += pb.size();
                    nfa += fb;
                }
            }
        }
        check(kept > 0 && nforced > 0 && nfa > 0, "the guard was reached (%d powers, %zu and %zu forced leaves)", kept, nforced, nfa);
        note("(%d powers of t^-1 on the two sides; the renderer: %zu leaves, %zu forced; arcs: %zu points, %zu forced)",
             kept, nleaves, nforced, npts, nfa);
    }

    section("loop_points vs the loop of the legacy arc_points5: relations of lengths 2, 3, 4");
    {
        long nloops = 0;
        size_t npts = 0, nrel[5] = {0, 0, 0, 0, 0};
        const Roots& rt = roots_of(1.8);
        for (int f = 0; f < 3; f++) {
            const Worlds& w = W[f];
            std::vector<std::vector<int>> rels = relations(w.N.L, 3);
            for (const auto& r : rels) nrel[r.size()]++;
            for (size_t i = 0; i < rels.size(); i++)
                for (int side = 0; side < 2; side++)
                    for (int hi = 0; hi < 2; hi++) {
                        if (f == 1 && side == 1) continue;   // Z- of t2: its arcs do not converge (above)
                        const Side5& ls = side ? w.L.minus : w.L.plus;
                        const z::Side<NB>& ns = side ? w.N.minus : w.N.plus;
                        M6 h = hi ? rt.L[1 + i % (rt.L.size() - 1)] : m6id();
                        NB::Elem nh = hi ? nm(rt.N[1 + i % (rt.N.size() - 1)]) : NB::identity();
                        size_t fb = 0;
                        std::vector<P5> a = legacy_loop(ls, w.L.L, h, rels[i], 0.05);
                        std::vector<z::SpherePoint<5>> b = z::loop_points(ns, w.N.L, nh, rels[i], 0.05, fb);
                        check(same_vector_bytes(a, b), "%s: the loop of %s (side %d, h %d): %zu points, legacy %zu",
                              FILES4[f], word_text(rels[i]).c_str(), side, hi, b.size(), a.size());
                        nloops++;
                        npts += b.size();
                    }
        }
        note("(%ld loops, %zu points; %zu, %zu and %zu relations of lengths 2, 3 and 4)", nloops, npts, nrel[2], nrel[3], nrel[4]);
        std::string le, ne;
        std::vector<int> notrel = {0, 1};
        if (W[0].N.L.inv[0] == 1) notrel = {0, 2};
        size_t fb = 0;
        int ls = rtest::in_child([&] { legacy_loop(W[0].L.plus, W[0].L.L, m6id(), notrel, 0.05); }, le);
        int ns = rtest::in_child([&] { z::loop_points(W[0].N.plus, W[0].N.L, NB::identity(), notrel, 0.05, fb); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "not a relation: %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
    }

    // ---- the view ----
    section("the view: PI, orthonormal_frame, rot3, turn4, move_pole5, eye5, init_view5");
    {
        check(!memcmp(&PI, &z::s4::PI, sizeof PI), "PI");
        for (int t = 0; t < 20000; t++) {           // poles: random unit vectors, axes, near-axes, not unit, zero
            double p[5];
            int kind = (int)rtest::rnd(0, 4);
            if (kind <= 1) random_unit5(p);
            else if (kind == 2) { memset(p, 0, sizeof p); p[rtest::rnd(0, 4)] = rtest::rnd(0, 1) ? 1 : -1; }
            else if (kind == 3) { memset(p, 0, sizeof p); int k = (int)rtest::rnd(0, 4); p[k] = 1; p[(k + 1) % 5] = rtest::uniform(-1e-3, 1e-3); }
            else for (double& x : p) x = rtest::uniform(-3, 3);
            double a[5][5], b[5][5];
            memset(a, 0x5a, sizeof a); memset(b, 0x5a, sizeof b);
            orthonormal_frame(p, a);
            z::s4::orthonormal_frame(p, b);
            check(!memcmp(a, b, sizeof a), "orthonormal_frame %d", t);
        }
        for (int t = 0; t < 20000; t++) {
            double yaw = t < 50 ? 90.0*(t % 5) - 180 : rtest::uniform(-400, 400), pitch = t < 50 ? 45.0*(t / 5 % 5) - 90 : rtest::uniform(-200, 200);
            double a[9], b[9];
            rot3(yaw, pitch, a);
            z::s4::rot3(yaw, pitch, b);
            check(!memcmp(a, b, sizeof a), "rot3(%g, %g)", yaw, pitch);
        }
        for (int t = 0; t < 20000; t++) {
            double a[4][4], b[4][4];
            for (auto& row : a) for (double& x : row) x = rtest::gauss();
            memcpy(b, a, sizeof b);
            int i = (int)rtest::rnd(0, 3), j = (int)rtest::rnd(0, 3);
            double th = rtest::uniform(-7, 7);
            turn4(a, i, j, th);
            z::s4::turn4(b, i, j, th);
            check(!memcmp(a, b, sizeof a), "turn4(%d, %d, %g)", i, j, th);
        }
        for (int t = 0; t < 20000; t++) {
            double a[5][5], b[5][5];
            for (auto& row : a) for (double& x : row) x = rtest::gauss();
            memcpy(b, a, sizeof b);
            int k = (int)rtest::rnd(0, 4);
            double th = rtest::uniform(-7, 7);
            move_pole5(a, k, th);
            z::s4::move_pole5(b, k, th);
            check(!memcmp(a, b, sizeof a), "move_pole5(%d, %g)", k, th);
        }
        long seen = 0, clipped = 0;
        for (int t = 0; t < 2000; t++) {
            View5 va;
            z::s4::View5 vb;
            random_view(va, vb);
            for (int q = 0; q < 50; q++) {           // points of S^4 (as floats), near the pole, and others
                double p[5];
                int kind = (int)rtest::rnd(0, 5);
                if (kind <= 2) random_unit5(p);
                else if (kind == 3) { for (int k = 0; k < 5; k++) p[k] = va.Q[4][k] + rtest::uniform(-1e-4, 1e-4); }
                else if (kind == 4) { for (int k = 0; k < 5; k++) p[k] = va.Q[4][k]; }
                else for (double& x : p) x = rtest::uniform(-2, 2);
                float x[5] = {(float)p[0], (float)p[1], (float)p[2], (float)p[3], (float)p[4]};
                double ea[3] = {7, 7, 7}, eb[3] = {7, 7, 7}, ha = 7, hb = 7;
                bool ra = eye5(va, x, ea, ha), rb = z::s4::eye5(vb, x, eb, hb);
                check(ra == rb && !memcmp(ea, eb, sizeof ea) && !memcmp(&ha, &hb, sizeof ha), "eye5 (view %d, point %d)", t, q);
                seen += ra;
                clipped += !ra;
            }
        }
        note("(eye5: 100000 points in 2000 views, %ld shown and %ld outside the clip ball or at the pole)", seen, clipped);
        for (int f = 0; f < 3; f++)
            for (int t = 0; t < 200; t++) {
                double yaw = t ? rtest::uniform(-180, 180) : 0, pitch = t ? rtest::uniform(-90, 90) : 15;
                View5 a;
                z::s4::View5 b;
                memset(&a, 0x33, sizeof a); memset(&b, 0x33, sizeof b);
                a.zoom = b.zoom = 1; a.clip = b.clip = 3; a.hidfog = b.hidfog = 1.5;
                if (t) init_view5(W[f].L, a, yaw, pitch); else init_view5(W[f].L, a);
                if (t) z::s4::init_view5(W[f].N, b, yaw, pitch); else z::s4::init_view5(W[f].N, b);
                check(same_bytes(a, b), "init_view5 (%s, %g, %g)", FILES4[f], yaw, pitch);
            }
    }

    // ---- the painter ----
    section("write_png5 vs write_png5: the PNG files of real renderers in random views");
    {
        // the renderers: the base arcs of every file, and R = 1.8 of the first, at accuracy 0.05, legacy and new
        struct Pair { Worlds w; const char* what; };
        std::vector<Pair> P(4);
        for (int k = 0; k < 4; k++) {
            int f = k < 3 ? k : 0;
            double R = k < 3 ? 0 : 1.8;
            std::string le, ne;
            prepare_both(P[k].w, repo_file("s4", FILES4[f]), le, ne);
            rtest::capture_stderr([&] { build5(P[k].w.L, R, 0.05, 20000000, 3); z::build<NP>(P[k].w.N, R, 0.05, 20000000, 3); });
            P[k].what = k < 3 ? FILES4[f] : "subst5_2.30_4.50_t1.txt R=1.8";
        }
        long nfiles = 0, nbytes = 0, ndiffer_from_white = 0;
        for (int t = 0; t < 60; t++) {
            int k = t < 50 ? (int)rtest::rnd(0, 2) : 3;
            Worlds& w = P[k].w;
            View5 va;
            z::s4::View5 vb;
            if (t % 3 == 0) random_view(va, vb);
            else {                                   // the views of zipper5: init_view5 and the two turns
                double yaw = rtest::uniform(-180, 180), pitch = rtest::uniform(-90, 90);
                init_view5(w.L, va, yaw, pitch);
                z::s4::init_view5(w.N, vb, yaw, pitch);
                va.clip = vb.clip = rtest::uniform(0.5, 5);
                va.zoom = vb.zoom = rtest::uniform(0.5, 3);
                va.hidfog = vb.hidfog = rtest::uniform(0.3, 3);
                double t1 = rtest::uniform(-180, 180), t2 = rtest::uniform(-180, 180);
                turn4(va.U, 2, 3, t1*PI/180);
                turn4(va.U, 0, 3, t2*PI/180);
                z::s4::turn4(vb.U, 2, 3, t1*z::s4::PI/180);
                z::s4::turn4(vb.U, 0, 3, t2*z::s4::PI/180);
            }
            check(same_bytes(va, vb), "the views of scene %d", t);
            int which = (int)rtest::rnd(1, 3), size = (int)rtest::rnd(8, t < 50 ? 260 : 400), thick = (int)rtest::rnd(0, 3);
            std::vector<Layer5> la;
            std::vector<z::s4::Layer5> lb;
            for (int side = 0; side < 2; side++) if (which & (side + 1)) {
                double col[3] = {rtest::uniform(0, 1), rtest::uniform(0, 1), rtest::uniform(0, 1)};
                int prio = (int)rtest::rnd(0, 2);
                la.push_back({side ? w.L.rn : w.L.rp, {col[0], col[1], col[2]}, prio});
                lb.push_back({side ? w.N.rn : w.N.rp, {col[0], col[1], col[2]}, prio});
            }
            if (rtest::rnd(0, 3) == 0) { la.push_back(la[0]); lb.push_back(lb[0]); la.back().prio = lb.back().prio = 5; }
            std::vector<P5> ma = {w.L.plus.V, w.L.minus.V};
            std::vector<z::SpherePoint<5>> mb = {w.N.plus.V, w.N.minus.V};
            if (t % 4 == 0) { ma.clear(); mb.clear(); }
            std::string fa = tmp.file("legacy.png"), fb = tmp.file("new.png");
            bool ra = write_png5(fa.c_str(), size, la, va, ma, thick);
            bool rb = z::s4::write_png5(fb.c_str(), size, lb, vb, mb, thick);
            std::string da = rtest::read_file(fa), db = rtest::read_file(fb);
            check(ra == rb && da == db, "scene %d (%s, size %d, thick %d, which %d): %zu bytes, legacy %zu", t, P[k].what, size,
                  thick, which, db.size(), da.size());
            nfiles++;
            nbytes += (long)db.size();
            // a picture with lines in it is not the size of an empty one
            std::vector<z::s4::Layer5> none;
            std::string fe = tmp.file("empty.png");
            z::s4::write_png5(fe.c_str(), size, none, vb, {}, thick);
            ndiffer_from_white += rtest::read_file(fe) != db;
            unlink(fa.c_str()); unlink(fb.c_str()); unlink(fe.c_str());
        }
        note("(%ld pictures, %ld bytes, %ld of them not blank)", nfiles, nbytes, ndiffer_from_white);
        {
            std::vector<Layer5> la = {{P[0].w.L.rp, {0.05, 0.15, 0.85}, 1}};
            std::vector<z::s4::Layer5> lb = {{P[0].w.N.rp, {0.05, 0.15, 0.85}, 1}};
            View5 va;
            z::s4::View5 vb;
            init_view5(P[0].w.L, va);
            z::s4::init_view5(P[0].w.N, vb);
            std::string bad = tmp.file("no such folder/x.png");
            bool ra = write_png5(bad.c_str(), 64, la, va, {}, 0), rb = z::s4::write_png5(bad.c_str(), 64, lb, vb, {}, 0);
            check(!ra && !rb, "a file that cannot be written: %d, legacy %d", rb, ra);
        }
        for (Pair& p : P) { delete p.w.L.rp; delete p.w.L.rn; delete p.w.N.rp; delete p.w.N.rn; }
    }

    rtest::end_section();
    printf("test_render_imm: %ld comparisons with the legacy code, %ld differ (%.1f s)\n", rtest::all_n, rtest::all_bad,
           now() - t_start);
    return rtest::all_bad ? 1 : 0;
}
