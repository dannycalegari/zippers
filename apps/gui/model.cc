// SPDX-License-Identifier: GPL-3.0-or-later
//
// model.cc: the window's model (model.h).

#include "model.h"

#include "examples/cartwright_steger.h"
#include "examples/imm_m5.h"
#include "analysis/pairs.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zippers::gui {

namespace {

// the limits of the S^2 program (apps/s2/zipper.cc: PNG_MAX_SIZE, MAX_ZOOM), which its window's 'p' and zoom keep
constexpr int PNG_MAX_SIZE = 16384;
constexpr double MAX_ZOOM = 1e7;
constexpr double MIN_ZOOM = 0.1;
// the shells that + and - step through: S^3 as far as the S^3 program's window, S^4 as far as the shells are tested
constexpr double SHELLS3 = 5.5, SHELLS4 = 2.5;

std::string fmt(const char* f, double x)
{
    char b[64];
    snprintf(b, sizeof b, f, x);
    return b;
}

// an argument as a shell needs it written
std::string quote(const std::string& s)
{
    if (!s.empty() && s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_@%+=:,./-") ==
                          std::string::npos)
        return s;
    std::string q = "'";
    for (char c : s) q += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return q + "'";
}

bool number(const std::string* v, double& x)
{
    if (!v) return false;
    char* end = nullptr;
    x = strtod(v->c_str(), &end);
    return end != v->c_str() && !*end;
}

bool numbers(const std::string* v, size_t n, double* out)
{
    std::vector<double> x;
    if (!v || !polylines::parse_list(*v, x) || x.size() != n) return false;
    std::copy(x.begin(), x.end(), out);
    return true;
}

const std::string* window_key(const session::Session& s, const std::string& key)
{
    for (const session::Line& l : s.window)
        if (l.key == key) return &l.value;
    return nullptr;
}

const char* sphere_name(int s) { return s == 2 ? "S2" : s == 3 ? "S3" : "S4"; }

}  // namespace

// ---- the start ------------------------------------------------------------------------------------------------------

Model::Model(const ModelConfig& config) : config_(config)
{
    session::Session s;
    std::vector<std::string> others;
    std::string why;
    const bool from_session = !config.session.empty();
    if (from_session) {
        std::vector<std::string> warnings;
        if (!session::read(config.session, s, &why, &warnings)) {
            error_ = why;
            return;
        }
        for (const std::string& w : warnings) say("warning: " + w);
        // the files that the session names must be those it was written with
        for (const char* file : {"f", "pairs"}) {
            const std::string* v = s.value(file);
            const std::string* h = s.value(std::string("sha256.") + file);
            if (!v || !h) continue;
            std::string hex, path;
            if (!session::hash_data_file(*v, s.sphere(), config.places, hex, path, why)) {
                error_ = config.session + ": " + why;
                return;
            }
            if (hex != *h) {
                error_ = config.session + ": the file " + *v + ", found at " + path + ", has the SHA-256 " + hex +
                         "; the session's is " + *h + " (it was written with another file of that name)";
                return;
            }
        }
        if (!session::apply(s, config.args, others, why)) {
            error_ = config.session + ": " + why;
            return;
        }
    } else if (!session::from_args(config.args, s, &others, why)) {
        error_ = why;
        return;
    }
    for (size_t i = 0; i < others.size(); i++) {
        if (others[i] == "-o" && i + 1 < others.size()) {
            stem_ = others[++i];
            continue;
        }
        error_ = others[i] + " is not an option of the window (its pictures are saved by p)";
        return;
    }
    sphere_ = s.sphere();
    base_ = s;
    std::string ex = *s.value("example");
    if (stem_.empty()) {
        const std::string* w = window_key(s, "window.stem");
        stem_ = w ? *w : ex;
    }
    double x;
    size_ = number(s.value("s"), x) ? (int)x : 512;
    if (number(window_key(s, "window.size"), x)) size_ = (int)x;
    win_w_ = win_h_ = size_;
    number(s.value("R"), R_);
    number(s.value("a"), acc_);
    if (sphere_ == 2) {
        const std::string* c = s.value("C");
        mode_ = c && *c == "yes" ? 1 : 0;
    } else {
        const std::string* o = s.value("only");
        mode_ = o && *o == "+" ? 1 : o && *o == "-" ? 2 : 3;
    }
    if (const std::string* l = window_key(s, "window.layers")) {
        if (sphere_ == 2) mode_ = *l == "wheel" ? 2 : l->find("wheel") != std::string::npos ? 1 : 0;
        else mode_ = *l == "Z+" ? 1 : *l == "Z-" ? 2 : 3;
    }
    if (sphere_ == 2) {
        if (number(window_key(s, "window.zoom"), x)) {
            zoom2_ = x;
            from_session_zoom_ = true;
        }
        if (numbers(window_key(s, "window.view"), 9, session_view2_.m)) start_view2_ = true;
    }
    if (sphere_ == 3 && s.value("pairs")) {
        npairs_ = -1;                        // (known when the curves come)
        if (number(s.value("pairindex"), x)) pair_ = (int)x;
        const std::string* c = s.value("context");
        context_ = c && *c == "yes";
    }
    from_session_ = from_session;
    // S^2's -C belongs to the layers shown (c), and so to each job
    for (size_t i = 0; i < base_.draw.size();)
        if (base_.draw[i].key == "C") base_.draw.erase(base_.draw.begin() + (long)i);
        else i++;
    if (sphere_ == 3) {
        cs::setup();
        shells_ = cs::shell_radii(SHELLS3);
    }
    if (sphere_ == 4) {
        imm::setup();
        shells_ = imm::shell_radii(SHELLS4);
    }
    jobs_.reset(new Jobs(config.jobs));
    if (!jobs_->ok()) {
        error_ = jobs_->error();
        return;
    }
    request_whole();
}

