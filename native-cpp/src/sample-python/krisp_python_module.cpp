#include <string>
#include <memory>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>

#include "common.hpp"


using Krisp::AudioSdk::NcSessionConfig;
using Krisp::AudioSdk::Nc;
using Krisp::AudioSdk::ModelInfo;
using Krisp::AudioSdk::FrameDuration;
using Krisp::AudioSdk::SamplingRate;
using Krisp::AudioSdk::globalInit;
using Krisp::AudioSdk::globalDestroy;


namespace py = pybind11;

template <typename SamplingFormat>
class KrispAudioProcessorTemplate
{
public:
    KrispAudioProcessorTemplate(unsigned sampleRate, const std::wstring & modelPath) : _sampleRate(sampleRate),
        _channels(1), _modelPath(modelPath)
    {
        _ncModelInfo.path = modelPath;
        reset_audio_session();
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;
        _frameBuffer.resize(frameLength);
    }

	~KrispAudioProcessorTemplate()
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

    void store_audio_chunk(const py::array_t<SamplingFormat>& audio_chunk)
    {
        py::buffer_info info = audio_chunk.request();
        const SamplingFormat* chunk_ptr = static_cast<SamplingFormat*>(info.ptr);
        size_t chunk_size = static_cast<size_t>(info.size);
        _audio_data.resize(chunk_size + _remainderSampleCount);
        std::memcpy(_audio_data.data() + _remainderSampleCount * sizeof(SamplingFormat),
            static_cast<const void *>(chunk_ptr),
            chunk_size * sizeof(SamplingFormat));
        _remainderSampleCount = 0;
    }

    size_t get_samples_count()
    {
        return _audio_data.size();
    }

    unsigned get_processed_frames(py::array_t<SamplingFormat>& python_output_frames, unsigned nc_aggr)
    {
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;

        py::buffer_info buf_info = python_output_frames.request();
        SamplingFormat* output_ptr = static_cast<SamplingFormat*>(buf_info.ptr);
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
        const float noiseSuppressionLevel = static_cast<float>(nc_aggr);
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
    std::vector<SamplingFormat> _audio_data;
    std::vector<SamplingFormat> _frameBuffer;
	std::wstring _modelPath;

    ModelInfo _ncModelInfo;
    NcSessionConfig _ncCfg;
    std::shared_ptr<Nc<SamplingFormat>> _ncSession;
};


static void module_constructor() {
    globalInit(L"");
}

static void module_destructor(PyObject *) {
    globalDestroy();
}

typedef KrispAudioProcessorTemplate<float> KrispAudioProcessorPcmFloat;
typedef KrispAudioProcessorTemplate<int16_t> KrispAudioProcessorPcm16;

PYBIND11_MODULE(krisp_module, m)
{
    module_constructor();
    py::class_<KrispAudioProcessorPcmFloat>(m, "KrispAudioProcessorPcmFloat")
        .def(py::init<unsigned, std::wstring>())
        .def("store_audio_chunk", &KrispAudioProcessorPcmFloat::store_audio_chunk)
        .def("get_processed_frames", &KrispAudioProcessorPcmFloat::get_processed_frames)
        .def("get_samples_count", &KrispAudioProcessorPcmFloat::get_samples_count);
    py::class_<KrispAudioProcessorPcm16>(m, "KrispAudioProcessorPcm16")
        .def(py::init<unsigned, std::wstring>())
        .def("store_audio_chunk", &KrispAudioProcessorPcm16::store_audio_chunk)
        .def("get_processed_frames", &KrispAudioProcessorPcm16::get_processed_frames)
        .def("get_samples_count", &KrispAudioProcessorPcm16::get_samples_count);
    static int dummy;
    m.add_object("_cleanup", py::capsule(&dummy, [](PyObject *) {
        module_destructor(nullptr);
    }));
}
