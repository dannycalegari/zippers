// SPDX-License-Identifier: GPL-3.0-or-later
//
// session.h: the session file, format 1: the draw command of a picture with every option written out, as text. Read by
// `zippers draw --session FILE`, which remakes the picture, and written beside each picture that the window saves (the
// key p of zview). namespace zippers::session.
//
// THE FILE. Text. The first line is "zippers session 1": the format and its version. Then lines "KEY = VALUE" (the key
// ends at the first '='; blanks around the key and the value are dropped); empty lines and lines that begin with '#'
// are comments. The keys:
//   made_by        the suite's version and the build that wrote the file (system, processor, compiler), for
//                  information
//   example        the example: fig8 or a word of R and L (S^2), cartwright-steger (S^3), imm-m5 (S^4). The first key
//                  after made_by
//   the draw keys  each an option of zippers draw for the example, named without its dashes, with its values as on a
//                  command line: R = 2.554, pole = 10 5, viewmatrix = 1,0,..., colour = Z+=0,0,1 (one line for each
//                  colour); a flag is yes or no (C = yes). KEYS below lists them, with their defaults. Every option of
//                  the picture is written, the defaults included, so that a later version of the suite that changes a
//                  default still remakes the picture; the options that have no default value (centre, viewmatrix,
//                  pairs, focus, cullk, only) are written when they are given. Not draw keys: the outputs (-o,
//                  --polylines), the options that only print (--check, --timing, and --stats in S^3 and S^4) and
//                  --format (the picture's name decides it). In S^2 --stats is a draw key: the memory of its tables
//                  counts against the drawing's limit, so it can change a picture that reaches the limit
//   sha256.f       the SHA-256 of the substitution file (S^3, S^4) that the data search found when the session was
//                  written; sha256.pairs that of the file of --pairs (S^3). zippers draw --session refuses another file
//   window.KEY     the window's own keys (its layers, views, panel): kept, and never options of the programs
// Numbers are written as the shortest decimals that read back to the same doubles (polylines::exact).
//
// THE VERSIONS. A reader of format N reads the formats 1 to N, and refuses a later one ("this session is of format 2,
// written by a newer zippers; this one reads format 1"). Within a format an unknown draw key is an error, since it would
// change the picture; an unknown window key is a warning, and is ignored. A key added with a default that keeps the old
// pictures does not change the format; a key that changes meaning, or is renamed or removed, does.
//
//   KEYS               the draw keys: name, spheres, kind, the default of each sphere (null: written when given)
//   Session            the file in memory: made_by, the example, the draw keys and the window keys, in order
//   from_args          a session from a zippers draw command line (EXAMPLE OPTIONS...), every default written out; for
//                      a picture (-o), the defaults of a zoomed still (culling and the focus ball) written as the options
//                      they stand for; in S^2, --stats as a draw key
//   apply              options given after a session replace the session's (by key; a colour by its name; the view
//                      of --viewmatrix and that of the angles replace each other, the angles not given then taking
//                      the programs' defaults); the outputs and the options that only print are returned, to be passed
//                      on
//   args_of            the zippers draw command line of a session: EXAMPLE, then its options
//   write, read        the file (read: false and a message if it is not a session file of format 1 to VERSION, or a
//                      key or a value is wrong; the warnings, for window keys that are not known)
//   made_by_text       "zippers VERSION, SYSTEM PROCESSOR, COMPILER"
//   hash_data_file     the SHA-256 of a data file, found by the data search (core/data_files.h)
//   set_hash           a hash line, after the line of its file
//   hash_files         sha256.f and sha256.pairs of a session, its files found by the data search

#pragma once

#include "core/data_files.h"
#include "core/sha256.h"
#include "output/polylines.h"

#include <sys/utsname.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace zippers::session {

inline constexpr int VERSION = 1;
inline const char* const MAGIC = "zippers session";

// the spheres of a key, as bits (S^2 = 1, S^3 = 2, S^4 = 4)
inline constexpr int SPHERE2 = 1, SPHERE3 = 2, SPHERE4 = 4, ALL = 7, S34 = 6;
inline int sphere_bit(int sphere) { return sphere == 2 ? SPHERE2 : sphere == 3 ? SPHERE3 : sphere == 4 ? SPHERE4 : 0; }

// the kind of a key's value
enum class Kind {
    number,     // a number
    integer,    // an integer
    order,      // an integer, or inf (S^2's -n)
    two,        // two numbers, separated by a blank (--pole)
    list,       // numbers separated by commas (--viewmatrix, --centre)
    word,       // a text without blanks (the example, the files, --phi, --only)
    flag,       // yes or no: the option given, or not
    colour,     // NAME=R,G,B, one line for each NAME
    hash        // 64 hexadecimal digits (sha256.f, sha256.pairs)
};

