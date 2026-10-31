// SPDX-License-Identifier: GPL-3.0-or-later
//
// test_gui_jobs: the jobs of apps/gui/jobs.h, with the build's zippers.
//
//   - find_zippers finds the build's zippers; statm_resident reads the text of /proc/PID/statm (the Linux branch of the
//     memory sampling, read here from a sample text); the machine's memory and this process's are known;
//   - A CURVES JOB of each sphere (S^2 at R = 2, S^3 and S^4 at R = 0, coarse) gives the file of zippers draw run
//     directly with the same command: the same layers and points, bit for bit, and the same header but the command;
//     the child's standard error is kept; its process is gone afterwards (waited for: no zombie), and no file is left;
//   - CANCELLING: a long job (S^3 at R = 4) ended after 0.5 s, by cancel() and by a newer request for curves: its process
//     is gone within 1 s, its files are removed, the result says why; a newer request's job then runs, and the
//     superseded job gives no result;
//   - FIVE REQUESTS for curves in quick succession start one job, the last one's (with a debounce of 1 s; the count is
//     checked when every gap between two requests was shorter);
//   - A PICTURE JOB is not ended by a request for curves, and the curves job waits for it: one child at a time;
//   - QUITTING during a picture job (the destructor) ends its child, and removes its partial files (an output that did
//     not exist when it started, and the polyline writer's temporary name of its process) and the folder; an output
//     that existed before is left;
//   - THE MEMORY LIMIT: a small limit ends an S^3 job, and says why; an S^2 job gets --memlimit (at least 0.2 GB);
//   - THE SIGNALS: gui_jobs_probe, whose jobs handle SIGINT, SIGTERM and SIGHUP, ended by each while its child runs: the
//     probe ends by the signal, its child is gone, and its folder is removed. One Jobs handles the signals at a time: a
//     second is refused (not ok(), answered at once), and the earlier dispositions are restored after the first;
//   - THE CHILD: the test holds descriptors at 1024 and 1500 (the soft limit raised to 2048 for them) and a pipe, has
//     SIGINT and SIGTERM at their defaults and SIGHUP and SIGUSR2 ignored, blocks SIGUSR1 in the thread that makes the
//     jobs, and sets ZIPPERS_DATA. The child (the stand-in gui_jobs_child, which tells what it got) of a picture job and
//     of a curves job, with handle_signals and without, has only 0, 1 and 2 (0 and 1 /dev/null), SIGINT and SIGTERM at
//     their defaults, SIGHUP and SIGUSR2 still ignored, no signal blocked, the test's folder, and its ZIPPERS_DATA and
//     TMPDIR;
//   - NOT ok() when zippers cannot run (no such file, no path, a folder of the name): nothing made, every request
//     answered at once with the reason;
//   - the S^2 --memlimit: the smaller of the jobs' limit and the one given; the peak memory of a job shorter than a
//     sample (from wait4); a cancelled picture whose output is a dangling link leaves no partial target, and the link;
//   - find_zippers (through gui_find_zippers): the build's zippers from the build; from an install's layout (copied into
//     L/libexec/zippers/, L the folder ZIPPERS_LAYOUT of the build), L/bin/zippers; with that removed, nothing, though a
//     zippers is in PATH; and nothing for a folder named zippers in its place.
// About 20 s; every child of the test and of its jobs is gone when it ends. ZIPPERS_ZIPPERS, ZIPPERS_PROBE, ZIPPERS_CHILD
// and ZIPPERS_FIND are the paths of zippers, gui_jobs_probe, gui_jobs_child and gui_find_zippers.

#include "jobs.h"
#include "output/polylines.h"
#include "render_test_support.h"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace gui = zippers::gui;
namespace pl = zippers::polylines;
using rtest::check;
using rtest::note;
using rtest::section;

#ifndef ZIPPERS_ZIPPERS
#define ZIPPERS_ZIPPERS ""
#endif
#ifndef ZIPPERS_PROBE
#define ZIPPERS_PROBE ""
#endif
#ifndef ZIPPERS_CHILD
#define ZIPPERS_CHILD ""
#endif
#ifndef ZIPPERS_FIND
#define ZIPPERS_FIND ""
#endif
#ifndef ZIPPERS_LAYOUT
#define ZIPPERS_LAYOUT ""
#endif

