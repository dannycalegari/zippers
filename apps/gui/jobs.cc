// SPDX-License-Identifier: GPL-3.0-or-later
//
// jobs.cc: the jobs of jobs.h.

#include "jobs.h"

#include "output/session.h"

#include <fcntl.h>
#include <ftw.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libproc.h>
#include <spawn.h>
#include <sys/sysctl.h>
extern char** environ;
#elif defined(__linux__)
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zippers::gui {

namespace {

using Clock = std::chrono::steady_clock;

double since(Clock::time_point t) { return std::chrono::duration<double>(Clock::now() - t).count(); }

// the signals of handle_signals: the one that came (the handler only sets it), which are caught, and their old actions
const int SIGNALS[] = {SIGINT, SIGTERM, SIGHUP};
const int N_SIGNALS = 3;
volatile sig_atomic_t g_signal = 0;
bool g_caught[N_SIGNALS] = {};
struct sigaction g_old[N_SIGNALS];
std::atomic<bool> g_handling{false};       // one Jobs handles the signals at a time
void on_signal(int sig) { g_signal = sig; }

int remove_entry(const char* path, const struct stat*, int, struct FTW*) { return remove(path); }
void remove_tree(const std::string& dir)
{
    if (!dir.empty()) nftw(dir.c_str(), remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

bool exists(const std::string& path)
{
    struct stat st;
    return lstat(path.c_str(), &st) == 0;
}

// a file that may be run: a regular file (or a link to one) that may be executed; a folder of the name is not
bool runnable(const std::string& path)
{
    struct stat st;
    return !path.empty() && stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0;
}

// the temporary names of the polyline writer of the process pid, beside path and beside its target: FILE.tmpPID_K
void remove_temporaries(const std::string& path, pid_t pid)
{
    for (const std::string& p : {path, polylines::target_of(path)})
        for (int k = 0; k < 100; k++) unlink((p + ".tmp" + std::to_string((long)pid) + "_" + std::to_string(k)).c_str());
}

std::string gb(size_t bytes)
{
    char b[32];
    snprintf(b, sizeof b, "%.2f GB", (double)bytes/1e9);
    return b;
}

}  // namespace

// ---- the jobs -------------------------------------------------------------------------------------------------------

Jobs::Jobs(const JobsOptions& options) : options_(options)
{
    limit_ = options_.memory_limit ? options_.memory_limit : (size_t)(0.4*(double)machine_memory());
    if (!runnable(options_.zippers)) {
        error_ = "cannot run zippers: " + (options_.zippers.empty() ? std::string("its place is not known") : options_.zippers);
        return;                              // not ok(): nothing is made
    }
    if (options_.handle_signals) {          // before the folder exists: a signal from here on removes it
        if (g_handling.exchange(true)) {
            error_ = "another Jobs handles the signals already (handle_signals: one at a time)";
            return;
        }
        handling_ = true;
        g_signal = 0;
        for (int k = 0; k < N_SIGNALS; k++) {
            if (sigaction(SIGNALS[k], nullptr, &g_old[k]) != 0 || g_old[k].sa_handler == SIG_IGN) continue;
            struct sigaction sa;
            memset(&sa, 0, sizeof sa);
            sa.sa_handler = on_signal;
            sigemptyset(&sa.sa_mask);
            g_caught[k] = sigaction(SIGNALS[k], &sa, nullptr) == 0;
        }
    }
    const char* t = getenv("TMPDIR");
    std::string root = t && *t ? t : "/tmp";
    while (root.size() > 1 && root.back() == '/') root.pop_back();
    root += "/zippers_jobs_XXXXXX";
    std::vector<char> tmpl(root.begin(), root.end());
    tmpl.push_back(0);
    if (!mkdtemp(tmpl.data())) {
        error_ = std::string("cannot make a temporary folder: ") + strerror(errno);
        restore_signals();
        return;
    }
    folder_ = tmpl.data();
    thread_ = std::thread([this] { run(); });
}

// the signals' earlier dispositions, when this Jobs handles them
void Jobs::restore_signals()
{
    if (!handling_) return;
    for (int k = 0; k < N_SIGNALS; k++)
        if (g_caught[k]) {
            sigaction(SIGNALS[k], &g_old[k], nullptr);
            g_caught[k] = false;
        }
    g_signal = 0;
    handling_ = false;
    g_handling = false;
}

Jobs::~Jobs()
{
    {
        std::lock_guard<std::mutex> lk(mu_);
        quit_ = true;
        pictures_.clear();
        have_curves_request_ = false;
        if (running_) end("quitting");
        wake_.notify_all();
    }
    if (thread_.joinable()) thread_.join();      // it ends when the child has ended and been waited for
    remove_tree(folder_);
    restore_signals();
}

// a request to jobs that are not ok() is answered at once, with the reason
void Jobs::answer_now(const Request& r)
{
    JobResult x;
    x.kind = r.kind;
    x.id = r.id;
    x.outputs = r.outputs;
    x.reason = error_;
    results_.push_back(x);
    results_cv_.notify_all();
}

uint64_t Jobs::request_curves(const std::vector<std::string>& args)
{
    std::lock_guard<std::mutex> lk(mu_);
    Request r;
    r.kind = JobKind::curves;
    r.id = ++next_id_;
    r.args = args;
    if (!ok()) {
        answer_now(r);
        return r.id;
    }
    r.due = Clock::now() + std::chrono::microseconds((long long)(options_.debounce*1e6));
    curves_request_ = r;
    have_curves_request_ = true;
    latest_curves_ = r.id;
    if (running_ && child_.req.kind == JobKind::curves) end("a newer request for curves");
    wake_.notify_all();
    return r.id;
}

uint64_t Jobs::request_picture(const std::vector<std::string>& args, const std::vector<std::string>& outputs)
{
    std::lock_guard<std::mutex> lk(mu_);
    Request r;
    r.kind = JobKind::picture;
    r.id = ++next_id_;
    r.args = args;
    r.outputs = outputs;
    if (!ok()) {
        answer_now(r);
        return r.id;
    }
    pictures_.push_back(r);
    wake_.notify_all();
    return r.id;
}

void Jobs::cancel()
{
    std::lock_guard<std::mutex> lk(mu_);
    have_curves_request_ = false;
    pictures_.clear();
    if (running_) end("cancelled");
    wake_.notify_all();
}

void Jobs::cancel_curves()
{
    std::lock_guard<std::mutex> lk(mu_);
    have_curves_request_ = false;
    if (running_ && child_.req.kind == JobKind::curves) end("cancelled");
    wake_.notify_all();
}

bool Jobs::poll(JobResult& r)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (results_.empty()) return false;
    r = std::move(results_.front());
    results_.pop_front();
    return true;
}

bool Jobs::wait(JobResult& r, double seconds)
{
    std::unique_lock<std::mutex> lk(mu_);
    results_cv_.wait_for(lk, std::chrono::microseconds((long long)(seconds*1e6)), [&] { return !results_.empty(); });
    if (results_.empty()) return false;
    r = std::move(results_.front());
    results_.pop_front();
    return true;
}

JobsState Jobs::state() const
{
    std::lock_guard<std::mutex> lk(mu_);
    JobsState s;
    s.running = running_;
    if (running_) {
        s.kind = child_.req.kind;
        s.id = child_.req.id;
        s.pid = child_.pid;
        s.memory = last_memory_;
        s.stderr_tail = child_.result.stderr_tail;
    }
    s.curves_waiting = have_curves_request_;
    s.pictures_waiting = pictures_.size();
    s.started = started_;
    return s;
}

// the jobs' thread: starts the jobs when they are due, reads the child's standard error, samples its memory, ends it
// when asked or past the limit, waits for it, and makes its result
void Jobs::run()
{
    std::unique_lock<std::mutex> lk(mu_);
    while (true) {
        if (options_.handle_signals && g_signal) interrupted(g_signal);
        if (running_) {
            read_stderr(false);
            int st = 0;
            struct rusage ru;
            memset(&ru, 0, sizeof ru);
            pid_t r = wait4(child_.pid, &st, WNOHANG, &ru);
            if (r == child_.pid || (r < 0 && errno == ECHILD)) {
                read_stderr(true);
                if (r == child_.pid) {               // its true peak: ru_maxrss, in bytes on macOS, kilobytes on Linux
#if defined(__APPLE__)
                    const size_t peak = (size_t)ru.ru_maxrss;
#else
                    const size_t peak = (size_t)ru.ru_maxrss*1024;
#endif
                    if (peak > child_.result.peak_memory) child_.result.peak_memory = peak;
                }
                // finish: the result made; the curves file read with the lock released
                Child c = std::move(child_);
                child_ = Child();
                running_ = false;
                if (c.err_fd >= 0) close(c.err_fd);
                c.result.status = r == c.pid && WIFEXITED(st) ? WEXITSTATUS(st) : -1;
                c.result.signal = r == c.pid && WIFSIGNALED(st) ? WTERMSIG(st) : 0;
                c.result.seconds = since(c.start);
                const bool current = c.req.kind == JobKind::picture || c.req.id == latest_curves_;
                if (c.req.kind == JobKind::curves) {
                    const bool done = c.result.status == 0 || (c.result.status == 3 && c.sphere == 2);
                    if (!c.result.ended && current && done) {
                        lk.unlock();
                        std::string err;
                        c.result.has_curves = polylines::read(c.curves_file, c.result.curves, &err);
                        lk.lock();
                        if (!c.result.has_curves) c.result.read_error = err;
                    } else if (!c.result.ended && !done)
                        c.result.read_error = c.result.signal ? "zippers draw ended by the signal " +
                                                                    std::to_string(c.result.signal)
                                                              : "zippers draw ended with the exit status " +
                                                                    std::to_string(c.result.status);
                    unlink(c.curves_file.c_str());
                    remove_temporaries(c.curves_file, c.pid);
                } else if (c.result.ended || c.result.status != 0) {
                    for (size_t i = 0; i < c.req.outputs.size(); i++) {
                        if (!c.existed[i] && c.result.ended) unlink(polylines::target_of(c.req.outputs[i]).c_str());
                        remove_temporaries(c.req.outputs[i], c.pid);
                    }
                }
                if (current) {
                    results_.push_back(std::move(c.result));
                    results_cv_.notify_all();
                }
                continue;
            }
            if (since(child_.last_sample) >= 0.25) {
                child_.last_sample = Clock::now();
                const size_t m = process_memory(child_.pid);
                last_memory_ = m;
                if (m > child_.result.peak_memory) child_.result.peak_memory = m;
                if (limit_ && m > limit_ && child_.sphere != 2 && !child_.ending)
                    end("its memory, " + gb(m) + ", passed the limit of " + gb(limit_));
            }
            if (child_.ending && !child_.killed && since(child_.term_sent) >= options_.grace) {
                kill(child_.pid, SIGKILL);
                child_.killed = true;
            }
        } else {
            if (quit_) break;
            if (!pictures_.empty()) {
                Request r = pictures_.front();
                pictures_.pop_front();
                start(r);
                continue;
            }
            if (have_curves_request_ && Clock::now() >= curves_request_.due) {
                have_curves_request_ = false;
                start(curves_request_);
                continue;
            }
        }
        wake_.wait_for(lk, std::chrono::milliseconds(running_ ? 20 : 50));
    }
}

// start a job (mu_ held); a job that cannot start gives its result at once
void Jobs::start(Request req)
{
    Child c;
    c.req = req;
    c.sphere = session::sphere_of_example(req.args.empty() ? std::string() : req.args[0]);
    std::vector<std::string> args = req.args;
    if (c.sphere == 2) {                    // the memory limit, as zipper's --memlimit (GB, 0.2 to 64): the smaller of the
        double g = (double)limit_/1e9;      // jobs' limit and one given in the arguments
        for (size_t i = 0; i + 1 < args.size();)
            if (args[i] == "--memlimit") {
                const double given = strtod(args[i + 1].c_str(), nullptr);
                if (given > 0 && given < g) g = given;
                args.erase(args.begin() + (long)i, args.begin() + (long)i + 2);
            } else i++;
        g = g < 0.2 ? 0.2 : g > 64 ? 64 : g;
        args.push_back("--memlimit");
        args.push_back(polylines::exact(g));
    }
    if (req.kind == JobKind::curves) {
        c.curves_file = folder_ + "/" + std::to_string(req.id) + ".zpl";
        args.push_back("--polylines");
        args.push_back(c.curves_file);
    } else                                   // (an output that is a link: whether its target exists)
        for (const std::string& o : req.outputs) c.existed.push_back(exists(polylines::target_of(o)));
    c.result.kind = req.kind;
    c.result.id = req.id;
    c.result.outputs = req.outputs;
    c.result.command = {options_.zippers, "draw"};
    c.result.command.insert(c.result.command.end(), args.begin(), args.end());
    auto refuse = [&](const std::string& why) {
        c.result.reason = why;
        results_.push_back(c.result);
        results_cv_.notify_all();
    };
    if (!error_.empty()) return refuse(error_);
    std::vector<char*> argv;                 // made before the child: the child allocates nothing
    for (std::string& a : c.result.command) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    int fd[2];
    if (pipe(fd) != 0) return refuse(std::string("cannot start zippers: ") + strerror(errno));
    // The child inherits no descriptor but its standard streams: 0 and 1 /dev/null, 2 the pipe. On macOS by posix_spawn
    // with POSIX_SPAWN_CLOEXEC_DEFAULT, which passes on only the descriptors of its file actions, whatever their
    // numbers. On Linux by fork, for PR_SET_PDEATHSIG, then close_range from 3 (or close up to the soft limit, read
    // here, where close_range is missing)
    pid_t pid = -1;
#if defined(__APPLE__)
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, fd[1], 2);
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    sigset_t none, dflt;
    sigemptyset(&none);
    sigemptyset(&dflt);
    for (int k = 0; k < N_SIGNALS; k++)
        if (g_caught[k]) sigaddset(&dflt, SIGNALS[k]);
    posix_spawnattr_setsigmask(&at, &none);
    posix_spawnattr_setsigdefault(&at, &dflt);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    const int rc = posix_spawn(&pid, argv[0], &fa, &at, argv.data(), environ);
    posix_spawnattr_destroy(&at);
    posix_spawn_file_actions_destroy(&fa);
    if (rc != 0) {
        pid = -1;
        errno = rc;
    }
#else
    long maxfd = 1024;
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
        maxfd = rl.rlim_cur == RLIM_INFINITY || rl.rlim_cur > (1u << 20) ? (1L << 20) : (long)rl.rlim_cur;
    const pid_t parent = getpid();
    pid = fork();
    if (pid == 0) {                          // the child: only calls that are safe after fork
        for (int k = 0; k < N_SIGNALS; k++)
            if (g_caught[k]) signal(SIGNALS[k], SIG_DFL);
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
#if defined(__linux__)
        prctl(PR_SET_PDEATHSIG, SIGKILL);   // ends with its parent
        if (getppid() != parent) _exit(1);
#else
        (void)parent;
#endif
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0) {
            dup2(nul, 0);
            dup2(nul, 1);
        }
        dup2(fd[1], 2);
#if defined(__linux__) && defined(SYS_close_range)
        if (syscall(SYS_close_range, 3u, ~0u, 0u) != 0)
#endif
            for (long f = 3; f < maxfd; f++) close((int)f);
        execv(argv[0], argv.data());
        _exit(127);
    }
#endif
    close(fd[1]);
    if (pid < 0) {
        close(fd[0]);
        return refuse(std::string("cannot start zippers: ") + strerror(errno));
    }
    fcntl(fd[0], F_SETFD, FD_CLOEXEC);
    fcntl(fd[0], F_SETFL, O_NONBLOCK);
    c.pid = pid;
    c.err_fd = fd[0];
    c.start = c.last_sample = Clock::now();
    c.result.pid = pid;
    child_ = std::move(c);
    running_ = true;
    last_memory_ = 0;
    started_++;
}