// which group of options it belongs to: the view by the angles and the view by matrices replace each other
enum Group { NONE = 0, ANGLES = 1, MATRIX = 2 };

struct Key {
    const char* name;
    int spheres;
    Kind kind;
    const char* dflt[3];      // the default in S^2, S^3, S^4; null: none (written only when given)
    Group group;
};

// the draw keys, in the order of a command line (example first; the hashes after their files)
inline const Key KEYS[] = {
    {"example", ALL, Kind::word, {nullptr, nullptr, nullptr}, NONE},
    {"phi", SPHERE3, Kind::word, {nullptr, "1,0", nullptr}, NONE},
    {"f", S34, Kind::word, {nullptr, "subst_4.00_9.50_t3_r1.txt", "subst5_2.30_4.50_t1_r0.txt"}, NONE},
    {"sha256.f", S34, Kind::hash, {nullptr, nullptr, nullptr}, NONE},
    {"pairs", SPHERE3, Kind::word, {nullptr, nullptr, nullptr}, NONE},
    {"sha256.pairs", SPHERE3, Kind::hash, {nullptr, nullptr, nullptr}, NONE},
    {"pairindex", SPHERE3, Kind::integer, {nullptr, "0", nullptr}, NONE},
    {"context", SPHERE3, Kind::flag, {nullptr, "no", nullptr}, NONE},
    {"n", SPHERE2, Kind::order, {"2", nullptr, nullptr}, NONE},
    {"R", ALL, Kind::number, {"6", "0", "0"}, NONE},
    {"a", ALL, Kind::number, {"0.004", "0.01", "0.02"}, NONE},
    {"s", ALL, Kind::integer, {"512", "900", "840"}, NONE},
    {"w", SPHERE2, Kind::number, {"0", nullptr, nullptr}, NONE},
    {"zoom", ALL, Kind::number, {"1", "1", "1"}, NONE},
    {"yaw", ALL, Kind::number, {"0", "0", "0"}, ANGLES},
    {"pitch", ALL, Kind::number, {"0", "20", "15"}, ANGLES},
    {"roll", SPHERE2, Kind::number, {"0", nullptr, nullptr}, NONE},
    {"pole", SPHERE3, Kind::two, {nullptr, "0 0", nullptr}, ANGLES},
    {"centre", S34, Kind::list, {nullptr, nullptr, nullptr}, ANGLES},
    {"viewmatrix", S34, Kind::list, {nullptr, nullptr, nullptr}, MATRIX},
    {"turn", SPHERE4, Kind::number, {nullptr, nullptr, "0"}, NONE},
    {"turn2", SPHERE4, Kind::number, {nullptr, nullptr, "0"}, NONE},
    {"hidfog", SPHERE4, Kind::number, {nullptr, nullptr, "1.5"}, NONE},
    {"clip", S34, Kind::number, {nullptr, "3", "3"}, NONE},
    {"only", S34, Kind::word, {nullptr, nullptr, nullptr}, NONE},
    {"overlap", ALL, Kind::integer, {"1", "0", "0"}, NONE},
    {"cap", S34, Kind::integer, {nullptr, "8000000", "20000000"}, NONE},
    {"thick", S34, Kind::integer, {nullptr, "0", "0"}, NONE},
    {"C", SPHERE2, Kind::flag, {"no", nullptr, nullptr}, NONE},
    {"wheelres", SPHERE2, Kind::integer, {"10000000", nullptr, nullptr}, NONE},
    {"cull", S34, Kind::flag, {nullptr, "no", "no"}, NONE},
    {"cullk", S34, Kind::number, {nullptr, nullptr, nullptr}, NONE},
    {"zoomacc", S34, Kind::flag, {nullptr, "no", "no"}, NONE},
    {"focus", S34, Kind::number, {nullptr, nullptr, nullptr}, NONE},
    {"nocull", ALL, Kind::flag, {"no", "no", "no"}, NONE},
    {"nofocus", ALL, Kind::flag, {"no", "no", "no"}, NONE},
    {"nochains", SPHERE2, Kind::flag, {"no", nullptr, nullptr}, NONE},
    {"memlimit", SPHERE2, Kind::number, {"4", nullptr, nullptr}, NONE},
    {"stats", SPHERE2, Kind::flag, {"no", nullptr, nullptr}, NONE},     // (S^3, S^4: only prints, one of OTHERS)
    {"colour", ALL, Kind::colour, {nullptr, nullptr, nullptr}, NONE},
};

