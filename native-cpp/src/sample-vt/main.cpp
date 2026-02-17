#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "krisp-audio-sdk-vt.hpp"
#include "krisp-audio-sdk.hpp"

using namespace Krisp::AudioSdk;

namespace
{
const std::string authToken = "your-auto-token"; // get your auth token from Krisp dashboard
const SamplingRate inputSampleRate = SamplingRate::Sr8000Hz;
const SamplingRate outputSampleRate = SamplingRate::Sr8000Hz;
const FrameDuration inputFrameDuration = FrameDuration::Fd10ms;
const VtCustomVocabularyData customVocabulary = {
    .vocabulary = {L"Krisp", L"SDK"},
    .dictionary = {
        {L"hello", L"hola"},
        {L"world", L"mundo"},
    }
};

std::shared_ptr<Vt<int16_t>> vtSession;
std::vector<int16_t> translatedSamples;

std::atomic<bool> isSessionReady{false};
std::condition_variable sessionReadyCondition;
std::mutex sessionReadyMutex;

std::ofstream logFile;

void print(const std::string& str)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << str << std::endl;
}

void printW(const std::wstring& wstr)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    std::wcout << wstr << std::endl;
}

void initSdk()
{
    // initialize the Krisp SDK
    logFile.open("log.txt");
    if (!logFile.is_open())
    {
        std::cerr << "Failed to open log file" << std::endl;
    }

    auto logCallback = [](const std::string& message, LogLevel level)
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        logFile << "[" << static_cast<int>(level) << "] " << message << std::endl;
    };

    globalInit(L"", logCallback, LogLevel::Trace);
}

void destroySdk()
{
    // destroy the Krisp SDK
    globalDestroy();
}

void initSession()
{
    // initialize the VT session
    VtSessionConfig config = {
        .authToken = authToken,
        .inputSampleRate = inputSampleRate,
        .inputFrameDuration = inputFrameDuration,
        .outputSampleRate = outputSampleRate,
        .inputLanguageCode = "en-US",
        .outputLanguageCode = "es-ES",
        .gender = VtGender::Female,
        .customVocabulary = customVocabulary
    };

    // callback to receive the translated audio samples
    auto audioResultCallback = [](const VtAudioResult<int16_t>& audioResult)
    {
        translatedSamples.insert(
            translatedSamples.end(), audioResult.outputSamples.begin(), audioResult.outputSamples.end());
    };

    // callback to receive the original transcript result
    auto originalTranscriptCallback = [](const VtOriginalTranscriptionResult& originalTranscriptResult)
    {
        printW(L"Original transcript result: " + originalTranscriptResult.transcript);
    };

    // callback to receive the translated transcript result
    auto translatedTranscriptCallback = [](const VtTranslatedTranscriptionResult& translatedTranscriptResult)
    {
        printW(L"Translated transcript result: " + translatedTranscriptResult.transcript);
    };

    // callback to receive events
    auto eventCallback = [](const VtEventType& event)
    {
        if (event == VtEventType::InputAllowed)
        {
            print("Input allowed received. We can push frames now.");
            isSessionReady = true;
        }
        else if (event == VtEventType::InputNotAllowed)
        {
            print("Input not allowed received. Session is not ready to push frames.");
            isSessionReady = false;
        }
        sessionReadyCondition.notify_one();
    };

    // callback to receive errors
    auto errorCallback = [](const VtErrorType& error)
    {
        print("Error: " + std::to_string(static_cast<int>(error)));
    };

    // create the VT session
    vtSession = Vt<int16_t>::create(
        config,
        originalTranscriptCallback,
        translatedTranscriptCallback,
        audioResultCallback,
        eventCallback,
        errorCallback);

    if (!vtSession)
    {
        print("Failed to create VT session");
        return;
    }
}

void destroySession()
{
    vtSession = nullptr;
}

void checkSessionReady()
{
    // check if the session is ready to process frames
    if (!isSessionReady)
    {
        // wait for session to be ready
        std::unique_lock<std::mutex> lock(sessionReadyMutex);

        sessionReadyCondition.wait_for(
            lock,
            std::chrono::seconds(3),
            []()
            {
                return isSessionReady.load();
            });
    }
}

std::vector<int16_t> getAudioData()
{
    std::vector<int16_t> data; // place your audio data here
    return data;
}

void processFrames()
{
    // read input data and push it to the VT session
    const std::vector<int16_t> inputSamples = getAudioData();
    std::cout << "Input audio samples size: " << inputSamples.size() << std::endl;

    const size_t numInputSamples =
        static_cast<size_t>(inputSampleRate) * static_cast<size_t>(inputFrameDuration) / 1000;

    for (size_t i = 0; i < inputSamples.size(); i += numInputSamples)
    {
        checkSessionReady();
        vtSession->process(&inputSamples[i], numInputSamples);

        // simulate real-time processing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace

int main()
{
    initSdk();
    initSession();

    processFrames();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    destroySession();
    destroySdk();

    std::cout << "Translated audio samples size: " << translatedSamples.size() << std::endl;
    return 0;
}
