#include <algorithm>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <locale>
#include <codecvt>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>

#ifdef ENABLE_ACCENT
#include <krisp-audio-sdk-ar.hpp>
#endif

#include "common.hpp"
#include "argument_parser.hpp"

namespace {

constexpr size_t FRAME_DURATION_MS = 10;
constexpr size_t PROGRESS_INTERVAL = 1000;
constexpr double INT16_SCALE = 32768.0;
constexpr double INT32_SCALE = 2147483648.0;
constexpr double INT16_MAX_ = 32767.0;
constexpr double INT32_MAX_ = 2147483647.0;

template<typename PcmType>
float pcmToFloat(PcmType sample);

template<>
float pcmToFloat<int16_t>(int16_t sample) {
    return float(double(sample) / INT16_SCALE);
}

template<>
float pcmToFloat<int32_t>(int32_t sample) {
    return float(double(sample) / INT32_SCALE);
}

template<>
float pcmToFloat<float>(float sample) {
    return sample;
}

template<typename PcmType>
PcmType floatToPcm(float sample);

template<>
int16_t floatToPcm<int16_t>(float sample) {
    double d = double(sample);
    d = std::max(-1.0, std::min(1.0, d));
    return int16_t(d * INT16_MAX_);
}

template<>
int32_t floatToPcm<int32_t>(float sample) {
    double d = double(sample);
    d = std::max(-1.0, std::min(1.0, d));
    return int32_t(d * INT32_MAX_);
}

template<>
float floatToPcm<float>(float sample) {
    return sample;
}

uint32_t readLE32(const uint8_t* p) {
    return uint32_t(p[0])
            | (uint32_t(p[1]) << 8)
            | (uint32_t(p[2]) << 16)
            | (uint32_t(p[3]) << 24);
}
void writeLE32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t( v        & 0xFF);
    p[1] = uint8_t((v >>  8) & 0xFF);
    p[2] = uint8_t((v >> 16) & 0xFF);
    p[3] = uint8_t((v >> 24) & 0xFF);
}

struct ChunkData {
    std::vector<uint8_t> headerAndBeforeData;
    uint32_t             dataSizeOffset = 0;
    std::vector<uint8_t> trailer;
};

struct Wav {
    ChunkData chunks;
    uint16_t fmtAudioFormat = 0; // 1 = PCM16, 3 = FLOAT32
    uint16_t fmtChannels = 0;
    uint16_t fmtBitDepth = 0; // bits per sample
    uint32_t fmtSampleRate = 0;
    std::vector<int16_t> samples16;
    std::vector<int32_t> samples32;
    std::vector<float> samplesF;
};

class UnsupportedFormatError : public std::runtime_error {
public:
    UnsupportedFormatError(uint16_t fmtAudioFormat, uint16_t fmtBitDepth)
        : std::runtime_error(buildErrorMessage(fmtAudioFormat, fmtBitDepth)) {}

private:
    static std::string buildErrorMessage(uint16_t fmtAudioFormat, uint16_t fmtBitDepth) {
        std::string formatName;
        if (fmtAudioFormat == 1) {
            formatName = "PCM";
        } else if (fmtAudioFormat == 3) {
            formatName = "IEEE Float";
        } else {
            formatName = "Unknown format " + std::to_string(fmtAudioFormat);
        }
        
        return "Unsupported audio format: " + formatName + 
                " with " + std::to_string(fmtBitDepth) + " bits per sample. " +
                "Supported formats: PCM 16-bit, PCM 32-bit, IEEE Float 32-bit.";
    }
};

