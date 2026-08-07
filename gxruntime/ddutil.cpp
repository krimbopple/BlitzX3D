#include "std.h"
#include "ddutil.h"
#include "asmcoder.h"
#include "gxcanvas.h"
#include "gxgraphics.h"
#include "gxruntime.h"

extern gxRuntime* gx_runtime;

#include <freeimage.h>

static AsmCoder asm_coder;

static thread_local std::string g_lastImageError;

const std::string& ddUtil::getLastImageError() {
    return g_lastImageError;
}

bool ddUtil::hasAlphaChannel(const std::string& file) {
    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(file.c_str(), 0);
    if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(file.c_str());
    if (fif == FIF_UNKNOWN) return false;

    FIBITMAP* fib = FreeImage_Load(fif, file.c_str(), 0);
    if (!fib) return false;

    bool has = (FreeImage_IsTransparent(fib) == TRUE) || (FreeImage_GetColorType(fib) == FIC_RGBALPHA);
    FreeImage_Unload(fib);
    return has;
}

bool ddUtil::hasActualAlpha(const std::string& file) {
    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(file.c_str(), 0);
    if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(file.c_str());
    if (fif == FIF_UNKNOWN) return false;

    FIBITMAP* fib = FreeImage_Load(fif, file.c_str(), 0);
    if (!fib) return false;

    if (FreeImage_GetColorType(fib) != FIC_RGBALPHA && !FreeImage_IsTransparent(fib)) {
        FreeImage_Unload(fib);
        return false;
    }

    FIBITMAP* fib32 = (FreeImage_GetBPP(fib) == 32) ? fib : FreeImage_ConvertTo32Bits(fib);
    if (!fib32) {
        if (fib32 != fib) FreeImage_Unload(fib);
        return false;
    }

    bool hasNonOpaque = false;
    int w = FreeImage_GetWidth(fib32);
    int h = FreeImage_GetHeight(fib32);
    for (int y = 0; y < h && !hasNonOpaque; ++y) {
        BYTE* bits = FreeImage_GetScanLine(fib32, y);
        for (int x = 0; x < w; ++x) {
            BYTE alpha = bits[x * 4 + 3];
            if (alpha != 255) {
                hasNonOpaque = true;
                break;
            }
        }
    }

    if (fib32 != fib) FreeImage_Unload(fib32);
    else FreeImage_Unload(fib);
    return hasNonOpaque;
}

PixelFormat::~PixelFormat() {
    if (plot_code) VirtualFree(plot_code, 0, MEM_RELEASE);
}

