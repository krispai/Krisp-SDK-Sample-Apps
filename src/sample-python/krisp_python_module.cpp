#include <string>
#include <memory>
#include <vector>
#include <set>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <krisp-audio-sdk-nc.hpp>


static KrispAudioSamplingRate  getKrispSamplingRate(unsigned rate)
{
	switch (rate)
    {
	case 8000:
		return KRISP_AUDIO_SAMPLING_RATE_8000HZ;
	case 16000:
		return KRISP_AUDIO_SAMPLING_RATE_16000HZ;
	case 32000:
		return KRISP_AUDIO_SAMPLING_RATE_32000HZ;
	case 44100:
		return KRISP_AUDIO_SAMPLING_RATE_44100HZ;
	case 48000:
		return KRISP_AUDIO_SAMPLING_RATE_48000HZ;
	case 88200:
		return KRISP_AUDIO_SAMPLING_RATE_88200HZ;
	case 96000:
		return KRISP_AUDIO_SAMPLING_RATE_96000HZ;
	default:
		throw std::runtime_error("Invalid sampling rate");
	}
}


static int krispAudioNcCleanAmbientNoise(KrispAudioSessionID  pSession,
        const short* pFrameIn, unsigned int frameInSize,
        short* pFrameOut, unsigned int frameOutSize) {
    return krispAudioNcCleanAmbientNoiseInt16(pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize);
}

static int krispAudioNcCleanAmbientNoise(KrispAudioSessionID  pSession,
        const float* pFrameIn, unsigned int frameInSize,
        float* pFrameOut, unsigned int frameOutSize) {
    return krispAudioNcCleanAmbientNoiseFloat(pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize);
}

static std::set<std::string> * loadedModelsPtr = nullptr;

static void loadKrispModel(const std::wstring & modelPath, const std::string & modelAlias) {
    if (krispAudioSetModel(modelPath.c_str(), modelAlias.c_str()) != 0)
    {
        throw std::runtime_error("Error loading Krisp model");
    }
    loadedModelsPtr->insert(modelAlias);
}


namespace py = pybind11;

template <typename SampleType>
class KrispAudioProcessorTemplate
{
public:
    KrispAudioProcessorTemplate(unsigned sampleRate, const std::string & modelAlias)
        : _sampleRate(sampleRate), _modelAlias(modelAlias)
    {
        reset_audio_session();
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;
        _frameBuffer.resize(frameLength);
    }

	~KrispAudioProcessorTemplate()
	{
        if (_sessionId)
        {
            if (krispAudioNcCloseSession(_sessionId) != 0) {
                //throw std::runtime_error("Error calling krispAudioNcWithStatsCloseSession");
            }
            _sessionId = nullptr;
        }
	}

	void reset_audio_session()
	{
        if (_sessionId)
        {
            if (krispAudioNcCloseSession(_sessionId) != 0) {
                //throw std::runtime_error("Error calling krispAudioNcWithStatsCloseSession");
            }
            _sessionId = nullptr;
        }
        auto inRate = getKrispSamplingRate(_sampleRate);
        auto outRate = inRate;
        constexpr KrispAudioFrameDuration krispFrameDuration = KRISP_AUDIO_FRAME_DURATION_10MS;
		_sessionId = krispAudioNcCreateSession(inRate, outRate, krispFrameDuration, _modelAlias.c_str());
        if (_sessionId == nullptr)
        {
            throw std::runtime_error("Error creating session");
        }
	}

    void store_audio_chunk(const py::array_t<SampleType>& audio_chunk)
    {
        py::buffer_info info = audio_chunk.request();
        const SampleType* chunk_ptr = static_cast<SampleType*>(info.ptr);
        size_t chunk_size = static_cast<size_t>(info.size);
        _audio_data.resize(chunk_size + _remainder_sample_count);
        std::memcpy(_audio_data.data() + _remainder_sample_count * sizeof(SampleType),
            static_cast<const void *>(chunk_ptr),
            chunk_size * sizeof(SampleType));
        _remainder_sample_count = 0;
    }

