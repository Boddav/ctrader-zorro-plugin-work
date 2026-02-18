#pragma once

namespace WebSocket {

bool Connect(const char* host, int port);
void Disconnect();
bool Send(const char* message);
int Receive(char* buffer, int bufferSize);
bool IsConnected();

} // namespace WebSocket