void PixelFormat::setFormat(D3DFORMAT fmt) {
    if (plot_code) VirtualFree(plot_code, 0, MEM_RELEASE);
    plot_code = (char*)VirtualAlloc(0, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    point_code = plot_code + 64;

    switch (fmt) {
    case D3DFMT_A8R8G8B8:
        depth = 32; amask = 0xff000000; rmask = 0x00ff0000; gmask = 0x0000ff00; bmask = 0x000000ff; break;
    case D3DFMT_X8R8G8B8:
        depth = 32; amask = 0;          rmask = 0x00ff0000; gmask = 0x0000ff00; bmask = 0x000000ff; break;
    case D3DFMT_R5G6B5:
        depth = 16; amask = 0;          rmask = 0xf800;     gmask = 0x07e0;     bmask = 0x001f;     break;
    case D3DFMT_A1R5G5B5:
        depth = 16; amask = 0x8000;     rmask = 0x7c00;     gmask = 0x03e0;     bmask = 0x001f;     break;
    case D3DFMT_A4R4G4B4:
        depth = 16; amask = 0xf000;     rmask = 0x0f00;     gmask = 0x00f0;     bmask = 0x000f;     break;
    default:
        depth = 32; amask = 0xff000000; rmask = 0x00ff0000; gmask = 0x0000ff00; bmask = 0x000000ff; break;
    }

    pitch = depth / 8;
    argbfill = 0;
    if (!amask) argbfill |= 0xff000000;
    if (!rmask) argbfill |= 0x00ff0000;
    if (!gmask) argbfill |= 0x0000ff00;
    if (!bmask) argbfill |= 0x000000ff;

    calcShifts(amask, &ashr, &ashl); ashr += 24;
    calcShifts(rmask, &rshr, &rshl); rshr += 16;
    calcShifts(gmask, &gshr, &gshl); gshr += 8;
    calcShifts(bmask, &bshr, &bshl);
    plot = (Plot)(void*)plot_code;
    point = (Point)(void*)point_code;
    asm_coder.CodePlot(plot_code, depth, amask, rmask, gmask, bmask);
    asm_coder.CodePoint(point_code, depth, amask, rmask, gmask, bmask);
}

// bruh
static bool hasRealAlpha(FIBITMAP* fib) {
    if (FreeImage_IsTransparent(fib) == TRUE) return true;
    return FreeImage_GetColorType(fib) == FIC_RGBALPHA;
}

static void adjustTexSize(int* width, int* height, IDirect3DDevice9* dev) {
    D3DCAPS9 caps;
    if (FAILED(dev->GetDeviceCaps(&caps))) {
        *width = *height = 256;
        return;
    }

    if (!(caps.TextureCaps & D3DPTEXTURECAPS_POW2)) {
        return;
    }

    int w = 1;
    while (w < *width)  w <<= 1;
    int h = 1;
    while (h < *height) h <<= 1;

    if (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) {
        if (w > h) h = w;
        else       w = h;
    }

    if (int maxAsp = caps.MaxTextureAspectRatio) {
        int asp = w > h ? w / h : h / w;
        if (asp > maxAsp) {
            if (w > h) h = w / maxAsp;
            else       w = h / maxAsp;
        }
    }

    if (caps.MaxTextureWidth && w > (int)caps.MaxTextureWidth)  w = caps.MaxTextureWidth;
    if (caps.MaxTextureHeight && h > (int)caps.MaxTextureHeight) h = caps.MaxTextureHeight;

    *width = w;
    *height = h;
}

void ddUtil::buildMipMaps(IDirect3DTexture9* tex) {
    if (!tex) return;
    DWORD levels = tex->GetLevelCount();
    if (levels <= 1) return;

    for (DWORD mip = 0; mip + 1 < levels; ++mip) {
        D3DLOCKED_RECT src_lr, dst_lr;
        D3DSURFACE_DESC src_desc, dst_desc;
        tex->GetLevelDesc(mip, &src_desc);
        tex->GetLevelDesc(mip + 1, &dst_desc);

        if (FAILED(tex->LockRect(mip, &src_lr, nullptr, D3DLOCK_READONLY))) break;
        if (FAILED(tex->LockRect(mip + 1, &dst_lr, nullptr, 0))) { tex->UnlockRect(mip); break; }

        PixelFormat src_fmt(src_desc.Format);
        PixelFormat dst_fmt(dst_desc.Format);

        unsigned char* src_p = (unsigned char*)src_lr.pBits;
        unsigned char* dst_p = (unsigned char*)dst_lr.pBits;

        for (UINT y = 0; y < dst_desc.Height; ++y) {
            unsigned char* src_t = src_p + (y * 2) * src_lr.Pitch;
            unsigned char* dst_t = dst_p + y * dst_lr.Pitch;
            for (UINT x = 0; x < dst_desc.Width; ++x) {
                unsigned char* p0 = src_t + x * 2 * src_fmt.getPitch();
                unsigned char* p1 = p0 + src_fmt.getPitch();
                unsigned char* p2 = p0 + src_lr.Pitch;
                unsigned char* p3 = p2 + src_fmt.getPitch();
                unsigned c0 = src_fmt.getPixel(p0), c1 = src_fmt.getPixel(p1);
                unsigned c2 = src_fmt.getPixel(p2), c3 = src_fmt.getPixel(p3);
                unsigned argb =
                    ((c0 & 0xfcfcfcfc) >> 2) + ((c1 & 0xfcfcfcfc) >> 2) +
                    ((c2 & 0xfcfcfcfc) >> 2) + ((c3 & 0xfcfcfcfc) >> 2);
                argb += (((c0 & 0x03030303) + (c1 & 0x03030303) +
                    (c2 & 0x03030303) + (c3 & 0x03030303)) >> 2) & 0x03030303;
                dst_fmt.setPixel(dst_t + x * dst_fmt.getPitch(), argb);
            }
        }
        tex->UnlockRect(mip + 1);
        tex->UnlockRect(mip);
    }
}

void ddUtil::copy(IDirect3DDevice9* dev, IDirect3DSurface9* dest_surf, int dx, int dy, int dw, int dh, IDirect3DSurface9* src_surf, int sx, int sy, int sw, int sh) {
    RECT srcRect = { sx, sy, sx + sw, sy + sh };
    RECT destRect = { dx, dy, dx + dw, dy + dh };
    HRESULT hr = dev->StretchRect(src_surf, &srcRect, dest_surf, &destRect, D3DTEXF_LINEAR);
    if (SUCCEEDED(hr)) return;

    D3DSURFACE_DESC src_desc, dst_desc;
    src_surf->GetDesc(&src_desc);
    dest_surf->GetDesc(&dst_desc);

    bool dstIsRT = (dst_desc.Usage & D3DUSAGE_RENDERTARGET) != 0;
    if (dstIsRT && dev) {
        IDirect3DSurface9* staging = nullptr;
        if (SUCCEEDED(dev->CreateOffscreenPlainSurface(dw, dh, dst_desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr))) {
            D3DLOCKED_RECT src_lr, stg_lr;
            if (SUCCEEDED(src_surf->LockRect(&src_lr, nullptr, D3DLOCK_READONLY))) {
                if (SUCCEEDED(staging->LockRect(&stg_lr, nullptr, 0))) {
                    PixelFormat src_fmt(src_desc.Format);
                    PixelFormat dst_fmt(dst_desc.Format);

                    unsigned char* src_p = (unsigned char*)src_lr.pBits + sy * src_lr.Pitch + sx * src_fmt.getPitch();
                    unsigned char* stg_p = (unsigned char*)stg_lr.pBits;

                    for (int y = 0; y < dh; ++y) {
                        unsigned char* src_row = src_p + src_lr.Pitch * (y * sh / dh);
                        unsigned char* stg_row = stg_p + stg_lr.Pitch * y;
                        for (int x = 0; x < dw; ++x) {
                            dst_fmt.setPixel(stg_row + x * dst_fmt.getPitch(),
                                dst_fmt.fromARGB(src_fmt.toARGB(
                                    src_fmt.getPixel(src_row + src_fmt.getPitch() * (x * sw / dw)))));
                        }
                    }

                    staging->UnlockRect();
                    RECT stagedRect = { 0, 0, dw, dh };
                    POINT destPoint = { dx, dy };
                    dev->UpdateSurface(staging, &stagedRect, dest_surf, &destPoint);
                }
                src_surf->UnlockRect();
            }
            staging->Release();
        }
        return;
    }

    D3DLOCKED_RECT src_lr, dst_lr;
    if (FAILED(src_surf->LockRect(&src_lr, nullptr, D3DLOCK_READONLY))) return;
    if (FAILED(dest_surf->LockRect(&dst_lr, nullptr, 0))) {
        src_surf->UnlockRect();
        return;
    }

    PixelFormat src_fmt(src_desc.Format);
    PixelFormat dst_fmt(dst_desc.Format);

    unsigned char* src_p = (unsigned char*)src_lr.pBits + sy * src_lr.Pitch + sx * src_fmt.getPitch();
    unsigned char* dst_p = (unsigned char*)dst_lr.pBits + dy * dst_lr.Pitch + dx * dst_fmt.getPitch();

    for (int y = 0; y < dh; ++y) {
        unsigned char* src_row = src_p + src_lr.Pitch * (y * sh / dh);
        unsigned char* dst_row = dst_p + dst_lr.Pitch * y;
        for (int x = 0; x < dw; ++x) {
            dst_fmt.setPixel(dst_row + x * dst_fmt.getPitch(),
                src_fmt.getPixel(src_row + src_fmt.getPitch() * (x * sw / dw)));
        }
    }

    dest_surf->UnlockRect();
    src_surf->UnlockRect();
}

IDirect3DSurface9* ddUtil::createDisplaySurface(int w, int h, gxGraphics* gfx) {
    IDirect3DSurface9* surf = nullptr;
    gfx->dir3dDev->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf, nullptr);
    return surf;
}

