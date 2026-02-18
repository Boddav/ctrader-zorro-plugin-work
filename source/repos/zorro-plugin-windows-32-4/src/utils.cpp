#include "../include/state.h"
#include "../include/utils.h"
#include <cstdio>
#include <algorithm>

namespace Utils {

const char* NextMsgId() {
    __declspec(thread) static char buffer[32];
    int id = InterlockedIncrement((volatile LONG*)&G.msgIdCounter);
    sprintf_s(buffer, "msg_%d", id);
    return buffer;
}

DATE UnixToOle(long long unixMs) {
    // OLE DATE: days since Dec 30, 1899
    // Unix epoch (Jan 1, 1970) = OLE day 25569
    return (double)unixMs / 86400000.0 + 25569.0;
}

long long OleToUnix(DATE oleDate) {
    return (long long)((oleDate - 25569.0) * 86400000.0);
}

DATE MinutesToOle(long long utcMinutes) {
    // Convert minutes since Unix epoch to OLE DATE
    long long unixMs = (long long)utcMinutes * 60000LL;
    return UnixToOle(unixMs);
}

std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

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

bool ContainsCI(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    std::string h(haystack), n(needle);
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

ULONGLONG NowMs() {
    return GetTickCount64();
}

} // namespace Utils
