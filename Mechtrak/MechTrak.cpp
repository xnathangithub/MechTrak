#pragma comment(lib, "pluginsdk.lib")
#include "pch.h"
#include "MechTrak.h"
#include <filesystem>
#include <fstream>

BAKKESMOD_PLUGIN(MechTrak, "MechTrak", "1.0", PLUGINTYPE_FREEPLAY)

// Required by PluginWindow — sets the ImGui context so our DLL uses the
// same ImGui instance as BakkesMod (critical, prevents context mismatch crash)
void MechTrak::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// Called every frame while the PluginWindow is open.
// ImGui::Begin/End and ImDrawList are fully valid here.
void MechTrak::Render()
{
    HUD::RenderImGui(cvarManager, gameWrapper,
        shotStats, shotTypes, currentShotNumber, sessionActive,
        showEditPanel,
        [this]() {
            cvarManager->executeCommand("stats_end_session");
        },
        [this](int shotNum, int newGoals, int newAttempts) {
            if (!shotStats.count(shotNum)) return;
            shotStats[shotNum].goals = newGoals < 0 ? 0 : newGoals;
            shotStats[shotNum].attempts = newAttempts < 0 ? 0 : newAttempts;
            // Clamp goals <= attempts
            if (shotStats[shotNum].goals > shotStats[shotNum].attempts)
                shotStats[shotNum].goals = shotStats[shotNum].attempts;
            if (!uploadInProgress) {
                uploadInProgress = true;
                std::thread([this]() {
                    Session::SaveToFile(cvarManager, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes);
                    Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
                    uploadInProgress = false;
                    }).detach();
            }
        }
    );
}

