package com.cz.android.media.ffmpeg.audio;

/**
 * Created by jack in 2020/10/17
 *
 * This is a jni bright work with audio-lib.
 * The class load all the FFMPEG library and the Audio-lib library.
 * All the native methods are private scope. You could be able to call public method I support.
 * @see #loadFile(String) load the audio file and prepare all the player resources.
 * @see #start() start play the audio
 * @see #pause() Temporarily pause the player.
 * @see #resume()  resume from the pause state and continue to play.
 * @see #stop() Stop play and release all the player resources.
 *
 * I hold the native pointer which is the AudioPlayer in cpp.
 * Be careful if you call {@link #stop()} I will release all the resources.
 */
public class AudioPlayer {
    static {
        System.loadLibrary("avutil");
        System.loadLibrary("swresample");
        System.loadLibrary("swscale");
        System.loadLibrary("avcodec");
        System.loadLibrary("avformat");
        System.loadLibrary("avfilter");
        System.loadLibrary("avdevice");
        System.loadLibrary("audio-lib");
    }
    private static final int INVALID_REF=0;

    /**
     * load the audio file and prepare all the player resources in native.
     * Return the player pointer from native code. 0 represents success others means failed.
     * @param filePath
     * @return
     */
    private native long nLoadFile(String filePath);

    /**
     * Start play the audio.
     * @param ref
     */
    private native void nStart(long ref);

    /**
     * Temporarily pause the player.
     * @param ref
     */
    private native void nPause(long ref);

    /**
     * If the player play state is pausing. continue to play.
     * @param ref
     */
    private native void nResume(long ref);

    /**
     * Stop the player and release all the player resources.
     * @param ref
     */
    private native void nStop(long ref);

    private native long nGetDuration(long ref);

    private native long nGetCurrentPlayTime(long ref);

    private native void nSeekTo(long ref,long timeStamp);

    /**
     * The player object pointer from Native code.
     */
    private long objectRef;

    /**
     * load the audio file and prepare all the player resources in native.
     * @param filePath
     * @return
     */
    public boolean loadFile(String filePath){
        objectRef=nLoadFile(filePath);
        return INVALID_REF!=objectRef;
    }

    /**
     * Start play the audio.
     */
    public void start(){
        assetObject();
        nStart(objectRef);
    }

    /**
     * Temporarily pause the player.
     */
    public void pause(){
        assetObject();
        nPause(objectRef);
    }

    /**
     * If the player play state is pausing. continue to play.
     */
    public void resume(){
        assetObject();
        nResume(objectRef);
    }

    /**
     * Stop the player and release all the player resources.
     */
    public void stop(){
        assetObject();
        nStop(objectRef);
    }

    public long getDuration(){
        assetObject();
        return nGetDuration(objectRef);
    }

    public long getCurrentPlayTime(){
        assetObject();
        return nGetCurrentPlayTime(objectRef);
    }

    public void seekTo(long timeStamp){
        assetObject();
        nSeekTo(objectRef,timeStamp);
    }

    private void assetObject(){
        if(objectRef == INVALID_REF){
            throw new ExceptionInInitializerError("The object reference is not valid.");
        }
    }

}
