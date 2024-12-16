#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-ar.hpp>

#include "argument_parser.hpp"
#include "common.hpp"

using namespace Krisp::AudioSdk;

namespace
{
    // After enroll store the voice info for the specified user to use it later for AR processing
    VoiceInfo voiceInfo;
}

static bool parseArguments(std::string &input, std::string &output,
                           std::string &weight, int argc, char **argv)
{
    ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i", IMPORTANT);
    p.addArgument("--output", "-o", IMPORTANT);
    p.addArgument("--model_path", "-m", IMPORTANT);
    if (p.parse())
    {
        input = p.getArgument("-i");
        output = p.getArgument("-o");
        weight = p.getArgument("-m");
    }
    else
    {
        std::cerr << p.getError();
        return false;
    }
    return true;
}

void arEnrollResultCb(const VoiceInfo& vInfo, uint16_t enrollProgress)
{
    std::cout << enrollProgress << std::endl;

    if (100 == enrollProgress)
    {
        if (vInfo.embedding.empty())
        {
            throw std::logic_error("Voice embedding is empty");
        }

        voiceInfo.embedding = vInfo.embedding;
    }
}

template <typename SamplingFormat>
int arEnrollWavFileImpl(
    const SoundFile &inSndFile,
    const std::string &weight,
    VoiceInfo &voiceInfo)
{
    std::vector<SamplingFormat> wavDataIn;
    readAllFrames(inSndFile, wavDataIn);

    if (inSndFile.getHasError())
    {
        return error(inSndFile.getErrorMsg());
    }

    uint32_t samplingRate = inSndFile.getHeader().getSamplingRate();
    auto samplingRateResult = getKrispSamplingRate(samplingRate);
    if (!samplingRateResult.second)
    {
        return error("Unsupported sample rate");
    }

    SamplingRate inRate = samplingRateResult.first;
    const SamplingRate outRate = inRate;
    constexpr FrameDuration frameDurationMillis = FrameDuration::Fd10ms;
    size_t inputFrameSize = (samplingRate * static_cast<size_t>(frameDurationMillis)) / 1000;
    size_t outputFrameSize = inputFrameSize;

    std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;

    ModelInfo arModelInfo;
    arModelInfo.path = wstringConverter.from_bytes(weight);

    ArEnrollSessionConfig arEnrollCfg =
    {
        inRate,
        frameDurationMillis,
        outRate,
        &arModelInfo,
    };

    std::shared_ptr<ArEnroll<SamplingFormat>> arEnrollSession = ArEnroll<SamplingFormat>::create(arEnrollCfg, arEnrollResultCb);
    if (!arEnrollSession)
    {
        return error("Failed to create AR enrollment session");
    }

    //
    // Start of the Stream's frame by frame processing
    //

    size_t i;
    for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i)
    {
        arEnrollSession->process(&wavDataIn[i * inputFrameSize], static_cast<size_t>(inputFrameSize));
    }

    //
    // End of the Stream's frame by frame processing
    //

    // // arSession is a shared_ptr. Need to make sure to free pointer before calling globalDestroy()
    // arEnrollSession.reset();

    return 0;    
}

