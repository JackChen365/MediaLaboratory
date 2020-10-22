//
// Created by Jack Chen on 10/18/2020.
//

#include "VideoPlayer.h"

VideoPlayer::VideoPlayer() {
    //All the decode stuffs.
    avFormatContext = NULL;
    audioCodecContext = NULL;
    videoCodecContext = NULL;
    avPacket = NULL;
    avFrame = NULL;
    swrContext = NULL;
    videoBuffer=NULL;
    audioStreamIndex=-1;
    channelNumber = 0;
    //About player time.
    audioTimeBase=NULL;
    currentPlayTime=0;
    seekTimeStamp=0;
}

VideoPlayer::~VideoPlayer() {
    av_frame_free(&avFrame);
    av_packet_free(&avPacket);
    swr_free(&swrContext);
    sws_freeContext(swsContext);
    avcodec_close(audioCodecContext);
    avcodec_close(videoCodecContext);
    avformat_close_input(&avFormatContext);
    delete audioTimeBase;
    delete videoTimeBase;
    delete[] audioBuffer;
    delete[] videoBuffer;
}

void VideoPlayer::setDataSource(const char *filePath) {
    this->filePath = (char*)filePath;
}

bool VideoPlayer::prepare(JNIEnv *env,jobject surface) {
    //Before we prepare the player. We should always release the resources.
    releaseResources();
    //Initial all the thread resources.
    if(!filePath){
        VIDEO_LOG_E("Couldn't open the input file.\n");
        return false;
    }
    mutex = new std::mutex();
    produceVar = new std::condition_variable();
    audioConsumeVar = new std::condition_variable();
    videoConsumeVar = new std::condition_variable();
    playCondition = new std::condition_variable();

    audioQueue = new std::deque<AVFrame*>();
    videoQueue = new std::deque<AVFrame*>();

    av_register_all();
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
        if (localCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (videoStreamIndex == INVALID_STREAM_INDEX) {
                //When the stream is video we keep the position, the parameters and the codec.
                videoStreamIndex = i;
                videoWidth = localCodecParameters->width;
                videoHeight = localCodecParameters->height;
                videoTimeBase = &avFormatContext->streams[i]->time_base;
                // Set up a codec context for the decoder
                videoCodecContext = avcodec_alloc_context3(localCodec);
                if (!videoCodecContext) {
                    VIDEO_LOG_E("Couldn't create AVCodecContext\n");
                    return 0;
                }
                if (avcodec_parameters_to_context(videoCodecContext, localCodecParameters) < 0) {
                    VIDEO_LOG_E("Couldn't initialize AVCodecContext\n");
                    return 0;
                }
                if (avcodec_open2(videoCodecContext, localCodec, NULL) < 0) {
                    VIDEO_LOG_E("Couldn't open codec\n");
                    return 0;
                }
            }
        } else if(localCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO){
            if(audioStreamIndex == INVALID_STREAM_INDEX){
                audioStreamIndex = i;
                audioSampleRate = localCodecParameters->sample_rate;
                audioChannels = localCodecParameters->channels;
                audioTimeBase = &avFormatContext->streams[i]->time_base;
                audioCodecContext = avcodec_alloc_context3(localCodec);
                if (!audioCodecContext) {
                    VIDEO_LOG_E("Couldn't create AVCodecContext\n");
                    return 0;
                }
                if (avcodec_parameters_to_context(audioCodecContext, localCodecParameters) < 0) {
                    VIDEO_LOG_E("Couldn't initialize AVCodecContext\n");
                    return 0;
                }
                if (avcodec_open2(audioCodecContext, localCodec, NULL) < 0) {
                    VIDEO_LOG_E("Couldn't open codec\n");
                    return 0;
                }
            }
        }
    }
    //Make the duration as milliseconds.
    duration = avFormatContext->duration / AV_TIME_BASE * 1000;
    //locate the av packet.
    avPacket = av_packet_alloc();
    if (!avPacket) {
        VIDEO_LOG_E("Couldn't allocate AVPacket\n");
        return false;
    }
    //locate the av frame.
    avFrame = av_frame_alloc();
    if (!avFrame) {
        VIDEO_LOG_E("Couldn't allocate AVFrame\n");
        return false;
    }
    if(!videoFrameData){
        videoFrameData=new uint8_t[videoWidth*videoHeight*4];
    }

    //Initial audio
    swrContext = swr_alloc();
    audioBuffer = (uint8_t *) av_malloc(44100 * 2);
    //The output audio chanel is stereo.
    uint64_t outChannelLayout=AV_CH_LAYOUT_STEREO;
    //Audio sample format is signed 16 bit.
    enum AVSampleFormat outFormat=AV_SAMPLE_FMT_S16;
    int out_sample_rate = audioCodecContext->sample_rate;
    swr_alloc_set_opts(swrContext, outChannelLayout, outFormat, out_sample_rate,
                       audioCodecContext->channel_layout, audioCodecContext->sample_fmt, audioCodecContext->sample_rate, 0,
                       NULL);
    swr_init(swrContext);

    //Initial video
    swsContext = sws_getContext(videoWidth, videoHeight, videoCodecContext->pix_fmt,
                                videoWidth, videoHeight, AV_PIX_FMT_RGB0,
                                SWS_BILINEAR, NULL, NULL, NULL);
    if(!swsContext){
        VIDEO_LOG_E("Couldn't initialize the SWS pVideoReaderState!");
        return false;
    }
    //Allocate SwrContext.
    swrContext = swr_alloc();
    if (!swrContext) {
        VIDEO_LOG_E("Couldn't allocate swrContext\n");
        return false;
    }
    //Initialize The buffer.
    videoBuffer = (uint8_t *) av_malloc(44100 * 2);
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, videoCodecContext->sample_rate,
                       videoCodecContext->channel_layout, videoCodecContext->sample_fmt, videoCodecContext->sample_rate, 0,
                       NULL);
    swr_init(swrContext);
    //The channel number
    channelNumber = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    videoBuffer = (uint8_t *) av_malloc(44100 * 2);

    //Initial the audio player engine.
    VideoPlayerOpenSLAudioEngine* openSlAudioEngine = new VideoPlayerOpenSLAudioEngine();
    openSlAudioEngine->setVideoPlayer(this);
    openSlAudioEngine->rate = audioSampleRate;
    openSlAudioEngine->channels = audioChannels;
    audioEngine = openSlAudioEngine;
    audioEngine->prepare();

    //Initial the video player engine.
    VideoPlayerSurfaceEngine* surfaceEngine = new VideoPlayerSurfaceEngine();
    surfaceEngine->setVideoPlayer(this);
    surfaceEngine->env = env;
    surfaceEngine->surface = surface;
    surfaceEngine->width = videoWidth;
    surfaceEngine->height = videoHeight;
    videoEngine = surfaceEngine;
    videoEngine->prepare();
}

