#include "dx9_gxruntime.h"
#include "dx9_gxgraphics.h"

gxRuntimeDX9::gxRuntimeDX9(HINSTANCE hinst, const std::string& cmd_line, HWND hwnd) : gxRuntime(hinst, cmd_line, hwnd), d3d9(nullptr), device(nullptr)
{
    d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
}

gxRuntimeDX9::~gxRuntimeDX9() {
    if (device) device->Release();
    if (d3d9)  d3d9->Release();
}

gxGraphics* gxRuntimeDX9::openGraphics(int w, int h, int d, int driver, int flags) {
    if (graphics) return nullptr;

    bool windowed = (flags & gxGraphics::GRAPHICS_WINDOWED) != 0;
    if (!createD3D9Device(w, h, windowed)) return nullptr;

    graphics = new gxGraphicsDX9(this, device, w, h, flags);
    return graphics;
}

bool gxRuntimeDX9::createD3D9Device(int w, int h, bool windowed) {
    if (!d3d9) return false;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = windowed;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = windowed ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = w;
    pp.BackBufferHeight = h;
    pp.hDeviceWindow = hwnd;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;

    DWORD behavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    if (FAILED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        behavior, &pp, &device)))
    {
        behavior = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        if (FAILED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            behavior, &pp, &device)))
            return false;
    }
    return true;
}

void gxRuntimeDX9::resetDevice() {
    // stub stub stub 
}