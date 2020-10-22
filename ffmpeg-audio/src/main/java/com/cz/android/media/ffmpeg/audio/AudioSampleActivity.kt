package com.cz.android.media.ffmpeg.audio

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.SeekBar
import android.widget.SeekBar.OnSeekBarChangeListener
import androidx.appcompat.app.AppCompatActivity
import com.cz.android.media.ffmpeg.audio.decode.AudioPlayer
import com.cz.android.media.ffmpeg.audio.utils.Util
import kotlinx.android.synthetic.main.activity_audio_sample.*
import java.io.File
import java.io.FileOutputStream

/**
 * This activity illustrate how we use the FFMPEG to player audio.
 * The audio player I implemented has a few common features.
 *
 * @see AudioPlayer.prepare
 * @see AudioPlayer.start
 * @see AudioPlayer.pause
 * @see AudioPlayer.resume
 * @see AudioPlayer.stop
 * @see AudioPlayer.release
 * @see AudioPlayer.fastForward
 * @see AudioPlayer.backward
 * @see AudioPlayer.seekTo
 * @see AudioPlayer This is a jni bright class.
 */
class AudioSampleActivity : AppCompatActivity() {
    private val handler= Handler(Looper.getMainLooper())
    private val audioPlayer = AudioPlayer()
    private val updateAction: Runnable = object : Runnable {
        override fun run() {
            val currentPlayTime: Long = audioPlayer.currentPlayTime
            val seekBar = findViewById<SeekBar>(R.id.seekBar)
            seekBar.progress = currentPlayTime.toInt()
            musicPlayerView.progress = currentPlayTime.toInt()
            progressText.text= Util.getStringForTime(currentPlayTime)
            handler.postDelayed(this, 200)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_audio_sample)
        musicPlayerView.setCoverDrawable(R.mipmap.icon_cover)
        musicPlayerView.setOnClickListener {
            if(!audioPlayer.isValid){
                initialAudioPlayer()
            } else if (musicPlayerView.isRotating) {
                musicPlayerView.stop()
                audioPlayer.pause()
                updateStopVisibility()
            } else {
                musicPlayerView.start()
                audioPlayer.resume()
                updatePlayVisibility()
            }
        }
        seekBar.isEnabled=false
        backwardButton.isEnabled = false
        fastForwardButton.isEnabled = false
        seekBar.setOnSeekBarChangeListener(object : OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    audioPlayer.seekTo(progress.toLong())
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar) {}
            override fun onStopTrackingTouch(seekBar: SeekBar) {}
        })
        playButton.setOnClickListener {
            if(!audioPlayer.isValid){
                initialAudioPlayer()
            } else {
                seekBar.isEnabled=true
                audioPlayer.resume()
                musicPlayerView.start()
                updatePlayVisibility()
            }
        }
        pauseButton.setOnClickListener {
            seekBar.isEnabled=false
            audioPlayer.pause()
            musicPlayerView.stop()
            updateStopVisibility()
        }

        backwardButton.setOnClickListener {
            audioPlayer.backward()
        }
        fastForwardButton.setOnClickListener {
            audioPlayer.fastForward()
        }
    }

    /**
     * Initialize the audio player.
     */
    private fun initialAudioPlayer() {
        //Copy file from assets and make it as a temp file.
        val tempImageFile = getTempImageFile("Take_My_Hand.mp3")
        if (audioPlayer.prepare(tempImageFile?.absolutePath)) {
            val duration = audioPlayer.duration
            seekBar.isEnabled=true
            backwardButton.isEnabled = true
            fastForwardButton.isEnabled = true
            seekBar.max = duration.toInt()
            musicPlayerView.setMax(duration.toInt())
            durationText.text = Util.getStringForTime(duration)
            audioPlayer.start()
            musicPlayerView.start()
            updatePlayVisibility()
            handler.post(updateAction)
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
        if(audioPlayer.isValid){
            musicPlayerView.start()
            audioPlayer.resume()
            updatePlayVisibility()
        }
    }

    override fun onPause() {
        super.onPause()
        if(audioPlayer.isValid){
            audioPlayer.pause()
            musicPlayerView.stop()
            updateStopVisibility()
        }
    }

    override fun onDestroy() {
        audioPlayer.release()
        super.onDestroy()
    }
}