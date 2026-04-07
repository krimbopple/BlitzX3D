#ifndef DX9_GXRUNTIME_H
#define DX9_GXRUNTIME_H

#include <d3d9.h>
#include "gxruntime.h"

class gxRuntimeDX9 : public gxRuntime {
public:
    gxRuntimeDX9(HINSTANCE hinst, const std::string& cmd_line, HWND hwnd);
    ~gxRuntimeDX9();

    gxGraphics* openGraphics(int w, int h, int d, int driver, int flags) override;

    IDirect3D9* getD3D9()   const { return d3d9; }
    IDirect3DDevice9* getDevice() const { return device; }
    void              resetDevice();

private:
    IDirect3D9* d3d9;
    IDirect3DDevice9* device;
    bool createD3D9Device(int w, int h, bool windowed);
};

#endif