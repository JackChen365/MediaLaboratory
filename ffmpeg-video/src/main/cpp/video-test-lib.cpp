#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/bitmap.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
}

#define TAG "FFMPEGNative"
#define  LOG_I(...)  __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define  LOG_E(...)  __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

class VideoReaderState{
public:
    AVFormatContext* avFormatContext;
    AVCodecContext* avCodecContext;
    SwsContext* swsScalerContext;
    AVRational* averageFrameRate;
    AVRational* timeBase;
    AVFrame* avFrame;
    AVPacket* avPacket;
    uint8_t* frameData;
    uint32_t width;
    uint32_t height;
    uint64_t duration;
    uint64_t currentPlayTime;
    uint32_t videoStreamIndex;

    VideoReaderState(){
        avFormatContext=NULL;
        avCodecContext=NULL;
        swsScalerContext=NULL;
        avFrame=NULL;
        avPacket=NULL;
        frameData=NULL;
        timeBase=NULL;
        width = height = 0;
        duration = 0;
        currentPlayTime = 0;
        videoStreamIndex = -1;
    }

    ~VideoReaderState(){
        sws_freeContext(swsScalerContext);
        avformat_close_input(&avFormatContext);
        avformat_free_context(avFormatContext);
        av_frame_free(&avFrame);
        av_packet_free(&avPacket);
        avcodec_free_context(&avCodecContext);
        delete[] frameData;
    }

};


extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nLoadFile(JNIEnv *env, jobject thiz, jstring jFilePath) {
    const char* filePath=env->GetStringUTFChars(jFilePath,0);
    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        LOG_E("Couldn't created AVFormatContext\n");
        return false;
    }
    if(0!=avformat_open_input(&pFormatContext,filePath,NULL,NULL)){
        LOG_E("Failed open the file:%s",filePath);
        return 0;
    }
    if(0!=avformat_find_stream_info(pFormatContext,NULL)){
        LOG_E("Failed find the stream info:%s",filePath);
        return 0;
    }
    VideoReaderState* videoReaderState=new VideoReaderState();
    videoReaderState->avFormatContext = pFormatContext;
    videoReaderState->videoStreamIndex = -1;
    AVCodec *pLocalCodec=NULL;
    AVCodecParameters *pLocalCodecParameters=NULL;
    for (int i = 0; i < pFormatContext->nb_streams; i++) {
        pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
        pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
        if (pLocalCodec == NULL) {
            LOG_E("ERROR unsupported codec!");
            continue;
        }
        //When the stream is video we keep the position, the parameters and the codec.
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (videoReaderState->videoStreamIndex == -1) {
                videoReaderState->videoStreamIndex = i;
                videoReaderState->width = pLocalCodecParameters->width;
                videoReaderState->height = pLocalCodecParameters->height;
                videoReaderState->averageFrameRate=&pFormatContext->streams[i]->avg_frame_rate;
                videoReaderState->timeBase = &pFormatContext->streams[i]->time_base;
                videoReaderState->duration = pFormatContext->streams[i]->duration;
                pLocalCodecParameters->sample_rate;
            }
            break;
            LOG_I("Video Codec: resolution %d x %d", pLocalCodecParameters->width,pLocalCodecParameters->height);
        }
    }

    // Set up a codec context for the decoder
    videoReaderState->avCodecContext = avcodec_alloc_context3(pLocalCodec);
    if (!videoReaderState->avCodecContext) {
        LOG_E("Couldn't create AVCodecContext\n");
        return 0;
    }
    if (avcodec_parameters_to_context(videoReaderState->avCodecContext, pLocalCodecParameters) < 0) {
        LOG_E("Couldn't initialize AVCodecContext\n");
        return 0;
    }
    if (avcodec_open2(videoReaderState->avCodecContext, pLocalCodec, NULL) < 0) {
        LOG_E("Couldn't open codec\n");
        return 0;
    }

    videoReaderState->avFrame = av_frame_alloc();
    if (!videoReaderState->avFrame) {
        LOG_E("Couldn't allocate AVFrame\n");
        return 0;
    }
    videoReaderState->avPacket = av_packet_alloc();
    if (!videoReaderState->avPacket) {
        LOG_E("Couldn't allocate AVPacket\n");
        return 0;
    }
    //Loop though all the streams and print its main information.
    env->ReleaseStringUTFChars(jFilePath,filePath);
    return (jlong)videoReaderState;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nGetWidth(JNIEnv *env, jobject thiz, jlong ref) {
    VideoReaderState *videoReaderState=(VideoReaderState *)ref;
    return videoReaderState->width;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nGetHeight(JNIEnv *env, jobject thiz, jlong ref) {
    VideoReaderState *videoReaderState=(VideoReaderState *)ref;
    return videoReaderState->height;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nGetDuration(JNIEnv *env, jobject thiz, jlong ref) {
    VideoReaderState *videoReaderState=(VideoReaderState *)ref;
    return videoReaderState->duration * av_q2d(*videoReaderState->timeBase)*1000;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nGetCurrentDuration(JNIEnv *env,
                                                                                   jobject thiz,
                                                                                   jlong ref) {
    VideoReaderState *videoReaderState=(VideoReaderState *)ref;
    return videoReaderState->currentPlayTime*av_q2d(*videoReaderState->timeBase)* 1000;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nGetFrameCount(JNIEnv *env, jobject thiz, jlong ref) {
    VideoReaderState *videoReaderState=(VideoReaderState *)ref;
    double av_d2q=av_q2d(*videoReaderState->timeBase);
    float totalTime=(float)videoReaderState->duration*av_d2q;
    return totalTime*av_q2d(*videoReaderState->averageFrameRate);
}

private void decodeFrameInternal(JNIEnv *env, jlong ref,jobject jbitmap);

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nDecodeFrame(JNIEnv *env, jobject thiz, jlong ref, jobject jbitmap){
    decodeFrameInternal(env,ref,jbitmap);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nFillFrame(JNIEnv *env, jobject thiz, jlong ref, jobject jbitmap, jint index) {
    VideoReaderState *pVideoReaderState=(VideoReaderState *)ref;
    // Unpack members of state
    AVFormatContext* pFormatContext=pVideoReaderState->avFormatContext;
    uint32_t videoStreamIndex=pVideoReaderState->videoStreamIndex;
    // Calculate the dst by frame index.
    double timeStampValue= 1.0 / av_q2d(*pVideoReaderState->averageFrameRate);
    int64_t timeStamp= (timeStampValue * index) * pVideoReaderState->timeBase->den;
    LOG_I("seek frame to:%d timeStamp:%d.",index,timeStamp);
    if(0 > av_seek_frame(pFormatContext,videoStreamIndex,timeStamp,AVSEEK_FLAG_ANY)){
        //seek failed.
        LOG_E("seek failed:%d.",index);
    }
    decodeFrameInternal(env,ref,jbitmap);
}

private void decodeFrameInternal(JNIEnv *env, jlong ref,jobject jbitmap){
    VideoReaderState *pVideoReaderState=(VideoReaderState *)ref;
    // Unpack members of state
    AVFormatContext* pFormatContext=pVideoReaderState->avFormatContext;
    uint32_t videoStreamIndex=pVideoReaderState->videoStreamIndex;
    AVCodecContext *pCodecContext = pVideoReaderState->avCodecContext;
    AVRational* timeBase=pVideoReaderState->timeBase;
    AVFrame *pFrame = pVideoReaderState->avFrame;
    AVPacket *pPacket = pVideoReaderState->avPacket;
    auto width=pVideoReaderState->width;
    auto height=pVideoReaderState->height;
    while(av_read_frame(pFormatContext, pPacket) >= 0) {
        if (pPacket->stream_index != videoStreamIndex) {
            av_packet_unref(pPacket);
            continue;
        }
        //This is a video stream.
        int ret = avcodec_send_packet(pCodecContext, pPacket);
        if (ret < 0) {
            LOG_E("Error while sending a packet to the decoder:%s", av_err2str(ret));
            return;
        }
        ret = avcodec_receive_frame(pCodecContext, pFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            LOG_E("Error while receiving a frame from the decoder:%s", av_err2str(ret));
            return;
        }
        if (0 <= ret) {
            LOG_I("Frame %c (%d) pts %lld dts %lld key_frame %d [coded_picture_number %d, display_picture_number %d]",
                  av_get_picture_type_char(pFrame->pict_type),
                  pCodecContext->frame_number,
                  pFrame->pts,
                  pFrame->pkt_dts,
                  pFrame->key_frame,
                  pFrame->coded_picture_number,
                  pFrame->display_picture_number
            );
            LOG_I("Frame pts:%lld", pFrame->pts * av_q2d(*timeBase));
        }
        pVideoReaderState->currentPlayTime = pFrame->pts;
        av_packet_unref(pPacket);
        break;
    }
    if(!pVideoReaderState->swsScalerContext){
        pVideoReaderState->swsScalerContext = sws_getContext(pFrame->width, pFrame->height, pCodecContext->pix_fmt,
                                                             pFrame->width, pFrame->height, AV_PIX_FMT_RGB0,
                                                             SWS_BILINEAR, NULL, NULL, NULL);
        if(!pVideoReaderState->swsScalerContext){
            LOG_E("Couldn't initialize the SWS pVideoReaderState!");
            return;
        }
    }
    if(!pVideoReaderState->frameData){
        pVideoReaderState->frameData=new uint8_t[width*height*4];
    }
    uint8_t* dest[4] = {pVideoReaderState->frameData, NULL, NULL, NULL };
    int dest_linesize[4] = { (int)width * 4, 0, 0, 0 };
    sws_scale(pVideoReaderState->swsScalerContext,pFrame->data,pFrame->linesize,0,pFrame->height,dest,dest_linesize);

    void* addrPtr;
    if(ANDROID_BITMAP_RESULT_SUCCESS!=AndroidBitmap_lockPixels(env,jbitmap,&addrPtr)){
        return;
    }
    uint32_t *bitmapPixels=(uint32_t*)addrPtr;
    memcpy(bitmapPixels,pVideoReaderState->frameData,pVideoReaderState->width*pVideoReaderState->height*4);
    AndroidBitmap_unlockPixels(env,jbitmap);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_test_NativeVideoDecoder_nRecycle(JNIEnv *env, jobject thiz, jlong ref) {
    VideoReaderState *videoReaderState=(VideoReaderState *)ref;
    delete videoReaderState;
}