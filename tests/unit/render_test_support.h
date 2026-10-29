// SPDX-License-Identifier: GPL-3.0-or-later
//
// render_test_support.h: what test_render_cs.cc and test_render_imm.cc share: the tally of comparisons by
// section, child processes for the paths that exit, the capture of stderr, a temporary folder, the masking of the time
// in the lines of build, and variants of substitution files for the reader. Not part of the library; it includes no
// legacy code, so each test includes its legacy sources first and this after them.

#pragma once

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace rtest {

// ---- the tally ------------------------------------------------------------------------------------------------------

inline const char* sec_name = nullptr;
inline long sec_n = 0, sec_bad = 0, all_n = 0, all_bad = 0;
inline std::string sec_notes;              // printed under the section's line
inline double sec_t0 = 0;

inline double clock_seconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + 1e-9*ts.tv_nsec;
}

// the peak resident memory of the process so far, in MB (macOS reports bytes, Linux kilobytes)
inline double peak_mb()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss/1e6;
#else
    return ru.ru_maxrss/1e3;
#endif
}

inline void end_section()
{
    if (sec_name) printf("  %-66s %9ld compared, %ld differ  (%.2f s, peak %.0f MB)\n%s", sec_name, sec_n, sec_bad,
                         clock_seconds() - sec_t0, peak_mb(), sec_notes.c_str());
    fflush(stdout);
}
inline void section(const char* name)
{
    end_section();
    sec_name = name; sec_n = sec_bad = 0; sec_notes.clear();
    sec_t0 = clock_seconds();
}

