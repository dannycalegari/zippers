// SPDX-License-Identifier: GPL-3.0-or-later
//
// gui_jobs_probe ZIPPERS ARGS...: jobs (apps/gui/jobs.h) that handle SIGINT, SIGTERM and SIGHUP, with one request for
// curves, zippers draw ARGS. When its child runs it prints "FOLDER PID" (the jobs' folder, the child's process) on one
// line, then waits to be ended by a signal: test_gui_jobs sends one, and checks that the folder is removed, the child
// ended, and the probe ended by that signal. It ends by itself after 60 s (exit status 3), so that it never outlives
// its test.

#include "jobs.h"

#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: gui_jobs_probe ZIPPERS ARGS...\n");
        return 2;
    }
    zippers::gui::JobsOptions o;
    o.zippers = argv[1];
    o.handle_signals = true;
    o.debounce = 0;
    zippers::gui::Jobs jobs(o);
    if (!jobs.ok()) {
        fprintf(stderr, "%s\n", jobs.error().c_str());
        return 2;
    }
    jobs.request_curves(std::vector<std::string>(argv + 2, argv + argc));
    bool told = false;
    for (int i = 0; i < 600; i++) {
        zippers::gui::JobsState s = jobs.state();
        if (s.running && !told) {
            printf("%s %ld\n", jobs.folder().c_str(), (long)s.pid);
            fflush(stdout);
            told = true;
        }
        usleep(100000);
    }
    return 3;
}
