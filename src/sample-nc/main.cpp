// Headers from other projects
#include "argument_parser.hpp"
#include "wave_reader.hpp"
#include "wave_writer.hpp"

// Headers from standard library
#include <iostream>
#include <cstring>
#include <sstream>
#include <fstream>

// Headers from other thz-sdk
#include <krisp-audio-sdk.hpp>

#define FRAME_DURATION 30

void print_help(char* e) {
    std::cerr << "\nUsage:" << std::endl;
    std::cerr << "\t" << e << " -r rate -i input.wav -o output.wav -w weightFile" << std::endl;
}

template <typename T>
int error(const T& e) {
    std::cerr << e << std::endl;
    return 1;
}

bool parse_arguments(std::string& input, std::string& output,
		std::string& weight, int argc, char** argv) {
    KRISP::TEST_UTILS::ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i",  IMPORTANT);
    p.addArgument("--output", "-o", IMPORTANT);
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

KrispAudioFrameDuration getFrameDur(size_t dur) {
    switch (dur) {
    case 10: return KRISP_AUDIO_FRAME_DURATION_10MS; break;
    case 15: return KRISP_AUDIO_FRAME_DURATION_15MS; break;
    case 20: return KRISP_AUDIO_FRAME_DURATION_20MS; break;
    case 30: return KRISP_AUDIO_FRAME_DURATION_30MS; break;
    case 32: return KRISP_AUDIO_FRAME_DURATION_32MS; break;
    case 40: return KRISP_AUDIO_FRAME_DURATION_40MS; break;
    default:
        std::cerr<<"Unsupported Frame Duration\n";
        return KRISP_AUDIO_FRAME_DURATION_30MS;
    }
}

KrispAudioSamplingRate getSampleRate(size_t val) {
    switch (val) {
    case 8000:  return KRISP_AUDIO_SAMPLING_RATE_8000HZ;  break;
    case 16000: return KRISP_AUDIO_SAMPLING_RATE_16000HZ; break;
    case 24000: return KRISP_AUDIO_SAMPLING_RATE_24000HZ; break;
    case 32000: return KRISP_AUDIO_SAMPLING_RATE_32000HZ; break;
    case 44100: return KRISP_AUDIO_SAMPLING_RATE_44100HZ; break;
    case 48000: return KRISP_AUDIO_SAMPLING_RATE_48000HZ; break;
    case 88200: return KRISP_AUDIO_SAMPLING_RATE_88200HZ; break;
    case 96000: return KRISP_AUDIO_SAMPLING_RATE_96000HZ; break;
    default:
        std::cerr<<"The input wav sampling rate is not supported";
        return KRISP_AUDIO_SAMPLING_RATE_16000HZ;
        break;
    }
}

int run_test(std::string& input, std::string& output, std::string& weight) {
    KRISP::TEST_UTILS::WaveReader reader;
    KRISP::TEST_UTILS::WaveWriter writer;
    std::vector<short> wavDataIn;
    std::vector<short> wavDataOut;
    int inRate_HZ;
    reader.read(input.c_str(), wavDataIn, inRate_HZ);
	int outRate_HZ = inRate_HZ;
    KrispAudioSamplingRate inRate = getSampleRate(inRate_HZ);
    KrispAudioSamplingRate outRate = getSampleRate(outRate_HZ);
    KrispAudioFrameDuration frameDuration = getFrameDur(FRAME_DURATION);

    size_t IN_BUF_SIZE = (inRate_HZ * FRAME_DURATION)/1000;
    size_t OUT_BUF_SIZE = (outRate_HZ * FRAME_DURATION)/1000;

    if (krispAudioGlobalInit(nullptr, 1)!=0) {
        error("GLOBAL INITIALIZATION ERROR");
	}

	if (krispAudioSetModel(
			convertMBString2WString(weight).c_str(),
			"model") != 0) {
        error("GLOBAL INITIALIZATION ERROR");
	}

    KrispAudioSessionID session =
		krispAudioNcCreateSession(inRate, outRate, frameDuration, nullptr);
    if (nullptr == session) {
        return error("Error creating session");
    }

    wavDataOut.resize(wavDataIn.size()*OUT_BUF_SIZE/IN_BUF_SIZE);
    size_t i;
    for (i = 0; (i+1)*IN_BUF_SIZE <= wavDataIn.size(); ++i ) {
		int result = krispAudioNcCleanAmbientNoiseInt16(
				session,
				&wavDataIn[i*IN_BUF_SIZE],
				static_cast<unsigned int>(IN_BUF_SIZE),
				&wavDataOut[i*OUT_BUF_SIZE],
				static_cast<unsigned int>(OUT_BUF_SIZE)
		);
        if (0 > result) {
            std::cerr << "Error cleaning noise on " << i << " frame"
				<< std::endl;
            break;
        }
    }
    wavDataOut.resize(i*OUT_BUF_SIZE);

    if (0 != krispAudioNcCloseSession(session)) {
		return error("Error in closing instance");
	}
    session=nullptr;

    if (0 != krispAudioGlobalDestroy()) {
		return error("Error in closing ALL");
	}

    writer.write(output.c_str(),wavDataOut,outRate_HZ);

    return 0;
}


int main(int argc, char** argv) {
	std::string in, out, weight;
    if (parse_arguments(in, out, weight, argc, argv)) {
        return run_test(in, out, weight);
    } else {
        print_help(argv[0]);
    }
    return 0;
}
