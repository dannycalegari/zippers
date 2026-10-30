// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_default_files: which data files the programs, the tools and zippers read, and how they find them (the search of
// src/core/data_files.h).
//
//   the defaults       zipper3 subst_4.00_9.50_t3_r1.txt; with --pairs, subst_4.00_9.50_t3.txt (the linked pairs are loops
//                      of its base arcs) and the pairs of --pairs; link3 subst_4.00_9.50_t3.txt; zipper5
//                      subst5_2.30_4.50_t1_r0.txt; film5 subst5_2.30_4.50_t1.txt; the tools roofs, cull, strays, overdraw
//                      (S^3 and S^4) and islands5 (S^4) follow zipper3 and zipper5. All of them are in data/.
//   the search         a name with a '/' as given; else the working folder, then the folders of ZIPPERS_DATA (D/s3/N, then
//                      D/N), then the data of the program: data/ of the source tree, for a program in the folder of its
//                      build. A file found in the working folder keeps its plain name; one found elsewhere is named by
//                      its full path. Found nowhere: exit status 1, and a message that lists the places looked in.
//   -f                 names the file, and wins over every default, --pairs included; a name without '/' is searched for.
//
// The cases:
//   - a folder that holds the files: each program reads them there, under their plain names (what the programs printed
//     when they read the working folder only, which is why no reference run changes);
//   - a folder that holds only the files of other names: a default is never replaced by a file of another name;
//   - an empty folder, from the build: the data of the source tree, named by their full paths;
//   - ZIPPERS_DATA: after the working folder and before the data of the program, D/s3/N before D/N;
//   - copies of the programs outside the folder of their build, which therefore have no data of their own: found
//     nowhere, the message says where they looked; with ZIPPERS_DATA, they find the files there;
//   - -f with a plain name that is nowhere, and with a path;
//   - zippers list: where it finds the default files, from the build, with ZIPPERS_DATA, and from a copy outside it.
// Each case runs the programs in a folder of their own, which holds only the files chosen; the programs print "FILE: |S|
// = n ..." for the substitution file they read (prepare, render/world.h) and "FILE: n linked pairs" for the pairs, and
// the test looks at those names. ZIPPERS_DATA is unset in every run that does not set it. Everything is compared between
// runs of this machine's own programs and with names, never with a stored file, so it holds on every platform. About
// twenty seconds (the runs are at ACC 0.3 and R = 0). Uses the programs themselves (ZIPPERS_ZIPPER3, ZIPPERS_ZIPPER5,
// ZIPPERS_LINK3, ZIPPERS_CLI, the tools, and ZIPPERS_FILM5 when it is built, run with --list, which opens no window);
// ZIPPERS_REPO is the source tree.

#include "render_test_support.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using rtest::check;
using rtest::note;
using rtest::section;

