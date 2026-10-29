// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_default_files: which substitution file each program and tool reads when -f is not given, and what they say when
// it is missing.
//
//   zipper3            subst_4.00_9.50_t3_r1.txt (data/s3/); with --pairs, subst_4.00_9.50_t3.txt (legacy/s3/): the linked
//                      pairs are loops of that file's base arcs
//   zipper5            subst5_2.30_4.50_t1_r0.txt (data/s4/)
//   link3              subst_4.00_9.50_t3.txt, as before (and film5 subst5_2.30_4.50_t1.txt: it needs a window, so the test
//                      reads its source)
//   the tools          roofs, cull, strays, overdraw (S^3 and S^4) and islands5 (S^4) follow zipper3 and zipper5
//   a missing default  exit status 1, "cannot open FILE", then "the default file FILE is in data/s3/ (or data/s4/, or
//                      legacy/s3/ for --pairs) of the repository: run there, or give -f"
//   -f                 names the file; it wins over every default, --pairs included
//
// Each case runs the program in a folder of its own, holding only the files chosen, so that it is the folder that decides
// which file can be read; the programs print "FILE: |S| = n ..." for the file they read (prepare, render/world.h), and the
// test looks at that name. Folders with BOTH the old and the new file see which one is read; folders with only one see that
// a missing default is not replaced by the other. Everything is compared between runs of this machine's own programs and
// with names, never with a stored file, so it holds on every platform. A few seconds (the runs are at ACC 0.3 and R = 0).
// Uses the programs themselves (ZIPPERS_ZIPPER3, ZIPPERS_ZIPPER5, ZIPPERS_LINK3 and the tools; ZIPPERS_REPO is the
// repository).

#include "render_test_support.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

using rtest::check;
using rtest::note;
using rtest::section;

