#include "pch.h"
#include "Heartbeat.h"
#include "Config.h"
#include <winhttp.h>
#include <thread>

void Heartbeat::Send(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    const std::string& sessionId)
{
    HINTERNET hSession = WinHttpOpen(L"RLStatsPlugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_HOST.c_str(), SERVER_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/heartbeat",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    std::string body = "{\"session_id\": \"" + sessionId + "\"}";
    std::wstring headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body.c_str(), body.length(), body.length(), 0);

    if (bResults) WinHttpReceiveResponse(hRequest, NULL);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

void Heartbeat::Start(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::string& sessionId,
    bool& sessionActive,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int& currentShotNumber)
{
    // Run heartbeat on background thread
    std::thread([cvarManager, gameWrapper, &sessionId, &sessionActive,
        &shotStats, &shotTypes, &currentShotNumber]() {
            Send(cvarManager, gameWrapper, sessionId);

        // If no session loaded yet, try again
        if (!sessionActive || sessionId.empty()) {
            cvarManager->log("No session loaded, retrying LoadActive...");
            Session::LoadActive(cvarManager, sessionId, sessionActive,
                shotStats, shotTypes, currentShotNumber);
        }
    }).detach();

    gameWrapper->SetTimeout([cvarManager, gameWrapper, &sessionId, &sessionActive,
        &shotStats, &shotTypes, &currentShotNumber](GameWrapper* gw) {
            Start(cvarManager, gameWrapper, sessionId, sessionActive,
                shotStats, shotTypes, currentShotNumber);
        }, 30.0f);
}