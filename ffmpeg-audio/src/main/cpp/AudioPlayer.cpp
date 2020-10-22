//
// Created by Jack Chen on 10/17/2020.
//

#include "AudioPlayer.h"

AudioPlayer::AudioPlayer() {
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
    duration=0;
    timeBase=NULL;
    startTime=0;
    currentPlayTime=0;
    seekTime=0;
    //About play state.
    isBeginSeeking=false;
}

AudioPlayer::~AudioPlayer() {
    av_frame_free(&avFrame);
    av_packet_free(&avPacket);
    swr_free(&swrContext);
    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);
    delete timeBase;
    delete[] outBuffer;
}

void AudioPlayer::readFrame(void **pcmBuffer,size_t *bufferSize) {
    if(isBeginSeeking){
        isBeginSeeking = false;
        if(av_seek_frame(avFormatContext,audioStreamIndex,seekTime,AVSEEK_FLAG_BACKWARD) < 0){
            AUDIO_LOG_E("Seek failed.\n");
        }
    }
    while (av_read_frame(avFormatContext, avPacket) >= 0) {
        if (avPacket->stream_index != audioStreamIndex) {
            av_packet_unref(avPacket);
            continue;
        }
        int ret=avcodec_send_packet(avCodecContext,avPacket);
        if (ret < 0) {
            AUDIO_LOG_E("Error while sending a packet to the decoder:%s", av_err2str(ret));
            return;
        }
        ret = avcodec_receive_frame(avCodecContext, avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            AUDIO_LOG_E("Error while receiving a frame from the decoder:%s", av_err2str(ret));
            return;
        }
//        LOG_E("Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d]",
//              av_get_picture_type_char(avFrame->pict_type),
//             avCodecContext->frame_number,
//             avFrame->pts,
//             avFrame->pkt_dts,
//             avFrame->key_frame,
//             avFrame->coded_picture_number,
//             avFrame->display_picture_number
//        );
        //Update current presentation time stamp.
        currentPlayTime = avFrame->pts;
        swr_convert(swrContext, &outBuffer, 44100 * 2, (const uint8_t **) avFrame->data, avFrame->nb_samples);
        //The sample buffer size.
        int sampleBufferSize = av_samples_get_buffer_size(NULL, channelNumber, avFrame->nb_samples,AV_SAMPLE_FMT_S16, 1);
        *pcmBuffer = outBuffer;
        *bufferSize = sampleBufferSize;
        break;
    }
    int64_t currentTime=av_gettime()-startTime;
    int64_t timeStamp = avFrame->pkt_dts*av_q2d(*timeBase)*1000;
    int64_t deltaTime=timeStamp-currentTime;
    if(0 < deltaTime){
        std::this_thread::sleep_for(std::chrono::microseconds(deltaTime));
    }
}

bool AudioPlayer::loadFile(const char *filePath) {
    avFormatContext = avformat_alloc_context();
    //open the input file.
    if (avformat_open_input(&avFormatContext, filePath, NULL, NULL) != 0) {
        AUDIO_LOG_E("Couldn't open the input file.\n");
        return false;
    }
    if(avformat_find_stream_info(avFormatContext,NULL) < 0){
        AUDIO_LOG_E("Couldn't find the stream info.\n");
        return false;
    }
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        if (avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            AUDIO_LOG_E("Found the audio stream index:%d\n", i);
            audioStreamIndex = i;
            duration=avFormatContext->streams[i]->duration;
            timeBase=&avFormatContext->streams[i]->time_base;
            break;
        }
    }
    avCodecContext=avFormatContext->streams[audioStreamIndex]->codec;
    AVCodec *pCodex = avcodec_find_decoder(avCodecContext->codec_id);
    if (avcodec_open2(avCodecContext, pCodex, NULL)<0) {
        AUDIO_LOG_E("Couldn't open the codec context.\n");
    }
    //locate the av packet.
    avPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    //locate the av frame.
    avFrame = av_frame_alloc();
    //Allocate SwrContext.
    swrContext = swr_alloc();
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
    audioPlayerEngine = new OpenSLAudioPlayerEngine();
    audioPlayerEngine->setAudioPlayer(this);
    audioPlayerEngine->createEngine(avCodecContext->sample_rate,avCodecContext->channels);
    return true;
}

void AudioPlayer::start() {
    audioPlayerEngine->start();
}

void AudioPlayer::resume() {
    audioPlayerEngine->resume();
}

void AudioPlayer::pause() {
    audioPlayerEngine->pause();
}

void AudioPlayer::seekTo(int64_t timeStamp) {
    isBeginSeeking = true;
    seekTime = (timeStamp/1000)*(timeBase->den/timeBase->num);
    AUDIO_LOG_I("seekTo:%lld",timeStamp);
}

void AudioPlayer::stop() {
    audioPlayerEngine->stop();
    release();
}

int64_t AudioPlayer::getCurrentPlayTime() {
    return currentPlayTime*av_q2d(*timeBase)*1000;
}

int64_t AudioPlayer::getDuration() {
    return duration*av_q2d(*timeBase)*1000;
}

