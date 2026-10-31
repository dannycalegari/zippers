// SPDX-License-Identifier: GPL-3.0-or-later
//
// model.h: the window's model: its state and the handlers of its keys and its mouse, with no OpenGL and no GLFW, so
// that they are the same with a window and without one (zview --headless, and the tests). namespace zippers::gui.
//
// THE CURVES come from zippers draw, run by the jobs (jobs.h): a curves job, `zippers draw EXAMPLE OPTIONS --polylines
// FILE`, for the whole sphere (S^2 with --nocull; S^3 and S^4 draw everything without -o), with the options of the
// command line but culling and the focus ball (--cull, --cullk, --zoomacc, --focus), which a view of the whole sphere
// does not take. The first curves give the START VIEW: the view that the program computed from the options, in the
// header of its polyline file (S^2: view and zoom; S^3: viewmatrix, zoom and clip; S^4: also turn, turn2 and hidfog).
// Until they come, the keys that change the view say so and do nothing.
//
// THE STATE.
//   - the example and its options (a session of zippers draw: session.h), the stem of the pictures (-o; by default the
//     example's name), the picture's size (-s);
//   - R and the accuracy asked for; the curves shown (whole, or in detail for the view: f) and the R they were computed
//     at; in S^3 and S^4 the shells (cs::shell_radii(5.5), imm::shell_radii(2.5));
//   - the layers shown (c), with the programs' colours or those of --colour (the session's colour keys);
//   - the view. S^2: the rotation gview and the zoom. S^3: the frame Q of R^4, the rotation of R^3, the zoom and the clip
//     radius (s3::ViewState). S^4: the frame of R^5, the rotation, zoom and clip, the turns turn and turn2 of the hidden
//     direction (U is turn4(I, 2, 3, turn) then turn4(U, 0, 3, turn2), as zipper5 makes it) and the hidden fog;
//   - S^3 with --pairs: the pair shown and its context (the zippers faintly behind it);
//   - the jobs: the curves job asked for and the pictures being saved; the messages (each also printed on the standard
//     error unless quiet).
//
// THE KEYS (key(token): one character, or <L> <R> <U> <D> the arrows, <E> Esc, <T> Tab). Each has its meaning in each
// geometry; a key that does nothing in the geometry shown says so.
//   arrows   S^2: gview <- roty(-+6) gview (left, right), rotx(-+6) gview (up, down), as the S^2 program's window.
//            S^3: move_pole(Q, rot^T s, 4 degrees), s the screen's -x, +x, +y, -y, as the S^3 program's window.
//            S^4: the same with move_pole5_toward(Q, U^T (rot^T s, 0), 4 degrees).
//   w s      S^3, S^4: the pole toward the screen's +z, -z (toward, away from the viewer).
//   + = - _  S^2: R + 0.5, checked at once against the header's maxR (maxR.wheel with the wheel shown); R - 0.5, at
//            least 1. Each computes new curves. S^3, S^4: the next shell above R + 1e-4, plus 1e-6; the largest below
//            R - 1e-4, plus 1e-6, or 0 (as the S^3 window). Up to the R of the curves shown, the curves of the new R are
//            their first polylines (the polylines come in the order of their roots, nearest first), so no job runs;
//            beyond it, or in detail (with the focus ball the roots are not in that order), a curves job.
//   ] [      the accuracy / 1.5, * 1.5: new curves.
//   z Z x X  the zoom * 1.5, / 1.5, kept in [0.1, 1e7]; the scroll wheel the same, a notch at a time.
//   c C      S^2: zippers -> zippers and the wheel -> the wheel -> zippers (the wheel's first showing computes the
//            curves with -C, checked at once against maxR.wheel). S^3, S^4: both -> Z+ -> Z- -> both.
//   b        S^3, S^4: the clip radius * 2 while it is below 8, else 1.5.
//   r        the start view again (S^3 with pairs: the pair's view).
//   n N l    S^3 with --pairs: the next and the previous pair (its view: the frame of its pole, its clip radius, zoom
//            1; the rotation kept), its line printed; l the zippers faintly behind it.
//   h H j J  S^4: turn2 +-5 degrees (the plane of the horizontal axis and the hidden one), turn +-5 (the depth axis and
//            the hidden one).
//   p P      save the picture of the view: a picture job `zippers draw --session NAME.zsession -o NAME.png`, the
//            session written from the state first (session.h), NAME = s2::export_stem(stem, {.png, .zsession}), so that
//            neither file is overwritten; when it is written, the command that remakes it and, zoomed, the command of
//            the full detail are printed. S^2's picture frames the window's square (s2::export_framing) at the angles
//            of s2::command_angles; S^3 and S^4 give the view as --viewmatrix.
//   f        the curves of this view in detail: the picture's command, culled to the view, at the accuracy on the
//            screen (S^2: -a divided by the zoom, down to 1e-6; S^3, S^4: --cull --zoomacc, and --focus 1 when zoomed),
//            as a curves job; f again, or any change of the view, returns to the whole picture, kept meanwhile.
//   ?        the keys. Tab: nothing (this window has no panel). q Esc: quit.
// THE MOUSE: drag(dx, dy), a drag of the left button: S^2 gview <- roty(0.4 dx) gview, then rotx(0.4 dy) gview; S^3, S^4
// the rotation of R^3 likewise (as the S^3 window). scroll(n): n notches of the zoom. resize(w, h): the window's size.
//
// THE JOBS' MEMORY LIMIT is the jobs' default, 40% of the machine's memory (jobs.h).

