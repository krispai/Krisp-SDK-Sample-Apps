#include <iostream>
#include <locale>
#include <codecvt>

#define NAPI_DISABLE_CPP_EXCEPTIONS 1
#include <napi.h>

#include <krisp-audio-sdk-nc.hpp>


class KrispAudioProcessor : public Napi::ObjectWrap<KrispAudioProcessor> {
public:
	~KrispAudioProcessor() {
		//if (m_session) {
		//	if (krispAudioNcCloseSession(m_session) != 0) {
		//		std::cerr << "krispAudioNcCloseSession failed in the destructor." << std::endl;
		//	}
		//	m_session = nullptr;
		//}
		//if (krispAudioRemoveModel("model") != 0) {
		//	std::cerr << "krispAudioRemoveModel failed in the destructor." << std::endl;
		//}
	}
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
		Napi::Function func = DefineClass(env, "KrispAudioProcessor", {
			InstanceMethod("loadModel", &KrispAudioProcessor::loadModel),
			InstanceMethod("setSampleRate", &KrispAudioProcessor::setSampleRate),
			InstanceMethod("processFramesFloat32", &KrispAudioProcessor::processFramesFloat32),
			InstanceMethod("processFramesPcm16", &KrispAudioProcessor::processFramesPcm16)
		});

		Napi::FunctionReference* constructor = new Napi::FunctionReference();
		*constructor = Napi::Persistent(func);
		exports.Set("KrispAudioProcessor", func);
		env.SetInstanceData(constructor);

		return exports;
	}

	KrispAudioProcessor(const Napi::CallbackInfo& info) : Napi::ObjectWrap<KrispAudioProcessor>(info) {
		Napi::Env env = info.Env();
		Napi::HandleScope scope(env);
	}

private:
    Napi::Value setSampleRate(const Napi::CallbackInfo& info);
    Napi::Value loadModel(const Napi::CallbackInfo& info);
    Napi::Value processFramesFloat32(const Napi::CallbackInfo& info);
    Napi::Value processFramesPcm16(const Napi::CallbackInfo& info);
	template <typename SampleType>
    Napi::Value processFrames(const Napi::CallbackInfo& info);

	KrispAudioSessionID m_session = nullptr;
	bool m_modelLoaded = false;
	KrispAudioSamplingRate m_krispSampleRate = static_cast<KrispAudioSamplingRate>(0);
	unsigned int m_frameSize = 0;
};

static std::pair<KrispAudioSamplingRate, bool> getKrispSamplingRate(unsigned rate) {
	std::pair<KrispAudioSamplingRate, bool> result;
	result.second = true;
	switch (rate) {
	case 8000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_8000HZ;
		break;
	case 16000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_16000HZ;
		break;
	case 32000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_32000HZ;
		break;
	case 44100:
		result.first = KRISP_AUDIO_SAMPLING_RATE_44100HZ;
		break;
	case 48000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_48000HZ;
		break;
	case 88200:
		result.first = KRISP_AUDIO_SAMPLING_RATE_88200HZ;
		break;
	case 96000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_96000HZ;
		break;
	default:
		result.first = KRISP_AUDIO_SAMPLING_RATE_16000HZ;
		result.second = false;
	}
	return result;
}

Napi::Value KrispAudioProcessor::setSampleRate(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    if (info.Length() != 1) {
        Napi::TypeError::New(env, "Expected a single argument for numerical sampleRate.").ThrowAsJavaScriptException();
        return env.Null();
    }

	if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected number as sampleRate").ThrowAsJavaScriptException();
        return env.Null();
	}

	unsigned int sampleRate = info[0].As<Napi::Number>().Uint32Value();
	auto samplingRateResult = getKrispSamplingRate(sampleRate);
	if (!samplingRateResult.second) {
        Napi::TypeError::New(env, "Unsupported sample rate").ThrowAsJavaScriptException();
        return env.Null();
	}
	m_krispSampleRate = samplingRateResult.first;
	if (m_session) {
		if (krispAudioNcCloseSession(m_session) != 0) {
			Napi::TypeError::New(env, "krispAudioNcCloseSession failed.").ThrowAsJavaScriptException();
			return env.Null();
		}
	}
	const KrispAudioFrameDuration krispFrameDuration = KRISP_AUDIO_FRAME_DURATION_10MS;
	m_frameSize = m_krispSampleRate * krispFrameDuration / 1000;
	if (!m_modelLoaded) {
		Napi::TypeError::New(env, "Krisp model is not loaded.").ThrowAsJavaScriptException();
		return env.Null();
	}
	m_session = krispAudioNcCreateSession(m_krispSampleRate, m_krispSampleRate, krispFrameDuration,
		"model");
	if (m_session == nullptr) {
		Napi::Error::New(env, "krispAudioNcCreateSession failed").ThrowAsJavaScriptException();
		return env.Null();
	}
    return env.Undefined();
}

