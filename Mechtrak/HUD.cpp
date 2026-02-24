#include "pch.h"
#include "HUD.h"
#include "imgui/imgui.h"
#include <cmath>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

std::string HUD::FmtNum(int n)
{
    if (n >= 1000) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (n / 1000.f) << "K";
        return ss.str();
    }
    return std::to_string(n);
}

void HUD::DrawPieChart(ImDrawList* dl, ImVec2 center, float radius,
    float fraction, ImU32 fillCol, ImU32 bgCol)
{
    const int   SEG = 64;
    const float START = -M_PI * 0.5f;
    dl->AddCircleFilled(center, radius, bgCol, SEG);
    if (fraction > 0.001f) {
        dl->PathLineTo(center);
        dl->PathArcTo(center, radius, START, START + fraction * 2.f * M_PI, SEG);
        dl->PathFillConvex(fillCol);
    }
    // no border
}

void HUD::DrawProgressBar(ImDrawList* dl, ImVec2 pos, float width, float height,
    float fraction, ImU32 bgCol, ImU32 fillCol, float rounding)
{
    dl->AddRectFilled(pos, { pos.x + width, pos.y + height }, bgCol, rounding);
    if (fraction > 0.001f) {
        float fw = width * (fraction > 1.f ? 1.f : fraction);
        dl->AddRectFilled(pos, { pos.x + fw, pos.y + height }, fillCol, rounding);
        dl->AddRectFilled(pos, { pos.x + fw, pos.y + height * 0.5f }, IM_COL32(255, 255, 255, 28), rounding);
    }
}

// ─── Canvas HUD (compact=OFF) ─────────────────────────────────────────────────

