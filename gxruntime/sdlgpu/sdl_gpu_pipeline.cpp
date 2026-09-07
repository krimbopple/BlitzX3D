#include "sdl_gpu_pipeline.h"

#include "../std.h"

#include <SDL3/SDL_gpu.h>

#include "shaders/trivial_shaders.h"

namespace sdlgpu {

namespace {
SDL_GPUDevice* g_dev = nullptr;
SDL_GPUTextureFormat g_format = SDL_GPU_TEXTUREFORMAT_INVALID;
SDL_GPUGraphicsPipeline* g_pipe = nullptr;
}

static SDL_GPUShader* LoadShader(SDL_GPUDevice* dev, SDL_GPUShaderFormat fmt, SDL_GPUShaderStage stage, const char* entry, const uint8_t* code, size_t size) {
	SDL_GPUShaderCreateInfo info{};
	info.code = code;
	info.code_size = size;
	info.entrypoint = entry;
	info.format = fmt;
	info.stage = stage;
	return SDL_CreateGPUShader(dev, &info);
}

static bool EnsureTrivial(SDL_GPUDevice* dev, SDL_Window* win) {
	SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(dev, win);
	if (g_pipe && g_dev == dev && g_format == fmt) return true;
	TeardownPipelines();

	SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(dev);
	const uint8_t* vsCode = nullptr;
	const uint8_t* psCode = nullptr;
	size_t vsSize = 0, psSize = 0;
	SDL_GPUShaderFormat useFmt = SDL_GPU_SHADERFORMAT_INVALID;
	const char* vsEntry = "VSMain";
	const char* psEntry = "PSMain";
	if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
		useFmt = SDL_GPU_SHADERFORMAT_SPIRV;
		vsCode = kTrivialVS_SPIRV; vsSize = kTrivialVS_SPIRV_size;
		psCode = kTrivialPS_SPIRV; psSize = kTrivialPS_SPIRV_size;
	}
	else if (supported & SDL_GPU_SHADERFORMAT_DXIL) {
		useFmt = SDL_GPU_SHADERFORMAT_DXIL;
		vsCode = kTrivialVS_DXIL; vsSize = kTrivialVS_DXIL_size;
		psCode = kTrivialPS_DXIL; psSize = kTrivialPS_DXIL_size;
	}
	if (useFmt == SDL_GPU_SHADERFORMAT_INVALID) return false;

	SDL_GPUShader* vs = LoadShader(dev, useFmt, SDL_GPU_SHADERSTAGE_VERTEX, vsEntry, vsCode, vsSize);
	if (!vs) return false;
	SDL_GPUShader* ps = LoadShader(dev, useFmt, SDL_GPU_SHADERSTAGE_FRAGMENT, psEntry, psCode, psSize);
	if (!ps) { SDL_ReleaseGPUShader(dev, vs); return false; }

	SDL_GPUColorTargetDescription target{};
	target.format = fmt;

	SDL_GPUGraphicsPipelineCreateInfo info{};
	info.vertex_shader = vs;
	info.fragment_shader = ps;
	info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	info.target_info.num_color_targets = 1;
	info.target_info.color_target_descriptions = &target;

	g_pipe = SDL_CreateGPUGraphicsPipeline(dev, &info);
	SDL_ReleaseGPUShader(dev, vs);
	SDL_ReleaseGPUShader(dev, ps);
	if (!g_pipe) return false;
	g_dev = dev;
	g_format = fmt;
	return true;
}

void DrawTrivial(SDL_GPUDevice* dev, SDL_Window* win, SDL_GPURenderPass* pass) {
	if (!dev || !win || !pass) return;
	if (!EnsureTrivial(dev, win)) return;
	SDL_BindGPUGraphicsPipeline(pass, g_pipe);
	SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

void TeardownPipelines() {
	if (g_pipe && g_dev) SDL_ReleaseGPUGraphicsPipeline(g_dev, g_pipe);
	g_pipe = nullptr;
	g_dev = nullptr;
	g_format = SDL_GPU_TEXTUREFORMAT_INVALID;
}

}
