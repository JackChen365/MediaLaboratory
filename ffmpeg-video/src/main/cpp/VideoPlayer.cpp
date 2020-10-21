//
// Created by Jack Chen on 10/18/2020.
//

#include "VideoPlayer.h"

VideoPlayer::VideoPlayer() {
    //All the decode stuffs.
    avFormatContext = NULL;
    avCodecContext = NULL;
    avPacket = NULL;
    avFrame = NULL;
    swrContext = NULL;
    outBuffer=NULL;
    audioStreamIndex=-1;
    channelNumber = 0;
    //About player time.
    audioDuration=0;
    audioTimeBase=NULL;
    startTime=0;
    currentPlayTime=0;
    seekTime=0;

    mutex = new std::mutex();
    consumeVar = new std::condition_variable();
    produceVar = new std::condition_variable();
    audioQueue = new std::queue<std::pair<uint8_t *,int>>();
    videoQueue = new std::queue<AVFrame>();
    //About play state.
    isBeginSeeking=false;
}

VideoPlayer::~VideoPlayer() {
    av_frame_free(&avFrame);
    av_packet_free(&avPacket);
    swr_free(&swrContext);
    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);
    delete audioTimeBase;
    delete[] outBuffer;
}

bool VideoPlayer::loadFile(const char *filePath) {
    VIDEO_LOG_I("Video player loadFile!");
    avFormatContext = avformat_alloc_context();
    //open the input file.
    if (avformat_open_input(&avFormatContext, filePath, NULL, NULL) != 0) {
        VIDEO_LOG_E("Couldn't open the input file.\n");
        return false;
    }
    if(avformat_find_stream_info(avFormatContext,NULL) < 0){
        VIDEO_LOG_E("Couldn't find the stream info.\n");
        return false;
    }
    AVCodecParameters *localCodecParameters=NULL;
    AVCodec *localCodec=NULL;
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        localCodecParameters = avFormatContext->streams[i]->codecpar;
        localCodec = avcodec_find_decoder(localCodecParameters->codec_id);
        if (localCodec == NULL) {
            VIDEO_LOG_E("ERROR unsupported codec!");
            continue;
        }
        VIDEO_LOG_I("Video player codec_type:%d",localCodecParameters->codec_type);
        if (localCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            VIDEO_LOG_I("AVMEDIA_TYPE_VIDEO:%d",videoStreamIndex);
            if (videoStreamIndex == -1) {
                //When the stream is video we keep the position, the parameters and the codec.
                videoStreamIndex = i;
                videoWidth = localCodecParameters->width;
                videoHeight = localCodecParameters->height;
                videoAverageFrameRate=&avFormatContext->streams[i]->avg_frame_rate;
                videoTimeBase = &avFormatContext->streams[i]->time_base;
                videoDuration = avFormatContext->streams[i]->duration;
            }
        } else if(localCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO){
            VIDEO_LOG_I("AVMEDIA_TYPE_AUDIO:%d",audioStreamIndex);
            if(audioStreamIndex == -1){
                audioStreamIndex = i;
                audioSampleRate = localCodecParameters->sample_rate;
                audioChannels = localCodecParameters->channels;
                audioDuration = avFormatContext->streams[i]->duration;
                audioTimeBase = &avFormatContext->streams[i]->time_base;
                VIDEO_LOG_I("audioSampleRate:%d audioChannels:%d",audioSampleRate,audioChannels);
            }
        }
    }
    // Set up a codec context for the decoder
    avCodecContext = avcodec_alloc_context3(localCodec);
    if (!avCodecContext) {
        VIDEO_LOG_E("Couldn't create AVCodecContext\n");
        return 0;
    }
    if (avcodec_parameters_to_context(avCodecContext, localCodecParameters) < 0) {
        VIDEO_LOG_E("Couldn't initialize AVCodecContext\n");
        return 0;
    }
    if (avcodec_open2(avCodecContext, localCodec, NULL) < 0) {
        VIDEO_LOG_E("Couldn't open codec\n");
        return 0;
    }
    //locate the av packet.
    avPacket = av_packet_alloc();
    if (!avPacket) {
        VIDEO_LOG_E("Couldn't allocate AVPacket\n");
        return 0;
    }
    //locate the av frame.
    avFrame = av_frame_alloc();
    if (!avFrame) {
        VIDEO_LOG_E("Couldn't allocate AVFrame\n");
        return 0;
    }
    swsContext = sws_getContext(videoWidth, videoHeight, avCodecContext->pix_fmt,
                                      videoWidth, videoHeight, AV_PIX_FMT_RGB0,
                                SWS_BILINEAR, NULL, NULL, NULL);
    if(!swsContext){
        VIDEO_LOG_E("Couldn't initialize the SWS pVideoReaderState!");
        return -1;
    }
    if(!videoFrameData){
        videoFrameData=new uint8_t[videoWidth*videoHeight*4];
    }
    //Allocate SwrContext.
    swrContext = swr_alloc();
    if (!swrContext) {
        VIDEO_LOG_E("Couldn't allocate swrContext\n");
        return 0;
    }
    //Initialize The buffer.
    outBuffer = (uint8_t *) av_malloc(44100 * 2);
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, avCodecContext->sample_rate,
                       avCodecContext->channel_layout, avCodecContext->sample_fmt, avCodecContext->sample_rate, 0,
                       NULL);
    swr_init(swrContext);
    //The channel number
    channelNumber = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    outBuffer = (uint8_t *) av_malloc(44100 * 2);

    //Initial the audio player engine.
    audioPlayerEngine = new VideoPlayerOpenSLAudioEngine();
    audioPlayerEngine->setVideoPlayer(this);
    audioPlayerEngine->createEngine(audioSampleRate,audioChannels);
    return true;
}

