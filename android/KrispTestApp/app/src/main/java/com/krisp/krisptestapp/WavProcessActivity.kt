package com.krisp.krisptestapp

import android.app.ActivityManager
import android.content.pm.ApplicationInfo
import android.net.Uri
import android.content.Intent
import android.content.res.Configuration
import android.os.Build
import android.provider.OpenableColumns
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import androidx.core.content.ContextCompat
import com.krisp.krisptestapp.databinding.ActivityMainBinding
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.withContext
import kotlinx.coroutines.launch
import androidx.documentfile.provider.DocumentFile
import androidx.appcompat.app.AlertDialog
import android.util.Log
import java.io.IOException
import java.util.concurrent.atomic.AtomicBoolean

class WavProcessActivity : AppCompatActivity() {

    external fun krispInit(logLevel: Int): Boolean
    external fun krispLoadModel(modelBytes: ByteArray, size: Int): Int
    external fun krispStartNcSessionPcmFloat(samplingRate: Int, lowPrecision: Boolean) : Boolean
    external fun krispNcFramePcmFloat(buf: ByteArray, valid: Int) : Boolean
    external fun krispStartNcSessionPcm16(samplingRate: Int, lowPrecision: Boolean) : Boolean
    external fun krispNcFramePcm16(buf: ByteArray, valid: Int) : Boolean
    external fun krispGetVersion(): String
    external fun krispTakeLastError(): String
    external fun krispCpuHasFp16(): Boolean

    private class ProcessedAudio(val buffer: NativeAudioBuffer, val stats: WavProcessingStats)

    private lateinit var binding: ActivityMainBinding
    private var selectedWavUri: Uri? = null
    private var selectedWavName: String? = null
    private var selectedWavSize: Long = -1 // bytes, -1 when the provider doesn't tell
    private var selectedModelUri: Uri? = null
    private var modelLoaded = false
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
    private val cpuHasFp16 by lazy { krispCpuHasFp16() }

    companion object {
        //private const val PERMISSION_REQUEST_READ_EXTERNAL = 100
        // Used to load the 'krisptestapp' library on application startup.
        init {
            System.loadLibrary("krisptestapp")
        }
        private const val TAG = "WavProcess"

        // Krisp::AudioSdk::LogLevel values, for krispInit.
        private const val SDK_LOG_INFO = 2
        private const val SDK_LOG_WARN = 3

        private fun megabytes(bytes: Long) = bytes / (1024.0 * 1024.0)
    }

