// SPDX-License-Identifier: GPL-3.0-or-later
//
// kernel_search.h: the block-filling search (Calegari-Loukidou, Zippers, Thm 4.9), written once, as a template over an
// example's SEARCH POLICY, and through it over a backend (geometry/backend.h).
//
// THE SEARCH. A lattice Gamma, a homomorphism phi: Gamma -> Z, and K = ker phi. For a step radius c let
// S_c = { s in K, s != 1 : d(o, s o) <= c }, and fix t in Gamma with phi(t) = 1. A breadth-first search of K with
// steps S_c, inside the ball B(R), looks for chains from 1 to t^-1 s t and to t s t^-1 (s in S_c): the roofs one level
// up and down over the horizontal segment (1, s), which are the substitution rule of the staircase family. If all of
// them are found, and every generator g of Gamma has g t^-phi(g) in <S_c> too, then <S_c> is normalised by t and
// <S_c, t> = Gamma, which PROVES that S_c generates K. A steep t (phi(t) = TPHI) climbs faster; its roofs are again
// words in S_c. The search then writes the substitution file, and reports how the chains of the staircase converge.
//
// THE STEPS, and who does each (G: the generic code here; P: the policy):
//    1. the small ball: a ball of Gamma, with the example's cocycle and its check                P: small_ball
//    2. t: the shortest element of the ball with phi = 1 (the example says which qualify)        P: pick_t, print_t, print_no_t
//    3. S_c: which elements, in which order, and a check of them                                 P: kernel_indices, generator, check_S
//       the line "|S_c| = ..."                                                                    G
//    4. the K-search: breadth first in B(R) with steps S_c, by the keys of the products          G: kernel_fill
//       with the example's labels and their check, its guard and its line                        P: kstore_*, kernel_guard, print_kstore
//    5. the roofs: looked up in the K-store, or else (if P::descent) found by a best-first
//       descent from outside the K-store; then (if P::check_roofs) each checked exactly          G: kernel_express, the roofs
//    6. membership: g t^-phi(g) in <S_c> for the example's generators g of Gamma                 P: membership (with G's find_word)
//    7. the certificate                                                                           G
//       the roof statistics                                                                       P: stats
//    8. the steep t, if TPHI != 1: its roofs, as in 5                                            G (with P: pick_t, print_t, stats)
//    9. the substitution file                                                                     P: write_file; G: the line "wrote ..."
//   10. the convergence of the chains                                                             G: kernel_chain_rows, kernel_convergence
//                                                                                                 (with P: chain_header, chain_point, dmul)
// Every line that the two legacy programs printed identically is printed here; every other line is the policy's
// (where the examples differ, each supplies its own version), but for the two
// failure messages of the exact check of the roofs, a path that cannot occur, which are chz5's (chz did not check).
//
// A SEARCH POLICY P is a class with these members (examples/cartwright_steger_search.h and examples/imm_m5_search.h
// are the two; a new example needs only a new policy):
//
//   typedef ... B;            the backend (geometry/backend.h)
//   typedef ... Label;        the label of the elements of the ball and of the K-store (Store<B, Label>, store/store.h)
//   typedef ... Gen;          an element of S_c, with what the example records of it; its member m is the element
//   Store<B, Label> ball;     the small ball (step 1): t and S_c are taken from it
//
//   static constexpr size_t kstore_cap;       the K-search stops when the K-store holds this many elements ([CAPPED])
//   static constexpr bool descent;            whether a word not found in the K-store is looked for by the descent
//   static constexpr size_t descent_budget;   (if descent) at most this many elements per descent
//   static constexpr double descent_radius;   (if descent) and none farther than this from o
//   static constexpr bool check_roofs;        whether every roof is checked exactly (a wrong one stops the program)
//   static constexpr double chain_stop;       the convergence report stops once d(o, chain) exceeds this
//
//   bool small_ball(double c);                           enumerate the ball, print its line; false: stop (status 1)
//   int pick_t(int ph) const;                            the index in the ball of the t with phi(t) = ph, or -1
//   void print_t(int i, bool steep) const;               the line of t (or of the steep t)
//   void print_no_t(int ph) const;                       the line when there is no t with phi(t) = ph
//   std::vector<int> kernel_indices(double c) const;     the indices in the ball of S_c, in the example's order
//   Gen generator(int i) const;                          the element of S_c that is element i of the ball
//   bool check_S(const std::vector<Gen>& S) const;       false (after its message): stop (status 1)
//   void kernel_guard(double R) const;                   before the K-search (it may refuse, and exit)
//   Label kstore_root_label() const;                     the label of 1 in the K-store
//   Label kstore_label(const Label& from, const Gen& s) const;     the label of an element (from) times s
//   void kstore_revisit(const Label& found, const Label& label);   an element reached again, by another path
//   void print_kstore(size_t n, double R, double seconds, int maxdepth, bool capped) const;   the K-search's line
//   void print_descents(long descents, long nodes) const;          (if descent) after each pass over the roofs
//   template <class F> KernelMembership membership(const Elem& t, const Elem& tinv, F& find_word);
//                                                        step 6, with find_word(x, word) of step 5
//   static void stats(const std::vector<std::vector<int>>& w, const std::vector<char>& ok, const char* name);
//   std::string write_file(double c, double R, int tphi, int ti, const std::vector<Gen>& S,
//                          const std::vector<std::vector<int>>& up, const std::vector<std::vector<int>>& down) const;
//                                                        writes the file of the t of ball index ti; returns its name
//   static void chain_header();                          the second line of the convergence report
//   static DMat dmul(const DMat& a, const DMat& b);      a*b in doubles (DMat = B::DMat)
//   static void chain_point(const DMat& m, double* s, double& d);  m o on S^{N-1} (N = B::N doubles), and d(o, m o)
//
// THE RULE. This is the code of kernelmode of legacy/s3/chz.cc and
// legacy/s4/chz5.cc (the same algorithm, written twice), moved here once. Every step visits the elements in the legacy
// order and computes the legacy expressions, so that chz and chz5 print and write exactly what the legacy programs did
// (tests/reference/, tests/unit/test_search_*.cc). What changed in the move, beyond names and templates:
//   - the K-search of chz5 identified a product by multiplying it out and looking it up; here, as in chz, by
//     B::product_key and Store::find. It is the same index and the same distance: ON1<5>::product_key is the last
//     column of the whole product, and Store::lookup is find(key(normal(g))) with normal the identity in O(5,1) (it
//     also checked that the element found is g, which cannot fail for a lattice acting freely). Both programs
//     multiply an element out only when it is added.
//   - the descent, the inverses in S_c and the exact check of the roofs were written for O(5,1) (with m6eq, and with
//     elements that are their own representatives); here they normalise where PU(2,1) needs it and compare with
//     B::same, which in O(5,1) are the identity map and m6eq, so chz5 computes what it did.
//   - the roofs of chz were found by two loops (the first t, the steep t); both programs now use chz5's roofs, which
//     make the same lookups in the same order.
//   - the two convergence reports are one loop (kernel_chain_rows), which hands each row to a sink; the program's sink
//     prints the legacy line, and the unit tests compare the rows bit for bit.
//
// THE OPTIONS (KernelOptions below; chz and chz5 read them after their positional arguments). Each is off by
// default, and with none of them the search does, prints and writes exactly what it did (the reference runs; the
// branches below are taken only when an option is given). They choose the presentation:
//   --tlist        list the CANDIDATES FOR t with phi(t) = TPHI and stop: the elements of the small ball that the
//                  example allows (P: t_candidates; CS: all of Pi \ 1 is loxodromic; IMM: its loxodromic test), sorted
//                  by d(o, t o), ties in the ball's order, so that candidate 0 is pick_t's t; and how many tie with
//                  the shortest (by the float d(o, t o) that pick_t compares, and exactly: P: same_length)
//   --tcand I      take candidate I as the t with phi(t) = TPHI. With TPHI = 1 it is the t of the certificate and of the
//                  file; with TPHI != 1 the certificate keeps pick_t's t (any t with phi(t) = 1 proves it), and
//                  candidate I is the steep t of the file
//   --noinv        S_c without its involution letters (s = s^-1): the one attempt at generators with no involution
//   --roofs        choose the roofs of the file's t (search/roof_choice.h): self-inverse roofs for the involution
//                  letters, up and down, at most --excess E letters longer than the shortest word found (default 2),
//                  then a greedy pass that lowers the cancelling junctions, over words at most --jexcess e letters
//                  longer than the shortest (default 0); the roof of every letter that is not its representative is
//                  the inverse word of its representative's, as the renderer would make it
// The file of a search with options gets a name that no stored file has: the default name with a suffix for each
// option given (_i<I>, _noinv, _r<e>, and _x<E> when E is not 2), and a comment line ("# options: ...") that says how it
// was made; the reader skips it.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "core/timing.h"
#include "search/roof_choice.h"
#include "store/store.h"

