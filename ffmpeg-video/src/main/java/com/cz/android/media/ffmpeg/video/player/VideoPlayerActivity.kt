package com.cz.android.media.ffmpeg.video.player

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.media.ffmpeg.video.R
import com.cz.android.media.ffmpeg.video.utils.Util
import kotlinx.android.synthetic.main.activity_video_player.*
import java.io.File
import java.io.FileOutputStream

class VideoPlayerActivity : AppCompatActivity() {
    private val handler= Handler(Looper.getMainLooper())
    private val updateAction: Runnable = object : Runnable {
        override fun run() {
            val currentPlayTime: Long = surfaceView.currentPlayTime
            val seekBar = findViewById<SeekBar>(R.id.seekBar)
            seekBar.progress = currentPlayTime.toInt()
            progressText.text= Util.getStringForTime(currentPlayTime)
            handler.postDelayed(this, 200)
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video_player)

        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                surfaceView.seekTo(seekBar.progress.toLong())
            }
        })
        playButton.setOnClickListener {
            if(!surfaceView.isValid){
                initialVideoPlayer()
            } else {
                surfaceView.resume()
                updatePlayVisibility()
            }
        }
        pauseButton.setOnClickListener {
            surfaceView.pause()
            updateStopVisibility()
        }

        fastForwardButton.setOnClickListener {
            surfaceView.fastForward()
        }
        rewindButton.setOnClickListener {
            surfaceView.rewind()
        }

    }

    private fun initialVideoPlayer() {
        val file = getTempImageFile("sample3.mp4")
        if (surfaceView.loadFile(file?.absolutePath)) {
            surfaceView.start()
            seekBar.max = surfaceView.duration.toInt()
            durationText.text=Util.getStringForTime(surfaceView.duration)
            handler.post(updateAction)
            updatePlayVisibility()
        }
    }

    /**
     * Start play.
     */
    private fun updatePlayVisibility() {
        playButton.visibility = View.GONE
        pauseButton.visibility = View.VISIBLE
    }

    /**
     * Stop the audio player.
     */
    private fun updateStopVisibility() {
        playButton.visibility = View.VISIBLE
        pauseButton.visibility = View.GONE
    }

    /**
     * Copy a temporary file from assets.
     */
    private fun getTempImageFile(assetFileName: String): File? {
        val file = File.createTempFile("tmp", ".mp4")
        assets.open(assetFileName).use { inputStream ->
            val available = inputStream.available()
            val bytes = ByteArray(available)
            inputStream.read(bytes)
            val fileInputStream = FileOutputStream(file)
            fileInputStream.write(bytes)
            fileInputStream.close()
            file.deleteOnExit()
        }
        return file
    }

    override fun onResume() {
        super.onResume()
        if(surfaceView.isValid){
            surfaceView.resume()
            updatePlayVisibility()
        }
    }

    override fun onPause() {
        super.onPause()
        if(surfaceView.isValid){
            surfaceView.pause()
            updateStopVisibility()
        }
    }

    override fun onDestroy() {
        surfaceView.release()
        super.onDestroy()
    }
}