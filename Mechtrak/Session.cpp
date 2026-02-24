#include "pch.h"
#include "Session.h"
#include "Config.h"
#include <winhttp.h>
#include <fstream>
#include <filesystem>
#include <chrono>


std::string Session::cachedToken = "";


std::string Session::GenerateId()
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return "session_" + std::to_string(timestamp);
}

void Session::SaveToFile(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    const std::string& sessionId,
    bool sessionActive,
    std::chrono::system_clock::time_point sessionStartTime,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes)
{
    json sessionData;
    sessionData["sessionId"] = sessionId;
    sessionData["status"] = sessionActive ? "active" : "completed";

    auto startTime_t = std::chrono::system_clock::to_time_t(sessionStartTime);
    char timeBuffer[100];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%dT%H:%M:%S",
        std::localtime(&startTime_t));
    sessionData["startTime"] = timeBuffer;

    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%dT%H:%M:%S",
        std::localtime(&now_t));
    sessionData["lastUpdated"] = timeBuffer;

    auto duration = std::chrono::duration_cast<std::chrono::minutes>(
        now - sessionStartTime).count();
    sessionData["durationMinutes"] = duration;

    int totalAttempts = 0, totalGoals = 0;
    for (const auto& shot : shotStats) {
        totalAttempts += shot.second.attempts;
        totalGoals += shot.second.goals;
    }
    sessionData["totalAttempts"] = totalAttempts;
    sessionData["totalGoals"] = totalGoals;
    sessionData["totalAccuracy"] = totalAttempts > 0
        ? (float)totalGoals / totalAttempts * 100.0f : 0.0f;

    json shotsData;
    for (const auto& shot : shotStats) {
        int shotNum = shot.first;
        const auto& stats = shot.second;
        json shotData;
        shotData["attempts"] = stats.attempts;
        shotData["goals"] = stats.goals;
        shotData["attemptHistory"] = stats.attemptHistory;
        shotData["shotType"] = (shotTypes.find(shotNum) != shotTypes.end())
            ? shotTypes[shotNum] : "Unknown";
        shotData["accuracy"] = stats.attempts > 0
            ? (float)stats.goals / stats.attempts * 100.0f : 0.0f;
        shotsData[std::to_string(shotNum)] = shotData;
    }

    sessionData["shots"] = shotsData;
    sessionData["totalShots"] = shotStats.size();

    char* appdata = getenv("APPDATA");
    if (!appdata) return;

    std::string folderPath = std::string(appdata) +
        "\\bakkesmod\\bakkesmod\\data\\rl_best_stats";
    std::string filepath = folderPath + "\\session_" + sessionId + ".json";

    try {
        std::filesystem::create_directories(folderPath);
    }
    catch (...) {}

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << sessionData.dump(2);
        file.close();
    }
}

