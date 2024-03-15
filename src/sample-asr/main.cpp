#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <typeinfo>

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

inline std::string convertWstrToStr(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    std::string str = myconv.to_bytes(wstr);
    return str;
}

inline std::wstring convertStrToWstr(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    std::wstring wstr = myconv.from_bytes(str);
    return wstr;
}

template <typename T>
int error(const T &e)
{
	std::cerr << e << std::endl;
	return 1;
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

bool parseArguments(int argc, char **argv, std::string &input, std::string &weight, KrispAudioAsrSessionConfig& asrCfg)
{
	ArgumentParser p(argc, argv);
	p.addArgument("--input", "-i", IMPORTANT);
	p.addArgument("--weight_file", "-w", IMPORTANT);
	p.addArgument("--do_pc", "-pc", OPTIONAL);
    p.addArgument("--do_itn", "-itn", OPTIONAL);
    p.addArgument("--do_rr", "-rr", OPTIONAL);
    p.addArgument("--do_pii", "-pii", OPTIONAL);
    p.addArgument("--use_vad", "-vad", OPTIONAL);
    p.addArgument("--diarize", "-diar", OPTIONAL);
	p.addArgument("--custom_vocab", "-cv", OPTIONAL);
	if (p.parse())
	{
		input = p.getArgument("-i");
		weight = p.getArgument("-w");

		auto doPc = p.tryGetArgument("-pc", "0");
		auto doItn = p.tryGetArgument("-itn", "0");
		auto doRr = p.tryGetArgument("-rr", "0");
		auto doPii = p.tryGetArgument("-pii", "0");
		auto vad = p.tryGetArgument("-vad", "0");
		auto diarize = p.tryGetArgument("-diar", "0");
		auto customVocab = p.tryGetArgument("-cv", "");

		asrCfg.enablePc = std::stoi(doPc);
		asrCfg.enableItn = std::stoi(doItn);
		asrCfg.enableRepetitionRemoval = std::stoi(doRr);
		asrCfg.enablePiiFiltering = std::stoi(doPii);
		asrCfg.enableDiarization = std::stoi(diarize);
		asrCfg.customVocabulary = readCustomVocabulary(customVocab);
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

// TODO: fix this
std::wstring _resultText;
std::wstring _resultTimestampsAndConf;
std::string _resultSpeakerEmbeddings;

std::string secondToHMS(float seconds)
{
    const size_t min = (static_cast<size_t>(seconds) % 3600) / 60;
    const size_t hour = seconds / 3600;
    const size_t sec = static_cast<size_t>(seconds) % 60;
    const size_t ms = (seconds - std::floor(seconds)) * 1000;

    const std::string hourStr = std::string(2 - std::to_string(hour).size(), '0') + std::to_string(hour);
    const std::string minStr = std::string(2 - std::to_string(min).size(), '0') + std::to_string(min);
    const std::string secStr = std::string(2 - std::to_string(sec).size(), '0') + std::to_string(sec);
    const std::string msStr = std::string(3 - std::to_string(ms).size(), '0') + std::to_string(ms);

    return hourStr + ":" + minStr + ":" + secStr + "," + msStr;
}

std::string outputDir = ".";
std::string inputAudioPath = "./aaa.wav";

std::string generateOutputFileName(std::string ext, std::string testMetadata = "")
{
	std::filesystem::path outPath = outputDir;
	outPath /= std::filesystem::path(inputAudioPath).stem();

	std::string outFileName = outPath.u8string();
	outFileName += "asr_10ms";
	outFileName += (testMetadata.empty() ? "" : "_") + testMetadata;
	outFileName += ext;

	std::cout << "* Output file: " << outFileName << std::endl;

	return outFileName;
}

void writeOutputToFile(KrispAudioAsrResult& asrResult, KrispAudioAsrSessionConfig& asrCfg)
{
    auto getResult = [&asrResult, &asrCfg]()
    {
        auto& words = asrResult.words;
        auto& speakers = asrResult.speakers;
        auto& resultText = _resultText;
        auto& resultConfText = _resultTimestampsAndConf;
        auto& resultSpeakerEmbeddings = _resultSpeakerEmbeddings;

        if (asrCfg.enableDiarization)
        {
            for (size_t c = 0; c < speakers.size(); ++c)
            {
                auto& el = speakers[c];
                const auto sp = el.speakerId;
                const auto start = el.startWordIndex;
                const auto end = el.endWordIndex;
                const auto& emb = el.speakerEmbedding;

                std::string diarText = std::to_string(c) + "\n";
                diarText += secondToHMS(words[start].start) + " --> " + secondToHMS(words[end].end) + "\n";
                diarText += "[" + sp + "]: ";

                // Speaker embeddings text.
                resultSpeakerEmbeddings += diarText;

                for (size_t i = start; i <= end; ++i)
                {
                    diarText += convertWstrToStr(words[i].text) + " ";
                }

                if (!diarText.empty())
                {
                    diarText.pop_back();
                }

                diarText += "\n\n";
                resultText += convertStrToWstr(diarText);

                // Collect speaker embeddings text.
                for (const auto& el : emb)
                {
                    resultSpeakerEmbeddings += std::to_string(el) + " ";
                }

                resultSpeakerEmbeddings += "\n\n";
            }
        }
        else
        {
            for (const auto& el : words)
            {
                resultText += el.text + L" ";
            }

            if (!words.empty())
            {
                resultText.pop_back();
            }
        }

        for (const auto& el : words)
        {
            std::stringstream ss;
            ss << convertWstrToStr(el.text) << "\t" << el.start << "\t" << el.end << "\t" << el.confidence << std::endl;
            resultConfText += convertStrToWstr(ss.str());
        }
    };

    getResult();

    auto saveResults = [&asrCfg]()
    {
        const auto& resultText = _resultText;
        const auto& resultTimestampsAndConf = _resultTimestampsAndConf;
        const auto& resultSpeakerEmbeddings = _resultSpeakerEmbeddings;
        const bool diarizationEnabled = asrCfg.enableDiarization;

        std::ofstream textOut(generateOutputFileName((diarizationEnabled ? ".diar" : ".asr")));
        textOut << convertWstrToStr(resultText);
        textOut.close();

        std::ofstream confOut(generateOutputFileName(".tms"));
        confOut << convertWstrToStr(resultTimestampsAndConf);
        confOut.close();

        if (diarizationEnabled)
        {
            std::ofstream speakerEmbeddingsOut(generateOutputFileName(".emb"));
            speakerEmbeddingsOut << resultSpeakerEmbeddings;
            speakerEmbeddingsOut.close();
        }
    };

    saveResults();
}

template <typename SamplingFormat>
int asrWavFileTmpl(const SoundFile &inSndFile, const std::string &weight, KrispAudioAsrSessionConfig& asrCfg)
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
	std::wstring modelPath = convertStrToWstr(weight);
	std::string modelAlias = "model";
	if (krispAudioSetModel(modelPath.c_str(), modelAlias.c_str()) != 0)
	{
		return error("Error loading AI model");
	}

	KrispAudioSessionID session = nullptr;
	KrispAudioFrameDuration krispFrameDuration = KRISP_AUDIO_FRAME_DURATION_10MS;

	if (typeid(SamplingFormat) == typeid(short))
	{
        session = krispAudioAsrCreateSessionInt16(inRate, krispFrameDuration, asrCfg, modelAlias.c_str());
    }
	else if (typeid(SamplingFormat) == typeid(float))
	{
        session = krispAudioAsrCreateSessionFloat(inRate, krispFrameDuration, asrCfg, modelAlias.c_str());
    }
	else
	{
        return error("Error invalid template type");
    }

	if (nullptr == session)
	{
		return error("Error creating ASR session");
	}

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

	// Audio stream ended, generate transcript.
	KrispAudioAsrResult asrResult;
	if (0 != krispAudioAsrGenerateResult(session, asrResult))
	{
		return error("Error calling krispAudioAsrGenerateResult");
	}

	writeOutputToFile(asrResult, asrCfg);

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

int asrWavFile(const std::string &input, const std::string &weight, KrispAudioAsrSessionConfig& asrCfg)
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
		return asrWavFileTmpl<short>(inSndFile, weight, asrCfg);
	}

	if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
	{
		return asrWavFileTmpl<float>(inSndFile, weight, asrCfg);
	}

	return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
	std::string inputAudio;
	std::string weight;
	KrispAudioAsrSessionConfig asrCfg;

	if (parseArguments(argc, argv, inputAudio, weight, asrCfg))
	{
		return asrWavFile(inputAudio, weight, asrCfg);
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
