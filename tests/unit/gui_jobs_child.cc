// SPDX-License-Identifier: GPL-3.0-or-later
//
// gui_jobs_child: a stand-in for zippers in test_gui_jobs. It writes on its standard error what it got from the jobs
// that started it, one line each, then exits 0 (its arguments are ignored):
//   fds: 0 1 2                      the descriptors open in it, up to its soft limit (at most 65536)
//   null: yes                       whether its standard input and output are /dev/null
//   signals: INT=default ...        the dispositions of SIGINT, SIGTERM, SIGHUP and SIGUSR2: default, ignored or caught
//   mask: none                      the signals blocked in it ("none", or their names)
//   cwd: FOLDER                     its working folder
//   env: ZIPPERS_DATA=V TMPDIR=V    two variables of its environment ("(unset)" when not set)

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

struct Name { int sig; const char* name; };
const Name NAMES[] = {{SIGHUP, "HUP"}, {SIGINT, "INT"}, {SIGQUIT, "QUIT"}, {SIGILL, "ILL"}, {SIGTRAP, "TRAP"},
                      {SIGABRT, "ABRT"}, {SIGFPE, "FPE"}, {SIGBUS, "BUS"}, {SIGSEGV, "SEGV"}, {SIGSYS, "SYS"},
                      {SIGPIPE, "PIPE"}, {SIGALRM, "ALRM"}, {SIGTERM, "TERM"}, {SIGURG, "URG"}, {SIGTSTP, "TSTP"},
                      {SIGCONT, "CONT"}, {SIGCHLD, "CHLD"}, {SIGTTIN, "TTIN"}, {SIGTTOU, "TTOU"}, {SIGIO, "IO"},
                      {SIGXCPU, "XCPU"}, {SIGXFSZ, "XFSZ"}, {SIGVTALRM, "VTALRM"}, {SIGPROF, "PROF"},
                      {SIGWINCH, "WINCH"}, {SIGUSR1, "USR1"}, {SIGUSR2, "USR2"}};

std::string name_of(int sig)
{
    for (const Name& n : NAMES)
        if (n.sig == sig) return n.name;
    return std::to_string(sig);
}

std::string disposition(int sig)
{
    struct sigaction sa;
    if (sigaction(sig, nullptr, &sa) != 0) return "?";
    return sa.sa_handler == SIG_DFL ? "default" : sa.sa_handler == SIG_IGN ? "ignored" : "caught";
}

std::string variable(const char* name)
{
    const char* v = getenv(name);
    return std::string(name) + "=" + (v ? v : "(unset)");
}

}  // namespace

int main()
{
    long max = 4096;
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) max = (long)rl.rlim_cur;
    if (max > 65536) max = 65536;
    std::string fds = "fds:";
    for (long f = 0; f < max; f++)
        if (fcntl((int)f, F_GETFD) != -1) fds += " " + std::to_string(f);
    struct stat a, b, n;
    const bool null = fstat(0, &a) == 0 && fstat(1, &b) == 0 && stat("/dev/null", &n) == 0 && a.st_rdev == n.st_rdev &&
                      b.st_rdev == n.st_rdev && S_ISCHR(a.st_mode) && S_ISCHR(b.st_mode);
    std::string sigs = "signals:";
    for (int s : {SIGINT, SIGTERM, SIGHUP, SIGUSR2}) sigs += " " + name_of(s) + "=" + disposition(s);
    sigset_t blocked;
    sigemptyset(&blocked);
    sigprocmask(SIG_BLOCK, nullptr, &blocked);
    std::string mask;
    for (int s = 1; s < NSIG; s++)
        if (sigismember(&blocked, s) == 1) mask += " " + name_of(s);
    char cwd[PATH_MAX];
    fprintf(stderr, "%s\nnull: %s\n%s\nmask:%s\ncwd: %s\nenv: %s %s\n", fds.c_str(), null ? "yes" : "no", sigs.c_str(),
            mask.empty() ? " none" : mask.c_str(), getcwd(cwd, sizeof cwd) ? cwd : "?", variable("ZIPPERS_DATA").c_str(),
            variable("TMPDIR").c_str());
    return 0;
}
