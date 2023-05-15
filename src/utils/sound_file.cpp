#include "sound_file.hpp"


SoundFileFormat SoundFileHeader::get_format() const {
	switch (m_info.format) {
	case (SF_FORMAT_WAV | SF_FORMAT_PCM_16):
		return SoundFileFormat::PCM16;
	case (SF_FORMAT_WAV | SF_FORMAT_FLOAT):
		return SoundFileFormat::FLOAT;
	default:
		return SoundFileFormat::UNSUPPORTED;
	}
}

SoundFile::SoundFile() :
	m_sf_handle{nullptr},
	m_has_error{false},
	m_error_msg() {
}

SoundFile::~SoundFile() {
	if (m_sf_handle) {
		sf_close(m_sf_handle);
	}
}

void SoundFile::set_error(const std::string & error_msg) const {
	m_has_error = true;
	m_error_msg = error_msg;
}

bool SoundFile::get_has_error() const {
	return m_has_error;
}

std::string SoundFile::get_error_msg() const {
	return m_error_msg;
}

const SoundFileHeader & SoundFile::get_header() const {
	return m_sf_header;
}

void SoundFile::load_header(const std::string & file_path) {
	if (m_sf_handle) {
		if (sf_close(m_sf_handle) != 0) {
			set_error("Failed to close the sound file handle.");
		}
	}
	SF_INFO sf_info{0};
	sf_info.format = 0;

	m_sf_handle = sf_open(file_path.c_str(), SFM_READ, &sf_info);
	if (m_sf_handle == nullptr) {
		set_error("Failed to open the file: " + file_path);
	}
	m_sf_header = SoundFileHeader(sf_info);
}

void SoundFile::read_all_frames_pcm16(std::vector<int16_t> * frames) const {
	if (m_sf_header.get_format() != SoundFileFormat::PCM16) {
		set_error("The file header format is not set to WAV PCM16.");
		return;
	}
	this->read_all_frames_tmpl(frames);
}

void SoundFile::read_all_frames_float(std::vector<float> * frames) const {
	if (m_sf_header.get_format() != SoundFileFormat::FLOAT) {
		set_error("The file header format is not set to WAV FLOAT.");
		return;
	}
	this->read_all_frames_tmpl(frames);
}

static int64_t sf_read(SNDFILE *sf_handle, short *frames, int64_t n_frames) {
	return sf_read_short(sf_handle, frames, n_frames);
}

static int64_t sf_read(SNDFILE *sf_handle, float *frames, int64_t n_frames) {
	return sf_read_float(sf_handle, frames, n_frames);
}

template <class T>
inline void SoundFile::read_all_frames_tmpl(std::vector<T> * frames) const {
	int64_t n_frames = m_sf_header.get_number_of_frames();
	frames->resize(n_frames);

	int64_t n_frames_read = sf_read(m_sf_handle, frames->data(), n_frames);

	if (n_frames != n_frames_read) {
		set_error(
			"The number of read frames does not match the header record.");
	}
}

static int64_t sf_write(SNDFILE *sf_handle, short *frames, int64_t n_frames) {
	return sf_write_short(sf_handle, frames, n_frames);
}

static int64_t sf_write(SNDFILE *sf_handle, float *frames, int64_t n_frames) {
	return sf_write_float(sf_handle, frames, n_frames);
}

template <typename SamplingFormat, int sf_info_format>
std::pair<bool, std::string> WriteFramesTmpl(
	const std::string & file_name, 
	const std::vector<SamplingFormat> & frames,
	unsigned sampling_rate)
{
	if (frames.empty()) {
		return std::pair<bool, std::string>(false, "Frame container is empty.");
	}
	SF_INFO sfinfo;
	sfinfo.frames = static_cast<sf_count_t>(frames.size());
	sfinfo.samplerate = sampling_rate;
	sfinfo.channels = 1;
	sfinfo.format = sf_info_format;
	SNDFILE *sf_handle = sf_open(file_name.c_str(), SFM_WRITE, &sfinfo);
	if (sf_handle == nullptr) {
		return std::pair<bool, std::string>(false,
			"Error open file for writing: " + file_name); 
	}
	sf_write(sf_handle, const_cast<SamplingFormat *>(frames.data()),
		frames.size());
	sf_write_sync(sf_handle);
	sf_close(sf_handle);
	return std::pair<bool, std::string>(true, "");
}

std::pair<bool, std::string> WriteSoundFilePCM16(
	const std::string & file_name,
	const std::vector<int16_t> & frames,
	unsigned sampling_rate)
{
	constexpr int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	return WriteFramesTmpl<int16_t, format>(file_name, frames, sampling_rate);
}

std::pair<bool, std::string> WriteSoundFileFloat(
		const std::string & file_name,
		const std::vector<float> & frames,
		unsigned sampling_rate)
{
	constexpr int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	return WriteFramesTmpl<float, format>(file_name, frames, sampling_rate);
}
