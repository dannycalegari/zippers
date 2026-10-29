// SPDX-License-Identifier: GPL-3.0-or-later
//
// hash.h: the hash functions of the suite.
//
//   splitmix      the SplitMix64 finaliser. Legacy: splitmix in legacy/s3/csgroup.h and legacy/s4/rh5.h (the same
//                 function, copied in both).
//   fingerprint   the renderer's fingerprint of a branch: an exact key (B::Key, see geometry/backend.h) and a letter.
//                 Legacy: fingerprint(col, letter) in legacy/s3/zipcore.h (a key of 12 integers, the normalised third
//                 column) and fingerprint6(g, letter) in legacy/s4/zipcore5.h (6 integers, the last column of 2g).
//                 The two legacy functions are the same up to the length of the key; here it is a template on it.
//
// Both are moved unchanged (tests/unit/ compares them with the legacy functions bit for bit).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace zippers {

inline uint64_t splitmix(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// the fingerprint of a branch with exact key `key` and letter `letter`; never 0 (0 marks an empty slot of a KeySet)
template <size_t K>
inline uint64_t fingerprint(const std::array<int64_t, K>& key, int letter)
{
    uint64_t h = 0x9ae16a3b2f90404fULL ^ ((uint64_t)(letter + 1) * 0xff51afd7ed558ccdULL);
    for (size_t i = 0; i < K; i++) h = splitmix(h ^ (uint64_t)key[i]);
    return h ? h : 1;
}

}  // namespace zippers
