#pragma once
#include "globals.h"

namespace OAuth {
    bool PerformInteractiveFlow();
    bool ListenForCode(std::string& outCode, int port = 53123, DWORD timeoutMs = 60000);
    bool ExchangeCodeForTokens(const std::string& code, std::string& accessOut, std::string& refreshOut);
}