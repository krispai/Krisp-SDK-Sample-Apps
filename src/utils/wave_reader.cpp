#include "wave_reader.hpp"


WaveReader::WaveReader() : sf(0), num(0) {
}

bool WaveReader::read(const char* p, std::vector<short>& d, int& srate) {
	info.format = 0;
	sf = sf_open(p, SFM_READ, &info);
	if (sf == NULL) {
		err_ = "Failed to open the file: " + std::string(p);
		return false;
	}
	int f = (int)info.frames;
	int c = info.channels;
	int sr = info.samplerate;
	if (1 != c) {
		err_ = "Unsupported Channel count!\n Supports Only Mono Wave files";
		return false;
	}
	d.resize(static_cast<size_t>(f));
	num = (int)sf_read_short(sf, d.data(), f);
	srate = sr;
	sf_close(sf);
	return true;
}

bool WaveReader::readFloat(const char* p, std::vector<float>& d, int& srate) {
	info.format = 0;
	sf = sf_open(p, SFM_READ, &info);
	if (sf == NULL) {
		err_ = "Failed to open the file: " + std::string(p);
		return false;
	}
	int f = (int)info.frames;
	int c = info.channels;
	int sr = info.samplerate;
	if (1 != c) {
		err_ = "Unsupported Channel count!\n Supports Only Mono Wave files";
		return false;
	}
	d.resize(static_cast<size_t>(f));
	num = (int)sf_read_float(sf, d.data(), f);
	srate = sr;
	sf_close(sf);
	return true;
}

const std::string& WaveReader::getError() const {
	return err_;
}

WaveReader::~WaveReader() {
}
