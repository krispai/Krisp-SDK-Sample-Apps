#include <string>
#include <memory>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>


using Krisp::AudioSdk::NcSessionConfig;
using Krisp::AudioSdk::Nc;
using Krisp::AudioSdk::ModelInfo;
using Krisp::AudioSdk::FrameDuration;
using Krisp::AudioSdk::SamplingRate;
using Krisp::AudioSdk::globalInit;
using Krisp::AudioSdk::globalDestroy;


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


namespace py = pybind11;

class KrispAudioProcessor
{
public:
    KrispAudioProcessor(unsigned sampleRate, const std::wstring & modelPath) : _sampleRate(sampleRate),
        _channels(1), _modelPath(modelPath)
    {
        _ncModelInfo.path = modelPath;
        reset_audio_session();
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;
        _frameBuffer.resize(frameLength);
    }

	~KrispAudioProcessor()
	{
	}

	void reset_audio_session()
	{
        auto samplingRateResult = getKrispSamplingRate(_sampleRate);
        if (!samplingRateResult.second)
        {
            throw std::runtime_error("Unsupported sample rate");
        }
        SamplingRate inRate = samplingRateResult.first;
        const SamplingRate outRate = inRate;
        constexpr FrameDuration frameDuration = FrameDuration::Fd10ms;
        bool withStats = false;
        _ncCfg = {inRate, frameDuration, outRate, &_ncModelInfo, withStats, nullptr};
        _ncSession = Nc<SamplingFormat>::create(_ncCfg);
	}

    void store_audio_chunk(const py::array_t<float>& audio_chunk)
    {
        py::buffer_info info = audio_chunk.request();
        const float* chunk_ptr = static_cast<float*>(info.ptr);
        size_t chunk_size = static_cast<size_t>(info.size);
        _audio_data.resize(chunk_size + _remainderSampleCount);
        std::memcpy(_audio_data.data() + _remainderSampleCount * sizeof(float),
            static_cast<const void *>(chunk_ptr),
            chunk_size * sizeof(float));
        _remainderSampleCount = 0;
    }

    size_t get_samples_count()
    {
        return _audio_data.size();
    }

    unsigned get_processed_frames(py::array_t<float>& python_output_frames)
    {
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;

        py::buffer_info buf_info = python_output_frames.request();
        float* output_ptr = static_cast<float*>(buf_info.ptr);
        size_t buffer_frame_count = static_cast<size_t>(buf_info.size) / frameLength;
        size_t audio_frame_count = _audio_data.size() / frameLength;
        if (buffer_frame_count < audio_frame_count)
        {
            throw std::runtime_error("buffer is too small");
        }
        _remainderSampleCount = _audio_data.size() % frameLength;

        unsigned processed_frames = 0;
        auto frame_start_it = _audio_data.begin();
        auto frame_end_it = _audio_data.begin();
        const float noiseSuppressionLevel = 100.0;
        for (unsigned i = 0; i < audio_frame_count; ++i)
        {
            std::advance(frame_end_it, frameLength);
            _ncSession->process(
                &(*frame_start_it),
                frameLength,
                _frameBuffer.data(),
                frameLength,
                noiseSuppressionLevel,
                nullptr
            );
            std::copy(_frameBuffer.begin(), _frameBuffer.end(), output_ptr + i * frameLength);
            frame_start_it = frame_end_it;
            ++processed_frames;
        }
        if (_remainderSampleCount)
        {
            std::copy(frame_end_it, frame_end_it + static_cast<long>(_remainderSampleCount), _audio_data.begin());
        }
        return processed_frames;
    }

private:
    const unsigned _frameSize = 10;
    unsigned _sampleRate;
    unsigned _channels;
    unsigned long _remainderSampleCount = 0;
    std::vector<float> _audio_data;
    std::vector<float> _frameBuffer;
	std::wstring _modelPath;

    ModelInfo _ncModelInfo;
    NcSessionConfig _ncCfg;
    typedef float SamplingFormat;
    std::shared_ptr<Nc<SamplingFormat>> _ncSession;
};


static void module_constructor() {
    globalInit(L"");
}

static void module_destructor(PyObject *) {
    globalDestroy();
}

PYBIND11_MODULE(krisp_module, m)
{
    module_constructor();
    py::class_<KrispAudioProcessor>(m, "KrispAudioProcessor")
        .def(py::init<unsigned, std::wstring>())
        .def("store_audio_chunk", &KrispAudioProcessor::store_audio_chunk)
        .def("get_processed_frames", &KrispAudioProcessor::get_processed_frames)
        .def("get_samples_count", &KrispAudioProcessor::get_samples_count);

    static int dummy;
    m.add_object("_cleanup", py::capsule(&dummy, [](PyObject *) {
        module_destructor(nullptr);
    }));
}
