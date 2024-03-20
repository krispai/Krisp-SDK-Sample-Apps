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


class AppCommandLineParser
{
public:
	bool parseCommandLine(int argc, char **argv)
	{
		ArgumentParser p(argc, argv);
		p.addArgument("--input_audio", "-i", IMPORTANT);
		p.addArgument("--output_dir", "-o", IMPORTANT);
		p.addArgument("--model_path", "-m", IMPORTANT);
		p.addArgument("--do_pc", "-pc", OPTIONAL);
		p.addArgument("--do_itn", "-itn", OPTIONAL);
		p.addArgument("--do_rr", "-rr", OPTIONAL);
		p.addArgument("--do_pii", "-pii", OPTIONAL);
		p.addArgument("--use_vad", "-vad", OPTIONAL);
		p.addArgument("--diarize", "-diar", OPTIONAL);
		p.addArgument("--custom_vocab", "-cv", OPTIONAL);
		if (p.parse())
		{
			m_input = p.getArgument("-i");
			m_weight = p.getArgument("-m");
			m_outputDir = p.getArgument("-o");

			m_doPc = p.tryGetArgument("-pc", "0");
			m_doItn = p.tryGetArgument("-itn", "0");
			m_doRr = p.tryGetArgument("-rr", "0");
			m_doPii = p.tryGetArgument("-pii", "0");
			m_vad = p.tryGetArgument("-vad", "0");
			m_diarize = p.tryGetArgument("-diar", "0");
			m_customVocab = p.tryGetArgument("-cv", "");
		}
		else
		{
			std::cerr << p.getError();
			return false;
		}
		return true;
	}

	KrispAudioAsrSessionConfig getAsrConfig()
	{
		KrispAudioAsrSessionConfig asrCfg;
		asrCfg.enablePc = std::stoi(m_doPc);
		asrCfg.enableItn = std::stoi(m_doItn);
		asrCfg.enableRepetitionRemoval = std::stoi(m_doRr);
		asrCfg.enablePiiFiltering = std::stoi(m_doPii);
		asrCfg.enableDiarization = std::stoi(m_diarize);
		asrCfg.customVocabulary = readCustomVocabulary(m_customVocab);
		return asrCfg;
	}

	std::string getInputAudioPath() const
	{
		return m_input;
	}

	std::string getModelPath() const
	{
		return m_weight;
	}

	std::string getOutputDirectory() const
	{
		return m_outputDir;
	}

private:
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

	std::string m_input;
	std::string m_weight;
	std::string m_outputDir;

	std::string m_doPc;
	std::string m_doItn;
	std::string m_doRr;
	std::string m_doPii;
	std::string m_vad;
	std::string m_diarize;
	std::string m_customVocab;
};


struct AppParameters
{
	std::string inputAudioPath;
	std::string asrOutputDirectory;
	std::string modelFilePath;
	KrispAudioAsrSessionConfig asrSessionConfig;
};

bool getInputFromCommandLine(int argc, char **argv, AppParameters * inputPtr)
{
	AppCommandLineParser p;
	if (!p.parseCommandLine(argc, argv))
	{
		std::cerr << "\nUsage:\n\t" << argv[0]
				<< " -i inputWavAudioPath -m modelPath" << std::endl;
		return false;
	}
	inputPtr->asrSessionConfig = p.getAsrConfig();
	inputPtr->inputAudioPath = p.getInputAudioPath();
	inputPtr->modelFilePath = p.getModelPath();
	inputPtr->asrOutputDirectory = p.getOutputDirectory();
	if (std::filesystem::exists(inputPtr->asrOutputDirectory))
	{
		std::cerr << "The output directory " << inputPtr->asrOutputDirectory << " already exists.";
		return false;
	}
	return true;
}

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

std::string generateOutputFileName(const AppParameters & appParams,
	std::string ext, std::string testMetadata = "")
{
	std::filesystem::path outPath = appParams.asrOutputDirectory;
	outPath /= std::filesystem::path(appParams.inputAudioPath).stem();

	std::string outFileName = outPath.u8string();
	outFileName += "asr_10ms";
	outFileName += (testMetadata.empty() ? "" : "_") + testMetadata;
	outFileName += ext;

	std::cout << "* Output file: " << outFileName << std::endl;

	return outFileName;
}

