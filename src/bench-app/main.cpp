// Headers from other projects
#include "argument_parser.hpp"
#include "wave_reader.hpp"
#include "wave_writer.hpp"
#include "metrics.hpp"


// Headers from standard library
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <sstream>
//#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>    // std::sort

// Headers from other thz-sdk
#include <krisp-audio-sdk-nc.hpp>
#include <krisp-audio-sdk-nc-stats.hpp>
#include <krisp-audio-sdk-rt.hpp>
#include <krisp-audio-sdk-noise-db.hpp>

#define EXEC argv[0]

// Skip frames when printing logs.
#define FRAME_SKIP_COUNT   100

using namespace std;

bool gBlob                  = false;
bool gPrfLogs               = false;
bool gKrispAppMode          = false;
bool gNoiseCleaner          = true;
bool gEnergyApi             = false;
bool gRingtone              = false;
bool gRingetoneNoiseCleaner = false;
bool gInputFrameEnergy      = false;
bool gNoiseDb               = false;

void print_version()
{
    std::cout << "SDK version: ";

    KrispAudioVersionInfo versionInfo;
    int ret = krispAudioGetVersion(&versionInfo);
    if (0 != ret)
    {
        std::cout << "WARNING: Failed to get version info." << std::endl;
        return;
    }

    std::cout << versionInfo.major << "."
              << versionInfo.minor << "."
              << versionInfo.patch << "."
              << versionInfo.build << std::endl;
}

void print_help(char* e)
{
    cerr << "\nUsage:" << endl;
    cerr << "\t" << e << " [OPTION]" << endl
                << "Options:" << endl
                << "\t -i  \t Required. Input wav file to be processed, e.g. input.wav"  << endl
                << "\t -o  \t Required. Output processed wav file, e.g. output.wav" << endl
                << "\t -w  \t Required. NC weight file, e.g. weightFile.thw/kw" << endl
                << "\t -r  \t Optional. Processed output file sample rate in HZ, e.g. 8000, 16000, 32000, ... etc." << endl
                << "\t -p  \t Optional. App working path" << endl
                << "\t -rw \t Optional. Ringtone weight file, e.g. ringtoneWeightFile.thw/kw " << endl
                << "\t -nvsi \t Optional. Interval in ms for getting noise/voice stats, default value 1000, recommended range > 200" << endl;
}

template <typename T>
int error(const T& e)
{
    cerr << e <<endl;
    return 1;
}

bool parse_arguments(std::string& input, std::string& output, std::string& weight, std::string& ringtoneWeight,
    int& rate, std::string& workingPath, int &ncStatsInterval,
    int argc, char** argv)
{
    KRISP::TEST_UTILS::ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i", ARG_IMPORTANT);
    p.addArgument("--output", "-o", ARG_IMPORTANT);
    p.addArgument("--weight_file", "-w", ARG_IMPORTANT);
    p.addArgument("--rate", "-r", ARG_OPTIONAL);
    p.addArgument("--work_path", "-p", ARG_OPTIONAL);
    p.addArgument("--ringtone_weight_file", "-rw", ARG_OPTIONAL);
    p.addArgument("--nc_stats_interval", "-nvsi", ARG_OPTIONAL);

    if (p.parse()) {
        input = p.getArgument("-i");
        output = p.getArgument("-o");
        weight = p.getArgument("-w");

        // Optional arguments
        rate = ("" != p.getArgument("-r")) ? std::stoi(p.getArgument("-r")) : 0;

        ncStatsInterval = ("" != p.getArgument("-nvsi")) ? std::stoi(p.getArgument("-nvsi")) : 1000;

        ringtoneWeight = p.getArgument("-rw");
        workingPath = p.getArgument("-p");
    }
    else {
        std::cerr<<p.getError();
        return false;
    }
    return true;
}

bool readWeight(const std::string& weightFilePath, std::string& out)
{
    std::ifstream weight(weightFilePath);
    if (!weight.is_open())
    {
        cerr << "Could not open the file - '" << weightFilePath << "'" << endl;
        return false;
    }
    out = std::string((std::istreambuf_iterator<char>(weight)), std::istreambuf_iterator<char>());
    return true;
}

bool setModel(const std::string& modelFilePath, const std::string& modelName, bool blob)
{
    if (blob)
    {
        std::string weightStr;
        if (!readWeight(modelFilePath, weightStr))
        {
            return false;
        }
        const void* weightBlob = static_cast<const void*>(weightStr.c_str());
        size_t blobSize = weightStr.size();

        if (krispAudioSetModelBlob(weightBlob, blobSize, modelName.c_str()) != 0)
        {
            return false;
        }
    }
    else if (
        krispAudioSetModel(
            convertMBString2WString(modelFilePath).c_str(),
            modelName.c_str()) != 0) // not blob
    {
        return false;
    }
    return true;
}