// end the running child (mu_ held): SIGTERM now, SIGKILL after the grace time (run)
void Jobs::end(const std::string& reason)
{
    if (!running_ || child_.ending) return;
    child_.ending = true;
    child_.result.ended = true;
    child_.result.reason = reason;
    child_.term_sent = Clock::now();
    kill(child_.pid, SIGTERM);
}

// the child's standard error, read so far (all: to its end), into its last lines (mu_ held)
void Jobs::read_stderr(bool all)
{
    if (child_.err_fd < 0) return;
    char buf[4096];
    while (true) {
        ssize_t k = read(child_.err_fd, buf, sizeof buf);
        if (k > 0) {
            child_.partial_line.append(buf, (size_t)k);
            size_t nl;
            while ((nl = child_.partial_line.find('\n')) != std::string::npos) {
                child_.result.stderr_tail.push_back(child_.partial_line.substr(0, nl));
                child_.partial_line.erase(0, nl + 1);
            }
            while (child_.result.stderr_tail.size() > options_.stderr_lines)
                child_.result.stderr_tail.erase(child_.result.stderr_tail.begin());
            continue;
        }
        if (k < 0 && errno == EINTR) continue;
        if (k == 0 || all) {                 // the end (or, after the child has ended, all that was there)
            if (!child_.partial_line.empty() && (k == 0 || all)) {
                child_.result.stderr_tail.push_back(child_.partial_line);
                child_.partial_line.clear();
                while (child_.result.stderr_tail.size() > options_.stderr_lines)
                    child_.result.stderr_tail.erase(child_.result.stderr_tail.begin());
            }
        }
        break;
    }
}

