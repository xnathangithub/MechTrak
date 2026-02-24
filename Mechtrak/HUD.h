#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/wrappers/canvaswrapper.h"
#include "imgui/imgui.h"
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
    // Standard canvas HUD (used when compact mode is OFF)
    static void Render(
        CanvasWrapper& canvas,
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::shared_ptr<GameWrapper> gameWrapper,
        std::map<int, ShotStats>& shotStats,
        std::map<int, std::string>& shotTypes,
        int currentShotNumber,
        bool sessionActive
    );

    // ImGui compact HUD — call this from PluginWindow::Render() ONLY
    static void RenderImGui(
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        std::shared_ptr<GameWrapper> gameWrapper,
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

private:
    static void DrawPieChart(ImDrawList* dl, ImVec2 center, float radius,
        float fraction, ImU32 fillCol, ImU32 bgCol);

    static void DrawProgressBar(ImDrawList* dl, ImVec2 pos,
        float width, float height, float fraction,
        ImU32 bgCol, ImU32 fillCol, float rounding = 3.f);

    static std::string FmtNum(int n);
};