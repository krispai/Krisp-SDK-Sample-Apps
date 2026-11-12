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
    val dataSize: Long,     // bytes of audio the data chunk declares (an unsigned 32-bit field), see audioBytes
    val dataOffset: Long,   // where the audio starts in the file
) {
    /** "PCM16" / "FLOAT32" for the formats the app processes, null for any other. */
    val formatName: String?
        get() = when {
            audioFormat == 1 && bitsPerSample == 16 -> "PCM16"
            audioFormat == 3 && bitsPerSample == 32 -> "FLOAT32"
            else -> null
        }

    /** The declared data size is a placeholder (0 or 0xFFFFFFFF), as streaming recorders leave it. */
    val dataSizeUnknown: Boolean
        get() = dataSize == 0L || dataSize == 0xFFFFFFFFL

    /**
     * Bytes of audio to process in a file of [fileSize] bytes (-1 if unknown): the declared size, or up to the end of
     * the file when the header gives no size or more than the file holds (some recorders leave it wrong). Null when
     * it can't be told: no size in the header, and the file size is unknown.
     */
    fun audioBytes(fileSize: Long): Long? {
        val inFile = if (fileSize > 0) (fileSize - dataOffset).coerceAtLeast(0) else -1L
        return when {
            inFile >= 0 && (dataSizeUnknown || dataSize > inFile) -> inFile
            dataSizeUnknown -> null
            else -> dataSize
        }
    }

    /** Duration of [bytes] of audio in this format, in seconds. */
    fun seconds(bytes: Long): Double =
        if (sampleRate > 0 && bitsPerSample > 0) bytes.toDouble() / (sampleRate * (bitsPerSample / 8)) else 0.0
}

/** Reads and checks the header of a mono WAV (up to the start of its audio data). */
fun parseWavHeader(input: InputStream): WavHeader = BufferedInputStream(input).parseWavHeaderMono()

/** The 44-byte header of a mono WAV ([audioFormat] 1 = PCM, 3 = IEEE float) whose audio is [dataBytes] long. */
fun monoWavHeader(audioFormat: Int, bitsPerSample: Int, sampleRate: Int, dataBytes: Long): ByteArray {
    val data = dataBytes.coerceAtMost(0xFFFFFFFFL - 36) // a RIFF size is 32 bits
    val bytesPerSample = bitsPerSample / 8
    return ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN).apply {
        put("RIFF".toByteArray(Charsets.US_ASCII))
        putInt((36 + data).toInt())
        put("WAVE".toByteArray(Charsets.US_ASCII))
        put("fmt ".toByteArray(Charsets.US_ASCII))
        putInt(16)                              // fmt chunk size
        putShort(audioFormat.toShort())
        putShort(1)                             // mono
        putInt(sampleRate)
        putInt(sampleRate * bytesPerSample)     // byte rate
        putShort(bytesPerSample.toShort())      // block align
        putShort(bitsPerSample.toShort())
        put("data".toByteArray(Charsets.US_ASCII))
        putInt(data.toInt())
    }.array()
}

/** The 44-byte header of a mono IEEE float (32-bit) WAV whose audio is [dataBytes] long. */
fun floatMonoWavHeader(sampleRate: Int, dataBytes: Long): ByteArray = monoWavHeader(3, 32, sampleRate, dataBytes)

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

/** Skips [n] bytes of the [chunk] chunk; throws if the file ends first. */
private fun InputStream.skipChunkBytes(n: Long, chunk: String) {
    var left = n
    while (left > 0) {
        val skipped = skip(left)
        if (skipped > 0) {
            left -= skipped
        } else {
            // skip() may skip nothing without being at the end: a read tells.
            if (read() < 0) throw EOFException("The WAV file ends inside its \"$chunk\" chunk")
            left--
        }
    }
}

/** The sample rates of Krisp::AudioSdk::SamplingRate, the ones krisp.cpp maps. */
private val SDK_SAMPLE_RATES = setOf(8000, 16000, 24000, 32000, 44100, 48000, 88200, 96000)