void VideoPlayer::clearBufferQueue() {
    auto audioIterator=std::begin(*audioQueue);
    while(audioIterator<std::end(*audioQueue)){
        av_frame_free(&*audioIterator);
        audioIterator++;
    }
    audioQueue->clear();
    auto videoIterator=std::begin(*videoQueue);
    while(videoIterator<std::end(*videoQueue)){
        av_frame_free(&*videoIterator);
        videoIterator++;
    }
    videoQueue->clear();
}


void VideoPlayer::startReadFrame() {
    VIDEO_LOG_I("start read frame:%d",std::this_thread::get_id());
    while (av_read_frame(avFormatContext, avPacket) >= 0) {
        if(isBeginSeeking){
            std::unique_lock l(mutex);
            isBeginSeeking = false;
            clearBufferQueue();
            int64_t audioTimeStamp = (int64_t)seekTimeStamp / 1000.0 / av_q2d(*audioTimeBase);
            currentPlayTime = audioTimeStamp;
            VIDEO_LOG_E("readFrame seek:%lld audioSize:%d videoSize:%d",audioTimeStamp,audioQueue->size(),videoQueue->size());
            if(av_seek_frame(avFormatContext,audioStreamIndex,audioTimeStamp,AVSEEK_FLAG_BACKWARD) < 0){
                VIDEO_LOG_E("Audio seek failed.");
            }
            avcodec_flush_buffers(audioCodecContext);
            avcodec_flush_buffers(videoCodecContext);
            l.unlock();
            //Skip this frame.
            continue;
        }
        if(playerState == PLAY_STATE_STOPPED){
            break;
        }
        if (avPacket->stream_index != audioStreamIndex&&avPacket->stream_index != videoStreamIndex) {
            av_packet_unref(avPacket);
            continue;
        }
        if(avPacket->stream_index == videoStreamIndex){
            int ret=avcodec_send_packet(videoCodecContext, avPacket);
            if (ret < 0) {
                VIDEO_LOG_E("Video Error while sending a packet to the decoder:%s currentTime:%lld", av_err2str(ret),currentPlayTime);
                continue;
            }
            ret = avcodec_receive_frame(videoCodecContext, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                VIDEO_LOG_E("Video error while receiving a frame from the decoder:%s", av_err2str(ret));
                continue;
            }
            VIDEO_LOG_I("Video Frame %c (%d) pts %lld dts %d key_frame %d [coded_picture_number %d, display_picture_number %d] timeRate:%f",
                 av_get_picture_type_char(avFrame->pict_type),
                 videoCodecContext->frame_number,
                 avFrame->pts,
                 avFrame->pkt_dts,
                 avFrame->key_frame,
                 avFrame->coded_picture_number,
                 avFrame->display_picture_number,
                 avFrame->pts*1.0*av_q2d(*audioTimeBase)
            );
            std::unique_lock l(*mutex);
            while(playerState == PLAY_STATE_PLAYING&&0 < audioQueue->size()&&videoQueue->size()>=MAX_QUEUE_SIZE){
                VIDEO_LOG_E("video produce wait\n");
                produceVar->wait(l);
            }
            AVFrame *dst = av_frame_alloc();
//            if(videoQueue.size()>=MAX_QUEUE_SIZE){
//                videoQueue.pop_front();
//            }
            if (av_frame_ref(dst, avFrame) >= 0) {
                videoQueue->push_back(dst);
            }
            VIDEO_LOG_E("video produce num:%d--------------\n",videoQueue->size());
            videoConsumeVar->notify_one();
            l.unlock();
        } else if(avPacket->stream_index == audioStreamIndex){
            int ret=avcodec_send_packet(audioCodecContext, avPacket);
            if (ret < 0) {
                VIDEO_LOG_E("Audio Error while sending a packet to the decoder:%s", av_err2str(ret));
                return;
            }
            ret = avcodec_receive_frame(audioCodecContext, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                VIDEO_LOG_E("Audio Error while receiving a frame from the decoder:%s", av_err2str(ret));
                return;
            }
            VIDEO_LOG_E("Audio Frame %c (%d) pts %lld dts %d key_frame %d [coded_picture_number %d, display_picture_number %d] timeRate:%f",
                 av_get_picture_type_char(avFrame->pict_type),
                 audioCodecContext->frame_number,
                 avFrame->pts,
                 avFrame->pkt_dts,
                 avFrame->key_frame,
                 avFrame->coded_picture_number,
                 avFrame->display_picture_number,
                 avFrame->pts*1.0*av_q2d(*audioTimeBase)
            );
            std::unique_lock l(*mutex);
            while(playerState == PLAY_STATE_PLAYING&&0 < videoQueue->size()&&audioQueue->size()>=MAX_QUEUE_SIZE){
                produceVar->wait(l);
            }
            AVFrame *dst = av_frame_alloc();
            if (av_frame_ref(dst, avFrame) >= 0) {
                audioQueue->push_back(dst);
            }
            audioConsumeVar->notify_one();
            l.unlock();
        }
        //After we processed the frame. If we know the player wants to pause. We are start to wait.
        if(playerState == PLAY_STATE_PAUSED){
            std::unique_lock l(*mutex);
            playCondition->wait(l);
            l.unlock();
        }
    }
    std::unique_lock l(mutex);
    clearBufferQueue();
    l.unlock();
    audioConsumeVar->notify_one();
    videoConsumeVar->notify_one();
    VIDEO_LOG_I("the read thread is done.");
}

