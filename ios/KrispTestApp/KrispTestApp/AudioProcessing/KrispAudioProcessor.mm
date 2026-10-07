#import <Foundation/Foundation.h>
#import <mach/mach.h>
#import <malloc/malloc.h>

#import "KrispAudioProcessor.h"
#include "KrispAudioSDK/krisp-audio-sdk.hpp"
#include "KrispAudioSDK/krisp-audio-sdk-nc.hpp"


static std::pair<Krisp::AudioSdk::SamplingRate, bool> getKrispSampleRate(uint32_t rate)
{
    std::pair<Krisp::AudioSdk::SamplingRate, bool> result;
    result.second = true;
    switch (rate)
    {
    case 8000:
        result.first = Krisp::AudioSdk::SamplingRate::Sr8000Hz;
        break;
    case 16000:
        result.first = Krisp::AudioSdk::SamplingRate::Sr16000Hz;
        break;
    case 32000:
        result.first = Krisp::AudioSdk::SamplingRate::Sr32000Hz;
        break;
    case 44100:
        result.first = Krisp::AudioSdk::SamplingRate::Sr44100Hz;
        break;
    case 48000:
        result.first = Krisp::AudioSdk::SamplingRate::Sr48000Hz;
        break;
    case 88200:
        result.first = Krisp::AudioSdk::SamplingRate::Sr88200Hz;
        break;
    case 96000:
        result.first = Krisp::AudioSdk::SamplingRate::Sr96000Hz;
        break;
    default:
        result.first = static_cast<Krisp::AudioSdk::SamplingRate>(0);
        result.second = false;
        break;
    }
    return result;
}


#pragma mark - KrispAudioBase

@interface KrispAudioBase()
@property (nonatomic) NSData * modelData;
@property (nonatomic) std::shared_ptr<Krisp::AudioSdk::Nc<float>> krispNcSession;
- (std::shared_ptr<Krisp::AudioSdk::Nc<float>> &)getKrispNcSession;
@property Krisp::AudioSdk::SamplingRate samplingRate;
@property (nonatomic) Krisp::AudioSdk::ModelInfo modelInfo;
@end


@implementation KrispAudioBase

- (std::shared_ptr<Krisp::AudioSdk::Nc<float>> &)getKrispNcSession {
    return _krispNcSession;
}

- (UInt32) getSampleRate {
    return static_cast<UInt32>(self.samplingRate);
}

- (BOOL) createNcSession {
    auto frameDuration = Krisp::AudioSdk::FrameDuration::Fd10ms;
    bool withStats = false;
    typedef float SamplingFormat;
    try {
        Krisp::AudioSdk::NcSessionConfig ncSessionCfg = {
            self.samplingRate,
            frameDuration,
            self.samplingRate,
            &_modelInfo,
            withStats,
            nullptr
        };
        using Krisp::AudioSdk::Nc;
        std::shared_ptr<Nc<SamplingFormat>> ncSession = Nc<SamplingFormat>::create(ncSessionCfg);
        self.krispNcSession = ncSession;
    }
    catch (const std::exception &ex) {
        NSLog(@"Failed loading model ");
        return NO;
    }
    catch (...) {
        return NO;
    }
    return YES;
}

- (BOOL) setSampleRate:(UInt32)sampleRate {
    auto sampleRateResult = getKrispSampleRate(sampleRate);
    if (!sampleRateResult.second) {
        NSLog(@"Unsupported sampling rate: %u", sampleRate);
        return NO;
    }
    self.samplingRate = sampleRateResult.first;
    return [self createNcSession];
}

- (BOOL)loadKrisp:(NSData *)modelDataIn sampleRate:(UInt32)sampleRate {
    using Krisp::AudioSdk::ModelInfo;
    self.modelData = modelDataIn;
    _modelInfo.blob.first = static_cast<const uint8_t *>(self.modelData.bytes);
    _modelInfo.blob.second = self.modelData.length;
    auto sampleRateResult = getKrispSampleRate(sampleRate);
    if (!sampleRateResult.second) {
        return NO;
    }
    self.samplingRate = sampleRateResult.first;
    return [self createNcSession];
}

- (void)unloadKrisp {
}

@end


#pragma mark - KrispLeakRepro

static double reproLiveMallocMiB(void) {
    malloc_statistics_t stats;
    malloc_zone_statistics(NULL, &stats);
    return stats.size_in_use / 1048576.0;
}

