#ifndef DX9_GXGRAPHICS_H
#define DX9_GXGRAPHICS_H

#include <d3d9.h>
#include "gxgraphics.h"

class gxRuntimeDX9;

class gxGraphicsDX9 : public gxGraphics {
public:
    gxGraphicsDX9(gxRuntimeDX9* rt, IDirect3DDevice9* dev, int w, int h, int flags);
    ~gxGraphicsDX9();

    void      flip(bool vwait) override;

    IDirect3DDevice9* getDevice() const { return device; }

private:
    gxRuntimeDX9* runtime;
    IDirect3DDevice9* device;
    int width, height;
    bool lost;
};

#endif