KrispAudioSamplingRate getSampleRate(size_t val)
{
    switch (val)
    {
    case 8000:  return KRISP_AUDIO_SAMPLING_RATE_8000HZ;  break;
    case 16000: return KRISP_AUDIO_SAMPLING_RATE_16000HZ; break;
    case 24000: return KRISP_AUDIO_SAMPLING_RATE_24000HZ; break;
    case 32000: return KRISP_AUDIO_SAMPLING_RATE_32000HZ; break;
    case 44100: return KRISP_AUDIO_SAMPLING_RATE_44100HZ; break;
    case 48000: return KRISP_AUDIO_SAMPLING_RATE_48000HZ; break;
    case 88200: return KRISP_AUDIO_SAMPLING_RATE_88200HZ; break;
    case 96000: return KRISP_AUDIO_SAMPLING_RATE_96000HZ; break;
    default:
        std::cerr<<"The input wav sampling rate is not supported: " << val;
        return KRISP_AUDIO_SAMPLING_RATE_16000HZ;
        break;
    }
}

int run_test(std::string& input, std::string& output, std::string& weight, std::string& ringtoneWeight, int outRate_HZ,
    std::string& workPath, int ncStatsInterval)
{
    KRISP::TEST_UTILS::Metrics prf;
    KRISP::TEST_UTILS::WaveReader reader;
    KRISP::TEST_UTILS::WaveWriter writer;

    std::vector<short> wavShortDataIn;
    std::vector<short> wavShortDataOut;
    std::vector<short> wavShortPosRingtoneOut;
    std::vector<short> wavShortNegRingtoneOut;
    std::vector<float> wavFloatDataIn;
    std::vector<float> wavFloatDataOut;
    std::vector<float> wavFloatPosRingtoneOut;
    std::vector<float> wavFloatNegRingtoneOut;

    bool isShort = true;

    SF_INFO fileInfo;
    if (!reader.open(input.c_str(), &fileInfo))
    {
        return error("Can't open input file...");
    }
    int inRateHz = fileInfo.samplerate;
    auto formatType = fileInfo.format & SF_FORMAT_TYPEMASK;

    // Based on the input file format and format type
    // read input file content into corresponding buffer
    if (formatType & SF_FORMAT_WAV) {
        // Microsoft WAV format (little endian)
        std::cout << "input file format: SF_FORMAT_WAV" << std::endl;
        std::cout << "Subtype: 0x" << std::hex << (fileInfo.format & SF_FORMAT_SUBMASK);

        auto formatSbmask = fileInfo.format & SF_FORMAT_SUBMASK;
        if (formatSbmask == SF_FORMAT_PCM_16) {
            std::cout << " : SF_FORMAT_PCM_16" << std::endl;
            reader.read(wavShortDataIn);
        }
        else if (SF_FORMAT_FLOAT == formatSbmask) {
            std::cout << " : SF_FORMAT_FLOAT" << std::endl;
            reader.read(wavFloatDataIn);
            isShort = false;
        }
        else {
            // Other formats, currently not supported
            std::cout << "Major format: 0x" << std::hex << (fileInfo.format & SF_FORMAT_TYPEMASK) << std::endl;
            std::cout << "Subtype: 0x" << std::hex << (fileInfo.format & SF_FORMAT_SUBMASK) << std::endl;
            return error("Unsupported file format...");
        }
    }
    std::cout << std::dec;
    // reader's close() will be called in destruction point if exist to exclude memory leaks.
    // Reader no more needed, just close it here.
    reader.close();

    // If output rate doesn't provided i.e. equal to 0, then use output rate same as the input.
    outRate_HZ = (outRate_HZ) ? outRate_HZ : inRateHz;

    KrispAudioSamplingRate inRate = getSampleRate(inRateHz);
    KrispAudioSamplingRate outRate = getSampleRate(outRate_HZ);
    size_t IN_BUF_SIZE = (inRateHz * KRISP_AUDIO_FRAME_DURATION_10MS) / 1000;
    size_t OUT_BUF_SIZE = (outRate_HZ * KRISP_AUDIO_FRAME_DURATION_10MS) / 1000;

    // If optional command line argument: woring_path was provided, then pass it to the GlobalInit,
    // otherwise use default path by providing nullptr.
    int globalInitRes = 0;
    if (workPath[0] != '\0')
    {
        globalInitRes = krispAudioGlobalInit(convertMBString2WString(workPath).c_str());
    }
    else
    {
        globalInitRes = krispAudioGlobalInit(nullptr);
    }
    if (globalInitRes != 0)
    {
        return error("GLOBAL INITIALIZATION ERROR");
    }

    print_version();

    // If Ringtone API is enabled, no need to set NC model as it will not be used.
    if (!gRingtone) {
        if(!setModel(weight,"model",gBlob))
        {
            return error("SET NC MODEL ERROR");
        }
    }
    if(gRingtone || gRingetoneNoiseCleaner)
    {
        if(ringtoneWeight.empty())
        {
            return error("Ringtone model wasn't provided");
        }
        if(!setModel(ringtoneWeight,"ringtoneModel",gBlob))
        {
            return error("SET RINGTONE MODEL ERROR");
        }
    }

    KrispAudioSessionID ncSessionID = nullptr;
    KrispAudioSessionID ncWithStatsSessionID = nullptr;
    KrispAudioSessionID noiseDBSessionID = nullptr;
    KrispAudioSessionID ringtoneSessionID = nullptr;

	std::cout << "Creating session with inRate " << inRate << " outRate " << outRate << std::endl;
    prf.startTimePoint();
    if (gNoiseDb)
    {
		if (isShort)
		{
			noiseDBSessionID = krispAudioNoiseDbCreateSessionInt16(inRate, KRISP_AUDIO_FRAME_DURATION_10MS, nullptr);
		}
		else
		{
			noiseDBSessionID = krispAudioNoiseDbCreateSessionFloat(inRate, KRISP_AUDIO_FRAME_DURATION_10MS, nullptr);
		}
    }
    else if (gRingtone)
    {
		if (isShort)
		{
			ringtoneSessionID = krispAudioRingtoneCreateSessionInt16(inRate, KRISP_AUDIO_FRAME_DURATION_10MS, nullptr);
		}
		else
		{
			ringtoneSessionID = krispAudioRingtoneCreateSessionFloat(inRate, KRISP_AUDIO_FRAME_DURATION_10MS, nullptr);
		}
    }
    else if (gRingetoneNoiseCleaner)
    {
		if (isShort)
		{
			ringtoneSessionID = krispAudioRingtoneCreateSessionInt16(inRate, KRISP_AUDIO_FRAME_DURATION_10MS, "ringtoneModel");
		}
		else
		{
			ringtoneSessionID = krispAudioRingtoneCreateSessionFloat(inRate, KRISP_AUDIO_FRAME_DURATION_10MS, "ringtoneModel");
		}
		if (isShort)
		{
			ncSessionID = krispAudioNcCreateSessionInt16(inRate, outRate, KRISP_AUDIO_FRAME_DURATION_10MS, "model");
		}
		else
		{
			ncSessionID = krispAudioNcCreateSessionFloat(inRate, outRate, KRISP_AUDIO_FRAME_DURATION_10MS, "model");
		}
    }
    else if (gEnergyApi)
    {
		if (isShort)
		{
			ncWithStatsSessionID = krispAudioNcWithStatsCreateSessionInt16(inRate, outRate, KRISP_AUDIO_FRAME_DURATION_10MS, "model");
		}
		else
		{
			ncWithStatsSessionID = krispAudioNcWithStatsCreateSessionFloat(inRate, outRate, KRISP_AUDIO_FRAME_DURATION_10MS, "model");
		}
    }
    else
    {
		if (isShort)
		{
			ncSessionID = krispAudioNcCreateSessionInt16(inRate, outRate, KRISP_AUDIO_FRAME_DURATION_10MS, "model");
		}
		else
		{
			ncSessionID = krispAudioNcCreateSessionFloat(inRate, outRate, KRISP_AUDIO_FRAME_DURATION_10MS, "model");
		}
    }
    prf.endTimePoint();
    std::cout << "Session time: " << prf.getTimeInUsec() << "us" << std::endl;

    if (ncSessionID == nullptr && ncWithStatsSessionID == nullptr &&
        ringtoneSessionID == nullptr && noiseDBSessionID == nullptr)
    {
        return error("Error in creating session");
    }

    size_t inDataSizeKb = 0, outDatSizeKb = 0;
    size_t inDataSize = 0;
    if (isShort) {
        assert(9 * IN_BUF_SIZE <= wavShortDataIn.size());
        wavShortDataOut.resize((wavShortDataIn.size() * (uint64_t)OUT_BUF_SIZE) / IN_BUF_SIZE);
        if (gRingtone) {
            wavShortNegRingtoneOut.resize(wavShortDataIn.size());
            wavShortPosRingtoneOut.resize(wavShortDataIn.size());
        }

        inDataSize = wavShortDataIn.size();
        inDataSizeKb = (inDataSize * sizeof(short) / 1024);
        outDatSizeKb = (wavShortDataOut.size() * sizeof(short) / 1024);
    }
    else {
        assert(9 * IN_BUF_SIZE <= wavFloatDataIn.size());
        wavFloatDataOut.resize((wavFloatDataIn.size() * (uint64_t)OUT_BUF_SIZE) / IN_BUF_SIZE);
        if (gRingtone) {
            wavFloatNegRingtoneOut.resize(wavFloatDataIn.size());
            wavFloatPosRingtoneOut.resize(wavFloatDataIn.size());
        }

        inDataSize = wavFloatDataIn.size();
        inDataSizeKb = (inDataSize * sizeof(float) / 1024);
        outDatSizeKb = (wavFloatDataOut.size() * sizeof(float) / 1024);
    }

    //std::cout << "wavDataIn.size(): " << wavDataIn.size() << std::endl;
    //std::cout << "wavDataOut.size(): " << wavDataOut.size() << std::endl;

    size_t blockProcessedCount{ 0 };
    size_t ncProcessedCount{ 0 };
    double ncProcTime{ 0.0 };
    float ringtoneThreshold = 0.5;

    std::vector<double> cpuUsage;
    std::vector<double> memoryUsage;
    std::vector<double> timeSpent;
	//std::vector<double> sleepDeltas;
	double lastSleepDelta = 0.0;
    if (gPrfLogs)
    {
        size_t buffSize = inDataSize / (IN_BUF_SIZE * FRAME_SKIP_COUNT);
        cpuUsage.reserve(buffSize);
        memoryUsage.reserve(buffSize);
        timeSpent.reserve(buffSize);
		//sleepDeltas.reserve(buffSize);
    }

	typedef std::chrono::high_resolution_clock::time_point TimePoint;
	TimePoint beforeAudioProcessing = std::chrono::high_resolution_clock::now();
    size_t i;
    uint64_t cpuTimeBeforeAudioProcessing = prf.getCPUTimes();
    for (i = 0; (i+1)*IN_BUF_SIZE <= inDataSize; ++i )
    {
        if (gPrfLogs || gKrispAppMode)
        {
            prf.startTimePoint();
        }

        if (gInputFrameEnergy)
        {
            //int energy = 0;
            //if (isShort)
            //{
            //    energy = krispAudioGetFrameEnergyInt16(&wavShortDataIn[i * IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));
            //}
            //else
            //{
            //    energy = krispAudioGetFrameEnergyFloat(&wavFloatDataIn[i * IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));
            //}

            //std::cout << "[" << i + 1 << " x " << KRISP_AUDIO_FRAME_DURATION_10MS << "ms] " << energy << "\n";
        }
        else if (gNoiseDb)
        {
            // NoiseDB API enabled
            if ((i % 400) >= 0 && (i % 400) < 100) {
                float res = 0;
                if (isShort) {
                    res = krispAudioNoiseDbFrameInt16(noiseDBSessionID, &wavShortDataIn[i * IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));
                }
                else {
                    res = krispAudioNoiseDbFrameFloat(noiseDBSessionID, &wavFloatDataIn[i * IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));
                }

                if (!gKrispAppMode)
                    std::cout << res << std::endl;
            }
            else {
                // Reset Session before next run of noiseDB
                if ((i % 400) == 399) {
                    krispAudioNoiseDbResetSession(noiseDBSessionID);
                    std::cout << "------" << std::endl;
                }
            }
        }
        else if (gRingtone) {
            float res = 0.0;
            if (isShort) {
                res = krispAudioDetectRingtoneFrameInt16(ringtoneSessionID, &wavShortDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));

                if (res > ringtoneThreshold) {
                    std::memcpy(&wavShortPosRingtoneOut[i * IN_BUF_SIZE], &wavShortDataIn[i * IN_BUF_SIZE], IN_BUF_SIZE * sizeof(short));
                    std::memset(&wavShortNegRingtoneOut[i * IN_BUF_SIZE], 0, IN_BUF_SIZE * sizeof(short));
                }
                else {
                    std::memcpy(&wavShortNegRingtoneOut[i * IN_BUF_SIZE], &wavShortDataIn[i * IN_BUF_SIZE], IN_BUF_SIZE * sizeof(short));
                    std::memset(&wavShortPosRingtoneOut[i * IN_BUF_SIZE], 0, IN_BUF_SIZE * sizeof(short));
                }
            }
            else {
                res = krispAudioDetectRingtoneFrameFloat(ringtoneSessionID, &wavFloatDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));

                if (res > ringtoneThreshold) {
                    std::memcpy(&wavFloatPosRingtoneOut[i * IN_BUF_SIZE], &wavFloatDataIn[i * IN_BUF_SIZE], IN_BUF_SIZE * sizeof(float));
                    std::memset(&wavFloatNegRingtoneOut[i * IN_BUF_SIZE], 0, IN_BUF_SIZE * sizeof(float));
                }
                else {
                    std::memcpy(&wavFloatNegRingtoneOut[i * IN_BUF_SIZE], &wavFloatDataIn[i * IN_BUF_SIZE], IN_BUF_SIZE * sizeof(float));
                    std::memset(&wavFloatPosRingtoneOut[i * IN_BUF_SIZE], 0, IN_BUF_SIZE * sizeof(float));
                }
            }

            if (res < 0 || res > 1) {
                cout << "WARNING ringtone result is not in range [0, 1] " << endl;
            }

            std::cout << "[" << i + 1 << " x " << KRISP_AUDIO_FRAME_DURATION_10MS << "ms] " << "With result : " << res << "\n";
        }
        else if (gRingetoneNoiseCleaner) {
            float ringtoneRes = 0.0f;
            int noiseCancelRes = 0;
            bool ringtoneBool = false;

            if (isShort) {
                ringtoneRes = krispAudioDetectRingtoneFrameInt16(ringtoneSessionID, &wavShortDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));

                if (ringtoneRes > ringtoneThreshold) {
                    ringtoneBool = true;
                }

                noiseCancelRes = krispAudioNcCleanAmbientNoiseWithRingtoneInt16(ncSessionID, &wavShortDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE),
                    &wavShortDataOut[i*OUT_BUF_SIZE], static_cast<unsigned int>(OUT_BUF_SIZE), ringtoneBool);

            }
            else {
                ringtoneRes = krispAudioDetectRingtoneFrameFloat(ringtoneSessionID, &wavFloatDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE));
                if (ringtoneRes > 0.5) {
                    ringtoneBool = true;
                }

                noiseCancelRes = krispAudioNcCleanAmbientNoiseWithRingtoneFloat(ncSessionID, &wavFloatDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE),
                    &wavFloatDataOut[i*OUT_BUF_SIZE], static_cast<unsigned int>(OUT_BUF_SIZE), ringtoneBool);
            }

            if (noiseCancelRes > 0) {
                cerr<<"Error in DeNoise processing!"<<endl;
            }
        }
        else {
            if (gEnergyApi) {
                // Voice/Noise energy API enabled, so run the specified API.
                KrispAudioNcPerFrameInfo perFrameInfo;
                if (isShort) {
                    if (0 > krispAudioNcWithStatsCleanAmbientNoiseInt16(ncWithStatsSessionID, &wavShortDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE),
                        &wavShortDataOut[i*OUT_BUF_SIZE], static_cast<unsigned int>(OUT_BUF_SIZE), &perFrameInfo)) {
                        cerr << "Error in DeNoise processing!" << endl;
                        break;
                    }
                }
                else {
                    if (0 > krispAudioNcWithStatsCleanAmbientNoiseFloat(ncWithStatsSessionID, &wavFloatDataIn[i*IN_BUF_SIZE], static_cast<unsigned int>(IN_BUF_SIZE),
                        &wavFloatDataOut[i*OUT_BUF_SIZE], static_cast<unsigned int>(OUT_BUF_SIZE), &perFrameInfo)) {
                        cerr << "Error in DeNoise processing!" << endl;
                        break;
                    }
                }
                if ((perFrameInfo.noiseEnergy < 0 && perFrameInfo.noiseEnergy > 100) ||
                    (perFrameInfo.voiceEnergy < 0 && perFrameInfo.voiceEnergy > 100))
                {
                    std::cout << "### ERROR: Invalid energy value" << std::endl;
                }

                if(!gKrispAppMode)
                    std::cout << "[" << i+1 << " x " << KRISP_AUDIO_FRAME_DURATION_10MS << "ms]" <<  " noiseEn: " << perFrameInfo.noiseEnergy
                                                                            << ", voiceEn: " << perFrameInfo.voiceEnergy
                                                                            << ", csd: ";
                switch (perFrameInfo.cleanedSecondarySpeechStatus)
                {
                    case KrispAudioCleanedSecondarySpeechStatus::UNDEFINED: std::cout << "UNDEFINED"; break;
                    case KrispAudioCleanedSecondarySpeechStatus::DETECTED: std::cout << "TRUE"; break;
                    case KrispAudioCleanedSecondarySpeechStatus::NOT_DETECTED: std::cout << "FALSE"; break;
                }
                std::cout << std::endl;

                // TODO: Do we need to test the stats in the middle of the execution?
                //       The case is supported, but printing values here will break the QA validation.
                /*static int ncStatsCounter = 0;
                ncStatsCounter++;

                if (ncStatsCounter % ncStatsInterval == 0)
                {
                    KrispAudioNcStats ncStats;
                    krispAudioNcWithStatsRetrieveStats(ncWithStatsSessionID, &ncStats);

                    if (!gKrispAppMode)
                    {
                        std::cout << "### Noise/Voice stats ###" << std::endl;
                        std::cout << "# - No     Noise: " << ncStats.noiseStats.noNoiseMs << "ms" << std::endl;
                        std::cout << "# - Low    Noise: " << ncStats.noiseStats.lowNoiseMs << "ms" << std::endl;
                        std::cout << "# - Medium Noise: " << ncStats.noiseStats.mediumNoiseMs << "ms" << std::endl;
                        std::cout << "# - High   Noise: " << ncStats.noiseStats.highNoiseMs << "ms" << std::endl;
                        std::cout << "#########################" << std::endl;
                        std::cout << "# - Talk time: " << ncStats.voiceStats.talkTimeMs << " ms" << std::endl;
                    }
                }*/
            }
            else {
                // Voice/Noise energy API disabled, run the main API.
                if (isShort) {
					auto result = krispAudioNcCleanAmbientNoiseInt16(
							ncSessionID,
							&wavShortDataIn[i*IN_BUF_SIZE],
							static_cast<unsigned int>(IN_BUF_SIZE),
							&wavShortDataOut[i*OUT_BUF_SIZE],
							static_cast<unsigned int>(OUT_BUF_SIZE));
                    if (0 > result) {
                        cerr << "Error in DeNoise processing!" << endl;
                        break;
                    }
                }
                else {
					auto result = krispAudioNcCleanAmbientNoiseFloat(
							ncSessionID,
							&wavFloatDataIn[i*IN_BUF_SIZE],
							static_cast<unsigned int>(IN_BUF_SIZE),
							&wavFloatDataOut[i*OUT_BUF_SIZE],
							static_cast<unsigned int>(OUT_BUF_SIZE));
                    if (0 > result) {
                        cerr << "Error in DeNoise processing!" << endl;
                        break;
                    }
                }
            }
        }

        // Print performance logs if performance profiling is enabled.

        if (gPrfLogs || gKrispAppMode) {
            prf.endTimePoint();

            if (gKrispAppMode)
            {
                double frameProcDur = prf.getTimeDurationMilli();
                double sleepDuration = KRISP_AUDIO_FRAME_DURATION_10MS - frameProcDur;
                if(sleepDuration <= 0) {
					std::cout << "WARNING: Frame processing time exits limits: "
						<< frameProcDur << " ms" << std::endl;
				}
				sleepDuration -= lastSleepDelta;
				if (sleepDuration > 0.0) {
					TimePoint beforeSleep = std::chrono::high_resolution_clock::now();
					prf.sleepMs(sleepDuration);
					TimePoint afterSleep = std::chrono::high_resolution_clock::now();
					std::chrono::duration<double, std::milli> actualSleepTime = afterSleep - beforeSleep;
					double sleepDelta = actualSleepTime.count() - sleepDuration;
					//sleepDeltas.push_back(sleepDelta);
					lastSleepDelta = sleepDelta;
				}
				else {
					lastSleepDelta = -sleepDuration;
				}
            }

            if (gPrfLogs) {
                ncProcTime += prf.getTimeDurationMilli();
                ncProcessedCount++;

                if (!(++blockProcessedCount % FRAME_SKIP_COUNT)) {
                    cpuUsage.push_back(prf.getCPUUsageOnCurrentProcess());
                    timeSpent.push_back(ncProcTime);
                    memoryUsage.push_back(prf.getCurrentProcessMemory() - (inDataSizeKb + outDatSizeKb));

                    std::cout << "Processed " << blockProcessedCount
                        << " blocks, calls: " << ncProcessedCount
                        << ", time spent: " << std::setprecision(3) << std::fixed << timeSpent.back() << " [ms]"
                        << ", CPU usage: " << cpuUsage.back() << " %"
                        << ", Memory usage: " << (int)memoryUsage.back() << " Kb" << std::endl;
                    ncProcTime = 0;
                }
            }
        }
    }

    uint64_t cpuTimeAfterAudioProcessing = prf.getCPUTimes();
    uint64_t cpuTimeAudioProcessingUSec = cpuTimeAfterAudioProcessing - cpuTimeBeforeAudioProcessing;
    TimePoint afterAudioProcessing = std::chrono::high_resolution_clock::now();

    std::cout << "cpu time before: " << cpuTimeBeforeAudioProcessing
              << "\ncpu time after: " << cpuTimeAfterAudioProcessing
              << "\ncpu time diff:" << cpuTimeAudioProcessingUSec
              << std::endl;

    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(
		afterAudioProcessing - beforeAudioProcessing);

    std::cout << "\ntime diff: " << processingTime.count() << std::endl;

    std::cout << "CPU %: " <<
		static_cast<double>(cpuTimeAudioProcessingUSec) * 100 / processingTime.count() << std::endl;

	std::chrono::duration<double> procesingTime = afterAudioProcessing - beforeAudioProcessing;
	std::cout << "MY PROCESING TIME SECONDS: " << procesingTime.count() << std::endl;

    if (gPrfLogs)
    {
        cpuUsage.erase(cpuUsage.begin(), cpuUsage.begin() + 2);
        auto sum = std::accumulate(cpuUsage.begin(), cpuUsage.end(), 0.0);
        auto cpuUsageAvg = sum / cpuUsage.size();
        std::sort(cpuUsage.begin(), cpuUsage.end());
        auto cpuUsageMin = cpuUsage.front();
        auto cpuUsageMax = cpuUsage.back();

        sum = std::accumulate(memoryUsage.begin(), memoryUsage.end(), 0.0);
        auto memoryUsageAvg = sum / memoryUsage.size();
        std::sort(memoryUsage.begin(), memoryUsage.end());
        auto memoryUsageMin = memoryUsage.front();
        auto memoryUsageMax = memoryUsage.back();

        sum = std::accumulate(timeSpent.begin(), timeSpent.end(), 0.0);
        auto timeSpentAvg = sum / timeSpent.size();
        std::sort(timeSpent.begin(), timeSpent.end());
		// TODO: it's not smart to sort array to find min and max point.
        auto timeSpentMin = timeSpent.front();
        auto timeSpentMax = timeSpent.back();

        std::cout << "-----------------------------------------------------------------------------------------------------------" << std::endl;
        std::cout << "AVG: Time spent: "  << std::setprecision(3) << std::fixed << timeSpentAvg << " [ms]"
                  << ", CPU usage: " << cpuUsageAvg << " %"
                  << ", Memory usage: " << (int)memoryUsageAvg << " Kb" << std::endl;
        std::cout << "MIN: Time spent: " << std::setprecision(3) << std::fixed << timeSpentMin << " [ms]"
            << ", CPU usage: " << cpuUsageMin << " %"
            << ", Memory usage: " << (int)memoryUsageMin << " Kb" << std::endl;

        std::cout << "MAX: Time spent: " << std::setprecision(3) << std::fixed << timeSpentMax << " [ms]"
            << ", CPU usage: " << cpuUsageMax << " %"
            << ", Memory usage: " << (int)memoryUsageMax << " Kb" << std::endl;
        std::cout << "-----------------------------------------------------------------------------------------------------------" << std::endl;

		//for (double sleepDelta : sleepDeltas) {
		//	std::cout << sleepDelta << " ";
		//}
		//std::cout << std::endl;
    }

    if (isShort) {
        wavShortDataOut.resize(i*OUT_BUF_SIZE);
    }
    else {
        wavFloatDataOut.resize(i*OUT_BUF_SIZE);
    }

    if (gNoiseDb) {
        if (0 != krispAudioNoiseDbCloseSession(noiseDBSessionID))
            return error("Error in closing instance");
    }
    else if (gRingtone) {
        if (0 != krispAudioRingtoneCloseSession(ringtoneSessionID)) {
            return error("Error in closing ringtone session.");
        }
    }
    else if (gRingetoneNoiseCleaner) {
        if (0 != krispAudioNcCloseSession(ncSessionID) && 0 != krispAudioRingtoneCloseSession(ringtoneSessionID)) {
            return error("Error in closing instance");
        }
    }
    else if (gEnergyApi) {
        KrispAudioNcStats ncStats;
        std::memset(&ncStats, 0, sizeof(KrispAudioNcStats));

        if(0 == krispAudioNcWithStatsRetrieveStats(ncWithStatsSessionID, &ncStats))
        {
            //if (!gKrispAppMode)
            {
                std::cout << "#--- Noise/Voice stats ---" << std::endl;
                std::cout << "# - No     Noise: " << ncStats.noiseStats.noNoiseMs << " ms" << std::endl;
                std::cout << "# - Low    Noise: " << ncStats.noiseStats.lowNoiseMs << " ms" << std::endl;
                std::cout << "# - Medium Noise: " << ncStats.noiseStats.mediumNoiseMs << " ms" << std::endl;
                std::cout << "# - High   Noise: " << ncStats.noiseStats.highNoiseMs << " ms" << std::endl;
                std::cout << "#-------------------------" << std::endl;
                std::cout << "# - Talk time :   " << ncStats.voiceStats.talkTimeMs << " ms" << std::endl;
                std::cout << "#-------------------------" << std::endl;
            }
        }

        if (0 != krispAudioNcWithStatsCloseSession(ncWithStatsSessionID)) {
            return error("Error in closing instance");
        }
    }
    else {
        if (0 != krispAudioNcCloseSession(ncSessionID))
            return error("Error in closing instance");
    }

    ncSessionID = nullptr;
    noiseDBSessionID = nullptr;
    ringtoneSessionID = nullptr;

    if (0 != krispAudioGlobalDestroy())
        return error("Error in closing ALL");

    if (gRingtone) {
        KRISP::TEST_UTILS::WaveWriter writer;

        const std::string posOut("RingtonePositive.wav");
        const std::string negOut("RingtoneNegative.wav");

        if (isShort) {
            writer.write(posOut.c_str(), wavShortPosRingtoneOut, inRateHz);
            writer.write(negOut.c_str(), wavShortNegRingtoneOut, inRateHz);
        }
        else {
            writer.write(posOut.c_str(), wavFloatPosRingtoneOut, inRateHz);
            writer.write(negOut.c_str(), wavFloatNegRingtoneOut, inRateHz);
        }
    }

    if (!gNoiseDb && !gRingtone) {
        if (isShort) {
            writer.write(output.c_str(), wavShortDataOut, outRate_HZ);
        }
        else {
            writer.write(output.c_str(), wavFloatDataOut, outRate_HZ);
        }
    }

    return 0;
}

