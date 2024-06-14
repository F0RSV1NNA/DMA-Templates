#include <kmboxNet.h>
#include <Memory.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

struct Config
{
    std::string kmboxIP;
    int kmboxPort;
    std::string kmboxUUID;
    int screenWidth;
    int screenHeight;
};

bool ReadConfig(Config& config)
{
    std::ifstream configFile("config.cfg");
    if (!configFile.is_open())
    {
        std::cerr << "Failed to open config.cfg" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(configFile, line))
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (key == "KmboxIP") config.kmboxIP = value;
        else if (key == "KmboxPort") config.kmboxPort = std::stoi(value);
        else if (key == "KmboxUUID") config.kmboxUUID = value;
        else if (key == "screenWidth") config.screenWidth = std::stoi(value);
        else if (key == "screenHeight") config.screenHeight = std::stoi(value);
    }

    return true;
}

void InitKMbox(const Config& config)
{
    std::cout << "Initializing KMbox with IP: " << config.kmboxIP
        << ", Port: " << config.kmboxPort
        << ", UUID: " << config.kmboxUUID << std::endl;

    int initResult = kmNet_init(const_cast<char*>(config.kmboxIP.c_str()),
        const_cast<char*>(std::to_string(config.kmboxPort).c_str()),
        const_cast<char*>(config.kmboxUUID.c_str()));

    if (initResult != success)
    {
        std::cerr << "Failed to initialize KMbox, error code: " << initResult << std::endl;
        exit(1);
    }

    std::cout << "KMbox initialized successfully\n";
}

void MoveMouse(const Config& config)
{
    int deltaX = 600;
    int deltaY = 0;

    std::cout << "Moving mouse 600 pixels to the right\n";
    kmNet_mouse_move_auto(deltaX, deltaY, 2000); 
}

void DMAExample()
{
    if (!mem.Init("explorer.exe", false, false))
    {
        std::cerr << "Failed to initialize DMA" << std::endl;
        exit(1);
    }

    std::cout << "DMA initialized" << std::endl;

    if (!mem.GetKeyboard()->InitKeyboard())
    {
        std::cerr << "Failed to initialize keyboard hotkeys through kernel." << std::endl;
        exit(1);
    }

    uintptr_t base = mem.GetBaseDaddy("explorer.exe");

    int value = 0;
    if (mem.Read(base + 0x66, &value, sizeof(value)))
        std::cout << "Read Value" << std::endl;
    else
        std::cout << "Failed to read Value" << std::endl;
    std::cout << "Value: " << value << std::endl;

    // Example keyboard usage.
    std::cout << "Continuing once 'A' has been pressed." << std::endl;
    while (!mem.GetKeyboard()->IsKeyDown(0x41))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main()
{
    Config config;
    if (!ReadConfig(config))
    {
        return 1;
    }

    // Initialize KMbox
    InitKMbox(config);

    // DMA example
    DMAExample();
    std::cout << "DMA example completed" << std::endl;

    // KMbox example
    MoveMouse(config);
    std::cout << "KMbox example completed" << std::endl;
    std::cout << "Made By F0RSV1NNA" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}
