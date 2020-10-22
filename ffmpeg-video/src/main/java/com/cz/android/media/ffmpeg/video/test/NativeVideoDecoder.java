package com.cz.android.media.ffmpeg.video.test;

import android.graphics.Bitmap;

public class NativeVideoDecoder {
    static {
        System.loadLibrary("avutil");
        System.loadLibrary("swresample");
        System.loadLibrary("avcodec");
        System.loadLibrary("swscale");
        System.loadLibrary("avformat");
        System.loadLibrary("avfilter");
        System.loadLibrary("avdevice");
        System.loadLibrary("video-test-lib");
    }
    private long decoderRef;

    private native long nLoadFile(String filePath);

    private native int nGetWidth(long ref);

    private native int nGetHeight(long ref);

    private native long nGetDuration(long ref);

    private native int nGetFrameCount(long ref);

    private native long nDecodeFrame(long ref, Bitmap bitmap);

    private native long nFillFrame(long ref, Bitmap bitmap, int index);

    private native void nRecycle(long ref);

    public boolean isValid(){
        return 0!=decoderRef;
    }

    public boolean loadFile(String filePath){
        decoderRef=nLoadFile(filePath);
        return 0!=decoderRef;
    }

    public int getWidth(){
        if(0 == decoderRef){
            throw new IllegalArgumentException("The decoder reference is null, Please initial the decoder with the method loadFile.");
        }
        return nGetWidth(decoderRef);
    }

    public int getHeight(){
        if(0 == decoderRef){
            throw new IllegalArgumentException("The decoder reference is null, Please initial the decoder with the method loadFile.");
        }
        return nGetHeight(decoderRef);
    }

    public long getDuration(){
        if(0 == decoderRef){
            throw new IllegalArgumentException("The decoder reference is null, Please initial the decoder with the method loadFile.");
        }
        return nGetDuration(decoderRef);
    }

    public long getFrameCount(){
        if(0 == decoderRef){
            throw new IllegalArgumentException("The decoder reference is null, Please initial the decoder with the method loadFile.");
        }
        return nGetFrameCount(decoderRef);
    }

    public long fillFrame(Bitmap bitmap, int index){
        if(0 == decoderRef){
            throw new IllegalArgumentException("The decoder reference is null, Please initial the decoder with the method loadFile.");
        }
        return nFillFrame(decoderRef,bitmap,index);
    }

    public long decodeFrame(Bitmap bitmap){
        if(0 == decoderRef){
            throw new IllegalArgumentException("The decoder reference is null, Please initial the decoder with the method loadFile.");
        }
        return nDecodeFrame(decoderRef,bitmap);
    }

    public void recycle(){
        if(0 != decoderRef){
            nRecycle(decoderRef);
            decoderRef = 0;
        }
    }
}
