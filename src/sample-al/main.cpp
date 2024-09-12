#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-al.hpp>

#include "argument_parser.hpp"
#include "sound_file.hpp"

using namespace Krisp::AudioSdk;

template <typename T>
int error(const T &e)
{
    std::cerr << e << std::endl;
    return 1;
}

static bool parseArguments(std::string &input, std::string &output,
                           std::string &weight, std::string &voiceModel, int argc, char **argv)
{
    ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i", IMPORTANT);
    p.addArgument("--output", "-o", IMPORTANT);
    p.addArgument("--model_path", "-m", IMPORTANT);
    p.addArgument("--voice_cfg", "-v", IMPORTANT);
    if (p.parse())
    {
        input = p.getArgument("-i");
        output = p.getArgument("-o");
        weight = p.getArgument("-m");
        voiceModel = p.getArgument("-v");
    }
    else
    {
        std::cerr << p.getError();
        return false;
    }
    return true;
}

static std::pair<SamplingRate, bool> getKrispSamplingRate(uint32_t rate)
{
    std::pair<SamplingRate, bool> result;
    result.second = true;
    switch (rate)
    {
    case 8000:
        result.first = SamplingRate::Sr8000Hz;
        break;
    case 16000:
        result.first = SamplingRate::Sr16000Hz;
        break;
    case 32000:
        result.first = SamplingRate::Sr32000Hz;
        break;
    case 44100:
        result.first = SamplingRate::Sr44100Hz;
        break;
    case 48000:
        result.first = SamplingRate::Sr48000Hz;
        break;
    case 88200:
        result.first = SamplingRate::Sr88200Hz;
        break;
    case 96000:
        result.first = SamplingRate::Sr96000Hz;
        break;
    }
    return result;
}

static void readAllFrames(const SoundFile &sndFile,
                          std::vector<short> &frames)
{
    sndFile.readAllFramesPCM16(&frames);
}

static void readAllFrames(const SoundFile &sndFile,
                          std::vector<float> &frames)
{
    sndFile.readAllFramesFloat(&frames);
}

static std::pair<bool, std::string> WriteFramesToFile(
    const std::string &fileName,
    const std::vector<int16_t> &frames,
    uint32_t samplingRate)
{
    return writeSoundFilePCM16(fileName, frames, samplingRate);
}

static std::pair<bool, std::string> WriteFramesToFile(
    const std::string &fileName,
    const std::vector<float> &frames,
    uint32_t samplingRate)
{
    return writeSoundFileFloat(fileName, frames, samplingRate);
}

template <typename SamplingFormat>
int alWavFileImpl(
    const SoundFile &inSndFile,
    const std::string &output,
    const std::string &weight,
    const std::string &voiceModel)
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

    try
    {
        globalInit(L"");

        std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;

        ModelInfo alBaseModelInfo;
        ModelInfo alVoiceModelInfo;
        alBaseModelInfo.path = wstringConverter.from_bytes(weight);
        alVoiceModelInfo.path = wstringConverter.from_bytes(voiceModel);

        AlSessionConfig alCfg =
            {
                inRate,
                frameDurationMillis,
                outRate,
                &alBaseModelInfo,
                &alVoiceModelInfo
            };

        std::shared_ptr<Al<SamplingFormat>> alSession = Al<SamplingFormat>::create(alCfg);

        //
        // End of the SDK initialization
        // Start of the Stream's frame by frame processing
        //

        std::vector<SamplingFormat> wavDataOut(wavDataIn.size() * outputFrameSize / inputFrameSize);
        size_t i;

        for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i)
        {
            int result;

            alSession->process(
                &wavDataIn[i * inputFrameSize],
                static_cast<size_t>(inputFrameSize),
                &wavDataOut[i * outputFrameSize],
                static_cast<size_t>(outputFrameSize));
        }

        //
        // End of the Stream's frame by frame processing
        // Finalizing and closing the SDK
        //

        // alSession is a shared_ptr. Need to make sure to free pointer before calling globalDestroy()
        alSession.reset();
        globalDestroy();

        // Write the output to the file
        //wavDataOut.resize(i * outputFrameSize);
        auto pairResult = WriteFramesToFile(output, wavDataOut, samplingRate);
        if (!pairResult.first)
        {
            return error(pairResult.second);
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << "std::exception: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown exception thrown..." << std::endl;
    }

    return 0;
}

static int alWavFile(const std::string &input, const std::string &output,
                     const std::string &weight, const std::string &voiceModel)
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
        return alWavFileImpl<int16_t>(inSndFile, output, weight, voiceModel);
    }
    if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
    {
        return alWavFileImpl<float>(inSndFile, output, weight, voiceModel);
    }
    return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
    std::string in;
    std::string out;
    std::string weight;
    std::string voiceModel;
    
    if (parseArguments(in, out, weight, voiceModel, argc, argv))
    {
        return alWavFile(in, out, weight, voiceModel);
    }
    else
    {
        std::cerr << "\nUsage:\n\t" << argv[0] << " -i input.wav -o output.wav -m model_path" << std::endl;
        if (argc == 1)
        {
            return 0;
        }
        return 1;
    }
}
