#include <android/log.h>
#include <jni.h>
#include <sched.h>
#include <sys/auxv.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
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

    // Appends len bytes from native memory.
    bool write(const uint8_t *src, size_t len) {
        if (len > _capacity - _size) {
            return false;
        }
        size_t copied = 0;
        while (copied < len) {
            const size_t chunkIndex = _size / _chunkSize;
            const size_t chunkOffset = _size % _chunkSize;
            const size_t n = std::min(len - copied, _chunkSize - chunkOffset);
            std::memcpy(_chunks[chunkIndex].get() + chunkOffset, src + copied, n);
            copied += n;
            _size += n;
        }
        return true;
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

    // Loads the model of every page. Sessions of the current model end first (a model can't change under a running
    // session). A model the SDK rejects throws, and the current model stays loaded.
    void loadModel(const jbyte *modelPtr, size_t modelSize) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_liveCaptureSessions > 0) {
            throw std::runtime_error("An audio capture is running: stop it before loading a model");
        }
        endSessionLocked();

        auto newModelData = std::make_unique<uint8_t[]>(modelSize);
        std::memcpy(newModelData.get(), modelPtr, modelSize);

        auto oldModelData = std::move(_modelData);
        const Krisp::AudioSdk::ModelInfo oldModelInfo = _modelInfo;
        auto oldCachedSession = std::move(_cachedSessionPtr);

        _modelData = std::move(newModelData);
        _modelInfo.blob.first = _modelData.get();
        _modelInfo.blob.second = modelSize;

        Krisp::AudioSdk::NcSessionConfig ncConfig;
        ncConfig.modelInfo = &_modelInfo;
        ncConfig.inputFrameDuration = Krisp::AudioSdk::FrameDuration::Fd10ms;
        ncConfig.inputSampleRate = Krisp::AudioSdk::SamplingRate::Sr32000Hz;
        ncConfig.outputSampleRate = Krisp::AudioSdk::SamplingRate::Sr32000Hz;
        ncConfig.enableSessionStats = false;

        try {
            // Validates the model (and keeps it warm for the sessions created from it later).
            _cachedSessionPtr = Krisp::AudioSdk::Nc<float>::create(ncConfig);
        } catch (...) {
            _modelData = std::move(oldModelData);
            _modelInfo = oldModelInfo;
            _cachedSessionPtr = std::move(oldCachedSession);
            throw;
        }
    }


    Krisp::AudioSdk::VersionInfo getVersion() const {
        Krisp::AudioSdk::VersionInfo info {0};
        Krisp::AudioSdk::getVersion(info);
        return info;
    }

    ~KrispLibrary() {
        Krisp::AudioSdk::globalDestroy();
    }

    // File processing sessions: 30 ms frames. The frame scratch buffer is sized here, so processing never allocates.
    void startSessionPcm16(unsigned int samplingRate, bool lowPrecision) {
        std::lock_guard<std::mutex> lock(_mutex);
        endSessionLocked();
        _activePcm16SessionPtr =
            createSession<int16_t>(samplingRate, lowPrecision, Krisp::AudioSdk::FrameDuration::Fd30ms);
        _fileFrame.assign(samplingRate * 30 / 1000 * sizeof(int16_t), 0);
    }

    void startSessionPcmFloat(unsigned int samplingRate, bool lowPrecision) {
        std::lock_guard<std::mutex> lock(_mutex);
        endSessionLocked();
        _activePcmFloatSessionPtr =
            createSession<float>(samplingRate, lowPrecision, Krisp::AudioSdk::FrameDuration::Fd30ms);
        _fileFrame.assign(samplingRate * 30 / 1000 * sizeof(float), 0);
    }

    // Processes one file frame (the first frameBytes of the Java array) in place. Returns the time the NC call took,
    // in nanoseconds: only the SDK's process(), not the copies around it.
    template<class SampleType>
    int64_t processFileFrame(JNIEnv *env, jbyteArray buf, jint frameBytes) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto &session = fileSession<SampleType>();
        if (!session) {
            throw std::runtime_error("No active NC session for this sample format");
        }
        if (frameBytes <= 0 || static_cast<size_t>(frameBytes) > _fileFrame.size() ||
            frameBytes % static_cast<jint>(sizeof(SampleType)) != 0) {
            throw std::runtime_error("Frame size doesn't match the NC session");
        }
        // Copied rather than pinned (GetPrimitiveArrayCritical): this lock must not be waited on in a critical region.
        env->GetByteArrayRegion(buf, 0, frameBytes, reinterpret_cast<jbyte *>(_fileFrame.data()));
        auto *samples = reinterpret_cast<SampleType *>(_fileFrame.data());
        const size_t sampleCount = static_cast<size_t>(frameBytes) / sizeof(SampleType);
        const auto start = std::chrono::steady_clock::now();
        session->process(samples, sampleCount, samples, sampleCount, 100.0, nullptr);
        const int64_t nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
        env->SetByteArrayRegion(buf, 0, frameBytes, reinterpret_cast<const jbyte *>(_fileFrame.data()));
        return nanos;
    }

    // Audio capture: every run gets a session of its own (10 ms float frames, like the iOS app), owned by the run (see
    // CaptureRun), so a run that is still finishing can't touch the next run's. No model can be loaded while one lives,
    // which is why the capture hot path needs no lock.
    SessionTypePcmFloat createCaptureSession(unsigned int samplingRate, bool lowPrecision) {
        std::lock_guard<std::mutex> lock(_mutex);
        SessionTypePcmFloat session =
            createSession<float>(samplingRate, lowPrecision, Krisp::AudioSdk::FrameDuration::Fd10ms);
        ++_liveCaptureSessions;
        return session;
    }

    void releaseCaptureSession(SessionTypePcmFloat &session) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (session) {
            session = nullptr;
            --_liveCaptureSessions;
        }
    }

