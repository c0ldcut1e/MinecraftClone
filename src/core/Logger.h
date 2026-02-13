#pragma once

#include <string>

class Logger {
public:
    static void init();
    static void shutdown();

    template<typename... Args>
    static void logInfo(const char *fmt, Args... args) {
        log("INFO", fmt, args...);
    }

    template<typename... Args>
    static void logWarn(const char *fmt, Args... args) {
        log("WARN", fmt, args...);
    }

    template<typename... Args>
    static void logError(const char *fmt, Args... args) {
        log("ERROR", fmt, args...);
    }

private:
    static void log(const char *level, const char *fmt, ...);
};
