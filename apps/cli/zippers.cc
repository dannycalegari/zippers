// SPDX-License-Identifier: GPL-3.0-or-later
/*  zippers.cc  --  the command line of the suite: one program, zippers, with subcommands.

    usage:  zippers [--dry-run] COMMAND [EXAMPLE] [options]
            zippers --help | --version
            zippers COMMAND --help

      list [EXAMPLE]                        the examples, and where their data files are found
      draw EXAMPLE [options] [-o FILE]      a picture (zipper, zipper3 or zipper5)
      view [EXAMPLE] [options] [-o STEM]    the window (zview)
      search EXAMPLE [C R [TPHI]] [options] the substitution and its certificate (chz or chz5)
      link cartwright-steger [options]      linking numbers of loops in the zippers (link3)
      film SHOT.txt [options]               the frames of a shot of the S^2 film engine (zmovie)
      check [EXAMPLE ...]                   the exact checks of the examples

    THE EXAMPLES. fig8, or any word in R and L with both letters (S^2: hyperbolic 3-orbifolds that fiber over the
    circle; the word is the monodromy, -n N the order of the cone point); cartwright-steger or cs (S^3: the
    Cartwright-Steger surface, phi = 1,0); imm-m5 or imm (S^4: the fibered hyperbolic 5-manifold of
    Italiano-Martelli-Migliorini).

    HOW IT WORKS. zippers reads the command and the example, and checks every option against the table OPTIONS below:
    its name, for the command and the sphere of the example, and the number of values that follow it, none of which may
    look like an option ('-' followed by a letter or by a second '-'; a negative number, and '-' itself, are values). It
    refuses anything else, with exit status 1 and a message (the table LEFT_OUT says why the options of the programs that
    it does not take are left out). The options keep the names and the meanings that they have in the programs, and are
    passed on unchanged and in the same order, but for these translations:
      - the S^2 example is the monodromy: zippers draw RRL -n 3 runs zipper -m RRL -n 3;
      - draw's --format FMT is zipper's -f FMT (in zippers, -f is always a substitution file);
      - search's numbers C R [TPHI] are those of chz kernel, and for imm-m5 RHO, the third number of chz5 kernel, is
        --rho RHO (default 3); without numbers, search runs the search that made the example's default file, with
        its options unless options are given;
      - link adds the settings of the scan that made the stored linked pairs, -a 0.01 --nrel 20 --hr 2.6, for those
        not given, and before a scan makes the folder pictures/ where link3 writes its pictures;
      - --phi 1,0 (cartwright-steger) is checked and not passed on: it is the only phi in this version;
      - draw --session FILE [options] takes the example and its options from a session file (src/output/session.h),
        the options given after it in place of the session's; the substitution file (and the file of --pairs) that the
        data search finds must have the SHA-256 the session gives, or zippers refuses, saying which file it found,
        where, and both hashes;
      - view runs zview, the window's program (apps/gui/), with the example and the options unchanged (fig8 -n 2 -R 6
        without an example), or with --session FILE and the options after it. Its options are draw's, less the outputs
        and those that only print (VIEW_LEFT_OUT), and -o STEM is the stem of the pictures that the window saves. zview
        runs zippers draw itself for the curves it shows and the pictures it saves. A build without GLFW has no window
        (ZIPPERS_HAVE_WINDOW is not defined): zview says so, and so do the usage texts. view's -o STEM and --session
        FILE have help texts of their own (VIEW_HELP).
    Then it runs the program that does the work, replacing itself by it (execv), so that the program's exit status,
    its output and its handling of signals are exactly the program's. check runs its programs as child processes,
    each in a temporary folder of its own and without ZIPPERS_DATA, and removes the folders, also when it is
    interrupted (SIGINT, SIGTERM, SIGHUP, and SIGPIPE when what reads its output stops, unless ignored when it
    started: it then ends with the signal); list runs none (it uses the search for data files itself). With --dry-run, zippers prints the command line of the program
    instead (its path, then its arguments, quoted for a shell where needed), and runs nothing.

    THE PROGRAMS are those of zippers' own build when zippers runs from the folder in which its build puts it (the paths
    CMake gives it, apps/cli/CMakeLists.txt), and otherwise those of ../libexec/zippers/ relative to zippers' own
    folder (an installed zippers). It never looks in PATH. The data files are found by the programs themselves, by the
    search of src/core/data_files.h; zippers list shows where that search finds each default file.

    Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
*/

#include "core/data_files.h"
#include "output/session.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <ftw.h>
#include <poll.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using std::string;
using std::vector;

#ifndef ZIPPERS_VERSION
#define ZIPPERS_VERSION "unknown"
#endif
#ifndef ZIPPERS_INSTALLED_PROGRAMS
#define ZIPPERS_INSTALLED_PROGRAMS ""
#endif

namespace {

// ---- the programs ---------------------------------------------------------------------------------------------------

// the programs that zippers runs, and their paths in zippers' build (compile definitions of apps/cli/CMakeLists.txt)
#ifndef ZIPPERS_PROGRAM_zipper
#define ZIPPERS_PROGRAM_zipper ""
#define ZIPPERS_PROGRAM_zmovie ""
#define ZIPPERS_PROGRAM_chz ""
#define ZIPPERS_PROGRAM_zipper3 ""
#define ZIPPERS_PROGRAM_link3 ""
#define ZIPPERS_PROGRAM_chz5 ""
#define ZIPPERS_PROGRAM_zipper5 ""
#endif
#ifndef ZIPPERS_PROGRAM_zview
#define ZIPPERS_PROGRAM_zview ""
#endif
struct Program { const char* name; const char* built; };
const Program PROGRAMS[] = {
    {"zipper", ZIPPERS_PROGRAM_zipper},   {"zmovie", ZIPPERS_PROGRAM_zmovie}, {"chz", ZIPPERS_PROGRAM_chz},
    {"zipper3", ZIPPERS_PROGRAM_zipper3}, {"link3", ZIPPERS_PROGRAM_link3},   {"chz5", ZIPPERS_PROGRAM_chz5},
    {"zipper5", ZIPPERS_PROGRAM_zipper5}, {"zview", ZIPPERS_PROGRAM_zview},
};

const zippers::DataPlaces PLACES = ZIPPERS_DATA_PLACES;

// the path of the program `name` that zippers runs, and in `looked` where it should be; "" if it is not there
string program_path(const string& name, string& looked)
{
    string path;
    if (zippers::in_build_folder(PLACES)) {
        for (const Program& p : PROGRAMS)
            if (name == p.name) path = p.built;
    } else {
        string self = zippers::own_path();
        if (!self.empty() && *ZIPPERS_INSTALLED_PROGRAMS)
            path = zippers::tidy_path(zippers::folder_of(self) + "/" + ZIPPERS_INSTALLED_PROGRAMS + "/" + name);
    }
    looked = path.empty() ? "(no place is known for it)" : path;
    return !path.empty() && access(path.c_str(), X_OK) == 0 ? path : string();
}

// ---- messages -------------------------------------------------------------------------------------------------------

[[noreturn]] void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void fail(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "zippers: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// an argument as a shell would need it written
string quoted(const string& s)
{
    if (!s.empty() && s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_@%+=:,./-") ==
                          string::npos)
        return s;
    string q = "'";
    for (char c : s) q += c == '\'' ? string("'\\''") : string(1, c);
    return q + "'";
}

bool ends_with_ci(const string& s, const string& suffix)
{
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); i++)
        if (tolower((unsigned char)s[s.size() - suffix.size() + i]) != tolower((unsigned char)suffix[i])) return false;
    return true;
}

// ---- the examples ---------------------------------------------------------------------------------------------------

enum Sphere { S2 = 1, S3 = 2, S4 = 4 };
const char* sphere_name(int s) { return s == S2 ? "S2" : s == S3 ? "S3" : "S4"; }

// a word in R and L with both letters (lower case too), as zipper's -m takes it
bool is_rl_word(const string& w)
{
    bool r = false, l = false;
    for (char c : w) {
        if (c == 'R' || c == 'r') r = true;
        else if (c == 'L' || c == 'l') l = true;
        else return false;
    }
    return r && l;
}

struct Example {
    string name;             // as the user wrote it (for S^2, the monodromy that zipper's -m gets)
    int sphere = 0;
    const char* canonical = "";   // cartwright-steger, imm-m5, or the word
};

// the example of a name; false if there is none
bool find_example(const string& name, Example& e)
{
    e.name = name;
    if (name == "fig8" || is_rl_word(name)) {
        e.sphere = S2;
        e.canonical = name == "fig8" ? "fig8" : "";
        return true;
    }
    if (name == "cartwright-steger" || name == "cs") {
        e.sphere = S3;
        e.canonical = "cartwright-steger";
        return true;
    }
    if (name == "imm-m5" || name == "imm") {
        e.sphere = S4;
        e.canonical = "imm-m5";
        return true;
    }
    return false;
}

