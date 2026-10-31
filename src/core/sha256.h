// SPDX-License-Identifier: GPL-3.0-or-later
//
// sha256.h: SHA-256 (FIPS 180-4, section 6.2), for the hashes of the data files that a session names. namespace zippers.
//
//   Sha256        the hash of a stream of bytes: update() any number of times, then hex() (64 lowercase hex digits) or
//                 digest() (32 bytes)
//   sha256_hex    the hash of a string
//   sha256_file   the hash of a file's bytes; false, with the reason, if it cannot be read
//
// Written from the standard: the eight initial words and the 64 round constants are the fractional parts of the square
// and cube roots of the first primes (FIPS 180-4, 5.3.3 and 4.2.2); the message is padded with a 1 bit, zeros and its
// length in bits as a big-endian 64-bit number, to a multiple of 512 bits (5.1.1).

#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace zippers {

class Sha256 {
public:
    Sha256() { reset(); }

    void reset()
    {
        static const uint32_t H0[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                       0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        memcpy(h_, H0, sizeof h_);
        n_ = 0;
        len_ = 0;
    }

    void update(const void* data, size_t size)
    {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        len_ += (uint64_t)size;
        while (size > 0) {
            size_t k = 64 - n_ < size ? 64 - n_ : size;
            memcpy(buf_ + n_, p, k);
            n_ += k;
            p += k;
            size -= k;
            if (n_ == 64) {
                block(buf_);
                n_ = 0;
            }
        }
    }
    void update(const std::string& s) { update(s.data(), s.size()); }

    // the hash of everything given; the object is then reset
    std::array<unsigned char, 32> digest()
    {
        const uint64_t bits = len_ * 8;
        const unsigned char one = 0x80, zero = 0;
        update(&one, 1);
        while (n_ != 56) update(&zero, 1);
        unsigned char L[8];
        for (int k = 0; k < 8; k++) L[k] = (unsigned char)(bits >> (56 - 8*k));
        update(L, 8);
        std::array<unsigned char, 32> d;
        for (int i = 0; i < 8; i++)
            for (int k = 0; k < 4; k++) d[(size_t)(4*i + k)] = (unsigned char)(h_[i] >> (24 - 8*k));
        reset();
        return d;
    }

    std::string hex()
    {
        static const char* const X = "0123456789abcdef";
        std::string s;
        for (unsigned char c : digest()) {
            s += X[c >> 4];
            s += X[c & 15];
        }
        return s;
    }

private:
    uint32_t h_[8];
    unsigned char buf_[64];
    size_t n_;
    uint64_t len_;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const unsigned char* b)
    {
        static const uint32_t K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32_t w[64];
        for (int t = 0; t < 16; t++)
            w[t] = (uint32_t)b[4*t] << 24 | (uint32_t)b[4*t + 1] << 16 | (uint32_t)b[4*t + 2] << 8 | (uint32_t)b[4*t + 3];
        for (int t = 16; t < 64; t++) {
            uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        uint32_t a = h_[0], b2 = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int t = 0; t < 64; t++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g);
            uint32_t T1 = h + S1 + ch + K[t] + w[t];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), maj = (a & b2) ^ (a & c) ^ (b2 & c);
            uint32_t T2 = S0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b2;
            b2 = a;
            a = T1 + T2;
        }
        h_[0] += a; h_[1] += b2; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }
};

inline std::string sha256_hex(const std::string& data)
{
    Sha256 s;
    s.update(data);
    return s.hex();
}

// the hash of the file's bytes, in hex; false, with the reason in *err, if it cannot be read
inline bool sha256_file(const std::string& path, std::string& hex, std::string* err = nullptr)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot read " + path + ": " + strerror(errno);
        return false;
    }
    Sha256 s;
    unsigned char buf[1 << 16];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) s.update(buf, k);
    const bool bad = ferror(f) != 0;
    fclose(f);
    if (bad) {
        if (err) *err = "cannot read " + path;
        return false;
    }
    hex = s.hex();
    return true;
}

}  // namespace zippers
