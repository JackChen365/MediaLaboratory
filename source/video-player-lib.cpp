#include <jni.h>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <queue>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
//added by ws for AVfilter start
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
//added by ws for AVfilter end
};


#define  LOG_TAG    "AudioPlayer"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,FORMAT,##__VA_ARGS__);
//added by ws for AVfilter start

#define MAX_VIDEO_QUEUE_SIZE 5
#define MAX_QUEUE_SIZE 1

#define PLAY_STATE_IDLE 0
#define PLAY_STATE_PLAYING 1
#define PLAY_STATE_PAUSED 2
#define PLAY_STATE_STOPPED 3

std::atomic_int playerState = PLAY_STATE_IDLE;

#define AV_SYNC_THRESHOLD 1000

//Open SL ES
SLObjectItf engineObject=NULL;//用SLObjectItf声明引擎接口对象
SLEngineItf engineEngine = NULL;//声明具体的引擎对象
SLObjectItf outputMixObject = NULL;//用SLObjectItf创建混音器接口对象
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;////具体的混音器对象实例
SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;//默认情况
SLObjectItf audioplayer=NULL;//用SLObjectItf声明播放器接口对象
SLPlayItf  slPlayItf=NULL;//播放器接口
SLAndroidSimpleBufferQueueItf  slBufferQueueItf=NULL;//缓冲区队列接口

//All the fields related to FFMPEG.
AVFormatContext *pFormatCtx = NULL;
AVCodecContext *audioCodecCtx = NULL;
AVCodecContext *videoCodecCtx = NULL;
AVPacket *packet = NULL;
AVFrame *frame = NULL;
SwsContext *swsContext;
SwrContext *swrContext = NULL;
uint8_t *out_buffer=NULL;
uint8_t* frameData;

int64_t currentPlayTime=0;

int32_t width;
int32_t height;
int video_stream_idx=-1;
AVRational* videoTimeBase=NULL;

int audio_stream_idx=-1;
int out_channer_nb = 0;

int64_t duration=0;
AVRational* audioTimeBase=NULL;
int64_t seekTime=0;
ANativeWindow *window = 0;

std::atomic_bool isBeginSeeking=false;
std::mutex mutex;
std::condition_variable produce_var;
std::condition_variable audioConsumeVar;
std::condition_variable videoConsumeVar;
std::condition_variable pauseCondition;
std::condition_variable releaseCondition;
std::deque<AVFrame*> audioQueue;
std::deque<AVFrame*> videoQueue;
std::atomic_int counter=0;

