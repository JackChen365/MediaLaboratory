//
// Created by Jack Chen on 10/17/2020.
//
#include <jni.h>
#include "AudioPlayer.h"

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nPrepare(JNIEnv *env, jobject thiz,
                                                              jstring file_path) {
    const char* filePath=env->GetStringUTFChars(file_path,0);
    AudioPlayer* player=new AudioPlayer();
    //Start load the audio file. return 0 if success, the others mean failed.
    if(player->prepare(filePath)){
        env->ReleaseStringUTFChars(file_path,filePath);
        //Return the object pointer.
        return (jlong)player;
    }
    env->ReleaseStringUTFChars(file_path,filePath);
    //Return zero means load failed.
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nStart(JNIEnv *env, jobject thiz, jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nPause(JNIEnv *env, jobject thiz, jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nResume(JNIEnv *env, jobject thiz, jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nStop(JNIEnv *env, jobject thiz, jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->stop();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nSeekTo(JNIEnv *env, jobject thiz, jlong ref, jlong time_stamp) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->seekTo(time_stamp);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nGetCurrentPlayTime(JNIEnv *env, jobject thiz, jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    return player->getCurrentPlayTime();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nGetDuration(JNIEnv *env, jobject thiz, jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    return player->getDuration();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nFastForward(JNIEnv *env, jobject thiz,jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->fastForward();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nBackward(JNIEnv *env, jobject thiz,jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->rewind();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_audio_decode_AudioPlayer_nRelease(JNIEnv *env, jobject thiz,jlong ref) {
    AudioPlayer* player=(AudioPlayer*)ref;
    player->release();
}