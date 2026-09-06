#ifndef SDL_GPU_CONTEXT_H
#define SDL_GPU_CONTEXT_H

struct SDL_Window;
class gxRuntime;

namespace sdlgpu {

SDL_Window* CreateGameWindow(int clientW, int clientH, bool resizable, bool borderless, const char* title);

void* GetHWND(SDL_Window* win);
void DestroyGameWindow(SDL_Window* win);
void SetWindowTitle(SDL_Window* win, const char* title);
void SizeWindowForClient(SDL_Window* win, int clientW, int clientH);
void CenterWindow(SDL_Window* win);
void PumpEvents(SDL_Window* win, ::gxRuntime* rt);
int SdlScancodeToDIK(int sdlScancode);

}

#endif
