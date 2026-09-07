#include "std.h"
#include "gxmesh.h"
#include "gxgraphics.h"

#include "gxruntime.h"
#include "sdlgpu/sdl_gpu_mesh.h"

extern gxRuntime* gx_runtime;

gxMesh::gxMesh(gxGraphics* g, IDirect3DVertexBuffer9* vs, IDirect3DIndexBuffer9* is,
    int max_vs, int max_ts) :
    graphics(g), vertex_buff(vs), index_buff(is), vertex_decl(nullptr),
    locked_verts(nullptr), locked_skin_verts(nullptr), locked_indices(nullptr),
    max_verts(max_vs), max_tris(max_ts), mesh_dirty(false), skinned(false) {
    if (g && g->runtime && g->runtime->sdlGpu) {
        gpuMirror = sdlgpu::CreateMesh(g->runtime->sdlGpu, sizeof(dxVertex), max_vs, max_ts);
    }
}

gxMesh::gxMesh(gxGraphics* g, IDirect3DVertexBuffer9* vs, IDirect3DIndexBuffer9* is,
    IDirect3DVertexDeclaration9* decl, int max_vs, int max_ts) :
    graphics(g), vertex_buff(vs), index_buff(is), vertex_decl(decl),
    locked_verts(nullptr), locked_skin_verts(nullptr), locked_indices(nullptr),
    max_verts(max_vs), max_tris(max_ts), mesh_dirty(false), skinned(true) {
    if (g && g->runtime && g->runtime->sdlGpu) {
        gpuMirror = sdlgpu::CreateMesh(g->runtime->sdlGpu, sizeof(dxSkinVertex), max_vs, max_ts);
    }
}

gxMesh::~gxMesh() {
    unlock();
    if (graphics && graphics->runtime && gpuMirror) {
        sdlgpu::ReleaseMesh(graphics->runtime->sdlGpu, gpuMirror);
        gpuMirror = nullptr;
    }
    if (vertex_buff) { vertex_buff->Release(); vertex_buff = nullptr; }
    if (index_buff) { index_buff->Release();  index_buff = nullptr; }
}

bool gxMesh::lock(bool all) {
    if ((locked_verts || locked_skin_verts) && locked_indices) return true;

    // lock vert buffer
    if (skinned) {
        if (!locked_skin_verts) {
            DWORD vflags = D3DLOCK_NOSYSLOCK | (all ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE);
            void* ptr = nullptr;
            if (FAILED(vertex_buff->Lock(0, 0, &ptr, vflags))) {
                return false;
            }
            locked_skin_verts = reinterpret_cast<dxSkinVertex*>(ptr);
        }
    }
    else if (!locked_verts) {
        DWORD vflags = D3DLOCK_NOSYSLOCK | (all ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE);
        void* ptr = nullptr;
        if (FAILED(vertex_buff->Lock(0, 0, &ptr, vflags))) {
            return false;
        }
        locked_verts = reinterpret_cast<dxVertex*>(ptr);
    }

    // lock index buffer
    if (!locked_indices) {
        DWORD iflags = D3DLOCK_NOSYSLOCK | (all ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE);
        void* ptr = nullptr;
        if (FAILED(index_buff->Lock(0, 0, &ptr, iflags))) {
            if (locked_verts) { vertex_buff->Unlock(); locked_verts = nullptr; }
            if (locked_skin_verts) { vertex_buff->Unlock(); locked_skin_verts = nullptr; }
            return false;
        }
        locked_indices = reinterpret_cast<WORD*>(ptr);
    }

    mesh_dirty = false;
    return true;
}

void gxMesh::unlock() {
    if (gpuMirror && graphics && graphics->runtime && graphics->runtime->sdlGpu) {
        const void* verts = skinned ? (const void*)locked_skin_verts : (const void*)locked_verts;
        unsigned stride = skinned ? sizeof(dxSkinVertex) : sizeof(dxVertex);
        if (verts && locked_indices) {
            sdlgpu::UploadMesh(graphics->runtime->sdlGpu, gpuMirror,
                verts, stride * (unsigned)max_verts,
                locked_indices, (unsigned)sizeof(WORD) * (unsigned)max_tris * 3);
        }
    }
    if (locked_verts) {
        vertex_buff->Unlock();
        locked_verts = nullptr;
    }
    if (locked_skin_verts) {
        vertex_buff->Unlock();
        locked_skin_verts = nullptr;
    }
    if (locked_indices) {
        index_buff->Unlock();
        locked_indices = nullptr;
    }
}

void gxMesh::backup() {
	unlock();
}

void gxMesh::restore() {
	mesh_dirty = true;
}

void gxMesh::render(int first_vert, int vert_cnt, int first_tri, int tri_cnt) {
    unlock();

    IDirect3DDevice9* dev = graphics->dir3dDev;

    if (skinned) {
        dev->SetVertexDeclaration(vertex_decl);
        dev->SetStreamSource(0, vertex_buff, 0, sizeof(dxSkinVertex));
        dev->SetIndices(index_buff);
        dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, first_vert, 0, vert_cnt, first_tri * 3, tri_cnt);
        return;
    }

    dev->SetStreamSource(0, vertex_buff, 0, sizeof(dxVertex));
    dev->SetFVF(VTXFMT);
    dev->SetIndices(index_buff);
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, first_vert, 0, vert_cnt, first_tri * 3, tri_cnt);
}

void gxMesh::renderSkinned(int first_vert, int vert_cnt, int first_tri, int tri_cnt,
    const float* bone_data, int bone_cnt) {
    unlock();

    IDirect3DDevice9* dev = graphics->dir3dDev;
    IDirect3DVertexShader9* shader = graphics->getSkinningShader();
    if (!shader || !vertex_decl) return;

    if (bone_cnt > MAX_SKIN_BONES) bone_cnt = MAX_SKIN_BONES;

    dev->SetVertexShaderConstantF(0, bone_data, bone_cnt * 3);

    dev->SetVertexDeclaration(vertex_decl);
    dev->SetVertexShader(shader);
    dev->SetStreamSource(0, vertex_buff, 0, sizeof(dxSkinVertex));
    dev->SetIndices(index_buff);
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, first_vert, 0, vert_cnt, first_tri * 3, tri_cnt);

    dev->SetVertexShader(nullptr);
    dev->SetVertexDeclaration(nullptr);
}