Model::~Model()
{
    poll();
    jobs_.reset();                           // ends a running job; a picture not written is removed
    for (const Picture& p : pictures_) {
        struct stat st;
        if (stat(p.png.c_str(), &st) != 0) unlink(p.zsession.c_str());
    }
}

void Model::say(const std::string& m)
{
    messages_.push_back(m);
    if (!config_.quiet) fprintf(stderr, "%s\n", m.c_str());
}

double Model::zoom() const { return sphere_ == 2 ? zoom2_ : sphere_ == 3 ? v3_.zoom : v5_.zoom; }

// ---- the command lines of the jobs ------------------------------------------------------------------------------------

std::vector<std::string> Model::with(const std::vector<std::string>& opts, const std::vector<std::string>& drop) const
{
    session::Session s = base_;
    for (size_t i = 0; i < s.draw.size();) {
        bool d = false;
        for (const std::string& k : drop) d = d || s.draw[i].key == k;
        if (d) s.draw.erase(s.draw.begin() + (long)i);
        else i++;
    }
    std::vector<std::string> others;
    std::string why;
    if (!session::apply(s, opts, others, why)) return {};
    return session::args_of(s);
}

std::vector<std::string> Model::whole_args() const
{
    std::vector<std::string> o = {"-R", polylines::exact(R_), "-a", polylines::exact(acc_)};
    if (sphere_ == 2) {
        o.push_back("--nocull");
        if (mode_ >= 1) o.push_back("-C");
    }
    return with(o, {"cull", "cullk", "zoomacc", "focus"});
}

bool Model::picture_args(std::vector<std::string>& args, bool in_detail, std::string& why) const
{
    std::vector<std::string> o = {"-R", polylines::exact(R_)};
    std::vector<std::string> drop;
    if (sphere_ == 2) {
        int size;
        double z;
        s2::export_framing(size_, zoom2_, PNG_MAX_SIZE, MAX_ZOOM, size, z);
        const double px = 1.0/(z*(size/2.0 - 2.0));      // one pixel of the picture
        std::string sy, sp, sr;
        double e;
        if (!s2::command_angles(gview_, px, sy, sp, sr, e)) {
            why = "no command line reproduces this orientation to 1/1000 of a pixel (the window's rotation has drifted "
                  "by " + fmt("%.2g", e) + ")";
            return false;
        }
        std::string sa = polylines::exact(acc_);
        if (in_detail && zoom2_ > 1.0) {
            const double a = std::min(acc_, std::max(1e-6, acc_/zoom2_));
            sa = s2::fmt_num(a, 5e-3*a);
        }
        const bool wheel = mode_ >= 1;
        o.insert(o.end(), {"-a", sa, "-s", std::to_string(size), "--yaw", sy, "--pitch", sp, "--roll", sr, "--zoom",
                           polylines::exact(z)});
        if (wheel) o.push_back("-C");
        else drop.push_back("C");
        const std::string* nc = base_.value("nocull");
        const std::string* nf = base_.value("nofocus");
        const bool focus = !wheel && !(nc && *nc == "yes") && !(nf && *nf == "yes") && z > 1.0 + 1e-9 && R_ > 1.0;
        double limit = maxR_;
        if (focus && R_ > maxR_focus_) o.push_back("--nofocus");
        else if (focus) limit = maxR_focus_;
        if (have_whole_ && R_ > limit) {
            why = "R = " + fmt("%g", R_) + " passes the program's limit for this example, " + fmt("%.2f", limit);
            return false;
        }
        if (have_whole_ && wheel && R_ > maxR_wheel_) {
            why = "R = " + fmt("%g", R_) + " passes the limit of the wheel, " + fmt("%.2f", maxR_wheel_);
            return false;
        }
    } else {
        o.insert(o.end(), {"-a", polylines::exact(acc_), "-s", std::to_string(size_)});
        std::vector<double> vm;
        if (sphere_ == 3) {
            for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) vm.push_back(v3_.Q[r][k]);
            for (int i = 0; i < 9; i++) vm.push_back(v3_.rot[i]);
        } else {
            for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) vm.push_back(v5_.Q[r][k]);
            for (int i = 0; i < 9; i++) vm.push_back(v5_.rot[i]);
            o.insert(o.end(), {"--turn", polylines::exact(turn_), "--turn2", polylines::exact(turn2_)});
        }
        o.insert(o.end(), {"--viewmatrix", polylines::exact_list(vm.data(), vm.size(), ','), "--zoom",
                           polylines::exact(zoom()), "--clip", polylines::exact(sphere_ == 3 ? v3_.clip : v5_.clip)});
        if (mode_ == 1 || mode_ == 2) o.insert(o.end(), {"--only", mode_ == 1 ? "+" : "-"});
        if (pairs()) {
            o.insert(o.end(), {"--pairindex", std::to_string(pair_)});
            if (context_) o.push_back("--context");
            else drop.push_back("context");
        }
        if (in_detail) {
            drop.insert(drop.end(), {"nocull", "nofocus", "focus"});
            o.insert(o.end(), {"--cull", "--zoomacc"});
        }
    }
    args = with(o, drop);
    if (args.empty()) {
        why = "the options of the picture cannot be made";
        return false;
    }
    return true;
}

