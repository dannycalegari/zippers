// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_roof_choice: the options of the presentation in the kernel search (search/kernel_search.h,
// search/roof_choice.h, the policies' candidates for t, and chz and chz5 with --tlist, --tcand, --roofs, --noinv).
//
//   1. THE WORDS, on a toy alphabet (a, A, b, B and two involution letters z, y): the inverse word, freely reduced, and
//      THE FACT of search/roof_choice.h: a self-inverse word is of odd length with an involution letter in the middle (u z u^-1);
//      "a A", "a b B A" (even: freely trivial) and "a z a" are not; the roofs as the renderer uses them; the junctions,
//      counted by hand on a toy substitution, and the level of its first cancellation, by hand; the growth rate of a
//      substitution whose Perron-Frobenius root is known. An involution letter keeps its own roof even when it is not
//      self-inverse (renderer_roofs, on a toy and on the stored S^4 file, against the roofs make_side makes).
//   2. THE JUNCTIONS OF THE STORED FILES, pinned (58 of 160 and 42 of 146 in S^3 _t3, 96 of 468 and
//      248 of 423 in S^4 _t1, 225 of them at an involution letter), and counted again by an independent loop; and the
//      renderer's roofs against renderer_roofs of the file's (no involution roof of the S^4 file is self-inverse).
//   3. THE CANDIDATES FOR t (the policies' t_candidates, kernel_search.h's ties): for S^3 (phi = 1, 3) and S^4 (phi = 1,
//      2) the list is every element the example allows (counted by a loop of its own here), sorted by d(o, t o) with
//      ties in the ball's order; candidate 0 is pick_t's; the ties (by the float, and exactly) are counted again here;
//      S^4 has 48 tied candidates for phi = 1 and S^3 4 (phi = 1) and 2 (phi = 3).
//   4. THE PROGRAMS, on light cases (S^4: chz5 kernel 2.3 4.2 3, K-store 116,369; S^3: chz kernel 4 9 1, 344,553; a few
//      seconds each), each run in a folder of its own:
//      - no option: the file is the stored one, byte for byte (the reference of s4_chz5_kernel_steep, and
//        legacy/s3/subst_4.00_9.00_t1.txt), and its name the default;
//      - --tlist: the list, and NO file written (no search: its own folder stays empty);
//      - --tcand I: the t of the file is candidate I of --tlist, and candidate I of the library's list; the
//        certificate holds; the file has a name of its own;
//      - --tcand I with a STEEP t (S^4, TPHI = 2: chz5 kernel 2.3 4.2 3 2 --tcand 1): the t of the file is candidate 1
//        of the phi = 2 list (its phi, by h, is 2; not candidate 1 of the phi = 1 list), the certificate keeps pick_t's
//        t, the file is subst5_2.30_4.20_t2_i1.txt, the line says "candidate 1 of the 462 with phi = 2", and the 80 + 80
//        roofs of the file are exact for the steep t;
//      - --roofs (S^4 with --jexcess 0, S^3 with --jexcess 1): run twice, the same file and the same lines (but the
//        times); every roof of the file passes an exact check written here; every involution roof is self-inverse,
//        as many as the program says AND all of them (8 and 8 in S^4); the roof of x^-1 is the inverse
//        word of the roof of x; the cancelling junctions of theta and theta^2, up and down, counted here, are no more
//        than the default file's; and in S^3 with --jexcess 1 the junction pass takes longer roofs for fewer cancelling
//        junctions (junctions before length: 20 -> 0, total length 174 -> 186 up and 166 -> 180 down);
//      - the options line (S^3, chz kernel 4 9 1 --roofs): the third line of the file is "# options: ...", the third
//        line of data/s3/subst_4.00_9.00_t1_r0.txt, and on Apple's arm64 the whole file is that file, byte for byte;
//      - --noinv (S^4): S_c without its 8 involution letters; the certificate fails (status 2), and the reason is a
//        fact checked here: the facet counts mod 2 of the 72 other letters span a space that contains no involution
//        letter's (W -> (Z/2)^16 is a homomorphism, so no involution is in their subgroup);
//      - bad options are refused with status 1.
// Uses src/ and the programs (ZIPPERS_CHZ, ZIPPERS_CHZ5: their paths; ZIPPERS_REPO: the repository).

#include "examples/cartwright_steger.h"
#include "examples/cartwright_steger_render.h"
#include "examples/cartwright_steger_search.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "examples/imm_m5_search.h"
#include "render/substitution.h"
#include "render/world.h"
#include "search/kernel_search.h"
#include "search/roof_choice.h"

#include "render_test_support.h"

#include <dirent.h>
#include <sys/wait.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace z = zippers;
using rtest::check;
using rtest::note;
using rtest::section;
typedef std::vector<int> W;

// ---- 1. the words --------------------------------------------------------------------------------------------------

// the toy alphabet: 0 a, 1 A, 2 b, 3 B, 4 z, 5 y (z and y involution letters)
static const std::vector<int> TINV = {1, 0, 3, 2, 4, 5};

