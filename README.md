# BlitzX3D

This is a fork of Blitz3D TSS, originally based on Blitz3D and maintained by ZiYueCommentary.

This project was made to explore updates to rendering, audio, performance, and compatibility whilst keeping the original Blitz3D workflow intact.

## Extending BlitzX3D
Please read [this document](EXTENDING.md).

## How to Build

### Prepare

- Visual Studio Community 2022
  - Desktop development with C++
  - C++ MFC for latest v142 build tools (x86 & x64)
  - C++ ATL for latest v142 build tools (x86 & x64)
  - ASP.NET and web development

### Steps

1. Open `blitz3d.sln` in Visual Studio 2022.
2. Select Release or Debug config and rebuild the entire solution.
3. All done! You can find output files in the `_release` and `_release/bin` dirs. Feel free to delete `.pdb` and `.ilk` files here.

- **Note:** BlitzX3D uses the OpenAL audio backend,
when redistributing programs built with BlitzX3D, ensure the required OpenAL runtime libraries are included with your application if not already present on the system.

## In memory of Mark Sibly

[Mark Sibly](https://github.com/blitz-research), the author of Blitz3D, died on 12 December 2024. 🕯️
