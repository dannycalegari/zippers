// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_reader_imm: the S^4 word reader gives chz5's data. The legacy reader of the substitution files of IMM's M5
// (legacy/s4/zipcore5.h: parse_fword) drops facet 1 from every word; imm::parse_fword, which imm::RenderPolicy::word
// calls, does not. This test holds the corrected reader to the data themselves. For each of the
// three files of legacy/s4, written by `chz5 kernel C R RHO TPHI`, it recomputes what chz5 computed, as chz5 computes it
// (the search policy of examples/imm_m5_search.h, with the c and rho of the file's header and the TPHI of its name):
// the small ball B(rho), S_c and t. Then it checks that
//   - the letters the reader makes are, in order, exactly the elements of S_c (the same exact matrices), and the words
//     of the file are chz5's words of them (fword of their paths in the ball);
//   - t, as the reader makes it, is the element of its path: chz5's t with phi(t) = TPHI, whose word is the file's, an
//     element of Gamma with h(t) = 2 TPHI;
//   - prepare<imm::RenderPolicy> accepts the file (first in a child process, for its exit status and message): the exact
//     check of the roofs passes, and its letters and t are those above; and every roof of the file, up and down, holds
//     exactly for them (prepare checks the representatives' roofs and derives the others');
// and it reports each file's counts: the letters whose words contain facet 1 (those the old reader misread), the
// involutions, the inverse pairs and the base arcs, and that no letter is there twice.
// It uses src/ only, no legacy code. Built with -O2, as the other unit tests. ZIPPERS_REPO is the repository.

#include "examples/imm_m5.h"
#include "examples/imm_m5_render.h"
#include "examples/imm_m5_search.h"
#include "geometry/on1/on1.h"
#include "render/substitution.h"
#include "render/world.h"

#include "render_test_support.h"

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef ZIPPERS_REPO
#error "define ZIPPERS_REPO as the path of the repository"
#endif

namespace z = zippers;
using z::imm::M6;
using z::imm::O51;
typedef z::imm::RenderPolicy RP;
using rtest::check;
using rtest::note;
using rtest::section;

// run f with stdout going to a temporary file; what f wrote there (rtest::capture_stderr, for stdout)
static std::string capture_stdout(const std::function<void()>& f)
{
    std::string path = rtest::tmp_root() + "/zippers_reader_test_XXXXXX";
    std::vector<char> p(path.begin(), path.end());
    p.push_back(0);
    int fd = mkstemp(p.data());
    if (fd < 0) { perror("mkstemp"); exit(2); }
    fflush(stdout);
    int saved = dup(1);
    dup2(fd, 1);
    f();
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    std::string out;
    lseek(fd, 0, SEEK_SET);
    char buf[4096];
    ssize_t k;
    while ((k = read(fd, buf, sizeof buf)) > 0) out.append(buf, (size_t)k);
    close(fd);
    unlink(p.data());
    return out;
}

// the tokens of a word, one blank between them (the t-word of a file begins with a blank)
static std::string tokens(const std::string& w)
{
    std::istringstream is(w);
    std::string tok, s;
    while (is >> tok) s += (s.empty() ? "" : " ") + tok;
    return s;
}

static bool has_facet1(const std::string& w)
{
    std::istringstream is(w);
    std::string tok;
    while (is >> tok) if (tok == "1") return true;
    return false;
}

