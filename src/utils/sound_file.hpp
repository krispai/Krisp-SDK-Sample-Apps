#ifndef SOUND_FILE_HPP
#define SOUND_FILE_HPP

#include <sndfile.h>

#include <string>
#include <utility>
#include <vector>


enum SoundFileFormat {
	UNSUPPORTED = 0,
	PCM16 = 1,
	FLOAT = 2
};


class SoundFileHeader {
private:
	SF_INFO m_info;
public:
	SoundFileHeader(): m_info{0} {
	}
	explicit SoundFileHeader(const SF_INFO & sfInfo): m_info{sfInfo} {
	}
	unsigned getNumberOfChannels() const {
		return m_info.channels;
	}
	int64_t getNumberOfFrames() const {
		return m_info.frames;
	}
	unsigned getSamplingRate() const {
		return m_info.samplerate;
	}
	SoundFileFormat getFormat() const;
};


class SoundFile {
private:
	SNDFILE* m_sfHandle;
	SoundFileHeader m_sfHeader;

	mutable bool m_hasError;
	mutable std::string m_errorMsg;

	void setError(const std::string & errorMsg) const;

	template <class T>
	void readAllFramesTmpl(std::vector<T> *) const;

public:
	SoundFile();
	~SoundFile();

	bool getHasError() const;
	std::string getErrorMsg() const;
	const SoundFileHeader & getHeader() const;
	void loadHeader(const std::string & filePath);
	void readAllFramesPCM16(std::vector<int16_t> * frames) const;
	void readAllFramesFloat(std::vector<float> * frames) const;
};


std::pair<bool, std::string> writeSoundFilePCM16(
	const std::string & file,
	const std::vector<int16_t> & frames,
	unsigned samplingRate);

std::pair<bool, std::string> writeSoundFileFloat(
	const std::string & file,
	const std::vector<float> & frames,
	unsigned samplingRate);

#endif
