#include <type_traits>
#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk-ex.hpp>


#include "argument_parser.hpp"
#include "sound_file.hpp"


using KrispAudioSdk::INoiseCleaner;


template <typename T>
int error(const T& e) {
	std::cerr << e << std::endl;
	return 1;
}

static bool parseArguments(std::string& input, std::string& output,
		std::string& modelsDir, bool &stats, int argc, char** argv) {
	ArgumentParser p(argc, argv);
	p.addArgument("--input", "-i", IMPORTANT);
	p.addArgument("--output", "-o",IMPORTANT);
	p.addArgument("--model_dir", "-m", IMPORTANT);
	p.addArgument("--stats", "-s", OPTIONAL);
	if (p.parse()) {
		input = p.getArgument("-i");
		output = p.getArgument("-o");
		modelsDir = p.getArgument("-m");
		stats = p.getOptionalArgument("-s");
	} else {
		std::cerr << p.getError();
		return false;
	}
	return true;
}

using KrispAudioSdk::SamplingRate;
static std::pair<SamplingRate, bool> getKrispSamplingRate(unsigned rate) {
	std::pair<SamplingRate, bool> result;
	result.second = true;
	switch (rate) {
	case 8000:
		result.first = SamplingRate::Sr8000;
		break;
	case 16000:
		result.first = SamplingRate::Sr16000;
		break;
	case 32000:
		result.first = SamplingRate::Sr32000;
		break;
	case 44100:
		result.first = SamplingRate::Sr44100;
		break;
	case 48000:
		result.first = SamplingRate::Sr48000;
		break;
	case 88200:
		result.first = SamplingRate::Sr88200;
		break;
	case 96000:
		result.first = SamplingRate::Sr96000;
		break;
	default:
		result.first = SamplingRate::Sr16000;
		result.second = false;
	}
	return result;
}

static void readAllFrames(const SoundFile & sndFile,
		std::vector<short> & frames) {
	sndFile.readAllFramesPCM16(&frames);
}

static void readAllFrames(const SoundFile & sndFile,
		std::vector<float> & frames) {
	sndFile.readAllFramesFloat(&frames);
}

static std::pair<bool, std::string> WriteFramesToFile(
	const std::string & fileName,
	const std::vector<int16_t> & frames,
	unsigned samplingRate)
{
	return writeSoundFilePCM16(fileName, frames, samplingRate);
}

static std::pair<bool, std::string> WriteFramesToFile(
	const std::string & fileName,
	const std::vector<float> & frames,
	unsigned samplingRate)
{
	return writeSoundFileFloat(fileName, frames, samplingRate);
}

static void printTotalStats(const std::unique_ptr<INoiseCleaner> & noiseCleanerPtr)
{
	INoiseCleaner::CumulativeStats totalStats = noiseCleanerPtr->getCumulativeStats();

	std::cout << "#--- Noise/Voice stats ---" << std::endl;
	std::cout << "# - No     Noise: " << totalStats._noNoiseMs << " ms" << std::endl;
	std::cout << "# - Low    Noise: " << totalStats._lowNoiseMs << " ms" << std::endl;
	std::cout << "# - Medium Noise: " << totalStats._mediumNoiseMs << " ms" << std::endl;
	std::cout << "# - High   Noise: " << totalStats._highNoiseMs << " ms" << std::endl;
	std::cout << "#-------------------------" << std::endl;
	std::cout << "# - Talk time :   " << totalStats._talkTimeMs << " ms" << std::endl;
	std::cout << "#-------------------------" << std::endl;
}


template <typename SamplingFormat>
KrispAudioSdk::SamplingType getSamplingType()
{
	static_assert(
		(std::is_same<SamplingFormat, short>::value ||
		std::is_same<SamplingFormat, float>::value),
	"SamplingFormat should be either float or short");

	if (std::is_same<SamplingFormat, short>::value)
	{
		return KrispAudioSdk::SamplingType::Pcm16;
	}
	if (std::is_same<SamplingFormat, float>::value)
	{
		return KrispAudioSdk::SamplingType::Float32;
	}
}

