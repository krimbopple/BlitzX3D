#ifndef SDL_GPU_PIPELINE_H
#define SDL_GPU_PIPELINE_H

struct SDL_GPUDevice;
struct SDL_Window;
struct SDL_GPURenderPass;

namespace sdlgpu {

void DrawTrivial(SDL_GPUDevice* dev, SDL_Window* win, SDL_GPURenderPass* pass);
void TeardownPipelines();

}

#endif