void VideoPlayer::startReadFrame() {
    if(isBeginSeeking){
        isBeginSeeking = false;
        if(av_seek_frame(avFormatContext,audioStreamIndex,seekTime,AVSEEK_FLAG_BACKWARD) < 0){
            VIDEO_LOG_E("Seek failed.\n");
        }
    }
    while (av_read_frame(avFormatContext, avPacket) >= 0) {
        if (avPacket->stream_index != videoStreamIndex&&avPacket->stream_index != audioStreamIndex) {
            av_packet_unref(avPacket);
            continue;
        }
        //This is a video stream.
        int ret = avcodec_send_packet(avCodecContext,avPacket);
        if (ret < 0) {
            VIDEO_LOG_E("Error while sending a packet to the decoder:%s", av_err2str(ret));
            return;
        }
        ret = avcodec_receive_frame(avCodecContext,avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            VIDEO_LOG_E("Error while receiving a frame from the decoder:%s", av_err2str(ret));
            return;
        }
        if (0 <= ret) {
            VIDEO_LOG_I("Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d]",
                  av_get_picture_type_char(avFrame->pict_type),
                  avCodecContext->frame_number,
                  avFrame->pts,
                  avFrame->pkt_dts,
                  avFrame->key_frame,
                  avFrame->coded_picture_number,
                  avFrame->display_picture_number
            );
        }
        av_packet_unref(avPacket);
        //Take one packet.
        if(avPacket->stream_index == videoStreamIndex){
            //It is a video packet.
            videoQueue->push(*avFrame);
        } else if(avPacket->stream_index == audioStreamIndex){
            //It is a video packet.
            currentPlayTime = avFrame->pts;
            swr_convert(swrContext, &outBuffer, 44100 * 2, (const uint8_t **) avFrame->data, avFrame->nb_samples);
            //The sample buffer size.
            int sampleBufferSize = av_samples_get_buffer_size(NULL, channelNumber, avFrame->nb_samples,AV_SAMPLE_FMT_S16, 1);
            std::pair<uint8_t *,int> buffer=std::make_pair(outBuffer,sampleBufferSize);

        }
    }
    int64_t currentTime=av_gettime()-startTime;
    int64_t timeStamp = avFrame->pkt_dts*av_q2d(*audioTimeBase)*1000;
    int64_t deltaTime=timeStamp-currentTime;
    if(0 < deltaTime){
        std::this_thread::sleep_for(std::chrono::microseconds(deltaTime));
    }
}

