#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-vad.hpp>

#include "argument_parser.hpp"
#include "common.hpp"
#include "sound_file.hpp"


using Krisp::AudioSdk::VadSessionConfig;
using Krisp::AudioSdk::Vad;
using Krisp::AudioSdk::ModelInfo;
using Krisp::AudioSdk::FrameDuration;
using Krisp::AudioSdk::SamplingRate;
using Krisp::AudioSdk::globalInit;
using Krisp::AudioSdk::globalDestroy;


static bool parseArguments(std::string &input, std::string &output,
                           std::string &weight, int argc, char **argv)
{
    ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i", IMPORTANT);
    p.addArgument("--output", "-o", IMPORTANT);
    p.addArgument("--vad_model_path", "-m", IMPORTANT);
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

template <typename SamplingFormat>
int vadWavFileTmpl(
    const SoundFile &inSndFile,
    const std::string &vadOutputTextFilePath,
    const std::string &weight)
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
    constexpr FrameDuration frameDurationMillis = FrameDuration::Fd10ms;
    size_t inputFrameSize = (samplingRate * static_cast<size_t>(frameDurationMillis)) / 1000;
    
    std::ostringstream vadResultStream;

    try
    {
        globalInit(L"");

        std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;

        ModelInfo ncModelInfo;
        ncModelInfo.path = wstringConverter.from_bytes(weight);

        VadSessionConfig vadConfig = { inRate, frameDurationMillis, &ncModelInfo };
        std::shared_ptr<Vad<SamplingFormat>> vadSession = Vad<SamplingFormat>::create(vadConfig);

        // End of the SDK initialization
        // Start of the Stream's frame by frame processing

        size_t i;

        vadResultStream.precision(4);
        float vadResult = 0.0;
        for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i)
        {
            vadSession->process(
                &wavDataIn[i * inputFrameSize],
                static_cast<size_t>(inputFrameSize),
                &vadResult);
            vadResultStream << std::fixed << vadResult << std::endl;
        }

        vadSession.reset();
        globalDestroy();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "std::exception: " << ex.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown..." << std::endl;
        return 1;
    }

    std::ofstream vadResultFileStream(vadOutputTextFilePath); 
    if (vadResultFileStream.is_open()) {
        vadResultFileStream << vadResultStream.str();
        vadResultFileStream.close();
    }
    else {
        std::cerr << "Error writing to " << vadOutputTextFilePath << " file.\n";
        return 1;
    }

    return 0;
}

static int vadWavFile(const std::string &input, const std::string &vadOutPath,
                     const std::string &weight)
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
        return vadWavFileTmpl<int16_t>(inSndFile, vadOutPath, weight);
    }
    if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
    {
        return vadWavFileTmpl<float>(inSndFile, vadOutPath, weight);
    }
    return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
    std::string in;
    std::string out;
    std::string weight;

    if (parseArguments(in, out, weight, argc, argv))
    {
        return vadWavFile(in, out, weight);
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
