#include <android/log.h>
#include <jni.h>
#include <sys/auxv.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <stdexcept>
#include <vector>

#include <krisp-audio-api-definitions.hpp>
#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>


// Logging. SDK messages go to logcat under "KrispSDK", this glue's own errors under "KrispTestApp":
//   adb logcat -s KrispSDK KrispTestApp WavProcess
// The last error (from either) is kept, so the app can show it in its error dialogs.
namespace {

constexpr const char *kSdkLogTag = "KrispSDK";
constexpr const char *kAppLogTag = "KrispTestApp";

// Set by krispInit before the SDK is initialized; Warn unless the app asks for more.
Krisp::AudioSdk::LogLevel gSdkLogLevel = Krisp::AudioSdk::LogLevel::Warn;

std::mutex gLastErrorMutex;
std::string gLastError;

void rememberError(const std::string &message) {
    std::lock_guard<std::mutex> lock(gLastErrorMutex);
    gLastError = message;
}

int androidLogPriority(Krisp::AudioSdk::LogLevel level) {
    using Krisp::AudioSdk::LogLevel;
    switch (level) {
        case LogLevel::Trace: return ANDROID_LOG_VERBOSE;
        case LogLevel::Debug: return ANDROID_LOG_DEBUG;
        case LogLevel::Info: return ANDROID_LOG_INFO;
        case LogLevel::Warn: return ANDROID_LOG_WARN;
        case LogLevel::Err: return ANDROID_LOG_ERROR;
        case LogLevel::Critical: return ANDROID_LOG_FATAL; // __android_log_print doesn't abort, unlike log_assert
        default: return ANDROID_LOG_INFO;
    }
}

// The SDK's log callback. It may be called from SDK threads, so it must not throw.
void sdkLogCallback(const std::string &message, Krisp::AudioSdk::LogLevel level) {
    try {
        __android_log_print(androidLogPriority(level), kSdkLogTag, "%s", message.c_str());
        if (level == Krisp::AudioSdk::LogLevel::Err || level == Krisp::AudioSdk::LogLevel::Critical) {
            rememberError(message);
        }
    } catch (...) {
    }
}

// Logs an error of this glue code (typically an exception the SDK threw) and keeps it as the last error.
void logAppError(const char *where, const std::string &what) {
    const std::string message = std::string(where) + ": " + what;
    __android_log_print(ANDROID_LOG_ERROR, kAppLogTag, "%s", message.c_str());
    rememberError(message);
}

// Whether the CPU does FP16 arithmetic (ARMv8.2-A FP16, "asimdhp" in /proc/cpuinfo), as the kernel reports it.
bool cpuHasFp16Arithmetic() {
#if defined(__aarch64__)
    constexpr unsigned long kHwcapAsimdhp = 1UL << 10; // HWCAP_ASIMDHP, arm64
    return (getauxval(AT_HWCAP) & kHwcapAsimdhp) != 0;
#elif defined(__arm__)
    constexpr unsigned long kHwcapAsimdhp = 1UL << 23; // HWCAP_ASIMDHP, arm (32-bit process on an ARMv8.2 core)
    return (getauxval(AT_HWCAP) & kHwcapAsimdhp) != 0;
#else
    return false;
#endif
}

} // namespace


// The processed WAV, kept in native memory as a vector of fixed-size buffers. A long recording doesn't fit in the
// Java heap (256 MB on many phones, and even ByteBuffer.allocateDirect is carved from it on Android); native memory
// is limited only by the device. All buffers are allocated and touched up front, before processing starts, so no
// allocation or page fault happens while frames are being processed and timed.
class ChunkedBuffer final {
public:
    static std::unique_ptr<ChunkedBuffer> allocate(size_t capacity, size_t chunkSize) {
        if (chunkSize == 0) {
            return nullptr;
        }
        std::unique_ptr<ChunkedBuffer> buffer(new (std::nothrow) ChunkedBuffer(capacity, chunkSize));
        if (!buffer) {
            return nullptr;
        }
        try {
            buffer->_chunks.reserve((capacity + chunkSize - 1) / chunkSize);
            for (size_t allocated = 0; allocated < capacity; allocated += chunkSize) {
                const size_t size = std::min(chunkSize, capacity - allocated);
                std::unique_ptr<uint8_t[]> chunk(new (std::nothrow) uint8_t[size]);
                if (!chunk) {
                    return nullptr;
                }
                std::memset(chunk.get(), 0, size); // commit the pages now, not during processing
                buffer->_chunks.push_back(std::move(chunk));
            }
        } catch (const std::bad_alloc &) {
            return nullptr;
        }
        return buffer;
    }

