#include "std.h"
#include "gxcanvas.h"
#include "gxgraphics.h"
#include "gxruntime.h"
#include "asmcoder.h"
#include "gxutf8.h"

static int canvas_cnt;

extern gxRuntime* gx_runtime;

static unsigned FWMS[] = {
    0xffffffff,0x7fffffff,0x3fffffff,0x1fffffff,
    0x0fffffff,0x07ffffff,0x03ffffff,0x01ffffff,
    0x00ffffff,0x007fffff,0x003fffff,0x001fffff,
    0x000fffff,0x0007ffff,0x0003ffff,0x0001ffff,
    0x0000ffff,0x00007fff,0x00003fff,0x00001fff,
    0x00000fff,0x000007ff,0x000003ff,0x000001ff,
    0x000000ff,0x0000007f,0x0000003f,0x0000001f,
    0x0000000f,0x00000007,0x00000003,0x00000001 };
static unsigned LWMS[] = {
    0x80000000,0xc0000000,0xe0000000,0xf0000000,
    0xf8000000,0xfc000000,0xfe000000,0xff000000,
    0xff800000,0xffc00000,0xffe00000,0xfff00000,
    0xfff80000,0xfffc0000,0xfffe0000,0xffff0000,
    0xffff8000,0xffffc000,0xffffe000,0xfffff000,
    0xfffff800,0xfffffc00,0xfffffe00,0xffffff00,
    0xffffff80,0xffffffc0,0xffffffe0,0xfffffff0,
    0xfffffff8,0xfffffffc,0xfffffffe,0xffffffff };

struct Rect : public RECT {
    Rect() {}
    Rect(int x, int y, int w, int h) { left = x; top = y; right = x + w; bottom = y + h; }
};

static bool clip(const RECT& viewport, RECT* d) {
    if (d->right <= d->left || d->bottom <= d->top ||
        d->left >= viewport.right || d->right <= viewport.left ||
        d->top >= viewport.bottom || d->bottom <= viewport.top) return false;
    if (d->left < viewport.left)   d->left = viewport.left;
    if (d->right > viewport.right)  d->right = viewport.right;
    if (d->top < viewport.top)    d->top = viewport.top;
    if (d->bottom > viewport.bottom) d->bottom = viewport.bottom;
    return true;
}

static bool clip(const RECT& viewport, RECT* d, RECT* s) {
    if (d->right <= d->left || d->bottom <= d->top ||
        d->left >= viewport.right || d->right <= viewport.left ||
        d->top >= viewport.bottom || d->bottom <= viewport.top) return false;
    int dx, dy;
    if ((dx = viewport.left - d->left) > 0) { d->left += dx; s->left += dx; }
    if ((dx = viewport.right - d->right) < 0) { d->right += dx; s->right += dx; }
    if ((dy = viewport.top - d->top) > 0) { d->top += dy; s->top += dy; }
    if ((dy = viewport.bottom - d->bottom) < 0) { d->bottom += dy; s->bottom += dy; }
    return true;
}

struct FillModeGuard {
    IDirect3DDevice9* dev;
    DWORD oldMode;
    FillModeGuard(IDirect3DDevice9* d) : dev(d) {
        if (dev) {
            dev->GetRenderState(D3DRS_FILLMODE, &oldMode);
            dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        }
    }
    ~FillModeGuard() {
        if (dev) dev->SetRenderState(D3DRS_FILLMODE, oldMode);
    }
};

class FillRectGuard {
    IDirect3DDevice9* dev;
    IDirect3DSurface9* oldRT;
    IDirect3DSurface9* oldDS;
    bool active;

public:
    FillRectGuard(IDirect3DDevice9* d) : dev(d), oldRT(nullptr), oldDS(nullptr), active(false) {
        if (!dev) return;
        dev->GetRenderTarget(0, &oldRT);
        dev->GetDepthStencilSurface(&oldDS);
        active = true;
    }
    // this is retarded
    // but it works!
    ~FillRectGuard() {
        if (!active || !dev) return;
        dev->SetRenderTarget(0, oldRT);
        if (oldRT) oldRT->Release();
        if (oldDS) oldDS->Release();
        active = false;
    }

    FillRectGuard(const FillRectGuard&) = delete;
    FillRectGuard& operator=(const FillRectGuard&) = delete;
};

static inline void fillRectRows(unsigned char* base, int basePitch, int w, int h, unsigned nat, int pitch) {
    for (int y = 0; y < h; ++y) {
        unsigned char* row = base + y * basePitch;
        if (pitch == 4) {
            unsigned* p = (unsigned*)row;
            for (int x = 0; x < w; ++x) p[x] = nat;
        }
        else if (pitch == 2) {
            unsigned short val = (unsigned short)nat;
            unsigned short* p = (unsigned short*)row;
            for (int x = 0; x < w; ++x) p[x] = val;
        }
        else {
            unsigned char b0 = nat & 0xff;
            unsigned char b1 = (nat >> 8) & 0xff;
            unsigned char b2 = (nat >> 16) & 0xff;
            unsigned char* p = row;
            for (int x = 0; x < w; ++x) {
                p[0] = b0; p[1] = b1; p[2] = b2;
                p += 3;
            }
        }
    }
}

void gxCanvas::fillRect(const RECT& r, unsigned argb) {
    if (locked_cnt > 0) {
        int w = r.right - r.left;
        int h = r.bottom - r.top;
        if (w <= 0 || h <= 0) return;
        unsigned nat = format.fromARGB(argb);
        int pitch = format.getPitch();
        unsigned char* base = locked_surf + r.top * locked_pitch + r.left * pitch;
        fillRectRows(base, locked_pitch, w, h, nat, pitch);
        ++mod_cnt;
        return;
    }

    if (graphics && graphics->dir3dDev) {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(surf->GetDesc(&desc)) && (desc.Usage & D3DUSAGE_RENDERTARGET)) {
            FillRectGuard guard(graphics->dir3dDev);
            if (SUCCEEDED(graphics->dir3dDev->SetRenderTarget(0, surf))) {
                D3DRECT rect = { r.left, r.top, r.right, r.bottom };
                if (SUCCEEDED(graphics->dir3dDev->Clear(1, &rect, D3DCLEAR_TARGET, argb, 0.0f, 0))) {
                    return;
                }
            }
        }
    }

    D3DLOCKED_RECT lr;
    if (FAILED(surf->LockRect(&lr, &r, 0))) return;

    int w = r.right - r.left;
    int h = r.bottom - r.top;
    unsigned nat = format.fromARGB(argb);
    int pitch = format.getPitch();

    fillRectRows((unsigned char*)lr.pBits, lr.Pitch, w, h, nat, pitch);
    surf->UnlockRect();
}

struct QuadVertex {
    float x, y, z, rhw;
    float u, v;
};
static const DWORD QUAD_FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

gxCanvas::gxCanvas(gxGraphics* g, IDirect3DSurface9* s, int f) :
    graphics(g), plain_surf(s), tex(nullptr), cube_tex(nullptr), surf(s), z_surf(nullptr),
    flags(f), cube_mode(CUBEMODE_REFLECTION | CUBESPACE_WORLD),
    t_surf(nullptr), cm_mask(nullptr), locked_cnt(0), mod_cnt(0), remip_cnt(0),
    blit_tex(nullptr), blit_tex_mod_cnt(-1), blit_tex_mask(~0u), lock_is_rt(false), effect2D(nullptr), has_mask(false),
    blit_batch_depth(0), blit_batch_active(false), blit_batch_saved(nullptr) {
    memset(cube_surfs, 0, sizeof(cube_surfs));

    D3DSURFACE_DESC desc;
    surf->GetDesc(&desc);
    format.setFormat(desc.Format);

    clip_rect.left = clip_rect.top = 0;
    clip_rect.right = desc.Width;
    clip_rect.bottom = desc.Height;
    logical_w = desc.Width;
    logical_h = desc.Height;
    mipmapNeeded = (flags & CANVAS_TEX_MIPMAP) != 0;
    cm_pitch = (clip_rect.right + 31) / 32 + 1;
    setMask(0); setColor(~0); setClsColor(0);
    has_mask = false;
    setOrigin(0, 0); setHandle(0, 0);
    setFont(graphics->getDefaultFont());
    setViewport(0, 0, getWidth(), getHeight());
}