string example_title(const Example& e)
{
    return e.sphere == S2 ? e.name + " (S2)" : string(e.canonical) + " (" + sphere_name(e.sphere) + ")";
}

// ---- the options ----------------------------------------------------------------------------------------------------

// An option that zippers takes: for the command `command` and the spheres `spheres`, `values` values follow it, and it
// is passed on to the program as `program` with the same values. A program option "" means that zippers uses the
// option itself (--phi, --rho, and --format, which becomes -f). `help` is its line in the usage text; null: hidden.
struct Option {
    const char* command;
    int spheres;
    const char* name;
    int values;
    const char* program;
    const char* args;
    const char* help;
};

const int ALL = S2 | S3 | S4, S34 = S3 | S4;

const Option OPTIONS[] = {
    // draw: zipper (S2), zipper3 (S3), zipper5 (S4)
    {"draw", ALL, "--session", 1, "", "FILE", "take the example and its options from a session file: zippers draw --session FILE [options] [-o FILE] (first, in place of the example)"},
    {"draw", ALL, "-o", 1, "-o", "FILE", "write the picture (PNG; S2 also EPS, its default without -o, on the standard output)"},
    {"draw", ALL, "-R", 1, "-R", "R", "the radius of the ball of the group whose translates are drawn (default S2 6; S3, S4 0)"},
    {"draw", ALL, "-a", 1, "-a", "ACC", "the accuracy, the largest chord on the sphere (default S2 0.004, S3 0.01, S4 0.02)"},
    {"draw", ALL, "-s", 1, "-s", "SIZE", "the size, in pixels or in EPS points (default S2 512, S3 900, S4 840)"},
    {"draw", ALL, "--zoom", 1, "--zoom", "F", "magnify about the centre (default 1)"},
    {"draw", ALL, "--yaw", 1, "--yaw", "D", "turn the view about the vertical axis, in degrees (default 0)"},
    {"draw", ALL, "--pitch", 1, "--pitch", "D", "turn it about the horizontal axis (default S2 0, S3 20, S4 15)"},
    {"draw", ALL, "--stats", 0, "--stats", "", "the checks (S2: the counts and the exactness checks; S3, S4: the root balls)"},
    {"draw", ALL, "--overlap", 1, "--overlap", "L", "mark the sub-branches of each leaf down to L levels as drawn (default S2 1; S3, S4 0)"},
    {"draw", ALL, "--nocull", 0, "--nocull", "", "with --zoom: no culling by default"},
    {"draw", ALL, "--nofocus", 0, "--nofocus", "", "with --zoom: no focus ball by default"},
    {"draw", ALL, "--polylines", 1, "--polylines", "FILE", "write the curves as data (a polyline file); S2 without -o: no picture"},
    {"draw", ALL, "--colour", 1, "--colour", "L=R,G,B", "the colour of L: Z+, Z- the zippers; S2: wheel; S3: pair.Z+, pair.Z- a pair's loops, context.Z+, context.Z- the zippers behind it; given twice, the last counts"},
    {"draw", S2, "-n", 1, "-n", "N", "the order of the cone point, 2..1000000, or inf for the cusp (default 2)"},
    {"draw", S2, "--format", 1, "-f", "FMT", "eps or png (default: png if -o ends in .png, else eps)"},
    {"draw", S2, "-w", 1, "-w", "WIDTH", "the line width in points, 0 = thinnest (default 0)"},
    {"draw", S2, "-C", 0, "-C", "", "also draw the Catherine wheel"},
    {"draw", S2, "--wheel", 0, "--wheel", "", "the same as -C"},
    {"draw", S2, "--wheelres", 1, "--wheelres", "N", "the finest edge pi/N of the wheel's mesh (default 10000000)"},
    {"draw", S2, "--timing", 0, "--timing", "", "the time of each phase"},
    {"draw", S2, "--memlimit", 1, "--memlimit", "GB", nullptr},
    {"draw", S2, "--roll", 1, "--roll", "D", nullptr},
    {"draw", S2, "--nochains", 0, "--nochains", "", nullptr},
    {"draw", S34, "-f", 1, "-f", "FILE", "the substitution file (default: S3 subst_4.00_9.50_t3_r1.txt, with --pairs subst_4.00_9.50_t3.txt; S4 subst5_2.30_4.50_t1_r0.txt)"},
    {"draw", S34, "--only", 1, "--only", "+|-", "draw only Z+ or only Z-"},
    {"draw", S34, "--clip", 1, "--clip", "RHO", "clip the picture to the ball of radius RHO (default 3)"},
    {"draw", S34, "--thick", 1, "--thick", "N", "thicker lines"},
    {"draw", S34, "--cap", 1, "--cap", "N", "stop after N leaves per zipper (default S3 8e6, S4 2e7)"},
    {"draw", S34, "--centre", 1, "--centre", "P", "centre the view at the point P (S3: x1,...,x4; S4: x1,...,x5, or a null vector x1,...,x6)"},
    {"draw", S34, "--cull", 0, "--cull", "", "draw only the branches that may meet the window (on by default for a still with --zoom above 1)"},
    {"draw", S34, "--cullk", 1, "--cullk", "K", "the margin factor of culling (default 16); implies --cull"},
    {"draw", S34, "--zoomacc", 0, "--zoomacc", "", "with culling: the accuracy ACC/zoom on the sphere"},
    {"draw", S34, "--focus", 1, "--focus", "M", "with --zoom: the focus ball, at M log(zoom) towards the centre (M = 1 by default for a still)"},
    {"draw", S34, "--viewmatrix", 1, "--viewmatrix", "N1,...", "the view as its matrices (S3: 25 numbers, S4: 34), orthonormal (S3: of determinant +1), in place of --yaw, --pitch, --pole, --centre"},
    {"draw", S3, "--pole", 2, "--pole", "D1 D2", "move the pole of the projection (degrees)"},
    {"draw", S3, "--check", 0, "--check", "", "the smallest distance between the drawn Z+ and Z-"},
    {"draw", S3, "--pairs", 1, "--pairs", "FILE", "show the linked pairs of FILE (linked_pairs.txt) instead of the zippers"},
    {"draw", S3, "--pairindex", 1, "--pairindex", "K", "with --pairs and -o: draw pair number K (default 0)"},
    {"draw", S3, "--context", 0, "--context", "", "with --pairs: the base arcs faintly behind the pair"},
    {"draw", S3, "--phi", 1, "", "1,0", "phi (the only one in this version)"},
    {"draw", S4, "--turn", 1, "--turn", "D", "turn the hidden direction, in the plane of the third visible axis (degrees)"},
    {"draw", S4, "--turn2", 1, "--turn2", "D", "the same, in the plane of the first visible axis"},
    {"draw", S4, "--hidfog", 1, "--hidfog", "H", "the fog of the hidden coordinate (default 1.5)"},
    // search: chz kernel (S3), chz5 kernel (S4)
    {"search", S34, "--tlist", 0, "--tlist", "", "list the candidates for t, and stop"},
    {"search", S34, "--tcand", 1, "--tcand", "I", "take candidate I for t"},
    {"search", S34, "--noinv", 0, "--noinv", "", "S_c without its involutions"},
    {"search", S34, "--roofs", 0, "--roofs", "", "choose the roofs (self-inverse involution roofs, then fewer cancelling junctions)"},
    {"search", S34, "--excess", 1, "--excess", "E", "with --roofs: the excess of a self-inverse roof (default 2)"},
    {"search", S34, "--jexcess", 1, "--jexcess", "e", "with --roofs: the excess of the junction pass (default 0)"},
    {"search", S4, "--rho", 1, "", "RHO", "the ball B(max(RHO, C)) of the search (default 3)"},
    {"search", S3, "--phi", 1, "", "1,0", "phi (the only one in this version)"},
    // link: link3 (S3)
    {"link", S3, "-f", 1, "-f", "FILE", "the substitution file (default subst_4.00_9.50_t3.txt)"},
    {"link", S3, "-a", 1, "-a", "ACC", "the accuracy of the loops (zippers' default 0.01)"},
    {"link", S3, "--rr", 1, "--rr", "R", "find the relations inside the ball B(R) (default 7.5)"},
    {"link", S3, "--maxlen", 1, "--maxlen", "N", "the longest relation (default 6)"},
    {"link", S3, "--nrel", 1, "--nrel", "N", "use the first N relations (zippers' default 20)"},
    {"link", S3, "--hr", 1, "--hr", "R", "translate the Z- loops by the elements of B(R) (zippers' default 2.6)"},
    {"link", S3, "--stats", 0, "--stats", "", "check the translates (closure, descent; exact)"},
    {"link", S3, "--seed", 1, "--seed", "N", "the random seed of the projections (default 1)"},
    {"link", S3, "--pair", 3, "--pair", "A B H", "the single pair (gamma_wA, h_H delta_wB), in --nproj projections"},
    {"link", S3, "--nproj", 1, "--nproj", "N", "the projections of --pair (default 20)"},
    {"link", S3, "--png", 1, "--png", "STEM", "with --pair: three pictures STEM_yaw000.png, ..."},
    {"link", S3, "--hinfo", 0, "--hinfo", "", "list the translates h, their phi and their words"},
    {"link", S3, "--export", 1, "--export", "FILE", "write the verified pairs of a scan's output FILE to linked_pairs.txt"},
    {"link", S3, "--phi", 1, "", "1,0", "phi (the only one in this version)"},
    // film: zmovie (S2 shots)
    {"film", S2, "-o", 1, "-o", "DIR", "the folder of the frames (else the shot's 'out')"},
    {"film", S2, "-j", 1, "-j", "N", "render with N processes"},
    {"film", S2, "--frames", 1, "--frames", "A:B", "render frames A to B only"},
    {"film", S2, "--quiet", 0, "--quiet", "", "print less"},
    {"film", S2, "--timing", 0, "--timing", "", nullptr},
    {"film", S2, "--assemble", 2, "--assemble", "TIMELINE.txt OUTDIR", nullptr},   // first, instead of a shot
};

