#include <kmboxNet.h>
#include <KmboxB.h>
#include <Memory.h>
#include "Menu.h"
#include "Config.h"

#include <iostream>
#include <thread>
#include <chrono>
_com comPort;

void InitKMbox(const Config& config)
{
    if (config.kmboxType == "Net")
    {
        std::cout << "Initializing KMbox NET with IP: " << config.kmboxIP
            << ", Port: " << config.kmboxPort
            << ", UUID: " << config.kmboxUUID << std::endl;

        int initResult = kmNet_init(const_cast<char*>(config.kmboxIP.c_str()),
            const_cast<char*>(std::to_string(config.kmboxPort).c_str()),
            const_cast<char*>(config.kmboxUUID.c_str()));

        if (initResult != success)
        {
            std::cerr << "Failed to initialize KMbox NET, error code: " << initResult << std::endl;
            exit(1);
        }
        std::cout << "KMbox NET initialized successfully\n";
    }
    else if (config.kmboxType == "BPro")
    {
        std::cout << "Initializing KMbox BPro on COM port " << config.KmboxComPort << std::endl;

        if (comPort.open(config.KmboxComPort, 115200)) // Adjust baud rate as necessary
        {
            std::cout << "KMbox BPro initialized successfully." << std::endl;
        }
        else
        {
            std::cerr << "Failed to initialize KMbox BPro on COM port " << config.KmboxComPort << std::endl;
            exit(1);
        }
    }
    else
    {
        std::cerr << "Wrong KMbox type specified, please type exactly Net or BPro in your cfg." << config.kmboxType << std::endl;
        exit(1);
    }

    
    int deltaX = 600; // Move right
    int deltaY = 0;   // No vertical move
    if (config.kmboxType == "NET")
    {
        std::cout << "Moving mouse 600 pixels to the right (NET)\n";
        kmNet_mouse_move_auto(deltaX, deltaY, 2000); // Adjust timing as necessary
    }
    else if (config.kmboxType == "BPro")
    {
        // Docx file included in the project files /Kmbox
        std::cout << "Moving mouse 600 pixels to the right (BPro)\n";
        char cmd[1024] = { 0 };
        sprintf_s(cmd, "km.move(%d, %d, 10)\r\n", deltaX, deltaY);
        comPort.write(cmd);
    }
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

    std::cout << "KMbox example completed" << std::endl;
    std::cout << "Made By F0RSV1NNA" << std::endl;

    // Menu/Overlay(fuser) Example
    Render(config);

    return 0;
}


