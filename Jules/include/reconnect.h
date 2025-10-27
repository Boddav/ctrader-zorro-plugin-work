#pragma once
#include "globals.h"

namespace Reconnect {
    bool AuthenticateAfterConnect();
    void ResubscribeSymbols();
    bool Attempt(int maxRetries = 3);
}