int main()
{
    double t_start = rtest::clock_seconds();
    z::imm::setup();
    printf("test_reader_imm: the S^4 word reader against chz5's S_c and t, for the substitution files of legacy/s4\n");

    const char* const files[3] = {"subst5_2.30_4.50_t1.txt", "subst5_2.30_4.50_t2.txt", "subst5_2.30_4.50_t3.txt"};
    std::string titles[3];
    std::unique_ptr<z::imm::SearchPolicy> P;          // chz5's small ball, for the last (c, rho)
    double Pc = -1, Prho = -1;
    for (int fi = 0; fi < 3; fi++) {
        const char* f = files[fi];
        std::string fn = std::string(ZIPPERS_REPO) + "/legacy/s4/" + f;

        // the file; c and rho from its header ("... c 2.3000  R 4.5000  rho 3.0000"), TPHI from its name ("..._t2.txt")
        std::string text = rtest::read_file(fn);
        z::Substitution D;
        bool read = !text.empty() && z::load_substitution(fn.c_str(), D, RP::dialect);
        double c = 0, R = 0, rho = 0;
        size_t hp = text.find("; c ");
        bool header = hp != std::string::npos && sscanf(text.c_str() + hp, "; c %lf R %lf rho %lf", &c, &R, &rho) == 3;
        std::string name = f;
        int tphi = atoi(name.c_str() + name.rfind("_t") + 2);
        char title[160];
        snprintf(title, sizeof title, "%s: c %g, rho %g, TPHI %d", f, c, rho, tphi);
        titles[fi] = title;
        section(titles[fi].c_str());
        check(read, "%s: read", fn.c_str());
        check(header, "%s: c, R and rho in the header", f);
        if (!read || !header) continue;

        // chz5's small ball, S_c and t (examples/imm_m5_search.h, as `chz5 kernel C R RHO TPHI` computes them: the ball
        // B(max(RHO, c)), whose radius the header records)
        if (!P || Pc != c || Prho != rho) {
            P.reset(new z::imm::SearchPolicy(rho));
            bool good = false;
            std::string line = capture_stdout([&] { good = P->small_ball(c); });
            check(good, "%s: the ball B(%g) has no (mu, h) inconsistency", f, rho);
            note("chz5's %s", rtest::chomp(line).c_str());
            Pc = c;
            Prho = rho;
        } else {
            note("chz5's ball: as for the file above (the same c and rho)");
        }
        const z::imm::Store6& ball = P->ball;
        int ti = P->pick_t(tphi);
        std::vector<int> Sidx = P->kernel_indices(c);
        check(ti >= 0, "%s: chz5 has a t with phi(t) = %d", f, tphi);
        check(Sidx.size() == D.sword.size(), "%s: %zu letters, |S_c| = %zu", f, D.sword.size(), Sidx.size());
        size_t n = std::min(Sidx.size(), D.sword.size());

        // the letters as the reader makes them: exactly S_c, in order, with chz5's words
        int with1 = 0;
        for (size_t j = 0; j < n; j++) {
            const std::string& w = D.sword[j];
            check(O51::eq(RP::word(w), ball.load(Sidx[j])), "%s: letter %zu (%s) is element %zu of S_c", f, j, w.c_str(), j);
            std::string cw = z::imm::fword(ball.path(Sidx[j]));
            check(w == cw, "%s: letter %zu: the word %s, chz5's %s", f, j, w.c_str(), cw.c_str());
            with1 += has_facet1(w);
        }

        // t as the reader makes it: the element of its path, chz5's t, in Gamma with h = 2 TPHI
        M6 t = RP::word(D.tword);
        if (ti >= 0) {
            check(O51::eq(t, ball.load(ti)), "%s: t (%s) is chz5's t", f, tokens(D.tword).c_str());
            std::string cw = z::imm::fword(ball.path(ti));
            check(tokens(D.tword) == cw, "%s: the word of t, %s, chz5's %s", f, tokens(D.tword).c_str(), cw.c_str());
        }
        int tmu = -1, th = 0;
        z::imm::word_mu_h(z::imm::parse_fword(D.tword), tmu, th);
        check(tmu == 0 && th == 2*tphi, "%s: t in Gamma with phi(t) = %d (mu %d, h %d)", f, tphi, tmu, th);

        // prepare accepts the file (a failing roof exits, so in a child first), with these letters and this t
        std::string err;
        int st = rtest::in_child([&] { z::World<O51> w; z::prepare<RP>(fn.c_str(), w); }, err);
        check(st == 0, "%s: prepare<imm::RenderPolicy> exits %d: \"%s\"", f, st, rtest::chomp(err).c_str());
        if (st != 0) continue;
        z::World<O51> W;
        std::string line = rtest::capture_stderr([&] { z::prepare<RP>(fn.c_str(), W); });
        check(line == err, "%s: prepare prints the same line twice", f);
        check(W.L.s.size() == n, "%s: prepare has %zu letters", f, W.L.s.size());
        bool letters = W.L.s.size() == n;
        for (size_t j = 0; letters && j < n; j++) letters = O51::eq(W.L.s[j], ball.load(Sidx[j]));
        check(letters, "%s: prepare's letters are S_c", f);
        check(O51::eq(W.plus.t, t) && O51::eq(W.minus.t, O51::inv(t)), "%s: prepare's t of Z+, and its inverse for Z-", f);
        if (W.L.s.size() != n) continue;

        // every roof of the file, as written, holds exactly: t^-1 s_j t (up) and t s_j t^-1 (down)
        M6 tinv = O51::inv(t);
        long nup = 0, ndown = 0;
        for (size_t j = 0; j < n; j++)
            for (int side = 0; side < 2; side++) {
                const std::vector<int>& roof = side ? D.down[j] : D.up[j];
                M6 p = O51::identity();
                bool inrange = true;
                for (int x : roof) {
                    if (x < 0 || x >= (int)n) { inrange = false; break; }
                    p = O51::mul(p, W.L.s[x]);
                }
                M6 want = side ? O51::mul(O51::mul(t, W.L.s[j]), tinv) : O51::mul(O51::mul(tinv, W.L.s[j]), t);
                check(inrange && O51::same(p, want), "%s: the %s roof of letter %zu", f, side ? "down" : "up", j);
                (side ? ndown : nup) += (long)roof.size();
            }

        // the counts: involutions, inverse pairs, base arcs (the representatives, plus), letters twice
        int invol = 0, pairs = 0, self = 0, plus = 0, twice = 0;
        for (size_t j = 0; j < n; j++) {
            invol += O51::eq(O51::mul(W.L.s[j], W.L.s[j]), O51::identity());
            self += W.L.inv[j] == (int)j;
            pairs += (int)j < W.L.inv[j];
            plus += W.L.plus[j];
            for (size_t k = j + 1; k < n; k++) twice += O51::eq(W.L.s[j], W.L.s[k]);
        }
        check(twice == 0, "%s: no letter twice (%d pairs of equal letters)", f, twice);
        check(self == invol, "%s: inv[j] = j for the %d involutions, and only for them (%d)", f, invol, self);
        check(plus == invol + pairs && invol + 2*pairs == (int)n, "%s: %d base arcs = %d involutions + %d inverse pairs",
              f, plus, invol, pairs);
        std::string want = "(" + std::to_string(plus) + " base arcs)";
        check(line.find(want) != std::string::npos, "%s: prepare's line says %s", f, want.c_str());
        note("%s", rtest::chomp(line.substr(line.rfind('/') + 1)).c_str());
        note("(%zu letters, %d of them with facet 1 in their words; %d involutions and %d inverse pairs: %d base arcs;",
             n, with1, invol, pairs, plus);
        note(" no letter twice; the roofs up and down, of %ld and %ld letters in all, every one exact)", nup, ndown);
        if (ti >= 0)
            note("(t = %s%s: d(o, t o) = %.4f, h(t) = %d)", tokens(D.tword).c_str(),
                 has_facet1(D.tword) ? ", with facet 1" : "", ball.dist[ti], th);
    }

    rtest::end_section();
    printf("test_reader_imm: %ld checks, %ld fail (%.1f s)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start);
    return rtest::all_bad ? 1 : 0;
}