// the colours of each sphere, with their defaults (those of the programs)
struct Colour { const char* name; int spheres; const char* dflt[3]; };
inline const Colour COLOURS[] = {
    {"Z+", ALL, {"0,0,1", "0.05,0.15,0.85", "0.05,0.15,0.85"}},
    {"Z-", ALL, {"1,0,0", "0.85,0.08,0.08", "0.85,0.08,0.08"}},
    {"wheel", SPHERE2, {"0,0.6,0", nullptr, nullptr}},
    {"pair.Z+", SPHERE3, {nullptr, "0.05,0.15,0.85", nullptr}},
    {"pair.Z-", SPHERE3, {nullptr, "0.85,0.08,0.08", nullptr}},
    {"context.Z+", SPHERE3, {nullptr, "0.7,0.75,0.95", nullptr}},
    {"context.Z-", SPHERE3, {nullptr, "0.95,0.72,0.72", nullptr}},
};

// the default substitution file of S^3 with --pairs (the file whose base arcs the pairs are loops of)
inline const char* const DEFAULT_PAIRS_FILE = "subst_4.00_9.50_t3.txt";

// the options of zippers draw that are not draw keys: the outputs, those that only print, and --format; with the
// number of values each takes (and --wheel, another name of -C). --stats is one in S^3 and S^4, where it only prints;
// in S^2 it is a draw key, since the memory of its tables counts against the drawing's limit there, so that it can
// change a picture that reaches the limit
struct Other { const char* name; int values; };
inline const Other OTHERS[] = {{"o", 1}, {"polylines", 1}, {"stats", 0}, {"check", 0}, {"timing", 0}, {"format", 1}};

// the window keys that are known: an exact name, or a prefix ending in '.'
inline const char* const WINDOW_KEYS[] = {"window.size", "window.zoom", "window.stem", "window.layers", "window.width.",
                                          "window.view", "window.savedview.", "window.panel."};

struct Line {
    std::string key, value;
    int line_no = 0;             // in the file read (0: made here)
};

struct Session {
    int version = VERSION;
    std::string made_by;
    std::vector<Line> draw;      // the draw keys, example first
    std::vector<Line> window;    // the window keys
    // the value of the first draw key of this name, or null
    const std::string* value(const std::string& key) const
    {
        for (const Line& l : draw) if (l.key == key) return &l.value;
        return nullptr;
    }
    int sphere() const;
};

// ---- the examples and the keys ------------------------------------------------------------------------------------

// the sphere of an example (2, 3 or 4), and its name as a session writes it; 0 if it is not an example
inline int sphere_of_example(const std::string& e, std::string* canonical = nullptr)
{
    if (e == "cartwright-steger" || e == "cs") {
        if (canonical) *canonical = "cartwright-steger";
        return 3;
    }
    if (e == "imm-m5" || e == "imm") {
        if (canonical) *canonical = "imm-m5";
        return 4;
    }
    bool r = false, l = false, other = e.empty();
    for (char c : e) {
        if (c == 'R' || c == 'r') r = true;
        else if (c == 'L' || c == 'l') l = true;
        else other = true;
    }
    if (e == "fig8" || (!other && r && l)) {
        if (canonical) *canonical = e;
        return 2;
    }
    return 0;
}

inline int Session::sphere() const
{
    const std::string* e = value("example");
    return e ? sphere_of_example(*e) : 0;
}

inline const Key* find_key(const std::string& name, int sphere)
{
    for (const Key& k : KEYS)
        if (name == k.name && (k.spheres & sphere_bit(sphere))) return &k;
    return nullptr;
}

inline const Colour* find_colour(const std::string& name, int sphere)
{
    for (const Colour& c : COLOURS)
        if (name == c.name && (c.spheres & sphere_bit(sphere))) return &c;
    return nullptr;
}

// the option of a key, as zippers draw takes it: one dash for a one-letter name, two for a longer one
inline std::string option_of(const std::string& key) { return (key.size() == 1 ? "-" : "--") + key; }

// the number of values that the option of a key of this kind takes
inline int values_of(Kind k) { return k == Kind::flag ? 0 : k == Kind::two ? 2 : 1; }

// ---- the values ---------------------------------------------------------------------------------------------------

namespace detail {

inline bool number(const std::string& s, double& x)
{
    if (s.empty() || s.find_first_of(" \t\n") != std::string::npos) return false;
    char* end = nullptr;
    x = strtod(s.c_str(), &end);
    return end && !*end && std::isfinite(x);
}

inline bool integer(const std::string& s, long long& n)
{
    if (s.empty() || s.find_first_not_of("+-0123456789") != std::string::npos) return false;
    char* end = nullptr;
    errno = 0;
    n = strtoll(s.c_str(), &end, 10);
    return end && !*end && errno == 0;
}

inline std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> v;
    size_t i = 0;
    while (true) {
        size_t j = s.find(sep, i);
        v.push_back(s.substr(i, j == std::string::npos ? std::string::npos : j - i));
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return v;
}

inline std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

}  // namespace detail