namespace {

using Clock = std::chrono::steady_clock;
double since(Clock::time_point t) { return std::chrono::duration<double>(Clock::now() - t).count(); }
void sleep_s(double s) { std::this_thread::sleep_for(std::chrono::microseconds((long long)(s*1e6))); }

bool gone(pid_t pid) { return pid > 0 && kill(pid, 0) != 0 && errno == ESRCH; }

// whether the process is gone within `seconds`
bool gone_within(pid_t pid, double seconds)
{
    auto t0 = Clock::now();
    while (since(t0) < seconds) {
        if (gone(pid)) return true;
        sleep_s(0.01);
    }
    return gone(pid);
}

std::vector<std::string> names_in(const std::string& dir)
{
    std::vector<std::string> v;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n != "." && n != "..") v.push_back(n);
        }
        closedir(d);
    }
    std::sort(v.begin(), v.end());
    return v;
}

bool folder_exists(const std::string& dir)
{
    DIR* d = opendir(dir.c_str());
    if (d) closedir(d);
    return d != nullptr;
}

gui::JobsOptions options(size_t limit = 0)
{
    gui::JobsOptions o;
    o.zippers = ZIPPERS_ZIPPERS;
    o.memory_limit = limit;
    return o;
}

// wait until the jobs run a child of this kind; its pid, or -1
pid_t running(gui::Jobs& jobs, gui::JobKind kind, double seconds)
{
    auto t0 = Clock::now();
    while (since(t0) < seconds) {
        gui::JobsState s = jobs.state();
        if (s.running && s.kind == kind) return s.pid;
        sleep_s(0.01);
    }
    return -1;
}

std::string joined(const std::vector<std::string>& v)
{
    std::string s;
    for (const std::string& a : v) s += (s.empty() ? "" : " ") + a;
    return s;
}

