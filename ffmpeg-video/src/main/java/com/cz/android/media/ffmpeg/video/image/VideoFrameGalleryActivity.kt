package com.cz.android.media.ffmpeg.video.image

import android.os.Bundle
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.media.ffmpeg.video.R
import com.cz.android.media.ffmpeg.video.image.adapter.VideoFrameAdapter
import kotlinx.android.synthetic.main.activity_video_frame_gallery.*
import java.io.File

class VideoFrameGalleryActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_frame_gallery)
        val tempImageFile = getTempImageFile("sample2.mp4")
        val videoFrameAdapter = VideoFrameAdapter(this, tempImageFile.absolutePath)
        gallery.adapter = VideoFrameAdapter(this,tempImageFile.absolutePath)
        gallery.setPageTransformer { page, fraction ->
            page.scaleX = 0.9f+0.1f*fraction
            page.scaleY = 0.9f+0.1f*fraction
            page.alpha=0.6f+0.4f*fraction
        }
        seekBar.max = videoFrameAdapter.itemCount
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                gallery.currentItem = progress
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
            }
        })
    }

    private fun getTempImageFile(assetFileName: String): File {
        val inputStream = assets.open(assetFileName)
        val readBytes = inputStream.readBytes()
        val file = File.createTempFile("tmp", ".mp4")
        file.writeBytes(readBytes)
        file.deleteOnExit()
        return file
    }
}