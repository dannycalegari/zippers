// SPDX-License-Identifier: GPL-3.0-or-later
//
// data_files.h: where the programs find their data files, the substitution files and the file of linked pairs that the
// folder data/ of the source tree holds (data/s3/, data/s4/), and that an install puts in share/zippers/.
//
// THE SEARCH. A file named N, of the sphere G ("s3" or "s4"), is looked for in these places, in this order:
//   1. N contains a '/': N itself (relative to the working folder, or absolute), and no other place;
//   2. the working folder: N;
//   3. each folder D of the environment variable ZIPPERS_DATA (folders separated by ':'): D/G/N, then D/N;
//   4. the data of the program, one folder, decided by where the program is:
//        - a program in the folder of its build: the data/ folder of the source tree it was built from;
//        - any other (an installed program): the installed data folder, at a fixed place relative to the program's own
//          folder (share/zippers/ of the same prefix), so that an installed tree still works when it is moved;
//      in it, G/N.
// The first place that holds an entry of the name N is the one used, whatever the entry is: when it is a readable file,
// that file is read; when it is anything else (a folder, a file that cannot be read, a link to nothing, a device), the
// program stops, and says what is there and where (data_file_missing). The search never passes over what it finds.
// A file found in the working folder keeps its name as given, so that what a program prints about it is what it printed
// when it read the working folder only; a file found anywhere else is named by the path at which it was found (a full
// path, or a relative one through a relative folder of ZIPPERS_DATA), which the programs then print. The search itself
// prints nothing, unless it stops (data_file_missing).
//
// THE PLACES, which CMake gives each program as three compile definitions (cmake/ZippersData.cmake:
// zippers_data_places), and which ZIPPERS_DATA_PLACES gathers:
//   ZIPPERS_BUILD_FOLDER    the folder in which the build puts the program
//   ZIPPERS_SOURCE_DATA     the data/ folder of the source tree
//   ZIPPERS_INSTALLED_DATA  the installed data folder, relative to the folder of the installed program
// A program is "in the folder of its build" when the real path of its own folder is the real path of
// ZIPPERS_BUILD_FOLDER. Without the definitions the search has no place 4.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace zippers {

// the places of a program's data (see the top of the file)
struct DataPlaces {
    const char* build_folder;        // the folder in which the build puts the program ("" if unknown)
    const char* source_data;         // the data/ folder of the source tree
    const char* installed_data;      // the installed data folder, relative to the installed program's folder
};

#if defined(ZIPPERS_BUILD_FOLDER) && defined(ZIPPERS_SOURCE_DATA) && defined(ZIPPERS_INSTALLED_DATA)
#define ZIPPERS_DATA_PLACES (zippers::DataPlaces{ZIPPERS_BUILD_FOLDER, ZIPPERS_SOURCE_DATA, ZIPPERS_INSTALLED_DATA})
#else
#define ZIPPERS_DATA_PLACES (zippers::DataPlaces{"", "", ""})
#endif

// the real path of a file or folder that exists, "" otherwise
inline std::string real_path(const std::string& path)
{
    char buf[PATH_MAX];
    return !path.empty() && realpath(path.c_str(), buf) ? std::string(buf) : std::string();
}

// the path of the folder that holds the file `path` (the part before its last '/'; "." if there is none)
inline std::string folder_of(const std::string& path)
{
    size_t k = path.rfind('/');
    return k == std::string::npos ? std::string(".") : k == 0 ? std::string("/") : path.substr(0, k);
}

// a path with its "." and "name/.." parts taken out, as text (for a folder that may not exist)
inline std::string tidy_path(const std::string& path)
{
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string::npos) j = path.size();
        std::string p = path.substr(i, j - i);
        if (p == "..") {
            if (!parts.empty() && parts.back() != "..") parts.pop_back();
            else if (path[0] != '/') parts.push_back(p);
        } else if (!p.empty() && p != ".") parts.push_back(p);
        i = j + 1;
    }
    std::string s = path[0] == '/' ? "/" : "";
    for (size_t k = 0; k < parts.size(); k++) s += (k ? "/" : "") + parts[k];
    return s.empty() ? std::string(".") : s;
}

// the real path of the running program, "" if it cannot be found
inline std::string own_path()
{
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, 0);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return "";
    return real_path(buf.data());
#elif defined(__linux__)
    return real_path("/proc/self/exe");
#else
    return "";
#endif
}

// whether the running program is in the folder of its build (see the top of the file)
inline bool in_build_folder(const DataPlaces& p)
{
    std::string self = own_path(), build = real_path(p.build_folder ? p.build_folder : "");
    return !self.empty() && !build.empty() && folder_of(self) == build;
}

// the data folder of the running program (place 4), and in `what` how the messages name it: "the data of this build" or
// "the installed data". "" if it cannot be told (the program's own path cannot be found, or there are no places).
inline std::string program_data(const DataPlaces& p, std::string* what = nullptr)
{
    std::string self = own_path();
    if (self.empty()) return "";
    if (in_build_folder(p)) {
        if (what) *what = "the data of this build";
        return p.source_data ? p.source_data : "";
    }
    if (!p.installed_data || !*p.installed_data) return "";
    if (what) *what = "the installed data";
    std::string dir = folder_of(self) + "/" + p.installed_data;
    std::string r = real_path(dir);
    return r.empty() ? tidy_path(dir) : r;
}

