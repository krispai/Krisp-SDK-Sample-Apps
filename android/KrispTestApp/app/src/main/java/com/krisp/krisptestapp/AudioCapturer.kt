package com.krisp.krisptestapp

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Build
import android.os.Process
import android.util.Log
import androidx.annotation.RequiresApi
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.Locale

/** Realtime: NC on every 10 ms frame as it is captured. Distributed: NC on whole blocks of captured audio at once. */
enum class CaptureMode { REALTIME, DISTRIBUTED }

/**
 * NC timing of the last [AudioCapturer.INTERVAL_FRAMES] frames (10 s of audio), and where they ran: the CPU core of
 * the processing thread at the end of the interval and its clock. Rising per-frame times on a slower core or clock are
 * the scheduler's doing, rising times on the same core and clock are the work's.
 */
data class CaptureInterval(
    val endSeconds: Double,
    val avgFrameMs: Double,
    val maxFrameMs: Double,
    val cpu: Int,
    val cpuMhz: Int,
)

/** A snapshot of a capture run. Times are of the Krisp NC calls only (the SDK's process(), timed natively). */
data class CaptureStats(
    val mode: CaptureMode,
    val sampleRate: Int,
    val frameMs: Int,
    val capturedFrames: Long,
    val processedFrames: Long,
    val ncNanos: Long,
    val maxFrameNanos: Long,
    val blocksProcessed: Int,
    val elapsedNanos: Long,
    val lastInterval: CaptureInterval?,
) {
    val capturedSeconds: Double get() = capturedFrames * frameMs / 1000.0
    val processedSeconds: Double get() = processedFrames * frameMs / 1000.0
    /** Whole minutes of audio processed, like the iOS app's "Processing Duration". */
    val processedMinutes: Long get() = processedFrames * frameMs / 60_000
    val ncSeconds: Double get() = ncNanos / 1e9
    /** NC time as a share of the audio's duration: how much of one core realtime NC needs. */
    val cpuLoadPercent: Double get() = if (processedSeconds > 0) ncSeconds / processedSeconds * 100 else 0.0
    val avgFrameMs: Double get() = if (processedFrames > 0) ncNanos / 1e6 / processedFrames else 0.0
    val maxFrameMs: Double get() = maxFrameNanos / 1e6
}

/**
 * Captures the microphone (mono float, 10 ms frames, like the iOS app's AudioCapturer) and runs Krisp NC on it with
 * the capture run [run] (from [KrispSdk.startCaptureRun], with [BLOCK_COUNT] blocks of [blockFrames] in Distributed
 * mode). The processed audio is appended to [output].
 *
 * Benchmark hygiene: every buffer (the output, the run's frame and blocks) is allocated before capture starts;
 * capture runs at urgent audio priority; NC is timed natively around the SDK's process(), frame by frame, so
 * [CaptureStats.maxFrameNanos] is real in both modes; stats are counters read with [snapshot] (the audio threads never
 * post UI work).
 *
 * Capture stops after [durationMinutes], on [stop] (the captured audio is still processed) or [cancel] (it isn't), or
 * on an error. Then the run is freed and [onFinished] is called once, from a capture thread.
 */
