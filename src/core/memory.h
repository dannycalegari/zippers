// SPDX-License-Identifier: GPL-3.0-or-later
//
// memory.h: the process's memory, for the memory guards of the programs.
//
//   resident_bytes   the process's current memory in bytes (0 if unknown): on macOS the larger of its resident size and
//                    its physical footprint (which includes compressed pages), elsewhere its resident size (Linux:
//                    /proc/self/statm)
//   release_memory   returns freed heap memory to the system (macOS: malloc_zone_pressure_relief; glibc: malloc_trim;
//                    elsewhere nothing), so that the resident size, which a memory guard watches, drops once a large
//                    structure is freed
//
// Legacy: resident_bytes and release_memory of legacy/s2/zipper.cc (the S2 program; its film engine calls
// release_memory too). Moved with their bodies unchanged: `static` is `inline`, and they are in the namespace zippers.
// They read the system, so tests/unit/test_memory.cc compares what they see with what the legacy functions see, in the
// same process, rather than bits.
//
// The guards that use them (S2: the drawing's limit, --memlimit; the Catherine wheel's mesh budget) stay with the
// program, as do their messages.

#pragma once

#include <algorithm>
#include <cstdio>
#include <unistd.h>
// (<cstdio> comes first: on Linux it defines __GLIBC__, which the test below reads)
#ifdef __APPLE__
#include <mach/mach.h>            // current resident memory (the drawing's memory guard)
#include <malloc/malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

namespace zippers {

// the process's current memory in bytes (0 if unknown): on macOS the larger of its resident
// size and its physical footprint (which includes compressed pages), elsewhere its resident
// size
inline double resident_bytes()
{
#ifdef __APPLE__
    double r = 0;
    task_vm_info_data_t vi; mach_msg_type_number_t vc = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vi, &vc) == KERN_SUCCESS)
        r = std::max((double)vi.phys_footprint, (double)vi.resident_size);
    mach_task_basic_info_data_t info; mach_msg_type_number_t cnt = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &cnt) == KERN_SUCCESS)
        r = std::max(r, (double)info.resident_size);
    return r;
#else
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long a = 0, r = 0;
    int ok = fscanf(f, "%ld %ld", &a, &r);
    fclose(f);
    return ok == 2 ? (double)r*(double)sysconf(_SC_PAGESIZE) : 0;
#endif
}

// return freed heap memory to the system (so that the resident size, which the memory guard
// watches, drops once a zipper's branch keys are freed)
inline void release_memory()
{
#ifdef __APPLE__
    malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}

}  // namespace zippers
