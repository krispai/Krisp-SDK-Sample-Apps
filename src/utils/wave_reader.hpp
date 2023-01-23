#ifndef KRISP_TEST_UTILS_WAVE_READER_HPP
#define KRISP_TEST_UTILS_WAVE_READER_HPP

// Headers from third party library
#include <sndfile.h>
#include <vector>
#include <string>

namespace KRISP {
    namespace TEST_UTILS {
        class WaveReader;
    }
}

class KRISP::TEST_UTILS::WaveReader
{
    private:
        std::string err_;

        // Members for sndfile
        SNDFILE* sf;
        SF_INFO info;
        int num;
    public:
        const std::string& getError() const;
        bool read(const char* p, std::vector<short>& d, int& srate);
        bool readFloat(const char* p, std::vector<float>& d, int& srate);

    public:
        WaveReader();
        ~WaveReader();
};

#endif // KRISP_TEST_UTILS_WAVE_READER_HPP
