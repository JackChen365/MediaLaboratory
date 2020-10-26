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
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/frame.h>
    #include <libavutil/mem.h>
    #include <libavutil/time.h>
    #include <libavcodec/avcodec.h>
}

#define VIDEO_TAG "VideoPlayerNative"
#define  VIDEO_LOG_I(...)  __android_log_print(ANDROID_LOG_DEBUG, VIDEO_TAG, __VA_ARGS__)
#define  VIDEO_LOG_E(...)  __android_log_print(ANDROID_LOG_ERROR, VIDEO_TAG, __VA_ARGS__)

#define DEFAULT_SEEK_SHORT_TIME 5000

#define INVALID_STREAM_INDEX -1
#define MAX_QUEUE_SIZE 1

#define PLAY_STATE_IDLE 0
#define PLAY_STATE_PLAYING 1
#define PLAY_STATE_PAUSED 2
#define PLAY_STATE_STOPPED 3

#define AV_SYNC_THRESHOLD 1000

class VideoPlayerAudioEngine;
class VideoPlayerVideoEngine;

/**
 * Video Player class.
 * We use the class: VideoPlayerAudioEngine and the class: VideoPlayerVideoEngine to help us manager all the audio and video things.
 */
class VideoPlayer {
    /**
     * Clear the buffered queue.
     */
    void clearBufferQueue();

public:
    //The audio engine implement by OpenSL ES.
    VideoPlayerAudioEngine *audioEngine = NULL;
    //The video engine use the Surface to render the image.
    VideoPlayerVideoEngine *videoEngine = NULL;

    //The FFMPEG decode field.
    AVFormatContext *avFormatContext = NULL;
    AVCodecContext *audioCodecContext = NULL;
    AVCodecContext *videoCodecContext = NULL;
    AVPacket *avPacket = NULL;
    AVFrame *avFrame = NULL;
    SwsContext* swsContext = NULL;
    SwrContext *swrContext = NULL;

    int64_t duration = 0;

    uint32_t videoStreamIndex = INVALID_STREAM_INDEX;
    AVRational* videoTimeBase=NULL;
    uint8_t *videoBuffer=NULL;
    uint32_t videoWidth = 0;
    uint32_t videoHeight = 0;

    uint32_t audioStreamIndex = INVALID_STREAM_INDEX;
    AVRational* audioTimeBase = NULL;
    uint32_t audioSampleRate = 0;
    uint32_t audioChannels = 0;
    uint8_t* audioBuffer=NULL;
    int channelNumber = 0;

    int64_t currentPlayTime=0;
    int64_t seekTimeStamp=0;

    //The mutex resources and condition variables.
    std::atomic_int playerState = PLAY_STATE_IDLE;
    std::atomic_bool isBeginSeeking=false;
    std::mutex* mutex=NULL;
    std::condition_variable* audioConsumeVar=NULL;
    std::condition_variable* videoConsumeVar=NULL;
    std::condition_variable* playCondition=NULL;
    std::condition_variable* produceVar=NULL;

    //The queue for caching frame data.
    std::deque<AVFrame*>* audioQueue=NULL;
    std::deque<AVFrame*>* videoQueue=NULL;

    //The player file
    char *filePath=NULL;

    VideoPlayer();
    ~VideoPlayer();
    /**
     * Read a audio frame from decoder.
     * @param pcmBuffer
     * @param bufferSize
     */
    void startReadFrame();
    /**
     * Prepare all the player resources, and ready to play.
     * For a player before it releases the resources. It only needs to be prepared for once.
     * @return if return true means prepare success other means failed.
     */
    bool prepare();

    void prepareSurface(JNIEnv *env,jobject surface);

    void surfaceDestroy();

    /**
     * Initial the data source.
     * @param filePath
     */
    void setDataSource(const char *filePath);
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
     * Stop play and releaseResources the resources.
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
     * Release all the resources.
     */
    void release();

    void rewind();

    void fastForward();

    /**
     * Return the video width.
     * @return
     */
    int32_t getWidth();

    /**
     * Return the video height.
     * @return
     */
    int32_t getHeight();
    /**
     * Return the total playback duration. It is millisecond.
     * @return
     */
    int64_t getDuration();
};
//------------------------------------------------------------------------------
// About audio player engine.
//------------------------------------------------------------------------------

/**
 * The abstraction of the audio player engine.
 * When use FFmpeg get the PCM data. We use this engine to play audio.
 *
 */
class VideoPlayerAudioEngine {
public:
    VideoPlayer* videoPlayer=NULL;
    SLObjectItf engineObject=NULL;
    SLEngineItf engineEngine = NULL;
    SLObjectItf outputMixObject = NULL;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
    SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    SLObjectItf playerObject=NULL;
    SLPlayItf playerInterface=NULL;
    SLAndroidSimpleBufferQueueItf  slBufferQueueItf=NULL;

    VideoPlayerAudioEngine();
    ~VideoPlayerAudioEngine();
    static void playerQueueCallBack(SLAndroidSimpleBufferQueueItf slBufferQueueItf, void* context);

    void prepare(uint32_t rate,uint32_t channels);

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
     * Stop the player and releaseResources all the resources.
     */
    void stop();
};

//------------------------------------------------------------------------------
// About video player engine.
//------------------------------------------------------------------------------
class VideoPlayerVideoEngine{
private:
public:
    VideoPlayer* videoPlayer=NULL;
    ANativeWindow *nativeWindow=NULL;
    /**
     * Because our video engine will destroy when it hides.
     * So This engine has its own play state which is different from the ViewPlayer's play state.
     */
    std::atomic_int playerState = PLAY_STATE_IDLE;

    VideoPlayerVideoEngine();
    ~VideoPlayerVideoEngine();

    void prepare(JNIEnv *env,jobject &surface,int32_t width,int32_t height);

    void run();

    void start();

    void pause();

    void resume();

    void stop();
};

#endif //ANDROIDFFMPEGSAMPLE_VIDEOPLAYER_H