static void words_cases()
{
    section("1. the words: inverse words, freely reduced, self-inverse (THE FACT), junctions and levels by hand");
    check(z::inverse_word({0, 2, 4}, TINV) == W({4, 3, 1}), "the inverse word of a b z is z B A");
    check(z::freely_reduced({0, 2, 1}, TINV) && !z::freely_reduced({0, 2, 3, 1}, TINV), "freely reduced: a b A yes, a b B A no");
    struct { W w; bool si; const char* what; } cases[] = {
        {{4}, true, "z"}, {{0, 4, 1}, true, "a z A"}, {{0, 2, 4, 3, 1}, true, "a b z B A"}, {{0, 5, 1}, true, "a y A"},
        {{0, 1}, false, "a A (even: freely trivial)"}, {{0, 2, 3, 1}, false, "a b B A (even: freely trivial)"},
        {{}, false, "the empty word (even)"}, {{4, 4}, false, "z z (even)"}, {{0, 4, 0}, false, "a z a"},
        {{0, 2, 1}, false, "a b A (the middle letter not an involution)"}, {{0, 4, 5, 1}, false, "a z y A (even)"},
        {{0, 2, 4, 1, 3}, false, "a b z A B"}, {{1, 4, 0}, true, "A z a"},
    };
    for (auto& c : cases) check(z::self_inverse(c.w, TINV) == c.si, "self_inverse(%s) = %d", c.what, (int)c.si);
    // every u z u^-1 with u reduced and not ending in z is self-inverse, and of odd length
    for (int k = 0; k < 200; k++) {
        W u;
        int len = (int)rtest::rnd(0, 6);
        for (int i = 0; i < len; i++) u.push_back((int)rtest::rnd(0, 5));
        int zz = (int)rtest::rnd(4, 5);
        W w = u;
        w.push_back(zz);
        W ui = z::inverse_word(u, TINV);
        w.insert(w.end(), ui.begin(), ui.end());
        check(z::self_inverse(w, TINV) && w.size() % 2 == 1, "u z u^-1 is self-inverse (case %d)", k);
    }
    // the roofs as the renderer uses them: a non-representative gets the inverse word of its representative's
    std::vector<W> roof = {{2, 4}, {5}, {0}, {1, 1}, {0, 4, 1}, {5}};
    std::vector<W> th = z::renderer_roofs(roof, TINV);
    check(th[1] == W({4, 3}) && th[3] == W({1}) && th[0] == roof[0] && th[4] == roof[4] && th[5] == roof[5],
          "renderer_roofs: A gets z B, the inverse word of b z; B gets A; a, z and y keep theirs");
    // an involution letter is its own representative (the renderer's j <= inv[j]) and keeps its OWN roof, whether or not
    // that roof is self-inverse: z -> a z b and y -> y a are not (a z b is not its inverse word B z A, y a is even), and
    // must not be replaced by their inverse words
    std::vector<W> roof2 = {{2, 4}, {5}, {0}, {1, 1}, {0, 4, 2}, {5, 0}};
    std::vector<W> th2 = z::renderer_roofs(roof2, TINV);
    check(!z::self_inverse(roof2[4], TINV) && !z::self_inverse(roof2[5], TINV) && th2[4] == W({0, 4, 2}) &&
          th2[5] == W({5, 0}) && th2[1] == W({4, 3}) && th2[3] == W({1}),
          "renderer_roofs: an involution letter whose roof is not self-inverse (z -> a z b, y -> y a) keeps it");
    // THREE TOYS, by hand. Write lt(x) for the last letter of theta(x) and if(x) for the inverse of its first letter; a
    // junction (y, w) cancels when lt(y) = if(w), and maps to the junction (lt(y), first of theta(w)) one level down.
    // (i) theta: a -> b z, A -> z B, b -> a, B -> A, z -> a z A, y -> y. lt: a z, A B, b a, B A, z A, y y; if: a B, A z,
    // b A, B a, z A, y y. Level 1, the junctions of the roofs: (b,z) (z,B) (a,z) (z,A): a/A, A/a, z/A, A/z: none
    // cancels. theta^2: a -> a a z A, A -> a z A A, b -> b z, B -> z B, z -> b z a z A z B: 3 + 3 + 1 + 1 + 6 = 14
    // junctions, (a,a) z/B, (a,z) z/A, (z,A) A/z, (A,A) B/z, (b,z) a/A, (z,B) A/a, (z,a) A/B, (A,z) B/A: none cancels.
    // The levels: 1 {(b,z) (z,B) (a,z) (z,A)} -> 2 {(a,a) (A,A) (z,a) (A,z)} -> 3 {(z,b) (B,z) (A,b) (B,a)}, and (z,b)
    // cancels: lt(z) = A = if(b). So the first cancellation is at level 3 (theta^4(a) is not freely reduced).
    std::vector<W> T = {{2, 4}, {4, 3}, {0}, {1}, {0, 4, 1}, {5}};
    z::JunctionCount jc = z::count_junctions(T, TINV);
    check(jc.junctions1 == 4 && jc.cancel1 == 0 && jc.junctions2 == 14 && jc.cancel2 == 0,
          "toy (i): junctions of theta %ld (4), cancelling %ld (0); of theta^2 %ld (14), cancelling %ld (0)",
          jc.junctions1, jc.cancel1, jc.junctions2, jc.cancel2);
    z::TurnClosure tc = z::turn_closure(T, TINV);
    check(tc.first_level == 3 && tc.cancelling >= 1, "toy (i): the first cancellation at level %d (3 by hand), %zu cancel",
          tc.first_level, tc.cancelling);
    // (ii) Fibonacci: a -> a b, A -> B A, b -> a, B -> A, z -> z, y -> y. Junctions (a,b) (B,A) -> (b,a) (A,B) ->
    // (a,a) (A,A) -> (b,a) (A,B): lt a b, A A, b a, B A; if a A, A b, b A, B a: (a,b) b/A, (B,A) A/b, (b,a) a/A,
    // (A,B) A/a, (a,a) b/A, (A,A) A/b: none cancels at any level (an automorphism of F_2 with a train track). theta^2:
    // a -> a b a, A -> A B A, b -> a b, B -> B A: 6 junctions
    std::vector<W> F = {{0, 2}, {3, 1}, {0}, {1}, {4}, {5}};
    z::JunctionCount jf = z::count_junctions(F, TINV);
    z::TurnClosure tf = z::turn_closure(F, TINV);
    check(jf.junctions1 == 2 && jf.cancel1 == 0 && jf.junctions2 == 6 && jf.cancel2 == 0 && tf.cancelling == 0 &&
          tf.first_level == 0 && tf.turns == 6, "toy (ii): junctions %ld (2) and %ld (6), none cancels at any level (%zu"
          " turns, 6 by hand; %zu cancel)", jf.junctions1, jf.junctions2, tf.turns, tf.cancelling);
    // (iii) a -> b z, A -> z B, b -> a b, B -> B A, z -> B y b, y -> y: lt a z, A B, b b, B A, z b, y y; if a B, A z,
    // b A, B b, z b, y y. Level 1: (b,z) b/b CANCELS, (z,B) b/b CANCELS, (a,b) z/A, (B,A) A/z, (B,y) A/y, (y,b) y/A:
    // 6 junctions, 2 cancel, neither at an involution letter (the cancelling letter is b); first level 1
    std::vector<W> C = {{2, 4}, {4, 3}, {0, 2}, {3, 1}, {3, 5, 2}, {5}};
    z::JunctionCount jcc = z::count_junctions(C, TINV);
    z::TurnClosure tcc = z::turn_closure(C, TINV);
    check(jcc.junctions1 == 6 && jcc.cancel1 == 2 && jcc.cancel1_inv == 0 && tcc.first_level == 1,
          "toy (iii): %ld junctions (6), %ld cancel (2), %ld at an involution (0), first level %d (1)", jcc.junctions1,
          jcc.cancel1, jcc.cancel1_inv, tcc.first_level);
    // the growth rate of (ii): the golden ratio
    double g = z::growth_rate(F);
    check(std::fabs(g - (1 + std::sqrt(5.0))/2) < 1e-9, "growth rate of a -> a b, b -> a: %.12f (the golden ratio)", g);
}

