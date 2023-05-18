#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk-nc.hpp>
#include <krisp-audio-sdk-nc-stats.hpp>

#include "argument_parser.hpp"
#include "sound_file.hpp"



static int krispAudioNcCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const short * pFrameIn,
		unsigned int frameInSize,
		short * pFrameOut,
		unsigned int frameOutSize)
{
	return krispAudioNcCleanAmbientNoiseInt16(
		pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize);
}

static int krispAudioNcCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const float * pFrameIn,
		unsigned int frameInSize,
		float * pFrameOut,
		unsigned int frameOutSize)
{
	return krispAudioNcCleanAmbientNoiseFloat(
		pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize);
}

static int krispAudioNcWithStatsCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const short* pFrameIn,
        unsigned int frameInSize,
		short* pFrameOut,
		unsigned int frameOutSize,
		KrispAudioNcPerFrameInfo* energyInfo)
{
	return krispAudioNcWithStatsCleanAmbientNoiseInt16(
		pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize, energyInfo);
}

static int krispAudioNcWithStatsCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const float* pFrameIn,
        unsigned int frameInSize,
		float* pFrameOut,
		unsigned int frameOutSize,
		KrispAudioNcPerFrameInfo* energyInfo)
{
	return krispAudioNcWithStatsCleanAmbientNoiseFloat(
		pSession, pFrameIn, frameInSize, pFrameOut, frameOutSize, energyInfo);
}

template <typename T>
int error(const T& e) {
	std::cerr << e << std::endl;
	return 1;
}

bool parseArguments(std::string& input, std::string& output,
		std::string& weight, bool &stats, int argc, char** argv) {
	ArgumentParser p(argc, argv);
	p.addArgument("--input", "-i", IMPORTANT);
	p.addArgument("--output", "-o",IMPORTANT);
	p.addArgument("--weight_file", "-w", IMPORTANT);
	p.addArgument("--stats", "-s", OPTIONAL);
	if (p.parse()) {
		input = p.getArgument("-i");
		output = p.getArgument("-o");
		weight = p.getArgument("-w");
		stats = p.getOptionalArgument("-s");
	} else {
		std::cerr << p.getError();
		return false;
	}
	return true;
}

std::pair<KrispAudioFrameDuration, bool> getKrispAudioFrameDuration(size_t ms) {
	std::pair<KrispAudioFrameDuration, bool> result;
	result.second = true;
	switch (ms) {
	case 10:
		result.first = KRISP_AUDIO_FRAME_DURATION_10MS;
		break;
	case 20:
		result.first = KRISP_AUDIO_FRAME_DURATION_20MS;
		break;
	case 30:
		result.first = KRISP_AUDIO_FRAME_DURATION_30MS;
		break;
	case 40:
		result.first = KRISP_AUDIO_FRAME_DURATION_40MS;
		break;
	default:
		result.second = false;
		result.first = KRISP_AUDIO_FRAME_DURATION_30MS;
	}
	return result;
}

