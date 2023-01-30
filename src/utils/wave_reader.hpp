#ifndef WAVE_READER_HPP
#define WAVE_READER_HPP

#include <sndfile.h>
#include <vector>
#include <string>


class WaveReader {
private:
	std::string err_;

	SNDFILE* sf;
	SF_INFO info;
	int num;
public:
	const std::string& getError() const;
	bool read(const char* p, std::vector<short>& d, int& srate);
	bool readFloat(const char* p, std::vector<float>& d, int& srate);

	WaveReader();
	~WaveReader();
};

#endif