// run a command (no shell) in the folder dir, without ZIPPERS_DATA; its exit status
int run_direct(const std::string& dir, const std::vector<std::string>& cmd)
{
    std::string line = "cd '" + dir + "' && unset ZIPPERS_DATA &&";
    for (const std::string& a : cmd) line += " '" + a + "'";
    int st = system((line + " >/dev/null 2>&1 </dev/null").c_str());
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

bool same_curves(const pl::File& a, const pl::File& b)
{
    if (a.sphere != b.sphere || a.layers.size() != b.layers.size() || !rtest::same_vector_bytes(a.rootd, b.rootd))
        return false;
    for (size_t i = 0; i < a.layers.size(); i++) {
        const pl::Layer &x = a.layers[i], &y = b.layers[i];
        if (x.name != y.name || x.first != y.first || x.root != y.root || x.letter != y.letter ||
            !rtest::same_vector_bytes(x.pts, y.pts))
            return false;
    }
    std::vector<std::pair<std::string, std::string>> ka, kb;
    for (const auto& kv : a.keys) if (kv.first != "command") ka.push_back(kv);
    for (const auto& kv : b.keys) if (kv.first != "command") kb.push_back(kv);
    return ka == kb;
}

void test_basics()
{
    section("finding zippers; the memory of the machine and of a process; /proc/PID/statm");
    const std::string z = gui::find_zippers(ZIPPERS_DATA_PLACES, ZIPPERS_ZIPPERS, "../../bin");
    check(z == ZIPPERS_ZIPPERS, "find_zippers in the build: the build's zippers (%s)", z.c_str());
    check(gui::find_zippers(ZIPPERS_DATA_PLACES, "/nowhere/zippers", "../../bin").empty(), "a zippers that is not there: none");
    check(gui::machine_memory() > ((size_t)1 << 28) && gui::process_memory(getpid()) > ((size_t)1 << 20),
          "the machine's memory (%.2f GB) and this process's (%.1f MB)", gui::machine_memory()/1e9,
          gui::process_memory(getpid())/1e6);
    check(gui::statm_resident("25131 5123 870 2 0 4567 0\n", 4096) == (size_t)5123*4096 &&
          gui::statm_resident("garbage", 4096) == 0 && gui::statm_resident("1 2", 0) == 0,
          "statm_resident: the second field, in pages");
    gui::Jobs jobs(options());
    check(jobs.ok() && folder_exists(jobs.folder()) && jobs.memory_limit() == (size_t)(0.4*(double)gui::machine_memory()),
          "the jobs' folder, and the default limit: 40%% of the machine's memory (%.2f GB)", jobs.memory_limit()/1e9);
}

void test_curves(const rtest::TempDir& t)
{
    section("a curves job gives the file of zippers draw run directly");
    gui::Jobs jobs(options());
    std::string folder = jobs.folder();
    struct Case { const char* what; std::vector<std::string> args; };
    const Case cases[] = {
        {"S2", {"fig8", "-R", "2", "-a", "0.01", "--nocull"}},
        {"S3", {"cartwright-steger", "-R", "0", "-a", "0.1"}},
        {"S4", {"imm-m5", "-R", "0", "-a", "0.1"}},
    };
    for (const Case& c : cases) {
        jobs.request_curves(c.args);
        gui::JobResult r;
        if (!check(jobs.wait(r, 60), "%s: the job ends", c.what)) continue;
        check(r.status == 0 && !r.ended && r.has_curves, "%s: exit 0, its curves read (%s; %s)", c.what, r.read_error.c_str(),
              joined(r.stderr_tail).substr(0, 200).c_str());
        // the same command, run directly, writing its file elsewhere
        std::vector<std::string> cmd = r.command;
        for (size_t i = 0; i + 1 < cmd.size(); i++)
            if (cmd[i] == "--polylines") cmd[i + 1] = t.file("direct.zpl");
        pl::File d;
        std::string err;
        int st = run_direct(t.path, cmd);
        check(st == 0 && pl::read(t.file("direct.zpl"), d, &err), "%s: zippers draw directly (exit %d, %s)", c.what, st,
              err.c_str());
        check(r.has_curves && same_curves(r.curves, d), "%s: the same curves and header (but the command), bit for bit", c.what);
        check(!r.stderr_tail.empty() && joined(r.stderr_tail).find("wrote " + folder + "/") != std::string::npos,
              "%s: its standard error kept, to its last line (%zu lines: ... %s)", c.what, r.stderr_tail.size(),
              r.stderr_tail.empty() ? "" : r.stderr_tail.back().c_str());
        check(gone(r.pid), "%s: its process is gone (waited for: no zombie)", c.what);
        check(names_in(folder).empty(), "%s: no file left in the folder", c.what);
        if (c.what[1] == '2') {
            bool memlimit = false;
            for (size_t i = 0; i + 1 < r.command.size(); i++) memlimit = memlimit || r.command[i] == "--memlimit";
            check(memlimit, "S2: the job has --memlimit (%s)", joined(r.command).c_str());
        }
    }
}

void test_cancel()
{
    section("cancelling: a long job ended after 0.5 s is gone within 1 s, with its files");
    const std::vector<std::string> longjob = {"cartwright-steger", "-R", "4", "-a", "0.01"};
    {
        gui::Jobs jobs(options());
        jobs.request_curves(longjob);
        pid_t pid = running(jobs, gui::JobKind::curves, 5);
        check(pid > 0, "the long job runs (pid %ld)", (long)pid);
        sleep_s(0.5);
        auto t0 = Clock::now();
        jobs.cancel();
        bool ok = gone_within(pid, 1.0);
        check(ok, "cancel(): its process is gone in %.2f s", since(t0));
        gui::JobResult r;
        check(jobs.wait(r, 2) && r.ended && r.reason == "cancelled" && !r.has_curves, "its result: ended, cancelled");
        check(names_in(jobs.folder()).empty(), "its files are removed");
    }
    {
        gui::Jobs jobs(options());
        const uint64_t first = jobs.request_curves(longjob);
        pid_t pid = running(jobs, gui::JobKind::curves, 5);
        sleep_s(0.5);
        auto t0 = Clock::now();
        const uint64_t second = jobs.request_curves({"cartwright-steger", "-R", "0", "-a", "0.1"});
        check(gone_within(pid, 1.0), "a newer request for curves ends it: gone in %.2f s", since(t0));
        gui::JobResult r;
        check(jobs.wait(r, 30) && r.id == second && r.status == 0 && r.has_curves,
              "the newer request's job runs, and gives the only result (request %llu, not %llu)", (unsigned long long)r.id,
              (unsigned long long)first);
        gui::JobResult extra;
        check(!jobs.wait(extra, 0.3), "the superseded job gives no result");
        check(names_in(jobs.folder()).empty() && jobs.state().started == 2, "no file left; two jobs were started");
    }

    section("five requests for curves in quick succession start one job");
    {
        // (a debounce of 1 s, so that a machine held up between two requests 0.05 s apart does not start a job; the
        // gaps are measured, and the count is checked only when they were all shorter than the debounce)
        gui::JobsOptions o = options();
        o.debounce = 1.0;
        gui::Jobs jobs(o);
        uint64_t last = 0;
        double widest = 0;
        auto prev = Clock::now();
        for (int k = 0; k < 5; k++) {
            last = jobs.request_curves({"imm-m5", "-R", "0", "-a", "0.1"});
            const double gap = since(prev);
            prev = Clock::now();
            if (k > 0 && gap > widest) widest = gap;
            sleep_s(0.05);
        }
        gui::JobResult r;
        check(jobs.wait(r, 30) && r.id == last && r.has_curves, "the result is the last request's (%llu)", (unsigned long long)r.id);
        gui::JobResult extra;
        const bool none = !jobs.wait(extra, 0.5);
        const unsigned long long started = jobs.state().started;
        if (widest < o.debounce)
            check(none && started == 1, "one job was started (%llu; the widest gap %.3f s)", started, widest);
        else
            note("a gap of %.3f s between two requests, longer than the debounce: the count (%llu) not checked", widest,
                 started);
    }
}

void test_picture(const rtest::TempDir& t)
{
    section("a picture job is not ended by a request for curves; one child at a time");
    {
        gui::Jobs jobs(options());
        const std::string png = t.file("p.png");
        jobs.request_picture({"cartwright-steger", "-R", "2.554", "-a", "0.02", "-s", "200", "-o", png}, {png});
        pid_t pp = running(jobs, gui::JobKind::picture, 5);
        check(pp > 0, "the picture job runs");
        jobs.request_curves({"cartwright-steger", "-R", "0", "-a", "0.1"});
        // while the picture runs, no other child. The state is taken first, then whether the picture is gone: the
        // jobs reap it and start the next child in one hold of their lock, so a state taken while it was not yet
        // reaped shows it (or no child), and one taken after may show the next child, which is not counted
        bool other = false;
        auto t0 = Clock::now();
        while (since(t0) < 30) {
            gui::JobsState s = jobs.state();
            if (gone(pp)) break;
            other = other || (s.running && s.pid != pp);
            sleep_s(0.005);
        }
        gui::JobResult r1, r2;
        check(jobs.wait(r1, 30) && r1.kind == gui::JobKind::picture && r1.status == 0 && !r1.ended &&
              !rtest::read_file(png).empty(), "the picture job ends by itself, its picture written (exit %d)", r1.status);
        check(!other, "no other child while it ran");
        check(jobs.wait(r2, 30) && r2.kind == gui::JobKind::curves && r2.has_curves && gone(r1.pid),
              "then the curves job runs, after it");
    }

    section("quitting during a picture job removes its partial files and the folder");
    {
        const std::string png = t.file("q.png"), old = t.file("old.zpl");
        rtest::write_file(old, "an older file");
        std::string folder;
        pid_t pid = -1;
        {
            gui::Jobs jobs(options());
            folder = jobs.folder();
            jobs.request_picture({"cartwright-steger", "-R", "4", "-a", "0.01", "-o", png, "--polylines", old}, {png, old});
            pid = running(jobs, gui::JobKind::picture, 5);
            sleep_s(0.5);
            // what a program writing its outputs leaves: a part of its picture, the polyline writer's temporary file
            rtest::write_file(png, "a part of a picture");
            rtest::write_file(old + ".tmp" + std::to_string((long)pid) + "_0", "a part of a polyline file");
        }
        check(gone(pid), "its child is gone");
        check(rtest::read_file(png).empty() && rtest::read_file(old + ".tmp" + std::to_string((long)pid) + "_0").empty(),
              "the partial picture and the temporary file are removed");
        check(rtest::read_file(old) == "an older file", "an output that existed before is left as it was");
        check(!folder_exists(folder), "the folder is removed");
    }
}

void test_memory_limit()
{
    section("the memory limit: an S3 job past it is ended; an S2 job gets --memlimit");
    {
        gui::Jobs jobs(options(60u << 20));
        jobs.request_curves({"cartwright-steger", "-R", "2.554", "-a", "0.01"});
        gui::JobResult r;
        check(jobs.wait(r, 60) && r.ended && r.reason.find("passed the limit of 0.06 GB") != std::string::npos &&
              r.peak_memory > (60u << 20) && gone(r.pid), "ended: %s (peak %.0f MB)", r.reason.c_str(), r.peak_memory/1e6);
    }
    {
        gui::Jobs jobs(options(60u << 20));
        jobs.request_curves({"fig8", "-R", "2", "-a", "0.01", "--nocull", "--memlimit", "3"});
        gui::JobResult r;
        bool ok = jobs.wait(r, 60);
        std::string ml;
        int n = 0;
        for (size_t i = 0; i + 1 < r.command.size(); i++)
            if (r.command[i] == "--memlimit") {
                ml = r.command[i + 1];
                n++;
            }
        check(ok && n == 1 && ml == "0.2" && !r.ended && (r.status == 0 || r.status == 3),
              "S2: one --memlimit, 0.2 GB (the job's limit, at least 0.2), in place of the given one; not ended (exit %d)",
              r.status);
    }
}

void test_signals()
{
    section("the folder is removed after SIGINT, SIGTERM and SIGHUP");
    if (!*ZIPPERS_PROBE) {
        check(false, "gui_jobs_probe is not built");
        return;
    }
    for (int sig : {SIGINT, SIGTERM, SIGHUP}) {
        int fd[2];
        if (pipe(fd) != 0) continue;
        pid_t probe = fork();
        if (probe == 0) {                   // (the signals at their defaults: a test started with one ignored, as a
            for (int s : {SIGINT, SIGTERM, SIGHUP})      // shell's background job ignores SIGINT, would not catch it)
                signal(s, SIG_DFL);
            dup2(fd[1], 1);
            close(fd[0]);
            close(fd[1]);
            execl(ZIPPERS_PROBE, ZIPPERS_PROBE, ZIPPERS_ZIPPERS, "cartwright-steger", "-R", "4", "-a", "0.01", (char*)nullptr);
            _exit(127);
        }
        close(fd[1]);
        std::string line;
        auto t0 = Clock::now();
        char c;
        while (since(t0) < 10 && line.find('\n') == std::string::npos) {
            ssize_t k = read(fd[0], &c, 1);
            if (k == 1) line += c;
            else if (k == 0) break;
        }
        close(fd[0]);
        char folder[4096] = "";
        long child = -1;
        const bool told = sscanf(line.c_str(), "%4095s %ld", folder, &child) == 2;
        if (!check(told, "%s: the probe's job runs (%s)", strsignal(sig), line.c_str())) {
            kill(probe, SIGKILL);
            waitpid(probe, nullptr, 0);
            continue;
        }
        kill(probe, sig);
        int st = 0;
        waitpid(probe, &st, 0);
        check(WIFSIGNALED(st) && WTERMSIG(st) == sig, "%s: the probe ends by the signal", strsignal(sig));
        check(gone_within((pid_t)child, 1.0), "%s: its job's child is gone", strsignal(sig));
        check(!folder_exists(folder), "%s: its folder is removed", strsignal(sig));
    }
}

// a command's standard output (its first line), run in the folder dir with the environment `env` before it
std::string output_of(const std::string& dir, const std::string& env, const std::string& cmd)
{
    std::string out;
    FILE* p = popen(("cd '" + dir + "' && " + env + " '" + cmd + "' 2>/dev/null </dev/null").c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, k);
    pclose(p);
    return out.substr(0, out.find('\n'));
}

bool copy_executable(const std::string& from, const std::string& to)
{
    const std::string bytes = rtest::read_file(from);
    return !bytes.empty() && rtest::write_file(to, bytes) && chmod(to.c_str(), 0755) == 0;
}

bool is_link(const std::string& p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

// the folders of jobs (zippers_jobs_XXXXXX) in TMPDIR
size_t jobs_folders()
{
    const char* t = getenv("TMPDIR");
    size_t n = 0;
    for (const std::string& name : names_in(t && *t ? t : "/tmp")) n += name.rfind("zippers_jobs_", 0) == 0;
    return n;
}

// the line of a job's standard error that begins with `key` ("" if there is none)
std::string line_of(const gui::JobResult& r, const std::string& key)
{
    for (const std::string& l : r.stderr_tail)
        if (l.rfind(key, 0) == 0) return l;
    return std::string();
}

void test_child()
{
    section("the child: only its standard streams, its signals at their defaults or still ignored, none blocked, this "
            "folder and environment");
    if (!*ZIPPERS_CHILD) {
        check(false, "gui_jobs_child is not built");
        return;
    }
    // descriptors open here at 1024 and 1500 (the soft limit raised to 2048 for them), and a pipe
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    const struct rlimit old = rl;
    if (rl.rlim_cur != RLIM_INFINITY && rl.rlim_cur < 2048) {
        rl.rlim_cur = rl.rlim_max == RLIM_INFINITY || rl.rlim_max >= 2048 ? 2048 : rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
    }
    getrlimit(RLIMIT_NOFILE, &rl);
    const long soft = rl.rlim_cur == RLIM_INFINITY || rl.rlim_cur > 2048 ? 2048 : (long)rl.rlim_cur;
    const int lo = soft > 1024 ? 1024 : (int)soft - 2, hi = soft > 1500 ? 1500 : (int)soft - 1;   // (a hard limit below 2048)
    const int nul = open("/dev/null", O_RDONLY), f_lo = dup2(nul, lo), f_hi = dup2(nul, hi);
    int p[2] = {-1, -1};
    const bool piped = pipe(p) == 0;
    check(nul >= 0 && f_lo == lo && f_hi == hi && piped, "open here: %d, %d, %d, and a pipe (%d, %d)", nul, lo, hi, p[0],
          p[1]);
    // SIGINT and SIGTERM at their defaults (so that jobs with handle_signals catch them), SIGHUP and SIGUSR2 ignored;
    // SIGUSR1 blocked in this thread, and so in the jobs' thread, which starts the children; ZIPPERS_DATA set
    const int sigs[] = {SIGINT, SIGTERM, SIGHUP, SIGUSR2};
    struct sigaction before[4];
    for (int k = 0; k < 4; k++) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = k < 2 ? SIG_DFL : SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sigaction(sigs[k], &sa, &before[k]);
    }
    sigset_t usr1, old_mask;
    sigemptyset(&usr1);
    sigaddset(&usr1, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &usr1, &old_mask);
    const char* had = getenv("ZIPPERS_DATA");
    const std::string old_data = had ? had : "";
    setenv("ZIPPERS_DATA", "/the-data-of-the-window", 1);
    char cwd[PATH_MAX];
    const std::string here = getcwd(cwd, sizeof cwd) ? cwd : "?";
    const char* td = getenv("TMPDIR");
    const std::string env = std::string("env: ZIPPERS_DATA=/the-data-of-the-window TMPDIR=") + (td ? td : "(unset)");
    for (bool handle : {true, false})
        for (gui::JobKind kind : {gui::JobKind::picture, gui::JobKind::curves}) {
            gui::JobsOptions o;
            o.zippers = ZIPPERS_CHILD;
            o.handle_signals = handle;
            gui::Jobs jobs(o);
            if (kind == gui::JobKind::picture) jobs.request_picture({"imm-m5"}, {});
            else jobs.request_curves({"imm-m5"});
            gui::JobResult r;
            const bool ended = jobs.wait(r, 10) && r.status == 0;
            const std::string what = std::string(kind == gui::JobKind::picture ? "a picture job" : "a curves job") +
                                     (handle ? ", handle_signals" : "");
            check(ended && line_of(r, "fds:") == "fds: 0 1 2" && line_of(r, "null:") == "null: yes",
                  "%s: the child has only 0, 1 and 2, and 0 and 1 are /dev/null (%s; %s)", what.c_str(),
                  line_of(r, "fds:").c_str(), line_of(r, "null:").c_str());
            check(line_of(r, "signals:") == "signals: INT=default TERM=default HUP=ignored USR2=ignored",
                  "%s: SIGINT and SIGTERM at their defaults, the ignored SIGHUP and SIGUSR2 still ignored (%s)",
                  what.c_str(), line_of(r, "signals:").c_str());
            check(line_of(r, "mask:") == "mask: none", "%s: no signal blocked (%s)", what.c_str(),
                  line_of(r, "mask:").c_str());
            check(line_of(r, "cwd:") == "cwd: " + here, "%s: this folder (%s)", what.c_str(), line_of(r, "cwd:").c_str());
            check(line_of(r, "env:") == env, "%s: this environment (%s)", what.c_str(), line_of(r, "env:").c_str());
            if (handle && kind == gui::JobKind::picture)
                for (const std::string& l : r.stderr_tail) note("the child: %s", l.c_str());
        }
    if (had) setenv("ZIPPERS_DATA", old_data.c_str(), 1);
    else unsetenv("ZIPPERS_DATA");
    pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);
    for (int k = 0; k < 4; k++) sigaction(sigs[k], &before[k], nullptr);
    close(f_hi);
    close(f_lo);
    close(nul);
    if (piped) {
        close(p[0]);
        close(p[1]);
    }
    setrlimit(RLIMIT_NOFILE, &old);
}

