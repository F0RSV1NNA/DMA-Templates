# DMA Template Project
***You must build in release mode!***

This project provides a simple and easy template for DMA Tools/Automation\
all the includes and nessecary files already provided with an example main.cpp


Metick DMA library for memory reading\
The KMbox API for mouse movement\
ImGui for menu's and overlays.

Please show some appreciation to those dev's here!

- [Metick](https://github.com/Metick)
- [Kvmaibox](https://github.com/kvmaibox)
- [Imgui](https://github.com/kvmaibox)

## Kmbox Info

  Ensure you configure your `config.cfg` file with the correct settings for your specific KMbox.\
  Without proper configuration, the KMbox will not connect.


  you can find more documentation inside the `kmboxnet.h` & `kmboxnet.cpp` files inside the "/KmBoxNet" folder\
  (i translated the comments from chinese to english)
  
## ImGui Info
  This is just a small example on how to use imgui for a menu with config options or an overlay for a fuser.\
  this could be half broken im unsure because i do not own a working fuser (3440x1440 and fusers dont agree) 
  

## Prerequisites

  **Build Requirements:**

  **Before compiling, ensure you have the following DLLs and files in the compiled folder as well as the DMALibrary folder:**

  - `FTD3XX.dll`
  - `leechcore.dll`
  - `vmm.dll`
  - `symsrv.dll`
  - `dbghelp.dll`
  - `info.db`

  Currently, these files are included, but feel free to replace them with new ones or build your own.

  **You can find these files at the following repositories:**

  - [FTD3XX.dll](https://ftdichip.com/drivers/d3xx-drivers/)
  - [Leechcore](https://github.com/ufrisk/LeechCore/releases)
  - [MemProcFS](https://github.com/ufrisk/MemProcFS/releases)

    *(As of writing this, the versions are Leechcore V2.18 and MemprocFS V5.9)*

  **The project requires the following libraries in the `DMALibrary/libs/` folder:**

  - `leechcore.lib`
  - `vmm.lib`

    **You can obtain these files from the following repositories:**

  - [Leechcore](https://github.com/ufrisk/LeechCore)
  - [MemProcFS](https://github.com/ufrisk/MemProcFS/)

  - [Lib Files Compiled if you're lazy](https://github.com/ufrisk/MemProcFS/tree/master/includes/lib32)

## License

  This DMALibrary/template is open-source and licensed under the MIT License. Feel free to use, modify, and distribute it in your projects.
