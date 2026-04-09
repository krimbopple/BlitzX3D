#define DX9

#include <windows.h>
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

extern "C" void* CreateD3D9Device(HWND hwnd, int width, int height, bool windowed) {
    IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d9) return nullptr;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = windowed;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = windowed ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = hwnd;
    pp.BackBufferWidth = width;
    pp.BackBufferHeight = height;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;

    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
    d3d9->Release();
    return FAILED(hr) ? nullptr : device;
}

extern "C" void ReleaseD3D9Device(void* device) {
    if (device) ((IDirect3DDevice9*)device)->Release();
}