enum MENU_ITEMS
{
    MENU_RUN_TEST = 0,
    MENU_ENABLE_BLOB,
    MENU_PERFORMANCE_PRF,
    MENU_KRISP_APP_MODE,
    MENU_NOISE_CANCELER,
    MENU_VOICE_NOISE_ENERGY,
    MENU_NOISE_DB,
    MENU_RINGTONE,
    MENU_RINGTONE_NOISE_CANCELLER,
    MENU_INPUT_FRAME_ENERGY
};

const std::vector<std::string> mainMenuOptions =
{
    std::string("* Choose one of the test functionalities by selecting specified option"),
    std::string("* Performance profiling may be enabled with test functionalities"),
    std::string("*"),
    std::string("[" + std::to_string(MENU_RUN_TEST)                 + "] Run test"),
    std::string("[" + std::to_string(MENU_ENABLE_BLOB)              + "] Enable blob "),
    std::string("[" + std::to_string(MENU_PERFORMANCE_PRF)          + "] Performance profiling (optional)"),
    std::string("[" + std::to_string(MENU_KRISP_APP_MODE)           + "] Krisp app mode        (optional)"),
    std::string("-----------------------------"),
    std::string("* Choose test functionality to be enabled:"),
    std::string("[" + std::to_string(MENU_NOISE_CANCELER)           + "] Noise canceler API (default)"),
    std::string("[" + std::to_string(MENU_VOICE_NOISE_ENERGY)       + "] Voice/Noise energy with call stats API"),
    std::string("[" + std::to_string(MENU_NOISE_DB)                 + "] NoiseDB API"),
    std::string("[" + std::to_string(MENU_RINGTONE)                 + "] Ringtone detection API"),
    std::string("[" + std::to_string(MENU_RINGTONE_NOISE_CANCELLER) + "] Ringtone detection with Noise canceler API"),
    std::string("[" + std::to_string(MENU_INPUT_FRAME_ENERGY)       + "] Input Frame Energy API"),
};

