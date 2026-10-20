package com.krisp.krisptestapp

import android.net.Uri
import java.io.*
import java.nio.ByteBuffer
import java.nio.ByteOrder

data class WavHeader(
    val audioFormat: Int,   // 1 = PCM16, 3 = IEEE float
    val numChannels: Int,   // must be 1 (mono)
    val sampleRate: Int,
    val byteRate: Int,
    val blockAlign: Int,
    val bitsPerSample: Int,
    val dataSize: Long,     // bytes of audio the data chunk declares (an unsigned 32-bit field)
    val dataOffset: Int,
    val headerBytes: ByteArray // RIFF.. up to and including "data" header
) {
    /** "PCM16" / "FLOAT32" for the formats the app processes, null for any other. */
    val formatName: String?
        get() = when {
            audioFormat == 1 && bitsPerSample == 16 -> "PCM16"
            audioFormat == 3 && bitsPerSample == 32 -> "FLOAT32"
            else -> null
        }

    /** Duration of the declared audio, in seconds. */
    val durationSeconds: Double
        get() = if (sampleRate > 0 && bitsPerSample > 0) dataSize.toDouble() / (sampleRate * (bitsPerSample / 8)) else 0.0
}

/** Reads and checks the header of a mono WAV (up to the start of its audio data). */
fun parseWavHeader(input: InputStream): WavHeader = BufferedInputStream(input).parseWavHeaderMono()

private fun InputStream.readNBytesOrThrow(n: Int): ByteArray {
    val buf = ByteArray(n)
    var off = 0
    while (off < n) {
        val r = read(buf, off, n - off)
        if (r <= 0) throw EOFException("Unexpected EOF while reading $n bytes")
        off += r
    }
    return buf
}

private fun InputStream.parseWavHeaderMono(): WavHeader {
    fun ascii(b: ByteArray) = String(b, Charsets.US_ASCII)
    val out = ByteArrayOutputStream()

    val h12 = readNBytesOrThrow(12).also(out::write) // RIFF size WAVE
    require(ascii(h12.copyOfRange(0, 4)) == "RIFF") { "Not RIFF" }
    require(ascii(h12.copyOfRange(8, 12)) == "WAVE") { "Not WAVE" }

    var audioFormat = -1
    var numChannels = -1
    var sampleRate = -1
    var byteRate = -1
    var blockAlign = -1
    var bitsPerSample = -1
    var dataSize = -1L
    var dataOffset = 12
    var fmtSeen = false

    while (true) {
        val chunkHdr = readNBytesOrThrow(8).also(out::write)
        val id = ascii(chunkHdr.copyOfRange(0, 4))
        val size = ByteBuffer.wrap(chunkHdr, 4, 4).order(ByteOrder.LITTLE_ENDIAN).int
        dataOffset += 8
        when (id) {
            "fmt " -> {
                val fmt = readNBytesOrThrow(size).also(out::write)
                val bb = ByteBuffer.wrap(fmt).order(ByteOrder.LITTLE_ENDIAN)
                audioFormat = bb.short.toInt() and 0xFFFF
                numChannels = bb.short.toInt() and 0xFFFF
                require(numChannels == 1) { "Only mono WAV is supported" }
                sampleRate = bb.int
                byteRate = bb.int
                blockAlign = bb.short.toInt() and 0xFFFF
                bitsPerSample = bb.short.toInt() and 0xFFFF
                dataOffset += size
                if (size % 2 == 1) {
                    out.write(readNBytesOrThrow(1))
                    dataOffset += 1
                }
                fmtSeen = true
            }
            "data" -> {
                require(fmtSeen) { "fmt must precede data" }
                dataSize = size.toLong() and 0xFFFFFFFFL // unsigned: a signed Int turns big sizes negative
                break
            }
            else -> {
                // copy unknown chunk, keep alignment
                val payload = readNBytesOrThrow(size).also(out::write)
                dataOffset += size
                if (size % 2 == 1) {
                    out.write(readNBytesOrThrow(1))
                    dataOffset += 1
                }
            }
        }
    }

    return WavHeader(
        audioFormat, numChannels, sampleRate, byteRate, blockAlign, bitsPerSample,
        dataSize, dataOffset, out.toByteArray()
    )
}