void VideoPlayer::start() {
    playerState = PLAY_STATE_PLAYING;
    VideoPlayer* player=this;
    std::thread readThread([player](){
        player->startReadFrame();
    });
    readThread.detach();
    audioEngine->start();
    videoEngine->start();
}

void VideoPlayer::resume() {
    playerState = PLAY_STATE_PLAYING;
    audioEngine->resume();
    videoEngine->resume();
    playCondition->notify_one();
}

void VideoPlayer::pause() {
    playerState = PLAY_STATE_PAUSED;
    audioEngine->pause();
    videoEngine->pause();
}

void VideoPlayer::seekTo(int64_t timeStamp) {
    std::unique_lock l(mutex);
    isBeginSeeking = true;
    seekTimeStamp = timeStamp;
    currentPlayTime = (int64_t)timeStamp/1000.0/av_q2d(*audioTimeBase);
    l.unlock();
    VIDEO_LOG_I("nSeekTo:%lld",currentPlayTime);
}

void VideoPlayer::stop() {
    audioEngine->stop();
    videoEngine->stop();
}

void VideoPlayer::fastForward() {
    isBeginSeeking = true;
    //Total duration which is millisecond.
    int64_t duration = getDuration();
    //Current millisecond;
    int64_t currentPlayTime = getCurrentPlayTime();
    int64_t newTimeStamp= currentPlayTime + DEFAULT_SEEK_SHORT_TIME < duration?
                          currentPlayTime + DEFAULT_SEEK_SHORT_TIME : duration;
    seekTimeStamp = (newTimeStamp/1000)*(audioTimeBase->den/audioTimeBase->num);
    VIDEO_LOG_I("fastForward:%lld currentPlayTime:%lld duration:%lld",newTimeStamp,currentPlayTime,duration);
}