// ---- 2. the junctions of the stored files --------------------------------------------------------------------------

// the junctions counted by a loop of this test's own: (junctions, cancelling) of theta and theta^2 of the roofs th, and
// their total length
struct Counts { long j1 = 0, c1 = 0, c1inv = 0, j2 = 0, c2 = 0, len = 0; };
static Counts my_counts(const std::vector<W>& th, const std::vector<int>& inv)
{
    Counts c;
    for (size_t s = 0; s < th.size(); s++) {
        c.len += (long)th[s].size();
        for (size_t i = 1; i < th[s].size(); i++) {
            int y = th[s][i - 1], w = th[s][i];
            c.j1++;
            int a = th[y][th[y].size() - 1], b = th[w][0];
            if (inv[b] == a) { c.c1++; if (inv[a] == a) c.c1inv++; }
        }
        W w2;
        for (int x : th[s]) for (int y : th[x]) w2.push_back(y);
        for (size_t i = 1; i < w2.size(); i++) {
            c.j2++;
            int y = w2[i - 1], w = w2[i];
            if (inv[th[w][0]] == th[y][th[y].size() - 1]) c.c2++;
        }
    }
    return c;
}

// the file of a search: t's word, the letters' words, the up and down roofs (as written)
template <class P>
static bool read_subst(const std::string& fn, z::Substitution& D)
{
    bool ok = false;
    rtest::capture_stderr([&] { ok = z::load_substitution(fn.c_str(), D, P::dialect); });
    return ok;
}

// want_ninv: the involution letters of the file (8 in S^4's _t1, none in S^3: Pi is torsion free)
template <class P>
static void stored_file_counts(const std::string& file, const char* name, long want[2][3], int want_ninv)
{
    typedef typename P::B B;
    z::World<B> W_;
    rtest::capture_stderr([&] { z::prepare<P>(file.c_str(), W_); });
    z::Substitution D;
    if (!read_subst<P>(file, D)) { check(false, "%s: the stored file can be read", name); return; }
    int ninv = 0;
    for (size_t j = 0; j < W_.L.inv.size(); j++) ninv += W_.L.inv[j] == (int)j;
    check(ninv == want_ninv, "%s: %d involution letters (%d expected)", name, ninv, want_ninv);
    for (int side = 0; side < 2; side++) {
        const std::vector<W>& th = side == 0 ? W_.plus.roof : W_.minus.roof;
        // the roofs the renderer makes (make_side, through prepare) are renderer_roofs of the file's: an involution letter
        // keeps its own roof, and every other letter that is not a representative gets the inverse word of its
        // representative's. The stored S^4 file has involution roofs that are NOT self-inverse (0 of 8 on
        // each side), where the inverse word of the roof of j is not the roof of j, so a helper that treated an involution
        // letter as a non-representative would differ here
        int nsi = 0;
        for (size_t j = 0; j < th.size(); j++) nsi += W_.L.inv[j] == (int)j && z::self_inverse(th[j], W_.L.inv);
        check(z::renderer_roofs(side == 0 ? D.up : D.down, W_.L.inv) == th,
              "%s %s: renderer_roofs of the file's roofs are the roofs the renderer makes (make_side)", name,
              side ? "Z-" : "Z+");
        check(nsi == 0, "%s %s: %d of the %d involution roofs are self-inverse (pinned: none)", name,
              side ? "Z-" : "Z+", nsi, ninv);
        z::JunctionCount jc = z::count_junctions(th, W_.L.inv);
        Counts mc = my_counts(th, W_.L.inv);
        check(jc.junctions1 == want[side][0] && jc.cancel1 == want[side][1] && jc.cancel1_inv == want[side][2],
              "%s %s: junctions of theta %ld, cancelling %ld, at an involution %ld (pinned: %ld, %ld, %ld)", name,
              side ? "Z-" : "Z+", jc.junctions1, jc.cancel1, jc.cancel1_inv, want[side][0], want[side][1], want[side][2]);
        check(jc.junctions1 == mc.j1 && jc.cancel1 == mc.c1 && jc.cancel1_inv == mc.c1inv && jc.junctions2 == mc.j2 &&
              jc.cancel2 == mc.c2, "%s %s: the library's counts are this test's (theta^2: %ld cancelling of %ld)", name,
              side ? "Z-" : "Z+", mc.c2, mc.j2);
    }
}