    // Appends len bytes of the Java array, starting at off.
    bool write(JNIEnv *env, jbyteArray src, jint off, jint len) {
        if (off < 0 || len < 0 || static_cast<size_t>(len) > _capacity - _size) {
            return false;
        }
        size_t copied = 0;
        while (copied < static_cast<size_t>(len)) {
            const size_t chunkIndex = _size / _chunkSize;
            const size_t chunkOffset = _size % _chunkSize;
            const size_t n = std::min(static_cast<size_t>(len) - copied, _chunkSize - chunkOffset);
            env->GetByteArrayRegion(src, off + static_cast<jint>(copied), static_cast<jsize>(n),
                                    reinterpret_cast<jbyte *>(_chunks[chunkIndex].get() + chunkOffset));
            if (env->ExceptionCheck()) {
                return false;
            }
            copied += n;
            _size += n;
        }
        return true;
    }

    // Writes everything written so far to the file descriptor.
    bool writeTo(int fd) const {
        size_t remaining = _size;
        for (const auto &chunk : _chunks) {
            if (remaining == 0) {
                break;
            }
            const size_t chunkBytes = std::min(_chunkSize, remaining);
            size_t done = 0;
            while (done < chunkBytes) {
                const ssize_t n = ::write(fd, chunk.get() + done, chunkBytes - done);
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return false;
                }
                done += static_cast<size_t>(n);
            }
            remaining -= chunkBytes;
        }
        return true;
    }

    size_t size() const {
        return _size;
    }

private:
    ChunkedBuffer(size_t capacity, size_t chunkSize) : _capacity(capacity), _chunkSize(chunkSize) {}

    std::vector<std::unique_ptr<uint8_t[]>> _chunks;
    size_t _capacity;
    size_t _chunkSize;
    size_t _size = 0;
};


class KrispLibrary final {
public:
    typedef std::shared_ptr<Krisp::AudioSdk::Nc<float>> SessionTypePcmFloat;
    typedef std::shared_ptr<Krisp::AudioSdk::Nc<int16_t>> SessionTypePcm16;

    // Initializes the SDK on first use (krispInit at app start). If globalInit throws, the exception propagates and
    // the next call tries again (a function-local static is only set once its initializer completes).
    static KrispLibrary * singletone() {
        static KrispLibrary * _ptr = [] {
            Krisp::AudioSdk::globalInit(L"", sdkLogCallback, gSdkLogLevel);
            return new KrispLibrary;
        }();
        return _ptr;
    }

    void loadModel(const jbyte *modelPtr, size_t modelSize) {
        _modelData = std::make_unique<uint8_t[]>(modelSize);
        std::memcpy(_modelData.get(), modelPtr, modelSize);
        _modelInfo.blob.first = _modelData.get();
        _modelInfo.blob.second = modelSize;

        Krisp::AudioSdk::NcSessionConfig ncConfig;
        ncConfig.modelInfo = &_modelInfo;
        ncConfig.inputFrameDuration = Krisp::AudioSdk::FrameDuration::Fd10ms;
        ncConfig.inputSampleRate = Krisp::AudioSdk::SamplingRate::Sr32000Hz;
        ncConfig.outputSampleRate = Krisp::AudioSdk::SamplingRate::Sr32000Hz;
        ncConfig.enableSessionStats = false;

        this->_cachedSessionPtr = Krisp::AudioSdk::Nc<float>::create(ncConfig);
    }


    Krisp::AudioSdk::VersionInfo getVersion() const {
        Krisp::AudioSdk::VersionInfo info {0};
        Krisp::AudioSdk::getVersion(info);
        return info;
    }

    ~KrispLibrary() {
        Krisp::AudioSdk::globalDestroy();
    }

