// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_render_cs: the renderer of src/render/ on the backend PU21, with the render policy of the Cartwright-Steger
// example (examples/cartwright_steger_render.h), agrees BIT FOR BIT with the legacy S^3 renderer of legacy/s3/zipcore.h
// (included here, with csgroup.h, in the global namespace):
//   - the substitution reader (render/substitution.h) in the Cartwright-Steger dialect against load_subst, on every
//     stored file of legacy/s3 and legacy/s4 and on many variants of them, and on a missing file: the return value,
//     the message and the contents;
//   - prepare<cs::RenderPolicy> against prepare: the letters, both sides, the message; and its error paths, in child
//     processes. No letter is its own inverse, so plus (j <= inv j) is the legacy plus (j < inv j);
//   - make_side against make_side, both sides, with each file's letters, and a roof that fails;
//   - Renderer<PU21> against Renderer: every stored file, R = 0, 2.554, 3.327, accuracies 0.05, 0.02, 0.01, both
//     sides, drawn from the roots as build draws them: the points (as floats), the polyline starts, the counters
//     (visited, skipped, leaves, spikes, forced, depth, cap, open), the fingerprints of the branches drawn, the frames
//     left by a capped draw; the root and letter of every polyline against the draws that made them (the legacy
//     Renderer does not record them). A leaf cap keeps each case light; some cases are complete, some capped, and
//     two use a small depth limit, so that leaves are forced;
//   - build<cs::RenderPolicy> against build: the lines it prints (time masked), the roots, the renderers;
//   - arc_points and loop_points against the legacy ones: branches at the identity, at roots and at products of
//     letters; relations of lengths 2 to 4 found among the letters; a word that is not a relation (in a child); and a
//     side with t = 1, whose arcs reach the depth limit (forced leaves); branches at powers of t^-1, which reach the
//     growth guard (forced leaves).
// cs::RenderPolicy turns on the look-ahead through one-letter roofs, which the legacy renderer does not
// have; the test uses the policy without it (NoLookThroughPolicy, below), so that it compares the renderers.
// Built with the flags of the programs (-O2), under which the results must agree. ZIPPERS_REPO is the repository.

#include "zipcore.h"                     // legacy/s3: csgroup.h and zipcore.h

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "geometry/pu21/pu21.h"
#include "render/arcs.h"
#include "render/renderer.h"
#include "render/substitution.h"
#include "render/world.h"

#include "render_test_support.h"

#include <map>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
typedef z::PU21 NB;
// The test's own render policy: cs::RenderPolicy without the look-ahead through one-letter roofs, which its
// sides turns on (render/renderer.h) and the legacy renderer does not have; so that the comparisons below
// are with the legacy code (the look-ahead is tested in test_lookthrough_cs.cc). No tips either, as cs::RenderPolicy.
struct NoLookThroughPolicy : z::cs::RenderPolicy {
    static void sides(const z::Letters<NB>&, z::Side<NB>&, z::Side<NB>&) {}       // neither: the legacy drawing
};
typedef NoLookThroughPolicy NP;
using rtest::check;
using rtest::note;
using rtest::section;
using rtest::same_bytes;
using rtest::same_vector_bytes;

// the legacy and the new elements have the same layout
static z::M3 nm(const M3& a) { z::M3 b; memcpy(&b, &a, sizeof b); return b; }

static const char* const FILES3[3] = {"subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t2.txt", "subst_4.00_9.50_t3.txt"};
static const char* const FILES4[3] = {"subst5_2.30_4.50_t1.txt", "subst5_2.30_4.50_t2.txt", "subst5_2.30_4.50_t3.txt"};
static std::string repo_file(const char* sub, const char* f) { return std::string(ZIPPERS_REPO) + "/legacy/" + sub + "/" + f; }

// ---- comparisons -----------------------------------------------------------------------------------------------------