namespace zippers {

// the options (the top of the file); the defaults are the search without options
struct KernelOptions {
    bool tlist = false;          // --tlist: list the candidates for t and stop
    int tcand = -1;              // --tcand I: the I-th candidate for t (-1: pick_t's)
    bool noinv = false;          // --noinv: S_c without its involution letters
    bool roofs = false;          // --roofs: choose the roofs
    RoofChoiceOptions choice;    // --excess E, --jexcess e (search/roof_choice.h)
    bool any() const { return tlist || tcand >= 0 || noinv || roofs; }
    // the suffix of the file's name, and the comment line of the file (both empty without options)
    std::string suffix() const
    {
        std::string s;
        if (tcand >= 0) s += "_i" + std::to_string(tcand);
        if (noinv) s += "_noinv";
        if (roofs) {
            s += "_r" + std::to_string(choice.jexcess);
            if (choice.excess != RoofChoiceOptions().excess) s += "_x" + std::to_string(choice.excess);
        }
        return s;
    }
    std::string comment() const
    {
        if (!any()) return "";
        std::string s = "# options:";
        s += tcand >= 0 ? " t = candidate " + std::to_string(tcand) + " (chz --tlist)" : " t as by default";
        if (noinv) s += "; S_c without its involution letters";
        s += roofs ? "; roofs chosen: self-inverse involution roofs (excess " + std::to_string(choice.excess) +
                     "), the junction pass (excess " + std::to_string(choice.jexcess) + "), inverse words" :
                     "; the roofs as by default";
        return s;
    }
};

// read the options from argv[first..argc-1]; tokens that do not begin with "--" are skipped, as the programs
// always ignored extra arguments. False (after a message on stderr) for an unknown option or a missing or bad value.
inline bool parse_kernel_options(int argc, char** argv, int first, KernelOptions& o)
{
    auto value = [&](int& i, int lo, int hi, int& out) -> bool {
        if (i + 1 >= argc) { fprintf(stderr, "%s needs a value\n", argv[i]); return false; }
        char* end = nullptr;
        long v = strtol(argv[i + 1], &end, 10);
        if (!*argv[i + 1] || *end || v < lo || v > hi) {
            fprintf(stderr, "%s: bad value '%s' (%d..%d)\n", argv[i], argv[i + 1], lo, hi);
            return false;
        }
        out = (int)v;
        i++;
        return true;
    };
    for (int i = first; i < argc; i++) {
        if (strncmp(argv[i], "--", 2)) continue;
        std::string a = argv[i];
        if (a == "--tlist") o.tlist = true;
        else if (a == "--tcand") { if (!value(i, 0, 1 << 30, o.tcand)) return false; }
        else if (a == "--noinv") o.noinv = true;
        else if (a == "--roofs") o.roofs = true;
        else if (a == "--excess") { if (!value(i, 0, 64, o.choice.excess)) return false; }
        else if (a == "--jexcess") { if (!value(i, 0, 64, o.choice.jexcess)) return false; }
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return false; }
    }
    if ((o.choice.excess != RoofChoiceOptions().excess || o.choice.jexcess != RoofChoiceOptions().jexcess) && !o.roofs) {
        fprintf(stderr, "--excess and --jexcess need --roofs\n");
        return false;
    }
    return true;
}