namespace {

const char* const NEW3 = "subst_4.00_9.50_t3_r1.txt";     // the default of zipper3 (data/s3/)
const char* const OLD3 = "subst_4.00_9.50_t3.txt";        // that of link3, and of zipper3 --pairs (legacy/s3/)
const char* const NEW4 = "subst5_2.30_4.50_t1_r0.txt";    // the default of zipper5 (data/s4/)
const char* const OLD4 = "subst5_2.30_4.50_t1.txt";       // that of film5, and the old default (legacy/s4/)
const char* const PAIRS = "linked_pairs.txt";

// run `cmd` in the folder `dir` (stdin closed); its standard output and standard error together; the exit status
int run_in(const std::string& dir, const std::string& cmd, std::string& out)
{
    out.clear();
    FILE* p = popen(("cd '" + dir + "' && " + cmd + " 2>&1 <&-").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// a copy of the repository's file `rel` in the folder, under its own name
void put(const rtest::TempDir& d, const std::string& rel)
{
    std::string text = rtest::read_file(std::string(ZIPPERS_REPO) + "/" + rel);
    size_t slash = rel.rfind('/');
    if (text.empty() || !rtest::write_file(d.file(rel.substr(slash + 1)), text)) {
        fprintf(stderr, "test_default_files: cannot copy %s\n", rel.c_str());
        exit(2);
    }
}

// the names of the files that a run read: the text before ": |S| = " on the lines that have it (prepare's line, and
// islands5's own)
std::set<std::string> files_read(const std::string& out)
{
    std::set<std::string> names;
    for (const std::string& line : rtest::split_lines(out)) {
        size_t p = line.find(": |S| = ");
        if (p != std::string::npos) names.insert(line.substr(0, p));
    }
    return names;
}

std::string quote(const char* exe) { return std::string("'") + exe + "'"; }

// the run reads exactly the file `want` (and exits 0)
void reads(const rtest::TempDir& d, const std::string& cmd, const std::string& what, const char* want)
{
    std::string out;
    int st = run_in(d.path, cmd, out);
    std::set<std::string> names = files_read(out);
    std::string got;
    for (const std::string& n : names) got += (got.empty() ? "" : ", ") + n;
    check(st == 0 && names == std::set<std::string>{want}, "%s reads %s (exit %d; it read: %s)", what.c_str(), want, st,
          got.empty() ? "nothing" : got.c_str());
}

// the run fails with status 1, says "cannot open FILE" and where FILE is (a folder of the repository), and reads nothing
void missing(const rtest::TempDir& d, const std::string& cmd, const std::string& what, const char* file,
             const char* folder)
{
    std::string out;
    int st = run_in(d.path, cmd, out);
    const std::string says = std::string("cannot open ") + file + "\nthe default file " + file + " is in " + folder +
                             " of the repository: run there, or give -f\n";
    check(st == 1 && out.find(says) != std::string::npos && files_read(out).empty(),
          "%s without %s says where the default is: %s (exit %d; output \"%s\")", what.c_str(), file, folder, st,
          st == 1 && out.find(says) != std::string::npos ? "as it should" : rtest::chomp(out).substr(0, 300).c_str());
}

// the source of a program contains the line `text`
bool source_has(const char* rel, const std::string& text)
{
    return rtest::read_file(std::string(ZIPPERS_REPO) + "/" + rel).find(text) != std::string::npos;
}

}  // namespace

int main()
{
    double t_start = rtest::clock_seconds();
    printf("test_default_files: which file the programs and tools read without -f\n");
#if defined(ZIPPERS_ZIPPER3) && defined(ZIPPERS_ZIPPER5) && defined(ZIPPERS_LINK3) && defined(ZIPPERS_ROOFS) && \
    defined(ZIPPERS_CULL) && defined(ZIPPERS_STRAYS) && defined(ZIPPERS_OVERDRAW) && defined(ZIPPERS_ISLANDS5) && \
    defined(ZIPPERS_REPO)
    const std::string z3 = quote(ZIPPERS_ZIPPER3), z5 = quote(ZIPPERS_ZIPPER5), l3 = quote(ZIPPERS_LINK3);
    const std::string roofs = quote(ZIPPERS_ROOFS), cull = quote(ZIPPERS_CULL), strays = quote(ZIPPERS_STRAYS),
                      overdraw = quote(ZIPPERS_OVERDRAW), islands = quote(ZIPPERS_ISLANDS5);
    // the light runs (R = 0, ACC 0.3)
    const std::string z3run = z3 + " -R 0 -a 0.3", z5run = z5 + " -R 0 -a 0.3";
    const std::string z3pairs = z3 + " -R 0 -a 0.1 --pairs " + PAIRS;        // (the pairs are built at the accuracy: a finer one)
    const std::string link3run = l3 + " --rr 0.5 --nrel 0 --hr 0.5 --hinfo";
    // each tool on S^3 (-e cs) and S^4 (-e imm)
    auto tool = [&](const std::string& t, const char* e) -> std::string {
        if (t == roofs) return t + " -e " + e;
        if (t == cull) return t + " -e " + e + " --mode tips";
        if (t == strays) return t + " -e " + e + " --mode picture -R 0 -a 0.3 -k 2";
        return t + " -e " + e + " -R 0 -a 0.3 --nogeo --keyshift 0";            // overdraw
    };
    const std::vector<std::pair<std::string, std::string>> tools = {
        {"roofs", roofs}, {"cull", cull}, {"strays", strays}, {"overdraw", overdraw}};

    section("S^3: zipper3 reads the new default, zipper3 --pairs and link3 the old file, -f wins (a folder with both files)");
    {
        rtest::TempDir d;
        put(d, std::string("data/s3/") + NEW3);
        put(d, std::string("legacy/s3/") + OLD3);
        put(d, std::string("legacy/s3/") + PAIRS);
        reads(d, z3run, "zipper3", NEW3);
        reads(d, z3pairs, "zipper3 --pairs", OLD3);
        reads(d, z3run + " -f " + OLD3, "zipper3 -f the old file", OLD3);
        reads(d, z3pairs + " -f " + NEW3, "zipper3 --pairs -f the new file (-f wins over the --pairs default)", NEW3);
        reads(d, z3run + " -f " + NEW3 + " --only +", "zipper3 -f the new file", NEW3);
        reads(d, link3run, "link3", OLD3);
        for (const auto& t : tools) reads(d, tool(t.second, "cs"), t.first + " -e cs", NEW3);
        for (const auto& t : tools)
            reads(d, tool(t.second, "cs") + " -f " + OLD3, t.first + " -e cs -f the old file", OLD3);
    }

    section("S^4: zipper5 and the tools read the new default, -f wins (a folder with both files)");
    {
        rtest::TempDir d;
        put(d, std::string("data/s4/") + NEW4);
        put(d, std::string("legacy/s4/") + OLD4);
        reads(d, z5run, "zipper5", NEW4);
        reads(d, z5run + " -f " + OLD4, "zipper5 -f the old file", OLD4);
        for (const auto& t : tools) reads(d, tool(t.second, "imm"), t.first + " -e imm", NEW4);
        reads(d, islands + " -R 0 -a 0.3 --graph", "islands5", NEW4);
        for (const auto& t : tools)
            reads(d, tool(t.second, "imm") + " -f " + OLD4, t.first + " -e imm -f the old file", OLD4);
        reads(d, islands + " -R 0 -a 0.3 --graph -f " + OLD4, "islands5 -f the old file", OLD4);
    }

    section("a missing default is not replaced by another file, and the message says where it is");
    {
        rtest::TempDir old;                                   // only the files that were the defaults before
        put(old, std::string("legacy/s3/") + OLD3);
        put(old, std::string("legacy/s4/") + OLD4);
        put(old, std::string("legacy/s3/") + PAIRS);
        missing(old, z3run, "zipper3", NEW3, "data/s3/");
        missing(old, z5run, "zipper5", NEW4, "data/s4/");
        for (const auto& t : tools) {
            missing(old, tool(t.second, "cs"), t.first + " -e cs", NEW3, "data/s3/");
            missing(old, tool(t.second, "imm"), t.first + " -e imm", NEW4, "data/s4/");
        }
        missing(old, islands + " -R 0 -a 0.3 --graph", "islands5", NEW4, "data/s4/");
        reads(old, z3pairs, "zipper3 --pairs (the old file only)", OLD3);
        reads(old, link3run, "link3 (the old file only)", OLD3);
        rtest::TempDir fresh;                                 // only the new files
        put(fresh, std::string("data/s3/") + NEW3);
        put(fresh, std::string("data/s4/") + NEW4);
        put(fresh, std::string("legacy/s3/") + PAIRS);
        missing(fresh, z3pairs, "zipper3 --pairs", OLD3, "legacy/s3/");
        std::string out;
        int st = run_in(fresh.path, link3run, out);
        check(st == 1 && out.find(std::string("cannot open ") + OLD3) != std::string::npos &&
              out.find("the default file") == std::string::npos,
              "link3 without %s fails as it did (exit %d; output \"%s\"): its default has not moved", OLD3, st,
              rtest::chomp(out).substr(0, 200).c_str());
        rtest::TempDir none;                                  // nothing: an explicit -f that is missing is not "the default"
        st = run_in(none.path, z3run + " -f nowhere.txt", out);
        check(st == 1 && out.find("cannot open nowhere.txt") != std::string::npos &&
              out.find("the default file") == std::string::npos,
              "zipper3 -f nowhere.txt: the usual \"cannot open\", and no word about a default (exit %d; output \"%s\")", st,
              rtest::chomp(out).substr(0, 200).c_str());
        st = run_in(none.path, z5run + " -f nowhere.txt", out);
        check(st == 1 && out.find("cannot open nowhere.txt") != std::string::npos &&
              out.find("the default file") == std::string::npos,
              "zipper5 -f nowhere.txt: the usual \"cannot open\", and no word about a default (exit %d; output \"%s\")", st,
              rtest::chomp(out).substr(0, 200).c_str());
        // the tools too: an explicit -f that is missing is the user's, and the message does not speak of a default
        std::vector<std::pair<std::string, std::string>> every;
        for (const auto& t : tools) {
            every.push_back({t.first + " -e cs", tool(t.second, "cs")});
            every.push_back({t.first + " -e imm", tool(t.second, "imm")});
        }
        every.push_back({"islands5", islands + " -R 0 -a 0.3 --graph"});
        for (const auto& t : every) {
            st = run_in(none.path, t.second + " -f nowhere.txt", out);
            check(st == 1 && out.find("cannot open nowhere.txt") != std::string::npos &&
                  out.find("the default file") == std::string::npos,
                  "%s -f nowhere.txt: the usual \"cannot open\", and no word about a default (exit %d; output \"%s\")",
                  t.first.c_str(), st, rtest::chomp(out).substr(0, 200).c_str());
        }
    }

    section("film5 and link3 do not change (film5 needs a window: its source is read)");
    check(source_has("apps/s4/film5.cc", "string subst = \"subst5_2.30_4.50_t1.txt\", landmarks, movie, still;"),
          "film5's default file is still subst5_2.30_4.50_t1.txt (the file of the legacy film's ending)");
    check(source_has("apps/s3/link3.cc", "string subst = \"subst_4.00_9.50_t3.txt\";"),
          "link3's default file is still subst_4.00_9.50_t3.txt (the linked pairs are loops of its arcs)");
    check(source_has("film/make_joined_film.sh", "subst5_2.30_4.50_t1.txt"),
          "film/make_joined_film.sh still names subst5_2.30_4.50_t1.txt");
#else
    note("the programs and tools are not all built in this configuration: the test is left out");
#endif
    rtest::end_section();
    printf("test_default_files: %ld checks, %ld fail (%.1f s, peak %.0f MB)\n", rtest::all_n, rtest::all_bad,
           rtest::clock_seconds() - t_start, rtest::peak_mb());
    return rtest::all_bad ? 1 : 0;
}
