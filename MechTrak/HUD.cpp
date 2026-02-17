#include "pch.h"
#include "HUD.h"

void HUD::Render(
    CanvasWrapper& canvas,
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int currentShotNumber,
    bool sessionActive)
{
    // Check if HUD should be hidden
    auto hideCvar = cvarManager->getCvar("mechtrak_hide_hud");
    if (hideCvar && hideCvar.getBoolValue()) return;

    // Check compact
    auto compactCvar = cvarManager->getCvar("mechtrak_compact_hud");
    bool isCompact = compactCvar && compactCvar.getBoolValue();

    // NO ACTIVE SESSION - show in any game mode
    if (!sessionActive) {
        if (!gameWrapper->IsInCustomTraining()) return;

        if (isCompact) {
            RenderCompact(canvas, shotStats, shotTypes, currentShotNumber, sessionActive);
            return;
        }

        Vector2 screenSize = canvas.GetSize();
        int x = screenSize.X / 2 - 250;
        int y = 50;

        canvas.SetColor(0, 0, 0, 200);
        canvas.SetPosition(Vector2{ x - 10, y - 5 });
        canvas.FillBox(Vector2{ 700, 110 });

        canvas.SetColor(100, 200, 255, 255);
        canvas.SetPosition(Vector2{ x, y });
        canvas.DrawString("Mech Trak", 1.5f, 1.5f);

        canvas.SetColor(255, 150, 50, 255);
        canvas.SetPosition(Vector2{ x, y + 30 });
        canvas.DrawString("No active session!", 2.0f, 2.0f);

        canvas.SetColor(150, 150, 150, 255);
        canvas.SetPosition(Vector2{ x, y + 65 });
        canvas.DrawString("Visit mechtrak.gg to start a session", 1.5f, 1.5f);
        return;
    }

    // Only show normal HUD in custom training
    if (!gameWrapper->IsInCustomTraining()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    if (isCompact) {
        RenderCompact(canvas, shotStats, shotTypes, currentShotNumber, sessionActive);
        return;
    }

    // Check remove graph
    auto removeGraphCvar = cvarManager->getCvar("mechtrak_remove_graph");
    bool removeGraph = removeGraphCvar && removeGraphCvar.getBoolValue();

    Vector2 screenSize = canvas.GetSize();
    int x = screenSize.X / 2 - 250;
    int y = 50;

    // Glow layers
    for (int i = 4; i > 0; i--) {
        int glowAlpha = 30 * (5 - i);
        canvas.SetColor(255, 255, 255, glowAlpha);
        canvas.SetPosition(Vector2{ x - 10 - i * 2, y - 5 - i * 2 });
        canvas.DrawBox(Vector2{ 700 + i * 4, 110 + i * 4 });
    }

    // Background
    int boxWidth = removeGraph ? 400 : 700;
    canvas.SetColor(0, 0, 0, 200);
    canvas.SetPosition(Vector2{ x - 10, y - 5 });
    canvas.FillBox(Vector2{ boxWidth, 110 });

    // Title
    canvas.SetColor(100, 200, 255, 255);
    canvas.SetPosition(Vector2{ x, y });
    canvas.DrawString("Mech Trak", 1.5f, 1.5f);

    // Keybind hints
    canvas.SetColor(150, 150, 150, 255);
    canvas.SetPosition(Vector2{ x + 280, y });
    canvas.DrawString("F8: Prev | F9: Next", 1.5f, 1.5f);

    // Shot info
    canvas.SetColor(255, 255, 255, 255);
    canvas.SetPosition(Vector2{ x, y + 30 });
    std::string shotText = "Tracking Shot: " + std::to_string(currentShotNumber);
    if (shotTypes.find(currentShotNumber) != shotTypes.end() && !shotTypes[currentShotNumber].empty()) {
        shotText += " (" + shotTypes[currentShotNumber] + ")";
    }
    canvas.DrawString(shotText, 2.0f, 2.0f);

    // Stats
    if (shotStats.find(currentShotNumber) != shotStats.end()) {
        auto stats = shotStats[currentShotNumber];
        canvas.SetPosition(Vector2{ x, y + 65 });
        std::string statsText = std::to_string(stats.goals) + " Goals | " +
            std::to_string(stats.attempts) + " Attempts";
        if (stats.attempts > 0) {
            float accuracy = (float)stats.goals / stats.attempts * 100.0f;
            statsText += " (" + std::to_string((int)accuracy) + "%)";
        }
        canvas.DrawString(statsText, 1.5f, 1.5f);
    }

    if (!removeGraph) {
        DrawMiniGraph(canvas, shotStats, currentShotNumber, x + 480, y + 20);
    }
}

void HUD::RenderCompact(
    CanvasWrapper& canvas,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int currentShotNumber,
    bool sessionActive)
{
    Vector2 screenSize = canvas.GetSize();
    float boxWidth = 200.0f;
    float boxHeight = 80.0f;
    float padding = 10.0f;
    float x = screenSize.X - boxWidth - padding;
    float y = padding;

    canvas.SetColor(0, 0, 0, 180);
    canvas.DrawRect(Vector2F{ x, y }, Vector2F{ x + boxWidth, y + boxHeight });
    canvas.SetColor(255, 255, 255, 80);
    canvas.DrawLine(Vector2F{ x, y }, Vector2F{ x + boxWidth, y });
    canvas.DrawLine(Vector2F{ x, y + boxHeight }, Vector2F{ x + boxWidth, y + boxHeight });
    canvas.DrawLine(Vector2F{ x, y }, Vector2F{ x, y + boxHeight });
    canvas.DrawLine(Vector2F{ x + boxWidth, y }, Vector2F{ x + boxWidth, y + boxHeight });

    if (!sessionActive) {
        canvas.SetColor(255, 150, 50, 255);
        canvas.SetPosition(Vector2F{ x + 10, y + 8 });
        canvas.DrawString("No Session!", 1.2f, 1.2f);
        canvas.SetColor(150, 150, 150, 255);
        canvas.SetPosition(Vector2F{ x + 10, y + 35 });
        canvas.DrawString("Visit mechtrak.gg", 1.0f, 1.0f);
        return;
    }

    if (shotStats.find(currentShotNumber) == shotStats.end()) return;
    auto stats = shotStats[currentShotNumber];

    float accuracy = stats.attempts > 0
        ? (float)stats.goals / stats.attempts * 100.0f : 0.0f;

    canvas.SetColor(255, 255, 255, 255);
    std::string shotName = "Shot " + std::to_string(currentShotNumber);
    if (shotTypes.find(currentShotNumber) != shotTypes.end()) {
        shotName = shotTypes[currentShotNumber];
    }
    canvas.SetPosition(Vector2F{ x + 10, y + 8 });
    canvas.DrawString(shotName, 1.2f, 1.2f);

    std::string accuracyStr = std::to_string((int)accuracy) + "%";
    canvas.SetPosition(Vector2F{ x + 10, y + 30 });
    canvas.DrawString(accuracyStr, 1.8f, 1.8f);

    std::string statsStr = std::to_string(stats.goals) + "/" + std::to_string(stats.attempts);
    canvas.SetPosition(Vector2F{ x + 120, y + 38 });
    canvas.DrawString(statsStr, 1.2f, 1.2f);
}

void HUD::DrawMiniGraph(
    CanvasWrapper& canvas,
    std::map<int, ShotStats>& shotStats,
    int currentShotNumber,
    int startX,
    int startY)
{
    if (shotStats.find(currentShotNumber) == shotStats.end()) return;
    auto& currentShot = shotStats[currentShotNumber];
    if (currentShot.attemptHistory.empty()) return;

    int graphWidth = 180;
    int graphHeight = 70;
    int windowSize = 5;

    canvas.SetColor(100, 200, 255, 255);
    Vector2F lastPoint = { -1, -1 };

    for (size_t i = 0; i < currentShot.attemptHistory.size(); i++) {
        int goalsInWindow = 0;
        int windowStart = (i >= windowSize - 1) ? i - windowSize + 1 : 0;
        int actualWindowSize = i - windowStart + 1;

        for (size_t j = windowStart; j <= i; j++) {
            if (currentShot.attemptHistory[j]) goalsInWindow++;
        }

        float movingAccuracy = (float)goalsInWindow / actualWindowSize;
        float xPos = startX + (i * graphWidth / (float)currentShot.attemptHistory.size());
        float yPos = startY + graphHeight - (movingAccuracy * graphHeight);
        Vector2F currentPoint = { xPos, yPos };

        if (lastPoint.X >= 0) {
            canvas.DrawLine(lastPoint, currentPoint, 2.0f);
        }
        lastPoint = currentPoint;
    }
}