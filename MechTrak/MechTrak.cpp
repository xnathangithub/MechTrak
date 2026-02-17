#pragma comment(lib, "pluginsdk.lib")
#include "pch.h"
#include "MechTrak.h"
#include <filesystem>
#include <fstream>

BAKKESMOD_PLUGIN(MechTrak, "MechTrak", "1.0", PLUGINTYPE_FREEPLAY)

void MechTrak::onLoad()
{
    // Auto-add to plugins.cfg if not already there
    std::filesystem::path configPath = gameWrapper->GetBakkesModPath();
    configPath = configPath / "cfg" / "plugins.cfg";

    std::ifstream configFile(configPath);
    std::string line;
    bool foundPlugin = false;

    // Check if already in config
    if (configFile.is_open()) {
        while (std::getline(configFile, line)) {
            if (line.find("plugin load mechtrak") != std::string::npos) {
                foundPlugin = true;
                break;
            }
        }
        configFile.close();
    }

    // Add to config if not found
    if (!foundPlugin) {
        std::ofstream configFileOut(configPath, std::ios::app);
        if (configFileOut.is_open()) {
            configFileOut << "\nplugin load mechtrak\n";
            configFileOut.close();
            cvarManager->log("MechTrak added to auto-load!");
        }
    }

    cvarManager->log("Mech Trak plugin loaded!");

    // Create settings catagory in bakkes plugin manager
    Settings::CreateFile(cvarManager);
    Settings::RegisterCvars(cvarManager);

    // Init
    currentShotNumber = 1;
    shotTypes[currentShotNumber] = "Unknown";
    shotStats[currentShotNumber] = ShotStats();
    roundActive = false;
    sessionId = Session::GenerateId();
    sessionStartTime = std::chrono::system_clock::now();

    // Hook events
    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        std::bind(&MechTrak::OnBallExplode, this, std::placeholders::_1));

    gameWrapper->HookEvent("Function TAGame.Team_TA.EventScoreUpdated",
        std::bind(&MechTrak::OnGoalScored, this, std::placeholders::_1));

    gameWrapper->HookEvent("Function TAGame.GameEvent_TrainingEditor_TA.StartNewRound",
        std::bind(&MechTrak::OnShotReset, this, std::placeholders::_1));

    gameWrapper->HookEvent("Function TAGame.Ball_TA.OnCarTouch",
        [this](std::string eventName) { roundActive = true; });

    // HUD
    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
        HUD::Render(canvas, cvarManager, gameWrapper,
            shotStats, shotTypes, currentShotNumber, sessionActive);
        });

	// Notifiers / Commands
    cvarManager->registerNotifier("stats_current", [this](std::vector<std::string> args) {
        if (shotStats.find(currentShotNumber) != shotStats.end()) {
            auto stats = shotStats[currentShotNumber];
            cvarManager->log("Shot " + std::to_string(currentShotNumber) + ": " +
                std::to_string(stats.attempts) + " attempts, " +
                std::to_string(stats.goals) + " goals");
        }
        }, "Shows current shot stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_show", [this](std::vector<std::string> args) {
        for (const auto& shot : shotStats) {
            float accuracy = shot.second.attempts > 0
                ? (float)shot.second.goals / shot.second.attempts * 100.0f : 0.0f;
            cvarManager->log("Shot " + std::to_string(shot.first) + ": " +
                std::to_string(shot.second.attempts) + " attempts, " +
                std::to_string(shot.second.goals) + " goals (" +
                std::to_string((int)accuracy) + "%)");
        }
        }, "Shows all stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_reset", [this](std::vector<std::string> args) {
        shotStats.clear();
        currentShotNumber = 1;
        shotStats[currentShotNumber] = ShotStats();
        cvarManager->log("Stats reset!");
        }, "Reset all stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_save", [this](std::vector<std::string> args) {
        Session::SaveToFile(cvarManager, sessionId, sessionActive,
            sessionStartTime, shotStats, shotTypes);
        }, "Save stats", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_upload", [this](std::vector<std::string> args) {
        Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive,
            sessionStartTime, shotStats, shotTypes, currentShotNumber);
        }, "Upload session", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_key_prev", [this](std::vector<std::string> args) {
        if (currentShotNumber > 1) {
            currentShotNumber--;
            if (shotStats.find(currentShotNumber) == shotStats.end())
                shotStats[currentShotNumber] = ShotStats();
        }
        }, "Previous shot", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_key_next", [this](std::vector<std::string> args) {
        currentShotNumber++;
        if (shotStats.find(currentShotNumber) == shotStats.end())
            shotStats[currentShotNumber] = ShotStats();
        if (shotTypes.find(currentShotNumber) == shotTypes.end())
            shotTypes[currentShotNumber] = "Unknown";
        }, "Next shot", PERMISSION_ALL);

    cvarManager->registerNotifier("stats_end_session", [this](std::vector<std::string> args) {
        sessionActive = false;
        Session::SaveToFile(cvarManager, sessionId, sessionActive,
            sessionStartTime, shotStats, shotTypes);
        Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive,
            sessionStartTime, shotStats, shotTypes, currentShotNumber);

        sessionId = Session::GenerateId();
        sessionStartTime = std::chrono::system_clock::now();
        sessionActive = true;
        shotStats.clear();
        shotTypes.clear();
        currentShotNumber = 1;
        shotStats[currentShotNumber] = ShotStats();
        shotTypes[currentShotNumber] = "Unknown";
        cvarManager->log("New session started: " + sessionId);
        }, "End session", PERMISSION_ALL);

    cvarManager->executeCommand("bind F8 stats_key_prev");
    cvarManager->executeCommand("bind F9 stats_key_next");

    // Load active session and start heartbeat
    gameWrapper->SetTimeout([this](GameWrapper* gw) {
        Session::LoadActive(cvarManager, sessionId, sessionActive,
            shotStats, shotTypes, currentShotNumber);
        Heartbeat::Start(cvarManager, gameWrapper, sessionId, sessionActive,
            shotStats, shotTypes, currentShotNumber);
        }, 2.0f);
}