#pragma once

#include "jobs.h"

#include "core/data_files.h"
#include "output/polylines.h"
#include "output/session.h"
#include "view/s2.h"
#include "view/s3.h"
#include "view/s4.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zippers::gui {

// what the model was started with
struct ModelConfig {
    std::vector<std::string> args;           // EXAMPLE OPTIONS..., as zippers draw takes them (with -o STEM: the stem)
    std::string session;                     // or a session file (then args are options given after it)
    std::string version;                     // the suite's version, for the sessions' made_by
    DataPlaces places{"", "", ""};           // the data search's places, for the hashes of the data files
    JobsOptions jobs;                        // zippers' path, ...
    bool quiet = false;                      // the messages are not printed (only kept)
};

// a job the model asked for (the log of requests, for the tests)
struct ModelRequest {
    JobKind kind = JobKind::curves;
    uint64_t id = 0;
    bool detail = false;                     // a curves job of the view in detail
    std::vector<std::string> args, outputs;
};

// a layer as it is shown: its polylines first .. end - 1, in its colour
struct Shown {
    std::string layer;                       // the polyline file's layer
    std::string colour;                      // the name of its colour (Z+, wheel, pair.Z-, context.Z+, ...)
    double rgb[3] = {0, 0, 0};
    size_t first = 0, end = 0;
};

class Model {
public:
    explicit Model(const ModelConfig& config);
    ~Model();                                // ends the jobs; a picture not yet written is removed, with its session
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    bool ok() const { return error_.empty(); }       // false: the command line, the session or the jobs refused
    const std::string& error() const { return error_; }

    // the handlers; key returns false for a token that is not a key
    bool key(const std::string& token);
    void drag(double dx, double dy);
    void scroll(int n);
    void resize(int w, int h);

    // the jobs' results: poll takes those there are, wait waits up to `seconds` for one; idle waits until no job runs
    // or waits (false if `seconds` passed first)
    bool poll();
    bool wait(double seconds);
    bool idle(double seconds);
    bool busy() const;
    void cancel();                           // ends the running job and drops those asked for (a picture not written
                                             // is removed, with its session)
    bool quit() const { return quit_; }

    // the state
    int sphere() const { return sphere_; }
    bool ready() const { return have_whole_; }       // the first curves are there
    double R() const { return R_; }
    double acc() const { return acc_; }
    double R_computed() const { return whole_R_; }   // the R of the whole curves
    bool detail() const { return detail_on_; }
    const std::vector<double>& shells() const { return shells_; }
    const std::string& stem() const { return stem_; }
    int size() const { return size_; }
    int width() const { return win_w_; }
    int height() const { return win_h_; }
    int mode() const { return mode_; }               // S^2: 0 zippers, 1 zippers and wheel, 2 wheel; else 3 both, 1 Z+, 2 Z-
    const s2::View& view2() const { return gview_; }
    double zoom() const;
    const s3::ViewState& view3() const { return v3_; }
    const s4::View5& view4() const { return v5_; }
    double turn() const { return turn_; }
    double turn2() const { return turn2_; }
    bool pairs() const { return npairs_ > 0; }
    int pair() const { return pair_; }
    bool context() const { return context_; }
    std::vector<Shown> shown() const;                // the layers shown, with their polylines
    const polylines::File* curves() const;           // the curves shown (whole or detail), or null
    uint64_t curves_serial() const { return installed_; }  // the number of sets of curves taken so far (a window
                                                     // uploads curves() again when it or the pointer changes)
    const std::vector<std::string>& messages() const { return messages_; }
    const std::vector<ModelRequest>& requests() const { return requests_; }