// what is at `path`: nothing (no entry of that name), a readable file, or something else, which `what` then describes
enum class Entry { none, file, other };
inline Entry entry_at(const std::string& path, std::string* what = nullptr)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return Entry::none;
    const char* w = nullptr;
    if (stat(path.c_str(), &st) != 0) w = "a link to nothing";
    else if (S_ISDIR(st.st_mode)) w = "a folder";
    else if (!S_ISREG(st.st_mode)) w = "an entry that is not a regular file";
    else if (access(path.c_str(), R_OK) != 0) w = "a file that cannot be read";
    if (!w) return Entry::file;
    if (what) *what = w;
    return Entry::other;
}

// whether `path` is a file that can be read
inline bool readable_file(const std::string& path) { return entry_at(path) == Entry::file; }

// how ZIPPERS_DATA stands, for the messages: "not set", "set to the empty string", "names no folder" (only ':'), or ""
// when it names at least one folder
inline std::string data_env_state()
{
    const char* e = getenv("ZIPPERS_DATA");
    if (!e) return "not set";
    if (!*e) return "set to the empty string";
    return std::string(e).find_first_not_of(':') == std::string::npos ? "names no folder" : "";
}

// the folders of ZIPPERS_DATA, in order (empty when it is not set, or names none)
inline std::vector<std::string> data_env_folders()
{
    std::vector<std::string> v;
    const char* e = getenv("ZIPPERS_DATA");
    if (!e) return v;
    std::string s = e;
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find(':', i);
        if (j == std::string::npos) j = s.size();
        if (j > i) v.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return v;
}

// what the search found: the name to open ("" if there is none); the places it looked in before, one line each, for the
// message; and, when the first place that holds an entry of the name holds no readable file, what is there and where
struct DataFile {
    std::string path;
    std::vector<std::string> looked;
    std::string problem;
};

// look for the data file `name` of the sphere `sphere` ("s3" or "s4"): see the top of the file. A name with a '/' is
// returned as it is, whether or not the file exists: its reader says so if it does not.
inline DataFile find_data_file(const std::string& name, const char* sphere, const DataPlaces& p)
{
    DataFile r;
    if (name.find('/') != std::string::npos) {
        r.path = name;
        return r;
    }
    // one place: true when the search stops there (a readable file, or an entry of the name that is something else)
    auto at = [&](const std::string& place, const std::string& open, const std::string& shown) -> bool {
        std::string what;
        Entry e = entry_at(open, &what);
        if (e == Entry::file) r.path = open;
        else if (e == Entry::other) r.problem = "it is " + what + ", in " + place + ": " + shown;
        else r.looked.push_back(place + ": " + shown);
        return e != Entry::none;
    };
    char cwd[PATH_MAX];
    if (at("the working folder", name, std::string(getcwd(cwd, sizeof cwd) ? cwd : ".") + "/" + name)) return r;
    std::vector<std::string> env = data_env_folders();
    std::string state = data_env_state();
    if (!state.empty()) r.looked.push_back("ZIPPERS_DATA: " + state);
    for (const std::string& d : env)
        for (const std::string& c : {d + "/" + sphere + "/" + name, d + "/" + name})
            if (at("ZIPPERS_DATA", c, c)) return r;
    std::string what, dir = program_data(p, &what);
    if (dir.empty()) {
        r.looked.push_back("the program's own data: none (its own place, or its data folder, is not known)");
        return r;
    }
    std::string c = dir + "/" + sphere + "/" + name;
    at(what, c, c);
    return r;
}

// the message when the search stops with no file to open (on stderr): the data file `name` was found nowhere, or the
// first place that holds an entry of its name holds no readable file; `what` says what the file is ("the default
// substitution file", or null), `option` how to give its path ("-f", or "--pairs")
inline void data_file_missing(const std::string& name, const DataFile& r, const char* what, const char* option)
{
    std::string w = what ? std::string(" (") + what + ")" : std::string();
    if (!r.problem.empty()) {
        fprintf(stderr, "cannot use %s%s: %s\n", name.c_str(), w.c_str(), r.problem.c_str());
        fprintf(stderr, "a data file is taken from the first place that holds its name; give the path of another file with "
                "%s\n", option);
        return;
    }
    fprintf(stderr, "cannot find %s%s; looked in:\n", name.c_str(), w.c_str());
    for (const std::string& l : r.looked) fprintf(stderr, "  %s\n", l.c_str());
    fprintf(stderr, "give its path with %s, or set ZIPPERS_DATA to a folder that holds s3/ and s4/ as the folder data/ of "
            "the source tree does\n", option);
}

// find the data file `name` in place: true, with `name` replaced by the name to open; or false, after the message
inline bool find_data(std::string& name, const char* sphere, const DataPlaces& p, const char* what, const char* option)
{
    DataFile r = find_data_file(name, sphere, p);
    if (r.path.empty()) {
        data_file_missing(name, r, what, option);
        return false;
    }
    name = r.path;
    return true;
}

}  // namespace zippers