IDirect3DTexture9* ddUtil::createTextureSurface(int w, int h, int flags, gxGraphics* gfx) {
    return createTextureSurface(w, h, flags, gfx, false);
}

IDirect3DTexture9* ddUtil::createTextureSurface(int w, int h, int flags, gxGraphics* gfx, bool renderTarget) {
    IDirect3DDevice9* dev = gfx->dir3dDev;
    if (!dev) return nullptr;
    adjustTexSize(&w, &h, dev);

    bool hasAlpha = (flags & gxCanvas::CANVAS_TEX_ALPHA) != 0;
    bool hasMask = (flags & gxCanvas::CANVAS_TEX_MASK) != 0;
    bool hasMips = (flags & gxCanvas::CANVAS_TEX_MIPMAP) != 0;

    DWORD usage = 0;
    D3DPOOL pool = D3DPOOL_DEFAULT;
    UINT mipLevels = hasMips ? 0 : 1;
    if (renderTarget) {
        usage = D3DUSAGE_RENDERTARGET;
        mipLevels = 1;
        D3DCAPS9 caps;
        if (SUCCEEDED(dev->GetDeviceCaps(&caps)) && (caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)) {
            usage |= D3DUSAGE_AUTOGENMIPMAP;
            mipLevels = 0;
        }
    }
    else {
        usage = D3DUSAGE_DYNAMIC;
    }

    D3DFORMAT fmt = (hasAlpha || hasMask) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    if (flags & gxCanvas::CANVAS_TEX_HICOLOR) fmt = D3DFMT_A4R4G4B4;

    IDirect3DTexture9* tex = nullptr;
    HRESULT hr = dev->CreateTexture(w, h, mipLevels, usage, fmt, pool, &tex, nullptr);
    if (FAILED(hr)) return nullptr;
    return tex;
}