// the value written for a key: its numbers as the shortest decimals that read back (polylines::exact); false, with the
// reason, if it is not a value of this kind. `values` are the option's values as on a command line
inline bool normalise(const Key& k, int sphere, const std::vector<std::string>& values, std::string& out, std::string& why)
{
    using namespace detail;
    const std::string v = values.empty() ? std::string() : values[0];
    double x;
    long long n;
    switch (k.kind) {
    case Kind::number:
        if (!number(v, x)) { why = "a number is wanted"; return false; }
        out = polylines::exact(x);
        return true;
    case Kind::integer:
        if (!integer(v, n)) { why = "an integer is wanted"; return false; }
        out = std::to_string(n);
        return true;
    case Kind::order:
        if (v == "inf") { out = v; return true; }
        if (!integer(v, n)) { why = "an integer, or inf, is wanted"; return false; }
        out = std::to_string(n);
        return true;
    case Kind::two: {
        std::vector<std::string> p = values.size() == 2 ? values : split(v, ' ');
        double y;
        if (p.size() != 2 || !number(p[0], x) || !number(p[1], y)) { why = "two numbers are wanted"; return false; }
        out = polylines::exact(x) + " " + polylines::exact(y);
        return true;
    }
    case Kind::list: {
        out.clear();
        for (const std::string& p : split(v, ',')) {
            if (!number(p, x)) { why = "numbers separated by commas are wanted"; return false; }
            out += (out.empty() ? "" : ",") + polylines::exact(x);
        }
        return true;
    }
    case Kind::word:
        if (v.empty() || v.find_first_of(" \t\n\r") != std::string::npos) { why = "a word is wanted"; return false; }
        if (!strcmp(k.name, "only") && v != "+" && v != "-") { why = "+ or - is wanted"; return false; }
        if (!strcmp(k.name, "phi") && v != "1,0") { why = "only 1,0 is available"; return false; }
        out = v;
        return true;
    case Kind::flag:
        if (v != "yes" && v != "no") { why = "yes or no is wanted"; return false; }
        out = v;
        return true;
    case Kind::colour: {
        size_t eq = v.find('=');
        std::vector<std::string> c = eq == std::string::npos ? std::vector<std::string>() : split(v.substr(eq + 1), ',');
        if (eq == std::string::npos || !find_colour(v.substr(0, eq), sphere) || c.size() != 3) {
            why = "a colour NAME=R,G,B of this example is wanted";
            return false;
        }
        out = v.substr(0, eq) + "=";
        for (size_t i = 0; i < 3; i++) {
            if (!number(c[i], x) || x < 0 || x > 1) { why = "three numbers in [0, 1] are wanted"; return false; }
            out += (i ? "," : "") + polylines::exact(x);
        }
        return true;
    }
    case Kind::hash:
        if (v.size() != 64 || v.find_first_not_of("0123456789abcdef") != std::string::npos) {
            why = "64 hexadecimal digits are wanted";
            return false;
        }
        out = v;
        return true;
    }
    return false;
}

// the name of a colour line's value (NAME of NAME=R,G,B)
inline std::string colour_name(const std::string& value) { return value.substr(0, value.find('=')); }

// ---- sessions from command lines, and command lines from sessions ---------------------------------------------------

namespace detail {

// the options of a command line (after the example), read against KEYS and OTHERS: (key, values) in order, and the
// others (the outputs, ...), as they are
inline bool parse_options(int sphere, const std::vector<std::string>& args, std::vector<std::pair<std::string,
                          std::vector<std::string>>>& keys, std::vector<std::string>& others, std::string& why)
{
    for (size_t i = 0; i < args.size(); i++) {
        const std::string& a = args[i];
        if (a.size() < 2 || a[0] != '-') { why = "unexpected argument '" + a + "'"; return false; }
        std::string name = a.substr(a[1] == '-' ? 2 : 1);
        if (name == "wheel") name = "C";
        if (option_of(name) != a && a != "--wheel") { why = "unknown option " + a; return false; }
        const Key* k = name == "example" || name.rfind("sha256.", 0) == 0 ? nullptr : find_key(name, sphere);
        int nv = -1;
        if (k) nv = values_of(k->kind);
        else
            for (const Other& o : OTHERS)
                if (name == o.name) nv = o.values;
        if (nv < 0) { why = "unknown option " + a + " for this example"; return false; }
        if (nv > 0 && i + (size_t)nv >= args.size()) { why = a + " needs " + (nv == 1 ? "a value" : "two values"); return false; }
        std::vector<std::string> vals(args.begin() + (long)i + 1, args.begin() + (long)i + 1 + nv);
        i += (size_t)nv;
        if (k) keys.push_back({name, vals});
        else {
            others.push_back(a);
            others.insert(others.end(), vals.begin(), vals.end());
        }
    }
    return true;
}

inline const char* default_of(const Key& k, int sphere) { return k.dflt[sphere - 2]; }

}  // namespace detail