    void startSessionPcm16(unsigned int samplingRate, bool lowPrecision) {
        endSession();
        _activePcm16SessionPtr = createSession<int16_t>(samplingRate, lowPrecision);
    }

    void startSessionPcmFloat(unsigned int samplingRate, bool lowPrecision) {
        endSession();
        _activePcmFloatSessionPtr = createSession<float>(samplingRate, lowPrecision);
    }

    void processFramePcm16(const int16_t * pcm16Samples, int16_t * processPcm16Samples, size_t sampleCount) {
        if (!_activePcm16SessionPtr) {
            throw std::runtime_error("No active PCM16 NC session");
        }
        _activePcm16SessionPtr->process(
                pcm16Samples,
                sampleCount,
                processPcm16Samples,
                sampleCount,
                100.0,
                nullptr);
    }

    void processFramePcmFloat(const float * floatSamples, float * processFloatSamples, size_t sampleCount) {
        if (!_activePcmFloatSessionPtr) {
            throw std::runtime_error("No active PCM float NC session");
        }
        _activePcmFloatSessionPtr->process(
                floatSamples,
                sampleCount,
                processFloatSamples,
                sampleCount,
                100.0,
                nullptr);
    }

    void endSession() {
        _activePcmFloatSessionPtr = nullptr;
        _activePcm16SessionPtr = nullptr;
    }
private:
    static Krisp::AudioSdk::SamplingRate getKrispSamplingRate(unsigned int samplingRate) {
        switch (samplingRate) {
            case 8000:
                return Krisp::AudioSdk::SamplingRate::Sr8000Hz;
            case 16000:
                return Krisp::AudioSdk::SamplingRate::Sr16000Hz;
            case 24000:
                return Krisp::AudioSdk::SamplingRate::Sr24000Hz;
            case 32000:
                return Krisp::AudioSdk::SamplingRate::Sr32000Hz;
            case 44100:
                return Krisp::AudioSdk::SamplingRate::Sr44100Hz;
            case 48000:
                return Krisp::AudioSdk::SamplingRate::Sr48000Hz;
            case 88200:
                return Krisp::AudioSdk::SamplingRate::Sr88200Hz;
            case 96000:
                return Krisp::AudioSdk::SamplingRate::Sr96000Hz;
            default:
                throw std::runtime_error("Unsupported sampling rate");
        }
    }

    template<class SampleType>
    std::shared_ptr<Krisp::AudioSdk::Nc<SampleType>> createSession(unsigned int samplingRate, bool lowPrecision) {
        Krisp::AudioSdk::SamplingRate krispSamplingRate = getKrispSamplingRate(samplingRate);
        Krisp::AudioSdk::NcSessionConfig ncConfig;
        ncConfig.modelInfo = &_modelInfo;
        ncConfig.inputFrameDuration = Krisp::AudioSdk::FrameDuration::Fd30ms;
        ncConfig.inputSampleRate = krispSamplingRate;
        ncConfig.outputSampleRate = krispSamplingRate;
        ncConfig.enableSessionStats = false;
        // fp16 for models that support it, on CPUs with FP16 arithmetic; the SDK runs fp32 otherwise.
        ncConfig.enableLowPrecisionExecution = lowPrecision;
        return Krisp::AudioSdk::Nc<SampleType>::create(ncConfig);
    }

    std::unique_ptr<uint8_t[]> _modelData;
    Krisp::AudioSdk::ModelInfo _modelInfo;
    SessionTypePcmFloat _cachedSessionPtr;
    SessionTypePcmFloat _activePcmFloatSessionPtr;
    SessionTypePcm16 _activePcm16SessionPtr;

    KrispLibrary() {
    }
};