Wav readWav(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open \"" + path + "\"");
    }
    uint8_t riffHeader[12];
    in.read((char*)riffHeader, 12);
    if (in.gcount() != 12
        || std::memcmp(riffHeader, "RIFF", 4)
        || std::memcmp(riffHeader+8, "WAVE", 4)) {
        throw std::runtime_error("Invalid WAV");
        }

    Wav w;
    w.chunks.headerAndBeforeData.insert(
        w.chunks.headerAndBeforeData.end(), riffHeader, riffHeader+12);

    bool foundData = false;

    // Walk chunks
    while (true) {
        uint8_t id[4], sizeLE[4];
        in.read((char*)id, 4);
        if (in.gcount() == 0) {
            break;
        }
        if (in.gcount() != 4) throw std::runtime_error("Unexpected EOF (chunk id)");
        in.read((char*)sizeLE, 4);
        if (in.gcount() != 4) throw std::runtime_error("Unexpected EOF (chunk size)");

        uint32_t sz = readLE32(sizeLE);

        if (std::memcmp(id, "data", 4) == 0) {
            foundData = true;
            // Append header and size to headerAndBeforeData
            size_t base = w.chunks.headerAndBeforeData.size();
            w.chunks.headerAndBeforeData.insert(
                w.chunks.headerAndBeforeData.end(), id, id+4);
            w.chunks.headerAndBeforeData.insert(
                w.chunks.headerAndBeforeData.end(), sizeLE, sizeLE+4);
            w.chunks.dataSizeOffset = uint32_t(base + 4);

            // Read audio data into buffer
            if (w.fmtAudioFormat == 1 && w.fmtBitDepth == 16) {
                w.samples16.resize(sz / 2);
                in.read((char*)w.samples16.data(), sz);
            } else if (w.fmtAudioFormat == 1 && w.fmtBitDepth == 32) {
                w.samples32.resize(sz / 4);
                in.read((char*)w.samples32.data(), sz);
            } else if (w.fmtAudioFormat == 3 && w.fmtBitDepth == 32) {
                w.samplesF.resize(sz / 4);
                in.read((char*)w.samplesF.data(), sz);
            } else {
                throw UnsupportedFormatError(w.fmtAudioFormat, w.fmtBitDepth);
            }
            // Skip padding if present
            if (sz & 1) in.get();
            continue;
        }

        // For all other chunks
        if (!foundData) {
            // Before data chunk - append to headerAndBeforeData
            w.chunks.headerAndBeforeData.insert(
                w.chunks.headerAndBeforeData.end(), id, id+4);
            w.chunks.headerAndBeforeData.insert(
                w.chunks.headerAndBeforeData.end(), sizeLE, sizeLE+4);
            size_t toRead = sz + (sz & 1);
            std::vector<uint8_t> buf(toRead);
            in.read((char*)buf.data(), toRead);
            w.chunks.headerAndBeforeData.insert(
                w.chunks.headerAndBeforeData.end(), buf.begin(), buf.end());

            // parse "fmt " for channels, bitDepth, format
            if (std::memcmp(id, "fmt ", 4) == 0) {
                w.fmtAudioFormat = uint16_t(buf[0] | (buf[1]<<8));
                w.fmtChannels = uint16_t(buf[2] | (buf[3]<<8));
                w.fmtSampleRate = uint32_t(readLE32(&buf[4]));
                w.fmtBitDepth = uint16_t(buf[14] | (buf[15]<<8));
            }
        } else {
            // After data chunk - append to trailer
            w.chunks.trailer.insert(
                w.chunks.trailer.end(), id, id+4);
            w.chunks.trailer.insert(
                w.chunks.trailer.end(), sizeLE, sizeLE+4);
            size_t toRead = sz + (sz & 1);
            std::vector<uint8_t> buf(toRead);
            in.read((char*)buf.data(), toRead);
            w.chunks.trailer.insert(
                w.chunks.trailer.end(), buf.begin(), buf.end());
        }
    }

    return w;
}

uint32_t getWavDataSize(const Wav& w) {
    switch (w.fmtAudioFormat) {
        case 1: // PCM
            switch (w.fmtBitDepth) {
                case 16: // PCM16
                    return uint32_t(w.samples16.size() * sizeof(int16_t));
                case 32: // PCM32
                    return uint32_t(w.samples32.size() * sizeof(int32_t));
            }
            break;
        case 3: // PCM FLOAT
            switch (w.fmtBitDepth) {
                case 32: // PCM FLOAT (32 bit)
                    return uint32_t(w.samplesF.size() * sizeof(float));
            }
            break;
    }   
    throw UnsupportedFormatError(w.fmtAudioFormat, w.fmtBitDepth);
}

