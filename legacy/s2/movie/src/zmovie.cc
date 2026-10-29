/*  zmovie.cc  --  the frame engine of the zipper film.

    Renders a SHOT (a scene of zippers, Catherine wheel and base arcs, with animated view,
    depth, reveal, cone parameter and layer opacities) to a directory of 840x840 PNG frames.
    The drawing itself is the zipper program's: this file #includes an unmodified snapshot of
    zipper.cc (main renamed) and calls its functions.  Only the following are new here:
      * the per-root tags (the distance d(o, g.o) of the root that drew each polyline), taken
        by running the program's own per-root drawing loop (Renderer::draw_arc in root order,
        exactly as Renderer::draw_roots does) and noting where each root's polylines start.
        The polylines are not joined afterwards (joining is bookkeeping; it never changes what
        is drawn);
      * a real cone parameter T >= 2 (tr[a,b] = -2 cos(pi/T)) for the base arcs, with a
        choice of framing (conformal centre of a reference T, or interpolated);
      * the rasteriser (supersampled, antialiased, layers with opacity), the animation
        tracks, the frame loop, and a frame-sequence assembler with crossfades.

    usage:  zmovie SHOT.txt [key=value ...] [-o DIR] [-j N] [--frames A:B] [--quiet]
            zmovie --assemble TIMELINE.txt OUTDIR
    See README.md for the shot description format.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wsign-compare"
#define main zipper_main
#include "zipper_snapshot.cc"
#undef main
#pragma clang diagnostic pop

#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <dirent.h>

using Clock = std::chrono::steady_clock;
static double secs(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double>(b - a).count();
}
static bool g_quiet = false;

// =====================================================================================
// Shot description: "key = value" lines ('#' starts a comment); later lines and command
// line overrides (key=value) win.
// =====================================================================================
struct Cfg {
    std::map<string, string> kv;
    mutable std::set<string> used;
    string file;
    static string trim(const string& s) {
        size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
        return a == string::npos ? string() : s.substr(a, b - a + 1);
    }
    bool add_line(const string& line0, int lineno) {
        string line = line0;
        size_t h = line.find('#');
        if (h != string::npos) line = line.substr(0, h);
        line = trim(line);
        if (line.empty()) return true;
        size_t eq = line.find('=');
        if (eq == string::npos) {
            fprintf(stderr, "%s:%d: expected key = value: %s\n", file.c_str(), lineno, line0.c_str());
            return false;
        }
        kv[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
        return true;
    }
    bool load(const string& fn) {
        file = fn;
        std::ifstream in(fn);
        if (!in) { fprintf(stderr, "cannot read %s\n", fn.c_str()); return false; }
        string line; int n = 0;
        while (std::getline(in, line)) if (!add_line(line, ++n)) return false;
        return true;
    }
    bool has(const string& k) const { return kv.count(k) > 0; }
    string str(const string& k, const string& def = "") const {
        auto it = kv.find(k);
        if (it == kv.end()) return def;
        used.insert(k); return it->second;
    }
    double num(const string& k, double def) const {
        string s = str(k, "");
        if (s.empty()) return def;
        if (s == "inf") return INFINITY;
        char* e = nullptr; double v = strtod(s.c_str(), &e);
        if (!e || *e) { fprintf(stderr, "%s: not a number: %s\n", k.c_str(), s.c_str()); exit(1); }
        return v;
    }
    vector<double> nums(const string& k, vector<double> def) const {
        string s = str(k, "");
        if (s.empty()) return def;
        std::istringstream is(s); vector<double> v; string t;
        while (is >> t) {
            char* e = nullptr; double x = strtod(t.c_str(), &e);
            if (!e || *e) { fprintf(stderr, "%s: not a number: %s\n", k.c_str(), t.c_str()); exit(1); }
            v.push_back(x);
        }
        return v;
    }
    void warn_unused() const {
        for (auto& p : kv) if (!used.count(p.first))
            fprintf(stderr, "warning: unknown or unused key '%s'\n", p.first.c_str());
    }
};

// =====================================================================================
// Animation tracks.  "v" (constant) or "K:v K:v ..." with K a frame number, P% (a fraction
// of the shot: 0% = frame 0, 100% = the last frame) or "end"; an optional third field
// gives the easing of the segment that ENDS at that key: smooth (cubic ease in/out, the
// default), smoother (quintic), sine, linear, in, out, hold (the value jumps at the key).
// Values are interpolated in a track-specific space: zoom in log space, T in 1/T.
// =====================================================================================
// Musical timing.  A timing map (one per recording) gives the time in seconds of each musical
// landmark: lines "NAME SECONDS".  A shot may then be placed by landmarks (start = NAME,
// end = NAME, each optionally +/- seconds) and its keys written as @NAME[+-s]; frames lie on
// the film's absolute grid (frame k at k/fps s), so consecutive shots tile without drift.
static std::map<string, double> g_tmap;
static string g_tmapFile;
static double g_fps = 30;
static long g_startFrame = 0;           // the absolute frame of the shot's frame 0
// A minimal JSON reader (objects, arrays, strings, numbers, true/false/null), for the timing
// maps written by movie/music/align.py: {"offset_s": ..., "landmarks": {NAME: {"t": ...}},
// "bars": {"12": t}, "beats": [{"bar": 12, "beat": 2, "t": ...}]}.  Landmark NAME, "bar12"
// (the downbeat of bar 12) and "bar12.2" (its beat 2) become names; offset_s is added.
struct JVal {
    enum K { NUL, NUM, STR, ARR, OBJ, BOOL } k = NUL;
    double num = 0; string str; vector<JVal> arr; vector<std::pair<string, JVal>> obj;
    const JVal* get(const string& key) const {
        for (auto& p : obj) if (p.first == key) return &p.second;
        return nullptr;
    }
};
struct JParser {
    const string& s; size_t i = 0; bool ok = true;
    explicit JParser(const string& t) : s(t) {}
    void ws() { while (i < s.size() && isspace((unsigned char)s[i])) ++i; }
    bool lit(const char* w) { size_t n = strlen(w); if (s.compare(i, n, w) == 0) { i += n; return true; } return false; }
    string strv() {
        string o; ++i;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[++i];
                if (c == 'u') { o += '?'; i += 4; } else o += (c == 'n' ? '\n' : c == 't' ? '\t' : c);
                ++i;
            } else o += s[i++];
        }
        if (i >= s.size()) ok = false; else ++i;
        return o;
    }
    JVal val() {
        JVal v; ws();
        if (i >= s.size()) { ok = false; return v; }
        char c = s[i];
        if (c == '{') {
            v.k = JVal::OBJ; ++i; ws();
            if (i < s.size() && s[i] == '}') { ++i; return v; }
            while (ok) {
                ws(); if (i >= s.size() || s[i] != '"') { ok = false; break; }
                string key = strv(); ws();
                if (i >= s.size() || s[i] != ':') { ok = false; break; }
                ++i; v.obj.push_back({key, val()}); ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; break; }
                ok = false;
            }
        } else if (c == '[') {
            v.k = JVal::ARR; ++i; ws();
            if (i < s.size() && s[i] == ']') { ++i; return v; }
            while (ok) {
                v.arr.push_back(val()); ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; break; }
                ok = false;
            }
        } else if (c == '"') { v.k = JVal::STR; v.str = strv(); }
        else if (lit("true")) { v.k = JVal::BOOL; v.num = 1; }
        else if (lit("false")) { v.k = JVal::BOOL; }
        else if (lit("null")) { v.k = JVal::NUL; }
        else {
            char* e = nullptr; v.num = strtod(s.c_str() + i, &e);
            if (e == s.c_str() + i) ok = false; else { v.k = JVal::NUM; i = e - s.c_str(); }
        }
        return v;
    }
};
static bool load_timing_json(const string& fn, const string& text) {
    JParser P(text); JVal root = P.val();
    if (!P.ok || root.k != JVal::OBJ) { fprintf(stderr, "%s: not a JSON object\n", fn.c_str()); return false; }
    double off = 0;
    if (const JVal* o = root.get("offset_s")) off = o->num;
    size_t n0 = g_tmap.size();
    if (const JVal* L = root.get("landmarks")) {
        if (L->k == JVal::OBJ) for (auto& p : L->obj) {
            const JVal* t = p.second.k == JVal::NUM ? &p.second : p.second.get("t");
            if (t && t->k == JVal::NUM) g_tmap[p.first] = t->num + off;
        } else if (L->k == JVal::ARR) for (auto& x : L->arr) {
            const JVal *nm = x.get("name"), *t = x.get("t");
            if (nm && t && t->k == JVal::NUM) g_tmap[nm->str] = t->num + off;
        }
    }
    if (const JVal* B = root.get("bars"); B && B->k == JVal::OBJ)
        for (auto& p : B->obj) if (p.second.k == JVal::NUM) g_tmap["bar" + p.first] = p.second.num + off;
    if (const JVal* B = root.get("beats"); B && B->k == JVal::ARR)
        for (auto& x : B->arr) {
            const JVal *b = x.get("bar"), *k = x.get("beat"), *t = x.get("t");
            if (b && k && t) {
                char nm[64]; snprintf(nm, sizeof nm, "bar%d.%g", (int)b->num, k->num);
                g_tmap[nm] = t->num + off;
            }
        }
    if (g_tmap.size() == n0) { fprintf(stderr, "%s: no landmarks, bars or beats found\n", fn.c_str()); return false; }
    g_tmapFile = fn;
    return true;
}
static bool load_timing(const string& fn) {
    std::ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot read the timing map %s\n", fn.c_str()); return false; }
    {
        std::stringstream ss; ss << in.rdbuf(); string text = ss.str();
        size_t p = text.find_first_not_of(" \t\r\n");
        if (p != string::npos && text[p] == '{') return load_timing_json(fn, text);
        in.clear(); in.seekg(0);
    }
    string line; int n = 0;
    while (std::getline(in, line)) {
        ++n;
        size_t h = line.find('#'); if (h != string::npos) line = line.substr(0, h);
        std::istringstream is(line); string name; double t;
        if (!(is >> name)) continue;
        if (!(is >> t)) { fprintf(stderr, "%s:%d: want NAME SECONDS\n", fn.c_str(), n); return false; }
        g_tmap[name] = t;
    }
    g_tmapFile = fn;
    return true;
}
// "NAME", "NAME+0.25", "NAME-0.1" (landmarks) or "12.5" (seconds) -> seconds
static bool time_of(const string& tok, double& t) {
    char* e = nullptr; t = strtod(tok.c_str(), &e);
    if (!tok.empty() && e && !*e) return true;
    string name = tok; double off = 0;
    size_t p = tok.find_last_of("+-");
    if (p != string::npos && p > 0) {
        char* e2 = nullptr; double o = strtod(tok.c_str() + p + 1, &e2);
        if (e2 && !*e2 && p + 1 < tok.size()) { off = tok[p] == '-' ? -o : o; name = tok.substr(0, p); }
    }
    auto it = g_tmap.find(name);
    if (it == g_tmap.end()) {
        fprintf(stderr, "unknown landmark '%s'%s\n", name.c_str(), g_tmapFile.empty() ? " (no timing map: timing = FILE)" : (" in " + g_tmapFile).c_str());
        return false;
    }
    t = it->second + off; return true;
}
static long frame_at(double t) { return std::lround(t*g_fps); }

enum Ease { E_SMOOTH, E_SMOOTHER, E_SINE, E_LINEAR, E_IN, E_OUT, E_HOLD };
static bool parse_ease(const string& s, Ease& e) {
    if (s == "smooth") e = E_SMOOTH; else if (s == "smoother") e = E_SMOOTHER;
    else if (s == "sine") e = E_SINE; else if (s == "linear") e = E_LINEAR;
    else if (s == "in") e = E_IN; else if (s == "out") e = E_OUT; else if (s == "hold") e = E_HOLD;
    else return false;
    return true;
}
static double ease(Ease e, double s) {
    s = std::max(0.0, std::min(1.0, s));
    switch (e) {
        case E_SMOOTH:   return s*s*(3 - 2*s);
        case E_SMOOTHER: return s*s*s*(s*(6*s - 15) + 10);
        case E_SINE:     return 0.5 - 0.5*std::cos(PI*s);
        case E_LINEAR:   return s;
        case E_IN:       return s*s;
        case E_OUT:      return 1 - (1 - s)*(1 - s);
        case E_HOLD:     return s >= 1 ? 1 : 0;
    }
    return s;
}
enum Space { SP_LIN, SP_LOG, SP_INV };
struct Track {
    struct Key { double f, v; Ease e; };
    vector<Key> k;
    Space sp = SP_LIN;
    bool set = false;
    double fwd(double v) const {
        if (sp == SP_LOG) return std::log(v);
        if (sp == SP_INV) return std::isinf(v) ? 0.0 : 1.0/v;
        return v;
    }
    double back(double w) const {
        if (sp == SP_LOG) return std::exp(w);
        if (sp == SP_INV) return w <= 1e-12 ? INFINITY : 1.0/w;
        return w;
    }
    static bool parse(const string& name, const string& s, int nframes, Space sp, Track& t) {
        t = Track(); t.sp = sp; t.set = true;
        std::istringstream is(s); string tok;
        auto val = [&](const string& x, double& v) {
            if (x == "inf") { v = INFINITY; return true; }
            char* e = nullptr; v = strtod(x.c_str(), &e); return e && !*e;
        };
        vector<string> toks;
        while (is >> tok) toks.push_back(tok);
        if (toks.size() == 1 && toks[0].find(':') == string::npos) {
            double v; if (!val(toks[0], v)) { fprintf(stderr, "%s: bad value %s\n", name.c_str(), toks[0].c_str()); return false; }
            t.k.push_back({0, t.fwd(v), E_SMOOTH}); return true;
        }
        for (auto& x : toks) {
            vector<string> f; size_t p = 0;
            for (;;) { size_t q = x.find(':', p); f.push_back(x.substr(p, q == string::npos ? string::npos : q - p));
                       if (q == string::npos) break; p = q + 1; }
            if (f.size() < 2 || f.size() > 3) { fprintf(stderr, "%s: bad key %s (want K:v[:ease])\n", name.c_str(), x.c_str()); return false; }
            double fr, v; Ease e = E_SMOOTH;
            const double last = std::max(0, nframes - 1);
            if (f[0] == "end") fr = last;
            else if (!f[0].empty() && f[0][0] == '@') {
                double tt; if (!time_of(f[0].substr(1), tt)) return false;
                fr = (double)(frame_at(tt) - g_startFrame);
            }
            else if (!f[0].empty() && f[0].back() == '%') {
                double pc; if (!val(f[0].substr(0, f[0].size() - 1), pc)) { fprintf(stderr, "%s: bad key %s\n", name.c_str(), x.c_str()); return false; }
                fr = pc/100.0*last;
            } else if (!val(f[0], fr)) { fprintf(stderr, "%s: bad frame %s\n", name.c_str(), x.c_str()); return false; }
            if (!val(f[1], v)) { fprintf(stderr, "%s: bad value %s\n", name.c_str(), x.c_str()); return false; }
            if (f.size() == 3 && !parse_ease(f[2], e)) { fprintf(stderr, "%s: unknown easing %s\n", name.c_str(), f[2].c_str()); return false; }
            if (!t.k.empty() && fr < t.k.back().f) { fprintf(stderr, "%s: keys must be in order\n", name.c_str()); return false; }
            t.k.push_back({fr, t.fwd(v), e});
        }
        return !t.k.empty();
    }
    double at(double fr) const {
        if (k.empty()) return 0;
        if (fr <= k[0].f) return back(k[0].v);
        for (size_t i = 1; i < k.size(); ++i)
            if (fr < k[i].f) {
                double s = (fr - k[i-1].f)/(k[i].f - k[i-1].f);
                return back(k[i-1].v + (k[i].v - k[i-1].v)*ease(k[i].e, s));
            }
        return back(k.back().v);
    }
    bool constant() const {
        for (auto& x : k) if (x.v != k[0].v) return false;
        return true;
    }
};
static Track get_track(const Cfg& c, const string& key, const string& def, int nframes, Space sp = SP_LIN) {
    Track t;
    string s = c.str(key, def);
    if (!Track::parse(key, s, nframes, sp, t)) exit(1);
    return t;
}

// =====================================================================================
// Monodromy and representation at a real cone parameter T (T = inf: the cusp)
// =====================================================================================
// The first half of prepare(): resolve the monodromy (proper powers, the table, or the cusp
// character of an R/L word) and fix the orientation convention.
static bool resolve_monodromy(string& name, Monodromy& monOut) {
    if (is_rl_word(name)) {
        string v = primitive_root(name);
        if (v.size() < name.size()) {
            if (!g_quiet) fprintf(stderr, "note: monodromy %s is a proper power; drawing its root %s\n", name.c_str(), v.c_str());
            name = v;
        }
    }
    auto table = monodromy_table();
    const Monodromy* mon = nullptr;
    Monodromy dyn;
    for (auto& m : table) if (m.name == name) { mon = &m; break; }
    if (!mon && is_rl_word(name)) {
        dyn.name = name; dyn.rl = name;
        compose_phi(name, dyn.phi_a, dyn.phi_b);
        compose_psi(name, dyn.psi_a, dyn.psi_b);
        cpx cx, cy, cz;
        if (!solve_cusp_character(dyn.phi_a, dyn.phi_b, cx, cy, cz)) {
            fprintf(stderr, "could not find a geometric cusped character for monodromy %s\n", name.c_str());
            return false;
        }
        dyn.cuspx = cx; dyn.cuspy = cy; dyn.cuspz = cz;
        mon = &dyn;
    }
    if (!mon) { fprintf(stderr, "unknown monodromy '%s'\n", name.c_str()); return false; }
    monOut = *mon;
    return orient_cusp_character(monOut);
}
// k = tr[a,b] = -2 cos(pi/T), computed as build_rep computes it for an integer order
static double kappa_of(double T) {
    if (std::isinf(T)) return -2.0;
    double r = std::round(T);
    if (std::abs(T - r) < 1e-12) return -2.0*std::cos(PI/(double)(int)r);
    return -2.0*std::cos(PI/T);
}
static bool is_order(double T, int& order) {      // an integer order >= 2, or the cusp (0)
    if (std::isinf(T)) { order = 0; return true; }
    double r = std::round(T);
    if (std::abs(T - r) < 1e-12 && r >= 2 && r <= 1e6) { order = (int)r; return true; }
    return false;
}
// the unconjugated representation (reconstruct_AB's normal form; continuous in T)
static bool raw_rep(const Monodromy& mon, double T, Rep& rep) {
    cpx x, y, z;
    if (!solve_character(mon, kappa_of(T), x, y, z)) return false;
    if (!build_rep_from_char(mon.phi_a, mon.phi_b, x, y, z, rep)) {
        fprintf(stderr, "degenerate or reducible representation at T = %g\n", T);
        return false;
    }
    return true;
}
static void conj_rep(Rep& rep, const Mat& Mc) {
    Mat Mi = matinv(Mc);
    auto cj = [&](const Mat& X){ return matmul(Mc, matmul(X, Mi)); };
    rep.A = cj(rep.A); rep.Ai = cj(rep.Ai);
    rep.B = cj(rep.B); rep.Bi = cj(rep.Bi);
    rep.C = cj(rep.C); rep.Ci = cj(rep.Ci);
}
// D^s for D in SL(2,C) (the one-parameter subgroup through D, D taken up to sign nearest 1)
static Mat mat_pow(Mat D, double s) {
    if (tr(D).real() < 0) D = { -D.a, -D.b, -D.c, -D.d };
    cpx c = tr(D)/2.0, th = std::acosh(c), sh = std::sinh(th);
    cpx f = std::abs(sh) < 1e-14 ? cpx(s, 0) : std::sinh(s*th)/sh, ch = std::cosh(s*th);
    return { ch + f*(D.a - c), f*D.b, f*D.c, ch + f*(D.d - c) };
}
// Framing.  conformal_center(rep) gives the program's frame for a lattice; for a
// non-integer T (not discrete) it is not meaningful, and it jumps between nearby T.
//   native        conformal_center at the current T (the program's frame at an integer T)
//   ref T0        that of T0, for every T
//   interp T0 T1  D(s) Mc(T0), D = Mc(T1) Mc(T0)^-1, s = (k - k0)/(k1 - k0) in [0,1]
//   none          the unconjugated normal form
struct Framing {
    enum Mode { NATIVE, REF, INTERP, NONE } mode = NATIVE;
    double T0 = 2, T1 = 3;
    Mat M0 = identity(), M1 = identity(), D = identity();
    bool ready = false;
    bool parse(const string& s) {
        std::istringstream is(s); string m; is >> m;
        auto rd = [&](double& v) { string t; if (!(is >> t)) return false;
                                    if (t == "inf") { v = INFINITY; return true; }
                                    char* e; v = strtod(t.c_str(), &e); return !*e; };
        if (m == "native") mode = NATIVE;
        else if (m == "none") mode = NONE;
        else if (m == "ref") { mode = REF; if (!rd(T0)) return false; }
        else if (m == "interp") { mode = INTERP; if (!rd(T0) || !rd(T1)) return false; }
        else return false;
        return true;
    }
    bool prepare(const Monodromy& mon) {
        if (ready) return true;
        if (mode == REF || mode == INTERP) {
            Rep r0; if (!raw_rep(mon, T0, r0)) return false;
            M0 = conformal_center(r0);
            if (mode == INTERP) {
                Rep r1; if (!raw_rep(mon, T1, r1)) return false;
                M1 = conformal_center(r1);
                D = matmul(M1, matinv(M0));
            }
        }
        ready = true; return true;
    }
    Mat at(const Rep& raw, double T) const {
        switch (mode) {
            case NATIVE: return conformal_center(raw);
            case NONE:   return identity();
            case REF:    return M0;
            case INTERP: {
                double k0 = kappa_of(T0), k1 = kappa_of(T1), k = kappa_of(T);
                double s = std::abs(k1 - k0) < 1e-15 ? 0.0 : (k - k0)/(k1 - k0);
                s = std::max(0.0, std::min(1.0, s));
                if (s <= 0) return M0;
                if (s >= 1) return M1;
                return matmul(mat_pow(D, s), M0);
            }
        }
        return identity();
    }
};

// =====================================================================================
// The scene: the program's renderers, with per-polyline tags
// =====================================================================================
struct Curves {
    const Renderer* r = nullptr;
    vector<float> tag;              // per polyline: d(o, g.o) of its root (the ball's sort key)
    vector<uint32_t> arc;           // per polyline: arc id = 2*root index + (letter == 'b')
    vector<float> cum;              // per point: arc length (on S^2, chords) from its arc's start
    vector<float> arclen;           // per arc id: total length
    vector<uint32_t> base;          // polylines of root 0 (the base arcs)
    size_t npts() const { return r ? r->px.size() : 0; }
};
static void finish_curves(Curves& c, const Renderer* r, vector<float>&& tag, vector<uint32_t>&& arc, size_t narcs) {
    c.r = r; c.tag = std::move(tag); c.arc = std::move(arc);
    c.cum.assign(r->px.size(), 0.0f); c.arclen.assign(std::max<size_t>(narcs, 1), 0.0f);
    c.base.clear();
    vector<double> run(c.arclen.size(), 0.0);
    for (size_t k = 0; k < r->npoly(); ++k) {
        uint32_t a = c.arc.empty() ? 0 : c.arc[k];
        if (a < 2) c.base.push_back((uint32_t)k);
        size_t b = r->poly_begin(k), e = r->poly_end(k);
        double L = run[a];
        c.cum[b] = (float)L;
        for (size_t i = b + 1; i < e; ++i) {
            double dx = r->px[i] - r->px[i-1], dy = r->py[i] - r->py[i-1], dz = r->pz[i] - r->pz[i-1];
            L += std::sqrt(dx*dx + dy*dy + dz*dz);
            c.cum[i] = (float)L;
        }
        run[a] = L;
    }
    for (size_t a = 0; a < run.size(); ++a) c.arclen[a] = (float)run[a];
}

// Renderer::draw_roots, with the polyline ranges of each root noted and without the final
// join_polylines (and without --stats).  Polylines never span two draw_arc calls (`open` is
// reset before each), so each polyline belongs to one root and one letter.
static void draw_roots_tagged(Renderer& R, const vector<Mat>& roots, const vector<double>& rootTag,
                              vector<float>& tag, vector<uint32_t>& arc) {
    R.done.set_limit(R.draw_mem());
    tag.clear(); arc.clear();
    for (size_t i = 0; i < roots.size(); ++i) {
        const Mat& M = roots[i];
        for (int l = 0; l < 2; ++l) {
            size_t p0 = R.pstart.size();
            R.open = false; R.draw_arc(M, l ? 'b' : 'a');
            for (size_t k = p0; k < R.pstart.size(); ++k) { tag.push_back((float)rootTag[i]); arc.push_back((uint32_t)(2*i + l)); }
        }
    }
    R.open = false;
    R.done = KeySet();
    R.stk.release();
    release_memory();
}

struct Scene {
    Monodromy mon;
    Rep rep;
    double T = 2; int order = 2; double R = 6, acc = 0.004, zoom = 1;
    std::unique_ptr<Renderer> rp, rn, rw;
    Curves cp, cn, cw;
    size_t nroots = 0;
    Pt Vp{0,0,1}, Vm{0,0,1};       // the attracting fixed points of c^-1 (Z+) and c (Z-)
    double tBuild = 0;
};
struct BuildSpec {
    string monName;
    double T = 2, R = 6, acc = 0.004;
    int overlap = 1;
    bool wheel = false; int wheelres = 10000000;
    bool cull = false, focus = true;
    double zoom = 1; View view = make_view(0, 0, 0); double lim = 1.05;
    Framing* framing = nullptr;
};

// representation for the spec (screened, centred and with the ball generators when R > 0),
// cached between frames of the same T
struct RepCache { bool have = false, ball = false; double T = 0; Rep rep; int order = -1; };
static RepCache g_rc;
static bool scene_rep(const BuildSpec& bs, Scene& sc, string& monName) {
    if (sc.mon.name.empty()) {
        static Monodromy monCache; static string monKey;
        if (monKey != bs.monName || monCache.name.empty()) {
            monName = bs.monName; monCache = Monodromy();
            if (!resolve_monodromy(monName, monCache)) return false;
            monKey = bs.monName;
        }
        sc.mon = monCache;
    }
    if (g_rc.have && g_rc.T == bs.T && (g_rc.ball || bs.R <= 0)) {
        sc.rep = g_rc.rep; sc.order = g_rc.order;
    } else {
        Rep raw;
        if (!raw_rep(sc.mon, bs.T, raw)) { fprintf(stderr, "failed to solve for the holonomy at T = %g\n", bs.T); return false; }
        int order;
        bool discrete = is_order(bs.T, order);
        if (bs.R > 0) {
            if (!discrete) { fprintf(stderr, "T = %g is not an integer order: only the base arcs (R = 0) can be drawn\n", bs.T); return false; }
            if (!screen_rep(raw, order, sc.mon.name)) return false;
        }
        if (!bs.framing->prepare(sc.mon)) return false;
        sc.rep = raw;
        conj_rep(sc.rep, bs.framing->at(raw, bs.T));
        sc.order = discrete ? order : -1;
        if (bs.R > 0) {
            auto bg = std::make_shared<BallGens>();
            make_ball_gens(sc.rep, order, *bg);
            sc.rep.ball = bg;
        }
        g_rc.have = true; g_rc.ball = bs.R > 0; g_rc.T = bs.T; g_rc.rep = sc.rep; g_rc.order = sc.order;
    }
    sc.Vp = project(attracting_fixed_point(sc.rep.Ci));
    sc.Vm = project(attracting_fixed_point(sc.rep.C));
    return true;
}

// build_zippers (zipper.cc), with draw_roots_tagged; R = 0 draws the root 1 alone
static bool build_scene(const BuildSpec& bs, Scene& sc) {
    auto t0 = Clock::now();
    string monName;
    if (!scene_rep(bs, sc, monName)) return false;
    sc.T = bs.T; sc.R = bs.R; sc.acc = bs.acc; sc.zoom = bs.zoom;
    Options opt;
    opt.monodromy = sc.mon.name; opt.order = sc.order < 0 ? 2 : sc.order;
    opt.accuracy = bs.acc; opt.radius = bs.R; opt.overlap = bs.overlap; opt.zoom = bs.zoom;
    opt.focus = bs.focus; opt.cull = bs.cull; opt.wheel = bs.wheel; opt.wheelres = bs.wheelres;
    opt.format = FMT_PNG; opt.size = 840;
    ZView zv{bs.view, bs.zoom, bs.lim};
    const ZView* zvp = (bs.cull && !bs.wheel) ? &zv : nullptr;
    if (bs.R > 0) {
        if (!check_radius(opt, sc.rep, zvp && will_focus(opt))) return false;
        if (bs.wheel && !check_wheel(opt, sc.rep)) return false;
    }
    sc.rp.reset(new Renderer(sc.rep)); sc.rn.reset(new Renderer(sc.rep));
    Renderer& rp = *sc.rp; Renderer& rn = *sc.rn;
    // ---- from build_zippers ----
    Mat Ti; bool focused = false;
    Pt axis{0, 0, 1};
    double focusL = 0;
    if (zvp) {
        axis = { zvp->v.m[6], zvp->v.m[7], zvp->v.m[8] };
        focusL = std::min(std::log(std::max(1.0, zvp->zoom)), std::max(0.0, opt.radius - 1.0));
        if (opt.focus && focusL > 1e-9) {
            Mat U = identity();
            if (1.0 + axis.z > 1e-9) {
                cpx zv0 = cpx(axis.x, axis.y)/(1.0 + axis.z);
                double nrm = std::sqrt(1.0 + std::norm(zv0));
                U = { zv0/nrm, cpx(-1.0/nrm, 0), cpx(1.0/nrm, 0), std::conj(zv0)/nrm };
            }
            double e = std::exp(focusL/2.0);
            Mat D = { cpx(e,0), cpx(0,0), cpx(0,0), cpx(1.0/e,0) };
            Ti = matinv(matmul(U, matmul(D, matinv(U))));
            focused = true;
        }
    }
    vector<Mat> roots;
    if (bs.R > 0) {
        bool capHit = false; size_t noDescent = 0;
        roots = enumerate_ball(sc.rep, opt.radius, focused ? &Ti : nullptr, ROOT_CAP, &capHit, &noDescent);
        if (capHit) fprintf(stderr, "warning: enumeration cap reached; picture truncated\n");
    } else roots.push_back(identity());
    sc.nroots = roots.size();
    vector<double> rtag(roots.size());
    for (size_t i = 0; i < roots.size(); ++i) {           // the ball's sort key, as a distance
        double k = frob2(roots[i]);
        if (focused) k = std::min(k, frob2(matmul(Ti, roots[i])));
        rtag[i] = std::acosh(std::max(1.0, k/2.0));
    }
    vector<float> tag[2]; vector<uint32_t> arc[2];
    int side = 0;
    for (Renderer* R : {&rp, &rn}) {
        R->accuracy = opt.accuracy;
        R->stats    = false;
        R->overlapLevels = opt.overlap;
        R->chainAccel = opt.chains;
        if (R == &rp) R->setup(sc.mon.phi_a, sc.mon.phi_b, sc.rep.Ci);
        else          R->setup(sc.mon.psi_a, sc.mon.psi_b, sc.rep.C);
        if (zvp) {
            double h = std::sqrt(2.0)*zvp->lim/zvp->zoom;
            R->cull = true; R->vaxis = axis;
            R->vrad = (h >= 1.0 ? PI/2 : std::asin(h)) + 2.0*opt.accuracy + 1e-3/std::max(1.0, zvp->zoom);
        }
        draw_roots_tagged(*R, roots, rtag, tag[side], arc[side]);
        if (R->memFull) fprintf(stderr, "warning: %s: drawing stopped at the memory limit\n", side ? "Z-" : "Z+");
        ++side;
    }
    // ---- end of build_zippers ----
    size_t narcs = 2*roots.size();
    vector<Mat>().swap(roots);
    finish_curves(sc.cp, &rp, std::move(tag[0]), std::move(arc[0]), narcs);
    finish_curves(sc.cn, &rn, std::move(tag[1]), std::move(arc[1]), narcs);
    if (bs.wheel) {
        sc.rw.reset(new Renderer(sc.rep));
        compute_wheel(rp, rn, opt.wheelres, *sc.rw);
        finish_curves(sc.cw, sc.rw.get(), vector<float>(sc.rw->npoly(), 0.0f), vector<uint32_t>(sc.rw->npoly(), 0), 1);
    }
    sc.tBuild = secs(t0, Clock::now());
    return true;
}

// =====================================================================================
// Rasteriser: per layer, a supersampled coverage buffer (one byte per sample: the largest
// alpha of the capsules covering it), box-filtered and composited over the frame.
// =====================================================================================
struct Raster {
    int W, H, SS, w, h;
    vector<uint8_t> s;             // samples of the current layer
    vector<float> img;             // the frame, RGB in [0,1]
    int bx0, by0, bx1, by1;        // dirty box (samples)
    Raster(int W_, int H_, int SS_) : W(W_), H(H_), SS(SS_), w(W_*SS_), h(H_*SS_),
        s((size_t)w*h, 0), img((size_t)W_*H_*3, 1.0f) { reset_box(); }
    void reset_box() { bx0 = w; by0 = h; bx1 = -1; by1 = -1; }
    void clear(const double bg[3]) {
        for (size_t i = 0; i < (size_t)W*H; ++i) for (int c = 0; c < 3; ++c) img[3*i + c] = (float)bg[c];
    }
    // capsule of radius r (samples) about the segment, sample value max(., v)
    void piece(double ax, double ay, double bx, double by, double r, uint8_t v) {
        int x0 = (int)std::floor(std::min(ax, bx) - r), x1 = (int)std::ceil(std::max(ax, bx) + r);
        int y0 = (int)std::floor(std::min(ay, by) - r), y1 = (int)std::ceil(std::max(ay, by) + r);
        x0 = std::max(x0, 0); y0 = std::max(y0, 0); x1 = std::min(x1, w - 1); y1 = std::min(y1, h - 1);
        if (x0 > x1 || y0 > y1) return;
        bx0 = std::min(bx0, x0); by0 = std::min(by0, y0); bx1 = std::max(bx1, x1); by1 = std::max(by1, y1);
        const double dx = bx - ax, dy = by - ay, L2 = dx*dx + dy*dy, r2 = r*r;
        const double inv = L2 > 1e-12 ? 1.0/L2 : 0.0;
        for (int y = y0; y <= y1; ++y) {
            const double py = y + 0.5 - ay;
            uint8_t* row = &s[(size_t)y*w];
            for (int x = x0; x <= x1; ++x) {
                const double px = x + 0.5 - ax;
                double t = (px*dx + py*dy)*inv;
                t = t < 0 ? 0 : (t > 1 ? 1 : t);
                const double ex = px - t*dx, ey = py - t*dy;
                if (ex*ex + ey*ey <= r2 && row[x] < v) row[x] = v;
            }
        }
    }
    void segment(double ax, double ay, double bx, double by, double r, uint8_t v) {
        // clip to the sample rectangle (expanded by r + 1), then split into short pieces
        double t0, t1;
        const double cxm = w/2.0, cym = h/2.0, Lx = w/2.0 + r + 1, Ly = h/2.0 + r + 1;
        // Liang-Barsky on the box centred at (cxm, cym)
        {
            t0 = 0; t1 = 1;
            const double d[2] = { bx - ax, by - ay }, p[2] = { ax - cxm, ay - cym }, L[2] = { Lx, Ly };
            for (int a = 0; a < 2; ++a)
                for (int sg = -1; sg <= 1; sg += 2) {
                    double num = L[a] - sg*p[a], den = sg*d[a];
                    if (den == 0) { if (num < 0) return; continue; }
                    double t = num/den;
                    if (den > 0) { if (t < t1) t1 = t; } else { if (t > t0) t0 = t; }
                    if (t0 > t1) return;
                }
        }
        double sx = ax + t0*(bx - ax), sy = ay + t0*(by - ay), ex = ax + t1*(bx - ax), ey = ay + t1*(by - ay);
        double len = std::hypot(ex - sx, ey - sy);
        int n = std::max(1, (int)std::ceil(len/6.0));
        for (int i = 0; i < n; ++i) {
            double u0 = (double)i/n, u1 = (double)(i + 1)/n;
            piece(sx + u0*(ex - sx), sy + u0*(ey - sy), sx + u1*(ex - sx), sy + u1*(ey - sy), r, v);
        }
    }
    void dot(double x, double y, double r, uint8_t v) { piece(x, y, x, y, r, v); }
    // box-filter the layer and composite it with colour col and opacity a; clears the samples
    void composite(const double col[3], double a) {
        if (bx1 < 0) return;
        int X0 = bx0/SS, Y0 = by0/SS, X1 = std::min(W - 1, bx1/SS), Y1 = std::min(H - 1, by1/SS);
        const float norm = (float)(a/(255.0*SS*SS));
        for (int Y = Y0; Y <= Y1; ++Y)
            for (int X = X0; X <= X1; ++X) {
                int sum = 0;
                for (int dy = 0; dy < SS; ++dy) {
                    uint8_t* row = &s[(size_t)(Y*SS + dy)*w + X*SS];
                    for (int dx = 0; dx < SS; ++dx) { sum += row[dx]; row[dx] = 0; }
                }
                if (!sum) continue;
                float c = sum*norm;
                float* p = &img[3*((size_t)Y*W + X)];
                for (int k = 0; k < 3; ++k) p[k] = p[k]*(1 - c) + (float)col[k]*c;
            }
        reset_box();
    }
    void to_rgb(vector<uint8_t>& out) const {
        out.resize((size_t)W*H*3);
        for (size_t i = 0; i < out.size(); ++i) {
            float v = img[i]; v = v < 0 ? 0 : (v > 1 ? 1 : v);
            out[i] = (uint8_t)std::lround(v*255.0f);
        }
    }
};

// =====================================================================================
// PNG in and out
// =====================================================================================
static bool png_write(const string& fn, const vector<uint8_t>& rgb, int W, int H, int level) {
    const size_t rowlen = 1 + (size_t)W*3;
    vector<uint8_t> raw((size_t)H*rowlen);
    for (int y = 0; y < H; ++y) {                    // filter "Sub" (smaller files than none)
        uint8_t* row = &raw[(size_t)y*rowlen]; row[0] = 1;
        const uint8_t* src = &rgb[(size_t)y*W*3];
        for (int i = 0; i < W*3; ++i) row[1 + i] = (uint8_t)(src[i] - (i >= 3 ? src[i-3] : 0));
    }
    uLongf clen = compressBound((uLong)raw.size());
    vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), (uLong)raw.size(), level) != Z_OK) return false;
    string tmp = fn + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb"); if (!f) return false;
    const uint8_t sig[8] = {137,80,78,71,13,10,26,10}; fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = { (uint8_t)(W>>24),(uint8_t)(W>>16),(uint8_t)(W>>8),(uint8_t)W,
                         (uint8_t)(H>>24),(uint8_t)(H>>16),(uint8_t)(H>>8),(uint8_t)H, 8,2,0,0,0 };
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", comp.data(), clen);
    png_chunk(f, "IEND", nullptr, 0);
    if (fclose(f) != 0) return false;
    return rename(tmp.c_str(), fn.c_str()) == 0;
}
// 8-bit RGB / RGBA (composited over white) / grey, non-interlaced
static bool png_read(const string& fn, vector<uint8_t>& rgb, int& W, int& H) {
    FILE* f = fopen(fn.c_str(), "rb"); if (!f) return false;
    vector<uint8_t> d; uint8_t buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    fclose(f);
    if (d.size() < 33 || memcmp(d.data(), "\x89PNG\r\n\x1a\n", 8)) return false;
    auto be = [&](size_t p) { return (uint32_t)d[p] << 24 | (uint32_t)d[p+1] << 16 | (uint32_t)d[p+2] << 8 | d[p+3]; };
    size_t p = 8; int depth = 0, ctype = 0, inter = 0; vector<uint8_t> idat;
    while (p + 8 <= d.size()) {
        uint32_t len = be(p); string type((const char*)&d[p+4], 4);
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
    vector<uint8_t> raw((size_t)H*(stride + 1));
    uLongf rl = raw.size();
    if (uncompress(raw.data(), &rl, idat.data(), idat.size()) != Z_OK || rl != raw.size()) return false;
    vector<uint8_t> px((size_t)H*stride);
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

// =====================================================================================
// The shot
// =====================================================================================
struct LayerDef {
    string name;
    double col[3] = {0, 0, 0};
    double width = 1.0;
    Track opacity, reveal;
    double pen = 0;                 // radius (px) of a dot at the reveal front (0: none)
    bool depth = false;             // subject to the depth threshold
};
struct Shot {
    Cfg cfg;
    int frames = 1, W = 840, H = 840, SS = 3, pngLevel = 3;
    double radiusPx = 418, fog = 0.0, bg[3] = {1, 1, 1};
    string outdir, prefix = "frame_";
    int first = 0;                  // the number of frame 0 in the file names
    BuildSpec bs;
    Framing framing;
    bool perframe = false;          // rebuild every frame (zoom paths: detail grows with the zoom)
    double accMin = 1e-6, accExp = 1.0;
    double zoomStep = 0;            // perframe: build at zoom zoomStep^floor(log zoom / log zoomStep)
    Track T, yaw, pitch, roll, zoom, depth, ox, oy;
    double depthFade = 0.25;
    string lookAt;                  // plus | minus | "x y z" | empty
    vector<LayerDef> layers;
    bool dynamic() const { return perframe || !T.constant(); }
};

static bool lookat_point(const Shot& sh, const Scene& sc, Pt& p) {
    if (sh.lookAt.empty()) return false;
    if (sh.lookAt == "plus") { p = sc.Vp; return true; }
    if (sh.lookAt == "minus") { p = sc.Vm; return true; }
    if (sh.lookAt == "mid") { p = {sc.Vp.x + sc.Vm.x, sc.Vp.y + sc.Vm.y, sc.Vp.z + sc.Vm.z}; }
    if (sh.lookAt == "arcs") {                   // the centroid of the base arcs (both zippers)
        double x = 0, y = 0, z = 0; size_t n = 0;
        for (const Curves* C : {&sc.cp, &sc.cn}) {
            if (!C->r) continue;
            for (uint32_t k : C->base)
                for (size_t i = C->r->poly_begin(k); i < C->r->poly_end(k); ++i) { x += C->r->px[i]; y += C->r->py[i]; z += C->r->pz[i]; ++n; }
        }
        if (!n) { p = sc.Vp; return true; }        // (not built yet: perframe builds use V+)
        p = {x, y, z};
    }
    if (sh.lookAt == "mid" || sh.lookAt == "arcs") {
        double n = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
        if (!(n > 1e-9)) { p = sc.Vp; return true; }
        p = {p.x/n, p.y/n, p.z/n}; return true;
    }
    std::istringstream is(sh.lookAt); double x, y, z;
    if (!(is >> x >> y >> z)) { fprintf(stderr, "look_at: plus, minus or x y z\n"); exit(1); }
    double n = std::sqrt(x*x + y*y + z*z); p = {x/n, y/n, z/n}; return true;
}
// the view at frame fr: make_view(yaw0 + yaw, pitch0 + pitch, roll), where (yaw0, pitch0) put
// the look_at point at the centre (make_view's third row is the world point seen at the centre)
static View frame_view(const Shot& sh, const Scene* sc, double fr) {
    double y0 = 0, p0 = 0;
    Pt p;
    if (sc && lookat_point(sh, *sc, p)) {
        y0 = -std::asin(std::max(-1.0, std::min(1.0, p.x)))*180.0/PI;
        p0 = std::atan2(p.y, p.z)*180.0/PI;
    }
    return make_view(y0 + sh.yaw.at(fr), p0 + sh.pitch.at(fr), sh.roll.at(fr));
}

static bool load_shot(Shot& sh, const string& file, const vector<string>& overrides) {
    Cfg& c = sh.cfg;
    if (!c.load(file)) return false;
    for (auto& o : overrides) if (!c.add_line(o, 0)) return false;
    g_fps = c.num("fps", 30);
    if (c.has("timing") && !load_timing(c.str("timing"))) return false;
    sh.frames = (int)c.num("frames", 1);
    if (c.has("start")) {
        double t0; if (!time_of(c.str("start"), t0)) return false;
        g_startFrame = frame_at(t0);
        sh.first = (int)g_startFrame;
        if (c.has("end")) {
            double t1; if (!time_of(c.str("end"), t1)) return false;
            sh.frames = (int)(frame_at(t1) - g_startFrame);
            if (c.has("frames")) fprintf(stderr, "note: frames is ignored (start and end are given)\n");
        }
    } else if (c.has("end")) { fprintf(stderr, "end needs start\n"); return false; }
    if (sh.frames < 1) { fprintf(stderr, "frames must be >= 1\n"); return false; }
    sh.W = (int)c.num("width", c.num("size", 840)); sh.H = (int)c.num("height", c.num("size", 840));
    sh.SS = (int)c.num("supersample", 3);
    sh.radiusPx = c.num("radius_px", sh.W/2.0 - 2.0);
    sh.fog = c.num("fog", 0.35);
    auto bg = c.nums("background", {1, 1, 1}); for (int k = 0; k < 3; ++k) sh.bg[k] = bg[k % bg.size()];
    sh.pngLevel = (int)c.num("png_level", 3);
    sh.outdir = c.str("out", "");
    sh.prefix = c.str("prefix", "frame_");
    sh.first = (int)c.num("first", sh.first);
    BuildSpec& b = sh.bs;
    b.monName = c.str("monodromy", "fig8");
    string ord = c.str("order", "");
    sh.T = get_track(c, "T", ord.empty() ? "2" : (ord == "inf" || ord == "0" || ord == "cusp" ? "inf" : ord), sh.frames, SP_INV);
    if (c.has("order") && c.has("T")) fprintf(stderr, "note: both order and T given; T wins\n");
    b.R = c.num("R", 6);
    b.acc = c.num("accuracy", 0.004);
    b.overlap = (int)c.num("overlap", 1);
    b.wheel = c.num("wheel", 0) != 0;
    b.wheelres = (int)c.num("wheelres", 10000000);
    b.focus = c.num("focus", 1) != 0;
    string bm = c.str("build", "once");
    if (bm == "perframe") sh.perframe = true; else if (bm != "once") { fprintf(stderr, "build: once or perframe\n"); return false; }
    b.cull = c.num("cull", sh.perframe ? 1 : 0) != 0;
    sh.accMin = c.num("accuracy_min", 1e-6);
    sh.accExp = c.num("accuracy_zoom_exponent", 1.0);
    sh.zoomStep = c.num("zoom_step", 0);
    if (!sh.framing.parse(c.str("center", "native"))) { fprintf(stderr, "center: native | ref T0 | interp T0 T1 | none\n"); return false; }
    b.framing = &sh.framing;
    sh.yaw = get_track(c, "yaw", "0", sh.frames);
    sh.pitch = get_track(c, "pitch", "0", sh.frames);
    sh.roll = get_track(c, "roll", "0", sh.frames);
    sh.zoom = get_track(c, "zoom", "1", sh.frames, SP_LOG);
    sh.depth = get_track(c, "depth", "inf", sh.frames);
    sh.depthFade = c.num("depth_fade", 0.25);
    sh.lookAt = c.str("look_at", "");
    // layers
    struct Def { const char* name; double r, g, b, w, op; bool depth; };
    const Def defs[] = {
        {"circle",     0.0,  0.0,  0.0,  1.0, 0.0, false},
        {"zplus",      0.05, 0.15, 0.85, 1.0, 1.0, true},
        {"zminus",     0.85, 0.08, 0.08, 1.0, 1.0, true},
        {"wheel",      0.0,  0.6,  0.0,  1.5, 1.0, false},
        {"arcs_plus",  0.05, 0.15, 0.85, 2.5, 0.0, false},
        {"arcs_minus", 0.85, 0.08, 0.08, 2.5, 0.0, false},
    };
    std::istringstream ls(c.str("layers", "circle zplus zminus wheel arcs_plus arcs_minus"));
    string ln;
    while (ls >> ln) {
        const Def* d = nullptr;
        for (auto& x : defs) if (ln == x.name) d = &x;
        if (!d) { fprintf(stderr, "unknown layer %s\n", ln.c_str()); return false; }
        LayerDef L; L.name = ln;
        auto col = c.nums(ln + ".color", {d->r, d->g, d->b});
        if (col.size() != 3) { fprintf(stderr, "%s.color: three numbers\n", ln.c_str()); return false; }
        for (int k = 0; k < 3; ++k) L.col[k] = col[k];
        L.width = c.num(ln + ".width", d->w);
        char op[32]; snprintf(op, sizeof op, "%g", (ln == "wheel" && !b.wheel) ? 0.0 : d->op);
        L.opacity = get_track(c, ln + ".opacity", op, sh.frames);
        L.reveal = get_track(c, ln + ".reveal", "1", sh.frames);
        L.pen = c.num(ln + ".pen", 0.0);
        L.depth = c.num(ln + ".depth", d->depth ? 1 : 0) != 0;
        sh.layers.push_back(L);
    }
    if (sh.dynamic() && b.wheel) { fprintf(stderr, "wheel = 1 needs a static scene (no T track, build = once)\n"); return false; }
    if (!sh.T.constant() && b.R > 0) { fprintf(stderr, "a T track (T-morph) draws the base arcs only: set R = 0\n"); return false; }
    return true;
}

// ---- drawing one layer ----
struct FrameCtx {
    View v; double zoom, cx, cy, rad;     // rad: unit-disk radius in samples
    double fog;
};
static inline bool proj(const FrameCtx& F, double x, double y, double z, double& X, double& Y, double& Zd) {
    double a, b, c; vrot(F.v, (float)x, (float)y, (float)z, a, b, c);
    X = F.cx + a*F.zoom*F.rad; Y = F.cy - b*F.zoom*F.rad; Zd = c;
    return c > 0;
}
static void draw_curves(Raster& ras, const FrameCtx& F, const Curves& C, const LayerDef& L, double fr,
                        bool baseOnly, double depth, double fade) {
    const Renderer* r = C.r;
    if (!r || r->px.empty()) return;
    const double rr = 0.5*L.width*ras.SS;
    const double rev = L.reveal.at(fr);
    const bool partial = rev < 1.0 - 1e-9;
    const size_t np = baseOnly ? C.base.size() : r->npoly();
    vector<std::pair<double,double>> pens;
    for (size_t q = 0; q < np; ++q) {
        size_t k = baseOnly ? C.base[q] : q;
        double a = 1.0;
        if (L.depth && !C.tag.empty() && std::isfinite(depth)) {
            double t = C.tag[k];
            if (t > depth) {
                if (fade <= 0) continue;
                a = 1.0 - (t - depth)/fade;            // fully drawn at tag <= depth, fading in above it
                if (a <= 0) continue;
                a = a*a*(3 - 2*a);
            }
        }
        uint32_t arc = C.arc.empty() ? 0 : C.arc[k];
        double lim = partial ? rev*C.arclen[arc] : INFINITY;
        size_t b = r->poly_begin(k), e = r->poly_end(k);
        if (partial && C.cum[b] >= lim) continue;
        double X0, Y0, Z0; bool ok0 = proj(F, r->px[b], r->py[b], r->pz[b], X0, Y0, Z0);
        for (size_t i = b + 1; i < e; ++i) {
            double X1, Y1, Z1; bool ok1;
            bool cut = partial && C.cum[i] > lim;
            if (cut) {
                double u = (lim - C.cum[i-1])/std::max(1e-30f, C.cum[i] - C.cum[i-1]);
                double x = r->px[i-1] + u*(r->px[i] - r->px[i-1]), y = r->py[i-1] + u*(r->py[i] - r->py[i-1]),
                       z = r->pz[i-1] + u*(r->pz[i] - r->pz[i-1]);
                ok1 = proj(F, x, y, z, X1, Y1, Z1);
                if (ok1 && L.pen > 0) pens.push_back({X1, Y1});
            } else ok1 = proj(F, r->px[i], r->py[i], r->pz[i], X1, Y1, Z1);
            if (ok0 && ok1) {
                double al = a;
                if (F.fog > 0) al *= 1.0 - F.fog*(1.0 - 0.5*(Z0 + Z1));
                uint8_t v = (uint8_t)std::lround(255.0*std::max(0.0, std::min(1.0, al)));
                if (v) ras.segment(X0, Y0, X1, Y1, rr, v);
            }
            if (cut) break;
            X0 = X1; Y0 = Y1; Z0 = Z1; ok0 = ok1;
        }
    }
    for (auto& p : pens) ras.dot(p.first, p.second, L.pen*ras.SS, 255);
}
static void draw_circle(Raster& ras, const FrameCtx& F, const LayerDef& L) {
    double R = F.zoom*F.rad;
    if (R > 2.0*(ras.w + ras.h)) return;               // wholly out of view
    int n = (int)std::min(1 << 20, std::max(720, (int)(R*0.5)));
    double rr = 0.5*L.width*ras.SS;
    for (int i = 0; i < n; ++i) {
        double a0 = 2*PI*i/n, a1 = 2*PI*(i + 1)/n;
        double x0 = F.cx + R*std::cos(a0), y0 = F.cy + R*std::sin(a0), x1 = F.cx + R*std::cos(a1), y1 = F.cy + R*std::sin(a1);
        if (std::max(x0, x1) < -rr || std::min(x0, x1) > ras.w + rr || std::max(y0, y1) < -rr || std::min(y0, y1) > ras.h + rr) continue;
        ras.segment(x0, y0, x1, y1, rr, 255);
    }
}
static void render_frame(const Shot& sh, const Scene& sc, double fr, Raster& ras) {
    FrameCtx F;
    F.v = frame_view(sh, &sc, fr);
    F.zoom = sh.zoom.at(fr);
    F.cx = ras.w/2.0; F.cy = ras.h/2.0; F.rad = sh.radiusPx*ras.SS;
    F.fog = sh.fog;
    ras.clear(sh.bg);
    const double depth = sh.depth.at(fr);
    for (const LayerDef& L : sh.layers) {
        double op = L.opacity.at(fr);
        if (op <= 0) continue;
        if (L.name == "circle") draw_circle(ras, F, L);
        else if (L.name == "zplus") draw_curves(ras, F, sc.cp, L, fr, false, depth, sh.depthFade);
        else if (L.name == "zminus") draw_curves(ras, F, sc.cn, L, fr, false, depth, sh.depthFade);
        else if (L.name == "wheel") draw_curves(ras, F, sc.cw, L, fr, false, depth, sh.depthFade);
        else if (L.name == "arcs_plus") draw_curves(ras, F, sc.cp, L, fr, true, depth, sh.depthFade);
        else if (L.name == "arcs_minus") draw_curves(ras, F, sc.cn, L, fr, true, depth, sh.depthFade);
        ras.composite(L.col, std::min(1.0, op));
    }
}

// the build for frame fr (dynamic shots): T, and with build = perframe the zoom, view and accuracy
static BuildSpec frame_spec(const Shot& sh, const Scene* scPrev, double fr) {
    BuildSpec b = sh.bs;
    b.T = sh.T.at(fr);
    int o; if (is_order(b.T, o)) b.T = o == 0 ? INFINITY : (double)o;
    if (sh.perframe) {
        b.zoom = sh.zoom.at(fr);
        if (sh.zoomStep > 1 && b.zoom > 1)      // (rounded down: the culled window holds the view)
            b.zoom = std::pow(sh.zoomStep, std::floor(std::log(b.zoom)/std::log(sh.zoomStep) + 1e-9));
        b.acc = std::max(sh.accMin, sh.bs.acc/std::pow(std::max(1.0, b.zoom), sh.accExp));
        b.view = frame_view(sh, scPrev, fr);
        b.lim = 1.05*(sh.W/2.0)/sh.radiusPx;
    }
    return b;
}
static bool same_spec(const BuildSpec& a, const BuildSpec& b) {
    if (a.T != b.T || a.acc != b.acc || a.zoom != b.zoom || a.R != b.R) return false;
    for (int i = 0; i < 9; ++i) if (a.view.m[i] != b.view.m[i]) return false;
    return true;
}

static int run_shot(Shot& sh, const string& outdir, int nJobs, int fA, int fB) {
    mkdir(outdir.c_str(), 0755);
    struct stat st;
    if (stat(outdir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) { fprintf(stderr, "cannot create %s\n", outdir.c_str()); return 1; }
    fA = std::max(0, fA); fB = std::min(sh.frames - 1, fB);
    auto tStart = Clock::now();
    std::unique_ptr<Scene> sc;
    if (!sh.dynamic()) {                         // build once, before forking (shared copy-on-write)
        sc.reset(new Scene);
        BuildSpec b = sh.bs;
        b.T = sh.T.at(0);
        int o; if (is_order(b.T, o)) b.T = o == 0 ? INFINITY : (double)o;
        if (b.cull) {
            double bf = sh.cfg.num("build_frame", 0);
            b.zoom = sh.cfg.num("build_zoom", sh.zoom.at(bf));
            // the scene is needed for look_at: resolve the representation first
            Scene tmp; string mn;
            if (!scene_rep(b, tmp, mn)) return 1;
            b.view = frame_view(sh, &tmp, bf);
            b.lim = 1.05*(sh.W/2.0)/sh.radiusPx;
        }
        if (!build_scene(b, *sc)) return 1;
        if (!g_quiet)
            fprintf(stderr, "built: %s T=%g R=%g a=%g: %zu roots, Z+ %zu points / %zu polylines, Z- %zu / %zu%s, %.2f s\n",
                    sc->mon.name.c_str(), sc->T, sc->R, sc->acc, sc->nroots, sc->rp->px.size(), sc->rp->npoly(),
                    sc->rn->px.size(), sc->rn->npoly(), sc->rw ? ", wheel" : "", sc->tBuild);
    }
    sh.cfg.warn_unused();
    vector<pid_t> kids;
    int me = 0;
    if (nJobs > 1) {
        fflush(stderr); fflush(stdout);
        for (int j = 0; j < nJobs; ++j) {
            pid_t p = fork();
            if (p < 0) { perror("fork"); return 1; }
            if (p == 0) { me = j; kids.clear(); break; }
            kids.push_back(p);
        }
        if (!kids.empty()) {                     // the parent waits
            int bad = 0;
            for (pid_t p : kids) { int s; waitpid(p, &s, 0); if (!WIFEXITED(s) || WEXITSTATUS(s)) ++bad; }
            if (!g_quiet) fprintf(stderr, "shot: %d frames in %.1f s (%d jobs)%s\n", fB - fA + 1, secs(tStart, Clock::now()), nJobs, bad ? ", FAILED" : "");
            return bad ? 1 : 0;
        }
    }
    Raster ras(sh.W, sh.H, sh.SS);
    vector<uint8_t> rgb;
    std::unique_ptr<Scene> dyn;
    BuildSpec last; bool haveLast = false;
    double tB = 0, tR = 0, tW = 0; int nB = 0, nF = 0;
    for (int f = fA; f <= fB; ++f) {
        if (nJobs > 1) {         // static: interleaved; dynamic: contiguous blocks (builds are reused)
            const int n = fB - fA + 1;
            if (sc ? (f - fA) % nJobs != me
                   : (f - fA < (long)n*me/nJobs || f - fA >= (long)n*(me + 1)/nJobs)) continue;
        }
        const Scene* use = sc.get();
        if (!use) {
            // a dynamic build: the view for culling needs the look_at point of this T
            BuildSpec b = frame_spec(sh, nullptr, f);
            if (!sh.lookAt.empty() && sh.perframe) {
                Scene tmp; string mn;
                if (!scene_rep(b, tmp, mn)) return 1;
                b = frame_spec(sh, &tmp, f);
            }
            if (!haveLast || !same_spec(b, last)) {
                dyn.reset(new Scene);
                if (!build_scene(b, *dyn)) return 1;
                last = b; haveLast = true; tB += dyn->tBuild; ++nB;
            }
            use = dyn.get();
        }
        auto t1 = Clock::now();
        render_frame(sh, *use, f, ras);
        ras.to_rgb(rgb);
        auto t2 = Clock::now();
        char name[64]; snprintf(name, sizeof name, "%s%05d.png", sh.prefix.c_str(), sh.first + f);
        if (!png_write(outdir + "/" + name, rgb, sh.W, sh.H, sh.pngLevel)) { fprintf(stderr, "cannot write %s\n", name); return 1; }
        auto t3 = Clock::now();
        tR += secs(t1, t2); tW += secs(t2, t3); ++nF;
    }
    if (!g_quiet && nF)
        fprintf(stderr, "job %d: %d frames: render %.3f s/frame, png %.3f s/frame%s\n", me, nF, tR/nF, tW/nF,
                nB ? (", builds " + std::to_string(nB) + " at " + std::to_string(tB/nB).substr(0, 6) + " s each").c_str() : "");
    if (nJobs <= 1 && !g_quiet) fprintf(stderr, "shot: %d frames in %.1f s\n", nF, secs(tStart, Clock::now()));
    return 0;
}

// =====================================================================================
// Assembler: a timeline of frame directories with crossfades
//   clip DIR [FIRST LAST]   append frames FIRST..LAST (default all) of DIR (sorted *.png)
//   xfade N [ease]          the next clip starts N frames before the end of the previous one,
//                           and the overlap is crossfaded (ease: smooth (default) or linear)
//   hold N                  repeat the last frame N times
//   blank N [r g b]         N frames of a plain colour (default white)
// =====================================================================================
static vector<string> list_pngs(const string& dir) {
    vector<string> v;
    DIR* d = opendir(dir.c_str());
    if (!d) return v;
    while (dirent* e = readdir(d)) {
        string n = e->d_name;
        if (n.size() > 4 && n.substr(n.size() - 4) == ".png" && n[0] != '.') v.push_back(dir + "/" + n);
    }
    closedir(d);
    std::sort(v.begin(), v.end());
    return v;
}
static int run_assemble(const string& tl, const string& outdir) {
    std::ifstream in(tl);
    if (!in) { fprintf(stderr, "cannot read %s\n", tl.c_str()); return 1; }
    mkdir(outdir.c_str(), 0755);
    struct Item { string kind, dir; int a = 0, b = -1, n = 0; Ease e = E_SMOOTH; double col[3] = {1, 1, 1}; };
    vector<Item> items;
    string line;
    while (std::getline(in, line)) {
        size_t h = line.find('#'); if (h != string::npos) line = line.substr(0, h);
        std::istringstream is(line); Item it;
        if (!(is >> it.kind)) continue;
        if (it.kind == "clip") {
            if (!(is >> it.dir)) { fprintf(stderr, "clip DIR [FIRST LAST]\n"); return 1; }
            if (is >> it.a) { if (!(is >> it.b)) it.b = it.a; }
        } else if (it.kind == "xfade") {
            string e; is >> it.n; if (is >> e && !parse_ease(e, it.e)) { fprintf(stderr, "xfade: unknown easing %s\n", e.c_str()); return 1; }
        } else if (it.kind == "hold") is >> it.n;
        else if (it.kind == "blank") { is >> it.n; double r, g, b; if (is >> r >> g >> b) { it.col[0] = r; it.col[1] = g; it.col[2] = b; } }
        else { fprintf(stderr, "unknown timeline item %s\n", it.kind.c_str()); return 1; }
        items.push_back(it);
    }
    // the output is built as a list of frame recipes, then rendered
    struct Src { int kind = 0; string file; double col[3] = {1, 1, 1}; };     // 0 file, 1 colour
    struct Rec { Src a, b; double w = 0; };     // w: weight of b
    vector<Rec> out;
    int pendingX = 0; Ease pendingE = E_SMOOTH;
    for (auto& it : items) {
        vector<Src> srcs;
        if (it.kind == "xfade") { pendingX = it.n; pendingE = it.e; continue; }
        if (it.kind == "hold") { if (out.empty()) continue; for (int i = 0; i < it.n; ++i) { Rec r = out.back(); out.push_back(r); } continue; }
        if (it.kind == "blank") { for (int i = 0; i < it.n; ++i) { Src s; s.kind = 1; for (int k = 0; k < 3; ++k) s.col[k] = it.col[k]; srcs.push_back(s); } }
        if (it.kind == "clip") {
            auto files = list_pngs(it.dir);
            if (files.empty()) { fprintf(stderr, "no frames in %s\n", it.dir.c_str()); return 1; }
            int b = it.b < 0 ? (int)files.size() - 1 : std::min(it.b, (int)files.size() - 1);
            for (int i = std::max(0, it.a); i <= b; ++i) { Src s; s.file = files[i]; srcs.push_back(s); }
        }
        int X = std::min({pendingX, (int)out.size(), (int)srcs.size()});
        size_t base = out.size() - X;
        for (int i = 0; i < X; ++i) {                // crossfade the overlap
            Rec& r = out[base + i];
            if (r.w > 0) { fprintf(stderr, "crossfades may not overlap\n"); return 1; }
            r.b = srcs[i]; r.w = ease(pendingE, (i + 1.0)/(X + 1.0));
        }
        for (size_t i = X; i < srcs.size(); ++i) { Rec r; r.a = srcs[i]; out.push_back(r); }
        pendingX = 0;
    }
    int W = 0, H = 0;
    auto load = [&](const Src& s, vector<uint8_t>& px) {
        if (s.kind == 1) {
            if (!W) { W = H = 840; }
            px.resize((size_t)W*H*3);
            for (size_t i = 0; i < (size_t)W*H; ++i) for (int k = 0; k < 3; ++k) px[3*i + k] = (uint8_t)std::lround(255*s.col[k]);
            return true;
        }
        int w, h;
        if (!png_read(s.file, px, w, h)) { fprintf(stderr, "cannot read %s\n", s.file.c_str()); return false; }
        if (W && (w != W || h != H)) { fprintf(stderr, "%s: size %dx%d differs from %dx%d\n", s.file.c_str(), w, h, W, H); return false; }
        W = w; H = h; return true;
    };
    for (auto& r : out) if (r.a.kind == 0) { vector<uint8_t> px; if (!load(r.a, px)) return 1; break; }
    vector<uint8_t> A, B;
    for (size_t i = 0; i < out.size(); ++i) {
        if (!load(out[i].a, A)) return 1;
        if (out[i].w > 0) {
            if (!load(out[i].b, B)) return 1;
            double w = out[i].w;
            for (size_t j = 0; j < A.size(); ++j) A[j] = (uint8_t)std::lround(A[j]*(1 - w) + B[j]*w);
        }
        char name[64]; snprintf(name, sizeof name, "/frame_%05zu.png", i);
        if (!png_write(outdir + name, A, W, H, 3)) { fprintf(stderr, "cannot write %s%s\n", outdir.c_str(), name); return 1; }
    }
    fprintf(stderr, "assembled %zu frames into %s\n", out.size(), outdir.c_str());
    return 0;
}

// =====================================================================================
#ifndef ZMOVIE_NO_MAIN
static void zm_usage() {
    fprintf(stderr,
"usage: zmovie SHOT.txt [key=value ...] [-o DIR] [-j N] [--frames A:B] [--quiet]\n"
"       zmovie --assemble TIMELINE.txt OUTDIR\n"
"  -o DIR        frame directory (else the shot's 'out')\n"
"  -j N          render with N processes\n"
"  --frames A:B  render frames A..B only\n"
"  key=value     override a key of the shot file (e.g. frames=240)\n");
}
int main(int argc, char** argv) {
    if (argc < 2) { zm_usage(); return 1; }
    string a1 = argv[1];
    if (a1 == "--assemble") {
        if (argc != 4) { zm_usage(); return 1; }
        return run_assemble(argv[2], argv[3]);
    }
    if (a1 == "-h" || a1 == "--help") { zm_usage(); return 0; }
    string shotFile = a1, outdir;
    vector<string> ov;
    int jobs = 1, fA = 0, fB = 1 << 30;
    for (int i = 2; i < argc; ++i) {
        string s = argv[i];
        if (s == "-o" && i + 1 < argc) outdir = argv[++i];
        else if (s == "-j" && i + 1 < argc) jobs = std::max(1, atoi(argv[++i]));
        else if (s == "--frames" && i + 1 < argc) {
            string r = argv[++i]; size_t c = r.find(':');
            if (c == string::npos) fA = fB = atoi(r.c_str()); else { fA = atoi(r.substr(0, c).c_str()); fB = atoi(r.substr(c + 1).c_str()); }
        }
        else if (s == "--quiet") g_quiet = true;
        else if (s == "--timing") g_timing = true;
        else if (s.find('=') != string::npos) ov.push_back(s);
        else { fprintf(stderr, "unknown argument %s\n", s.c_str()); zm_usage(); return 1; }
    }
    Shot sh;
    if (!load_shot(sh, shotFile, ov)) return 1;
    if (outdir.empty()) outdir = sh.outdir;
    if (outdir.empty()) { fprintf(stderr, "no output directory (-o DIR or out = DIR)\n"); return 1; }
    return run_shot(sh, outdir, jobs, fA, fB);
}
#endif