    // the state as a session: the picture's draw keys (as p writes them, without its file) and the window keys
    bool state_session(session::Session& s, std::string& why) const;
    // the curves job's arguments for the whole sphere at the current R and accuracy, and those of the view in detail
    std::vector<std::string> whole_args() const;
    bool detail_args(std::vector<std::string>& args, std::string& why) const;
    // the picture's arguments (EXAMPLE OPTIONS, without -o) for the current view; false, with the reason, if there is
    // none (S^2: no command reproduces the rotation, or R passes the program's limits)
    bool picture_args(std::vector<std::string>& args, bool in_detail, std::string& why) const;

private:
    struct Picture {
        uint64_t id = 0;
        std::string png, zsession;
        std::vector<std::string> lines;      // printed when it is written
    };

    void say(const std::string& m);
    bool view_ready(const std::string& key);
    void view_changed();                     // leaves the detail
    void request_whole();
    void request_detail();
    void take(const JobResult& r);
    void install(const polylines::File& f, bool detail);
    void reset_view();
    void set_pair(int k);
    void make_U();
    void zoom_by(double f);
    void plus_minus(bool up, const std::string& k);
    void accuracy(bool finer, const std::string& k);
    void picture(const std::string& k);
    void toggle_detail();
    void help();
    void arrows(char a);
    void pole_screen(double sx, double sy, double sz, double deg);
    void rot_screen(bool aboutX, double deg);
    // the command line's draw keys, less the keys `drop`, with the options `opts` given after them (session::apply)
    std::vector<std::string> with(const std::vector<std::string>& opts, const std::vector<std::string>& drop) const;
    // the session of the picture png of the view, with the window keys
    bool make_session(const std::string& png, session::Session& s, std::string& why) const;

    ModelConfig config_;
    std::string error_;
    std::unique_ptr<Jobs> jobs_;
    int sphere_ = 0;
    session::Session base_;                  // the command line's draw keys (no outputs)
    std::string stem_;
    int size_ = 0, win_w_ = 0, win_h_ = 0;
    double R_ = 0, acc_ = 0;
    std::vector<double> shells_;
    int mode_ = 3;
    bool quit_ = false, zoom_noted_ = false;
    // the view, and the start view
    s2::View gview_{}, gview0_{};
    double zoom2_ = 1, zoom20_ = 1;
    s3::ViewState v3_{}, v30_{};
    s4::View5 v5_{}, v50_{};
    double turn_ = 0, turn2_ = 0, turn0_ = 0, turn20_ = 0;
    bool have_view_ = false;
    bool from_session_ = false;              // started from a session
    bool from_session_zoom_ = false;         // S^2: the window's zoom given by a session (window.zoom)
    bool start_view2_ = false;               // S^2: the start rotation given by a session (window.view)
    s2::View session_view2_{};
    // S^3 pairs
    int npairs_ = 0, pair_ = 0;
    bool context_ = false;
    // the curves
    polylines::File whole_, detail_;
    bool have_whole_ = false, detail_on_ = false;
    uint64_t installed_ = 0;                 // the sets of curves taken
    double whole_R_ = -1, whole_acc_ = 0;
    int whole_wheel_ = 0;                    // S^2: whether the whole curves hold the wheel
    double maxR_ = 0, maxR_focus_ = 0, maxR_wheel_ = 0;
    // the jobs asked for
    uint64_t curves_id_ = 0;                 // the curves job waited for (0: none)
    bool curves_detail_ = false;
    std::vector<Picture> pictures_;
    std::vector<ModelRequest> requests_;
    std::vector<std::string> messages_;
};

}  // namespace zippers::gui