private:
    template<class SampleType>
    std::shared_ptr<Krisp::AudioSdk::Nc<SampleType>> &fileSession();

    void endSessionLocked() {
        _activePcmFloatSessionPtr = nullptr;
        _activePcm16SessionPtr = nullptr;
    }

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
    std::shared_ptr<Krisp::AudioSdk::Nc<SampleType>> createSession(unsigned int samplingRate, bool lowPrecision,
                                                                   Krisp::AudioSdk::FrameDuration frameDuration) {
        if (!_modelData) {
            throw std::runtime_error("No model is loaded");
        }
        Krisp::AudioSdk::SamplingRate krispSamplingRate = getKrispSamplingRate(samplingRate);
        Krisp::AudioSdk::NcSessionConfig ncConfig;
        ncConfig.modelInfo = &_modelInfo;
        ncConfig.inputFrameDuration = frameDuration;
        ncConfig.inputSampleRate = krispSamplingRate;
        ncConfig.outputSampleRate = krispSamplingRate;
        ncConfig.enableSessionStats = false;
        // fp16 for models that support it, on CPUs with FP16 arithmetic; the SDK runs fp32 otherwise.
        ncConfig.enableLowPrecisionExecution = lowPrecision;
        return Krisp::AudioSdk::Nc<SampleType>::create(ncConfig);
    }

    // Guards the model and the sessions: pages may run sessions on background threads while another loads a model.
    std::mutex _mutex;
    std::unique_ptr<uint8_t[]> _modelData;
    Krisp::AudioSdk::ModelInfo _modelInfo;
    SessionTypePcmFloat _cachedSessionPtr;
    SessionTypePcmFloat _activePcmFloatSessionPtr;
    SessionTypePcm16 _activePcm16SessionPtr;
    std::vector<uint8_t> _fileFrame; // the file session's frame, sized when the session starts
    int _liveCaptureSessions = 0;

    KrispLibrary() {
    }
};

