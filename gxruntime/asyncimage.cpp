#include "std.h"
#include "asyncimage.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <chrono>

AsyncImageLoader& AsyncImageLoader::instance() {
	static AsyncImageLoader loader;
	return loader;
}

std::unique_ptr<DecodedImage> DecodeImageFile(const std::string& file, std::string* err) {
	auto fail = [&](const char* what) -> std::unique_ptr<DecodedImage> {
		if (err) {
			*err = std::string(what) + ": " + file + " (" + SDL_GetError() + ")";
			SDL_ClearError();
		}
		return nullptr;
	};

	SDL_Surface* surf = IMG_Load(file.c_str());
	if (!surf) return fail("Load failed");

	SDL_Surface* cvt = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
	if (!cvt) {
		SDL_DestroySurface(surf);
		return fail("Convert failed");
	}

	int w = cvt->w, h = cvt->h;
	if (w <= 0 || h <= 0) {
		SDL_DestroySurface(cvt);
		SDL_DestroySurface(surf);
		return fail("Empty image");
	}

	auto img = std::make_unique<DecodedImage>();
	img->w = w;
	img->h = h;
	img->rgba.resize((size_t)w * (size_t)h * 4);

	Uint8 keyR = 0, keyG = 0, keyB = 0;
	bool hasKey = false;
	if (SDL_SurfaceHasColorKey(surf)) {
		Uint32 key = 0;
		if (SDL_GetSurfaceColorKey(surf, &key)) {
			if (SDL_Palette* pal = SDL_GetSurfacePalette(surf)) {
				if (key < (Uint32)pal->ncolors) {
					keyR = pal->colors[key].r;
					keyG = pal->colors[key].g;
					keyB = pal->colors[key].b;
					hasKey = true;
				}
			} else if (const SDL_PixelFormatDetails* det = SDL_GetPixelFormatDetails(surf->format)) {
				SDL_GetRGB(key, det, nullptr, &keyR, &keyG, &keyB);
				hasKey = true;
			}
		}
	}

	bool hasAlpha = false;
	for (int y = 0; y < h; ++y) {
		const Uint8* src = (const Uint8*)cvt->pixels + (size_t)y * cvt->pitch;
		Uint8* dst = img->rgba.data() + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x) {
			Uint8 r = src[x * 4 + 0], g = src[x * 4 + 1], b = src[x * 4 + 2], a = src[x * 4 + 3];
			if (hasKey && r == keyR && g == keyG && b == keyB) a = 0;
			if (a != 255) hasAlpha = true;
			dst[x * 4 + 0] = r;
			dst[x * 4 + 1] = g;
			dst[x * 4 + 2] = b;
			dst[x * 4 + 3] = a;
		}
	}
	img->hasAlpha = hasAlpha;

	SDL_DestroySurface(cvt);
	SDL_DestroySurface(surf);
	return img;
}

AsyncImageLoader::AsyncImageLoader() :
	inFlight(0), shutdown(false) {
	unsigned count = std::thread::hardware_concurrency() / 2;
	if (count < 2) count = 2;
	if (count > 4) count = 4;
	for (unsigned n = 0; n < count; ++n) threads.push_back(std::thread(&AsyncImageLoader::worker, this));
}

AsyncImageLoader::~AsyncImageLoader() {
	{
		std::unique_lock<std::mutex> lock(mutex);
		shutdown = true;
	}
	cv.notify_all();
	for (auto& t : threads) t.join();
}

std::shared_ptr<AsyncImageLoader::Job> AsyncImageLoader::load(const std::string& file) {
	auto job = std::make_shared<Job>(file);
	{
		std::unique_lock<std::mutex> lock(mutex);
		queue.push_back(job);
	}
	cv.notify_one();
	return job;
}

void AsyncImageLoader::worker() {
	for (;;) {
		std::shared_ptr<Job> job;
		{
			std::unique_lock<std::mutex> lock(mutex);
			cv.wait(lock, [this]() { return shutdown || !queue.empty(); });
			if (shutdown && queue.empty()) return;
			job = queue.back();
			queue.pop_back();
			++inFlight;
			job->state.store(STATE_DECODING);
		}

		auto img = DecodeImageFile(job->file);

		std::unique_lock<std::mutex> lock(mutex);
		if (job->state.load() == STATE_CANCELLED) {
			--inFlight;
			cv.notify_all();
			continue;
		}
		if (img) {
			job->image = std::move(img);
			job->state.store(STATE_DONE);
		}
		else {
			job->state.store(STATE_FAILED);
		}
		--inFlight;
		cv.notify_all();
	}
}

void AsyncImageLoader::wait(const std::shared_ptr<Job>& job) {
	std::unique_lock<std::mutex> lock(mutex);

	//decode synchronously as fallback to avoid infinite loads
	if (job->state.load() == STATE_QUEUED) {
		for (auto it = queue.begin(); it != queue.end(); ++it) {
			if (*it == job) { queue.erase(it); break; }
		}
		job->state.store(STATE_DECODING);
		lock.unlock();

		auto img = DecodeImageFile(job->file);

		lock.lock();
		if (img) {
			job->image = std::move(img);
			job->state.store(STATE_DONE);
		}
		else {
			job->state.store(STATE_FAILED);
		}
		cv.notify_all();
		return;
	}

	cv.wait(lock, [&]() {
		int s = job->state.load();
		return s == STATE_DONE || s == STATE_FAILED || s == STATE_CANCELLED;
	});
}

void AsyncImageLoader::waitAll() {
	std::unique_lock<std::mutex> lock(mutex);
	cv.wait_for(lock, std::chrono::seconds(20), [this]() { return queue.empty() && inFlight == 0; });
}

void AsyncImageLoader::cancel(const std::shared_ptr<Job>& job) {
	std::unique_lock<std::mutex> lock(mutex);
	for (auto it = queue.begin(); it != queue.end(); ++it) {
		if (*it == job) {
			queue.erase(it);
			break;
		}
	}
	job->image.reset();
	job->state.store(STATE_CANCELLED);
}