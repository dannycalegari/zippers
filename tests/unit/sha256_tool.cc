// SPDX-License-Identifier: GPL-3.0-or-later
//
// sha256_tool FILE...: the SHA-256 of each file (src/core/sha256.h), one line each: 64 hexadecimal digits, two blanks,
// the file. Exit status 1 if a file cannot be read. For test_sha256.py, which holds it to Python's hashlib.

#include "core/sha256.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    int bad = 0;
    for (int i = 1; i < argc; i++) {
        std::string hex, err;
        if (zippers::sha256_file(argv[i], hex, &err)) printf("%s  %s\n", hex.c_str(), argv[i]);
        else {
            fprintf(stderr, "%s\n", err.c_str());
            bad = 1;
        }
    }
    return bad;
}