// An option (or a mode) of a program that zippers does not take, and why: the message when it is given, for the
// command and the spheres named, and the record that every option of every program is accounted for (zippers --table).
struct LeftOut {
    const char* program;
    const char* name;
    const char* command;
    int spheres;
    const char* reason;
};

const LeftOut LEFT_OUT[] = {
    {"zipper", "-m", "draw", S2, "the example is the monodromy: zippers draw WORD (fig8, RRL, ...)"},
    {"zipper", "-f", "draw", S2, "an S2 example has no substitution file (its arcs come from the monodromy); the format of the picture is --format"},
    {"zipper", "-v", "draw", S2, "the window is another command: zippers view"},
    {"zipper", "--view", "draw", S2, "the window is another command: zippers view"},
    {"zipper", "-e", "draw", S2, "it is obsolete, and zipper ignores it"},
    {"zipper", "-d", "draw", S2, "it is another name of -R"},
    {"zipper", "--nest", "draw", S2, "it is another name of --overlap"},
    {"zipper", "-h", "draw", S2, "zippers has its own help: zippers draw EXAMPLE --help"},
    {"zipper", "--help", "draw", S2, "zippers has its own help: zippers draw EXAMPLE --help"},
    {"zmovie", "-h", "film", S2, "zippers has its own help: zippers film --help"},
    {"zmovie", "--help", "film", S2, "zippers has its own help: zippers film --help"},
    {"chz", "kernel", "search", S3, "zippers search runs it"},
    {"chz", "selftest", "check", S3, "zippers check runs it"},
    {"chz", "ball", "", S3, "the counts of a ball: run chz ball itself"},
    {"chz", "geo", "", S3, "a word of chz ball"},
    {"chz5", "kernel", "search", S4, "zippers search runs it"},
    {"chz5", "selftest", "check", S4, "zippers check runs it"},
    {"chz5", "ball", "", S4, "the counts of a ball: run chz5 ball itself"},
    {"zipper3", "-v", "draw", S3, "the window is another command: zippers view"},
    {"zipper3", "--snap", "draw", S3, "it is an option of zipper3's own window; the window is zippers view, whose p saves a picture"},
    {"zipper3", "--movie", "draw", S3, "the films of zipper3 play in its own window, and zippers view plays none"},
    {"zipper3", "--moviescript", "draw", S3, "the films of zipper3 play in its own window, and zippers view plays none"},
    {"zipper3", "--moviestart", "draw", S3, "the films of zipper3 play in its own window, and zippers view plays none"},
    {"zipper3", "--moviescale", "draw", S3, "the films of zipper3 play in its own window, and zippers view plays none"},
    {"link3", "selftest", "check", S3, "zippers check runs it"},
    {"zipper5", "-h", "draw", S4, "zippers has its own help: zippers draw imm-m5 --help"},
    {"zipper5", "--help", "draw", S4, "zippers has its own help: zippers draw imm-m5 --help"},
};

// the programs that zippers does not run, and why (zippers --table)
const char* const NOT_RUN[][2] = {
    {"ztest", "a measurement of the film engine against zipper, with no verdict"},
    {"film5", "the film's ending on S4: a window, and the landmarks of a recording"},
};

// the program that a command runs for a sphere
const char* program_of(const string& command, int sphere)
{
    if (command == "draw") return sphere == S2 ? "zipper" : sphere == S3 ? "zipper3" : "zipper5";
    if (command == "search") return sphere == S3 ? "chz" : sphere == S4 ? "chz5" : "";
    if (command == "link") return sphere == S3 ? "link3" : "";
    if (command == "film") return "zmovie";
    return "";
}

// the options of draw that view does not take, and why: the outputs (the window saves its own pictures, with p) and
// those that only print
const char* const VIEW_LEFT_OUT[][2] = {
    {"--stats", "it only prints (zippers draw EXAMPLE --stats)"},
    {"--check", "it only prints (zippers draw cartwright-steger --check)"},
    {"--timing", "it only prints (zippers draw EXAMPLE --timing)"},
    {"--polylines", "it is an output of draw; the window saves its pictures with p"},
    {"--format", "it is an output of draw; the window saves its pictures with p, as PNG"},
};

// the options of draw whose help in view says what view takes: {name, values, help}
const char* const VIEW_HELP[][3] = {
    {"-o", "STEM", "the stem of the pictures that p saves, STEM.png and STEM.zsession (by default the example's name)"},
    {"--session", "FILE", "take the example and its options from a session file: zippers view --session FILE [options] [-o STEM] (first, in place of the example)"},
};

const char* view_left_out(const string& name)
{
    for (const auto& v : VIEW_LEFT_OUT)
        if (name == v[0]) return v[1];
    return nullptr;
}

// the options of a command: view takes those of draw, less VIEW_LEFT_OUT
const char* options_of(const string& command) { return command == "view" ? "draw" : command.c_str(); }

const Option* find_option(const string& command, int sphere, const string& name)
{
    if (command == "view" && view_left_out(name)) return nullptr;
    for (const Option& o : OPTIONS)
        if (!strcmp(options_of(command), o.command) && (o.spheres & sphere) && name == o.name) return &o;
    return nullptr;
}

// the examples (by sphere) for which `name` is an option of `command`: "" if none
string spheres_with(const string& command, const string& name)
{
    int s = 0;
    if (command == "view" && view_left_out(name)) return "";
    for (const Option& o : OPTIONS)
        if (!strcmp(options_of(command), o.command) && name == o.name) s |= o.spheres;
    string r;
    if (s & S2) r += "fig8 (S2)";
    if (s & S3) r += string(r.empty() ? "" : ", ") + "cartwright-steger (S3)";
    if (s & S4) r += string(r.empty() ? "" : ", ") + "imm-m5 (S4)";
    return r;
}

// the options given after the example, read against the table: refused with a message if anything does not fit
struct Given {
    const Option* option;
    vector<string> values;
};
struct Parsed {
    vector<Given> options;
    vector<string> words;    // the arguments that are not options (search's numbers)
};

// whether a word given as an option's value looks like an option: '-' followed by a letter, or by a second '-'. A
// negative number (-1, -0.5, -.5) and the lone '-' (--only -) are values.
bool option_like(const string& v)
{
    return v.size() >= 2 && v[0] == '-' && (isalpha((unsigned char)v[1]) || v[1] == '-');
}

// the values of the option `name` (args[first], ..., n of them): refused if one of them looks like an option
void check_values(const string& command, const char* name, int n, const char* placeholders, const vector<string>& args,
                  size_t first)
{
    for (size_t k = first; k < first + (size_t)n && k < args.size(); k++)
        if (option_like(args[k]))
            fail("%s: %s takes %d value%s (%s %s), but '%s' after it looks like an option (a value may begin with '-' "
                 "only as a number, or be '-' itself)", command.c_str(), name, n, n == 1 ? "" : "s", name, placeholders,
                 args[k].c_str());
}