//将pcm数据添加到缓冲区中
void getQueueCallBack(SLAndroidSimpleBufferQueueItf  slBufferQueueItf, void* context){
    AVFrame * frame=NULL;
    std::unique_lock l(mutex);
    if(audioQueue.empty()){
        audioConsumeVar.wait(l);
    }
    if(playerState == PLAY_STATE_STOPPED){
        return;
    }
    frame = audioQueue.front();
    audioQueue.pop_front();
    if(0 == audioQueue.size()){
        produce_var.notify_one();
    }
    currentPlayTime = frame->pts;
    uint32_t audioPosition;
    (*slPlayItf)->GetPosition(slPlayItf,&audioPosition);
    LOGE("Audio duration:%lld currentPlayTime:%lld pts:%lld",audioPosition,currentPlayTime,frame->pts)
    if(0 > currentPlayTime){
        LOGE("currentPlayTime is negative!")
    }
    l.unlock();
    swr_convert(swrContext, &out_buffer, 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
    uint64_t bufferSize = av_samples_get_buffer_size(NULL, out_channer_nb, frame->nb_samples,AV_SAMPLE_FMT_S16, 1);
    if(out_buffer!=NULL&&bufferSize!=0){
        (*slBufferQueueItf)->Enqueue(slBufferQueueItf,out_buffer,bufferSize);
    }
    av_frame_free(&frame);
}

//创建引擎
void createEngine(){
    slCreateEngine(&engineObject,0,NULL,0,NULL,NULL);//创建引擎
    (*engineObject)->Realize(engineObject,SL_BOOLEAN_FALSE);//实现engineObject接口对象
    (*engineObject)->GetInterface(engineObject,SL_IID_ENGINE,&engineEngine);//通过引擎调用接口初始化SLEngineItf
}

//创建混音器
void createMixVolume(){
    (*engineEngine)->CreateOutputMix(engineEngine,&outputMixObject,0,0,0);//用引擎对象创建混音器接口对象
    (*outputMixObject)->Realize(outputMixObject,SL_BOOLEAN_FALSE);//实现混音器接口对象
    SLresult   sLresult = (*outputMixObject)->GetInterface(outputMixObject,SL_IID_ENVIRONMENTALREVERB,&outputMixEnvironmentalReverb);//利用混音器实例对象接口初始化具体的混音器对象
    //设置
    if (SL_RESULT_SUCCESS == sLresult) {
        (*outputMixEnvironmentalReverb)->
                SetEnvironmentalReverbProperties(outputMixEnvironmentalReverb, &settings);
    }
}

//创建播放器
void createPlayer(unsigned int rate,unsigned int channels){
    SLDataLocator_AndroidBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};
    /**
    typedef struct SLDataFormat_PCM_ {
        SLuint32 		formatType;  pcm
        SLuint32 		numChannels;  通道数
        SLuint32 		samplesPerSec;  采样率
        SLuint32 		bitsPerSample;  采样位数
        SLuint32 		containerSize;  包含位数
        SLuint32 		channelMask;     立体声
        SLuint32		endianness;    end标志位
    } SLDataFormat_PCM;
     */
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM,channels,rate*1000
            ,SL_PCMSAMPLEFORMAT_FIXED_16
            ,SL_PCMSAMPLEFORMAT_FIXED_16
            ,SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT,SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource dataSource = {&android_queue,&pcm};
    SLDataLocator_OutputMix slDataLocator_outputMix={SL_DATALOCATOR_OUTPUTMIX,outputMixObject};
    SLDataSink slDataSink = {&slDataLocator_outputMix,NULL};
    const SLInterfaceID ids[3]={SL_IID_BUFFERQUEUE,SL_IID_EFFECTSEND,SL_IID_VOLUME};
    const SLboolean req[3]={SL_BOOLEAN_FALSE,SL_BOOLEAN_FALSE,SL_BOOLEAN_FALSE};

    (*engineEngine)->CreateAudioPlayer(engineEngine,&audioplayer,&dataSource,&slDataSink,3,ids,req);
    (*audioplayer)->Realize(audioplayer,SL_BOOLEAN_FALSE);
    (*audioplayer)->GetInterface(audioplayer,SL_IID_PLAY,&slPlayItf);//初始化播放器
    //注册缓冲区,通过缓冲区里面 的数据进行播放
    (*audioplayer)->GetInterface(audioplayer,SL_IID_BUFFERQUEUE,&slBufferQueueItf);
    //设置回调接口
    (*slBufferQueueItf)->RegisterCallback(slBufferQueueItf,getQueueCallBack,NULL);
}

void renderSurface(){
    AVFrame * frame=NULL;
    while(playerState == PLAY_STATE_PLAYING||playerState==PLAY_STATE_PAUSED){
        std::unique_lock l(mutex);
        if(videoQueue.empty()){
            videoConsumeVar.wait(l);
        }
        if(playerState == PLAY_STATE_STOPPED){
            break;
        }
        if(playerState == PLAY_STATE_PAUSED){
            videoConsumeVar.wait(l);
        }
        frame = videoQueue.front();
        videoQueue.pop_front();
        if(videoQueue.empty()){
            produce_var.notify_one();
        }
        l.unlock();
        int64_t audioPlayTime=(int64_t)(currentPlayTime * av_q2d(*audioTimeBase) * videoTimeBase->den);
        int64_t delta = frame->pts - audioPlayTime;
        int64_t deltaMilliseconds = delta * av_q2d(*videoTimeBase) * 1000;
        if(AV_SYNC_THRESHOLD > abs(deltaMilliseconds)){
            if(0 < deltaMilliseconds){
                std::this_thread::sleep_for(std::chrono::milliseconds (deltaMilliseconds));
            }
        } else {
            //skip
            LOGE("renderSurface skip:%lld",deltaMilliseconds)
            continue;
        }
        LOGE("renderSurface:%lld dts:%lld time:%lld",audioPlayTime,frame->pts,delta)
        ANativeWindow_Buffer window_buffer;
        if (ANativeWindow_lock(window, &window_buffer, 0)) {
            return;
        }
        uint8_t* dest[4] = {frameData, NULL, NULL, NULL };
        int dest_linesize[4] = { (int)width * 4, 0, 0, 0 };
        sws_scale(swsContext,frame->data,frame->linesize,0,height,dest,dest_linesize);
        uint8_t *dst = (uint8_t *) window_buffer.bits;
        int dstStride = window_buffer.stride * 4;
        int srcStride = frame->linesize[0]*4;
        for (int i = 0; i < height; ++i) {
            memcpy(dst + i * dstStride, frameData + i * srcStride, srcStride);
        }
        ANativeWindow_unlockAndPost(window);
        av_frame_free(&frame);
    }
    LOGE("The read surface thread is done.")
}

