package com.krisp.krisptestapp

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.Log
import android.view.View
import android.view.WindowManager
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import com.krisp.krisptestapp.databinding.ActivityAudioCaptureBinding
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.FileOutputStream
import java.util.Locale

/**
 * Audio Capture page: Krisp NC on the live microphone for a set duration, like the iOS app's AudioCaptureView, in
 * Realtime mode (every 10 ms frame as it arrives) or Distributed mode (whole blocks of captured audio at once).
 */
class AudioCaptureActivity : KrispPageActivity() {

    companion object {
        private const val TAG = "AudioCapture"
        private const val PROGRESS_INTERVAL_MS = 250L // 4 updates per second, like the file processing page
    }

    private lateinit var binding: ActivityAudioCaptureBinding
    private val handler = Handler(Looper.getMainLooper())

    // The running capture and its output, null when not capturing.
    private var capturer: AudioCapturer? = null
    private var captureOutput: NativeAudioBuffer? = null
    // Buffers being allocated for a capture that is about to start.
    private var isPreparing = false
    // Why a running capture was asked to stop (a status string), null until then.
    private var stopReason: Int? = null

    // The last capture's processed audio (raw floats, in native memory), savable as a WAV.
    private var processedAudio: NativeAudioBuffer? = null
    private var processedSampleRate = 0
    private var processedSaved = false
    private var isSaving = false
    // Stats and low-precision setting of the shown capture, null when there is none.
    private var lastStats: CaptureStats? = null
    private var lastRunLowPrecision: Boolean? = null

    private val isCapturing: Boolean
        get() = capturer != null

    private val progressTick = object : Runnable {
        override fun run() {
            val running = capturer ?: return
            showStats(running.snapshot())
            handler.postDelayed(this, PROGRESS_INTERVAL_MS)
        }
    }