Parsed parse(const string& command, const Example& e, const vector<string>& args, bool words_allowed)
{
    Parsed p;
    for (size_t i = 0; i < args.size(); i++) {
        const string& a = args[i];
        if (a.empty() || a[0] != '-' || a == "-") {
            if (!words_allowed)
                fail("%s %s: unexpected argument '%s' (zippers %s --help)", command.c_str(), e.name.c_str(), a.c_str(),
                     command.c_str());
            p.words.push_back(a);
            continue;
        }
        const Option* o = find_option(command, e.sphere, a);
        if (!o) {
            if (command == "view" && view_left_out(a))
                fail("%s %s: %s is not an option of zippers view: %s", command.c_str(), e.name.c_str(), a.c_str(),
                     view_left_out(a));
            for (const LeftOut& l : LEFT_OUT)
                if (!strcmp(options_of(command), l.command) && (l.spheres & e.sphere) && a == l.name)
                    fail("%s %s: %s is not an option of zippers: %s", command.c_str(), e.name.c_str(), a.c_str(), l.reason);
            string other = spheres_with(command, a);
            if (!other.empty())
                fail("%s: %s is not an option for %s; it is one for %s", command.c_str(), a.c_str(),
                     example_title(e).c_str(), other.c_str());
            fail("%s %s: unknown option %s (zippers %s %s --help)", command.c_str(), e.name.c_str(), a.c_str(),
                 command.c_str(), e.name.c_str());
        }
        check_values(command, o->name, o->values, o->args, args, i + 1);
        if (i + (size_t)o->values >= args.size())
            fail("%s: %s needs %d value%s (%s %s)", command.c_str(), a.c_str(), o->values, o->values == 1 ? "" : "s",
                 a.c_str(), o->args);
        Given g{o, {}};
        for (int k = 0; k < o->values; k++) g.values.push_back(args[++i]);
        p.options.push_back(g);
    }
    return p;
}

bool has(const Parsed& p, const char* name)
{
    for (const Given& g : p.options)
        if (!strcmp(g.option->name, name)) return true;
    return false;
}

// --phi: only 1,0 in this version
void check_phi(const Parsed& p, const string& command)
{
    for (const Given& g : p.options)
        if (!strcmp(g.option->name, "--phi")) {
            const string& v = g.values[0];
            int a = 0, b = 0;
            char tail = 0;
            if (sscanf(v.c_str(), "%d,%d%c", &a, &b, &tail) != 2 || a != 1 || b != 0)
                fail("%s: --phi %s: only phi = 1,0 (the first coordinate of H_1 = Z^2) is available in this version",
                     command.c_str(), v.c_str());
        }
}

// the options as the program takes them, in order (those that zippers uses itself are left out)
void pass_on(const Parsed& p, vector<string>& out)
{
    for (const Given& g : p.options) {
        if (!*g.option->program) continue;
        out.push_back(g.option->program);
        for (const string& v : g.values) out.push_back(v);
    }
}

// ---- running a program ----------------------------------------------------------------------------------------------

[[noreturn]] void run(const string& program, const vector<string>& args, bool dry)
{
    string looked, path = program_path(program, looked);
    if (path.empty() && program == "zview")
        fail("view: this installation has no window: it has no zview (looked for %s)", looked.c_str());
    if (path.empty()) fail("cannot find the program %s (looked for %s)", program.c_str(), looked.c_str());
    if (dry) {
        string line = quoted(path);
        for (const string& a : args) line += " " + quoted(a);
        printf("%s\n", line.c_str());
        exit(0);
    }
    vector<char*> argv;
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    fflush(stdout);
    fflush(stderr);
    execv(path.c_str(), argv.data());
    fail("cannot run %s: %s", path.c_str(), strerror(errno));
}

// ---- the usage texts ------------------------------------------------------------------------------------------------

#ifdef ZIPPERS_HAVE_WINDOW
#define NO_WINDOW ""
#else
#define NO_WINDOW " (this build has no window)"
#endif

void usage_top(FILE* f)
{
    fprintf(f,
"usage: zippers [--dry-run] COMMAND [EXAMPLE] [options]\n"
"  list [EXAMPLE]                         the examples, and where their data files are found\n"
"  draw EXAMPLE [options] [-o FILE]       a picture: PNG (S2 examples also EPS)\n"
"  view [EXAMPLE] [options] [-o STEM]     the window on the curves of draw" NO_WINDOW "\n"
"  search EXAMPLE [C R [TPHI]] [options]  the substitution and the certificate that S_c generates the kernel\n"
"  link cartwright-steger [options]       linking numbers of loops in the zippers\n"
"  film SHOT.txt [options]                the frames of a shot of the S2 film engine\n"
"  check [EXAMPLE ...]                    the exact checks of the examples (about half a minute)\n"
"\n"
"  zippers COMMAND --help                 the options of a command (zippers draw EXAMPLE --help: for that example)\n"
"  zippers --version                      the version\n"
"  --dry-run                              print the program command that zippers would run, and run nothing\n"
"\n"
"examples: fig8, or any word in R and L such as RRL (S2, with -n N: the order of the cone point);\n"
"          cartwright-steger or cs (S3); imm-m5 or imm (S4); zippers list says more\n");
}

void usage_options(FILE* f, const char* command, int spheres)
{
    for (const Option& o : OPTIONS) {
        if (strcmp(options_of(command), o.command) || !(o.spheres & spheres) || !o.help) continue;
        if (!strcmp(command, "view") && view_left_out(o.name)) continue;
        const char* args = o.args;
        const char* help = o.help;
        if (!strcmp(command, "view"))
            for (const auto& v : VIEW_HELP)
                if (!strcmp(o.name, v[0])) {
                    args = v[1];
                    help = v[2];
                }
        string left = string("  ") + o.name + (*args ? string(" ") + args : string());
        string tag;
        if (spheres != o.spheres && strcmp(command, "film")) {
            for (int s : {S2, S3, S4})
                if (o.spheres & s) tag += string(tag.empty() ? "" : ",") + sphere_name(s);
            tag = " [" + tag + "]";
        }
        fprintf(f, "%-22s %s%s\n", left.c_str(), help, (spheres == ALL || spheres == S34) ? tag.c_str() : "");
    }
}

void usage_command(FILE* f, const string& command, int sphere)
{
    if (command == "list") {
        fprintf(f,
"usage: zippers list [EXAMPLE]\n"
"  the examples; with EXAMPLE, its programs, its default files and where they are found, and its other data files.\n"
"  A data file is looked for in the working folder, then in the folders of ZIPPERS_DATA (folder/s3/ or folder/s4/,\n"
"  then folder/), then in the data of the suite (data/ of the source tree for a build, share/zippers/ installed).\n");
    } else if (command == "draw") {
        fprintf(f,
"usage: zippers draw EXAMPLE [options] [-o FILE]\n"
"       zippers draw --session SESSION [options] [-o FILE]\n"
"  runs zipper -m EXAMPLE (S2), zipper3 (cartwright-steger) or zipper5 (imm-m5) with the options, which have the\n"
"  programs' names, meanings and defaults; in zippers -f is always a substitution file (S2's format is --format).\n"
"  Without -o an S2 picture is EPS on the standard output (refused when that is a terminal), and S3 and S4 draw no\n"
"  picture, but build the zippers and print what they print (with --check, --stats). --polylines FILE writes the\n"
"  curves as data, a polyline file (with -o as well as the picture; S2 without -o writes only the file).\n"
"  The colours, --colour L=R,G,B (one option for each L), default to the programs' own: L is Z+ or Z- for the\n"
"  zippers; in S2 wheel for the wheel; in S3 pair.Z+ and pair.Z- for the loops of a pair (--pairs), and context.Z+\n"
"  and context.Z- for the zippers drawn faintly behind it (--context). A name that the picture does not draw\n"
"  changes nothing, and when one is given twice, the last one counts.\n"
"  With --session, the example and every option of the picture come from a session file (KEY = VALUE lines, as a\n"
"  window writes beside each picture it saves); options given after it replace the session's. The substitution\n"
"  file, and the file of --pairs, are found as always, and must have the SHA-256 that the session gives.\n");
        usage_options(f, "draw", sphere ? sphere : ALL);
    } else if (command == "view") {
        fprintf(f,
"usage: zippers view [EXAMPLE] [options] [-o STEM]\n"
"       zippers view --session SESSION [options] [-o STEM]\n"
"  runs zview, the window on the curves of zippers draw EXAMPLE [options] (by default fig8 -n 2 -R 6), or of a\n"
"  session file, with the options given after it in place of the session's. The window runs zippers draw for the\n"
"  curves it shows and for the pictures it saves (p: NAME.png and NAME.zsession, never overwriting a file, and the\n"
"  commands that remake them printed). The options are those of draw, less its outputs and those that only print\n"
"  (--polylines, --format, --stats, --check, --timing); -o STEM is the stem of the pictures (by default the\n"
"  example's name)."
#ifndef ZIPPERS_HAVE_WINDOW
" This build has no window: zview says so."
#endif
"\n");
        usage_options(f, "view", sphere ? sphere : ALL);
    } else if (command == "search") {
        fprintf(f,
"usage: zippers search EXAMPLE [C R [TPHI]] [options]\n"
"  runs chz kernel C R [TPHI] (cartwright-steger) or chz5 kernel C R RHO [TPHI] (imm-m5): the search of the\n"
"  substitution with step radius C inside the ball B(R), for t with phi(t) = TPHI (default 1), and the certificate\n"
"  that S_c generates the kernel. It writes its substitution file into the working folder, replacing a file of the\n"
"  same name; zippers draw run there then reads it. The words of the roofs can depend on the platform.\n"
"  Without numbers it runs the search that made the example's default file, with its options unless options are\n"
"  given: for cartwright-steger chz kernel 4 9.5 3 --roofs --jexcess 1, for imm-m5 chz5 kernel 2.3 4.5 3 --roofs.\n");
        usage_options(f, "search", sphere ? sphere : S34);
    } else if (command == "link") {
        fprintf(f,
"usage: zippers link cartwright-steger [options]\n"
"  runs link3: by default the scan of the linking numbers of the loops of relations in Z+ and Z-, with the\n"
"  settings of the scan that made the stored linked pairs unless they are given (-a 0.01 --nrel 20 --hr 2.6; about\n"
"  8 minutes and 0.5 GB on an Apple M2). It writes its pictures into the folder pictures/, which zippers makes if it\n"
"  is missing.\n");
        usage_options(f, "link", S3);
    } else if (command == "film") {
        fprintf(f,
"usage: zippers film SHOT.txt [key=value ...] [options]\n"
"       zippers film --assemble TIMELINE.txt OUTDIR\n"
"  runs zmovie, the film engine of S2, with the arguments unchanged: it renders the frames of a shot file (keys such\n"
"  as monodromy = fig8, order = 2), or assembles frame folders. key=value overrides a key of the shot.\n");
        usage_options(f, "film", S2);
    } else if (command == "check") {
        fprintf(f,
"usage: zippers check [EXAMPLE ...]\n"
"  the exact checks of the examples (all by default), each with a verdict that holds on every machine: the\n"
"  exactness checks of an S2 drawing; the selftests of chz, link3 and chz5; the certificates that S_c generates\n"
"  the kernel (chz kernel 4 9.5 3, chz5 kernel 2.3 4.5 3); the checks of the root balls (zipper3 --stats and\n"
"  zipper5 --stats); and the linking numbers of the 13 stored linked pairs. Each runs in a temporary folder of its\n"
"  own (under TMPDIR), without ZIPPERS_DATA in its environment, so that it reads the suite's own data. Interrupted\n"
"  (Ctrl-C, SIGTERM, SIGHUP, or SIGPIPE when what reads its output stops, as | head), zippers check stops the running\n"
"  check, removes the folder, and ends with the signal; a signal ignored when it started (SIGHUP under nohup) stays\n"
"  ignored, by it and by its checks.\n"
"  The comparison of pictures with recorded ones is a test of the source tree (make test), not of zippers check.\n");
    }
}

