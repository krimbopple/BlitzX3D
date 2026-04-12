#include "std.h"
#include "bbaudio.h"
#include "../MultiLang/MultiLang.h"
#include "../gxruntime/gxaudio_stream.h"

gxAudio* gx_audio;

static inline void debugSound(gxSound* s, const char* function) {
	if (!gx_audio->verifySound(s)) ErrorLog(function, MultiLang::sound_not_exist);
}

static gxSound* loadSound(BBStr* f, bool use_3d) {
	std::string t = *f; delete f;
	return gx_audio ? gx_audio->loadSound(t, use_3d) : 0;
}

static gxChannel* playMusic(BBStr* f, bool use_3d, int mode) {
	std::string t = *f; delete f;
	return gx_audio ? gx_audio->playFile(t, use_3d, mode) : 0;
}

int bbVerifySound(gxSound* sound) {
	return (bool)gx_audio->verifySound(sound);
}

gxSound* bbLoadSound(BBStr* f) {
	return loadSound(f, false);
}

void bbFreeSound(gxSound* sound) {
	if (!sound) return;
	debugSound(sound, "FreeSound");
	gx_audio->freeSound(sound);
}

void bbLoopSound(gxSound* sound) {
	if (!sound) return;
	debugSound(sound, "LoopSound");
	sound->setLoop(true);
}

void bbSoundPitch(gxSound* sound, int pitch) {
	if (!sound) return;
	debugSound(sound, "SoundPitch");
	sound->setPitch(pitch);
}

void bbSoundVolume(gxSound* sound, float volume) {
	if (!sound) return;
	debugSound(sound, "SoundVolume");
	sound->setVolume(volume);
}

void bbSoundPan(gxSound* sound, float pan) {
	if (!sound) return;
	debugSound(sound, "SoundPan");
	sound->setPan(pan);
}

gxChannel* bbPlaySound(gxSound* sound) {
	if (!sound) return 0;
	debugSound(sound, "PlaySound");
	return sound->play();
}

gxChannel* bbPlaySound3D(gxSound* sound, float x, float y, float z,
	float vx, float vy, float vz) {
	if (!sound) return 0;
	debugSound(sound, "PlaySound3D");
	const float pos[3] = { x, y, z };
	const float vel[3] = { vx, vy, vz };
	return sound->play3d(pos, vel);
}

AsyncSoundHandle* bbLoadSoundAsync(BBStr* f) {
	std::string t = *f; delete f;
	if (!gx_audio) return nullptr;
	return gxAudio_LoadSoundAsync(gx_audio, t, false);
}

AsyncSoundHandle* bbLoad3DSoundAsync(BBStr* f) {
	std::string t = *f; delete f;
	if (!gx_audio) return nullptr;
	return gxAudio_LoadSoundAsync(gx_audio, t, true);
}

int bbAsyncSoundReady(AsyncSoundHandle* handle) {
	return (handle && handle->isReady()) ? 1 : 0;
}

int bbAsyncSoundFailed(AsyncSoundHandle* handle) {
	return (handle && handle->isFailed()) ? 1 : 0;
}

gxSound* bbAsyncSoundGet(AsyncSoundHandle* handle) {
	if (!handle || !handle->isReady()) return nullptr;
	return handle->sound;
}

void bbFreeAsyncSound(AsyncSoundHandle* handle) {
	gxAudio_FreeAsyncHandle(handle);
}

gxChannel* bbOpenStreamSound(BBStr* f, int loop) {
	std::string t = *f; delete f;
	gxStreamChannel* sc = gxAudio_OpenStream(t, loop != 0);
	return sc;
}

gxChannel* bbStreamSound(BBStr* f, float volume, int loop) {
	std::string t = *f; delete f;
	gxStreamChannel* sc = gxAudio_OpenStream(t, loop != 0);
	if (!sc) return nullptr;
	sc->setVolume(volume);
	return sc;
}

void bbFreeStreamSound(gxChannel* channel) {
	if (!channel) return;
	channel->stop();
	delete channel;
}