// a session from a zippers draw command line, EXAMPLE OPTIONS...: every key of the example, with the value given or its
// default (in the order of KEYS); the colours of the example, given or default. The session is that command line: the
// defaults of a zoomed still of S^3 and S^4 (a picture, -o, at --zoom above 1, not --pairs: culling and the focus ball at
// 1, unless --nocull, --nofocus or options of their own say otherwise) are written as the options they stand for only
// when the command line makes a picture (-o); without -o they are not, so the session, replayed without -o, draws the
// command's curves, and replayed with -o it is a still that takes the defaults of a zoomed still of the zippers that
// replays it. In S^2 --stats is a draw key (stats = yes), and the drawing's memory limit is the program's default for
// the command: 6 GB with --stats, else 4; so the session runs the command with its own limit. (A picture whose drawing
// reaches the limit can still differ from run to run: zipper's guard reads the process's resident memory.) The hashes are left out (hash_data_file gives them). The outputs and the options that only print are
// returned in `others`. False, with the reason, if an option is unknown or a value wrong.
inline bool from_args(const std::vector<std::string>& args, Session& s, std::vector<std::string>* others_out,
                      std::string& why)
{
    s = Session();
    if (args.empty()) { why = "no example"; return false; }
    std::string ex;
    const int sphere = sphere_of_example(args[0], &ex);
    if (!sphere) { why = "unknown example '" + args[0] + "'"; return false; }
    std::vector<std::pair<std::string, std::vector<std::string>>> given;
    std::vector<std::string> others;
    if (!detail::parse_options(sphere, std::vector<std::string>(args.begin() + 1, args.end()), given, others, why))
        return false;
    auto given_value = [&](const std::string& name) -> const std::vector<std::string>* {
        const std::vector<std::string>* v = nullptr;
        for (const auto& g : given) if (g.first == name) v = &g.second;     // the last one counts
        return v;
    };
    auto flag_given = [&](const std::string& name) { return given_value(name) != nullptr; };
    const bool matrix = flag_given("viewmatrix"), pairs = flag_given("pairs");
    double zoom = 1;
    if (const auto* z = given_value("zoom")) detail::number((*z)[0], zoom);
    bool picture = false;                                // -o: a picture (a still)
    for (const std::string& o : others) picture = picture || o == "-o";
    const bool stats = flag_given("stats");              // (S^2's draw key)
    const bool still_zoomed = sphere != 2 && zoom > 1 && !pairs && picture;
    s.draw.push_back({"example", ex});
    for (const Key& k : KEYS) {
        if (!(k.spheres & sphere_bit(sphere)) || !strcmp(k.name, "example") || k.kind == Kind::hash) continue;
        if ((k.group == ANGLES && matrix) || (k.group == MATRIX && !matrix && !flag_given(k.name))) continue;
        if (k.kind == Kind::colour) {
            for (const Colour& c : COLOURS) {
                if (!(c.spheres & sphere_bit(sphere))) continue;
                std::string v = std::string(c.name) + "=" + c.dflt[sphere - 2];
                for (const auto& g : given)
                    if (g.first == "colour" && colour_name(g.second[0]) == c.name) v = g.second[0];
                std::string out;
                if (!normalise(k, sphere, {v}, out, why)) { why = "colour " + v + ": " + why; return false; }
                s.draw.push_back({"colour", out});
            }
            for (const auto& g : given)
                if (g.first == "colour" && !find_colour(colour_name(g.second[0]), sphere)) {
                    why = "colour " + g.second[0] + ": not a colour of this example";
                    return false;
                }
            continue;
        }
        std::vector<std::string> v;
        if (const auto* g = given_value(k.name)) v = k.kind == Kind::flag ? std::vector<std::string>{"yes"} : *g;
        else if (!strcmp(k.name, "f") && pairs) v = {DEFAULT_PAIRS_FILE};
        else if (!strcmp(k.name, "cull") && still_zoomed && !flag_given("cullk") && !flag_given("nocull")) v = {"yes"};
        else if (!strcmp(k.name, "focus") && still_zoomed && !flag_given("nofocus")) v = {"1"};
        else if (!strcmp(k.name, "memlimit") && stats) v = {"6"};   // (zipper's drawing limit with --stats)
        else if (detail::default_of(k, sphere)) v = {detail::default_of(k, sphere)};
        else continue;
        std::string out;
        if (!normalise(k, sphere, v, out, why)) { why = option_of(k.name) + " " + (v.empty() ? "" : v[0]) + ": " + why; return false; }
        s.draw.push_back({k.name, out});
    }
    if (others_out) *others_out = others;
    return true;
}

