#ifndef GXAUDIO_H
#define GXAUDIO_H

#include <string>
#include <set>
#include <map>
#include <vector>

#include "gxsound.h"

class gxRuntime;

bool gxAudio_Init();

class gxAudio {
public:
	gxRuntime* runtime;

	gxAudio(gxRuntime* runtime);
	~gxAudio();

	gxChannel* play(gxSound* sound);
	gxChannel* play3d(gxSound* sound, const float pos[3], const float vel[3]);

	void pause();
	void resume();

private:
	bool masterPaused;

	/***** GX INTERFACE *****/
public:
	enum {
		CD_MODE_ONCE = 1, CD_MODE_LOOP, CD_MODE_ALL
	};

	gxSound* loadSound(const std::string& filename, bool use3d);
	gxSound* verifySound(gxSound* sound);
	void     freeSound(gxSound* sound);

	void setPaused(bool paused);   // master pause
	void setVolume(float volume);  // master volume

	void set3dOptions(float rolloff, float doppler, float distance);
	void set3dListener(const float pos[3], const float vel[3], const float forward[3], const float up[3]);

	gxChannel* playCDTrack(int track, int mode);
	gxChannel* playFile(const std::string& filename, bool use3d, int mode);
};

#endif