    size_t get_samples_count()
    {
        return _audio_data.size();
    }

    unsigned get_processed_frames(py::array_t<SampleType>& python_output_frames)
    {
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;

        py::buffer_info buf_info = python_output_frames.request();
        SampleType* output_ptr = static_cast<SampleType*>(buf_info.ptr);
        size_t buffer_frame_count = static_cast<size_t>(buf_info.size) / frameLength;
        size_t audio_frame_count = _audio_data.size() / frameLength;
        if (buffer_frame_count < audio_frame_count)
        {
            throw std::runtime_error("buffer is too small");
        }
        _remainder_sample_count = _audio_data.size() % frameLength;

        unsigned processed_frames = 0;
        auto frame_start_it = _audio_data.begin();
        auto frame_end_it = _audio_data.begin();
        for (unsigned i = 0; i < audio_frame_count; ++i)
        {
            std::advance(frame_end_it, frameLength);
            int result = krispAudioNcCleanAmbientNoise(
                _sessionId, &(*frame_start_it), frameLength, _frameBuffer.data(), frameLength);

            if (result != 0)
            {
                throw std::runtime_error("error processing audio");
            }

            std::copy(_frameBuffer.begin(), _frameBuffer.end(), output_ptr + i * frameLength);


            frame_start_it = frame_end_it;
            ++processed_frames;
        }
        if (_remainder_sample_count)
        {
            std::copy(frame_end_it, frame_end_it + static_cast<long>(_remainder_sample_count), _audio_data.begin());
        }
        return processed_frames;
    }

private:
    const unsigned _frameSize = 10;
    unsigned _sampleRate;
    static constexpr unsigned _channels = 1;
    unsigned long _remainder_sample_count = 0;
    std::vector<SampleType> _audio_data;
    std::vector<SampleType> _frameBuffer;
	std::string _modelAlias;
    KrispAudioSessionID _sessionId = nullptr;
};

typedef KrispAudioProcessorTemplate<float> KrispAudioProcessorPcmFloat;
typedef KrispAudioProcessorTemplate<int16_t> KrispAudioProcessorPcm16;

static void module_constructor() {

    if (!loadedModelsPtr) {
        loadedModelsPtr = new std::set<std::string>;
    }
    if (krispAudioGlobalInit(nullptr) != 0)
    {
        throw std::runtime_error("Failed to initialization Krisp SDK");
    }
}

static void module_destructor(PyObject *) {
    for (const std::string & loadedModel : *loadedModelsPtr) {
       if (krispAudioRemoveModel(loadedModel.c_str()) != 0) {
            throw std::runtime_error("Error calling krispAudioRemoveModel");
       }
    }
    delete loadedModelsPtr;
    loadedModelsPtr = nullptr;
    if (krispAudioGlobalDestroy() != 0)
    {
      throw std::runtime_error("Error calling krispAudioGlobalDestroy");
    }
}

PYBIND11_MODULE(krisp_module, m)
{
    module_constructor();
    m.def("loadKrispModel", &loadKrispModel);
    py::class_<KrispAudioProcessorPcmFloat>(m, "KrispAudioProcessorPcmFloat")
        .def(py::init<unsigned, std::string>())
        .def("store_audio_chunk", &KrispAudioProcessorPcmFloat::store_audio_chunk)
        .def("get_processed_frames", &KrispAudioProcessorPcmFloat::get_processed_frames)
        .def("get_samples_count", &KrispAudioProcessorPcmFloat::get_samples_count);
    py::class_<KrispAudioProcessorPcm16>(m, "KrispAudioProcessorPcm16")
        .def(py::init<unsigned, std::string>())
        .def("store_audio_chunk", &KrispAudioProcessorPcm16::store_audio_chunk)
        .def("get_processed_frames", &KrispAudioProcessorPcm16::get_processed_frames)
        .def("get_samples_count", &KrispAudioProcessorPcm16::get_samples_count);
    static int dummy;
    m.add_object("_cleanup", py::capsule(&dummy, [](PyObject *) {
        module_destructor(nullptr);
    }));
}
