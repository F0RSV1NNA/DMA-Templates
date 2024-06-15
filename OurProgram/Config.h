#pragma once
#include <string>

struct Config
{
    std::string kmboxIP;
    int kmboxPort;
    std::string kmboxUUID;
    int screenWidth;
    int screenHeight;
    bool exampleBool;
    float exampleFloat;
    int exampleInt;
};

bool ReadConfig(Config& config);
bool SaveConfig(const Config& config);
