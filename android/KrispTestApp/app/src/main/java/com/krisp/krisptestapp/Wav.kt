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
    val dataSize: Int,
    val dataOffset: Int,
    val headerBytes: ByteArray // RIFF.. up to and including "data" header
)

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
    var dataSize = -1
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
                dataSize = size
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

class WavStreamProcessor(
    private val openInput: () -> InputStream,
    private val onPcm16StartSession: ((samplingRate: Int) -> Boolean)? = null,
    private val onPcmFloatStartSession: ((samplingRate: Int) -> Boolean)? = null,
    private val onPcm16Frame: ((buffer: ByteArray, valid: Int) -> Boolean)? = null,
    private val onFloat32Frame: ((buffer: ByteArray, valid: Int) -> Boolean)? = null
) {
    private val frameMs: Int = 30 // hardcoded frame duration in milliseconds

    fun processToMemory(): ByteArray {
        openInput().use { raw ->
            BufferedInputStream(raw).use { input ->
                val hdr = input.parseWavHeaderMono()
                val bytesPerSample = hdr.bitsPerSample / 8
                require(bytesPerSample in setOf(2, 4)) {
                    "Unsupported bitsPerSample=${hdr.bitsPerSample}; expected 16 or 32f"
                }

                if (hdr.audioFormat == 1 && hdr.bitsPerSample == 16) {
                    onPcm16StartSession?.invoke(hdr.sampleRate)
                }
                else if ( hdr.audioFormat == 3 && hdr.bitsPerSample == 32) {
                    onPcmFloatStartSession?.invoke(hdr.sampleRate)
                }
                else {
                    throw IllegalArgumentException(
                        "Unsupported format: audioFormat=${hdr.audioFormat}, bits=${hdr.bitsPerSample}"
                    )
                }

                val frameHandler: (ByteArray, Int) -> Boolean =
                    if (hdr.audioFormat == 1 && hdr.bitsPerSample == 16) {
                        { buf, valid -> onPcm16Frame?.invoke(buf, valid) ?: true }
                    } else {
                        { buf, valid -> onFloat32Frame?.invoke(buf, valid) ?: true }
                    }

                val frameBytes = ((hdr.sampleRate * frameMs) / 1000) * bytesPerSample
                require(frameBytes > 0) { "Frame too small for ${hdr.sampleRate} Hz" }

                val out = ByteArrayOutputStream(hdr.headerBytes.size + hdr.dataSize)
                out.write(hdr.headerBytes)

                val buffer = ByteArray(frameBytes)
                var remaining = hdr.dataSize
                var totalWritten = 0
                while (remaining > 0) {
                    val toRead = minOf(buffer.size, remaining)
                    val read = input.read(buffer, 0, toRead)
                    if (read <= 0) break

                    val fullFrameForProcessing = if (read < frameBytes) {
                        // make a full frame copy padded with zeros for DSP, but only write back the original count
                        val tmp = buffer.copyOf(frameBytes)
                        java.util.Arrays.fill(tmp, read, frameBytes, 0)
                        tmp
                    } else buffer

                    val continueProcessing = frameHandler(fullFrameForProcessing, frameBytes)
                    if (!continueProcessing) break

                    // Write only the originally read bytes; header remains unchanged
                    out.write(fullFrameForProcessing, 0, read)
                    totalWritten += read
                    remaining -= read

                    if (read < frameBytes) break // last chunk handled
                }
                return out.toByteArray()
            }
        }
    }
}