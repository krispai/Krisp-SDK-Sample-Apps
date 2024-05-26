#include <memory>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>


namespace py = pybind11;

class AudioProcessor
{
public:
    AudioProcessor(unsigned sample_rate, unsigned channels) : _sample_rate(sample_rate), _channels(channels)
    {
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
        unsigned samples_per_frame = (_sample_rate * _frame_size) / 1000;
        unsigned frame_length = samples_per_frame * _channels;

        py::buffer_info buf_info = python_output_frames.request();
        float* output_ptr = static_cast<float*>(buf_info.ptr);
        size_t buffer_frame_count = static_cast<size_t>(buf_info.size) / frame_length;
        size_t audio_frame_count = _audio_data.size() / frame_length;
        if (buffer_frame_count < audio_frame_count)
        {
            throw std::runtime_error("buffer is too small");
        }
        _remainder_sample_count = _audio_data.size() % frame_length;

        unsigned processed_frames = 0;
        auto frame_start_it = _audio_data.begin();
        auto frame_end_it = _audio_data.begin();
        for (unsigned i = 0; i < audio_frame_count; ++i)
        {
            std::advance(frame_end_it, frame_length);
            std::copy(frame_start_it, frame_end_it, output_ptr + i * frame_length);
            frame_start_it = frame_end_it;
            ++processed_frames;
        }
        if (_remainder_sample_count)
        {
            std::copy(frame_end_it, frame_end_it + _remainder_sample_count, _audio_data.begin());
        }
        return processed_frames;
    }

private:
    const unsigned _frame_size = 10;
    unsigned _sample_rate;
    unsigned _channels;
    unsigned _remainder_sample_count = 0;
    std::vector<float> _audio_data;
};


PYBIND11_MODULE(audio_processor, m)
{
    py::class_<AudioProcessor>(m, "AudioProcessor")
        .def(py::init<unsigned, unsigned>())
        .def("store_audio_chunk", &AudioProcessor::store_audio_chunk)
        .def("get_processed_frames", &AudioProcessor::get_processed_frames)
        .def("get_samples_count", &AudioProcessor::get_samples_count);
}
