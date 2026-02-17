#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/wrappers/canvaswrapper.h"
#include <map>
#include <string>
#include <vector>

struct ShotStats {
    int attempts = 0;
    int goals = 0;
    std::vector<bool> attemptHistory;
};

class HUD {
public:
    static void Render(
        CanvasWrapper& canvas,
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::shared_ptr<GameWrapper> gameWrapper,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes,
        int currentShotNumber,
        bool sessionActive
    );

    static void RenderCompact(
        CanvasWrapper& canvas,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes,
        int currentShotNumber,
        bool sessionActive
    );

    static void DrawMiniGraph(
        CanvasWrapper& canvas,
        std::map<int, ShotStats>& shotStats,
        int currentShotNumber,
        int startX,
        int startY
    );
};