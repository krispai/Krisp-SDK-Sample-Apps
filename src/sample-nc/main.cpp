#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include "argument_parser.hpp"
#include "wave_reader.hpp"
#include "wave_writer.hpp"
#include "sound_file.hpp"

#include <krisp-audio-sdk-nc.hpp>


static int krispAudioNcCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const short * pFrameIn,
		unsigned int frameInSize,
		short * pFrameOut,
		unsigned int frameOutSize) {
	return krispAudioNcCleanAmbientNoiseInt16(
		pSession,
		pFrameIn,
		frameInSize,
		pFrameOut,
		frameOutSize);
}

static int krispAudioNcCleanAmbientNoise(
		KrispAudioSessionID pSession,
		const float * pFrameIn,
		unsigned int frameInSize,
		float * pFrameOut,
		unsigned int frameOutSize) {
	return krispAudioNcCleanAmbientNoiseFloat(
		pSession,
		pFrameIn,
		frameInSize,
		pFrameOut,
		frameOutSize);
}

template <typename T>
int error(const T& e) {
	std::cerr << e << std::endl;
	return 1;
}

bool parse_arguments(std::string& input, std::string& output,
		std::string& weight, int argc, char** argv) {
	ArgumentParser p(argc, argv);
	p.addArgument("--input", "-i", IMPORTANT);
	p.addArgument("--output", "-o",IMPORTANT);
	p.addArgument("--weight_file", "-w", IMPORTANT);
	if (p.parse()) {
		input = p.getArgument("-i");
		output = p.getArgument("-o");
		weight = p.getArgument("-w");
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

void read_all_frames(const SoundFile & sndFile,
		std::vector<short> & frames) {
	sndFile.read_all_frames_pcm16(&frames);
}

void read_all_frames(const SoundFile & sndFile,
		std::vector<float> & frames) {
	sndFile.read_all_frames_float(&frames);
}

std::pair<bool, std::string> WriteFramesToFile(
	const std::string & fileName, 
	const std::vector<int16_t> & frames,
	unsigned samplingRate)
{
	return WriteSoundFilePCM16(fileName, frames, samplingRate);
}

std::pair<bool, std::string> WriteFramesToFile(
	const std::string & fileName, 
	const std::vector<float> & frames,
	unsigned samplingRate)
{
	return WriteSoundFileFloat(fileName, frames, samplingRate);
}

template <typename SamplingFormat>
int nc_wav_file_tmpl(const SoundFile & inSndFile, const std::string & output,
		const std::string & weight) {
	WaveWriter writer;
	std::vector<SamplingFormat> wavDataIn;
	std::vector<SamplingFormat> wavDataOut;

	read_all_frames(inSndFile, wavDataIn);

	if (inSndFile.get_has_error()) {
		return error(inSndFile.get_error_msg());
	}

	unsigned samplingRate = inSndFile.get_header().get_sampling_rate();
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
	std::wstring model_path = wstringConverter.from_bytes(weight);
	std::string model_alias = "model";
	if (krispAudioSetModel(model_path.c_str(), model_alias.c_str()) != 0) {
		return error("Error loading AI model");
	}

	KrispAudioSessionID session = krispAudioNcCreateSession(inRate, outRate,
		krispFrameDuration, model_alias.c_str());
	if (nullptr == session) {
		return error("Error creating session");
	}

	wavDataOut.resize(wavDataIn.size() * outputFrameSize / inputFrameSize);
	size_t i;
	for (i = 0; (i + 1) * inputFrameSize <= wavDataIn.size(); ++i) {
		int result = krispAudioNcCleanAmbientNoise(
				session,
				&wavDataIn[i * inputFrameSize],
				static_cast<unsigned int>(inputFrameSize),
				&wavDataOut[i * outputFrameSize],
				static_cast<unsigned int>(outputFrameSize)
		);
		if (0 != result) {
			std::cerr << "Error cleaning noise on " << i << " frame"
				<< std::endl;
			break;
		}
	}
	wavDataOut.resize(i * outputFrameSize);

	if (0 != krispAudioNcCloseSession(session)) {
		return error("Error calling krispAudioNcCloseSession");
	}
	session = nullptr;

	if (0 != krispAudioGlobalDestroy()) {
		return error("Error calling krispAudioGlobalDestroy");
	}

	auto result = WriteFramesToFile(output, wavDataOut, samplingRate);
	if (!result.first) {
		return error(result.second);
	}

	return 0;
}

int nc_wav_file(const std::string& input, const std::string& output,
		const std::string& weight) {
	SoundFile inSndFile;
	inSndFile.load_header(input);
	if (inSndFile.get_has_error()) {
		return error(inSndFile.get_error_msg());
	}
	auto sndFileHeader = inSndFile.get_header();
	if (sndFileHeader.get_format() == SoundFileFormat::PCM16) {
		return nc_wav_file_tmpl<short>(inSndFile, output, weight);
	}
	if (sndFileHeader.get_format() == SoundFileFormat::FLOAT) {
		return nc_wav_file_tmpl<float>(inSndFile, output, weight);
	}
	return error("The sound file format should be PCM16 or FLOAT.");
}

int main(int argc, char** argv) {
	std::string in, out, weight;
	if (parse_arguments(in, out, weight, argc, argv)) {
		return nc_wav_file(in, out, weight);
	} else {
		std::cerr << "\nUsage:\n\t" << argv[0]
			<< " -i input.wav -o output.wav -w weightFile" << std::endl;
		if (argc == 1) {
			return 0;
		}
		return 1;
	}
}
