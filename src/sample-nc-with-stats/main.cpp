#include <iostream>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include "argument_parser.hpp"
#include "wave_reader.hpp"
#include "wave_writer.hpp"

#include <krisp-audio-sdk-nc-stats.hpp>

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

std::pair<KrispAudioSamplingRate, bool> getKrispSamplingRate(size_t rate) {
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

void getNcStats(KrispAudioSessionID session, KrispAudioNcStats* ncStats)
{
	int result = krispAudioNcWithStatsRetrieveStats(session, ncStats);
	if (0 != result) {
		std::cerr << "Error retrieving NC stats " << std::endl;
		return;
	}

	std::cout << "#--- Noise/Voice stats ---" << std::endl;
	std::cout << "# - No     Noise: " << ncStats->noiseStats.noNoiseMs << " ms" << std::endl;
	std::cout << "# - Low    Noise: " << ncStats->noiseStats.lowNoiseMs << " ms" << std::endl;
	std::cout << "# - Medium Noise: " << ncStats->noiseStats.mediumNoiseMs << " ms" << std::endl;
	std::cout << "# - High   Noise: " << ncStats->noiseStats.highNoiseMs << " ms" << std::endl;
	std::cout << "#-------------------------" << std::endl;
	std::cout << "# - Talk time :   " << ncStats->voiceStats.talkTimeMs << " ms" << std::endl;
	std::cout << "#-------------------------" << std::endl;
}

int nc_wav_file(std::string& input, std::string& output, std::string& weight) {
	WaveReader reader;
	WaveWriter writer;
	std::vector<short> wavDataIn;
	std::vector<short> wavDataOut;
	int sampleRate;
	reader.read(input.c_str(), wavDataIn, sampleRate);
	KrispAudioSamplingRate inRate;
	auto samplingRateResult = getKrispSamplingRate(sampleRate);
	if (!samplingRateResult.second) {
		return error("Unsupported sample rate");
	}
	inRate = samplingRateResult.first;
	const KrispAudioSamplingRate outRate = inRate;
	const size_t frameDurationMillis = 10;
	auto durationResult = getKrispAudioFrameDuration(frameDurationMillis);
	if (!durationResult.second) {
		return error("Unsupported frame duration");
	}
	KrispAudioFrameDuration krispFrameDuration = durationResult.first;

	size_t inputBufferSize = (sampleRate * frameDurationMillis) / 1000;
	size_t outputBufferSize = inputBufferSize;

	if (krispAudioGlobalInit(nullptr) != 0) {
		return error("Failed to initialization Krisp SDK");
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
	std::wstring model_path = wstringConverter.from_bytes(weight);
	std::string model_alias = "model";
	if (krispAudioSetModel(model_path.c_str(), model_alias.c_str()) != 0) {
		return error("Error loading AI model");
	}

	KrispAudioSessionID session = krispAudioNcWithStatsCreateSession(inRate, outRate,
		krispFrameDuration, model_alias.c_str());
	if (nullptr == session) {
		return error("Error creating session");
	}

	wavDataOut.resize(wavDataIn.size() * outputBufferSize / inputBufferSize);
	KrispAudioNcPerFrameInfo perFrameInfo;
	KrispAudioNcStats ncStats;
	size_t i;
	for (i = 0; (i + 1) * inputBufferSize <= wavDataIn.size(); ++i) {
		int result = krispAudioNcWithStatsCleanAmbientNoiseInt16(
				session,
				&wavDataIn[i * inputBufferSize],
				static_cast<unsigned int>(inputBufferSize),
				&wavDataOut[i * outputBufferSize],
				static_cast<unsigned int>(outputBufferSize),
				&perFrameInfo
		);
		if (0 != result) {
			std::cerr << "Error cleaning noise on " << i << " frame" << std::endl;
			break;
		}

		std::cout << "[" << i + 1 << " x " << frameDurationMillis << "ms]"
			<< " noiseEn: " << perFrameInfo.noiseEnergy
			<< ", voiceEn: " << perFrameInfo.voiceEnergy << std::endl;

		if (i % 100 == 0)
		{
			// It's possible to get the stats in the middle of the stream processing.
			// It will contain the noise stats (noise level and amount) and the voice stats (talk duration)
			// for the processing time-frame. To reset the accumulation the new session should be started.
			getNcStats(session, &ncStats);
		}
	}
	wavDataOut.resize(i * outputBufferSize);

	std::cout << "Getting Final Call stats..." << std::endl;
	getNcStats(session, &ncStats);

	if (0 != krispAudioNcWithStatsCloseSession(session)) {
		return error("Error in closing instance");
	}
	session=nullptr;

	if (0 != krispAudioGlobalDestroy()) {
		return error("Error in closing ALL");
	}

	writer.write(output.c_str(), wavDataOut, sampleRate);
	return 0;
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