void VideoPlayer::start() {
    startReadFrame();
    audioPlayerEngine->start();
}

void VideoPlayer::resume() {
    audioPlayerEngine->resume();
}

void VideoPlayer::pause() {
    audioPlayerEngine->pause();
}

void VideoPlayer::seekTo(int64_t timeStamp) {
    isBeginSeeking = true;
    seekTime = (timeStamp/1000)*(audioTimeBase->den/audioTimeBase->num);
}

void VideoPlayer::stop() {
    audioPlayerEngine->stop();
    release();
}

int64_t VideoPlayer::getCurrentPlayTime() {
    return currentPlayTime*av_q2d(*audioTimeBase)*1000;
}

int64_t VideoPlayer::getDuration() {
    return audioDuration*av_q2d(*audioTimeBase)*1000;
}

void VideoPlayer::release() {
    av_frame_free(&avFrame);
    swr_free(&swrContext);
    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);
    swr_free(&swrContext);
    delete[] outBuffer;
}



//--------------------------------------------------------------------------------
// Implementation of the class: AudioPlayerEngine
//--------------------------------------------------------------------------------
VideoPlayerAudioEngine::VideoPlayerAudioEngine() {
}

VideoPlayerAudioEngine::~VideoPlayerAudioEngine(){
    //Do not delete the pointer: audio player.
//    delete[] buffer;
}

void *VideoPlayerAudioEngine::getBuffer() const {
    return buffer;
}

void VideoPlayerAudioEngine::setBuffer(void *buffer) {
    VideoPlayerAudioEngine::buffer = buffer;
}

size_t VideoPlayerAudioEngine::getBufferSize() const {
    return bufferSize;
}

void VideoPlayerAudioEngine::setBufferSize(size_t bufferSize) {
    VideoPlayerAudioEngine::bufferSize = bufferSize;
}

VideoPlayer *VideoPlayerAudioEngine::getVideoPlayer() const {
    return videoPlayer;
}

void VideoPlayerAudioEngine::setVideoPlayer(VideoPlayer *audioPlayer) {
    VideoPlayerAudioEngine::videoPlayer = audioPlayer;
}

void VideoPlayerAudioEngine::createEngine(unsigned int rate, unsigned int channels) {
}

void VideoPlayerAudioEngine::start() {
}

void VideoPlayerAudioEngine::resume() {
}

void VideoPlayerAudioEngine::pause() {
}

void VideoPlayerAudioEngine::stop() {
}

//--------------------------------------------------------------------------------
// Implementation of the class: OpenSLAudioPlayerEngine
//--------------------------------------------------------------------------------
VideoPlayerOpenSLAudioEngine::VideoPlayerOpenSLAudioEngine():VideoPlayerAudioEngine(){
    engineObject=NULL;
    engineEngine = NULL;
    outputMixObject = NULL;
    outputMixEnvironmentalReverb = NULL;
    settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    playerObject=NULL;
    playerInterface=NULL;
    slBufferQueueItf=NULL;
}

VideoPlayerOpenSLAudioEngine::~VideoPlayerOpenSLAudioEngine(){
    delete engineObject;
    delete engineEngine;
    delete outputMixObject;
    delete outputMixEnvironmentalReverb;
    delete playerObject;
    delete playerInterface;
    delete slBufferQueueItf;
}

void VideoPlayerOpenSLAudioEngine::playerQueueCallBack(SLAndroidSimpleBufferQueueItf slBufferQueueItf,void *context) {
    VideoPlayerOpenSLAudioEngine* engine=(VideoPlayerOpenSLAudioEngine*) context;
    VIDEO_LOG_I("playerQueueCallBack:%d",engine);
    auto videoPlayer=engine->getVideoPlayer();
    void *buffer=engine->getBuffer();
    auto bufferSize=engine->getBufferSize();
//    videoPlayer->readAudioFrame(&buffer,&bufferSize);
    if(buffer!=NULL&&bufferSize!=0){
        (*slBufferQueueItf)->Enqueue(slBufferQueueItf,buffer,bufferSize);
    }
}