template<>
KrispLibrary::SessionTypePcmFloat &KrispLibrary::fileSession<float>() {
    return _activePcmFloatSessionPtr;
}

template<>
KrispLibrary::SessionTypePcm16 &KrispLibrary::fileSession<int16_t>() {
    return _activePcm16SessionPtr;
}


// One audio capture run (AudioCapturer): its own NC session, its buffers (allocated and touched up front, so the
// capture never allocates while it times frames), and its timing. Owned by the run through a JNI handle.
class CaptureRun final {
public:
    CaptureRun(unsigned int samplingRate, bool lowPrecision, size_t blockCount, size_t blockFrames)
        : _samplesPerFrame(samplingRate * 10 / 1000),
          _session(KrispLibrary::singletone()->createCaptureSession(samplingRate, lowPrecision)),
          _frame(_samplesPerFrame, 0.0f) {
        try {
            _blocks.resize(blockCount);
            for (auto &block : _blocks) {
                block.assign(blockFrames * _samplesPerFrame, 0.0f); // assign() touches every page now
            }
        } catch (...) {
            // The destructor won't run for a constructor that throws: give the session back here.
            KrispLibrary::singletone()->releaseCaptureSession(_session);
            throw;
        }
    }

    ~CaptureRun() {
        KrispLibrary::singletone()->releaseCaptureSession(_session);
    }

    CaptureRun(const CaptureRun &) = delete;
    CaptureRun &operator=(const CaptureRun &) = delete;

    size_t samplesPerFrame() const {
        return _samplesPerFrame;
    }

    // Realtime: NC on one frame of the Java array (copied in), the result appended to output. Returns the NC time.
    int64_t processFrame(JNIEnv *env, jfloatArray samples, jint offset, ChunkedBuffer *output) {
        env->GetFloatArrayRegion(samples, offset, static_cast<jsize>(_samplesPerFrame), _frame.data());
        const int64_t nanos = processInPlace(_frame.data());
        appendToOutput(_frame.data(), output);
        return nanos;
    }

    // Distributed: stores one captured frame of the Java array as frame frameIndex of block.
    bool storeFrame(JNIEnv *env, size_t block, size_t frameIndex, jfloatArray samples, jint offset) {
        if (block >= _blocks.size() || (frameIndex + 1) * _samplesPerFrame > _blocks[block].size()) {
            return false;
        }
        env->GetFloatArrayRegion(samples, offset, static_cast<jsize>(_samplesPerFrame),
                                 _blocks[block].data() + frameIndex * _samplesPerFrame);
        return true;
    }

    // Distributed: NC on frames [first, first + count) of block, frame by frame, appended to output. Adds the NC time
    // to totalNanos and raises maxNanos to the slowest frame.
    void processBlock(size_t block, size_t first, size_t count, ChunkedBuffer *output, int64_t &totalNanos,
                      int64_t &maxNanos) {
        if (block >= _blocks.size() || (first + count) * _samplesPerFrame > _blocks[block].size()) {
            throw std::runtime_error("Frames outside the capture block");
        }
        for (size_t i = first; i < first + count; ++i) {
            float *frame = _blocks[block].data() + i * _samplesPerFrame;
            const int64_t nanos = processInPlace(frame);
            totalNanos += nanos;
            maxNanos = std::max(maxNanos, nanos);
            appendToOutput(frame, output);
        }
    }

private:
    int64_t processInPlace(float *frame) {
        const auto start = std::chrono::steady_clock::now();
        _session->process(frame, _samplesPerFrame, frame, _samplesPerFrame, 100.0, nullptr);
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }

    void appendToOutput(const float *frame, ChunkedBuffer *output) {
        if (output != nullptr &&
            !output->write(reinterpret_cast<const uint8_t *>(frame), _samplesPerFrame * sizeof(float))) {
            throw std::runtime_error("The capture output buffer is full");
        }
    }

    size_t _samplesPerFrame;
    KrispLibrary::SessionTypePcmFloat _session;
    std::vector<float> _frame;
    std::vector<std::vector<float>> _blocks;
};