// the candidates for t with phi(t) = ph (the policy's t_candidates: candidate 0 is pick_t's), and how many tie with
// the shortest: by the float d(o, t o) (ties_float) and exactly (ties_exact)
template <class Policy>
std::vector<int> kernel_t_candidates(const Policy& p, int ph, int& ties_float, int& ties_exact)
{
    std::vector<int> c = p.t_candidates(ph);
    ties_float = ties_exact = 0;
    for (int i : c) {
        if (p.ball.dist[i] == p.ball.dist[c[0]]) ties_float++;
        if (p.same_length(i, c[0])) ties_exact++;
    }
    return c;
}

// what a policy's membership test (step 6) found
enum class KernelMembership {
    all,        // every generator g of Gamma has g t^-phi(g) in <S_c>
    not_all,    // some have not: no certificate
    stop        // the test could not be made (the policy has said why): the program stops, with status 1
};

// the counts of the descent, which chz5 prints: the descents made, and the elements they visited
struct KernelDescentCount {
    long descents = 0, nodes = 0;
};

// Step 4: the breadth-first search of K with steps S, inside B(R), into the empty store K. It stops when K holds
// Policy::kstore_cap elements, and returns whether it did. The elements are added in the order of the legacy searches
// (by index in K, then by index in S); a product is identified by its key, which B::product_key may compute without
// the whole product, and multiplied out only when it is added.
template <class Policy>
bool kernel_fill(Policy& p, Store<typename Policy::B, typename Policy::Label>& K,
                 const std::vector<typename Policy::Gen>& S, double R)
{
    typedef typename Policy::B B;
    typedef typename B::Elem Elem;
    typedef typename Policy::Label Label;
    K.add(B::normal(B::identity()), -1, -1, 0, p.kstore_root_label(), 0.f);
    bool capped = false;
    for (size_t q = 0; q < K.size() && !capped; q++) {
        Elem g = K.load(q);
        for (size_t j = 0; j < S.size(); j++) {
            typename B::Key key = B::product_key(g, S[j].m);
            Label lab = p.kstore_label(K.label[q], S[j]);
            int f = K.find(key);
            if (f >= 0) { p.kstore_revisit(K.label[f], lab); continue; }
            double d = B::key_displacement(key);
            if (d > R) continue;
            Elem h = B::normal(B::mul(g, S[j].m));
            K.add(h, q, j, K.depth[q] + 1, lab, (float)d);
            if (K.size() >= Policy::kstore_cap) { capped = true; break; }
        }
    }
    return capped;
}