// ---- list -----------------------------------------------------------------------------------------------------------

// the search for the data file `name`: "found in FOLDER", "CANNOT BE USED: WHY", or "NOT FOUND" (and where it looked)
string where(const string& name, const char* sphere, vector<string>* looked = nullptr)
{
    zippers::DataFile r = zippers::find_data_file(name, sphere, PLACES);
    if (!r.problem.empty()) return "CANNOT BE USED: " + r.problem;
    if (r.path.empty()) {
        if (looked) *looked = r.looked;
        return "NOT FOUND";
    }
    if (r.path == name) return "found in the working folder";
    return "found in " + zippers::folder_of(r.path);
}

void list_files(const char* sphere, const vector<string>& defaults)
{
    string what, dir = zippers::program_data(PLACES, &what);
    if (dir.empty()) return;
    string folder = dir + "/" + sphere;
    DIR* d = opendir(folder.c_str());
    if (!d) return;
    vector<string> names;
    while (dirent* x = readdir(d)) {
        string n = x->d_name;
        bool def = false;
        for (const string& s : defaults) def = def || s == n;
        if (!def && n.size() > 4 && n.compare(n.size() - 4, 4, ".txt") == 0) names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    if (names.empty()) return;
    string line;
    for (const string& n : names) line += (line.empty() ? "" : ", ") + n;
    printf("  other files: %s (%s, %s)\n", line.c_str(), what.c_str(), folder.c_str());
}

void list_searched()
{
    string what, dir = zippers::program_data(PLACES, &what);
    vector<string> env = zippers::data_env_folders();
    string e = zippers::data_env_state();
    for (const string& x : env) e += (e.empty() ? "" : ":") + x;
    printf("  the data files are looked for in: the working folder; ZIPPERS_DATA (%s); %s\n", e.c_str(),
           dir.empty() ? "(no data folder is known)" : (what + ", " + dir).c_str());
}

void list_entry(const char* program, const char* label, const string& file, const char* sphere)
{
    vector<string> looked;
    string w = where(file, sphere, &looked);
    printf("  %-14s %-8s %-28s %s\n", label, program, file.c_str(), w.c_str());
    for (const string& l : looked) printf("                 (looked in %s)\n", l.c_str());
}

int list(const vector<string>& args)
{
    if (args.size() > 1) fail("list: one example at most (zippers list --help)");
    if (args.empty()) {
        printf(
"S2: hyperbolic 3-orbifolds that fiber over the circle, the fiber a torus with one cone point (or one puncture)\n"
"  fig8                the figure-eight monodromy RL, with its cusped character built in\n"
"  WORD                any word in R and L with both letters, such as RRL or RLLRL\n"
"                      -n N: the order of the cone point, 2..1000000, or inf (the cusp); default 2\n"
"                      commands: draw, check (and film, whose shot files name the monodromy and the order)\n"
"S3: the Cartwright-Steger surface\n"
"  cartwright-steger   (also cs)  --phi 1,0: the first coordinate of H_1 = Z^2 (the only phi in this version)\n"
"                      commands: draw, link, search, check\n"
"S4: the fibered hyperbolic 5-manifold of Italiano-Martelli-Migliorini\n"
"  imm-m5              (also imm)  phi = h/2\n"
"                      commands: draw, search, check\n"
"zippers list EXAMPLE: its data files, and where they are found\n");
        return 0;
    }
    Example e;
    if (!find_example(args[0], e)) fail("list: unknown example '%s' (zippers list)", args[0].c_str());
    if (e.sphere == S2) {
        printf("%s: the monodromy %s; its arcs, the group and its character are computed, with no data file\n",
               example_title(e).c_str(), e.name.c_str());
        printf("  draw            zipper   -m %s\n  check           zipper   -m %s -n 2 -R 5 --stats\n", e.name.c_str(),
               e.name.c_str());
        return 0;
    }
    if (e.sphere == S3) {
        printf("cartwright-steger (S3): the Cartwright-Steger surface, phi = 1,0\n");
        list_entry("zipper3", "draw", "subst_4.00_9.50_t3_r1.txt", "s3");
        list_entry("zipper3", "draw --pairs", "subst_4.00_9.50_t3.txt", "s3");
        list_entry("", "", "linked_pairs.txt", "s3");
        list_entry("link3", "link", "subst_4.00_9.50_t3.txt", "s3");
        printf("  %-14s %-8s %s\n", "search", "chz", "without numbers: chz kernel 4 9.5 3 --roofs --jexcess 1");
        list_files("s3", {"subst_4.00_9.50_t3_r1.txt", "subst_4.00_9.50_t3.txt", "linked_pairs.txt"});
    } else {
        printf("imm-m5 (S4): the fibered hyperbolic 5-manifold of Italiano-Martelli-Migliorini, phi = h/2\n");
        list_entry("zipper5", "draw", "subst5_2.30_4.50_t1_r0.txt", "s4");
        printf("  %-14s %-8s %s\n", "search", "chz5", "without numbers: chz5 kernel 2.3 4.5 3 --roofs");
        list_files("s4", {"subst5_2.30_4.50_t1_r0.txt"});
    }
    list_searched();
    return 0;
}

// ---- check ----------------------------------------------------------------------------------------------------------

struct Check {
    int sphere;
    string what;
    const char* program;
    vector<string> args;
    vector<string> want;       // every one of these must be in the output
    vector<int> times;         // ... this many times (0: at least once)
    bool status_only;          // the verdict is the exit status alone
};

vector<Check> checks_of(const Example& e)
{
    vector<Check> v;
    if (e.sphere == S2) {
        v.push_back({S2, "the exactness checks of a drawing (zipper --stats)", "zipper",
                     {"-m", e.name, "-n", "2", "-R", "5", "--stats", "--memlimit", "2", "-o", "stats.eps"}, {}, {}, true});
        return v;
    }
    if (e.sphere == S3) {
        v.push_back({S3, "the arithmetic of Z[zeta_12] and the lattice (chz selftest)", "chz", {"selftest"},
                     {"  u' unitary for F': yes", "  v' unitary for F': yes", "  b' unitary for F': yes",
                      "  j unitary for F': yes", "  u u^-1 = 1: yes", "  u^3              = zeta^0  I",
                      "  v^4              = zeta^0  I", "  b^3              = zeta^0  I",
                      "  (uv)^2 (vu)^-2   = zeta^0  I", "  v b v^-1 b^-1    = zeta^0  I",
                      "  (buv)^3          = zeta^11  I", "  (buvu)^2 v       = zeta^11  I",
                      "  (uv)^2 j^-1      = zeta^0  I", "  a1 unitary for F': yes", "  a2 unitary for F': yes",
                      "  a3 unitary for F': yes", "  j^4 a1 j^-4 = a3 a2^-3 a3^3 a1 : yes",
                      "  j^4 a2 j^-4 = a3^-1             : yes", "  j^4 a3 j^-4 = (CKY's word)      : yes"},
                     {}, false});
        v.push_back({S3, "the linking-number code on four known links (link3 selftest)", "link3", {"selftest"},
                     {"  -1/-1 (", "  +1/+1 (", "  +0/+0 (", "  -2/-2 ("}, {4, 4, 4, 4}, false});
        v.push_back({S3, "S_c generates the kernel (chz kernel 4 9.5 3)", "chz", {"kernel", "4", "9.5", "3"},
                     {"CERTIFICATE that S_c generates K: YES"}, {}, false});
        v.push_back({S3, "the root balls, R = 4.5 (zipper3 --stats)", "zipper3", {"-R", "4.5", "-a", "0.3", "--stats"},
                     {"; checked: "}, {}, false});
        const int pairs[13][4] = {{4, 10, 1, 1},  {4, 12, 1, -1},  {4, 17, 1, 1},   {4, 14, 6, 1},   {2, 10, 13, 1},
                                  {4, 4, 13, -1}, {10, 3, 13, -1}, {10, 4, 13, 1},  {13, 10, 13, 1}, {17, 4, 13, 1},
                                  {2, 12, 16, -1}, {12, 4, 16, -1}, {13, 12, 16, -1}};
        for (const auto& q : pairs) {
            char what[160], want[96];
            snprintf(what, sizeof what, "the linking number %+d of the stored pair w%d w%d h%d (link3 --pair)", q[3], q[0],
                     q[1], q[2]);
            snprintf(want, sizeof want, ": lk %+d  (A over B and B over A agree)", q[3]);
            v.push_back({S3, what, "link3",
                         {"-a", "0.01", "--nrel", "20", "--hr", "2.6", "--pair", std::to_string(q[0]), std::to_string(q[1]),
                          std::to_string(q[2])},
                         {want}, {20}, false});
        }
        return v;
    }
    v.push_back({S4, "the reflection group and the cocycle (chz5 selftest)", "chz5", {"selftest"},
                 {"selftest: 160 orthogonal pairs, cocycle on every square from all 16 states; ok"}, {}, false});
    v.push_back({S4, "S_c generates the kernel (chz5 kernel 2.3 4.5 3)", "chz5", {"kernel", "2.3", "4.5", "3"},
                 {"CERTIFICATE that S_c generates K: YES"}, {}, false});
    v.push_back({S4, "the root balls, R = 2.1 (zipper5 --stats)", "zipper5", {"-R", "2.1", "-a", "0.3", "--stats"},
                 {"; checked: "}, {}, false});
    return v;
}

size_t count_of(const string& text, const string& s)
{
    size_t n = 0;
    for (size_t p = text.find(s); p != string::npos; p = text.find(s, p + s.size())) n++;
    return n;
}

int remove_entry(const char* path, const struct stat*, int, struct FTW*) { return remove(path); }

// The signal (SIGINT, SIGTERM, SIGHUP, or SIGPIPE: what reads zippers' output, as `| head`, has stopped) that
// interrupted zippers check, 0 if none. The handler only sets it; check() does the rest in its normal flow: it
// stops the running check if there is one, removes its folder, says so, and ends with the signal.
volatile sig_atomic_t g_interrupted = 0;
void on_signal(int sig) { g_interrupted = sig; }

const int SIGNALS[] = {SIGINT, SIGTERM, SIGHUP, SIGPIPE};
const int N_SIGNALS = sizeof SIGNALS / sizeof SIGNALS[0];
bool g_caught[N_SIGNALS] = {};               // which of SIGNALS zippers check catches

// catch SIGNALS, but for those ignored when zippers started (SIGHUP under nohup, SIGINT in a background job of a
// shell without job control): they stay ignored, by zippers and by its checks
void catch_signals()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                       // no SA_RESTART: a signal ends the wait for the check's output
    for (int k = 0; k < N_SIGNALS; k++) {
        struct sigaction old;
        if (sigaction(SIGNALS[k], nullptr, &old) == 0 && old.sa_handler == SIG_IGN) continue;
        g_caught[k] = sigaction(SIGNALS[k], &sa, nullptr) == 0;
    }
}