static double reproFootprintMiB(void) {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
        return 0;
    }
    return info.phys_footprint / 1048576.0;
}

@implementation KrispLeakRepro

+ (NSString *)runWithModelData:(NSData *)modelData
                    sampleRate:(UInt32)sampleRate
                    iterations:(int)iterations
                  processFrames:(int)framesPerSession {
    using namespace Krisp::AudioSdk;
    NSMutableString *report = [NSMutableString string];
    auto rateResult = getKrispSampleRate(sampleRate);
    if (!rateResult.second) {
        return [NSString stringWithFormat:@"Unsupported sample rate %u", sampleRate];
    }
    ModelInfo modelInfo;
    modelInfo.blob.first = static_cast<const uint8_t *>(modelData.bytes);
    modelInfo.blob.second = modelData.length;
    const size_t frameSamples = (size_t)sampleRate * 10 / 1000;
    std::vector<short> in(frameSamples, 0), out(frameSamples, 0);
    for (size_t i = 0; i < frameSamples; i++) {
        in[i] = (short)((i % 97) * 50 - 2400);
    }
    [report appendFormat:@"start: live malloc %.2f MiB, footprint %.2f MiB\n", reproLiveMallocMiB(), reproFootprintMiB()];
    double afterFirst = 0;
    for (int it = 1; it <= iterations; it++) {
        NcSessionConfig cfg = {rateResult.first, FrameDuration::Fd10ms, rateResult.first, &modelInfo, false, nullptr};
        std::shared_ptr<Nc<short>> session;
        try {
            session = Nc<short>::create(cfg);
        } catch (const std::exception &ex) {
            [report appendFormat:@"iteration %d: create failed: %s\n", it, ex.what()];
            break;
        }
        for (int f = 0; f < framesPerSession && session; f++) {
            session->process(in.data(), frameSamples, out.data(), frameSamples, 100.0f, nullptr);
        }
        session.reset(); // destroys the session
        double live = reproLiveMallocMiB();
        if (it == 1) {
            afterFirst = live;
        }
        [report appendFormat:@"iteration %2d: after destroy live malloc %.2f MiB (%+.2f vs iteration 1), footprint %.2f MiB\n",
                             it, live, live - afterFirst, reproFootprintMiB()];
    }
    return report;
}

@end

#pragma mark - KrispWavFileProcessor


@interface KrispWavFileProcessor()
@property (nonatomic) int samplesPerFrame;
@property (nonatomic) SInt16 * outputFrame;
@property (atomic) BOOL stopFlag;
@end


@implementation KrispWavFileProcessor

- (instancetype)init {
    self = [super init];
    _stopFlag = NO;
    return self;
}


- (void) stopProcessing {
    self.stopFlag = YES;
}

- (double)getTimeMs {
    return CFAbsoluteTimeGetCurrent() * 1000.0;
}

