package com.cz.android.media.ffmpeg.video.image

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.media.ffmpeg.video.R
import com.cz.android.media.ffmpeg.video.utils.Util
import kotlinx.android.synthetic.main.activity_video_image_sample.*

class VideoImageSampleActivity : AppCompatActivity() {
    private val handler= Handler(Looper.getMainLooper())
    private val updateAction: Runnable = object : Runnable {
        override fun run() {
            val currentPlayTime: Long = videoView.currentDuration
            val seekBar = findViewById<SeekBar>(R.id.seekBar)
            seekBar.progress = currentPlayTime.toInt()
            progressText.text= Util.getStringForTime(currentPlayTime)
            handler.postDelayed(this, 200)
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_image_sample)
        val file = Util.copyTempFileAsset(this,"sample3.mp4")
        videoView.loadImage(file)
        videoView.decodeFrame(0)
        seekBar.max= videoView.duration.toInt()
        durationText.text= Util.getStringForTime(videoView.duration)

        seekBar.setOnTouchListener { _, _ -> true }
        seekBar.setOnSeekBarChangeListener(object :SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
            }
        })

        playerPlayButton.setOnClickListener {
            playerPlayButton.visibility = View.GONE
            playerPauseButton.visibility = View.VISIBLE
            handler.postDelayed(updateAction,200)
            videoView.startAnimation()
        }
        playerPauseButton.setOnClickListener {
            playerPlayButton.visibility = View.VISIBLE
            playerPauseButton.visibility = View.GONE
            handler.removeCallbacks(updateAction)
            videoView.stopAnimation()
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(updateAction)
        super.onDestroy()
    }
}