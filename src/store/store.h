// SPDX-License-Identifier: GPL-3.0-or-later
//
// store.h: a store of exact group elements, for any backend B (geometry/backend.h), with a hash table on their keys.
//
// Each element is kept packed (B::pack: B::packed_size int32 words), with the parent and the letter of the path that
// reached it, its depth, a label of the example's choosing (Label: the f-value of Cartwright-Steger, the (mu, h) of
// IMM's M5), and d(o, g o) as a float. Elements get the indices 0, 1, 2, ... in the order they are added, and the
// first one added is the root (parent -1).
//
// Legacy: Store (legacy/s3/csgroup.h, labels f0 and f1) and Store6 (legacy/s4/rh5.h, labels mu and h), which are the
// same store up to the arithmetic and the labels. What is the same as there, and must stay so: the packed layout and
// the storage guard (B::pack: 2^28 for PU(2,1), the int32 range for O(n,1)), the indices of the elements, and
// path(). What may differ, since no output depends on it: the hash table. Here it is the one of the legacy Store (an
// open-addressing table of int32 indices, at most half full, 1024 slots to begin with) with the hash of its keys
// (splitmix over the entries of the key); Store6 had another seed and a table of int64 entries. Every store of the
// programs holds each key at most once (an element is added only after find or lookup has failed), so find returns
// the same index whatever the table.
//
//   add(g, parent, letter, depth, label, dist)   append g (packed, so it aborts past the storage guard); its index
//   find(key)                                    the index of the element with this key, or -1
//   lookup(g)                                    find(key(normal(g))), and the check that the element found is
//                                                normal(g): it aborts if the keys agree but the elements differ
//   load(i), key_of(i), path(i), size()          the element, its key, the letters from the root to it, the count
//
// lookup is the legacy Store::lookup (normalise, then check). The legacy Store6::lookup did not check (its find
// compared the last columns only); the check cannot fail for a lattice acting freely, so the indices are the same.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/hash.h"

namespace zippers {

template <class B, class Label>
struct Store {
    typedef typename B::Elem Elem;
    typedef typename B::Key Key;

    std::vector<int32_t> packed;                  // B::packed_size words per element (B::pack)
    std::vector<int32_t> parent, letter, depth;   // parent -1 for the root
    std::vector<Label> label;
    std::vector<float> dist;                      // d(o, g o)
    std::vector<int32_t> slot;                    // the hash table: an index, or -1 for an empty slot
    uint64_t mask = 0;

    Store() { slot.assign(1024, -1); mask = slot.size() - 1; }

    size_t size() const { return parent.size(); }

    static uint64_t hkey(const Key& k)
    {
        uint64_t h = 0x2545f4914f6cdd1dULL;
        for (int i = 0; i < B::key_size; i++) h = splitmix(h ^ (uint64_t)k[i]);
        return h;
    }

    Key key_of(size_t idx) const { return B::packed_key(&packed[(size_t)B::packed_size*idx]); }
    Elem load(size_t idx) const { return B::unpack(&packed[(size_t)B::packed_size*idx]); }

    int find(const Key& k) const
    {
        uint64_t h = hkey(k) & mask;
        while (slot[h] >= 0) {
            if (key_of(slot[h]) == k) return slot[h];
            h = (h + 1) & mask;
        }
        return -1;
    }

    int add(const Elem& g, int par, int let, int dep, const Label& lab, float d)
    {
        int32_t w[B::packed_size];
        B::pack(g, w);
        // one word at a time, as the legacy stores did: the vector then grows (and peaks in memory) as theirs did
        for (int i = 0; i < B::packed_size; i++) packed.push_back(w[i]);
        parent.push_back(par); letter.push_back(let); depth.push_back(dep);
        label.push_back(lab); dist.push_back(d);
        if (2*size() > slot.size()) grow(); else place(size() - 1);
        return (int)size() - 1;
    }

    // look up an arbitrary element (normalising it first)
    int lookup(const Elem& g) const
    {
        Elem h = B::normal(g);
        int f = find(B::key(h));
        if (f >= 0 && !B::eq(load(f), h)) {
            fprintf(stderr, "the keys agree but the elements differ: the action is not free?!\n");
            exit(1);
        }
        return f;
    }

    std::vector<int> path(int idx) const          // the letters from the root to idx
    {
        std::vector<int> w;
        for (int i = idx; parent[i] >= 0; i = parent[i]) w.push_back(letter[i]);
        std::reverse(w.begin(), w.end());
        return w;
    }

private:
    void place(size_t idx)
    {
        uint64_t h = hkey(key_of(idx)) & mask;
        while (slot[h] >= 0) h = (h + 1) & mask;
        slot[h] = (int32_t)idx;
    }
    void grow()
    {
        size_t n = 2*slot.size();
        slot.assign(n, -1);
        mask = n - 1;
        for (size_t i = 0; i < size(); i++) place(i);
    }
};

}  // namespace zippers
