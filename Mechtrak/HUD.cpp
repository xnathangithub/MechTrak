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

// Canvas HUD removed
void HUD::Render(
    CanvasWrapper& canvas,
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int currentShotNumber,
    bool sessionActive)
{
    return;
}

// ─── ImGui HUD + Edit Panel ───────────────────────────────────────────────────

void HUD::RenderImGui(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    std::map<int, ShotStats>& shotStats,
    std::map<int, std::string>& shotTypes,
    int currentShotNumber,
    bool sessionActive,
    bool& showEditPanel,
    std::function<void()> onEndSession,
    std::function<void(int, int, int)> onEditShot)
{
    auto hideCvar = cvarManager->getCvar("mechtrak_hide_hud");
    if (hideCvar && hideCvar.getBoolValue()) return;

    if (!gameWrapper->IsInCustomTraining() && sessionActive) return;

    ImGuiIO& io = ImGui::GetIO();
    const float PW = 265.f;
    const float PAD = 12.f;

    const ImU32 cBg = IM_COL32(0, 0, 0, 242);
    const ImU32 cHdr = IM_COL32(8, 25, 70, 235);
    const ImU32 cBorder = IM_COL32(70, 130, 255, 40);
    const ImU32 cPillBg = IM_COL32(255, 255, 255, 8);
    const ImU32 cPieBg = IM_COL32(64, 64, 64, 255);
    const ImU32 cPieFill = IM_COL32(10, 30, 90, 255);
    const ImU32 cWhite = IM_COL32(255, 255, 255, 255);
    const ImU32 cSub = IM_COL32(150, 185, 255, 175);
    const ImU32 cBadge = IM_COL32(35, 75, 175, 65);
    const ImU32 cGreen = IM_COL32(50, 200, 100, 255);
    const ImU32 cBtnBg = IM_COL32(30, 60, 160, 220);
    const ImU32 cBtnHov = IM_COL32(50, 90, 210, 255);
    const ImU32 cEndBg = IM_COL32(160, 30, 30, 220);
    const ImU32 cEndHov = IM_COL32(210, 50, 50, 255);

    auto xCvar = cvarManager->getCvar("mechtrak_hud_x");
    auto yCvar = cvarManager->getCvar("mechtrak_hud_y");
    float xFrac = xCvar ? xCvar.getFloatValue() : -1.f;
    float yFrac = yCvar ? yCvar.getFloatValue() : 0.02f;
    float winX = (xFrac < 0.f) ? (io.DisplaySize.x - PW - 18.f) : (io.DisplaySize.x * xFrac);
    float winY = io.DisplaySize.y * yFrac;

    // Read hotkey cvars once — used in both HUD and edit panel
    auto editKeyCvar = cvarManager->getCvar("mechtrak_key_edit_panel");
    std::string editKeyStr = editKeyCvar ? editKeyCvar.getStringValue() : "F4";

    // ── Window 1: compact HUD (no input) ─────────────────────────────────
    ImGuiWindowFlags hudFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::SetNextWindowPos({ winX, winY }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ PW, 0 }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 4 });

    bool hudOpen = ImGui::Begin("##MechTrakHUD", nullptr, hudFlags);
    if (hudOpen) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      wp = ImGui::GetWindowPos();
        ImFont* fnt = ImGui::GetFont();
        const float FS = fnt->FontSize;

        ShotStats cur;
        if (shotStats.count(currentShotNumber)) cur = shotStats[currentShotNumber];
        float curAcc = cur.attempts > 0 ? (float)cur.goals / cur.attempts : 0.f;

        std::string shotType;
        if (shotTypes.count(currentShotNumber)) shotType = shotTypes[currentShotNumber];

        const float PANEL_H = sessionActive ? 220.f : 115.f;
        dl->AddRectFilled(wp, { wp.x + PW, wp.y + PANEL_H }, cBg, 14.f);
        dl->AddRect(wp, { wp.x + PW, wp.y + PANEL_H }, cBorder, 14.f, 0, 1.f);
        dl->AddRect({ wp.x + 1, wp.y + 1 }, { wp.x + PW - 1, wp.y + PANEL_H - 1 }, IM_COL32(255, 255, 255, 7), 14.f, 0, 1.f);

        const float HDR = 34.f;
        dl->AddRectFilled(wp, { wp.x + PW, wp.y + HDR }, cHdr, 14.f);
        dl->AddRectFilled({ wp.x, wp.y + HDR * 0.5f }, { wp.x + PW, wp.y + HDR }, cHdr, 0.f);
        dl->AddLine({ wp.x, wp.y + HDR }, { wp.x + PW, wp.y + HDR }, IM_COL32(120, 170, 255, 65), 1.f);

        std::string titleStr = "MECHTRAK  [" + editKeyStr + "]";
        const char* TITLE = titleStr.c_str();
        ImVec2 tSz = fnt->CalcTextSizeA(FS, FLT_MAX, 0, TITLE);
        dl->AddText(fnt, FS, { wp.x + (PW - tSz.x) * 0.5f, wp.y + (HDR - tSz.y) * 0.5f }, cWhite, TITLE);

        const char* lblF8 = "< F8";
        const char* lblF9 = "F9 >";
        ImVec2 f9Sz = fnt->CalcTextSizeA(FS * 0.78f, FLT_MAX, 0, lblF9);
        float arrowY = wp.y + (HDR - FS * 0.78f) * 0.5f;
        dl->AddText(fnt, FS * 0.78f, { wp.x + PAD, arrowY }, IM_COL32(150, 185, 255, 175), lblF8);
        dl->AddText(fnt, FS * 0.78f, { wp.x + PW - f9Sz.x - PAD, arrowY }, IM_COL32(150, 185, 255, 175), lblF9);

        ImGui::SetCursorPosY(HDR + 10.f);

        if (!sessionActive) {
            float cy = ImGui::GetCursorPosY();
            dl->AddText(fnt, FS, { wp.x + PAD, wp.y + cy + 4.f }, IM_COL32(255, 140, 65, 255), "NO ACTIVE SESSION");
            dl->AddText(fnt, FS, { wp.x + PAD, wp.y + cy + 22.f }, cSub, "Visit mechtrak.gg to begin");
            ImGui::Dummy({ PW, 50.f });
        }
        else {
            // shot pill
            {
                float cy = ImGui::GetCursorPosY();
                const float PH = 22.f, PW2 = 200.f;
                ImVec2 pm = { wp.x + PAD, wp.y + cy };
                ImVec2 pmx = { pm.x + PW2, pm.y + PH };
                dl->AddRectFilled(pm, pmx, cPillBg, 5.f);
                float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.14f);
                dl->AddCircleFilled({ pm.x + 11.f, pm.y + PH * 0.5f }, 3.5f, IM_COL32(50, 220, 90, (int)(200 + 55 * pulse)));
                std::string lbl = "SHOT " + std::to_string(currentShotNumber);
                if (!shotType.empty()) {
                    std::string up = shotType;
                    for (auto& c : up) c = (char)toupper((unsigned char)c);
                    lbl += ": " + up;
                }
                dl->AddText(fnt, FS, { pm.x + 20.f, pm.y + (PH - FS) * 0.5f }, IM_COL32(195, 220, 255, 215), lbl.c_str());
                ImGui::Dummy({ PW, PH + 12.f });
            }
            // pie + stats
            {
                float cy = ImGui::GetCursorPosY();
                const float PR = 52.f;
                DrawPieChart(dl, { wp.x + PAD + PR, wp.y + cy + PR + 4.f }, PR, curAcc, cPieFill, cPieBg);
                std::string pctStr = std::to_string((int)(curAcc * 100)) + "%";
                float sx = wp.x + PAD + PR * 2.f + 14.f, sy = wp.y + cy + 4.f;
                const float GAP = 44.f;
                auto drawStat = [&](const std::string& val, const char* badge, float y) {
                    dl->AddText(fnt, FS * 1.45f, { sx, y }, cWhite, val.c_str());
                    ImVec2 bSz = fnt->CalcTextSizeA(FS * 0.88f, FLT_MAX, 0, badge);
                    float  by = y + FS * 1.45f + 2.f;
                    dl->AddRectFilled({ sx, by }, { sx + bSz.x + 10.f, by + bSz.y + 4.f }, cBadge, 3.f);
                    dl->AddText(fnt, FS * 0.88f, { sx + 5.f, by + 2.f }, cSub, badge);
                    };
                drawStat(pctStr, "ACCURACY", sy);
                drawStat(FmtNum(cur.goals), "GOALS", sy + GAP);
                drawStat(FmtNum(cur.attempts), "ATTEMPTS", sy + GAP * 2.f);
                ImGui::Dummy({ PW, PR * 2.f + 16.f });
            }
            ImGui::Dummy({ PW, 14.f });
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    // ── Window 2: edit panel (receives mouse input) ───────────────────────
    if (!showEditPanel || !sessionActive) return;

    // Layout constants — column positions
    // [SHOT NAME 12..190] [G- 198] [G val 224] [G+ 242] [A- 290] [A val 316] [A+ 334]
    const float EPW = 370.f;
    const float ROW = 38.f;
    const float HDR_EP = 34.f;
    const float COL_HDR_Y_OFFSET = 8.f;
    const float ROWS_START = HDR_EP + 26.f;
    const float NAME_MAX_X = 188.f;   // max x before truncation
    const float G_MINUS = 198.f;
    const float G_VAL = 226.f;
    const float G_PLUS = 250.f;
    const float A_MINUS = 294.f;
    const float A_VAL = 322.f;
    const float A_PLUS = 346.f;
    const float BTN_W = 20.f;
    const float BTN_H = 18.f;
    const int   NSHOTS = (int)shotStats.size();
    const float EP_H = ROWS_START + NSHOTS * ROW + 36.f;

    float epX = winX - EPW - 10.f;
    if (epX < 4.f) epX = winX + PW + 10.f;
    float epY = winY;

    ImGuiWindowFlags epFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::SetNextWindowPos({ epX, epY }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ EPW, EP_H }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 4, 4 });

    if (!ImGui::Begin("##MechTrakEdit", nullptr, epFlags)) {
        ImGui::End();
        ImGui::PopStyleVar(4);
        return;
    }

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      wp = ImGui::GetWindowPos();
        ImFont* fnt = ImGui::GetFont();
        const float FS = fnt->FontSize;

        // bg + border
        dl->AddRectFilled(wp, { wp.x + EPW, wp.y + EP_H }, cBg, 14.f);
        dl->AddRect(wp, { wp.x + EPW, wp.y + EP_H }, cBorder, 14.f, 0, 1.f);
        dl->AddRect({ wp.x + 1, wp.y + 1 }, { wp.x + EPW - 1, wp.y + EP_H - 1 }, IM_COL32(255, 255, 255, 7), 14.f, 0, 1.f);

        // header
        dl->AddRectFilled(wp, { wp.x + EPW, wp.y + HDR_EP }, cHdr, 14.f);
        dl->AddRectFilled({ wp.x, wp.y + HDR_EP * 0.5f }, { wp.x + EPW, wp.y + HDR_EP }, cHdr, 0.f);
        dl->AddLine({ wp.x, wp.y + HDR_EP }, { wp.x + EPW, wp.y + HDR_EP }, IM_COL32(120, 170, 255, 65), 1.f);
        std::string epTitleStr = "EDIT SESSION  [" + editKeyStr + " TO CLOSE]";
        const char* ET = epTitleStr.c_str();
        ImVec2 etSz = fnt->CalcTextSizeA(FS, FLT_MAX, 0, ET);
        dl->AddText(fnt, FS, { wp.x + (EPW - etSz.x) * 0.5f, wp.y + (HDR_EP - etSz.y) * 0.5f }, cWhite, ET);

        // column headers
        float hy = wp.y + HDR_EP + COL_HDR_Y_OFFSET;
        dl->AddText(fnt, FS * 0.72f, { wp.x + 12.f,   hy }, cSub, "SHOT");
        dl->AddText(fnt, FS * 0.72f, { wp.x + G_MINUS, hy }, cSub, "GOALS");
        dl->AddText(fnt, FS * 0.72f, { wp.x + A_MINUS, hy }, cSub, "ATTEMPTS");
        dl->AddLine({ wp.x + 8.f, hy + 13.f }, { wp.x + EPW - 8.f, hy + 13.f }, IM_COL32(70, 100, 200, 60), 1.f);

        // button helper — x is left edge, y is row top (ry), centers button vertically in row
        auto btn = [&](const char* id, const char* lbl, float x, float ry) -> bool {
            float bY = ry + (ROW - BTN_H) * 0.5f;
            ImVec2 bMin = { wp.x + x, wp.y + bY };
            ImVec2 bMax = { bMin.x + BTN_W, bMin.y + BTN_H };
            bool hov = ImGui::IsMouseHoveringRect(bMin, bMax);
            dl->AddRectFilled(bMin, bMax, hov ? cBtnHov : cBtnBg, 5.f);
            dl->AddRect(bMin, bMax, IM_COL32(100, 150, 255, 90), 5.f);
            ImVec2 lSz = fnt->CalcTextSizeA(FS, FLT_MAX, 0, lbl);
            dl->AddText(fnt, FS, { bMin.x + (BTN_W - lSz.x) * 0.5f, bMin.y + (BTN_H - lSz.y) * 0.5f }, cWhite, lbl);
            return hov && ImGui::IsMouseClicked(0);
            };

        // name truncation helper — clips to NAME_MAX_X with "..." suffix
        auto truncName = [&](const std::string& full) -> std::string {
            const float maxW = NAME_MAX_X - 12.f;
            ImVec2 sz = fnt->CalcTextSizeA(FS * 0.88f, FLT_MAX, 0, full.c_str());
            if (sz.x <= maxW) return full;
            std::string t = full;
            while (t.size() > 1) {
                t.pop_back();
                std::string candidate = t + "...";
                ImVec2 csz = fnt->CalcTextSizeA(FS * 0.88f, FLT_MAX, 0, candidate.c_str());
                if (csz.x <= maxW) return candidate;
            }
            return "...";
            };

        int rowIdx = 0;
        for (auto& [shotNum, s] : shotStats) {
            float ry = ROWS_START + rowIdx * ROW;
            float rcy = ry + (ROW - FS) * 0.5f;

            // row highlight for active shot
            if (shotNum == currentShotNumber)
                dl->AddRectFilled({ wp.x + 6.f, wp.y + ry }, { wp.x + EPW - 6.f, wp.y + ry + ROW },
                    IM_COL32(40, 80, 180, 55), 5.f);

            // shot name (truncated)
            std::string sLabel = "Shot " + std::to_string(shotNum);
            if (shotTypes.count(shotNum) && !shotTypes[shotNum].empty())
                sLabel += " - " + shotTypes[shotNum];
            sLabel = truncName(sLabel);
            dl->AddText(fnt, FS * 0.88f, { wp.x + 12.f, wp.y + rcy }, IM_COL32(195, 220, 255, 215), sLabel.c_str());

            // accuracy bar (thin strip at bottom of row)
            float acc = s.attempts > 0 ? (float)s.goals / s.attempts : 0.f;
            float bx = wp.x + 12.f, by2 = wp.y + ry + ROW - 5.f, barW = NAME_MAX_X - 12.f;
            dl->AddRectFilled({ bx, by2 }, { bx + barW, by2 + 3.f }, IM_COL32(255, 255, 255, 18), 2.f);
            if (acc > 0.f)
                dl->AddRectFilled({ bx, by2 }, { bx + barW * acc, by2 + 3.f },
                    acc >= 0.5f ? cGreen : IM_COL32(220, 150, 40, 255), 2.f);

            // vertical divider between name and goals column
            dl->AddLine({ wp.x + NAME_MAX_X + 4.f, wp.y + ry + 4.f },
                { wp.x + NAME_MAX_X + 4.f, wp.y + ry + ROW - 4.f },
                IM_COL32(70, 100, 200, 40), 1.f);

            // GOALS: [-] val [+]
            if (btn(("g-" + std::to_string(shotNum)).c_str(), "-", G_MINUS, ry))
                if (onEditShot) onEditShot(shotNum, s.goals > 0 ? s.goals - 1 : 0, s.attempts);
            std::string gStr = std::to_string(s.goals);
            ImVec2 gSz = fnt->CalcTextSizeA(FS, FLT_MAX, 0, gStr.c_str());
            dl->AddText(fnt, FS, { wp.x + G_VAL + (24.f - gSz.x) * 0.5f, wp.y + rcy }, cWhite, gStr.c_str());
            if (btn(("g+" + std::to_string(shotNum)).c_str(), "+", G_PLUS, ry))
                if (onEditShot) onEditShot(shotNum, s.goals + 1, s.attempts);

            // vertical divider between goals and attempts
            dl->AddLine({ wp.x + A_MINUS - 6.f, wp.y + ry + 4.f },
                { wp.x + A_MINUS - 6.f, wp.y + ry + ROW - 4.f },
                IM_COL32(70, 100, 200, 40), 1.f);

            // ATTEMPTS: [-] val [+]
            if (btn(("a-" + std::to_string(shotNum)).c_str(), "-", A_MINUS, ry))
                if (onEditShot) onEditShot(shotNum, s.goals, s.attempts > 0 ? s.attempts - 1 : 0);
            std::string aStr = std::to_string(s.attempts);
            ImVec2 aSz = fnt->CalcTextSizeA(FS, FLT_MAX, 0, aStr.c_str());
            dl->AddText(fnt, FS, { wp.x + A_VAL + (24.f - aSz.x) * 0.5f, wp.y + rcy }, cWhite, aStr.c_str());
            if (btn(("a+" + std::to_string(shotNum)).c_str(), "+", A_PLUS, ry))
                if (onEditShot) onEditShot(shotNum, s.goals, s.attempts + 1);

            if (rowIdx < NSHOTS - 1)
                dl->AddLine({ wp.x + 8.f, wp.y + ry + ROW }, { wp.x + EPW - 8.f, wp.y + ry + ROW },
                    IM_COL32(70, 100, 200, 30), 1.f);
            rowIdx++;
        }

        // flip key hint at bottom — two lines: key on top, label below
        auto flipKeyCvar = cvarManager->getCvar("mechtrak_key_flip_last");
        std::string flipKeyStr = flipKeyCvar ? flipKeyCvar.getStringValue() : "F7";
        std::string flipLine1 = "[" + flipKeyStr + "] Reverse Last Attempt";
        ImVec2 fl1Sz = fnt->CalcTextSizeA(FS * 0.85f, FLT_MAX, 0, flipLine1.c_str());
        float fl1Y = wp.y + EP_H - 18.f;
        dl->AddText(fnt, FS * 0.85f, { wp.x + (EPW - fl1Sz.x) * 0.5f, fl1Y }, cSub, flipLine1.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar(4);
}

void HUD::DrawMiniGraph(CanvasWrapper& canvas,
    std::map<int, ShotStats>& shotStats, int currentShotNumber, int startX, int startY)
{
    if (!shotStats.count(currentShotNumber)) return;
    auto& s = shotStats[currentShotNumber];
    if (s.attemptHistory.empty()) return;
    int gW = 180, gH = 70, wSz = 5;
    canvas.SetColor(100, 200, 255, 255);
    Vector2F last = { -1.f, -1.f };
    for (size_t i = 0; i < s.attemptHistory.size(); i++) {
        int goals = 0;
        size_t ws = (i >= (size_t)(wSz - 1)) ? i - wSz + 1 : 0;
        int    aw = (int)(i - ws + 1);
        for (size_t j = ws; j <= i; j++) if (s.attemptHistory[j]) goals++;
        float xp = startX + (i * gW / (float)s.attemptHistory.size());
        float yp = startY + gH - ((float)goals / aw * gH);
        Vector2F pt = { xp, yp };
        if (last.X >= 0) canvas.DrawLine(last, pt, 2.f);
        last = pt;
    }
}