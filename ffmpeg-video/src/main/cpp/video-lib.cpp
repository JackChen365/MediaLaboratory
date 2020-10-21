//
// Created by Jack Chen on 10/17/2020.
//
#include <jni.h>
#include "video/VideoPlayer.h"

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nLoadFile(JNIEnv *env, jobject thiz,
                                                              jstring file_path) {
    const char* filePath=env->GetStringUTFChars(file_path,0);
    VideoPlayer* player=new VideoPlayer();
    //Start load the audio file. return 0 if success, the others mean failed.
    if(player->loadFile(filePath)){
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
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nStart(JNIEnv *env, jobject thiz, jlong ref,jobject surface) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nPause(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nResume(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nStop(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->stop();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nSeekTo(JNIEnv *env, jobject thiz, jlong ref, jlong time_stamp) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->seekTo(time_stamp);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nGetCurrentPlayTime(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    return player->getCurrentPlayTime();
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_ffmpeg_sample_video_VideoPlayer_nGetDuration(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    return player->getDuration();
}