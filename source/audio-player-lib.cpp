

#include <jni.h>
#include <thread>
#include <chrono>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES_Android.h>
#include "audio-player-lib.h"

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

//Open SL ES
SLObjectItf engineObject=NULL;//用SLObjectItf声明引擎接口对象
SLEngineItf engineEngine = NULL;//声明具体的引擎对象
SLObjectItf outputMixObject = NULL;//用SLObjectItf创建混音器接口对象
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;////具体的混音器对象实例
SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;//默认情况
SLObjectItf audioplayer=NULL;//用SLObjectItf声明播放器接口对象
SLPlayItf  slPlayItf=NULL;//播放器接口
SLAndroidSimpleBufferQueueItf  slBufferQueueItf=NULL;//缓冲区队列接口

size_t buffersize =0;
void *buffer;

//All the fields related to FFMPEG.
AVFormatContext *pFormatCtx = NULL;
AVCodecContext *pCodecCtx = NULL;
AVPacket *packet = NULL;
AVFrame *frame = NULL;
SwrContext *swrContext = NULL;
uint8_t *out_buffer=NULL;
int audio_stream_idx=-1;
int out_channer_nb = 0;

int64_t duration=0;
AVRational* timeBase=NULL;
int64_t startTime=0;
int64_t currentPlayTime=0;
int64_t seekTime=0;

std::atomic_bool isBeginSeeking=false;

private void getPCM(void **pcm,size_t *pcm_size);
//将pcm数据添加到缓冲区中
void getQueueCallBack(SLAndroidSimpleBufferQueueItf  slBufferQueueItf, void* context){
    buffersize=0;
    getPCM(&buffer,&buffersize);
    if(buffer!=NULL&&buffersize!=0){
        (*slBufferQueueItf)->Enqueue(slBufferQueueItf,buffer,buffersize);
    }
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
    //播放
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_PLAYING);
    //开始播放
    getQueueCallBack(slBufferQueueItf,NULL);
}

private void release(){
    av_frame_free(&frame);
    swr_free(&swrContext);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
}

private void getPCM(void **pcm,size_t *pcm_size){
    int got_frame;
    if(isBeginSeeking){
        LOGE("Start seek:%d",seekTime);
        isBeginSeeking = false;
        if(av_seek_frame(pFormatCtx,audio_stream_idx,seekTime,AVSEEK_FLAG_BACKWARD) < 0){
            LOGE("Seek failed.");
        }
    }
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index != audio_stream_idx) {
            av_packet_unref(packet);
            continue;
        }
        int ret=avcodec_send_packet(pCodecCtx,packet);
        if (ret < 0) {
            LOGE("Error while sending a packet to the decoder:%s", av_err2str(ret));
            return;
        }
        ret = avcodec_receive_frame(pCodecCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            LOGE("Error while receiving a frame from the decoder:%s", av_err2str(ret));
            return;
        }
//                LOGE("Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d]",
//                      av_get_picture_type_char(frame->pict_type),
//                      pCodecCtx->frame_number,
//                      frame->pts,
//                      frame->pkt_dts,
//                      frame->key_frame,
//                      frame->coded_picture_number,
//                      frame->display_picture_number
//                );
        //Update current presentation time stamp.
        currentPlayTime = frame->pts;
        swr_convert(swrContext, &out_buffer, 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
        //缓冲区的大小
        int size = av_samples_get_buffer_size(NULL, out_channer_nb, frame->nb_samples,AV_SAMPLE_FMT_S16, 1);
        *pcm = out_buffer;
        *pcm_size = size;
        break;
    }
    int64_t currentTime=av_gettime()-startTime;
    int64_t timeStamp = frame->pkt_dts*av_q2d(*timeBase)*1000;
    int64_t deltaTime=timeStamp-currentTime;
//    LOGE("frame_number:%d currentTime:%d delta:%d",pCodecCtx->frame_number,currentTime,deltaTime)
    if(0 < deltaTime){
        std::this_thread::sleep_for(std::chrono::microseconds(deltaTime));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_MainActivity_playSound(JNIEnv *env, jobject instance, jstring input_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    av_register_all();
    pFormatCtx = avformat_alloc_context();
    //open
    if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0) {
        LOGE("%s","打开输入视频文件失败");
        return;
    }
    //获取视频信息
    if(avformat_find_stream_info(pFormatCtx,NULL) < 0){
        LOGE("%s","获取视频信息失败");
        return;
    }
    for (int i = 0; i < pFormatCtx->nb_streams; ++i) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            LOGE("  找到音频id %d", pFormatCtx->streams[i]->codec->codec_type);
            audio_stream_idx=i;
            duration=pFormatCtx->streams[i]->duration;
            timeBase=&pFormatCtx->streams[i]->time_base;
            break;
        }
    }
    //获取解码器上下文
    pCodecCtx=pFormatCtx->streams[audio_stream_idx]->codec;
    //获取解码器
    AVCodec *pCodex = avcodec_find_decoder(pCodecCtx->codec_id);
    //打开解码器
    if (avcodec_open2(pCodecCtx, pCodex, NULL)<0) {
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
    int out_sample_rate = pCodecCtx->sample_rate;

//swr_alloc_set_opts将PCM源文件的采样格式转换为自己希望的采样格式
    swr_alloc_set_opts(swrContext, out_ch_layout, out_formart, out_sample_rate,
                       pCodecCtx->channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0,
                       NULL);

    swr_init(swrContext);
    out_channer_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

    startTime=av_gettime();
    createEngine();
    createMixVolume();
    createPlayer(pCodecCtx->sample_rate,pCodecCtx->channels);

    env->ReleaseStringUTFChars(input_, input);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_MainActivity_seekTo(JNIEnv *env, jobject thiz, jlong time_stamp) {
    isBeginSeeking = true;
    seekTime = (time_stamp/1000)*(timeBase->den/timeBase->num);
    LOGE("seekTo:%d",seekTime);
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpegsample2_MainActivity_getDuration(JNIEnv *env, jobject thiz) {
    return duration*av_q2d(*timeBase)*1000;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_MainActivity_pause(JNIEnv *env, jobject thiz) {
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_PAUSED);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpegsample2_MainActivity_getCurrentPlayTime(JNIEnv *env, jobject thiz) {
    return currentPlayTime*av_q2d(*timeBase)*1000;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpegsample2_MainActivity_resume(JNIEnv *env, jobject thiz) {
    (*slPlayItf)->SetPlayState(slPlayItf,SL_PLAYSTATE_PLAYING);
}