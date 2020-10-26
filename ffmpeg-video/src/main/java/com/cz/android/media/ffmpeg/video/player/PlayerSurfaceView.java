package com.cz.android.media.ffmpeg.video.player;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;


/**
 * @author Created by cz
 * @date 2020/10/15 3:32 PM
 * @email bingo110@126.com
 */
public class PlayerSurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    private final VideoPlayer videoPlayer=new VideoPlayer();
    /**
     * If you call the function {@link #start()}, But the surface hasn't created yet.
     * It will be pending this behavior until the surface is creating.
     */
    private boolean isPendingPlay;
    private boolean isCreating;

    public PlayerSurfaceView(Context context) {
        this(context,null,0);
    }

    public PlayerSurfaceView(Context context, AttributeSet attrs) {
        this(context,attrs,0);
    }

    public PlayerSurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        SurfaceHolder holder = getHolder();
        holder.addCallback(this);
    }

    public boolean loadFile(String filePath){
        SurfaceHolder holder = getHolder();
        Surface surface = holder.getSurface();
        return videoPlayer.prepare(filePath,surface);
    }

    /**
     * Start play the video.
     * @return
     */
    public void start(){
        SurfaceHolder holder = getHolder();
        if(isCreating){
            int width = videoPlayer.getWidth();
            int height = videoPlayer.getHeight();
            holder.setFixedSize(width,height);
            videoPlayer.start();
        }
        isPendingPlay = holder.isCreating();
    }

    public void pause(){
        videoPlayer.pause();
    }

    public void resume(){
        videoPlayer.resume();
    }

    public void stop(){
        videoPlayer.stop();
    }

    public long getDuration(){
        return videoPlayer.getDuration();
    }

    public long getCurrentPlayTime(){
        return videoPlayer.getCurrentPlayTime();
    }

    public void seekTo(long timeStamp){
        videoPlayer.seekTo(timeStamp);
    }

    public boolean isValid(){
        return videoPlayer.isValid();
    }

    public void fastForward(){
        videoPlayer.fastForward();
    }

    public void rewind(){
        videoPlayer.rewind();
    }

    public void release(){
        videoPlayer.release();
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
        isCreating = true;
        if(isPendingPlay){
            isPendingPlay = false;
            int width = videoPlayer.getWidth();
            int height = videoPlayer.getHeight();
            surfaceHolder.setFixedSize(width,height);
            videoPlayer.start();
        }
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int format, int width,int height) {

    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
        isCreating = false;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