bool Model::detail_args(std::vector<std::string>& args, std::string& why) const
{
    if (pairs()) {
        why = "there is no detail of a linked pair (zipper3 culls no pair)";
        return false;
    }
    if (!picture_args(args, true, why)) return false;
    if (sphere_ != 2 && zoom() > 1.0) args.insert(args.end(), {"--focus", "1"});
    return true;
}

bool Model::make_session(const std::string& png, session::Session& s, std::string& why) const
{
    std::vector<std::string> args;
    if (!picture_args(args, detail_on_, why)) return false;
    args.insert(args.end(), {"-o", png});
    std::vector<std::string> others;
    if (!session::from_args(args, s, &others, why)) return false;
    if (!session::hash_files(s, config_.places, why)) return false;
    s.made_by = session::made_by_text(config_.version);
    s.window.push_back({"window.stem", stem_});
    s.window.push_back({"window.size", std::to_string(size_)});
    std::string layers;
    if (sphere_ == 2) layers = mode_ == 0 ? "Z+ Z-" : mode_ == 1 ? "Z+ Z- wheel" : "wheel";
    else layers = mode_ == 1 ? "Z+" : mode_ == 2 ? "Z-" : "Z+ Z-";
    s.window.push_back({"window.layers", layers});
    if (sphere_ == 2) {
        s.window.push_back({"window.zoom", polylines::exact(zoom2_)});
        s.window.push_back({"window.view", polylines::exact_list(gview_.m, 9)});
    }
    return true;
}

bool Model::state_session(session::Session& s, std::string& why) const
{
    if (!have_view_) {
        why = "the view is not known yet";
        return false;
    }
    return make_session(stem_ + ".png", s, why);
}

// ---- the jobs ---------------------------------------------------------------------------------------------------------

void Model::request_whole()
{
    ModelRequest q;
    q.kind = JobKind::curves;
    q.args = whole_args();
    q.id = jobs_->request_curves(q.args);
    curves_id_ = q.id;
    curves_detail_ = false;
    requests_.push_back(q);
}

void Model::request_detail()
{
    ModelRequest q;
    q.kind = JobKind::curves;
    q.detail = true;
    std::string why;
    if (!detail_args(q.args, why)) {
        say("f: " + why);
        return;
    }
    q.id = jobs_->request_curves(q.args);
    curves_id_ = q.id;
    curves_detail_ = true;
    requests_.push_back(q);
}

bool Model::busy() const { return curves_id_ != 0 || !pictures_.empty(); }

void Model::cancel()
{
    if (!jobs_) return;
    poll();                                  // (the pictures already written)
    jobs_->cancel();                         // (the jobs remove a picture's partial file when its child has ended)
    curves_id_ = 0;
    for (const Picture& p : pictures_) {
        unlink(p.zsession.c_str());
        say("p: " + p.png + " was not written: cancelled");
    }
    pictures_.clear();
}

bool Model::poll()
{
    if (!jobs_) return false;
    bool any = false;
    JobResult r;
    while (jobs_->poll(r)) {
        take(r);
        any = true;
    }
    return any;
}

bool Model::wait(double seconds)
{
    if (!jobs_) return false;
    JobResult r;
    if (!jobs_->wait(r, seconds)) return false;
    take(r);
    poll();
    return true;
}

bool Model::idle(double seconds)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds((long long)(seconds*1e6));
    while (busy()) {
        const double left = std::chrono::duration<double>(end - std::chrono::steady_clock::now()).count();
        if (left <= 0) return false;
        wait(std::min(left, 0.25));
    }
    return true;
}