IDirect3DCubeTexture9* ddUtil::createCubeTextureSurface(int size, int flags, gxGraphics* gfx) {
    IDirect3DDevice9* dev = gfx->dir3dDev;
    if (!dev) return nullptr;

    int w = size, h = size;
    adjustTexSize(&w, &h, dev);
    int adjSize = w > h ? w : h;

    bool hasAlpha = (flags & gxCanvas::CANVAS_TEX_ALPHA) != 0;
    bool hasMask = (flags & gxCanvas::CANVAS_TEX_MASK) != 0;

    D3DFORMAT fmt = (hasAlpha || hasMask) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    if (flags & gxCanvas::CANVAS_TEX_HICOLOR) fmt = D3DFMT_A4R4G4B4;

    IDirect3DCubeTexture9* cubeTex = nullptr;
    HRESULT hr = dev->CreateCubeTexture(adjSize, 1, D3DUSAGE_DYNAMIC, fmt, D3DPOOL_DEFAULT, &cubeTex, nullptr);
    if (FAILED(hr)) return nullptr;
    return cubeTex;
}

static void buildMask(FIBITMAP* fib, BYTE* bits, int pitch, int w, int h) {
    for (int y = 0; y < h; ++y) {
        BYTE* src = FreeImage_GetScanLine(fib, h - 1 - y);
        DWORD* dst = (DWORD*)(bits + y * pitch);
        for (int x = 0; x < w; ++x) {
            RGBQUAD* p = (RGBQUAD*)(src + x * 4);
            unsigned rgb = ((unsigned)p->rgbRed << 16) | ((unsigned)p->rgbGreen << 8) | p->rgbBlue;
            dst[x] = rgb ? (0xff000000 | rgb) : 0;
        }
    }
}

static void buildAlpha(FIBITMAP* fib, BYTE* bits, int pitch, int w, int h, bool whiten) {
    for (int y = 0; y < h; ++y) {
        BYTE* src = FreeImage_GetScanLine(fib, h - 1 - y);
        DWORD* dst = (DWORD*)(bits + y * pitch);
        for (int x = 0; x < w; ++x) {
            RGBQUAD* p = (RGBQUAD*)(src + x * 4);
            unsigned alpha = ((unsigned)p->rgbRed + p->rgbGreen + p->rgbBlue) / 3;
            unsigned argb = (alpha << 24) | ((unsigned)p->rgbRed << 16) | ((unsigned)p->rgbGreen << 8) | p->rgbBlue;
            if (whiten) argb |= 0xffffff;
            dst[x] = argb;
        }
    }
}

