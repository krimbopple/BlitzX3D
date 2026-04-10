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

#include <algorithm>
#include <cctype>

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
	std::string ext = toLowerExt(filename);

	ALuint buffer = AL_NONE;
	alGenBuffers(1, &buffer);
	if (alGetError() != AL_NO_ERROR) return AL_NONE;

	bool ok = false;

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
					alBufferData(buffer, fmt, pcm.data(),
						(ALsizei)(mono * sizeof(short)), (ALsizei)sampleRate);
				}
				else {
					fmt = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					alBufferData(buffer, fmt, pcm.data(),
						(ALsizei)(read * channels * sizeof(short)), (ALsizei)sampleRate);
				}
				ok = (alGetError() == AL_NO_ERROR);
			}
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
					alBufferData(buffer, fmt, pcm.data(),
						(ALsizei)(mono * sizeof(short)), (ALsizei)sampleRate);
				}
				else {
					fmt = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
					alBufferData(buffer, fmt, pcm.data(),
						(ALsizei)(read * channels * sizeof(short)), (ALsizei)sampleRate);
				}
				ok = (alGetError() == AL_NO_ERROR);
			}
		}
	}
	// OGG vorbis.. like.. verbose logging..
	else if (ext == "ogg") {
		int channels = 0;
		int sampleRate = 0;
		short* decoded = nullptr;
		int samples = stb_vorbis_decode_filename(
			filename.c_str(), &channels, &sampleRate, &decoded);

		if (samples > 0 && decoded) {
			outFreq = sampleRate;
			ALenum fmt;
			if (forceMono && channels > 1) {
				// downmix
				std::vector<short> mono(samples);
				for (int i = 0; i < samples; ++i) {
					int sum = 0;
					for (int c = 0; c < channels; ++c) sum += decoded[i * channels + c];
					mono[i] = (short)(sum / channels);
				}
				fmt = AL_FORMAT_MONO16;
				alBufferData(buffer, fmt, mono.data(),
					(ALsizei)(samples * sizeof(short)), sampleRate);
			}
			else {
				fmt = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
				alBufferData(buffer, fmt, decoded,
					(ALsizei)(samples * channels * sizeof(short)), sampleRate);
			}
			free(decoded);
			ok = (alGetError() == AL_NO_ERROR);
		}
		else if (decoded) {
			free(decoded);
		}
	}

	if (!ok) {
		alDeleteBuffers(1, &buffer);
		return AL_NONE;
	}
	return buffer;
}


struct SoundChannel : public gxChannel {
	ALuint src;
	int    nativeFreq; // base freq for pitch

	SoundChannel() : src(AL_NONE), nativeFreq(44100) {
		alGenSources(1, &src);
		alSourcef(src, AL_ROLLOFF_FACTOR, 1.0f);
	}
	~SoundChannel() override {
		if (src != AL_NONE) {
			alSourceStop(src);
			alDeleteSources(1, &src);
		}
	}

	void stop() override {
		alSourceStop(src);
	}
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
		alSource3f(src, AL_POSITION, pan, 0.0f, 0.0f);
		alSourcef(src, AL_ROLLOFF_FACTOR, 0.0f);
		alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
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
		alSourcei(src, AL_BUFFER, (ALint)alBuffer);
		alSourcei(src, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
		alSourcef(src, AL_GAIN, 1.0f);
		alSourcePlay(src);
	}
	~StreamChannel() override {
		if (src != AL_NONE) {
			alSourceStop(src);
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
		}
		if (!c->isPlaying()) {
			next_chan = (int)((idx + 1) % soundChannels.size());
			return c;
		}
	}
	// if all slots are full we grow grow grow
	size_t oldSize = soundChannels.size();
	soundChannels.resize(oldSize * 2, nullptr);
	SoundChannel* c = soundChannels[oldSize] = new SoundChannel();
	channels.push_back(c);
	next_chan = (int)(oldSize + 1);
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

gxChannel* gxAudio::play(gxSound* sound) {
	SoundChannel* chan = allocSoundChannel();
	chan->nativeFreq = sound->getFreq();

	alSourcei(chan->src, AL_BUFFER, (ALint)sound->getBuffer());
	alSourcei(chan->src, AL_LOOPING, sound->isLooping() ? AL_TRUE : AL_FALSE);
	alSourcef(chan->src, AL_GAIN, sound->getVol());
	alSourcei(chan->src, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcef(chan->src, AL_ROLLOFF_FACTOR, 0.0f);
	alSource3f(chan->src, AL_POSITION, sound->getPan(), 0.0f, 0.0f);
	alSourcePlay(chan->src);
	return chan;
}

gxChannel* gxAudio::play3d(gxSound* sound, const float pos[3], const float vel[3]) {
	SoundChannel* chan = allocSoundChannel();
	chan->nativeFreq = sound->getFreq();

	alSourcei(chan->src, AL_BUFFER, (ALint)sound->getBuffer());
	alSourcei(chan->src, AL_LOOPING, sound->isLooping() ? AL_TRUE : AL_FALSE);
	alSourcef(chan->src, AL_GAIN, sound->getVol());
	alSourcei(chan->src, AL_SOURCE_RELATIVE, AL_FALSE);
	alSourcef(chan->src, AL_ROLLOFF_FACTOR, 1.0f);
	alSource3f(chan->src, AL_POSITION, pos[0], pos[1], pos[2]);
	alSource3f(chan->src, AL_VELOCITY, vel[0], vel[1], vel[2]);
	alSourcePlay(chan->src);
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
	int freq = 44100;
	ALuint buf = loadALBuffer(filename, use3d, freq);
	if (buf == AL_NONE) return nullptr;

	gxSound* sound = new gxSound(this, buf, freq);
	sound_set.insert(sound);
	return sound;
}

gxSound* gxAudio::verifySound(gxSound* s) {
	return sound_set.count(s) ? s : nullptr;
}

void gxAudio::freeSound(gxSound* s) {
	if (!sound_set.erase(s)) return;
	for (auto* c : soundChannels) {
		if (!c) continue;
		if (c->isPlaying()) {
			ALint buf = AL_NONE;
			alGetSourcei(c->src, AL_BUFFER, &buf);
			if ((ALuint)buf == s->getBuffer()) {
				alSourceStop(c->src);
				alSourcei(c->src, AL_BUFFER, AL_NONE);
			}
		}
	}
	delete s;
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
	}
	else {
		int freq = 44100;
		buf = loadALBuffer(f, false, freq);
		if (buf == AL_NONE) return nullptr;
		song_buffers[f] = buf;
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