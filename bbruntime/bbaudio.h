#ifndef BBAUDIO_H
#define BBAUDIO_H

#include "bbsys.h"
#include "../gxruntime/gxaudio.h"
#include "../gxruntime/gxaudio_stream.h"

extern gxAudio* gx_audio;

gxSound* bbLoadSound(BBStr* file);
void		 bbFreeSound(gxSound* sound);
gxChannel* bbPlaySound(gxSound* sound);
gxChannel* bbPlaySound3D(gxSound* sound, float x, float y, float z, float vx, float vy, float vz);
void		 bbLoopSound(gxSound* sound);
void		 bbSoundPitch(gxSound* sound, int pitch);
void		 bbSoundVolume(gxSound* sound, float volume);
void		 bbSoundPan(gxSound* sound, float pan);

AsyncSoundHandle* bbLoadSoundAsync(BBStr* filename);
AsyncSoundHandle* bbLoad3DSoundAsync(BBStr* filename);
int               bbAsyncSoundReady(AsyncSoundHandle* handle);
int               bbAsyncSoundFailed(AsyncSoundHandle* handle);
gxSound*          bbAsyncSoundGet(AsyncSoundHandle* handle);
void              bbFreeAsyncSound(AsyncSoundHandle* handle);

gxChannel* bbOpenStreamSound(BBStr* filename, int loop);
gxChannel* bbStreamSound(BBStr* filename, float volume, int loop);
void       bbFreeStreamSound(gxChannel* channel);
void       bbStopStream(gxChannel* channel);
void       bbSetStreamVolume(gxChannel* channel, float volume);

gxChannel* bbPlayMusic(BBStr* s, int mode);
gxChannel* bbPlayCDTrack(int track, int mode);
void		 bbStopChannel(gxChannel* channel);
void		 bbPauseChannel(gxChannel* channel);
void		 bbResumeChannel(gxChannel* channel);
void		 bbChannelPitch(gxChannel* channel, int pitch);
void		 bbChannelVolume(gxChannel* channel, float volume);
void		 bbChannelPan(gxChannel* channel, float pan);
int			 bbChannelPlaying(gxChannel* channel);

#endif