// the zippers draw command line of a session: EXAMPLE, then its options in order (a flag yes is the option, no is
// nothing; the hashes are not options)
inline std::vector<std::string> args_of(const Session& s)
{
    std::vector<std::string> a;
    const std::string* e = s.value("example");
    a.push_back(e ? *e : std::string());
    const int sphere = s.sphere();
    for (const Line& l : s.draw) {
        if (l.key == "example" || l.key.rfind("sha256.", 0) == 0) continue;
        const Key* k = find_key(l.key, sphere);
        if (!k) continue;
        if (k->kind == Kind::flag) {
            if (l.value == "yes") a.push_back(option_of(l.key));
            continue;
        }
        a.push_back(option_of(l.key));
        if (k->kind == Kind::two) {
            std::vector<std::string> p = detail::split(l.value, ' ');
            a.insert(a.end(), p.begin(), p.end());
        } else a.push_back(l.value);
    }
    return a;
}

// options given after a session (OPTIONS..., as on a zippers draw command line) replace the session's: a key by its
// name, a colour by its colour's name, a flag given sets it to yes; the view by --viewmatrix and that by --yaw,
// --pitch, --pole, --centre replace each other (--yaw after a session of a matrix drops the matrix, and the angles not
// given are then the programs' defaults, which a session does not hold); --nocull drops the session's culling (cull,
// cullk, zoomacc) and
// --cull or --cullk its nocull, --nofocus its focus and --focus its nofocus; -f drops sha256.f, and --pairs
// sha256.pairs (the file given is not checked). The outputs and the options that only print are returned in `others`,
// to be passed on. False, with the reason, if an option is unknown or a value wrong.
inline bool apply(Session& s, const std::vector<std::string>& args, std::vector<std::string>& others, std::string& why)
{
    const int sphere = s.sphere();
    std::vector<std::pair<std::string, std::vector<std::string>>> given;
    others.clear();
    if (!detail::parse_options(sphere, args, given, others, why)) return false;
    for (const auto& g : given) {
        const Key* k = find_key(g.first, sphere);
        std::string out;
        if (!normalise(*k, sphere, k->kind == Kind::flag ? std::vector<std::string>{"yes"} : g.second, out, why)) {
            why = option_of(g.first) + ": " + why;
            return false;
        }
        auto drop = [&](auto pred) {
            for (size_t i = 0; i < s.draw.size();) i = pred(s.draw[i]) ? (s.draw.erase(s.draw.begin() + (long)i), i) : i + 1;
        };
        auto drop_keys = [&](std::initializer_list<const char*> names) {
            drop([&](const Line& l) {
                for (const char* n : names) if (l.key == n) return true;
                return false;
            });
        };
        if (k->group == MATRIX)
            drop([&](const Line& l) { const Key* q = find_key(l.key, sphere); return q && q->group == ANGLES; });
        if (k->group == ANGLES) drop_keys({"viewmatrix"});
        if (g.first == "nocull") drop_keys({"cull", "cullk", "zoomacc"});
        if (g.first == "cull" || g.first == "cullk") drop_keys({"nocull"});
        if (g.first == "nofocus") drop_keys({"focus"});
        if (g.first == "focus") drop_keys({"nofocus"});
        if (g.first == "f") drop_keys({"sha256.f"});
        if (g.first == "pairs") drop_keys({"sha256.pairs"});
        bool replaced = false;
        for (Line& l : s.draw) {
            const bool same = k->kind == Kind::colour ? l.key == "colour" && colour_name(l.value) == colour_name(out)
                                                      : l.key == g.first;
            if (same && !replaced) {
                l.value = out;
                l.line_no = 0;
                replaced = true;
            }
        }
        if (!replaced) s.draw.push_back({g.first, out});
    }
    return true;
}

// ---- the file -------------------------------------------------------------------------------------------------------

inline bool known_window_key(const std::string& key)
{
    for (const char* w : WINDOW_KEYS) {
        const size_t n = strlen(w);
        if (w[n - 1] == '.' ? key.compare(0, n, w) == 0 && key.size() > n : key == w) return true;
    }
    return false;
}

