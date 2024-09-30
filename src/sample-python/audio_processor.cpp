#include <string>
#include <memory>
#include <vector>

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


namespace py = pybind11;

class AudioProcessor
{
public:
    AudioProcessor(unsigned sampleRate, unsigned channels, const std::wstring & modelPath) : _sampleRate(sampleRate),
        _channels(channels), _modelPath(modelPath)
    {
        if (krispAudioGlobalInit(nullptr) != 0)
        {
            throw std::runtime_error("Failed to initialization Krisp SDK");
        }
        if (krispAudioSetModel(_modelPath.c_str(), "myModelName") != 0)
        {
            krispAudioGlobalDestroy();
            throw std::runtime_error("Error loading AI model");
        }
        reset_audio_session();
        unsigned samplesPerFrame = (_sampleRate * _frameSize) / 1000;
        unsigned frameLength = samplesPerFrame * _channels;
        _frameBuffer.resize(frameLength);
    }

	~AudioProcessor()
	{
        if (krispAudioRemoveModel("myModelName") != 0)
        {
        }
		if (krispAudioGlobalDestroy() != 0)
		{
  //          throw std::runtime_error("Error calling krispAudioGlobalDestroy");
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
		_sessionId = krispAudioNcCreateSession(inRate, outRate, krispFrameDuration, "myModelName");
        if (_sessionId == nullptr)
        {
            throw std::runtime_error("Error creating session");
        }
	}

    void store_audio_chunk(const py::array_t<float>& audio_chunk)
    {
        py::buffer_info info = audio_chunk.request();
        const float* chunk_ptr = static_cast<float*>(info.ptr);
        size_t chunk_size = static_cast<size_t>(info.size);
        _audio_data.resize(chunk_size + _remainder_sample_count);
        std::memcpy(_audio_data.data() + _remainder_sample_count * sizeof(float),
            static_cast<const void *>(chunk_ptr),
            chunk_size * sizeof(float));
        _remainder_sample_count = 0;
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
        _remainder_sample_count = _audio_data.size() % frameLength;

        unsigned processed_frames = 0;
        auto frame_start_it = _audio_data.begin();
        auto frame_end_it = _audio_data.begin();
        for (unsigned i = 0; i < audio_frame_count; ++i)
        {
            std::advance(frame_end_it, frameLength);
            int result = krispAudioNcCleanAmbientNoiseFloat(
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
    unsigned _channels;
    unsigned long _remainder_sample_count = 0;
    std::vector<float> _audio_data;
    std::vector<float> _frameBuffer;
	std::wstring _modelPath;
    KrispAudioSessionID _sessionId = nullptr;
};


PYBIND11_MODULE(audio_processor, m)
{
    py::class_<AudioProcessor>(m, "AudioProcessor")
        .def(py::init<unsigned, unsigned, std::wstring>())
        .def("store_audio_chunk", &AudioProcessor::store_audio_chunk)
        .def("get_processed_frames", &AudioProcessor::get_processed_frames)
        .def("get_samples_count", &AudioProcessor::get_samples_count);
}
