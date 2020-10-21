package com.cz.android.ffmpeg.sample.image

import android.os.Bundle
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.ffmpeg.sample.R
import kotlinx.android.synthetic.main.activity_video_image_sample.*
import java.io.File

class VideoImageSampleActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_image_sample)
        val file = getTempImageFile("sample3.mp4")
        videoView.loadImage(file)
        videoView.decodeFrame(0)
        seekBar.max= videoView.frameCount.toInt()
        seekBar.setOnSeekBarChangeListener(object :SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                videoView.decodeFrame(progress)
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
            }
        })
        startButton.setOnClickListener {
            videoView.startAnimation()
        }
        stopButton.setOnClickListener {
            videoView.stopAnimation()
        }

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