void on_test_signal(int) {}

void test_one_handler()
{
    section("one Jobs handles the signals at a time; after it, the earlier dispositions are back");
    const int sigs[] = {SIGINT, SIGTERM, SIGHUP};
    struct sigaction original[3], mine;
    memset(&mine, 0, sizeof mine);
    mine.sa_handler = on_test_signal;
    sigemptyset(&mine.sa_mask);
    for (int k = 0; k < 3; k++) sigaction(sigs[k], &mine, &original[k]);     // the earlier dispositions: this test's
    auto all = [&](bool mine_now) {
        bool ok = true;
        for (int sig : sigs) {
            struct sigaction now;
            sigaction(sig, nullptr, &now);
            ok = ok && (now.sa_handler == on_test_signal) == mine_now;
        }
        return ok;
    };
    gui::JobsOptions o = options();
    o.handle_signals = true;
    {
        gui::Jobs a(o);
        check(a.ok() && all(false), "the first one handles SIGINT, SIGTERM and SIGHUP (%s)", a.error().c_str());
        const size_t folders = jobs_folders();
        gui::Jobs b(o);
        check(!b.ok() && b.error() == "another Jobs handles the signals already (handle_signals: one at a time)" &&
              b.folder().empty() && jobs_folders() == folders, "a second one is refused, and makes nothing (%s)",
              b.error().c_str());
        const uint64_t id = b.request_curves({"imm-m5", "-R", "0", "-a", "0.3"});
        gui::JobResult r;
        check(b.poll(r) && r.id == id && r.reason == b.error() && r.pid == -1 && !b.state().running,
              "its request is answered at once, with the reason");
        gui::JobsOptions plain = options();
        gui::Jobs c(plain);
        check(c.ok(), "a Jobs that does not handle the signals may be made meanwhile (%s)", c.error().c_str());
    }
    check(all(true), "after it, the earlier dispositions are back");
    {
        gui::Jobs d(o);
        check(d.ok() && all(false), "and a new one may handle the signals (%s)", d.error().c_str());
    }
    check(all(true), "and after that one too");
    for (int k = 0; k < 3; k++) sigaction(sigs[k], &original[k], nullptr);
}

