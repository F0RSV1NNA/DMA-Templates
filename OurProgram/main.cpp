#include <kmboxNet.h>
#include <Memory.h>
#include "Menu.h"
#include "Config.h"

#include <iostream>
#include <thread>
#include <chrono>

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
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    uintptr_t base = mem.GetBaseDaddy("explorer.exe");

    int value = 0;
    if (mem.Read(base + 0x66, &value, sizeof(value)))
        std::cout << "Read Value" << std::endl;
    else
        std::cout << "Failed to read Value" << std::endl;
    std::cout << "Value: " << value << std::endl;

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

    // Menu/Overlay(fuser) Example
    Render(config);

    return 0;
}


