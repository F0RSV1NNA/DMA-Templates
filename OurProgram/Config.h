#pragma once
#include <string>

struct Config
{
    std::string kmboxIP;
    int kmboxPort;
    std::string kmboxUUID;
    int KmboxComPort;
    int screenWidth;
    int screenHeight;
    bool exampleBool;
    float exampleFloat;
    int exampleInt;
    std::string kmboxType;
};

bool ReadConfig(Config& config);
bool SaveConfig(const Config& config);
