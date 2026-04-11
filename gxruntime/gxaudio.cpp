#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

extern "C" int stb_vorbis_decode_filename(const char* filename, int* channels, int* sample_rate, short** output);

#include <AL/al.h>
#include <AL/alc.h>

#include "std.h"
#include "gxaudio.h"
#include "gxsound.h"
#include "gxchannel.h"
#include "gxruntime.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <sys/stat.h>

extern gxRuntime* gx_runtime;

static void logALError(const char* operation) {
	ALenum err = alGetError();
	if (err != AL_NO_ERROR) {
		const char* msg = "unknown";
		switch (err) {
		case AL_INVALID_NAME:      msg = "AL_INVALID_NAME"; break;
		case AL_INVALID_ENUM:      msg = "AL_INVALID_ENUM"; break;
		case AL_INVALID_VALUE:     msg = "AL_INVALID_VALUE"; break;
		case AL_INVALID_OPERATION: msg = "AL_INVALID_OPERATION"; break;
		case AL_OUT_OF_MEMORY:     msg = "AL_OUT_OF_MEMORY"; break;
		}
		if (gx_runtime) gx_runtime->debugLog(std::format("OpenAL error after {}: {}", operation, msg).c_str());
	}
}

static std::string toLowerExt(const std::string& path) {
	std::string ext;
	auto dot = path.rfind('.');
	if (dot != std::string::npos) {
		ext = path.substr(dot + 1);
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return std::tolower(c); });
	}
	return ext;
}

static drwav_uint64 downmixToMono_s16(short* buf, drwav_uint64 frames, unsigned int channels) {
	if (channels == 1) return frames;
	for (drwav_uint64 i = 0; i < frames; ++i) {
		int sum = 0;
		for (unsigned int c = 0; c < channels; ++c) sum += buf[i * channels + c];
		buf[i] = (short)(sum / (int)channels);
	}
	return frames;
}

static drmp3_uint64 downmixToMono_mp3(short* buf, drmp3_uint64 frames, drmp3_uint32 channels) {
	if (channels == 1) return frames;
	for (drmp3_uint64 i = 0; i < frames; ++i) {
		int sum = 0;
		for (drmp3_uint32 c = 0; c < channels; ++c) sum += buf[i * channels + c];
		buf[i] = (short)(sum / (int)channels);
	}
	return frames;
}

