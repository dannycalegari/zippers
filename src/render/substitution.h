// SPDX-License-Identifier: GPL-3.0-or-later
//
// substitution.h: the substitution file of a zipper, as a kernel search writes it (`chz kernel`, `chz5 kernel`), and
// its reader.
//
// THE FILE. Lines that are empty or begin with '#' are skipped. The others begin with a word that says what they are:
//   t WORD              the element t (phi(t) > 0), as a word of the example
//   S n                 the number of letters; the n lines after it are the letters s_0 .. s_{n-1}, in order:
//                       "j  FIELDS  WORD", where the fields between j and the word depend on the example (below)
//   up                  the roofs of Z+ (t^-1 s_j t = the product of the listed letters); then lines "j  x_1 ... x_k"
//   down                the roofs of Z- (t s_j t^-1 = ...); then lines "j  x_1 ... x_k"
//   member              (Cartwright-Steger only) the membership certificate; its lines are not read
// The rest of a "t" line is the word of t, with its leading blank (the words are read token by token).
//
// THE DIALECTS. The reader is the legacy load_subst (legacy/s3/zipcore.h, for chz's files) and load_subst5
// (legacy/s4/zipcore5.h, for chz5's), which are the same function up to two details. Each example supplies its own
// (examples/*_render.h):
//   - fields: the number of fields between j and the word of a letter. Cartwright-Steger: 2 ("j d=... f2=... word");
//     IMM: 1 ("j d=... word").
//   - member: whether a line "member" begins the membership section, whose lines are then skipped (Cartwright-Steger).
//     Without it (IMM, as load_subst5) such a line is read as a line of the section it is in.
// Everything else, and every message, is the legacy text: "cannot open FILE" and "FILE: incomplete substitution file"
// on stderr, and false, when the file cannot be read or does not have t, S, up and down with n lines each. Line numbers
// j out of the range 0..n-1 in up and down are not checked (as in the legacy readers); kernel searches do not write
// them.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace zippers {

// the contents of a substitution file (legacy Subst, Subst5)
struct Substitution {
    std::string tword;                           // the word of t (the rest of the "t" line)
    std::vector<std::string> sword;              // the words of the letters, in order
    std::vector<std::vector<int>> up, down;      // the roofs of Z+ and Z-: letter numbers
};

// what distinguishes the files of one example from another's (see the top of the file)
struct SubstitutionDialect {
    int fields;                                  // the fields between the number of a letter and its word
    bool member;                                 // a "member" section, skipped
};

// read the file fn into D; false (after a message on stderr) if it cannot be read or is incomplete
inline bool load_substitution(const char* fn, Substitution& D, const SubstitutionDialect& dialect)
{
    std::ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot open %s\n", fn); return false; }
    std::string line;
    int mode = 0;                      // 1: S lines, 2: up, 3: down, 4: member
    size_t n = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        std::string h;
        is >> h;
        if (h == "t") { std::getline(is, D.tword); mode = 0; continue; }
        if (h == "S") { is >> n; mode = 1; continue; }
        if (h == "up") { mode = 2; D.up.assign(n, {}); continue; }
        if (h == "down") { mode = 3; D.down.assign(n, {}); continue; }
        if (dialect.member && h == "member") { mode = 4; continue; }
        if (mode == 1) {               // "j  d=...  f2=...  word" (Cartwright-Steger), "j  d=...  word" (IMM)
            std::string field, w, tok;
            for (int k = 0; k < dialect.fields; k++) is >> field;
            while (is >> tok) { if (!w.empty()) w += " "; w += tok; }
            D.sword.push_back(w);
        } else if (mode == 2 || mode == 3) {
            int j = atoi(h.c_str());
            int x;
            std::vector<int> r;
            while (is >> x) r.push_back(x);
            (mode == 2 ? D.up : D.down)[j] = r;
        }
    }
    if (D.tword.empty() || D.sword.size() != n || D.up.size() != n || D.down.size() != n) {
        fprintf(stderr, "%s: incomplete substitution file\n", fn); return false;
    }
    return true;
}

}  // namespace zippers
