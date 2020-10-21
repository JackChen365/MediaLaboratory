package com.cz.android.media.ffmpeg.audio

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.SeekBar
import android.widget.SeekBar.OnSeekBarChangeListener
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_audio_sample.*
import java.io.File
import java.io.FileOutputStream

class AudioSampleActivity : AppCompatActivity() {
    private val handler= Handler(Looper.getMainLooper())
    private val audioPlayer = AudioPlayer()
    private val updateAction: Runnable = object : Runnable {
        override fun run() {
            val currentPlayTime: Long = audioPlayer.currentPlayTime
            val seekBar = findViewById<SeekBar>(R.id.seekBar)
            seekBar.progress = currentPlayTime.toInt()
            handler.postDelayed(this, 200)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_audio_sample)
        val seekBar = findViewById<SeekBar>(R.id.seekBar)
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
            val tempImageFile = getTempImageFile("Take_My_Hand.mp3")
            if(audioPlayer.loadFile(tempImageFile?.absolutePath)){
                audioPlayer.start()
                val duration = audioPlayer.duration.toInt()
                seekBar.max = duration
                handler.post(updateAction)
            }
        }
        pauseButton.setOnClickListener { audioPlayer.pause() }
        resumeButton.setOnClickListener { audioPlayer.resume() }
    }

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
}