void clearBufferedQueue(){
    auto audioIterator=std::begin(audioQueue);
    while(audioIterator<std::end(audioQueue)){
        av_frame_free(&*audioIterator);
        audioIterator++;
    }
    audioQueue.clear();
    auto videoIterator=std::begin(videoQueue);
    while(videoIterator<std::end(videoQueue)){
        av_frame_free(&*videoIterator);
        videoIterator++;
    }
    videoQueue.clear();
}

void readFrame(){
    LOGE("start read frame:%d",std::this_thread::get_id())
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if(isBeginSeeking){
            std::unique_lock l(mutex);
            isBeginSeeking = false;
            clearBufferedQueue();
            int64_t audioTimeStamp = (int64_t)seekTime/1000.0/av_q2d(*audioTimeBase);
            currentPlayTime = audioTimeStamp;
            LOGE("readFrame seek:%lld audioSize:%d videoSize:%d",audioTimeStamp,audioQueue.size(),videoQueue.size())
            if(av_seek_frame(pFormatCtx,audio_stream_idx,audioTimeStamp,AVSEEK_FLAG_BACKWARD) < 0){
                LOGE("Audio seek failed.");
            }
            avcodec_flush_buffers(audioCodecCtx);
            avcodec_flush_buffers(videoCodecCtx);
            l.unlock();
            //Skip this frame.
            continue;
        }
        if(playerState == PLAY_STATE_STOPPED){
            break;
        }
        if (packet->stream_index != audio_stream_idx&&packet->stream_index != video_stream_idx) {
            av_packet_unref(packet);
            continue;
        }
        if(packet->stream_index == video_stream_idx){
            int ret=avcodec_send_packet(videoCodecCtx, packet);
            if (ret < 0) {
                LOGE("Video Error while sending a packet to the decoder:%s currentTime:%lld", av_err2str(ret),currentPlayTime);
                continue;
            }
            ret = avcodec_receive_frame(videoCodecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                LOGE("Video error while receiving a frame from the decoder:%s", av_err2str(ret));
                continue;
            }
            LOGE("Video Frame %c (%d) pts %lld dts %d key_frame %d [coded_picture_number %d, display_picture_number %d] timeRate:%f",
                 av_get_picture_type_char(frame->pict_type),
                 audioCodecCtx->frame_number,
                 frame->pts,
                 frame->pkt_dts,
                 frame->key_frame,
                 frame->coded_picture_number,
                 frame->display_picture_number,
                 frame->pts*1.0*av_q2d(*audioTimeBase)
            )
            std::unique_lock l(mutex);
            while(playerState == PLAY_STATE_PLAYING&&0 < audioQueue.size()&&videoQueue.size()>=MAX_QUEUE_SIZE){
                LOGE("video produce wait\n")
                produce_var.wait(l);
            }
            counter++;
            AVFrame *dst = av_frame_alloc();
//            if(videoQueue.size()>=MAX_QUEUE_SIZE){
//                videoQueue.pop_front();
//            }
            if (av_frame_ref(dst, frame) >= 0) {
                videoQueue.push_back(dst);
            }
            LOGE("video produce num:%d--------------\n",videoQueue.size());
            videoConsumeVar.notify_one();
            l.unlock();
        } else if(packet->stream_index == audio_stream_idx){
            int ret=avcodec_send_packet(audioCodecCtx, packet);
            if (ret < 0) {
                LOGE("Audio Error while sending a packet to the decoder:%s", av_err2str(ret));
                return;
            }
            ret = avcodec_receive_frame(audioCodecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                LOGE("Audio Error while receiving a frame from the decoder:%s", av_err2str(ret));
                return;
            }
            LOGE("Audio Frame %c (%d) pts %lld dts %d key_frame %d [coded_picture_number %d, display_picture_number %d] timeRate:%f",
                 av_get_picture_type_char(frame->pict_type),
                 audioCodecCtx->frame_number,
                 frame->pts,
                 frame->pkt_dts,
                 frame->key_frame,
                 frame->coded_picture_number,
                 frame->display_picture_number,
                 frame->pts*1.0*av_q2d(*audioTimeBase)
            );
            std::unique_lock l(mutex);
            while(playerState == PLAY_STATE_PLAYING&&0 < videoQueue.size()&&audioQueue.size()>=MAX_QUEUE_SIZE){
                produce_var.wait(l);
            }
            counter++;
            AVFrame *dst = av_frame_alloc();
            if (av_frame_ref(dst, frame) >= 0) {
                audioQueue.push_back(dst);
//                LOGE("audio produce num:%d--------------\n",audioQueue.size());
            }
            audioConsumeVar.notify_one();
            l.unlock();
        }
        //After we processed the frame. If we know the player wants to pause. We are start to wait.
        if(playerState == PLAY_STATE_PAUSED){
            std::unique_lock l(mutex);
            pauseCondition.wait(l);
            l.unlock();
        }
    }
    std::unique_lock l(mutex);
    clearBufferedQueue();
    l.unlock();
    audioConsumeVar.notify_one();
    videoConsumeVar.notify_one();
    releaseCondition.notify_one();
    LOGE("the read thread is done.")
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nLoadFile(JNIEnv *env, jobject thiz,
                                                        jstring file_path) {
    const char *input = env->GetStringUTFChars(file_path, 0);
    av_register_all();
    pFormatCtx = avformat_alloc_context();
    //open
    if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0) {
        LOGE("%s","打开输入视频文件失败");
        return false;
    }
    //获取视频信息
    if(avformat_find_stream_info(pFormatCtx,NULL) < 0){
        LOGE("%s","获取视频信息失败");
        return false;
    }
    AVCodec *pLocalCodec=NULL;
    AVCodecParameters *pLocalCodecParameters=NULL;
    for (int i = 0; i < pFormatCtx->nb_streams; ++i) {
        pLocalCodecParameters = pFormatCtx->streams[i]->codecpar;
        pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
        if (pLocalCodec == NULL) {
            LOGE("ERROR unsupported codec!");
            continue;
        }
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            if(video_stream_idx == -1){
                video_stream_idx = i;
                //获取解码器上下文
                width=pFormatCtx->streams[i]->codec->width;
                height=pFormatCtx->streams[i]->codec->height;
                videoTimeBase = &pFormatCtx->streams[i]->time_base;
                // Set up a codec context for the decoder
                videoCodecCtx = avcodec_alloc_context3(pLocalCodec);
                if (!videoCodecCtx) {
                    LOGE("Couldn't create AVCodecContext\n");
                    return 0;
                }
                if (avcodec_parameters_to_context(videoCodecCtx, pLocalCodecParameters) < 0) {
                    LOGE("Couldn't initialize AVCodecContext\n");
                    return 0;
                }
                if (avcodec_open2(videoCodecCtx, pLocalCodec, NULL) < 0) {
                    LOGE("Couldn't open codec\n");
                    return 0;
                }
            }
        } else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            if(audio_stream_idx == -1){
                audio_stream_idx=i;
                //获取解码器上下文
                duration=pFormatCtx->streams[i]->duration;
                audioTimeBase=&pFormatCtx->streams[i]->time_base;
                LOGE("找到音频id %d audioTimeBase:%ld", pFormatCtx->streams[i]->codec->codec_type,audioTimeBase->den);
                audioCodecCtx = avcodec_alloc_context3(pLocalCodec);
                if (!audioCodecCtx) {
                    LOGE("Couldn't create AVCodecContext\n");
                    return 0;
                }
                if (avcodec_parameters_to_context(audioCodecCtx, pLocalCodecParameters) < 0) {
                    LOGE("Couldn't initialize AVCodecContext\n");
                    return 0;
                }
                if (avcodec_open2(audioCodecCtx, pLocalCodec, NULL) < 0) {
                    LOGE("Couldn't open codec\n");
                    return 0;
                }
            }
        }
        if(video_stream_idx!=-1&&audio_stream_idx!=-1){
            break;
        }
    }
    //获取解码器
    AVCodec *pCodex = avcodec_find_decoder(audioCodecCtx->codec_id);
    //打开解码器
    if (avcodec_open2(audioCodecCtx, pCodex, NULL) < 0) {
    }
    //申请avpakcet，装解码前的数据
    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    //申请avframe，装解码后的数据
    frame = av_frame_alloc();
    //得到SwrContext ，进行重采样，具体参考http://blog.csdn.net/jammg/article/details/52688506
    swrContext = swr_alloc();
    //缓存区
    out_buffer = (uint8_t *) av_malloc(44100 * 2);