- (NSUInteger) getWavHeaderSize: (NSData *)audioData {
    // Pointer to raw bytes
    const uint8_t *fileBytes = (const uint8_t *)audioData.bytes;
    NSUInteger fileSize = audioData.length;
    
    // 1) Parse the first 12 bytes: "RIFF" + overallSize + "WAVE"
    //    Make sure we actually have a 'RIFF' file, and that it says 'WAVE'.
    //    This is quick sanity, not always mandatory if you trust the input.
    const uint32_t chunkRiff = *(const uint32_t *)(fileBytes + 0);  // 'RIFF'
    const uint32_t overallSize = *(const uint32_t *)(fileBytes + 4);
    const uint32_t waveID = *(const uint32_t *)(fileBytes + 8);     // 'WAVE'
    
    // Optional: verify those match their expected values
    // 'RIFF' is 0x46464952 LE, 'WAVE' is 0x45564157 LE
    // For clarity, Apple’s CFSwapInt32... might help, but let's keep it straightforward
    if (chunkRiff != 0x46464952 || waveID != 0x45564157) {
        NSLog(@"File is not a standard RIFF/WAVE file.");
        return 0;
    }
    
    // Start scanning subchunks after the first 12 bytes
    NSUInteger offset = 12;
    BOOL foundDataChunk = NO;
    NSUInteger dataChunkOffset = NSNotFound;
    NSUInteger dataChunkSize = 0;
    
    // 2) Walk subchunks until we find 'data'
    while (offset + 8 <= fileSize) {
        // Each subchunk has an 8-byte header: ID (4 bytes) + size (4 bytes)
        uint32_t chunkID  = *(const uint32_t *)(fileBytes + offset);
        uint32_t chunkLen = *(const uint32_t *)(fileBytes + offset + 4);
        
        // Move pointer forward past the subchunk header
        offset += 8;
        
        // Safety: chunkLen might run beyond file. Check it.
        if (offset + chunkLen > fileSize) {
            NSLog(@"Invalid chunk length that goes beyond file size. Possibly corrupted.");
            return 0;
        }
        
        // Compare chunkID to 'data' (LE = 0x61746164)
        if (chunkID == 0x61746164) {
            // Found 'data'
            foundDataChunk = YES;
            dataChunkOffset = offset;  // points to start of PCM frames
            dataChunkSize   = chunkLen;
            break;
        } else {
            // Skip this chunk’s payload
            offset += chunkLen;
        }
    }
    
    if (!foundDataChunk) {
        NSLog(@"No 'data' chunk found in WAV file.");
        return 0;
    }
    
    // Now dataChunkOffset is where the actual PCM samples begin
    // dataChunkSize is how many bytes of PCM are in that chunk
    if (dataChunkSize == 0) {
        NSLog(@"'data' chunk has zero size. Nothing to process.");
        return 0;
    }
    
    // Sanity check
    if (dataChunkOffset + dataChunkSize > fileSize) {
        NSLog(@"Invalid 'data' chunk extends beyond file end.");
        return 0;
    }
    return dataChunkOffset;
}

- (BOOL)readWAVHeader: (NSData *)headerData
          numChannels:(uint16_t *)numChannels
          audioFormat:(uint16_t *)audioFormat
           sampleRate:(uint32_t *)sampleRate
       bytesPerSample:(uint16_t *)bitsPerSample {
    if (headerData.length < 36) {
        NSLog(@"WAV header is too short");
        return NO;
    }
    
    uint16_t tmp16;
    uint32_t tmp32;
    
    // Read audioFormat (offset 20, 2 bytes)
    [headerData getBytes:&tmp16 range:NSMakeRange(20, 2)];
    *audioFormat = CFSwapInt16LittleToHost(tmp16);
    
    // Read numChannels (offset 22, 2 bytes)
    [headerData getBytes:&tmp16 range:NSMakeRange(22, 2)];
    *numChannels = CFSwapInt16LittleToHost(tmp16);
    
    // Read sampleRate (offset 24, 4 bytes)
    [headerData getBytes:&tmp32 range:NSMakeRange(24, 4)];
    *sampleRate = CFSwapInt32LittleToHost(tmp32);
    
    // Read bitsPerSample (offset 34, 2 bytes)
    [headerData getBytes:&tmp16 range:NSMakeRange(34, 2)];
    *bitsPerSample = CFSwapInt16LittleToHost(tmp16);
        
    NSLog(@"WAV Header Info: audioFormat=%hu, numChannels=%hu, sampleRate=%u, bytesPerSample=%hu",
          *audioFormat, *numChannels, *sampleRate, *bitsPerSample);
    
    return YES;
}

