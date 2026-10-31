// SPDX-License-Identifier: GPL-3.0-or-later
//
// jobs.h: a window's jobs: zippers draw run as child processes, one at a time, in the window's working folder and
// environment, so that every set of curves a window shows and every picture it saves is a zippers draw command's
// output. No OpenGL, no GLFW. namespace zippers::gui.
//
// THE JOBS.
//   - A CURVES job runs `zippers draw ARGS --polylines FOLDER/N.zpl`, FOLDER the jobs' own temporary folder and N the
//     request's number. A new request for curves ends a running curves job, and starts DEBOUNCE seconds (0.3) after the
//     last request, so that a quick run of requests starts one job. When it finishes (exit status 0, or 3 in S^2: the
//     curves are incomplete), its file is read, in the jobs' thread, into a polylines::File, and removed; a curves job
//     that finished for a request that is no longer the latest is discarded.
//   - A PICTURE job runs `zippers draw ARGS`, ARGS with their -o (and the outputs it writes named to the jobs). It is
//     ended only by cancel() and by quitting (the destructor); its partial files are then removed: the outputs that did
//     not exist when it started (for an output that is a symbolic link, its target), and the temporary names of the
//     polyline writer beside them (FILE.tmpPID_K).
//   - AT MOST ONE CHILD AT A TIME. Pictures wait for a running job; a request for curves waits for a running picture
//     job, and for the pictures asked for before it starts.
//   - THE CHILD: its standard input and output are /dev/null; its standard error is read, and its last lines kept. It
//     inherits no other descriptor of its parent, whatever its number (macOS: posix_spawn with
//     POSIX_SPAWN_CLOEXEC_DEFAULT; Linux: fork, then close_range, or closing up to the soft limit). In it the signals
//     that the jobs catch are at their defaults, those its parent ignores stay ignored, and none is blocked; its
//     working folder and its environment are its parent's. Its resident memory is sampled 4 times a second
//     (proc_pid_rusage on macOS, /proc/PID/statm on Linux), and its peak is also taken from wait4's ru_maxrss when it
//     ends, so that a job shorter than a sample has one.
//   - THE MEMORY LIMIT: by default 40% of the machine's memory. An S^2 job gets it as zipper's --memlimit (in GB, from
//     0.2 to 64; the smaller of it and a --memlimit in the job's arguments), so that zipper stops drawing and keeps
//     what it has (exit status 3; such curves can differ from run to run, since zipper's guard reads the process's
//     resident memory); an S^3 or S^4 job whose memory passes it is ended, and the reason kept.
//   - ENDING A CHILD: SIGTERM, then SIGKILL after GRACE seconds (0.5); it is always waited for (wait4), so no zombie is
//     left. On Linux the child is given PR_SET_PDEATHSIG (SIGKILL), so that it ends if its parent dies.
//   - THE FOLDER (mkdtemp, under TMPDIR, else /tmp) is removed by the destructor; with handle_signals also on SIGINT,
//     SIGTERM and SIGHUP (those not ignored when the jobs start): the child is ended, the partial files and the folder
//     removed, and the process then ends with the signal. Without handle_signals a signal that ends the process leaves
//     the folder (and the child runs to its end, but on Linux, where it ends with its parent). The signals'
//     dispositions belong to the process, so ONE Jobs AT A TIME may handle them: a second one with handle_signals,
//     while the first lives, is refused (not ok()); the earlier dispositions are restored when the one that handles
//     them ends.
//   - NOT ok(): when zippers cannot run (it is not a regular file that may be executed: a folder of the name is not),
//     or another Jobs handles the signals, or the folder cannot be made. Then nothing is made (no folder, no thread, no
//     handler), error() says why, and every request is answered at once with that reason.
//
//   Jobs             the jobs of one window: request_curves, request_picture, cancel, cancel_curves, poll / wait, state
//   JobsOptions      zippers' path, the memory limit, the debounce and grace times, the lines of standard error kept,
//                    whether to handle the signals
//   JobResult        a finished job: its command, exit status or signal, whether the jobs ended it and why, the last
//                    lines of its standard error, its peak memory, its curves (a curves job)
//   find_zippers     the zippers of a program of the suite: the build's, or the installed one beside it; never PATH
//   machine_memory   the machine's memory; process_memory a process's resident memory; statm_resident the resident
//                    size in the text of /proc/PID/statm

#pragma once

#include "core/data_files.h"
#include "output/polylines.h"

#include <sys/types.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace zippers::gui {

enum class JobKind { curves, picture };

