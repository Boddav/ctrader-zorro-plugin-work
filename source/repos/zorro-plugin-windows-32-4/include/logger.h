#pragma once

namespace Log {

void Info(const char* tag, const char* fmt, ...);
void Warn(const char* tag, const char* fmt, ...);
void Error(const char* tag, const char* fmt, ...);
void Msg(const char* text);           // Show to user via BrokerMessage
void Diag(int level, const char* fmt, ...);  // Only if G.diagLevel >= level
void ToFile(const char* tag, const char* message);

} // namespace Log
