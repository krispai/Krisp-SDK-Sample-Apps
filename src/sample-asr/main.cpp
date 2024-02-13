#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk-asr.hpp>

#include "argument_parser.hpp"
#include "sound_file.hpp"

static int krispAudioAsrProcess(
	KrispAudioSessionID pSession,
	const std::vector<short> &frame)
{
	return krispAudioAsrProcessFrameInt16(pSession, frame);
}

static int krispAudioAsrProcess(
	KrispAudioSessionID pSession,
	const std::vector<float> &frame)
{
	return krispAudioAsrProcessFrameFloat(pSession, frame);
}

template <typename T>
int error(const T &e)
{
	std::cerr << e << std::endl;
	return 1;
}

bool parseArguments(std::string &input, std::string &weight, std::string customVocab, int argc, char **argv)
{
	ArgumentParser p(argc, argv);
	p.addArgument("--input", "-i", IMPORTANT);
	p.addArgument("--weight_file", "-w", IMPORTANT);
	p.addArgument("--custom_vocab", "-cv", IMPORTANT);
	if (p.parse())
	{
		input = p.getArgument("-i");
		weight = p.getArgument("-w");
		customVocab = p.getArgument("-cv");
	}
	else
	{
		std::cerr << p.getError();
		return false;
	}
	return true;
}

std::pair<KrispAudioSamplingRate, bool> getKrispSamplingRate(unsigned rate)
{
	std::pair<KrispAudioSamplingRate, bool> result;
	result.second = true;
	switch (rate)
	{
	case 8000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_8000HZ;
		break;
	case 16000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_16000HZ;
		break;
	case 32000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_32000HZ;
		break;
	case 44100:
		result.first = KRISP_AUDIO_SAMPLING_RATE_44100HZ;
		break;
	case 48000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_48000HZ;
		break;
	case 88200:
		result.first = KRISP_AUDIO_SAMPLING_RATE_88200HZ;
		break;
	case 96000:
		result.first = KRISP_AUDIO_SAMPLING_RATE_96000HZ;
		break;
	default:
		result.first = KRISP_AUDIO_SAMPLING_RATE_16000HZ;
		result.second = false;
	}
	return result;
}

void readAllFrames(const SoundFile &sndFile,
				   std::vector<short> &frames)
{
	sndFile.readAllFramesPCM16(&frames);
}

void readAllFrames(const SoundFile &sndFile,
				   std::vector<float> &frames)
{
	sndFile.readAllFramesFloat(&frames);
}

std::vector<std::string> readCustomVocabulary(const std::string& customVocabularyPath)
{
    if (customVocabularyPath.empty())
    {
        return std::vector<std::string>{};
    }

    std::ifstream customVocabularyFile(customVocabularyPath);
    if (!customVocabularyFile.is_open())
    {
        throw std::logic_error("Failed to open the custom vocabulary file");
    }

    std::vector<std::string> customVocabulary;
    std::string line;
    while (std::getline(customVocabularyFile, line))
    {
        customVocabulary.push_back(line);
    }

    customVocabularyFile.close();

    return customVocabulary;
}

template <typename SamplingFormat>
int asrWavFileTmpl(const SoundFile &inSndFile, const std::string &weight, std::string customVocab)
{
	std::vector<SamplingFormat> wavDataIn;

	readAllFrames(inSndFile, wavDataIn);

	if (inSndFile.getHasError())
	{
		return error(inSndFile.getErrorMsg());
	}

	unsigned samplingRate = inSndFile.getHeader().getSamplingRate();
	auto samplingRateResult = getKrispSamplingRate(samplingRate);
	if (!samplingRateResult.second)
	{
		return error("Unsupported sample rate");
	}

	KrispAudioSamplingRate inRate = samplingRateResult.first;
	constexpr size_t frameDurationMillis = 10;
	size_t inputFrameSize = (samplingRate * frameDurationMillis) / 1000;

	if (krispAudioGlobalInit(nullptr) != 0)
	{
		return error("Failed to initialization Krisp SDK");
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
	std::wstring modelPath = wstringConverter.from_bytes(weight);
	std::string modelAlias = "model";
	if (krispAudioSetModel(modelPath.c_str(), modelAlias.c_str()) != 0)
	{
		return error("Error loading AI model");
	}

	KrispAudioSessionID session = nullptr;
	KrispAudioFrameDuration krispFrameDuration = KRISP_AUDIO_FRAME_DURATION_10MS;

	KrispAudioAsrSessionConfig asrCfg;
	asrCfg.enablePc = true;
    asrCfg.enableItn = true;
    asrCfg.enableRepetitionRemoval = true;
    asrCfg.enablePiiFiltering = true;
    asrCfg.enableDiarization = true;
    asrCfg.customVocabulary = readCustomVocabulary(customVocab);

	session = krispAudioAsrCreateSession(inRate, outRate, asrCfg, modelAlias.c_str());
	if (nullptr == session)
	{
		return error("Error creating session");
	}

	KrispAudioNcPerFrameInfo perFrameInfo;

	size_t i;
	for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i)
	{
		int result;
		std::vector<SamplingFormat> frame;
		frame.insert(frame.cend(), wavDataIn[i * inputFrameSize], wavDataIn[i * inputFrameSize + inputFrameSize]);

		result = krispAudioAsrProcess(session, frame);
		if (0 != result)
		{
			std::cerr << "Error while processing ASR" << i << " frame" << std::endl;
			break;
		}
	}

	KrispAudioAsrResult asrResult;
	if (0 != krispAudioAsrGenerateResult(session, asrResult))
	{
		return error("Error calling krispAudioAsrGenerateResult");
	}

	// TODO[GH]: Write ASR result to the file

	if (0 != krispAudioAsrCloseSession(session))
	{
		return error("Error calling krispAudioAsrCloseSession");
	}

	if (0 != krispAudioGlobalDestroy())
	{
		return error("Error calling krispAudioGlobalDestroy");
	}

	return 0;
}

int asrWavFile(const std::string &input, const std::string &weight, std::string customVocab)
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
		return asrWavFileTmpl<short>(inSndFile, weight, customVocab);
	}

	if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
	{
		return asrWavFileTmpl<float>(inSndFile, weight, customVocab);
	}

	return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
	std::string inputAudio;
	std::string weight;
	std::string customVocab;

	if (parseArguments(in, weight, argc, argv))
	{
		return asrWavFile(in, weight, customVocab);
	}
	else
	{
		std::cerr << "\nUsage:\n\t" << argv[0]
				  << " -i input.wav -w weightFile" << std::endl;
		if (argc == 1)
		{
			return 0;
		}
		return 1;
	}
}
