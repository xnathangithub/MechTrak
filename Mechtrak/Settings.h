#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"

class Settings {
public:
    static void CreateFile(
        std::shared_ptr<CVarManagerWrapper> cvarManager
    );

    static void RegisterCvars(
        std::shared_ptr<CVarManagerWrapper> cvarManager
    );
};