void writeWav(const std::string& path, Wav& w) {
    uint32_t dataBytes = getWavDataSize(w);
    
    // patch "data" chunk size
    writeLE32(&w.chunks.headerAndBeforeData[w.chunks.dataSizeOffset],
                dataBytes);

    // patch RIFF size = fileSize-8
    uint32_t riffSize = uint32_t(
        w.chunks.headerAndBeforeData.size() - 8
        + dataBytes
        + w.chunks.trailer.size());
    writeLE32(&w.chunks.headerAndBeforeData[4], riffSize);

    // write out
    std::ofstream out(path, std::ios::binary);
    out.write((char*)w.chunks.headerAndBeforeData.data(),
                w.chunks.headerAndBeforeData.size());
    if (w.fmtAudioFormat == 1 && w.fmtBitDepth == 16) {
        // PCM16
        out.write((char*)w.samples16.data(), dataBytes);
    }
    else if (w.fmtAudioFormat == 1 && w.fmtBitDepth == 32) {
        // PCM32
        out.write((char*)w.samples32.data(), dataBytes);
    }
    else if (w.fmtAudioFormat == 3 && w.fmtBitDepth == 32) {
        // PCM FLOAT (32 bit)
        out.write((char*)w.samplesF .data(), dataBytes);
    }
    else {
        throw UnsupportedFormatError(w.fmtAudioFormat, w.fmtBitDepth);
    }
    if (!w.chunks.trailer.empty())
        out.write((char*)w.chunks.trailer.data(),
                    w.chunks.trailer.size());
}

template<typename SessionConfig>
struct SessionTraits;

// Specialization for NcSessionConfig
template<>
struct SessionTraits<Krisp::AudioSdk::NcSessionConfig> {
    using SessionType = Krisp::AudioSdk::Nc<float>;
};

#ifdef ENABLE_ACCENT
// Specialization for ArSessionConfig
template<>
struct SessionTraits<Krisp::AudioSdk::ArSessionConfig> {
    using SessionType = Krisp::AudioSdk::Ar<float>;
};
#endif

void processFrameImpl(std::shared_ptr<Krisp::AudioSdk::Nc<float>> & session, const float * inFrameSamples, float * outFrameSamples, size_t frameSize, float noiseSuppressionLevel) {
    session->process(
        inFrameSamples,
        frameSize,
        outFrameSamples,
        frameSize,
        noiseSuppressionLevel);
}

#ifdef ENABLE_ACCENT
void processFrameImpl(std::shared_ptr<Krisp::AudioSdk::Ar<float>> & session, const float * inFrameSamples, float * outFrameSamples, size_t frameSize, float) {
    // ArSessionConfig ignores noiseSuppressionLevel
    session->process(
        inFrameSamples,
        frameSize,
        outFrameSamples,
        frameSize);
}
#endif

// Generic processFrame that works with any session type
template <typename SampleType, typename SessionType>
void processFrame(std::shared_ptr<SessionType> & session, const SampleType * inFrameSamples, SampleType * outFrameSamples, size_t frameSize, float noiseSuppressionLevel = 100.0f) {
    processFrameImpl(session, inFrameSamples, outFrameSamples, frameSize, noiseSuppressionLevel);
}

