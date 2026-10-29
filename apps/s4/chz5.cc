/*  chz5.cc  --  the block-filling search for the fibered hyperbolic 5-manifold of
    Italiano-Martelli-Migliorini (see rh5.h): Gamma = ker mu in the reflection group W of the
    right-angled polytope P5, phi = h/2, K = ker phi.

    THE SEARCH (Calegari-Loukidou, Zippers, Thm 4.9), as in chz.cc.  For a step radius c let
    S_c = { s in K, s != 1 : d(o, s o) <= c }, and fix t in Gamma with phi(t) = 1.  A
    breadth-first search of K with steps S_c, inside the ball of radius R, looks for chains from
    1 to t^-1 s t and to t s t^-1 (s in S_c): the roofs one level up and down.  If all of them
    are found, and every Schreier generator g of Gamma has g t^-phi(g) reached too, then <S_c>
    is normalised by t and <S_c, t> = Gamma, which PROVES that S_c generates K.

    Usage:  chz5 selftest
            chz5 ball RHO                   the tiles of B(RHO): counts, and the cocycle check
            chz5 kernel C R RHO [TPHI]      the search; writes subst5_C_R_tTPHI.txt
              [--tlist] [--tcand I] [--noinv] [--roofs [--excess E] [--jexcess e]]
                                            the options, all off by default (search/kernel_search.h):
                                            list the candidates for t and stop; take candidate I; S_c without its
                                            involution letters; choose the roofs. A file written with options gets
                                            a name of its own

    THIS VERSION is legacy/s4/chz5.cc on the shared library src/: the arithmetic of
    geometry/on1/on1.h (the backend zippers::ON1<5>), the store of store/store.h and the data of
    examples/imm_m5.h (namespace zippers::imm).  The kernel mode is the block-filling
    search of search/kernel_search.h, which chz shares, with the policy of
    examples/imm_m5_search.h: what chz5 does where the two differ.  Its outputs are the
    legacy ones, byte for byte (tests/reference/).

    The options of the presentation (--tlist, --tcand, --noinv, --roofs, --excess,
    --jexcess; search/kernel_search.h) are new.  Without them nothing that chz5 prints or writes changes.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/hash.h"
#include "core/timing.h"
#include "examples/imm_m5.h"
#include "examples/imm_m5_search.h"
#include "geometry/on1/on1.h"
#include "search/kernel_search.h"

using namespace zippers;
using imm::O51;
using imm::M6;
using std::vector;
using std::string;

static int selftest()
{
    int bad = 0;
    for (int f = 0; f < 16; f++) if (!O51::eq(O51::inv(imm::REF[f]), imm::REF[f])) bad++;
    // the relations of W and the cocycle on them: (r_f r_g)^2 = 1 for orthogonal facets, and
    // h, mu agree on r_f r_g and r_g r_f from every starting state
    int nsq = 0;
    for (int f = 0; f < 16; f++)
        for (int g = 0; g < 16; g++) {
            int d = 0;
            for (int k = 0; k < 5; k++) d += imm::FS[f][k] != imm::FS[g][k];
            if (d != 2) continue;
            nsq++;
            for (int mu = 0; mu < 16; mu++) {
                int h1 = imm::hstep(mu, f) + imm::hstep(mu ^ (1 << imm::MOVE[f]), g);
                int h2 = imm::hstep(mu, g) + imm::hstep(mu ^ (1 << imm::MOVE[g]), f);
                if (h1 != h2) bad++;
            }
        }
    // random words: g g^-1 = 1, and J-orthogonality
    uint64_t r = 12345;
    for (int trial = 0; trial < 200; trial++) {
        M6 g = O51::identity();
        for (int k = 0; k < 12; k++) { r = splitmix(r); g = O51::mul(g, imm::REF[r % 16]); }
        if (!O51::eq(O51::mul(g, O51::inv(g)), O51::identity())) bad++;
    }
    printf("selftest: %d orthogonal pairs, cocycle on every square from all 16 states; %s\n",
           nsq, bad ? "FAILED" : "ok");
    return bad ? 1 : 0;
}

static int ballmode(double rho)
{
    imm::guard6(rho);
    double t0 = now();
    imm::Store6 B;
    long bad = imm::enumerate6(B, rho);
    printf("B(%.2f): %zu tiles (%.2fs); (mu, h) inconsistencies %ld\n", rho, B.size(), now() - t0, bad);
    // counts by radius, in Gamma = ker mu, and in K (mu = 0, h = 0)
    for (double r = 1; r <= rho + 1e-9; r += 0.5) {
        size_t n = 0, ng = 0, nk = 0;
        std::map<int, int> hcount;
        for (size_t i = 0; i < B.size(); i++) if (B.dist[i] <= r) {
            n++;
            if (B.label[i].mu == 0) { ng++; hcount[B.label[i].h]++; if (B.label[i].h == 0) nk++; }
        }
        printf("  d <= %.1f: %8zu tiles, %7zu in Gamma, %6zu in K   (0.39 e^4r = %.0f)   h on Gamma:",
               r, n, ng, nk, 0.39*std::exp(4*r));
        for (auto& kv : hcount) printf(" %d:%d", kv.first, kv.second);
        printf("\n");
    }
    return bad ? 1 : 0;
}

// The block-filling search: the generic search of search/kernel_search.h, with this example's policy
// (examples/imm_m5_search.h: the ball B(rho), rho = max(RHO, c), the loxodromic t, the descent, and what chz5 prints
// and writes).
static int kernelmode(double c, double R, double rho, int tphi, const KernelOptions& opt)
{
    imm::SearchPolicy policy(rho);
    return opt.any() ? kernel_search(policy, c, R, tphi, opt) : kernel_search(policy, c, R, tphi);
}

int main(int argc, char** argv)
{
    imm::setup();
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc >= 3 && !strcmp(argv[1], "ball")) return ballmode(atof(argv[2]));
    if (argc >= 5 && !strcmp(argv[1], "kernel")) {
        // TPHI, if given, is the fifth argument; the options follow (kernel_search.h)
        bool has_tphi = argc >= 6 && strncmp(argv[5], "--", 2);
        KernelOptions opt;
        if (!parse_kernel_options(argc, argv, has_tphi ? 6 : 5, opt)) return 1;
        return kernelmode(atof(argv[2]), atof(argv[3]), atof(argv[4]), has_tphi ? atoi(argv[5]) : 1, opt);
    }
    fprintf(stderr, "usage: chz5 selftest | ball RHO | kernel C R RHO [TPHI] [--tlist] [--tcand I] [--noinv]"
            " [--roofs [--excess E] [--jexcess e]]\n");
    return 1;
}
