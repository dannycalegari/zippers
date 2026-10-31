// SPDX-License-Identifier: GPL-3.0-or-later
//
// feed.h: the key feed, a hook for the tests (not in the usage texts): the environment variable ZIPPERS_KEYS holds a
// sequence of tokens, fed to the model's handlers (model.h) as a person's presses reach them. namespace zippers::gui.
//
// THE TOKENS (blanks between them are skipped):
//   one character           a key (key(c)): '+', 'N', ']', ...
//   <L> <R> <U> <D>         the arrows; <E> Esc; <T> Tab
//   <W>                     a pause of ZIPPERS_KEYDELAY milliseconds
//   <sWxH>                  resize the window to W x H (resize)
//   <drag DX,DY>            a drag of the left button by DX, DY pixels (drag)
//   <scroll N>              N notches of the scroll wheel (scroll; N < 0 zooms out)
//   <wait>                  until no job runs or waits (the jobs do not block the keys)
//   <S>                     write the state as a session into STATE_NN.zsession (NN = 01, 02, ...) in the working folder
//   <G>                     grab the picture area into GRAB_NN.png (NN = 01, 02, ...): with a window only, the
//                           centred square of its shorter side as drawn last; refused without one
// A token that is none of these (an unknown <...>, a < with no >) is refused, with a message, and the rest is fed.
// ZIPPERS_KEYDELAY (milliseconds, default 500) is also the time between two tokens with a window; without one (zview
// --headless) the tokens follow one another at once.
//
//   Token       a token: its kind and its numbers
//   parse_keys  the tokens of a sequence (refused ones included, as Token::bad, with the reason)
//   FeedHooks   what a window adds: the resize of the window itself, and the grab
//   FeedState   the exit status so far, and the numbers of the files <S> and <G> wrote
//   apply       one token but <W> and <wait>, which the caller times: false if it was refused
//   feed        feed the tokens to a model without a window: the exit status, 1 if a token was refused, else 0. It
//               polls the jobs between tokens, and stops at q or Esc. A window feeds them through apply, one at a time
//               between the frames.

#pragma once

#include "model.h"

#include <functional>
#include <string>
#include <vector>

namespace zippers::gui {

struct Token {
    enum Kind { key, pause, resize, drag, scroll, wait, state, grab, bad } kind = bad;
    std::string text;                        // the token as written (a key: the key's token for Model::key)
    int w = 0, h = 0, n = 0;                 // resize; scroll
    double dx = 0, dy = 0;                   // drag
    std::string why;                         // bad: why it is refused
};

std::vector<Token> parse_keys(const std::string& keys);

struct FeedOptions {
    int delay_ms = 500;                      // <W>
    bool headless = true;                    // without a window: <G> is refused
    bool quiet = false;                      // the tokens are not logged on the standard error
};

struct FeedHooks {
    std::function<void(int w, int h)> resize;                               // else Model::resize
    std::function<bool(const std::string& file, std::string& why)> grab;    // else <G> is refused
};

struct FeedState {
    int status = 0;                          // 1 once a token was refused
    int states = 0, grabs = 0;               // the files written by <S> and <G>
};

bool apply(Model& m, const Token& t, const FeedOptions& options, FeedState& state, const FeedHooks& hooks);

int feed(Model& m, const std::vector<Token>& tokens, const FeedOptions& options);

}  // namespace zippers::gui
