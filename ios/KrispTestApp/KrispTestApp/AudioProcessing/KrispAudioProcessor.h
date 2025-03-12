#ifndef KRISP_AUDIO_PROCESSOR_H
#define KRISP_AUDIO_PROCESSOR_H

#pragma mark - KrispAudioBase

@interface KrispAudioBase : NSObject
- (BOOL)loadKrisp:(NSData *)modelData sampleRate: (UInt32) sampleRate;
- (void)unloadKrisp;
- (UInt32)getSampleRate;
- (BOOL)setSampleRate:(UInt32) sampleRate;
@end


#pragma mark - KrispWavFileProcessor

typedef void (^ProcessingCallback)(NSUInteger processedFrames, NSUInteger processingTimeSeconds);
typedef void (^HeaderCallback)(NSUInteger numberOfFrames);


@interface KrispWavFileProcessor : KrispAudioBase
- (BOOL)processWavAudioData:(NSData *)audioData
    processedAudioData: (NSData **) processedAudioData
    progressCallback:(ProcessingCallback)progressCallback
    headerCallback:(HeaderCallback)headerCallback;
- (void)stopProcessing;
@end

#pragma mark - KrispAudioProcessor

@interface KrispAudioProcessor : KrispAudioBase
- (BOOL)processAudioFrames:(NSData *)audioData
        processedAudioData: (NSData *) processedData;
@end

#endif
