#ifndef WAVE_WRITER_HPP
#define WAVE_WRITER_HPP

#include <vector>
#include <string>


class WaveWriter {
private:
	std::string err_;
public:
	const std::string& getError() const;
	bool write(const char* p, const std::vector<short>& d, const int& srate);
	bool writeFloat(const char* p, const std::vector<float>& d, const int& srate);

	WaveWriter();
	~WaveWriter();
};

#endif