// The index in S of the inverse of each element of S (-1 if it has none; the last one, if several).
template <class B, class Gen>
std::vector<int> kernel_inverses(const std::vector<Gen>& S)
{
    std::vector<int> INV(S.size(), -1);
    for (size_t j = 0; j < S.size(); j++) {
        typename B::Elem x = B::inv(S[j].m);
        for (size_t k = 0; k < S.size(); k++) if (B::same(S[k].m, x)) INV[j] = (int)k;
    }
    return INV;
}

// A word in S for an element x of K: from the K-store, or else by a best-first descent from x (steps S, priority
// d(o, g o), at most `budget` elements, none farther than `radius` from o) until it meets the K-store. If
// x s_j1 ... s_jm = y, then x = y s_jm^-1 ... s_j1^-1 (INV: kernel_inverses). Returns whether a word was found; if not,
// word is left as it was. The descent store's labels are not used (Label{}).
template <class B, class Label, class Gen>
bool kernel_express(const Store<B, Label>& K, const std::vector<Gen>& S, const std::vector<int>& INV,
                    const typename B::Elem& x, std::vector<int>& word, size_t budget, double radius,
                    KernelDescentCount& count)
{
    typedef typename B::Elem Elem;
    int k = K.lookup(x);
    if (k >= 0) { word = K.path(k); return true; }
    count.descents++;
    Store<B, Label> Bk;
    Elem x0 = B::normal(x);
    Bk.add(x0, -1, -1, 0, Label{}, (float)B::displacement(x0));
    typedef std::pair<float, int> QE;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    pq.push({Bk.dist[0], 0});
    while (!pq.empty() && Bk.size() < budget) {
        int i = pq.top().second;
        pq.pop();
        Elem g = Bk.load(i);
        for (size_t j = 0; j < S.size(); j++) {
            Elem y = B::normal(B::mul(g, S[j].m));
            if (Bk.lookup(y) >= 0) continue;
            double dy = B::displacement(y);
            if (dy > radius) continue;
            int idx = Bk.add(y, i, (int)j, Bk.depth[i] + 1, Label{}, (float)dy);
            int kk = K.lookup(y);
            if (kk >= 0) {
                std::vector<int> back = Bk.path(idx);
                word = K.path(kk);
                for (int a = (int)back.size() - 1; a >= 0; a--) word.push_back(INV[back[a]]);
                count.nodes += Bk.size();
                return true;
            }
            pq.push({(float)dy, idx});
        }
    }
    count.nodes += Bk.size();
    return false;
}

