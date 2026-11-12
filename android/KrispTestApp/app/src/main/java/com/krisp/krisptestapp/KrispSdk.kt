package com.krisp.krisptestapp

import android.util.Log

/**
 * The Krisp Audio SDK, shared by every page of the app (JNI glue in krisp.cpp).
 *
 * There is one SDK instance and one loaded model per process: a model loaded on one page is the model of the other
 * pages too. File processing and audio capture use separate NC sessions.
 */
object KrispSdk {

    init {
        System.loadLibrary("krisptestapp")
    }

    private const val TAG = "KrispSdk"

    // Krisp::AudioSdk::LogLevel values, for krispInit.
    private const val SDK_LOG_INFO = 2
    private const val SDK_LOG_WARN = 3

    /** False after a failed [init]; [initError] then holds the SDK's reason (may be empty). */
    var initialized: Boolean = false
        private set

    /** Why [init] failed, as the SDK reported it ("" when it didn't say). */
    var initError: String = ""
        private set

    /** File name of the loaded model, null when none is loaded. */
    var modelName: String? = null
        private set

    val modelLoaded: Boolean
        get() = modelName != null

    val version: String by lazy { krispGetVersion() }

    /** Whether the CPU does FP16 arithmetic (ARMv8.2-A FP16, "asimdhp"), as the kernel reports it. */
    val cpuHasFp16: Boolean by lazy { krispCpuHasFp16() }

    /**
     * globalInit with a logcat log callback (tag KrispSDK). [verboseLogs] (debug builds) also shows the SDK's Info
     * messages, e.g. that a model runs in fp16; otherwise only warnings and errors.
     */
    fun init(verboseLogs: Boolean) {
        initialized = krispInit(if (verboseLogs) SDK_LOG_INFO else SDK_LOG_WARN)
        if (initialized) {
            Log.i(TAG, "Krisp Audio SDK $version initialized, SDK log level ${if (verboseLogs) "Info" else "Warn"} " +
                "(tag KrispSDK), CPU FP16 arithmetic: $cpuHasFp16")
        } else {
            initError = takeLastError()
            Log.e(TAG, "Krisp Audio SDK initialization failed: $initError")
        }
    }

    /**
     * Loads a model for every page; running sessions end first. If the SDK rejects it, the current model (if any)
     * stays loaded, and the reason is in [takeLastError].
     */
    fun loadModel(bytes: ByteArray, name: String): Boolean {
        takeLastError() // drop older errors: only this load's should be reported
        val loaded = krispLoadModel(bytes, bytes.size) == 0
        if (loaded) modelName = name
        return loaded
    }

    /** [message], plus the last error the SDK reported (also in logcat, tags KrispSDK / KrispTestApp), if any. */
    fun withSdkError(message: String): String {
        val sdkError = takeLastError()
        return if (sdkError.isEmpty() || message.contains(sdkError)) message else "$message\n\nSDK: $sdkError"
    }

    /** The last error the SDK logged or the JNI glue caught, "" if none; clears it. */
    fun takeLastError(): String = krispTakeLastError()

    // File processing session (30 ms frames, the PCM format of the WAV).
    fun startFileSessionPcmFloat(sampleRate: Int, lowPrecision: Boolean) = krispStartNcSessionPcmFloat(sampleRate, lowPrecision)
    fun startFileSessionPcm16(sampleRate: Int, lowPrecision: Boolean) = krispStartNcSessionPcm16(sampleRate, lowPrecision)

    /** NC on one frame (the first [valid] bytes of [buf]) in place; the NC call's time in ns, negative on failure. */
    fun processFileFramePcmFloat(buf: ByteArray, valid: Int): Long = krispNcFramePcmFloat(buf, valid)
    fun processFileFramePcm16(buf: ByteArray, valid: Int): Long = krispNcFramePcm16(buf, valid)

    // Audio capture: each run owns a native CaptureRun (NC session of 10 ms float frames, like the iOS app, plus its
    // buffers) through a handle, so a run that is still finishing can't touch the next run's session.

    /**
     * Creates a capture run; in Distributed mode with [blockCount] blocks of [blockFrames] frames, allocated now.
     * Returns its handle, 0 on failure (the reason is in [takeLastError]).
     */
    fun startCaptureRun(sampleRate: Int, lowPrecision: Boolean, blockCount: Int, blockFrames: Int): Long =
        krispCaptureStart(sampleRate, lowPrecision, blockCount, blockFrames)

    /** Realtime: NC on the frame at samples[offset], appended to [output]; the NC call's time in ns, negative on failure. */
    fun processCaptureFrame(run: Long, samples: FloatArray, offset: Int, output: NativeAudioBuffer): Long =
        krispCaptureProcessFrame(run, samples, offset, output.nativeHandle)

    /** Distributed: stores the frame at samples[offset] as frame [frameIndex] of [block]. */
    fun storeCaptureFrame(run: Long, block: Int, frameIndex: Int, samples: FloatArray, offset: Int): Boolean =
        krispCaptureStoreFrame(run, block, frameIndex, samples, offset)

    /**
     * Distributed: NC on [count] frames of [block] from [first], appended to [output]. [times] receives their NC time
     * in ns: [0] total, [1] the slowest frame.
     */
    fun processCaptureBlock(run: Long, block: Int, first: Int, count: Int, output: NativeAudioBuffer,
                            times: LongArray): Boolean =
        krispCaptureProcessBlock(run, block, first, count, output.nativeHandle, times)

    /** Frees a capture run (its NC session and buffers). */
    fun stopCaptureRun(run: Long) = krispCaptureStop(run)

    /** The CPU core the calling thread runs on right now. */
    fun currentCpu(): Int = krispCurrentCpu()

    /** A core's current clock in MHz, -1 when the system doesn't let the app read it. */
    fun cpuFreqMhz(cpu: Int): Int = krispCpuFreqKhz(cpu).let { if (it > 0) it / 1000 else -1 }

    @JvmStatic private external fun krispInit(logLevel: Int): Boolean
    @JvmStatic private external fun krispGetVersion(): String
    @JvmStatic private external fun krispCpuHasFp16(): Boolean
    @JvmStatic private external fun krispTakeLastError(): String
    @JvmStatic private external fun krispLoadModel(modelBytes: ByteArray, size: Int): Int
    @JvmStatic private external fun krispStartNcSessionPcmFloat(samplingRate: Int, lowPrecision: Boolean): Boolean
    @JvmStatic private external fun krispStartNcSessionPcm16(samplingRate: Int, lowPrecision: Boolean): Boolean
    @JvmStatic private external fun krispNcFramePcmFloat(buf: ByteArray, valid: Int): Long
    @JvmStatic private external fun krispNcFramePcm16(buf: ByteArray, valid: Int): Long
    @JvmStatic private external fun krispCaptureStart(samplingRate: Int, lowPrecision: Boolean, blockCount: Int,
                                                      blockFrames: Int): Long
    @JvmStatic private external fun krispCaptureProcessFrame(run: Long, samples: FloatArray, offset: Int,
                                                             output: Long): Long
    @JvmStatic private external fun krispCaptureStoreFrame(run: Long, block: Int, frameIndex: Int, samples: FloatArray,
                                                           offset: Int): Boolean
    @JvmStatic private external fun krispCaptureProcessBlock(run: Long, block: Int, first: Int, count: Int,
                                                             output: Long, times: LongArray): Boolean
    @JvmStatic private external fun krispCaptureStop(run: Long)
    @JvmStatic private external fun krispCurrentCpu(): Int
    @JvmStatic private external fun krispCpuFreqKhz(cpu: Int): Int
}
