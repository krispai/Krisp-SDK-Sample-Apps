package com.krisp.krisptestapp

import android.net.Uri
import android.content.Intent
import androidx.activity.result.contract.ActivityResultContracts
import android.os.Bundle
import com.krisp.krisptestapp.databinding.ActivityMainBinding
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.withContext
import kotlinx.coroutines.launch
import android.util.Log
import java.io.IOException
import java.util.concurrent.atomic.AtomicBoolean

/** File Processing page: runs NC over a WAV file and reports benchmark stats, like the iOS app's FileProcessingView. */
class WavProcessActivity : KrispPageActivity() {

    private class ProcessedAudio(val buffer: NativeAudioBuffer, val stats: WavProcessingStats)

    private lateinit var binding: ActivityMainBinding
    private var selectedWavUri: Uri? = null
    private var selectedWavName: String? = null
    private var selectedWavSize: Long = -1 // bytes, -1 when the provider doesn't tell
    private var processJob: Job? = null
    // The processed WAV, in native memory (a long recording doesn't fit in the Java heap).
    private var processedAudio: NativeAudioBuffer? = null
    // The output of the run in progress, until it's handed over to processedAudio (freed if the run fails).
    @Volatile private var inFlightOutput: NativeAudioBuffer? = null
    private var processedSaved = false
    // Set by Stop (and onDestroy); checked by the processing loop before every frame, like the iOS app's stopFlag.
    private val stopRequested = AtomicBoolean(false)

    // Not processJob.isActive: the job is still active while its finally block refreshes the UI.
    private var isProcessing = false
    private var isSaving = false
    // The low-precision setting the shown result was processed with, null when there is no result.
    private var lastRunLowPrecision: Boolean? = null

    companion object {
        private const val TAG = "WavProcess"
    }

    /** Drops the processed audio and its stats, e.g. when a different WAV or model is loaded. */
    private fun clearProcessed() {
        processedAudio?.release()
        processedAudio = null
        processedSaved = false
        lastRunLowPrecision = null
        showStats(null)
    }

    override fun onModelPicked(loaded: Boolean) {
        // Results of the previous model are stale; after a failed load they stay valid (and savable).
        if (loaded) clearProcessed()
        binding.textModelFile.text = modelText()
        showIdleStatus()
        refreshUi()
    }

    private fun handleWavUri(uri: Uri) {
        val name = displayName(uri)
        val size = fileSize(uri)
        // Check the file now, like the iOS app does on import: a bad pick keeps the previous WAV and its ✘/✔ state.
        val header = try {
            if (size == 0L) throw IOException("File is empty")
            val header = contentResolver.openInputStream(uri)?.use { parseWavHeader(it) }
                ?: throw IOException("Unable to access the selected file.")
            if (header.formatName == null) {
                throw IOException(
                    "Unsupported WAV format (audioFormat ${header.audioFormat}, ${header.bitsPerSample} bits): " +
                        "mono PCM16 or FLOAT32 is needed"
                )
            }
            if (header.dataSize == 0L) throw IOException("The WAV file has no audio data")
            header
        } catch (t: Throwable) {
            Log.e(TAG, "WAV import failed for $uri", t)
            showError(getString(R.string.title_file_import_error), "$name: ${t.message ?: t.javaClass.simpleName}")
            return
        }

        if (uri != selectedWavUri) {
            clearProcessed()
        }
        selectedWavUri = uri
        selectedWavName = name
        selectedWavSize = size
        val shownSize = if (size > 0) size else header.headerBytes.size + header.dataSize
        binding.textWavFile.text = getString(
            R.string.txt_wav_loaded_info, name, megabytes(shownSize), header.sampleRate, header.formatName,
            header.durationSeconds / 60.0
        )
        showIdleStatus()
        refreshUi()
    }

    private val pickWav = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        uri?.let {
            // Optional: persist permission if you want to reuse after restart
            try {
                contentResolver.takePersistableUriPermission(it, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: Throwable) { /* not every provider grants persistable permissions */ }
            handleWavUri(it)
        }
    }

