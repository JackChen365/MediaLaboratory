package com.cz.android.media.ffmpeg.video.player

import android.os.Bundle
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.media.ffmpeg.video.R
import kotlinx.android.synthetic.main.activity_video_player.*
import java.io.File

class VideoPlayerActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_player)

        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                videoView.seekTo(seekBar.progress.toLong())
            }
        })
        playButton.setOnClickListener {
            val file = getTempImageFile("sample3.mp4")
            if(videoView.loadFile(file?.absolutePath)){
                videoView.start()
                seekBar.max= videoView.duration.toInt()
            }
        }
        pauseButton.setOnClickListener { videoView.pause() }
        resumeButton.setOnClickListener { videoView.resume() }

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