// Step 10: the convergence of the staircase chains. The level-n chain over the segment (1, s_y) is the sequence of
// points t^n u o, u running over the prefixes of theta^n(s_y) (theta = th, the roofs); it goes from t^n o to
// s_y t^n o, and its limit is the arc of the zipper from t^+ to s_y t^+. At each level: the largest chord between
// consecutive points (radially projected to the sphere at infinity), which must shrink geometrically for the arcs to
// exist, and the least and greatest d(o, point). Double precision suffices: nothing is identified here.
//
// kernel_chain_rows computes the levels and hands each to sink(row); kernel_convergence prints them, as the legacy
// convergence did (each row as it is computed). The two legacy functions printed in the loop; the sink lets the unit
// tests compare the rows bit for bit, which the printed digits could not show.
struct KernelChainRow {
    int n;                  // the level
    size_t points;          // the number of points of the chain
    double chord;           // the largest chord between consecutive points
    double dmin, dmax;      // the least and the greatest d(o, point)
};

template <class Policy, class Sink>
void kernel_chain_rows(const std::vector<typename Policy::Gen>& S, const std::vector<std::vector<int>>& th,
                       const typename Policy::B::Elem& t, int y, size_t Lmax, Sink&& sink)
{
    typedef typename Policy::B B;
    typedef typename B::DMat DMat;
    constexpr int N = B::N;
    std::vector<DMat> Sd;
    for (auto& s : S) Sd.push_back(B::to_double(s.m));
    DMat td = B::to_double(t), T = B::to_double(B::identity());
    std::vector<int> w = {y};
    for (int n = 0; n < 40; n++) {
        DMat P = T;
        double prev[N], cur[N], d, dmin, dmax, mc = 0;
        Policy::chain_point(P, prev, d);
        dmin = dmax = d;
        for (size_t m = 0; m < w.size(); m++) {
            P = Policy::dmul(P, Sd[w[m]]);
            Policy::chain_point(P, cur, d);
            dmin = std::min(dmin, d); dmax = std::max(dmax, d);
            double e = 0;
            for (int i = 0; i < N; i++) e += (cur[i]-prev[i])*(cur[i]-prev[i]);
            mc = std::max(mc, std::sqrt(e));
            std::memcpy(prev, cur, sizeof prev);
        }
        sink(KernelChainRow{n, w.size() + 1, mc, dmin, dmax});
        size_t next = 0;
        for (int l : w) next += th[l].size();
        if (next > Lmax || dmax > Policy::chain_stop) break;
        std::vector<int> w2;
        w2.reserve(next);
        for (int l : w) for (int x : th[l]) w2.push_back(x);
        w.swap(w2);
        T = Policy::dmul(T, td);
    }
}