    private val requestMicPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            when {
                granted -> startCapture()
                // Denied for good ("don't ask again", or twice on Android 11+): only App Settings can grant it now.
                !ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.RECORD_AUDIO) ->
                    AlertDialog.Builder(this)
                        .setTitle(R.string.title_capture_error)
                        .setMessage(R.string.msg_no_mic_permission_settings)
                        .setPositiveButton(R.string.btn_app_settings) { _, _ ->
                            startActivity(
                                Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
                                    .setData(Uri.fromParts("package", packageName, null))
                            )
                        }
                        .setNegativeButton(android.R.string.cancel, null)
                        .show()
                else -> showError(getString(R.string.title_capture_error), getString(R.string.msg_no_mic_permission))
            }
        }

    private val saveProcessed = registerForActivityResult(ActivityResultContracts.CreateDocument("audio/wav")) { uri: Uri? ->
        val audio = processedAudio
        if (uri == null || audio == null) {
            if (audio != null) binding.textCaptureStatus.text = getString(R.string.txt_save_canceled)
            return@registerForActivityResult
        }
        val sampleRate = processedSampleRate
        isSaving = true
        binding.textCaptureStatus.text = getString(R.string.txt_saving)
        refreshUi()
        lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) {
                    contentResolver.openFileDescriptor(uri, "w")?.use { pfd ->
                        // Header first, then the samples straight from native memory, on the same file descriptor.
                        // The stream isn't closed: that would close the descriptor the samples still go to.
                        FileOutputStream(pfd.fileDescriptor).write(floatMonoWavHeader(sampleRate, audio.size))
                        audio.writeTo(pfd.fd)
                    } ?: throw IllegalStateException("Unable to open the destination file")
                }
                processedSaved = true
                binding.textCaptureStatus.text = getString(R.string.txt_saved_to, displayName(uri))
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (t: Throwable) {
                processedSaved = false
                binding.textCaptureStatus.text = getString(R.string.txt_save_failed, t.message ?: t.javaClass.simpleName)
                Log.e(TAG, "Saving captured audio failed for $uri", t)
            } finally {
                isSaving = false
                if (isDestroyed) {
                    audio.release() // onDestroy left it to us, it was being written
                } else {
                    refreshUi()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityAudioCaptureBinding.inflate(layoutInflater)
        setContentView(binding.root)
        applySystemBarInsets(binding.root)

        binding.textSdkVersion.text = getString(R.string.txt_sdk_version, KrispSdk.version)

        binding.buttonModelFile.setOnClickListener { openModelPicker() }
        binding.buttonModelFile.setOnLongClickListener {
            openModelFolderPicker()
            true
        }
        binding.buttonCapture.setOnClickListener {
            if (isCapturing) {
                stopCapture(R.string.txt_capture_stopped)
            } else if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
                PackageManager.PERMISSION_GRANTED
            ) {
                startCapture()
            } else {
                requestMicPermission.launch(Manifest.permission.RECORD_AUDIO)
            }
        }
        binding.groupCaptureMode.setOnCheckedChangeListener { _, _ -> refreshUi() }
        binding.buttonSave.setOnClickListener {
            if (processedAudio == null) return@setOnClickListener
            saveProcessed.launch("capture_processed_${processedSampleRate}hz.wav")
        }

        showStats(null)
    }

    override fun onResume() {
        super.onResume()
        // The model is shared: another page may have loaded a different one meanwhile.
        binding.textModelFile.text = modelText()
        showIdleStatus()
        refreshUi()
    }

    override fun onStop() {
        super.onStop()
        // Android gives background apps silence instead of the microphone: the rest of the run would be meaningless.
        if (isCapturing && !isChangingConfigurations) {
            stopCapture(R.string.txt_capture_left_app)
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(progressTick)
        // Nobody will see or save the rest: drop it rather than process it (its output is freed in onCaptureFinished).
        capturer?.cancel()
        if (!isSaving) {
            // A save in progress frees the buffer itself when it's done writing.
            processedAudio?.release()
            processedAudio = null
        }
        super.onDestroy()
    }

    override fun onModelPicked(loaded: Boolean) {
        // Results of the previous model are stale; after a failed load they stay valid (and savable).
        if (loaded) clearProcessed()
        binding.textModelFile.text = modelText()
        showIdleStatus()
        refreshUi()
    }

    private val selectedMode: CaptureMode
        get() = if (binding.radioDistributed.isChecked) CaptureMode.DISTRIBUTED else CaptureMode.REALTIME

    private fun startCapture() {
        if (isCapturing || isPreparing || !KrispSdk.modelLoaded) return

        // Everything is checked before the previous capture is dropped: a start that fails here changes nothing.
        val durationMinutes = binding.editDuration.text.toString().trim().toIntOrNull()
        if (durationMinutes == null || durationMinutes < 1) {
            showError(getString(R.string.title_capture_error), getString(R.string.msg_invalid_duration))
            return
        }
        val mode = selectedMode
        val blockMinutes = if (mode == CaptureMode.DISTRIBUTED) {
            val block = binding.editBlockSize.text.toString().trim().toDoubleOrNull()
            // Like the iOS app: a block of ~0 minutes is an error.
            if (block == null || block < 0.005 || block > durationMinutes) {
                showError(getString(R.string.title_capture_error), getString(R.string.msg_invalid_block_size))
                return
            }
            block
        } else {
            0.0
        }
        val sampleRate = AudioCapturer.chooseSampleRate()
        if (sampleRate == 0) {
            showError(getString(R.string.title_capture_error), getString(R.string.msg_no_sample_rate))
            return
        }
        val outputBytes = AudioCapturer.outputBytes(sampleRate, durationMinutes)
        val blockCount = if (mode == CaptureMode.DISTRIBUTED) AudioCapturer.BLOCK_COUNT else 0
        val blockFrames = if (mode == CaptureMode.DISTRIBUTED) AudioCapturer.blockFrames(blockMinutes) else 0
        val blockBytes = if (mode == CaptureMode.DISTRIBUTED) AudioCapturer.blockBytes(sampleRate, blockMinutes) else 0L
        val lowPrecision = binding.checkboxLowPrecision.isChecked

        // Free the previous capture first: its memory counts towards what's available for the next one.
        clearProcessed()
        memoryShortageFor(outputBytes + blockBytes, R.string.msg_memory_advice_capture)?.let { shortage ->
            showIdleStatus()
            refreshUi()
            showError(getString(R.string.title_capture_error), shortage)
            return
        }

        // Everything the run needs is allocated (and touched) before capture, and its clock, start. Hundreds of MB
        // take a moment: off the UI thread, with the controls locked.
        isPreparing = true
        binding.textCaptureStatus.text = getString(R.string.txt_capture_preparing)
        refreshUi()
        KrispSdk.takeLastError() // drop older errors: only this run's should show up in its error dialog
        lifecycleScope.launch {
            var output: NativeAudioBuffer? = null
            var run = 0L
            try {
                withContext(Dispatchers.Default) {
                    output = NativeAudioBuffer.allocate(outputBytes)
                    run = KrispSdk.startCaptureRun(sampleRate, lowPrecision, blockCount, blockFrames)
                }
                check(run != 0L) { getString(R.string.msg_capture_session_failed, sampleRate) }
                val newOutput = checkNotNull(output)
                val newCapturer = AudioCapturer(sampleRate, mode, durationMinutes, blockFrames, run, newOutput) { stats, error ->
                    runOnUiThread { onCaptureFinished(stats, error) }
                }
                newCapturer.start()
                // From here the capturer owns the run (it frees it when it finishes).
                run = 0L
                capturer = newCapturer
                captureOutput = newOutput
                output = null
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (t: Throwable) {
                Log.e(TAG, "Starting the capture failed", t)
                if (!isDestroyed) {
                    binding.textCaptureStatus.text = getString(R.string.txt_capture_failed)
                    showError(getString(R.string.title_capture_error), KrispSdk.withSdkError(t.message ?: t.javaClass.simpleName))
                }
            } finally {
                // Whatever wasn't handed over to a running capture is freed.
                if (run != 0L) KrispSdk.stopCaptureRun(run)
                output?.release()
                isPreparing = false
                if (!isDestroyed) refreshUi()
            }
            if (isDestroyed) {
                capturer?.cancel()
                return@launch
            }
            if (isCapturing) onCaptureStarted(mode, sampleRate, durationMinutes, blockMinutes, lowPrecision)
        }
    }

    private fun onCaptureStarted(mode: CaptureMode, sampleRate: Int, durationMinutes: Int, blockMinutes: Double,
                                 lowPrecision: Boolean) {
        stopReason = null
        lastRunLowPrecision = lowPrecision
        // A long run must not end because the screen went off: Android would then mute the microphone.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        binding.textCaptureStatus.text = getString(R.string.txt_capture_running, modeName(mode), sampleRate)
        Log.i(TAG, "Capture started: $mode, $sampleRate Hz, $durationMinutes min, block $blockMinutes min, " +
            "lowPrecision=$lowPrecision, cpuFp16=${KrispSdk.cpuHasFp16}")
        handler.post(progressTick)
        refreshUi()
        // The user left while the buffers were being allocated: onStop has already run, and the microphone is muted.
        if (!lifecycle.currentState.isAtLeast(Lifecycle.State.STARTED)) stopCapture(R.string.txt_capture_left_app)
    }

    private fun stopCapture(reason: Int) {
        val running = capturer ?: return
        if (stopReason == null) stopReason = reason
        running.stop()
        binding.textCaptureStatus.text = getString(R.string.txt_capture_stopping)
        refreshUi()
    }

    /** On the UI thread, once per capture: after the duration, a stop, or an error. The run is already freed. */
    private fun onCaptureFinished(stats: CaptureStats, error: Throwable?) {
        handler.removeCallbacks(progressTick)
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val output = captureOutput
        capturer = null
        captureOutput = null
        Log.i(TAG, "Capture finished: $stats" + (error?.let { ", error: $it" } ?: ""))
        if (isDestroyed) {
            output?.release()
            return
        }

        if (output != null && stats.processedFrames > 0) {
            processedAudio = output
            processedSampleRate = stats.sampleRate
        } else {
            output?.release()
        }
        lastStats = stats
        showStats(stats)
        binding.textCaptureStatus.text = getString(
            when {
                error != null -> R.string.txt_capture_failed
                else -> stopReason ?: R.string.txt_capture_done
            }
        )
        if (error != null) {
            Log.e(TAG, "Capture failed", error)
            showError(getString(R.string.title_capture_error), KrispSdk.withSdkError(error.message ?: error.javaClass.simpleName))
        }
        refreshUi()
    }

    /** Drops the last capture's audio and stats, e.g. when a different model is loaded. */
    private fun clearProcessed() {
        processedAudio?.release()
        processedAudio = null
        processedSaved = false
        lastStats = null
        lastRunLowPrecision = null
        showStats(null)
    }

    /** The status line while idle with no capture shown: what is still missing, or that it's ready. */
    private fun showIdleStatus() {
        if (isCapturing || isPreparing || isSaving || lastStats != null) return
        binding.textCaptureStatus.text = getString(
            if (KrispSdk.modelLoaded) R.string.txt_capture_ready else R.string.txt_capture_select_model
        )
    }

    private fun refreshUi() {
        val busy = isCapturing || isPreparing || isSaving
        binding.buttonModelFile.isEnabled = !busy
        binding.checkboxLowPrecision.isEnabled = !busy
        binding.radioRealtime.isEnabled = !busy
        binding.radioDistributed.isEnabled = !busy
        binding.editDuration.isEnabled = !busy
        binding.editBlockSize.isEnabled = !busy
        // Like iOS: the block size only shows (and its space stays reserved) in Distributed mode.
        binding.rowBlockSize.visibility = if (selectedMode == CaptureMode.DISTRIBUTED) View.VISIBLE else View.INVISIBLE

        binding.buttonCapture.text = getString(if (isCapturing) R.string.btn_stop_capture else R.string.btn_start_capture)
        binding.buttonCapture.isEnabled = when {
            isCapturing -> stopReason == null
            isPreparing || isSaving -> false
            else -> KrispSdk.modelLoaded
        }
        binding.buttonSave.isEnabled = !busy && processedAudio != null

        setStatusMark(binding.statusModel, KrispSdk.modelLoaded)
        if (isCapturing) {
            binding.statusCapture.text = getString(R.string.status_recording)
            binding.statusCapture.setTextColor(ContextCompat.getColor(this, R.color.status_fail))
        } else {
            setStatusMark(binding.statusCapture, KrispSdk.modelLoaded)
        }
        setStatusMark(binding.statusSave, processedSaved)
    }

    private fun modeName(mode: CaptureMode) = getString(
        if (mode == CaptureMode.DISTRIBUTED) R.string.txt_mode_distributed else R.string.txt_mode_realtime
    )

    /** m:ss.s (rounded to tenths first, so 59.96 s shows as 1:00.0, not 0:60.0). */
    private fun clock(seconds: Double): String {
        val tenths = Math.round(seconds * 10)
        return String.format(Locale.US, "%d:%04.1f", tenths / 600, (tenths % 600) / 10.0)
    }

    /** Shows a capture's stats; null (no capture) shows placeholders. */
    private fun showStats(stats: CaptureStats?) {
        val cpuFp16 = getString(if (KrispSdk.cpuHasFp16) R.string.txt_yes else R.string.txt_no)
        binding.textPrecision.text = when (lastRunLowPrecision) {
            null -> getString(R.string.txt_precision_none, cpuFp16)
            true -> getString(R.string.txt_precision, getString(R.string.txt_on), cpuFp16)
            false -> getString(R.string.txt_precision, getString(R.string.txt_off), cpuFp16)
        }
        if (stats == null) {
            binding.textMinutes.text = getString(R.string.txt_minutes_processed, 0L)
            binding.textCaptureAudio.text = getString(R.string.txt_capture_audio_none)
            binding.textCaptureCpu.text = getString(R.string.txt_capture_cpu_none)
            binding.textCaptureFrames.text = getString(R.string.txt_capture_frames_none)
            binding.textCaptureInterval.text = getString(R.string.txt_capture_interval_none)
            binding.textCaptureBlocks.visibility = View.GONE
            return
        }
        binding.textMinutes.text = getString(R.string.txt_minutes_processed, stats.processedMinutes)
        binding.textCaptureAudio.text = getString(
            R.string.txt_capture_audio, clock(stats.processedSeconds), clock(stats.capturedSeconds), stats.sampleRate,
            stats.frameMs
        )
        binding.textCaptureCpu.text = getString(R.string.txt_capture_cpu, stats.ncSeconds, stats.cpuLoadPercent)
        binding.textCaptureFrames.text = getString(R.string.txt_capture_frames, stats.avgFrameMs, stats.maxFrameMs)
        val interval = stats.lastInterval
        binding.textCaptureInterval.text = if (interval == null) {
            getString(R.string.txt_capture_interval_none)
        } else {
            getString(
                R.string.txt_capture_interval, interval.avgFrameMs, interval.maxFrameMs, interval.cpu,
                if (interval.cpuMhz > 0) getString(R.string.txt_mhz, interval.cpuMhz) else getString(R.string.txt_mhz_unknown)
            )
        }
        if (stats.mode == CaptureMode.DISTRIBUTED) {
            binding.textCaptureBlocks.visibility = View.VISIBLE
            binding.textCaptureBlocks.text = getString(R.string.txt_capture_blocks, stats.blocksProcessed)
        } else {
            binding.textCaptureBlocks.visibility = View.GONE
        }
    }
}