extern "C"
JNIEXPORT jint JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispLoadModel(
        JNIEnv* env, jclass /*clazz*/, jbyteArray modelBytes, jint modelSize) {
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
// File processing frames: NC on the first frameBytes of buf, in place. Returns the NC time in nanoseconds (the SDK's
// process() only), or -1 on failure (logged, and kept as the last error).
template<class SampleType>
static jlong processFileFrameJni(JNIEnv *env, jbyteArray buf, jint frameBytes, const char *what) {
    if (buf == nullptr || frameBytes <= 0 || frameBytes > env->GetArrayLength(buf)) return -1;
    try {
        return static_cast<jlong>(KrispLibrary::singletone()->processFileFrame<SampleType>(env, buf, frameBytes));
    } catch (const std::exception &err) {
        logAppError(what, err.what());
    } catch (...) {
        logAppError(what, "unknown error");
    }
    return -1;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispNcFramePcmFloat(JNIEnv *env, jclass /*clazz*/, jbyteArray buf, jint valid) {
    return processFileFrameJni<float>(env, buf, valid, "Processing a float frame failed");
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispNcFramePcm16(JNIEnv *env, jclass /*clazz*/, jbyteArray buf, jint valid) {
    return processFileFrameJni<int16_t>(env, buf, valid, "Processing a PCM16 frame failed");
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispStartNcSessionPcmFloat(JNIEnv *env,
                                                                           jclass /*clazz*/,
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
Java_com_krisp_krisptestapp_KrispSdk_krispStartNcSessionPcm16(JNIEnv *env, jclass /*clazz*/,
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
Java_com_krisp_krisptestapp_KrispSdk_krispInit(JNIEnv *env, jclass /*clazz*/, jint log_level) {
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
Java_com_krisp_krisptestapp_KrispSdk_krispTakeLastError(JNIEnv *env, jclass /*clazz*/) {
    std::string error;
    {
        std::lock_guard<std::mutex> lock(gLastErrorMutex);
        error.swap(gLastError);
    }
    return env->NewStringUTF(error.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCpuHasFp16(JNIEnv *env, jclass /*clazz*/) {
    return cpuHasFp16Arithmetic() ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispGetVersion(JNIEnv *env, jclass /*clazz*/) {
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
// Audio capture (AudioCapturer). Each run owns a CaptureRun through a handle: its NC session and its buffers.

// Creates a run: an NC session for 10 ms float frames at sampling_rate, and (Distributed mode) block_count blocks of
// block_frames frames each, all allocated and touched now. Returns the handle, 0 on failure (logged).
extern "C"
JNIEXPORT jlong JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCaptureStart(JNIEnv *env, jclass /*clazz*/, jint sampling_rate,
                                                       jboolean low_precision, jint block_count, jint block_frames) {
    if (sampling_rate <= 0 || block_count < 0 || block_frames < 0) return 0;
    try {
        return reinterpret_cast<jlong>(new CaptureRun(static_cast<unsigned int>(sampling_rate), low_precision == JNI_TRUE,
                                                      static_cast<size_t>(block_count),
                                                      static_cast<size_t>(block_frames)));
    } catch (const std::bad_alloc &) {
        logAppError("Creating the capture run failed", "not enough memory for its buffers");
    } catch (const std::exception &err) {
        logAppError("Creating the capture NC session failed", err.what());
    } catch (...) {
        logAppError("Creating the capture NC session failed", "unknown error");
    }
    return 0;
}

// Realtime: NC on the 10 ms frame at samples[offset], appended to the output buffer. Returns the NC time in
// nanoseconds, -1 on failure.
extern "C"
JNIEXPORT jlong JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCaptureProcessFrame(JNIEnv *env, jclass /*clazz*/, jlong run_handle,
                                                              jfloatArray samples, jint offset, jlong output_handle) {
    auto *run = reinterpret_cast<CaptureRun *>(run_handle);
    if (run == nullptr || samples == nullptr || offset < 0 ||
        static_cast<jlong>(offset) + static_cast<jlong>(run->samplesPerFrame()) > env->GetArrayLength(samples)) {
        return -1;
    }
    try {
        return static_cast<jlong>(run->processFrame(env, samples, offset, reinterpret_cast<ChunkedBuffer *>(output_handle)));
    } catch (const std::exception &err) {
        logAppError("Processing a capture frame failed", err.what());
    } catch (...) {
        logAppError("Processing a capture frame failed", "unknown error");
    }
    return -1;
}

// Distributed: stores the 10 ms frame at samples[offset] as frame frame_index of block.
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCaptureStoreFrame(JNIEnv *env, jclass /*clazz*/, jlong run_handle,
                                                            jint block, jint frame_index, jfloatArray samples,
                                                            jint offset) {
    auto *run = reinterpret_cast<CaptureRun *>(run_handle);
    if (run == nullptr || samples == nullptr || block < 0 || frame_index < 0 || offset < 0 ||
        static_cast<jlong>(offset) + static_cast<jlong>(run->samplesPerFrame()) > env->GetArrayLength(samples)) {
        return JNI_FALSE;
    }
    return run->storeFrame(env, static_cast<size_t>(block), static_cast<size_t>(frame_index), samples, offset)
               ? JNI_TRUE
               : JNI_FALSE;
}

// Distributed: NC on frames [first, first + count) of block, appended to the output buffer. times receives the NC
// time of those frames: [0] total, [1] the slowest frame, in nanoseconds.
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCaptureProcessBlock(JNIEnv *env, jclass /*clazz*/, jlong run_handle,
                                                              jint block, jint first, jint count, jlong output_handle,
                                                              jlongArray times) {
    auto *run = reinterpret_cast<CaptureRun *>(run_handle);
    if (run == nullptr || times == nullptr || env->GetArrayLength(times) < 2 || block < 0 || first < 0 || count < 0) {
        return JNI_FALSE;
    }
    int64_t total = 0;
    int64_t slowest = 0;
    try {
        run->processBlock(static_cast<size_t>(block), static_cast<size_t>(first), static_cast<size_t>(count),
                          reinterpret_cast<ChunkedBuffer *>(output_handle), total, slowest);
    } catch (const std::exception &err) {
        logAppError("Processing a capture block failed", err.what());
        return JNI_FALSE;
    } catch (...) {
        logAppError("Processing a capture block failed", "unknown error");
        return JNI_FALSE;
    }
    const jlong result[2] = {static_cast<jlong>(total), static_cast<jlong>(slowest)};
    env->SetLongArrayRegion(times, 0, 2, result);
    return JNI_TRUE;
}

// Ends a run: its NC session and buffers are freed.
extern "C"
JNIEXPORT void JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCaptureStop(JNIEnv *env, jclass /*clazz*/, jlong run_handle) {
    try {
        delete reinterpret_cast<CaptureRun *>(run_handle);
    } catch (...) {
        logAppError("Ending the capture run failed", "unknown error");
    }
}

// Diagnostics for the capture page: the CPU core the calling thread runs on, and a core's current clock.

extern "C"
JNIEXPORT jint JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCurrentCpu(JNIEnv *env, jclass /*clazz*/) {
    return sched_getcpu();
}

// kHz, -1 when the system doesn't let the app read it.
extern "C"
JNIEXPORT jint JNICALL
Java_com_krisp_krisptestapp_KrispSdk_krispCpuFreqKhz(JNIEnv *env, jclass /*clazz*/, jint cpu) {
    if (cpu < 0) return -1;
    char path[96];
    std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
    FILE *file = std::fopen(path, "r");
    if (file == nullptr) return -1;
    int khz = -1;
    if (std::fscanf(file, "%d", &khz) != 1) khz = -1;
    std::fclose(file);
    return khz;
}