IDirect3DSurface9* ddUtil::loadDisplaySurface(const std::string& file, int flags, gxGraphics* gfx) {
    g_lastImageError.clear();

    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(file.c_str(), 0);
    if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(file.c_str());
    if (fif == FIF_UNKNOWN) { g_lastImageError = "Unknown format: " + file; return nullptr; }

    FIBITMAP* fib = FreeImage_Load(fif, file.c_str(), 0);
    if (!fib) { g_lastImageError = "Load failed: " + file; return nullptr; }

    FIBITMAP* fib32;
    if (FreeImage_GetBPP(fib) == 32) {
        fib32 = fib;
    }
    else {
        fib32 = FreeImage_ConvertTo32Bits(fib);
        FreeImage_Unload(fib);
        if (!fib32) { g_lastImageError = "ConvertTo32Bits failed: " + file; return nullptr; }
    }

    int w = FreeImage_GetWidth(fib32);
    int h = FreeImage_GetHeight(fib32);

    IDirect3DSurface9* surf = nullptr;
    if (FAILED(gfx->dir3dDev->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf, nullptr))) {
        g_lastImageError = "CreateOffscreenPlainSurface failed: " + file;
        FreeImage_Unload(fib32);
        return nullptr;
    }

    D3DLOCKED_RECT lr;
    if (FAILED(surf->LockRect(&lr, nullptr, 0))) {
        g_lastImageError = "LockRect failed: " + file;
        surf->Release();
        FreeImage_Unload(fib32);
        return nullptr;
    }

    bool hasMask = (flags & gxCanvas::CANVAS_TEX_MASK) != 0;
    bool hasAlpha = (flags & gxCanvas::CANVAS_TEX_ALPHA) != 0;
    bool hasActualAlpha = hasRealAlpha(fib32);
    BYTE* bits = (BYTE*)lr.pBits;

    if (hasMask) {
        buildMask(fib32, bits, lr.Pitch, w, h);
    }
    else if (hasAlpha) {
        if (hasActualAlpha) {
            for (int y = 0; y < h; ++y) {
                BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
                DWORD* dst = (DWORD*)(bits + y * lr.Pitch);
                for (int x = 0; x < w; ++x) {
                    RGBQUAD* p = (RGBQUAD*)(src + x * 4);
                    dst[x] = ((DWORD)p->rgbReserved << 24) |
                        ((DWORD)p->rgbRed << 16) |
                        ((DWORD)p->rgbGreen << 8) |
                        p->rgbBlue;
                }
            }
        }
        else {
            buildAlpha(fib32, bits, lr.Pitch, w, h, false);
        }
    }
    else {
        for (int y = 0; y < h; ++y) {
            BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
            DWORD* dst = (DWORD*)(bits + y * lr.Pitch);
            for (int x = 0; x < w; ++x) {
                RGBQUAD* p = (RGBQUAD*)(src + x * 4);
                dst[x] = 0xff000000 | ((DWORD)p->rgbRed << 16) | ((DWORD)p->rgbGreen << 8) | p->rgbBlue;
            }
        }
    }

    surf->UnlockRect();
    FreeImage_Unload(fib32);
    return surf;
}

IDirect3DTexture9* ddUtil::loadTextureSurface(const std::string& file, int flags, gxGraphics* gfx) {
    return loadTextureSurface(file, flags, gfx, false, nullptr, nullptr);
}

IDirect3DTexture9* ddUtil::loadTextureSurface(const std::string& file, int flags, gxGraphics* gfx, bool renderTarget) {
    return loadTextureSurface(file, flags, gfx, renderTarget, nullptr, nullptr);
}

