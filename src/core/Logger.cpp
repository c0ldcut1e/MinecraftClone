#include "Logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static std::ofstream s_logFile;
static std::mutex s_mutex;
static std::filesystem::path s_logsDir;

static std::tm safeLocalTime(std::time_t time) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return tm;
}

static std::string getTimeString() {
    std::time_t time = std::time(nullptr);
    std::tm tm       = safeLocalTime(time);

    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
    return buffer;
}

static std::string getFileTimestamp() {
    std::time_t time = std::time(nullptr);
    std::tm tm       = safeLocalTime(time);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm);
    return buffer;
}

static std::filesystem::path getExeDir() {
#ifdef _WIN32
    wchar_t wpath[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, wpath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return std::filesystem::current_path();
    return std::filesystem::path(wpath).parent_path();
#else
    char exePath[4096];
    ssize_t length = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (length <= 0) return std::filesystem::current_path();
    exePath[length] = '\0';
    return std::filesystem::path(exePath).parent_path();
#endif
}

void Logger::init() {
    std::filesystem::path base = getExeDir();
    s_logsDir                  = base / "logs";
    std::filesystem::create_directories(s_logsDir);

    std::filesystem::path latest = s_logsDir / "latest.log";

    if (std::filesystem::exists(latest)) std::filesystem::rename(latest, s_logsDir / (getFileTimestamp() + ".log"));

    s_logFile.open(latest, std::ios::out | std::ios::app);
}

void Logger::shutdown() {
    if (s_logFile.is_open()) s_logFile.close();
}

void Logger::log(const char *level, const char *fmt, ...) {
    std::lock_guard<std::mutex> lock(s_mutex);

    char messageBuffer[2048];

    va_list args;
    va_start(args, fmt);
    vsnprintf(messageBuffer, sizeof(messageBuffer), fmt, args);
    va_end(args);

    std::string timeStr = getTimeString();
    std::string final   = "[" + timeStr + "] [" + level + "]: " + messageBuffer;

#ifndef _WIN32
    const char *color = "\033[0m";
    if (std::string(level) == "INFO") color = "\033[92m";
    if (std::string(level) == "WARN") color = "\033[93m";
    if (std::string(level) == "ERROR") color = "\033[91m";
    std::printf("%s%s\033[0m\n", color, final.c_str());
#else
    std::printf("%s\n", final.c_str());
#endif

    if (s_logFile.is_open()) s_logFile << final << std::endl;
}