template<typename SessionConfig>
void processMultiChannelAudioFloat(std::vector<float> & samples, size_t frameSize, size_t numChannels, SessionConfig& cfg, float noiseSuppressionLevel) {
    if (!frameSize) {
        throw std::runtime_error("Frame size is 0");
    }
    
    size_t sampleCount = samples.size() / numChannels;
    auto data = samples.data();
    
    size_t frameCount = sampleCount / frameSize;
    std::vector<float> frameSamples(frameSize);
    std::vector<float> inputFrame(frameSize);
    
    std::cout << "Processing " << numChannels << " channels with " << frameCount << " frames each..." << std::endl;
    
    for (size_t channel = 0; channel < numChannels; ++channel) {
        std::cout << "Processing channel " << (channel + 1) << "/" << numChannels << "..." << std::endl;
        
        std::shared_ptr<typename SessionTraits<SessionConfig>::SessionType> session = 
            SessionTraits<SessionConfig>::SessionType::create(cfg);
        size_t frame = 0;
        for (; frame < frameCount; ++frame) {
            if (frame % PROGRESS_INTERVAL == 0 && frame > 0) {
                std::cout << "\r  Channel " << (channel + 1) << ": processed " << frame << "/" << frameCount << " frames (" 
                            << (frame * 100 / frameCount) << "%)" << std::flush;
            }
            
            // Extract frame for this channel into the frameSamples buffer
            for (size_t frameSample = 0; frameSample < frameSize; ++frameSample) {
                frameSamples[frameSample] = data[(frame * frameSize + frameSample) * numChannels + channel];
            }
            // Copy the buffer
            std::copy(frameSamples.begin(), frameSamples.end(), inputFrame.begin());
            // Process the frame
            processFrame(session, inputFrame.data(), frameSamples.data(), frameSize);
            // Write the processed frame back to the audio buffer for this channel
            for (size_t frameSample = 0; frameSample < frameSize; ++frameSample) {
                data[(frame * frameSize + frameSample) * numChannels + channel] = frameSamples[frameSample];
            }
        }
        
        // Handle the partial frame at the end of the file
        if (sampleCount % frameSize) {
            // Extract the partial frame into the frameSamples buffer
            for (size_t frameSample = 0; frameSample < (sampleCount % frameSize); ++frameSample) {
                frameSamples[frameSample] = data[(frame * frameSize + frameSample) * numChannels + channel];
            }
            // Fill the rest of the buffer with zeros
            for (size_t frameSample = sampleCount % frameSize; frameSample < frameSize; ++frameSample) {
                frameSamples[frameSample] = 0.0f;
            }
            // Copy the buffer
            std::copy(frameSamples.begin(), frameSamples.end(), inputFrame.begin());
            // Process the frame
            processFrame(session, inputFrame.data(), frameSamples.data(), frameSize, noiseSuppressionLevel);
            // Write the processed frame back to the audio buffer for this channel
            for (size_t frameSample = 0; frameSample < (sampleCount % frameSize); ++frameSample) {
                data[(frame * frameSize + frameSample) * numChannels + channel] = frameSamples[frameSample];
            }
        }
        
        // Show completion for this channel - clear the line and show final status
        std::cout << "\r  Channel " << (channel + 1) << ": completed " << frameCount << " frames (100%)    " << std::endl;
    }
    
    std::cout << "Completed processing all " << numChannels << " channels." << std::endl;
}

// Specialization for PCM types (PCM16 and PCM32)
template<typename PcmType, typename SessionConfig>
void processMultiChannelAudio(std::vector<PcmType> & samples, size_t frameSize, size_t numChannels, SessionConfig& config, float noiseSuppressionLevel) {
    if (!frameSize) {
        throw std::runtime_error("Frame size is 0");
    }
    
    std::vector<float> floatSamples(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        floatSamples[i] = pcmToFloat(samples[i]);
    }
    
    processMultiChannelAudioFloat(floatSamples, frameSize, numChannels, config, noiseSuppressionLevel);
    
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = floatToPcm<PcmType>(floatSamples[i]);
    }
}

// Specialization for PCM FLOAT (32 bit)
template<typename SessionConfig>
void processMultiChannelAudio(std::vector<float> & samples, size_t frameSize, size_t numChannels, SessionConfig& ncCfg, float noiseSuppressionLevel) {
    processMultiChannelAudioFloat(samples, frameSize, numChannels, ncCfg, noiseSuppressionLevel);
}


template<typename SessionConfig>
void processWavData(Wav& w, SessionConfig& ncCfg, float noiseSuppressionLevel = 100.0f) {
    size_t frameSize = w.fmtSampleRate * FRAME_DURATION_MS / 1000;
    
    switch (w.fmtAudioFormat) {
        case 1: // PCM
            switch (w.fmtBitDepth) {
                case 16: // PCM16
                    processMultiChannelAudio(w.samples16, frameSize, w.fmtChannels, ncCfg, noiseSuppressionLevel);
                    return;
                case 32: // PCM32
                    processMultiChannelAudio(w.samples32, frameSize, w.fmtChannels, ncCfg, noiseSuppressionLevel);
                    return;
            }
            break;
        case 3: // PCM FLOAT
            switch (w.fmtBitDepth) {
                case 32: // PCM FLOAT (32 bit)
                    processMultiChannelAudio(w.samplesF, frameSize, w.fmtChannels, ncCfg, noiseSuppressionLevel);
                    return;
            }
            break;
    }
    throw UnsupportedFormatError(w.fmtAudioFormat, w.fmtBitDepth);
}