// ---- 3. the candidates for t ---------------------------------------------------------------------------------------

template <class Policy, class Qualifies>
static void candidate_cases(const Policy& p, int ph, const char* name, Qualifies qualifies, int want_ties)
{
    std::vector<int> c = p.t_candidates(ph);
    std::vector<int> mine;
    for (size_t i = 0; i < p.ball.size(); i++) if (qualifies((int)i)) mine.push_back((int)i);
    std::sort(mine.begin(), mine.end(), [&](int a, int b) {
        if (p.ball.dist[a] != p.ball.dist[b]) return p.ball.dist[a] < p.ball.dist[b];
        return a < b;                                         // ties in the ball's order
    });
    check(c == mine, "%s phi = %d: the candidates are every element allowed, by d(o, t o), ties in the ball's order "
          "(%zu, here %zu)", name, ph, c.size(), mine.size());
    check(!c.empty() && c[0] == p.pick_t(ph), "%s phi = %d: candidate 0 is pick_t's t (%d, %d)", name, ph,
          c.empty() ? -1 : c[0], p.pick_t(ph));
    int tf, te;
    std::vector<int> c2 = z::kernel_t_candidates(p, ph, tf, te);
    int ties = 0;
    for (int i : mine) if (p.ball.dist[i] == p.ball.dist[mine[0]]) ties++;
    check(c2 == c && tf == ties && te == ties && ties == want_ties, "%s phi = %d: %d tie with the shortest (by the float %d,"
          " exactly %d; expected %d)", name, ph, ties, tf, te, want_ties);
    // exact ties are float ties, and a float tie that is not exact would be counted apart: none among the first 64
    for (size_t k = 0; k < c.size() && k < 64; k++)
        for (size_t l = k + 1; l < c.size() && l < 64; l++) {
            double dk = p.ball.dist[c[k]], dl = p.ball.dist[c[l]];
            if (p.same_length(c[k], c[l]) != (dk == dl))
                check(false, "%s phi = %d: candidates %zu and %zu: same_length %d, float d %.9g %.9g", name, ph, k, l,
                      (int)p.same_length(c[k], c[l]), dk, dl);
        }
}

// ---- 4. the programs -----------------------------------------------------------------------------------------------