std::pair<KrispAudioSamplingRate, bool> getKrispSamplingRate(unsigned rate) {
	std::pair<KrispAudioSamplingRate, bool> result;
	result.second = true;
	switch (rate) {
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

void readAllFrames(const SoundFile & sndFile,
		std::vector<short> & frames) {
	sndFile.readAllFramesPCM16(&frames);
}

void readAllFrames(const SoundFile & sndFile,
		std::vector<float> & frames) {
	sndFile.readAllFramesFloat(&frames);
}

std::pair<bool, std::string> WriteFramesToFile(
	const std::string & fileName, 
	const std::vector<int16_t> & frames,
	unsigned samplingRate)
{
	return writeSoundFilePCM16(fileName, frames, samplingRate);
}

std::pair<bool, std::string> WriteFramesToFile(
	const std::string & fileName, 
	const std::vector<float> & frames,
	unsigned samplingRate)
{
	return writeSoundFileFloat(fileName, frames, samplingRate);
}

void getNcStats(KrispAudioSessionID session, KrispAudioNcStats* ncStats)
{
	int result = krispAudioNcWithStatsRetrieveStats(session, ncStats);
	if (0 != result) {
		std::cerr << "Error retrieving NC stats " << std::endl;
		return;
	}

	std::cout << "#--- Noise/Voice stats ---" << std::endl;
	std::cout << "# - No     Noise: " <<
		ncStats->noiseStats.noNoiseMs << " ms" << std::endl;
	std::cout << "# - Low    Noise: " <<
		ncStats->noiseStats.lowNoiseMs << " ms" << std::endl;
	std::cout << "# - Medium Noise: " <<
		ncStats->noiseStats.mediumNoiseMs << " ms" << std::endl;
	std::cout << "# - High   Noise: " <<
		ncStats->noiseStats.highNoiseMs << " ms" << std::endl;
	std::cout << "#-------------------------" << std::endl;
	std::cout << "# - Talk time :   " <<
		ncStats->voiceStats.talkTimeMs << " ms" << std::endl;
	std::cout << "#-------------------------" << std::endl;
}

template <typename SamplingFormat>
int ncWavFileTmpl(
		const SoundFile & inSndFile,
		const std::string & output,
		const std::string & weight,
		bool withStats) {
	std::vector<SamplingFormat> wavDataIn;
	std::vector<SamplingFormat> wavDataOut;

	readAllFrames(inSndFile, wavDataIn);

	if (inSndFile.getHasError()) {
		return error(inSndFile.getErrorMsg());
	}

	unsigned samplingRate = inSndFile.getHeader().getSamplingRate();
	auto samplingRateResult = getKrispSamplingRate(samplingRate);
	if (!samplingRateResult.second) {
		return error("Unsupported sample rate");
	}
	KrispAudioSamplingRate inRate = samplingRateResult.first;
	const KrispAudioSamplingRate outRate = inRate;
	const size_t frameDurationMillis = 10;
	auto durationResult = getKrispAudioFrameDuration(frameDurationMillis);
	if (!durationResult.second) {
		return error("Unsupported frame duration");
	}
	KrispAudioFrameDuration krispFrameDuration = durationResult.first;

	size_t inputFrameSize = (samplingRate * frameDurationMillis) / 1000;
	size_t outputFrameSize = inputFrameSize;

	if (krispAudioGlobalInit(nullptr) != 0) {
		return error("Failed to initialization Krisp SDK");
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
	std::wstring modelPath = wstringConverter.from_bytes(weight);
	std::string modelAlias = "model";
	if (krispAudioSetModel(modelPath.c_str(), modelAlias.c_str()) != 0) {
		return error("Error loading AI model");
	}

	KrispAudioSessionID session = nullptr;

	if (withStats) {
		session = krispAudioNcWithStatsCreateSession(inRate, outRate,
				krispFrameDuration, modelAlias.c_str());
	}
	else {
		session = krispAudioNcCreateSession(inRate, outRate,
				krispFrameDuration, modelAlias.c_str());
	}


	if (nullptr == session) {
		return error("Error creating session");
	}

	KrispAudioNcStats ncStats;
	KrispAudioNcPerFrameInfo perFrameInfo;

	wavDataOut.resize(wavDataIn.size() * outputFrameSize / inputFrameSize);
	size_t i;
	for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i) {
		int result;
		if (withStats) {
			result = krispAudioNcWithStatsCleanAmbientNoise(
				session,
				&wavDataIn[i * inputFrameSize],
				static_cast<unsigned int>(inputFrameSize),
				&wavDataOut[i * outputFrameSize],
				static_cast<unsigned int>(outputFrameSize),
				&perFrameInfo
			);
		}
		else {
			result = krispAudioNcCleanAmbientNoise(
				session,
				&wavDataIn[i * inputFrameSize],
				static_cast<unsigned int>(inputFrameSize),
				&wavDataOut[i * outputFrameSize],
				static_cast<unsigned int>(outputFrameSize)
			);
		}
		if (0 != result) {
			std::cerr << "Error cleaning noise on " << i << " frame"
				<< std::endl;
			break;
		}
		if (withStats) {
			std::cout << "[" << i + 1 << " x " << frameDurationMillis << "ms]"
				<< " noiseEn: " << perFrameInfo.noiseEnergy
				<< ", voiceEn: " << perFrameInfo.voiceEnergy << std::endl;
		}
		if (withStats && i % 100 == 0) {
			getNcStats(session, &ncStats);
		}
	}
	wavDataOut.resize(i * outputFrameSize);

	if (withStats) {
		std::cout << "Getting Final Call stats..." << std::endl;
		getNcStats(session, &ncStats);
	}

	if (withStats) {
		if (0 != krispAudioNcWithStatsCloseSession(session)) {
			return error("Error calling krispAudioNcWithStatsCloseSession");
		}
	}
	else {
		if (0 != krispAudioNcCloseSession(session)) {
			return error("Error calling krispAudioNcCloseSession");
		}
	}
	session = nullptr;

	if (0 != krispAudioGlobalDestroy()) {
		return error("Error calling krispAudioGlobalDestroy");
	}

	auto pairResult = WriteFramesToFile(output, wavDataOut, samplingRate);
	if (!pairResult.first) {
		return error(pairResult.second);
	}

	return 0;
}

int ncWavFile(const std::string& input, const std::string& output,
		const std::string& weight, bool withStats) {
	SoundFile inSndFile;
	inSndFile.loadHeader(input);
	if (inSndFile.getHasError()) {
		return error(inSndFile.getErrorMsg());
	}
	auto sndFileHeader = inSndFile.getHeader();
	if (sndFileHeader.getFormat() == SoundFileFormat::PCM16) {
		return ncWavFileTmpl<short>(inSndFile, output, weight, withStats);
	}
	if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT) {
		return ncWavFileTmpl<float>(inSndFile, output, weight, withStats);
	}
	return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char** argv) {
	std::string in, out, weight;
	bool stats = false;
	if (parseArguments(in, out, weight, stats, argc, argv)) {
		return ncWavFile(in, out, weight, stats);
	} else {
		std::cerr << "\nUsage:\n\t" << argv[0]
			<< " -i input.wav -o output.wav -w weightFile" << std::endl;
		if (argc == 1) {
			return 0;
		}
		return 1;
	}
}