/**
 * Progress of a WAV processing run.
 *
 * [ncNanos] is the time spent inside the Krisp NC calls only, so the speed figures are not skewed by file reading;
 * [elapsedNanos] is the wall time of the whole run, file reading included.
 */
data class WavProcessingStats(
    val sampleRate: Int,
    val frameMs: Int,
    val totalFrames: Long,
    val processedFrames: Long,
    val ncNanos: Long,
    val elapsedNanos: Long,
) {
    /** Duration of the audio processed so far. */
    val audioSeconds: Double get() = processedFrames * frameMs / 1000.0
    val ncSeconds: Double get() = ncNanos / 1e9
    val elapsedSeconds: Double get() = elapsedNanos / 1e9

    /** How many times faster than realtime NC runs: seconds of audio per second of NC processing. */
    val realtimeFactor: Double get() = if (ncNanos > 0) audioSeconds / ncSeconds else 0.0

    /** Average NC processing time of one frame, in milliseconds. */
    val msPerFrame: Double get() = if (processedFrames > 0) ncNanos / 1e6 / processedFrames else 0.0
}

/** Thrown by [WavStreamProcessor.process] when [WavStreamProcessor]'s isStopped returns true. */
class WavProcessingStoppedException(val stats: WavProcessingStats) : Exception("Processing stopped")