    /** The file name the user sees (e.g. "speech.wav"), not the content:// URI. */
    private fun displayName(uri: Uri): String {
        val name = try {
            contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0) else null
            }
        } catch (_: Throwable) {
            null
        }
        return name ?: uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':') ?: uri.toString()
    }

    /** The file's size in bytes, or -1 if the provider doesn't tell. */
    private fun fileSize(uri: Uri): Long {
        val size = try {
            contentResolver.query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { c ->
                if (c.moveToFirst() && !c.isNull(0)) c.getLong(0) else null
            }
        } catch (_: Throwable) {
            null
        }
        if (size != null && size > 0) return size
        return try {
            contentResolver.openFileDescriptor(uri, "r")?.use { it.statSize } ?: -1
        } catch (_: Throwable) {
            -1
        }
    }

    /** Null if [bytes] fit in the memory the system can give the app, else why they don't. */
    private fun memoryShortageFor(bytes: Long): String? {
        val activityManager = getSystemService(ACTIVITY_SERVICE) as ActivityManager
        val memory = ActivityManager.MemoryInfo().also { activityManager.getMemoryInfo(it) }
        // Below the threshold the system starts killing apps, this one included.
        val usable = (memory.availMem - memory.threshold).coerceAtLeast(0)
        return if (bytes > usable) {
            getString(R.string.msg_not_enough_memory, megabytes(bytes), megabytes(usable))
        } else {
            null
        }
    }

    /** [message], plus the last error the SDK reported (also in logcat, tags KrispSDK / KrispTestApp), if any. */
    private fun withSdkError(message: String): String {
        val sdkError = krispTakeLastError()
        return if (sdkError.isEmpty() || message.contains(sdkError)) message else "$message\n\nSDK: $sdkError"
    }

    private fun showError(title: String, message: String) {
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    /** Drops the processed audio and its stats, e.g. when a different WAV or model is loaded. */
    private fun clearProcessed() {
        processedAudio?.release()
        processedAudio = null
        processedSaved = false
        lastRunLowPrecision = null
        showStats(null)
    }

    private fun handleModelUri(uri: Uri) {
        val name = displayName(uri)
        try {
            contentResolver.openInputStream(uri)?.use { input ->
                val bytes = input.readBytes()
                if (bytes.isEmpty()) {
                    showError(getString(R.string.title_file_import_error), "File is empty")
                    return
                }
                krispTakeLastError() // drop older errors: only this load's should show up below
                val rc = krispLoadModel(bytes, bytes.size)
                // A failed load leaves no usable model in the native layer, so the previous one is gone too.
                modelLoaded = rc == 0
                if (modelLoaded) {
                    // Results of the previous model are stale; after a failed load they stay valid (and savable).
                    clearProcessed()
                    selectedModelUri = uri
                    binding.textModelFile.text = getString(R.string.txt_model_loaded, name)
                } else {
                    selectedModelUri = null
                    binding.textModelFile.text = getString(R.string.txt_model_failed, name)
                    showError(
                        getString(R.string.title_file_import_error),
                        withSdkError(getString(R.string.msg_model_load_failed, name))
                    )
                }
            } ?: run {
                binding.textModelFile.text = "Failed to open model: null stream"
                Log.e(TAG, "openInputStream returned null for $uri")
                showError(getString(R.string.title_file_import_error), "Unable to access the selected file.")
            }
        } catch (t: Throwable) {
            // Reading the file failed before it reached the SDK: the loaded model (if any) is still in use.
            val msg = "Model open failed: ${t.javaClass.simpleName}: ${t.message}"
            Log.e(TAG, "handleModelUri failed for $uri", t)
            showError(getString(R.string.title_file_import_error), msg)
        } finally {
            showIdleStatus()
            refreshUi()
        }
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

    private val pickModelGet = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let { handleModelUri(it) } // No persistable permission with GetContent
    }

    private val pickModelDoc = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        if (uri == null) {
            return@registerForActivityResult
        }
        try {
            // Persist only if supported (OpenDocument supports; GetContent does not)
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (_: Throwable) { /* ignore */ }
        handleModelUri(uri)
    }

    private val pickModelActivity = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { res ->
        val uri = res.data?.data ?: return@registerForActivityResult
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (_: Throwable) { /* may be unnecessary on some providers */ }
        handleModelUri(uri)
    }

    private val pickFolder = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { treeUri: Uri? ->
        treeUri ?: return@registerForActivityResult
        try {
            contentResolver.takePersistableUriPermission(
                treeUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (_: Throwable) {}
        val dir = DocumentFile.fromTreeUri(this, treeUri) ?: return@registerForActivityResult
        showKefChooser(dir)
    }

    private fun showKefChooser(dir: DocumentFile) {
        val files = dir.listFiles().filter { it.isFile && (it.name?.endsWith(".kef", true) == true) }
        if (files.isEmpty()) {
            AlertDialog.Builder(this)
                .setTitle("No .kef files found")
                .setMessage("Pick a different folder or copy your model into this folder.")
                .setPositiveButton(android.R.string.ok, null)
                .show()
            return
        }
        val names = files.map { it.name ?: "(unnamed)" }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Select model")
            .setItems(names) { _, which ->
                val file = files[which]
                handleModelUri(file.uri)
            }
            .show()
    }

    private fun openFolderPicker() {
        // Launch tree picker; user can pick "Downloads" manually if not pre-seeded
        pickFolder.launch(null)
    }

    private fun openModelPicker() {
        // Clean, contract-based OpenDocument flow (persistable)
        pickModelDoc.launch(arrayOf("application/*", "application/octet-stream", "*/*"))
    }

    private val pickModel = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        uri?.let {
            contentResolver.takePersistableUriPermission(it, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            handleModelUri(it)
        }
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
        applySystemBarInsets()

        // globalInit with a logcat log callback. Debug builds also show the SDK's Info messages (e.g. that a model
        // runs in fp16); release builds only warnings and errors. Like the iOS app, a failed init is reported.
        val debuggable = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
        if (krispInit(if (debuggable) SDK_LOG_INFO else SDK_LOG_WARN)) {
            Log.i(TAG, "Krisp Audio SDK ${krispGetVersion()} initialized, SDK log level " +
                "${if (debuggable) "Info" else "Warn"} (tag KrispSDK), CPU FP16 arithmetic: $cpuHasFp16")
        } else {
            showError(getString(R.string.title_sdk_error), withSdkError(getString(R.string.msg_sdk_load_failed)))
        }
        binding.textSdkVersion.text = getString(R.string.txt_sdk_version, krispGetVersion())

        binding.buttonModelFile.setOnClickListener {
            // Force real Files (DocumentsUI) with persistable URI; fallback to GetContent if needed
            openModelPicker()
        }
        binding.buttonModelFile.setOnLongClickListener {
            // Manual fallback: let user pick a folder and choose a .kef inside it
            openFolderPicker()
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
        refreshUi()
    }

    /**
     * Android 15+ lays apps targeting SDK 35 out edge-to-edge: the page starts under the status bar and ends under the
     * navigation bar. Pad it by their sizes so the header and the bottom row stay visible, and pick system bar icon
     * colors that show on the page background (the bars are transparent there).
     */
    private fun applySystemBarInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
        if (Build.VERSION.SDK_INT >= 35) {
            val night = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) ==
                Configuration.UI_MODE_NIGHT_YES
            WindowCompat.getInsetsController(window, window.decorView).apply {
                isAppearanceLightStatusBars = !night
                isAppearanceLightNavigationBars = !night
            }
        }
    }

    private fun suggestedOutputName(): String {
        val base = selectedWavName?.replace(Regex("\\.wav$", RegexOption.IGNORE_CASE), "")
        return if (base.isNullOrEmpty()) "processed_audio.wav" else "${base}_processed.wav"
    }

    private fun startProcessing() {
        val wavUri = selectedWavUri ?: return
        if (!modelLoaded) return

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
        krispTakeLastError() // drop older errors: only this run's should show up in its error dialog
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
                    "cpuFp16=$cpuHasFp16: ${result.stats}")
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
                showError(getString(R.string.title_processing_error), withSdkError(t.message ?: t.javaClass.simpleName))
            } finally {
                isProcessing = false
                if (!isDestroyed) refreshUi()
            }
        }
        refreshUi()
    }

    /** The status line while idle with nothing processed: what is still missing, or that it's ready. */
    private fun showIdleStatus() {
        if (isProcessing || processedAudio != null) return
        binding.textProcessStatus.text = getString(
            if (modelLoaded && selectedWavUri != null) R.string.txt_process_ready else R.string.txt_process
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
            else -> modelLoaded && selectedWavUri != null
        }
        binding.buttonSave.isEnabled = !busy && processedAudio != null

        setStatusMark(binding.statusWav, selectedWavUri != null)
        setStatusMark(binding.statusModel, modelLoaded)
        setStatusMark(binding.statusProcess, processedAudio != null)
        setStatusMark(binding.statusSave, processedSaved)
    }

    private fun setStatusMark(view: TextView, ok: Boolean) {
        view.text = getString(if (ok) R.string.status_ok else R.string.status_fail)
        view.setTextColor(ContextCompat.getColor(this, if (ok) R.color.status_ok else R.color.status_fail))
    }

    /** Shows frame progress, processing time and speed; null (nothing processed) shows placeholders. */
    private fun showStats(stats: WavProcessingStats?) {
        // Which setting the shown numbers were measured with: the checkbox may have changed since.
        val cpuFp16 = getString(if (cpuHasFp16) R.string.txt_yes else R.string.txt_no)
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
            onPcmFloatStartSession = { sampleRate -> krispStartNcSessionPcmFloat(sampleRate, lowPrecision) },
            onPcm16StartSession = { sampleRate -> krispStartNcSessionPcm16(sampleRate, lowPrecision) },
            onPcm16Frame = ::krispNcFramePcm16,
            onFloat32Frame = ::krispNcFramePcmFloat,
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