Napi::Value KrispAudioProcessor::loadModel(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    if (info.Length() != 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected a single string argument for modelPath").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string modelPath = info[0].As<Napi::String>().Utf8Value();

	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
	std::wstring modelWidePath = wstringConverter.from_bytes(modelPath);

	if (krispAudioSetModel(modelWidePath.c_str(), "model") != 0) {
        Napi::Error::New(env, std::string("Failed to load model at path ") + modelPath).ThrowAsJavaScriptException();
        return env.Null();
	}

	m_modelLoaded = true;

	if (!m_session && m_krispSampleRate) {
		const KrispAudioFrameDuration krispFrameDuration = KRISP_AUDIO_FRAME_DURATION_10MS;
		m_session = krispAudioNcCreateSession(m_krispSampleRate, m_krispSampleRate, krispFrameDuration,
			"model");
		if (m_session == nullptr) {
			Napi::Error::New(env, "krispAudioNcCreateSession failed").ThrowAsJavaScriptException();
			return env.Null();
		}
	}

    return env.Undefined();
}

static int krispAudioNcCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const int16_t * pFrameIn,
		unsigned int frameInSize,
		int16_t * pFrameOut,
		unsigned int frameOutSize)
{
	return krispAudioNcCleanAmbientNoiseInt16(
		pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize);
}

static int krispAudioNcCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const float * pFrameIn,
		unsigned int frameInSize,
		float * pFrameOut,
		unsigned int frameOutSize)
{
	return krispAudioNcCleanAmbientNoiseFloat(
		pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize);
}

template <typename SampleType>
Napi::Value KrispAudioProcessor::processFrames(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);

    if (info.Length() != 2 ||
        !info[0].IsBuffer() ||
        !info[1].IsBuffer()) {
        Napi::TypeError::New(env, "Expected 2 arguments: Buffer input, Buffer output").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Buffer<char> inputBuffer = info[0].As<Napi::Buffer<char>>();
    Napi::Buffer<char> outputBuffer = info[1].As<Napi::Buffer<char>>();

	// Ensure buffer sizes match
	if (inputBuffer.Length() != outputBuffer.Length()) {
		Napi::Error::New(env, "Input and output buffers must have the same length for PCM16").ThrowAsJavaScriptException();
		return env.Null();
	}

	size_t numSamples = inputBuffer.Length() / sizeof(SampleType);
	const SampleType* input = reinterpret_cast<const SampleType*>(inputBuffer.Data());
	SampleType* output = reinterpret_cast<SampleType*>(outputBuffer.Data());

	if (!m_frameSize) {
		Napi::Error::New(env, "frame size is not set").ThrowAsJavaScriptException();
		return env.Null();
	}

	for (unsigned int i = 0 ; i < numSamples; i += m_frameSize) {
		if (krispAudioNcCleanAmbientNoise(m_session, input + i, m_frameSize, output + i, m_frameSize) != 0) {
			Napi::Error::New(env, "krispAudioNcCleanAmbientNoise failed").ThrowAsJavaScriptException();
			return env.Null();
		}
	}

    return env.Undefined();
}

Napi::Value KrispAudioProcessor::processFramesFloat32(const Napi::CallbackInfo& info) {
	return processFrames<float>(info);
}

Napi::Value KrispAudioProcessor::processFramesPcm16(const Napi::CallbackInfo& info) {
	return processFrames<int16_t>(info);
}

static void CleanupKrisp(void *) {
	if (krispAudioGlobalDestroy() != 0) {
		std::cerr << "Error: krispAudioGlobalDestroy failed during module clean up." << std::endl;
	}
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	KrispAudioProcessor::Init(env, exports);
	if (krispAudioGlobalInit(nullptr) != 0) {
		Napi::Error::New(env, "krispAudioGlobalInit failed.").ThrowAsJavaScriptException();
		return Napi::Object::New(env);
	}
	napi_add_env_cleanup_hook(env, CleanupKrisp, nullptr);
	return exports;
}

NODE_API_MODULE(addon, Init)
