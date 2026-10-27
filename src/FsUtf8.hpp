// Kinetik — UTF-8 file helpers (identical in every Kinetik DPF repository).
// Every path in the plug-ins is UTF-8. On Windows the ANSI C functions (getenv, fopen, _mkdir, opendir) break
// user folders whose name is outside the system code page, so these helpers go through the wide (UTF-16)
// C runtime instead — without including windows.h (it clashes with DPF).
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace kn {
namespace fs {

inline std::wstring widen(const std::string& s)
{
    std::wstring w;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        uint32_t cp = c; size_t n = 1;
        if (c >= 0xF0 && i + 3 < s.size()) { cp = (c & 7u) << 18 | ((unsigned char)s[i + 1] & 63u) << 12 | ((unsigned char)s[i + 2] & 63u) << 6 | ((unsigned char)s[i + 3] & 63u); n = 4; }
        else if (c >= 0xE0 && i + 2 < s.size()) { cp = (c & 15u) << 12 | ((unsigned char)s[i + 1] & 63u) << 6 | ((unsigned char)s[i + 2] & 63u); n = 3; }
        else if (c >= 0xC0 && i + 1 < s.size()) { cp = (c & 31u) << 6 | ((unsigned char)s[i + 1] & 63u); n = 2; }
        if (sizeof(wchar_t) == 2 && cp >= 0x10000) { cp -= 0x10000; w += (wchar_t)(0xD800 + (cp >> 10)); w += (wchar_t)(0xDC00 + (cp & 0x3FF)); }
        else w += (wchar_t)cp;
        i += n;
    }
    return w;
}
inline std::string narrow(const std::wstring& w)
{
    std::string s;
    for (size_t i = 0; i < w.size(); ++i) {
        uint32_t cp = (uint32_t)w[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < w.size()) { cp = 0x10000 + ((cp - 0xD800) << 10) + ((uint32_t)w[i + 1] - 0xDC00); ++i; }
        if (cp < 0x80) s += (char)cp;
        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
        else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 63)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
    }
    return s;
}

// environment variable as UTF-8 ("" if unset)
inline std::string env(const char* name)
{
#ifdef _WIN32
    const wchar_t* v = _wgetenv(widen(name).c_str());
    return v ? narrow(v) : std::string();
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}
inline void makeDir(const std::string& d)
{
#ifdef _WIN32
    _wmkdir(widen(d).c_str());
#else
    mkdir(d.c_str(), 0755);
#endif
}
inline FILE* open(const std::string& path, const char* mode)
{
#ifdef _WIN32
    return _wfopen(widen(path).c_str(), widen(mode).c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}
inline bool remove(const std::string& path)
{
#ifdef _WIN32
    return _wremove(widen(path).c_str()) == 0;
#else
    return std::remove(path.c_str()) == 0;
#endif
}
// <Documents>/Kinetik/<sub>/ (created), for files the user takes elsewhere (MIDI exports); "" if no home folder
inline std::string exportDir(const std::string& sub)
{
#ifdef _WIN32
    std::string d = env("USERPROFILE");
    if (d.empty()) return std::string();
    d += "\\Documents"; makeDir(d); d += "\\Kinetik"; makeDir(d); d += "\\" + sub; makeDir(d);
    return d + "\\";
#else
    std::string d = env("HOME");
    if (d.empty()) return std::string();
    d += "/Documents"; makeDir(d); d += "/Kinetik"; makeDir(d); d += "/" + sub; makeDir(d);
    return d + "/";
#endif
}
// file names (not paths) in dir ending with ext
inline std::vector<std::string> list(const std::string& dir, const std::string& ext)
{
    std::vector<std::string> out;
#ifdef _WIN32
    _wfinddata_t fd;
    const intptr_t h = _wfindfirst(widen(dir + "*" + ext).c_str(), &fd);
    if (h == -1) return out;
    do {
        if (!(fd.attrib & _A_SUBDIR)) out.push_back(narrow(fd.name));
    } while (_wfindnext(h, &fd) == 0);
    _findclose(h);
#else
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string f = e->d_name;
            if (f.size() > ext.size() && f.compare(f.size() - ext.size(), ext.size(), ext) == 0) out.push_back(f);
        }
        closedir(d);
    }
#endif
    return out;
}

// sub-folder names (not paths) of dir ("." / ".." and hidden folders left out)
inline std::vector<std::string> listDirs(const std::string& dir)
{
    std::vector<std::string> out;
#ifdef _WIN32
    _wfinddata_t fd;
    const intptr_t h = _wfindfirst(widen(dir + "*").c_str(), &fd);
    if (h == -1) return out;
    do {
        const std::string n = narrow(fd.name);
        if ((fd.attrib & _A_SUBDIR) && !n.empty() && n[0] != '.') out.push_back(n);
    } while (_wfindnext(h, &fd) == 0);
    _findclose(h);
#else
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.empty() || n[0] == '.') continue;
            struct stat st;
            if (stat((dir + n).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) out.push_back(n);
        }
        closedir(d);
    }
#endif
    return out;
}

} // namespace fs
} // namespace kn