gxCanvas::gxCanvas(gxGraphics* g, IDirect3DTexture9* t, int f) :
    graphics(g), plain_surf(nullptr), tex(t), cube_tex(nullptr), surf(nullptr), z_surf(nullptr),
    flags(f), cube_mode(CUBEMODE_REFLECTION | CUBESPACE_WORLD),
    t_surf(nullptr), cm_mask(nullptr), locked_cnt(0), mod_cnt(0), remip_cnt(0),
    blit_tex(nullptr), blit_tex_mod_cnt(-1), blit_tex_mask(~0u), lock_is_rt(false), effect2D(nullptr), has_mask(false),
    blit_batch_depth(0), blit_batch_active(false), blit_batch_saved(nullptr) {
    memset(cube_surfs, 0, sizeof(cube_surfs));

    tex->GetSurfaceLevel(0, &surf);

    D3DSURFACE_DESC desc;
    surf->GetDesc(&desc);
    format.setFormat(desc.Format);

    clip_rect.left = clip_rect.top = 0;
    clip_rect.right = desc.Width;
    clip_rect.bottom = desc.Height;
    logical_w = desc.Width;
    logical_h = desc.Height;
    mipmapNeeded = (flags & CANVAS_TEX_MIPMAP) != 0;
    cm_pitch = (clip_rect.right + 31) / 32 + 1;
    setMask(0); setColor(~0); setClsColor(0);
    has_mask = false;
    setOrigin(0, 0); setHandle(0, 0);
    setFont(graphics->getDefaultFont());
    setViewport(0, 0, getWidth(), getHeight());

    if (flags & gxCanvas::CANVAS_TEX_MIPMAP) ddUtil::buildMipMaps(tex);
}

gxCanvas::gxCanvas(gxGraphics* g, IDirect3DCubeTexture9* ct, int f) :
    graphics(g), plain_surf(nullptr), tex(nullptr), cube_tex(ct), surf(nullptr), z_surf(nullptr),
    flags(f), cube_mode(CUBEMODE_REFLECTION | CUBESPACE_WORLD),
    t_surf(nullptr), cm_mask(nullptr), locked_cnt(0), mod_cnt(0), remip_cnt(0),
    blit_tex(nullptr), blit_tex_mod_cnt(-1), blit_tex_mask(~0u), lock_is_rt(false), effect2D(nullptr), has_mask(false),
    blit_batch_depth(0), blit_batch_active(false), blit_batch_saved(nullptr) {

    D3DCUBEMAP_FACES faceMap[6] = {
        D3DCUBEMAP_FACE_NEGATIVE_X,
        D3DCUBEMAP_FACE_POSITIVE_Z,
        D3DCUBEMAP_FACE_POSITIVE_X,
        D3DCUBEMAP_FACE_NEGATIVE_Z,
        D3DCUBEMAP_FACE_POSITIVE_Y,
        D3DCUBEMAP_FACE_NEGATIVE_Y
    };
    for (int k = 0; k < 6; ++k)
        cube_tex->GetCubeMapSurface(faceMap[k], 0, &cube_surfs[k]);
    surf = cube_surfs[2];

    D3DSURFACE_DESC desc;
    surf->GetDesc(&desc);
    format.setFormat(desc.Format);

    clip_rect.left = clip_rect.top = 0;
    clip_rect.right = desc.Width;
    clip_rect.bottom = desc.Height;
    logical_w = desc.Width;
    logical_h = desc.Height;
    mipmapNeeded = (flags & CANVAS_TEX_MIPMAP) != 0;
    cm_pitch = (clip_rect.right + 31) / 32 + 1;
    setMask(0); setColor(~0); setClsColor(0);
    has_mask = false;
    setOrigin(0, 0); setHandle(0, 0);
    setFont(graphics->getDefaultFont());
    setViewport(0, 0, getWidth(), getHeight());
}

gxCanvas::~gxCanvas() {
    delete[] cm_mask;
    if (locked_cnt) surf->UnlockRect();
    if (t_surf) t_surf->Release();
    if (blit_tex) { blit_tex->Release(); blit_tex = nullptr; }
    releaseZBuffer();

    for (int k = 0; k < 6; ++k) {
        if (cube_surfs[k]) { cube_surfs[k]->Release(); cube_surfs[k] = nullptr; }
    }

    if (tex && surf) { surf->Release(); surf = nullptr; }

    if (tex) { tex->Release();       tex = nullptr; }
    if (cube_tex) { cube_tex->Release();  cube_tex = nullptr; }
    if (plain_surf && !tex && !cube_tex) { plain_surf->Release(); plain_surf = nullptr; }
}

void gxCanvas::backup() {
    if (flags & CANVAS_TEX_CUBE) return;
    if (!surf) return;

    D3DSURFACE_DESC desc;
    if (FAILED(surf->GetDesc(&desc))) return;

    IDirect3DDevice9* dev = graphics->dir3dDev;
    if (!dev) return;

    if (t_surf) { t_surf->Release(); t_surf = nullptr; }
    if (FAILED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &t_surf, nullptr))) {
        return;
    }

    bool isRT = (desc.Usage & D3DUSAGE_RENDERTARGET) != 0;
    if (isRT) {
        dev->GetRenderTargetData(surf, t_surf);
    }
    else {
        D3DLOCKED_RECT srcLR, dstLR;
        if (SUCCEEDED(surf->LockRect(&srcLR, nullptr, D3DLOCK_READONLY))) {
            if (SUCCEEDED(t_surf->LockRect(&dstLR, nullptr, 0))) {
                for (UINT y = 0; y < desc.Height; ++y) {
                    memcpy((BYTE*)dstLR.pBits + y * dstLR.Pitch,
                        (BYTE*)srcLR.pBits + y * srcLR.Pitch,
                        min((UINT)dstLR.Pitch, (UINT)srcLR.Pitch));
                }
                t_surf->UnlockRect();
            }
            surf->UnlockRect();
        }
    }
}

void gxCanvas::restore() {
    if (!t_surf) return;

    D3DSURFACE_DESC tdesc;
    t_surf->GetDesc(&tdesc);

    IDirect3DDevice9* dev = graphics->dir3dDev;
    if (!dev) return;

    if (tex) {
        if (surf) surf->Release();
        tex->Release();
    }
    else if (plain_surf) {
        plain_surf->Release();
    }
    tex = nullptr;
    surf = nullptr;
    plain_surf = nullptr;

    if (blit_tex) { blit_tex->Release(); blit_tex = nullptr; }
    blit_tex_mod_cnt = -1;

    IDirect3DTexture9* newTex = nullptr;
    if (FAILED(dev->CreateTexture(tdesc.Width, tdesc.Height, 1, D3DUSAGE_RENDERTARGET, tdesc.Format, D3DPOOL_DEFAULT, &newTex, nullptr))) return;

    IDirect3DSurface9* newSurf = nullptr;
    newTex->GetSurfaceLevel(0, &newSurf);

    D3DLOCKED_RECT srcLR, dstLR;
    if (SUCCEEDED(t_surf->LockRect(&srcLR, nullptr, D3DLOCK_READONLY))) {
        if (SUCCEEDED(newSurf->LockRect(&dstLR, nullptr, 0))) {
            for (UINT y = 0; y < tdesc.Height; ++y) {
                memcpy((BYTE*)dstLR.pBits + y * dstLR.Pitch,
                    (BYTE*)srcLR.pBits + y * srcLR.Pitch,
                    min((UINT)dstLR.Pitch, (UINT)srcLR.Pitch));
            }
            newSurf->UnlockRect();
        }
        t_surf->UnlockRect();
    }

    tex = newTex;
    surf = newSurf;
}

