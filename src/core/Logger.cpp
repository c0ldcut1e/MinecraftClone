#include "Logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unistd.h>

static std::ofstream g_logFile;
static std::mutex g_mutex;
static std::filesystem::path g_logsDir;

static std::string getTimeString() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_r(&t, &tm);

    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
    return buffer;
}

static std::string getFileTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_r(&t, &tm);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &tm);
    return buffer;
}

void Logger::init() {
    char exePath[1024];
    ssize_t len  = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    exePath[len] = '\0';

    std::filesystem::path base = std::filesystem::path(exePath).parent_path();
    g_logsDir                  = base / "logs";
    std::filesystem::create_directories(g_logsDir);

    std::filesystem::path latest = g_logsDir / "latest.log";

    if (std::filesystem::exists(latest)) std::filesystem::rename(latest, g_logsDir / (getFileTimestamp() + ".log"));

    g_logFile.open(latest, std::ios::out | std::ios::app);
}

void Logger::shutdown() {
    if (g_logFile.is_open()) g_logFile.close();
}

void Logger::log(const char *level, const char *fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);

    char messageBuffer[2048];

    va_list args;
    va_start(args, fmt);
    vsnprintf(messageBuffer, sizeof(messageBuffer), fmt, args);
    va_end(args);

    std::string timeStr = getTimeString();
    std::string final   = "[" + timeStr + "] [" + level + "]: " + messageBuffer;

    const char *color = "\033[0m";
    if (std::string(level) == "INFO") color = "\033[92m";
    if (std::string(level) == "WARN") color = "\033[93m";
    if (std::string(level) == "ERROR") color = "\033[91m";

    printf("%s%s\033[0m\n", color, final.c_str());

    if (g_logFile.is_open()) g_logFile << final << std::endl;
}
