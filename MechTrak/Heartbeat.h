#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "HUD.h"
#include "Session.h"
#include <string>
#include <map>

class Heartbeat {
public:
    static void Send(
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::shared_ptr<GameWrapper> gameWrapper,
        const std::string& sessionId
    );

    static void Start(
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::shared_ptr<GameWrapper> gameWrapper,
        std::string& sessionId,
        bool& sessionActive,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes,
        int& currentShotNumber
    );
};