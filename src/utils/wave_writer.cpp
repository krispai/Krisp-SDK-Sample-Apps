#include "wave_writer.hpp"

#include <sndfile.h>


WaveWriter::WaveWriter() {
}

bool WaveWriter::write(const char* p, const std::vector<short>& d, const int& srate) {
	if (d.empty()) {
		err_ = "Empty raw data";
		return false;
	}
	SNDFILE* sndfile;
	SF_INFO sfinfo;
	sfinfo.frames = static_cast<sf_count_t>(d.size());
	sfinfo.samplerate = srate;
	sfinfo.channels = 1;
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	sndfile = sf_open(p, SFM_WRITE, &sfinfo);
	if (sndfile == nullptr) {
		err_ = "Error open file for writing: " + std::string(p);
		return false;
	}
	sf_write_short(sndfile, d.data(), static_cast<sf_count_t>(d.size()));
	sf_write_sync(sndfile);
	sf_close(sndfile);
	return true;
}

bool WaveWriter::writeFloat(const char* p, const std::vector<float>& d, const int& srate) {
	if (d.empty()) {
		err_ = "Empty raw data";
	}
	SNDFILE* sndfile;
	SF_INFO sfinfo;
	sfinfo.frames = static_cast<sf_count_t>(d.size());
	sfinfo.samplerate = srate;
	sfinfo.channels = 1;
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	sndfile = sf_open(p, SFM_WRITE, &sfinfo);
	if (sndfile == nullptr) {
		err_ = "Error open file for writing: " + std::string(p);
		return false;
	}
	sf_write_float(sndfile, d.data(), static_cast<sf_count_t>(d.size()));
	sf_write_sync(sndfile);
	sf_close(sndfile);
	return true;
}

const std::string& WaveWriter::getError() const {
	return err_;
}

WaveWriter::~WaveWriter() {
}
