package com.krisp.krisptestapp

import android.content.Intent
import android.os.Bundle
import com.krisp.krisptestapp.databinding.ActivityWelcomeBinding

/** Start page: SDK version and the choice of testing mode, like the iOS app's WelcomeView. */
class WelcomeActivity : KrispPageActivity() {

    private lateinit var binding: ActivityWelcomeBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityWelcomeBinding.inflate(layoutInflater)
        setContentView(binding.root)
        applySystemBarInsets(binding.root)

        binding.textSdkVersion.text = getString(R.string.txt_sdk_version, KrispSdk.version)
        binding.buttonFileProcessing.setOnClickListener {
            startActivity(Intent(this, WavProcessActivity::class.java))
        }
        binding.buttonAudioCapture.setOnClickListener {
            startActivity(Intent(this, AudioCaptureActivity::class.java))
        }

        // Like the iOS app: the app is useless without the SDK, so say so up front (once, not after every rotation).
        if (savedInstanceState == null && !KrispSdk.initialized) {
            val reason = KrispSdk.initError
            val message = getString(R.string.msg_sdk_load_failed)
            showError(getString(R.string.title_sdk_error), if (reason.isEmpty()) message else "$message\n\nSDK: $reason")
        }
    }

    override fun onModelPicked(loaded: Boolean) {
        // No model picker on this page.
    }
}