void AudioPlayer::fastForward() {
    isBeginSeeking = true;
    //Total duration which is millisecond.
    int64_t duration = getDuration();
    //Current millisecond;
    int64_t currentPlayTime = getCurrentPlayTime();
    int64_t newTimeStamp= currentPlayTime + DEFAULT_SEEK_SHORT_TIME < duration?
            currentPlayTime + DEFAULT_SEEK_SHORT_TIME : duration;
    seekTime = (newTimeStamp/1000)*(timeBase->den/timeBase->num);
    AUDIO_LOG_I("fastForward:%lld currentPlayTime:%lld duration:%lld",newTimeStamp,currentPlayTime,duration);
}

void AudioPlayer::backward() {
    isBeginSeeking = true;
    //Current millisecond;
    int64_t currentPlayTime = getCurrentPlayTime();
    int64_t newTimeStamp = currentPlayTime - DEFAULT_SEEK_SHORT_TIME > 0?
               currentPlayTime - DEFAULT_SEEK_SHORT_TIME : 0;
    seekTime = (newTimeStamp/1000)*(timeBase->den/timeBase->num);
    AUDIO_LOG_I("backward:%lld",newTimeStamp);
}

void AudioPlayer::release() {
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
AudioPlayerEngine::AudioPlayerEngine() {
}

AudioPlayerEngine::~AudioPlayerEngine(){
    //Do not delete the pointer: audio player.
//    delete[] buffer;
}

void *AudioPlayerEngine::getBuffer() const {
    return buffer;
}

void AudioPlayerEngine::setBuffer(void *buffer) {
    AudioPlayerEngine::buffer = buffer;
}

size_t AudioPlayerEngine::getBufferSize() const {
    return bufferSize;
}

void AudioPlayerEngine::setBufferSize(size_t bufferSize) {
    AudioPlayerEngine::bufferSize = bufferSize;
}

AudioPlayer *AudioPlayerEngine::getAudioPlayer() const {
    return audioPlayer;
}

void AudioPlayerEngine::setAudioPlayer(AudioPlayer *audioPlayer) {
    AudioPlayerEngine::audioPlayer = audioPlayer;
}

void AudioPlayerEngine::createEngine(unsigned int rate, unsigned int channels) {
}

void AudioPlayerEngine::start() {
}

void AudioPlayerEngine::resume() {
}

void AudioPlayerEngine::pause() {
}

void AudioPlayerEngine::stop() {
}

//--------------------------------------------------------------------------------
// Implementation of the class: OpenSLAudioPlayerEngine
//--------------------------------------------------------------------------------
OpenSLAudioPlayerEngine::OpenSLAudioPlayerEngine():AudioPlayerEngine(){
    engineObject=NULL;
    engineEngine = NULL;
    outputMixObject = NULL;
    outputMixEnvironmentalReverb = NULL;
    settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    playerObject=NULL;
    playerInterface=NULL;
    slBufferQueueItf=NULL;
}

OpenSLAudioPlayerEngine::~OpenSLAudioPlayerEngine(){
    delete engineObject;
    delete engineEngine;
    delete outputMixObject;
    delete outputMixEnvironmentalReverb;
    delete playerObject;
    delete playerInterface;
    delete slBufferQueueItf;
}

void OpenSLAudioPlayerEngine::playerQueueCallBack(SLAndroidSimpleBufferQueueItf slBufferQueueItf,void *context) {
    OpenSLAudioPlayerEngine* engine=(OpenSLAudioPlayerEngine*) context;
    auto audioPlayer=engine->getAudioPlayer();
    void *buffer=engine->getBuffer();
    auto bufferSize=engine->getBufferSize();
    audioPlayer->readFrame(&buffer,&bufferSize);
    if(buffer!=NULL&&bufferSize!=0){
        (*slBufferQueueItf)->Enqueue(slBufferQueueItf,buffer,bufferSize);
    }
}

void OpenSLAudioPlayerEngine::createEngine(unsigned int rate,unsigned int channels) {
    //Create audio engine.
    AUDIO_LOG_I("OpenSLAudioPlayerEngine::createEngine rate:%d channels:%d",rate,channels);
    slCreateEngine(&engineObject,0,NULL,0,NULL,NULL);//创建引擎
    (*engineObject)->Realize(engineObject,SL_BOOLEAN_FALSE);
    (*engineObject)->GetInterface(engineObject,SL_IID_ENGINE,&engineEngine);

    //Create audio mixer.
    AUDIO_LOG_I("OpenSLAudioPlayerEngine::Create audio mixer.");
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
    (*slBufferQueueItf)->RegisterCallback(slBufferQueueItf,playerQueueCallBack,this);
}

void OpenSLAudioPlayerEngine::start() {
    AUDIO_LOG_I("start");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_PLAYING);
    playerQueueCallBack(slBufferQueueItf,this);
}

void OpenSLAudioPlayerEngine::pause() {
    AUDIO_LOG_I("pause");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_PAUSED);
}

void OpenSLAudioPlayerEngine::resume() {
    AUDIO_LOG_I("resume");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_PLAYING);
}

void OpenSLAudioPlayerEngine::stop() {
    AUDIO_LOG_I("stop");
    (*playerInterface)->SetPlayState(playerInterface,SL_PLAYSTATE_STOPPED);
}