private fun InputStream.parseWavHeaderMono(): WavHeader {
    fun ascii(b: ByteArray, from: Int) = String(b, from, 4, Charsets.US_ASCII)

    val riff = readNBytesOrThrow(12) // RIFF size WAVE
    require(ascii(riff, 0) == "RIFF") { "Not RIFF" }
    require(ascii(riff, 8) == "WAVE") { "Not WAVE" }

    var audioFormat = -1
    var numChannels = -1
    var sampleRate = -1
    var byteRate = -1
    var blockAlign = -1
    var bitsPerSample = -1
    var dataSize = -1L
    var dataOffset = 12L
    var fmtSeen = false

    while (true) {
        val chunkHdr = readNBytesOrThrow(8)
        val id = ascii(chunkHdr, 0)
        // Unsigned: a signed Int turns sizes of 2 GB and more negative.
        val size = ByteBuffer.wrap(chunkHdr, 4, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL
        dataOffset += 8
        if (id == "data") {
            require(fmtSeen) { "fmt must precede data" }
            dataSize = size
            break
        }
        // Chunks are word-aligned: an odd size is followed by a pad byte.
        val padded = size + (size and 1L)
        if (id == "fmt ") {
            require(size >= 16) { "The fmt chunk is too small ($size bytes)" }
            val bb = ByteBuffer.wrap(readNBytesOrThrow(16)).order(ByteOrder.LITTLE_ENDIAN)
            audioFormat = bb.short.toInt() and 0xFFFF
            numChannels = bb.short.toInt() and 0xFFFF
            require(numChannels == 1) { "Only mono WAV is supported" }
            sampleRate = bb.int
            // Only the SDK's rates: anything else is a corrupt header (or one the SDK can't take), caught at import.
            require(sampleRate in SDK_SAMPLE_RATES) {
                "Unsupported sample rate: ${sampleRate.toLong() and 0xFFFFFFFFL} Hz " +
                    "(the SDK takes 8, 16, 24, 32, 44.1, 48, 88.2 and 96 kHz)"
            }
            byteRate = bb.int
            blockAlign = bb.short.toInt() and 0xFFFF
            bitsPerSample = bb.short.toInt() and 0xFFFF
            skipChunkBytes(padded - 16, id) // a format extension, if any
            fmtSeen = true
        } else {
            // Metadata (LIST, bext, JUNK, fact...): not needed, the output gets a header of its own. Skipped, not read
            // into memory: these chunks can be big.
            skipChunkBytes(padded, id)
        }
        dataOffset += padded
    }

    return WavHeader(audioFormat, numChannels, sampleRate, byteRate, blockAlign, bitsPerSample, dataSize, dataOffset)
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
    /** Audio the header declares that the file doesn't have (it ended early), in seconds; set when the run ends. */
    val missingSeconds: Double = 0.0,
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
    /** NC on one frame in place; returns the NC call's time in ns (measured natively), negative on failure. */
    private val onPcm16Frame: ((buffer: ByteArray, valid: Int) -> Long)? = null,
    private val onFloat32Frame: ((buffer: ByteArray, valid: Int) -> Long)? = null,
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
     * Processes the WAV, a file of [fileSize] bytes (-1 if unknown), and writes the processed audio (no header:
     * [monoWavHeader] makes one for the size actually written) to the stream that [createOutput] returns for the
     * audio's size in bytes, e.g. a [NativeAudioBuffer]. The output is created before the clock starts, so allocating
     * it doesn't count as processing time.
     *
     * The audio runs to the end of the file when the header's data size is a placeholder or bigger than the file (see
     * [WavHeader.audioBytes]). A file that ends before the audio its header declares is processed up to its end, and
     * kept: [WavProcessingStats.missingSeconds] tells how much was missing.
     */
    fun process(fileSize: Long, createOutput: (capacity: Long) -> OutputStream): WavProcessingStats {
        openInput().use { raw ->
            BufferedInputStream(raw).use { input ->
                val hdr = input.parseWavHeaderMono()
                val bytesPerSample = hdr.bitsPerSample / 8
                require(bytesPerSample in setOf(2, 4)) {
                    "Unsupported bitsPerSample=${hdr.bitsPerSample}; expected 16 or 32f"
                }
                val audioBytes = hdr.audioBytes(fileSize) ?: throw IOException(
                    "The WAV header gives no audio size and the file size is unknown: the output can't be allocated"
                )
                require(audioBytes > 0) { "The WAV file has no audio data" }

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

                val frameHandler: (ByteArray, Int) -> Long =
                    if (hdr.audioFormat == 1 && hdr.bitsPerSample == 16) {
                        { buf, valid -> onPcm16Frame?.invoke(buf, valid) ?: 0L }
                    } else {
                        { buf, valid -> onFloat32Frame?.invoke(buf, valid) ?: 0L }
                    }

                val frameBytes = ((hdr.sampleRate * frameMs) / 1000) * bytesPerSample
                require(frameBytes > 0) { "Frame too small for ${hdr.sampleRate} Hz" }

                // A trailing partial frame is processed too (zero-padded), so it counts as a frame.
                val totalFrames = (audioBytes + frameBytes - 1) / frameBytes

                val out = createOutput(audioBytes)

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
                var remaining = audioBytes
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

                    // The SDK's process() only, timed in native code: no JNI or copies in the number.
                    val frameNanos = frameHandler(fullFrameForProcessing, frameBytes)
                    check(frameNanos >= 0) { "Krisp NC failed on frame ${processedFrames + 1} of $totalFrames" }
                    ncNanos += frameNanos
                    processedFrames++

                    // Write only the originally read bytes
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
                out.flush()
                // The file ended before the audio its header declares (truncated, or a stale size): what it has is
                // processed and kept, and the stats say how much was missing.
                val missingBytes = if (hdr.dataSizeUnknown) 0L else (hdr.dataSize - totalWritten).coerceAtLeast(0L)
                val finalStats = stats().copy(missingSeconds = hdr.seconds(missingBytes))
                onProgress?.invoke(finalStats)
                return finalStats
            }
        }
    }
}