class WavStreamProcessor(
    private val openInput: () -> InputStream,
    private val onPcm16StartSession: ((samplingRate: Int) -> Boolean)? = null,
    private val onPcmFloatStartSession: ((samplingRate: Int) -> Boolean)? = null,
    private val onPcm16Frame: ((buffer: ByteArray, valid: Int) -> Boolean)? = null,
    private val onFloat32Frame: ((buffer: ByteArray, valid: Int) -> Boolean)? = null,
    /** Called once the header is parsed, before processing starts. */
    private val onHeader: ((header: WavHeader, stats: WavProcessingStats) -> Unit)? = null,
    /** Called during processing (at most every [progressIntervalMs]) and once at the end. */
    private val onProgress: ((stats: WavProcessingStats) -> Unit)? = null,
    /** Checked before every frame; returning true stops the run with [WavProcessingStoppedException]. */
    private val isStopped: () -> Boolean = { false },
    // 4 updates per second: live enough, and keeps UI work away from the run being benchmarked.
    private val progressIntervalMs: Long = 250,
) {
    private val frameMs: Int = 30 // hardcoded frame duration in milliseconds

    /**
     * Processes the WAV and writes the result (the original header, then the processed audio) to the stream that
     * [createOutput] returns for the output's exact size in bytes, e.g. a [NativeAudioBuffer]. The output is created
     * before the clock starts, so allocating it doesn't count as processing time.
     */
    fun process(createOutput: (capacity: Long) -> OutputStream): WavProcessingStats {
        openInput().use { raw ->
            BufferedInputStream(raw).use { input ->
                val hdr = input.parseWavHeaderMono()
                val bytesPerSample = hdr.bitsPerSample / 8
                require(bytesPerSample in setOf(2, 4)) {
                    "Unsupported bitsPerSample=${hdr.bitsPerSample}; expected 16 or 32f"
                }
                require(hdr.dataSize > 0) { "WAV has no audio data (data chunk size ${hdr.dataSize})" }

                val sessionStarted = if (hdr.audioFormat == 1 && hdr.bitsPerSample == 16) {
                    onPcm16StartSession?.invoke(hdr.sampleRate) ?: true
                }
                else if ( hdr.audioFormat == 3 && hdr.bitsPerSample == 32) {
                    onPcmFloatStartSession?.invoke(hdr.sampleRate) ?: true
                }
                else {
                    throw IllegalArgumentException(
                        "Unsupported format: audioFormat=${hdr.audioFormat}, bits=${hdr.bitsPerSample}"
                    )
                }
                // Processing frames without a session would crash in native code.
                check(sessionStarted) {
                    "Failed to start a Krisp NC session for ${hdr.sampleRate} Hz " +
                        "(is the model loaded, and does it support this sample rate?)"
                }

                val frameHandler: (ByteArray, Int) -> Boolean =
                    if (hdr.audioFormat == 1 && hdr.bitsPerSample == 16) {
                        { buf, valid -> onPcm16Frame?.invoke(buf, valid) ?: true }
                    } else {
                        { buf, valid -> onFloat32Frame?.invoke(buf, valid) ?: true }
                    }

                val frameBytes = ((hdr.sampleRate * frameMs) / 1000) * bytesPerSample
                require(frameBytes > 0) { "Frame too small for ${hdr.sampleRate} Hz" }

                // A trailing partial frame is processed too (zero-padded), so it counts as a frame.
                val totalFrames = (hdr.dataSize + frameBytes - 1) / frameBytes

                val out = createOutput(hdr.headerBytes.size.toLong() + hdr.dataSize)
                out.write(hdr.headerBytes)

                val startNanos = System.nanoTime()
                var processedFrames = 0L
                var ncNanos = 0L
                fun stats() = WavProcessingStats(
                    hdr.sampleRate, frameMs, totalFrames, processedFrames, ncNanos, System.nanoTime() - startNanos
                )
                onHeader?.invoke(hdr, stats())

                val progressIntervalNanos = progressIntervalMs * 1_000_000
                var lastProgressNanos = startNanos
                val buffer = ByteArray(frameBytes)
                var remaining = hdr.dataSize
                var totalWritten = 0L
                while (remaining > 0) {
                    if (isStopped()) throw WavProcessingStoppedException(stats())

                    // A frame needs frameBytes; one read() may return less even mid-file, so keep reading.
                    var read = 0
                    val toRead = minOf(buffer.size.toLong(), remaining).toInt()
                    while (read < toRead) {
                        val r = input.read(buffer, read, toRead - read)
                        if (r <= 0) break
                        read += r
                    }
                    if (read <= 0) break

                    val fullFrameForProcessing = if (read < frameBytes) {
                        // make a full frame copy padded with zeros for DSP, but only write back the original count
                        val tmp = buffer.copyOf(frameBytes)
                        java.util.Arrays.fill(tmp, read, frameBytes, 0)
                        tmp
                    } else buffer

                    val frameStartNanos = System.nanoTime()
                    val frameOk = frameHandler(fullFrameForProcessing, frameBytes)
                    ncNanos += System.nanoTime() - frameStartNanos
                    check(frameOk) { "Krisp NC failed on frame ${processedFrames + 1} of $totalFrames" }
                    processedFrames++

                    // Write only the originally read bytes; header remains unchanged
                    out.write(fullFrameForProcessing, 0, read)
                    totalWritten += read
                    remaining -= read

                    val now = System.nanoTime()
                    if (onProgress != null && now - lastProgressNanos >= progressIntervalNanos) {
                        lastProgressNanos = now
                        onProgress.invoke(stats())
                    }

                    if (read < frameBytes) break // last chunk handled
                }
                // The file ended before the audio its header declares: a truncated or corrupt WAV, not a complete run.
                if (remaining > 0) {
                    throw IOException(
                        "The WAV file is truncated: its header declares ${hdr.dataSize} bytes of audio, " +
                            "but the file ends after $totalWritten"
                    )
                }
                out.flush()
                val finalStats = stats()
                onProgress?.invoke(finalStats)
                return finalStats
            }
        }
    }
}