package com.krisp.krisptestapp

import android.Manifest
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.krisp.krisptestapp.databinding.ActivityMainBinding


class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    companion object {
        private const val PERMISSION_REQUEST_READ_EXTERNAL = 100
        // Used to load the 'krisptestapp' library on application startup.
        init {
            System.loadLibrary("krisptestapp")
        }
    }

    private fun handleWavUri(uri: Uri) {
        // TODO: actually read/process the WAV (this was your “step 4”)
        binding.sampleText.text = "Picked: $uri"
    }

    private val pickWav = registerForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
        uri?.let { handleWavUri(it) }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)


        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = stringFromJNI()

        // Step 3: hook up a button to request permission + launch picker
        binding.buttonWavFile.setOnClickListener {
            // pick the right permission for your target OS
            val readPerm = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                Manifest.permission.READ_MEDIA_AUDIO
            } else {
                Manifest.permission.READ_EXTERNAL_STORAGE
            }

            if (ContextCompat.checkSelfPermission(this, readPerm)
                != PackageManager.PERMISSION_GRANTED
            ) {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(readPerm),
                    PERMISSION_REQUEST_READ_EXTERNAL
                )
            } else {
                pickWav.launch("audio/wav")
            }
        }
    }

    // handle the user’s permission response
    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISSION_REQUEST_READ_EXTERNAL &&
            grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED
        ) {
            pickWav.launch("audio/wav")
        } else {
            Toast.makeText(this, "Storage permission is needed to select WAV files", Toast.LENGTH_SHORT).show()
        }
    }

    /**
      * A native method that is implemented by the 'krisptestapp' native library,
      * which is packaged with this application.
      */
     external fun stringFromJNI(): String

}