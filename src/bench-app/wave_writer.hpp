#ifndef KRISP_TEST_UTILS_WAVE_WRITER_HPP
#define KRISP_TEST_UTILS_WAVE_WRITER_HPP

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
        class WaveWriter;
    }
}

class KRISP::TEST_UTILS::WaveWriter
{
    private:
        std::string err_;
    public:
        TEST_UTILS_EXPORT const std::string& getError() const;
        TEST_UTILS_EXPORT bool write(const char* p, const std::vector<short>& d, const int& srate);
        TEST_UTILS_EXPORT bool write(const char* p, const std::vector<float>& d, const int& srate);

    public:
        TEST_UTILS_EXPORT WaveWriter();
        TEST_UTILS_EXPORT ~WaveWriter();
};

#endif // KRISP_TEST_UTILS_WAVE_WRITER_HPP
