#include "sound_file.hpp"


SoundFileFormat SoundFileHeader::getFormat() const {
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
	m_sfHandle{nullptr},
	m_hasError{false},
	m_errorMsg() {
}

SoundFile::~SoundFile() {
	if (m_sfHandle) {
		sf_close(m_sfHandle);
	}
}

void SoundFile::setError(const std::string & errorMsg) const {
	m_hasError = true;
	m_errorMsg = errorMsg;
}

bool SoundFile::getHasError() const {
	return m_hasError;
}

std::string SoundFile::getErrorMsg() const {
	return m_errorMsg;
}

const SoundFileHeader & SoundFile::getHeader() const {
	return m_sfHeader;
}

void SoundFile::loadHeader(const std::string & filePath) {
	if (m_sfHandle) {
		if (sf_close(m_sfHandle) != 0) {
			setError("Failed to close the sound file handle.");
		}
	}
	SF_INFO sfInfo{0};
	sfInfo.format = 0;

	m_sfHandle = sf_open(filePath.c_str(), SFM_READ, &sfInfo);
	if (m_sfHandle == nullptr) {
		setError("Failed to open the file: " + filePath);
	}
	m_sfHeader = SoundFileHeader(sfInfo);
}

void SoundFile::readAllFramesPCM16(std::vector<int16_t> * frames) const {
	if (m_sfHeader.getFormat() != SoundFileFormat::PCM16) {
		setError("The file header format is not set to WAV PCM16.");
		return;
	}
	this->readAllFramesTmpl(frames);
}

void SoundFile::readAllFramesFloat(std::vector<float> * frames) const {
	if (m_sfHeader.getFormat() != SoundFileFormat::FLOAT) {
		setError("The file header format is not set to WAV FLOAT.");
		return;
	}
	this->readAllFramesTmpl(frames);
}

static int64_t sf_read(SNDFILE *sfHandle, short *frames, int64_t nFrames) {
	return sf_read_short(sfHandle, frames, nFrames);
}

static int64_t sf_read(SNDFILE *sfHandle, float *frames, int64_t nFrames) {
	return sf_read_float(sfHandle, frames, nFrames);
}

template <class T>
inline void SoundFile::readAllFramesTmpl(std::vector<T> * frames) const {
	int64_t nFrames = m_sfHeader.getNumberOfFrames();
	frames->resize(nFrames);

	int64_t nFramesRead = sf_read(m_sfHandle, frames->data(), nFrames);

	if (nFrames != nFramesRead) {
		setError("The number of read frames does not match the header record.");
	}
}

static int64_t sf_write(SNDFILE *sfHandle, short *frames, int64_t nFrames) {
	return sf_write_short(sfHandle, frames, nFrames);
}

static int64_t sf_write(SNDFILE *sfHandle, float *frames, int64_t nFrames) {
	return sf_write_float(sfHandle, frames, nFrames);
}

template <typename SamplingFormat, int sfInfoFormat>
std::pair<bool, std::string> writeFramesTmpl(
	const std::string & fileName, 
	const std::vector<SamplingFormat> & frames,
	unsigned samplingRate)
{
	if (frames.empty()) {
		return std::pair<bool, std::string>(false, "Frame container is empty.");
	}
	SF_INFO sfinfo;
	sfinfo.frames = static_cast<sf_count_t>(frames.size());
	sfinfo.samplerate = samplingRate;
	sfinfo.channels = 1;
	sfinfo.format = sfInfoFormat;
	SNDFILE *sfHandle = sf_open(fileName.c_str(), SFM_WRITE, &sfinfo);
	if (sfHandle == nullptr) {
		return std::pair<bool, std::string>(false,
			"Error open file for writing: " + fileName); 
	}
	sf_write(sfHandle, const_cast<SamplingFormat *>(frames.data()),
		frames.size());
	sf_write_sync(sfHandle);
	sf_close(sfHandle);
	return std::pair<bool, std::string>(true, "");
}

std::pair<bool, std::string> writeSoundFilePCM16(
	const std::string & fileName,
	const std::vector<int16_t> & frames,
	unsigned samplingRate)
{
	constexpr int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	return writeFramesTmpl<int16_t, format>(fileName, frames, samplingRate);
}

std::pair<bool, std::string> writeSoundFileFloat(
		const std::string & fileName,
		const std::vector<float> & frames,
		unsigned samplingRate)
{
	constexpr int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	return writeFramesTmpl<float, format>(fileName, frames, samplingRate);
}
