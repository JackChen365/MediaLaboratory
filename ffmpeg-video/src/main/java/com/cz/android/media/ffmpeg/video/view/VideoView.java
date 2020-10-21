package com.cz.android.media.ffmpeg.video.view;

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
    private static final String TAG="GifView";
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
                long dst;
                if((dst=decoder.decodeFrame(currentFrame)) < 0){
                    break;
                }
                long time=SystemClock.uptimeMillis()-st;
                long delta=dst - time > 0? dst-time:0;
                SystemClock.sleep(delta);
                postInvalidate();
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

    public long getFrameCount(){
        return decoder.getFrameCount();
    }

    public void decodeFrame(int index){
        if(decoder.isValid()){
            frameIndex = index;
            if(0 <= decoder.fillFrame(currentFrame,frameIndex)){
                invalidate();
            }
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

    public void loadImage(String filePath) {
        loadImage(new File(filePath));
    }

    public void recycle(){
        decoder.recycle();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec,heightMeasureSpec);
        int measuredWidth = getMeasuredWidth();
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