void MechTrak::onLoad()
{
    // ── Auto-add to plugins.cfg ──────────────────────────────────────────
    std::filesystem::path cfgPath = gameWrapper->GetBakkesModPath() / "cfg" / "plugins.cfg";
    std::ifstream cfgIn(cfgPath);
    bool found = false;
    std::string line;
    if (cfgIn.is_open()) {
        while (std::getline(cfgIn, line))
            if (line.find("plugin load mechtrak") != std::string::npos) { found = true; break; }
        cfgIn.close();
    }
    if (!found) {
        std::ofstream out(cfgPath, std::ios::app);
        if (out.is_open()) { out << "\nplugin load mechtrak\n"; out.close(); }
    }

    cvarManager->log("Mech Trak plugin loaded!");

    Settings::CreateFile(cvarManager);
    Settings::RegisterCvars(cvarManager);

    currentShotNumber = 1;
    shotTypes[currentShotNumber] = "Unknown";
    shotStats[currentShotNumber] = ShotStats();
    roundActive = false;
    sessionId = Session::GenerateId();
    sessionStartTime = std::chrono::system_clock::now();

    // ── Game event hooks ─────────────────────────────────────────────────
    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        std::bind(&MechTrak::OnBallExplode, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.Team_TA.EventScoreUpdated",
        std::bind(&MechTrak::OnGoalScored, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.GameEvent_TrainingEditor_TA.StartNewRound",
        std::bind(&MechTrak::OnShotReset, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.Ball_TA.OnCarTouch",
        [this](std::string) { roundActive = true; });
    gameWrapper->HookEvent("Function TAGame.GameEvent_TrainingEditor_TA.OnInit",
        [this](std::string) {
            auto compactCvar = cvarManager->getCvar("mechtrak_compact_hud");
            if (compactCvar && compactCvar.getBoolValue()) {
                cvarManager->executeCommand("togglemenu mechtrak");
            }
        });
    // ── Canvas HUD drawable ───────────────────────────────────────────────
    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
        HUD::Render(canvas, cvarManager, gameWrapper,
            shotStats, shotTypes, currentShotNumber, sessionActive);
        });

    // ── Compact HUD toggle — opens/closes the PluginWindow ────────────────
    // Settings.cpp registers a BUTTON that calls mechtrak_compact_hud_toggle.
    // We override that notifier here to also fire togglemenu.
    cvarManager->registerNotifier("mechtrak_compact_hud_toggle",
        [this](std::vector<std::string>) {
            auto cvar = cvarManager->getCvar("mechtrak_compact_hud");
            bool nowOn = !cvar.getBoolValue();
            cvar.setValue(nowOn ? 1 : 0);
            // Open or close the PluginWindow
            cvarManager->executeCommand(nowOn ? "togglemenu mechtrak" : "togglemenu mechtrak");
        }, "Toggle compact HUD", PERMISSION_ALL);

    // ── Other notifiers ───────────────────────────────────────────────────
    cvarManager->registerNotifier("stats_current", [this](std::vector<std::string>) {
        if (shotStats.count(currentShotNumber)) {
            auto& s = shotStats[currentShotNumber];
            cvarManager->log("Shot " + std::to_string(currentShotNumber) + ": " + std::to_string(s.attempts) + " attempts, " + std::to_string(s.goals) + " goals");
        }
        }, "Shows current shot stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_show", [this](std::vector<std::string>) {
        for (auto& [id, s] : shotStats) {
            float acc = s.attempts > 0 ? (float)s.goals / s.attempts * 100.f : 0.f;
            cvarManager->log("Shot " + std::to_string(id) + ": " + std::to_string(s.attempts) + " attempts, " + std::to_string(s.goals) + " goals (" + std::to_string((int)acc) + "%)");
        }
        }, "Shows all stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_reset", [this](std::vector<std::string>) {
        shotStats.clear(); currentShotNumber = 1; shotStats[currentShotNumber] = ShotStats();
        cvarManager->log("Stats reset!");
        }, "Reset all stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_save", [this](std::vector<std::string>) {
        Session::SaveToFile(cvarManager, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes);
        }, "Save stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_upload", [this](std::vector<std::string>) {
        Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
        }, "Upload session", PERMISSION_ALL);


    cvarManager->registerNotifier("mechtrak_toggle_edit", [this](std::vector<std::string>) {
        showEditPanel = !showEditPanel;
        }, "Toggle edit panel", PERMISSION_ALL);
    std::string editKey = cvarManager->getCvar("mechtrak_key_edit_panel").getStringValue();
    std::string flipKey = cvarManager->getCvar("mechtrak_key_flip_last").getStringValue();
    cvarManager->executeCommand("bind " + editKey + " mechtrak_toggle_edit");
    cvarManager->executeCommand("bind " + flipKey + " mechtrak_flip_last");

    cvarManager->registerNotifier("stats_key_prev", [this](std::vector<std::string>) {
        if (currentShotNumber > 1) {
            currentShotNumber--;
            if (!shotStats.count(currentShotNumber))shotStats[currentShotNumber] = ShotStats();
        }
        }, "Previous shot", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_key_next", [this](std::vector<std::string>) {
        if (shotStats.empty()) return;
        auto it = shotStats.find(currentShotNumber);
        if (it != shotStats.end()) {
            ++it;
            if (it != shotStats.end())
                currentShotNumber = it->first;
            // if already at last shot, do nothing
        }
        }, "Next shot", PERMISSION_ALL);


    // Flip last attempt
    cvarManager->registerNotifier("mechtrak_flip_last", [this](std::vector<std::string>) {
        if (!shotStats.count(currentShotNumber)) return;
        auto& s = shotStats[currentShotNumber];
        if (s.attemptHistory.empty()) return;

        bool lastWasGoal = s.attemptHistory.back();
        if (lastWasGoal) {
            s.goals--;
            s.attemptHistory.back() = false;
            cvarManager->log("Corrected: goal -> miss");
        }
        else {
            s.goals++;
            s.attemptHistory.back() = true;
            cvarManager->log("Corrected: miss -> goal");
        }
        if (s.goals < 0) s.goals = 0;
        if (s.goals > s.attempts) s.goals = s.attempts;

        if (!uploadInProgress) {
            uploadInProgress = true;
            std::thread([this]() {
                Session::SaveToFile(cvarManager, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes);
                Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
                uploadInProgress = false;
                }).detach();
        }
        }, "Flip last attempt goal/miss", PERMISSION_ALL);


 

    // Auto-rebind when user changes the key in settings
    cvarManager->getCvar("mechtrak_key_edit_panel").addOnValueChanged([this](std::string oldVal, CVarWrapper newCvar) {
        std::string newKey = newCvar.getStringValue();
        if (!oldVal.empty())
            this->cvarManager->executeCommand("unbind " + oldVal);
        if (!newKey.empty())
            this->cvarManager->executeCommand("bind " + newKey + " mechtrak_toggle_edit");
        });

    cvarManager->getCvar("mechtrak_key_flip_last").addOnValueChanged([this](std::string oldVal, CVarWrapper newCvar) {
        std::string newKey = newCvar.getStringValue();
        if (!oldVal.empty())
            this->cvarManager->executeCommand("unbind " + oldVal);
        if (!newKey.empty())
            this->cvarManager->executeCommand("bind " + newKey + " mechtrak_flip_last");
        });

    // Initial key binds — use cvar values so they survive settings changes
    cvarManager->executeCommand("bind " +
        cvarManager->getCvar("mechtrak_key_edit_panel").getStringValue() + " mechtrak_toggle_edit");
    cvarManager->executeCommand("bind " +
        cvarManager->getCvar("mechtrak_key_flip_last").getStringValue() + " mechtrak_flip_last");

    cvarManager->registerNotifier("stats_end_session", [this](std::vector<std::string>) {
        sessionActive = false;
        Session::SaveToFile(cvarManager, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes);
        Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
        shotStats.clear();
        shotTypes.clear();
        currentShotNumber = 1;
        cvarManager->log("Session ended.");
        }, "End session", PERMISSION_ALL);

    cvarManager->executeCommand("bind F8 stats_key_prev");
    cvarManager->executeCommand("bind F9 stats_key_next");

    // ── Delayed init ──────────────────────────────────────────────────────
    gameWrapper->SetTimeout([this](GameWrapper*) {
        Session::LoadActive(cvarManager, sessionId, sessionActive, shotStats, shotTypes, currentShotNumber);
          
        if (gameWrapper->IsInCustomTraining()) {
            cvarManager->executeCommand("togglemenu mechtrak");
        }

        std::thread([this]() {
            std::string token = Session::GetPluginToken(cvarManager);
            if (!token.empty())
                cvarManager->log("MechTrak: token ready (" + std::to_string(token.length()) + " chars)");
            }).detach();

        Heartbeat::Start(cvarManager, gameWrapper, sessionId, sessionActive, shotStats, shotTypes, currentShotNumber);
        }, 2.0f);
}

