#pragma once
#include <cstdio>
#include <cstdarg>

void log(const char file[], int line, const char* format, ...);

namespace Phoenix {

struct Debug {
    static void Log(const char* msg){
        ::log("Script", 0, "%s", msg);
    }

    static void LogWarning(const char* msg){
        ::log("Script", 0, "[WARNING] %s", msg);
    }

    static void LogError(const char* msg){
        ::log("Script", 0, "[ERROR] %s", msg);
    }

    static void LogFormat(const char* fmt, ...){
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        ::log("Script", 0, "%s", buf);
    }
};

} // namespace Phoenix
