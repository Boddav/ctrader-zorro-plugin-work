#pragma once
#include <string>

typedef double DATE;

namespace Utils {

// Generate unique message ID (msg_1, msg_2, ...)
const char* NextMsgId();

// OLE DATE <-> Unix timestamp conversion
DATE UnixToOle(long long unixMs);
long long OleToUnix(DATE oleDate);

// Minutes timestamp to OLE DATE
DATE MinutesToOle(long long utcMinutes);

// String helpers
std::string Trim(const std::string& str);
std::string ReplaceAll(const std::string& str, const std::string& from, const std::string& to);
bool ContainsCI(const char* haystack, const char* needle);

// Get current time in ms
ULONGLONG NowMs();

} // namespace Utils
