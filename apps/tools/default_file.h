// SPDX-License-Identifier: GPL-3.0-or-later
//
// default_file.h: the default substitution file of the measuring tools, and how they find it.
//
// The tools take their default as the programs do: zipper3 draws subst_4.00_9.50_t3_r1.txt and zipper5
// subst5_2.30_4.50_t1_r0.txt, both in data/ of the source tree (data/s3/, data/s4/). The default, like a file given with
// -f, is found by the search of src/core/data_files.h: a name with a '/' as given, the working folder, the folders of
// ZIPPERS_DATA, then data/ of the source tree. The first place that holds an entry of the name is used (an entry that is
// not a readable file stops the tool, which says what is there); when it is found nowhere the message says where it
// looked.

#pragma once

#include <string>

#include "core/data_files.h"

namespace zippers {
namespace tools {

// the default substitution file of the S^3 example (cs: true) or the S^4 example, as zipper3 and zipper5 read it
inline std::string default_subst(bool cs)
{
    return cs ? "subst_4.00_9.50_t3_r1.txt" : "subst5_2.30_4.50_t1_r0.txt";
}

// find the substitution file `file` of the S^3 example (cs: true) or the S^4 example, the default if `is_default`:
// true, with `file` the name to open, or false after the message; `places` are the tool's (ZIPPERS_DATA_PLACES)
inline bool find_subst(std::string& file, bool cs, bool is_default, const DataPlaces& places)
{
    return find_data(file, cs ? "s3" : "s4", places, is_default ? "the default substitution file" : nullptr, "-f");
}

}  // namespace tools
}  // namespace zippers
