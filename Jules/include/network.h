#pragma once
#include "globals.h"

namespace Network {
    bool Connect(const char* host, const char* port);
    void Disconnect();
    bool Send(const char* data);
    int Receive(char* buffer, int bufferSize);
    bool IsConnected();
}