IDirect3DSurface9* gxCanvas::getSurface() const {
    return surf;
}

IDirect3DBaseTexture9* gxCanvas::getTexture() const {
    if (cube_tex) return cube_tex;
    if (tex)      return tex;
    return nullptr;
}

IDirect3DBaseTexture9* gxCanvas::getTexSurface() const {
    if (mod_cnt != remip_cnt && tex && (flags & CANVAS_TEX_MIPMAP) && mipmapNeeded) {
        ddUtil::buildMipMaps(tex);
    }
    remip_cnt = mod_cnt;
    return getTexture();
}

bool gxCanvas::clip(RECT* d) const {
    return ::clip(viewport, d);
}
bool gxCanvas::clip(RECT* d, RECT* s) const {
    return ::clip(viewport, d, s);
}

void gxCanvas::set2DEffect(gxEffect* effect) {
    effect2D = effect;
}

gxEffect* gxCanvas::get2DEffect() const {
    return effect2D;
}

void gxCanvas::updateBitMask(const RECT& r) const {
    int w = r.right - r.left; if (w <= 0) return;
    int h = r.bottom - r.top; if (h <= 0) return;

    lock();
    RECT t = r;
    t.left &= ~31;
    t.right = (t.right + 31) & ~31;
    w = (t.right - t.left) / 32;
    unsigned char* src_row = locked_surf + t.top * locked_pitch + t.left * format.getPitch();
    unsigned* dest_row = cm_mask + t.top * cm_pitch + t.left / 32;
    unsigned mask_argb = format.toARGB(mask_surf) & 0xffffff;

    while (h--) {
        unsigned* dest = dest_row;
        unsigned char* src = src_row;
        for (int c = 0; c < w; ++c) {
            unsigned mask = 0;
            for (int x = 0; x < 32; ++x) {
                unsigned pix = format.getPixel(src) & 0xffffff;
                mask = (mask << 1) | (pix != mask_argb);
                src += format.getPitch();
            }
            *dest++ = mask;
        }
        dest_row += cm_pitch;
        src_row += locked_pitch;
    }
    unlock();
}

void gxCanvas::setModify(int n) { mod_cnt = n; }
int  gxCanvas::getModify() const { return mod_cnt; }

bool gxCanvas::attachZBuffer() {
    if (z_surf) return true;
    D3DSURFACE_DESC desc;
    surf->GetDesc(&desc);
    IDirect3DDevice9* dev = graphics->dir3dDev;
    HRESULT hr = dev->CreateDepthStencilSurface(desc.Width, desc.Height, graphics->zbuffFmt, D3DMULTISAMPLE_NONE, 0, TRUE, &z_surf, nullptr);
    if (FAILED(hr) || !z_surf) {
        char buf[256];
        sprintf(buf, "CreateDepthStencilSurface failed: 0x%08X", hr);
        // MessageBoxA(NULL, buf, "Error", MB_OK);
        return false;
    }
    return true;
}

void gxCanvas::releaseZBuffer() {
    if (!z_surf) return;
    z_surf->Release();
    z_surf = nullptr;
}

void gxCanvas::damage(const RECT& r) const {
    ++mod_cnt;
    if (cm_mask) updateBitMask(r);
}

void gxCanvas::setFont(gxFont* f) { font = f; }

void gxCanvas::setMask(unsigned argb) { mask_surf = format.fromARGB(argb); has_mask = true; }

void gxCanvas::setColor(unsigned argb) { argb |= 0xff000000; color_argb = argb; color_surf = format.fromARGB(argb); }

void gxCanvas::setClsColor(unsigned argb) { argb |= 0xff000000; clsColor_surf = format.fromARGB(argb); }

void gxCanvas::setOrigin(int x, int y) { origin_x = x; origin_y = y; }

void gxCanvas::setHandle(int x, int y) { handle_x = x; handle_y = y; }

void gxCanvas::setViewport(int x, int y, int w, int h) {
    Rect r(x, y, w, h);
    if (!::clip(clip_rect, &r)) r = Rect(0, 0, 0, 0);
    viewport = r;
}

void gxCanvas::cls() {
    fillRect(viewport, format.toARGB(clsColor_surf));
    damage(viewport);
}

void gxCanvas::plot(int x, int y) {
    x += origin_x; if (x < viewport.left || x >= viewport.right)  return;
    y += origin_y; if (y < viewport.top || y >= viewport.bottom) return;
    Rect dest(x, y, 1, 1);
    fillRect(dest, format.toARGB(color_surf));
    damage(dest);
}

void gxCanvas::line(int x0, int y0, int x1, int y1) {
    int ddf, padj, sadj;
    int dx, dy, sx, sy, ax, ay;
    x0 += origin_x; y0 += origin_y;
    x1 += origin_x; y1 += origin_y;

    int cx0 = viewport.left, cx1 = viewport.right - 1;
    int cy0 = viewport.top, cy1 = viewport.bottom - 1;

    while (true) {
        int clip0 = 0, clip1 = 0;
        if (y0 > cy1) clip0 |= 1; else if (y0 < cy0) clip0 |= 2;
        if (x0 > cx1) clip0 |= 4; else if (x0 < cx0) clip0 |= 8;
        if (y1 > cy1) clip1 |= 1; else if (y1 < cy0) clip1 |= 2;
        if (x1 > cx1) clip1 |= 4; else if (x1 < cx0) clip1 |= 8;
        if ((clip0 | clip1) == 0) break;
        if ((clip0 & clip1) != 0) return;
        if ((clip0 & 1) == 1) { x0 = x0 + ((x1 - x0) * (cy1 - y0)) / (y1 - y0); y0 = cy1; continue; }
        if ((clip0 & 2) == 2) { x0 = x0 + ((x1 - x0) * (cy0 - y0)) / (y1 - y0); y0 = cy0; continue; }
        if ((clip0 & 4) == 4) { y0 = y0 + ((y1 - y0) * (cx1 - x0)) / (x1 - x0); x0 = cx1; continue; }
        if ((clip0 & 8) == 8) { y0 = y0 + ((y1 - y0) * (cx0 - x0)) / (x1 - x0); x0 = cx0; continue; }
        if ((clip1 & 1) == 1) { x1 = x0 + ((x1 - x0) * (cy1 - y0)) / (y1 - y0); y1 = cy1; continue; }
        if ((clip1 & 2) == 2) { x1 = x0 + ((x1 - x0) * (cy0 - y0)) / (y1 - y0); y1 = cy0; continue; }
        if ((clip1 & 4) == 4) { y1 = y0 + ((y1 - y0) * (cx1 - x0)) / (x1 - x0); x1 = cx1; continue; }
        if ((clip1 & 8) == 8) { y1 = y0 + ((y1 - y0) * (cx0 - x0)) / (x1 - x0); x1 = cx0; continue; }
    }
    dx = x1 - x0; dy = y1 - y0;
    if ((dx | dy) == 0) { setPixel(x0, y0, color_argb); return; }
    if (dx >= 0) { sx = 1; ax = dx; }
    else { sx = -1; ax = -dx; }
    if (dy >= 0) { sy = 1; ay = dy; }
    else { sy = -1; ay = -dy; }
    lock();
    if (ax > ay) {
        ddf = -ax; sadj = ax + ax; padj = ay + ay;
        while (ax-- >= 0) { setPixelFast(x0, y0, color_argb); x0 += sx; ddf += padj; if (ddf >= 0) { y0 += sy; ddf -= sadj; } }
    }
    else {
        ddf = -ay; sadj = ay + ay; padj = ax + ax;
        while (ay-- >= 0) { setPixelFast(x0, y0, color_argb); y0 += sy; ddf += padj; if (ddf >= 0) { x0 += sx; ddf -= sadj; } }
    }
    unlock();
}

