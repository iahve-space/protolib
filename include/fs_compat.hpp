#pragma once
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cerrno>


static inline bool ends_with(const std::string& s, const char* suf) {
    const size_t n = std::strlen(suf);
    return s.size() >= n && std::memcmp(s.data() + (s.size() - n), suf, n) == 0;
}

static inline bool is_regular_file_posix(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static inline std::vector<std::string> list_bin_files(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (auto* de = readdir(d)) {
        if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0) continue;
        std::string p = dir;
        if (!p.empty() && p.back() != '/') p.push_back('/');
        p += de->d_name;
        if (is_regular_file_posix(p) && ends_with(p, ".bin")) out.push_back(p);
    }
    closedir(d);
    return out;
}