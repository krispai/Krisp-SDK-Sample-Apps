package com.krisp.krisptestapp

import android.net.Uri
import android.content.Intent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.krisp.krisptestapp.databinding.ActivityMainBinding
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.withContext
import kotlinx.coroutines.launch


class WavProcessActivity : AppCompatActivity() {

    external fun krispLoadModel(modelBytes: ByteArray, size: Int): Int
    external fun krispStartNcSessionPcmFloat(samplingRate: Int) : Boolean
    external fun krispNcFramePcmFloat(buf: ByteArray, valid: Int) : Boolean
    external fun krispStartNcSessionPcm16(samplingRate: Int) : Boolean
    external fun krispNcFramePcm16(buf: ByteArray, valid: Int) : Boolean

    private lateinit var binding: ActivityMainBinding
    private var selectedWavUri: Uri? = null
    private var selectedModelUri: Uri? = null
    private var processJob: Job? = null
    private var lastProcessedBytes: ByteArray? = null

    companion object {
        //private const val PERMISSION_REQUEST_READ_EXTERNAL = 100
        // Used to load the 'krisptestapp' library on application startup.
        init {
            System.loadLibrary("krisptestapp")
        }
    }

    private fun handleModelUri(uri: Uri) {
        contentResolver.openInputStream(uri)?.use { input ->
            val bytes = input.readBytes()
            var msg = ""
            if (krispLoadModel(bytes, bytes.size) == 0) {
                msg = getString(R.string.txt_model_loaded_prefix) + uri.toString() + " " + getString(R.string.txt_model_loaded_postfix)
                selectedModelUri = uri
            }
            else {
                msg = getString(R.string.txt_model_failed_prefix) + uri.toString()
                selectedModelUri = null
            }
            binding.textModelFile.text = msg
        }
    }

    private fun handleWavUri(uri: Uri) {
        selectedWavUri = uri
        val wavMsg = getString(R.string.txt_wav_loaded_prefix) + uri.toString() + " " + getString(R.string.txt_wav_loaded_postfix)
        binding.textWavFile.text = wavMsg
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
            contentResolver.takePersistableUriPermission(it, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            handleWavUri(it)
        }
    }

    private val saveProcessed = registerForActivityResult(ActivityResultContracts.CreateDocument("audio/wav")) { uri: Uri? ->
        val data = lastProcessedBytes
        if (uri != null && data != null) {
            try {
                contentResolver.openOutputStream(uri)?.use { out ->
                    out.write(data)
                }
                binding.textWavFile.text = getString(R.string.txt_wav_loaded_prefix) +
                        " Saved to: " + uri.toString()
            } catch (t: Throwable) {
                binding.textWavFile.text = "Save failed: ${t.message}"
            }
        } else if (data != null) {
            binding.textWavFile.text = "Save canceled"
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.buttonModelFile.setOnClickListener {
            // `.kef` usually has no official MIME; allow generic fallback
            pickModel.launch(arrayOf("application/octet-stream", "*/*"))
        }
        binding.buttonWavFile.setOnClickListener {
            pickWav.launch(arrayOf("audio/wav", "audio/x-wav", "audio/vnd.wave"))
        }
        binding.buttonProcess.setOnClickListener {
            if (processJob?.isActive == true) {
                // already running; ignore or you could show a toast
                return@setOnClickListener
            }
            val wavUri = selectedWavUri
            if (wavUri == null) {
                binding.textWavFile.text = getString(R.string.txt_wav_loaded_prefix)
                return@setOnClickListener
            }

            // Disable buttons while processing
            setUiBusy(true)

            processJob = lifecycleScope.launch {
                try {
                    val resultBytes = withContext(Dispatchers.IO) {
                        // Long-running WAV processing
                        processWavFile(wavUri)
                    }
                    lastProcessedBytes = resultBytes

                    // Suggest a filename based on input Uri
                    val suggestedName = (wavUri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
                        ?.replace(".wav", "", ignoreCase = true)
                        ?.plus("_processed.wav")) ?: "processed.wav"

                    binding.textWavFile.text = "Processed in memory (" + resultBytes.size + " bytes). Choose where to save…"
                    saveProcessed.launch(suggestedName)
                } catch (t: Throwable) {
                    binding.textWavFile.text = "Processing failed: ${t.message}"
                } finally {
                    setUiBusy(false)
                }
            }
        }
    }
    private fun setUiBusy(busy: Boolean) {
        binding.buttonWavFile.isEnabled = !busy
        binding.buttonModelFile.isEnabled = !busy
        binding.buttonProcess.isEnabled = !busy
        if (busy) {
            binding.textProcessStatus?.text = getString(R.string.txt_process_in_progress)
        } else {
            binding.textProcessStatus?.text = ""
        }
    }

    private fun processWavFile(uri: Uri): ByteArray {
        val processor = WavStreamProcessor(
            openInput = { contentResolver.openInputStream(uri)!! },
            onPcmFloatStartSession = ::krispStartNcSessionPcmFloat,
            onPcm16StartSession = ::krispStartNcSessionPcm16,
            onPcm16Frame = ::krispNcFramePcm16,
            onFloat32Frame = ::krispNcFramePcmFloat
        )
        val processedWavBytes = processor.processToMemory()
        return processedWavBytes
    }

    override fun onDestroy() {
        processJob?.cancel()
        super.onDestroy()
    }

    }