void gxCanvas::rect(int x, int y, int w, int h, bool solid) {
    x += origin_x; y += origin_y;
    Rect dest(x, y, w, h);
    if (!clip(&dest)) return;
    unsigned argb = format.toARGB(color_surf);
    if (solid) {
        fillRect(dest, argb);
        damage(dest);
        return;
    }
    Rect r1(x, y, w, 1);           if (clip(&r1)) fillRect(r1, argb);
    Rect r2(x, y, 1, h);           if (clip(&r2)) fillRect(r2, argb);
    Rect r3(x + w - 1, y, 1, h);   if (clip(&r3)) fillRect(r3, argb);
    Rect r4(x, y + h - 1, w, 1);   if (clip(&r4)) fillRect(r4, argb);
    damage(dest);
}

void gxCanvas::oval(int x1, int y1, int w, int h, bool solid) {
    x1 += origin_x; y1 += origin_y;
    Rect dest(x1, y1, w, h);
    if (!clip(&dest)) return;
    float xr = w * .5f, yr = h * .5f, ar = (float)w / (float)h;
    float cx = x1 + xr + .5f, cy = y1 + yr - .5f, rsq = yr * yr, y;
    unsigned argb = format.toARGB(color_surf);
    if (solid) {
        y = dest.top - cy;
        for (int t = dest.top; t < dest.bottom; ++y, ++t) {
            float x = sqrtf(rsq - y * y) * ar;
            int xa = (int)floor(cx - x), xb = (int)floor(cx + x);
            if (xb <= xa || xa >= viewport.right || xb <= viewport.left) continue;
            Rect dr; dr.top = t; dr.bottom = t + 1;
            dr.left = xa < viewport.left ? viewport.left : xa;
            dr.right = xb > viewport.right ? viewport.right : xb;
            fillRect(dr, argb);
        }
        damage(dest);
        return;
    }
    int p_xa, p_xb, t, hh = (int)floor(cy);
    p_xa = p_xb = (int)cx;
    t = dest.top; y = t - cy;
    if (dest.top > y1) { --t; --y; }
    for (; t <= hh; ++y, ++t) {
        float x = sqrtf(rsq - y * y) * ar;
        int xa = (int)floor(cx - x), xb = (int)floor(cx + x);
        Rect r1(xa, t, p_xa - xa, 1); if (r1.right <= r1.left)r1.right = r1.left + 1; if (clip(&r1)) fillRect(r1, argb);
        Rect r2(p_xb, t, xb - p_xb, 1); if (r2.left >= r2.right)r2.left = r2.right - 1; if (clip(&r2)) fillRect(r2, argb);
        p_xa = xa; p_xb = xb;
    }
    p_xa = p_xb = (int)cx;
    t = dest.bottom - 1; y = t - cy;
    if (dest.bottom < y1 + h) { ++t; ++y; }
    for (; t > hh; --y, --t) {
        float x = sqrtf(rsq - y * y) * ar;
        int xa = (int)floor(cx - x), xb = (int)floor(cx + x);
        Rect r1(xa, t, p_xa - xa, 1); if (r1.right <= r1.left)r1.right = r1.left + 1; if (clip(&r1)) fillRect(r1, argb);
        Rect r2(p_xb, t, xb - p_xb, 1); if (r2.left >= r2.right)r2.left = r2.right - 1; if (clip(&r2)) fillRect(r2, argb);
        p_xa = xa; p_xb = xb;
    }
    damage(dest);
}

static void drawQuadWithEffect(IDirect3DDevice9* dev, gxEffect* effect, IDirect3DTexture9* tex, const RECT& dst, const RECT& src, int texW, int texH, bool useAlpha = false, unsigned color_argb = 0xFFFFFFFF) {
    D3DXMATRIX proj, view, world;
    D3DXMatrixIdentity(&world);
    D3DXMatrixIdentity(&view);
    D3DVIEWPORT9 vp;
    dev->GetViewport(&vp);
    float w = (float)vp.Width;
    float h = (float)vp.Height;
    D3DXMatrixOrthoOffCenterLH(&proj, 0.0f, w, h, 0.0f, 0.0f, 1.0f);

    effect->setAutoMatrices(world, view, proj);
    if (tex) {
        effect->setTexture("tex0", tex);
    }
    if (useAlpha) {
        float color[4] = {
            ((color_argb >> 16) & 0xFF) / 255.0f,
            ((color_argb >> 8) & 0xFF) / 255.0f,
            (color_argb & 0xFF) / 255.0f,
            ((color_argb >> 24) & 0xFF) / 255.0f
        };
        effect->setVector("color", color);
    }

    UINT passes;
    if (effect->begin(&passes)) {
        float x0 = (float)dst.left - 0.5f;
        float y0 = (float)dst.top - 0.5f;
        float x1 = (float)dst.right - 0.5f;
        float y1 = (float)dst.bottom - 0.5f;
        float u0 = (float)src.left / texW;
        float v0 = (float)src.top / texH;
        float u1 = (float)src.right / texW;
        float v1 = (float)src.bottom / texH;
        struct QuadVertex { float x, y, z, rhw; float u, v; };
        QuadVertex verts[4] = {
            { x0, y0, 0.0f, 1.0f, u0, v0 },
            { x1, y0, 0.0f, 1.0f, u1, v0 },
            { x0, y1, 0.0f, 1.0f, u0, v1 },
            { x1, y1, 0.0f, 1.0f, u1, v1 }
        };
        static const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

        for (UINT p = 0; p < passes; ++p) {
            effect->beginPass(p);
            dev->SetFVF(FVF);
            dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(QuadVertex));
            effect->endPass();
        }
        effect->end();
    }
}

static IDirect3DTexture9* getOrBuildBlitTex(IDirect3DDevice9* dev, gxCanvas* src, unsigned maskRGB) {
    if (src->blit_tex && src->blit_tex_mod_cnt == src->mod_cnt && src->blit_tex_mask == maskRGB)
        return src->blit_tex;

    if (src->blit_tex) { src->blit_tex->Release(); src->blit_tex = nullptr; }

    int texW = src->clip_rect.right;
    int texH = src->clip_rect.bottom;
    int logW = src->logical_w;
    int logH = src->logical_h;

    IDirect3DTexture9* newTex = nullptr;
    if (FAILED(dev->CreateTexture(texW, texH, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &newTex, nullptr)))
        return nullptr;

    IDirect3DSurface9* texSurf = nullptr;
    if (FAILED(newTex->GetSurfaceLevel(0, &texSurf))) { newTex->Release(); return nullptr; }

    D3DLOCKED_RECT srcLR, dstLR;
    RECT srcRect = { 0, 0, texW, texH };
    if (FAILED(src->surf->LockRect(&srcLR, &srcRect, D3DLOCK_READONLY))) {
        texSurf->Release(); newTex->Release(); return nullptr;
    }
    if (FAILED(texSurf->LockRect(&dstLR, nullptr, 0))) {
        src->surf->UnlockRect(); texSurf->Release(); newTex->Release(); return nullptr;
    }

    bool doMask = (maskRGB != ~0u);
    const PixelFormat& fmt = src->format;
    int pitch = fmt.getPitch();

    for (int y = 0; y < texH; ++y) {
        const unsigned char* srcRow = (const unsigned char*)srcLR.pBits + y * srcLR.Pitch;
        unsigned* dstRow = (unsigned*)((unsigned char*)dstLR.pBits + y * dstLR.Pitch);
        if (y < logH) {
            for (int x = 0; x < logW; ++x) {
                unsigned argb = fmt.toARGB(fmt.getPixel((void*)(srcRow + x * pitch)));
                if (doMask && (argb & 0x00ffffffu) == maskRGB)
                    argb = 0x00000000u;   // fully transparent
                else
                    if (!(src->getFlags() & gxCanvas::CANVAS_TEX_ALPHA)) {
                        argb |= 0xff000000u;  // fully opaque
                    }
                dstRow[x] = argb;
            }
        }

        // zero out padding it never bleeds into samples!!
        if (logW < texW) memset(dstRow + logW, 0, (texW - logW) * sizeof(unsigned));
        if (y >= logH) memset(dstRow, 0, texW * sizeof(unsigned));
    }

    texSurf->UnlockRect();
    src->surf->UnlockRect();
    texSurf->Release();

    src->blit_tex = newTex;
    src->blit_tex_mod_cnt = src->mod_cnt;
    src->blit_tex_mask = maskRGB;
    return newTex;
}

