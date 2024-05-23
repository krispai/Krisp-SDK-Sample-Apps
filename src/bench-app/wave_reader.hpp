#ifndef KRISP_TEST_UTILS_WAVE_READER_HPP
#define KRISP_TEST_UTILS_WAVE_READER_HPP

// Headers from third party library
#include <sndfile.h>
#include <vector>
#include <string>

#if defined _WIN32 || defined __CYGWIN__
    #define TEST_UTILS_EXPORT   __declspec( dllexport )
#else
    #if __GNUC__ >= 4
        #define TEST_UTILS_EXPORT __attribute__ ((visibility ("default")))
    #else
        #define TEST_UTILS_EXPORT
    #endif
#endif

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
        TEST_UTILS_EXPORT const std::string& getError() const;
        TEST_UTILS_EXPORT bool open(const char* p, SF_INFO* pInfo);
        TEST_UTILS_EXPORT void close();
        TEST_UTILS_EXPORT bool read(std::vector<short>& d);
        TEST_UTILS_EXPORT bool read(std::vector<float>& d);
        TEST_UTILS_EXPORT bool read(const char* p, std::vector<short>& d, int& srate);
        TEST_UTILS_EXPORT bool readFloat(const char* p, std::vector<float>& d, int& srate);

    public:
        TEST_UTILS_EXPORT WaveReader();
        TEST_UTILS_EXPORT ~WaveReader();
};

#endif // KRISP_TEST_UTILS_WAVE_READER_HPP
