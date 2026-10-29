// SPDX-License-Identifier: GPL-3.0-or-later
//
// timing.h: the clock the programs use for the timings they print.
//
// Legacy: now() in legacy/s3/csgroup.h and legacy/s4/rh5.h (the same function, copied in both).

#pragma once

#include <chrono>

namespace zippers {

// seconds on the steady clock (only differences mean anything)
inline double now()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace zippers