template <class Policy>
void kernel_convergence(const std::vector<typename Policy::Gen>& S, const std::vector<std::vector<int>>& th,
                        const typename Policy::B::Elem& t, int y, size_t Lmax, const char* nm)
{
    printf("convergence of the chains for %s over the segment (1, s_%d):\n", nm, y);
    Policy::chain_header();
    kernel_chain_rows<Policy>(S, th, t, y, Lmax, [](const KernelChainRow& r) {
        printf("  %2d %10zu   %14.3e   %7.2f .. %.2f\n", r.n, r.points, r.chord, r.dmin, r.dmax);
    });
}

// --tlist, --tcand: the line of a chosen candidate, or the list of all of them
template <class Policy>
void kernel_print_candidates(const Policy& p, int ph, bool all, int chosen)
{
    int tf, te;
    std::vector<int> c = kernel_t_candidates(p, ph, tf, te);
    if (all) {
        printf("candidates for t with phi = %d: %zu; %d tie with the shortest (d(o, t o) = %.6f), %d exactly\n", ph,
               c.size(), tf, c.empty() ? 0.0 : (double)p.ball.dist[c[0]], te);
        for (size_t k = 0; k < c.size(); k++) p.print_candidate((int)k, c[k]);
    } else
        printf("  (candidate %d of the %zu with phi = %d; %d tie with the shortest, %d exactly)\n", chosen, c.size(), ph,
               tf, te);
}

// --tcand: the ball index of candidate k with phi = ph, or -1 (after a message) if there is none
template <class Policy>
int kernel_candidate(const Policy& p, int ph, int k)
{
    int tf, te;
    std::vector<int> c = kernel_t_candidates(p, ph, tf, te);
    if (k >= (int)c.size()) { printf("no candidate %d for t with phi = %d: there are %zu\n", k, ph, c.size()); return -1; }
    return c[k];
}