extern "C"
JNIEXPORT jint JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispLoadModel(
        JNIEnv* env, jobject /*thiz*/, jbyteArray modelBytes, jint modelSize) {
    if (modelBytes == nullptr || modelSize <= 0) return -1;

    // Option A: Pinned or copied pointer
    jboolean isCopy = JNI_FALSE;
    jbyte* modelDataPtr = env->GetByteArrayElements(modelBytes, &isCopy);
    if (!modelDataPtr) return -2;

    int rc = 0; // stub return for now

    try {
        KrispLibrary::singletone()->loadModel(modelDataPtr, modelSize);
    }
    catch (const std::exception & err) {
        logAppError("Loading the model failed", err.what());
        rc = -1;
    }
    catch (...) {
        logAppError("Loading the model failed", "unknown error");
        rc = -1;
    }

    env->ReleaseByteArrayElements(modelBytes, modelDataPtr, JNI_ABORT); // don't modify Java array
    return rc;
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispNcFramePcmFloat(JNIEnv *env, jobject thiz,
                                                                    jbyteArray buf, jint valid) {
    {
        if (buf == nullptr || valid <= 0) return JNI_FALSE;

        // Access raw bytes; Critical minimizes copying and GC moves.
        jboolean isCopy = JNI_FALSE;
        void* raw = env->GetPrimitiveArrayCritical(buf, &isCopy);
        if (!raw) return JNI_FALSE;

        jboolean ok = JNI_FALSE;
        try {
            // Treat the underlying jbyte[] as an array of float samples (PCM float)
            // valid is in bytes, so the sample count is valid / sizeof(float)
            if ((valid % sizeof(float)) != 0) {
                // not an integral number of float samples
                env->ReleasePrimitiveArrayCritical(buf, raw, 0);
                return JNI_FALSE;
            }
            size_t sampleCount = static_cast<size_t>(valid) / sizeof(float);
            float* samples = reinterpret_cast<float*>(raw);

            // In-place processing: input and output are the same buffer
            KrispLibrary::singletone()->processFramePcmFloat(samples, samples, sampleCount);
            ok = JNI_TRUE;
        } catch (const std::exception &err) {
            logAppError("Processing a float frame failed", err.what());
            ok = JNI_FALSE;
        } catch (...) {
            logAppError("Processing a float frame failed", "unknown error");
            ok = JNI_FALSE;
        }

        // 0 => commit changes back to the Java byte[]
        env->ReleasePrimitiveArrayCritical(buf, raw, 0);
        return ok;
    }
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispNcFramePcm16(JNIEnv *env, jobject thiz,
                                                                 jbyteArray buf, jint valid) {
    {
        if (buf == nullptr || valid <= 0) return JNI_FALSE;

        jboolean isCopy = JNI_FALSE;
        void* raw = env->GetPrimitiveArrayCritical(buf, &isCopy);
        if (!raw) return JNI_FALSE;

        jboolean ok = JNI_FALSE;
        try {
            if ((valid % sizeof(int16_t)) != 0) {
                env->ReleasePrimitiveArrayCritical(buf, raw, 0);
                return JNI_FALSE;
            }
            size_t sampleCount = static_cast<size_t>(valid) / sizeof(int16_t);
            int16_t* samples = reinterpret_cast<int16_t*>(raw);

            KrispLibrary::singletone()->processFramePcm16(samples, samples, sampleCount);
            ok = JNI_TRUE;
        } catch (const std::exception &err) {
            logAppError("Processing a PCM16 frame failed", err.what());
            ok = JNI_FALSE;
        } catch (...) {
            logAppError("Processing a PCM16 frame failed", "unknown error");
            ok = JNI_FALSE;
        }

        env->ReleasePrimitiveArrayCritical(buf, raw, 0);
        return ok;
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispStartNcSessionPcmFloat(JNIEnv *env,
                                                                           jobject thiz,
                                                                           jint sampling_rate,
                                                                           jboolean low_precision) {
    {
        try {
            KrispLibrary::singletone()->startSessionPcmFloat(static_cast<unsigned int>(sampling_rate),
                                                             low_precision == JNI_TRUE);
            return JNI_TRUE;
        } catch (const std::exception &err) {
            logAppError("Creating the float NC session failed", err.what());
            return JNI_FALSE;
        } catch (...) {
            logAppError("Creating the float NC session failed", "unknown error");
            return JNI_FALSE;
        }
    }
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispStartNcSessionPcm16(JNIEnv *env, jobject thiz,
                                                                        jint sampling_rate,
                                                                        jboolean low_precision) {
    {
        try {
            KrispLibrary::singletone()->startSessionPcm16(static_cast<unsigned int>(sampling_rate),
                                                          low_precision == JNI_TRUE);
            return JNI_TRUE;
        } catch (const std::exception &err) {
            logAppError("Creating the PCM16 NC session failed", err.what());
            return JNI_FALSE;
        } catch (...) {
            logAppError("Creating the PCM16 NC session failed", "unknown error");
            return JNI_FALSE;
        }
    }
}

// Sets the SDK log level and initializes the SDK (globalInit with the logcat callback). Call once at app start,
// before anything else touches the SDK. [log_level] is a Krisp::AudioSdk::LogLevel value (2 = Info, 3 = Warn, ...).
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispInit(JNIEnv *env, jobject /*thiz*/, jint log_level) {
    if (log_level >= static_cast<jint>(Krisp::AudioSdk::LogLevel::Trace) &&
        log_level <= static_cast<jint>(Krisp::AudioSdk::LogLevel::Off)) {
        gSdkLogLevel = static_cast<Krisp::AudioSdk::LogLevel>(log_level);
    }
    try {
        KrispLibrary::singletone();
        return JNI_TRUE;
    } catch (const std::exception &err) {
        logAppError("Krisp Audio SDK initialization (globalInit) failed", err.what());
    } catch (...) {
        logAppError("Krisp Audio SDK initialization (globalInit) failed", "unknown error");
    }
    return JNI_FALSE;
}

// The last error the SDK logged or this glue caught, "" if none; clears it.
extern "C"
JNIEXPORT jstring JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispTakeLastError(JNIEnv *env, jobject /*thiz*/) {
    std::string error;
    {
        std::lock_guard<std::mutex> lock(gLastErrorMutex);
        error.swap(gLastError);
    }
    return env->NewStringUTF(error.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispCpuHasFp16(JNIEnv *env, jobject /*thiz*/) {
    return cpuHasFp16Arithmetic() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispGetVersion(JNIEnv *env, jobject /*thiz*/) {
    std::string text = "unknown";
    try {
        const Krisp::AudioSdk::VersionInfo v = KrispLibrary::singletone()->getVersion();
        text = std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch) + "." +
               std::to_string(v.build);
    } catch (...) {
    }
    return env->NewStringUTF(text.c_str());
}

// NativeAudioBuffer (Kotlin): the handle is the ChunkedBuffer pointer, 0 when allocation failed.

extern "C"
JNIEXPORT jlong JNICALL
Java_com_krisp_krisptestapp_NativeAudioBuffer_nativeAllocate(JNIEnv *env, jclass /*clazz*/, jlong capacity,
                                                             jint chunkSize) {
    if (capacity < 0 || chunkSize <= 0) return 0;
    std::unique_ptr<ChunkedBuffer> buffer =
        ChunkedBuffer::allocate(static_cast<size_t>(capacity), static_cast<size_t>(chunkSize));
    return reinterpret_cast<jlong>(buffer.release());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_NativeAudioBuffer_nativeWrite(JNIEnv *env, jclass /*clazz*/, jlong handle,
                                                          jbyteArray buf, jint off, jint len) {
    auto *buffer = reinterpret_cast<ChunkedBuffer *>(handle);
    if (buffer == nullptr || buf == nullptr) return JNI_FALSE;
    return buffer->write(env, buf, off, len) ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_krisp_krisptestapp_NativeAudioBuffer_nativeSize(JNIEnv *env, jclass /*clazz*/, jlong handle) {
    auto *buffer = reinterpret_cast<ChunkedBuffer *>(handle);
    return buffer == nullptr ? 0 : static_cast<jlong>(buffer->size());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_NativeAudioBuffer_nativeWriteToFd(JNIEnv *env, jclass /*clazz*/, jlong handle,
                                                              jint fd) {
    auto *buffer = reinterpret_cast<ChunkedBuffer *>(handle);
    if (buffer == nullptr || fd < 0) return JNI_FALSE;
    return buffer->writeTo(fd) ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_krisp_krisptestapp_NativeAudioBuffer_nativeFree(JNIEnv *env, jclass /*clazz*/, jlong handle) {
    delete reinterpret_cast<ChunkedBuffer *>(handle);
}