void Session::Upload(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::string& sessionId,
    bool& sessionActive,
    std::chrono::system_clock::time_point sessionStartTime,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int& currentShotNumber)
{
    // Don't upload if no active session
    if (!sessionActive || sessionId.empty()) {
        cvarManager->log("No active session, skipping upload");
        return;
    }

    // Check if session is still active on server
    HINTERNET hCheckSession = WinHttpOpen(L"RLStatsPlugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (hCheckSession) {
        HINTERNET hCheckConnect = WinHttpConnect(hCheckSession,
            SERVER_HOST.c_str(), SERVER_PORT, 0);
        if (hCheckConnect) {
            HINTERNET hCheckRequest = WinHttpOpenRequest(hCheckConnect,
                L"GET", L"/api/sessions/active", NULL, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

            if (hCheckRequest) {
                // Add auth header
                std::string pluginToken = GetPluginToken(cvarManager);
                std::wstring checkHeaders = L"Content-Type: application/json\r\n";
                if (!pluginToken.empty()) {
                    std::wstring wtoken(pluginToken.begin(), pluginToken.end());
                    checkHeaders += L"Authorization: Bearer " + wtoken + L"\r\n";
                }
                WinHttpAddRequestHeaders(hCheckRequest, checkHeaders.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

                if (WinHttpSendRequest(hCheckRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
                    0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                    WinHttpReceiveResponse(hCheckRequest, NULL);

                    DWORD dwSize = 0;
                    std::string checkResponse;

                    do {
                        dwSize = 0;
                        if (!WinHttpQueryDataAvailable(hCheckRequest, &dwSize)) break;
                        if (dwSize == 0) break;

                        char* buf = new char[dwSize + 1];
                        ZeroMemory(buf, dwSize + 1);
                        DWORD dwDownloaded = 0;
                        if (WinHttpReadData(hCheckRequest, buf, dwSize, &dwDownloaded)) {
                            checkResponse.append(buf, dwDownloaded);
                        }
                        delete[] buf;
                    } while (dwSize > 0);

                    try {
                        auto checkJson = nlohmann::json::parse(checkResponse);
                        if (checkJson["success"] == true && !checkJson["session"].is_null()) {
                            std::string activeSessionId = checkJson["session"]["session_id"];
                            if (activeSessionId != sessionId) {
                                cvarManager->log("New session detected, switching...");
                                sessionActive = false;
                                SaveToFile(cvarManager, sessionId, sessionActive,
                                    sessionStartTime, shotStats, shotTypes);

                                WinHttpCloseHandle(hCheckRequest);
                                WinHttpCloseHandle(hCheckConnect);
                                WinHttpCloseHandle(hCheckSession);

                                LoadActive(cvarManager, sessionId, sessionActive,
                                    shotStats, shotTypes, currentShotNumber);
                                return;
                            }
                        }
                        else {
                            // No active session on server - session was deleted
                            cvarManager->log("Session deleted from dashboard, stopping uploads");
                            sessionActive = false;
                            sessionId = "";

                            WinHttpCloseHandle(hCheckRequest);
                            WinHttpCloseHandle(hCheckConnect);
                            WinHttpCloseHandle(hCheckSession);
                            return;
                        }
                    }
                    catch (...) {}
                }
                WinHttpCloseHandle(hCheckRequest);
            }
            WinHttpCloseHandle(hCheckConnect);
        }
        WinHttpCloseHandle(hCheckSession);
    }

    // Upload session file
     

    // Don't upload if no active session
    if (!sessionActive || sessionId.empty()) {
        cvarManager->log("No active session, skipping upload");
        return;
    }

    char* appdata = getenv("APPDATA");
    if (!appdata) return;

    std::string folderPath = std::string(appdata) +
        "\\bakkesmod\\bakkesmod\\data\\rl_best_stats";
    std::string filepath = folderPath + "\\session_" + sessionId + ".json";

    std::ifstream file(filepath);
    if (!file.is_open()) {
        cvarManager->log("Error: Could not open session file");
        return;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    HINTERNET hSession = WinHttpOpen(L"RL Stats Plugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_HOST.c_str(), SERVER_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/sessions",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)jsonContent.c_str(), jsonContent.length(), jsonContent.length(), 0);

    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
            WINHTTP_NO_HEADER_INDEX);
        cvarManager->log(dwStatusCode == 200
            ? "Session uploaded successfully!"
            : "Upload failed: " + std::to_string(dwStatusCode));
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

void Session::LoadActive(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::string& sessionId,
    bool& sessionActive,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int& currentShotNumber)
{
    cvarManager->log("Loading active session...");

    HINTERNET hSession = WinHttpOpen(L"RLStatsPlugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_HOST.c_str(), SERVER_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        L"/api/sessions/active", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    // Get token first
    std::string pluginToken = GetPluginToken(cvarManager);
    cvarManager->log("Plugin token length: " + std::to_string(pluginToken.length()));

    // Add auth header if we have a token
    std::wstring authHeaders = L"Content-Type: application/json\r\n";
    if (!pluginToken.empty()) {
        std::wstring wtoken(pluginToken.begin(), pluginToken.end());
        authHeaders += L"Authorization: Bearer " + wtoken + L"\r\n";
    }
    WinHttpAddRequestHeaders(hRequest, authHeaders.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

   
    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

    if (bResults) {
        DWORD dwSize = 0;
        std::string responseData;

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            char* buf = new char[dwSize + 1];
            ZeroMemory(buf, dwSize + 1);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, (LPVOID)buf, dwSize, &dwDownloaded)) {
                responseData.append(buf, dwDownloaded);
            }
            delete[] buf;
        } while (dwSize > 0);

        try {
            auto response = nlohmann::json::parse(responseData);
            if (response["success"] == true && !response["session"].is_null()) {
                auto session = response["session"];
                sessionId = session["session_id"];
                sessionActive = true;

                shotStats.clear();
                shotTypes.clear();

                for (auto& [shotNumStr, shotData] : session["shots_data"].items()) {
                    int shotNum = std::stoi(shotNumStr);
                    ShotStats stats;
                    stats.attempts = shotData["attempts"];
                    stats.goals = shotData["goals"];
                    for (bool result : shotData["attemptHistory"]) {
                        stats.attemptHistory.push_back(result);
                    }
                    shotStats[shotNum] = stats;
                    shotTypes[shotNum] = shotData["shotType"];
                }

                if (!shotStats.empty()) {
                    currentShotNumber = shotStats.begin()->first;
                }

                cvarManager->log("Loaded " + std::to_string(shotStats.size()) + " shots");
            }
            else {
                cvarManager->log("No active session found");
            }
        }
        catch (const std::exception& e) {
            cvarManager->log("Error parsing session: " + std::string(e.what()));
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

std::string Session::GetPluginToken(std::shared_ptr<CVarManagerWrapper> cvarManager)
{
    HINTERNET hSession = WinHttpOpen(L"RLStatsPlugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_HOST.c_str(), SERVER_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        L"/api/plugin/token", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

    std::string token = "";


    if (bResults) {
        DWORD dwSize = 0;
        std::string responseData;

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            char* buf = new char[dwSize + 1];
            ZeroMemory(buf, dwSize + 1);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, (LPVOID)buf, dwSize, &dwDownloaded)) {
                responseData.append(buf, dwDownloaded);
            }
            delete[] buf;
        } while (dwSize > 0);

        try {
            auto response = nlohmann::json::parse(responseData);
            if (response["success"] == true && !response["token"].is_null()) {
                token = response["token"].get<std::string>();
                cachedToken = token; // Keep this to store latest
                cvarManager->log("Token fetched, length: " + std::to_string(token.length()));
            }
            else {
                cachedToken = ""; // Clear cache if no token
                cvarManager->log("No token available");
            }
        }
        catch (...) {
            cachedToken = "";
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return token;
}