static void drawBlitQuad(IDirect3DDevice9* dev,
    IDirect3DTexture9* tex,
    const RECT& dst,
    const RECT& srcRect,
    int texW, int texH)
{
    float x0 = (float)dst.left - 0.5f;
    float y0 = (float)dst.top - 0.5f;
    float x1 = (float)dst.right - 0.5f;
    float y1 = (float)dst.bottom - 0.5f;

    float u0 = (float)srcRect.left / texW;
    float v0 = (float)srcRect.top / texH;
    float u1 = (float)srcRect.right / texW;
    float v1 = (float)srcRect.bottom / texH;

    QuadVertex verts[4] = {
        { x0, y0, 0.0f, 1.0f, u0, v0 },
        { x1, y0, 0.0f, 1.0f, u1, v0 },
        { x0, y1, 0.0f, 1.0f, u0, v1 },
        { x1, y1, 0.0f, 1.0f, u1, v1 },
    };

    dev->SetTexture(0, tex);
    dev->SetFVF(QUAD_FVF);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(QuadVertex));
}

static void setupBlitRenderState(IDirect3DDevice9* dev, bool solid) {
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    if (!solid) {
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
        dev->SetRenderState(D3DRS_ALPHAREF, 0);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }
    else {
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    }
}

struct SavedBlitState {
    IDirect3DSurface9* oldRT;
    IDirect3DSurface9* oldDS;
    IDirect3DBaseTexture9* oldTex;
    D3DVIEWPORT9 oldVP;
    DWORD oldZ, oldAlphaTest, oldAlphaFunc, oldAlphaRef, oldAlphaBlend;
    DWORD oldSrcBlend, oldDestBlend;
    DWORD oldLighting, oldTextureFactor;
    DWORD oldCOp, oldCArg1, oldCArg2, oldAOp, oldAArg1, oldMag, oldMin;
};

static void saveBlitState(IDirect3DDevice9* dev, SavedBlitState& s) {
    dev->GetRenderTarget(0, &s.oldRT);
    dev->GetDepthStencilSurface(&s.oldDS);
    dev->GetTexture(0, &s.oldTex);
    dev->GetViewport(&s.oldVP);
    dev->GetRenderState(D3DRS_ZENABLE, &s.oldZ);
    dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &s.oldAlphaBlend);
    dev->GetRenderState(D3DRS_SRCBLEND, &s.oldSrcBlend);
    dev->GetRenderState(D3DRS_DESTBLEND, &s.oldDestBlend);
    dev->GetRenderState(D3DRS_ALPHATESTENABLE, &s.oldAlphaTest);
    dev->GetRenderState(D3DRS_ALPHAFUNC, &s.oldAlphaFunc);
    dev->GetRenderState(D3DRS_ALPHAREF, &s.oldAlphaRef);
    dev->GetRenderState(D3DRS_LIGHTING, &s.oldLighting);
    dev->GetRenderState(D3DRS_TEXTUREFACTOR, &s.oldTextureFactor);
    dev->GetTextureStageState(0, D3DTSS_COLOROP, &s.oldCOp);
    dev->GetTextureStageState(0, D3DTSS_COLORARG1, &s.oldCArg1);
    dev->GetTextureStageState(0, D3DTSS_COLORARG2, &s.oldCArg2);
    dev->GetTextureStageState(0, D3DTSS_ALPHAOP, &s.oldAOp);
    dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &s.oldAArg1);
    dev->GetSamplerState(0, D3DSAMP_MAGFILTER, &s.oldMag);
    dev->GetSamplerState(0, D3DSAMP_MINFILTER, &s.oldMin);
}

static void restoreBlitState(IDirect3DDevice9* dev, SavedBlitState& s) {
    dev->SetRenderTarget(0, s.oldRT);
    dev->SetDepthStencilSurface(s.oldDS);
    if (s.oldRT) s.oldRT->Release();
    if (s.oldDS) s.oldDS->Release();
    dev->SetViewport(&s.oldVP);
    dev->SetRenderState(D3DRS_ZENABLE, s.oldZ);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, s.oldAlphaBlend);
    dev->SetRenderState(D3DRS_SRCBLEND, s.oldSrcBlend);
    dev->SetRenderState(D3DRS_DESTBLEND, s.oldDestBlend);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, s.oldAlphaTest);
    dev->SetRenderState(D3DRS_ALPHAFUNC, s.oldAlphaFunc);
    dev->SetRenderState(D3DRS_ALPHAREF, s.oldAlphaRef);
    dev->SetRenderState(D3DRS_LIGHTING, s.oldLighting);
    dev->SetRenderState(D3DRS_TEXTUREFACTOR, s.oldTextureFactor);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, s.oldCOp);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, s.oldCArg1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, s.oldCArg2);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, s.oldAOp);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, s.oldAArg1);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, s.oldMag);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, s.oldMin);
    dev->SetTexture(0, s.oldTex);
    if (s.oldTex) s.oldTex->Release();
}

static bool isRenderTarget(IDirect3DSurface9* s) {
    D3DSURFACE_DESC desc;
    return SUCCEEDED(s->GetDesc(&desc)) && (desc.Usage & D3DUSAGE_RENDERTARGET);
}

void gxCanvas::beginBlitBatch() const {
    if (blit_batch_depth++ > 0) return;

    IDirect3DDevice9* dev = graphics ? graphics->dir3dDev : nullptr;
    if (!dev || !isRenderTarget(surf)) {
        blit_batch_active = false;
        return;
    }

    SavedBlitState* saved = new SavedBlitState();
    saveBlitState(dev, *saved);
    blit_batch_saved = saved;

    dev->SetRenderTarget(0, surf);
    dev->SetDepthStencilSurface(nullptr);
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)clip_rect.right, (DWORD)clip_rect.bottom, 0.0f, 1.0f };
    dev->SetViewport(&vp);

    dev->BeginScene();
    blit_batch_active = true;
}

void gxCanvas::endBlitBatch() const {
    if (blit_batch_depth == 0) return;
    if (--blit_batch_depth > 0) return;

    if (blit_batch_active) {
        IDirect3DDevice9* dev = graphics->dir3dDev;
        dev->EndScene();
        SavedBlitState* saved = (SavedBlitState*)blit_batch_saved;
        restoreBlitState(dev, *saved);
        delete saved;
        blit_batch_saved = nullptr;
        blit_batch_active = false;
    }
}