//输出的声道布局（立体声）
    uint64_t  out_ch_layout=AV_CH_LAYOUT_STEREO;
//输出采样位数  16位
    enum AVSampleFormat out_formart=AV_SAMPLE_FMT_S16;
//输出的采样率必须与输入相同
    int out_sample_rate = audioCodecCtx->sample_rate;

//swr_alloc_set_opts将PCM源文件的采样格式转换为自己希望的采样格式
    swr_alloc_set_opts(swrContext, out_ch_layout, out_formart, out_sample_rate,
                       audioCodecCtx->channel_layout, audioCodecCtx->sample_fmt, audioCodecCtx->sample_rate, 0,
                       NULL);

    swr_init(swrContext);

    swsContext = sws_getContext(width, height, videoCodecCtx->pix_fmt,
                                                         width, height, AV_PIX_FMT_RGBA,
                                                         SWS_BILINEAR, NULL, NULL, NULL);
    if(!swsContext){
        LOGE("Couldn't initialize the SWS pVideoReaderState!");
        return -1;
    }
    frameData = new uint8_t[width*height*4];
    out_channer_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    createEngine();
    createMixVolume();
    createPlayer(audioCodecCtx->sample_rate, audioCodecCtx->channels);

    env->ReleaseStringUTFChars(file_path, input);
    return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nStart(JNIEnv *env, jobject thiz, jobject surface) {
    window = ANativeWindow_fromSurface(env, surface);
    LOGE("start width:%d height:%d",width,height)
    ANativeWindow_setBuffersGeometry(window, width,height,WINDOW_FORMAT_RGBA_8888);
    std::thread readThread(readFrame);
    readThread.detach();

    std::thread renderThread(renderSurface);
    renderThread.detach();

    playerState = PLAY_STATE_PLAYING;
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_PLAYING);
    getQueueCallBack(slBufferQueueItf, NULL);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nSeekTo(JNIEnv *env, jobject thiz, jlong time_stamp) {
    std::unique_lock l(mutex);
    isBeginSeeking = true;
    seekTime = time_stamp;
    currentPlayTime = (int64_t)seekTime/1000.0/av_q2d(*audioTimeBase);
    LOGE("nSeekTo:%lld",currentPlayTime)
    l.unlock();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nGetDuration(JNIEnv *env, jobject thiz) {
    return duration * av_q2d(*audioTimeBase) * 1000;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nPause(JNIEnv *env, jobject thiz) {
    playerState = PLAY_STATE_PAUSED;
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_PAUSED);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nGetCurrentPlayTime(JNIEnv *env, jobject thiz) {
    return currentPlayTime * av_q2d(*audioTimeBase) * 1000;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nResume(JNIEnv *env, jobject thiz) {
    playerState = PLAY_STATE_PLAYING;
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_PLAYING);
    pauseCondition.notify_one();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nStop(JNIEnv *env, jobject thiz) {
    playerState = PLAY_STATE_STOPPED;
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_STOPPED);
    pauseCondition.notify_one();
    produce_var.notify_one();

    //Wait until all the work is done.
    std::unique_lock l(mutex);
    releaseCondition.wait(l);
    l.unlock();

    av_frame_free(&frame);
    swr_free(&swrContext);
    sws_freeContext(swsContext);
    avcodec_close(audioCodecCtx);
    avcodec_close(videoCodecCtx);
    avformat_close_input(&pFormatCtx);
    av_packet_free(&packet);

    delete[] out_buffer;
    delete[] frameData;

    delete videoTimeBase;
    delete audioTimeBase;
    LOGE("release all the resources.")
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nGetWidth(JNIEnv *env, jobject thiz) {
    return width;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_ffmpegsample2_VideoPlayer_nGetHeight(JNIEnv *env, jobject thiz) {
    return height;
}