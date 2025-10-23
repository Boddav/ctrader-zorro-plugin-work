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
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"clientId\":\"%s\",\"clientSecret\":\"%s\"}}",
        Utils::GetMsgId(), ToInt(PayloadType::ApplicationAuthReq), G.ClientId, G.ClientSecret);

    if (!Network::Send(request) || Network::Receive(response, sizeof(response)) <= 0 ||
        !Utils::ContainsPayloadType(response, PayloadType::ApplicationAuthRes)) {
        return false;
    }

    ZeroMemory(request, sizeof(request));
    ZeroMemory(response, sizeof(response));

    sprintf_s(request,
        "{\"clientMsgId\":\"%s\",\"payloadType\":%d,\"payload\":{\"accessToken\":\"%s\",\"ctidTraderAccountId\":%lld}}",
        Utils::GetMsgId(), ToInt(PayloadType::AccountAuthReq), G.Token, G.CTraderAccountId);

    if (!Network::Send(request) || Network::Receive(response, sizeof(response)) <= 0 ||
        !Utils::ContainsPayloadType(response, PayloadType::AccountAuthRes)) {
        return false;
    }

    return true;
}

void ResubscribeSymbols() {
    // Reset quote counter and restart subscription timer
    G.quoteCount = 0;
    G.subscriptionStartMs = GetTickCount64();
    
    Symbols::BatchResubscribe(G.CTraderAccountId);
    Symbols::BatchResubscribeDepth(G.CTraderAccountId, 10);
}

bool Attempt(int maxRetries) {
    Utils::ShowMsg("Reconnecting...");
    Network::Disconnect();

    // Debug: Log the environment for reconnection
    char envMsg[256];
    sprintf_s(envMsg, "Reconnecting with environment: %s",
             (G.Env == CtraderEnv::Live) ? "LIVE" : "DEMO");
    Utils::LogToFile("RECONNECT_ENV", envMsg);

    // Use the persistent G.Env and any explicit host override for reconnection
    const char* host = nullptr;
    if (!G.hostOverride.empty()) {
        host = G.hostOverride.c_str();
    } else {
        host = (G.Env == CtraderEnv::Live) ? CTRADER_HOST_LIVE : CTRADER_HOST_DEMO;
    }

    // Debug: Log the host being used for reconnection
    char hostMsg[256];
    sprintf_s(hostMsg, "Reconnecting to host: %s (using persistent env)", host);
    Utils::LogToFile("RECONNECT_HOST", hostMsg);

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        char msg[128];
        sprintf_s(msg, "Attempt %d/%d", attempt + 1, maxRetries);
        Utils::ShowMsg(msg);

        Sleep((DWORD)(1000 * pow(3.0, (double)attempt)));

        if (Network::Connect(host, "5036") && AuthenticateAfterConnect()) {
            ResubscribeSymbols();
            G.snapshotsRequested = false;
            G.cashFlowRequested = false;
            G.cashFlowHydrated = false;
            G.assetMetadataRequested = false;
            Utils::Notify(Utils::UserMessageType::Success, "RECONNECT", "Reconnected!");
            return true;
        }
    }

    return false;
}

}