static unsigned int loadALBuffer(const std::string& filename, bool forceMono, int& outFreq) {
	if (gx_runtime) gx_runtime->debugLog(
		std::format("loadALBuffer: {} (forceMono={})", filename, forceMono).c_str());

	alGetError();

	std::string ext = toLowerExt(filename);
	ALuint buffer = AL_NONE;

	alGenBuffers(1, &buffer);
	ALenum err = alGetError();
	if (err != AL_NO_ERROR) {
		if (gx_runtime) gx_runtime->debugLog(std::format("alGenBuffers failed: error={}", err).c_str());
		return AL_NONE;
	}
	if (gx_runtime) gx_runtime->debugLog(std::format("Generated buffer: {}", buffer).c_str());

	bool ok = false;
	outFreq = 44100;

	// WAV 
	if (ext == "wav") {
		drwav wav;
		if (drwav_init_file(&wav, filename.c_str(), nullptr)) {
			drwav_uint64 frameCount = wav.totalPCMFrameCount;
			unsigned int channels = wav.channels;
			unsigned int sampleRate = wav.sampleRate;
			outFreq = (int)sampleRate;

			std::vector<short> pcm(frameCount * channels);
			drwav_uint64 read = drwav_read_pcm_frames_s16(&wav, frameCount, pcm.data());
			drwav_uninit(&wav);

			if (read > 0) {
				ALenum fmt;
				if (forceMono && channels > 1) {
					drwav_uint64 mono = downmixToMono_s16(pcm.data(), read, channels);
					fmt = AL_FORMAT_MONO16;
					alBufferData(buffer, fmt, pcm.data(), (ALsizei)(mono * sizeof(short)), (ALsizei)sampleRate);
				}
				else {
					fmt = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					alBufferData(buffer, fmt, pcm.data(), (ALsizei)(read * channels * sizeof(short)), (ALsizei)sampleRate);
				}
				ok = (alGetError() == AL_NO_ERROR);
				if (!ok && gx_runtime) gx_runtime->debugLog("alBufferData failed for WAV");
			}
			else {
				if (gx_runtime) gx_runtime->debugLog("WAV: read 0 frames");
			}
		}
		else {
			if (gx_runtime) gx_runtime->debugLog(
				std::format("WAV: drwav_init_file failed for {}", filename).c_str());
		}
	}
	// MP3
	else if (ext == "mp3") {
		drmp3 mp3;
		if (drmp3_init_file(&mp3, filename.c_str(), nullptr)) {
			drmp3_uint32 channels = mp3.channels;
			drmp3_uint32 sampleRate = mp3.sampleRate;
			outFreq = (int)sampleRate;

			drmp3_uint64 frameCount = drmp3_get_pcm_frame_count(&mp3);
			std::vector<short> pcm(frameCount * channels);
			drmp3_uint64 read = drmp3_read_pcm_frames_s16(&mp3, frameCount, pcm.data());
			drmp3_uninit(&mp3);

			if (read > 0) {
				ALenum fmt;
				if (forceMono && channels > 1) {
					drmp3_uint64 mono = downmixToMono_mp3(pcm.data(), read, channels);
					fmt = AL_FORMAT_MONO16;
					alBufferData(buffer, fmt, pcm.data(), (ALsizei)(mono * sizeof(short)), (ALsizei)sampleRate);
				}
				else {
					fmt = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					alBufferData(buffer, fmt, pcm.data(), (ALsizei)(read * channels * sizeof(short)), (ALsizei)sampleRate);
				}
				ok = (alGetError() == AL_NO_ERROR);
				if (!ok && gx_runtime) gx_runtime->debugLog("alBufferData failed for MP3");
			}
			else {
				if (gx_runtime) gx_runtime->debugLog("MP3: read 0 frames");
			}
		}
		else {
			if (gx_runtime) gx_runtime->debugLog(
				std::format("MP3: drmp3_init_file failed for {}", filename).c_str());
		}
	}
	// OGG vorbis.. like.. verbose logging..
	else if (ext == "ogg") {
		if (gx_runtime) gx_runtime->debugLog("Processing OGG file");

		struct stat st;
		if (stat(filename.c_str(), &st) != 0) {
			if (gx_runtime) gx_runtime->debugLog(
				std::format("OGG stat failed: {}", filename).c_str());
		}
		else {
			if (gx_runtime) gx_runtime->debugLog("OGG file exists, decoding...");

			int  channels = 0;
			int  sampleRate = 0;
			short* decoded = nullptr;
			int totalSamples = stb_vorbis_decode_filename(filename.c_str(), &channels, &sampleRate, &decoded);

			if (gx_runtime) gx_runtime->debugLog(std::format("stb_vorbis returned: samples={}, channels={}, rate={}", totalSamples, channels, sampleRate).c_str());

			if (totalSamples <= 0 || !decoded) {
				if (gx_runtime) gx_runtime->debugLog(std::format("OGG decode failed, code={}", totalSamples).c_str());
				if (decoded) free(decoded);
			}
			else {
				outFreq = sampleRate;
				ALenum fmt;

				if (forceMono && channels > 1) {
					int frames = totalSamples / channels;
					std::vector<short> mono(frames);
					for (int i = 0; i < frames; ++i) {
						int sum = 0;
						for (int c = 0; c < channels; ++c)
							sum += decoded[i * channels + c];
						mono[i] = (short)(sum / channels);
					}
					fmt = AL_FORMAT_MONO16;
					alBufferData(buffer, fmt, mono.data(), (ALsizei)(frames * sizeof(short)), sampleRate);
				}
				else {
					fmt = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					alBufferData(buffer, fmt, decoded, (ALsizei)(totalSamples * sizeof(short)), sampleRate);
				}

				free(decoded);
				err = alGetError();
				if (err == AL_NO_ERROR) {
					ok = true;
					if (gx_runtime) gx_runtime->debugLog("alBufferData succeeded for OGG");
				}
				else {
					if (gx_runtime) gx_runtime->debugLog(
						std::format("alBufferData failed for OGG, error={}", err).c_str());
				}
			}
		}
	}
	else {
		if (gx_runtime) gx_runtime->debugLog(
			std::format("Unknown extension: {}", ext).c_str());
	}

	if (!ok) {
		alDeleteBuffers(1, &buffer);
		if (gx_runtime) gx_runtime->debugLog("loadALBuffer failed, buffer deleted");
		return AL_NONE;
	}

	if (gx_runtime) gx_runtime->debugLog(
		std::format("loadALBuffer success, buffer={}, freq={}", buffer, outFreq).c_str());
	return buffer;
}