// a signal of handle_signals (mu_ held): the child ended at once and waited for, its partial files and the folder
// removed; then the process ends with the signal
void Jobs::interrupted(int sig)
{
    if (running_) {
        kill(child_.pid, SIGKILL);
        int st = 0;
        while (waitpid(child_.pid, &st, 0) < 0 && errno == EINTR) {}
        if (child_.req.kind == JobKind::picture)
            for (size_t i = 0; i < child_.req.outputs.size(); i++) {
                if (!child_.existed[i]) unlink(polylines::target_of(child_.req.outputs[i]).c_str());
                remove_temporaries(child_.req.outputs[i], child_.pid);
            }
        running_ = false;
    }
    remove_tree(folder_);
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
    usleep(200000);                          // (the signal ends the process; this only waits for it)
    _exit(128 + sig);
}

// ---- finding zippers; memory --------------------------------------------------------------------------------------

std::string find_zippers(const DataPlaces& places, const std::string& built, const std::string& installed_relative)
{
    std::string path;
    if (in_build_folder(places)) path = built;
    else {
        const std::string self = own_path();
        if (!self.empty() && !installed_relative.empty())
            path = tidy_path(folder_of(self) + "/" + installed_relative + "/zippers");
    }
    return runnable(path) ? path : std::string();
}