void test_not_ok()
{
    section("when zippers cannot run: not ok(), nothing made, every request answered at once");
    const size_t folders = jobs_folders();
    rtest::TempDir w;                        // (a folder named zippers, which may be "executed": searched)
    const std::string folder = w.file("zippers");
    mkdir(folder.c_str(), 0755);
    for (const std::string& z : {std::string("/nowhere/zippers"), std::string(), folder}) {
        gui::JobsOptions o;
        o.zippers = z;
        gui::Jobs jobs(o);
        const std::string want = z.empty() ? "cannot run zippers: its place is not known" : "cannot run zippers: " + z;
        check(!jobs.ok() && jobs.error() == want && jobs.folder().empty() && jobs_folders() == folders,
              "zippers \"%s\": not ok(), no folder (%s)", z.c_str(), jobs.error().c_str());
        jobs.request_picture({"fig8"}, {"x.png"});
        jobs.request_curves({"fig8"});
        gui::JobResult r1, r2;
        check(jobs.poll(r1) && jobs.poll(r2) && r1.kind == gui::JobKind::picture && r2.kind == gui::JobKind::curves &&
              r1.reason == want && r2.reason == want && r1.status == -1 && r1.outputs.size() == 1,
              "its requests are answered at once, with the reason");
    }
}

