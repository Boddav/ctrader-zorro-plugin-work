#include "../include/reconnect.h"
#include "../include/network.h"
#include "../include/utils.h"
#include "../include/symbols.h"
#include <cmath>

namespace Reconnect {

bool AuthenticateAfterConnect() {
    char request[1024] = {0};
    char response[131072] = {0};

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":2100,\"payload\":{\"clientId\":\"%s\",\"clientSecret\":\"%s\"}}",
        Utils::GetMsgId(), G.ClientId, G.ClientSecret);

    if (!Network::Send(request) || Network::Receive(response, sizeof(response)) <= 0 ||
        !strstr(response, "\"payloadType\":2101")) {
        return false;
    }

    ZeroMemory(request, sizeof(request));
    ZeroMemory(response, sizeof(response));

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":2102,\"payload\":{\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
        Utils::GetMsgId(), G.Token, G.CTraderAccountId);

    if (!Network::Send(request) || Network::Receive(response, sizeof(response)) <= 0 ||
        !strstr(response, "\"payloadType\":2103")) {
        return false;
    }

    return true;
}

void ResubscribeSymbols() {
    Symbols::BatchResubscribe(G.CTraderAccountId);
}

bool Attempt(int maxRetries) {
    Utils::ShowMsg("Reconnecting...");
    Network::Disconnect();

    const char* host = (G.Env == CtraderEnv::Live) ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        char msg[128];
        sprintf_s(msg, "Attempt %d/%d", attempt + 1, maxRetries);
        Utils::ShowMsg(msg);

        Sleep((DWORD)(1000 * pow(3.0, (double)attempt)));

        if (Network::Connect(host, "5036") && AuthenticateAfterConnect()) {
            ResubscribeSymbols();
            Utils::ShowMsg("Reconnected!");
            return true;
        }
    }

    return false;
}

}