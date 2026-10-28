package com.krisp.krisptestapp

import java.io.IOException
import java.io.OutputStream

/**
 * The processed WAV, kept in native memory as a vector of fixed-size buffers (see ChunkedBuffer in krisp.cpp).
 *
 * Long recordings don't fit in the Java heap: it is capped at 256 MB on many phones, a single array for the whole
 * file needs one contiguous block, and on Android even direct ByteBuffers are allocated from it. Native memory is
 * limited only by the device. All buffers are allocated (and their pages committed) by [allocate], before
 * processing starts, so no allocation happens while frames are processed and timed.
 *
 * Writing past the allocated capacity fails. [close] doesn't free the memory (the data is still needed to save it);
 * [release] does.
 */
class NativeAudioBuffer private constructor(private var handle: Long) : OutputStream() {

    companion object {
        init {
            System.loadLibrary("krisptestapp")
        }

        const val DEFAULT_CHUNK_BYTES = 16 * 1024 * 1024

        /** Allocates [capacity] bytes in buffers of [chunkBytes]; throws [OutOfMemoryError] if the device can't. */
        fun allocate(capacity: Long, chunkBytes: Int = DEFAULT_CHUNK_BYTES): NativeAudioBuffer {
            val handle = nativeAllocate(capacity, chunkBytes)
            if (handle == 0L) {
                throw OutOfMemoryError("Cannot allocate ${capacity / (1024 * 1024)} MB of native memory for the output")
            }
            return NativeAudioBuffer(handle)
        }

        @JvmStatic private external fun nativeAllocate(capacity: Long, chunkBytes: Int): Long
        @JvmStatic private external fun nativeWrite(handle: Long, buf: ByteArray, off: Int, len: Int): Boolean
        @JvmStatic private external fun nativeSize(handle: Long): Long
        @JvmStatic private external fun nativeWriteToFd(handle: Long, fd: Int): Boolean
        @JvmStatic private external fun nativeFree(handle: Long)
    }

    /** Bytes written so far. */
    val size: Long
        get() = if (handle == 0L) 0 else nativeSize(handle)

    override fun write(b: Int) {
        write(byteArrayOf(b.toByte()), 0, 1)
    }

    override fun write(b: ByteArray, off: Int, len: Int) {
        check(handle != 0L) { "NativeAudioBuffer was released" }
        if (!nativeWrite(handle, b, off, len)) {
            throw IOException("Writing $len bytes to the native audio buffer failed (capacity exceeded?)")
        }
    }

    /** The native ChunkedBuffer, for native code that appends to it directly (the audio capture). */
    val nativeHandle: Long
        get() = handle.also { check(it != 0L) { "NativeAudioBuffer was released" } }

    /** Writes everything written so far to the file descriptor (e.g. ParcelFileDescriptor.fd of the save target). */
    fun writeTo(fd: Int) {
        check(handle != 0L) { "NativeAudioBuffer was released" }
        if (!nativeWriteToFd(handle, fd)) {
            throw IOException("Writing the processed audio to the file failed")
        }
    }

    /** Frees the native memory. The buffer can't be used afterwards. */
    fun release() {
        if (handle != 0L) {
            nativeFree(handle)
            handle = 0L
        }
    }
}
