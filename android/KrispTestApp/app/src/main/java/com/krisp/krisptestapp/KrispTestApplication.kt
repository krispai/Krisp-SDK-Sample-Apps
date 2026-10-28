package com.krisp.krisptestapp

import android.app.Application
import android.content.pm.ApplicationInfo

/** Initializes the Krisp Audio SDK once per process, before any page uses it (pages report [KrispSdk.initError]). */
class KrispTestApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        // Debug builds also log the SDK's Info messages (e.g. that a model runs in fp16); release only warnings/errors.
        val debuggable = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
        KrispSdk.init(verboseLogs = debuggable)
    }
}