namespace {

const char* const NEW3 = "subst_4.00_9.50_t3_r1.txt";     // the default of zipper3
const char* const OLD3 = "subst_4.00_9.50_t3.txt";        // that of link3, and of zipper3 --pairs
const char* const NEW4 = "subst5_2.30_4.50_t1_r0.txt";    // the default of zipper5
const char* const OLD4 = "subst5_2.30_4.50_t1.txt";       // that of film5
const char* const PAIRS = "linked_pairs.txt";

std::string repo_data(const std::string& sphere, const std::string& name)
{
    return std::string(ZIPPERS_REPO) + "/data/" + sphere + "/" + name;
}

// run `cmd` in the folder `dir` (stdin closed), with ZIPPERS_DATA unset unless `env` sets it ("ZIPPERS_DATA=..."); its
// standard output and standard error together; the exit status
int run_in(const std::string& dir, const std::string& cmd, std::string& out, const std::string& env = "")
{
    out.clear();
    std::string line = "cd '" + dir + "' && unset ZIPPERS_DATA && " + (env.empty() ? "" : env + " ") + cmd + " 2>&1 <&-";
    FILE* p = popen(line.c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// a copy of the repository's file `rel` (relative to the source tree) in the folder `dir`, under the name `as`
void put(const std::string& dir, const std::string& rel, const std::string& as = "")
{
    std::string text = rtest::read_file(std::string(ZIPPERS_REPO) + "/" + rel);
    size_t slash = rel.rfind('/');
    if (text.empty() || !rtest::write_file(dir + "/" + (as.empty() ? rel.substr(slash + 1) : as), text)) {
        fprintf(stderr, "test_default_files: cannot copy %s\n", rel.c_str());
        exit(2);
    }
}

// a copy of the program `exe` in the folder `dir` (outside the folder of its build); its path
std::string copy_program(const std::string& exe, const std::string& dir)
{
    std::string to = dir + "/" + exe.substr(exe.rfind('/') + 1);
    std::string bytes = rtest::read_file(exe);
    if (bytes.empty() || !rtest::write_file(to, bytes) || chmod(to.c_str(), 0755) != 0) {
        fprintf(stderr, "test_default_files: cannot copy %s\n", exe.c_str());
        exit(2);
    }
    return to;
}

// the names before `marker` on the lines that have it ("FILE: |S| = ...": the substitution file that a run read)
std::set<std::string> names_before(const std::string& out, const std::string& marker)
{
    std::set<std::string> names;
    for (const std::string& line : rtest::split_lines(out)) {
        size_t p = line.find(marker);
        if (p != std::string::npos) names.insert(line.substr(0, p));
    }
    return names;
}
std::set<std::string> files_read(const std::string& out) { return names_before(out, ": |S| = "); }
std::set<std::string> pairs_read(const std::string& out) { return names_before(out, ": 13 linked pairs"); }

std::string join(const std::set<std::string>& s)
{
    std::string r;
    for (const std::string& n : s) r += (r.empty() ? "" : ", ") + n;
    return r.empty() ? "nothing" : r;
}

std::string quote(const std::string& exe) { return "'" + exe + "'"; }

// the run reads exactly the substitution file `want`, named so (and exits 0); `pairs`: and the pairs file `pairs`
void reads(const std::string& dir, const std::string& cmd, const std::string& what, const std::string& want,
           const std::string& env = "", const std::string& pairs = "")
{
    std::string out;
    int st = run_in(dir, cmd, out, env);
    std::set<std::string> got = files_read(out);
    bool ok = st == 0 && got == std::set<std::string>{want};
    if (!pairs.empty()) ok = ok && pairs_read(out) == std::set<std::string>{pairs};
    check(ok, "%s reads %s%s%s (exit %d; it read: %s%s%s)", what.c_str(), want.c_str(), pairs.empty() ? "" : " and ",
          pairs.c_str(), st, join(got).c_str(), pairs.empty() ? "" : "; pairs: ", pairs.empty() ? "" : join(pairs_read(out)).c_str());
}

// the run finds the file `file` nowhere: exit status 1, the message with the places it looked in (`places`: the first
// words of each line after "looked in:"), and nothing read; `what`: the words after the name, or ""
void finds_nothing(const std::string& dir, const std::string& cmd, const std::string& what, const std::string& file,
                   const std::string& paren, const std::vector<std::string>& places, const std::string& env = "")
{
    std::string out;
    int st = run_in(dir, cmd, out, env);
    std::string head = "cannot find " + file + (paren.empty() ? "" : " (" + paren + ")") + "; looked in:\n";
    size_t p = out.find(head);
    bool ok = st == 1 && p != std::string::npos && files_read(out).empty() &&
              out.find("give its path with ") != std::string::npos;
    if (ok) {
        std::vector<std::string> lines = rtest::split_lines(out.substr(p + head.size()));
        ok = lines.size() > places.size();
        for (size_t i = 0; ok && i < places.size(); i++) ok = lines[i].rfind("  " + places[i], 0) == 0;
        if (ok) ok = lines[places.size()].rfind("give its path with ", 0) == 0;
    }
    check(ok, "%s finds %s nowhere, and says where it looked (exit %d; output \"%s\")", what.c_str(), file.c_str(), st,
          ok ? "as it should" : rtest::chomp(out).substr(0, 600).c_str());
}

// the run stops at the first place that holds an entry of the name `file`, which is not a readable file: exit status 1,
// "cannot use FILE (...): it is WHAT, in PLACE: PATH", and nothing read
void stops_at(const std::string& dir, const std::string& cmd, const std::string& what, const std::string& file,
              const std::string& paren, const std::string& is, const std::string& place, const std::string& path,
              const std::string& env = "")
{
    std::string out;
    int st = run_in(dir, cmd, out, env);
    const std::string says = "cannot use " + file + (paren.empty() ? "" : " (" + paren + ")") + ": it is " + is + ", in " +
                             place + ": " + path + "\n";
    bool ok = st == 1 && out.find(says) != std::string::npos && files_read(out).empty() &&
              out.find("a data file is taken from the first place that holds its name") != std::string::npos;
    check(ok, "%s stops at %s, %s in %s (exit %d; output \"%s\")", what.c_str(), file.c_str(), is.c_str(), place.c_str(), st,
          ok ? "as it should" : rtest::chomp(out).substr(0, 600).c_str());
}

// the source of a program contains the text `text`
bool source_has(const char* rel, const std::string& text)
{
    return rtest::read_file(std::string(ZIPPERS_REPO) + "/" + rel).find(text) != std::string::npos;
}

}  // namespace

int main()
{
    double t_start = rtest::clock_seconds();
    printf("test_default_files: which data files the programs, the tools and zippers read, and how they find them\n");
#if defined(ZIPPERS_ZIPPER3) && defined(ZIPPERS_ZIPPER5) && defined(ZIPPERS_LINK3) && defined(ZIPPERS_ROOFS) && \
    defined(ZIPPERS_CULL) && defined(ZIPPERS_STRAYS) && defined(ZIPPERS_OVERDRAW) && defined(ZIPPERS_ISLANDS5) && \
    defined(ZIPPERS_CLI) && defined(ZIPPERS_REPO)
    // the runs: light ones (R = 0, ACC 0.3); each program as a command for a given path
    auto z3 = [](const std::string& exe) { return quote(exe) + " -R 0 -a 0.3"; };
    auto z3pairs = [](const std::string& exe) { return quote(exe) + " -R 0 -a 0.1 --pairs " + PAIRS; };
    auto z5 = [](const std::string& exe) { return quote(exe) + " -R 0 -a 0.3"; };
    auto l3 = [](const std::string& exe) { return quote(exe) + " --rr 0.5 --nrel 0 --hr 0.5 --hinfo"; };
    struct Tool { const char* name; std::string exe; bool cs; };      // cs: the S^3 example, else S^4
    auto tool_cmd = [](const Tool& t, const std::string& exe) -> std::string {
        std::string e = t.cs ? " -e cs" : " -e imm";
        std::string n = t.name;
        if (n == "roofs") return quote(exe) + e;
        if (n == "cull") return quote(exe) + e + " --mode tips";
        if (n == "strays") return quote(exe) + e + " --mode picture -R 0 -a 0.3 -k 2";
        if (n == "overdraw") return quote(exe) + e + " -R 0 -a 0.3 --nogeo --keyshift 0";
        return quote(exe) + " -R 0 -a 0.3 --graph";                    // islands5 (S^4 only)
    };
    std::vector<Tool> tools;
    for (bool cs : {true, false}) {
        tools.push_back({"roofs", ZIPPERS_ROOFS, cs});
        tools.push_back({"cull", ZIPPERS_CULL, cs});
        tools.push_back({"strays", ZIPPERS_STRAYS, cs});
        tools.push_back({"overdraw", ZIPPERS_OVERDRAW, cs});
    }
    tools.push_back({"islands5", ZIPPERS_ISLANDS5, false});
    auto tname = [](const Tool& t) { return std::string(t.name) + (std::string(t.name) == "islands5" ? "" : t.cs ? " -e cs" : " -e imm"); };
    const std::string Z3 = ZIPPERS_ZIPPER3, Z5 = ZIPPERS_ZIPPER5, L3 = ZIPPERS_LINK3, ZC = ZIPPERS_CLI;

    section("a folder that holds the files: each reads them there, under their plain names");
    {
        rtest::TempDir d;
        for (const char* f : {NEW3, OLD3, PAIRS}) put(d.path, std::string("data/s3/") + f);
        for (const char* f : {NEW4, OLD4}) put(d.path, std::string("data/s4/") + f);
        reads(d.path, z3(Z3), "zipper3", NEW3);
        reads(d.path, z3pairs(Z3), "zipper3 --pairs", OLD3, "", PAIRS);
        reads(d.path, z3(Z3) + " -f " + OLD3, "zipper3 -f the old file", OLD3);
        reads(d.path, z3pairs(Z3) + " -f " + NEW3, "zipper3 --pairs -f the new file (-f wins over the --pairs default)", NEW3,
              "", PAIRS);
        reads(d.path, l3(L3), "link3", OLD3);
        reads(d.path, z5(Z5), "zipper5", NEW4);
        reads(d.path, z5(Z5) + " -f " + OLD4, "zipper5 -f the old file", OLD4);
        for (const Tool& t : tools) reads(d.path, tool_cmd(t, t.exe), tname(t), t.cs ? NEW3 : NEW4);
        for (const Tool& t : tools)
            reads(d.path, tool_cmd(t, t.exe) + " -f " + (t.cs ? OLD3 : OLD4), tname(t) + " -f the old file", t.cs ? OLD3 : OLD4);
#ifdef ZIPPERS_FILM5
        reads(d.path, quote(ZIPPERS_FILM5) + " --list", "film5 --list", OLD4);
#endif
    }

    section("a folder that holds only the files of other names: a default is never replaced by another file");
    {
        rtest::TempDir old;                          // only the old files: the new defaults come from the data of the build
        put(old.path, std::string("data/s3/") + OLD3);
        put(old.path, std::string("data/s4/") + OLD4);
        reads(old.path, z3(Z3), "zipper3 (the old file only, here)", repo_data("s3", NEW3));
        reads(old.path, z5(Z5), "zipper5 (the old file only, here)", repo_data("s4", NEW4));
        for (const Tool& t : tools)
            reads(old.path, tool_cmd(t, t.exe), tname(t) + " (the old file only, here)", t.cs ? repo_data("s3", NEW3) : repo_data("s4", NEW4));
        rtest::TempDir fresh;                        // only the new files: the old ones come from the data of the build
        put(fresh.path, std::string("data/s3/") + NEW3);
        put(fresh.path, std::string("data/s4/") + NEW4);
        reads(fresh.path, z3pairs(Z3), "zipper3 --pairs (the new file only, here)", repo_data("s3", OLD3), "",
              repo_data("s3", PAIRS));
        reads(fresh.path, l3(L3), "link3 (the new file only, here)", repo_data("s3", OLD3));
    }

    section("an empty folder, from the build: the data of the source tree, named by their full paths");
    {
        rtest::TempDir e;
        reads(e.path, z3(Z3), "zipper3", repo_data("s3", NEW3));
        reads(e.path, z3pairs(Z3), "zipper3 --pairs", repo_data("s3", OLD3), "", repo_data("s3", PAIRS));
        reads(e.path, l3(L3), "link3", repo_data("s3", OLD3));
        reads(e.path, z5(Z5), "zipper5", repo_data("s4", NEW4));
        reads(e.path, z3(Z3) + " -f " + OLD3, "zipper3 -f a plain name", repo_data("s3", OLD3));
        for (const Tool& t : tools)
            reads(e.path, tool_cmd(t, t.exe), tname(t), t.cs ? repo_data("s3", NEW3) : repo_data("s4", NEW4));
#ifdef ZIPPERS_FILM5
        reads(e.path, quote(ZIPPERS_FILM5) + " --list", "film5 --list", repo_data("s4", OLD4));
#endif
    }

    section("ZIPPERS_DATA: after the working folder, before the data of the build; D/s3/N before D/N");
    {
        rtest::TempDir d1, d2, e, here;
        mkdir((d1.path + "/s3").c_str(), 0755);
        put(d1.path + "/s3", std::string("data/s3/") + NEW3);
        put(d1.path + "/s3", std::string("data/s3/") + OLD3);
        put(d1.path, std::string("data/s3/") + OLD3);               // D/N: shadowed by D/s3/N
        put(d2.path, std::string("data/s4/") + NEW4);               // D/N alone
        put(d2.path, std::string("data/s3/") + PAIRS);
        put(here.path, std::string("data/s3/") + NEW3);
        const std::string env = "ZIPPERS_DATA='" + d1.path + ":" + d2.path + "'";
        reads(e.path, z3(Z3), "zipper3", d1.path + "/s3/" + NEW3, env);
        reads(e.path, l3(L3), "link3 (D/s3/N before D/N)", d1.path + "/s3/" + OLD3, env);
        reads(e.path, z3pairs(Z3), "zipper3 --pairs (the pairs from the second folder)", d1.path + "/s3/" + OLD3, env,
              d2.path + "/" + PAIRS);
        reads(e.path, z5(Z5), "zipper5 (D/N of the second folder)", d2.path + "/" + NEW4, env);
        for (const Tool& t : tools)
            reads(e.path, tool_cmd(t, t.exe), tname(t), t.cs ? d1.path + "/s3/" + NEW3 : d2.path + "/" + NEW4, env);
        reads(here.path, z3(Z3), "zipper3 (the working folder before ZIPPERS_DATA)", NEW3, env);
        reads(e.path, z5(Z5), "zipper5 (folders that do not exist are passed over)", repo_data("s4", NEW4),
              "ZIPPERS_DATA=/nowhere/at/all::/nor/here");
    }

    section("copies outside the folder of their build: found nowhere, they say where they looked");
    {
        rtest::TempDir bin, e, d1;
        const std::string c3 = copy_program(Z3, bin.path), c5 = copy_program(Z5, bin.path), cl = copy_program(L3, bin.path);
        const std::vector<std::string> places = {"the working folder: ", "ZIPPERS_DATA: not set", "the installed data: "};
        finds_nothing(e.path, z3(c3), "zipper3, copied", NEW3, "the default substitution file", places);
        finds_nothing(e.path, z5(c5), "zipper5, copied", NEW4, "the default substitution file", places);
        finds_nothing(e.path, l3(cl), "link3, copied", OLD3, "the default substitution file", places);
        {
            rtest::TempDir s;                        // --pairs: the substitution file is here, the pairs are not
            put(s.path, std::string("data/s3/") + OLD3);
            finds_nothing(s.path, z3pairs(c3), "zipper3 --pairs, copied", PAIRS, "the file of linked pairs", places);
        }
        for (const Tool& t : tools) {
            std::string c = copy_program(t.exe, bin.path);
            finds_nothing(e.path, tool_cmd(t, c), tname(t) + ", copied", t.cs ? NEW3 : NEW4, "the default substitution file",
                          places);
        }
        mkdir((d1.path + "/s3").c_str(), 0755);
        put(d1.path + "/s3", std::string("data/s3/") + NEW3);
        reads(e.path, z3(c3), "zipper3, copied, with ZIPPERS_DATA", d1.path + "/s3/" + NEW3, "ZIPPERS_DATA='" + d1.path + "'");
        finds_nothing(e.path, z5(c5), "zipper5, copied, with ZIPPERS_DATA", NEW4, "the default substitution file",
                      {"the working folder: ", "ZIPPERS_DATA: " + d1.path + "/s4/" + NEW4, "ZIPPERS_DATA: " + d1.path + "/" + NEW4,
                       "the installed data: "},
                      "ZIPPERS_DATA='" + d1.path + "'");
    }

    section("-f: a plain name is searched for; a path is read as given");
    {
        rtest::TempDir e;
        const std::vector<std::string> places = {"the working folder: ", "ZIPPERS_DATA: not set", "the data of this build: "};
        finds_nothing(e.path, z3(Z3) + " -f nowhere.txt", "zipper3 -f nowhere.txt", "nowhere.txt", "", places);
        finds_nothing(e.path, z5(Z5) + " -f nowhere.txt", "zipper5 -f nowhere.txt", "nowhere.txt", "", places);
        finds_nothing(e.path, l3(L3) + " -f nowhere.txt", "link3 -f nowhere.txt", "nowhere.txt", "", places);
        finds_nothing(e.path, z3(Z3) + " --pairs nowhere.txt", "zipper3 --pairs nowhere.txt", "nowhere.txt",
                      "the file of linked pairs", places);
        for (const Tool& t : tools)
            finds_nothing(e.path, tool_cmd(t, t.exe) + " -f nowhere.txt", tname(t) + " -f nowhere.txt", "nowhere.txt", "",
                          places);
        std::string out;
        for (const std::pair<std::string, std::string>& p : {std::make_pair(std::string("zipper3"), z3(Z3)),
                                                              std::make_pair(std::string("zipper5"), z5(Z5)),
                                                              std::make_pair(std::string("link3"), l3(L3))}) {
            int st = run_in(e.path, p.second + " -f ./nowhere.txt", out);
            check(st == 1 && out.find("cannot open ./nowhere.txt") != std::string::npos &&
                  out.find("looked in") == std::string::npos,
                  "%s -f ./nowhere.txt: the reader's \"cannot open\", and no search (exit %d; output \"%s\")", p.first.c_str(),
                  st, rtest::chomp(out).substr(0, 200).c_str());
        }
    }

    section("zippers list: where the default files are found");
    {
        rtest::TempDir e, here, d1, bin;
        auto found = [&](const std::string& dir, const std::string& cmd, const std::string& file, const std::string& where,
                         const std::string& env, const std::string& what) {
            std::string out;
            int st = run_in(dir, cmd, out, env);
            bool ok = st == 0, any = false;
            for (const std::string& line : rtest::split_lines(out))
                if ((line + " ").find(" " + file + " ") != std::string::npos && line.find("other files") == std::string::npos) {
                    any = true;
                    ok = ok && line.find(where) != std::string::npos;
                }
            check(ok && any, "%s: %s %s (exit %d; output \"%s\")", what.c_str(), file.c_str(), where.c_str(), st,
                  ok && any ? "as it should" : rtest::chomp(out).substr(0, 500).c_str());
        };
        const std::string list3 = quote(ZC) + " list cartwright-steger", list4 = quote(ZC) + " list imm-m5";
        for (const char* f : {NEW3, OLD3, PAIRS})
            found(e.path, list3, f, "found in " + std::string(ZIPPERS_REPO) + "/data/s3", "", "zippers list, from the build");
        found(e.path, list4, NEW4, "found in " + std::string(ZIPPERS_REPO) + "/data/s4", "", "zippers list, from the build");
        put(here.path, std::string("data/s3/") + NEW3);
        found(here.path, list3, NEW3, "found in the working folder", "", "zippers list, in a folder that holds it");
        found(here.path, list3, OLD3, "found in " + std::string(ZIPPERS_REPO) + "/data/s3", "",
              "zippers list, in a folder that holds another");
        mkdir((d1.path + "/s3").c_str(), 0755);
        put(d1.path + "/s3", std::string("data/s3/") + NEW3);
        found(e.path, list3, NEW3, "found in " + d1.path + "/s3", "ZIPPERS_DATA='" + d1.path + "'", "zippers list, with ZIPPERS_DATA");
        const std::string cz = copy_program(ZC, bin.path);
        found(e.path, quote(cz) + " list cartwright-steger", NEW3, "NOT FOUND", "", "zippers list, copied outside the build");
        std::string out;
        int st = run_in(e.path, quote(cz) + " list imm-m5", out);
        check(st == 0 && out.find("(looked in the working folder: ") != std::string::npos &&
              out.find("(looked in ZIPPERS_DATA: not set)") != std::string::npos &&
              out.find("(looked in the installed data: ") != std::string::npos,
              "zippers list, copied outside the build: the places it looked in (exit %d; output \"%s\")", st,
              rtest::chomp(out).substr(0, 500).c_str());
        st = run_in(e.path, quote(cz) + " --dry-run draw cartwright-steger", out);
        check(st == 1 && out.find("cannot find the program zipper3 (looked for ") != std::string::npos,
              "zippers, copied outside the build: no programs of its own to run, and it says where it looked (exit %d; "
              "output \"%s\")", st, rtest::chomp(out).substr(0, 300).c_str());
    }

    section("an entry of the name that is not a readable file: the search stops there, and says what is there");
    {
        // the working folder of the run, made real (getcwd's answer, which the message prints)
        auto real = [](const std::string& d) { char b[PATH_MAX]; return std::string(realpath(d.c_str(), b) ? b : d.c_str()); };
        rtest::TempDir w, d1, d2, e;
        const std::string W = real(w.path), D1 = real(d1.path), D2 = real(d2.path);
        mkdir((w.path + "/" + NEW3).c_str(), 0755);                 // a folder of the name, in the working folder
        stops_at(w.path, z3(Z3), "zipper3", NEW3, "the default substitution file", "a folder", "the working folder",
                 W + "/" + NEW3);
        stops_at(w.path, z3(Z3) + " -f " + NEW3, "zipper3 -f NAME", NEW3, "", "a folder", "the working folder",
                 W + "/" + NEW3);
        for (const Tool& t : tools)
            if (t.cs)
                stops_at(w.path, tool_cmd(t, t.exe), tname(t), NEW3, "the default substitution file", "a folder",
                         "the working folder", W + "/" + NEW3);
        // a folder of the name in ZIPPERS_DATA, D/s4/N (D/N, a readable file, is not read: the search stops first)
        mkdir((d1.path + "/s4").c_str(), 0755);
        mkdir((d1.path + "/s4/" + NEW4).c_str(), 0755);
        put(d1.path, std::string("data/s4/") + NEW4);
        const std::string env1 = "ZIPPERS_DATA='" + D1 + "'";
        stops_at(e.path, z5(Z5), "zipper5", NEW4, "the default substitution file", "a folder", "ZIPPERS_DATA",
                 D1 + "/s4/" + NEW4, env1);
        stops_at(e.path, quote(ZIPPERS_ISLANDS5) + " -R 0 -a 0.3 --graph", "islands5", NEW4, "the default substitution file",
                 "a folder", "ZIPPERS_DATA", D1 + "/s4/" + NEW4, env1);
        // the second folder of ZIPPERS_DATA is not reached either
        put(d2.path, std::string("data/s4/") + NEW4);
        stops_at(e.path, z5(Z5), "zipper5 (a second folder of ZIPPERS_DATA)", NEW4, "the default substitution file",
                 "a folder", "ZIPPERS_DATA", D1 + "/s4/" + NEW4, "ZIPPERS_DATA='" + D1 + ":" + D2 + "'");
        // a folder at D/N, with no D/s3/N
        mkdir((d2.path + "/" + PAIRS).c_str(), 0755);
        {
            rtest::TempDir s3;
            put(s3.path, std::string("data/s3/") + OLD3);
            stops_at(s3.path, z3pairs(Z3), "zipper3 --pairs", PAIRS, "the file of linked pairs", "a folder", "ZIPPERS_DATA",
                     D2 + "/" + PAIRS, "ZIPPERS_DATA='" + D2 + "'");
        }
        // a file of the name that cannot be read (as root every file can be read: then this case is left out)
        if (geteuid() != 0) {
            rtest::TempDir u;
            const std::string U = real(u.path);
            put(u.path, std::string("data/s4/") + NEW4);
            chmod((u.path + "/" + NEW4).c_str(), 0);
            stops_at(u.path, z5(Z5), "zipper5", NEW4, "the default substitution file", "a file that cannot be read",
                     "the working folder", U + "/" + NEW4);
            chmod((u.path + "/" + NEW4).c_str(), 0644);
        } else {
            note("run as root, a file cannot be made unreadable: that case is left out");
        }
        // a link to nothing
        {
            rtest::TempDir l;
            const std::string L = real(l.path);
            if (symlink("nowhere.txt", (l.path + "/" + NEW3).c_str()) == 0)
                stops_at(l.path, l3(L3) + " -f " + NEW3, "link3 -f NAME", NEW3, "", "a link to nothing", "the working folder",
                         L + "/" + NEW3);
        }
        // zippers list says so
        std::string out;
        int st = run_in(w.path, quote(ZC) + " list cartwright-steger", out);
        check(st == 0 && out.find(std::string("CANNOT BE USED: it is a folder, in the working folder: ") + W + "/" + NEW3) !=
                             std::string::npos,
              "zippers list: the default that cannot be used, and why (exit %d; output \"%s\")", st,
              rtest::chomp(out).substr(0, 500).c_str());
    }

    section("ZIPPERS_DATA set to the empty string, or to separators only, is reported as such");
    {
        rtest::TempDir e;
        const std::vector<std::string> empty = {"the working folder: ", "ZIPPERS_DATA: set to the empty string",
                                                "the data of this build: "};
        finds_nothing(e.path, z3(Z3) + " -f nowhere.txt", "zipper3 -f nowhere.txt, ZIPPERS_DATA=''", "nowhere.txt", "",
                      empty, "ZIPPERS_DATA=''");
        const std::vector<std::string> colons = {"the working folder: ", "ZIPPERS_DATA: names no folder",
                                                 "the data of this build: "};
        finds_nothing(e.path, z5(Z5) + " -f nowhere.txt", "zipper5 -f nowhere.txt, ZIPPERS_DATA=::", "nowhere.txt", "",
                      colons, "ZIPPERS_DATA=::");
        reads(e.path, z3(Z3), "zipper3, ZIPPERS_DATA=''", repo_data("s3", NEW3), "ZIPPERS_DATA=''");
        std::string out;
        int st = run_in(e.path, quote(ZC) + " list cartwright-steger", out, "ZIPPERS_DATA=''");
        check(st == 0 && out.find("ZIPPERS_DATA (set to the empty string)") != std::string::npos,
              "zippers list, ZIPPERS_DATA='': says so (exit %d; output \"%s\")", st, rtest::chomp(out).substr(0, 400).c_str());
    }

    section("the sources: film5's default and link3's, and the film's script");
    check(source_has("apps/s4/film5.cc", "string subst = \"subst5_2.30_4.50_t1.txt\", landmarks, movie, still;"),
          "film5's default file is still subst5_2.30_4.50_t1.txt (the file of the film's ending)");
    check(source_has("apps/s3/link3.cc", "string subst = \"subst_4.00_9.50_t3.txt\";"),
          "link3's default file is still subst_4.00_9.50_t3.txt (the linked pairs are loops of its arcs)");
    check(source_has("film/make_joined_film.sh", "subst5_2.30_4.50_t1.txt"),
          "film/make_joined_film.sh still names subst5_2.30_4.50_t1.txt");
#ifndef ZIPPERS_FILM5
    note("film5 is not built in this configuration: its runs are left out");
#endif
#else
    note("the programs, the tools and zippers are not all built in this configuration: the test is left out");
#endif
    rtest::end_section();
    printf("test_default_files: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