static void cpuBlit(gxCanvas* dest, const RECT& dest_r, gxCanvas* src, const RECT& src_r, bool solid) {
    D3DSURFACE_DESC destDesc;
    bool destIsSysMem = SUCCEEDED(dest->surf->GetDesc(&destDesc)) && destDesc.Pool == D3DPOOL_SYSTEMMEM;

    if (!destIsSysMem) {
        IDirect3DDevice9* dev = dest->graphics->dir3dDev;
        if (dev) {
            RECT srcRect = { src_r.left, src_r.top, src_r.right, src_r.bottom };
            RECT destRect = { dest_r.left, dest_r.top, dest_r.right, dest_r.bottom };
            HRESULT hr = dev->StretchRect(src->getSurface(), &srcRect,
                dest->getSurface(), &destRect,
                D3DTEXF_LINEAR);
            if (SUCCEEDED(hr)) {
                dest->damage(dest_r);
                return;
            }
        }
    }

    int dw = dest_r.right - dest_r.left;
    int dh = dest_r.bottom - dest_r.top;
    int sw = src_r.right - src_r.left;
    int sh = src_r.bottom - src_r.top;
    bool stretch = (dw != sw || dh != sh);
    bool srcPreLocked = src->isLocked();
    bool destPreLocked = dest->isLocked();

    D3DLOCKED_RECT srcLR, dstLR;
    unsigned char* srcBits;  int srcPitchBytes;
    unsigned char* dstBits;  int dstPitchBytes;

    if (srcPreLocked) {
        srcBits = src->getLockedSurf();
        srcPitchBytes = src->getLockedPitch();
    }
    else {
        if (FAILED(src->surf->LockRect(&srcLR, nullptr, D3DLOCK_READONLY))) return;
        srcBits = (unsigned char*)srcLR.pBits;
        srcPitchBytes = srcLR.Pitch;
    }

    if (destPreLocked) {
        dstBits = dest->getLockedSurf();
        dstPitchBytes = dest->getLockedPitch();
    }
    else {
        if (FAILED(dest->surf->LockRect(&dstLR, nullptr, 0))) {
            if (!srcPreLocked) src->surf->UnlockRect();
            return;
        }
        dstBits = (unsigned char*)dstLR.pBits;
        dstPitchBytes = dstLR.Pitch;
    }

    const PixelFormat& sf = src->format;
    const PixelFormat& df = dest->format;
    int sp = sf.getPitch(), dp = df.getPitch();

    unsigned maskRGB = solid ? ~0u : (sf.toARGB(src->mask_surf) & 0x00ffffffu);
    bool doMask = (maskRGB != ~0u);
    bool doAlpha = (src->getFlags() & gxCanvas::CANVAS_TEX_ALPHA) != 0;

    if (!stretch && !doMask && !doAlpha && sp == dp && sf.getDepth() == df.getDepth()) {
        int rowBytes = dw * sp;
        for (int y = 0; y < dh; ++y) {
            const unsigned char* srow = srcBits + (src_r.top + y) * srcPitchBytes + src_r.left * sp;
            unsigned char* drow = dstBits + (dest_r.top + y) * dstPitchBytes + dest_r.left * dp;
            memcpy(drow, srow, rowBytes);
        }
    }
    else {
        for (int y = 0; y < dh; ++y) {
            int sy = stretch ? (y * sh / dh) : y;
            const unsigned char* srow = srcBits + (src_r.top + sy) * srcPitchBytes + src_r.left * sp;
            unsigned char* drow = dstBits + (dest_r.top + y) * dstPitchBytes + dest_r.left * dp;
            for (int x = 0; x < dw; ++x) {
                int sx = stretch ? (x * sw / dw) : x;
                unsigned argb = sf.toARGB(sf.getPixel((void*)(srow + sx * sp)));
                if (doMask && (argb & 0x00ffffffu) == maskRGB) continue;
                if (!doAlpha) argb |= 0xff000000u;
                df.setPixel(drow + x * dp, df.fromARGB(argb));
            }
        }
    }

    if (!destPreLocked) dest->surf->UnlockRect();
    if (!srcPreLocked) src->surf->UnlockRect();
    dest->damage(dest_r);
}

void gxCanvas::blit(int x, int y, gxCanvas* src, int src_x, int src_y,
    int src_w, int src_h, bool solid)
{
    x += origin_x - src->handle_x;
    y += origin_y - src->handle_y;

    Rect dest_r(x, y, src_w, src_h);
    Rect src_r(src_x, src_y, src_w, src_h);

    if (!clip(&dest_r, &src_r)) return;
    if (!::clip(src->clip_rect, &src_r, &dest_r)) return;

    if (solid) {
        D3DSURFACE_DESC srcDesc, dstDesc;
        if (SUCCEEDED(src->surf->GetDesc(&srcDesc)) && SUCCEEDED(surf->GetDesc(&dstDesc))) {
            if ((srcDesc.Usage & D3DUSAGE_RENDERTARGET) && (dstDesc.Usage & D3DUSAGE_RENDERTARGET)) {
                ddUtil::copy(graphics->dir3dDev, surf, dest_r.left, dest_r.top, dest_r.right - dest_r.left, dest_r.bottom - dest_r.top,
                    src->surf, src_r.left, src_r.top, src_r.right - src_r.left, src_r.bottom - src_r.top);
                damage(dest_r);
                return;
            }
        }
    }

    if (!isRenderTarget(surf)) {
        cpuBlit(this, dest_r, src, src_r, solid);
        damage(dest_r);
        return;
    }

    IDirect3DDevice9* dev = graphics->dir3dDev;
    if (!dev) return;

    if (effect2D) {
        FillModeGuard guard(dev);
        unsigned maskRGB = solid ? ~0u : (src->format.toARGB(src->mask_surf) & 0x00ffffffu);
        IDirect3DTexture9* blitTex = getOrBuildBlitTex(dev, src, maskRGB);
        if (!blitTex) return;

        SavedBlitState saved;
        saveBlitState(dev, saved);

        dev->SetRenderTarget(0, surf);
        dev->SetDepthStencilSurface(nullptr);
        D3DVIEWPORT9 vp = { 0, 0, (DWORD)clip_rect.right, (DWORD)clip_rect.bottom, 0.0f, 1.0f };
        dev->SetViewport(&vp);

        dev->BeginScene();
        drawQuadWithEffect(dev, effect2D, blitTex, dest_r, src_r, src->clip_rect.right, src->clip_rect.bottom, false, 0);
        dev->EndScene();

        restoreBlitState(dev, saved);
        damage(dest_r);
        return;
    }

    FillModeGuard guard(dev);

    unsigned maskRGB = solid ? ~0u : (src->format.toARGB(src->mask_surf) & 0x00ffffffu);
    IDirect3DTexture9* blitTex = getOrBuildBlitTex(dev, src, maskRGB);
    if (!blitTex) return;

    SavedBlitState saved;
    saveBlitState(dev, saved);

    dev->SetRenderTarget(0, surf);
    dev->SetDepthStencilSurface(nullptr);

    D3DVIEWPORT9 vp = { 0, 0, (DWORD)clip_rect.right, (DWORD)clip_rect.bottom, 0.0f, 1.0f };
    dev->SetViewport(&vp);

    setupBlitRenderState(dev, solid);

    dev->BeginScene();
    drawBlitQuad(dev, blitTex, dest_r, src_r, src->clip_rect.right, src->clip_rect.bottom);
    dev->EndScene();

    restoreBlitState(dev, saved);
    damage(dest_r);
}