void Model::take(const JobResult& r)
{
    auto tail = [&](const std::string& head) {
        say(head);
        const size_t n = r.stderr_tail.size(), from = n > 4 ? n - 4 : 0;
        for (size_t i = from; i < n; i++) say("  " + r.stderr_tail[i]);
    };
    if (r.kind == JobKind::curves) {
        if (r.id != curves_id_) return;                          // for a request that is no longer the latest
        const bool detail = curves_detail_;
        curves_id_ = 0;
        if (r.has_curves) {
            install(r.curves, detail);
            if (r.status == 3) say("the curves are incomplete: zipper reached the memory limit of the jobs");
        } else if (r.ended)
            say("the curves job was ended: " + r.reason);
        else
            tail("the curves could not be computed: " + (r.read_error.empty() ? r.reason : r.read_error));
        return;
    }
    for (size_t i = 0; i < pictures_.size(); i++) {
        if (pictures_[i].id != r.id) continue;
        const Picture p = pictures_[i];
        pictures_.erase(pictures_.begin() + (long)i);
        if (r.status == 0 && !r.ended) {
            for (const std::string& l : p.lines) say(l);
        } else {
            unlink(p.zsession.c_str());
            if (r.ended) say("p: " + p.png + " was not written: " + r.reason);
            else tail("p: " + p.png + " was not written (zippers draw ended with the exit status " +
                      std::to_string(r.status) + ")");
        }
        return;
    }
}

void Model::install(const polylines::File& f, bool detail)
{
    installed_++;
    for (const std::string& c : f.values("capped"))
        if (c.size() > 4 && c.compare(c.size() - 4, 4, " yes") == 0)
            say("the curves of " + c.substr(0, c.size() - 4) + " are incomplete: a cap was reached");
    if (detail) {
        detail_ = f;
        detail_on_ = true;
        size_t n = 0;
        for (const polylines::Layer& L : f.layers) n += L.npoly();
        say("f: the curves of this view in detail (" + std::to_string(n) + " polylines; f again for the whole picture)");
        return;
    }
    whole_ = f;
    number(f.value("R"), whole_R_);
    number(f.value("acc"), whole_acc_);
    whole_wheel_ = f.layer("wheel") ? 1 : 0;
    if (sphere_ == 2) {
        number(f.value("maxR"), maxR_);
        number(f.value("maxR.focus"), maxR_focus_);
        number(f.value("maxR.wheel"), maxR_wheel_);
    }
    if (sphere_ == 3) {
        double n;
        npairs_ = number(f.value("pairs"), n) ? (int)n : 0;
        if (npairs_ > 0) pair_ = ((pair_ % npairs_) + npairs_) % npairs_;
    }
    const bool first = !have_whole_;
    have_whole_ = true;
    if (!first) return;
    // the start view: the program's own, from the header
    bool ok = true;
    if (sphere_ == 2) {
        ok = numbers(f.value("view"), 9, gview0_.m);
        double z;
        if (number(f.value("zoom"), z)) zoom20_ = z;
        if (start_view2_) gview0_ = session_view2_;
        if (from_session_zoom_) zoom20_ = zoom2_;
    } else if (sphere_ == 3) {
        double vm[25];
        ok = numbers(f.value("viewmatrix"), 25, vm) && number(f.value("zoom"), v30_.zoom) &&
             number(f.value("clip"), v30_.clip);
        if (ok) {
            for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) v30_.Q[r][k] = vm[4*r + k];
            for (int i = 0; i < 9; i++) v30_.rot[i] = vm[16 + i];
        }
    } else {
        double vm[34];
        ok = numbers(f.value("viewmatrix"), 34, vm) && number(f.value("zoom"), v50_.zoom) &&
             number(f.value("clip"), v50_.clip) && number(f.value("hidfog"), v50_.hidfog) &&
             number(f.value("turn"), turn0_) && number(f.value("turn2"), turn20_);
        if (ok) {
            for (int r = 0; r < 5; r++) for (int k = 0; k < 5; k++) v50_.Q[r][k] = vm[5*r + k];
            for (int i = 0; i < 9; i++) v50_.rot[i] = vm[25 + i];
        }
    }
    if (!ok) {
        say("the curves' file gives no view; the window cannot show them");
        have_whole_ = false;
        return;
    }
    have_view_ = true;
    if (from_session_ && sphere_ == 3) {         // a session's view is the window's, also with a pair
        v3_ = v30_;
        return;
    }
    reset_view();
}