template <typename SamplingFormat>
int ncWavFileTmpl(
		const SoundFile & inSndFile,
		const std::string & output,
		const std::wstring & modelsDir,
		bool withStats) {
	std::vector<SamplingFormat> wavDataIn;
	std::vector<SamplingFormat> wavDataOut;

	readAllFrames(inSndFile, wavDataIn);

	wavDataOut.resize(wavDataIn.size());

	if (inSndFile.getHasError()) {
		return error(inSndFile.getErrorMsg());
	}

	unsigned samplingRateInt = inSndFile.getHeader().getSamplingRate();
	auto samplingRateResult = getKrispSamplingRate(samplingRateInt);
	if (!samplingRateResult.second) {
		return error("Unsupported sample rate");
	}
	KrispAudioSdk::SamplingRate samplingRate = samplingRateResult.first;

	auto voiceProcessorBuild = KrispAudioSdk::VoiceProcessorBuilder();
	std::vector<KrispAudioSdk::ModelId> modelsFound = voiceProcessorBuild.registerModels(modelsDir, true);

	KrispAudioSdk::SamplingType samplingType = getSamplingType<SamplingFormat>();
	auto attr = KrispAudioSdk::OutboundNoiseCleanerAttributes(samplingType, samplingRate);
	attr.setStats(withStats);
	std::unique_ptr<INoiseCleaner> noiseCleanerPtr =
		voiceProcessorBuild.createNoiseCleaner(attr);

	size_t frameSize = noiseCleanerPtr->getFrameSize();
	size_t i;
	for (i = 0; (i + 1) * frameSize <= wavDataIn.size(); ++i) {
		int result = noiseCleanerPtr->processFrame(
			&wavDataIn[i * frameSize], &wavDataOut[i * frameSize]);
		if (!result) {
			std::cerr << "Error cleaning noise on " << i << " frame"
				<< std::endl;
			break;
		}
		if (withStats) {
			INoiseCleaner::FrameStats frameStats =
				noiseCleanerPtr->getFrameStats();
			std::cout << "[" << i + 1 << " x 10ms]"
					<< " noiseEn: " << frameStats._noiseEnergy
					<< ", voiceEn: " << frameStats._voiceEnergy << std::endl;
		}
		if (withStats && i % 100 == 0) {
			printTotalStats(noiseCleanerPtr);
		}
	}
	wavDataOut.resize(i * frameSize);

	if (withStats) {
		std::cout << "Getting Final Call stats..." << std::endl;
		printTotalStats(noiseCleanerPtr);
	}

	auto pairResult = WriteFramesToFile(output, wavDataOut, samplingRateInt);
	if (!pairResult.first) {
		return error(pairResult.second);
	}

	return 0;
}

static int ncWavFile(const std::string& input, const std::string& output,
		const std::string& modelsDir, bool withStats) {
	SoundFile inSndFile;
	inSndFile.loadHeader(input);
	if (inSndFile.getHasError()) {
		return error(inSndFile.getErrorMsg());
	}
	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
	std::wstring modelsDirWide = wstringConverter.from_bytes(modelsDir);
	auto sndFileHeader = inSndFile.getHeader();
	if (sndFileHeader.getFormat() == SoundFileFormat::PCM16) {
		return ncWavFileTmpl<short>(inSndFile, output, modelsDirWide, withStats);
	}
	if (sndFileHeader.getFormat() == SoundFileFormat::FLOAT) {
		return ncWavFileTmpl<float>(inSndFile, output, modelsDirWide, withStats);
	}
	return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char** argv) {
	std::string in, out, modelsDir;
	bool stats = false;
	if (parseArguments(in, out, modelsDir, stats, argc, argv)) {
		return ncWavFile(in, out, modelsDir, stats);
	} else {
		std::cerr << "\nUsage:\n\t" << argv[0]
			<< " -i input.wav -o output.wav -m model_path" << std::endl;
		if (argc == 1) {
			return 0;
		}
		return 1;
	}
}
