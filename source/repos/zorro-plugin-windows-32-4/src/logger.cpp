#include "../include/state.h"
#include "../include/logger.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>

namespace Log {

static void WriteToFile(const char* tag, const char* message) {
    if (!tag || !message) return;

    CsLock lock(G.csLog);

    char path[MAX_PATH];
    sprintf_s(path, "%scTrader.log", G.dllDir);

    FILE* f = nullptr;
    if (fopen_s(&f, path, "a") == 0 && f) {
        time_t t = time(nullptr);
        struct tm tmv;
        localtime_s(&tmv, &t);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
        fprintf(f, "[%s] %s: %s\n", ts, tag, message);
        fclose(f);
    }
}

void ToFile(const char* tag, const char* message) {
    WriteToFile(tag, message);
}

void Info(const char* tag, const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    WriteToFile(tag ? tag : "INFO", buf);
}

void Warn(const char* tag, const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char full[4096];
    sprintf_s(full, "[WARNING] %s", buf);
    WriteToFile(tag ? tag : "WARN", full);
}

void Error(const char* tag, const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char full[4096];
    sprintf_s(full, "[ERROR] %s", buf);
    WriteToFile(tag ? tag : "ERROR", full);

    if (BrokerMessage) {
        BrokerMessage(full);
    }
}

void Msg(const char* text) {
    if (!text) return;
    WriteToFile("MSG", text);
    if (BrokerMessage) {
        BrokerMessage(text);
    }
}

void Diag(int level, const char* fmt, ...) {
    if (G.diagLevel < level) return;

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    WriteToFile("DIAG", buf);
    if (G.diagLevel >= 2 && BrokerMessage) {
        BrokerMessage(buf);
    }
}

} // namespace Log
