// SPDX-License-Identifier: GPL-3.0-or-later
//
// default_file.h: the default substitution file of the measuring tools, and the message when it is missing.
//
// The tools take their default as the programs do: zipper3 draws subst_4.00_9.50_t3_r1.txt and zipper5
// subst5_2.30_4.50_t1_r0.txt. Both are read from the working folder, and both are in data/ of the repository (data/s3/,
// data/s4/), not in legacy/, where the files that were the defaults before are. So when the default is missing the error
// says where it is.

#pragma once

#include <cstdio>
#include <string>

namespace zippers {
namespace tools {

// the default substitution file of the S^3 example (cs: true) or the S^4 example, as zipper3 and zipper5 read it
inline std::string default_subst(bool cs)
{
    return cs ? "subst_4.00_9.50_t3_r1.txt" : "subst5_2.30_4.50_t1_r0.txt";
}

// true if the default file can be opened; otherwise false, after saying on stderr that it cannot and where it is
inline bool default_subst_readable(const std::string& file, bool cs)
{
    FILE* f = fopen(file.c_str(), "r");
    if (f) {
        fclose(f);
        return true;
    }
    fprintf(stderr, "cannot open %s\nthe default file %s is in %s of the repository: run there, or give -f\n",
            file.c_str(), file.c_str(), cs ? "data/s3/" : "data/s4/");
    return false;
}

}  // namespace tools
}  // namespace zippers
