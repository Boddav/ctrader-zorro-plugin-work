#include "../include/utils.h"
#include "../include/auth.h"
#include <ctime>
#include <fstream>
#include <algorithm>

namespace Utils {

void ShowMsg(const char* text, const char* detail) {
    static char msg[4096];
    if (detail && *detail) {
        sprintf_s(msg, "%s %s", text, detail);
    } else {
        strcpy_s(msg, sizeof(msg), text);
    }

    LogToFile("MESSAGE", msg);
    if (BrokerMessage) BrokerMessage(msg);
}

void LogToFile(const char* type, const char* message) {
    if (!type || !message) return;

    EnterCriticalSection(&G.cs_log);

    char path[MAX_PATH];
    sprintf_s(path, "%swesocket.txt", G.DllPath);

    FILE* f = nullptr;
    if (fopen_s(&f, path, "a") == 0 && f) {
        time_t t = time(nullptr);
        struct tm tmv;
        localtime_s(&tmv, &t);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
        fprintf(f, "[%s] %s: %s\n", ts, type, message);
        fclose(f);
    }

    LeaveCriticalSection(&G.cs_log);
}

const char* GetMsgId() {
    static int id = 0;
    static char buffer[32];
    sprintf_s(buffer, "msg_%d", ++id);
    return buffer;
}

bool BodyIndicatesError(const char* buffer) {
    if (!buffer || strlen(buffer) == 0) return true;
    std::string body(buffer);
    return Auth::BodyIndicatesError(body);
}

bool StrContainsCaseInsensitive(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    std::string h(haystack), n(needle);
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

}