package com.cz.android.media.ffmpeg.video.player

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.media.ffmpeg.video.R
import kotlinx.android.synthetic.main.activity_video_player.pauseButton
import kotlinx.android.synthetic.main.activity_video_player.playButton
import kotlinx.android.synthetic.main.activity_video_player.resumeButton
import kotlinx.android.synthetic.main.activity_video_player.videoView
import java.io.File

class VideoPlayerActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_player)
//        seekBar.max= videoView.frameCount.toInt()
//        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener{
//            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
//                videoView.decodeFrame(progress)
//            }
//
//            override fun onStartTrackingTouch(seekBar: SeekBar) {
//            }
//
//            override fun onStopTrackingTouch(seekBar: SeekBar) {
//            }
//        })
        playButton.setOnClickListener {
            val file = getTempImageFile("sample3.mp4")
            if(videoView.loadFile(file?.absolutePath)){
                videoView.start()
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