IDirect3DTexture9* ddUtil::loadTextureSurface(const std::string& file, int flags, gxGraphics* gfx, bool renderTarget, int* outW, int* outH) {
    g_lastImageError.clear();

    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(file.c_str(), 0);
    if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(file.c_str());
    if (fif == FIF_UNKNOWN) {
        g_lastImageError = "Unknown format: " + file;
        return nullptr;
    }

    FIBITMAP* fib = FreeImage_Load(fif, file.c_str(), 0);
    if (!fib) {
        g_lastImageError = "Load failed: " + file;
        return nullptr;
    }

    FIBITMAP* fib32;
    if (FreeImage_GetBPP(fib) == 32) {
        fib32 = fib;
    }
    else {
        fib32 = FreeImage_ConvertTo32Bits(fib);
        FreeImage_Unload(fib);
        if (!fib32) {
            g_lastImageError = "ConvertTo32Bits failed: " + file;
            return nullptr;
        }
    }

    int w = FreeImage_GetWidth(fib32);
    int h = FreeImage_GetHeight(fib32);
    int adjW = w, adjH = h;
    adjustTexSize(&adjW, &adjH, gfx->dir3dDev);
    if (outW) *outW = w;
    if (outH) *outH = h;

    bool hasMask = (flags & gxCanvas::CANVAS_TEX_MASK) != 0;
    bool hasAlpha = (flags & gxCanvas::CANVAS_TEX_ALPHA) != 0;
    bool hasMips = (flags & gxCanvas::CANVAS_TEX_MIPMAP) != 0;
    bool hasActualAlpha = hasRealAlpha(fib32);

    D3DFORMAT fmt = D3DFMT_A8R8G8B8;
    if (flags & gxCanvas::CANVAS_TEX_HICOLOR) fmt = D3DFMT_A4R4G4B4;

    IDirect3DDevice9* dev = gfx->dir3dDev;
    if (!dev) { FreeImage_Unload(fib32); return nullptr; }

    DWORD usage = 0;
    D3DPOOL pool = D3DPOOL_DEFAULT;
    UINT mipLevels = 1;

    if (renderTarget) {
        usage = D3DUSAGE_RENDERTARGET;
        if (hasMips) {
            mipLevels = 1;
            D3DCAPS9 caps;
            if (SUCCEEDED(dev->GetDeviceCaps(&caps)) && (caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)) {
                usage |= D3DUSAGE_AUTOGENMIPMAP;
                mipLevels = 0;
            }
        }
    }
    else {
        usage = D3DUSAGE_DYNAMIC;
        if (hasMips) mipLevels = 0;
    }

    IDirect3DTexture9* tex = nullptr;
    HRESULT hr = dev->CreateTexture(adjW, adjH, mipLevels, usage, fmt, pool, &tex, nullptr);
    if (FAILED(hr)) {
        FreeImage_Unload(fib32);
        return nullptr;
    }

    if (renderTarget) {
        IDirect3DSurface9* tempSurf = nullptr;
        hr = dev->CreateOffscreenPlainSurface(adjW, adjH, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tempSurf, nullptr);
        if (FAILED(hr)) {
            tex->Release();
            FreeImage_Unload(fib32);
            return nullptr;
        }

        D3DLOCKED_RECT lr;
        hr = tempSurf->LockRect(&lr, nullptr, 0);
        if (FAILED(hr)) {
            tempSurf->Release();
            tex->Release();
            FreeImage_Unload(fib32);
            return nullptr;
        }

        BYTE* bits = (BYTE*)lr.pBits;
        int pitch = lr.Pitch;

        if (hasMask) {
            buildMask(fib32, bits, pitch, w, h);
        }
        else if (hasAlpha) {
            if (hasActualAlpha) {
                for (int y = 0; y < h && y < adjH; ++y) {
                    BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
                    DWORD* dst = (DWORD*)(bits + y * pitch);
                    for (int x = 0; x < w && x < adjW; ++x) {
                        RGBQUAD* p = (RGBQUAD*)(src + x * 4);
                        dst[x] = ((DWORD)p->rgbReserved << 24) |
                            ((DWORD)p->rgbRed << 16) |
                            ((DWORD)p->rgbGreen << 8) |
                            p->rgbBlue;
                    }
                    // zero padding so it never bleeds into bilinear samples
                    if (w < adjW) memset(dst + w, 0, (adjW - w) * sizeof(DWORD));
                }
                for (int y = h; y < adjH; ++y) memset(bits + y * pitch, 0, adjW * sizeof(DWORD));
            }
            else {
                buildAlpha(fib32, bits, pitch, w, h, false);
                if (w < adjW) {
                    for (int y = 0; y < h; ++y) {
                        memset(bits + y * pitch + w * sizeof(DWORD), 0, (adjW - w) * sizeof(DWORD));
                    }
                }
                for (int y = h; y < adjH; ++y) memset(bits + y * pitch, 0, adjW * sizeof(DWORD));
            }
        }
        else {
            for (int y = 0; y < h && y < adjH; ++y) {
                BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
                DWORD* dst = (DWORD*)(bits + y * pitch);
                for (int x = 0; x < w && x < adjW; ++x) {
                    RGBQUAD* p = (RGBQUAD*)(src + x * 4);
                    dst[x] = 0xff000000 | ((DWORD)p->rgbRed << 16) | ((DWORD)p->rgbGreen << 8) | p->rgbBlue;
                }
                // zero padding so it never bleeds into bilinear samples
                if (w < adjW) memset(dst + w, 0, (adjW - w) * sizeof(DWORD));
            }
            for (int y = h; y < adjH; ++y) memset(bits + y * pitch, 0, adjW * sizeof(DWORD));
        }

        tempSurf->UnlockRect();

        IDirect3DSurface9* texSurf = nullptr;
        hr = tex->GetSurfaceLevel(0, &texSurf);
        if (SUCCEEDED(hr)) {
            RECT rect = { 0, 0, adjW, adjH };
            hr = dev->UpdateSurface(tempSurf, &rect, texSurf, nullptr);
            texSurf->Release();
        }
        tempSurf->Release();

        if (FAILED(hr)) {
            tex->Release();
            FreeImage_Unload(fib32);
            return nullptr;
        }

    }
    else {
        D3DLOCKED_RECT lr;
        hr = tex->LockRect(0, &lr, nullptr, 0);
        if (FAILED(hr)) {
            tex->Release();
            FreeImage_Unload(fib32);
            return nullptr;
        }

        BYTE* bits = (BYTE*)lr.pBits;
        int pitch = lr.Pitch;

        if (hasMask) {
            buildMask(fib32, bits, pitch, w, h);
        }
        else if (hasAlpha) {
            if (hasActualAlpha) {
                for (int y = 0; y < h && y < adjH; ++y) {
                    BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
                    DWORD* dst = (DWORD*)(bits + y * pitch);
                    for (int x = 0; x < w && x < adjW; ++x) {
                        RGBQUAD* p = (RGBQUAD*)(src + x * 4);
                        dst[x] = ((DWORD)p->rgbReserved << 24) |
                            ((DWORD)p->rgbRed << 16) |
                            ((DWORD)p->rgbGreen << 8) |
                            p->rgbBlue;
                    }
                    if (w < adjW) memset(dst + w, 0, (adjW - w) * sizeof(DWORD));
                }
                for (int y = h; y < adjH; ++y) memset(bits + y * pitch, 0, adjW * sizeof(DWORD));
            }
            else {
                buildAlpha(fib32, bits, pitch, w, h, false);
                if (w < adjW) {
                    for (int y = 0; y < h; ++y) {
                        memset(bits + y * pitch + w * sizeof(DWORD), 0, (adjW - w) * sizeof(DWORD));
                    }
                }
                for (int y = h; y < adjH; ++y) memset(bits + y * pitch, 0, adjW * sizeof(DWORD));
            }
        }
        else {
            for (int y = 0; y < h && y < adjH; ++y) {
                BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
                DWORD* dst = (DWORD*)(bits + y * pitch);
                for (int x = 0; x < w && x < adjW; ++x) {
                    RGBQUAD* p = (RGBQUAD*)(src + x * 4);
                    dst[x] = 0xff000000 | ((DWORD)p->rgbRed << 16) | ((DWORD)p->rgbGreen << 8) | p->rgbBlue;
                }
                if (w < adjW) memset(dst + w, 0, (adjW - w) * sizeof(DWORD));
            }
            for (int y = h; y < adjH; ++y) memset(bits + y * pitch, 0, adjW * sizeof(DWORD));
        }

        tex->UnlockRect(0);

        if (hasMips && !(usage & D3DUSAGE_AUTOGENMIPMAP)) {
            ddUtil::buildMipMaps(tex);
        }
    }

    FreeImage_Unload(fib32);
    return tex;
}