const char* signal_name(int sig)
{
    return sig == SIGINT ? "SIGINT" : sig == SIGTERM ? "SIGTERM" : sig == SIGHUP ? "SIGHUP" : "SIGPIPE";
}

// run a check in the folder `dir`, without ZIPPERS_DATA in its environment (the checks read the suite's own data):
// its exit status (-1 if it could not run, or was stopped), its output (stdout and stderr). An interruption
// (g_interrupted) stops it: the wait for its output ends at the signal (no SA_RESTART), and lasts a fifth of a second
// at most between two looks at the flag, for a signal that comes just before a wait begins.
int run_check(const Check& c, const string& path, const string& dir, string& out)
{
    int fd[2];
    if (pipe(fd) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }
    if (pid == 0) {
        for (int k = 0; k < N_SIGNALS; k++)    // the signals that zippers catches; those it found ignored stay ignored
            if (g_caught[k]) signal(SIGNALS[k], SIG_DFL);
        unsetenv("ZIPPERS_DATA");
        close(fd[0]);
        dup2(fd[1], 1);
        dup2(fd[1], 2);
        close(fd[1]);
        if (chdir(dir.c_str()) != 0) _exit(126);
        vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));
        for (const string& a : c.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(path.c_str(), argv.data());
        _exit(127);
    }
    close(fd[1]);
    char buf[8192];
    while (!g_interrupted) {
        struct pollfd pfd = {fd[0], POLLIN, 0};
        int r = poll(&pfd, 1, 200);
        if (r == 0 || (r < 0 && errno == EINTR)) continue;
        if (r < 0) break;
        ssize_t k = read(fd[0], buf, sizeof buf);
        if (k > 0) out.append(buf, (size_t)k);
        else if (!(k < 0 && errno == EINTR)) break;
    }
    if (g_interrupted) kill(pid, SIGKILL);
    close(fd[0]);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

bool passes(const Check& c, int status, const string& out, string& why)
{
    if (c.status_only) {
        why = status == 0 ? "" : "exit status " + std::to_string(status);
        return status == 0;
    }
    if (status != 0) {
        why = "exit status " + std::to_string(status);
        return false;
    }
    for (size_t i = 0; i < c.want.size(); i++) {
        size_t n = count_of(out, c.want[i]);
        size_t need = i < c.times.size() ? (size_t)c.times[i] : 0;
        if (need ? n != need : n == 0) {
            why = "\"" + c.want[i] + "\" " + (need ? std::to_string(n) + " times, not " + std::to_string(need) : "missing");
            return false;
        }
    }
    if (out.find("FAILED") != string::npos || out.find("roots: NOT") != string::npos) {
        why = "a check FAILED";
        return false;
    }
    return true;
}