[[nodiscard]] static bool parseArguments(std::string &input, std::string &output,
                            std::string &weight, float &noiseSuppressionLevel, int argc, char **argv)
{
    ArgumentParser p(argc, argv);
    p.addArgument("--input", "-i", IMPORTANT);
    p.addArgument("--output", "-o", IMPORTANT);
    p.addArgument("--model_path", "-m", IMPORTANT);
    p.addArgument("--suppress_level", "-sl", OPTIONAL_VALUE);
    if (p.parse())
    {
        input = p.getArgument("-i");
        output = p.getArgument("-o");
        weight = p.getArgument("-m");
        std::string noiseSuppressionLevelStr;
        if (p.getOptionalArgumentValue("-sl", noiseSuppressionLevelStr))
        {
            noiseSuppressionLevel = std::stof(noiseSuppressionLevelStr);
        }
    }
    else
    {
        std::cerr << p.getError();
        return false;
    }
    return true;
}

bool detectAccent(const std::string& modelPath) {
    const std::string pathSeparators = "/\\";
    size_t lastSeparatorPos = modelPath.find_last_of(pathSeparators);
    std::string fileName;
    if (lastSeparatorPos != std::string::npos) {
        fileName = modelPath.substr(lastSeparatorPos + 1);
    } else {
        fileName = modelPath;
    }
    const std::string accentPrefix = "ar_";
    const size_t prefixLength = accentPrefix.length();
    bool isAccentModel = (fileName.length() >= prefixLength) && 
                        (fileName.substr(0, prefixLength) == accentPrefix);
    return isAccentModel;
}

#ifdef ENABLE_ACCENT
Krisp::AudioSdk::ArSessionConfig createArConfig(Krisp::AudioSdk::ModelInfo& modelInfo, Krisp::AudioSdk::SamplingRate rate) {
    Krisp::AudioSdk::ArSessionConfig arCfg =
    {
        rate,
        Krisp::AudioSdk::FrameDuration::Fd10ms,
        rate,
        &modelInfo
    };
    return arCfg;
}
#endif

Krisp::AudioSdk::NcSessionConfig createNcConfig(Krisp::AudioSdk::ModelInfo& modelInfo, Krisp::AudioSdk::SamplingRate rate) {
    Krisp::AudioSdk::NcSessionConfig ncCfg = {
        rate,
        Krisp::AudioSdk::FrameDuration::Fd10ms,
        rate,
        &modelInfo,
        false,
        nullptr
    };
    return ncCfg;
}
}

int main(int argc, char* argv[]) {
    std::string in;
    std::string out;
    std::string model;
    float noiseSuppressionLevel = 100.0;
    if (!parseArguments(in,out, model, noiseSuppressionLevel,argc,argv))
    {
        std::cerr << "\nUsage:\n\t" << argv[0] << " -i input.wav -o output.wav -m model_path" << std::endl;
        return -1;
    }
    try {
        auto wav = readWav(in);
        
        std::string formatName;
        if (wav.fmtAudioFormat == 1) {
            formatName = "PCM";
        } else if (wav.fmtAudioFormat == 3) {
            formatName = "IEEE Float";
        } else {
            formatName = "Unknown (" + std::to_string(wav.fmtAudioFormat) + ")";
        }
        
        std::cout << "Format: " << formatName 
                  << "   Channels: " << wav.fmtChannels
                  << "   Bits/sample: " << wav.fmtBitDepth 
                  << "   Sample rate: " << wav.fmtSampleRate << " Hz\n";

        Krisp::AudioSdk::globalInit(L"");

        std::wstring_convert<std::codecvt_utf8<wchar_t>> wstringConverter;
        Krisp::AudioSdk::ModelInfo modelInfo;
        modelInfo.path = wstringConverter.from_bytes(model);

        auto samplingRateResult = getKrispSamplingRate(wav.fmtSampleRate);
        if (!samplingRateResult.second)
        {
            return error("Unsupported sample rate");
        }
#ifdef ENABLE_ACCENT
        if (detectAccent(model)) {
            auto arCfg = createArConfig(modelInfo, samplingRateResult.first);
            processWavData(wav, arCfg);
        }
        else {
#endif
            auto ncCfg = createNcConfig(modelInfo, samplingRateResult.first);
            processWavData(wav, ncCfg, noiseSuppressionLevel);
#ifdef ENABLE_ACCENT
        }
#endif
        Krisp::AudioSdk::globalDestroy();
        writeWav(out, wav);

        std::cout << "Done. All metadata (track names, channel chunks) preserved.\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