- (BOOL)processWavAudioData:(NSData *)audioData
            processedAudioData: (NSData **) processedAudioData
            progressCallback:(ProcessingCallback)progressCallback
            headerCallback:(HeaderCallback)headerCallback {

    NSUInteger wavHeaderSize = [self getWavHeaderSize:audioData];
    if (audioData.length < wavHeaderSize) {
        NSLog(@"Audio data is too small to contain a valid WAV header.");
        return NO;
    }

    // Extract WAV header and payload data
    NSData *headerData = [audioData subdataWithRange:NSMakeRange(0, wavHeaderSize)];
    NSData *payloadData = [audioData subdataWithRange:NSMakeRange(wavHeaderSize, audioData.length - wavHeaderSize)];

    uint16_t numChannels, audioFormat, bitsPerSample;
    uint32_t sampleRate;
    
    [self readWAVHeader:headerData
            numChannels:&numChannels
            audioFormat:&audioFormat
             sampleRate:&sampleRate
         bytesPerSample:&bitsPerSample];
    
    if (audioFormat != 3) {
        NSLog(@"WAV file should be IEEE Float (format is: %hu", audioFormat);
        return NO;
    }
    
    if (bitsPerSample != 32) {
        NSLog(@"WAV file is not PCM FLOAT32 (bytes per sample is: %hu", bitsPerSample);
        return NO;
    }
    
    if (numChannels != 1) {
        NSLog(@"The code is limited to work only with mono WAV file. The numChannels is: %hu", numChannels);
        return NO;
    }
    
    if (![self setSampleRate:sampleRate]) {
        return NO;
    }
    auto ncSession = [self getKrispNcSession];

    if (!ncSession.get()) {
        return NO;
    }
  
    const int frameDurationMs = 10;
    const int bytesPerSample = bitsPerSample / 8;

    int samplesPerFrame = ([self getSampleRate] * frameDurationMs) / 1000;
    const int frameSize = samplesPerFrame * bytesPerSample;

    NSUInteger payloadLength = payloadData.length;
    const float *inputSamples = (const float *)[payloadData bytes];
    NSUInteger totalFrames = payloadLength / frameSize;
    NSUInteger leftoverBytes = payloadLength % frameSize;

    if (leftoverBytes > 0) {
        NSLog(@"Skipping leftover frame smaller than 10ms: %lu bytes", (unsigned long)leftoverBytes);
    }
    if (headerCallback) {
        headerCallback(totalFrames);
    }
    
    self.stopFlag = NO;
    
    NSUInteger processedDataSize = headerData.length + totalFrames * frameSize;
    NSMutableData *processedData = [NSMutableData dataWithLength:processedDataSize + leftoverBytes];
    *processedAudioData = processedData;
    [processedData replaceBytesInRange:NSMakeRange(0, headerData.length) withBytes:headerData.bytes];
    float *outputSamples = (float *)((uint8_t *)processedData.mutableBytes + headerData.length);
    NSUInteger processedFramesCount = 0;

    BOOL success = YES;
    double beforeMilliseconds = [self getTimeMs];
    
    for (NSUInteger frameIndex = 0; frameIndex < totalFrames; frameIndex++) {
        const float *frameInput = &inputSamples[frameIndex * samplesPerFrame];
        float *frameOutput = &outputSamples[frameIndex * samplesPerFrame];
        ncSession->process(
            frameInput,
            samplesPerFrame,
            frameOutput,
            samplesPerFrame,
            100,
            nullptr
        );
        processedFramesCount++;
        if (processedFramesCount % 5000 == 0 && progressCallback) {
            double fileProcessingTimeSeconds = (NSInteger)(([self getTimeMs] - beforeMilliseconds) / 1000);
            progressCallback(processedFramesCount, fileProcessingTimeSeconds);
        }
        if (self.stopFlag) {
            success = NO;
            break;
        }
    }
    if (progressCallback) {
        double fileProcessingTimeSeconds = (NSInteger)(([self getTimeMs] - beforeMilliseconds) / 1000);
        progressCallback(processedFramesCount, fileProcessingTimeSeconds);
    }
    return success;
}

- (void)processFrame {
}

@end


#pragma mark - KrispAudioProcessor



static const int kChannels = 1;
static const int kFrameDurationMs = 10;


@interface KrispAudioProcessor()
@end


@implementation KrispAudioProcessor

- (BOOL)processAudioFrames:(NSData *)audioData
        processedAudioData: (NSData *) processedData {
    
    auto ncSession = [self getKrispNcSession];

    if (!ncSession.get()) {
        return NO;
    }
    
    const int frameDurationMs = 10;
    const int bytesPerSample = 4;
    int samplesPerFrame = ([self getSampleRate] * frameDurationMs) / 1000;
    int frameSize = samplesPerFrame * bytesPerSample;
    
    const float *inputSamples = (const float *)[audioData bytes];
    float *outputSamples = (float *)((uint8_t *)processedData.bytes);

    NSUInteger totalFrames = audioData.length / frameSize;
    
    for (NSUInteger frameIndex = 0; frameIndex < totalFrames; frameIndex++) {
        const float *frameInput = &inputSamples[frameIndex * samplesPerFrame];
        float *frameOutput = &outputSamples[frameIndex * samplesPerFrame];
        ncSession->process(
                 frameInput,
                 samplesPerFrame,
                 frameOutput,
                 samplesPerFrame,
                 100,
                 nullptr
        );
    }
    return YES;
}

@end