static void compare_letters(const Letters& a, const z::Letters<NB>& b, const std::string& what)
{
    check(same_vector_bytes(a.s, b.s), "%s: the letters", what.c_str());
    check(a.inv == b.inv, "%s: inv", what.c_str());
    check(a.plus == b.plus, "%s: plus", what.c_str());
    check(a.canon == b.canon, "%s: canon", what.c_str());
}

static void compare_side(const Side& a, const z::Side<NB>& b, const std::string& what)
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

static void compare_renderer(const Renderer& a, const z::Renderer<NB>& b, const std::vector<int>& eroot,
                             const std::vector<int>& eletter, const std::string& what)
{
    const char* w = what.c_str();
    check(same_vector_bytes(a.pts, b.pts), "%s: the points (%zu floats, legacy %zu)", w, b.pts.size(), a.pts.size());
    check(a.starts == b.starts, "%s: the polyline starts (%zu, legacy %zu)", w, b.starts.size(), a.starts.size());
    check(b.polyroot == eroot, "%s: the root of every polyline", w);
    check(b.polyletter == eletter, "%s: the letter of every polyline", w);
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
    World L;                              // legacy (its renderers stay null: the tests make their own)
    z::World<NB> N;
};

static void prepare_both(Worlds& w, const std::string& fn, std::string& lerr, std::string& nerr)
{
    w.fn = fn;
    Options opt;
    opt.subst = fn;
    lerr = rtest::capture_stderr([&] { prepare(opt, w.L); });
    nerr = rtest::capture_stderr([&] { z::prepare<NP>(fn.c_str(), w.N); });
}

// the roots of radius R: the legacy find_roots, the policy's roots, and the legacy roots_info (for the distances)
struct Roots { std::vector<M3> L; std::vector<z::M3> N; std::vector<double> Nd; std::vector<RootInfo> info; };

static const Roots& roots_of(double R)
{
    static std::map<double, Roots> cache;
    auto it = cache.find(R);
    if (it != cache.end()) return it->second;
    Roots& r = cache[R];
    find_roots(R, r.L);
    NP::roots(R, r.N, r.Nd);
    r.info = roots_info(R);
    return r;
}

// a legacy renderer of one side drawn from the roots as build draws them, recording for every polyline the root and
// the letter of the draw that made it (the legacy Renderer does not record them; draw starts a new polyline)
static void legacy_records(Renderer& a, const Letters& L, const std::vector<M3>& roots, std::vector<int>& eroot,
                           std::vector<int>& eletter)
{
    for (size_t r = 0; r < roots.size(); r++)
        for (size_t j = 0; j < L.s.size(); j++) if (L.plus[j]) {
            size_t before = a.npoly();
            a.draw(roots[r], j);
            for (size_t k = before; k < a.npoly(); k++) { eroot.push_back((int)r); eletter.push_back((int)j); }
        }
}

// one renderer of each kind for one side, drawn from the roots as build draws them
struct Tally { size_t leaves = 0, forced = 0, skipped = 0, spikes = 0, left = 0; int complete = 0, capped = 0; };