void writeOutputToFile(const AppParameters & appParams, const KrispAudioAsrResult & asrResult)
{
    auto getResult = [&asrResult, &appParams]()
    {
        auto& words = asrResult.words;
        auto& speakers = asrResult.speakers;
        auto& resultText = _resultText;
        auto& resultConfText = _resultTimestampsAndConf;
        auto& resultSpeakerEmbeddings = _resultSpeakerEmbeddings;

        if (appParams.asrSessionConfig.enableDiarization)
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

    auto saveResults = [&appParams]()
    {
        const auto& resultText = _resultText;
        const auto& resultTimestampsAndConf = _resultTimestampsAndConf;
        const auto& resultSpeakerEmbeddings = _resultSpeakerEmbeddings;
        const bool diarizationEnabled = appParams.asrSessionConfig.enableDiarization;

		if (!std::filesystem::create_directory(appParams.asrOutputDirectory))
		{
			assert(0);
			// TODO: report error
		}

        std::ofstream textOut(generateOutputFileName(appParams, (diarizationEnabled ? ".diar" : ".asr")));
        textOut << convertWstrToStr(resultText);
        textOut.close();
		// TODO: check for IO error

        std::ofstream confOut(generateOutputFileName(appParams, ".tms"));
        confOut << convertWstrToStr(resultTimestampsAndConf);
        confOut.close();
		// TODO: check for IO error

        if (diarizationEnabled)
        {
            std::ofstream speakerEmbeddingsOut(generateOutputFileName(appParams, ".emb"));
            speakerEmbeddingsOut << resultSpeakerEmbeddings;
            speakerEmbeddingsOut.close();
        }
    };

    saveResults();
}

template <typename SamplingFormat>
int asrWavFileTmpl(const SoundFile &inSndFile, const AppParameters & appParams)
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
	std::wstring modelPathW = convertStrToWstr(appParams.modelFilePath);
	std::string modelAlias = "anyUniqueString";
	if (krispAudioSetModel(modelPathW.c_str(), modelAlias.c_str()) != 0)
	{
		return error("Error loading AI model");
	}

	KrispAudioSessionID session = nullptr;
	KrispAudioFrameDuration krispFrameDuration = KRISP_AUDIO_FRAME_DURATION_10MS;

	if (typeid(SamplingFormat) == typeid(short))
	{
        session = krispAudioAsrCreateSessionInt16(inRate, krispFrameDuration,
			appParams.asrSessionConfig, modelAlias.c_str());
    }
	else if (typeid(SamplingFormat) == typeid(float))
	{
        session = krispAudioAsrCreateSessionFloat(inRate, krispFrameDuration,
			appParams.asrSessionConfig, modelAlias.c_str());
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

	auto frameIterator = wavDataIn.begin();
	while (std::distance(frameIterator, wavDataIn.end()) >= inputFrameSize)
	{
		std::vector<SamplingFormat> frame(frameIterator, frameIterator + inputFrameSize);
		std::advance(frameIterator, inputFrameSize);

		auto result = krispAudioAsrProcess(session, frame);
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

	writeOutputToFile(appParams, asrResult);

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

int asrWavFile(const AppParameters & appParams)
{
	SoundFile inSndFile;
	inSndFile.loadHeader(appParams.inputAudioPath);
	if (inSndFile.getHasError())
	{
		return error(inSndFile.getErrorMsg());
	}

	auto sndFileHeader = inSndFile.getHeader();

	if (sndFileHeader.getFormat() == SoundFileFormat::PCM16)
	{
		return asrWavFileTmpl<short>(inSndFile, appParams);
	}

	if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT)
	{
		return asrWavFileTmpl<float>(inSndFile, appParams);
	}

	return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char **argv)
{
	AppParameters appParams;
	if (!getInputFromCommandLine(argc, argv, &appParams)) {
		return 1;
	}
    return asrWavFile(appParams);
}