inline void note(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
inline void note(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sec_notes += std::string("      ") + buf + "\n";
}

inline bool check(bool ok, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
inline bool check(bool ok, const char* fmt, ...)
{
    sec_n++; all_n++;
    if (ok) return true;
    sec_bad++; all_bad++;
    if (all_bad > 40) return false;
    va_list ap;
    va_start(ap, fmt);
    printf("    DIFFERS: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    fflush(stdout);
    return false;
}

template <class X, class Y>
inline bool same_bytes(const X& a, const Y& b)
{
    static_assert(sizeof(X) == sizeof(Y), "the two types must have the same size");
    return !memcmp(&a, &b, sizeof(X));
}

// two vectors of trivially copyable elements of the same size, element by element as bytes
template <class X, class Y>
inline bool same_vector_bytes(const std::vector<X>& a, const std::vector<Y>& b)
{
    static_assert(sizeof(X) == sizeof(Y), "the two element types must have the same size");
    return a.size() == b.size() && (a.empty() || !memcmp(a.data(), b.data(), a.size()*sizeof(X)));
}

// ---- random numbers -------------------------------------------------------------------------------------------------

inline std::mt19937_64 rng(20261025);
inline int64_t rnd(int64_t lo, int64_t hi) { return std::uniform_int_distribution<int64_t>(lo, hi)(rng); }
inline double uniform(double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); }
inline double gauss() { return std::normal_distribution<double>(0, 1)(rng); }

// ---- child processes and stderr -------------------------------------------------------------------------------------

// run f in a child process: its exit status (or -signal), and what it wrote to stderr
inline int in_child(const std::function<void()>& f, std::string& err)
{
    int fd[2];
    if (pipe(fd) != 0) { perror("pipe"); return -1000; }
    fflush(stdout); fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1000; }
    if (pid == 0) {
        dup2(fd[1], 2);
        close(fd[0]); close(fd[1]);
        f();
        fflush(stderr);
        _exit(0);                         // f returned
    }
    close(fd[1]);
    err.clear();
    char buf[512];
    ssize_t k;
    while ((k = read(fd[0], buf, sizeof buf)) > 0) err.append(buf, (size_t)k);
    close(fd[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return -WTERMSIG(st);
    return -1001;
}

// the folder for temporary files: $TMPDIR, or /tmp
inline std::string tmp_root()
{
    const char* t = getenv("TMPDIR");
    std::string s = t && *t ? t : "/tmp";
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

// run f with stderr going to a temporary file; what f wrote there
inline std::string capture_stderr(const std::function<void()>& f)
{
    std::string path = tmp_root() + "/zippers_render_test_XXXXXX";
    std::vector<char> p(path.begin(), path.end());
    p.push_back(0);
    int fd = mkstemp(p.data());
    if (fd < 0) { perror("mkstemp"); exit(2); }
    fflush(stderr);
    int saved = dup(2);
    dup2(fd, 2);
    f();
    fflush(stderr);
    dup2(saved, 2);
    close(saved);
    std::string out;
    lseek(fd, 0, SEEK_SET);
    char buf[4096];
    ssize_t k;
    while ((k = read(fd, buf, sizeof buf)) > 0) out.append(buf, (size_t)k);
    close(fd);
    unlink(p.data());
    return out;
}

// the text with the time of "built in 0.25s" replaced by <time> (as tests/run_references.py masks it)
inline std::string mask_times(const std::string& s)
{
    std::string out;
    size_t i = 0;
    const std::string key = "built in ";
    while (true) {
        size_t p = s.find(key, i);
        if (p == std::string::npos) { out += s.substr(i); break; }
        p += key.size();
        out += s.substr(i, p - i);
        size_t q = p;
        while (q < s.size() && (isdigit((unsigned char)s[q]) || s[q] == '.')) q++;
        out += q > p ? "<time>" : "";
        i = q;
    }
    return out;
}

inline std::string chomp(const std::string& s) { return s.size() && s.back() == '\n' ? s.substr(0, s.size() - 1) : s; }

// ---- files ----------------------------------------------------------------------------------------------------------

struct TempDir {
    std::string path;
    TempDir()
    {
        std::string t = tmp_root() + "/zippers_render_test_dir_XXXXXX";
        std::vector<char> p(t.begin(), t.end());
        p.push_back(0);
        if (!mkdtemp(p.data())) { perror("mkdtemp"); exit(2); }
        path = p.data();
    }
    ~TempDir() { std::string cmd = "rm -rf '" + path + "'"; if (system(cmd.c_str()) != 0) fprintf(stderr, "could not remove %s\n", path.c_str()); }
    std::string file(const std::string& name) const { return path + "/" + name; }
};

inline std::string read_file(const std::string& fn)
{
    std::ifstream in(fn, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline bool write_file(const std::string& fn, const std::string& text)
{
    FILE* f = fopen(fn.c_str(), "wb");
    if (!f) return false;
    fwrite(text.data(), 1, text.size(), f);
    return fclose(f) == 0;
}

inline std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> v;
    std::string line;
    std::istringstream is(text);
    while (std::getline(is, line)) v.push_back(line);
    return v;
}

inline std::string join_lines(const std::vector<std::string>& v)
{
    std::string s;
    for (const std::string& x : v) s += x + "\n";
    return s;
}

// ---- variants of a substitution file, for the reader ----------------------------------------------------------------
// Every variant keeps the line numbers j of the up and down sections in the range 0..n-1 of the S line before them, so
// that the legacy readers (which do not check j) stay defined; otherwise anything goes: comments, blank lines, white
// space, lost and repeated lines and headers, a moved or missing member section, truncations, a second t.

inline size_t find_header(const std::vector<std::string>& L, const std::string& h)
{
    for (size_t i = 0; i < L.size(); i++) {
        std::istringstream is(L[i]);
        std::string w;
        is >> w;
        if (w == h) return i;
    }
    return std::string::npos;
}

inline std::vector<std::pair<std::string, std::string>> subst_variants(const std::string& text)
{
    std::vector<std::pair<std::string, std::string>> out;
    const std::vector<std::string> L = split_lines(text);
    auto add = [&](const std::string& name, const std::vector<std::string>& v) { out.push_back({name, join_lines(v)}); };
    out.push_back({"as stored", text});
    out.push_back({"without its last newline", text.size() ? text.substr(0, text.size() - 1) : text});
    {   // comments and blank lines anywhere
        std::vector<std::string> v;
        for (const std::string& x : L) {
            if (rnd(0, 3) == 0) v.push_back(rnd(0, 1) ? "" : "# a comment, t S up down member 1 2 3");
            v.push_back(x);
        }
        add("with comments and blank lines", v);
    }
    {   // white space: tabs, several blanks, trailing blanks, carriage returns
        std::vector<std::string> v;
        for (std::string x : L) {
            std::string y;
            for (char c : x) y += c == ' ' ? (rnd(0, 2) ? std::string("  ") : std::string("\t")) : std::string(1, c);
            v.push_back(y + (rnd(0, 1) ? " " : "") + (rnd(0, 4) == 0 ? "\r" : ""));
        }
        add("with other white space", v);
    }
    {   // an indented comment: not skipped (line[0] is not '#')
        std::vector<std::string> v = L;
        size_t i = find_header(L, "S");
        if (i != std::string::npos && i + 2 < v.size()) v.insert(v.begin() + (long)i + 2, "   # indented");
        add("with an indented comment among the letters", v);
    }
    size_t it = find_header(L, "t"), is_ = find_header(L, "S"), iu = find_header(L, "up"), id = find_header(L, "down"),
           im = find_header(L, "member");
    if (it != std::string::npos) {
        std::vector<std::string> v = L;
        v.erase(v.begin() + (long)it);
        add("without t", v);
        v = L;
        v.push_back("t 1");
        add("with a second t at the end", v);
        v = L;
        v[it] = "t";
        add("with an empty t", v);
    }
    if (is_ != std::string::npos) {
        std::vector<std::string> v = L;
        std::istringstream ss(L[is_]);
        std::string h;
        size_t n = 0;
        ss >> h >> n;
        v[is_] = "S " + std::to_string(n + 1);
        add("with S one larger", v);
        if (is_ + 3 < L.size()) {
            v = L;
            v.insert(v.begin() + (long)is_ + 3, L[is_ + 2]);
            add("with a letter repeated", v);
            v = L;
            v.erase(v.begin() + (long)is_ + 2);
            add("without a letter", v);
            v = L;
            v[is_ + 1] = "0";
            add("with a letter with no fields", v);
            v = L;
            v[is_ + 2] += "   1   1";
            add("with a longer word", v);
        }
    }
    if (iu != std::string::npos && id != std::string::npos && id > iu + 2) {
        std::vector<std::string> v = L;
        std::swap(v[iu + 1], v[iu + 2]);
        add("with two roofs in the other order", v);
        v = L;
        v.erase(v.begin() + (long)id);
        add("without the down header", v);
        v = L;
        v.erase(v.begin() + (long)iu);
        add("without the up header", v);
        v = L;
        v[iu + 1] += " 3";
        add("with a longer roof", v);
        v = L;
        v.insert(v.begin() + (long)iu + 1, "up");
        add("with up twice", v);
    }
    if (im != std::string::npos) {
        std::vector<std::string> v(L.begin(), L.begin() + (long)im);
        add("without the member section", v);
        v = L;
        v.erase(v.begin() + (long)im);
        add("without the member header (its lines read as roofs)", v);
        if (iu != std::string::npos) {
            std::vector<std::string> w(L.begin(), L.begin() + (long)iu);
            w.insert(w.end(), L.begin() + (long)im, L.end());
            w.insert(w.end(), L.begin() + (long)iu, L.begin() + (long)im);
            add("with the member section before up", w);
        }
    } else {
        std::vector<std::string> v = L;
        v.push_back("member  # a_i t^-phi(a_i) = product of the listed s's");
        v.push_back("a1:");
        v.push_back("a2: 3 4 5");
        v.push_back("a3: 2");
        add("with a member section added", v);
    }
    for (int k = 0; k < 6; k++) {          // truncated after a random line (after the S line, so that j stays in range)
        size_t from = is_ != std::string::npos ? is_ + 1 : 0;
        if (from >= L.size()) break;
        size_t cut = (size_t)rnd((int64_t)from, (int64_t)L.size() - 1);
        std::vector<std::string> v(L.begin(), L.begin() + (long)cut);
        add("truncated after line " + std::to_string(cut), v);
    }
    add("empty", {});
    return out;
}

}  // namespace rtest