struct SoundChannel : public gxChannel {
	ALuint src;
	int    nativeFreq; // base freq for pitch

	SoundChannel() : src(AL_NONE), nativeFreq(44100) {
		alGenSources(1, &src);
		alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcef(src, AL_ROLLOFF_FACTOR, 0.0f);
	}
	~SoundChannel() override {
		if (src != AL_NONE) {
			alSourceStop(src);
			alSourcei(src, AL_BUFFER, AL_NONE); // detach before source delete
			alDeleteSources(1, &src);
		}
	}

	void stop() override { alSourceStop(src); }
	void setPaused(bool paused) override {
		if (paused) alSourcePause(src);
		else        alSourcePlay(src);
	}
	void setPitch(int pitch) override {
		float ratio = (nativeFreq > 0) ? (float)pitch / (float)nativeFreq : 1.0f;
		if (ratio <= 0.0f) ratio = 0.001f;
		alSourcef(src, AL_PITCH, ratio);
	}
	void setVolume(float volume) override {
		if (volume < 0.0f) volume = 0.0f;
		if (volume > 1.0f) volume = 1.0f;
		alSourcef(src, AL_GAIN, volume);
	}
	void setPan(float pan) override {
		alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcef(src, AL_ROLLOFF_FACTOR, 0.0f);
		alSource3f(src, AL_POSITION, pan, 0.0f, 0.0f);
	}
	void set3d(const float pos[3], const float vel[3]) override {
		alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
		alSourcef(src, AL_ROLLOFF_FACTOR, 1.0f);
		alSource3f(src, AL_POSITION, pos[0], pos[1], pos[2]);
		alSource3f(src, AL_VELOCITY, vel[0], vel[1], vel[2]);
	}
	bool isPlaying() override {
		ALint state = AL_STOPPED;
		alGetSourcei(src, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING || state == AL_PAUSED;
	}
};

struct StreamChannel : public gxChannel {
	ALuint src;

