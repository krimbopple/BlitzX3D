# BlitzX3D
This is a fork of Blitz3D TSS, originally based on Blitz3D and maintained by ZiYueCommentary.

## License

Please read the license files before using, modifying, or distributing this
project!!!

BlitzX3D contains code derived from Blitz3D. The original Blitz3D code remains
licensed under the zlib/libpng License.

Original BlitzX3D contributions by Chris A. (krimbopple) are licensed under the
BlitzX3D Community License.

---

## How to Build

### Prepare

- Visual Studio Community 2022
  - Desktop development with C++
  - C++ MFC for latest v143 build tools (x86 & x64)
  - C++ ATL for latest v143 build tools (x86 & x64)
  - ASP.NET and web development
    
### Before building `linker` or `bbruntime_dll`:

1. Copy `linker/cryptseed.h.example` to `linker/cryptseed.h`.
2. Open `cryptseed.h` and change `RUNTIME_KEY_SEED` to any nonzero value of your own choosing.
   
### Steps

1. Open `blitz3d.sln` in Visual Studio 2022.
2. Select the **Release**/**Debug** configuration and rebuild the entire solution.
3. All done! You can find the output files in the `_release` and `_release/bin` directories. Feel free to delete any `.pdb` and `.ilk` files.

## Properly Debugging

Because the launcher (`bblaunch`) spawns the IDE (`ide.exe`) and then exits, Visual Studio will lose the debug session by default. To debug properly:

1. Install the **Microsoft Child Process Debugging Power Tool 2022+** from the [Marketplace](https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool2022).
2. In Visual Studio, go to **Debug -> Other Debug Targets -> Child Process Debugging Settings** and enable **"Enable child process debugging"**.

The debugger will now automatically attach to `ide.exe` when it launches, and you will now be able to properly debug your programs!

---

## In Memory of Mark Sibly

[Mark Sibly](https://github.com/blitz-research), the creator of Blitz3D, passed away on 12 December 2024. 🕯️