void Model::reset_view()
{
    gview_ = gview0_;
    zoom2_ = zoom20_;
    v3_ = v30_;
    v5_ = v50_;
    turn_ = turn0_;
    turn2_ = turn20_;
    if (sphere_ == 4) make_U();
    if (pairs()) {
        const std::string* pole = whole_.value("pair." + std::to_string(pair_) + ".pole");
        double p[4], clip;
        if (numbers(pole, 4, p) && number(whole_.value("pair." + std::to_string(pair_) + ".clip"), clip)) {
            s3::frame_with_pole(p, v3_.Q);
            v3_.clip = clip;
            v3_.zoom = 1;
        }
    }
}

void Model::make_U()
{
    std::memset(v5_.U, 0, sizeof v5_.U);
    for (int i = 0; i < 4; i++) v5_.U[i][i] = 1;
    s4::turn4(v5_.U, 2, 3, turn_*s4::PI/180);
    s4::turn4(v5_.U, 0, 3, turn2_*s4::PI/180);
}

// ---- what is shown ----------------------------------------------------------------------------------------------------

const polylines::File* Model::curves() const
{
    if (detail_on_) return &detail_;
    return have_whole_ ? &whole_ : nullptr;
}

std::vector<Shown> Model::shown() const
{
    std::vector<Shown> v;
    const polylines::File* f = curves();
    if (!f) return v;
    auto colour = [&](const std::string& name, double rgb[3]) {
        for (const session::Line& l : base_.draw)
            if (l.key == "colour" && session::colour_name(l.value) == name) {
                std::vector<double> c;
                polylines::parse_list(l.value.substr(l.value.find('=') + 1), c);
                for (int i = 0; i < 3 && i < (int)c.size(); i++) rgb[i] = c[i];
            }
    };
    auto add = [&](const std::string& layer, const std::string& cname, size_t first, size_t end) {
        const polylines::Layer* L = f->layer(layer);
        if (!L) return;
        Shown s;
        s.layer = layer;
        s.colour = cname;
        colour(cname, s.rgb);
        s.first = std::min(first, L->npoly());
        s.end = std::min(end, L->npoly());
        v.push_back(s);
    };
    // the polylines of the roots up to R: in S^3 and S^4 they come first (in the order of their roots)
    auto upto = [&](const std::string& layer) -> size_t {
        const polylines::Layer* L = f->layer(layer);
        if (!L) return 0;
        if (sphere_ == 2 || detail_on_) return L->npoly();
        size_t k = 0;
        while (k < L->npoly() && L->root[k] >= 0 && (size_t)L->root[k] < f->rootd.size() && f->rootd[L->root[k]] <= R_)
            k++;
        return k;
    };
    const size_t all = (size_t)-1;
    if (sphere_ == 2) {
        if (mode_ <= 1) {
            add("Z+", "Z+", 0, all);
            add("Z-", "Z-", 0, all);
        }
        if (mode_ >= 1) add("wheel", "wheel", 0, all);
        return v;
    }
    if (pairs()) {
        if (context_) {
            if (mode_ != 2) add("Z+", "context.Z+", 0, upto("Z+"));
            if (mode_ != 1) add("Z-", "context.Z-", 0, upto("Z-"));
        }
        add("pair.Z+", "pair.Z+", (size_t)pair_, (size_t)pair_ + 1);
        add("pair.Z-", "pair.Z-", (size_t)pair_, (size_t)pair_ + 1);
        return v;
    }
    if (mode_ != 2) add("Z+", "Z+", 0, upto("Z+"));
    if (mode_ != 1) add("Z-", "Z-", 0, upto("Z-"));
    return v;
}

// ---- the handlers -----------------------------------------------------------------------------------------------------

bool Model::view_ready(const std::string& k)
{
    if (have_view_) return true;
    say(k + ": the view is not known yet (the first curves are being computed)");
    return false;
}

void Model::view_changed()
{
    if (!detail_on_ && !(curves_id_ && curves_detail_)) return;
    if (curves_id_ && curves_detail_) {
        jobs_->cancel_curves();
        curves_id_ = 0;
    }
    detail_on_ = false;
    say("back to the whole picture");
    const bool stale = sphere_ == 2 ? whole_R_ != R_ || whole_acc_ != acc_ || (mode_ >= 1 && !whole_wheel_)
                                    : R_ > whole_R_ || whole_acc_ != acc_;
    if (stale) request_whole();
}

void Model::zoom_by(double f)
{
    double& z = sphere_ == 2 ? zoom2_ : sphere_ == 3 ? v3_.zoom : v5_.zoom;
    z *= f;
    z = std::max(MIN_ZOOM, std::min(z, MAX_ZOOM));
    say(fmt("zoom %.4g", z));
    if (z > 1e5 && !zoom_noted_) {
        zoom_noted_ = true;
        say("note: beyond zoom 1e5 the curves' single-precision points show as steps, in the window and in p's "
            "pictures alike");
    }
}