int check(const vector<string>& args, bool dry)
{
    vector<Example> examples;
    for (const string& a : args) {
        if (a == "-h" || a == "--help") {
            usage_command(stdout, "check", 0);
            return 0;
        }
        Example e;
        if (!find_example(a, e)) fail("check: unknown example '%s' (zippers list)", a.c_str());
        examples.push_back(e);
    }
    if (examples.empty())
        for (const char* n : {"fig8", "cartwright-steger", "imm-m5"}) {
            Example e;
            find_example(n, e);
            examples.push_back(e);
        }
    vector<Check> all;
    for (const Example& e : examples)
        for (const Check& c : checks_of(e)) all.push_back(c);
    vector<string> paths;
    for (const Check& c : all) {
        string looked, p = program_path(c.program, looked);
        if (p.empty()) fail("cannot find the program %s (looked for %s)", c.program, looked.c_str());
        paths.push_back(p);
    }
    if (dry) {
        for (size_t i = 0; i < all.size(); i++) {
            string line = quoted(paths[i]);
            for (const string& a : all[i].args) line += " " + quoted(a);
            printf("%s\n", line.c_str());
        }
        return 0;
    }
    const char* t = getenv("TMPDIR");
    string root = string(t && *t ? t : "/tmp");
    while (root.size() > 1 && root.back() == '/') root.pop_back();
    root += "/zippers_check_XXXXXX";
    catch_signals();                       // before the folder exists: an interruption from here on removes it
    vector<char> tmpl(root.begin(), root.end());
    tmpl.push_back(0);
    if (!mkdtemp(tmpl.data())) fail("check: cannot make a temporary folder: %s", strerror(errno));
    root = tmpl.data();
    printf("zippers check: %zu exact checks, each in a folder of its own under %s\n", all.size(), root.c_str());
    fflush(stdout);
    int bad = 0;
    bool stopped = false;                  // whether an interruption stopped a running check (a SIGPIPE comes
                                           // between two: at the writing of a check's line)
    for (size_t i = 0; i < all.size() && !g_interrupted; i++) {
        const Check& c = all[i];
        string dir = root + "/" + std::to_string(i + 1);
        if (mkdir(dir.c_str(), 0755) != 0) {
            nftw(root.c_str(), remove_entry, 16, FTW_DEPTH | FTW_PHYS);
            fail("check: cannot make %s: %s", dir.c_str(), strerror(errno));
        }
        auto t0 = std::chrono::steady_clock::now();
        string out, why;
        int st = run_check(c, paths[i], dir, out);
        if (g_interrupted) {               // the check was stopped: it has no verdict
            stopped = true;
            break;
        }
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        bool ok = passes(c, st, out, why);
        printf("  %s  %-86s %s (%.1f s)%s%s\n", sphere_name(c.sphere), c.what.c_str(), ok ? "pass" : "FAIL", secs,
               ok ? "" : ": ", why.c_str());
        if (!ok) {
            bad++;
            string line = quoted(paths[i]);
            for (const string& a : c.args) line += " " + quoted(a);
            printf("    the command: %s\n    its output:\n", line.c_str());
            size_t p = 0;
            while (p < out.size()) {
                size_t q = out.find('\n', p);
                if (q == string::npos) q = out.size();
                printf("      %s\n", out.substr(p, q - p).c_str());
                p = q + 1;
            }
        }
        fflush(stdout);
    }
    nftw(root.c_str(), remove_entry, 16, FTW_DEPTH | FTW_PHYS);
    if (g_interrupted) {
        const int sig = g_interrupted;
        fflush(stdout);
        fprintf(stderr, "zippers check: interrupted (%s): %s%s %s\n", signal_name(sig),
                stopped ? "the running check was stopped, and " : "", root.c_str(),
                access(root.c_str(), F_OK) != 0 ? "was removed" : "could not be removed: remove it by hand");
        fflush(stderr);
        signal(sig, SIG_DFL);
        raise(sig);                        // end with the signal's status
        return 128 + sig;
    }
    if (bad) printf("zippers check: %d of %zu checks FAILED\n", bad, all.size());
    else printf("zippers check: all %zu checks pass\n", all.size());
    return bad ? 1 : 0;
}

// ---- --table: the options, as a table, for the tests ------------------------------------------------------------------

int table()
{
    printf("# zippers --table: the options of the programs that zippers runs, and what zippers does with them\n");
    printf("# option\tPROGRAM\tPROGRAM_OPTION\tVALUES\tCOMMAND\tSPHERE\tZIPPERS_OPTION\tshown|hidden (in the usage)\n");
    printf("# left-out\tPROGRAM\tPROGRAM_OPTION\tCOMMAND\tREASON\n");
    printf("# not-run\tPROGRAM\tREASON\n");
    for (const Option& o : OPTIONS)
        for (int s : {S2, S3, S4})
            if (o.spheres & s)
                printf("option\t%s\t%s\t%d\t%s\t%s\t%s\t%s\n", program_of(o.command, s), *o.program ? o.program : "-",
                       o.values, o.command, sphere_name(s), o.name, o.help ? "shown" : "hidden");
    for (const LeftOut& l : LEFT_OUT)
        printf("left-out\t%s\t%s\t%s\t%s\n", l.program, l.name, *l.command ? l.command : "-", l.reason);
    for (const auto& n : NOT_RUN) printf("not-run\t%s\t%s\n", n[0], n[1]);
    return 0;
}

// ---- the commands that run one program --------------------------------------------------------------------------------

Example example_arg(const string& command, const vector<string>& args)
{
    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        usage_command(args.empty() ? stderr : stdout, command, 0);
        exit(args.empty() ? 1 : 0);
    }
    Example e;
    if (!find_example(args[0], e)) fail("%s: unknown example '%s' (zippers list)", command.c_str(), args[0].c_str());
    for (size_t i = 1; i < args.size(); i++)
        if (args[i] == "-h" || args[i] == "--help") {
            usage_command(stdout, command, e.sphere);
            exit(0);
        }
    return e;
}

int draw_session(const vector<string>& args, bool dry);

int draw(const vector<string>& args, bool dry)
{
    if (!args.empty() && args[0] == "--session") return draw_session(args, dry);
    Example e = example_arg("draw", args);
    Parsed p = parse("draw", e, vector<string>(args.begin() + 1, args.end()), false);
    if (has(p, "--session"))
        fail("draw: --session takes the place of the example: zippers draw --session FILE [options] [-o FILE]");
    check_phi(p, "draw");
    string out;
    bool png_format = false;
    for (const Given& g : p.options) {
        if (!strcmp(g.option->name, "-o")) out = g.values[0];
        if (!strcmp(g.option->name, "--format")) {
            const string& f = g.values[0];
            if (f != "eps" && f != "EPS" && f != "png" && f != "PNG")
                fail("draw: --format %s: the format is eps or png", f.c_str());
            png_format = f == "png" || f == "PNG";
        }
    }
    if (ends_with_ci(out, ".svg")) fail("draw: -o %s: SVG is not available in this version", out.c_str());
    if (e.sphere != S2 && ends_with_ci(out, ".eps"))
        fail("draw: -o %s: %s draws PNG pictures only", out.c_str(), example_title(e).c_str());
    if (e.sphere == S2 && out.empty() && !png_format && !has(p, "--polylines") && isatty(1) && !dry)
        fail("draw: without -o an S2 picture is EPS on the standard output, which is a terminal here: give -o FILE "
             "(.eps or .png), or redirect the output");
    vector<string> a;
    if (e.sphere == S2) {
        a.push_back("-m");
        a.push_back(e.name);
    }
    pass_on(p, a);
    run(program_of("draw", e.sphere), a, dry);
}

// a file that a session names (key: f, the substitution file, or pairs), found by the data search, against the hash
// the session gives (sha256.f, sha256.pairs; none when an option given after the session replaced the file)
void check_session_file(const zippers::session::Session& s, const string& fn, const Example& e, const char* key,
                        const char* hash_key, const char* what, const char* option)
{
    const string* h = s.value(hash_key);
    if (!h) return;
    const string* v = s.value(key);
    if (!v && strcmp(key, "f")) return;                  // (no file of pairs: nothing to check)
    string name;
    if (v) name = *v;
    else if (e.sphere == S3 && s.value("pairs")) name = zippers::session::DEFAULT_PAIRS_FILE;
    else name = zippers::session::find_key("f", e.sphere == S3 ? 3 : 4)->dflt[e.sphere == S3 ? 1 : 2];
    zippers::DataFile r = zippers::find_data_file(name, e.sphere == S4 ? "s4" : "s3", PLACES);
    if (r.path.empty()) {
        zippers::data_file_missing(name, r, what, option);
        exit(1);
    }
    string hex, err;
    if (!zippers::sha256_file(r.path, hex, &err)) fail("draw --session %s: %s", fn.c_str(), err.c_str());
    if (hex != *h) {
        const string where = r.path == name ? "in the working folder" : "at " + r.path;
        fail("draw --session %s: %s %s, found %s, has the SHA-256 %s; the session's is %s (it was written with another "
             "file of that name)", fn.c_str(), what, name.c_str(), where.c_str(), hex.c_str(), h->c_str());
    }
}

// zippers draw --session FILE [options]: the session's example and options, with those given after it in their place,
// and its files checked by their hashes; then the picture, as from that command line
int draw_session(const vector<string>& args, bool dry)
{
    if (args.size() < 2 || option_like(args[1]))
        fail("draw: --session needs a session file: zippers draw --session FILE [options] [-o FILE]");
    const string fn = args[1];
    zippers::session::Session s;
    string err;
    vector<string> warnings;
    if (!zippers::session::read(fn, s, &err, &warnings)) fail("draw --session: %s", err.c_str());
    for (const string& w : warnings) fprintf(stderr, "zippers: draw --session: warning: %s\n", w.c_str());
    Example e;
    if (!find_example(*s.value("example"), e)) fail("draw --session %s: unknown example", fn.c_str());
    const vector<string> rest(args.begin() + 2, args.end());
    for (const string& a : rest)
        if (a == "-h" || a == "--help") {
            usage_command(stdout, "draw", e.sphere);
            exit(0);
        }
    Parsed p = parse("draw", e, rest, false);          // the options given after it, against the table
    if (has(p, "--session")) fail("draw: --session is given once, first: zippers draw --session FILE [options] [-o FILE]");
    vector<string> others;
    if (!zippers::session::apply(s, rest, others, err)) fail("draw --session %s: %s", fn.c_str(), err.c_str());
    if (e.sphere != S2) check_session_file(s, fn, e, "f", "sha256.f", "the substitution file", "-f");
    if (e.sphere == S3) check_session_file(s, fn, e, "pairs", "sha256.pairs", "the file of linked pairs", "--pairs");
    vector<string> a = zippers::session::args_of(s);
    a.insert(a.end(), others.begin(), others.end());
    return draw(a, dry);
}

