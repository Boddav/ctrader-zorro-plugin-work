#pragma once
#include "globals.h"

namespace Utils {
    void ShowMsg(const char* text, const char* detail = "");
    void LogToFile(const char* type, const char* message);
    const char* GetMsgId();
    bool BodyIndicatesError(const char* buffer);
    bool StrContainsCaseInsensitive(const char* haystack, const char* needle);
}