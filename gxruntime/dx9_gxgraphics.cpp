#include "dx9_gxgraphics.h"
#include "dx9_gxruntime.h"

gxGraphicsDX9::gxGraphicsDX9(gxRuntimeDX9* rt, IDirect3DDevice9* dev, int w, int h, int flags) : gxGraphics(rt, nullptr, nullptr, nullptr, flags), runtime(rt), device(dev), width(w), height(h), lost(false)
{
    device->AddRef();
}

gxGraphicsDX9::~gxGraphicsDX9() {
    if (device) device->Release();
}

void gxGraphicsDX9::flip(bool vwait) {
    if (device) {
        HRESULT hr = device->Present(nullptr, nullptr, runtime->hwnd, nullptr);
        if (FAILED(hr)) lost = true;
    }
}