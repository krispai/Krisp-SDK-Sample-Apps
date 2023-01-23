#ifndef KRISP_TEST_UTILS_WAVE_WRITER_HPP
#define KRISP_TEST_UTILS_WAVE_WRITER_HPP

// Headers from third party library
#include <sndfile.h>
#include <vector>
#include <string>

namespace KRISP {
    namespace TEST_UTILS {
        class WaveWriter;
    }
}

class KRISP::TEST_UTILS::WaveWriter
{
    private:
        std::string err_;
    public:
        const std::string& getError() const;
        bool write(const char* p, const std::vector<short>& d, const int& srate);
        bool writeFloat(const char* p, const std::vector<float>& d, const int& srate);

    public:
        WaveWriter();
        ~WaveWriter();
};

#endif // KRISP_TEST_UTILS_WAVE_WRITER_HPP
