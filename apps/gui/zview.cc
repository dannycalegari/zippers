// SPDX-License-Identifier: GPL-3.0-or-later
/*  zview.cc  --  the program of the window, reached by zippers view.

    usage:  zview EXAMPLE [options] [-o STEM]
            zview --session FILE [options] [-o STEM]

    EXAMPLE and the options are those of zippers draw (fig8 -n 2 -R 6 when none is given), less the outputs and the
    options that only print; -o STEM is the stem of the pictures that p saves (by default the example's name). The
    window shows the curves of zippers draw, which it runs as child processes (model.h, jobs.h).

    THE WINDOW (a build with GLFW: ZIPPERS_WINDOW). A GLFW window with an OpenGL 3.3 core context, drawn by draw.h. Its
    characters go to the model's key(), the arrows, Esc and Tab as <L> <R> <U> <D> <E> <T>; a drag of the left button
    to drag(), in the window's points; the scroll wheel to scroll(), a notch at a time; the window's size, in points,
    to resize(). The loop waits for events at most 0.1 s (glfwWaitEventsTimeout), then takes the jobs' results (poll),
    and draws again only when something changed: no busy loop. The title says the example, R, the zoom, and whether a
    job is running; the messages are on the standard error. On macOS glfwInit does not change the working folder
    (GLFW_COCOA_CHDIR_RESOURCES off), which the data search and the pictures depend on. The window opens after the
    model has checked its options and started the first job. With ZIPPERS_KEYS set (feed.h), its tokens are fed once
    the first curves have come, one every ZIPPERS_KEYDELAY ms (default 500), <wait> until no job runs, <G> grabbing
    the picture area as drawn; zview exits at their end, with status 1 if one was refused.
    ZIPPERS_NO_WINDOW (not in the usage text, for the tests) set to anything but the empty text: zview checks its
    options and exits with status 1 without opening the window.
    WITHOUT GLFW the build has no window: zview checks its options, says so, and exits with status 1.

    --headless (not in the usage text, for the tests) runs the model and its jobs with no window: the first curves are
    computed, then the tokens of ZIPPERS_KEYS are fed to the model's handlers (feed.h), and zview exits at their end (or
    at q or Esc) with status 0, or 1 if a token was refused or the first curves could not be computed. Nothing is drawn.

    zview runs the zippers of its own build when it runs from the folder in which its build puts it, and otherwise
    ../../bin/zippers relative to its own folder (an installed libexec/zippers/zview); never one found in PATH.
*/

#include "feed.h"
#include "model.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef ZIPPERS_WINDOW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "draw.h"
#include "output/png.h"
#endif

#ifndef ZIPPERS_VERSION
#define ZIPPERS_VERSION "unknown"
#endif
#ifndef ZIPPERS_ZIPPERS
#define ZIPPERS_ZIPPERS ""
#endif
#ifndef ZIPPERS_INSTALLED_RELATIVE
#define ZIPPERS_INSTALLED_RELATIVE ""
#endif

