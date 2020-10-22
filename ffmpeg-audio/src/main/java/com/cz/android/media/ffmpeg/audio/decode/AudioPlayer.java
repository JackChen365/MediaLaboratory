package com.cz.android.media.ffmpeg.audio.decode;

/**
 * Created by jack in 2020/10/17
 *
 * This is a jni bright work with audio-lib.
 * The class load all the FFMPEG library and the Audio-lib library.
 * All the native methods are private scope. You could be able to call public method I support.
 * @see #prepare(String) load the audio file and prepare all the player resources.
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
     * Prepare all the player resources in native.
     * Return the player pointer from native code. 0 represents success others means failed.
     * @return
     */
    private native long nPrepare(String filePath);

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
    private native void nFastForward(long ref);
    private native void nBackward(long ref);
    private native void nRelease(long ref);


    /**
     * The player object pointer from Native code.
     */
    private long objectRef;

    /**
     * load the audio file and prepare all the player resources in native.
     * @param filePath
     * @return
     */
    public boolean prepare(String filePath){
        objectRef=nPrepare(filePath);
        return INVALID_REF!=objectRef;
    }

    public boolean isValid(){
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

    /**
     * Return the total duration of the playback.
     * @return
     */
    public long getDuration(){
        assetObject();
        return nGetDuration(objectRef);
    }

    /**
     * Return the current play time.
     * @return
     */
    public long getCurrentPlayTime(){
        assetObject();
        return nGetCurrentPlayTime(objectRef);
    }

    /**
     * Seek to a specific position.
     * @param timeStamp
     */
    public void seekTo(long timeStamp){
        assetObject();
        nSeekTo(objectRef,timeStamp);
    }

    /**
     * Move to a short distance like 5s.
     */
    public void fastForward(){
        assetObject();
        nFastForward(objectRef);
    }

    /**
     * Move back to a short distance like 5s.
     */
    public void backward(){
        assetObject();
        nBackward(objectRef);
    }

    /**
     * Release all the resources.
     */
    public void release(){
        assetObject();
        nRelease(objectRef);
    }

    private void assetObject(){
        if(objectRef == INVALID_REF){
            throw new ExceptionInInitializerError("The object reference is not valid.");
        }
    }

}
