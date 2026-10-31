// SPDX-License-Identifier: GPL-3.0-or-later
//
// feed.cc: the key feed (feed.h).

#include "feed.h"

#include <sys/stat.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace zippers::gui {

namespace {

bool integer(const std::string& s, int& n)
{
    if (s.empty()) return false;
    char* end = nullptr;
    long v = strtol(s.c_str(), &end, 10);
    if (*end || v < -1000000 || v > 1000000) return false;
    n = (int)v;
    return true;
}

bool real(const std::string& s, double& x)
{
    if (s.empty()) return false;
    char* end = nullptr;
    x = strtod(s.c_str(), &end);
    return !*end && x > -1e6 && x < 1e6;
}

}  // namespace

std::vector<Token> parse_keys(const std::string& keys)
{
    std::vector<Token> v;
    for (size_t i = 0; i < keys.size(); i++) {
        const char c = keys[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        Token t;
        if (c != '<') {
            t.kind = Token::key;
            t.text = std::string(1, c);
            v.push_back(t);
            continue;
        }
        const size_t e = keys.find('>', i);
        if (e == std::string::npos) {
            t.text = keys.substr(i);
            t.why = "a < with no >";
            v.push_back(t);
            break;
        }
        t.text = keys.substr(i, e - i + 1);
        const std::string in = keys.substr(i + 1, e - i - 1);
        i = e;
        if (in == "L" || in == "R" || in == "U" || in == "D" || in == "E" || in == "T") t.kind = Token::key;
        else if (in == "W") t.kind = Token::pause;
        else if (in == "wait") t.kind = Token::wait;
        else if (in == "S") t.kind = Token::state;
        else if (in == "G") t.kind = Token::grab;
        else if (in.size() > 1 && in[0] == 's' && in.find('x') != std::string::npos) {
            const size_t x = in.find('x');
            if (integer(in.substr(1, x - 1), t.w) && integer(in.substr(x + 1), t.h) && t.w > 0 && t.h > 0)
                t.kind = Token::resize;
            else t.why = "a size <sWxH> with W, H positive integers is wanted";
        } else if (in.compare(0, 5, "drag ") == 0) {
            const std::string a = in.substr(5);
            const size_t comma = a.find(',');
            if (comma != std::string::npos && real(a.substr(0, comma), t.dx) && real(a.substr(comma + 1), t.dy))
                t.kind = Token::drag;
            else t.why = "<drag DX,DY> with two numbers is wanted";
        } else if (in.compare(0, 7, "scroll ") == 0) {
            if (integer(in.substr(7), t.n)) t.kind = Token::scroll;
            else t.why = "<scroll N> with an integer N is wanted";
        } else t.why = "not a token of ZIPPERS_KEYS";
        v.push_back(t);
    }
    return v;
}

bool apply(Model& m, const Token& t, const FeedOptions& o, FeedState& st, const FeedHooks& hooks)
{
    bool ok = true;
    switch (t.kind) {
    case Token::key:
        if (!m.key(t.text)) {
            fprintf(stderr, "keys: %s is not a key; refused\n", t.text.c_str());
            ok = false;
        }
        break;
    case Token::pause:
    case Token::wait: break;
    case Token::resize:
        if (hooks.resize) hooks.resize(t.w, t.h);
        else m.resize(t.w, t.h);
        break;
    case Token::drag: m.drag(t.dx, t.dy); break;
    case Token::scroll: m.scroll(t.n); break;
    case Token::state: {
        char name[40];
        snprintf(name, sizeof name, "STATE_%02d.zsession", ++st.states);
        session::Session s;
        std::string why;
        if (!m.state_session(s, why) || !session::write(name, s, &why)) {
            fprintf(stderr, "keys: <S>: %s\n", why.c_str());
            ok = false;
        } else if (!o.quiet) fprintf(stderr, "keys: <S>: wrote %s\n", name);
        break;
    }
    case Token::grab: {
        if (o.headless || !hooks.grab) {
            fprintf(stderr, "keys: <G>: there is no window to grab; refused\n");
            ok = false;
            break;
        }
        char name[40];
        snprintf(name, sizeof name, "GRAB_%02d.png", ++st.grabs);
        std::string why;
        if (!hooks.grab(name, why)) {
            fprintf(stderr, "keys: <G>: %s\n", why.c_str());
            ok = false;
        } else if (!o.quiet) fprintf(stderr, "keys: <G>: wrote %s\n", name);
        break;
    }
    case Token::bad:
        fprintf(stderr, "keys: %s: %s; refused\n", t.text.c_str(), t.why.c_str());
        ok = false;
        break;
    }
    if (!ok) st.status = 1;
    return ok;
}

int feed(Model& m, const std::vector<Token>& tokens, const FeedOptions& o)
{
    FeedState st;
    const FeedHooks none;
    for (const Token& t : tokens) {
        if (m.quit()) break;
        m.poll();
        if (!o.quiet) fprintf(stderr, "keys: %s\n", t.text.c_str());
        if (t.kind == Token::pause) std::this_thread::sleep_for(std::chrono::milliseconds(o.delay_ms));
        else if (t.kind == Token::wait) while (!m.idle(3600)) {}
        else apply(m, t, o, st, none);
        if (o.delay_ms > 0 && !o.headless) std::this_thread::sleep_for(std::chrono::milliseconds(o.delay_ms));
    }
    m.poll();
    return st.status;
}

}  // namespace zippers::gui