template <typename SamplingFormat>
int arProcessWavFileImpl(
    const SoundFile &inSndFile,
    const std::string &output,
    const std::string &weight,
    const VoiceInfo &voiceInfo)
{
    std::vector<SamplingFormat> wavDataIn;
    readAllFrames(inSndFile, wavDataIn);

    if (inSndFile.getHasError())
    {
        return error(inSndFile.getErrorMsg());
    }

    uint32_t samplingRate = inSndFile.getHeader().getSamplingRate();
    auto samplingRateResult = getKrispSamplingRate(samplingRate);
    if (!samplingRateResult.second)
    {
        return error("Unsupported sample rate");
    }

    SamplingRate inRate = samplingRateResult.first;
    const SamplingRate outRate = inRate;
    constexpr FrameDuration frameDurationMillis = FrameDuration::Fd10ms;
    size_t inputFrameSize = (samplingRate * static_cast<size_t>(frameDurationMillis)) / 1000;
    size_t outputFrameSize = inputFrameSize;

    std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;

    ModelInfo arModelInfo;
    arModelInfo.path = wstringConverter.from_bytes(weight);

    ArSessionConfig arCfg =
    {
        inRate,
        frameDurationMillis,
        outRate,
        &arModelInfo,
        voiceInfo
    };

    std::shared_ptr<Ar<SamplingFormat>> arSession = Ar<SamplingFormat>::create(arCfg);
    if (!arSession)
    {
        return error("Failed to create AR session");
    }

    //
    // Start of the Stream's frame by frame processing
    //

    std::vector<SamplingFormat> wavDataOut(wavDataIn.size() * outputFrameSize / inputFrameSize);
    size_t i;
    for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i)
    {
        arSession->process(
            &wavDataIn[i * inputFrameSize],
            static_cast<size_t>(inputFrameSize),
            &wavDataOut[i * outputFrameSize],
            static_cast<size_t>(outputFrameSize));
    }

    //
    // Finalizing and closing the SDK
    //

    // // arSession is a shared_ptr. Need to make sure to free pointer before calling globalDestroy()
    // arSession.reset();

    // Write the output to the file
    // wavDataOut.resize(i * outputFrameSize);
    auto pairResult = WriteFramesToFile(output, wavDataOut, samplingRate);
    if (!pairResult.first)
    {
        return error(pairResult.second);
    }

    return 0;
}

static int arEnrollWavFile(const std::string &input, const std::string &weight)
{
    SoundFile inSndFile;
    inSndFile.loadHeader(input);
    if (inSndFile.getHasError())
    {
        return error(inSndFile.getErrorMsg());
    }

    auto sndFileHeader = inSndFile.getHeader();
    if (sndFileHeader.getFormat() == SoundFileFormat::PCM16)
    {
        return arEnrollWavFileImpl<int16_t>(inSndFile, weight, voiceInfo);
    }

    if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
    {
        return arEnrollWavFileImpl<float>(inSndFile, weight, voiceInfo);
    }

    return error("The sound file format should be PCM16 or FLOAT.");
}

static int arProcessWavFile(const std::string &input, const std::string &output,
                            const std::string &weight, const VoiceInfo &voiceInfo)
{
    SoundFile inSndFile;
    inSndFile.loadHeader(input);
    if (inSndFile.getHasError())
    {
        return error(inSndFile.getErrorMsg());
    }

    auto sndFileHeader = inSndFile.getHeader();
    if (sndFileHeader.getFormat() == SoundFileFormat::PCM16)
    {
        return arProcessWavFileImpl<int16_t>(inSndFile, output, weight, voiceInfo);
    }

    if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
    {
        return arProcessWavFileImpl<float>(inSndFile, output, weight, voiceInfo);
    }

    return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
    std::cout << "Krisp Audio SDK AR Sample" << std::endl;

    int ret = 0;
    std::string in;
    std::string out;
    std::string weight;

    if (!parseArguments(in, out, weight, argc, argv))
    {
        std::cerr << "\nUsage:\n\t" << argv[0] << " -i input.wav -o output.wav -m model_path" << std::endl;
        return 1;
    }

    try
    {
        globalInit(L"");

        std::cout << "Running AR Enroll" << std::endl;

        // Enroll to get voice info of the speaker
        ret = arEnrollWavFile(in, weight);
        if (ret != 0)
        {
            std::cerr << "Enrollment failed" << std::endl;
            return ret;
        }
        std::cout << "Enrollment successful!!!" << std::endl;

        std::cout << "Running AR Process" << std::endl;

        // Process the audio file using voice info retrieved during enrollment
        ret = arProcessWavFile(in, out, weight, voiceInfo);
        if (ret != 0)
        {
            std::cerr << "AR processing failed" << std::endl;
            return ret;
        }
        std::cout << "AR processing successful!!!" << std::endl;

        globalDestroy();
    }
    catch (const std::exception &ex)
    {
        std::cout << "std::exception: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown exception thrown..." << std::endl;
    }
    
    return ret;
}