    private val saveProcessed = registerForActivityResult(ActivityResultContracts.CreateDocument("audio/wav")) { uri: Uri? ->
        val audio = processedAudio
        if (uri == null || audio == null) {
            if (audio != null) binding.textProcessStatus.text = getString(R.string.txt_save_canceled)
            return@registerForActivityResult
        }
        // Hundreds of MB can take a while to write: do it off the UI thread, with the buttons locked so the buffer
        // can't be freed (by loading another file) while it's being written.
        isSaving = true
        binding.textProcessStatus.text = getString(R.string.txt_saving)
        refreshUi()
        lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) {
                    contentResolver.openFileDescriptor(uri, "w")?.use { pfd -> audio.writeTo(pfd.fd) }
                        ?: throw IllegalStateException("Unable to open the destination file")
                }
                processedSaved = true
                binding.textProcessStatus.text = getString(R.string.txt_saved_to, displayName(uri))
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (t: Throwable) {
                processedSaved = false
                binding.textProcessStatus.text = getString(R.string.txt_save_failed, t.message ?: t.javaClass.simpleName)
                Log.e(TAG, "Saving processed audio failed for $uri", t)
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

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        applySystemBarInsets(binding.root)

        binding.textSdkVersion.text = getString(R.string.txt_sdk_version, KrispSdk.version)

        binding.buttonModelFile.setOnClickListener {
            // Force real Files (DocumentsUI) with persistable URI
            openModelPicker()
        }
        binding.buttonModelFile.setOnLongClickListener {
            // Manual fallback: let user pick a folder and choose a .kef inside it
            openModelFolderPicker()
            true
        }
        binding.buttonWavFile.setOnClickListener {
            pickWav.launch(arrayOf("audio/wav", "audio/x-wav", "audio/vnd.wave"))
        }
        binding.buttonProcess.setOnClickListener {
            if (isProcessing) {
                stopRequested.set(true)
                binding.buttonProcess.isEnabled = false // until the loop notices
            } else {
                startProcessing()
            }
        }
        binding.buttonSave.setOnClickListener {
            if (processedAudio == null) return@setOnClickListener
            saveProcessed.launch(suggestedOutputName())
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

    private fun suggestedOutputName(): String {
        val base = selectedWavName?.replace(Regex("\\.wav$", RegexOption.IGNORE_CASE), "")
        return if (base.isNullOrEmpty()) "processed_audio.wav" else "${base}_processed.wav"
    }

    private fun startProcessing() {
        val wavUri = selectedWavUri ?: return
        if (!KrispSdk.modelLoaded) return

        // Free the previous output first: its memory counts towards what's available for the next one.
        clearProcessed()
        val wavSize = selectedWavSize
        if (wavSize > 0) {
            memoryShortageFor(wavSize)?.let { shortage ->
                showError(getString(R.string.title_processing_error), shortage)
                return
            }
        }

        val lowPrecision = binding.checkboxLowPrecision.isChecked
        lastRunLowPrecision = lowPrecision // shown with the stats while this run goes on
        stopRequested.set(false)
        isProcessing = true
        KrispSdk.takeLastError() // drop older errors: only this run's should show up in its error dialog
        binding.textProcessStatus.text = getString(R.string.txt_process_in_progress)

        processJob = lifecycleScope.launch {
            try {
                val result = withContext(Dispatchers.Default) {
                    // Long-running WAV processing
                    processWavFile(wavUri, wavSize, lowPrecision)
                }
                processedAudio = result.buffer
                inFlightOutput = null
                lastRunLowPrecision = lowPrecision
                showStats(result.stats)
                binding.textProcessStatus.text = getString(R.string.txt_process_done)
                Log.i(TAG, "Processed ${result.stats.processedFrames} frames, lowPrecision=$lowPrecision, " +
                    "cpuFp16=${KrispSdk.cpuHasFp16}: ${result.stats}")
            } catch (stopped: WavProcessingStoppedException) {
                // Like the iOS app: a stopped run leaves no processed audio to save.
                lastRunLowPrecision = lowPrecision
                showStats(stopped.stats)
                binding.textProcessStatus.text = getString(R.string.txt_process_stopped)
            } catch (cancelled: CancellationException) {
                // The activity is gone: a run that completed meanwhile has nobody to hand its output to.
                inFlightOutput?.release()
                inFlightOutput = null
                throw cancelled
            } catch (t: Throwable) {
                lastRunLowPrecision = null
                showStats(null)
                binding.textProcessStatus.text = getString(R.string.txt_process_failed)
                Log.e(TAG, "Processing failed for $wavUri", t)
                showError(getString(R.string.title_processing_error), KrispSdk.withSdkError(t.message ?: t.javaClass.simpleName))
            } finally {
                isProcessing = false
                if (!isDestroyed) refreshUi()
            }
        }
        refreshUi()
    }

    /** The status line while idle with nothing processed: what is still missing, or that it's ready. */
    private fun showIdleStatus() {
        if (isProcessing || isSaving || processedAudio != null) return
        binding.textProcessStatus.text = getString(
            if (KrispSdk.modelLoaded && selectedWavUri != null) R.string.txt_process_ready else R.string.txt_process
        )
    }

    /** Enables/labels the buttons and sets the ✔/✘ marks from the current state, like the iOS view's bindings. */
    private fun refreshUi() {
        val busy = isProcessing || isSaving
        binding.buttonWavFile.isEnabled = !busy
        binding.buttonModelFile.isEnabled = !busy
        binding.checkboxLowPrecision.isEnabled = !busy // read when a run starts
        binding.buttonProcess.text = getString(if (isProcessing) R.string.btn_stop_name else R.string.btn_process_name)
        binding.buttonProcess.isEnabled = when {
            isProcessing -> !stopRequested.get()
            isSaving -> false
            else -> KrispSdk.modelLoaded && selectedWavUri != null
        }
        binding.buttonSave.isEnabled = !busy && processedAudio != null

        setStatusMark(binding.statusWav, selectedWavUri != null)
        setStatusMark(binding.statusModel, KrispSdk.modelLoaded)
        setStatusMark(binding.statusProcess, processedAudio != null)
        setStatusMark(binding.statusSave, processedSaved)
    }

    /** Shows frame progress, processing time and speed; null (nothing processed) shows placeholders. */
    private fun showStats(stats: WavProcessingStats?) {
        // Which setting the shown numbers were measured with: the checkbox may have changed since.
        val cpuFp16 = getString(if (KrispSdk.cpuHasFp16) R.string.txt_yes else R.string.txt_no)
        binding.textPrecision.text = when (lastRunLowPrecision) {
            null -> getString(R.string.txt_precision_none, cpuFp16)
            true -> getString(R.string.txt_precision, getString(R.string.txt_on), cpuFp16)
            false -> getString(R.string.txt_precision, getString(R.string.txt_off), cpuFp16)
        }
        if (stats == null) {
            binding.textFrames.text = getString(R.string.txt_frames, 0L, 0L)
            binding.textAudioDuration.text = getString(R.string.txt_audio_duration_none)
            binding.textProcessingTime.text = getString(R.string.txt_processing_time_none)
            binding.textSpeed.text = getString(R.string.txt_speed_none)
            return
        }
        binding.textFrames.text = getString(R.string.txt_frames, stats.processedFrames, stats.totalFrames)
        binding.textAudioDuration.text =
            getString(R.string.txt_audio_duration, stats.audioSeconds, stats.sampleRate, stats.frameMs)
        binding.textProcessingTime.text = getString(R.string.txt_processing_time, stats.ncSeconds, stats.elapsedSeconds)
        binding.textSpeed.text = getString(R.string.txt_speed, stats.realtimeFactor, stats.msPerFrame)
    }

    /**
     * Processes the WAV into native buffers allocated up front, before processing (and its clock) starts.
     * [inputSize] is the WAV file's size, -1 if unknown; [lowPrecision] is the NC session's
     * enableLowPrecisionExecution.
     */
    private fun processWavFile(uri: Uri, inputSize: Long, lowPrecision: Boolean): ProcessedAudio {
        val processor = WavStreamProcessor(
            openInput = { contentResolver.openInputStream(uri) ?: throw IllegalStateException("Unable to open $uri") },
            onPcmFloatStartSession = { sampleRate -> KrispSdk.startFileSessionPcmFloat(sampleRate, lowPrecision) },
            onPcm16StartSession = { sampleRate -> KrispSdk.startFileSessionPcm16(sampleRate, lowPrecision) },
            onPcm16Frame = KrispSdk::processFileFramePcm16,
            onFloat32Frame = KrispSdk::processFileFramePcmFloat,
            onHeader = { _, stats -> runOnUiThread { if (!isDestroyed) showStats(stats) } },
            onProgress = { stats -> runOnUiThread { if (!isDestroyed) showStats(stats) } },
            isStopped = { stopRequested.get() }
        )
        try {
            val stats = processor.process { capacity ->
                // The output is the header plus the processed audio, so never bigger than the input file. The header's
                // data size alone can't be trusted (some recorders leave it wrong); the file size can.
                val bytes = if (inputSize > 0) minOf(inputSize, capacity) else capacity
                NativeAudioBuffer.allocate(bytes).also { inFlightOutput = it }
            }
            return ProcessedAudio(checkNotNull(inFlightOutput), stats)
        } catch (t: Throwable) {
            inFlightOutput?.release()
            inFlightOutput = null
            throw t
        }
    }

    override fun onDestroy() {
        // The processing loop is blocking native work: the flag stops it, cancelling the job alone would not.
        stopRequested.set(true)
        processJob?.cancel()
        if (!isSaving) {
            // A save in progress frees the buffer itself when it's done writing.
            processedAudio?.release()
            processedAudio = null
        }
        super.onDestroy()
    }

}