void Model::rot_screen(bool aboutX, double deg)             // rotate R^3 about a screen axis (S^3, S^4)
{
    double* rot = sphere_ == 3 ? v3_.rot : v5_.rot;
    double t = deg*s3::PI/180, c = std::cos(t), s = std::sin(t), M[9];
    if (aboutX) { double X[9] = {1,0,0, 0,c,-s, 0,s,c}; std::memcpy(M, X, sizeof M); }
    else { double Y[9] = {c,0,s, 0,1,0, -s,0,c}; std::memcpy(M, Y, sizeof M); }
    double T[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { T[3*i+j] = 0; for (int k = 0; k < 3; k++) T[3*i+j] += M[3*i+k]*rot[3*k+j]; }
    std::memcpy(rot, T, sizeof T);
}

// move the pole toward the frame direction that is currently the screen axis (sx, sy, sz)
void Model::pole_screen(double sx, double sy, double sz, double deg)
{
    if (sphere_ == 3) {
        const double* rot = v3_.rot;
        double a[3];
        for (int i = 0; i < 3; i++) a[i] = rot[i]*sx + rot[3 + i]*sy + rot[6 + i]*sz;    // rot^T s
        s3::move_pole(v3_.Q, a, deg*s3::PI/180);
        return;
    }
    const double* rot = v5_.rot;
    double u[3], a[4] = {0, 0, 0, 0};
    for (int i = 0; i < 3; i++) u[i] = rot[i]*sx + rot[3 + i]*sy + rot[6 + i]*sz;        // rot^T s
    for (int k = 0; k < 4; k++) for (int i = 0; i < 3; i++) a[k] += v5_.U[i][k]*u[i];  // U^T (u, 0)
    s4::move_pole5_toward(v5_.Q, a, deg*s4::PI/180);
}

void Model::arrows(char a)
{
    const char* name = a == 'L' ? "<L>" : a == 'R' ? "<R>" : a == 'U' ? "<U>" : "<D>";
    if (!view_ready(name)) return;
    if (sphere_ == 2) {
        const double d = 6.0;   // pre-multiply: rotate about the screen-fixed axis
        if      (a == 'L') gview_ = s2::mul3(s2::roty(-d), gview_);
        else if (a == 'R') gview_ = s2::mul3(s2::roty( d), gview_);
        else if (a == 'U') gview_ = s2::mul3(s2::rotx(-d), gview_);
        else               gview_ = s2::mul3(s2::rotx( d), gview_);
    } else {
        const double d = 4.0;
        if (a == 'L') pole_screen(-1, 0, 0, d);
        else if (a == 'R') pole_screen(1, 0, 0, d);
        else if (a == 'U') pole_screen(0, 1, 0, d);
        else pole_screen(0, -1, 0, d);
    }
    view_changed();
}

void Model::plus_minus(bool up, const std::string& k)
{
    if (sphere_ == 2) {
        if (up) {
            const bool wheel = mode_ >= 1;
            const double R = R_ + 0.5, limit = wheel ? maxR_wheel_ : maxR_;
            if (have_whole_ && R > limit) {
                say(fmt(("+: R stays at %.1f" + std::string(wheel ? " (press c until the wheel is hidden to go deeper)"
                                                                   : "")).c_str(), R_));
                return;
            }
            R_ = R;
            say(fmt("+: computing R = %.1f ...", R_));
        } else {
            if (!(R_ > 1.0)) {
                say("-: R is already at its minimum, 1");
                return;
            }
            R_ = std::max(1.0, R_ - 0.5);
            say(fmt("-: computing R = %.1f ...", R_));
        }
        if (detail_on_) request_detail();
        else request_whole();
        return;
    }
    // next / previous shell of roots
    double R = R_;
    if (up) { for (double r : shells_) if (r > R_ + 1e-4) { R = r + 1e-6; break; } }
    else { R = 0; for (double r : shells_) if (r < R_ - 1e-4) R = r + 1e-6; }
    if (R == R_) {
        say(up ? k + ": R stays at " + fmt("%.6g", R_) + " (the shells stepped through end at " +
                     fmt("%g", sphere_ == 3 ? SHELLS3 : SHELLS4) + "; -R gives any R)"
               : k + ": R is already at its minimum, 0");
        return;
    }
    R_ = R;
    const polylines::File* f = have_whole_ ? &whole_ : nullptr;
    const std::string* focus = f ? f->value("focus") : nullptr;
    if (detail_on_ || (curves_id_ && curves_detail_)) {
        say(k + ": R = " + fmt("%.6g", R_) + ", computing the detail of this view ...");
        request_detail();
    } else if (f && R_ <= whole_R_ && focus && *focus == "off") {
        say(k + ": R = " + fmt("%.6g", R_) + " (the first polylines of the curves of R = " + fmt("%.6g", whole_R_) + ")");
    } else {
        say(k + ": computing R = " + fmt("%.6g", R_) + " ...");
        request_whole();
    }
}

void Model::accuracy(bool finer, const std::string& k)
{
    if (finer) acc_ /= 1.5;
    else acc_ *= 1.5;
    say(k + ": computing the accuracy " + fmt("%.4g", acc_) + " ...");
    if (detail_on_ || (curves_id_ && curves_detail_)) request_detail();
    else request_whole();
}

void Model::set_pair(int k)
{
    const int n = npairs_;
    pair_ = ((k % n) + n) % n;
    const std::string K = "pair." + std::to_string(pair_) + ".";
    double p[4], clip;
    if (numbers(whole_.value(K + "pole"), 4, p) && number(whole_.value(K + "clip"), clip)) {
        s3::frame_with_pole(p, v3_.Q);       // pair_view: the frame of its pole, its clip, zoom 1
        v3_.clip = clip;
        v3_.zoom = 1;
    }
    const std::string* line = whole_.value(K + "line");
    say(line ? *line : "pair " + std::to_string(pair_));
}

void Model::picture(const std::string& k)
{
    if (!view_ready(k)) return;
    std::string why;
    const std::string name = s2::export_stem(stem_, {".png", ".zsession"});
    const std::string png = name + ".png", zs = name + ".zsession";
    session::Session s;
    if (!make_session(png, s, why)) {
        say(k + ": no picture: " + why);
        return;
    }
    if (!session::write(zs, s, &why, png.substr(png.find_last_of('/') + 1))) {
        say(k + ": " + why);
        return;
    }
    Picture p;
    p.png = png;
    p.zsession = zs;
    p.lines.push_back("p: wrote " + png + "; it is remade exactly by");
    p.lines.push_back("  zippers draw --session " + quote(zs) + " -o " + quote(png));
    const bool zoomed = zoom() > 1.0;
    if (zoomed && !detail_on_ && sphere_ == 2) {
        const double a = std::min(acc_, std::max(1e-6, acc_/zoom2_));
        const std::string how = a > 1e-6 || acc_ <= 1e-6 ? "-a divided by the zoom" : "-a divided by the zoom, down to 1e-6";
        p.lines.push_back("p: and at full detail (" + how + ") by");
        p.lines.push_back("  zippers draw --session " + quote(zs) + " -a " + s2::fmt_num(a, 5e-3*a) + " -o " +
                          quote(name + "_detail.png"));
    } else if (zoomed && !detail_on_ && !pairs()) {
        p.lines.push_back("p: and at full detail (the accuracy " + fmt("%g", acc_) + " on the screen) by");
        p.lines.push_back("  zippers draw --session " + quote(zs) + " --zoomacc -o " + quote(name + "_detail.png"));
    }
    if (sphere_ == 2 && mode_ == 2) p.lines.push_back("p: (the picture draws the zippers with the wheel)");
    ModelRequest q;
    q.kind = JobKind::picture;
    q.args = {"--session", zs, "-o", png};
    q.outputs = {png};
    q.id = jobs_->request_picture(q.args, q.outputs);
    p.id = q.id;
    requests_.push_back(q);
    pictures_.push_back(p);
    say(k + ": computing " + png + " ...");
}

void Model::toggle_detail()
{
    if (detail_on_ || (curves_id_ && curves_detail_)) {
        view_changed();
        return;
    }
    if (!view_ready("f")) return;
    say("f: computing the curves of this view in detail ...");
    request_detail();
}

void Model::help()
{
    const char* common =
        "  ] [        the accuracy / 1.5, * 1.5 (new curves)\n"
        "  z Z x X    zoom in, out (* 1.5, / 1.5; the scroll wheel too)\n"
        "  r          the start view\n"
        "  p P        save the picture of this view (NAME.png, with NAME.zsession), and print its commands\n"
        "  f          the curves of this view in detail; f again for the whole picture\n"
        "  ?          these keys\n"
        "  q Esc      quit\n"
        "  drag       rotate (0.4 degree a pixel)\n";
    std::string t = std::string("the keys (") + sphere_name(sphere_) + "):\n";
    if (sphere_ == 2)
        t += "  arrows     rotate the sphere 6 degrees about the screen's axes\n"
             "  + -        R + 0.5, R - 0.5 (new curves)\n"
             "  c          the zippers -> and the wheel -> the wheel\n";
    else
        t += "  arrows     move the pole 4 degrees toward the screen's axes\n"
             "  w s        move the pole toward, away from the viewer\n"
             "  + -        the next, the previous shell of roots\n"
             "  c          both zippers -> Z+ -> Z-\n"
             "  b          the clip radius (3 -> 6 -> 12 -> 1.5)\n";
    if (sphere_ == 3) t += "  n N l      with --pairs: the next, the previous linked pair; the zippers behind it\n";
    if (sphere_ == 4) t += "  h H j J    turn the hidden direction 5 degrees (with the horizontal axis; with the depth)\n";
    t += common;
    while (!t.empty() && t.back() == '\n') t.pop_back();
    say(t);
}

bool Model::key(const std::string& token)
{
    if (token.empty()) return false;
    if (token.size() > 1) {
        if (token == "<L>" || token == "<R>" || token == "<U>" || token == "<D>") {
            arrows(token[1]);
            return true;
        }
        if (token == "<E>") {
            quit_ = true;
            return true;
        }
        if (token == "<T>") {
            say("Tab: this window has no panel");
            return true;
        }
        return false;
    }
    const char c = token[0];
    const std::string k(1, c);
    auto not_here = [&](const char* where) { say(k + ": does nothing in " + sphere_name(sphere_) + " (" + where + ")"); };
    switch (c) {
    case '+': case '=': plus_minus(true, k); return true;
    case '-': case '_': plus_minus(false, k); return true;
    case ']': accuracy(true, k); return true;
    case '[': accuracy(false, k); return true;
    case 'z': case 'Z': case 'x': case 'X':
        if (view_ready(k)) {
            zoom_by((c == 'z' || c == 'Z') ? 1.5 : 1.0/1.5);
            view_changed();
        }
        return true;
    case 'c': case 'C':
        if (sphere_ == 2) {
            int next = (mode_ + 1) % 3;
            if (next == 1 && have_whole_ && R_ > maxR_wheel_) {
                say("c: the wheel is not shown (press - for a smaller R)");
                return true;
            }
            mode_ = next;
            say(mode_ == 0 ? "c: the zippers" : mode_ == 1 ? "c: the zippers and the wheel" : "c: the wheel");
            if (mode_ == 1 && detail_on_) request_detail();
            else if (mode_ == 1 && !whole_wheel_ && !detail_on_) {
                say("c: computing the Catherine wheel ...");
                request_whole();
            }
        } else {
            mode_ = mode_ == 3 ? 1 : mode_ + 1;
            say(mode_ == 3 ? "c: both zippers" : mode_ == 1 ? "c: Z+" : "c: Z-");
        }
        return true;
    case 'w': case 's':
        if (sphere_ == 2) not_here("it moves the pole in S3 and S4");
        else if (view_ready(k)) {
            pole_screen(0, 0, c == 'w' ? 1 : -1, 4);
            view_changed();
        }
        return true;
    case 'b':
        if (sphere_ == 2) not_here("it changes the clip radius in S3 and S4");
        else if (view_ready(k)) {
            double& clip = sphere_ == 3 ? v3_.clip : v5_.clip;
            clip = clip >= 8 ? 1.5 : clip*2;
            say(fmt("clip radius %.3g", clip));
            view_changed();
        }
        return true;
    case 'r':
        if (view_ready(k)) {
            reset_view();
            say("r: the start view");
            view_changed();
        }
        return true;
    case 'n': case 'N': case 'l':
        if (sphere_ != 3) not_here("the linked pairs are in S3");
        else if (!pairs()) say(k + ": there are no linked pairs (zippers view cartwright-steger --pairs linked_pairs.txt)");
        else if (c == 'l') {
            context_ = !context_;
            say(context_ ? "l: the zippers behind the pair" : "l: the pair alone");
        } else if (view_ready(k)) {
            set_pair(pair_ + (c == 'n' ? 1 : -1));
            view_changed();
        }
        return true;
    case 'h': case 'H': case 'j': case 'J':
        if (sphere_ != 4) not_here("it turns the hidden direction in S4");
        else if (view_ready(k)) {
            double& t = (c == 'h' || c == 'H') ? turn2_ : turn_;
            t += (c == 'h' || c == 'j') ? 5 : -5;
            make_U();
            say(k + ((c == 'h' || c == 'H') ? ": turn2 " : ": turn ") + fmt("%g", t));
            view_changed();
        }
        return true;
    case 'p': case 'P': picture(k); return true;
    case 'f': toggle_detail(); return true;
    case '?': help(); return true;
    case '\t': say("Tab: this window has no panel"); return true;
    case 'q': case 27: quit_ = true; return true;
    default:
        say(k + ": not a key of this window (? lists them)");
        return true;
    }
}

void Model::drag(double dx, double dy)
{
    if (!view_ready("drag")) return;
    if (sphere_ == 2) {
        gview_ = s2::mul3(s2::roty(0.4*dx), gview_);
        gview_ = s2::mul3(s2::rotx(0.4*dy), gview_);
    } else {
        rot_screen(false, 0.4*dx);
        rot_screen(true, 0.4*dy);
    }
    view_changed();
}

void Model::scroll(int n)
{
    if (!n || !view_ready("scroll")) return;
    for (int i = 0; i < std::abs(n); i++) zoom_by(n > 0 ? 1.5 : 1.0/1.5);
    view_changed();
}

void Model::resize(int w, int h)
{
    win_w_ = w;
    win_h_ = h;
}

}  // namespace zippers::gui