// The search, for the step radius c, inside B(R), with a steep t of phi(t) = tphi. Returns the program's exit status:
// 0 when the certificate holds and the file is written, 2 when the certificate (or the steep t's) fails, 1 when the
// search cannot be made (as the policy has said). With the options (opt; the top of the file), which are off
// by default, it chooses the presentation; without them it is the search as it was.
template <class Policy>
int kernel_search(Policy& p, double c, double R, int tphi, const KernelOptions& opt = KernelOptions())
{
    typedef typename Policy::B B;
    typedef typename B::Elem Elem;
    typedef typename Policy::Label Label;
    typedef typename Policy::Gen Gen;

    // 1. the small ball
    if (!p.small_ball(c)) return 1;
    if (opt.tlist) { kernel_print_candidates(p, tphi, true, 0); return 0; }      // --tlist

    // 2. t: the shortest element with phi = 1 (of those the example allows)
    int ti = p.pick_t(1);
    if (ti < 0) { p.print_no_t(1); return 1; }
    if (opt.tcand >= 0 && tphi == 1) {                                        // --tcand, the t of the certificate
        ti = kernel_candidate(p, 1, opt.tcand);
        if (ti < 0) return 1;
    }
    Elem t = p.ball.load(ti), tinv = B::inv(t);
    p.print_t(ti, false);
    if (opt.tcand >= 0 && tphi == 1) kernel_print_candidates(p, 1, false, opt.tcand);

    // 3. S_c
    std::vector<int> Sidx = p.kernel_indices(c);
    std::vector<Gen> S;
    for (int i : Sidx) S.push_back(p.generator(i));
    if (opt.noinv) {                                                          // --noinv
        std::vector<int> Sidx2;
        std::vector<Gen> S2;
        for (size_t j = 0; j < S.size(); j++)
            if (!B::same(B::inv(S[j].m), S[j].m)) { Sidx2.push_back(Sidx[j]); S2.push_back(S[j]); }
        printf("--noinv: S_c without its %zu involution letters\n", S.size() - S2.size());
        Sidx.swap(Sidx2);
        S.swap(S2);
    }
    printf("|S_c| = %zu kernel elements with d <= %.2f", S.size(), c);
    if (!S.empty()) printf(" (shortest %.4f)", p.ball.dist[Sidx[0]]);
    printf("\n");
    if (S.empty()) return 1;
    if (!p.check_S(S)) return 1;

    // 4. breadth-first search of K with steps S_c inside B(R)
    p.kernel_guard(R);
    Store<B, Label> K;
    double t0 = now();
    bool capped = kernel_fill(p, K, S, R);
    int maxdep = 0;
    for (size_t i = 0; i < K.size(); i++) maxdep = std::max(maxdep, K.depth[i]);
    p.print_kstore(K.size(), R, now() - t0, maxdep, capped);

    // 5. a word in S_c for an element of K: from the K-store, or else (if the example uses it) by the descent
    std::vector<int> INV;
    if constexpr (Policy::descent) INV = kernel_inverses<B>(S);
    KernelDescentCount count;
    auto find_word = [&](const Elem& x, std::vector<int>& word) -> bool {
        if constexpr (Policy::descent) {
            return kernel_express(K, S, INV, x, word, Policy::descent_budget, Policy::descent_radius, count);
        } else {
            int k = K.lookup(x);
            if (k >= 0) { word = K.path(k); return true; }
            return false;
        }
    };
    // the roofs over the vertical step tt (tti its inverse): t^-1 s t (up) and t s t^-1 (down) for every s in S_c
    auto roofs = [&](const Elem& tt, const Elem& tti, std::vector<std::vector<int>>& up,
                     std::vector<std::vector<int>>& down, std::vector<char>& uok, std::vector<char>& dok,
                     long& nu, long& nd) {
        up.assign(S.size(), {}); down.assign(S.size(), {});
        uok.assign(S.size(), 0); dok.assign(S.size(), 0);
        nu = nd = 0;
        for (size_t j = 0; j < S.size(); j++) {
            if (find_word(B::mul(B::mul(tti, S[j].m), tt), up[j])) { uok[j] = 1; nu++; }
            if (find_word(B::mul(B::mul(tt, S[j].m), tti), down[j])) { dok[j] = 1; nd++; }
        }
        if constexpr (Policy::check_roofs) {
            // check every roof exactly
            for (size_t j = 0; j < S.size(); j++) {
                Elem pu = B::identity(), pd = B::identity();
                for (int l : up[j]) pu = B::mul(pu, S[l].m);
                for (int l : down[j]) pd = B::mul(pd, S[l].m);
                if (uok[j] && !B::same(pu, B::mul(B::mul(tti, S[j].m), tt))) { printf("an up roof fails the exact check\n"); exit(1); }
                if (dok[j] && !B::same(pd, B::mul(B::mul(tt, S[j].m), tti))) { printf("a down roof fails the exact check\n"); exit(1); }
            }
        }
        if constexpr (Policy::descent) p.print_descents(count.descents, count.nodes);
    };
    std::vector<std::vector<int>> up, down;
    std::vector<char> uok, dok;
    long nu, nd;
    roofs(t, tinv, up, down, uok, dok, nu, nd);
    printf("roofs up   (t^-1 s t): %ld / %zu found\n", nu, S.size());
    printf("roofs down (t s t^-1): %ld / %zu found\n", nd, S.size());

    // 6. membership, and 7. the certificate
    KernelMembership memb = p.membership(t, tinv, find_word);
    if (memb == KernelMembership::stop) return 1;
    bool cert = nu == (long)S.size() && nd == (long)S.size() && memb == KernelMembership::all;
    printf("CERTIFICATE that S_c generates K: %s\n", cert ? "YES" : "no");
    Policy::stats(up, uok, "up  ");
    Policy::stats(down, dok, "down");
    if (!cert) return 2;

    // 8. The vertical step need not climb just one level: any t with phi(t) > 0 normalises K, and t^-1 s t is again
    // a word in S_c. A steep t (phi(t) = tphi > 1) climbs faster.
    if (tphi != 1) {
        int tsi = p.pick_t(tphi);
        if (tsi < 0) { p.print_no_t(tphi); return 1; }
        if (opt.tcand >= 0) {                                                 // --tcand, the steep t
            tsi = kernel_candidate(p, tphi, opt.tcand);
            if (tsi < 0) return 1;
        }
        ti = tsi;
        t = p.ball.load(ti); tinv = B::inv(t);
        p.print_t(ti, true);
        if (opt.tcand >= 0) kernel_print_candidates(p, tphi, false, opt.tcand);
        roofs(t, tinv, up, down, uok, dok, nu, nd);
        printf("steep roofs up %ld / %zu, down %ld / %zu\n", nu, S.size(), nd, S.size());
        Policy::stats(up, uok, "up  ");
        Policy::stats(down, dok, "down");
        if (nu < (long)S.size() || nd < (long)S.size()) return 2;
    }

    // --roofs: the choice of the roofs of the file's t (search/roof_choice.h), each then checked exactly
    if (opt.roofs) {
        std::vector<int> CINV = kernel_inverses<B>(S);
        for (int k : CINV) if (k < 0) { printf("--roofs: S_c is not closed under inverses\n"); return 1; }
        for (int side = 0; side < 2; side++) {
            std::vector<std::vector<int>>& th = side == 0 ? up : down;
            std::vector<Elem> target(S.size());
            for (size_t j = 0; j < S.size(); j++)
                target[j] = side == 0 ? B::mul(B::mul(tinv, S[j].m), t) : B::mul(B::mul(t, S[j].m), tinv);
            RoofChoiceReport rep;
            double t1 = now();
            choose_roofs<B>(K, S, CINV, target, th, opt.choice, rep);
            for (size_t j = 0; j < S.size(); j++) {
                Elem pr = B::identity();
                for (int l : th[j]) pr = B::mul(pr, S[l].m);
                if (!B::same(pr, target[j])) { printf("a chosen roof fails the exact check\n"); exit(1); }
            }
            printf("roofs chosen, %s: %d representatives from %ld candidate words (%.2fs), %d changed, %d shorter;"
                   " involution letters with a self-inverse roof: %d / %d", side == 0 ? "up  " : "down",
                   rep.representatives, rep.candidates, now() - t1, rep.changed, rep.shorter, rep.selfinv,
                   rep.involutions);
            if (!rep.excess_needed.empty()) {
                printf(" (their excess over the shortest word:");
                bool first = true;
                for (const auto& kv : rep.excess_needed) {
                    printf("%s %d for %d", first ? "" : ",", kv.first, kv.second);
                    first = false;
                }
                printf(")");
            }
            for (int z : rep.no_selfinv) printf("; none for %d", z);
            printf("\n  cancelling junctions of theta: %ld / %ld -> %ld / %ld, of theta^2: %ld / %ld -> %ld / %ld;"
                   " total length %ld -> %ld (%d passes)\n", rep.before.cancel1, rep.before.junctions1,
                   rep.after.cancel1, rep.after.junctions1, rep.before.cancel2, rep.before.junctions2,
                   rep.after.cancel2, rep.after.junctions2, rep.before.length, rep.after.length, rep.passes);
            std::vector<char> all(S.size(), 1);
            Policy::stats(th, all, side == 0 ? "up  " : "down");
        }
    }

    // 9. the substitution file
    std::string fn = opt.any() ? p.write_file(c, R, tphi, ti, S, up, down, opt.suffix(), opt.comment())
                               : p.write_file(c, R, tphi, ti, S, up, down);
    printf("wrote %s\n", fn.c_str());

    // 10. the convergence of the chains
    kernel_convergence<Policy>(S, up, t, 0, 400000, "Z+ (t, theta_up)");
    kernel_convergence<Policy>(S, down, tinv, 0, 400000, "Z- (t^-1, theta_down)");
    return 0;
}

}  // namespace zippers
