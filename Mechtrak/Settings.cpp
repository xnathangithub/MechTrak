#include "pch.h"
#include "Settings.h"
#include <fstream>

void Settings::CreateFile(std::shared_ptr<CVarManagerWrapper> cvarManager)
{
    char* appdata = getenv("APPDATA");
    if (!appdata) return;
    std::string settingsPath = std::string(appdata) +
        "\\bakkesmod\\bakkesmod\\plugins\\settings\\MechTrak.set";
    std::ofstream settingsFile(settingsPath);
    if (!settingsFile.is_open()) {
        cvarManager->log("Failed to create settings file");
        return;
    }
    settingsFile << "MECH TRAK|Settings\n";
    settingsFile << "1|Hide HUD|mechtrak_hide_hud\n";
    settingsFile << "4|HUD X Position|mechtrak_hud_x|0|1\n";
    settingsFile << "4|HUD Y Position|mechtrak_hud_y|0|1\n";
    settingsFile << "12|Edit Panel Key (default F4)|mechtrak_key_edit_panel\n";
    settingsFile << "12|Flip Last Attempt Key (default F7)|mechtrak_key_flip_last\n";
    settingsFile.close();
    cvarManager->log("Settings file created!");
}

void Settings::RegisterCvars(std::shared_ptr<CVarManagerWrapper> cvarManager)
{
    cvarManager->registerCvar("mechtrak_hide_hud", "0", "Hide the HUD overlay",
        true, true, 0, true, 1);
    cvarManager->registerCvar("mechtrak_compact_hud", "1", "Use compact HUD in top right",
        true, true, 0, true, 1);
    cvarManager->registerCvar("mechtrak_remove_graph", "0", "Remove the bar graph",
        true, true, 0, true, 1);
    cvarManager->registerCvar("mechtrak_hud_x", "-1", "HUD X position (0-1, -1 = top right)",
        true, true, -1.f, true, 1.f);
    cvarManager->registerCvar("mechtrak_hud_y", "0.02", "HUD Y position (0-1)",
        true, true, 0.f, true, 1.f);

    cvarManager->registerCvar("mechtrak_key_edit_panel", "F4", "Key to toggle the edit panel");
    cvarManager->registerCvar("mechtrak_key_flip_last", "F7", "Key to flip last attempt goal/miss");

    cvarManager->registerNotifier("mechtrak_hide_hud_toggle",
        [cvarManager](std::vector<std::string> args) {
            auto cvar = cvarManager->getCvar("mechtrak_hide_hud");
            cvar.setValue(cvar.getBoolValue() ? 0 : 1);
        }, "Toggle Hide HUD", PERMISSION_ALL);
}