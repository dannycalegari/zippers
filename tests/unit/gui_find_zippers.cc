// SPDX-License-Identifier: GPL-3.0-or-later
//
// gui_find_zippers: prints what find_zippers (apps/gui/jobs.h) finds for this program: the build's zippers when it runs
// from the folder of its build, else ZIPPERS_INSTALLED_RELATIVE/zippers beside it (an install's layout: libexec/zippers/
// then bin/zippers), or nothing. test_gui_jobs runs it there, and copied into an install's layout, with a zippers in PATH.

#include "jobs.h"

#include <cstdio>

#ifndef ZIPPERS_ZIPPERS
#define ZIPPERS_ZIPPERS ""
#endif
#ifndef ZIPPERS_INSTALLED_RELATIVE
#define ZIPPERS_INSTALLED_RELATIVE ""
#endif

int main()
{
    printf("%s\n", zippers::gui::find_zippers(ZIPPERS_DATA_PLACES, ZIPPERS_ZIPPERS, ZIPPERS_INSTALLED_RELATIVE).c_str());
    return 0;
}