void VideoPlayer::rewind() {
    isBeginSeeking = true;
    //Current millisecond;
    int64_t currentPlayTime = getCurrentPlayTime();
    int64_t newTimeStamp = currentPlayTime - DEFAULT_SEEK_SHORT_TIME > 0?
                           currentPlayTime - DEFAULT_SEEK_SHORT_TIME : 0;
    seekTimeStamp = (newTimeStamp/1000)*(audioTimeBase->den/audioTimeBase->num);
    VIDEO_LOG_I("rewind:%lld",newTimeStamp);
}

int32_t VideoPlayer::getWidth() {
    return videoWidth;
}

int32_t VideoPlayer::getHeight() {
    return videoHeight;
}

void VideoPlayer::releaseResources() {
    av_packet_free(&avPacket);
    av_frame_free(&avFrame);
    swr_free(&swrContext);
    sws_freeContext(swsContext);
    avcodec_close(audioCodecContext);
    avcodec_close(videoCodecContext);
    avformat_close_input(&avFormatContext);

    delete[] audioBuffer;
    delete[] videoBuffer;

    delete videoTimeBase;
    delete audioTimeBase;
    VIDEO_LOG_I("releaseResources all the resources.");
}

int64_t VideoPlayer::getCurrentPlayTime() {
    return currentPlayTime*av_q2d(*audioTimeBase)*1000;
}

int64_t VideoPlayer::getDuration() {
    return duration;
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

void VideoPlayerAudioEngine::prepare() {
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

void VideoPlayerOpenSLAudioEngine::prepare() {
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
    playerQueueCallBack(slBufferQueueItf, this);
}

void VideoPlayerOpenSLAudioEngine::stop() {
    VIDEO_LOG_I("stop");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_STOPPED);
}


//--------------------------------------------------------------------------------
// Implementation of the class: VideoPlayerAudioEngine
//--------------------------------------------------------------------------------
VideoPlayer* VideoPlayerVideoEngine::getVideoPlayer() const {
    return videoPlayer;
}

void VideoPlayerVideoEngine::setVideoPlayer(VideoPlayer *videoPlayer) {
    VideoPlayerVideoEngine::videoPlayer = videoPlayer;
}

void VideoPlayerSurfaceEngine::prepare() {
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_setBuffersGeometry(nativeWindow, width,height,WINDOW_FORMAT_RGBA_8888);
}

void VideoPlayerSurfaceEngine::run() {
//    videoPlayer->readVideoFrame()
}