std::string memlimit_of(const gui::JobResult& r)
{
    std::string v;
    int n = 0;
    for (size_t i = 0; i + 1 < r.command.size(); i++)
        if (r.command[i] == "--memlimit") {
            v = r.command[i + 1];
            n++;
        }
    return n == 1 ? v : "(" + std::to_string(n) + " of them)";
}

void test_limit_and_peak()
{
    section("S2: --memlimit the smaller of the jobs' limit and the one given; the peak of a job shorter than a sample");
    {
        gui::Jobs a(options(1000000000));
        a.request_curves({"fig8", "-R", "1", "-a", "0.05", "--nocull", "--memlimit", "0.25"});
        gui::JobResult r;
        check(a.wait(r, 60) && memlimit_of(r) == "0.25", "0.25 given, under the jobs' 1 GB: 0.25 (%s)", memlimit_of(r).c_str());
        gui::Jobs b(options(300000000));
        b.request_curves({"fig8", "-R", "1", "-a", "0.05", "--nocull", "--memlimit", "3"});
        check(b.wait(r, 60) && memlimit_of(r) == "0.3", "3 given, over the jobs' 0.3 GB: 0.3 (%s)", memlimit_of(r).c_str());
        b.request_curves({"fig8", "-R", "1", "-a", "0.05", "--nocull"});
        check(b.wait(r, 60) && memlimit_of(r) == "0.3", "none given: the jobs' 0.3 (%s)", memlimit_of(r).c_str());
    }
    if (*ZIPPERS_CHILD) {
        gui::JobsOptions o;
        o.zippers = ZIPPERS_CHILD;
        gui::Jobs jobs(o);
        jobs.request_picture({"imm-m5"}, {});
        gui::JobResult r;
        // (a job shorter than the first sample, at 0.25 s, as it is but on a machine held up, has its peak from wait4
        // alone; between 256 KB and 1 GB, so that bytes and kilobytes are not confused)
        check(jobs.wait(r, 10) && r.peak_memory > ((size_t)1 << 18) && r.peak_memory < ((size_t)1 << 30),
              "a job of %.3f s: its peak, %.2f MB", r.seconds, r.peak_memory/1e6);
        note("a job of %.3f s: its peak %.2f MB (%s)", r.seconds, r.peak_memory/1e6,
             r.seconds < 0.25 ? "wait4's alone: no sample was taken" : "the job was sampled");
    }
}

