#include "sdl_gpu_pipeline.h"

#include "../std.h"

#include <cstring>

#include <SDL3/SDL_gpu.h>

#include "shaders/blit_shaders.h"

namespace sdlgpu {

namespace {
SDL_GPUDevice* g_blitDev = nullptr;
SDL_GPUTextureFormat g_blitFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
SDL_GPUGraphicsPipeline* g_blitPipe = nullptr;
SDL_GPUSampler* g_blitSamp = nullptr;
SDL_GPUTexture* g_blitTex = nullptr;
unsigned g_blitW = 0, g_blitH = 0;
}

static void TeardownBlit();

static SDL_GPUShader* LoadShader(SDL_GPUDevice* dev, SDL_GPUShaderFormat fmt, SDL_GPUShaderStage stage, const char* entry, const uint8_t* code, size_t size, unsigned samplers = 0) {
	SDL_GPUShaderCreateInfo info{};
	info.code = code;
	info.code_size = size;
	info.entrypoint = entry;
	info.format = fmt;
	info.stage = stage;
	info.num_samplers = samplers;
	return SDL_CreateGPUShader(dev, &info);
}

static void TeardownBlit() {
	if (g_blitPipe && g_blitDev) SDL_ReleaseGPUGraphicsPipeline(g_blitDev, g_blitPipe);
	if (g_blitSamp && g_blitDev) SDL_ReleaseGPUSampler(g_blitDev, g_blitSamp);
	if (g_blitTex && g_blitDev) SDL_ReleaseGPUTexture(g_blitDev, g_blitTex);
	g_blitPipe = nullptr;
	g_blitSamp = nullptr;
	g_blitTex = nullptr;
	g_blitDev = nullptr;
	g_blitFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	g_blitW = g_blitH = 0;
}

static bool EnsureBlit(SDL_GPUDevice* dev, SDL_Window* win, unsigned w, unsigned h) {
	SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(dev, win);
	if (g_blitPipe && g_blitDev == dev && g_blitFormat == fmt && g_blitTex && g_blitW == w && g_blitH == h) return true;
	if (!g_blitPipe || g_blitDev != dev || g_blitFormat != fmt) {
		TeardownBlit();
		SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(dev);
		const uint8_t* vsCode = nullptr;
		const uint8_t* psCode = nullptr;
		size_t vsSize = 0, psSize = 0;
		SDL_GPUShaderFormat useFmt = SDL_GPU_SHADERFORMAT_INVALID;
		if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
			useFmt = SDL_GPU_SHADERFORMAT_SPIRV;
			vsCode = kBlitVS_SPIRV; vsSize = kBlitVS_SPIRV_size;
			psCode = kBlitPS_SPIRV; psSize = kBlitPS_SPIRV_size;
		}
		else if (supported & SDL_GPU_SHADERFORMAT_DXIL) {
			useFmt = SDL_GPU_SHADERFORMAT_DXIL;
			vsCode = kBlitVS_DXIL; vsSize = kBlitVS_DXIL_size;
			psCode = kBlitPS_DXIL; psSize = kBlitPS_DXIL_size;
		}
		if (useFmt == SDL_GPU_SHADERFORMAT_INVALID) return false;

		SDL_GPUShader* vs = LoadShader(dev, useFmt, SDL_GPU_SHADERSTAGE_VERTEX, "VSMain", vsCode, vsSize);
		if (!vs) return false;
		SDL_GPUShader* ps = LoadShader(dev, useFmt, SDL_GPU_SHADERSTAGE_FRAGMENT, "PSMain", psCode, psSize, 1);
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

		g_blitPipe = SDL_CreateGPUGraphicsPipeline(dev, &info);
		SDL_ReleaseGPUShader(dev, vs);
		SDL_ReleaseGPUShader(dev, ps);
		if (!g_blitPipe) return false;

		SDL_GPUSamplerCreateInfo samp{};
		samp.min_filter = SDL_GPU_FILTER_LINEAR;
		samp.mag_filter = SDL_GPU_FILTER_LINEAR;
		samp.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		samp.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		g_blitSamp = SDL_CreateGPUSampler(dev, &samp);
		if (!g_blitSamp) { TeardownBlit(); return false; }

		g_blitDev = dev;
		g_blitFormat = fmt;
	}
	if (!g_blitTex || g_blitW != w || g_blitH != h) {
		if (g_blitTex) SDL_ReleaseGPUTexture(g_blitDev, g_blitTex);
		SDL_GPUTextureCreateInfo texInfo{};
		texInfo.type = SDL_GPU_TEXTURETYPE_2D;
		texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		texInfo.width = w;
		texInfo.height = h;
		texInfo.layer_count_or_depth = 1;
		texInfo.num_levels = 1;
		g_blitTex = SDL_CreateGPUTexture(dev, &texInfo);
		if (!g_blitTex) return false;
		g_blitW = w;
		g_blitH = h;
	}
	return true;
}

bool PresentBlit(SDL_GPUDevice* dev, SDL_Window* win, float r, float g, float b, unsigned w, unsigned h, const void* px) {
	if (!dev || !win || !w || !h || !px) return false;
	if (!EnsureBlit(dev, win, w, h)) return false;

	Uint32 size = w * h * 4;
	SDL_GPUTransferBufferCreateInfo bufInfo{};
	bufInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	bufInfo.size = size;
	SDL_GPUTransferBuffer* buf = SDL_CreateGPUTransferBuffer(dev, &bufInfo);
	if (!buf) return false;
	void* dst = SDL_MapGPUTransferBuffer(dev, buf, false);
	if (!dst) { SDL_ReleaseGPUTransferBuffer(dev, buf); return false; }
	memcpy(dst, px, size);
	SDL_UnmapGPUTransferBuffer(dev, buf);

	SDL_GPUCommandBuffer* cmds = SDL_AcquireGPUCommandBuffer(dev);
	if (!cmds) { SDL_ReleaseGPUTransferBuffer(dev, buf); return false; }

	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmds);
	SDL_GPUTextureTransferInfo src{};
	src.transfer_buffer = buf;
	src.pixels_per_row = w;
	src.rows_per_layer = h;
	SDL_GPUTextureRegion reg{};
	reg.texture = g_blitTex;
	reg.w = w;
	reg.h = h;
	reg.d = 1;
	SDL_UploadToGPUTexture(copy, &src, &reg, false);
	SDL_EndGPUCopyPass(copy);

	SDL_GPUTexture* tex = nullptr;
	Uint32 sw = 0, sh = 0;
	if (!SDL_AcquireGPUSwapchainTexture(cmds, win, &tex, &sw, &sh)) {
		SDL_CancelGPUCommandBuffer(cmds);
		SDL_ReleaseGPUTransferBuffer(dev, buf);
		return false;
	}
	if (tex) {
		SDL_GPUColorTargetInfo target{};
		target.texture = tex;
		target.load_op = SDL_GPU_LOADOP_CLEAR;
		target.store_op = SDL_GPU_STOREOP_STORE;
		target.clear_color = SDL_FColor{ r, g, b, 1.0f };
		SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmds, &target, 1, nullptr);
		SDL_BindGPUGraphicsPipeline(pass, g_blitPipe);
		SDL_GPUTextureSamplerBinding bind{};
		bind.texture = g_blitTex;
		bind.sampler = g_blitSamp;
		SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
		SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
		SDL_EndGPURenderPass(pass);
	}
	bool ok = SDL_SubmitGPUCommandBuffer(cmds);
	SDL_ReleaseGPUTransferBuffer(dev, buf);
	return ok;
}

void TeardownPipelines() {
	TeardownBlit();
}

}
