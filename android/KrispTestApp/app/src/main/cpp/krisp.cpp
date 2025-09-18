#include <jni.h>
#include <cstring>
#include <string>
#include <stdexcept>

#include <krisp-audio-api-definitions.hpp>
#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>


class KrispLibrary final {
public:
    typedef std::shared_ptr<Krisp::AudioSdk::Nc<float>> SessionTypePcmFloat;
    typedef std::shared_ptr<Krisp::AudioSdk::Nc<int16_t>> SessionTypePcm16;

    static KrispLibrary * singletone() {
        static KrispLibrary * _ptr = nullptr;
        if (!_ptr) {
            Krisp::AudioSdk::globalInit(L"", nullptr, Krisp::AudioSdk::LogLevel::Off);
            _ptr = new KrispLibrary;
        }
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

    void startSessionPcm16(unsigned int samplingRate) {
        endSession();
        _activePcm16SessionPtr = createSession<int16_t>(samplingRate);
    }

    void startSessionPcmFloat(unsigned int samplingRate) {
        endSession();
        _activePcmFloatSessionPtr = createSession<float>(samplingRate);
    }

    void processFramePcm16(const int16_t * pcm16Samples, int16_t * processPcm16Samples, size_t sampleCount) {
        _activePcm16SessionPtr->process(
                pcm16Samples,
                sampleCount,
                processPcm16Samples,
                sampleCount,
                100.0,
                nullptr);
    }

    void processFramePcmFloat(const float * floatSamples, float * processFloatSamples, size_t sampleCount) {
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
    std::shared_ptr<Krisp::AudioSdk::Nc<SampleType>> createSession(unsigned int samplingRate) {
        Krisp::AudioSdk::SamplingRate krispSamplingRate = getKrispSamplingRate(samplingRate);
        Krisp::AudioSdk::NcSessionConfig ncConfig;
        ncConfig.modelInfo = &_modelInfo;
        ncConfig.inputFrameDuration = Krisp::AudioSdk::FrameDuration::Fd30ms;
        ncConfig.inputSampleRate = krispSamplingRate;
        ncConfig.outputSampleRate = krispSamplingRate;
        ncConfig.enableSessionStats = false;
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
        } catch (...) {
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
        } catch (...) {
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
                                                                           jint sampling_rate) {
    {
        try {
            KrispLibrary::singletone()->startSessionPcmFloat(static_cast<unsigned int>(sampling_rate));
            return JNI_TRUE;
        } catch (...) {
            return JNI_FALSE;
        }
    }
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_krisp_krisptestapp_WavProcessActivity_krispStartNcSessionPcm16(JNIEnv *env, jobject thiz,
                                                                        jint sampling_rate) {
    {
        try {
            KrispLibrary::singletone()->startSessionPcm16(static_cast<unsigned int>(sampling_rate));
            return JNI_TRUE;
        } catch (...) {
            return JNI_FALSE;
        }
    }
}