void test_link_output()
{
    section("a cancelled picture whose output is a link to no file leaves no partial target, and the link");
    rtest::TempDir w;
    const std::string link = w.file("link.png"), target = w.file("target.png");
    check(symlink("target.png", link.c_str()) == 0, "a link to a picture that does not exist yet");
    gui::Jobs jobs(options());
    jobs.request_picture({"cartwright-steger", "-R", "4", "-a", "0.01", "-o", link}, {link});
    pid_t pid = running(jobs, gui::JobKind::picture, 5);
    sleep_s(0.3);
    const bool written = rtest::write_file(link, "a part of a picture");     // as a program writing through the link
    const bool made = written && !rtest::read_file(target).empty();
    jobs.cancel();
    gui::JobResult r;
    check(pid > 0 && jobs.wait(r, 5) && r.ended && gone(pid), "the job cancelled, its child gone");
    check(made && rtest::read_file(target).empty() && is_link(link) && names_in(w.path) == std::vector<std::string>{"link.png"},
          "the partial target removed; the link kept");
}

void test_find_zippers(const rtest::TempDir& t)
{
    section("find_zippers: from the build, from an install's layout, never from PATH");
    if (!*ZIPPERS_FIND || !*ZIPPERS_CHILD) {
        check(false, "gui_find_zippers or gui_jobs_child is not built");
        return;
    }
    const std::string built = output_of(t.path, "", ZIPPERS_FIND);
    check(built == ZIPPERS_ZIPPERS, "from the build: the build's zippers (%s)", built.c_str());
    const std::string rel = ZIPPERS_INSTALLED_RELATIVE;
    int ups = 0;                             // its leading ..
    for (size_t i = 0; i + 1 < rel.size() && rel[i] == '.' && rel[i + 1] == '.' && (i + 2 == rel.size() || rel[i + 2] == '/');
         i += 3)
        ups++;
    if (!check(!rel.empty() && rel[0] != '/' && ups <= 2, "the install's layout: the program's folder, then %s/zippers",
               rel.c_str()))
        return;
    // the layout in the build folder, as test_install's install (a temporary folder may not let programs run)
    const std::string root = ZIPPERS_LAYOUT;
    auto clear = [&] { return !root.empty() && system(("rm -rf '" + root + "'").c_str()) == 0; };
    if (!check(clear(), "the layout's folder %s, cleared", root.c_str())) return;
    const std::string pdir = root + "/libexec/zippers", zdir = pdir + "/" + rel, pathdir = root + "/path";
    const std::string mk = "mkdir -p '" + pdir + "' '" + zdir + "' '" + pathdir + "'";
    const std::string prog = pdir + "/gui_find_zippers", zip = zdir + "/zippers";
    check(system(mk.c_str()) == 0 && copy_executable(ZIPPERS_FIND, prog) && copy_executable(ZIPPERS_CHILD, zip),
          "an install's layout: the program copied into libexec/zippers/, an executable zippers in %s", rel.c_str());
    const std::string found = output_of(t.path, "", prog);
    check(!found.empty() && zippers::real_path(found) == zippers::real_path(zip), "installed: the zippers beside it (%s)",
          found.c_str());
    unlink(zip.c_str());
    copy_executable(ZIPPERS_CHILD, pathdir + "/zippers");
    const std::string none = output_of(t.path, "PATH='" + pathdir + "':\"$PATH\"", prog);
    check(none.empty(), "that one removed: nothing, though a zippers is in PATH (%s)", none.c_str());
    mkdir(zip.c_str(), 0755);
    const std::string dir = output_of(t.path, "", prog);
    check(dir.empty(), "a folder named zippers in its place: nothing (%s)", dir.c_str());
    check(clear(), "the layout removed");
}

}  // namespace

int main()
{
    printf("test_gui_jobs: the jobs (apps/gui/jobs.h): zippers draw as child processes\n");
    if (!*ZIPPERS_ZIPPERS) {
        printf("zippers is not built\n");
        return 1;
    }
    rtest::TempDir t;
    test_basics();
    test_curves(t);
    test_cancel();
    test_picture(t);
    test_memory_limit();
    test_signals();
    test_child();
    test_one_handler();
    test_not_ok();
    test_limit_and_peak();
    test_link_output();
    test_find_zippers(t);
    rtest::end_section();
    printf("%ld compared, %ld differ\n", rtest::all_n, rtest::all_bad);
    return rtest::all_bad ? 1 : 0;
}
