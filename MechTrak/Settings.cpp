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

    settingsFile << "MECH TRAK|HUD Options\n";
    settingsFile << "BUTTON|Hide HUD|mechtrak_hide_hud_toggle\n";
    settingsFile << "BUTTON|Compact HUD|mechtrak_compact_hud_toggle\n";

    settingsFile.close();
    cvarManager->log("Settings file created!");
}

void Settings::RegisterCvars(std::shared_ptr<CVarManagerWrapper> cvarManager)
{
    cvarManager->registerCvar("mechtrak_hide_hud", "0", "Hide the HUD overlay",
        true, true, 0, true, 1);
    cvarManager->registerCvar("mechtrak_compact_hud", "0", "Use compact HUD in top right",
        true, true, 0, true, 1);
    cvarManager->registerCvar("mechtrak_remove_graph", "0", "Remove the bar graph",
        true, true, 0, true, 1);

    cvarManager->registerNotifier("mechtrak_hide_hud_toggle",
        [cvarManager](std::vector<std::string> args) {
            auto cvar = cvarManager->getCvar("mechtrak_hide_hud");
            cvar.setValue(cvar.getBoolValue() ? 0 : 1);
        }, "Toggle Hide HUD", PERMISSION_ALL);

    cvarManager->registerNotifier("mechtrak_compact_hud_toggle",
        [cvarManager](std::vector<std::string> args) {
            auto cvar = cvarManager->getCvar("mechtrak_compact_hud");
            cvar.setValue(cvar.getBoolValue() ? 0 : 1);
        }, "Toggle Compact HUD", PERMISSION_ALL);
}