// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_gui_session: the session file (src/output/session.h) and zippers draw --session.
//
// ROUND TRIPS, in this process: a session made from a command line (every default written out), written and read back,
// is the same, key for key, and its text is written again byte for byte; every number of every key reads back to the
// same double, bit for bit (random numbers, written by from_args, read by read); every draw key of every example (S^2,
// S^3 with the view by angles and with the view by matrices, S^3 with pairs, S^4); a session's command line (args_of)
// gives the session again (from_args).
// REFUSED by the reader, each with a message naming the file: a file that is not a session, a later version (the
// message of the format's rule), an unknown draw key, a key of another sphere, a bad value of each kind, a key given
// twice, a draw key before the example, viewmatrix with the angles, no example, no sha256.f in S^3 and S^4. Kept: the
// known window keys; an unknown window key is left out with a warning.
// OPTIONS AFTER A SESSION replace its keys (apply): a number, a colour by its name, a flag; --viewmatrix and the angles
// replace each other; --nocull drops the culling; -f drops sha256.f. Through zippers --dry-run: one -R, the new one.
// THE STORED SESSIONS of tests/sessions/ (format 1: S^2, S^3 a zoomed still, S^4 with --viewmatrix, a pair picture):
// each loads, names its files by plain names (no folder), and zippers draw --session gives, byte for byte, the picture
// of the same command line given to zippers draw directly (both made here, on this machine); zippers --dry-run draw
// --session prints the program's command line.
// THE HASHES: another file of the name in the working folder is refused, with the file found, where, and both hashes;
// a session whose hash was changed is refused; -f after the session replaces the file and its check.
// A ZOOMED COMMAND LINE WITHOUT -o (S^3, S^4): its session holds no culling and no focus ball, and replayed with
// --polylines gives the command's own polyline file, byte for byte but the line of its command; replayed with -o, the
// still of the command line with -o. A command line with -o: the still's culling and focus ball written, and its still.
// S^2: --stats is a draw key (stats = yes), and the session's memory limit 6 GB with it (the program's then), else 4;
// one given is kept; a session of a --stats command replays it with --stats (zippers --dry-run), and remakes its
// picture byte for byte; --stats after an S^2 session sets the key; in S^3 and S^4 --stats is not a draw key.
// The programs run in folders of this test's own, without ZIPPERS_DATA. About 10 s. ZIPPERS_ZIPPERS is the path of
// zippers, ZIPPERS_REPO the repository (for tests/sessions/ and data/).

#include "output/session.h"
#include "render_test_support.h"

#include <sys/stat.h>
#include <sys/wait.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace se = zippers::session;
using rtest::check;
using rtest::note;
using rtest::section;

#ifndef ZIPPERS_ZIPPERS
#define ZIPPERS_ZIPPERS ""
#endif
#ifndef ZIPPERS_REPO
#define ZIPPERS_REPO "."
#endif