void functionalityMenuHandler()
{
    int consoleInput = 0;

    std::cout << "**---------------------------------**" << std::endl;
    std::cout << "**   Krisp Audio Test Application  **" << std::endl;
    std::cout << "**---------------------------------**" << std::endl;
    for (auto menuItem : mainMenuOptions)
    {
        std::cout << menuItem << std::endl;
    }
    std::cout << std::string(
                     "Select [" + std::to_string(MENU_PERFORMANCE_PRF) + ", " +
                     std::to_string(MENU_INPUT_FRAME_ENERGY) +
                     "] option to enable the specified functionality or " +
                     std::to_string(MENU_ENABLE_BLOB) +
                     "] option to pass weight data as blob pointer" + "[" +
                     std::to_string(MENU_RUN_TEST) + "] to run test")
              << std::endl;

    do
    {
        std::cin >> consoleInput;
        switch (consoleInput)
        {
            case MENU_RUN_TEST:
            {
                std::cout << "Run test-->" << std::endl;
                break;
            }
            case MENU_ENABLE_BLOB:
            {
                gBlob = !gBlob;
                std::cout << "Blob: " << (gBlob ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_PERFORMANCE_PRF:
            {
                gPrfLogs = (gPrfLogs ? false : true);
                std::cout << "Performance profiling: " << (gPrfLogs ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_NOISE_CANCELER:
            {
                gEnergyApi    = false;
                gKrispAppMode = false;
                gNoiseDb      = false;

                std::cout << "Noise canceler enabled" << std::endl;
                break;
            }
            case MENU_VOICE_NOISE_ENERGY:
            {
                if (gNoiseDb)
                {
                    std::cout << "Warning: <Voice/Noise energy> incompatible with <NoiseDb>, disable one of them..." << std::endl;
                    continue;
                }

                gEnergyApi = (gEnergyApi ? false : true);
                std::cout << "Voice/Noise energy: " << (gEnergyApi ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_NOISE_DB:
            {
                if (gEnergyApi)
                {
                    std::cout << "Warning: <NoiseDb> incompatible with <Voice/Noise energy>, disable one of them..." << std::endl;
                    continue;
                }
                gNoiseDb = (gNoiseDb ? false : true);
                std::cout << "NoiseDb: " << (gNoiseDb ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_KRISP_APP_MODE:
            {
                gKrispAppMode = (gKrispAppMode ? false : true);
                std::cout << "Krisp app mode: " << (gKrispAppMode ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_RINGTONE:
            {
                gRingtone = (gRingtone ? false : true);
                std::cout << "Ringtone detection: " << (gRingtone ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_RINGTONE_NOISE_CANCELLER:
            {
                gRingetoneNoiseCleaner = (gRingetoneNoiseCleaner ? false : true);
                std::cout << "Ringetone detection with noise cleaning: " << (gRingetoneNoiseCleaner ? "enabled" : "disabled") << std::endl;
                break;
            }
            case MENU_INPUT_FRAME_ENERGY:
            {
                gInputFrameEnergy = (gInputFrameEnergy ? false : true);
                std::cout << "Input frame energy: " << (gRingetoneNoiseCleaner ? "enabled" : "disabled") << std::endl;
                break;
            }
        };

        std::cout << std::string( "Select [" + std::to_string(MENU_PERFORMANCE_PRF) + ", " + std::to_string(MENU_INPUT_FRAME_ENERGY) + "] option to enable the specified functionality or " + "[" + std::to_string(MENU_RUN_TEST) + "] to run test") << std::endl;
    } while (MENU_RUN_TEST != consoleInput);
}

//using namespace InferenceEngine;
int main(int argc, char** argv)
{
    std::string in, out, cont, workPath, ringtoneWeight;
    int rate = 0;
    int ncStatsInterval = 1000;

    if (parse_arguments(in, out, cont, ringtoneWeight, rate, workPath, ncStatsInterval, argc, argv)) {
        int ret = 0;

        functionalityMenuHandler();

        ret = run_test(in, out, cont, ringtoneWeight, rate, workPath, ncStatsInterval);

        std::cout << "Test " << ((0 == ret) ? "completed successfully, result: " : "failed with result: ") << ret << std::endl;

        return ret;
    } else {
        print_help(EXEC);
    }
    return 0;
}
