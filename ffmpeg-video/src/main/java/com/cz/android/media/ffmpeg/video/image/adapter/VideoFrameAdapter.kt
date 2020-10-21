package com.cz.android.media.ffmpeg.video.image.adapter

import android.content.Context
import android.graphics.Bitmap
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.cz.android.media.ffmpeg.video.R
import com.cz.android.media.ffmpeg.video.test.NativeVideoDecoder
import com.cz.android.media.ffmpeg.video.view.ScaleImageView
import java.util.*

class VideoFrameAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder> {
    private val bitmapList=LinkedList<Bitmap>()
    private val decoder = NativeVideoDecoder()
    private val layoutInflater:LayoutInflater
    private val itemCount:Int

    constructor(context: Context,filePath:String){
        this.layoutInflater= LayoutInflater.from(context)
        if(!decoder.loadFile(filePath)){
            this.itemCount = 0
        } else {
            this.itemCount= decoder.frameCount.toInt()
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        return object :RecyclerView.ViewHolder(layoutInflater.inflate(R.layout.image_layout,parent,false)){}
    }

    override fun getItemCount(): Int {
        return itemCount
    }

    override fun onViewDetachedFromWindow(holder: RecyclerView.ViewHolder) {
        super.onViewDetachedFromWindow(holder)
        val imageView=holder.itemView.findViewById<ScaleImageView>(R.id.imageView)
        if(null!=imageView.bitmap){
            bitmapList.offer(imageView.bitmap)
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        var bitmap = bitmapList.pollFirst()
        if(null==bitmap){
            bitmap=Bitmap.createBitmap(decoder.width,decoder.height,Bitmap.Config.ARGB_8888)
        }
        decoder.fillFrame(bitmap,position)
        val imageView=holder.itemView.findViewById<ScaleImageView>(R.id.imageView)
        val positionText=holder.itemView.findViewById<TextView>(R.id.positionText)
        imageView.setImageBitmap(bitmap)
        positionText.text="$position"
    }
}