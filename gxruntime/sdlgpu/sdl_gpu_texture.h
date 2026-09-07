#ifndef SDL_GPU_TEXTURE_H
#define SDL_GPU_TEXTURE_H

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace sdlgpu {

SDL_GPUTexture* CreateTexture2D(SDL_GPUDevice* dev, unsigned w, unsigned h);
bool UploadTextureRGBA(SDL_GPUDevice* dev, SDL_GPUTexture* tex, unsigned w, unsigned h, const void* px);
void ReleaseTexture(SDL_GPUDevice* dev, SDL_GPUTexture* tex);

}

#endif
