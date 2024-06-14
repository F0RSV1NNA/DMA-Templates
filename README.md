# DMA Template Project

This is a simple and easy template for DMA Tools/Automation in C++ using the metick DMA library and KMbox API.

Please show some appreciation to metick and KMbox for their work on the DMA library and KMbox API.

- [Metick](https://github.com/Metick)
- [Kvmaibox](https://github.com/kvmaibox)

## Kmbox Info
there is a config.cfg file included with examples on connecting to your specific kmbox.

## Prerequisites
**YOU MUST BUILD IN RELEASE MODE!**


**Before Attempting Compile make sure you have the following DLLs and files in the Compiled Folder as well as the DMAlibrary folder:**

(Currently i am including them but feel free to replace with new ones/build you own)

- FTD3XX.dll
- leechcore.dll 
- vmm.dll
- symsrv.dll
- dbghelp.dll
- info.db

**You can find these files at the following repositories:**

- [FTD3XX.dll](https://ftdichip.com/drivers/d3xx-drivers/)
- [leechcore](https://github.com/ufrisk/LeechCore/releases)
- [MemProcFS](https://github.com/ufrisk/MemProcFS/releases)

  (as of writing this, the versions are Leechcore V2.18 and MemprocFS V5.9)

**The project requires the following libraries in the DMALibrary\libs\ folder:**

- leechcore.lib
- vmm.lib

**You can obtain these files from the following repositories:**

- [leechcore](https://github.com/ufrisk/LeechCore)
- [MemProcFS](https://github.com/ufrisk/MemProcFS/)

- [Lib Files Compiled if you're lazy](https://github.com/ufrisk/MemProcFS/tree/master/includes/lib32)



## License

This DMALibrary/template is open-source and licensed under the MIT License. Feel free to use, modify, and distribute it in your projects.