void MechTrak::onUnload()
{
    cvarManager->log("Mech Trak plugin unloaded!");
}

// ─── Game event handlers ──────────────────────────────────────────────────────

void MechTrak::OnBallExplode(std::string)
{
    if (!gameWrapper->IsInCustomTraining()) return;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastGoalTime).count() < 12) {
        lastGoalTime = std::chrono::steady_clock::time_point(); return;
    }
    if (!shotStats.count(currentShotNumber))shotStats[currentShotNumber] = ShotStats();
    shotStats[currentShotNumber].attempts++;
    shotStats[currentShotNumber].attemptHistory.push_back(false);
    justRecordedAttempt = true;
    if (!uploadInProgress) {
        uploadInProgress = true;
        auto sc = shotStats; auto tc = shotTypes; auto ic = sessionId; auto tm = sessionStartTime;
        std::thread([this, sc, tc, ic, tm]()mutable {
            Session::SaveToFile(cvarManager, ic, sessionActive, tm, sc, tc);
            Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
            uploadInProgress = false; }).detach();
    }
}

void MechTrak::OnShotReset(std::string)
{
    if (!gameWrapper->IsInCustomTraining()) return;
    lastGoalTime = std::chrono::steady_clock::time_point();
    if (roundActive && !justRecordedAttempt) {
        if (!shotStats.count(currentShotNumber))shotStats[currentShotNumber] = ShotStats();
        shotStats[currentShotNumber].attempts++;
        shotStats[currentShotNumber].attemptHistory.push_back(false);
    }
    roundActive = false; justRecordedAttempt = false;
    if (!uploadInProgress) {
        uploadInProgress = true;
        auto sc = shotStats; auto tc = shotTypes; auto ic = sessionId; auto tm = sessionStartTime;
        std::thread([this, sc, tc, ic, tm]()mutable {
            Session::SaveToFile(cvarManager, ic, sessionActive, tm, sc, tc);
            Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
            uploadInProgress = false; }).detach();
    }
}

void MechTrak::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInCustomTraining()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;
    auto teams = server.GetTeams();
    if (teams.Count() == 0) return;
    int currentScore = teams.Get(0).GetScore();
    if (currentScore <= lastKnownScore) { lastKnownScore = currentScore; return; }
    lastKnownScore = currentScore;
    auto now = std::chrono::steady_clock::now();
    auto timeSince = std::chrono::duration_cast<std::chrono::seconds>(now - lastGoalTime).count();
    if (timeSince >= 2 && timeSince < 12 && shotStats.count(currentShotNumber)) {
        shotStats[currentShotNumber].attempts++;
        shotStats[currentShotNumber].attemptHistory.push_back(true);
    }
    lastGoalTime = std::chrono::steady_clock::now();
    if (shotStats.count(currentShotNumber)) {
        shotStats[currentShotNumber].goals++;
        shotStats[currentShotNumber].attemptHistory.push_back(true);
        justRecordedAttempt = true;
    }
    if (!uploadInProgress) {
        uploadInProgress = true;
        auto sc = shotStats; auto tc = shotTypes; auto ic = sessionId; auto tm = sessionStartTime;
        std::thread([this, sc, tc, ic, tm]()mutable {
            Session::SaveToFile(cvarManager, ic, sessionActive, tm, sc, tc);
            Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive, sessionStartTime, shotStats, shotTypes, currentShotNumber);
            uploadInProgress = false; }).detach();
    }
}