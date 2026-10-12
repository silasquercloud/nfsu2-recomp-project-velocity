#pragma once
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <stdexcept>
#include <android/log.h>

constexpr const char* kGameRoot = "/storage/emulated/0/nfsu2";
inline void checkGameData() {
    const std::string root(kGameRoot);
    FILE* exe = std::fopen((root + "/SPEED2.EXE").c_str(), "rb");
    if (!exe) throw std::runtime_error("exe_read_error");
    unsigned char magic[2]{};
    const size_t read = std::fread(magic, 1, 2, exe);
    std::fclose(exe);
    struct stat info{};
    if (read != 2 || magic[0] != 'M' || magic[1] != 'Z' ||
        stat((root + "/SPEED2.EXE").c_str(), &info) || info.st_size != 4800512)
        throw std::runtime_error("exe_mismatch");
    for (const char* name : {"CARS", "CREDITS", "FRONTEND", "GLOBAL", "LANGUAGES", "MOVIES",
                             "NIS", "SDATA", "SOUND", "SUBTITLES", "TRACKS"}) {
        const auto path = root + "/" + name;
        DIR* dir = opendir(path.c_str());
        if (!dir) throw std::runtime_error("directory_read_error:" + path);
        unsigned files = 0;
        while (auto* entry = readdir(dir)) if (entry->d_name[0] != '.') ++files;
        closedir(dir);
        if (!files) throw std::runtime_error("directory_empty:" + path);
        __android_log_print(ANDROID_LOG_INFO, "NFSU2", "Data directory %s: %u entries", name, files);
    }
    __android_log_print(ANDROID_LOG_INFO, "NFSU2", "Game data readable at %s; executable header/size OK (guest not running)", kGameRoot);
}
