#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "json.hpp"
#include "HUD.h"
#include <map>
#include <string>
#include <chrono>

using json = nlohmann::json;

class Session {
public:
    static std::string GenerateId();
    static std::string GetPluginToken(std::shared_ptr<CVarManagerWrapper> cvarManager);
    static std::string cachedToken;

    static void SaveToFile(
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        const std::string& sessionId,
        bool sessionActive,
        std::chrono::system_clock::time_point sessionStartTime,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes
    );

    static void Upload(
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::shared_ptr<GameWrapper> gameWrapper,
        std::string& sessionId,
        bool& sessionActive,
        std::chrono::system_clock::time_point sessionStartTime,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes,
        int& currentShotNumber
    );

    static void LoadActive(
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::string& sessionId,
        bool& sessionActive,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes,
        int& currentShotNumber
    );
}; 