	explicit StreamChannel(ALuint alBuffer, bool loop) : src(AL_NONE) {
		alGenSources(1, &src);
		alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcef(src, AL_ROLLOFF_FACTOR, 0.0f);
		alSourcei(src, AL_BUFFER, (ALint)alBuffer);
		alSourcei(src, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
		alSourcef(src, AL_GAIN, 1.0f);
		alSourcePlay(src);
	}
	~StreamChannel() override {
		if (src != AL_NONE) {
			alSourceStop(src);
			alSourcei(src, AL_BUFFER, AL_NONE);
			alDeleteSources(1, &src);
		}
	}

	void play() { alSourcePlay(src); }

	void stop() override { alSourceStop(src); }
	void setPaused(bool paused) override {
		if (paused) alSourcePause(src);
		else        alSourcePlay(src);
	}
	void setPitch(int /*pitch*/) override {}  // not meaningful for music streams
	void setVolume(float volume) override {
		if (volume < 0.0f) volume = 0.0f;
		if (volume > 1.0f) volume = 1.0f;
		alSourcef(src, AL_GAIN, volume);
	}
	void setPan(float /*pan*/) override {}
	void set3d(const float[3], const float[3]) override {}
	bool isPlaying() override {
		ALint state = AL_STOPPED;
		alGetSourcei(src, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING || state == AL_PAUSED;
	}
};

// returned when CD-track playback is requested
struct NullChannel : public gxChannel {
	void stop() override {}
	void setPaused(bool) override {}
	void setPitch(int) override {}
	void setVolume(float) override {}
	void setPan(float) override {}
	void set3d(const float[3], const float[3]) override {}
	bool isPlaying() override { return false; }
};

static ALCdevice* al_device = nullptr;
static ALCcontext* al_context = nullptr;

static std::set<gxSound*>   sound_set;
static std::vector<gxChannel*> channels;

// cache of "music" streams
static std::map<std::string, StreamChannel*> songs;
static std::map<std::string, ALuint> song_buffers;

static NullChannel  s_nullChannel;

static int next_chan = 0;
static std::vector<SoundChannel*> soundChannels;

static SoundChannel* allocSoundChannel() {
	for (size_t i = 0; i < soundChannels.size(); ++i) {
		size_t idx = (next_chan + i) % soundChannels.size();
		SoundChannel* c = soundChannels[idx];
		if (!c) {
			c = soundChannels[idx] = new SoundChannel();
			channels.push_back(c);
			if (gx_runtime) gx_runtime->debugLog(
				std::format("allocSoundChannel: created new at slot {}", idx).c_str());
			next_chan = (int)((idx + 1) % soundChannels.size());
			return c;
		}
		if (!c->isPlaying()) {
			// detach old buffer on reuse so we never hold a stale ref
			alSourceStop(c->src);
			alSourcei(c->src, AL_BUFFER, AL_NONE);
			next_chan = (int)((idx + 1) % soundChannels.size());
			if (gx_runtime) gx_runtime->debugLog(
				std::format("allocSoundChannel: reusing slot {}", idx).c_str());
			return c;
		}
	}
	// if all slots are full we grow grow grow
	size_t oldSize = soundChannels.size();
	soundChannels.resize(oldSize * 2, nullptr);
	SoundChannel* c = soundChannels[oldSize] = new SoundChannel();
	channels.push_back(c);
	next_chan = (int)(oldSize + 1);
	if (gx_runtime) gx_runtime->debugLog(
		std::format("allocSoundChannel: grew pool to {}, new at {}", oldSize * 2, oldSize).c_str());
	return c;
}

gxAudio::gxAudio(gxRuntime* r) : runtime(r), masterPaused(false) {
	next_chan = 0;
	soundChannels.resize(256, nullptr);
}

gxAudio::~gxAudio() {
	for (auto* c : channels) delete c;
	channels.clear();
	soundChannels.clear();

	while (!sound_set.empty()) freeSound(*sound_set.begin());

	for (auto& kv : song_buffers) alDeleteBuffers(1, &kv.second);
	song_buffers.clear();
	songs.clear();
}

gxChannel* gxAudio::play(gxSound* sound) {
	if (gx_runtime) gx_runtime->debugLog(
		std::format("play: buffer={}, loop={}", sound->getBuffer(), sound->isLooping()).c_str());

	SoundChannel* chan = allocSoundChannel();
	if (!chan) {
		if (gx_runtime) gx_runtime->debugLog("allocSoundChannel returned nullptr!");
		return nullptr;
	}
	chan->nativeFreq = sound->getFreq();

	alSourcei(chan->src, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcef(chan->src, AL_ROLLOFF_FACTOR, 0.0f);
	alSource3f(chan->src, AL_POSITION, sound->getPan(), 0.0f, 0.0f);
	alSourcei(chan->src, AL_BUFFER, (ALint)sound->getBuffer());
	alSourcei(chan->src, AL_LOOPING, sound->isLooping() ? AL_TRUE : AL_FALSE);
	alSourcef(chan->src, AL_GAIN, sound->getVol());
	alSourcePlay(chan->src);
	logALError("play");
	if (gx_runtime) gx_runtime->debugLog("play: alSourcePlay called");
	return chan;
}

gxChannel* gxAudio::play3d(gxSound* sound, const float pos[3], const float vel[3]) {
	SoundChannel* chan = allocSoundChannel();
	if (!chan) return nullptr;
	chan->nativeFreq = sound->getFreq();

	alSourcei(chan->src, AL_SOURCE_RELATIVE, AL_FALSE);
	alSourcef(chan->src, AL_ROLLOFF_FACTOR, 1.0f);
	alSource3f(chan->src, AL_POSITION, pos[0], pos[1], pos[2]);
	alSource3f(chan->src, AL_VELOCITY, vel[0], vel[1], vel[2]);
	alSourcei(chan->src, AL_BUFFER, (ALint)sound->getBuffer());
	alSourcei(chan->src, AL_LOOPING, sound->isLooping() ? AL_TRUE : AL_FALSE);
	alSourcef(chan->src, AL_GAIN, sound->getVol());
	alSourcePlay(chan->src);
	logALError("play3d");
	return chan;
}

void gxAudio::pause() {
	masterPaused = true;
	alListenerf(AL_GAIN, 0.0f);
}

void gxAudio::resume() {
	masterPaused = false;
	alListenerf(AL_GAIN, 1.0f);
}

gxSound* gxAudio::loadSound(const std::string& filename, bool use3d) {
	std::string key = makeCacheKey(filename, use3d);
	auto it = soundCache.find(key);
	if (it != soundCache.end()) {
		it->second.refCount++;
		if (gx_runtime) gx_runtime->debugLog(
			std::format("Cache hit: {} (refcount={})", filename, it->second.refCount).c_str());
		return it->second.sound;
	}

	int freq = 44100;
	ALuint buf = loadALBuffer(filename, use3d, freq);
	if (buf == AL_NONE) {
		if (gx_runtime) gx_runtime->debugLog(
			std::format("loadSound: loadALBuffer failed for {}", filename).c_str());
		return nullptr;
	}
	gxSound* sound = new gxSound(this, buf, freq);
	sound_set.insert(sound);
	soundCache[key] = { sound, 1 };
	if (gx_runtime) gx_runtime->debugLog(
		std::format("Cache new: {} (refcount=1)", filename).c_str());
	return sound;
}

gxSound* gxAudio::verifySound(gxSound* s) {
	return sound_set.count(s) ? s : nullptr;
}

void gxAudio::freeSound(gxSound* s) {
	for (auto it = soundCache.begin(); it != soundCache.end(); ++it) {
		if (it->second.sound != s) continue;

		it->second.refCount--;
		if (gx_runtime) gx_runtime->debugLog(
			std::format("FreeSound: {} refcount now {}", it->first, it->second.refCount).c_str());
		if (it->second.refCount > 0) return;

		soundCache.erase(it);
		sound_set.erase(s);

		// detach from ALL sources regardless of playing state because deleting a buffer still attached to any source even stopped WILL POISON ALL OF THEM!! WHAT THE FUCK!
		ALuint targetBuf = s->getBuffer();
		for (auto* c : soundChannels) {
			if (!c) continue;
			ALint attached = AL_NONE;
			alGetSourcei(c->src, AL_BUFFER, &attached);
			if ((ALuint)attached == targetBuf) {
				alSourceStop(c->src);
				alSourcei(c->src, AL_BUFFER, AL_NONE);
			}
		}

		delete s;
		if (gx_runtime) gx_runtime->debugLog("Sound deleted");
		return;
	}

	// not in cache
	if (sound_set.erase(s)) {
		ALuint targetBuf = s->getBuffer();
		for (auto* c : soundChannels) {
			if (!c) continue;
			ALint attached = AL_NONE;
			alGetSourcei(c->src, AL_BUFFER, &attached);
			if ((ALuint)attached == targetBuf) {
				alSourceStop(c->src);
				alSourcei(c->src, AL_BUFFER, AL_NONE);
			}
		}
		delete s;
		if (gx_runtime) gx_runtime->debugLog("FreeSound: not in cache, deleted directly");
	}
}

void gxAudio::setPaused(bool paused) {
	if (paused) pause(); else resume();
}

void gxAudio::setVolume(float volume) {
	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	alListenerf(AL_GAIN, masterPaused ? 0.0f : volume);
}

void gxAudio::set3dOptions(float roll, float dopp, float dist) {
	alDopplerFactor(dopp);
	alSpeedOfSound(343.3f * dist);
	for (auto* c : soundChannels) {
		if (c) alSourcef(c->src, AL_ROLLOFF_FACTOR, roll);
	}
}

void gxAudio::set3dListener(const float pos[3], const float vel[3],
	const float forward[3], const float up[3]) {
	alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);
	alListener3f(AL_VELOCITY, vel[0], vel[1], vel[2]);
	float orient[6] = {
		forward[0], forward[1], forward[2],
		up[0],      up[1],      up[2]
	};
	alListenerfv(AL_ORIENTATION, orient);
}

gxChannel* gxAudio::playFile(const std::string& t, bool /*use3d*/, int mode) {
	std::string f = t;
	std::transform(f.begin(), f.end(), f.begin(),
		[](unsigned char c) { return std::tolower(c); });

	auto it = songs.find(f);
	if (it != songs.end()) {
		it->second->play();
		return it->second;
	}

	ALuint buf = AL_NONE;
	auto bit = song_buffers.find(f);
	if (bit != song_buffers.end()) {
		buf = bit->second;
		if (gx_runtime) gx_runtime->debugLog(
			std::format("playFile: reusing buffer for {}", f).c_str());
	}
	else {
		int freq = 44100;
		buf = loadALBuffer(f, false, freq);
		if (buf == AL_NONE) {
			if (gx_runtime) gx_runtime->debugLog(
				std::format("playFile: failed to load {}", f).c_str());
			return nullptr;
		}
		song_buffers[f] = buf;
		if (gx_runtime) gx_runtime->debugLog(
			std::format("playFile: loaded new buffer for {}", f).c_str());
	}

	bool loop = (mode == CD_MODE_LOOP || mode == CD_MODE_ALL);
	StreamChannel* chan = new StreamChannel(buf, loop);
	channels.push_back(chan);
	songs[f] = chan;
	return chan;
}

gxChannel* gxAudio::playCDTrack(int /*track*/, int /*mode*/) {
	// CD audio is not supported anymore
	return &s_nullChannel;
}

bool gxAudio_Init() {
	if (al_device && al_context) {
		alcMakeContextCurrent(al_context);
		alGetError();
		return true;
	}
	al_device = alcOpenDevice(nullptr); // default output device
	if (!al_device) return false;

	al_context = alcCreateContext(al_device, nullptr);
	if (!al_context) {
		alcCloseDevice(al_device);
		al_device = nullptr;
		return false;
	}
	alcMakeContextCurrent(al_context);
	alGetError(); // ignore any startup errors like regalis intended!
	return true;
}

void gxAudio_Shutdown() {
	if (al_context) {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(al_context);
		al_context = nullptr;
	}
	if (al_device) {
		alcCloseDevice(al_device);
		al_device = nullptr;
	}
}