void HUD::Render(
    CanvasWrapper& canvas,
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int currentShotNumber,
    bool sessionActive)
{
    auto hideCvar = cvarManager->getCvar("mechtrak_hide_hud");
    if (hideCvar && hideCvar.getBoolValue()) return;

    // Compact mode is shown via PluginWindow, not canvas
    auto compactCvar = cvarManager->getCvar("mechtrak_compact_hud");
    if (compactCvar && compactCvar.getBoolValue()) return;

    if (!sessionActive) {
        if (!gameWrapper->IsInCustomTraining()) return;
        Vector2 ss = canvas.GetSize();
        int x = ss.X / 2 - 250, y = 50;
        canvas.SetColor(0, 0, 0, 200); canvas.SetPosition(Vector2{ x - 10,y - 5 }); canvas.FillBox(Vector2{ 700,110 });
        canvas.SetColor(100, 200, 255, 255); canvas.SetPosition(Vector2{ x,y }); canvas.DrawString("Mech Trak", 1.5f, 1.5f);
        canvas.SetColor(255, 150, 50, 255); canvas.SetPosition(Vector2{ x,y + 30 }); canvas.DrawString("No active session!", 2.f, 2.f);
        canvas.SetColor(150, 150, 150, 255); canvas.SetPosition(Vector2{ x,y + 65 }); canvas.DrawString("Visit mechtrak.gg to start a session", 1.5f, 1.5f);
        return;
    }
    if (!gameWrapper->IsInCustomTraining()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto removeGraphCvar = cvarManager->getCvar("mechtrak_remove_graph");
    bool removeGraph = removeGraphCvar && removeGraphCvar.getBoolValue();
    Vector2 ss = canvas.GetSize();
    int x = ss.X / 2 - 250, y = 50;

    for (int i = 4; i > 0; i--) {
        canvas.SetColor(255, 255, 255, 30 * (5 - i));
        canvas.SetPosition(Vector2{ x - 10 - i * 2,y - 5 - i * 2 });
        canvas.DrawBox(Vector2{ 700 + i * 4,110 + i * 4 });
    }
    canvas.SetColor(0, 0, 0, 200); canvas.SetPosition(Vector2{ x - 10,y - 5 }); canvas.FillBox(Vector2{ removeGraph ? 400 : 700,110 });
    canvas.SetColor(100, 200, 255, 255); canvas.SetPosition(Vector2{ x,y }); canvas.DrawString("Mech Trak", 1.5f, 1.5f);
    canvas.SetColor(150, 150, 150, 255); canvas.SetPosition(Vector2{ x + 280,y }); canvas.DrawString("F8: Prev | F9: Next", 1.5f, 1.5f);
    canvas.SetColor(255, 255, 255, 255); canvas.SetPosition(Vector2{ x,y + 30 });
    std::string st = "Tracking Shot: " + std::to_string(currentShotNumber);
    if (shotTypes.count(currentShotNumber) && !shotTypes[currentShotNumber].empty())
        st += " (" + shotTypes[currentShotNumber] + ")";
    canvas.DrawString(st, 2.f, 2.f);
    if (shotStats.count(currentShotNumber)) {
        auto& s = shotStats[currentShotNumber];
        canvas.SetPosition(Vector2{ x,y + 65 });
        std::string txt = std::to_string(s.goals) + " Goals | " + std::to_string(s.attempts) + " Attempts";
        if (s.attempts > 0) txt += " (" + std::to_string((int)((float)s.goals / s.attempts * 100)) + "%)";
        canvas.DrawString(txt, 1.5f, 1.5f);
    }
    if (!removeGraph) DrawMiniGraph(canvas, shotStats, currentShotNumber, x + 480, y + 20);
}

// ─── ImGui compact HUD ────────────────────────────────────────────────────────
// Called from MechTrak::Render() which is a PluginWindow callback.
// ImGui::Begin/End and all ImDrawList calls are fully valid here.

void HUD::RenderImGui(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int currentShotNumber,
    bool sessionActive)
{
    if (!gameWrapper->IsInCustomTraining() && sessionActive) return;

    ImGuiIO& io = ImGui::GetIO();
    const float PW = 265.f;
    const float PAD = 12.f;

    // Position top-right, no title bar, not movable, transparent bg
    ImGui::SetNextWindowPos({ io.DisplaySize.x - PW - 18.f, 18.f }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ PW, 0 }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,4 });

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    if (!ImGui::Begin("##MechTrakHUD", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();
    ImFont* fnt = ImGui::GetFont();
    const float FS = fnt->FontSize;

    // ── colours ──────────────────────────────────────────────────────────
    const ImU32 cBg = IM_COL32(0, 0, 0, 242);  // pure black
    const ImU32 cHdr = IM_COL32(8, 25, 70, 235);  // dark navy blue
    const ImU32 cBorder = IM_COL32(70, 130, 255, 40);
    const ImU32 cPillBg = IM_COL32(255, 255, 255, 8);
    const ImU32 cPillBrd = IM_COL32(0, 0, 0, 0);  // invisible — no border
    const ImU32 cPieBg = IM_COL32(64, 64, 64, 255);    // grey = attempts
    const ImU32 cPieFill = IM_COL32(10, 30, 90, 255);  // dark blue = goals
    const ImU32 cBarBg = IM_COL32(255, 255, 255, 15);
    const ImU32 cBarFill = IM_COL32(55, 135, 255, 255);
    const ImU32 cBtnBg = IM_COL32(45, 90, 210, 190);
    const ImU32 cBtnBrd = IM_COL32(90, 150, 255, 90);
    const ImU32 cDiv = IM_COL32(70, 120, 220, 80);
    const ImU32 cBestBg = IM_COL32(45, 90, 210, 200);
    const ImU32 cGIcon = IM_COL32(190, 50, 50, 210);
    const ImU32 cSIcon = IM_COL32(45, 90, 210, 210);
    const ImU32 cWhite = IM_COL32(255, 255, 255, 255);
    const ImU32 cSub = IM_COL32(150, 185, 255, 175);
    const ImU32 cGold = IM_COL32(255, 210, 50, 255);
    const ImU32 cBadge = IM_COL32(35, 75, 175, 65);

    // ── stats ─────────────────────────────────────────────────────────────
    ShotStats cur;
    if (shotStats.count(currentShotNumber)) cur = shotStats[currentShotNumber];
    float curAcc = cur.attempts > 0 ? (float)cur.goals / cur.attempts : 0.f;

    int totalGoals = 0, totalAttempts = 0; float bestPct = 0.f;
    for (auto& [id, s] : shotStats) {
        totalGoals += s.goals; totalAttempts += s.attempts;
        if (s.attempts > 0 && (float)s.goals / s.attempts > bestPct) bestPct = (float)s.goals / s.attempts;
    }
    float totalAcc = totalAttempts > 0 ? (float)totalGoals / totalAttempts : 0.f;
    float shotsFrac = (totalAttempts > 0 && cur.attempts > 0) ? (float)cur.attempts / totalAttempts : 0.f;
    if (shotsFrac > 1.f) shotsFrac = 1.f;

    std::string shotType;
    if (shotTypes.count(currentShotNumber)) shotType = shotTypes[currentShotNumber];

    // ── draw panel bg (estimated height 370) ─────────────────────────────
    const float PANEL_H = sessionActive ? 220.f : 115.f;
    dl->AddRectFilled(wp, { wp.x + PW,wp.y + PANEL_H }, cBg, 14.f);
    dl->AddRect(wp, { wp.x + PW,wp.y + PANEL_H }, cBorder, 14.f, 0, 1.f);
    dl->AddRect({ wp.x + 1,wp.y + 1 }, { wp.x + PW - 1,wp.y + PANEL_H - 1 }, IM_COL32(255, 255, 255, 7), 14.f, 0, 1.f);

    // ── header ────────────────────────────────────────────────────────────
    const float HDR = 34.f;
    dl->AddRectFilled(wp, { wp.x + PW, wp.y + HDR }, cHdr, 14.f);
    dl->AddRectFilled({ wp.x,wp.y + HDR * 0.5f }, { wp.x + PW,wp.y + HDR }, cHdr, 0.f); // square bottom corners
    dl->AddLine({ wp.x,wp.y + HDR }, { wp.x + PW,wp.y + HDR }, IM_COL32(120, 170, 255, 65), 1.f);

    const char* TITLE = "MECHTRAK.GG";
    ImVec2 tSz = fnt->CalcTextSizeA(FS, FLT_MAX, 0, TITLE);
    dl->AddText(fnt, FS, { wp.x + (PW - tSz.x) * 0.5f,wp.y + (HDR - tSz.y) * 0.5f }, cWhite, TITLE);

    const char* lblF8 = "< F8";
    const char* lblF9 = "F9 >";
    ImVec2 f8Sz = fnt->CalcTextSizeA(FS * 0.78f, FLT_MAX, 0, lblF8);
    ImVec2 f9Sz = fnt->CalcTextSizeA(FS * 0.78f, FLT_MAX, 0, lblF9);
    float arrowY = wp.y + (HDR - FS * 0.78f) * 0.5f;
    dl->AddText(fnt, FS * 0.78f, { wp.x + PAD, arrowY }, IM_COL32(150, 185, 255, 175), lblF8);
    dl->AddText(fnt, FS * 0.78f, { wp.x + PW - f9Sz.x - PAD, arrowY }, IM_COL32(150, 185, 255, 175), lblF9);

    // Use ImGui cursor to track vertical position (local coords)
    ImGui::SetCursorPosY(HDR + 10.f);

    // ── no session ────────────────────────────────────────────────────────
    if (!sessionActive) {
        float cy = ImGui::GetCursorPosY();
        dl->AddText(fnt, FS, { wp.x + PAD,wp.y + cy + 4.f }, IM_COL32(255, 140, 65, 255), "NO ACTIVE SESSION");
        dl->AddText(fnt, FS, { wp.x + PAD,wp.y + cy + 22.f }, cSub, "Visit mechtrak.gg to begin");
        ImGui::Dummy({ PW, 50.f });
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    // ── shot pill ─────────────────────────────────────────────────────────
    {
        float cy = ImGui::GetCursorPosY();
        const float PH = 22.f, PW2 = 200.f;
        ImVec2 pm = { wp.x + PAD,wp.y + cy }, pmx = { pm.x + PW2,pm.y + PH };
        dl->AddRectFilled(pm, pmx, cPillBg, 5.f);
        // no border rect
        float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.14f);
        dl->AddCircleFilled({ pm.x + 11.f,pm.y + PH * 0.5f }, 3.5f, IM_COL32(50, 220, 90, (int)(200 + 55 * pulse))); // green
        std::string lbl = "SHOT " + std::to_string(currentShotNumber);
        if (!shotType.empty()) { std::string up = shotType; for (auto& c : up)c = (char)toupper((unsigned char)c); lbl += ": " + up; }
        dl->AddText(fnt, FS, { pm.x + 20.f,pm.y + (PH - FS) * 0.5f }, IM_COL32(195, 220, 255, 215), lbl.c_str());
        ImGui::Dummy({ PW,PH + 12.f });
    }

    // ── pie + stats ───────────────────────────────────────────────────────
    {
        float cy = ImGui::GetCursorPosY();
        const float PR = 52.f;
        ImVec2 pc = { wp.x + PAD + PR,wp.y + cy + PR + 4.f };
        DrawPieChart(dl, pc, PR, curAcc, cPieFill, cPieBg);
        // no pct label in pie

        std::string pctStr = std::to_string((int)(curAcc * 100)) + "%";
        float sx = wp.x + PAD + PR * 2.f + 14.f, sy = wp.y + cy + 4.f;
        const float GAP = 44.f;
        auto drawStat = [&](const std::string& val, const char* badge, float y) {
            dl->AddText(fnt, FS * 1.45f, { sx,y }, cWhite, val.c_str());
            ImVec2 bSz = fnt->CalcTextSizeA(FS * 0.88f, FLT_MAX, 0, badge);
            float by = y + FS * 1.45f + 2.f;
            dl->AddRectFilled({ sx,by }, { sx + bSz.x + 10.f,by + bSz.y + 4.f }, cBadge, 3.f);
            dl->AddText(fnt, FS * 0.88f, { sx + 5.f,by + 2.f }, cSub, badge);
            };
        drawStat(pctStr, "ACCURACY", sy);
        drawStat(FmtNum(cur.goals), "GOALS", sy + GAP);
        drawStat(FmtNum(cur.attempts), "ATTEMPTS", sy + GAP * 2.f);
        ImGui::Dummy({ PW, PR * 2.f + 16.f });
    }

    ImGui::Dummy({ PW, 14.f });

    ImGui::End();
    ImGui::PopStyleVar(3);
}

// ─── Mini graph ───────────────────────────────────────────────────────────────

void HUD::DrawMiniGraph(CanvasWrapper& canvas,
    std::map<int, ShotStats>& shotStats, int currentShotNumber, int startX, int startY)
{
    if (!shotStats.count(currentShotNumber)) return;
    auto& s = shotStats[currentShotNumber];
    if (s.attemptHistory.empty()) return;
    int gW = 180, gH = 70, wSz = 5;
    canvas.SetColor(100, 200, 255, 255);
    Vector2F last = { -1.f,-1.f };
    for (size_t i = 0; i < s.attemptHistory.size(); i++) {
        int goals = 0;
        size_t ws = (i >= (size_t)(wSz - 1)) ? i - wSz + 1 : 0;
        int aw = (int)(i - ws + 1);
        for (size_t j = ws; j <= i; j++) if (s.attemptHistory[j]) goals++;
        float xp = startX + (i * gW / (float)s.attemptHistory.size());
        float yp = startY + gH - ((float)goals / aw * gH);
        Vector2F pt = { xp,yp };
        if (last.X >= 0) canvas.DrawLine(last, pt, 2.f);
        last = pt;
    }
}