// run cmd in the folder dir; its exit status and stdout
static int run_in(const std::string& dir, const std::string& cmd, std::string& out)
{
    out.clear();
    FILE* p = popen(("cd '" + dir + "' && " + cmd + " 2>/dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// the names of the entries of a folder (not . and ..), sorted: what a program run there has written
static std::vector<std::string> dir_entries(const std::string& path)
{
    std::vector<std::string> v;
    if (DIR* d = opendir(path.c_str())) {
        while (struct dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n != "." && n != "..") v.push_back(n);
        }
        closedir(d);
    }
    std::sort(v.begin(), v.end());
    return v;
}

// the text with every number followed by "s" (a time: "0.25s", "(4.69s)") masked
static std::string mask_seconds(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        size_t j = i;
        while (j < s.size() && (isdigit((unsigned char)s[j]) || s[j] == '.')) j++;
        if (j > i && j < s.size() && s[j] == 's' && (j + 1 == s.size() || !isalpha((unsigned char)s[j + 1]))) {
            out += "<t>s";
            i = j + 1;
        } else if (j > i) { out += s.substr(i, j - i); i = j; }
        else { out += s[i]; i++; }
    }
    return out;
}

// every roof of the file is a word for its element, checked here: the products of the letters' elements, against
// t^-1 s t (up) and t s t^-1 (down), with the letters and t read by the example's word reader
template <class P>
static void exact_check(const z::Substitution& D, const char* name)
{
    typedef typename P::B B;
    typedef typename B::Elem E;
    std::vector<E> s;
    for (const std::string& w : D.sword) s.push_back(P::word(w));
    E t = P::word(D.tword), ti = B::inv(t);
    int bad = 0, n = 0;
    for (size_t j = 0; j < s.size(); j++)
        for (int side = 0; side < 2; side++) {
            const W& r = side == 0 ? D.up[j] : D.down[j];
            E pr = B::identity();
            for (int x : r) pr = B::mul(pr, s[x]);
            E want = side == 0 ? B::mul(B::mul(ti, s[j]), t) : B::mul(B::mul(t, s[j]), ti);
            n++;
            if (r.empty() || !B::same(pr, want)) bad++;
        }
    check(bad == 0, "%s: every roof of the file passes the exact check (%d of %d fail)", name, bad, n);
}

// the inverse letters, from the elements (the last match, as the renderer's prepare)
template <class P>
static std::vector<int> inverses(const z::Substitution& D)
{
    typedef typename P::B B;
    std::vector<typename B::Elem> s;
    for (const std::string& w : D.sword) s.push_back(P::word(w));
    std::vector<int> inv(s.size(), -1);
    for (size_t j = 0; j < s.size(); j++)
        for (size_t k = 0; k < s.size(); k++) if (B::eq(B::normal(B::inv(s[j])), s[k])) inv[j] = (int)k;
    return inv;
}

// the roofs as the renderer uses them, built here
static std::vector<W> drawn_roofs(const std::vector<W>& r, const std::vector<int>& inv)
{
    std::vector<W> th(r.size());
    for (size_t j = 0; j < r.size(); j++) {
        if ((int)j <= inv[j]) th[j] = r[j];
        else for (size_t i = r[inv[j]].size(); i-- > 0;) th[j].push_back(inv[r[inv[j]][i]]);
    }
    return th;
}

// the number in the line of out that contains key, just after it ("... key N ..."), or -1
static long number_after(const std::string& out, const std::string& key)
{
    size_t p = out.find(key);
    if (p == std::string::npos) return -1;
    p += key.size();
    while (p < out.size() && out[p] == ' ') p++;
    long v = 0;
    bool any = false;
    while (p < out.size() && isdigit((unsigned char)out[p])) { v = 10*v + (out[p] - '0'); p++; any = true; }
    return any ? v : -1;
}

// the --roofs case: run twice, then check the file against the default file of the same search.
//   want_inv: the number of involution letters of the search (8 in S^4, none in S^3), and every one of their roofs must
//             be self-inverse, up and down: 8 and 8 in S^4, counted here, which the program's own count would not show
//             (a choice that missed some would print a smaller number and the check "as the program says" would still
//             pass)
//   trade:    the junction pass puts the cancelling junctions BEFORE the length: with an excess it takes a longer roof
//             when that removes a cancelling junction, so the roofs are fewer in cancelling junctions and longer in all
//             than the default's (S^3 with --jexcess 1: 20 -> 0 junctions of theta, total length 174 -> 186 up and
//             166 -> 180 down). A pass that compared the length first would never take a longer roof
template <class P>
static void roofs_case(const char* name, const std::string& exe, const std::string& args, const std::string& opts,
                       const std::string& defname, const std::string& newname, int want_inv, bool trade)
{
    rtest::TempDir d0, d1, d2;
    std::string o0, o1, o2;
    run_in(d0.path, "'" + exe + "' " + args, o0);                  // the default file of the same search
    const std::string deffile = d0.file(defname);
    int s1 = run_in(d1.path, "'" + exe + "' " + args + " " + opts, o1);
    int s2 = run_in(d2.path, "'" + exe + "' " + args + " " + opts, o2);
    std::string f1 = rtest::read_file(d1.file(newname)), f2 = rtest::read_file(d2.file(newname));
    check(s1 == 0 && s2 == 0 && !f1.empty(), "%s %s: exit %d and %d, and the file %s written", name, opts.c_str(), s1, s2,
          newname.c_str());
    check(f1 == f2, "%s %s: the same command twice writes the same file", name, opts.c_str());
    check(mask_seconds(o1) == mask_seconds(o2), "%s %s: and prints the same lines (but the times)", name, opts.c_str());
    z::Substitution D, D0;
    if (!read_subst<P>(d1.file(newname), D) || !read_subst<P>(deffile, D0)) { check(false, "%s: the files can be read", name); return; }
    check(D.sword == D0.sword && D.tword == D0.tword, "%s: the same letters and t as the default file", name);
    exact_check<P>(D, name);
    std::vector<int> inv = inverses<P>(D);
    // the roof of x^-1 is the inverse word of the roof of x (both sides; for the involutions: self-inverse)
    int ninv = 0, nself[2] = {0, 0}, notinvw = 0;
    for (size_t j = 0; j < inv.size(); j++) {
        if (inv[j] == (int)j) ninv++;
        for (int side = 0; side < 2; side++) {
            const std::vector<W>& R = side == 0 ? D.up : D.down;
            W iw;
            for (size_t i = R[inv[j]].size(); i-- > 0;) iw.push_back(inv[R[inv[j]][i]]);
            if (inv[j] == (int)j) {
                bool si = R[j].size() % 2 == 1 && R[j] == iw;          // equal to its inverse word, odd length
                nself[side] += si;
            } else if (R[j] != iw) notinvw++;
        }
    }
    check(notinvw == 0, "%s: the roof of x^-1 is the inverse word of the roof of x (%d differ)", name, notinvw);
    long said_up = number_after(o1, "roofs chosen, up  : "), said_down = number_after(o1, "roofs chosen, down: ");
    size_t pu = o1.find("roofs chosen, up  :"), pd = o1.find("roofs chosen, down:");
    long su = pu == std::string::npos ? -1 : number_after(o1.substr(pu), "with a self-inverse roof:");
    long sd = pd == std::string::npos ? -1 : number_after(o1.substr(pd), "with a self-inverse roof:");
    check(said_up > 0 && said_down > 0 && su == nself[0] && sd == nself[1],
          "%s: the involution roofs that are self-inverse: up %d, down %d, as the program says (%ld, %ld; of %d)", name,
          nself[0], nself[1], su, sd, ninv);
    // the count made here, not the program's: all of them (8 of 8 up and 8 of 8 down in S^4; there is none in S^3)
    check(ninv == want_inv && nself[0] == want_inv && nself[1] == want_inv,
          "%s %s: %d involution letters (%d expected), and %d of them up and %d down have a self-inverse roof (all of them: %d)",
          name, opts.c_str(), ninv, want_inv, nself[0], nself[1], want_inv);
    // no more cancelling junctions than the default file, up and down, of theta and of theta^2 (counted here)
    for (int side = 0; side < 2; side++) {
        Counts cn = my_counts(drawn_roofs(side == 0 ? D.up : D.down, inv), inv);
        Counts c0 = my_counts(drawn_roofs(side == 0 ? D0.up : D0.down, inv), inv);
        check(cn.c1 <= c0.c1 && cn.c2 <= c0.c2, "%s %s: cancelling junctions of theta %ld (default %ld), of theta^2 %ld"
              " (default %ld)", name, side ? "Z-" : "Z+", cn.c1, c0.c1, cn.c2, c0.c2);
        if (trade)
            check(cn.c1 < c0.c1 && cn.len > c0.len, "%s %s: junctions before length: fewer cancelling junctions of theta "
                  "(%ld against %ld) at a larger total length (%ld against %ld)", name, side ? "Z-" : "Z+", cn.c1, c0.c1,
                  cn.len, c0.len);
        note("%s %s %s: cancelling junctions of theta %ld / %ld (default %ld / %ld), of theta^2 %ld / %ld (default %ld / %ld),"
             " total length %ld (default %ld)", name, opts.c_str(), side ? "Z-" : "Z+", cn.c1, cn.j1, c0.c1, c0.j1, cn.c2,
             cn.j2, c0.c2, c0.j2, cn.len, c0.len);
    }
}

// line k (from 1) of a text, without its newline ("" if there is none)
static std::string nth_line(const std::string& s, int k)
{
    size_t a = 0;
    for (int i = 1; i < k; i++) {
        a = s.find('\n', a);
        if (a == std::string::npos) return "";
        a++;
    }
    if (a >= s.size()) return "";
    size_t e = s.find('\n', a);
    return s.substr(a, e == std::string::npos ? std::string::npos : e - a);
}

// the line "  I  d(o, t o) = ...  WORD" of --tlist: the word of candidate I (the rest of the line after the f(t) or
// h(t) field)
static std::string tlist_word(const std::string& out, int k, bool imm)
{
    for (const std::string& line : rtest::split_lines(out)) {
        int kk = -1;
        if (sscanf(line.c_str(), " %d d(o", &kk) != 1 || kk != k) continue;
        size_t f = line.find("f(t) = (");
        size_t p = imm ? line.find('[') : f == std::string::npos ? f : line.find(')', f + 8);
        if (p == std::string::npos) return "";
        if (imm) return line.substr(p + 1, line.find(']') - p - 1);
        p += 1;
        while (p < line.size() && line[p] == ' ') p++;
        return line.substr(p);
    }
    return "";
}

static std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::cs::setup();
    z::imm::setup();
    printf("test_roof_choice: the options of the presentation in the kernel search\n");
    words_cases();
#ifdef ZIPPERS_REPO
    const std::string repo = ZIPPERS_REPO;
    section("2. the junctions of the stored files, pinned and counted again");
    long cs_want[2][3] = {{160, 58, 0}, {146, 42, 0}}, imm_want[2][3] = {{468, 96, 29}, {423, 248, 225}};
    stored_file_counts<z::cs::RenderPolicy>(repo + "/legacy/s3/subst_4.00_9.50_t3.txt", "S^3 _t3", cs_want, 0);
    stored_file_counts<z::imm::RenderPolicy>(repo + "/legacy/s4/subst5_2.30_4.50_t1.txt", "S^4 _t1", imm_want, 8);
#endif
    section("3. the candidates for t: every element allowed, sorted, candidate 0 = pick_t, the ties");
    z::imm::SearchPolicy pi(3.0);
    {
        // the small balls (their lines go to stdout; this test prints them once)
        pi.small_ball(2.3);
        z::cs::SearchPolicy pc;
        pc.small_ball(4.0);
        for (int ph : {1, 2}) {
            auto q = [&](int i) {
                if (pi.ball.label[i].mu != 0 || pi.ball.label[i].h != 2*ph) return false;
                z::imm::M6 g = pi.ball.load(i), p = z::imm::O51::identity();
                for (int k = 0; k < 8; k++) p = z::imm::O51::mul(p, g);
                return z::imm::O51::displacement(p) >= 4*pi.ball.dist[i];
            };
            candidate_cases(pi, ph, "S^4", q, ph == 1 ? 48 : 36);
        }
        for (int ph : {1, 3}) {
            auto q = [&](int i) { return pc.ball.label[i].f0 == ph; };
            candidate_cases(pc, ph, "S^3", q, ph == 1 ? 4 : 2);
        }
        section("4. the programs: defaults, --tcand, --roofs, --noinv, bad options");
#if defined(ZIPPERS_CHZ) && defined(ZIPPERS_CHZ5) && defined(ZIPPERS_REPO)
        const std::string chz = ZIPPERS_CHZ, chz5 = ZIPPERS_CHZ5;
        // no option: the stored files, byte for byte
        {
            rtest::TempDir d;
            std::string out;
            int st = run_in(d.path, "'" + chz5 + "' kernel 2.3 4.2 3 2", out);
            std::string got = rtest::read_file(d.file("subst5_2.30_4.20_t2.txt"));
            std::string want = rtest::read_file(repo + "/tests/reference/s4_chz5_kernel_steep/files/subst5_2.30_4.20_t2.txt");
            check(st == 0 && !want.empty() && got == want, "chz5 kernel 2.3 4.2 3 2 (no option) writes the reference file, byte for byte");
            check(out.find("# options:") == std::string::npos && out.find("candidate") == std::string::npos &&
                  out.find("roofs chosen") == std::string::npos, "chz5 with no option prints no line of the options");
        }
        {
            rtest::TempDir d;
            std::string out;
            int st = run_in(d.path, "'" + chz + "' kernel 4 9 1", out);
            std::string got = rtest::read_file(d.file("subst_4.00_9.00_t1.txt"));
            std::string want = rtest::read_file(repo + "/legacy/s3/subst_4.00_9.00_t1.txt");
#if defined(__APPLE__) && defined(__aarch64__)
            check(st == 0 && !want.empty() && got == want, "chz kernel 4 9 1 (no option) writes legacy/s3/subst_4.00_9.00_t1.txt, byte for byte");
#else
            // Off Apple's arm64 the S^3 search can choose other words (on Linux the
            // file differs), as the reference harness expects: off the reference machine it does not compare the
            // hash of an S^3 kernel file either. So here the file must be written, read, and have every roof exact.
            z::Substitution D;
            bool rd = !got.empty() && read_subst<z::cs::RenderPolicy>(d.file("subst_4.00_9.00_t1.txt"), D);
            check(st == 0 && rd, "chz kernel 4 9 1 (no option) writes subst_4.00_9.00_t1.txt, and it reads");
            if (rd) exact_check<z::cs::RenderPolicy>(D, "chz kernel 4 9 1 (no option)");
            int nl = 0, ndiff = 0;
            for (size_t a = 0, b = 0; a < got.size() || b < want.size(); nl++) {
                size_t ea = got.find('\n', a), eb = want.find('\n', b);
                if (ea == std::string::npos) ea = got.size();
                if (eb == std::string::npos) eb = want.size();
                if (got.compare(a, ea - a, want, b, eb - b) != 0) ndiff++;
                a = ea < got.size() ? ea + 1 : ea;
                b = eb < want.size() ? eb + 1 : eb;
            }
            note("chz kernel 4 9 1 (no option): %d of the %d lines differ from legacy/s3/subst_4.00_9.00_t1.txt "
                 "(compared byte for byte only on Apple's arm64)", ndiff, nl);
#endif
        }
        // --tlist and --tcand: the t of the file is candidate I of --tlist and of the library; --tlist lists and STOPS,
        // writing no file (its own folder: a program that went on after the list would leave the default file there)
        {
            rtest::TempDir dl, d;
            std::string list, out;
            int sl = run_in(dl.path, "'" + chz5 + "' kernel 2.3 4.2 3 --tlist", list);
            int st = run_in(d.path, "'" + chz5 + "' kernel 2.3 4.2 3 --tcand 7", out);
            z::Substitution D;
            bool rd = read_subst<z::imm::RenderPolicy>(d.file("subst5_2.30_4.20_t1_i7.txt"), D);
            std::vector<int> c = pi.t_candidates(1);
            std::string lib = c.size() > 7 ? z::imm::fword(pi.ball.path(c[7])) : "";
            check(st == 0 && rd && trim(D.tword) == lib && tlist_word(list, 7, true) == lib && !lib.empty(),
                  "chz5 --tcand 7: the file's t [%s] is candidate 7 of --tlist [%s] and of the library [%s]",
                  trim(D.tword).c_str(), tlist_word(list, 7, true).c_str(), lib.c_str());
            check(out.find("CERTIFICATE that S_c generates K: YES") != std::string::npos, "chz5 --tcand 7: the certificate holds");
            check(number_after(list, "candidates for t with phi = 1:") == (long)c.size(), "chz5 --tlist: %ld candidates (%zu)",
                  number_after(list, "candidates for t with phi = 1:"), c.size());
            std::vector<std::string> written = dir_entries(dl.path);
            check(sl == 0 && written.empty() && list.find("wrote") == std::string::npos &&
                  list.find("CERTIFICATE") == std::string::npos && list.find("K-search") == std::string::npos,
                  "chz5 --tlist: exit %d, lists and stops (no search), and writes no file (%zu written, %s)", sl,
                  written.size(), written.empty() ? "none" : written[0].c_str());
        }
        {
            rtest::TempDir dl, d;
            std::string list, out;
            int sl = run_in(dl.path, "'" + chz + "' kernel 4 9 1 --tlist", list);
            int st = run_in(d.path, "'" + chz + "' kernel 4 9 1 --tcand 1", out);
            z::Substitution D;
            bool rd = read_subst<z::cs::RenderPolicy>(d.file("subst_4.00_9.00_t1_i1.txt"), D);
            std::vector<int> c = pc.t_candidates(1);
            std::string lib = c.size() > 1 ? z::cs::word(pc.ball, c[1], pc.G) : "";
            check(st == 0 && rd && trim(D.tword) == lib && tlist_word(list, 1, false) == lib && !lib.empty(),
                  "chz --tcand 1: the file's t (%s) is candidate 1 of --tlist (%s) and of the library (%s)",
                  trim(D.tword).c_str(), tlist_word(list, 1, false).c_str(), lib.c_str());
            std::vector<std::string> written = dir_entries(dl.path);
            check(sl == 0 && written.empty() && list.find("wrote") == std::string::npos &&
                  number_after(list, "candidates for t with phi = 1:") == (long)c.size(),
                  "chz --tlist: exit %d, %ld candidates (%zu), and writes no file (%zu written, %s)", sl,
                  number_after(list, "candidates for t with phi = 1:"), c.size(), written.size(),
                  written.empty() ? "none" : written[0].c_str());
        }
        // --tcand with a STEEP t (TPHI = 2, the lightest steep case): candidate I of the phi = 2 list is the t of the file
        // (its phi is 2: h = 4), the certificate keeps pick_t's t (phi = 1), the file is named subst5_..._t2_i1, and the
        // candidate line says "candidate 1 of the 462 with phi = 2". A program that took candidate I of the phi = 1 list
        // (whose t has h = 2) would write that t, with roofs for it
        {
            rtest::TempDir dl, d;
            std::string list, out;
            int sl = run_in(dl.path, "'" + chz5 + "' kernel 2.3 4.2 3 2 --tlist", list);
            int st = run_in(d.path, "'" + chz5 + "' kernel 2.3 4.2 3 2 --tcand 1", out);
            z::Substitution D;
            bool rd = read_subst<z::imm::RenderPolicy>(d.file("subst5_2.30_4.20_t2_i1.txt"), D);
            std::vector<int> c1 = pi.t_candidates(1), c2 = pi.t_candidates(2);
            std::string lib0 = z::imm::fword(pi.ball.path(pi.pick_t(1)));
            std::string lib1 = c1.size() > 1 ? z::imm::fword(pi.ball.path(c1[1])) : "";
            std::string lib2 = c2.size() > 1 ? z::imm::fword(pi.ball.path(c2[1])) : "";
            int mu = -1, h = -1;
            if (rd) z::imm::word_mu_h(z::imm::parse_fword(D.tword), mu, h);
            check(sl == 0 && st == 0 && rd && !lib2.empty() && trim(D.tword) == lib2 && tlist_word(list, 1, true) == lib2 &&
                  lib2 != lib1,
                  "chz5 ... 3 2 --tcand 1: the file's t [%s] is candidate 1 of the phi = 2 list: --tlist [%s], the library [%s],"
                  " and not candidate 1 of the phi = 1 list [%s]", trim(D.tword).c_str(), tlist_word(list, 1, true).c_str(),
                  lib2.c_str(), lib1.c_str());
            check(rd && mu == 0 && h == 4, "chz5 ... 3 2 --tcand 1: the file's t has mu = %d, h = %d (phi = 2: mu 0, h 4)", mu, h);
            check(number_after(list, "candidates for t with phi = 2:") == (long)c2.size() && c2.size() == 462 &&
                  dir_entries(dl.path).empty(),
                  "chz5 ... 3 2 --tlist: %ld candidates with phi = 2 (%zu; 462 in the ball of radius 3), and no file",
                  number_after(list, "candidates for t with phi = 2:"), c2.size());
            check(out.find("\nt = [" + lib0 + "]") != std::string::npos &&
                  out.find("\nsteep t = [" + lib2 + "]") != std::string::npos &&
                  out.find("h(t) = 4") != std::string::npos &&
                  out.find("  (candidate 1 of the " + std::to_string(c2.size()) + " with phi = 2;") != std::string::npos,
                  "chz5 ... 3 2 --tcand 1 prints the certificate's t [%s] (pick_t's), the steep t [%s] and the line "
                  "\"candidate 1 of the %zu with phi = 2\"", lib0.c_str(), lib2.c_str(), c2.size());
            check(out.find("CERTIFICATE that S_c generates K: YES") != std::string::npos &&
                  out.find("steep roofs up 80 / 80, down 80 / 80") != std::string::npos &&
                  out.find("wrote subst5_2.30_4.20_t2_i1.txt") != std::string::npos,
                  "chz5 ... 3 2 --tcand 1: the certificate holds, the 80 + 80 steep roofs are found, the file is "
                  "subst5_2.30_4.20_t2_i1.txt");
            if (rd) exact_check<z::imm::RenderPolicy>(D, "chz5 ... 3 2 --tcand 1");   // every roof is for the file's steep t
        }
        // --roofs
        roofs_case<z::imm::RenderPolicy>("chz5 kernel 2.3 4.2 3", chz5, "kernel 2.3 4.2 3", "--roofs",
                                         "subst5_2.30_4.20_t1.txt", "subst5_2.30_4.20_t1_r0.txt", 8, false);
        roofs_case<z::cs::RenderPolicy>("chz kernel 4 9 1", chz, "kernel 4 9 1", "--roofs --jexcess 1",
                                        "subst_4.00_9.00_t1.txt", "subst_4.00_9.00_t1_r1.txt", 0, true);
        // the options line: with --roofs the third line of the file is "# options: ...", a comment that the readers
        // skip, and it is the third line of the file of data/ that the same command made. On Apple's arm64, where the
        // S^3 files of data/ were made, the whole file is that file, byte for byte (elsewhere the S^3 search can choose
        // other words: the case of no option above)
        {
            rtest::TempDir d;
            std::string out;
            const std::string name = "subst_4.00_9.00_t1_r0.txt";
            int st = run_in(d.path, "'" + chz + "' kernel 4 9 1 --roofs", out);
            std::string got = rtest::read_file(d.file(name));
            std::string want = rtest::read_file(repo + "/data/s3/" + name);
            const std::string og = nth_line(got, 3), ow = nth_line(want, 3);
            check(st == 0 && !want.empty() && og.rfind("# options: ", 0) == 0 && og == ow,
                  "chz kernel 4 9 1 --roofs: exit %d; the third line of its %s is \"%s\", that of data/s3/%s \"%s\"", st,
                  name.c_str(), og.c_str(), name.c_str(), ow.c_str());
#if defined(__APPLE__) && defined(__aarch64__)
            check(got == want, "chz kernel 4 9 1 --roofs writes data/s3/%s, byte for byte", name.c_str());
#endif
            note("chz kernel 4 9 1 --roofs: %s", og.c_str());
        }
        // --noinv: the certificate fails, and why
        {
            rtest::TempDir d;
            std::string out;
            int st = run_in(d.path, "'" + chz5 + "' kernel 2.3 4.2 3 --noinv", out);
            check(st == 2 && out.find("S_c without its 8 involution letters") != std::string::npos &&
                  out.find("|S_c| = 72 ") != std::string::npos && out.find("CERTIFICATE that S_c generates K: no") != std::string::npos,
                  "chz5 --noinv: 72 letters, the certificate fails (exit %d)", st);
            // the reason: the facet counts mod 2 (W -> (Z/2)^16 is a homomorphism) of the 72 letters span no involution's
            std::vector<int> Sidx = pi.kernel_indices(2.3);
            std::vector<unsigned> vs, invs;
            for (int i : Sidx) {
                z::imm::Gen6 g = pi.generator(i);
                unsigned v = 0;
                for (int f : g.word) v ^= 1u << f;
                (z::imm::O51::eq(z::imm::O51::inv(g.m), g.m) ? invs : vs).push_back(v);
            }
            std::vector<unsigned> basis;
            auto reduce = [&](unsigned v) { for (unsigned b : basis) v = std::min(v, v ^ b); return v; };
            for (unsigned v : vs) { unsigned r = reduce(v); if (r) { basis.push_back(r); std::sort(basis.rbegin(), basis.rend()); } }
            int inspan = 0;
            for (unsigned v : invs) inspan += reduce(v) == 0;
            check(vs.size() == 72 && invs.size() == 8 && inspan == 0, "--noinv: the 72 letters' facet counts mod 2 span a"
                  " space of rank %zu that contains none of the 8 involution letters' (%d do)", basis.size(), inspan);
        }
        // bad options
        for (const char* bad : {"--tcand", "--tcand x", "--jexcess 1", "--roofs --excess -1", "--frobnicate", "--tcand 99999"}) {
            rtest::TempDir d;
            std::string out;
            int st = run_in(d.path, "'" + chz5 + "' kernel 2.3 4.2 3 " + bad, out);
            check(st == 1 && out.find("wrote") == std::string::npos, "chz5 ... %s is refused (exit %d)", bad, st);
        }
#else
        note("chz and chz5 not built in this configuration: their runs are left out");
#endif
    }
    rtest::end_section();
    printf("test_roof_choice: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