struct JobResult {
    JobKind kind = JobKind::curves;
    uint64_t id = 0;                         // the request's number
    std::vector<std::string> command;        // the command run: the path of zippers, draw, then the arguments
    pid_t pid = -1;
    int status = -1;                         // the exit status; -1 if it ended by a signal, or did not start
    int signal = 0;                          // the signal that ended it, 0 if none
    bool ended = false;                      // ended by the jobs: a cancel, a newer request for curves, the memory limit,
                                             // or quitting
    std::string reason;                      // why it was ended, or why it could not start
    std::vector<std::string> stderr_tail;    // the last lines of its standard error
    size_t peak_memory = 0;                  // the largest resident memory sampled, in bytes
    double seconds = 0;                      // from its start to its end
    bool has_curves = false;                 // a curves job: its file was read
    polylines::File curves;
    std::string read_error;                  // why it was not (an exit status, the reader's message)
    std::vector<std::string> outputs;        // a picture job: the files it writes
};

struct JobsOptions {
    std::string zippers;                     // the path of zippers (find_zippers)
    size_t memory_limit = 0;                 // in bytes; 0: 40% of the machine's memory
    double debounce = 0.3;                   // seconds: a request for curves starts this long after the last one
    double grace = 0.5;                      // seconds from SIGTERM to SIGKILL
    size_t stderr_lines = 20;                // the lines of a child's standard error that are kept
    bool handle_signals = false;             // catch SIGINT, SIGTERM and SIGHUP (see the top of the file)
};

struct JobsState {
    bool running = false;                    // a child runs
    JobKind kind = JobKind::curves;
    uint64_t id = 0;
    pid_t pid = -1;
    size_t memory = 0;                       // its last sample, in bytes
    std::vector<std::string> stderr_tail;
    bool curves_waiting = false;             // a request for curves waits
    size_t pictures_waiting = 0;
    uint64_t started = 0;                    // the jobs started so far
};

class Jobs {
public:
    explicit Jobs(const JobsOptions& options);
    ~Jobs();                                 // quits: ends the running job, removes its partial files and the folder
    Jobs(const Jobs&) = delete;
    Jobs& operator=(const Jobs&) = delete;

    bool ok() const { return !folder_.empty(); }   // whether the jobs can run (see NOT ok() above; error() says why)
    const std::string& error() const { return error_; }
    const std::string& folder() const { return folder_; }
    size_t memory_limit() const { return limit_; }

    // ARGS: EXAMPLE OPTIONS..., as for zippers draw. Each returns the request's number.
    uint64_t request_curves(const std::vector<std::string>& args);
    uint64_t request_picture(const std::vector<std::string>& args, const std::vector<std::string>& outputs);
    void cancel();                           // ends the running job and drops the waiting requests
    void cancel_curves();                    // ends a running curves job and drops a waiting request for curves; the
                                             // pictures are left as they are
    bool poll(JobResult& r);                 // the next result, if there is one
    bool wait(JobResult& r, double seconds); // the same, waiting up to `seconds` for it
    JobsState state() const;

private:
    struct Request {
        JobKind kind = JobKind::curves;
        uint64_t id = 0;
        std::vector<std::string> args, outputs;
        std::chrono::steady_clock::time_point due;
    };
    struct Child {
        Request req;
        JobResult result;
        pid_t pid = -1;
        int err_fd = -1;
        std::string partial_line;
        std::vector<bool> existed;           // a picture's outputs: whether each existed when it started
        std::chrono::steady_clock::time_point start, last_sample, term_sent;
        bool ending = false, killed = false;
        int sphere = 0;
        std::string curves_file;
    };

    void run();                              // the jobs' thread
    void start(Request req);                 // with mu_ held
    void end(const std::string& reason);     // with mu_ held: SIGTERM now, SIGKILL after the grace time
    void read_stderr(bool all);              // with mu_ held
    void interrupted(int sig);               // with mu_ held: a signal of handle_signals
    void restore_signals();                  // the earlier dispositions, if this Jobs handles the signals
    void answer_now(const Request& r);       // with mu_ held: the answer of jobs that are not ok()

    JobsOptions options_;
    bool handling_ = false;                  // this Jobs handles the signals
    size_t limit_ = 0;
    std::string folder_, error_;
    mutable std::mutex mu_;
    std::condition_variable wake_, results_cv_;
    std::thread thread_;
    bool quit_ = false;
    uint64_t next_id_ = 0, latest_curves_ = 0, started_ = 0;
    bool have_curves_request_ = false;
    Request curves_request_;
    std::deque<Request> pictures_;
    bool running_ = false;
    Child child_;
    size_t last_memory_ = 0;
    std::deque<JobResult> results_;
};

// the zippers of a program of the suite: `built` (the build's zippers) when the program runs from the folder of its
// build (places: core/data_files.h), else the program's folder + "/" + installed_relative + "/zippers" (an installed
// program in libexec/zippers/ finds bin/zippers); "" when it is not there, or is not a regular file that may be
// executed (a folder of the name). Never PATH.
std::string find_zippers(const DataPlaces& places, const std::string& built, const std::string& installed_relative);

size_t machine_memory();                     // the machine's memory, in bytes (0 if it cannot be told)
size_t process_memory(pid_t pid);            // a process's resident memory, in bytes (0 if it cannot be read)
size_t statm_resident(const std::string& statm, long page_size);   // from the text of /proc/PID/statm

}  // namespace zippers::gui
