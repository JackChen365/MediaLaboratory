package com.cz.android.media.ffmpeg.video.test;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.View;

import com.cz.android.media.ffmpeg.video.test.NativeVideoDecoder;

import java.io.File;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class VideoView extends View {
    private NativeVideoDecoder decoder=new NativeVideoDecoder();
    private Matrix matrix=new Matrix();
    private Bitmap currentFrame;
    private ExecutorService executors= Executors.newSingleThreadExecutor();
    private volatile boolean isRunning=false;
    private int frameIndex;
    private float scale=1f;

    private Runnable renderRunnable=new Runnable() {
        @Override
        public void run() {
            long st = SystemClock.uptimeMillis();
            while(isRunning){
                decoder.decodeFrame(currentFrame);
                long dst=decoder.getCurrentDuration();
                if(dst < 0){
                    break;
                }
                long time=SystemClock.uptimeMillis()-st;
                long delta=dst - time > 0? dst-time:0;
                if(delta > 1000){
                    //Need to synchronize the time stamp and skip
                    st = SystemClock.uptimeMillis() - dst;
                } else {
                    SystemClock.sleep(delta);
                    invalidate();
                }
            }
        }
    };

    public VideoView(Context context) {
        super(context);
    }

    public VideoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public VideoView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void loadImage(File imageFile) {
        if(imageFile.exists()){
            decoder.loadFile(imageFile.getAbsolutePath());
            int width = decoder.getWidth();
            int height = decoder.getHeight();
            currentFrame=Bitmap.createBitmap(width,height, Bitmap.Config.ARGB_8888);
            requestLayout();
        }
    }

    public long getDuration(){
        return decoder.getDuration();
    }

    public long getFrameCount(){
        return decoder.getFrameCount();
    }

    public void decodeFrame(int index){
        if(decoder.isValid()){
            frameIndex = index;
            decoder.fillFrame(currentFrame,frameIndex);
            invalidate();
        }
    }

    public void startAnimation(){
        if(decoder.isValid()){
            frameIndex = 0;
            isRunning = true;
            executors.execute(renderRunnable);
        }
    }

    public void stopAnimation(){
        if(decoder.isValid()){
            isRunning = false;
        }
    }

    public long getCurrentDuration(){
        if(decoder.isValid()){
            return decoder.getCurrentDuration();
        }
        return 0;
    }

    public void loadImage(String filePath) {
        loadImage(new File(filePath));
    }

    public void recycle(){
        decoder.recycle();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec,heightMeasureSpec);
        if(decoder.isValid()){
            int width = decoder.getWidth();
            int height = decoder.getHeight();
            if(0 == width || 0 == height){
                setMeasuredDimension(0,0);
            } else {
                setMeasuredDimension(width, height);
            }
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        stopAnimation();
        decoder.recycle();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if(null!=currentFrame){
            canvas.save();
            matrix.setScale(scale,scale);
            canvas.drawBitmap(currentFrame,matrix,null);
            canvas.restore();
        }
    }

}
