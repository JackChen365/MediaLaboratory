//
// Created by Jack Chen on 10/17/2020.
//

#ifndef ANDROIDFFMPEGSAMPLE_AUDIOPLAYER_H
#define ANDROIDFFMPEGSAMPLE_AUDIOPLAYER_H

#include <chrono>
#include <thread>
#include <android/log.h>
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

#define AUDIO_TAG "AudioPlayerNative"
#define  AUDIO_LOG_I(...)  __android_log_print(ANDROID_LOG_DEBUG, AUDIO_TAG, __VA_ARGS__)
#define  AUDIO_LOG_E(...)  __android_log_print(ANDROID_LOG_ERROR, AUDIO_TAG, __VA_ARGS__)

#define DEFAULT_SEEK_SHORT_TIME 5000

class AudioPlayerEngine;

class AudioPlayer {
private:
    AudioPlayerEngine *audioPlayerEngine = NULL;
    AVFormatContext *avFormatContext = NULL;
    AVCodecContext *avCodecContext = NULL;
    AVPacket *avPacket = NULL;
    AVFrame *avFrame = NULL;
    SwrContext *swrContext = NULL;
    uint8_t *outBuffer=NULL;
    int audioStreamIndex=-1;
    int channelNumber = 0;

    int64_t duration=0;
    AVRational* timeBase=NULL;
    int64_t startTime=0;
    int64_t currentPlayTime=0;
    int64_t seekTime=0;

    std::atomic_bool isBeginSeeking=false;
public:
    AudioPlayer();
    ~AudioPlayer();
    /**
     * Load the audio file.
     * If load succeeds. return true otherwise false.
     * @param filePath
     * @return
     */
    bool loadFile(const char* filePath);
    /**
     * Read a frame from decoder.
     * @param pcmBuffer
     * @param bufferSize
     */
    void readFrame(void **pcmBuffer,size_t *bufferSize);
    /**
     * Prepare all the resources, and ready to play.
     */
    void prepare();
    /**
     * Initial the data source.
     * @param dataSource
     * @return
     */
    void setDataSource(const char* dataSource);
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
     * Move for a short distance.
     */
    void fastForward();

    /**
     * Move back for a short distance.
     */
    void backward();

    /**
     * Release all the resources.
     */
    void release();
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
class AudioPlayerEngine {
private:
    AudioPlayer* audioPlayer;
    void *buffer;
    size_t bufferSize =0;
public:
    AudioPlayerEngine();
    virtual ~AudioPlayerEngine();

    AudioPlayer *getAudioPlayer() const;

    void setAudioPlayer(AudioPlayer *audioPlayer);

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
class OpenSLAudioPlayerEngine : public AudioPlayerEngine{
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
    OpenSLAudioPlayerEngine();
    virtual ~OpenSLAudioPlayerEngine();

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


#endif //ANDROIDFFMPEGSAMPLE_AUDIOPLAYER_H
