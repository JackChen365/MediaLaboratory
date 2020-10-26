//
// Created by Jack Chen on 10/17/2020.
//
#include <jni.h>
#include "VideoPlayer.h"

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nAllocatePlayer(JNIEnv *env,jobject thiz) {

    VideoPlayer* player=new VideoPlayer();
    return (jlong)player;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nPrepare(JNIEnv *env, jobject thiz,
                                                                   jlong ref, jstring file_path,jobject surface) {
    VideoPlayer* player=(VideoPlayer*)ref;
    const char* filePath=env->GetStringUTFChars(file_path,0);
    //Start load the audio file. return 0 if success, the others mean failed.
    player->setDataSource(filePath);
    if(player->prepare()){
        env->ReleaseStringUTFChars(file_path,filePath);
        //Return the object pointer.
        return true;
    }
    env->ReleaseStringUTFChars(file_path,filePath);
    //Return zero means load failed.
    return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nPrepareWindow(JNIEnv *env, jobject thiz,
                                                                          jlong ref,
                                                                          jobject surface) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->prepareSurface(env,surface);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nSurfaceDestroy(JNIEnv *env, jobject thiz,
                                                                          jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->surfaceDestroy();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nStart(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nPause(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nResume(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nStop(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->stop();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nSeekTo(JNIEnv *env, jobject thiz, jlong ref, jlong time_stamp) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->seekTo(time_stamp);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nGetCurrentPlayTime(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    return player->getCurrentPlayTime();
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nGetDuration(JNIEnv *env, jobject thiz, jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    return player->getDuration();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nGetWidth(JNIEnv *env, jobject thiz,
                                                                    jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    return player->getWidth();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nGetHeight(JNIEnv *env, jobject thiz,
                                                                     jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    return player->getHeight();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nRelease(JNIEnv *env, jobject thiz,
                                                                   jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->release();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nFastForward(JNIEnv *env, jobject thiz,
                                                                       jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->fastForward();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_cz_android_media_ffmpeg_video_player_VideoPlayer_nRewind(JNIEnv *env, jobject thiz,
                                                                  jlong ref) {
    VideoPlayer* player=(VideoPlayer*)ref;
    player->rewind();
}