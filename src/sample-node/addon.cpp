#include <iostream>
#include <locale>
#include <codecvt>

#define NAPI_DISABLE_CPP_EXCEPTIONS 1
#include <napi.h>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>


using Krisp::AudioSdk::NcSessionConfig;
using Krisp::AudioSdk::Nc;
using Krisp::AudioSdk::ModelInfo;
using Krisp::AudioSdk::FrameDuration;
using Krisp::AudioSdk::SamplingRate;
using Krisp::AudioSdk::globalInit;
using Krisp::AudioSdk::globalDestroy;


template <class SampleType>
class KrispAudioProcessor : public Napi::ObjectWrap<KrispAudioProcessor<SampleType>> {
public:
	~KrispAudioProcessor() override {
	}
    //friend Napi::Object Init(Napi::Env env, Napi::Object exports);
	//typedef KrispAudioProcessor<float> KrispAudioProcessorPcmFloat;
	//typedef KrispAudioProcessor<int16_t> KrispAudioProcessorPcm16;

	static Napi::Object Init(Napi::Env env, Napi::Object exports, const char * nodeClassName) {
		Napi::Function func = Napi::ObjectWrap<KrispAudioProcessor<SampleType>>::DefineClass(env, nodeClassName, {
			Napi::ObjectWrap<KrispAudioProcessor<SampleType>>::InstanceMethod("configure", &KrispAudioProcessor<SampleType>::configure),
			Napi::ObjectWrap<KrispAudioProcessor<SampleType>>::InstanceMethod("processFrames", &KrispAudioProcessor<SampleType>::processFrames),
		});

		Napi::FunctionReference* constructor = new Napi::FunctionReference();
		*constructor = Napi::Persistent(func);
		exports.Set(nodeClassName, func);
		env.SetInstanceData(constructor);

		return exports;
	}

	KrispAudioProcessor(const Napi::CallbackInfo& info) : Napi::ObjectWrap<KrispAudioProcessor>(info) {
		Napi::Env env = info.Env();
		Napi::HandleScope scope(env);
	}

    Napi::Value configure(const Napi::CallbackInfo& info);
    Napi::Value processFrames(const Napi::CallbackInfo& info);

private:
	SamplingRate m_krispSampleRate = static_cast<SamplingRate>(0);
	unsigned int m_frameSize = 0;
	//typedef float SampleType;
    std::shared_ptr<Nc<SampleType>> m_ncSession;
};



static std::pair<SamplingRate, bool> getKrispSamplingRate(uint32_t rate)
{
    std::pair<SamplingRate, bool> result;
    result.second = true;
    switch (rate)
    {
    case 8000:
        result.first = SamplingRate::Sr8000Hz;
        break;
    case 16000:
        result.first = SamplingRate::Sr16000Hz;
        break;
    case 32000:
        result.first = SamplingRate::Sr32000Hz;
        break;
    case 44100:
        result.first = SamplingRate::Sr44100Hz;
        break;
    case 48000:
        result.first = SamplingRate::Sr48000Hz;
        break;
    case 88200:
        result.first = SamplingRate::Sr88200Hz;
        break;
    case 96000:
        result.first = SamplingRate::Sr96000Hz;
        break;
    }
    return result;
}

template <class SampleType>
Napi::Value KrispAudioProcessor<SampleType>::configure(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    if (info.Length() != 2) {
        Napi::TypeError::New(env, "Expected 2 arguments, model path and the sampling rate.").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Expected a string for the first argument.").ThrowAsJavaScriptException();
        return env.Null();
    }
	if (!info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected a number as the sampling rate.").ThrowAsJavaScriptException();
        return env.Null();
	}
	Napi::String napiModelPath = info[0].As<Napi::String>();
	std::string modelPath = napiModelPath.Utf8Value();
	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
    ModelInfo ncModelInfo;
	ncModelInfo.path = wstringConverter.from_bytes(modelPath);
	unsigned int sampleRate = info[1].As<Napi::Number>().Uint32Value();
	auto samplingRateResult = getKrispSamplingRate(sampleRate);
	if (!samplingRateResult.second) {
        Napi::TypeError::New(env, "Unsupported sample rate").ThrowAsJavaScriptException();
        return env.Null();
	}
	m_krispSampleRate = samplingRateResult.first;
	constexpr FrameDuration frameDuration = FrameDuration::Fd10ms;
	m_frameSize = static_cast<unsigned int>(m_krispSampleRate) * static_cast<unsigned int>(frameDuration) / 1000;
	bool withStats = false;
    NcSessionConfig ncCfg = {m_krispSampleRate, frameDuration, m_krispSampleRate, &ncModelInfo, withStats, nullptr};
	try { 
		m_ncSession = Nc<SampleType>::create(ncCfg);
	}
    catch (const std::exception &ex) {
		Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
		return env.Null();
	}
    catch (...) {
		Napi::Error::New(env, "Unknown exception when calling Nc<T>::create(ncCfg)").ThrowAsJavaScriptException();
		return env.Null();
	}
    return env.Undefined();
}

template <class SampleType>
Napi::Value KrispAudioProcessor<SampleType>::processFrames(const Napi::CallbackInfo& info) {
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
	if (inputBuffer.Length() != outputBuffer.Length()) {
		std::string errorMessage = "Input and output buffers must have the same length for PCM16. "
								"Input length: " + std::to_string(inputBuffer.Length()) +
								", Output length: " + std::to_string(outputBuffer.Length()) + ".";
		Napi::Error::New(env, errorMessage).ThrowAsJavaScriptException();
		return env.Null();
	}
	size_t numSamples = inputBuffer.Length() / sizeof(SampleType);
	const SampleType* input = reinterpret_cast<const SampleType*>(inputBuffer.Data());
	SampleType* output = reinterpret_cast<SampleType*>(outputBuffer.Data());
	if (!m_frameSize) {
		Napi::Error::New(env, "frame size is not set").ThrowAsJavaScriptException();
		return env.Null();
	}
	const float noiseSuppressionLevel = 100.0;
	try {
		for (unsigned int i = 0 ; i < numSamples; i += m_frameSize) {
			m_ncSession->process(
				input + i,
				m_frameSize,
				output + i,
				m_frameSize,
				noiseSuppressionLevel,
				nullptr
			);
		}
	}
    catch (const std::exception &ex) {
		Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
		return env.Null();
	} catch (...) {
		Napi::Error::New(env, "Unknown error callign m_ncSesion->process").ThrowAsJavaScriptException();
		return env.Null();
	}
    return env.Undefined();
}

static void CleanupKrisp(void *) {
	try {
		//globalDestroy();
	}
	catch (...) {
		std::cerr << "Error: globalDestroy failed during module clean up." << std::endl;
	}
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    try
    {
        globalInit(L"");
	}
    catch (const std::exception &ex)
    {
		Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
		return Napi::Object::New(env);
    }
    catch (...)
    {
		Napi::Error::New(env, "Uknown exception calling globalInit").ThrowAsJavaScriptException();
		return Napi::Object::New(env);
    }
	KrispAudioProcessor<float>::Init(env, exports, "KrispAudioProcessorPcmFloat");
	KrispAudioProcessor<int16_t>::Init(env, exports, "KrispAudioProcessorPcm16");
	napi_add_env_cleanup_hook(env, CleanupKrisp, nullptr);
	return exports;
}

NODE_API_MODULE(addon, Init)