void bbStopStream(gxChannel* channel) {
	bbFreeStreamSound(channel);
}

void bbSetStreamVolume(gxChannel* channel, float volume) {
	if (!channel) return;
	channel->setVolume(volume);
}

gxChannel* bbPlayMusic(BBStr* f, int mode) {
	return playMusic(f, false, mode);
}

gxChannel* bbPlayCDTrack(int track, int mode) {
	return gx_audio ? gx_audio->playCDTrack(track, mode) : 0;
}

void bbStopChannel(gxChannel* channel) {
	if (!channel) return;
	channel->stop();
}

void bbPauseChannel(gxChannel* channel) {
	if (!channel) return;
	channel->setPaused(true);
}

void bbResumeChannel(gxChannel* channel) {
	if (!channel) return;
	channel->setPaused(false);
}

void bbChannelPitch(gxChannel* channel, int pitch) {
	if (!channel) return;
	channel->setPitch(pitch);
}

void bbChannelVolume(gxChannel* channel, float volume) {
	if (!channel) return;
	channel->setVolume(volume);
}

void bbChannelPan(gxChannel* channel, float pan) {
	if (!channel) return;
	channel->setPan(pan);
}

int bbChannelPlaying(gxChannel* channel) {
	return channel ? channel->isPlaying() : 0;
}

gxSound* bbLoad3DSound(BBStr* f) {
	return loadSound(f, true);
}

bool audio_create() {
	gx_audio = gx_runtime->openAudio(0);
	return true;
}

bool audio_destroy() {
	if (gx_audio) gx_runtime->closeAudio(gx_audio);
	gx_audio = nullptr;
	gxAudioStream_Shutdown();
	return true;
}

void audio_link(void(*rtSym)(const char*, void*)) {
	rtSym("%VerifySound%sound", bbVerifySound);
	rtSym("%LoadSound$filename", bbLoadSound);
	rtSym("FreeSound%sound", bbFreeSound);
	rtSym("LoopSound%sound", bbLoopSound);
	rtSym("SoundPitch%sound%pitch", bbSoundPitch);
	rtSym("SoundVolume%sound#volume", bbSoundVolume);
	rtSym("SoundPan%sound#pan", bbSoundPan);
	rtSym("%PlaySound%sound", bbPlaySound);
	rtSym("%PlaySound3D%sound#x#y#z#vx=0#vy=0#vz=0", bbPlaySound3D);

	rtSym("%LoadSoundAsync$filename", bbLoadSoundAsync);
	rtSym("%Load3DSoundAsync$filename", bbLoad3DSoundAsync);
	rtSym("%AsyncSoundReady%handle", bbAsyncSoundReady);
	rtSym("%AsyncSoundFailed%handle", bbAsyncSoundFailed);
	rtSym("%AsyncSoundGet%handle", bbAsyncSoundGet);
	rtSym("FreeAsyncSound%handle", bbFreeAsyncSound);

	rtSym("%OpenStreamSound$filename%loop=0", bbOpenStreamSound);
	rtSym("%StreamSound$filename#volume%loop=0", bbStreamSound);
	rtSym("FreeStreamSound%channel", bbFreeStreamSound);
	rtSym("StopStream%channel", bbStopStream);
	rtSym("SetStreamVolume%channel#volume", bbSetStreamVolume);

	rtSym("%PlayMusic$midifile%mode=0", bbPlayMusic);
	rtSym("%PlayCDTrack%track%mode=1", bbPlayCDTrack);
	rtSym("StopChannel%channel", bbStopChannel);
	rtSym("PauseChannel%channel", bbPauseChannel);
	rtSym("ResumeChannel%channel", bbResumeChannel);
	rtSym("ChannelPitch%channel%pitch", bbChannelPitch);
	rtSym("ChannelVolume%channel#volume", bbChannelVolume);
	rtSym("ChannelPan%channel#pan", bbChannelPan);
	rtSym("%ChannelPlaying%channel", bbChannelPlaying);
	rtSym("%Load3DSound$filename", bbLoad3DSound);
}