void gxCanvas::blitstretch(int x, int y, int w, int h,
    gxCanvas* src, int src_x, int src_y,
    int src_w, int src_h, bool solid)
{
    x += origin_x - src->handle_x;
    y += origin_y - src->handle_y;

    Rect dest_r(x, y, w, h);
    if (!::clip(viewport, &dest_r)) return;

    int clipLeft = dest_r.left - x;
    int clipTop = dest_r.top - y;
    int clipRight = dest_r.right - x;
    int clipBottom = dest_r.bottom - y;

    Rect src_r;
    src_r.left = src_x + clipLeft * src_w / w;
    src_r.top = src_y + clipTop * src_h / h;
    src_r.right = src_x + clipRight * src_w / w;
    src_r.bottom = src_y + clipBottom * src_h / h;

    if (!::clip(src->clip_rect, &src_r)) return;

    if (!isRenderTarget(surf)) {
        cpuBlit(this, dest_r, src, src_r, solid);
        damage(dest_r);
        return;
    }

    IDirect3DDevice9* dev = graphics->dir3dDev;
    if (!dev) return;
    FillModeGuard guard(dev);

    unsigned maskRGB = solid ? ~0u : (src->format.toARGB(src->mask_surf) & 0x00ffffffu);
    IDirect3DTexture9* blitTex = getOrBuildBlitTex(dev, src, maskRGB);
    if (!blitTex) return;

    SavedBlitState saved;
    saveBlitState(dev, saved);

    dev->SetRenderTarget(0, surf);
    dev->SetDepthStencilSurface(nullptr);

    D3DVIEWPORT9 vp = { 0, 0, (DWORD)clip_rect.right, (DWORD)clip_rect.bottom, 0.0f, 1.0f };
    dev->SetViewport(&vp);

    bool useAlpha = (src->getFlags() & gxCanvas::CANVAS_TEX_ALPHA) != 0;
    bool useMask = src->hasMask();

    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

    if (useAlpha) {
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }
    else if (useMask) {
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
        dev->SetRenderState(D3DRS_ALPHAREF, 0);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }
    else {
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

    dev->BeginScene();
    drawBlitQuad(dev, blitTex, dest_r, src_r, src->clip_rect.right, src->clip_rect.bottom);
    dev->EndScene();

    restoreBlitState(dev, saved);
    damage(dest_r);
}

static void cpuBlitAlpha(gxCanvas* dest, const RECT& dest_r, gxCanvas* src, const RECT& src_r, unsigned color_argb) {
    int dw = dest_r.right - dest_r.left;
    int dh = dest_r.bottom - dest_r.top;

    if (!dest->lock()) return;
    if (!src->lock()) { dest->unlock(); return; }

    unsigned tintR = (color_argb >> 16) & 0xff;
    unsigned tintG = (color_argb >> 8) & 0xff;
    unsigned tintB = color_argb & 0xff;
    unsigned tintA = (color_argb >> 24) & 0xff;

    const PixelFormat& sf = src->format;
    const PixelFormat& df = dest->format;

    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            unsigned srcArgb = sf.toARGB(src->getPixelFast(src_r.left + x, src_r.top + y));
            unsigned srcA = ((srcArgb >> 24) & 0xff) * tintA / 255;
            if (srcA == 0) continue;

            int dx = dest_r.left + x, dy = dest_r.top + y;
            if (srcA >= 255) {
                unsigned outArgb = 0xff000000 | (tintR << 16) | (tintG << 8) | tintB;
                dest->setPixelFast(dx, dy, df.fromARGB(outArgb));
                continue;
            }

            unsigned dstArgb = df.toARGB(dest->getPixelFast(dx, dy));
            unsigned dstR = (dstArgb >> 16) & 0xff, dstG = (dstArgb >> 8) & 0xff, dstB = dstArgb & 0xff;
            unsigned outR = (tintR * srcA + dstR * (255 - srcA)) / 255;
            unsigned outG = (tintG * srcA + dstG * (255 - srcA)) / 255;
            unsigned outB = (tintB * srcA + dstB * (255 - srcA)) / 255;
            dest->setPixelFast(dx, dy, df.fromARGB(0xff000000 | (outR << 16) | (outG << 8) | outB));
        }
    }

    src->unlock();
    dest->unlock();
    dest->damage(dest_r);
}

void gxCanvas::blitAlpha(int x, int y, gxCanvas* src,
    int src_x, int src_y, int src_w, int src_h,
    unsigned color_argb, bool filter) {
    x += origin_x - src->handle_x;
    y += origin_y - src->handle_y;

    Rect dest_r(x, y, src_w, src_h);
    Rect src_r(src_x, src_y, src_w, src_h);

    if (!clip(&dest_r, &src_r)) return;
    if (!::clip(src->clip_rect, &src_r, &dest_r)) return;

    if (!isRenderTarget(surf)) {
        cpuBlitAlpha(this, dest_r, src, src_r, color_argb);
        return;
    }

    IDirect3DDevice9* dev = graphics->dir3dDev;
    if (!dev) return;

    IDirect3DBaseTexture9* tex = src->getTexture();
    if (!tex) return;

    bool ownBatch = !blit_batch_active;
    if (ownBatch) beginBlitBatch();

    if (!blit_batch_active) {
        endBlitBatch();
        return;
    }

    FillModeGuard guard(dev);

    if (effect2D) {
        drawQuadWithEffect(dev, effect2D, (IDirect3DTexture9*)tex, dest_r, src_r, src->clip_rect.right, src->clip_rect.bottom, true, color_argb);
    }
    else {
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

        dev->SetRenderState(D3DRS_TEXTUREFACTOR, color_argb);

        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

        if (filter) {
            dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        }
        else {
            dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        }

        drawBlitQuad(dev, (IDirect3DTexture9*)tex, dest_r, src_r, src->clip_rect.right, src->clip_rect.bottom);
    }

    damage(dest_r);

    if (ownBatch) endBlitBatch();
}

void gxCanvas::text(int x, int y, const std::string& t) {
    int ty = y + origin_y;
    if (ty >= viewport.bottom) return;
    if (ty + font->getHeight() <= viewport.top) return;
    int tx = x + origin_x;
    if (tx >= viewport.right) return;
    int b = 0, w;
    while (b < (int)t.size() && tx + (w = font->charAdvance(UTF8::decodeCharacter(t.c_str(), b))) <= viewport.left) {
        tx += w; x += w;
        b += UTF8::measureCodepoint(t[b]);
    }
    int e = b;
    while (e < (int)t.size() && tx < viewport.right) {
        tx += font->charAdvance(UTF8::decodeCharacter(t.c_str(), e));
        e += UTF8::measureCodepoint(t[e]);
    }
    if (e > b) {
        beginBlitBatch();
        font->render(this, format.toARGB(color_surf), x, y, t.substr(b, e - b));
        endBlitBatch();
    }
}

int gxCanvas::getWidth()  const { return logical_w; }
int gxCanvas::getHeight() const { return logical_h; }
int gxCanvas::getDepth()  const { return format.getDepth(); }

void gxCanvas::getOrigin(int* x, int* y)  const { *x = origin_x; *y = origin_y; }
void gxCanvas::getHandle(int* x, int* y)  const { *x = handle_x; *y = handle_y; }

void gxCanvas::getViewport(int* x, int* y, int* w, int* h) const {
    *x = viewport.left; *y = viewport.top;
    *w = viewport.right - viewport.left; *h = viewport.bottom - viewport.top;
}

unsigned gxCanvas::getMask()     const { return format.toARGB(mask_surf); }
unsigned gxCanvas::getColor()    const { return format.toARGB(color_surf); }
unsigned gxCanvas::getClsColor() const { return format.toARGB(clsColor_surf); }

bool gxCanvas::collide(int x1, int y1, const gxCanvas* i2, int x2, int y2, bool solid) const {
    x1 -= handle_x; x2 -= i2->handle_x;
    if (x1 + clip_rect.right <= x2 || x1 >= x2 + i2->clip_rect.right) return false;
    y1 -= handle_y; y2 -= i2->handle_y;
    if (y1 + clip_rect.bottom <= y2 || y1 >= y2 + i2->clip_rect.bottom) return false;
    if (solid) return true;
    if (!cm_mask) { cm_mask = new unsigned[cm_pitch * clip_rect.bottom]; updateBitMask(clip_rect); }
    if (!i2->cm_mask) { i2->cm_mask = new unsigned[i2->cm_pitch * i2->clip_rect.bottom]; i2->updateBitMask(i2->clip_rect); }
    const gxCanvas* i1 = this;
    if (x1 > x2) { std::swap(x1, x2); std::swap(y1, y2); std::swap(i1, i2); }
    Rect r1, r2, ir;
    r1.left = x1; r1.top = y1; r1.right = x1 + i1->clip_rect.right; r1.bottom = y1 + i1->clip_rect.bottom;
    r2.left = x2; r2.top = y2; r2.right = x2 + i2->clip_rect.right; r2.bottom = y2 + i2->clip_rect.bottom;
    ir.left = r1.left > r2.left ? r1.left : r2.left; ir.right = r1.right < r2.right ? r1.right : r2.right;
    ir.top = r1.top > r2.top ? r1.top : r2.top;      ir.bottom = r1.bottom < r2.bottom ? r1.bottom : r2.bottom;
    unsigned* s1 = i1->cm_mask, * s2 = i2->cm_mask;
    int i1p = i1->cm_pitch, i2p = i2->cm_pitch;
    s1 += (ir.top - r1.top) * i1p;
    s2 += (ir.top - r2.top) * i2p;
    int startx = ir.left - r1.left, stopx = ir.right - r1.left - 1;
    int shr = startx & 31, shl = 32 - shr, cnt = stopx / 32 - startx / 32;
    unsigned lwm = LWMS[stopx & 31];
    s1 += startx / 32;
    for (int y = ir.top; y < ir.bottom; ++y) {
        unsigned p = 0, * row1 = s1, * row2 = s2;
        for (int x = 0; x < cnt; ++x) { unsigned n = *row2++; if (((n >> shr) | p) & *row1++) return true; p = shl < 32 ? n << shl : 0; }
        if (((*row2 >> shr) | p) & *row1 & lwm) return true;
        s1 += i1p; s2 += i2p;
    }
    return false;
}