size_t machine_memory()
{
#if defined(__APPLE__)
    uint64_t m = 0;
    size_t len = sizeof m;
    return sysctlbyname("hw.memsize", &m, &len, nullptr, 0) == 0 ? (size_t)m : 0;
#else
    long pages = sysconf(_SC_PHYS_PAGES), page = sysconf(_SC_PAGESIZE);
    return pages > 0 && page > 0 ? (size_t)pages*(size_t)page : 0;
#endif
}

size_t statm_resident(const std::string& statm, long page_size)
{
    unsigned long long size = 0, resident = 0;
    if (page_size <= 0 || sscanf(statm.c_str(), "%llu %llu", &size, &resident) != 2) return 0;
    return (size_t)resident*(size_t)page_size;
}

size_t process_memory(pid_t pid)
{
#if defined(__APPLE__)
    struct rusage_info_v2 ri;
    if (proc_pid_rusage(pid, RUSAGE_INFO_V2, (rusage_info_t*)&ri) != 0) return 0;
    return (size_t)ri.ri_resident_size;
#elif defined(__linux__)
    FILE* f = fopen(("/proc/" + std::to_string((long)pid) + "/statm").c_str(), "r");
    if (!f) return 0;
    char buf[256] = "";
    size_t k = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[k] = 0;
    return statm_resident(buf, sysconf(_SC_PAGESIZE));
#else
    (void)pid;
    return 0;
#endif
}

}  // namespace zippers::gui
