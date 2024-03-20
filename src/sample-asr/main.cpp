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


class AppParameters
{
public:
	bool loadFromCommandLine(int argc, char **argv)
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
			loadCommandLineParams(p);
		}
		else
		{
			std::cerr << p.getError();
			return false;
		}
		if (std::filesystem::exists(m_asrOutputDirectory))
		{
			std::cerr << "The output directory " << m_asrOutputDirectory << " already exists.";
			return false;
		}
		return true;
	}

	const std::string & getInputAudioPath() const
	{
		return m_inputAudioPath;
	}
	const std::string & getAsrOutputDirectory() const
	{
		return m_asrOutputDirectory;
	}
	const std::string & getModelPath() const
	{
		return m_modelPath;
	}
	const KrispAudioAsrSessionConfig & getAsrSessionConfig() const
	{
		return m_asrSessionConfig;
	}

private:
	void loadCommandLineParams(const ArgumentParser & p)
	{
		m_inputAudioPath = p.getArgument("-i");
		m_modelPath = p.getArgument("-m");
		m_asrOutputDirectory = p.getArgument("-o");
		std::string doPc = p.tryGetArgument("-pc", "0");
		std::string doItn = p.tryGetArgument("-itn", "0");
		std::string doRr = p.tryGetArgument("-rr", "0");
		std::string doPii = p.tryGetArgument("-pii", "0");
		std::string vad = p.tryGetArgument("-vad", "0");
		std::string diarize = p.tryGetArgument("-diar", "0");
		std::string customVocab = p.tryGetArgument("-cv", "");
		m_asrSessionConfig.enablePc = std::stoi(doPc);
		m_asrSessionConfig.enableItn = std::stoi(doItn);
		m_asrSessionConfig.enableRepetitionRemoval = std::stoi(doRr);
		m_asrSessionConfig.enablePiiFiltering = std::stoi(doPii);
		m_asrSessionConfig.enableDiarization = std::stoi(diarize);
		m_asrSessionConfig.customVocabulary = readCustomVocabulary(customVocab);
	}

	static std::vector<std::string> readCustomVocabulary(const std::string& customVocabularyPath)
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


	std::string m_inputAudioPath;
	std::string m_asrOutputDirectory;
	std::string m_modelPath;
	KrispAudioAsrSessionConfig m_asrSessionConfig;

};

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
	std::filesystem::path outPath = appParams.getAsrOutputDirectory();
	outPath /= std::filesystem::path(appParams.getInputAudioPath()).stem();

	std::string outFileName = outPath.u8string();
	outFileName += "asr_10ms";
	outFileName += (testMetadata.empty() ? "" : "_") + testMetadata;
	outFileName += ext;

	std::cout << "* Output file: " << outFileName << std::endl;

	return outFileName;
}

class AsrResultProcessor
{
public:
	void getResulsts(const AppParameters & appParams, const KrispAudioAsrResult & asrResult)
	{
        auto& words = asrResult.words;
        auto& speakers = asrResult.speakers;
        if (appParams.getAsrSessionConfig().enableDiarization)
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
                m_speakerEmbeddings += diarText;
                for (size_t i = start; i <= end; ++i)
                {
                    diarText += convertWstrToStr(words[i].text) + " ";
                }
                if (!diarText.empty())
                {
                    diarText.pop_back();
                }
                diarText += "\n\n";
                m_text += convertStrToWstr(diarText);
                // Collect speaker embeddings text.
                for (const auto& el : emb)
                {
                    m_speakerEmbeddings += std::to_string(el) + " ";
                }
                m_speakerEmbeddings += "\n\n";
            }
        }
        else
        {
            for (const auto& el : words)
            {
                m_text += el.text + L" ";
            }
            if (!words.empty())
            {
                m_text.pop_back();
            }
        }

        for (const auto& el : words)
        {
            std::stringstream ss;
            ss << convertWstrToStr(el.text) << "\t" << el.start << "\t" << el.end << "\t" << el.confidence << std::endl;
            m_timestampsAndConf += convertStrToWstr(ss.str());
        }
	}

	void saveResults(const AppParameters & appParams)
    {
        const bool diarizationEnabled = appParams.getAsrSessionConfig().enableDiarization;
		if (!std::filesystem::create_directory(appParams.getAsrOutputDirectory()))
		{
			assert(0);
			// TODO: report error
		}
        std::ofstream textOut(generateOutputFileName(appParams, (diarizationEnabled ? ".diar" : ".asr")));
        textOut << convertWstrToStr(m_text);
        textOut.close();
		// TODO: check for IO error
        std::ofstream confOut(generateOutputFileName(appParams, ".tms"));
        confOut << convertWstrToStr(m_timestampsAndConf);
        confOut.close();
		// TODO: check for IO error
        if (diarizationEnabled)
        {
            std::ofstream speakerEmbeddingsOut(generateOutputFileName(appParams, ".emb"));
            speakerEmbeddingsOut << m_speakerEmbeddings;
            speakerEmbeddingsOut.close();
        }
    }
private:
	std::wstring m_text;
	std::wstring m_timestampsAndConf;
	std::string m_speakerEmbeddings;
};

void writeOutputToFile(const AppParameters & appParams, const KrispAudioAsrResult & asrResult)
{
	AsrResultProcessor a;
	a.getResulsts(appParams, asrResult);
	a.saveResults(appParams);
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
	std::wstring modelPathW = convertStrToWstr(appParams.getModelPath());
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
			appParams.getAsrSessionConfig(), modelAlias.c_str());
    }
	else if (typeid(SamplingFormat) == typeid(float))
	{
        session = krispAudioAsrCreateSessionFloat(inRate, krispFrameDuration,
			appParams.getAsrSessionConfig(), modelAlias.c_str());
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
	inSndFile.loadHeader(appParams.getInputAudioPath());
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
	if (!appParams.loadFromCommandLine(argc, argv)) {
		return 1;
	}
    return asrWavFile(appParams);
}