void VideoPlayerOpenSLAudioEngine::createEngine(unsigned int rate,unsigned int channels) {
    //Create audio engine.
    VIDEO_LOG_I("OpenSLAudioPlayerEngine::createEngine rate:%d channels:%d",rate,channels);
    slCreateEngine(&engineObject,0,NULL,0,NULL,NULL);
    (*engineObject)->Realize(engineObject,SL_BOOLEAN_FALSE);
    (*engineObject)->GetInterface(engineObject,SL_IID_ENGINE,&engineEngine);

    //Create audio mixer.
    VIDEO_LOG_I("OpenSLAudioPlayerEngine::Create audio mixer.");
    (*engineEngine)->CreateOutputMix(engineEngine,&outputMixObject,0,0,0);
    (*outputMixObject)->Realize(outputMixObject,SL_BOOLEAN_FALSE);
    SLresult result = (*outputMixObject)->GetInterface(outputMixObject,SL_IID_ENVIRONMENTALREVERB,&outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(outputMixEnvironmentalReverb, &settings);
    }

    //Create audio player.
    SLDataLocator_AndroidBufferQueue androidQueue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};
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
    SLDataSource dataSource = {&androidQueue,&pcm};
    SLDataLocator_OutputMix slDataLocatorOutputMix={SL_DATALOCATOR_OUTPUTMIX,outputMixObject};
    SLDataSink slDataSink = {&slDataLocatorOutputMix,NULL};
    const SLInterfaceID ids[3]={SL_IID_BUFFERQUEUE,SL_IID_EFFECTSEND,SL_IID_VOLUME};
    const SLboolean req[3]={SL_BOOLEAN_FALSE,SL_BOOLEAN_FALSE,SL_BOOLEAN_FALSE};

    (*engineEngine)->CreateAudioPlayer(engineEngine,&playerObject,&dataSource,&slDataSink,3,ids,req);
    (*playerObject)->Realize(playerObject,SL_BOOLEAN_FALSE);
    (*playerObject)->GetInterface(playerObject,SL_IID_PLAY,&playerInterface);
    (*playerObject)->GetInterface(playerObject,SL_IID_BUFFERQUEUE,&slBufferQueueItf);
    //Register player callback.
    int ret=(*slBufferQueueItf)->RegisterCallback(slBufferQueueItf,playerQueueCallBack,this);
    if (ret != SL_RESULT_SUCCESS) {
        VIDEO_LOG_I("register read pcm data callback failed");
    }

}

void VideoPlayerOpenSLAudioEngine::start() {
    VIDEO_LOG_I("start");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_PLAYING);
    playerQueueCallBack(slBufferQueueItf,this);
}

void VideoPlayerOpenSLAudioEngine::pause() {
    VIDEO_LOG_I("pause");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_PAUSED);
}

void VideoPlayerOpenSLAudioEngine::resume() {
    VIDEO_LOG_I("resume");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_PLAYING);
}

void VideoPlayerOpenSLAudioEngine::stop() {
    VIDEO_LOG_I("stop");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_STOPPED);
}


//--------------------------------------------------------------------------------
// Implementation of the class: VideoPlayerAudioEngine
//--------------------------------------------------------------------------------
template<> VideoPlayer* VideoPlayerVideoEngine<void*>::getVideoPlayer() const {
    return videoPlayer;
}

template<> void VideoPlayerVideoEngine<void*>::setVideoPlayer(VideoPlayer *videoPlayer) {
    VideoPlayerVideoEngine::videoPlayer = videoPlayer;
}

template<> void VideoPlayerVideoEngine<void*>::createEngine(JNIEnv *env,void *surface) {}

void VideoPlayerSurfaceEngine::createEngine(JNIEnv *env,jobject surface) {
    nativeWindow = ANativeWindow_fromSurface(env, surface);
}

void VideoPlayerSurfaceEngine::draw() {
    videoPlayer->readVideoFrame()
}
