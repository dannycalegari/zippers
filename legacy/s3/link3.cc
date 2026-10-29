/*  link3.cc  --  linking numbers of loops in the zippers Z+ and Z- of the Cartwright-Steger
    surface (see zipper3.cc for the arcs, README.md for the data).

    LOOPS.  A relation w = s_{j_1} ... s_{j_m} = 1 among the generators S of K = ker phi gives
    a closed loop in Z+,
        gamma_w = alpha_{j_1} u s_{j_1} alpha_{j_2} u ... u s_{j_1}...s_{j_{m-1}} alpha_{j_m},
    through the vertices t^+, s_{j_1} t^+, ..., and with the arcs of Z- in place of the alpha's a
    loop delta_w in Z-.  Translates h gamma_w, h delta_w are loops too.  Relations are found
    exactly by a breadth-first search of K: every edge outside the search tree closes a cycle;
    they are freely and cyclically reduced, and identified up to rotation and inversion (that
    is, the loops up to translation).

    WHY IT DECIDES THE QUESTION.  If lk(gamma, delta) != 0 for gamma in Z+ and delta in Z-, then
    gamma is not null-homotopic in Z+ (Z+ misses Z- and hence delta, so a null-homotopy in Z+
    would be one in S^3 - delta), and likewise delta in Z-.  A degenerate loop (one through a
    tree-like set) links nothing.

    THE COMPUTATION.  Each loop is a closed polyline: every arc is split, in order and without
    de-duplication, until its chords are below the accuracy.  Two loops are projected
    stereographically from a pole far from both, and the linking number is the sum of the
    signed crossings of one over the other in a random planar projection (the counts "A over
    B" and "B over A" must agree).  It is trusted only when the loops are much farther apart
    than the accuracy and the value is stable under refinement.

    Usage:  link3 selftest                    test the linking-number code on known links
            link3 [options]                   the search (see usage())

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "zipcore.h"
#include <set>
#include <map>
#include <random>


// ---------------------------------------------------------------------------------------
// Linking numbers of closed polylines in R^3

// sum of signed crossings of A over B, and of B over A, in the projection to the xy-plane;
// both closed (the segment from the last point back to the first is included)
static void crossings(const vector<V3>& A, const vector<V3>& B, long& aOverB, long& bOverA, long& ncross)
{
    aOverB = bOverA = ncross = 0;
    size_t n = A.size(), m = B.size();
    if (n < 2 || m < 2) return;
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300}, len = 0;
    for (const V3& p : A) for (int c = 0; c < 2; c++) { lo[c] = std::min(lo[c], p[c]); hi[c] = std::max(hi[c], p[c]); }
    for (const V3& p : B) for (int c = 0; c < 2; c++) { lo[c] = std::min(lo[c], p[c]); hi[c] = std::max(hi[c], p[c]); }
    for (size_t i = 0; i < n; i++) { const V3 &p = A[i], &q = A[(i+1)%n]; len += std::hypot(q[0]-p[0], q[1]-p[1]); }
    double cell = std::max(4*len/n, 1e-12);
    long nx = std::min(4096L, (long)((hi[0]-lo[0])/cell) + 1), ny = std::min(4096L, (long)((hi[1]-lo[1])/cell) + 1);
    double cx = (hi[0]-lo[0])/nx + 1e-300, cy = (hi[1]-lo[1])/ny + 1e-300;
    std::unordered_map<long, vector<uint32_t>> grid;
    auto cellrange = [&](const V3& p, const V3& q, long& x0, long& x1, long& y0, long& y1) {
        x0 = std::max(0L, (long)((std::min(p[0],q[0]) - lo[0])/cx)); x1 = std::min(nx-1, (long)((std::max(p[0],q[0]) - lo[0])/cx));
        y0 = std::max(0L, (long)((std::min(p[1],q[1]) - lo[1])/cy)); y1 = std::min(ny-1, (long)((std::max(p[1],q[1]) - lo[1])/cy));
    };
    for (size_t i = 0; i < n; i++) {
        long x0, x1, y0, y1;
        cellrange(A[i], A[(i+1)%n], x0, x1, y0, y1);
        for (long x = x0; x <= x1; x++) for (long y = y0; y <= y1; y++) grid[x*ny + y].push_back((uint32_t)i);
    }
    vector<uint32_t> stamp(n, (uint32_t)-1);
    for (size_t k = 0; k < m; k++) {
        const V3 &p = B[k], &q = B[(k+1)%m];
        long x0, x1, y0, y1;
        cellrange(p, q, x0, x1, y0, y1);
        for (long x = x0; x <= x1; x++) for (long y = y0; y <= y1; y++) {
            auto it = grid.find(x*ny + y);
            if (it == grid.end()) continue;
            for (uint32_t i : it->second) {
                if (stamp[i] == (uint32_t)k) continue;            // already tested against this segment
                stamp[i] = (uint32_t)k;
                const V3 &a = A[i], &b = A[(i+1)%n];
                double ux = b[0]-a[0], uy = b[1]-a[1], vx = q[0]-p[0], vy = q[1]-p[1];
                double den = ux*vy - uy*vx;
                if (den == 0) continue;
                double wx = p[0]-a[0], wy = p[1]-a[1];
                double s = (wx*vy - wy*vx)/den, u = (wx*uy - wy*ux)/den;
                if (s < 0 || s >= 1 || u < 0 || u >= 1) continue;
                double za = a[2] + s*(b[2]-a[2]), zb = p[2] + u*(q[2]-p[2]);
                // the sign of the crossing is that of (over direction) x (under direction)
                // seen from +z; den is (A direction) x (B direction)
                int sg = den > 0 ? 1 : -1;
                ++ncross;
                if (za > zb) aOverB += sg; else bOverA -= sg;
            }
        }
    }
}

// the Gauss integral by the exact segment-pair solid angles (Klenin-Langowski): for testing
static double gauss_lk(const vector<V3>& A, const vector<V3>& B)
{
    auto sub = [](const V3& a, const V3& b) { return V3{a[0]-b[0], a[1]-b[1], a[2]-b[2]}; };
    auto cr = [](const V3& a, const V3& b) { return V3{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]}; };
    auto dt = [](const V3& a, const V3& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
    auto unit = [&](V3 a) { double l = std::sqrt(dt(a, a)); for (double& x : a) x /= l; return a; };
    double tot = 0;
    size_t n = A.size(), m = B.size();
    for (size_t i = 0; i < n; i++) for (size_t k = 0; k < m; k++) {
        const V3 &p1 = A[i], &p2 = A[(i+1)%n], &p3 = B[k], &p4 = B[(k+1)%m];
        V3 r13 = sub(p3,p1), r14 = sub(p4,p1), r23 = sub(p3,p2), r24 = sub(p4,p2);
        V3 n1 = unit(cr(r13, r14)), n2 = unit(cr(r14, r24)), n3 = unit(cr(r24, r23)), n4 = unit(cr(r23, r13));
        auto as = [](double x) { return std::asin(std::max(-1.0, std::min(1.0, x))); };
        double om = as(dt(n1,n2)) + as(dt(n2,n3)) + as(dt(n3,n4)) + as(dt(n4,n1));
        double sgn = dt(cr(sub(p4,p3), sub(p2,p1)), r13);
        tot += sgn > 0 ? om : -om;
    }
    return tot/(4*PI);
}

static void random_rotation(std::mt19937_64& rng, double R[9])
{
    std::normal_distribution<double> N(0, 1);
    double q[4], s = 0;
    for (double& x : q) { x = N(rng); s += x*x; }
    for (double& x : q) x /= std::sqrt(s);
    double a = q[0], b = q[1], c = q[2], d = q[3];
    double M[9] = { a*a+b*b-c*c-d*d, 2*(b*c-a*d), 2*(b*d+a*c),
                    2*(b*c+a*d), a*a-b*b+c*c-d*d, 2*(c*d-a*b),
                    2*(b*d-a*c), 2*(c*d+a*b), a*a-b*b-c*c+d*d };
    memcpy(R, M, sizeof M);
}

static vector<V3> rotate(const vector<V3>& X, const double R[9])
{
    vector<V3> Y(X.size());
    for (size_t i = 0; i < X.size(); i++)
        for (int r = 0; r < 3; r++) Y[i][r] = R[3*r]*X[i][0] + R[3*r+1]*X[i][1] + R[3*r+2]*X[i][2];
    return Y;
}

// ---------------------------------------------------------------------------------------
// Loops in S^3, and their linking number

// the smallest distance between two point sets on S^3, searched below h (returns h if none)
static double minsep(const vector<P4>& A, const vector<P4>& B, double h)
{
    std::unordered_map<int64_t, vector<uint32_t>> grid;
    auto ci = [&](double x) { return (int64_t)std::floor((x + 2)/h); };
    auto key = [&](const int64_t c[4]) { int64_t k = 0; for (int i = 0; i < 4; i++) k = k*1000003 + c[i]; return k; };
    for (size_t i = 0; i < B.size(); i++) {
        int64_t c[4];
        for (int k = 0; k < 4; k++) c[k] = ci(B[i].x[k]);
        grid[key(c)].push_back((uint32_t)i);
    }
    double best = h;
    for (const P4& a : A) {
        int64_t c0[4];
        for (int k = 0; k < 4; k++) c0[k] = ci(a.x[k]);
        for (int m = 0; m < 81; m++) {
            int64_t c[4] = {c0[0] + m%3 - 1, c0[1] + (m/3)%3 - 1, c0[2] + (m/9)%3 - 1, c0[3] + (m/27)%3 - 1};
            auto it = grid.find(key(c));
            if (it == grid.end()) continue;
            for (uint32_t j : it->second) best = std::min(best, d4(a, B[j]));
        }
    }
    return best;
}

// the true smallest distance between two point sets on S^3 (doubling the search radius)
static double separation(const vector<P4>& A, const vector<P4>& B)
{
    for (double h = 0.02; h < 2.1; h *= 2) {
        double d = minsep(A, B, h);
        if (d < h) return d;
    }
    return 2;
}

struct LinkResult { long lk = 0; bool agree = false; long ncross = 0; double sep = 0, poledist = 0; double pole[4]; };

// the linking number of two disjoint closed polylines on S^3
static LinkResult linking(const vector<P4>& A, const vector<P4>& B, std::mt19937_64& rng, double sephint)
{
    LinkResult r;
    double pole[4];
    r.poledist = far_pole(A, B, rng, pole);
    double F[4][4], R[9];
    frame_with_pole(pole, F);
    random_rotation(rng, R);
    vector<V3> a = rotate(stereo(A, F), R), b = rotate(stereo(B, F), R);
    long ab, ba;
    crossings(a, b, ab, ba, r.ncross);
    r.lk = ab;
    r.agree = (ab == ba);
    r.sep = sephint > 0 ? minsep(A, B, sephint) : separation(A, B);
    memcpy(r.pole, pole, sizeof pole);
    return r;
}

// a picture of two loops: A blue, B red, stereographically projected from the given pole
static void draw_pair(const char* fn, const vector<P4>& A, const vector<P4>& B, const double pole[4],
                      const Side& sd, const Letters& L, double yaw = 30, double pitch = 20)
{
    Renderer ra(sd, L, 1, 1), rb(sd, L, 1, 1);
    for (auto* pr : {&ra, &rb}) {
        const vector<P4>& X = pr == &ra ? A : B;
        pr->starts.push_back(0);
        for (const P4& p : X) pr->push(p);
        pr->push(X[0]);                          // close the loop
    }
    ViewState v;
    frame_with_pole(pole, v.Q);
    double rmax = 0;
    for (const vector<P4>* X : {&A, &B})
        for (const V3& y : stereo(*X, v.Q)) rmax = std::max(rmax, std::sqrt(y[0]*y[0] + y[1]*y[1] + y[2]*y[2]));
    v.clip = 1.02*rmax;
    rot3(yaw, pitch, v.rot);
    vector<Layer> layers = {{&ra, {0.05, 0.15, 0.85}}, {&rb, {0.85, 0.08, 0.08}}};
    write_png(fn, 900, layers, v, {}, 2);
}

// ---------------------------------------------------------------------------------------
// Relations of K in the letters S

static vector<int> free_reduce(const vector<int>& w, const Letters& L)
{
    vector<int> r;
    for (int x : w) { if (!r.empty() && r.back() == L.inv[x]) r.pop_back(); else r.push_back(x); }
    return r;
}

static vector<int> cyclic_reduce(vector<int> w, const Letters& L)
{
    w = free_reduce(w, L);
    size_t a = 0, b = w.size();
    while (b - a >= 2 && w[a] == L.inv[w[b-1]]) { a++; b--; }
    return vector<int>(w.begin() + a, w.begin() + b);
}

// the least of the rotations of w and of its inverse: one representative per loop up to translation
static vector<int> canonical(const vector<int>& w, const Letters& L)
{
    vector<int> wi;
    for (int i = (int)w.size() - 1; i >= 0; i--) wi.push_back(L.inv[w[i]]);
    vector<int> best = w;
    for (const vector<int>* v : {&w, (const vector<int>*)&wi})
        for (size_t r = 0; r < v->size(); r++) {
            vector<int> c(v->begin() + r, v->end());
            c.insert(c.end(), v->begin(), v->begin() + r);
            if (c < best) best = c;
        }
    return best;
}

// relations of length <= maxlen from a breadth-first search of K with steps S inside B(Rr)
static vector<vector<int>> find_relations(const Letters& L, double Rr, size_t maxlen, size_t maxcount)
{
    Store K;
    K.add(mnormal(mid()), -1, -1, 0, 0, 0, 0.f);
    std::set<vector<int>> seen;
    vector<vector<int>> rels;
    i64 key[12];
    Zz col[3];
    const size_t CAP = 1500000;
    for (size_t q = 0; q < K.size(); q++) {
        if ((size_t)K.depth[q] > maxlen/2) break;
        M3 g = K.load(q);
        for (size_t j = 0; j < L.s.size(); j++) {
            int k = prodkey(g, L.s[j], key, col);
            int f = K.find(key);
            if (f < 0) {
                if (hdist(col[2]) > Rr || K.size() >= CAP) continue;
                K.add(mrotk(mmul(g, L.s[j]), k), q, j, K.depth[q] + 1, 0, 0, (float)hdist(col[2]));
                continue;
            }
            if ((size_t)(K.depth[q] + 1 + K.depth[f]) > maxlen) continue;
            vector<int> w = K.path(q);
            w.push_back(j);
            vector<int> pf = K.path(f);
            for (int i = (int)pf.size() - 1; i >= 0; i--) w.push_back(L.inv[pf[i]]);
            w = cyclic_reduce(w, L);
            if (w.empty()) continue;
            vector<int> c = canonical(w, L);
            if (seen.insert(c).second) rels.push_back(c);
        }
        if (rels.size() >= maxcount) break;
    }
    std::stable_sort(rels.begin(), rels.end(), [](const vector<int>& a, const vector<int>& b) { return a.size() < b.size(); });
    if (rels.size() > maxcount) rels.resize(maxcount);
    return rels;
}

// ---------------------------------------------------------------------------------------

static int selftest()
{
    std::mt19937_64 rng(1);
    auto circle = [](V3 c, int plane, double r, int n, double phase) {
        vector<V3> X;
        for (int i = 0; i < n; i++) {
            double t = 2*PI*i/n + phase;
            V3 p = c;
            if (plane == 0) { p[0] += r*std::cos(t); p[1] += r*std::sin(t); }
            else { p[0] += r*std::cos(t); p[2] += r*std::sin(t); }
            X.push_back(p);
        }
        return X;
    };
    auto torus = [](double shift, int n) {           // a (1,2) curve on the torus R = 2, r = 1
        vector<V3> X;
        for (int i = 0; i < n; i++) {
            double t = 2*PI*i/n, m = 2*t + shift;
            X.push_back({(2 + std::cos(m))*std::cos(t), (2 + std::cos(m))*std::sin(t), std::sin(m)});
        }
        return X;
    };
    struct T { const char* name; vector<V3> a, b; };
    vector<T> tests = {
        {"Hopf link", circle({0,0,0}, 0, 1, 300, 0), circle({1,0,0}, 1, 1, 300, 0.1)},
        {"Hopf link, reversed", circle({0,0,0}, 0, 1, 300, 0), [&]{ auto x = circle({1,0,0}, 1, 1, 300, 0.1); std::reverse(x.begin(), x.end()); return x; }()},
        {"split circles", circle({0,0,0}, 0, 1, 300, 0), circle({3,0,0}, 1, 1, 300, 0.1)},
        {"(2,4) torus link", torus(0, 600), torus(PI, 600)},
    };
    for (auto& t : tests) {
        double g = gauss_lk(t.a, t.b);
        printf("  %-22s Gauss %+.4f   crossings:", t.name, g);
        for (int k = 0; k < 4; k++) {
            double R[9];
            random_rotation(rng, R);
            long ab, ba, nc;
            crossings(rotate(t.a, R), rotate(t.b, R), ab, ba, nc);
            printf("  %+ld/%+ld (%ld)", ab, ba, nc);
        }
        printf("\n");
    }
    return 0;
}

static void usage()
{
    fprintf(stderr,
"usage: link3 selftest\n"
"       link3 [options]\n"
"  -f FILE      substitution file (default subst_4.00_9.50_t3.txt)\n"
"  -a ACC       accuracy of the loops (default 0.005)\n"
"  --rr R       find relations inside the hyperbolic ball B(R) (default 7.5)\n"
"  --maxlen N   longest relation (default 6)\n"
"  --nrel N     use the first N relations (default 30)\n"
"  --hr R       translate the Z- loops by the elements of B(R) (default 3.4)\n"
"  --seed N     random seed for the projections (default 1)\n"
"  --pair A B H the single pair (gamma_wA, h_H delta_wB), with --nproj projections (default 20)\n");
}

int main(int argc, char** argv)
{
    setup();
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    Options opt;
    opt.acc = 0.005;
    double rr = 7.5, hr = 3.4;
    size_t maxlen = 6, nrel = 30;
    uint64_t seed = 1;
    int pairA = -1, pairB = -1, pairH = -1, nproj = 20;
    string pngout, exportfrom;
    bool hinfo = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string { if (i + 1 >= argc) { usage(); exit(1); } return argv[++i]; };
        if (a == "-f") opt.subst = next();
        else if (a == "-a") opt.acc = atof(next().c_str());
        else if (a == "--rr") rr = atof(next().c_str());
        else if (a == "--maxlen") maxlen = atoi(next().c_str());
        else if (a == "--nrel") nrel = atoi(next().c_str());
        else if (a == "--hr") hr = atof(next().c_str());
        else if (a == "--seed") seed = strtoull(next().c_str(), nullptr, 10);
        else if (a == "--pair") { pairA = atoi(next().c_str()); pairB = atoi(next().c_str()); pairH = atoi(next().c_str()); }
        else if (a == "--nproj") nproj = atoi(next().c_str());
        else if (a == "--png") pngout = next();
        else if (a == "--hinfo") hinfo = true;
        else if (a == "--export") exportfrom = next();
        else { usage(); return 1; }
    }
    World W;
    prepare(opt, W);
    std::mt19937_64 rng(seed);

    double t0 = now();
    vector<vector<int>> rels = find_relations(W.L, rr, maxlen, nrel);
    if (hinfo) {                       // the translates: phi(h) = f0, and their a-words
        vector<RootInfo> RI = roots_info(hr);
        for (size_t i = 0; i < RI.size(); i++)
            printf("h%zu  d=%.4f  f=(%d,%d)  phi=%d  %s\n", i, RI[i].d, RI[i].f0, RI[i].f1, RI[i].f0, RI[i].word.c_str());
        return 0;
    }
    if (!exportfrom.empty()) {         // the verified pairs of a run, as explicit words
        vector<RootInfo> RI = roots_info(hr);
        std::ifstream in(exportfrom);
        string line;
        FILE* out = fopen("linked_pairs.txt", "w");
        fprintf(out, "# linked pairs (gamma_w in Z+, h delta_w' in Z-), verified by link3; letters are indices into S\n");
        int n = 0;
        while (std::getline(in, line)) {
            if (line.find("STABLE") == string::npos || line.find("ALL AGREE") == string::npos) continue;
            size_t a, b, hi;
            if (sscanf(line.c_str(), "w%zu w%zu h%zu:", &a, &b, &hi) != 3) continue;
            size_t p = line.find("[");
            long lk = 0;
            sscanf(line.c_str() + line.find(":", p) + 1, "%ld", &lk);
            fprintf(out, "lk %+ld | w", lk);
            for (int x : rels[a]) fprintf(out, " %d", x);
            fprintf(out, " | w'");
            for (int x : rels[b]) fprintf(out, " %d", x);
            fprintf(out, " | phi(h) %d | h %s\n", RI[hi].f0, RI[hi].word.c_str());
            n++;
        }
        fclose(out);
        fprintf(stderr, "wrote %d pairs to linked_pairs.txt\n", n);
        return 0;
    }
    if (pairA >= 0) {                  // one pair, many projections
        vector<M3> H;
        find_roots(hr, H);
        size_t forced = 0;
        vector<P4> A = loop_points(W.plus, W.L, mid(), rels[pairA], opt.acc, forced);
        vector<P4> B = loop_points(W.minus, W.L, H[pairH], rels[pairB], opt.acc, forced);
        printf("pair w%d w%d h%d at acc %.4g: %zu + %zu points, separation %.4f\n", pairA, pairB, pairH, opt.acc,
               A.size(), B.size(), separation(A, B));
        double pole0[4] = {0,0,0,1};
        for (int k = 0; k < nproj; k++) {
            LinkResult r = linking(A, B, rng, 0.05);
            if (k == 0) memcpy(pole0, r.pole, sizeof pole0);
            printf("  projection %2d: lk %+ld  (A over B and B over A %s)  crossings %ld  pole distance %.4f\n",
                   k, r.lk, r.agree ? "agree" : "DISAGREE", r.ncross, r.poledist);
        }
        if (!pngout.empty())
            for (double yaw : {0.0, 60.0, 120.0}) {
                char fn[200];
                snprintf(fn, sizeof fn, "%s_yaw%03d.png", pngout.c_str(), (int)yaw);
                draw_pair(fn, A, B, pole0, W.plus, W.L, yaw, 15);
                fprintf(stderr, "wrote %s\n", fn);
            }
        return 0;
    }
    fprintf(stderr, "%zu relations of length <= %zu (%.2fs):\n", rels.size(), maxlen, now() - t0);
    for (size_t r = 0; r < rels.size(); r++) {
        fprintf(stderr, "  w%zu:", r);
        for (int x : rels[r]) fprintf(stderr, " %d", x);
        fprintf(stderr, "\n");
    }

    // the Z+ loops, based at the identity
    t0 = now();
    vector<vector<P4>> gam;
    size_t forced = 0;
    for (auto& w : rels) gam.push_back(loop_points(W.plus, W.L, mid(), w, opt.acc, forced));
    size_t np = 0;
    for (auto& g : gam) np += g.size();
    fprintf(stderr, "Z+ loops: %zu points in all (%.2fs), %zu forced leaves\n", np, now() - t0, forced);

    // the Z- loops h.delta_w, h in B(hr), against every Z+ loop
    vector<M3> H;
    find_roots(hr, H);
    fprintf(stderr, "translating the Z- loops by the %zu elements of B(%.2f)\n", H.size(), hr);
    long tested = 0, nonzero = 0, disagree = 0, close = 0, projdis = 0;
    struct Hit { size_t a, b, hi; };
    vector<Hit> hits;
    double minsepall = 1;
    t0 = now();
    printf("# nonzero linking numbers lk(gamma_w, h delta_w'): w  w'  h-index  d(o,ho)  lk  crossings  separation\n");
    for (size_t hi = 0; hi < H.size(); hi++) {
        vector<vector<P4>> del;
        for (auto& w : rels) del.push_back(loop_points(W.minus, W.L, H[hi], w, opt.acc, forced));
        for (size_t a = 0; a < gam.size(); a++)
            for (size_t b = 0; b < del.size(); b++) {
                LinkResult r = linking(gam[a], del[b], rng, 0.05);
                LinkResult r2 = linking(gam[a], del[b], rng, 0.05);
                if (r2.lk != r.lk) { projdis++; r.agree = false; }
                tested++;
                minsepall = std::min(minsepall, r.sep);
                if (!r.agree) disagree++;
                if (r.sep < 5*opt.acc) close++;
                if (r.lk != 0 || r2.lk != 0) {
                    nonzero++;
                    hits.push_back({a, b, hi});
                    printf("w%zu w%zu h%zu %.4f  lk %+ld  %ld  %s%.4f%s\n", a, b, hi, hdist(H[hi].e[2][2]), r.lk, r.ncross,
                           r.sep >= 0.05 ? ">= " : "", r.sep, r.agree ? "" : "  [over/under counts DISAGREE]");
                    fflush(stdout);
                }
            }
    }
    fprintf(stderr, "tested %ld pairs in %.1fs: %ld nonzero, %ld over/under disagreements, %ld pairs where two projections disagree, %ld closer than 5 acc; min separation %.4f\n",
            tested, now() - t0, nonzero, disagree, projdis, close, minsepall);

    // verification of the nonzero pairs: refine twice, three projections each, true separation
    printf("# verification: w w' h | for acc, acc/2, acc/4: lk over three projections, separation, points\n");
    int pic = 0;
    for (const Hit& x : hits) {
        printf("w%zu w%zu h%zu:", x.a, x.b, x.hi);
        bool stable = true;
        long lk0 = 0;
        double minpole = 9;
        double pole[4] = {0,0,0,1};
        vector<P4> Afin, Bfin;
        for (int lev = 0; lev < 3; lev++) {
            double acc = opt.acc/(1 << lev);
            vector<P4> A = loop_points(W.plus, W.L, mid(), rels[x.a], acc, forced);
            vector<P4> B = loop_points(W.minus, W.L, H[x.hi], rels[x.b], acc, forced);
            printf("  [%.4g:", acc);
            double sep = 0;
            for (int s = 0; s < 3; s++) {
                LinkResult r = linking(A, B, rng, s == 0 ? -1 : 0.05);
                if (s == 0) { sep = r.sep; memcpy(pole, r.pole, sizeof pole); }
                printf(" %+ld%s%s", r.lk, r.agree ? "" : "!", r.poledist < 0.15 ? "?" : "");
                minpole = std::min(minpole, r.poledist);
                if (lev == 0 && s == 0) lk0 = r.lk;
                if (r.lk != lk0 || !r.agree) stable = false;
            }
            printf(" sep %.4f, %zu+%zu pts]", sep, A.size(), B.size());
            if (lev == 2) { Afin.swap(A); Bfin.swap(B); }
        }
        printf("  pole >= %.3f  %s", minpole, stable ? "STABLE" : "not stable");
        // invariance: lk(g gamma, g h delta) = lk(gamma, h delta) for g in Pi (here: four g's
        // from B(2.6), at the middle accuracy); the arcs are then drawn in other positions
        printf("  | translated by g:");
        for (size_t gi : {(size_t)2, (size_t)7, (size_t)11, (size_t)17}) {
            if (gi >= H.size()) continue;
            double acc = opt.acc/2;
            vector<P4> A = loop_points(W.plus, W.L, H[gi], rels[x.a], acc, forced);
            vector<P4> B = loop_points(W.minus, W.L, mmul(H[gi], H[x.hi]), rels[x.b], acc, forced);
            LinkResult r = linking(A, B, rng, 0.05);
            printf(" %+ld%s", r.lk, r.agree ? "" : "!");
            if (r.lk != lk0 || !r.agree) stable = false;
        }
        printf("  %s\n", stable ? "ALL AGREE" : "MISMATCH");
        fflush(stdout);
        if (stable && pic < 6) {
            char fn[80];
            snprintf(fn, sizeof fn, "pictures/link_w%zu_w%zu_h%zu.png", x.a, x.b, x.hi);
            draw_pair(fn, Afin, Bfin, pole, W.plus, W.L);
            fprintf(stderr, "wrote %s\n", fn);
            pic++;
        }
    }
    return 0;
}