// zippers view [EXAMPLE] [options], or --session FILE [options]: the options checked against draw's table (less
// VIEW_LEFT_OUT), then zview with the arguments unchanged
int view(const vector<string>& args_given, bool dry)
{
    const vector<string> args = args_given.empty() ? vector<string>{"fig8", "-n", "2", "-R", "6"} : args_given;
    if (args[0] == "--session") {
        if (args.size() < 2 || option_like(args[1]))
            fail("view: --session needs a session file: zippers view --session FILE [options] [-o STEM]");
        zippers::session::Session s;
        string err;
        if (!zippers::session::read(args[1], s, &err)) fail("view --session: %s", err.c_str());
        Example e;
        if (!find_example(*s.value("example"), e)) fail("view --session %s: unknown example", args[1].c_str());
        const vector<string> rest(args.begin() + 2, args.end());
        for (const string& a : rest)
            if (a == "-h" || a == "--help") {
                usage_command(stdout, "view", e.sphere);
                exit(0);
            }
        Parsed p = parse("view", e, rest, false);
        if (has(p, "--session")) fail("view: --session is given once, first: zippers view --session FILE [options]");
        vector<string> others;
        if (!zippers::session::apply(s, rest, others, err)) fail("view --session %s: %s", args[1].c_str(), err.c_str());
        run("zview", args, dry);
    }
    Example e = example_arg("view", args);
    Parsed p = parse("view", e, vector<string>(args.begin() + 1, args.end()), false);
    if (has(p, "--session"))
        fail("view: --session takes the place of the example: zippers view --session FILE [options]");
    check_phi(p, "view");
    run("zview", args, dry);
}

bool is_number(const string& s, bool integer)
{
    if (s.empty()) return false;
    char* end = nullptr;
    if (integer) strtol(s.c_str(), &end, 10);
    else strtod(s.c_str(), &end);
    return end && !*end;
}

int search(const vector<string>& args, bool dry)
{
    Example e = example_arg("search", args);
    if (e.sphere == S2)
        fail("search: an S2 example has no search: its substitution is the monodromy (zippers search cartwright-steger, "
             "imm-m5)");
    Parsed p = parse("search", e, vector<string>(args.begin() + 1, args.end()), true);
    check_phi(p, "search");
    vector<string> n = p.words;
    if (n.size() == 1 || n.size() > 3)
        fail("search: give C R [TPHI], or no numbers (zippers search --help); found %zu number%s", n.size(),
             n.size() == 1 ? "" : "s");
    for (size_t i = 0; i < n.size(); i++)
        if (!is_number(n[i], i == 2))
            fail("search: '%s' is not %s (C R [TPHI])", n[i].c_str(), i == 2 ? "an integer" : "a number");
    string rho = "3";
    bool presentation = false;
    for (const Given& g : p.options) {
        if (!strcmp(g.option->name, "--rho")) {
            rho = g.values[0];
            if (!is_number(rho, false)) fail("search: --rho %s: not a number", rho.c_str());
        } else if (strcmp(g.option->name, "--phi")) presentation = true;
    }
    vector<string> a = {"kernel"};
    bool defaults = n.empty();
    if (defaults) n = e.sphere == S3 ? vector<string>{"4", "9.5", "3"} : vector<string>{"2.3", "4.5"};
    a.push_back(n[0]);
    a.push_back(n[1]);
    if (e.sphere == S4) a.push_back(rho);
    if (n.size() == 3) a.push_back(n[2]);
    pass_on(p, a);
    if (defaults && !presentation) {
        if (e.sphere == S3) a.insert(a.end(), {"--roofs", "--jexcess", "1"});
        else a.push_back("--roofs");
    }
    if (defaults) {
        string line = program_of("search", e.sphere);
        for (const string& x : a) line += " " + quoted(x);
        fprintf(stderr, "zippers search: without numbers, %s of the default file %s: %s\n",
                presentation ? "the numbers of the search" : "the search",
                e.sphere == S3 ? "subst_4.00_9.50_t3_r1.txt" : "subst5_2.30_4.50_t1_r0.txt", line.c_str());
    }
    run(program_of("search", e.sphere), a, dry);
}

int link(const vector<string>& args, bool dry)
{
    Example e = example_arg("link", args);
    if (e.sphere != S3)
        fail("link: only cartwright-steger: link3 computes the linking numbers of loops in S3 (zippers link "
             "cartwright-steger)");
    Parsed p = parse("link", e, vector<string>(args.begin() + 1, args.end()), false);
    check_phi(p, "link");
    vector<string> a;
    if (!has(p, "-a")) a.insert(a.end(), {"-a", "0.01"});
    if (!has(p, "--nrel")) a.insert(a.end(), {"--nrel", "20"});
    if (!has(p, "--hr")) a.insert(a.end(), {"--hr", "2.6"});
    pass_on(p, a);
    bool scan = !has(p, "--pair") && !has(p, "--export") && !has(p, "--hinfo");
    if (scan && !dry) {
        struct stat st;
        if (stat("pictures", &st) != 0) {
            if (mkdir("pictures", 0755) != 0) fail("link: cannot make the folder pictures/: %s", strerror(errno));
            fprintf(stderr, "zippers link: made the folder pictures/, into which the scan writes its pictures\n");
        } else if (!S_ISDIR(st.st_mode))
            fail("link: pictures exists and is not a folder: the scan writes its pictures into the folder pictures/");
    }
    run("link3", a, dry);
}

int film(const vector<string>& args, bool dry)
{
    if (args.empty()) {
        usage_command(stderr, "film", S2);
        return 1;
    }
    if (args[0] == "-h" || args[0] == "--help") {
        usage_command(stdout, "film", S2);
        return 0;
    }
    if (args[0] == "--assemble") {
        check_values("film", "--assemble", 2, "TIMELINE.txt OUTDIR", args, 1);
        if (args.size() != 3) fail("film: --assemble TIMELINE.txt OUTDIR (two arguments)");
        run("zmovie", args, dry);
    }
    if (args[0][0] == '-') fail("film: the shot file comes first (zippers film SHOT.txt [options])");
    for (size_t i = 1; i < args.size(); i++) {
        const string& a = args[i];
        if (a == "-h" || a == "--help") {
            usage_command(stdout, "film", S2);
            return 0;
        }
        if (a.find('=') != string::npos && a[0] != '-') continue;     // key=value
        if (a == "--assemble") fail("film: --assemble comes first: zippers film --assemble TIMELINE.txt OUTDIR");
        if (a.empty() || a[0] != '-') fail("film: unexpected argument '%s' (key=value, or an option)", a.c_str());
        const Option* o = find_option("film", S2, a);
        if (!o) {
            for (const LeftOut& l : LEFT_OUT)
                if (!strcmp(l.command, "film") && a == l.name) fail("film: %s is not an option of zippers: %s", a.c_str(), l.reason);
            fail("film: unknown option %s (zippers film --help)", a.c_str());
        }
        check_values("film", o->name, o->values, o->args, args, i + 1);
        if (i + (size_t)o->values >= args.size()) fail("film: %s needs a value (%s %s)", a.c_str(), a.c_str(), o->args);
        i += (size_t)o->values;
    }
    run("zmovie", args, dry);
}

}  // namespace

int main(int argc, char** argv)
{
    vector<string> args(argv + 1, argv + argc);
    bool dry = false;
    size_t i = 0;
    for (; i < args.size() && !args[i].empty() && args[i][0] == '-'; i++) {
        const string& a = args[i];
        if (a == "--dry-run") dry = true;
        else if (a == "-h" || a == "--help") {
            usage_top(stdout);
            return 0;
        } else if (a == "--version") {
            printf("zippers %s\n", ZIPPERS_VERSION);
            return 0;
        } else if (a == "--table") return table();
        else fail("unknown option %s (zippers --help)", a.c_str());
    }
    if (i >= args.size()) {
        usage_top(stderr);
        return 1;
    }
    string command = args[i];
    vector<string> rest(args.begin() + (long)i + 1, args.end());
    if (command == "list") {
        for (const string& a : rest)
            if (a == "-h" || a == "--help") {
                usage_command(stdout, "list", 0);
                return 0;
            }
        return list(rest);
    }
    if (command == "draw") return draw(rest, dry);
    if (command == "view") return view(rest, dry);
    if (command == "search") return search(rest, dry);
    if (command == "link") return link(rest, dry);
    if (command == "film") return film(rest, dry);
    if (command == "check") return check(rest, dry);
    fail("unknown command '%s' (zippers --help)", command.c_str());
}
