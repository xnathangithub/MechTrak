#pragma once
#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"
#include "HUD.h"
#include "Session.h"
#include "Heartbeat.h"
#include "Settings.h"
#include <map>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

std::atomic<bool> uploadInProgress{ false };

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class MechTrak : public BakkesMod::Plugin::BakkesModPlugin,
    public BakkesMod::Plugin::PluginWindow
{
private:
    std::map<int, ShotStats> shotStats;
    std::map<int, std::string> shotTypes;
    int currentShotNumber = 1;

    std::chrono::steady_clock::time_point lastGoalTime;
    int lastKnownScore = 0;
    bool roundActive = false;
    bool justRecordedAttempt = false;

    std::string sessionId;
    std::chrono::system_clock::time_point sessionStartTime;
    bool sessionActive = false;

    void OnBallExplode(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnShotReset(std::string eventName);

public:
    void onLoad()   override;
    void onUnload() override;

    // PluginWindow — ImGui is valid here, called each frame when menu is open
    void        Render()                        override;
    std::string GetMenuName()                   override { return "mechtrak"; }
    std::string GetMenuTitle()                  override { return "MECHTRAK.GG"; }
    void        SetImGuiContext(uintptr_t ctx)  override;
    bool        ShouldBlockInput()              override { return false; }
    bool        IsActiveOverlay()               override { return false; }
    void        OnOpen()                        override {}
    void        OnClose()                       override {}
};