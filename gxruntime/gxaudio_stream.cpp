#include "gxaudio_stream.h"

#include "gxaudio.h"
#include "gxsound.h"
#include "gxchannel.h"
#include "gxruntime.h"
#include "std.h"

#include "../openAL/include/dr_wav.h"
#include "../openAL/include/dr_mp3.h"

extern "C" {
    typedef struct stb_vorbis stb_vorbis;

    typedef struct {
        unsigned int sample_rate;
        int channels;
        unsigned int setup_memory_required;
        unsigned int setup_temp_memory_required;
        unsigned int temp_memory_required;
        int max_frame_size;
    } stb_vorbis_info;

    stb_vorbis_info stb_vorbis_get_info(stb_vorbis* f);
    stb_vorbis* stb_vorbis_open_filename(const char* filename, int* error, const void* alloc_buffer);
    int             stb_vorbis_get_samples_short_interleaved(stb_vorbis* f, int channels, short* buffer, int num_shorts);
    void            stb_vorbis_seek_start(stb_vorbis* f);
    void            stb_vorbis_close(stb_vorbis* f);
}

#include <AL/al.h>
#include <AL/alc.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <memory>

extern gxRuntime* gx_runtime;

static std::string streamExt(const std::string& path) {
    std::string ext;
    auto dot = path.rfind('.');
    if (dot != std::string::npos) {
        ext = path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
    }
    return ext;
}

static void streamLog(const std::string& msg) {
    if (gx_runtime) gx_runtime->debugLog(msg.c_str());
}

static std::mutex              s_streamMtx;
static std::vector<gxStreamChannel*> s_streams;

static void registerStream(gxStreamChannel* s) {
    std::lock_guard<std::mutex> lk(s_streamMtx);
    s_streams.push_back(s);
}

static void unregisterStream(gxStreamChannel* s) {
    std::lock_guard<std::mutex> lk(s_streamMtx);
    s_streams.erase(std::remove(s_streams.begin(), s_streams.end(), s),
        s_streams.end());
}

