// fs_posix.hpp
#pragma once
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

inline bool path_exists(const std::string& p) {
    struct stat st{};
    return ::lstat(p.c_str(), &st) == 0;
}

inline bool is_dir(const std::string& p) {
    struct stat st{};
    return ::lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

inline bool is_symlink(const std::string& p) {
    struct stat st{};
    return ::lstat(p.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

inline bool is_reg(const std::string& p) {
    struct stat st{};
    return ::lstat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

inline std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty() || a.back() == '/') return a + b;
    return a + "/" + b;
}

inline std::vector<std::string> list_dir(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = ::opendir(dir.c_str());
    if (!d) return out;
    while (auto* de = ::readdir(d)) {
        if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0'))) continue;
        out.push_back(join_path(dir, de->d_name));
    }
    ::closedir(d);
    return out;
}

inline std::string readlink_once(const std::string& p) {
    std::string buf;
    buf.resize(4096);
    ssize_t n = ::readlink(p.c_str(), &buf[0], buf.size()-1);
    if (n < 0) return {};
    buf[n] = '\0';
    buf.resize(static_cast<size_t>(n));
    // readlink может вернуть относительный путь — нормализуйте при необходимости
    if (!buf.empty() && buf[0] != '/') {
        // вернуть абсолютом относительно родителя p
        auto pos = p.find_last_of('/');
        std::string base = (pos == std::string::npos) ? "." : p.substr(0, pos);
        return join_path(base, buf);
    }
    return buf;
}