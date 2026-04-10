#include "std.h"
#include "gxsound.h"
#include "gxaudio.h"

gxSound::gxSound(gxAudio* a, unsigned int buf, int freq)
	: audio(a), alBuffer(buf), looping(false),
	def_freq(freq), def_vol(1.0f), def_pan(0.0f) {
	memset(pos, 0, sizeof(pos));
	memset(vel, 0, sizeof(vel));
}

gxSound::~gxSound() {
	alDeleteBuffers(1, &alBuffer);
}

gxChannel* gxSound::play() {
	return audio->play(this);
}

gxChannel* gxSound::play3d(const float p[3], const float v[3]) {
	return audio->play3d(this, p, v);
}

void gxSound::setLoop(bool loop) {
	looping = loop;
}

void gxSound::setPitch(int hertz) {
	def_freq = hertz;
}

void gxSound::setVolume(float volume) {
	// keep within 0,1
	def_vol = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
}

void gxSound::setPan(float pan) {
	// keep within -1,+1
	def_pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
}