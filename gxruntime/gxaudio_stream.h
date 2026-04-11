/*
This file contains modified code derived from the Blitz3D engine.

Original code:
Copyright (c) 2013 Blitz Research Ltd
Licensed under the zlib/libpng License.

Modifications and additional code:
Copyright (c) 2026 krimbopple

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented.
2. Altered source versions must be plainly marked as such.
3. This notice may not be removed or altered from any source distribution.

This software is provided "as-is", without any express or implied warranty.
*/

#ifndef GXAUDIO_STREAM_H
#define GXAUDIO_STREAM_H

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

#include "../openAL/include/AL/al.h"
#include "../openAL/include/AL/alc.h"

class gxSound;
class gxAudio;
#include "gxchannel.h"

struct AsyncSoundHandle {
    enum class State { Pending, Ready, Failed };

    std::atomic<State>  state{ State::Pending };
    gxSound* sound{ nullptr };
    std::string         filename;
    bool                use3d{ false };

    void wait();

    bool isReady()  const { return state.load() == State::Ready; }
    bool isFailed() const { return state.load() == State::Failed; }
};

class gxStreamChannel : public gxChannel {
public:
    static constexpr int  NUM_BUFFERS = 3;
    static constexpr int  BUFFER_BYTES = 65536;   // 64 KB / ~370ms @ 44.1 kHz mono16

    enum class FileType { WAV, MP3, OGG, Unknown };

    gxStreamChannel(const std::string& filename, bool loop);
    ~gxStreamChannel() override;

    void update();

    void stop()                                 override;
    void setPaused(bool paused)                 override;
    void setPitch(int pitch)                    override;
    void setVolume(float volume)                override;
    void setPan(float pan)                      override;
    void set3d(const float pos[3], const float vel[3])              override;
    bool isPlaying()                            override;
    bool openFailed() const { return _openFailed; }

private:
    ALuint  _src{ AL_NONE };
    ALuint  _bufs[NUM_BUFFERS]{};
    ALenum  _alFmt{ AL_FORMAT_MONO16 };
    ALsizei _sampleRate{ 44100 };

    FileType     _fileType{ FileType::Unknown };
    std::string  _filename;
    bool         _loop{ false };
    bool         _openFailed{ false };
    bool         _finished{ false };

    void* _wavHandle{ nullptr };
    void* _mp3Handle{ nullptr };
    void* _vorbisHandle{ nullptr };
    int   _vorbisChannels{ 1 };
    int   _vorbisSampleRate{ 44100 };

    bool openDecoder();
    void closeDecoder();
    int  decode(short* dst, int frames);
    bool fillAndQueue(ALuint buf);
    void rewindDecoder();
};

AsyncSoundHandle* gxAudio_LoadSoundAsync(gxAudio* audio, const std::string& filename, bool use3d);
void gxAudio_FreeAsyncHandle(AsyncSoundHandle* handle);
gxStreamChannel* gxAudio_OpenStream(const std::string& filename, bool loop);
void gxAudio_UpdateStreams();
void gxAudioStream_Shutdown();

#endif