namespace {

const std::string REPO = ZIPPERS_REPO;

// run a command in the folder `dir`, without ZIPPERS_DATA; its exit status, its standard output and its standard error
int run(const std::string& dir, const std::string& cmd, std::string& out, std::string& err)
{
    out.clear();
    err.clear();
    const std::string ef = dir + "/.stderr";
    FILE* p = popen(("cd '" + dir + "' && unset ZIPPERS_DATA && " + cmd + " 2>'" + ef + "' < /dev/null").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    int st = pclose(p);
    err = rtest::read_file(ef);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

std::string zippers() { return std::string("'") + ZIPPERS_ZIPPERS + "'"; }

std::string quote(const std::string& a)
{
    std::string s = "'";
    for (char c : a) s += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return s + "'";
}

std::string joined(const std::vector<std::string>& v)
{
    std::string s;
    for (const std::string& a : v) s += (s.empty() ? "" : " ") + quote(a);
    return s;
}

bool same_lines(const se::Session& a, const se::Session& b)
{
    if (a.made_by != b.made_by || a.draw.size() != b.draw.size() || a.window.size() != b.window.size()) return false;
    for (size_t i = 0; i < a.draw.size(); i++)
        if (a.draw[i].key != b.draw[i].key || a.draw[i].value != b.draw[i].value) return false;
    for (size_t i = 0; i < a.window.size(); i++)
        if (a.window[i].key != b.window[i].key || a.window[i].value != b.window[i].value) return false;
    return true;
}

// write the text as a session file and read it: the reader's verdict, and its message
bool read_text(const rtest::TempDir& t, const std::string& text, se::Session& s, std::string& err,
               std::vector<std::string>* warnings = nullptr)
{
    const std::string fn = t.file("t.zsession");
    rtest::write_file(fn, text);
    return se::read(fn, s, &err, warnings);
}

void refused(const rtest::TempDir& t, const std::string& what, const std::string& text, const std::string& why)
{
    se::Session s;
    std::string err;
    bool ok = read_text(t, text, s, err);
    check(!ok && err.find(why) != std::string::npos && err.find("t.zsession") != std::string::npos,
          "refused: %s (%s)", what.c_str(), ok ? "it was read" : err.c_str());
}

const std::string HASH = std::string(64, 'a');

void test_round_trips(const rtest::TempDir& t)
{
    section("round trips: a session written and read back is the same");
    struct Case { const char* what; std::vector<std::string> args; };
    const Case cases[] = {
        {"S2, every key", {"RRL", "-n", "inf", "-R", "3.25", "-a", "0.002", "-s", "333", "-w", "0.5", "--zoom", "2.5",
                           "--yaw", "-70", "--pitch", "12.5", "--roll", "1e-9", "--overlap", "2", "-C", "--wheelres",
                           "5000", "--nochains", "--nocull", "--nofocus", "--memlimit", "2.5", "--colour", "Z+=0.1,0.2,0.3",
                           "--colour", "wheel=1,0,1"}},
        {"S3, the view by angles", {"cs", "--phi", "1,0", "-f", "subst_4.00_9.50_t3.txt", "-R", "2.554", "-a", "0.05", "-s",
                                     "400", "--zoom", "7", "--yaw", "33", "--pitch", "-12", "--pole", "17", "-41",
                                     "--centre", "0.1,0.2,-0.3,0.9", "--clip", "2.5", "--only", "+", "--overlap", "1",
                                     "--cap", "1000", "--thick", "2", "--cullk", "8", "--zoomacc", "--focus", "0.5",
                                     "--colour", "context.Z-=0,0,0"}},
        {"S3, the view by matrices", {"cs", "-R", "0", "--viewmatrix",
                                       "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,0,0,1", "--nocull", "--nofocus"}},
        {"S3, a pair", {"cs", "--pairs", "linked_pairs.txt", "--pairindex", "7", "--context", "-s", "300"}},
        {"S4", {"imm", "-R", "1.318", "-a", "0.06", "--turn", "25", "--turn2", "-40", "--hidfog", "0.7", "--yaw", "-30",
                "--pitch", "40", "--only", "-", "--cull"}},
    };
    for (const Case& c : cases) {
        se::Session s, g;
        std::vector<std::string> others;
        std::string why;
        if (!check(se::from_args(c.args, s, &others, why), "%s: from the command line (%s)", c.what, why.c_str())) continue;
        s.made_by = se::made_by_text("test");
        if (s.sphere() != 2) se::set_hash(s, "sha256.f", HASH);
        if (s.value("pairs")) se::set_hash(s, "sha256.pairs", HASH);
        s.window.push_back({"window.layers", "Z+ Z-"});
        const std::string fn = t.file("rt.zsession"), fn2 = t.file("rt2.zsession");
        std::string err;
        check(se::write(fn, s, &err), "%s: written (%s)", c.what, err.c_str());
        check(se::read(fn, g, &err), "%s: read back (%s)", c.what, err.c_str());
        check(same_lines(s, g), "%s: the same keys and values, in order (%zu draw keys)", c.what, s.draw.size());
        check(se::write(fn2, g, &err) && rtest::read_file(fn) == rtest::read_file(fn2), "%s: written again, the same bytes",
              c.what);
        // the command line of the session gives the session again
        se::Session h;
        std::vector<std::string> o2;
        if (check(se::from_args(se::args_of(g), h, &o2, why), "%s: from its command line (%s)", c.what, why.c_str())) {
            h.made_by = g.made_by;
            if (h.sphere() != 2) se::set_hash(h, "sha256.f", HASH);
            if (h.value("pairs")) se::set_hash(h, "sha256.pairs", HASH);
            h.window = g.window;
            check(same_lines(g, h), "%s: args_of then from_args: the same session", c.what);
        }
        // every key of the example is there once: those with a default (but the angles with --viewmatrix), and those
        // given; and every colour of the example once
        const int sp = s.sphere();
        const bool matrix = s.value("viewmatrix") != nullptr;
        size_t missing = 0, twice = 0;
        std::string which;
        for (const se::Key& k : se::KEYS) {
            if (!(k.spheres & se::sphere_bit(sp)) || k.kind == se::Kind::colour || k.kind == se::Kind::hash) continue;
            bool given = !strcmp(k.name, "example");
            for (size_t i = 1; i < c.args.size(); i++) given = given || c.args[i] == se::option_of(k.name);
            const bool expected = given || (k.dflt[sp - 2] && !(k.group == se::ANGLES && matrix));
            size_t times = 0;
            for (const se::Line& l : s.draw) times += l.key == k.name;
            if (expected && times == 0) { missing++; which += std::string(" ") + k.name; }
            if (times > 1) twice++;
        }
        size_t colours = 0, want_colours = 0;
        for (const se::Line& l : s.draw) colours += l.key == "colour";
        for (const se::Colour& col : se::COLOURS) want_colours += (col.spheres & se::sphere_bit(sp)) != 0;
        check(missing == 0 && twice == 0 && colours == want_colours,
              "%s: every key of the example, once (%zu draw keys; missing:%s; colours %zu of %zu)", c.what, s.draw.size(),
              which.c_str(), colours, want_colours);
    }

    section("every number reads back to the same double, bit for bit");
    size_t bad = 0, n = 0;
    for (int trial = 0; trial < 300; trial++) {
        double x[8];
        for (double& v : x) v = rtest::uniform(-1, 1)*std::pow(10.0, (double)rtest::rnd(-12, 12));
        char vm[25][40];
        std::string m;
        for (int i = 0; i < 25; i++) {
            snprintf(vm[i], sizeof vm[i], "%.17g", rtest::uniform(-1, 1));
            m += (i ? "," : "") + std::string(vm[i]);
        }
        auto g = [](double v) { char b[40]; snprintf(b, sizeof b, "%.17g", v); return std::string(b); };
        std::vector<std::string> args = {"cs", "-R", g(std::fabs(x[0])), "-a", g(std::fabs(x[1])), "--zoom", g(std::fabs(x[2])),
                                         "--clip", g(std::fabs(x[3])), "--viewmatrix", m, "--cullk", g(std::fabs(x[4])),
                                         "--colour", "Z+=" + g(std::fabs(x[5]) - std::floor(std::fabs(x[5]))) + ",0,1"};
        se::Session s, r;
        std::vector<std::string> others;
        std::string why, err;
        if (!se::from_args(args, s, &others, why)) { bad++; continue; }
        se::set_hash(s, "sha256.f", HASH);
        if (!se::write(t.file("num.zsession"), s, &err) || !se::read(t.file("num.zsession"), r, &err)) { bad++; continue; }
        auto same = [&](const std::string& text, double v) {
            double y = strtod(text.c_str(), nullptr);
            n++;
            return !memcmp(&y, &v, sizeof y);
        };
        bad += !same(*r.value("R"), strtod(g(std::fabs(x[0])).c_str(), nullptr));
        bad += !same(*r.value("a"), strtod(g(std::fabs(x[1])).c_str(), nullptr));
        bad += !same(*r.value("zoom"), strtod(g(std::fabs(x[2])).c_str(), nullptr));
        bad += !same(*r.value("clip"), strtod(g(std::fabs(x[3])).c_str(), nullptr));
        bad += !same(*r.value("cullk"), strtod(g(std::fabs(x[4])).c_str(), nullptr));
        std::vector<std::string> parts = se::detail::split(*r.value("viewmatrix"), ',');
        for (int i = 0; i < 25 && i < (int)parts.size(); i++) bad += !same(parts[(size_t)i], strtod(vm[i], nullptr));
        bad += parts.size() != 25;
    }
    check(bad == 0 && n >= 9000, "%zu numbers written and read back (%zu not the same)", n, bad);
}

void test_refusals(const rtest::TempDir& t)
{
    section("the reader refuses what is not a session of format 1");
    const std::string head = "zippers session 1\nmade_by = x\nexample = cartwright-steger\nsha256.f = " + HASH + "\n";
    refused(t, "a file that is not a session", "zippers polylines 1\n", "not a session file");
    refused(t, "an empty file", "", "not a session file");
    refused(t, "a later version", "zippers session 2\nexample = fig8\n",
            "this session is of format 2, written by a newer zippers; this one reads format 1");
    refused(t, "an unknown draw key", head + "bogus = 1\n", "line 5: unknown draw key bogus for the example cartwright-steger");
    refused(t, "a key of another sphere", head + "turn = 3\n", "unknown draw key turn");
    refused(t, "a bad number", head + "R = abc\n", "R = abc: a number is wanted");
    refused(t, "an infinite number", head + "R = inf\n", "a number is wanted");
    refused(t, "a bad integer", head + "s = 3.5\n", "an integer is wanted");
    refused(t, "a bad pair", head + "pole = 1\n", "two numbers are wanted");
    refused(t, "a bad list", head + "viewmatrix = 1,,2\n", "numbers separated by commas");
    refused(t, "a bad flag", head + "context = maybe\n", "yes or no is wanted");
    refused(t, "a bad colour", head + "colour = wheel=0,0,0\n", "a colour NAME=R,G,B of this example");
    refused(t, "a colour out of [0, 1]", head + "colour = Z+=0,0,2\n", "three numbers in [0, 1]");
    refused(t, "a bad hash", "zippers session 1\nexample = imm-m5\nsha256.f = 123\n", "64 hexadecimal digits");
    refused(t, "a bad only", head + "only = both\n", "+ or - is wanted");
    refused(t, "a key given twice", head + "R = 1\nR = 2\n", "R is given twice");
    refused(t, "a colour given twice", head + "colour = Z+=0,0,1\ncolour = Z+=1,0,0\n", "colour Z+ is given twice");
    refused(t, "a draw key before the example", "zippers session 1\nR = 2\nexample = fig8\n", "the example comes first");
    refused(t, "viewmatrix with the angles", head + "yaw = 3\nviewmatrix = 1,0\n", "yaw does not go with viewmatrix");
    refused(t, "no example", "zippers session 1\nmade_by = x\n", "no example");
    refused(t, "an unknown example", "zippers session 1\nexample = torus\n", "unknown example 'torus'");
    refused(t, "no sha256.f in S3", "zippers session 1\nexample = cs\n", "no sha256.f");
    refused(t, "pairs without sha256.pairs", head + "pairs = linked_pairs.txt\n", "no sha256.pairs");
    refused(t, "a line that is not KEY = VALUE", head + "just words\n", "not a line KEY = VALUE");

    section("the window keys: kept when known, an unknown one left out with a warning");
    {
        se::Session s;
        std::string err;
        std::vector<std::string> warnings;
        bool ok = read_text(t, "zippers session 1\n# a comment\n\nexample = fig8\nwindow.layers = Z+ Z-\nwindow.panel.open = yes\n"
                            "window.bogus = 1\nR = 2\n", s, err, &warnings);
        check(ok && s.window.size() == 2 && s.window[0].key == "window.layers" && s.window[1].key == "window.panel.open",
              "the known window keys kept (%s)", err.c_str());
        check(warnings.size() == 1 && warnings[0].find("line 7: the window key window.bogus is not known; it is ignored") !=
              std::string::npos, "one warning: %s", warnings.empty() ? "none" : warnings[0].c_str());
        check(ok && s.value("R") && *s.value("R") == "2" && s.draw.size() == 2, "and the draw keys read");
    }
}

void test_apply()
{
    section("options given after a session replace the session's");
    se::Session s;
    std::vector<std::string> others;
    std::string why;
    check(se::from_args({"cs", "-R", "2.554", "--yaw", "30", "--zoom", "3", "--colour", "Z+=1,0,0"}, s, &others, why),
          "the session (%s)", why.c_str());
    se::set_hash(s, "sha256.f", HASH);
    se::Session a = s;
    check(se::apply(a, {"-R", "3.5", "--colour", "Z+=0,1,0", "--context", "-o", "x.png", "--stats"}, others, why),
          "apply (%s)", why.c_str());
    int nR = 0, nZ = 0;
    for (const se::Line& l : a.draw) {
        nR += l.key == "R";
        nZ += l.key == "colour" && se::colour_name(l.value) == "Z+";
    }
    check(nR == 1 && *a.value("R") == "3.5" && nZ == 1 && *a.value("context") == "yes",
          "one R (3.5), one colour of Z+, the flag set");
    bool z0 = false;
    for (const se::Line& l : a.draw) z0 = z0 || (l.key == "colour" && l.value == "Z+=0,1,0");
    check(z0, "the colour of Z+ replaced by its name");
    check(others == std::vector<std::string>{"-o", "x.png", "--stats"}, "the outputs and the options that print, passed on");
    a = s;
    check(se::apply(a, {"--viewmatrix", "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,0,0,1"}, others, why) &&
          !a.value("yaw") && !a.value("pitch") && !a.value("pole") && a.value("viewmatrix"),
          "--viewmatrix replaces the angles (%s)", why.c_str());
    check(se::apply(a, {"--pitch", "5"}, others, why) && !a.value("viewmatrix") && *a.value("pitch") == "5",
          "and the angles replace --viewmatrix");
    a = s;
    se::apply(a, {"--nocull", "--nofocus"}, others, why);
    check(!a.value("cull") && !a.value("focus") && *a.value("nocull") == "yes" && *a.value("nofocus") == "yes",
          "--nocull and --nofocus drop the session's culling and focus ball (a zoomed still's)");
    a = s;
    check(se::apply(a, {"-f", "subst_4.00_9.50_t3.txt"}, others, why) && !a.value("sha256.f") && *a.value("f") == "subst_4.00_9.50_t3.txt",
          "-f replaces the file and drops its hash");
    a = s;
    check(!se::apply(a, {"--turn", "3"}, others, why) && why.find("unknown option --turn") != std::string::npos,
          "an option of another sphere is refused (%s)", why.c_str());
}

// the stored sessions and the command lines they stand for
struct Stored { const char* file; const char* direct; };
const Stored STORED[] = {
    {"s2_fig8.zsession", "draw fig8 -n 3 -R 3 -s 300 --yaw 20 --pitch -10 --roll 5"},
    {"s3_cartwright_steger.zsession", "draw cs -R 2.554 -a 0.05 -s 300 --zoom 3 --yaw 30 --pole 10 5"},
    {"s4_imm_m5.zsession", "draw imm -R 1.318 -a 0.06 -s 300 --turn 25 --viewmatrix VIEWMATRIX"},
    {"s3_pair.zsession", "draw cs --pairs linked_pairs.txt --pairindex 5 --context -s 300"},
};

void test_stored(const rtest::TempDir& t)
{
    section("the stored sessions: each loads, and remakes the picture of its command line, byte for byte");
    for (const Stored& st : STORED) {
        const std::string fn = REPO + "/tests/sessions/" + st.file;
        se::Session s;
        std::string err;
        if (!check(se::read(fn, s, &err), "%s: loads (%s)", st.file, err.c_str())) continue;
        bool plain = true;
        for (const char* k : {"f", "pairs"})
            if (const std::string* v = s.value(k)) plain = plain && v->find('/') == std::string::npos;
        check(plain && s.version == 1, "%s: format 1, its files by plain names", st.file);
        std::string direct = st.direct;
        const size_t vm = direct.find("VIEWMATRIX");
        if (vm != std::string::npos) direct.replace(vm, 10, s.value("viewmatrix") ? *s.value("viewmatrix") : "none");
        std::string out, e1, e2;
        int a = run(t.path, zippers() + " draw --session " + quote(fn) + " -o a.png", out, e1);
        int b = run(t.path, zippers() + " " + direct + " -o b.png", out, e2);
        check(a == 0 && b == 0, "%s: both run (exit %d: %s; exit %d: %s)", st.file, a, e1.substr(0, 200).c_str(), b,
              e2.substr(0, 200).c_str());
        const std::string pa = rtest::read_file(t.file("a.png")), pb = rtest::read_file(t.file("b.png"));
        check(!pa.empty() && pa == pb, "%s: the session's picture is that of zippers %s, byte for byte (%zu bytes)", st.file,
              st.direct, pa.size());
        remove(t.file("a.png").c_str());
        remove(t.file("b.png").c_str());
    }
    {
        std::string out, err;
        int st = run(t.path, zippers() + " --dry-run draw --session " + quote(REPO + "/tests/sessions/s3_cartwright_steger.zsession") +
                     " -R 2 -o x.png", out, err);
        check(st == 0 && out.find("zipper3' ") == std::string::npos && out.find("/zipper3 -f subst_4.00_9.50_t3_r1.txt") !=
              std::string::npos && out.find(" -R 2 ") != std::string::npos && out.find("-R 2.554") == std::string::npos &&
              out.find(" -o x.png") != std::string::npos && out.find("--cull --focus 1") != std::string::npos,
              "--dry-run: the program's command line, -R 2 in place of the session's (%s)", out.substr(0, 300).c_str());
    }
}

// a polyline file's bytes without the header line "command ...", which names the command that wrote it
std::string without_command(const std::string& bytes)
{
    const size_t data = bytes.find("\ndata\n");
    const size_t p = data == std::string::npos ? std::string::npos : bytes.rfind("\ncommand ", data);
    if (p == std::string::npos) return bytes;
    return bytes.substr(0, p) + bytes.substr(bytes.find('\n', p + 1));
}

// a session made here from a command line, its file's hash that of the suite's data, written to fn
bool session_of(const std::vector<std::string>& args, const std::string& fn, se::Session& s, std::string& why)
{
    std::vector<std::string> others;
    if (!se::from_args(args, s, &others, why)) return false;
    s.made_by = se::made_by_text("test");
    const int sp = s.sphere();
    if (sp == 3 || sp == 4) {
        std::string hex;
        if (!zippers::sha256_file(REPO + "/data/s" + std::to_string(sp) + "/" + *s.value("f"), hex, &why)) return false;
        se::set_hash(s, "sha256.f", hex);
    }
    std::string err;
    if (!se::write(fn, s, &err)) {
        why = err;
        return false;
    }
    return true;
}

void test_zoomed(const rtest::TempDir& t)
{
    section("a zoomed command line without -o: its session replays the command's curves; with -o, its still");
    struct Case { const char* what; std::vector<std::string> args; };
    const Case cases[] = {
        {"S3", {"cs", "-R", "2.554", "-a", "0.05", "--zoom", "3"}},
        {"S4", {"imm", "-R", "1.318", "-a", "0.08", "--zoom", "3"}},
    };
    for (const Case& c : cases) {
        se::Session s;
        std::string why, out, e1, e2;
        if (!check(session_of(c.args, t.file("z.zsession"), s, why), "%s: a session of the command line %s (%s)", c.what,
                   joined(c.args).c_str(), why.c_str()))
            continue;
        check(s.value("cull") && *s.value("cull") == "no" && !s.value("focus"),
              "%s: without -o, cull = no and no focus ball written (the command makes no still)", c.what);
        const std::string direct = zippers() + " draw " + joined(c.args);
        int a = run(t.path, direct + " --polylines d.zpl", out, e1);
        int b = run(t.path, zippers() + " draw --session z.zsession --polylines r.zpl", out, e2);
        const std::string d = rtest::read_file(t.file("d.zpl")), r = rtest::read_file(t.file("r.zpl"));
        check(a == 0 && b == 0 && !d.empty() && without_command(d) != d && without_command(d) == without_command(r),
              "%s: replayed with --polylines, the command's own file, byte for byte but its command line (%zu bytes; exit "
              "%d: %s; exit %d: %s)", c.what, d.size(), a, e1.substr(0, 200).c_str(), b, e2.substr(0, 200).c_str());
        a = run(t.path, direct + " -s 200 -o d.png", out, e1);
        b = run(t.path, zippers() + " draw --session z.zsession -s 200 -o r.png", out, e2);
        const std::string dp = rtest::read_file(t.file("d.png")), rp = rtest::read_file(t.file("r.png"));
        check(a == 0 && b == 0 && !dp.empty() && dp == rp, "%s: replayed with -o, the still of the command line with -o (the "
              "programs' defaults of a zoomed still), byte for byte (%zu bytes)", c.what, dp.size());
        // a command line with -o: the still's culling and focus ball written, and its still remade
        std::vector<std::string> with_o = c.args;
        with_o.insert(with_o.end(), {"-s", "200", "-o", "x.png"});
        se::Session so;
        if (check(session_of(with_o, t.file("zo.zsession"), so, why), "%s: a session of it with -o (%s)", c.what, why.c_str())) {
            check(so.value("cull") && *so.value("cull") == "yes" && so.value("focus") && *so.value("focus") == "1",
                  "%s: with -o, cull = yes and focus = 1 written", c.what);
            b = run(t.path, zippers() + " draw --session zo.zsession -o r2.png", out, e2);
            check(b == 0 && rtest::read_file(t.file("r2.png")) == dp && !dp.empty(), "%s: and its still, byte for byte "
                  "(exit %d: %s)", c.what, b, e2.substr(0, 200).c_str());
        }
        for (const char* f : {"d.zpl", "r.zpl", "d.png", "r.png", "r2.png", "z.zsession", "zo.zsession"})
            remove(t.file(f).c_str());
    }

    section("S2: --stats is a draw key, and the memory limit is the program's for the command: 6 GB with --stats, else 4");
    struct M { std::vector<std::string> args; const char* memlimit; const char* stats; };
    const M ms[] = {{{"fig8"}, "4", "no"}, {{"fig8", "--stats"}, "6", "yes"}, {{"fig8", "--stats", "--memlimit", "2"}, "2", "yes"},
                    {{"fig8", "--memlimit", "1.5", "-o", "x.png"}, "1.5", "no"}};
    for (const M& m : ms) {
        se::Session s;
        std::vector<std::string> others;
        std::string why;
        const bool ok = se::from_args(m.args, s, &others, why);
        const std::string* ml = ok ? s.value("memlimit") : nullptr;
        const std::string* st = ok ? s.value("stats") : nullptr;
        check(ml && *ml == m.memlimit && st && *st == m.stats &&
              std::find(others.begin(), others.end(), "--stats") == others.end(),
              "%s: memlimit = %s, stats = %s (%s)", joined(m.args).c_str(), ml ? ml->c_str() : "none", st ? st->c_str() : "none",
              why.c_str());
    }
    {
        se::Session s;
        std::vector<std::string> others;
        std::string why;
        se::from_args({"fig8"}, s, &others, why);
        check(se::apply(s, {"--stats", "-o", "x.png"}, others, why) && s.value("stats") && *s.value("stats") == "yes" &&
              others == std::vector<std::string>{"-o", "x.png"}, "--stats after an S2 session sets its key (%s)", why.c_str());
        for (const char* ex : {"cs", "imm"}) {
            se::Session s3;
            check(se::from_args({ex, "--stats"}, s3, &others, why) && !s3.value("stats") &&
                  others == std::vector<std::string>{"--stats"}, "%s: --stats only prints, and is not a draw key", ex);
        }
    }
    {
        // through zippers: a session of a --stats command replays it with --stats, and one without, without
        se::Session s;
        std::string why, out, err, o2, e2;
        session_of({"fig8", "-R", "3", "-s", "200", "--stats"}, t.file("st.zsession"), s, why);
        session_of({"fig8", "-R", "3", "-s", "200"}, t.file("ns.zsession"), s, why);
        int a = run(t.path, zippers() + " --dry-run draw --session st.zsession -o x.png", out, err);
        int b = run(t.path, zippers() + " --dry-run draw --session ns.zsession -o x.png", o2, e2);
        check(a == 0 && out.find(" --memlimit 6 ") != std::string::npos && out.find(" --stats") != std::string::npos &&
              b == 0 && o2.find(" --memlimit 4 ") != std::string::npos && o2.find("--stats") == std::string::npos,
              "through zippers: --stats --memlimit 6 replayed, and --memlimit 4 without it (%s; %s)", out.substr(0, 200).c_str(),
              o2.substr(0, 200).c_str());
        // its picture, byte for byte
        a = run(t.path, zippers() + " draw fig8 -R 3 -s 200 --stats -o d.png", out, err);
        b = run(t.path, zippers() + " draw --session st.zsession -o r.png", o2, e2);
        const std::string dp = rtest::read_file(t.file("d.png"));
        check(a == 0 && b == 0 && !dp.empty() && dp == rtest::read_file(t.file("r.png")),
              "the picture of fig8 -R 3 -s 200 --stats, remade by its session, byte for byte (exit %d: %s; exit %d: %s)", a,
              err.substr(0, 200).c_str(), b, e2.substr(0, 200).c_str());
        for (const char* f : {"st.zsession", "ns.zsession", "d.png", "r.png"}) remove(t.file(f).c_str());
    }
}

void test_hashes(const rtest::TempDir& t)
{
    section("the hashes: another file of the name is refused, with where it was found and both hashes");
    const std::string fn = REPO + "/tests/sessions/s3_cartwright_steger.zsession";
    se::Session s;
    std::string err;
    se::read(fn, s, &err);
    rtest::TempDir w;
    // a file of the default's name in the working folder, which the data search finds first
    rtest::write_file(w.file("subst_4.00_9.50_t3_r1.txt"), rtest::read_file(REPO + "/data/s3/subst_4.00_9.50_t3.txt"));
    std::string other;
    zippers::sha256_file(w.file("subst_4.00_9.50_t3_r1.txt"), other);
    std::string out;
    int st = run(w.path, zippers() + " draw --session " + quote(fn) + " -o x.png", out, err);
    check(st == 1 && err.find("the substitution file subst_4.00_9.50_t3_r1.txt, found in the working folder, has the SHA-256 " +
                              other + "; the session's is " + *s.value("sha256.f")) != std::string::npos &&
          rtest::read_file(w.file("x.png")).empty(), "a file of the name in the working folder: refused (exit %d: %s)", st,
          err.substr(0, 400).c_str());
    // -f after the session replaces the file and its check
    st = run(w.path, zippers() + " --dry-run draw --session " + quote(fn) + " -f subst_4.00_9.50_t3_r1.txt -o x.png", out, err);
    check(st == 0 && out.find("-f subst_4.00_9.50_t3_r1.txt") != std::string::npos,
          "-f after the session: the file given is not checked (exit %d: %s)", st, err.substr(0, 200).c_str());
    // a session whose hash was changed
    std::string text = rtest::read_file(fn);
    const size_t p = text.find("sha256.f = ") + 11;
    text[p] = text[p] == '0' ? '1' : '0';
    rtest::write_file(t.file("changed.zsession"), text);
    st = run(t.path, zippers() + " --dry-run draw --session changed.zsession", out, err);
    check(st == 1 && err.find("has the SHA-256 " + *s.value("sha256.f")) != std::string::npos &&
          err.find("found at ") != std::string::npos, "a changed hash: refused, the file found at its path (exit %d: %s)", st,
          err.substr(0, 400).c_str());
    // the warning for an unknown window key, through zippers
    rtest::write_file(t.file("w.zsession"), rtest::read_file(REPO + "/tests/sessions/s2_fig8.zsession") + "window.bogus = 1\n");
    st = run(t.path, zippers() + " --dry-run draw --session w.zsession", out, err);
    check(st == 0 && err.find("warning:") != std::string::npos && err.find("window.bogus is not known") != std::string::npos,
          "an unknown window key: a warning, and the picture's command (exit %d: %s)", st, err.substr(0, 300).c_str());
    // refused by zippers: a later version, an unknown draw key, after an example
    rtest::write_file(t.file("v2.zsession"), "zippers session 2\nexample = fig8\n");
    st = run(t.path, zippers() + " --dry-run draw --session v2.zsession", out, err);
    check(st == 1 && err.find("this session is of format 2, written by a newer zippers; this one reads format 1") !=
          std::string::npos, "a later version: refused by zippers (%s)", err.substr(0, 200).c_str());
    st = run(t.path, zippers() + " --dry-run draw fig8 --session w.zsession", out, err);
    check(st == 1 && err.find("--session takes the place of the example") != std::string::npos,
          "--session after an example: refused (%s)", err.substr(0, 200).c_str());
}

}  // namespace

int main()
{
    printf("test_gui_session: the session file and zippers draw --session\n");
    if (!*ZIPPERS_ZIPPERS) {
        printf("zippers is not built\n");
        return 1;
    }
    rtest::TempDir t;
    test_round_trips(t);
    test_refusals(t);
    test_apply();
    test_stored(t);
    test_hashes(t);
    test_zoomed(t);
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
