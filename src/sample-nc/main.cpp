#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>

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
                           std::string &weight, float &noiseSuppressionLevel, bool &stats, int argc, char **argv)
{
    ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i", IMPORTANT);
    p.addArgument("--output", "-o", IMPORTANT);
    p.addArgument("--model_path", "-m", IMPORTANT);
    p.addArgument("--suppress_level", "-sl", OPTIONAL);
    p.addArgument("--stats", "-s", OPTIONAL);
    if (p.parse())
    {
        input = p.getArgument("-i");
        output = p.getArgument("-o");
        weight = p.getArgument("-m");
        stats = p.getOptionalArgument("-s");

        const auto noiseSuppressionLevelStr = p.tryGetArgument("-sl", "100.0");
        noiseSuppressionLevel = std::stof(noiseSuppressionLevelStr);
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
        result.first = SamplingRate::Sr8000Hz;
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
 static void getNcStats(std::shared_ptr<Nc<SamplingFormat>> ncSession)
{
     SessionStats ncSessionStats;

     ncSession->getSessionStats(&ncSessionStats);

     std::cout << "#--- Noise/Voice stats ---" << std::endl;
     std::cout << "# - No     Noise: " <<
         ncSessionStats.noiseStats.noNoiseMs << " ms" << std::endl;
     std::cout << "# - Low    Noise: " <<
         ncSessionStats.noiseStats.lowNoiseMs << " ms" << std::endl;
     std::cout << "# - Medium Noise: " <<
         ncSessionStats.noiseStats.mediumNoiseMs << " ms" << std::endl;
     std::cout << "# - High   Noise: " <<
         ncSessionStats.noiseStats.highNoiseMs << " ms" << std::endl;
     std::cout << "#-------------------------" << std::endl;
     std::cout << "# - Talk time :   " <<
         ncSessionStats.voiceStats.talkTimeMs << " ms" << std::endl;
     std::cout << "#-------------------------" << std::endl;
 }

template <typename SamplingFormat>
int ncWavFileTmpl(
    const SoundFile &inSndFile,
    const std::string &output,
    const std::string &weight,
    float noiseSuppressionLevel,
    bool withStats)
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

        ModelInfo ncModelInfo;
        ncModelInfo.path = wstringConverter.from_bytes(weight);

        NcSessionConfig ncCfg =
            {
                inRate,
                frameDurationMillis,
                outRate,
                &ncModelInfo,
                withStats,
                nullptr // Ringtone model cfg for inbound
            };

        std::shared_ptr<Nc<SamplingFormat>> ncSession = Nc<SamplingFormat>::create(ncCfg);

        //
        // End of the SDK initialization
        // Start of the Stream's frame by frame processing
        //

        PerFrameStats perFrameStats;

        std::vector<SamplingFormat> wavDataOut(wavDataIn.size() * outputFrameSize / inputFrameSize);
        size_t i;

        for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i)
        {
            int result;

            ncSession->process(
                &wavDataIn[i * inputFrameSize],
                static_cast<size_t>(inputFrameSize),
                &wavDataOut[i * outputFrameSize],
                static_cast<size_t>(outputFrameSize),
                noiseSuppressionLevel,
                withStats ? &perFrameStats : nullptr);

            if (withStats)
            {
                std::cout << "[" << i + 1 << " x " << static_cast<uint64_t>(frameDurationMillis) << "ms]"
                          << " noiseEn: " << perFrameStats.energy.noiseEnergy
                          << ", voiceEn: " << perFrameStats.energy.voiceEnergy << std::endl;

                if (withStats && i % 100 == 0)
                {
                    // Get NC session stats in the middle of the processing
                    // calculated from the start of the session processing
                    getNcStats(ncSession);
                }
            }
        }

        //
        // End of the Stream's frame by frame processing
        // Finalizing and closing the SDK
        //

        if (withStats)
        {
            std::cout << "Getting Final NC session stats..." << std::endl;
            getNcStats(ncSession);
        }

        // ncSession is a shared_ptr. Need to make sure to free pointer before calling globalDestroy()
        ncSession.reset();
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

static int ncWavFile(const std::string &input, const std::string &output,
                     const std::string &weight, float noiseSuppressionLevel, bool withStats)
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
        return ncWavFileTmpl<int16_t>(inSndFile, output, weight, noiseSuppressionLevel, withStats);
    }
    if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
    {
        return ncWavFileTmpl<float>(inSndFile, output, weight, noiseSuppressionLevel, withStats);
    }
    return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
    std::string in;
    std::string out;
    std::string weight;
    float noiseSuppressionLevel = 100;
    bool stats = false;

    if (parseArguments(in, out, weight, noiseSuppressionLevel, stats, argc, argv))
    {
        return ncWavFile(in, out, weight, noiseSuppressionLevel, stats);
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