bool gxCanvas::rect_collide(int x1, int y1, int x2, int y2, int w2, int h2, bool solid) const {
    x1 -= handle_x; if (x1 + clip_rect.right <= x2 || x1 >= x2 + w2) return false;
    y1 -= handle_y; if (y1 + clip_rect.bottom <= y2 || y1 >= y2 + h2) return false;
    if (solid) return true;
    Rect r1(x1, y1, clip_rect.right, clip_rect.bottom), r2(x2, y2, w2, h2), ir;
    ir.left = r1.left > r2.left ? r1.left : r2.left; ir.right = r1.right < r2.right ? r1.right : r2.right;
    ir.top = r1.top > r2.top ? r1.top : r2.top;      ir.bottom = r1.bottom < r2.bottom ? r1.bottom : r2.right;
    if (!cm_mask) { cm_mask = new unsigned[cm_pitch * clip_rect.bottom]; updateBitMask(clip_rect); }
    unsigned* s1 = cm_mask + (ir.top - r1.top) * cm_pitch;
    int startx = ir.left - r1.left, stopx = ir.right - r1.left - 1;
    int cnt = stopx / 32 - startx / 32;
    unsigned fwm = FWMS[startx & 31], lwm = LWMS[stopx & 31];
    if (!cnt) { fwm &= lwm; lwm = 0; }
    s1 += startx / 32;
    for (int h = ir.top; h < ir.bottom; ++h) {
        unsigned* row = s1;
        if (*row & fwm) return true;
        for (int x = 1; x < cnt; ++x) if (*++row) return true;
        if (lwm && (*++row & lwm)) return true;
        s1 += cm_pitch;
    }
    return false;
}

bool gxCanvas::lock() const {
    if (locked_cnt == 0) {
        D3DSURFACE_DESC desc;
        HRESULT hr = surf->GetDesc(&desc);
        if (FAILED(hr)) return false;

        bool isRT = (desc.Usage & D3DUSAGE_RENDERTARGET) != 0;
        lock_is_rt = isRT;

        if (isRT) {
            if (t_surf) {
                D3DSURFACE_DESC tdesc;
                t_surf->GetDesc(&tdesc);
                if (tdesc.Width != desc.Width || tdesc.Height != desc.Height || tdesc.Format != desc.Format) {
                    t_surf->Release();
                    t_surf = nullptr;
                }
            }
            if (!t_surf) {
                hr = graphics->dir3dDev->CreateOffscreenPlainSurface(
                    desc.Width, desc.Height, desc.Format,
                    D3DPOOL_SYSTEMMEM, &t_surf, nullptr);
                if (FAILED(hr)) return false;
            }

            hr = graphics->dir3dDev->GetRenderTargetData(surf, t_surf);
            if (FAILED(hr)) return false;

            D3DLOCKED_RECT lr;
            hr = t_surf->LockRect(&lr, nullptr, 0);
            if (FAILED(hr)) return false;

            locked_pitch = lr.Pitch;
            locked_surf = (unsigned char*)lr.pBits;
        }
        else {
            D3DLOCKED_RECT lr;
            hr = surf->LockRect(&lr, nullptr, D3DLOCK_NOSYSLOCK);
            if (FAILED(hr)) return false;

            locked_pitch = lr.Pitch;
            locked_surf = (unsigned char*)lr.pBits;
        }

        lock_mod_cnt = mod_cnt;
    }
    ++locked_cnt;
    return true;
}

void gxCanvas::unlock() const {
    if (locked_cnt == 0) return;

    if (locked_cnt == 1) {
        if (lock_is_rt) {
            if (t_surf) {
                t_surf->UnlockRect();
                graphics->dir3dDev->UpdateSurface(t_surf, nullptr, surf, nullptr);
            }
        }
        else {
            surf->UnlockRect();
        }

        if (lock_mod_cnt != mod_cnt && cm_mask) {
            updateBitMask(clip_rect);
        }
    }
    --locked_cnt;
}

void gxCanvas::setPixel(int x, int y, unsigned argb) {
    x += origin_x; if (x < viewport.left || x >= viewport.right)  return;
    y += origin_y; if (y < viewport.top || y >= viewport.bottom) return;
    lock();
    setPixelFast(x, y, argb);
    unlock();
}

unsigned gxCanvas::getPixel(int x, int y) const {
    x += origin_x; if (x < viewport.left || x >= viewport.right)  return format.toARGB(mask_surf);
    y += origin_y; if (y < viewport.top || y >= viewport.bottom) return format.toARGB(mask_surf);
    lock();
    unsigned p = getPixelFast(x, y);
    unlock();
    return p;
}

void gxCanvas::copyPixelFast(int x, int y, gxCanvas* src, int src_x, int src_y) {
    if (format.getDepth() == src->format.getDepth()) {
        switch (format.getDepth()) {
        case 16: *(short*)(locked_surf + y * locked_pitch + x * 2) = *(short*)(src->locked_surf + src_y * src->locked_pitch + src_x * 2); return;
        case 24: { unsigned char* p = locked_surf + y * locked_pitch + x * 3; unsigned char* t = src->locked_surf + src_y * src->locked_pitch + src_x * 3; *(short*)p = *(short*)t; *(char*)(p + 2) = *(char*)(t + 2); } return;
        case 32: *(int*)(locked_surf + y * locked_pitch + x * 4) = *(int*)(src->locked_surf + src_y * src->locked_pitch + src_x * 4); return;
        }
    }
    int sp = src->format.getPitch();
    int dp = format.getPitch();
    unsigned char* sPix = src->locked_surf + src_y * src->locked_pitch + src_x * sp;
    unsigned char* dPix = locked_surf + y * locked_pitch + x * dp;
    unsigned argb = src->format.toARGB(src->format.getPixel(sPix));
    format.setPixel(dPix, format.fromARGB(argb));
}

void gxCanvas::copyPixel(int x, int y, gxCanvas* src, int src_x, int src_y) {
    x += origin_x; if (x < viewport.left || x >= viewport.right) return;
    y += origin_y; if (y < viewport.top || y >= viewport.bottom) return;
    src_x += src->origin_x; if (src_x < src->viewport.left || src_x >= src->viewport.right) return;
    src_y += src->origin_y; if (src_y < src->viewport.top || src_y >= src->viewport.bottom) return;
    lock(); src->lock();
    copyPixelFast(x, y, src, src_x, src_y);
    src->unlock(); unlock();
}

void gxCanvas::setCubeMode(int mode) { cube_mode = mode; }

void gxCanvas::setCubeFace(int face) {
    if (face >= 0 && face < 6 && cube_surfs[face])
        surf = cube_surfs[face];
}