namespace {

void usage(FILE* f)
{
    fprintf(f,
"usage: zview EXAMPLE [options] [-o STEM]\n"
"       zview --session FILE [options] [-o STEM]\n"
"  the window on the curves of zippers draw EXAMPLE [options] (fig8 -n 2 -R 6 when no example is given); -o STEM is\n"
"  the stem of the pictures that p saves. It is reached by zippers view."
#ifdef ZIPPERS_WINDOW
" In the window, ? prints the keys.\n"
#else
" This build has no window.\n"
#endif
    );
}

// the options, checked as the model reads them (session.h), with no job started; false, with the reason
bool check(const zippers::gui::ModelConfig& c, std::string& why)
{
    zippers::session::Session s;
    std::vector<std::string> others;
    if (!c.session.empty()) {
        if (!zippers::session::read(c.session, s, &why)) return false;
        if (!zippers::session::apply(s, c.args, others, why)) return false;
    } else if (!zippers::session::from_args(c.args, s, &others, why))
        return false;
    for (size_t i = 0; i < others.size(); i++) {
        if (others[i] == "-o" && i + 1 < others.size()) {
            i++;
            continue;
        }
        why = others[i] + " is not an option of the window (its pictures are saved by p)";
        return false;
    }
    return true;
}


#ifdef ZIPPERS_WINDOW

namespace gui = zippers::gui;

// the window, as its callbacks see it
struct Window {
    gui::Model* m = nullptr;
    GLFWwindow* w = nullptr;
    bool dirty = true;                       // to be drawn again
    bool dragging = false;
    double lx = 0, ly = 0;                   // the cursor at the last drag event, in points
    double notches = 0;                      // the scroll wheel's fractions of a notch
};

Window* of(GLFWwindow* w) { return static_cast<Window*>(glfwGetWindowUserPointer(w)); }

void on_char(GLFWwindow* w, unsigned int c)
{
    if (c < 32 || c > 126) return;           // the keys are ASCII characters
    of(w)->m->key(std::string(1, (char)c));
    of(w)->dirty = true;
}

void on_key(GLFWwindow* w, int key, int, int action, int)
{
    if (action == GLFW_RELEASE) return;
    const char* t = key == GLFW_KEY_LEFT ? "<L>" : key == GLFW_KEY_RIGHT ? "<R>" : key == GLFW_KEY_UP ? "<U>"
                  : key == GLFW_KEY_DOWN ? "<D>" : key == GLFW_KEY_ESCAPE ? "<E>" : key == GLFW_KEY_TAB ? "<T>" : nullptr;
    if (!t) return;
    of(w)->m->key(t);
    of(w)->dirty = true;
}

void on_button(GLFWwindow* w, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    Window* W = of(w);
    W->dragging = action == GLFW_PRESS;
    if (W->dragging) glfwGetCursorPos(w, &W->lx, &W->ly);
}

void on_cursor(GLFWwindow* w, double x, double y)
{
    Window* W = of(w);
    if (!W->dragging) return;
    const double dx = x - W->lx, dy = y - W->ly;
    W->lx = x;
    W->ly = y;
    if (dx == 0 && dy == 0) return;
    W->m->drag(dx, dy);
    W->dirty = true;
}

void on_scroll(GLFWwindow* w, double, double y)
{
    Window* W = of(w);
    W->notches += y;
    const int n = (int)W->notches;           // whole notches (a trackpad gives fractions)
    if (!n) return;
    W->notches -= n;
    W->m->scroll(n);
    W->dirty = true;
}

void on_size(GLFWwindow* w, int width, int height)
{
    of(w)->m->resize(width, height);
    of(w)->dirty = true;
}

void on_redraw(GLFWwindow* w) { of(w)->dirty = true; }
void on_fbsize(GLFWwindow* w, int, int) { of(w)->dirty = true; }

void on_error(int code, const char* what) { fprintf(stderr, "zview: GLFW error %d: %s\n", code, what); }

void* proc(const char* name) { return reinterpret_cast<void*>(glfwGetProcAddress(name)); }

std::string number(double x)
{
    char b[40];
    snprintf(b, sizeof b, "%.4g", x);
    return b;
}

// the title: the example, its sphere, R, the zoom, and the job's state
std::string title(const gui::Model& m, const std::string& example)
{
    std::string t = "zippers view: " + example + " (S" + std::to_string(m.sphere()) + ")";
    if (m.ready()) {
        t += ", R " + number(m.R()) + ", zoom " + number(m.zoom());
        if (m.detail()) t += ", in detail";
    }
    if (!m.ready()) t += ", computing the first curves";
    else if (m.busy()) t += ", computing";
    return t;
}

// the window's loop, until q, Esc, the window closed, or the end of the key feed: the exit status
int window(gui::Model& m, const std::string& example)
{
#if defined(__APPLE__) && defined(GLFW_COCOA_CHDIR_RESOURCES)
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
#endif
    glfwSetErrorCallback(on_error);
    if (!glfwInit()) {
        fprintf(stderr, "zview: GLFW cannot start (no display?)\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    // the picture's size, at most 85% of the screen's work area
    int side = m.size() > 0 ? m.size() : 800;
    if (GLFWmonitor* mon = glfwGetPrimaryMonitor()) {
        int x, y, ww = 0, wh = 0;
        glfwGetMonitorWorkarea(mon, &x, &y, &ww, &wh);
        const int most = (int)(0.85*std::min(ww, wh));
        if (most > 0 && side > most) side = most;
    }
    if (side < 300) side = 300;
    Window W;
    W.m = &m;
    W.w = glfwCreateWindow(side, side, title(m, example).c_str(), nullptr, nullptr);
    if (!W.w) {
        fprintf(stderr, "zview: GLFW cannot open a window with OpenGL 3.3\n");
        glfwTerminate();
        return 1;
    }
    glfwSetWindowUserPointer(W.w, &W);
    glfwMakeContextCurrent(W.w);
    glfwSwapInterval(1);
    int status = 0;
    {
        gui::draw::Renderer r;
        std::string why;
        if (!r.init(proc, why)) {
            fprintf(stderr, "zview: %s\n", why.c_str());
            status = 1;
        } else {
            fprintf(stderr, "zview: OpenGL %s\n", r.version().c_str());
            glfwSetCharCallback(W.w, on_char);
            glfwSetKeyCallback(W.w, on_key);
            glfwSetMouseButtonCallback(W.w, on_button);
            glfwSetCursorPosCallback(W.w, on_cursor);
            glfwSetScrollCallback(W.w, on_scroll);
            glfwSetWindowSizeCallback(W.w, on_size);
            glfwSetFramebufferSizeCallback(W.w, on_fbsize);
            glfwSetWindowRefreshCallback(W.w, on_redraw);
            int ww, wh;
            glfwGetWindowSize(W.w, &ww, &wh);
            m.resize(ww, wh);

            auto frame = [&]() {
                int fw, fh, pw, ph;
                glfwGetFramebufferSize(W.w, &fw, &fh);
                glfwGetWindowSize(W.w, &pw, &ph);
                r.draw(m, fw, fh, pw > 0 ? (double)fw/pw : 1.0);
            };
            // the key feed
            const char* keys = getenv("ZIPPERS_KEYS");
            const bool feeding = keys != nullptr;
            const std::vector<gui::Token> tokens = gui::parse_keys(keys ? keys : "");
            gui::FeedOptions o;
            o.headless = false;
            if (const char* d = getenv("ZIPPERS_KEYDELAY")) o.delay_ms = atoi(d);
            if (o.delay_ms < 0) o.delay_ms = 0;
            gui::FeedState st;
            gui::FeedHooks hooks;
            hooks.resize = [&](int w, int h) { glfwSetWindowSize(W.w, w, h); };
            hooks.grab = [&](const std::string& file, std::string& why) {
                frame();
                std::vector<uint8_t> rgb;
                int n = 0;
                int fw, fh;
                glfwGetFramebufferSize(W.w, &fw, &fh);
                if (!r.grab(fw, fh, rgb, n)) {
                    why = "the picture area cannot be read back";
                    return false;
                }
                glfwSwapBuffers(W.w);
                if (!zippers::png::write_rgb(file.c_str(), rgb, n, n)) {
                    why = "cannot write " + file;
                    return false;
                }
                return true;
            };
            size_t next = 0;
            typedef std::chrono::steady_clock Clock;
            Clock::time_point due = Clock::now();
            std::string shown_title;
            while (!glfwWindowShouldClose(W.w) && !m.quit()) {
                if (m.poll()) W.dirty = true;
                if (feeding && m.ready() && Clock::now() >= due) {
                    if (next == tokens.size()) break;
                    const gui::Token& t = tokens[next];
                    if (t.kind == gui::Token::wait) {
                        if (!m.busy()) next++;
                    } else {
                        if (!o.quiet) fprintf(stderr, "keys: %s\n", t.text.c_str());
                        gui::apply(m, t, o, st, hooks);
                        next++;
                        due = Clock::now() + std::chrono::milliseconds(o.delay_ms*(t.kind == gui::Token::pause ? 2 : 1));
                        W.dirty = true;
                    }
                }
                const std::string t = title(m, example);
                if (t != shown_title) {
                    glfwSetWindowTitle(W.w, t.c_str());
                    shown_title = t;
                }
                if (W.dirty) {
                    W.dirty = false;
                    frame();
                    glfwSwapBuffers(W.w);
                }
                double wait = 0.1;
                if (feeding && m.ready()) {
                    const double left = std::chrono::duration<double>(due - Clock::now()).count();
                    wait = std::max(0.0, std::min(wait, left));
                }
                if (wait > 0) glfwWaitEventsTimeout(wait);
                else glfwPollEvents();
            }
            status = st.status;
        }
    }                                        // (the renderer's buffers freed with the context current)
    glfwDestroyWindow(W.w);
    glfwTerminate();
    return status;
}

#endif

}  // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--headless") headless = true;
        else if (a == "-h" || a == "--help") {
            usage(stdout);
            return 0;
        } else args.push_back(a);
    }
    zippers::gui::ModelConfig c;
    if (args.empty()) args = {"fig8", "-n", "2", "-R", "6"};
    if (args[0] == "--session") {
        if (args.size() < 2) {
            fprintf(stderr, "zview: --session needs a session file\n");
            return 1;
        }
        c.session = args[1];
        c.args.assign(args.begin() + 2, args.end());
    } else c.args = args;
    std::string why;
    if (!check(c, why)) {
        fprintf(stderr, "zview: %s\n", why.c_str());
        return 1;
    }
#ifdef ZIPPERS_WINDOW
    if (!headless) {
        const char* no = getenv("ZIPPERS_NO_WINDOW");
        if (no && *no) {
            fprintf(stderr, "zview: ZIPPERS_NO_WINDOW is set: the window is not opened\n");
            return 1;
        }
    }
#else
    if (!headless) {
        fprintf(stderr, "zview: this build has no window; zippers draw draws pictures\n");
        return 1;
    }
#endif
    c.version = ZIPPERS_VERSION;
    c.places = ZIPPERS_DATA_PLACES;
    c.jobs.zippers = zippers::gui::find_zippers(c.places, ZIPPERS_ZIPPERS, ZIPPERS_INSTALLED_RELATIVE);
    c.jobs.handle_signals = true;
    if (c.jobs.zippers.empty()) {
        fprintf(stderr, "zview: cannot find the zippers of this build or installation\n");
        return 1;
    }
    zippers::gui::Model m(c);
    if (!m.ok()) {
        fprintf(stderr, "zview: %s\n", m.error().c_str());
        return 1;
    }
#ifdef ZIPPERS_WINDOW
    if (!headless) {
        std::string example = c.session.empty() ? c.args[0] : c.session;
        const size_t sl = example.find_last_of('/');
        if (sl != std::string::npos) example = example.substr(sl + 1);
        return window(m, example);
    }
#endif
    while (!m.idle(3600)) {}
    if (!m.ready()) {
        fprintf(stderr, "zview: the first curves could not be computed\n");
        return 1;
    }
    zippers::gui::FeedOptions o;
    if (const char* d = getenv("ZIPPERS_KEYDELAY")) o.delay_ms = atoi(d);
    const char* keys = getenv("ZIPPERS_KEYS");
    return zippers::gui::feed(m, zippers::gui::parse_keys(keys ? keys : ""), o);
}