static void render_case(const Worlds& w, bool plus, const Roots& rt, double acc, size_t cap, int maxdepth,
                        const std::string& what, Tally& T)
{
    const Side& ls = plus ? w.L.plus : w.L.minus;
    const z::Side<NB>& ns = plus ? w.N.plus : w.N.minus;
    Renderer a(ls, w.L.L, acc, cap);
    z::Renderer<NB> b(ns, w.N.L, acc, cap);
    a.maxdepth = maxdepth;
    b.maxdepth = maxdepth;
    std::vector<int> eroot, eletter;
    legacy_records(a, w.L.L, rt.L, eroot, eletter);
    for (size_t r = 0; r < rt.N.size(); r++)
        for (size_t j = 0; j < w.N.L.s.size(); j++) if (w.N.L.plus[j]) b.draw(rt.N[r], (int)j, (int)r);
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
    for (size_t c = 0; c < n; c++) single[NB::key(NB::normal(L.s[c]))] = (int)c;
    for (size_t a = 0; a < n; a++)
        for (size_t b = 0; b < n; b++) {
            if ((int)b == L.inv[a]) continue;
            NB::Key k = NB::key(NB::normal(NB::mul(L.s[a], L.s[b])));
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

static std::string word_text(const std::vector<int>& w)
{
    std::string s;
    for (int x : w) s += (s.empty() ? "" : " ") + std::to_string(x);
    return s;
}

// ----------------------------------------------------------------------------------------------------------------------

int main()
{
    double t_start = now();
    setup();                               // legacy: F', u', v', b', j, a1, a2, a3
    z::cs::setup();
    printf("test_render_cs: src/render/ with the Cartwright-Steger policy against legacy/s3, bit for bit\n");
    rtest::TempDir tmp;

    // ---- the substitution reader ----
    section("substitution reader (CS dialect) vs load_subst: stored files and variants");
    {
        long nfiles = 0, nok = 0;
        std::vector<std::string> sources;
        for (const char* f : FILES3) sources.push_back(repo_file("s3", f));
        for (const char* f : FILES4) sources.push_back(repo_file("s4", f));
        for (const std::string& src : sources) {
            std::string text = rtest::read_file(src);
            check(!text.empty(), "reading %s", src.c_str());
            for (const auto& v : rtest::subst_variants(text)) {
                std::string fn = tmp.file("variant.txt");
                rtest::write_file(fn, v.second);
                Subst a;
                z::Substitution b;
                bool ra = false, rb = false;
                std::string ea = rtest::capture_stderr([&] { ra = load_subst(fn.c_str(), a); });
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
            Subst a;
            z::Substitution b;
            bool ra = true, rb = true;
            std::string ea = rtest::capture_stderr([&] { ra = load_subst(fn.c_str(), a); });
            std::string eb = rtest::capture_stderr([&] { rb = z::load_substitution(fn.c_str(), b, NP::dialect); });
            check(!ra && !rb && ea == eb, "a missing file: %d \"%s\", legacy %d \"%s\"", rb, eb.c_str(), ra, ea.c_str());
            note("(a missing file: \"%s\")", rtest::chomp(eb).c_str());
        }
        note("(%ld files: 6 stored ones and their variants; %ld read as complete)", nfiles, nok);
    }

    // ---- prepare ----
    section("prepare<cs::RenderPolicy> vs prepare: letters, both sides, the message");
    std::vector<Worlds> W(3);
    for (int f = 0; f < 3; f++) {
        std::string le, ne;
        prepare_both(W[f], repo_file("s3", FILES3[f]), le, ne);
        compare_letters(W[f].L.L, W[f].N.L, FILES3[f]);
        compare_side(W[f].L.plus, W[f].N.plus, std::string(FILES3[f]) + " Z+");
        compare_side(W[f].L.minus, W[f].N.minus, std::string(FILES3[f]) + " Z-");
        check(le == ne, "%s: the message \"%s\", legacy \"%s\"", FILES3[f], ne.c_str(), le.c_str());
        note("%s", rtest::chomp(ne).c_str());
    }

    section("no letter of S is its own inverse (so plus j <= inv j is the legacy j < inv j)");
    for (int f = 0; f < 3; f++) {
        int inv = 0;
        for (size_t j = 0; j < W[f].N.L.s.size(); j++) {
            check(W[f].N.L.inv[j] != (int)j, "%s: letter %zu is its own inverse", FILES3[f], j);
            check(W[f].N.L.plus[j] == ((int)j < W[f].N.L.inv[j]), "%s: plus of letter %zu", FILES3[f], j);
            inv += W[f].N.L.inv[j] == (int)j;
        }
        note("(%s: %zu letters, %d their own inverse)", FILES3[f], W[f].N.L.s.size(), inv);
    }

    section("prepare's error paths (in child processes): exit status and message");
    {
        std::string text = rtest::read_file(repo_file("s3", FILES3[2]));
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
            v[iS + 1] = "0  d=3.3  f2=-2  a1 a1 a1 a1 a1 a1";
            bad.push_back({"S not closed under inversion", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[iS + 1] = "0  d=3.3  f2=-2  a1 b2";
            bad.push_back({"a bad letter", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[iu + 1] += " 0";
            bad.push_back({"an up roof that fails", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            size_t im = rtest::find_header(lines, "member");
            for (size_t i = id + 1; i < im; i++) v[i] += " 5";
            bad.push_back({"down roofs that fail", rtest::join_lines(v)});
        }
        {
            std::vector<std::string> v = lines;
            v[rtest::find_header(lines, "t")] = "t a1 A4";
            bad.push_back({"a bad letter in t", rtest::join_lines(v)});
        }
        for (const auto& b : bad) {
            std::string fn = b.second.empty() ? tmp.file("missing.txt") : tmp.file("bad.txt");
            if (!b.second.empty()) rtest::write_file(fn, b.second);
            std::string le, ne;
            int ls = rtest::in_child([&] { World w; Options o; o.subst = fn; prepare(o, w); }, le);
            int ns = rtest::in_child([&] { z::World<NB> w; z::prepare<NP>(fn.c_str(), w); }, ne);
            check(ls == ns && le == ne, "%s: %d \"%s\", legacy %d \"%s\"", b.first.c_str(), ns, ne.c_str(), ls, le.c_str());
            check(ls == 1, "%s: the legacy prepare exits with status 1 (%d)", b.first.c_str(), ls);
            std::string m = rtest::chomp(ne);
            if (m.size() > 70) m = m.substr(0, 30) + "..." + m.substr(m.size() - 37);
            note("(%s: exit %d, \"%s\")", b.first.c_str(), ns, m.c_str());
        }
    }

    // ---- make_side ----
    section("make_side vs make_side: both sides of every file, from the same letters and roofs");
    for (int f = 0; f < 3; f++) {
        Subst D;
        z::Substitution ND;
        std::string fn = repo_file("s3", FILES3[f]);
        rtest::capture_stderr([&] { load_subst(fn.c_str(), D); z::load_substitution(fn.c_str(), ND, NP::dialect); });
        M3 t = parse_word(D.tword);
        z::M3 nt = NP::word(ND.tword);
        check(same_bytes(t, nt), "%s: t", FILES3[f]);
        Side lp, lmi;
        z::Side<NB> np, nmi;
        make_side(lp, "Z+", t, D.up, W[f].L.L);
        z::make_side(np, "Z+", nt, ND.up, W[f].N.L);
        make_side(lmi, "Z-", mnormal(minv(t)), D.down, W[f].L.L);
        z::make_side(nmi, "Z-", NB::normal(NB::inv(nt)), ND.down, W[f].N.L);
        compare_side(lp, np, std::string(FILES3[f]) + " Z+");
        compare_side(lmi, nmi, std::string(FILES3[f]) + " Z-");
    }
    {
        // a roof that fails: the message and the exit status
        Subst D;
        std::string fn = repo_file("s3", FILES3[2]);
        rtest::capture_stderr([&] { load_subst(fn.c_str(), D); });
        std::vector<std::vector<int>> roofs = D.up;
        roofs[0].push_back(0);
        roofs[3].pop_back();
        M3 t = parse_word(D.tword);
        std::string le, ne;
        int ls = rtest::in_child([&] { Side s; make_side(s, "Z+", t, roofs, W[2].L.L); }, le);
        int ns = rtest::in_child([&] { z::Side<NB> s; z::make_side(s, "Z+", nm(t), roofs, W[2].N.L); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "failing roofs: %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        note("(failing roofs: \"%s\")", rtest::chomp(ne).c_str());
    }

    // ---- the renderer ----
    section("Renderer<PU21> vs Renderer: files x R x accuracies, both sides");
    {
        struct Case { int file; double R, acc; size_t cap; int maxdepth; };
        std::vector<Case> cases;
        const size_t CAP = 25000;             // leaves per side: keeps each case light
        for (int f = 0; f < 3; f++)
            for (double R : {0.0, 2.554, 3.327})
                for (double acc : {0.05, 0.02, 0.01}) cases.push_back({f, R, acc, CAP, 400});
        // complete runs (no cap) of moderate size, and forced leaves (a small depth limit)
        cases.push_back({2, 2.554, 0.02, 100000000, 400});
        cases.push_back({2, 3.327, 0.05, 100000000, 400});
        cases.push_back({0, 0.0, 0.05, 100000000, 400});
        cases.push_back({2, 0.0, 0.01, 100000000, 4});
        cases.push_back({0, 2.554, 0.02, 100000000, 6});
        Tally T;
        for (const Case& c : cases) {
            const Roots& rt = roots_of(c.R);
            for (int side = 0; side < 2; side++) {
                char what[200];
                snprintf(what, sizeof what, "%s R=%g acc=%g cap=%zu maxdepth=%d %s", FILES3[c.file], c.R, c.acc, c.cap,
                         c.maxdepth, side ? "Z-" : "Z+");
                render_case(W[c.file], side == 0, rt, c.acc, c.cap, c.maxdepth, what, T);
            }
        }
        note("(%zu cases x 2 sides: %d complete, %d capped; in all %zu leaves, %zu of them forced, %zu branches skipped,",
             cases.size(), T.complete, T.capped, T.leaves, T.forced, T.skipped);
        note(" %zu spikes, %zu frames left by capped draws)", T.spikes, T.left);
    }

    section("the roots (cs::RenderPolicy::roots vs find_roots, roots_info)");
    for (double R : {0.0, 2.554, 3.327}) {
        const Roots& rt = roots_of(R);
        check(same_vector_bytes(rt.L, rt.N), "R = %g: the roots (%zu, legacy %zu)", R, rt.N.size(), rt.L.size());
        bool d = rt.Nd.size() == rt.info.size();
        for (size_t i = 0; d && i < rt.info.size(); i++) d = !memcmp(&rt.Nd[i], &rt.info[i].d, sizeof(double));
        check(d, "R = %g: the distances", R);
        note("(R = %g: %zu roots)", R, rt.N.size());
    }

    // ---- build ----
    section("build<cs::RenderPolicy> vs build: the lines printed (time masked), the roots, the renderers");
    {
        struct BCase { int file; double R, acc; size_t cap; int which; };
        std::vector<BCase> bc = {{2, 2.554, 0.05, 8000000, 3}, {0, 0.0, 0.05, 8000000, 1}, {1, 3.327, 0.05, 30000, 2},
                                 {2, 0.0, 0.02, 5000, 3}, {2, 0.0, 0.05, 8000000, 0}};
        for (const BCase& c : bc) {
            Worlds w;
            std::string le, ne;
            prepare_both(w, repo_file("s3", FILES3[c.file]), le, ne);
            Options opt;
            opt.subst = w.fn;
            opt.R = c.R; opt.acc = c.acc; opt.cap = c.cap; opt.which = c.which;
            le = rtest::capture_stderr([&] { build(opt, w.L); });
            ne = rtest::capture_stderr([&] { z::build<NP>(w.N, c.R, c.acc, c.cap, c.which); });
            char what[200];
            snprintf(what, sizeof what, "%s R=%g acc=%g cap=%zu which=%d", FILES3[c.file], c.R, c.acc, c.cap, c.which);
            check(rtest::mask_times(le) == rtest::mask_times(ne), "%s: the lines\n%s    legacy\n%s", what, ne.c_str(), le.c_str());
            check(same_vector_bytes(w.L.roots, w.N.roots), "%s: the roots", what);
            for (int side = 0; side < 2; side++) {
                const Renderer& a = side ? *w.L.rn : *w.L.rp;
                const z::Renderer<NB>& b = side ? *w.N.rn : *w.N.rp;
                // the records the polylines should have: a legacy renderer of this side drawn as build draws it
                Renderer e(side ? w.L.minus : w.L.plus, w.L.L, c.acc, c.cap);
                std::vector<int> eroot, eletter;
                if (c.which & (side ? 2 : 1)) legacy_records(e, w.L.L, w.L.roots, eroot, eletter);
                compare_renderer(a, b, eroot, eletter, std::string(what) + (side ? " Z-" : " Z+"));
            }
            if (&c == &bc[3] || &c == &bc.back()) {
                std::string m = rtest::mask_times(ne);
                for (const std::string& line : rtest::split_lines(m)) note("%s", line.c_str());
            }
            delete w.L.rp; delete w.L.rn; delete w.N.rp; delete w.N.rn;
        }
        std::string le, ne;
        int ls = rtest::in_child([&] { Options o; o.R = 40; build(o, W[2].L); }, le);
        int ns = rtest::in_child([&] { z::World<NB> w = W[2].N; z::build<NP>(w, 40, 0.01, 8000000, 3); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "build at R = 40 (the guard): %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        note("(R = 40: \"%s\")", rtest::chomp(ne).c_str());
    }

    // ---- arc_points and loop_points ----
    section("arc_points vs arc_points: branches at 1, at roots, at products of letters");
    {
        long narcs = 0;
        size_t npts = 0;
        const Roots& rt = roots_of(2.554);
        for (int f = 0; f < 3; f++) {
            const Worlds& w = W[f];
            size_t n = w.N.L.s.size();
            for (int side = 0; side < 2; side++) {
                const Side& ls = side ? w.L.minus : w.L.plus;
                const z::Side<NB>& ns = side ? w.N.minus : w.N.plus;
                std::vector<std::pair<M3, z::M3>> hs = {{mid(), NB::identity()}};
                for (size_t r = 1; r < rt.L.size(); r += 6) hs.push_back({rt.L[r], rt.N[r]});
                for (int k = 0; k < 3; k++) {              // a product of letters (not normalised)
                    M3 g = mid();
                    z::M3 ng = NB::identity();
                    for (int m = (int)rtest::rnd(1, 3); m > 0; m--) {
                        size_t x = (size_t)rtest::rnd(0, (int64_t)n - 1);
                        g = mmul(g, w.L.L.s[x]);
                        ng = NB::mul(ng, w.N.L.s[x]);
                    }
                    hs.push_back({g, ng});
                }
                for (size_t hi = 0; hi < hs.size(); hi++)
                    for (size_t j = 0; j < n; j += (hi == 0 ? (f == 0 ? 3 : 1) : 7))
                        for (double acc : {0.05, 0.02}) {
                            if (acc == 0.02 && (f == 0 || hi > 0 || j % 3)) continue;     // keep it light
                            std::vector<P4> a;
                            std::vector<z::SpherePoint<4>> b;
                            size_t fa = 7, fb = 7;
                            arc_points(ls, hs[hi].first, (int)j, acc, a, fa);
                            z::arc_points(ns, hs[hi].second, (int)j, acc, b, fb);
                            check(same_vector_bytes(a, b) && fa == fb, "%s side %d h %zu letter %zu acc %g: %zu points, legacy %zu",
                                  FILES3[f], side, hi, j, acc, b.size(), a.size());
                            narcs++;
                            npts += b.size();
                            if (j % 7 == 0) {                // appended to points already there
                                arc_points(ls, hs[hi].first, (int)((j + 1) % n), acc, a, fa);
                                z::arc_points(ns, hs[hi].second, (int)((j + 1) % n), acc, b, fb);
                                check(same_vector_bytes(a, b) && fa == fb, "%s: two arcs in one list", FILES3[f]);
                            }
                        }
            }
        }
        note("(%ld arcs, %zu points)", narcs, npts);
    }

    section("a side with t = 1 (roofs s_j = s_j): arcs that reach the depth limit; the renderer meets its own branch");
    {
        // the roofs t^-1 s_j t = s_j pass the exact check, but the arcs never contract: every complete arc is split 400
        // times and ends in one forced leaf; the renderer finds the child's key already drawn (a frame of kind 1)
        const Worlds& w = W[2];
        size_t n = w.N.L.s.size();
        std::vector<std::vector<int>> roofs(n);
        for (size_t j = 0; j < n; j++) roofs[j] = {(int)j};
        Side ls;
        z::Side<NB> ns;
        make_side(ls, "Z1", mid(), roofs, w.L.L);
        z::make_side(ns, "Z1", NB::identity(), roofs, w.N.L);
        compare_side(ls, ns, "t = 1");
        size_t fa = 0, fb = 0, npts = 0;
        for (size_t j = 0; j < n; j++) {
            std::vector<P4> a;
            std::vector<z::SpherePoint<4>> b;
            arc_points(ls, mid(), (int)j, 0.05, a, fa);
            z::arc_points(ns, NB::identity(), (int)j, 0.05, b, fb);
            check(same_vector_bytes(a, b), "t = 1, letter %zu: %zu points, legacy %zu", j, b.size(), a.size());
            npts += b.size();
        }
        check(fa == fb && fb > 0, "t = 1: forced leaves %zu, legacy %zu", fb, fa);
        size_t la = 0, lb = 0;
        std::vector<int> rel = {0, w.N.L.inv[0]};
        std::vector<P4> a = loop_points(ls, w.L.L, mid(), rel, 0.05, la);
        std::vector<z::SpherePoint<4>> b = z::loop_points(ns, w.N.L, NB::identity(), rel, 0.05, lb);
        check(same_vector_bytes(a, b) && la == lb, "t = 1: a loop (forced %zu, legacy %zu)", lb, la);
        Renderer ra(ls, w.L.L, 0.05, 1000);
        z::Renderer<NB> rb(ns, w.N.L, 0.05, 1000);
        std::vector<int> eroot, eletter;
        legacy_records(ra, w.L.L, std::vector<M3>{mid()}, eroot, eletter);
        for (size_t j = 0; j < n; j++) if (w.N.L.plus[j]) rb.draw(NB::identity(), (int)j, 0);
        compare_renderer(ra, rb, eroot, eletter, "t = 1: the renderer");
        note("(%zu arcs, %zu points, %zu forced leaves; the renderer: %zu branches, %zu leaves)", n, npts, fb, rb.nVisited, rb.nLeaves);
    }

    section("the growth guard: branches at powers of t^-1, whose coefficients pass 2^40");
    {
        // h = (t^-1)^k: its repelling point is t^+ = V, where every arc begins, so the arcs h alpha_j stay coarse while
        // the coefficients of h grow with k. Those with the largest coefficient in [2^30, 2^42) are drawn: below 2^40
        // the first branch splits; from 2^40 on every first branch is a forced leaf (the guard)
        const Worlds& w = W[2];
        size_t n = w.N.L.s.size();
        int kept = 0;
        size_t nforced = 0, nleaves = 0, npts = 0, nfa = 0;
        for (int side = 0; side < 2; side++) {
            const Side& ls = side ? w.L.minus : w.L.plus;
            const z::Side<NB>& ns = side ? w.N.minus : w.N.plus;
            M3 lti = mnormal(minv(ls.t)), h = mid();
            z::M3 nti = NB::normal(NB::inv(ns.t)), nh = NB::identity();
            for (int k = 1; k <= 60; k++) {
                h = mnormal(mmul(h, lti));
                nh = NB::normal(NB::mul(nh, nti));
                int64_t mc = NB::max_coefficient(nh);
                if (mc >= (1LL << 42)) break;
                if (mc < (1LL << 30)) continue;
                check(same_bytes(h, nh), "side %d: (t^-1)^%d", side, k);
                kept++;
                Renderer a(ls, w.L.L, 0.05, 30000);
                z::Renderer<NB> b(ns, w.N.L, 0.05, 30000);
                std::vector<int> eroot, eletter;
                legacy_records(a, w.L.L, std::vector<M3>{h}, eroot, eletter);
                for (size_t jj = 0; jj < n; jj++) if (w.N.L.plus[jj]) b.draw(nh, (int)jj, 0);
                char what[100];
                snprintf(what, sizeof what, "side %d, (t^-1)^%d (largest coefficient 2^%.1f)", side, k, std::log2((double)mc));
                compare_renderer(a, b, eroot, eletter, what);
                nforced += b.nForced;
                nleaves += b.nLeaves;
                for (size_t jj = 0; jj < n; jj += 6) {
                    std::vector<P4> pa;
                    std::vector<z::SpherePoint<4>> pb;
                    size_t fa = 0, fb = 0;
                    arc_points(ls, h, (int)jj, 0.05, pa, fa);
                    z::arc_points(ns, nh, (int)jj, 0.05, pb, fb);
                    check(same_vector_bytes(pa, pb) && fa == fb, "%s: the arc of letter %zu (forced %zu, legacy %zu)", what, jj, fb, fa);
                    npts += pb.size();
                    nfa += fb;
                }
            }
        }
        check(kept > 0 && nforced > 0 && nfa > 0, "the guard was reached (%d powers, %zu and %zu forced leaves)", kept, nforced, nfa);
        note("(%d powers of t^-1 on the two sides; the renderer: %zu leaves, %zu forced; arcs: %zu points, %zu forced)",
             kept, nleaves, nforced, npts, nfa);
    }

    section("loop_points vs loop_points: relations of lengths 2, 3, 4; a word that is not one");
    {
        long nloops = 0;
        size_t npts = 0, nrel[5] = {0, 0, 0, 0, 0};
        const Roots& rt = roots_of(2.554);
        for (int f = 0; f < 3; f++) {
            const Worlds& w = W[f];
            std::vector<std::vector<int>> rels = relations(w.N.L, 3);
            for (const auto& r : rels) nrel[r.size()]++;
            for (size_t i = 0; i < rels.size(); i++)
                for (int side = 0; side < 2; side++)
                    for (int hi = 0; hi < 2; hi++) {
                        if (f == 0 && hi == 0) continue;      // t1's long arcs at 1: keep it light
                        const Side& ls = side ? w.L.minus : w.L.plus;
                        const z::Side<NB>& ns = side ? w.N.minus : w.N.plus;
                        M3 h = hi ? rt.L[1 + i % (rt.L.size() - 1)] : mid();
                        z::M3 nh = hi ? rt.N[1 + i % (rt.N.size() - 1)] : NB::identity();
                        size_t fa = 3, fb = 3;
                        std::vector<P4> a = loop_points(ls, w.L.L, h, rels[i], 0.05, fa);
                        std::vector<z::SpherePoint<4>> b = z::loop_points(ns, w.N.L, nh, rels[i], 0.05, fb);
                        check(same_vector_bytes(a, b) && fa == fb, "%s: the loop of %s (side %d, h %d): %zu points, legacy %zu",
                              FILES3[f], word_text(rels[i]).c_str(), side, hi, b.size(), a.size());
                        nloops++;
                        npts += b.size();
                    }
        }
        note("(%ld loops, %zu points; %zu, %zu and %zu relations of lengths 2, 3 and 4)", nloops, npts, nrel[2], nrel[3], nrel[4]);
        std::string le, ne;
        std::vector<int> notrel = {0, 1};
        if (W[2].N.L.inv[0] == 1) notrel = {0, 2};
        size_t fa = 0, fb = 0;
        int ls = rtest::in_child([&] { loop_points(W[2].L.plus, W[2].L.L, mid(), notrel, 0.05, fa); }, le);
        int ns = rtest::in_child([&] { z::loop_points(W[2].N.plus, W[2].N.L, NB::identity(), notrel, 0.05, fb); }, ne);
        check(ls == 1 && ns == 1 && le == ne, "not a relation: %d \"%s\", legacy %d \"%s\"", ns, ne.c_str(), ls, le.c_str());
        note("(not a relation: \"%s\")", rtest::chomp(ne).c_str());
    }

    rtest::end_section();
    printf("test_render_cs: %ld comparisons with the legacy code, %ld differ (%.1f s)\n", rtest::all_n, rtest::all_bad,
           now() - t_start);
    return rtest::all_bad ? 1 : 0;
}
