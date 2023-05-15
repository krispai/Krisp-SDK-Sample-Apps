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
	explicit SoundFileHeader(const SF_INFO & sf_info): m_info{sf_info} {
	}
	unsigned get_number_of_channels() const {
		return m_info.channels;
	}
	int64_t get_number_of_frames() const {
		return m_info.frames;
	}
	unsigned get_sampling_rate() const {
		return m_info.samplerate;
	}
	SoundFileFormat get_format() const;
};


class SoundFile {
private:
	SNDFILE* m_sf_handle;
	SoundFileHeader m_sf_header;

	mutable bool m_has_error;
	mutable std::string m_error_msg;

	void set_error(const std::string & error_msg) const;

	template <class T>
	void read_all_frames_tmpl(std::vector<T> *) const;

public:
	SoundFile();
	~SoundFile();

	bool get_has_error() const;
	std::string get_error_msg() const;
	const SoundFileHeader & get_header() const;
	void load_header(const std::string & file_path);
	void read_all_frames_pcm16(std::vector<int16_t> * frames) const;
	void read_all_frames_float(std::vector<float> * frames) const;
};


std::pair<bool, std::string> WriteSoundFilePCM16(
	const std::string & file,
	const std::vector<int16_t> & frames,
	unsigned sampling_rate);

std::pair<bool, std::string> WriteSoundFileFloat(
	const std::string & file,
	const std::vector<float> & frames,
	unsigned sampling_rate);

#endif
