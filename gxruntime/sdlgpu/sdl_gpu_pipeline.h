#ifndef SDL_GPU_PIPELINE_H
#define SDL_GPU_PIPELINE_H

struct SDL_GPUDevice;
struct SDL_Window;
struct SDL_GPURenderPass;

namespace sdlgpu {

bool PresentBlit(SDL_GPUDevice* dev, SDL_Window* win, float r, float g, float b, unsigned w, unsigned h, const void* px);
void TeardownPipelines();

}

#endif
