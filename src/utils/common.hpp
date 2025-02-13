#pragma once

#include <iostream>
#include "krisp-audio-api-definitions.hpp"
#include "sound_file.hpp"

using namespace Krisp::AudioSdk;

template <typename T>
int error(const T &e)
{
    std::cerr << e << std::endl;
    return 1;
}

static std::pair<SamplingRate, bool> getKrispSamplingRate(uint32_t rate)
{
    std::pair<SamplingRate, bool> result;
    result.second = true;
    switch (rate)
    {
    case 8000:
        result.first = SamplingRate::Sr8000Hz;
        break;
    case 16000:
        result.first = SamplingRate::Sr16000Hz;
        break;
    case 24000:
        result.first = SamplingRate::Sr24000Hz;
        break;
    case 32000:
        result.first = SamplingRate::Sr32000Hz;
        break;
    case 44100:
        result.first = SamplingRate::Sr44100Hz;
        break;
    case 48000:
        result.first = SamplingRate::Sr48000Hz;
        break;
    case 88200:
        result.first = SamplingRate::Sr88200Hz;
        break;
    case 96000:
        result.first = SamplingRate::Sr96000Hz;
        break;
    }
    return result;
}

static void readAllFrames(const SoundFile &sndFile,
                          std::vector<short> &frames)
{
    sndFile.readAllFramesPCM16(&frames);
}

static void readAllFrames(const SoundFile &sndFile,
                          std::vector<float> &frames)
{
    sndFile.readAllFramesFloat(&frames);
}

static std::pair<bool, std::string> WriteFramesToFile(
    const std::string &fileName,
    const std::vector<int16_t> &frames,
    uint32_t samplingRate)
{
    return writeSoundFilePCM16(fileName, frames, samplingRate);
}

static std::pair<bool, std::string> WriteFramesToFile(
    const std::string &fileName,
    const std::vector<float> &frames,
    uint32_t samplingRate)
{
    return writeSoundFileFloat(fileName, frames, samplingRate);
}
