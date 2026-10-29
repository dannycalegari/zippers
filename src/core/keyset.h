// SPDX-License-Identifier: GPL-3.0-or-later
//
// keyset.h: a set of 64-bit fingerprints (nonzero), by open addressing.
//
// Legacy: KeySet in legacy/s3/zipcore.h, and the identical copy in legacy/s4/zipcore5.h. Moved unchanged; the
// renderer keeps the fingerprints (core/hash.h) of the branches it has drawn in one of these.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace zippers {

struct KeySet {                      // open addressing, 0 = empty
    std::vector<uint64_t> t;
    size_t n = 0;
    uint64_t mask = 0;
    KeySet() { t.assign(1 << 16, 0); mask = t.size() - 1; }
    bool contains(uint64_t k) const
    {
        for (uint64_t i = k & mask; t[i]; i = (i + 1) & mask) if (t[i] == k) return true;
        return false;
    }
    bool insert(uint64_t k)
    {
        if (2*(n + 1) > t.size()) {
            std::vector<uint64_t> old;
            old.swap(t);
            t.assign(2*old.size(), 0);
            mask = t.size() - 1;
            n = 0;
            for (uint64_t x : old) if (x) insert(x);
        }
        uint64_t i = k & mask;
        for (; t[i]; i = (i + 1) & mask) if (t[i] == k) return false;
        t[i] = k; n++;
        return true;
    }
};

}  // namespace zippers