void AsyncSoundHandle::wait() {
    while (state.load() == State::Pending)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

static std::mutex                          s_asyncMtx;
static std::vector<AsyncSoundHandle*>      s_asyncHandles;

namespace {

    struct AsyncJob {
        AsyncSoundHandle* handle;
        gxAudio* audio;
    };

    static std::mutex              s_jobMtx;
    static std::condition_variable s_jobCv;
    static std::vector<AsyncJob>   s_jobQueue;
    static std::atomic<bool>       s_workerRunning{ false };
    static std::thread             s_workerThread;

    static void workerProc() {
        while (true) {
            AsyncJob job{};
            {
                std::unique_lock<std::mutex> lk(s_jobMtx);
                s_jobCv.wait(lk, [] { return !s_jobQueue.empty() || !s_workerRunning; });
                if (!s_workerRunning && s_jobQueue.empty()) break;
                if (s_jobQueue.empty()) continue;
                job = s_jobQueue.front();
                s_jobQueue.erase(s_jobQueue.begin());
            }

            gxSound* snd = job.audio->loadSound(job.handle->filename,
                job.handle->use3d);
            if (snd) {
                job.handle->sound = snd;
                job.handle->state.store(AsyncSoundHandle::State::Ready);
                streamLog(std::format("Async Loaded: {}", job.handle->filename));
            }
            else {
                job.handle->state.store(AsyncSoundHandle::State::Failed);
                streamLog(std::format("Async FAILED: {}", job.handle->filename));
            }
        }
    }

    static void ensureWorker() {
        bool expected = false;
        if (s_workerRunning.compare_exchange_strong(expected, true)) {
            s_workerThread = std::thread(workerProc);
        }
    }

    static void stopWorker() {
        {
            std::lock_guard<std::mutex> lk(s_jobMtx);
            s_workerRunning = false;
        }
        s_jobCv.notify_all();
        if (s_workerThread.joinable()) s_workerThread.join();
    }

}

AsyncSoundHandle* gxAudio_LoadSoundAsync(gxAudio* audio, const std::string& filename, bool use3d) {
    ensureWorker();

    auto* handle = new AsyncSoundHandle();
    handle->filename = filename;
    handle->use3d = use3d;

    {
        std::lock_guard<std::mutex> lk(s_asyncMtx);
        s_asyncHandles.push_back(handle);
    }
    {
        std::lock_guard<std::mutex> lk(s_jobMtx);
        s_jobQueue.push_back({ handle, audio });
    }
    s_jobCv.notify_one();

    streamLog(std::format("Async Queued: {}", filename));
    return handle;
}

void gxAudio_FreeAsyncHandle(AsyncSoundHandle* handle) {
    if (!handle) return;

    handle->wait();

    {
        std::lock_guard<std::mutex> lk(s_asyncMtx);
        s_asyncHandles.erase(std::remove(s_asyncHandles.begin(),
            s_asyncHandles.end(), handle),
            s_asyncHandles.end());
    }
    delete handle;
}

static void asyncShutdown() {
    stopWorker();
    std::lock_guard<std::mutex> lk(s_asyncMtx);
    for (auto* h : s_asyncHandles) delete h;
    s_asyncHandles.clear();
}

gxStreamChannel::gxStreamChannel(const std::string& filename, bool loop) : _filename(filename), _loop(loop)
{
    alGenSources(1, &_src);
    alSourcei(_src, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(_src, AL_ROLLOFF_FACTOR, 0.0f);

    alGenBuffers(NUM_BUFFERS, _bufs);

    if (!openDecoder()) {
        _openFailed = true;
        streamLog(std::format("Stream Failed to open decoder: {}", filename));
        return;
    }

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        if (!fillAndQueue(_bufs[i])) break;
    }

    alSourcePlay(_src);
    streamLog(std::format("Stream Started: {}", filename));

    registerStream(this);
}

gxStreamChannel::~gxStreamChannel() {
    unregisterStream(this);

    alSourceStop(_src);

    ALint queued = 0;
    alGetSourcei(_src, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0) {
        ALuint tmp;
        alSourceUnqueueBuffers(_src, 1, &tmp);
    }

    alDeleteSources(1, &_src);
    alDeleteBuffers(NUM_BUFFERS, _bufs);

    closeDecoder();
    streamLog(std::format("Stream Stopped: {}", _filename));
}

bool gxStreamChannel::openDecoder() {
    std::string ext = streamExt(_filename);

    if (ext == "wav") {
        _fileType = FileType::WAV;
        auto* wav = new drwav();
        if (!drwav_init_file(wav, _filename.c_str(), nullptr)) {
            delete wav; return false;
        }
        _sampleRate = (ALsizei)wav->sampleRate;
        int ch = (int)wav->channels;
        _alFmt = (ch == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        _wavHandle = wav;
    }
    else if (ext == "mp3") {
        _fileType = FileType::MP3;
        auto* mp3 = new drmp3();
        if (!drmp3_init_file(mp3, _filename.c_str(), nullptr)) {
            delete mp3; return false;
        }
        _sampleRate = (ALsizei)mp3->sampleRate;
        int ch = (int)mp3->channels;
        _alFmt = (ch == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        _mp3Handle = mp3;
    }
    else if (ext == "ogg") {
        _fileType = FileType::OGG;
        int error = 0;
        auto* vb = stb_vorbis_open_filename(_filename.c_str(), &error, nullptr);
        if (!vb) return false;
        stb_vorbis_info info = stb_vorbis_get_info(vb);
        _vorbisSampleRate = info.sample_rate;
        _vorbisChannels = info.channels;
        _sampleRate = (ALsizei)info.sample_rate;
        _alFmt = (_vorbisChannels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        _vorbisHandle = vb;
    }
    else {
        streamLog(std::format("Stream Unknown extension: {}", ext));
        return false;
    }
    return true;
}

void gxStreamChannel::closeDecoder() {
    if (_wavHandle) {
        drwav_uninit((drwav*)_wavHandle);
        delete (drwav*)_wavHandle;
        _wavHandle = nullptr;
    }
    if (_mp3Handle) {
        drmp3_uninit((drmp3*)_mp3Handle);
        delete (drmp3*)_mp3Handle;
        _mp3Handle = nullptr;
    }
    if (_vorbisHandle) {
        stb_vorbis_close((stb_vorbis*)_vorbisHandle);
        _vorbisHandle = nullptr;
    }
}

int gxStreamChannel::decode(short* dst, int frames) {
    switch (_fileType) {
    case FileType::WAV: {
        auto* wav = (drwav*)_wavHandle;
        int ch = (_alFmt == AL_FORMAT_STEREO16) ? 2 : 1;
        drwav_uint64 got = drwav_read_pcm_frames_s16(wav, (drwav_uint64)frames, dst);
        return (int)got;
    }
    case FileType::MP3: {
        auto* mp3 = (drmp3*)_mp3Handle;
        int ch = (_alFmt == AL_FORMAT_STEREO16) ? 2 : 1;
        drmp3_uint64 got = drmp3_read_pcm_frames_s16(mp3, (drmp3_uint64)frames, dst);
        return (int)got;
    }
    case FileType::OGG: {
        auto* vb = (stb_vorbis*)_vorbisHandle;
        int shorts_wanted = frames * _vorbisChannels;
        int got = stb_vorbis_get_samples_short_interleaved(vb, _vorbisChannels, dst, shorts_wanted);
        return got;
    }
    default:
        return 0;
    }
}

void gxStreamChannel::rewindDecoder() {
    switch (_fileType) {
    case FileType::WAV:
        drwav_seek_to_pcm_frame((drwav*)_wavHandle, 0);
        break;
    case FileType::MP3:
        drmp3_seek_to_pcm_frame((drmp3*)_mp3Handle, 0);
        break;
    case FileType::OGG:
        stb_vorbis_seek_start((stb_vorbis*)_vorbisHandle);
        break;
    default:
        break;
    }
}

bool gxStreamChannel::fillAndQueue(ALuint buf) {
    if (_finished) return false;

    int ch = (_alFmt == AL_FORMAT_STEREO16) ? 2 : 1;
    int frames = BUFFER_BYTES / (sizeof(short) * ch);

    std::vector<short> pcm((size_t)(frames * ch));
    int got = decode(pcm.data(), frames);

    if (got == 0) {
        if (_loop) {
            rewindDecoder();
            got = decode(pcm.data(), frames);
            if (got == 0) return false;
        }
        else {
            _finished = true;
            return false;
        }
    }

    // handle a partial buffer loop wrap if we decoded less than a full buffer
    if (_loop && got < frames) {
        rewindDecoder();
        int remaining = frames - got;
        int extra = decode(pcm.data() + got * ch, remaining);
        got += extra;
    }

    alBufferData(buf, _alFmt,
        pcm.data(),
        (ALsizei)(got * ch * (int)sizeof(short)),
        _sampleRate);
    alSourceQueueBuffers(_src, 1, &buf);
    return true;
}

void gxStreamChannel::update() {
    if (_openFailed) return;

    ALint processed = 0;
    alGetSourcei(_src, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buf;
        alSourceUnqueueBuffers(_src, 1, &buf);
        fillAndQueue(buf);
    }

    // stop buffer underruns from stalling source
    if (!_finished) {
        ALint state = AL_STOPPED;
        alGetSourcei(_src, AL_SOURCE_STATE, &state);
        ALint queued = 0;
        alGetSourcei(_src, AL_BUFFERS_QUEUED, &queued);
        if (state == AL_STOPPED && queued > 0) {
            streamLog("Stream Buffer underrun, restarting source");
            alSourcePlay(_src);
        }
    }
}

void gxStreamChannel::stop() {
    alSourceStop(_src);
    _finished = true;
}

void gxStreamChannel::setPaused(bool paused) {
    if (paused) alSourcePause(_src);
    else        alSourcePlay(_src);
}

void gxStreamChannel::setPitch(int pitch) {
    float ratio = (float)pitch / (float)(_sampleRate > 0 ? _sampleRate : 44100);
    if (ratio <= 0.0f) ratio = 0.001f;
    alSourcef(_src, AL_PITCH, ratio);
}

void gxStreamChannel::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    alSourcef(_src, AL_GAIN, volume);
}

void gxStreamChannel::setPan(float pan) {
    alSourcei(_src, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(_src, AL_ROLLOFF_FACTOR, 0.0f);
    alSource3f(_src, AL_POSITION, pan, 0.0f, 0.0f);
}

void gxStreamChannel::set3d(const float pos[3], const float vel[3]) {
    alSourcei(_src, AL_SOURCE_RELATIVE, AL_FALSE);
    alSourcef(_src, AL_ROLLOFF_FACTOR, 1.0f);
    alSource3f(_src, AL_POSITION, pos[0], pos[1], pos[2]);
    alSource3f(_src, AL_VELOCITY, vel[0], vel[1], vel[2]);
}

bool gxStreamChannel::isPlaying() {
    ALint state = AL_STOPPED;
    alGetSourcei(_src, AL_SOURCE_STATE, &state);
    return (state == AL_PLAYING || state == AL_PAUSED) && !_finished;
}

gxStreamChannel* gxAudio_OpenStream(const std::string& filename, bool loop) {
    auto* sc = new gxStreamChannel(filename, loop);
    if (sc->openFailed()) {
        delete sc;
        return nullptr;
    }
    return sc;
}

void gxAudio_UpdateStreams() {
    std::lock_guard<std::mutex> lk(s_streamMtx);
    for (auto* s : s_streams) s->update();
}

void gxAudioStream_Shutdown() {
    asyncShutdown();
    std::lock_guard<std::mutex> lk(s_streamMtx);
    s_streams.clear();
}