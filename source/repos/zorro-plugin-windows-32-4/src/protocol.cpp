#include "../include/state.h"
#include "../include/protocol.h"
#include "../include/logger.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace Protocol {

const char* BuildMessage(const char* clientMsgId, PayloadType type, const char* payloadJson) {
    __declspec(thread) static char buffer[8192];
    if (payloadJson && *payloadJson) {
        sprintf_s(buffer, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{%s}}",
                  clientMsgId, ToInt(type), payloadJson);
    } else {
        sprintf_s(buffer, "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{}}",
                  clientMsgId, ToInt(type));
    }
    return buffer;
}

int ExtractPayloadType(const char* buffer) {
    if (!buffer) return 0;
    const char* p = strstr(buffer, "\"payloadType\":");
    if (!p) return 0;
    p += 14; // strlen("\"payloadType\":")
    return atoi(p);
}

bool ContainsPayloadType(const char* buffer, PayloadType type) {
    return ExtractPayloadType(buffer) == ToInt(type);
}

// Internal helper: find value after "fieldName": in JSON
static const char* FindField(const char* buffer, const char* fieldName) {
    if (!buffer || !fieldName) return nullptr;

    // Build search pattern: "fieldName":
    char pattern[256];
    sprintf_s(pattern, "\"%s\":", fieldName);

    const char* p = strstr(buffer, pattern);
    if (!p) return nullptr;

    p += strlen(pattern);

    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    return p;
}

const char* ExtractString(const char* buffer, const char* fieldName) {
    __declspec(thread) static char result[2048];
    result[0] = '\0';

    const char* p = FindField(buffer, fieldName);
    if (!p) return result;

    // String value starts with "
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < (int)sizeof(result) - 1) {
            if (*p == '\\' && *(p + 1)) {
                p++; // skip escape char
                if (*p == 'n') result[i++] = '\n';
                else if (*p == 't') result[i++] = '\t';
                else if (*p == '"') result[i++] = '"';
                else if (*p == '\\') result[i++] = '\\';
                else result[i++] = *p;
            } else {
                result[i++] = *p;
            }
            p++;
        }
        result[i] = '\0';
    }
    // Handle unquoted values (numbers as strings, booleans)
    else {
        int i = 0;
        while (*p && *p != ',' && *p != '}' && *p != ']' && i < (int)sizeof(result) - 1) {
            result[i++] = *p++;
        }
        result[i] = '\0';
    }

    return result;
}

long long ExtractInt64(const char* buffer, const char* fieldName) {
    const char* p = FindField(buffer, fieldName);
    if (!p) return 0;

    // Handle quoted numbers
    if (*p == '"') p++;

    return _atoi64(p);
}

double ExtractDouble(const char* buffer, const char* fieldName) {
    const char* p = FindField(buffer, fieldName);
    if (!p) return 0.0;

    if (*p == '"') p++;

    return atof(p);
}

bool ExtractBool(const char* buffer, const char* fieldName) {
    const char* p = FindField(buffer, fieldName);
    if (!p) return false;
    return (*p == 't' || *p == 'T' || *p == '1');
}

int ExtractInt(const char* buffer, const char* fieldName) {
    const char* p = FindField(buffer, fieldName);
    if (!p) return 0;
    if (*p == '"') p++;
    return atoi(p);
}

const char* ExtractArray(const char* buffer, const char* fieldName) {
    // Bug #17: fixed-size TLS buffer instead of malloc (no leak on thread exit)
    // Increased from 128KB to 2MB to handle large trendbar responses (8000+ bars)
    __declspec(thread) static char arrayBuf[2 * 1024 * 1024];
    static char emptyBuf[4] = "";  // read-only, safe to share

    const char* p = FindField(buffer, fieldName);
    if (!p || *p != '[') return emptyBuf;

    // Find matching ]
    int depth = 0;
    const char* start = p;
    while (*p) {
        if (*p == '[') depth++;
        else if (*p == ']') {
            depth--;
            if (depth == 0) {
                size_t len = (p - start + 1);
                if (len >= sizeof(arrayBuf)) {
                    Log::Warn("PROTO", "Array too large: %zu bytes (max %zu)", len, sizeof(arrayBuf));
                    return emptyBuf;
                }
                memcpy(arrayBuf, start, len);
                arrayBuf[len] = '\0';
                return arrayBuf;
            }
        }
        p++;
    }

    return emptyBuf;
}

int CountArrayElements(const char* arrayStr) {
    if (!arrayStr || *arrayStr != '[') return 0;

    // Empty array
    const char* p = arrayStr + 1;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == ']') return 0;

    int count = 1;
    int depth = 0;
    bool inString = false;

    p = arrayStr + 1;
    while (*p && !(*p == ']' && depth == 0)) {
        if (*p == '"' && *(p - 1) != '\\') inString = !inString;
        if (!inString) {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') depth--;
            else if (*p == ',' && depth == 0) count++;
        }
        p++;
    }

    return count;
}

const char* GetArrayElement(const char* arrayStr, int index) {
    __declspec(thread) static char elemBuf[8192];
    elemBuf[0] = '\0';

    if (!arrayStr || *arrayStr != '[') return elemBuf;

    const char* p = arrayStr + 1;
    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    int currentIndex = 0;
    while (*p && *p != ']') {
        if (currentIndex == index) {
            // Find end of this element
            const char* start = p;
            int depth = 0;
            bool inString = false;

            while (*p) {
                if (*p == '"' && (p == start || *(p - 1) != '\\')) inString = !inString;
                if (!inString) {
                    if (*p == '{' || *p == '[') depth++;
                    else if (*p == '}' || *p == ']') {
                        depth--;
                        if (depth < 0) break;  // end of array
                    }
                    else if (*p == ',' && depth == 0) break;
                }
                p++;
            }

            size_t len = p - start;
            if (len < sizeof(elemBuf)) {
                memcpy(elemBuf, start, len);
                elemBuf[len] = '\0';
            }
            return elemBuf;
        }

        // Skip to next element
        int depth = 0;
        bool inString = false;
        while (*p) {
            if (*p == '"' && *(p - 1) != '\\') inString = !inString;
            if (!inString) {
                if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') {
                    depth--;
                    if (depth < 0) return elemBuf;  // end of array
                }
                else if (*p == ',' && depth == 0) {
                    p++;  // skip comma
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                    currentIndex++;
                    break;
                }
            }
            p++;
        }
    }

    return elemBuf;
}

bool HasField(const char* buffer, const char* fieldName) {
    if (!buffer || !fieldName) return false;
    char pattern[256];
    sprintf_s(pattern, "\"%s\":", fieldName);
    return strstr(buffer, pattern) != nullptr;
}

} // namespace Protocol