class AudioCapturer(
    private val sampleRate: Int,
    private val mode: CaptureMode,
    durationMinutes: Int,
    private val blockFrames: Int,
    private val run: Long,
    private val output: NativeAudioBuffer,
    private val onFinished: (stats: CaptureStats, error: Throwable?) -> Unit,
) {
    companion object {
        private const val TAG = "AudioCapture"
        const val FRAME_MS = 10

        /** Distributed mode: one block being filled, one being processed. */
        const val BLOCK_COUNT = 2

        /** Frames per diagnostics interval: 10 s of audio. */
        const val INTERVAL_FRAMES = 1000

        /** Distributed mode processes a block this many frames per native call, so progress shows during a block. */
        private const val BLOCK_CHUNK_FRAMES = 100

        /** Sample rates to try, best first; all are supported by the SDK. */
        private val SAMPLE_RATES = intArrayOf(48000, 44100, 32000, 16000)

        /** The best sample rate the device's microphone supports for this capture, 0 if none. */
        fun chooseSampleRate(): Int = SAMPLE_RATES.firstOrNull { rate ->
            AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, encoding()) > 0
        } ?: 0

        /** Bytes of processed audio [minutes] of capture produce. */
        fun outputBytes(sampleRate: Int, minutes: Int): Long = minutes.toLong() * 60 * sampleRate * 4

        /** Frames in a Distributed block of [blockMinutes]. */
        fun blockFrames(blockMinutes: Double): Int =
            Math.round(blockMinutes * 60 * 1000 / FRAME_MS).toInt().coerceAtLeast(1)

        /** Bytes the Distributed blocks take (native memory). */
        fun blockBytes(sampleRate: Int, blockMinutes: Double): Long =
            BLOCK_COUNT.toLong() * blockFrames(blockMinutes) * (sampleRate * FRAME_MS / 1000) * 4

        // Float capture needs API 23; older devices capture PCM16, converted to float.
        private fun encoding() =
            if (Build.VERSION.SDK_INT >= 23) AudioFormat.ENCODING_PCM_FLOAT else AudioFormat.ENCODING_PCM_16BIT
    }

    private val samplesPerFrame = sampleRate * FRAME_MS / 1000
    private val durationFrames = durationMinutes.toLong() * 60 * 1000 / FRAME_MS

    // Distributed mode: block indices passed between the capture thread and the worker. END ends the worker.
    private val freeBlocks = ArrayBlockingQueue<Int>(BLOCK_COUNT).apply { repeat(BLOCK_COUNT) { add(it) } }
    private val fullBlocks = ArrayBlockingQueue<Pair<Int, Int>>(BLOCK_COUNT + 1) // block, frames in it
    private val endOfCapture = Pair(-1, 0)

    private val stopRequested = AtomicBoolean(false)
    private val cancelled = AtomicBoolean(false)
    private val finished = AtomicBoolean(false)
    private var startNanos = 0L
    private val capturedFrames = AtomicLong(0)
    private val processedFrames = AtomicLong(0)
    private val ncNanos = AtomicLong(0)
    private val maxFrameNanos = AtomicLong(0)
    private val blocksProcessed = AtomicInteger(0)
    @Volatile private var lastInterval: CaptureInterval? = null
    @Volatile private var captureError: Throwable? = null
    @Volatile private var ncError: Throwable? = null

    // The processing thread's current diagnostics interval (one processing thread per run).
    private var intervalFrames = 0
    private var intervalNanos = 0L
    private var intervalMaxNanos = 0L

    fun snapshot() = CaptureStats(
        mode, sampleRate, FRAME_MS, capturedFrames.get(), processedFrames.get(), ncNanos.get(), maxFrameNanos.get(),
        blocksProcessed.get(), if (startNanos == 0L) 0 else System.nanoTime() - startNanos, lastInterval
    )

    /** Starts capturing. Needs the RECORD_AUDIO permission; throws if the microphone can't be opened. */
    @SuppressLint("MissingPermission") // the activity requests it before starting
    fun start() {
        val encoding = encoding()
        val bytesPerSample = if (encoding == AudioFormat.ENCODING_PCM_FLOAT) 4 else 2
        val minBuffer = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, encoding)
        check(minBuffer > 0) { "The microphone doesn't support $sampleRate Hz mono capture" }
        // Half a second of headroom, so a slow frame (or a busy moment) doesn't lose audio.
        val bufferBytes = maxOf(minBuffer, samplesPerFrame * bytesPerSample * 50)
        // VOICE_RECOGNITION: the least device-side processing (no AGC/NS on most phones), so NC gets the raw signal.
        val record = AudioRecord(
            MediaRecorder.AudioSource.VOICE_RECOGNITION, sampleRate, AudioFormat.CHANNEL_IN_MONO, encoding, bufferBytes
        )
        if (record.state != AudioRecord.STATE_INITIALIZED) {
            record.release()
            throw IllegalStateException("The microphone couldn't be opened ($sampleRate Hz)")
        }

        startNanos = System.nanoTime()
        record.startRecording()
        if (mode == CaptureMode.DISTRIBUTED) {
            Thread({ runWorker() }, "KrispCaptureNc").start()
        }
        Thread({ runCapture(record, encoding) }, "KrispCapture").start()
    }

    /** Stops capturing; the audio captured so far is still processed (Distributed), then [onFinished] follows. */
    fun stop() {
        stopRequested.set(true)
    }

    /** Stops capturing and drops what isn't processed yet (the page is gone: nobody would see or save it). */
    fun cancel() {
        cancelled.set(true)
        stopRequested.set(true)
    }

    private fun runCapture(record: AudioRecord, encoding: Int) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
        val frame = FloatArray(samplesPerFrame)
        val pcm16 = if (encoding == AudioFormat.ENCODING_PCM_16BIT) ShortArray(samplesPerFrame) else null
        var block = -1
        var framesInBlock = 0
        try {
            while (!stopRequested.get() && capturedFrames.get() < durationFrames && ncError == null) {
                if (!readFrame(record, frame, pcm16)) break
                capturedFrames.incrementAndGet()
                when (mode) {
                    CaptureMode.REALTIME -> {
                        val nanos = KrispSdk.processCaptureFrame(run, frame, 0, output)
                        if (nanos < 0) {
                            ncError = IllegalStateException("Krisp NC failed on frame ${processedFrames.get() + 1}")
                            break
                        }
                        countProcessed(1, nanos, nanos)
                    }
                    CaptureMode.DISTRIBUTED -> {
                        if (block < 0) block = freeBlocks.take()
                        check(KrispSdk.storeCaptureFrame(run, block, framesInBlock, frame, 0)) {
                            "Storing a captured frame in block $block failed"
                        }
                        framesInBlock++
                        if (framesInBlock == blockFrames) {
                            fullBlocks.put(Pair(block, framesInBlock))
                            block = -1
                            framesInBlock = 0
                        }
                    }
                }
            }
        } catch (t: Throwable) {
            captureError = t
        } finally {
            try {
                record.stop()
            } catch (_: Throwable) {
            }
            record.release()
        }
        if (mode == CaptureMode.DISTRIBUTED) {
            // The last, partial block is processed too (unless cancelled); then the worker finishes.
            if (block >= 0 && framesInBlock > 0) fullBlocks.put(Pair(block, framesInBlock))
            fullBlocks.put(endOfCapture)
        } else {
            finish()
        }
    }

    private fun runWorker() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO)
        val times = LongArray(2)
        while (true) {
            val (block, frames) = fullBlocks.take()
            if (block == endOfCapture.first) break
            try {
                // Blocks captured before a capture-side error still count; only an NC failure (or cancel) skips them.
                var first = 0
                while (first < frames && ncError == null && !cancelled.get()) {
                    val count = minOf(BLOCK_CHUNK_FRAMES, frames - first)
                    if (!KrispSdk.processCaptureBlock(run, block, first, count, output, times)) {
                        ncError = IllegalStateException("Krisp NC failed in block ${blocksProcessed.get() + 1}")
                        stopRequested.set(true)
                        break
                    }
                    countProcessed(count, times[0], times[1])
                    first += count
                }
                if (first == frames) blocksProcessed.incrementAndGet()
            } catch (t: Throwable) {
                if (ncError == null) ncError = t
                stopRequested.set(true)
            } finally {
                freeBlocks.put(block)
            }
        }
        finish()
    }

    /** Reads one 10 ms frame (blocking). False when stopped before the frame was complete. */
    private fun readFrame(record: AudioRecord, frame: FloatArray, pcm16: ShortArray?): Boolean {
        var read = 0
        while (read < samplesPerFrame) {
            val n = if (pcm16 == null) {
                readFloats(record, frame, read, samplesPerFrame - read)
            } else {
                record.read(pcm16, read, samplesPerFrame - read)
            }
            if (n < 0) throw IllegalStateException("Reading the microphone failed (AudioRecord error $n)")
            if (n == 0 && stopRequested.get()) return false // a blocking read that returns nothing: don't spin
            read += n
        }
        if (pcm16 != null) {
            for (i in 0 until samplesPerFrame) frame[i] = pcm16[i] / 32768f
        }
        return true
    }

    @RequiresApi(23)
    private fun readFloats(record: AudioRecord, frame: FloatArray, offset: Int, count: Int): Int =
        record.read(frame, offset, count, AudioRecord.READ_BLOCKING)

    /** Adds [frames] processed frames that took [nanos] of NC, the slowest [maxNanos]. One processing thread per run. */
    private fun countProcessed(frames: Int, nanos: Long, maxNanos: Long) {
        ncNanos.addAndGet(nanos)
        if (maxNanos > maxFrameNanos.get()) maxFrameNanos.set(maxNanos)
        val total = processedFrames.addAndGet(frames.toLong())

        intervalFrames += frames
        intervalNanos += nanos
        if (maxNanos > intervalMaxNanos) intervalMaxNanos = maxNanos
        if (intervalFrames >= INTERVAL_FRAMES) {
            // Where the processing thread runs now, and at what clock: read once per 10 s, outside the timed calls.
            val cpu = KrispSdk.currentCpu()
            val interval = CaptureInterval(
                endSeconds = total * FRAME_MS / 1000.0,
                avgFrameMs = intervalNanos / 1e6 / intervalFrames,
                maxFrameMs = intervalMaxNanos / 1e6,
                cpu = cpu,
                cpuMhz = KrispSdk.cpuFreqMhz(cpu),
            )
            lastInterval = interval
            Log.i(TAG, String.format(Locale.US,
                "%s t=%.0fs last 10 s: avg %.3f ms, max %.3f ms per frame, cpu%d @ %s; total load %.2f%%",
                mode, interval.endSeconds, interval.avgFrameMs, interval.maxFrameMs, interval.cpu,
                if (interval.cpuMhz > 0) "${interval.cpuMhz} MHz" else "? MHz",
                ncNanos.get() / 1e9 / (total * FRAME_MS / 1000.0) * 100))
            intervalFrames = 0
            intervalNanos = 0
            intervalMaxNanos = 0
        }
    }

    private fun finish() {
        if (finished.compareAndSet(false, true)) {
            // No thread uses the run any more: free its NC session and buffers before anything else can start.
            KrispSdk.stopCaptureRun(run)
            onFinished(snapshot(), ncError ?: captureError)
        }
    }
}
