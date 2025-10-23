#include "../include/utils.h"
#include "../include/auth.h"
#include <ctime>
#include <fstream>
#include <algorithm>

namespace Utils {

namespace {

const char* LevelString(UserMessageType type) {
    switch (type) {
    case UserMessageType::Success:
        return "SUCCESS";
    case UserMessageType::Warning:
        return "WARNING";
    case UserMessageType::Error:
        return "ERROR";
    default:
        return "INFO";
    }
}

} // namespace

void Notify(UserMessageType type,
            const char* tag,
            const char* text,
            const char* detail,
            bool surfaceToHost) {
    if (!text) {
        return;
    }

    static char composed[4096];
    static char formatted[4096];

    if (detail && *detail) {
        sprintf_s(composed, "%s %s", text, detail);
    } else {
        strcpy_s(composed, sizeof(composed), text);
    }

    const char* level = LevelString(type);
    if (type == UserMessageType::Info) {
        strcpy_s(formatted, sizeof(formatted), composed);
    } else {
        sprintf_s(formatted, "[%s] %s", level, composed);
    }

    const char* logLabel = (tag && *tag) ? tag : level;
    LogToFile(logLabel, formatted);

    if (surfaceToHost && BrokerMessage) {
        BrokerMessage(formatted);
    }
}

void ShowMsg(const char* text, const char* detail) {
    Notify(UserMessageType::Info, "MESSAGE", text, detail, true);
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

// New functions from enhanced utils.h

std::string ReplaceAll(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

bool StrContainsCaseInsensitive(const std::string& str, const std::string& substr) {
    std::string lowerStr = str;
    std::string lowerSub = substr;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    std::transform(lowerSub.begin(), lowerSub.end(), lowerSub.begin(), ::tolower);
    return lowerStr.find(lowerSub) != std::string::npos;
}

// Enhanced LogToFile with two parameters - redirect to the existing one
void LogToFileEx(const char* line1, const char* line2) {
    if (!line1) return;
    if (line2 && strlen(line2) > 0) {
        std::string combined = std::string(line1) + " " + std::string(line2);
        LogToFile("COMBINED", combined.c_str());
    } else {
        LogToFile("LOG", line1);
    }
}

std::string GetErrorDescription(const char* buffer) {
    if (!buffer) return "Unknown error";

    std::string description;

    // Try to extract error description
    const char* pDesc = strstr(buffer, "\"description\":\"");
    if (pDesc) {
        pDesc += 15; // Skip "description":"
        const char* pEnd = strchr(pDesc, '"');
        if (pEnd) {
            description = std::string(pDesc, pEnd - pDesc);
        }
    }

    // Try alternative error message fields
    if (description.empty()) {
        const char* pError = strstr(buffer, "\"errorMessage\":\"");
        if (pError) {
            pError += 16;
            const char* pEnd = strchr(pError, '"');
            if (pEnd) {
                description = std::string(pError, pEnd - pError);
            }
        }
    }

    // Extract error code if available
    const char* pErrorCode = strstr(buffer, "\"errorCode\":");
    if (pErrorCode) {
        int errorCode = 0;
        sscanf_s(pErrorCode, "\"errorCode\":%d", &errorCode);
        if (errorCode != 0) {
            char codeStr[32];
            sprintf_s(codeStr, " (Code: %d)", errorCode);
            description += codeStr;
        }
    }

    return description.empty() ? "Unknown error" : description;
}

std::string FormatError(const char* context, const char* error) {
    std::string formatted = "ERROR";
    if (context && strlen(context) > 0) {
        formatted += " in ";
        formatted += context;
    }
    if (error && strlen(error) > 0) {
        formatted += ": ";
        formatted += error;
    }
    return formatted;
}

bool ContainsPayloadType(const char* buffer, PayloadType type) {
    if (!buffer) {
        return false;
    }

    char pattern[32];
    sprintf_s(pattern, "\"payloadType\":%d", ToInt(type));
    return strstr(buffer, pattern) != nullptr;
}

} // namespace Utils

