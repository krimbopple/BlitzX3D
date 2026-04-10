#ifndef GXSOUND_H
#define GXSOUND_H

#include "gxchannel.h"

class gxAudio;

class gxSound {
public:
	gxAudio* audio;

	gxSound(gxAudio* audio, unsigned int alBuffer, int freq);
	~gxSound();

private:
	unsigned int alBuffer;
	bool    looping;
	int     def_freq; // hz
	float   def_vol;
	float   def_pan; // -1 to +1
	float   pos[3], vel[3];

	/***** GX INTERFACE *****/
public:
	//actions
	gxChannel* play();
	gxChannel* play3d(const float pos[3], const float vel[3]);

	//modifiers
	void setLoop(bool loop);
	void setPitch(int hertz);
	void setVolume(float volume);
	void setPan(float pan);

	unsigned int getBuffer() const { return alBuffer; }
	bool   isLooping()       const { return looping; }
	int    getFreq()         const { return def_freq; }
	float  getVol()          const { return def_vol; }
	float  getPan()          const { return def_pan; }
};

#endif