// "zippers VERSION, SYSTEM PROCESSOR, COMPILER" (for made_by)
inline std::string made_by_text(const std::string& version)
{
    char compiler[80];
#if defined(__apple_build_version__)
    snprintf(compiler, sizeof compiler, "AppleClang %d.%d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__,
             __apple_build_version__);
#elif defined(__clang__)
    snprintf(compiler, sizeof compiler, "Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    snprintf(compiler, sizeof compiler, "GNU %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    snprintf(compiler, sizeof compiler, "an unknown compiler");
#endif
    struct utsname u;
    std::string sys = uname(&u) == 0 ? std::string(u.sysname) + " " + u.machine : std::string("an unknown system");
    return "zippers " + version + ", " + sys + ", " + compiler;
}

// the SHA-256 of the data file `name` of the sphere (3 or 4), as the data search finds it (core/data_files.h): true,
// with the hash and the path found; false, with the reason
inline bool hash_data_file(const std::string& name, int sphere, const DataPlaces& places, std::string& hex,
                           std::string& path, std::string& why)
{
    DataFile r = find_data_file(name, sphere == 4 ? "s4" : "s3", places);
    if (r.path.empty()) {
        why = r.problem.empty() ? "cannot find " + name : "cannot use " + name + ": " + r.problem;
        return false;
    }
    path = r.path;
    return sha256_file(r.path, hex, &why);
}

// the line `hash_key = hex` (sha256.f, sha256.pairs) in its place, after the line of its file (f, pairs), or in place
// of an older one
inline void set_hash(Session& s, const std::string& hash_key, const std::string& hex)
{
    for (Line& l : s.draw)
        if (l.key == hash_key) {
            l.value = hex;
            return;
        }
    const std::string file = hash_key == "sha256.f" ? "f" : "pairs";
    for (size_t i = 0; i < s.draw.size(); i++)
        if (s.draw[i].key == file) {
            s.draw.insert(s.draw.begin() + (long)i + 1, Line{hash_key, hex});
            return;
        }
    s.draw.push_back({hash_key, hex});
}

// the hashes of a session's files (S^3, S^4: f; S^3: pairs, when it is given), found by the data search with these
// places: true, with sha256.f and sha256.pairs set; false, with the reason
inline bool hash_files(Session& s, const DataPlaces& places, std::string& why)
{
    const int sphere = s.sphere();
    if (sphere != 3 && sphere != 4) return true;
    for (const char* file : {"f", "pairs"}) {
        const std::string* v = s.value(file);
        if (!v) continue;
        std::string hex, path;
        if (!hash_data_file(*v, sphere, places, hex, path, why)) return false;
        set_hash(s, file[0] == 'f' ? "sha256.f" : "sha256.pairs", hex);
    }
    return true;
}

// the text of a session; `remakes`, if not empty, the picture it remakes (for the comment of its second line)
inline std::string text_of(const Session& s, const std::string& file = std::string(), const std::string& remakes = std::string())
{
    std::string t = std::string(MAGIC) + " " + std::to_string(VERSION) + "\n";
    if (!file.empty() && !remakes.empty())
        t += "# zippers draw --session " + file + " -o " + remakes + " remakes " + remakes + "\n";
    if (!s.made_by.empty()) t += "made_by = " + s.made_by + "\n";
    for (const Line& l : s.draw) t += l.key + " = " + l.value + "\n";
    for (const Line& l : s.window) t += l.key + " = " + l.value + "\n";
    return t;
}

// write the session (under a temporary name in the file's folder, renamed at the end, as polylines::write does); false,
// and a message in *err, if it cannot
inline bool write(const std::string& fn, const Session& s, std::string* err = nullptr, const std::string& remakes = std::string())
{
    for (const Line& l : s.draw)
        if (!polylines::recordable(l.key + l.value) || l.key.find('=') != std::string::npos) {
            if (err) *err = "cannot write " + fn + ": a line that the file cannot hold: " + l.key;
            return false;
        }
    const std::string wp = polylines::write_problem(fn);
    if (!wp.empty()) {
        if (err) *err = "cannot write " + fn + ": " + wp;
        return false;
    }
    const size_t sl = fn.find_last_of('/');
    const std::string text = text_of(s, sl == std::string::npos ? fn : fn.substr(sl + 1), remakes);
    const std::string target = polylines::target_of(fn);
    std::string tmp;
    FILE* out = nullptr;
    for (int k = 0; k < 100 && !out; k++) {
        tmp = target + ".tmp" + std::to_string((long)getpid()) + "_" + std::to_string(k);
        int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (fd < 0) {
            if (errno != EEXIST) break;
            continue;
        }
        out = fdopen(fd, "w");
        if (!out) close(fd);
    }
    if (!out) {
        if (err) *err = "cannot write " + fn + ": " + strerror(errno);
        return false;
    }
    bool ok = fwrite(text.data(), 1, text.size(), out) == text.size();
    int e = ok ? 0 : errno;
    if (fclose(out) != 0 && ok) { ok = false; e = errno; }
    if (ok && rename(tmp.c_str(), target.c_str()) != 0) { ok = false; e = errno; }
    if (!ok) {
        unlink(tmp.c_str());
        if (err) *err = "cannot write " + fn + ": " + strerror(e ? e : EIO);
    }
    return ok;
}

// read a session; false, and a message in *err, unless it is a session of format 1 to VERSION whose keys and values
// are those of its example. The window keys that are not known are left out, each with a warning in *warnings.
inline bool read(const std::string& fn, Session& s, std::string* err = nullptr, std::vector<std::string>* warnings = nullptr)
{
    auto bad = [&](const std::string& what) {
        if (err) *err = fn + ": " + what;
        return false;
    };
    s = Session();
    FILE* in = fopen(fn.c_str(), "rb");
    if (!in) return bad(std::string("cannot read it: ") + strerror(errno));
    std::string text;
    char buf[8192];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, in)) > 0) text.append(buf, k);
    fclose(in);
    std::vector<std::string> lines = detail::split(text, '\n');
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    const std::string magic = std::string(MAGIC) + " ";
    if (lines.empty() || lines[0].compare(0, magic.size(), magic) != 0)
        return bad("not a session file (its first line is not \"" + std::string(MAGIC) + " " + std::to_string(VERSION) + "\")");
    long long v = 0;
    if (!detail::integer(detail::trim(lines[0].substr(magic.size())), v) || v < 1)
        return bad("not a session file (its first line is not \"" + std::string(MAGIC) + " " + std::to_string(VERSION) + "\")");
    if (v > VERSION)
        return bad("this session is of format " + std::to_string(v) + ", written by a newer zippers; this one reads format " +
                   std::to_string(VERSION));
    s.version = (int)v;
    int sphere = 0;
    std::vector<std::string> seen;
    for (size_t i = 1; i < lines.size(); i++) {
        const std::string line = detail::trim(lines[i]);
        const std::string at = "line " + std::to_string(i + 1) + ": ";
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) return bad(at + "not a line KEY = VALUE");
        const std::string key = detail::trim(line.substr(0, eq)), value = detail::trim(line.substr(eq + 1));
        if (key.empty()) return bad(at + "no key");
        if (key == "made_by") {
            s.made_by = value;
            continue;
        }
        if (key.rfind("window.", 0) == 0) {
            if (known_window_key(key)) s.window.push_back({key, value, (int)i + 1});
            else if (warnings) warnings->push_back(fn + ": " + at + "the window key " + key + " is not known; it is ignored");
            continue;
        }
        if (key == "example") {
            if (sphere) return bad(at + "the example is given twice");
            std::string ex;
            sphere = sphere_of_example(value, &ex);
            if (!sphere) return bad(at + "unknown example '" + value + "'");
            s.draw.push_back({key, ex, (int)i + 1});
            continue;
        }
        if (!sphere) return bad(at + "the example comes first (example = ...)");
        const Key* kk = find_key(key, sphere);
        if (!kk) return bad(at + "unknown draw key " + key + " for the example " + *s.value("example"));
        std::string out, why;
        if (!normalise(*kk, sphere, {value}, out, why)) return bad(at + key + " = " + value + ": " + why);
        const std::string id = kk->kind == Kind::colour ? "colour " + colour_name(out) : key;
        for (const std::string& x : seen)
            if (x == id) return bad(at + id + " is given twice");
        seen.push_back(id);
        if (kk->kind == Kind::flag || kk->kind == Kind::hash) out = value;
        s.draw.push_back({key, out, (int)i + 1});
    }
    if (!sphere) return bad("no example (example = ...)");
    if (sphere != 2 && !s.value("sha256.f")) return bad("no sha256.f: its substitution file cannot be checked");
    if (s.value("pairs") && !s.value("sha256.pairs")) return bad("no sha256.pairs: its file of pairs cannot be checked");
    const std::string* m = s.value("viewmatrix");
    for (const Line& l : s.draw) {
        const Key* q = find_key(l.key, sphere);
        if (m && q && q->group == ANGLES) return bad("line " + std::to_string(l.line_no) + ": " + l.key + " does not go with viewmatrix");
    }
    return true;
}

}  // namespace zippers::session
