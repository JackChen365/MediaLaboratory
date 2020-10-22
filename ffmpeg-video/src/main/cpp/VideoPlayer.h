//
// Created by Jack Chen on 10/18/2020.
//

#ifndef ANDROIDFFMPEGSAMPLE_VIDEOPLAYER_H
#define ANDROIDFFMPEGSAMPLE_VIDEOPLAYER_H

#include <chrono>
#include <thread>
#include <queue>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include "libavutil/time.h"
#include <libavcodec/avcodec.h>
}

#define VIDEO_TAG "VideoPlayerNative"
#define  VIDEO_LOG_I(...)  __android_log_print(ANDROID_LOG_DEBUG, VIDEO_TAG, __VA_ARGS__)
#define  VIDEO_LOG_E(...)  __android_log_print(ANDROID_LOG_ERROR, VIDEO_TAG, __VA_ARGS__)

class VideoPlayerAudioEngine;
class VideoPlayer {
private:
    VideoPlayerAudioEngine *audioPlayerEngine = NULL;

    AVFormatContext *avFormatContext = NULL;
    AVCodecContext *avCodecContext = NULL;
    AVPacket *avPacket = NULL;
    AVFrame *avFrame = NULL;
    SwsContext* swsContext;
    SwrContext *swrContext = NULL;
    uint8_t *outBuffer=NULL;

    uint32_t videoStreamIndex = -1;
    AVRational* videoTimeBase=NULL;
    AVRational* videoAverageFrameRate = 0;
    uint8_t* videoFrameData=NULL;
    uint32_t videoWidth = 0;
    uint32_t videoHeight = 0;
    uint64_t videoDuration = 0;

    uint32_t audioStreamIndex = -1;
    AVRational* audioTimeBase = NULL;
    uint64_t audioDuration = 0;
    uint32_t audioSampleRate = 0;
    uint32_t audioChannels = 0;
    uint64_t audioStartTime;
    int channelNumber = 0;

    int64_t startTime=0;
    int64_t currentPlayTime=0;
    int64_t seekTime=0;

    std::mutex* mutex=NULL;
    std::condition_variable* consumeVar=NULL;
    std::condition_variable* produceVar=NULL;
    std::queue<AVFrame>* videoQueue=NULL;
    std::queue<std::pair<uint8_t *,int>>* audioQueue=NULL;
    std::atomic_bool isBeginSeeking=false;

    void release();
public:
    VideoPlayer();
    ~VideoPlayer();
    /**
     * Load the audio file.
     * If load succeeds. return true otherwise false.
     * @param filePath
     * @return
     */
    bool loadFile(const char* filePath);
    /**
     * Read a audio frame from decoder.
     * @param pcmBuffer
     * @param bufferSize
     */
    void startReadFrame();
    /**
     * Start play the audio
     * @param filePath
     * @return
     */
    void start();
    /**
     * Temporarily pause the player. Call the method: #resume continues to play.
     * @param filePath
     * @return
     */
    void pause();
    /**
     * If you call the method: pause and want to continue to play. Call this method.
     * @return
     */
    void resume();
    /**
     * Stop play and release the resources.
     * @return
     */
    void stop();

    /**
     * Seek to a specific time stamp
     * @param timeStamp
     */
    void seekTo(int64_t timeStamp);
    /**
     * Return the current play time.
     * @return
     */
    int64_t getCurrentPlayTime();

    /**
     * Return the total playback duration. It is millisecond.
     * @return
     */
    int64_t getDuration();
};


/**
 * The abstraction of the audio player engine.
 * When use FFmpeg get the PCM data. We use this engine to play audio.
 *
 */
class VideoPlayerAudioEngine {
private:
    VideoPlayer* videoPlayer;
    void *buffer;
    size_t bufferSize =0;
public:
    VideoPlayerAudioEngine();
    virtual ~VideoPlayerAudioEngine();

    VideoPlayer *getVideoPlayer() const;

    void setVideoPlayer(VideoPlayer *videoPlayer);

    void *getBuffer() const;

    void setBuffer(void *buffer);

    size_t getBufferSize() const;

    void setBufferSize(size_t bufferSize);

    virtual void createEngine(unsigned int rate,unsigned int channels);

    virtual void start();

    virtual void pause();

    virtual void resume();

    virtual void stop();
};

/**
 * The implementation using the OpenSL.
 */
class VideoPlayerOpenSLAudioEngine : public VideoPlayerAudioEngine{
private:
    SLObjectItf engineObject=NULL;
    SLEngineItf engineEngine = NULL;
    SLObjectItf outputMixObject = NULL;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
    SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    SLObjectItf playerObject=NULL;
    SLPlayItf playerInterface=NULL;
    SLAndroidSimpleBufferQueueItf  slBufferQueueItf=NULL;
public:
    VideoPlayerOpenSLAudioEngine();
    virtual ~VideoPlayerOpenSLAudioEngine();

    static void playerQueueCallBack(SLAndroidSimpleBufferQueueItf slBufferQueueItf, void* context);

    void createEngine(unsigned int rate,unsigned int channels);

    /**
     * The player start play the audio.
     */
    void start();
    /**
     * The player paused temporarily.
     */
    void pause();
    /**
     * Continue to playing.
     */
    void resume();
    /**
     * Stop the player and release all the resources.
     */
    void stop();
};

template <typename T>
class VideoPlayerVideoEngine{
public:
    VideoPlayer* videoPlayer=NULL;
    VideoPlayerVideoEngine();
    virtual ~VideoPlayerVideoEngine();

    VideoPlayer *getVideoPlayer() const;

    void setVideoPlayer(VideoPlayer *videoPlayer);

    virtual void createEngine(JNIEnv *env,T surface);

    virtual void draw();
};

class VideoPlayerSurfaceEngine:public VideoPlayerVideoEngine<jobject>{
private:
    ANativeWindow* nativeWindow;
    ANativeWindow_Buffer windowBuffer;
public:
    VideoPlayerSurfaceEngine();
    virtual ~VideoPlayerSurfaceEngine();
    void createEngine(JNIEnv *env,jobject surface);
    void draw();
};

#endif //ANDROIDFFMPEGSAMPLE_VIDEOPLAYER_H
