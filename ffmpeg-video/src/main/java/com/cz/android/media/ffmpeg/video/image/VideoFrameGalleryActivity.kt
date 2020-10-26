package com.cz.android.media.ffmpeg.video.image

import android.os.Bundle
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.RecyclerView
import androidx.viewpager2.widget.MarginPageTransformer
import androidx.viewpager2.widget.ViewPager2
import com.cz.android.media.ffmpeg.video.R
import com.cz.android.media.ffmpeg.video.image.adapter.VideoFrameAdapter
import com.cz.android.media.ffmpeg.video.utils.Util
import kotlinx.android.synthetic.main.activity_video_frame_gallery.*

class VideoFrameGalleryActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_frame_gallery)
        val tempImageFile = Util.copyTempFileAsset(this,"sample2.mp4")
        val videoFrameAdapter = VideoFrameAdapter(this, tempImageFile.absolutePath)
        viewPager.adapter = VideoFrameAdapter(this,tempImageFile.absolutePath)
        viewPager.setPageTransformer { page, position ->
            val absPos = Math.abs(position)
            page.alpha = 0.9f+0.1f*(1 - absPos)
            page.scaleY = 0.9f+0.1f*(1 - absPos)
            page.scaleY = 0.9f+0.1f*(1 - absPos)
        }
        val recyclerView = viewPager.getChildAt(0) as RecyclerView
        recyclerView.apply {
            val padding = resources.getDimensionPixelOffset(R.dimen.halfPageMargin) +
                    resources.getDimensionPixelOffset(R.dimen.peekOffset)
            // setting padding on inner RecyclerView puts overscroll effect in the right place
            setPadding(padding, 0, padding, 0)
            clipToPadding = false
        }

        viewPager.registerOnPageChangeCallback(object : ViewPager2.OnPageChangeCallback() {
            override fun onPageSelected(position: Int) {
                super.onPageSelected(position)
                seekBar.progress = position
            }
        })
        seekBar.max = videoFrameAdapter.itemCount
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if(fromUser){
                    viewPager.setCurrentItem(progress,false)
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
            }
        })
    }
}