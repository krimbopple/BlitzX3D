#ifndef SDL_GPU_PIPELINE_H
#define SDL_GPU_PIPELINE_H

struct SDL_GPUDevice;
struct SDL_Window;
struct SDL_GPURenderPass;
struct SDL_GPUCommandBuffer;
struct SDL_GPUTexture;

namespace sdlgpu {

struct GpuMesh;

bool PresentBlit(SDL_GPUDevice* dev, SDL_Window* win, float r, float g, float b, unsigned w, unsigned h, const void* px);
void DrawMesh(SDL_GPUDevice* dev, SDL_Window* win, SDL_GPUCommandBuffer* cmds, SDL_GPURenderPass* pass, GpuMesh* mesh, const float* viewProj, SDL_GPUTexture* tex, unsigned indexCount);
void TeardownPipelines();

}

#endif