void MechTrak::onUnload()
{
    cvarManager->log("Mech Trak plugin unloaded!");
}


/*
                //  LOGIC FOR COMPUTING ATTEMPTS AND GOALS. //


Attempts are triggered for every ball explosion or resetting ball through manual reset. 
Ball explosions trigger when user scores or when the ball touches ground after time expires.

For every goal it will add an attempt and also add 1 to the goals scored.

Due to detection during goal replay (if ball explodes in replay then it will count as an attempt).
I corrected this error by implementing a 12 second timer to ignore any attempts after a goal is scored.

This timer is reset/removed either after a user resets ball manually or player touches ball again.
This is implemented so if timer is still running it will cancel the timer so the attempt is counted.


                //LOGIC FOR RESETING BALL. //

Reseting will only count as an attempt if user touches the ball first, this is to prevent accidental attempts when a round was not supposed to start.

*/

void MechTrak::OnBallExplode(std::string eventName)
{
    if (!gameWrapper->IsInCustomTraining()) return;

    auto now = std::chrono::steady_clock::now();
    auto timeSinceGoal = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastGoalTime).count();

    if (timeSinceGoal < 12) {
        lastGoalTime = std::chrono::steady_clock::time_point();
        return;
    }

    if (shotStats.find(currentShotNumber) == shotStats.end())
        shotStats[currentShotNumber] = ShotStats();

    shotStats[currentShotNumber].attempts++;
    shotStats[currentShotNumber].attemptHistory.push_back(false);
    justRecordedAttempt = true;

    if (!uploadInProgress) {
        uploadInProgress = true;
        auto shotStatsCopy = shotStats;
        auto shotTypesCopy = shotTypes;
        auto sessionIdCopy = sessionId;
        auto sessionStartTimeCopy = sessionStartTime;

        std::thread([this, shotStatsCopy, shotTypesCopy, sessionIdCopy, sessionStartTimeCopy]() mutable {
            Session::SaveToFile(cvarManager, sessionIdCopy, sessionActive,
                sessionStartTimeCopy, shotStatsCopy, shotTypesCopy);
            Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive,
                sessionStartTime, shotStats, shotTypes, currentShotNumber);
            uploadInProgress = false;
            }).detach();
    }
}

void MechTrak::OnShotReset(std::string eventName)
{
    if (!gameWrapper->IsInCustomTraining()) return;

    lastGoalTime = std::chrono::steady_clock::time_point();

    if (roundActive && !justRecordedAttempt) {
        if (shotStats.find(currentShotNumber) == shotStats.end())
            shotStats[currentShotNumber] = ShotStats();
        shotStats[currentShotNumber].attempts++;
        shotStats[currentShotNumber].attemptHistory.push_back(false);
    }

    roundActive = false;
    justRecordedAttempt = false;

    if (!uploadInProgress) {
        uploadInProgress = true;
        auto shotStatsCopy = shotStats;
        auto shotTypesCopy = shotTypes;
        auto sessionIdCopy = sessionId;
        auto sessionStartTimeCopy = sessionStartTime;

        std::thread([this, shotStatsCopy, shotTypesCopy, sessionIdCopy, sessionStartTimeCopy]() mutable {
            Session::SaveToFile(cvarManager, sessionIdCopy, sessionActive,
                sessionStartTimeCopy, shotStatsCopy, shotTypesCopy);
            Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive,
                sessionStartTime, shotStats, shotTypes, currentShotNumber);
            uploadInProgress = false;
            }).detach();
    }
}

void MechTrak::OnGoalScored(std::string eventName)
{
    if (!gameWrapper->IsInCustomTraining()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto teams = server.GetTeams();
    if (teams.Count() == 0) return;

    int currentScore = teams.Get(0).GetScore();
    if (currentScore <= lastKnownScore) {
        lastKnownScore = currentScore;
        return;
    }
    lastKnownScore = currentScore;

    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastGoal = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastGoalTime).count();

    if (timeSinceLastGoal >= 2 && timeSinceLastGoal < 12 &&
        shotStats.find(currentShotNumber) != shotStats.end()) {
        shotStats[currentShotNumber].attempts++;
        shotStats[currentShotNumber].attemptHistory.push_back(true);
    }

    lastGoalTime = std::chrono::steady_clock::now();

    if (shotStats.find(currentShotNumber) != shotStats.end()) {
        shotStats[currentShotNumber].goals++;
        shotStats[currentShotNumber].attemptHistory.push_back(true);
        justRecordedAttempt = true;
    }

    if (!uploadInProgress) {
        uploadInProgress = true;
        auto shotStatsCopy = shotStats;
        auto shotTypesCopy = shotTypes;
        auto sessionIdCopy = sessionId;
        auto sessionStartTimeCopy = sessionStartTime;

        std::thread([this, shotStatsCopy, shotTypesCopy, sessionIdCopy, sessionStartTimeCopy]() mutable {
            Session::SaveToFile(cvarManager, sessionIdCopy, sessionActive,
                sessionStartTimeCopy, shotStatsCopy, shotTypesCopy);
            Session::Upload(cvarManager, gameWrapper, sessionId, sessionActive,
                sessionStartTime, shotStats, shotTypes, currentShotNumber);
            uploadInProgress = false;
            }).detach();
    }
}